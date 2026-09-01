# Pinmap — projeto_final (FRDM-KL25Z)

GERADO por `tools/gen_pinmap.py` a partir de `pinmap.yaml` — não
editar à mão, editar o YAML e rodar o gerador de novo. Substitui a
manutenção manual de `experiências/Exp2_PSI3422/Pinmap.md` e
`experiências/Exp4_PSI3422/pinmap.md` (quase duplicados) por uma
fonte única, que também gera `pinmap.h` e o símbolo KiCad.

## Carrinho

| Sinal | Pino | Tipo/Direção | Observação |
|---|---|---|---|
| MOTOR_L_IN1 | PTC8 | saida | Ponte H, direção do motor esquerdo (lib/motor) |
| MOTOR_L_IN2 | PTC9 | saida | Ponte H, direção do motor esquerdo (lib/motor) |
| MOTOR_L_ENA | PTA4 | pwm | TPM0_CH1, velocidade do motor esquerdo, via lib/pwm_z42 |
| MOTOR_R_IN1 | PTA12 | saida | Ponte H, direção do motor direito (lib/motor) |
| MOTOR_R_IN2 | PTD5 | saida | Ponte H, direção do motor direito (lib/motor) |
| MOTOR_R_ENB | PTA5 | pwm | TPM0_CH2, velocidade do motor direito, via lib/pwm_z42 |
| ULTRASSOM_TRIG | PTD0 | saida | HC-SR04 trigger (lib/ultrassom) |
| ULTRASSOM_ECHO | PTD4 | entrada_irq | HC-SR04 echo, interrupção de borda (lib/ultrassom) |
| ENCODER_L | PTD1 | entrada_irq | IR HW-201 esquerda (lib/encoder) |
| ENCODER_R | PTD3 | entrada_irq | IR HW-201 direita (lib/encoder) |
| RADIO_SCK | PTC5 | fixo | SPI0 bare-metal (lib/spi, ALT_0) — fixo dentro de spi_init(), sem define em pinmap.h |
| RADIO_MOSI | PTC6 | fixo | idem RADIO_SCK |
| RADIO_MISO | PTC7 | fixo | idem RADIO_SCK |
| RADIO_CSN | PTC4 | saida | chip-select manual do nRF24 (lib/nrf24) |
| RADIO_CE | PTA13 | saida | lib/nrf24 |
| RADIO_IRQ | PTD2 | entrada_irq | ativo em LOW (lib/nrf24) |
| LED_RED | PTB18 | saida | active low |
| LED_GREEN | PTB19 | saida | active low |

## Controle

| Sinal | Pino | Tipo/Direção | Observação |
|---|---|---|---|
| RADIO_SCK | PTC5 | fixo | SPI0 bare-metal (lib/spi, ALT_0) — fixo dentro de spi_init(), sem define em pinmap.h |
| RADIO_MOSI | PTC6 | fixo | idem RADIO_SCK |
| RADIO_MISO | PTC7 | fixo | idem RADIO_SCK |
| RADIO_CSN | PTC4 | saida | chip-select manual do nRF24 (lib/nrf24) |
| RADIO_CE | PTA13 | saida | lib/nrf24 |
| RADIO_IRQ | PTD2 | entrada_irq | ativo em LOW (lib/nrf24) |
| LED_RED | PTB18 | saida | active low |
| LED_GREEN | PTB19 | saida | active low |
| CONSOLE_UART0 | PTA1 | fixo | PTA1/PTA2 (RX/TX), console fixo via devicetree (zephyr,console) — comandos via script/controle_serial.py |
| JOYSTICK_X | PTB0 | adc | ADC0_SE8 — defeito de hardware conhecido, ver PENDENCIAS.md |
| JOYSTICK_Y | PTB1 | adc | ADC0_SE9 |
| JOYSTICK_BTN | PTB2 | entrada | botão de freio, pull-up interno, active low |
