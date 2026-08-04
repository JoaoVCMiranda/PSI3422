#include <zephyr.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include "h_ultrasonic.h"

#define STACKSIZE 1024
#define D1_PIN 0
#define D2_PIN 1
#define D3_PIN 2
#define D4_PIN 3

K_THREAD_STACK_DEFINE(stack_ultra, STACKSIZE);
K_THREAD_STACK_DEFINE(stack_run, STACKSIZE);

const struct device *gpio = DEVICE_DT_GET(DT_NODELABEL(gpioc));
struct k_thread ultra_thread;
struct k_thread run_thread;

volatile float distancia = 4.0f; //m
float max_distancia = 0.3f; // m

// Função de inicialização das GPIO
void gpio_init(void)
{
    gpio_pin_configure(gpio, 0, GPIO_OUTPUT);
    gpio_pin_configure(gpio, 1, GPIO_OUTPUT);
    gpio_pin_configure(gpio, 2, GPIO_OUTPUT);
    gpio_pin_configure(gpio, 3, GPIO_OUTPUT);
}

// Função de Set dos controles
void set_gpio(bool valor, uint8_t pin){
    gpio_pin_set(gpio, pin, valor);
}
// Função de encapsulamento das outras
void set_motor(uint8_t d1,
               uint8_t d2,
               uint8_t d3,
               uint8_t d4){
    set_gpio(d1,D1_PIN);
    set_gpio(d2,D2_PIN);
    set_gpio(d3,D3_PIN);
    set_gpio(d4,D4_PIN);
}
// Controle do carrinho
void w(void)
{
    set_motor(1,0,1,0);
}

void a(void)
{
    set_motor(0,0,1,0);
}

void d(void)
{
    set_motor(1,0,0,0);
}

void s(void)
{
    set_motor(1,1,1,1);
}

void pwm_init(void)
{
    create_trigger(TPM0, 1, 1000, 0, GPIOA, 3);
}

void pwm(uint16_t duty)
{
    pwm_tpm_CnV(TPM0, 1, duty);
}

void ultrassom_init(void)
{
    create_trigger(TPM0, 0, 40, 10, GPIOA, 4); // Trigger
}

void ultrassom(void)
{
    while (1)
    {
        uint16_t ticks = create_input(2, GPIOA, 5);

        distancia = ticks * FATOR_CONVERSAO;

        k_msleep(50);
    }
}

void run(void)
{
    while (1)
    {
        if (distancia <= max_distancia)
        {
            d();
        }
        else
        {
            w();
        }

        k_msleep(10);
    }
}



void main(void)
{
    gpio_init();
    pwm_init();
    ultrassom_init();

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
