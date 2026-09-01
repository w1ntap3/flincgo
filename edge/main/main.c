#include "flincgo.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include <lwip/netdb.h>
#include <stdint.h>
#include <string.h>

const char *message = "Salam from ESP32";

void app_main(void) {
  // TODO: Add a flincgo_config_t so a user can set some stuff up for himself
  flincgo_init();

  vTaskDelay(pdMS_TO_TICKS(5000));
  for (int i = 0; i < 20; i++) {
    char *data = "this is flincgo and we are dope.";
    struct flincgo_mhdr mhdr = {.item_id = 12,
                                .magic = FLINCGO_MAGIC_ARRAY,
                                .payload = data,
                                .payload_len = strlen(data),
                                .sequence = i,
                                .timestamp = 2318824};
    flincgo_send(mhdr);
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
  flincgo_deinit();
  return;
}
