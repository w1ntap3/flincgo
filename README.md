# FlinCGo (/flɪŋk-ɡoʊ/)

A lightweight observability platform for ESP32 devices with a collector and output interface on Linux PC. FlinCGo allows streaming real-time low-overhead logs and metrics over lossy networks with high throughput while efficiently implementing a custom binary protocol, non-blocking batching, payload encryption, sequence loss tracking and comfortable user experience.

## Components

| Component                                | Responsibility                                                                   |
| ---------------------------------------- | -------------------------------------------------------------------------------- |
| Edge (C firmware for ESP32)              | Batched and encrypted message transmission to a datagram socket without blocking |
| Collector (Golang software for Linux PC) | Datagram ingestion, decryption, sequence loss tracking and output interface      |

### Detailed overviews

For the design and architecture information for each component:

- Edge by [@wintape](https://github.com/w1ntap3)
  - [edge/README.md](edge/README.md)
- Collector by [@kdiffin](https://github.com/kdiffin/):
  - [collector/README.md](collector/README.md)

## Usage

### Edge usage

1. FlinCGo expects you to have [ESP-IDF](https://github.com/espressif/esp-idf/) set up on your machine and `idf.py` command available as a prerequisite.
2. Make sure your working directory is `./edge/`. Set your ESP32 MCU as the target.

```bash
cd edge
idf.py set-target esp32
```

3. Run the following command to access `menuconfig`:

```bash
idf.py menuconfig
```

After the menuconfig TUI loads, navigate to "FlinCGo Edge Configuration" and fill-in the self explanatory configurations.

4. In the same working directory, run the following command to flash the code and watch the FlinCGo logs in case of errors.

```bash
idf.py build flash monitor -p [PORT] # PORT is usually /dev/ttyUSB*
```

### Collector usage

(TBD)
