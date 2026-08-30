#ifndef FLINCGO_H
#define FLINCGO_H

#include "esp_err.h"
#include <stdint.h>

// MACRO DEFINITIONS
#define FLINCGO_PROTOCOL_MAGIC_BYTES 4 // F C G O

// TYPE DEFINITIONS
typedef uint64_t flincgo_timestamp_t;
typedef uint16_t flincgo_item_t;

// STRUCT DEFINITIONS
// note: its all Little Endian bytes
struct __attribute__((__packed__)) flincgo_phdr_t {
  uint8_t magic[FLINCGO_PROTOCOL_MAGIC_BYTES]; // Magic bytes for integrity
  uint32_t sequence;                           // To track lost packets
  flincgo_timestamp_t timestamp;               // microseconds since boot
  uint8_t severity;       // To be parsed on application level e.g.
  flincgo_item_t item_id; // Abstract distinction of what causes the log. e.g.
                          // sensor, thing in IoT..
  uint16_t payload_len;
  char payload[]; // A string (encrypted before sending) that contains the main
                  // message.
};

esp_err_t flincgo_init(void);

#endif
