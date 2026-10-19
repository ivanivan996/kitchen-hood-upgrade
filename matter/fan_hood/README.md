# fan_hood: kitchen hood as a Matter device

ESP-IDF / esp-matter port of the Arduino firmware in the `kitchen-hood-upgrade` repo
(`Code/kitchen-hood-upgrade`). The Modbus TCP server and the
Home Assistant template entities are replaced by native Matter endpoints, so the hood works
directly with Home Assistant, Apple Home, Google Home and Alexa.

| Endpoint | Device type | Controls |
|---|---|---|
| 1 | Fan (MultiSpeed, `SpeedMax` = 6) | Off / speed 1–6 (also as 0–100 % and Low/Medium/High) |
| 2 | On/Off Light | Hood light |

The telegram to the fan controller (`main/hood_comm.c`) is the same state machine with the same
100 µs tick and timings as `CommStack.cpp`, driven by a GPTimer interrupt that lives in IRAM,
so it keeps running during flash writes.

## Pins (ESP32-C6, change in `idf.py menuconfig` → Kitchen Hood Configuration)

| Function | GPIO | Arduino name |
|---|---|---|
| Telegram to fan controller | 10 | `FanCommPin` |
| Light button (to GND) | 1 | `BUTTON_LED` |
| Speed up button (to GND) | 2 | `BUTTON_UP` |
| Speed down button (to GND) | 3 | `BUTTON_DOWN` |
| Factory reset (hold ~5 s) | 9 (BOOT) | – |

## Behaviour changes vs. the Arduino firmware

- Wi-Fi credentials and the static IP are no longer in the code; the phone provides Wi-Fi
  during Matter commissioning (2.4 GHz only).
- After a power cut the fan and light always start **off**.
- Controllers set speed as a percentage; it is rounded to the nearest of the 6 steps
  (17 % → 1, 33 % → 2, 50 % → 3, 67 % → 4, 83 % → 5, 100 % → 6).

## Build and flash

```bash
# ESP-IDF v6 + esp-matter must be exported (ESP_MATTER_PATH set)
cd matter/fan_hood
idf.py set-target esp32c6
idf.py build
idf.py -p <PORT> flash monitor
```

Pair with the manual code `34970112332` (test credentials, "Uncertified accessory" warning is expected).
Once paired, remove the `modbus:` include and the `Aspirator` template entities from
`configuration.yaml`.
