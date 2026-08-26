# Fan Communication Protocol

The ESP32 communicates with the fan controller through `GPIO 8` using a custom pulse-width protocol.

## Timing

| Signal | HIGH | LOW | Total |
|---|---:|---:|---:|
| Preamble | 1920 µs | 1154 µs | 3074 µs |
| Logic `1` | 1380 µs | 640 µs | 2020 µs |
| Logic `0` | 520 µs | 1530 µs | 2050 µs |
| Packet pause | 0 µs | 5100 µs | 5100 µs |

The hardware timer runs at **1 MHz** and the state machine is executed every **100 µs**.

## Telegram Structure

The controller continuously transmits **3 × 16-bit packets**:

```text
Packet 1 → FanCommTelegram[2]
Packet 2 → FanCommTelegram[1]
Packet 3 → FanCommTelegram[0]