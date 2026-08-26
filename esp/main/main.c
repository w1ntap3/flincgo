#include <stdint.h>

#define MAGIC_BYTES 4

typedef uint64_t timestamp_t;
typedef uint16_t item_t;

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

void app_main(void) {
  while (1) {
  }
}
