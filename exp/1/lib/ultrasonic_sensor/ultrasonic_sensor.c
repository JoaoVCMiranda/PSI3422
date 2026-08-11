#include "ultrasonic_sensor.h"

#define TRIGGER_PULSE_US 10
#define ECHO_TIMEOUT_MS 30

// distancia(m) = tempo_do_pulso(us) * velocidade_do_som(343 m/s) / 2 / 1e6
#define METROS_POR_MICROSSEGUNDO 0.0001715f

static void echo_isr(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(pins);

    ultrasonic_sensor_t *sensor = CONTAINER_OF(cb, ultrasonic_sensor_t, echo_cb);

    if (gpio_pin_get(sensor->echo_gpio, sensor->echo_pin)) {
        sensor->rise_cycles = k_cycle_get_32();
    } else {
        sensor->pulse_cycles = k_cycle_get_32() - sensor->rise_cycles;
        k_sem_give(&sensor->echo_sem);
    }
}

void ultrasonic_init(ultrasonic_sensor_t *sensor,
                      const struct device *trigger_gpio, uint8_t trigger_pin,
                      const struct device *echo_gpio, uint8_t echo_pin)
{
    sensor->trigger_gpio = trigger_gpio;
    sensor->trigger_pin = trigger_pin;
    sensor->echo_gpio = echo_gpio;
    sensor->echo_pin = echo_pin;
    sensor->distance = 0.0f;

    k_sem_init(&sensor->echo_sem, 0, 1);

    gpio_pin_configure(trigger_gpio, trigger_pin, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure(echo_gpio, echo_pin, GPIO_INPUT);

    gpio_init_callback(&sensor->echo_cb, echo_isr, BIT(echo_pin));
    gpio_add_callback(echo_gpio, &sensor->echo_cb);
    gpio_pin_interrupt_configure(echo_gpio, echo_pin, GPIO_INT_EDGE_BOTH);
}

float ultrasonic_read(ultrasonic_sensor_t *sensor)
{
    gpio_pin_set(sensor->trigger_gpio, sensor->trigger_pin, 1);
    k_busy_wait(TRIGGER_PULSE_US);
    gpio_pin_set(sensor->trigger_gpio, sensor->trigger_pin, 0);

    if (k_sem_take(&sensor->echo_sem, K_MSEC(ECHO_TIMEOUT_MS)) == 0) {
        uint32_t pulse_us = k_cyc_to_us_floor32(sensor->pulse_cycles);
        sensor->distance = pulse_us * METROS_POR_MICROSSEGUNDO;
    }

    return sensor->distance;
}
