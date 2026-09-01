/*
 * PSI3422 — Exp2_PSI3422 / debug / JoystickCheck
 *
 * Firmware isolado só pra validar a aquisição ADC do joystick do
 * Controle (mesmos pinos/canais de ../../Controle/src/main.c):
 * X=PTB0 (ADC0_SE8), Y=PTB1 (ADC0_SE9), botão de freio=PTB2 (GPIO,
 * pull-up interno). Sem rádio, sem UART de comando — só lê e
 * imprime, pra isolar problema de hardware (ex.: eixo travado em
 * 3,3V/4095, já visto nesta bancada) de problema de lógica de
 * controle.
 *
 * Como usar: flashar no board do Controle, abrir serial monitor
 * (9600 8N1), mexer no joystick e observar os valores brutos (0..4095)
 * e o motor_l/motor_r que ../../Controle/src/main.c derivaria deles.
 */

#include <zephyr.h>
#include <device.h>
#include <drivers/gpio.h>
#include <drivers/adc.h>
#include <sys/printk.h>

#define JOYSTICK_ADC_RESOLUTION 12
#define JOYSTICK_CENTER         2048
#define JOYSTICK_DEADZONE       200
#define JOYSTICK_X_CHANNEL      8   /* PTB0 = ADC0_SE8 */
#define JOYSTICK_Y_CHANNEL      9   /* PTB1 = ADC0_SE9 */
#define JOYSTICK_BTN_PORT DEVICE_DT_GET(DT_NODELABEL(gpiob))
#define JOYSTICK_BTN_PIN  2   /* PTB2, botão de freio */

static const struct adc_channel_cfg x_cfg = {
    .gain             = ADC_GAIN_1,
    .reference        = ADC_REF_VDD_1,
    .acquisition_time = ADC_ACQ_TIME_DEFAULT,
    .channel_id       = JOYSTICK_X_CHANNEL,
    .differential     = 0,
};
static const struct adc_channel_cfg y_cfg = {
    .gain             = ADC_GAIN_1,
    .reference        = ADC_REF_VDD_1,
    .acquisition_time = ADC_ACQ_TIME_DEFAULT,
    .channel_id       = JOYSTICK_Y_CHANNEL,
    .differential     = 0,
};

static int16_t clamp_motor(int32_t v)
{
    if (v > INT16_MAX) return INT16_MAX;
    if (v < INT16_MIN) return INT16_MIN;
    return (int16_t)v;
}

/* mesma conta de ../../Controle/src/main.c: raw 0..4095 (centro ~2048)
 * -> -32768..32752, com zona morta central */
static int16_t adc_to_motor(int32_t raw)
{
    int32_t centered = raw - JOYSTICK_CENTER;
    if (centered > -JOYSTICK_DEADZONE && centered < JOYSTICK_DEADZONE) {
        return 0;
    }
    return clamp_motor(centered * 16);
}

void main(void)
{
    const struct device *adc = DEVICE_DT_GET(DT_NODELABEL(adc0));
    struct gpio_dt_spec btn = {
        .port = JOYSTICK_BTN_PORT, .pin = JOYSTICK_BTN_PIN, .dt_flags = GPIO_ACTIVE_LOW
    };

    if (!device_is_ready(adc) || !device_is_ready(btn.port)) {
        printk("ERRO: ADC ou GPIO do botao de freio indisponivel\n");
        return;
    }

    adc_channel_setup(adc, &x_cfg);
    adc_channel_setup(adc, &y_cfg);
    gpio_pin_configure_dt(&btn, GPIO_INPUT | GPIO_PULL_UP);

    printk("\n=== JoystickCheck -- validacao ADC do joystick ===\n");
    printk("X=PTB0 (ADC0_SE8), Y=PTB1 (ADC0_SE9), freio=PTB2\n");
    printk("Esperado: X/Y variando suavemente entre ~0 e ~4095 conforme\n");
    printk("o movimento do stick, parados perto de %d no centro.\n\n", JOYSTICK_CENTER);

    int16_t adc_buf;
    struct adc_sequence seq = {
        .buffer      = &adc_buf,
        .buffer_size = sizeof(adc_buf),
        .resolution  = JOYSTICK_ADC_RESOLUTION,
    };

    for (;;) {
        seq.channels = BIT(JOYSTICK_X_CHANNEL);
        int32_t x_raw = (adc_read(adc, &seq) == 0) ? adc_buf : -1;

        seq.channels = BIT(JOYSTICK_Y_CHANNEL);
        int32_t y_raw = (adc_read(adc, &seq) == 0) ? adc_buf : -1;

        bool pressed = gpio_pin_get_dt(&btn) > 0;

        int32_t throttle = adc_to_motor(y_raw);
        int32_t turn     = adc_to_motor(x_raw);
        int16_t ml = clamp_motor(throttle + turn);
        int16_t mr = clamp_motor(throttle - turn);

        printk("X=%4d Y=%4d freio=%d -> motor_l=%6d motor_r=%6d\n",
               x_raw, y_raw, pressed, ml, mr);

        k_msleep(200);
    }
}
