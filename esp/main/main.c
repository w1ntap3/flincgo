#include "esp_err.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include <stdint.h>

// MACRO DEFINITIONS
#define MAGIC_BYTES 4

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
static wifi_init_config_t wifi_cfg;

// STATIC FUNCTION DECLARATIONS
static int establish_wifi(const char *ssid, const char *password);

void app_main(void) {
  ESP_ERROR_CHECK(nvs_flash_init());
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());

  establish_wifi(CONFIG_WIFI_SSID, CONFIG_WIFI_PASSWORD);
}

// STATIC FUNCTION IMPLEMENTATIONS
static int establish_wifi(const char *ssid, const char *password) { return 0; }
