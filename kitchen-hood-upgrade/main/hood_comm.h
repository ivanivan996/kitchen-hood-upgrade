/*
 * Pulse-width telegram to the kitchen hood's fan controller.
 * Port of Code/kitchen-hood-upgrade/CommStack (Arduino) to ESP-IDF.
 */

#pragma once

#include <stdint.h>
#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Highest speed step the controller understands (0 = off) */
#define HOOD_SPEED_MAX 6

/** Configure the output pin and start the 100 us telegram timer.
 *
 * Holds the line HIGH for 1 s first, as the original firmware does, so this blocks for ~1 s.
 */
esp_err_t hood_comm_init(int gpio_num);

/** Set the state that is transmitted continuously from now on.
 *
 * @param[in] speed 0..HOOD_SPEED_MAX, clamped.
 * @param[in] light Hood light on/off.
 */
void hood_comm_set(uint8_t speed, bool light);

#ifdef __cplusplus
}
#endif
