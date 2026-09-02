#include "flincgo.h"
#include "cc.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_wifi_default.h"
#include "esp_wifi_types_generic.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include <stdint.h>
#include <string.h>
#define FLINCGO_NVS_PARTITION "flincgo"

#define NVS_TAG "NVS Flash"
#define EVENT_LOOP_TAG "Default Event Loop"
#define NETIF_TAG "TCP/IP Stack"
#define WIFI_TAG "Wi-Fi"
#define SOCKET_TAG "Socket"
#define FLINCGO_TAG "FlinCGo"

#define WIFI_MAX_RETRIES 10

#define FLINCGO_STATION_IP "192.168.4.2"

#define MSG_BATCH_MAX_SIZE CONFIG_FLINCGO_MTU
/* -1 = never initialized
 * 0 = not initialized right now but it was before
 * 1 = currently initialized
 */
static int8_t flincgo_initialized = -1;

static esp_netif_t *ap_netif;
static int fd = -1;
static struct msg_ring_buf msg_batch;

static struct sockaddr_in dest_addr;
static esp_err_t flincgo_start_ap(void) {
  size_t ssid_len = strlen(CONFIG_FLINCGO_AP_SSID);
  size_t pass_len = strlen(CONFIG_FLINCGO_AP_PASS);
  if ((ssid_len == 0) || (CONFIG_FLINCGO_AP_SSID[0] == '\0')) {
    ESP_LOGE(WIFI_TAG,
             "The config has not provided a valid SSID. (ssid=\"%s\")",
             CONFIG_FLINCGO_AP_SSID);
    return ESP_ERR_WIFI_SSID;
  } else if (ssid_len > MAX_SSID_LEN || pass_len > MAX_PASSPHRASE_LEN) {
    // Using MAX_SSID_LEN and MAX_PASSPHRASE_LEN to adapt if ESP changes the
    // fundamental values
    ESP_LOGE(WIFI_TAG,
             "The config-provided SSID or password are too long.\nSSID length: "
             "%d (Max is %d)\nPassword length: %d (Max is %d)",
             ssid_len, MAX_SSID_LEN, pass_len, MAX_PASSPHRASE_LEN);
    return ESP_ERR_WIFI_SSID;
  }

  ap_netif = esp_netif_create_default_wifi_ap();
  wifi_init_config_t wifi_init_cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_err_t err = 0;
  err = esp_wifi_init(&wifi_init_cfg);
  if (err == ESP_ERR_NO_MEM) {
    ESP_LOGE(WIFI_TAG, "Could not initialize Wi-Fi. (%s)",
             esp_err_to_name(err));
    return err;
  }
  ESP_LOGI(WIFI_TAG, "Successfully initialized Wi-Fi.");

  // WPA2_PSK wifi Access Point at channel 1 with 1 allowed connection
  wifi_config_t wifi_cfg = {.ap = {.ssid = CONFIG_FLINCGO_AP_SSID,
                                   .ssid_len = strlen(CONFIG_FLINCGO_AP_SSID),
                                   .ssid_hidden = 0,
                                   .channel = 1,
                                   .max_connection = 1,
                                   .authmode = WIFI_AUTH_WPA2_PSK,
                                   .password = CONFIG_FLINCGO_AP_PASS}};
  // If password is not set, make the AP open
  if (strlen(CONFIG_FLINCGO_AP_PASS) == 0) {
    wifi_cfg.ap.authmode = WIFI_AUTH_OPEN;
  }
  err = esp_wifi_set_mode(WIFI_MODE_AP);
  if (err == ESP_ERR_WIFI_NOT_INIT) {
    ESP_LOGE(WIFI_TAG,
             "Unexpected error, Wi-Fi was not initialized despite "
             "passing the check for initialization. (%s)",
             esp_err_to_name(err));
    return err;
  }

  err = esp_wifi_set_config(WIFI_IF_AP, &wifi_cfg);
  if (err == ESP_ERR_WIFI_STATE) {
    for (uint8_t retries = 0; retries < WIFI_MAX_RETRIES; retries++) {
      ESP_LOGW(WIFI_TAG,
               "Wi-Fi is still connecting while setting a config. Retrying to "
               "set the config (%d). Wait 1s",
               retries + 1); // display as retry 1 when the variable is 0, pure
                             // convenience thing
      vTaskDelay(pdMS_TO_TICKS(1000));
      err = esp_wifi_set_config(WIFI_IF_AP, &wifi_cfg);
      if (err != ESP_ERR_WIFI_STATE)
        break;
    }
  }
  if (err != ESP_OK) {
    ESP_LOGE(WIFI_TAG,
             "Config passed to esp_wifi_set_confg() is invalid or "
             "internal NVS error occured. (%s)",
             esp_err_to_name(err)); // im rushing to my friend, lazy to
                                    // decouple the error
    return err;
  }

  err = esp_wifi_start();
  if (err != ESP_OK) {
    ESP_LOGE(WIFI_TAG, "Could not start Wi-Fi. (%s)", esp_err_to_name(err));
    return err;
  }
  return ESP_OK;
}

static esp_err_t flincgo_start_socket() {
  struct in_addr buf;
  inet_pton(AF_INET, FLINCGO_STATION_IP, &buf);
  dest_addr = (struct sockaddr_in){.sin_addr = buf,
                                   .sin_len = sizeof(buf),
                                   .sin_family = AF_INET,
                                   .sin_port = htons(CONFIG_FLINCGO_STA_PORT)};
  memset(dest_addr.sin_zero, 0, sizeof(dest_addr.sin_zero));

  fd = socket(PF_INET, SOCK_DGRAM,
              IPPROTO_UDP); // PF_INET instead of dest_addr.sin_family
                            // JUST because i remember from the manuals
                            // (boasting about manual code 2026)

  return ESP_OK;
}

esp_err_t flincgo_init(void) {
  if (flincgo_initialized == 1) {
    flincgo_deinit();
  }
  esp_err_t err = nvs_flash_init();
  if (err != ESP_OK) {
    err = nvs_flash_erase();
    if (err != ESP_OK) {
      ESP_LOGE(NVS_TAG,
               "Could not erase the default NVS Flash partition after "
               "initialization failure. (%s)",
               esp_err_to_name(err));
      return err;
    }
    err = nvs_flash_init();
    if (err != ESP_OK) {
      ESP_LOGE(NVS_TAG,
               "Could not initialize default NVS Flash partition despite "
               "erasing it after first fail. (%s)",
               esp_err_to_name(err));
      return err;
    }
  }
  ESP_LOGI(NVS_TAG, "Successfully initialized default NVS Flash.");

  err = nvs_flash_init_partition(FLINCGO_NVS_PARTITION);
  if ((err == ESP_ERR_NVS_NO_FREE_PAGES) || (err == ESP_ERR_NO_MEM)) {
    err = nvs_flash_erase_partition(FLINCGO_NVS_PARTITION);
    if (err != ESP_OK) {
      ESP_LOGE(NVS_TAG,
               "Could not erase the \"%s\" NVS Flash partition after "
               "initialization failure. (%s)",
               FLINCGO_NVS_PARTITION, esp_err_to_name(err));
      return err;
    }
    err = nvs_flash_init_partition(FLINCGO_NVS_PARTITION);
    if (err != ESP_OK) {
      ESP_LOGE(NVS_TAG,
               "Could not initialize \"%s\" NVS Flash partition despite "
               "erasing it after first fail. (%s)",
               FLINCGO_NVS_PARTITION, esp_err_to_name(err));
      return err;
    }
  }
  ESP_LOGI(NVS_TAG, "Successfully initialized \"%s\" NVS Flash partition.",
           FLINCGO_NVS_PARTITION);
  err = esp_netif_init();
  if (err != ESP_OK) {
    ESP_LOGE(NETIF_TAG,
             "Could not initialize TCP/IP stack (esp-netif module). (%s)",
             esp_err_to_name(err));
    return err;
  }
  ESP_LOGI(NETIF_TAG, "Successfully initialized the TCP/IP stack.");
  err = esp_event_loop_create_default();
  if ((err == ESP_ERR_NO_MEM) || (err == ESP_FAIL)) {
    ESP_LOGE(EVENT_LOOP_TAG, "Could not initialize Default Event Loop., %s",
             esp_err_to_name(err));
    return err;
  }
  ESP_LOGI(EVENT_LOOP_TAG, "Successfully initialized Default Event Loop.");

  err = flincgo_start_ap();
  if (err != ESP_OK) {
    ESP_LOGE(WIFI_TAG, "Could not start the Access Point. (%s)",
             esp_err_to_name(err));
    return err;
  }
  ESP_LOGI(WIFI_TAG,
           "Successfully started the Wi-Fi Access "
           "Point.\nSSID=\"%s\"\nPassword=\"%s\"",
           CONFIG_FLINCGO_AP_SSID, CONFIG_FLINCGO_AP_PASS);
  err = flincgo_start_socket();
  if (err != ESP_OK) {
    ESP_LOGE(FLINCGO_TAG, "Could not start a datagram socket.",
             esp_err_to_name(err));
    return err;
  }
  ESP_LOGI(FLINCGO_TAG, "Successfully initialized FlinCGo networking stack.");

  // Ring buffer init
  msg_batch = (struct msg_ring_buf){.available = MSG_BATCH_MAX_SIZE,
                                    .read = 0,
                                    .write = 0,
                                    .messages_count = 0};
  memset(msg_batch.ring_buf, 0, sizeof(msg_batch.ring_buf));

  flincgo_initialized = 1;
  return ESP_OK;
}

void flincgo_deinit(void) {
  if (flincgo_initialized == 0) {
    ESP_LOGW(FLINCGO_TAG, "FlinCGo is already de-initialized.");
    return;
  } else if (flincgo_initialized == -1) {
    ESP_LOGW(FLINCGO_TAG,
             "FlincGo cannot de-initialize when it was never initialized.");
    return;
  }
  ESP_LOGW(FLINCGO_TAG, "Attempting to deinit FlinCGo.");
  if (fd >= 0) {
    if (close(fd) != 0) {
      ESP_LOGW(SOCKET_TAG, "Could not close the socket file descriptor. (%s)",
               strerror(errno));
    }
    fd = -1;
  }
  esp_err_t err = 0;
  err = esp_wifi_stop();
  if (err != ESP_OK) {
    ESP_LOGW(WIFI_TAG, "Wi-Fi wasn't stopped. (%s)", esp_err_to_name(err));
  }
  err = esp_wifi_deinit();
  if (err != ESP_OK) {
    ESP_LOGW(WIFI_TAG, "Could not deinit Wi-Fi. (%s)", esp_err_to_name(err));
  }
  esp_netif_destroy_default_wifi(&ap_netif);

  flincgo_initialized = 0;
  ESP_LOGW(FLINCGO_TAG, "FlincGo has been de-initialized.");
}

esp_err_t flincgo_quicksend(const struct flincgo_mhdr mhdr,
                            const char *payload) {
  if (flincgo_initialized != 1) {
    ESP_LOGE(
        FLINCGO_TAG,
        "Could not send your message. FlinCGo is not currently initialized.");
    return ESP_ERR_INVALID_STATE;
  }
  return ESP_OK;
}

esp_err_t flincgo_queue(const struct flincgo_mhdr mhdr, const char *payload) {
  if (flincgo_initialized != 1) {
    ESP_LOGE(
        FLINCGO_TAG,
        "Could not queue your message. FlinCGo is not currently initialized.");
    return ESP_ERR_INVALID_STATE;
  }

  if (payload == NULL)
    return ESP_ERR_INVALID_ARG;

  const size_t payload_strlen = strlen(payload);
  if (payload_strlen != mhdr.payload_len)
    return ESP_ERR_INVALID_ARG;
  const size_t message_size = sizeof(mhdr) + payload_strlen;

  if (message_size > msg_batch.available) {
    if (message_size > MSG_BATCH_MAX_SIZE) {
      return ESP_ERR_NO_MEM;
    }
    // TODO: check if it fails
    flincgo_flush();
  }

  const uint8_t *mhdr_addr = (const uint8_t *)&mhdr;

  for (uint16_t byte = 0; byte < sizeof(mhdr); byte++) {
    msg_batch.ring_buf[msg_batch.write] = mhdr_addr[byte];
    msg_batch.write = (msg_batch.write + 1) % MSG_BATCH_MAX_SIZE;
    msg_batch.available--;
  }

  for (uint16_t byte = 0; byte < payload_strlen; byte++) {
    msg_batch.ring_buf[msg_batch.write] = payload[byte];
    msg_batch.write = (msg_batch.write + 1) % MSG_BATCH_MAX_SIZE;
    msg_batch.available--;
  }

  // just check for an overflow, impossible in theory but why not lmao
  if (msg_batch.messages_count >=
      (uint16_t)MSG_BATCH_MAX_SIZE / (sizeof(mhdr) + 1)) {
    ESP_LOGE(FLINCGO_TAG, "Unexpected error, how the hell did "
                          "msg_batch.messages_count just overflow..");
    return ESP_ERR_NO_MEM;
  }
  msg_batch.messages_count++;
  return ESP_OK;
}

esp_err_t flincgo_flush(void) {
  if (flincgo_initialized != 1) {
    ESP_LOGE(
        FLINCGO_TAG,
        "Could not queue your message. FlinCGo is not currently initialized.");
    return ESP_ERR_INVALID_STATE;
  }
  // just a stub
  msg_batch.available = MSG_BATCH_MAX_SIZE;
  return ESP_OK;
}
