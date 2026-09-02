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
  struct flincgo_mhdr test_mhdr = {.item_id = 1,
                                   .magic = FLINCGO_MAGIC,
                                   .payload_len = strlen(message),
                                   .sequence = 0,
                                   .severity = 22,
                                   .timestamp = 31421};
  flincgo_quicksend(test_mhdr, message);
  while (1) {
    flincgo_flush();
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
  flincgo_deinit();
  return;
}
