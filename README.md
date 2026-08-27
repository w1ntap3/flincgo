# FlinCGo
A lightweight UDP logging platform for ESP32 with a high-throughput Go collector and interface. Stream real-time system logs and metrics over lossy networks using encryption and sequence-tracked framing.

## Components

| Component | Responsibility |
| -------------- | --------------- |
| Edge (C firmware for ESP32) | Batched and encrypted message transmission to a datagram socket without blocking |
| Collector (Golang software for Linux PC) | Concurrent datagram ingestion, decryption, sequence loss tracking and output interface|

### Detailed overviews
For the design and architecture information for each component:
* Edge by [@wintape](https://github.com/w1ntap3)
  * [edge/README.md](edge/README.md)
* Collector by [@kdiffin](https://github.com/kdiffin/): 
  * [collector/README.md](collector/README.md)

## Usage
TBD
