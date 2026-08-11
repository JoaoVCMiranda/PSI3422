#include <zephyr.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include "motor.h"
#include "ultrasonic_sensor.h"

#define STACKSIZE 1024

#define D1_PIN 0
#define D2_PIN 1
#define D3_PIN 2
#define D4_PIN 3

#define TRIGGER_PIN 4
#define ECHO_PIN 5

#define DISTANCIA_MINIMA 0.3f // m

K_THREAD_STACK_DEFINE(stack_ultra, STACKSIZE);
K_THREAD_STACK_DEFINE(stack_run, STACKSIZE);

struct k_thread ultra_thread;
struct k_thread run_thread;

motor_t carrinho;
ultrasonic_sensor_t sensor;

void ultrassom(void)
{
    while (1)
    {
        ultrasonic_read(&sensor);
        k_msleep(50);
    }
}

void run(void)
{
    while (1)
    {
        if (sensor.distance <= DISTANCIA_MINIMA)
        {
            motor_right(&carrinho);
        }
        else
        {
            motor_forward(&carrinho);
        }

        k_msleep(10);
    }
}

void main(void)
{
    const struct device *gpioc = DEVICE_DT_GET(DT_NODELABEL(gpioc));
    const struct device *gpioa = DEVICE_DT_GET(DT_NODELABEL(gpioa));

    motor_init(&carrinho, gpioc, D1_PIN, D2_PIN, D3_PIN, D4_PIN);
    ultrasonic_init(&sensor, gpioa, TRIGGER_PIN, gpioa, ECHO_PIN);

    k_thread_create(&ultra_thread,
                    stack_ultra,
                    STACKSIZE,
                    ultrassom,
                    NULL, NULL, NULL,
                    1, 0, K_NO_WAIT);

    k_thread_create(&run_thread,
                    stack_run,
                    STACKSIZE,
                    run,
                    NULL, NULL, NULL,
                    2, 0, K_NO_WAIT);
}
