# Pinmap — Exp4_PSI3422 (CarrinhoBase, FRDM-KL25Z)

Mesmo chassi/pinagem de motores da Exp2_PSI3422 (ver `../Exp2_PSI3422/Pinmap.md`) — a Aula 4 só acopla os encoders IR HW-201, já reservados em PORTD desde a Exp2 (`debug/EncoderCheck`). Sem rádio nem ultrassom neste projeto: o escopo da Aula 4 é ponte H + contadores de volta com sinal, não o carrinho completo.

| Sinal | Pino | Observação |
|---|---|---|
| Motor L — IN1 | PTC8 | GPIO |
| Motor L — IN2 | PTC9 | GPIO |
| Motor L — ENA (PWM) | PTA4 | TPM0_CH1, via `lib/pwm_z42` |
| Motor R — IN1 | PTA12 | GPIO |
| Motor R — IN2 | PTD5 | GPIO |
| Motor R — ENB (PWM) | PTA5 | TPM0_CH2, via `lib/pwm_z42` |
| Encoder IR HW-201 — esquerda | PTD1 | GPIO entrada, interrupção de borda de subida |
| Encoder IR HW-201 — direita | PTD3 | GPIO entrada, interrupção de borda de subida |

`gpioc` não vem habilitado por padrão neste board (só `gpioa/b/d`) — ligado em `zephyr/boards/frdm_kl25z.overlay`, mesmo motivo do overlay da Exp2. `gpiod` já é default, não precisa de overlay para os encoders.

Encoders: mesma escolha de pino da Exp2 (PTD1/PTD3) — só PORTA e PORTC/PORTD geram interrupção por mudança de pino nesta subfamília KL25Z (PORTB/PORTE não, confirmado em bancada). Ver `lib/SPEC.md`, seção `encoder/`, para o racional de como o sentido (frente/ré) é inferido do comando do motor.
