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
// Controles
void w(motor_t *motor);
void a(motor_t *motor);
void d(motor_t *motor);
void s(motor_t *motor);

#endif /* MOTOR_H_ */
