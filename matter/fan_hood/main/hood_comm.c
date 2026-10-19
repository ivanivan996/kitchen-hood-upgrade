/*
 * Pulse-width telegram to the kitchen hood's fan controller.
 * Port of Code/kitchen-hood-upgrade/CommStack (Arduino) to ESP-IDF.
 *
 * The state machine and tick counts are kept identical to the original, which is known to work
 * with the controller: a 100 us timer tick, 3 x 16-bit packets sent MSB first, each preceded by
 * a preamble and followed by a pause, repeated forever.
 */

#include "hood_comm.h"

#include <driver/gpio.h>
#include <driver/gptimer.h>
#include <esp_attr.h>
#include <esp_check.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *TAG = "hood_comm";

#define TICK_US 100

#define FIRST_PULSE_HIGH_TIME 1920
#define FIRST_PULSE_LOW_TIME 1152
#define HIGH_PULSE_HIGH_TIME 1408
#define HIGH_PULSE_LOW_TIME 640
#define LOW_PULSE_HIGH_TIME 512
#define LOW_PULSE_LOW_TIME 1536
#define PAUSE_TIME 5640

static const uint32_t FIRST_PACKET_LED_OFF = 0b0000101000000000;
static const uint32_t SECOND_PACKET_LED_OFF = 0b0000111000000000;
static const uint32_t THIRD_PACKET_LED_OFF = 0b0001010000000000;

static const uint32_t FIRST_PACKET_LED_ON = 0b0000101100000000;
static const uint32_t SECOND_PACKET_LED_ON = 0b0001010100000101;
static const uint32_t THIRD_PACKET_LED_ON = 0b0000110000000101;

static const uint32_t SPEED_MASKS_LED_OFF[HOOD_SPEED_MAX + 1] = {
    0,
    (1 << 0) | (1 << 8),
    (1 << 1) | (1 << 8),
    (1 << 0) | (1 << 1) | (1 << 8),
    (1 << 0) | (1 << 2) | (1 << 8),
    (1 << 1) | (1 << 2) | (1 << 8),
    (1 << 0) | (1 << 1) | (1 << 2) | (1 << 8),
};

static const uint32_t SPEED_MASKS_LED_ON[HOOD_SPEED_MAX + 1] = {
    0,
    (1 << 0),
    (1 << 1),
    (1 << 0) | (1 << 1),
    (1 << 0) | (1 << 2),
    (1 << 1) | (1 << 2),
    (1 << 0) | (1 << 1) | (1 << 2),
};

typedef enum {
    DECIDE,
    SEND_PREAMBLE_HIGH,
    SEND_PREAMBLE_LOW,
    SEND_HIGH_HIGH,
    SEND_HIGH_LOW,
    SEND_LOW_HIGH,
    SEND_LOW_LOW,
    SEND_PAUSE,
} comm_state_t;

/* Written by hood_comm_set() inside a critical section, read by the ISR */
static volatile uint32_t s_telegram[3];

/* ISR-only state */
static gpio_num_t s_gpio;
static comm_state_t s_state = DECIDE;
static int32_t s_ticks;
static int32_t s_target_ticks;
static int32_t s_packet = 2;
static int32_t s_bit = 16;

static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static gptimer_handle_t s_timer;

static void IRAM_ATTR comm_cycle(void)
{
    s_ticks++;

    switch (s_state) {
    case DECIDE:
        if (s_bit == 16) {
            s_state = SEND_PREAMBLE_HIGH;
            s_ticks = 0;
            s_target_ticks = FIRST_PULSE_HIGH_TIME / TICK_US;
        } else if (s_bit == -1) {
            s_packet--;
            if (s_packet < 0) {
                s_packet = 2;
            }
            s_bit = 16;
            s_ticks = 0;
            s_target_ticks = PAUSE_TIME / TICK_US;
            s_state = SEND_PAUSE;
        } else if ((s_telegram[s_packet] >> s_bit) & 1) {
            s_ticks = 0;
            s_target_ticks = HIGH_PULSE_HIGH_TIME / TICK_US;
            s_state = SEND_HIGH_HIGH;
        } else {
            s_ticks = 0;
            s_target_ticks = LOW_PULSE_HIGH_TIME / TICK_US;
            s_state = SEND_LOW_HIGH;
        }
        break;

    case SEND_PAUSE:
        gpio_set_level(s_gpio, 0);
        if (s_ticks >= s_target_ticks) {
            s_state = DECIDE;
        }
        break;

    case SEND_PREAMBLE_HIGH:
        gpio_set_level(s_gpio, 1);
        if (s_ticks >= s_target_ticks) {
            s_ticks = 0;
            s_target_ticks = FIRST_PULSE_LOW_TIME / TICK_US;
            s_state = SEND_PREAMBLE_LOW;
        }
        break;

    case SEND_PREAMBLE_LOW:
        gpio_set_level(s_gpio, 0);
        if (s_ticks >= s_target_ticks) {
            s_bit--;
            s_state = DECIDE;
        }
        break;

    case SEND_HIGH_HIGH:
        gpio_set_level(s_gpio, 1);
        if (s_ticks >= s_target_ticks) {
            s_ticks = 0;
            s_target_ticks = HIGH_PULSE_LOW_TIME / TICK_US;
            s_state = SEND_HIGH_LOW;
        }
        break;

    case SEND_HIGH_LOW:
        gpio_set_level(s_gpio, 0);
        if (s_ticks >= s_target_ticks) {
            s_bit--;
            s_state = DECIDE;
        }
        break;

    case SEND_LOW_HIGH:
        gpio_set_level(s_gpio, 1);
        if (s_ticks >= s_target_ticks) {
            s_ticks = 0;
            s_target_ticks = LOW_PULSE_LOW_TIME / TICK_US;
            s_state = SEND_LOW_LOW;
        }
        break;

    case SEND_LOW_LOW:
        gpio_set_level(s_gpio, 0);
        if (s_ticks >= s_target_ticks) {
            s_bit--;
            s_state = DECIDE;
        }
        break;
    }
}

static bool IRAM_ATTR on_timer_alarm(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx)
{
    comm_cycle();
    return false;
}

void hood_comm_set(uint8_t speed, bool light)
{
    if (speed > HOOD_SPEED_MAX) {
        speed = HOOD_SPEED_MAX;
    }

    uint32_t t2, t1, t0;
    if (light) {
        t2 = FIRST_PACKET_LED_ON | SPEED_MASKS_LED_ON[speed];
        t1 = SECOND_PACKET_LED_ON;
        t0 = THIRD_PACKET_LED_ON;
    } else {
        t2 = FIRST_PACKET_LED_OFF | SPEED_MASKS_LED_OFF[speed];
        t1 = SECOND_PACKET_LED_OFF;
        t0 = THIRD_PACKET_LED_OFF;
    }

    /* Update all three packets together so the ISR never sends a mix of old and new state */
    portENTER_CRITICAL(&s_lock);
    s_telegram[2] = t2;
    s_telegram[1] = t1;
    s_telegram[0] = t0;
    portEXIT_CRITICAL(&s_lock);

    ESP_LOGI(TAG, "Telegram: speed %u, light %s", speed, light ? "on" : "off");
}

esp_err_t hood_comm_init(int gpio_num)
{
    s_gpio = (gpio_num_t)gpio_num;

    gpio_config_t io = {
        .pin_bit_mask = 1ULL << gpio_num,
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&io), TAG, "GPIO%d config failed", gpio_num);

    /* Same wake-up sequence as the original firmware */
    gpio_set_level(s_gpio, 1);
    vTaskDelay(pdMS_TO_TICKS(1000));
    gpio_set_level(s_gpio, 0);

    hood_comm_set(0, false);

    gptimer_config_t timer_cfg = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1000000,
    };
    ESP_RETURN_ON_ERROR(gptimer_new_timer(&timer_cfg, &s_timer), TAG, "timer create failed");

    gptimer_event_callbacks_t cbs = {
        .on_alarm = on_timer_alarm,
    };
    ESP_RETURN_ON_ERROR(gptimer_register_event_callbacks(s_timer, &cbs, NULL), TAG, "timer callback failed");

    gptimer_alarm_config_t alarm = {
        .alarm_count = TICK_US,
        .reload_count = 0,
        .flags.auto_reload_on_alarm = true,
    };
    ESP_RETURN_ON_ERROR(gptimer_set_alarm_action(s_timer, &alarm), TAG, "timer alarm failed");
    ESP_RETURN_ON_ERROR(gptimer_enable(s_timer), TAG, "timer enable failed");
    ESP_RETURN_ON_ERROR(gptimer_start(s_timer), TAG, "timer start failed");

    ESP_LOGI(TAG, "Telegram running on GPIO%d, %d us tick", gpio_num, TICK_US);
    return ESP_OK;
}
