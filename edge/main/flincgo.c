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

#define STATION_CONNECTED BIT0

static int8_t flincgo_initialized = 0;
static int8_t wifi_initialized = 0;
static esp_netif_t *ap_netif = NULL;
static int fd = -1;
static struct msg_ring_buf msg_batch;
static struct sockaddr_in dest_addr;
static EventGroupHandle_t wifi_events = NULL;

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data) {
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
    xEventGroupSetBits(wifi_events, STATION_CONNECTED);
  }
}

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
  if (ap_netif == NULL) {
    return ESP_ERR_WIFI_INIT_STATE;
  }
  wifi_init_config_t wifi_init_cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_err_t err = 0;
  err = esp_wifi_init(&wifi_init_cfg);
  if (err != ESP_OK) {
    ESP_LOGE(WIFI_TAG, "Could not initialize Wi-Fi. (%s)",
             esp_err_to_name(err));
    return err;
  }
  wifi_initialized = 1;
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
  if (err != ESP_OK) {
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

  wifi_events = xEventGroupCreate();
  if (wifi_events == NULL) {
    ESP_LOGE(WIFI_TAG, "Could not create the Wi-Fi event group.");
    return ESP_ERR_NO_MEM;
  }

  err = esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_AP_STACONNECTED,
                                   wifi_event_handler, NULL);
  if (err != ESP_OK) {
    ESP_LOGE(WIFI_TAG, "Could not register Wi-Fi event handler. (%s)",
             esp_err_to_name(err));
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
  int err = inet_pton(AF_INET, FLINCGO_STATION_IP, &buf);
  if (err != 1) {
    if (err == -1) {
      ESP_LOGE(SOCKET_TAG,
               "Could not convert the network address string into "
               "network bytes. (%s)",
               strerror(errno));
    }
    ESP_LOGE(SOCKET_TAG, "Unexpected error, string provided to inet_pton() "
                         "appears to be invalid.");
    return ESP_ERR_INVALID_ARG;
  }
  dest_addr = (struct sockaddr_in){.sin_addr = buf,
                                   .sin_len = sizeof(buf),
                                   .sin_family = AF_INET,
                                   .sin_port = htons(CONFIG_FLINCGO_STA_PORT)};
  memset(dest_addr.sin_zero, 0, sizeof(dest_addr.sin_zero));

  fd = socket(PF_INET, SOCK_DGRAM,
              IPPROTO_UDP); // PF_INET instead of dest_addr.sin_family
                            // JUST because i remember from the manuals
                            // (boasting about manual code 2026)
  if (fd < 0) {
    ESP_LOGE(SOCKET_TAG,
             "Could not create the file descriptor for the socket. (errno: %s)",
             strerror(errno));
    return ESP_ERR_INVALID_STATE;
  }

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
      flincgo_deinit();
      return err;
    }
    err = nvs_flash_init();
    if (err != ESP_OK) {
      ESP_LOGE(NVS_TAG,
               "Could not initialize default NVS Flash partition despite "
               "erasing it after first fail. (%s)",
               esp_err_to_name(err));
      flincgo_deinit();
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
      flincgo_deinit();
      return err;
    }
    err = nvs_flash_init_partition(FLINCGO_NVS_PARTITION);
    if (err != ESP_OK) {
      ESP_LOGE(NVS_TAG,
               "Could not initialize \"%s\" NVS Flash partition despite "
               "erasing it after first fail. (%s)",
               FLINCGO_NVS_PARTITION, esp_err_to_name(err));
      flincgo_deinit();
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
    flincgo_deinit();
    return err;
  }
  ESP_LOGI(NETIF_TAG, "Successfully initialized the TCP/IP stack.");
  err = esp_event_loop_create_default();
  if ((err == ESP_ERR_NO_MEM) || (err == ESP_FAIL)) {
    ESP_LOGE(EVENT_LOOP_TAG, "Could not initialize Default Event Loop., %s",
             esp_err_to_name(err));
    flincgo_deinit();
    return err;
  }
  ESP_LOGI(EVENT_LOOP_TAG, "Successfully initialized Default Event Loop.");

  err = flincgo_start_ap();
  if (err != ESP_OK) {
    ESP_LOGE(WIFI_TAG, "Could not start the Access Point. (%s)",
             esp_err_to_name(err));
    flincgo_deinit();
    return err;
  }
  ESP_LOGI(WIFI_TAG,
           "Successfully started the Wi-Fi Access "
           "Point.\nSSID=\"%s\"\nPassword=\"%s\"",
           CONFIG_FLINCGO_AP_SSID, CONFIG_FLINCGO_AP_PASS);

  ESP_LOGI(FLINCGO_TAG, "Waiting for the Collector to connect...");

  // Blocking for 5 minutes to wait for the Collector station, return if timed
  // out
  EventBits_t ev_bits = xEventGroupWaitBits(
      wifi_events, STATION_CONNECTED, pdFALSE, pdTRUE, pdMS_TO_TICKS(300000));

  if ((ev_bits & STATION_CONNECTED) == 0) {
    ESP_LOGE(FLINCGO_TAG, "Collector could not connect to the AP.");
    flincgo_deinit();
    return ESP_ERR_TIMEOUT;
  }

  ESP_LOGI(FLINCGO_TAG, "Collector connected to the AP.");

  err = flincgo_start_socket();
  if (err != ESP_OK) {
    ESP_LOGE(FLINCGO_TAG, "Failed to start a datagram socket. (%s)",
             esp_err_to_name(err));
    flincgo_deinit();
    return err;
  }
  ESP_LOGI(FLINCGO_TAG, "Successfully initialized FlinCGo networking stack.");

  // Ring buffer init
  msg_batch = (struct msg_ring_buf){.available = MSG_BATCH_MAX_SIZE,
                                    .read_idx = 0,
                                    .write_idx = 0,
                                    .message_count = 0};
  memset(msg_batch.ring_buf, 0, sizeof(msg_batch.ring_buf));

  flincgo_initialized = 1;
  return ESP_OK;
}

void flincgo_deinit(void) {
  ESP_LOGW(FLINCGO_TAG, "Attempting to deinit FlinCGo.");

  if (wifi_events != NULL) {
    esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_AP_STACONNECTED,
                                 wifi_event_handler);
    vEventGroupDelete(wifi_events);
    wifi_events = NULL;
  }

  if (fd >= 0) {
    if (close(fd) != 0) {
      ESP_LOGW(SOCKET_TAG,
               "Could not close the socket file descriptor. (errno: %s)",
               strerror(errno));
    }
    fd = -1;
  }

  esp_err_t err = 0;
  if (wifi_initialized == 1) {
    err = esp_wifi_stop();
    if (err != ESP_OK) {
      ESP_LOGW(WIFI_TAG, "Wi-Fi wasn't stopped. (%s)", esp_err_to_name(err));
    }
    err = esp_wifi_deinit();
    if (err != ESP_OK) {
      ESP_LOGW(WIFI_TAG, "Could not deinit Wi-Fi. (%s)", esp_err_to_name(err));
    }
    wifi_initialized = 0;
  }
  if (ap_netif != NULL) {
    esp_netif_destroy_default_wifi(&ap_netif);
    ap_netif = NULL;
  }

  flincgo_initialized = 0;
  ESP_LOGW(FLINCGO_TAG, "FlincGo has been de-initialized.");
}

static inline esp_err_t validate_payload_and_len(const struct flincgo_mhdr mhdr,
                                                 const char *payload) {
  if (payload == NULL)
    return ESP_ERR_INVALID_ARG;

  if (mhdr.payload_len < 1)
    return ESP_ERR_INVALID_ARG;

  const size_t payload_strlen = strlen(payload);
  if (payload_strlen != mhdr.payload_len)
    return ESP_ERR_INVALID_ARG;
  return ESP_OK;
}

esp_err_t flincgo_quicksend(const struct flincgo_mhdr mhdr,
                            const char *payload) {
  if (flincgo_initialized != 1) {
    ESP_LOGE(FLINCGO_TAG, "Could not quicksend the message, FlinCGo is not "
                          "currently initialized.");
    return ESP_ERR_INVALID_STATE;
  }

  esp_err_t err = validate_payload_and_len(mhdr, payload);
  if (err != ESP_OK) {
    ESP_LOGE(FLINCGO_TAG, "Could not quicksend the message, it did not pass "
                          "the payload validation.");
    return err;
  }

  const size_t message_size = sizeof(mhdr) + strlen(payload);
  if (message_size > CONFIG_FLINCGO_MTU) {
    ESP_LOGE(FLINCGO_TAG, "Could not quicksend the message, its longer than "
                          "the configured Maximum Transmission Unit.");
    return ESP_ERR_NO_MEM;
  }

  uint8_t compiled_message[message_size];
  memcpy(compiled_message, (uint8_t *)&mhdr, sizeof(mhdr));
  memcpy(compiled_message + sizeof(mhdr), payload, strlen(payload));

  int bytes_sent = sendto(fd, compiled_message, sizeof(compiled_message), 0,
                          (struct sockaddr *)&dest_addr, sizeof(dest_addr));
  if (bytes_sent < 0) {
    ESP_LOGE(SOCKET_TAG, "sendto() failed. (errno: %s)", strerror(errno));
    return ESP_ERR_INVALID_RESPONSE;
  }

  if (bytes_sent != (int)message_size) {
    ESP_LOGE(SOCKET_TAG, "sendto() sent %d bytes, expected %d.", bytes_sent,
             message_size);
    return ESP_ERR_INVALID_RESPONSE;
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

  esp_err_t err = validate_payload_and_len(mhdr, payload);
  if (err != ESP_OK) {
    ESP_LOGE(FLINCGO_TAG, "Could not queue the message, it did not pass "
                          "the payload validation.");
    return err;
  }
  const size_t payload_strlen = strlen(payload);
  const size_t message_size = sizeof(mhdr) + payload_strlen;

  if (message_size > msg_batch.available) {
    if (message_size > MSG_BATCH_MAX_SIZE) {
      ESP_LOGE(FLINCGO_TAG,
               "Unexpected error, message occurs to be fitting into the queue "
               "while being bigger than queue's size.");
      return ESP_ERR_NO_MEM;
    }
    esp_err_t err = flincgo_flush();
    if (err != ESP_OK) {
      ESP_LOGE(FLINCGO_TAG,
               "Unexpected error, the message was valid yet didn't fit the "
               "queue so an automatic flush was attempted but failed.");
      return err;
    }
  }

  // just check for an overflow, impossible in theory but why not lmao
  if (msg_batch.message_count >=
      (uint16_t)MSG_BATCH_MAX_SIZE / (sizeof(mhdr) + 1)) {
    ESP_LOGE(FLINCGO_TAG, "Unexpected error, how the hell did "
                          "msg_batch.message_count just overflow..");
    return ESP_ERR_NO_MEM;
  }

  const uint8_t *mhdr_addr = (const uint8_t *)&mhdr;

  for (uint16_t byte = 0; byte < sizeof(mhdr); byte++) {
    msg_batch.ring_buf[msg_batch.write_idx] = mhdr_addr[byte];
    msg_batch.write_idx = (msg_batch.write_idx + 1) % MSG_BATCH_MAX_SIZE;
    msg_batch.available--;
  }

  for (uint16_t byte = 0; byte < payload_strlen; byte++) {
    msg_batch.ring_buf[msg_batch.write_idx] = payload[byte];
    msg_batch.write_idx = (msg_batch.write_idx + 1) % MSG_BATCH_MAX_SIZE;
    msg_batch.available--;
  }

  msg_batch.message_count++;
  return ESP_OK;
}

esp_err_t flincgo_flush(void) {
  if (flincgo_initialized != 1) {
    ESP_LOGE(
        FLINCGO_TAG,
        "Could not flush your messages. FlinCGo is not currently initialized.");
    return ESP_ERR_INVALID_STATE;
  }
  if (msg_batch.message_count == 0) {
    // Paranoia checks, impossible in theory
    if (msg_batch.available != MSG_BATCH_MAX_SIZE) {
      ESP_LOGE(FLINCGO_TAG, "Unexpected error, message count appears to be 0,"
                            "but available space in the queue is not full.");
      return ESP_ERR_INVALID_STATE;
    }
    if (msg_batch.read_idx != msg_batch.write_idx) {
      ESP_LOGE(FLINCGO_TAG,
               "Unexpected error, message count appears to be 0, but the "
               "Collector is not up-to-date with the messages of the Edge.");
      return ESP_OK;
    }
    ESP_LOGW(FLINCGO_TAG, "No messages registered to flush.");
    return ESP_ERR_INVALID_STATE;
  }

  // Easier implementation when we don't have to jump to the beginning of the
  // ring buffer
  if (msg_batch.read_idx < msg_batch.write_idx) {
    int bytes_sent = sendto(fd, msg_batch.ring_buf + msg_batch.read_idx,
                            msg_batch.write_idx - msg_batch.read_idx, 0,
                            (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (bytes_sent < 0) {
      ESP_LOGE(SOCKET_TAG, "sendto() failed. (errno: %s)", strerror(errno));
      return ESP_ERR_INVALID_RESPONSE;
    }

    if (bytes_sent != (msg_batch.write_idx - msg_batch.read_idx)) {
      ESP_LOGE(SOCKET_TAG, "sendto() sent %d bytes, expected %d.", bytes_sent,
               (msg_batch.write_idx - msg_batch.read_idx));
      return ESP_ERR_INVALID_RESPONSE;
    }

  } else {
    uint8_t compiled_message[MSG_BATCH_MAX_SIZE - msg_batch.available];
    memcpy(compiled_message, msg_batch.ring_buf + msg_batch.read_idx,
           MSG_BATCH_MAX_SIZE - msg_batch.read_idx);
    memcpy(compiled_message + (MSG_BATCH_MAX_SIZE - msg_batch.read_idx),
           msg_batch.ring_buf, msg_batch.write_idx);
    int bytes_sent = sendto(fd, compiled_message, sizeof(compiled_message), 0,
                            (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (bytes_sent < 0) {
      ESP_LOGE(SOCKET_TAG, "sendto() failed. (errno: %s)", strerror(errno));
      return ESP_ERR_INVALID_RESPONSE;
    }

    if (bytes_sent != sizeof(compiled_message)) {
      ESP_LOGE(SOCKET_TAG, "sendto() sent %d bytes, expected %d.", bytes_sent,
               (msg_batch.write_idx - msg_batch.read_idx));
      return ESP_ERR_INVALID_RESPONSE;
    }
  }
  msg_batch = (struct msg_ring_buf){.read_idx = 0,
                                    .message_count = 0,
                                    .available = MSG_BATCH_MAX_SIZE,
                                    .write_idx = 0};
  return ESP_OK;
}
