# Fan Communication Protocol

The ESP32 communicates with the fan controller through `GPIO 8` using a custom pulse-width protocol.

## Timing

The communication timer runs at **1 MHz** and `FanCommCycle()` is executed every **100 µs**.

| Signal | HIGH | LOW | Total |
|---|---:|---:|---:|
| Preamble | 1920 µs | 1152 µs | 3072 µs |
| Logic `1` | 1408 µs | 640 µs | 2048 µs |
| Logic `0` | 512 µs | 1536 µs | 2048 µs |
| Packet pause | — | 5640 µs | 5640 µs |

## Telegram

The protocol consists of **3 × 16-bit packets**.

```text
Packet 1 → FanCommTelegram[2]
Packet 2 → FanCommTelegram[1]
Packet 3 → FanCommTelegram[0]