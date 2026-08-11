#ifndef MOTOR_H_
#define MOTOR_H_

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

// Motor de carrinho controlado por ponte H (4 pinos digitais).
typedef struct {
    const struct device *gpio;
    uint8_t d1, d2, d3, d4;
} motor_t;

void motor_init(motor_t *motor, const struct device *gpio,
                 uint8_t d1, uint8_t d2, uint8_t d3, uint8_t d4);

void motor_forward(motor_t *motor);
void motor_left(motor_t *motor);
void motor_right(motor_t *motor);
void motor_stop(motor_t *motor);

#endif /* MOTOR_H_ */
