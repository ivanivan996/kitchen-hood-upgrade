/*
   Kitchen hood as a Matter device: a Fan endpoint (6 speeds) and an On/Off Light endpoint
   (hood light), both driven over the pulse-width telegram in hood_comm.c.
*/

#pragma once

#include <esp_err.h>
#include <esp_matter.h>

#include <app/clusters/fan-control-server/fan-control-delegate.h>

#if CHIP_DEVICE_CONFIG_ENABLE_THREAD
#include "esp_openthread_types.h"
#endif

typedef void *app_driver_handle_t;

/** Start the hood telegram on CONFIG_HOOD_COMM_GPIO and register the hood buttons.
 *
 * Blocks for ~1 s (controller wake-up sequence).
 *
 * @return ESP_OK on success.
 * @return error in case of failure.
 */
esp_err_t app_driver_hood_init();

/** Initialize the BOOT button, used only for factory reset (long press).
 *
 * @return Handle on success.
 * @return NULL in case of failure.
 */
app_driver_handle_t app_driver_button_init();

/** FanControl delegate that drives the hood motor. Pass it in fan::config_t before creating the endpoint. */
chip::app::Clusters::FanControl::Delegate *app_driver_fan_get_delegate();

/** Driver update for non-fan attributes (hood light OnOff). Called from app_attribute_update_cb(). */
esp_err_t app_driver_attribute_update(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id,
                                      esp_matter_attr_val_t *val);

/** Put the hood in its power-on state (fan off, light as per StartUpOnOff) and report it.
 *
 * Call after esp_matter::start().
 *
 * @return ESP_OK on success.
 * @return error in case of failure.
 */
esp_err_t app_driver_hood_set_defaults();

#if CHIP_DEVICE_CONFIG_ENABLE_THREAD
#define ESP_OPENTHREAD_DEFAULT_RADIO_CONFIG()                                           \
    {                                                                                   \
        .radio_mode = RADIO_MODE_NATIVE,                                                \
    }

#define ESP_OPENTHREAD_DEFAULT_HOST_CONFIG()                                            \
    {                                                                                   \
        .host_connection_mode = HOST_CONNECTION_MODE_NONE,                              \
    }

#define ESP_OPENTHREAD_DEFAULT_PORT_CONFIG()                                            \
    {                                                                                   \
        .storage_partition_name = "nvs", .netif_queue_size = 10, .task_queue_size = 10, \
    }
#endif
