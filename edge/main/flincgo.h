#ifndef FLINCGO_H
#define FLINCGO_H

#include "esp_err.h"
#include "sdkconfig.h"
#include <stddef.h>
#include <stdint.h>

// MACRO DEFINITIONS
#define FLINCGO_MAGIC_SIZE 4 // F C G O
#define FLINCGO_MAGIC {'F', 'C', 'G', 'O'}
// TYPE DEFINITIONS
typedef uint64_t flincgo_timestamp_t;
typedef uint16_t flincgo_item_t;

// STRUCT DEFINITIONS
// note: its all Little Endian bytes
struct __attribute__((__packed__)) flincgo_mhdr {
  uint8_t magic[FLINCGO_MAGIC_SIZE]; // Magic bytes for integrity
  uint32_t sequence;                 // To track lost packets
  flincgo_timestamp_t timestamp;     // microseconds since boot
  uint8_t severity;                  // To be parsed on application level e.g.
  flincgo_item_t item_id; // Abstract distinction of what causes the log. e.g.
                          // sensor, thing in IoT..
  uint16_t payload_len;   // Minimum 1
};

struct msg_ring_buf {
  uint8_t ring_buf[CONFIG_FLINCGO_MTU];
  uint16_t write;
  uint16_t read;
  uint16_t capacity;
};

esp_err_t flincgo_init(void);
void flincgo_deinit(void);

esp_err_t flincgo_quicksend(const struct flincgo_mhdr mhdr,
                            const char *payload);
esp_err_t flincgo_queue(const struct flincgo_mhdr mhdr, const char *payload);
esp_err_t flincgo_flush(void);

#endif
