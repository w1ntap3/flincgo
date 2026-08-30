#include "esp_err.h"
#include "flincgo.h"
#include <lwip/netdb.h>
#include <stdint.h>

const char *message = "Salam from ESP32";

void app_main(void) {
  // TODO: Add a flincgo_config_t so a user can set some stuff up for himself
  ESP_ERROR_CHECK(flincgo_init());

  return;
}
