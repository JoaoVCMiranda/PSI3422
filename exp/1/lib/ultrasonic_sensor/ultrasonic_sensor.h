#ifndef ULTRASONIC_SENSOR_H_
#define ULTRASONIC_SENSOR_H_

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

// Sensor ultrassônico (ex.: HC-SR04): trigger + echo via GPIO nativo do Zephyr.
typedef struct {
    const struct device *trigger_gpio;
    uint8_t trigger_pin;

    const struct device *echo_gpio;
    uint8_t echo_pin;

    struct gpio_callback echo_cb;
    struct k_sem echo_sem;

    volatile uint32_t rise_cycles;
    volatile uint32_t pulse_cycles;

    float distance; // metros, atualizado a cada ultrasonic_read()
} ultrasonic_sensor_t;

void ultrasonic_init(ultrasonic_sensor_t *sensor,
                      const struct device *trigger_gpio, uint8_t trigger_pin,
                      const struct device *echo_gpio, uint8_t echo_pin);

// Dispara o trigger e bloqueia até capturar o pulso de echo (ou 30ms de timeout).
// Atualiza e retorna sensor->distance.
float ultrasonic_read(ultrasonic_sensor_t *sensor);

#endif /* ULTRASONIC_SENSOR_H_ */
