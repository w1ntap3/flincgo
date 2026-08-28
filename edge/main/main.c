#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_types.h"
#include "esp_wifi.h"
#include "esp_wifi_default.h"
#include "esp_wifi_types_generic.h"
#include "freertos/idf_additions.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include <stdint.h>
#include <string.h>

// MACRO DEFINITIONS
#define MAGIC_BYTES 4
#define NVS_PARTITION "flincgo"

#define AP_SSID CONFIG_WIFI_SSID
#define AP_PASS CONFIG_WIFI_PASSWORD

#define NVS_TAG "NVS Flash"
#define EVENT_LOOP_TAG "Default Event Loop"
#define NETIF_TAG "TCP/IP Stack"
#define WIFI_TAG "Wi-Fi"

#define WIFI_MAX_RETRIES 10
// TYPE DEFINITIONS
typedef uint64_t timestamp_t;
typedef uint16_t item_t;

// STRUCT DEFINITIONS
// note: its all LE bytes
struct __attribute__((__packed__)) protocol_info {
  uint8_t magic[MAGIC_BYTES]; // Magic bytes for integrity
  uint32_t sequence;          // To track lost packets
  timestamp_t timestamp;      // microseconds since boot
  uint8_t severity;           // To be parsed on application level e.g.
  item_t item_id; // Abstract distinction of what causes the log. e.g. sensor,
                  // thing in IoT..
  uint16_t payload_len;
  char payload[]; // A string (encrypted before sending) that contains the main
                  // message.
};

// STATIC VARIABLE DEFINITIONS
static esp_netif_t *ap_netif;

static esp_err_t connect_to_collector(const char *ssid, const char *password) {
  if (AP_SSID == NULL) {
    ESP_LOGE(WIFI_TAG, "The config has not provided a valid SSID.");
    return ESP_ERR_WIFI_SSID;
  }

  ap_netif = esp_netif_create_default_wifi_ap();
  wifi_init_config_t wifi_init_cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_err_t err = 0;
  err = esp_wifi_init(&wifi_init_cfg);
  if (err == ESP_ERR_NO_MEM) {
    ESP_LOGE(WIFI_TAG, "Could not initialize Wi-Fi.");
  }
  ESP_LOGI(WIFI_TAG, "Successfully initialized Wi-Fi.");

  // WPA2_PSK wifi Access Point with 1 allowed connection
  wifi_config_t wifi_cfg = {.ap = {.ssid = AP_SSID,
                                   .ssid_len = strlen(AP_SSID),
                                   .ssid_hidden = 0,
                                   .channel = 0,
                                   .max_connection = 1,
                                   .authmode = WIFI_AUTH_WPA2_PSK,
                                   .password = AP_PASS}};
  // If password is not set, make the AP open
  if (strlen(AP_PASS) == 0) {
    wifi_cfg.ap.authmode = WIFI_AUTH_OPEN;
  }
  err = esp_wifi_set_mode(WIFI_MODE_AP);
  if (err == ESP_ERR_WIFI_NOT_INIT) {
    ESP_LOGE(WIFI_TAG, "Unexpected error, Wi-Fi was not initialized despite "
                       "passing the check for initialization");
  }

  err = esp_wifi_set_config(WIFI_IF_AP, &wifi_cfg);
  if (err == ESP_ERR_WIFI_STATE) {
    for (uint8_t retries = 0; retries < WIFI_MAX_RETRIES; retries++) {
      ESP_LOGW(WIFI_TAG,
               "Wi-Fi is still connecting while setting a config. Retrying to "
               "set the config (%d)",
               retries + 1); // display as retry 1 when the variable is 0, pure
                             // convenience thing
      vTaskDelay(1000);
      err = esp_wifi_set_config(WIFI_IF_AP, &wifi_cfg);
      if (err != ESP_ERR_WIFI_STATE)
        break;
    }
  }
  if (err != ESP_OK) {
    ESP_LOGE(WIFI_TAG,
             "Config passed to esp_wifi_set_confg() is invalid or "
             "internal NVS error occured."); // im rushing to my friend, lazy to
                                             // decouple the error
    return ESP_ERR_WIFI_NOT_STARTED;
  }
  return ESP_OK;
}

void app_main(void) {
  esp_err_t err = nvs_flash_init_partition(NVS_PARTITION);
  err = nvs_flash_init_partition(NVS_PARTITION);
  if ((err == ESP_ERR_NVS_NO_FREE_PAGES) || (err == ESP_ERR_NO_MEM)) {
    err = nvs_flash_erase_partition(NVS_PARTITION);
    if (err != ESP_OK) {
      ESP_LOGE(NVS_TAG, "Could not initialize NVS Flash.");
      return;
    }
    nvs_flash_init_partition(NVS_PARTITION);
  }
  ESP_LOGI(NVS_TAG, "Successfully initialized NVS Flash.");

  err = esp_netif_init();
  if (err == ESP_FAIL) {
    ESP_LOGE(NETIF_TAG,
             "Could not initialize TCP/IP stack (esp-netif module).");
    return;
  }
  ESP_LOGI(NETIF_TAG, "Successfully initialized the TCP/IP stack.");
  err = esp_event_loop_create_default();
  if ((err == ESP_ERR_NO_MEM) || (err = ESP_FAIL)) {
    ESP_LOGE(EVENT_LOOP_TAG, "Could not initialize Default Event Loop.");
    return;
  }
  ESP_LOGI(EVENT_LOOP_TAG, "Successfully initialized Default Event Loop.");

  err = connect_to_collector(AP_SSID, AP_PASS);
  if (err == ESP_OK) {
    ESP_LOGI(WIFI_TAG, "Successfully initialized a Wi-Fi Access Point.");
  }
}
