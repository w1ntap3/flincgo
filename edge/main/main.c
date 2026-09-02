#include "flincgo.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include <lwip/netdb.h>
#include <stdint.h>
#include <string.h>

void app_main(void) {
  esp_err_t err = flincgo_init();
  if (err != ESP_OK) {
    flincgo_deinit();
    return;
  }
  struct flincgo_mhdr test_mhdr = {
      .item_id = 1,
      .magic = FLINCGO_MAGIC,
      .payload_len = strlen("Successfully talking to you my collecta"),
      .sequence = 0,
      .severity = 22,
      .timestamp = 31421};
  flincgo_quicksend(test_mhdr, "Successfully talking to you my collecta");

  while (1) {
    for (int i = 0; i < 32; i++) {
      flincgo_queue(test_mhdr, "SACKessfully talking to you my collecta");
      test_mhdr.sequence++;
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
  flincgo_deinit();
  return;
}
