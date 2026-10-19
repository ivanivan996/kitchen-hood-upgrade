/*
   Kitchen hood driver: maps the Matter Fan / On-Off Light endpoints to the hood telegram and
   handles the hood's physical buttons.
*/

#include <esp_log.h>
#include <stdlib.h>

#include <app_priv.h>
#include <common_macros.h>
#include <esp_check.h>
#include <esp_matter.h>

#include <button_gpio.h>
#include <device.h>

#include <app/clusters/fan-control-server/CodegenIntegration.h>

#include "hood_comm.h"

using namespace chip::app::Clusters;
using namespace chip::app::Clusters::FanControl;
using namespace esp_matter;
using chip::Protocols::InteractionModel::Status;

static const char *TAG = "app_driver";
extern uint16_t fan_endpoint_id;
extern uint16_t light_endpoint_id;

/* What is currently being transmitted */
static uint8_t s_speed = 0;
static bool s_light = false;

/* Controllers work in percent; the hood has HOOD_SPEED_MAX steps. Rounding keeps the round trip stable
 * (step 1 -> 16 % -> step 1, etc.), and any non-zero percent runs the fan at least at step 1. */
static uint8_t percent_to_speed(uint8_t percent)
{
    if (percent == 0) {
        return 0;
    }
    uint8_t speed = (percent * HOOD_SPEED_MAX + 50) / 100;
    return speed ? speed : 1;
}

static void hood_apply()
{
    hood_comm_set(s_speed, s_light);
}

/* The fan runs at PercentSetting, or stops when FanMode is Off */
static uint8_t fan_target_percent(FanModeEnum mode, const chip::app::DataModel::Nullable<chip::Percent> &setting)
{
    if (mode == FanModeEnum::kOff || setting.IsNull()) {
        return 0;
    }
    return setting.Value();
}

/* Drive the motor and report the actual speed back (PercentCurrent / SpeedCurrent are app-owned) */
static void fan_apply(FanControlCluster *cluster, uint8_t percent)
{
    s_speed = percent_to_speed(percent);
    hood_apply();

    if (cluster) {
        cluster->SetPercentCurrent(percent);
        cluster->SetSpeedCurrent(s_speed);
    }
}

class HoodFanDelegate : public Delegate {
public:
    HoodFanDelegate() : Delegate(chip::kInvalidEndpointId) {}

    Status HandleStep(StepDirectionEnum aDirection, bool aWrap, bool aLowestOff) override
    {
        /* Step feature is not enabled on this endpoint */
        return Status::UnsupportedCommand;
    }

    void OnFanDriveStateChanged(const FanDriveState &newState) override
    {
        fan_apply(FindClusterOnEndpoint(fan_endpoint_id), fan_target_percent(newState.mode, newState.percentSetting));
    }
};

static HoodFanDelegate s_fan_delegate;

Delegate *app_driver_fan_get_delegate()
{
    return &s_fan_delegate;
}

esp_err_t app_driver_attribute_update(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id,
                                      esp_matter_attr_val_t *val)
{
    if (endpoint_id == light_endpoint_id && cluster_id == OnOff::Id && attribute_id == OnOff::Attributes::OnOff::Id) {
        s_light = val->val.b;
        hood_apply();
    }
    return ESP_OK;
}

esp_err_t app_driver_hood_set_defaults()
{
    lock::ScopedChipStackLock lock(portMAX_DELAY);

    /* A hood should not start blowing after a power cut: always boot with the fan off */
    FanControlCluster *cluster = FindClusterOnEndpoint(fan_endpoint_id);
    ESP_RETURN_ON_FALSE(cluster, ESP_ERR_INVALID_STATE, TAG, "FanControl cluster not found on endpoint %u", fan_endpoint_id);
    cluster->SetFanMode(FanModeEnum::kOff);
    fan_apply(cluster, 0);

    /* Light follows its OnOff attribute (StartUpOnOff is set to Off in app_main) */
    esp_matter_attr_val_t val;
    attribute_t *attribute = attribute::get(light_endpoint_id, OnOff::Id, OnOff::Attributes::OnOff::Id);
    ESP_RETURN_ON_FALSE(attribute, ESP_ERR_INVALID_STATE, TAG, "OnOff attribute not found on endpoint %u", light_endpoint_id);
    attribute::get_val(attribute, &val);
    s_light = val.val.b;
    hood_apply();
    return ESP_OK;
}

/* Button actions go through the data model, so controllers see the change and the normal
 * delegate / attribute callback path drives the hood. */
static void button_speed_step(int delta)
{
    lock::ScopedChipStackLock lock(portMAX_DELAY);
    FanControlCluster *cluster = FindClusterOnEndpoint(fan_endpoint_id);
    if (!cluster) {
        return;
    }

    int next = s_speed + delta;
    if (next < 0 || next > HOOD_SPEED_MAX) {
        return;
    }
    ESP_LOGI(TAG, "Button: speed %u -> %d", s_speed, next);

    Status status = cluster->SetSpeedSetting(chip::app::DataModel::MakeNullable(static_cast<uint8_t>(next)));
    if (status != Status::Success) {
        ESP_LOGE(TAG, "SetSpeedSetting failed: 0x%02x", chip::to_underlying(status));
    }
}

static void button_up_cb(void *arg, void *data)
{
    button_speed_step(+1);
}

static void button_down_cb(void *arg, void *data)
{
    button_speed_step(-1);
}

static void button_light_cb(void *arg, void *data)
{
    lock::ScopedChipStackLock lock(portMAX_DELAY);
    esp_matter_attr_val_t val = esp_matter_bool(!s_light);
    ESP_LOGI(TAG, "Button: light %s", val.val.b ? "on" : "off");
    attribute::update(light_endpoint_id, OnOff::Id, OnOff::Attributes::OnOff::Id, &val);
}

static void hood_button_register(int gpio_num, button_cb_t cb)
{
    if (gpio_num < 0) {
        return;
    }

    /* Buttons pull the pin to GND, as with INPUT_PULLUP in the Arduino firmware */
    const button_config_t btn_cfg = {0};
    button_gpio_config_t gpio_cfg = {};
    gpio_cfg.gpio_num = gpio_num;
    gpio_cfg.active_level = 0;

    button_handle_t handle = NULL;
    if (iot_button_new_gpio_device(&btn_cfg, &gpio_cfg, &handle) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create button on GPIO%d", gpio_num);
        return;
    }
    iot_button_register_cb(handle, BUTTON_PRESS_DOWN, NULL, cb, NULL);
}

esp_err_t app_driver_hood_init()
{
    ESP_RETURN_ON_ERROR(hood_comm_init(CONFIG_HOOD_COMM_GPIO), TAG, "hood telegram init failed");

    hood_button_register(CONFIG_HOOD_BUTTON_LIGHT_GPIO, button_light_cb);
    hood_button_register(CONFIG_HOOD_BUTTON_UP_GPIO, button_up_cb);
    hood_button_register(CONFIG_HOOD_BUTTON_DOWN_GPIO, button_down_cb);
    return ESP_OK;
}

app_driver_handle_t app_driver_button_init()
{
    /* BOOT button: only used for factory reset via app_reset_button_register() */
    button_handle_t handle = NULL;
    const button_config_t btn_cfg = {0};
    const button_gpio_config_t btn_gpio_cfg = button_driver_get_config();

    if (iot_button_new_gpio_device(&btn_cfg, &btn_gpio_cfg, &handle) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create button device");
        return NULL;
    }
    return (app_driver_handle_t)handle;
}
