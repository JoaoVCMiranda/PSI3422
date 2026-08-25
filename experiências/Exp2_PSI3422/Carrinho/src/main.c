/*
 * PSI3422 — Exp2_PSI3422 / carrinho
 *
 * Carrinho com 2 motores DC (ponte H dupla), sensor ultrassônico
 * HC-SR04 e rádio nRF24L01+ para receber comandos e devolver
 * telemetria. Board: FRDM-KL25Z. Ver ../Pinmap.md para o pinmap
 * completo e a justificativa de cada escolha de pino.
 *
 * ── Arquitetura híbrida: o que é Zephyr nativo e o que é bare-metal, e por quê ──
 *
 * GPIO (direção dos motores, trigger/echo do ultrassom, CE/CSN/IRQ do
 * rádio): API nativa do Zephyr (`gpio_dt_spec`, `gpio_pin_*_dt`).
 * Funciona sem ressalvas neste board/framework — gpioa/b/c/d/e são
 * nós reais e funcionais da devicetree deste port
 * (dts/arm/nxp/nxp_kl25z.dtsi), só gpioc/gpioe não vêm habilitados
 * por padrão (por isso o overlay em boards/frdm_kl25z.overlay liga
 * `status = "okay"` para eles).
 *
 * PWM dos motores (enA/enB — velocidade): biblioteca `pwm_z42` do
 * Prof. Gustavo Rehder (bare-metal, acesso direto a registrador TPM
 * via CMSIS/MKL25Z4.h), NÃO a API Zephyr de PWM
 * (`<zephyr/drivers/pwm.h>`, `pwm_dt_spec`). Motivo verificado nesta
 * árvore de build: procurando em
 * `~/.platformio/packages/framework-zephyr/dts/arm/nxp/nxp_kl25z.dtsi`
 * não existe NENHUM nó de TPM — só flash, mcg, i2c0/i2c1, sim, uart0,
 * adc0, pinmux (porta-e) e gpioa-e. Sem nó TPM não há como obter um
 * `pwm_dt_spec`: a API Zephyr de PWM simplesmente não é uma opção
 * neste framework, não é só uma questão de o motor só precisar variar
 * duty (o que, em teoria, a API nativa cobriria — ver comentário mais
 * longo no topo de lib/motor/motor.c). pwm_z42 funciona porque não
 * depende do modelo de device do Zephyr.
 *
 * SPI do rádio: biblioteca `spi.c` (mesmo autor de pwm_z42, também
 * bare-metal), NÃO `spi_dt_spec`/`spi_transceive_dt` do Zephyr — pelo
 * mesmo motivo: nenhum nó SPI existe na devicetree deste framework.
 * Uma versão anterior do `nrf24.c` dependia de um nó devicetree
 * customizado `nrf24` que nunca chegou a ser declarado em overlay
 * nenhum do repositório — e mesmo se fosse, não teria `&spi0` para
 * apontar. Reescrito para chamar `spi_send`/`spi_read` diretamente
 * (ver ../../../lib/nrf24/nrf24.c).
 *
 * ── Por que não duas threads (rádio + controle) ──
 *
 * Uma versão anterior deste MVP usava uma thread dedicada bloqueada
 * em `nrf24_receive()` mais um mutex para o comando compartilhado.
 * Isso criava uma corrida real: `nrf24_send()` (usado para telemetria)
 * e `nrf24_receive()` mexem no mesmo estado do rádio (CE, registrador
 * CONFIG, semáforo de IRQ) — chamar os dois de threads diferentes sem
 * seção crítica podia interromper uma recepção em andamento. A solução
 * mais simples e correta para um rádio único e half-duplex: um laço
 * só, síncrono — recebe (com timeout) → aplica controle → responde
 * com telemetria → repete. Sem mutex, sem corrida.
 */

#include <zephyr.h>
#include <device.h>
#include <drivers/gpio.h>
#include <sys/printk.h>

#include "pwm_z42.h"
#include "motor.h"
#include "ultrassom.h"
#include "nrf24.h"
#include "control_fsm.h"
#include "../../protocol.h"

/* ── Pinmap — ver ../Pinmap.md ─────────── */
#define MOTOR_L_IN1_PORT DEVICE_DT_GET(DT_NODELABEL(gpioc))
#define MOTOR_L_IN1_PIN  8   /* PTC8 */
#define MOTOR_L_IN2_PIN  9   /* PTC9 */
#define MOTOR_L_ENA_GPIO GPIOA
#define MOTOR_L_ENA_PIN  4   /* PTA4 = TPM0_CH1 */
#define MOTOR_L_ENA_CH   1

#define MOTOR_R_IN1_PORT DEVICE_DT_GET(DT_NODELABEL(gpioa))
#define MOTOR_R_IN1_PIN  12  /* PTA12 */
#define MOTOR_R_IN2_PORT DEVICE_DT_GET(DT_NODELABEL(gpiod))
#define MOTOR_R_IN2_PIN  5   /* PTD5 */
#define MOTOR_R_ENB_GPIO GPIOA
#define MOTOR_R_ENB_PIN  5   /* PTA5 = TPM0_CH2 */
#define MOTOR_R_ENB_CH   2

#define ULTRASSOM_PORT   DEVICE_DT_GET(DT_NODELABEL(gpiod))
#define ULTRASSOM_TRIG_PIN 0 /* PTD0 */
#define ULTRASSOM_ECHO_PIN 4 /* PTD4 */

#define RADIO_CE_PORT  DEVICE_DT_GET(DT_NODELABEL(gpioa))
#define RADIO_CE_PIN   13  /* PTA13 */
#define RADIO_CSN_PORT DEVICE_DT_GET(DT_NODELABEL(gpioc))
#define RADIO_CSN_PIN  4   /* PTC4 — CS manual; SCK/MOSI/MISO (PTC5/6/7) ficam dentro de spi_init() */
#define RADIO_IRQ_PORT DEVICE_DT_GET(DT_NODELABEL(gpiod))
#define RADIO_IRQ_PIN  2   /* PTD2 (suporta interrupção, pino D11) */


/* ── LED de Status (Resolvido sem macros excessivas) ─────────────────── */
#define LED_RED_PORT DEVICE_DT_GET(DT_NODELABEL(gpiob))
#define LED_RED_PIN  18  /* PTB18 (Active Low) */
#define LED_GREEN_PORT DEVICE_DT_GET(DT_NODELABEL(gpiob))
#define LED_GREEN_PIN  19  /* PTB19 (Active Low) */


/* TPM0 compartilhado pelos 2 canais de PWM dos motores: MCGIRCLK (4MHz,
 * independente do PLL) / PS_1 -> f_tpm = 4MHz; MOD=3999 -> f_pwm = 1kHz
 * (audível mas comum e seguro para motor DC via L298N). */
#define TPM_MOTOR_MOD 3999U

static motor_t motor_l;
static motor_t motor_r;
static ultrassom_t sensor;

void main()
{
    struct gpio_dt_spec l_in1 = { .port = MOTOR_L_IN1_PORT, .pin = MOTOR_L_IN1_PIN, .dt_flags = GPIO_ACTIVE_HIGH };
    struct gpio_dt_spec l_in2 = { .port = MOTOR_L_IN1_PORT, .pin = MOTOR_L_IN2_PIN, .dt_flags = GPIO_ACTIVE_HIGH };
    struct gpio_dt_spec r_in1 = { .port = MOTOR_R_IN1_PORT, .pin = MOTOR_R_IN1_PIN, .dt_flags = GPIO_ACTIVE_HIGH };
    struct gpio_dt_spec r_in2 = { .port = MOTOR_R_IN2_PORT, .pin = MOTOR_R_IN2_PIN, .dt_flags = GPIO_ACTIVE_HIGH };
    struct gpio_dt_spec trig  = { .port = ULTRASSOM_PORT, .pin = ULTRASSOM_TRIG_PIN, .dt_flags = GPIO_ACTIVE_HIGH };
    struct gpio_dt_spec echo  = { .port = ULTRASSOM_PORT, .pin = ULTRASSOM_ECHO_PIN, .dt_flags = GPIO_ACTIVE_HIGH };
    struct gpio_dt_spec ce    = { .port = RADIO_CE_PORT, .pin = RADIO_CE_PIN, .dt_flags = GPIO_ACTIVE_HIGH };
    struct gpio_dt_spec csn   = { .port = RADIO_CSN_PORT, .pin = RADIO_CSN_PIN, .dt_flags = GPIO_ACTIVE_LOW };
    struct gpio_dt_spec irq   = { .port = RADIO_IRQ_PORT, .pin = RADIO_IRQ_PIN, .dt_flags = GPIO_ACTIVE_LOW };
    
    struct gpio_dt_spec led_red = { .port = LED_RED_PORT, .pin = LED_RED_PIN, .dt_flags = GPIO_ACTIVE_LOW };
    struct gpio_dt_spec led_green = { .port = LED_GREEN_PORT, .pin = LED_GREEN_PIN, .dt_flags = GPIO_ACTIVE_LOW };
    if (device_is_ready(led_red.port)) {
        gpio_pin_configure_dt(&led_red, GPIO_OUTPUT_INACTIVE);
        gpio_pin_configure_dt(&led_green, GPIO_OUTPUT_INACTIVE);
    }
    int ret;

    /* TPM0 uma vez só — MOD (período) é compartilhado por todos os canais */
    if (!pwm_tpm_Init(TPM0, TPM_MCGIRCLK, TPM_MOTOR_MOD, TPM_CLK, PS_1, EDGE_PWM)) {
        printk("ERRO: pwm_tpm_Init(TPM0) falhou\n");
        return;
    }
    if (!pwm_tpm_Ch_Init(TPM0, MOTOR_L_ENA_CH, TPM_PWM_H, MOTOR_L_ENA_GPIO, MOTOR_L_ENA_PIN)) {
        printk("ERRO: pwm_tpm_Ch_Init(motor L) falhou\n");
        return;
    }
    if (!pwm_tpm_Ch_Init(TPM0, MOTOR_R_ENB_CH, TPM_PWM_H, MOTOR_R_ENB_GPIO, MOTOR_R_ENB_PIN)) {
        printk("ERRO: pwm_tpm_Ch_Init(motor R) falhou\n");
        return;
    }

    ret = motor_init(&motor_l, &l_in1, &l_in2, TPM0, MOTOR_L_ENA_CH, TPM_MOTOR_MOD);
    if (ret < 0) { printk("ERRO: motor_init(L) = %d\n", ret); return; }

    ret = motor_init(&motor_r, &r_in1, &r_in2, TPM0, MOTOR_R_ENB_CH, TPM_MOTOR_MOD);
    if (ret < 0) { printk("ERRO: motor_init(R) = %d\n", ret); return; }

    ret = ultrassom_init(&sensor, &trig, &echo);
    if (ret < 0) { printk("ERRO: ultrassom_init = %d\n", ret); return; }

    printk("\n==================================\n");
    printk("=== BOOTING CARRINHO APP       ===\n");
    printk("==================================\n");

    ret = nrf24_init(&ce, &csn, &irq);
    if (ret < 0) {
        printk("ERRO: rádio falhou = %d. Carrinho continuará sem rádio.\n", ret);
    }

    printk("PSI3422 Exp2_PSI3422 — carrinho pronto\n");

    radio_cmd_t cmd = { .auto_mode = 1 }; /* começa em modo seguro */
    control_fsm_heartbeat();

    for (;;) {
        /*
         * O rádio é configurado (nrf24_init) com payload fixo de
         * NRF24_MAX_PAYLOAD_SIZE (32) nos dois lados — nrf24_receive()
         * exige um buffer de pelo menos esse tamanho e sempre lê os
         * 32 bytes inteiros, então o buffer de rádio continua sendo
         * uint8_t[32]; radio_cmd_t/radio_telemetry_t (protocol.h) só
         * descrevem os bytes que de fato importam no início dele.
         */
        uint8_t rx_payload[NRF24_MAX_PAYLOAD_SIZE];
        uint8_t tx_payload[NRF24_MAX_PAYLOAD_SIZE] = {0};

        ret = nrf24_receive(rx_payload, sizeof(rx_payload), K_MSEC(100));
        if (ret >= 0) {
            cmd = *(const radio_cmd_t *)rx_payload;
            control_fsm_heartbeat();
            
            static int rx_cnt = 0;
            if (++rx_cnt % 10 == 0) {
                printk("-> recebido: auto=%d freio=%d motL=%d motR=%d\n", cmd.auto_mode, cmd.freio, cmd.motor_l, cmd.motor_r);
            }
        }

        /* ── Controle dos LEDs de Status ── */
        /* O watchdog retorna true se não tiver controle (timeout) */
        bool sem_radio = control_fsm_watchdog(&cmd); /* sem comando há > 500ms -> força auto_mode */
        gpio_pin_set_dt(&led_red, sem_radio ? 1 : 0);
        gpio_pin_set_dt(&led_green, sem_radio ? 0 : 1);
        
        // Se ficar 2 segundos sem rádio (após handshake), tenta reiniciar o módulo
        static int64_t radio_lost_time = 0;
        static bool handshake = false;
        
        if (!sem_radio) {
            handshake = true;
            radio_lost_time = 0;
        } else if (handshake) {
            if (radio_lost_time == 0) {
                radio_lost_time = k_uptime_get();
            } else if (k_uptime_get() - radio_lost_time > 2000) {
                printk("Reconectando modulo NRF24 do Carrinho...\n");
                nrf24_init(&ce, &csn, &irq);
                radio_lost_time = k_uptime_get(); // Tenta de novo em 2s
            }
        }
        
        /* ── Atualizar Sensor Ultrassom ── */
        ultrassom_read(&sensor); // Atualiza sensor->distance
        
        control_fsm_apply(&cmd, &motor_l, &motor_r, &sensor);

        uint16_t dist = (uint16_t)(sensor.distance * 100.0f);
        int16_t out_l = (int16_t)(motor_get_duty(&motor_l) * 1000.0f);
        int16_t out_r = (int16_t)(motor_get_duty(&motor_r) * 1000.0f);

        *(radio_telemetry_t *)tx_payload = (radio_telemetry_t){
            .dist_cm = dist, .duty_l = out_l, .duty_r = out_r,
        };
        nrf24_send(tx_payload, sizeof(tx_payload));

        static int loop_cnt = 0;
        if (++loop_cnt % 10 == 0) { // roughly every 500ms
            printk("car heartbeat: dist=%dcm cmd.auto=%d outL=%d outR=%d (sem_radio=%d)\n", 
                   dist, cmd.auto_mode, out_l, out_r, sem_radio);
        }

        k_msleep(50);
    }
}
