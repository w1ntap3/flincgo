#include "esp_err.h"
#include "flincgo.h"

void app_main(void) {
  // TODO: Add a flincgo_config_t so a user can set some stuff up for himself
  ESP_ERROR_CHECK(flincgo_init());

  return;
}
