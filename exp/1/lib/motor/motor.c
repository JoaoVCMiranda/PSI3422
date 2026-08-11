#include "motor.h"

void motor_init(motor_t *motor, const struct device *gpio,
                 uint8_t d1, uint8_t d2, uint8_t d3, uint8_t d4)
{
    motor->gpio = gpio;
    motor->d1 = d1;
    motor->d2 = d2;
    motor->d3 = d3;
    motor->d4 = d4;

    gpio_pin_configure(gpio, d1, GPIO_OUTPUT);
    gpio_pin_configure(gpio, d2, GPIO_OUTPUT);
    gpio_pin_configure(gpio, d3, GPIO_OUTPUT);
    gpio_pin_configure(gpio, d4, GPIO_OUTPUT);
}

static void motor_set(motor_t *motor, uint8_t d1, uint8_t d2, uint8_t d3, uint8_t d4)
{
    gpio_pin_set(motor->gpio, motor->d1, d1);
    gpio_pin_set(motor->gpio, motor->d2, d2);
    gpio_pin_set(motor->gpio, motor->d3, d3);
    gpio_pin_set(motor->gpio, motor->d4, d4);
}

void w(motor_t *motor){ motor_set(motor, 1, 0, 1, 0); }

void a(motor_t *motor){ motor_set(motor, 0, 0, 1, 0); }

void d(motor_t *motor){ motor_set(motor, 1, 0, 0, 0); }

void s(motor_t *motor){ motor_set(motor, 1, 1, 1, 1); }
