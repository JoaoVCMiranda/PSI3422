#include "ultrassom.h"

#include <errno.h>

#define TRIGGER_PULSE_US 10
#define ECHO_TIMEOUT_MS  30

/* distancia(m) = tempo_do_pulso(us) * velocidade_do_som(343 m/s) / 2 / 1e6 */
#define METROS_POR_MICROSSEGUNDO 0.0001715f

static void echo_isr(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(pins);

    ultrassom_t *sensor = CONTAINER_OF(cb, ultrassom_t, echo_cb);

    if (gpio_pin_get_dt(&sensor->echo)) {
        sensor->rise_cycles = k_cycle_get_32();
    } else {
        sensor->pulse_cycles = k_cycle_get_32() - sensor->rise_cycles;
        k_sem_give(&sensor->echo_sem);
    }
}

int ultrassom_init(ultrassom_t *sensor,
                    const struct gpio_dt_spec *trigger,
                    const struct gpio_dt_spec *echo)
{
    int ret;

    sensor->trigger = *trigger;
    sensor->echo = *echo;
    sensor->distance = 0.0f;

    if (!device_is_ready(sensor->trigger.port) || !device_is_ready(sensor->echo.port)) {
        return -ENODEV;
    }

    k_sem_init(&sensor->echo_sem, 0, 1);

    ret = gpio_pin_configure_dt(&sensor->trigger, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) {
        return ret;
    }

    ret = gpio_pin_configure_dt(&sensor->echo, GPIO_INPUT);
    if (ret < 0) {
        return ret;
    }

    gpio_init_callback(&sensor->echo_cb, echo_isr, BIT(sensor->echo.pin));

    ret = gpio_add_callback(sensor->echo.port, &sensor->echo_cb);
    if (ret < 0) {
        return ret;
    }

    return gpio_pin_interrupt_configure_dt(&sensor->echo, GPIO_INT_EDGE_BOTH);
}

float ultrassom_read(ultrassom_t *sensor)
{
    gpio_pin_set_dt(&sensor->trigger, 1);
    k_busy_wait(TRIGGER_PULSE_US);
    gpio_pin_set_dt(&sensor->trigger, 0);

    if (k_sem_take(&sensor->echo_sem, K_MSEC(ECHO_TIMEOUT_MS)) == 0) {
        uint32_t pulse_us = k_cyc_to_us_floor32(sensor->pulse_cycles);
        sensor->distance = pulse_us * METROS_POR_MICROSSEGUNDO;
    }

    return sensor->distance;
}
