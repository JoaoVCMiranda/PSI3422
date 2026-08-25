/*
 * PSI3422 — Exp2_PSI3422 / controle
 *
 * Base de controle: lê comandos de um teclado (via UART0/USB-serial,
 * `script/controle_serial.py` ou `cat`/`screen` no /dev/ttyACM0) e
 * reenvia por rádio nRF24L01+ para o carrinho; recebe telemetria de
 * volta e imprime uma linha legível por pacote. Ver ../Pinmap.md e
 * ../Carrinho/src/main.c para a explicação completa da arquitetura
 * híbrida (por que SPI é bare-metal aqui, não Zephyr nativo).
 *
 * Não usa pwm_z42 — este board não tem motor, só rádio + UART.
 */

#include <zephyr.h>
#include <device.h>
#include <drivers/gpio.h>
#include <drivers/uart.h>
#include <drivers/adc.h>
#include <sys/printk.h>
#include <string.h>

#include "nrf24.h"

/* ── Pinmap — mesmo rádio do carrinho, ver ../Pinmap.md ── */
#define RADIO_CE_PORT  DEVICE_DT_GET(DT_NODELABEL(gpioa))
#define RADIO_CE_PIN   13  /* PTA13 */
#define RADIO_CSN_PORT DEVICE_DT_GET(DT_NODELABEL(gpioc))
#define RADIO_CSN_PIN  4   /* PTC4 */
#define RADIO_IRQ_PORT DEVICE_DT_GET(DT_NODELABEL(gpiod))
#define RADIO_IRQ_PIN  2   /* PTD2 (suporta interrupção, pino D11) */

/* ── LED de Status ────────────────────────────────────────── */
#define LED_RED_PORT DEVICE_DT_GET(DT_NODELABEL(gpiob))
#define LED_RED_PIN  18  /* PTB18 */
#define LED_GREEN_PORT DEVICE_DT_GET(DT_NODELABEL(gpiob))
#define LED_GREEN_PIN  19  /* PTB19 */

/*
 * ── Joystick (ADC) ───────────────────────────────────────────
 * X=PTB0 (ADC0_SE8), Y=PTB1 (ADC0_SE9), freio=PTB2 (GPIO, botão).
 * Padrão de aquisição (adc_channel_cfg + adc_sequence de 1 canal,
 * lido sob demanda) copiado de PSI3441/entregas/4/src/main.c —
 * mesma placa/framework, só trocamos "acender LED" por "converter em
 * velocidade de motor".
 *
 * AVISO: nesta bancada o eixo do joystick já foi visto preso em
 * 3,3 V / 4095 (potenciômetro ou fiação com defeito). Com hardware
 * bom, zona morta em torno do centro evita drift; com hardware
 * defeituoso, ml/mr vão saturar e o joystick some com o controle por
 * UART — confira a leitura antes de confiar neste caminho.
 */
#define JOYSTICK_ADC_RESOLUTION 12
#define JOYSTICK_CENTER         2048
#define JOYSTICK_DEADZONE       200
#define JOYSTICK_X_CHANNEL      8   /* PTB0 = ADC0_SE8 */
#define JOYSTICK_Y_CHANNEL      9   /* PTB1 = ADC0_SE9 */
#define JOYSTICK_BTN_PORT DEVICE_DT_GET(DT_NODELABEL(gpiob))
#define JOYSTICK_BTN_PIN  2   /* PTB2, botão de freio */

static const struct adc_channel_cfg joystick_x_cfg = {
    .gain             = ADC_GAIN_1,
    .reference        = ADC_REF_VDD_1,
    .acquisition_time = ADC_ACQ_TIME_DEFAULT,
    .channel_id       = JOYSTICK_X_CHANNEL,
    .differential     = 0,
};
static const struct adc_channel_cfg joystick_y_cfg = {
    .gain             = ADC_GAIN_1,
    .reference        = ADC_REF_VDD_1,
    .acquisition_time = ADC_ACQ_TIME_DEFAULT,
    .channel_id       = JOYSTICK_Y_CHANNEL,
    .differential     = 0,
};

static const struct device *joystick_adc;
static struct gpio_dt_spec joystick_btn;
static bool joystick_hw_ready;
static int16_t joystick_adc_buf;
static struct adc_sequence joystick_seq = {
    .buffer      = &joystick_adc_buf,
    .buffer_size = sizeof(joystick_adc_buf),
    .resolution  = JOYSTICK_ADC_RESOLUTION,
};

#define VELOCIDADE_PADRAO 16000
#define VELOCIDADE_GIRO   12000

typedef struct {
    bool auto_mode;
    bool freio;
    int16_t motor_l;
    int16_t motor_r;
} control_cmd_t;

static void pack_cmd(uint8_t *payload, const control_cmd_t *cmd)
{
    memset(payload, 0, NRF24_MAX_PAYLOAD_SIZE);
    payload[0] = cmd->auto_mode ? 1 : 0;
    payload[1] = cmd->freio ? 1 : 0;
    payload[2] = (uint8_t)(cmd->motor_l & 0xFF);
    payload[3] = (uint8_t)(cmd->motor_l >> 8);
    payload[4] = (uint8_t)(cmd->motor_r & 0xFF);
    payload[5] = (uint8_t)(cmd->motor_r >> 8);
}

static void unpack_telemetry(const uint8_t *payload, uint16_t *dist_cm, int16_t *duty_l, int16_t *duty_r)
{
    *dist_cm = (uint16_t)(payload[0] | (payload[1] << 8));
    *duty_l = (int16_t)(payload[2] | (payload[3] << 8));
    *duty_r = (int16_t)(payload[4] | (payload[5] << 8));
}

static int16_t clamp_motor(int32_t v)
{
    if (v > INT16_MAX) return INT16_MAX;
    if (v < INT16_MIN) return INT16_MIN;
    return (int16_t)v;
}

/* raw 0..4095 (centro ~2048) -> -32768..32752, com zona morta central */
static int16_t adc_to_motor(int32_t raw)
{
    int32_t centered = raw - JOYSTICK_CENTER;
    if (centered > -JOYSTICK_DEADZONE && centered < JOYSTICK_DEADZONE) {
        return 0;
    }
    return clamp_motor(centered * 16);
}

/* Y = aceleração, X = curva; mixagem arcade (mesma convenção de key_to_cmd). */
static void joystick_read(int16_t *ml, int16_t *mr, bool *brk)
{
    if (!joystick_hw_ready) {
        *ml = 0;
        *mr = 0;
        *brk = false;
        return;
    }

    joystick_seq.channels = BIT(JOYSTICK_X_CHANNEL);
    int32_t x_raw = (adc_read(joystick_adc, &joystick_seq) == 0) ? joystick_adc_buf : JOYSTICK_CENTER;

    joystick_seq.channels = BIT(JOYSTICK_Y_CHANNEL);
    int32_t y_raw = (adc_read(joystick_adc, &joystick_seq) == 0) ? joystick_adc_buf : JOYSTICK_CENTER;

    int32_t throttle = adc_to_motor(y_raw);
    int32_t turn     = adc_to_motor(x_raw);

    *ml = clamp_motor(throttle + turn);
    *mr = clamp_motor(throttle - turn);
    *brk = gpio_pin_get_dt(&joystick_btn) > 0; /* GPIO_ACTIVE_LOW já inverte: pressionado = 1 */
}

/*
 * Traduz uma tecla em um novo estado de comando (mantido para compatibilidade com UART)
 */
static bool key_to_cmd(uint8_t key, control_cmd_t *cmd)
{
    switch (key) {
    case 'w': case 'W':
        cmd->auto_mode = false; cmd->freio = false;
        cmd->motor_l = VELOCIDADE_PADRAO; cmd->motor_r = VELOCIDADE_PADRAO;
        return true;
    case 's': case 'S':
        cmd->auto_mode = false; cmd->freio = false;
        cmd->motor_l = -VELOCIDADE_PADRAO; cmd->motor_r = -VELOCIDADE_PADRAO;
        return true;
    case 'a': case 'A':
        cmd->auto_mode = false; cmd->freio = false;
        cmd->motor_l = -VELOCIDADE_GIRO; cmd->motor_r = VELOCIDADE_GIRO;
        return true;
    case 'd': case 'D':
        cmd->auto_mode = false; cmd->freio = false;
        cmd->motor_l = VELOCIDADE_GIRO; cmd->motor_r = -VELOCIDADE_GIRO;
        return true;
    case 'q': case 'Q':
        cmd->auto_mode = false; cmd->freio = false;
        cmd->motor_l = 0; cmd->motor_r = 0;
        return true;
    case 'x': case 'X': case ' ':
        cmd->auto_mode = false; cmd->freio = true;
        return true;
    case 'o': case 'O':
        cmd->auto_mode = true; cmd->freio = false;
        return true;
    default:
        return false; /* tecla ignorada, mantém o comando atual */
    }
}

void main()
{
    const struct device *console = DEVICE_DT_GET(DT_NODELABEL(uart0));

    struct gpio_dt_spec ce  = { .port = RADIO_CE_PORT, .pin = RADIO_CE_PIN, .dt_flags = GPIO_ACTIVE_HIGH };
    struct gpio_dt_spec csn = { .port = RADIO_CSN_PORT, .pin = RADIO_CSN_PIN, .dt_flags = GPIO_ACTIVE_LOW };
    struct gpio_dt_spec irq = { .port = RADIO_IRQ_PORT, .pin = RADIO_IRQ_PIN, .dt_flags = GPIO_ACTIVE_LOW };

    struct gpio_dt_spec led_red = { .port = LED_RED_PORT, .pin = LED_RED_PIN, .dt_flags = GPIO_ACTIVE_LOW };
    struct gpio_dt_spec led_green = { .port = LED_GREEN_PORT, .pin = LED_GREEN_PIN, .dt_flags = GPIO_ACTIVE_LOW };
    if (device_is_ready(led_red.port)) {
        gpio_pin_configure_dt(&led_red, GPIO_OUTPUT_INACTIVE);
        gpio_pin_configure_dt(&led_green, GPIO_OUTPUT_INACTIVE);
    }

    joystick_btn = (struct gpio_dt_spec){
        .port = JOYSTICK_BTN_PORT, .pin = JOYSTICK_BTN_PIN, .dt_flags = GPIO_ACTIVE_LOW
    };
    joystick_adc = DEVICE_DT_GET(DT_NODELABEL(adc0));
    joystick_hw_ready = device_is_ready(joystick_adc) && device_is_ready(joystick_btn.port);
    if (joystick_hw_ready) {
        gpio_pin_configure_dt(&joystick_btn, GPIO_INPUT | GPIO_PULL_UP);
        adc_channel_setup(joystick_adc, &joystick_x_cfg);
        adc_channel_setup(joystick_adc, &joystick_y_cfg);
    } else {
        printk("AVISO: ADC/GPIO do joystick indisponivel, so UART vai funcionar\n");
    }

    printk("\n==================================\n");
    printk("=== BOOTING CONTROLE APP       ===\n");
    printk("==================================\n");

    int ret = nrf24_init(&ce, &csn, &irq);
    if (ret < 0) {
        printk("ERRO: nrf24_init = %d\n", ret);
        // Do not return, let it loop so we don't just die
        // return;
    }


    printk("\n=== PSI3422 Exp2_PSI3422 -- controle ===\n");
    printk("Joystick ativado (PTB0=X, PTB1=Y, PTB2=Botao de Freio)\n");
    printk("UART fallback: w/a/s/d = mover, x/espaco = frear, q = ponto morto, o = modo automatico\n\n");

    control_cmd_t cmd = { .auto_mode = true }; /* começa em modo seguro */

    for (;;) {
        uint8_t key;

        if (uart_poll_in(console, &key) == 0) {
            key_to_cmd(key, &cmd);
        } else {
            // Read joystick if no UART input
            int16_t ml = 0;
            int16_t mr = 0;
            bool brk = false;

            joystick_read(&ml, &mr, &brk);

            if (brk || ml != 0 || mr != 0) {
                // Joystick takes priority if it's being used
                cmd.auto_mode = false;
                cmd.freio = brk;
                cmd.motor_l = ml;
                cmd.motor_r = mr;
            } else if (!cmd.auto_mode && !cmd.freio && cmd.motor_l == 0 && cmd.motor_r == 0) {
                // If joystick is idle and cmd is idle, make sure freio is updated correctly
                // or keep the last command (e.g. from UART).
            } else {
                // Joystick is idle, allow UART commands to persist
                // If we want joystick to override and stop when released, we can do this:
                cmd.auto_mode = false;
                cmd.freio = brk;
                cmd.motor_l = ml;
                cmd.motor_r = mr;
            }
        }

        uint8_t tx_payload[NRF24_MAX_PAYLOAD_SIZE];
        pack_cmd(tx_payload, &cmd);
        nrf24_send(tx_payload, sizeof(tx_payload)); /* melhor esforço */

        uint8_t rx_payload[NRF24_MAX_PAYLOAD_SIZE];
        ret = nrf24_receive(rx_payload, sizeof(rx_payload), K_MSEC(100));
        static int64_t radio_lost_time = 0;
        
        static bool handshake = false;
        
        if (ret >= 0) {
            uint16_t dist_cm;
            int16_t duty_l, duty_r;

            unpack_telemetry(rx_payload, &dist_cm, &duty_l, &duty_r);
            printk("dist=%3ucm dutyL=%5d dutyR=%5d\n", dist_cm, duty_l, duty_r);
            
            radio_lost_time = 0;
            handshake = true;
            gpio_pin_set_dt(&led_red, 0);
            gpio_pin_set_dt(&led_green, 1);
        } else {
            gpio_pin_set_dt(&led_red, 1);
            gpio_pin_set_dt(&led_green, 0);
            
            if (handshake) {
                if (radio_lost_time == 0) {
                    radio_lost_time = k_uptime_get();
                } else if (k_uptime_get() - radio_lost_time > 2000) {
                    printk("Reconectando modulo NRF24 do Controle...\n");
                    nrf24_init(&ce, &csn, &irq);
                    radio_lost_time = k_uptime_get(); // Tenta de novo em 2s
                }
            } else {
                radio_lost_time = 0;
            }
        }    
            // Print joystick command periodically to verify main loop is alive
            static int loop_cnt = 0;
            if (++loop_cnt % 25 == 0) { // roughly every 500ms
                printk("heartbeat: tx_cmd = %d %d %d %d\n", cmd.auto_mode, cmd.freio, cmd.motor_l, cmd.motor_r);
            }

        k_msleep(20);
    }
}
