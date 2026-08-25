# Pinmap — Exp2_PSI3422 (Carrinho + Controle, FRDM-KL25Z)

Validado por build real (`pio run` em ambos os projetos) e pelos `#define` em cada `src/main.c` — não pelo diagrama Arduino do board (os headers D-numerados não batem com os pinos reais usados aqui).

Nem SPI nem TPM têm nó de devicetree neste port (Zephyr 2.7.1 empacotado pelo PlatformIO — conferido em `dts/arm/nxp/nxp_kl25z.dtsi`). Por isso o rádio usa `lib/spi` bare-metal em vez de `spi_dt_spec`, e os motores usam `pwm_z42` em vez de `pwm_dt_spec`. `gpioc`/`gpioe` também não vêm habilitados por padrão (só `gpioa/b/d`) — cada projeto liga isso em `zephyr/boards/frdm_kl25z.overlay`.

## Carrinho

| Sinal | Pino | Observação |
|---|---|---|
| Motor L — IN1 | PTC8 | GPIO |
| Motor L — IN2 | PTC9 | GPIO |
| Motor L — ENA (PWM) | PTA4 | TPM0_CH1, via `pwm_z42` |
| Motor R — IN1 | PTA12 | GPIO |
| Motor R — IN2 | PTD5 | GPIO |
| Motor R — ENB (PWM) | PTA5 | TPM0_CH2, via `pwm_z42` |
| Ultrassom — TRIG | PTD0 | GPIO saída |
| Ultrassom — ECHO | PTD4 | GPIO entrada, interrupção |
| Rádio — SCK/MOSI/MISO | PTC5/PTC6/PTC7 | SPI0 bare-metal (`lib/spi`, `ALT_0`) |
| Rádio — CSN | PTC4 | GPIO, chip-select manual |
| Rádio — CE | PTA13 | GPIO |
| Rádio — IRQ | PTD2 | GPIO entrada, interrupção ativa em LOW |
| LED status (vermelho/verde) | PTB18 / PTB19 | Active low |
| Encoder IR HW-201 — esquerda (reservado) | PTD1 | GPIO entrada, interrupção — ver `debug/EncoderCheck` |
| Encoder IR HW-201 — direita (reservado) | PTD3 | GPIO entrada, interrupção — ver `debug/EncoderCheck` |

Encoders: ainda não instalados fisicamente (Aula 4). Reservados em PORTD de propósito — nesta subfamília KL25Z só PORTA e PORTC/PORTD têm vetor de interrupção de pino; PORTB/PORTE não geram IRQ por mudança de pino (confirmado na bancada: `gpioe` como IRQ deu problema). PTD1/PTD3 ficam nos "buracos" entre os pinos de PORTD já usados (TRIG=PTD0, IRQ do rádio=PTD2, ECHO=PTD4, motor R IN2=PTD5), mesmo port já comprovado como fonte de interrupção.

## Controle

| Sinal | Pino | Observação |
|---|---|---|
| Rádio — SCK/MOSI/MISO | PTC5/PTC6/PTC7 | SPI0 bare-metal (`lib/spi`, `ALT_0`) |
| Rádio — CSN | PTC4 | GPIO, chip-select manual |
| Rádio — CE | PTA13 | GPIO |
| Rádio — IRQ | PTD2 | GPIO entrada, interrupção ativa em LOW |
| LED status (vermelho/verde) | PTB18 / PTB19 | Active low |
| Console (UART0) | PTA1/PTA2 | fixo, reservado, `printk` + comandos via `script/controle_serial.py` |
| Joystick — X | PTB0 | ADC0_SE8, `adc_channel_cfg`/`adc_sequence` (padrão de `PSI3441/entregas/4`) |
| Joystick — Y | PTB1 | ADC0_SE9, idem |
| Joystick — Botão de freio | PTB2 | GPIO entrada, pull-up interno, active low |

Pinos do joystick: não passaram por build real (`pio run`) ainda, só pelo padrão comprovado do PSI3441 Ativ.4 — nesta bancada o eixo já foi visto travado em 3,3 V/4095 (ver aviso em `Controle/src/main.c`).

## TPM disponível

A KL25Z tem 3 módulos TPM (`TPM0`: 6 canais, `TPM1`/`TPM2`: 2 canais cada — 10 no total). O Carrinho usa só 2 canais de `TPM0`; sobra folga para experiências futuras.
