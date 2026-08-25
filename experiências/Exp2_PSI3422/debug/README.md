# debug/ — validações de bancada, fora da entrega

Firmwares Zephyr isolados pra validar hardware sem depender do resto
do Exp2_PSI3422 (rádio, control_fsm, etc.) — se algo não bate aqui, o
problema é de fiação/mecânica, não de lógica de controle. Vivem na
branch `debug/exp2-validacoes`, não em `main`.

Serial monitor: **115200 8N1**, não 9600 — `current-speed` no
`zephyr.dts` gerado pelo build (Carrinho, Controle e os três
firmwares daqui) é sempre `0x1c200` = 115200; `pio device monitor`
sem `--baud` já cai nesse default do board.

## monitor.py

Painel serial (`uv run monitor.py [porta] [--control]`) que lê a
saída de qualquer um dos firmwares (Carrinho, Controle, JoystickCheck,
AlinhamentoCheck), separa os tipos de log em métricas ao vivo
(telemetria, comando, joystick bruto, fase do AlinhamentoCheck,
erros/avisos) e, com `--control`, repassa w/a/s/d/x/espaço/q/o pra
UART do Controle — mesmo protocolo de
`../Controle/script/controle_serial.py`. Ver docstring do script pra
detalhes. As linhas `SWEEP ...` do DutySweepCheck ainda não têm painel
dedicado, caem no log bruto.

## JoystickCheck

Flasha no board do **Controle**. Lê X/Y do joystick (ADC) e o botão
de freio a cada 200ms e imprime bruto + o `motor_l`/`motor_r` que
`../Controle/src/main.c` derivaria.

Usa isso pra confirmar que o eixo não está preso em 3,3V/4095 (defeito
já visto nesta bancada, ver aviso em `../Controle/src/main.c`) antes
de confiar no joystick pra controlar o carrinho de verdade.

## AlinhamentoCheck

Flasha no board do **Carrinho**. Só motor + PWM (sem ultrassom, sem
rádio, sem control_fsm): manda o mesmo duty fixo (~50%) pros dois
motores por 3s, freia, pausa 2s, repete indefinidamente. Coloque o
carrinho no chão e observe se ele desvia pra um lado.

Como os dois motores recebem literalmente o mesmo valor, qualquer
desvio observado aqui é mecânico (roda torta, folga, atrito
desigual) — não dá pra atribuir a uma diferença de código entre os
dois lados.

## DutySweepCheck

Flasha no board do **Carrinho**. Testado depois do `AlinhamentoCheck`
mostrar o carrinho andando torto: sobe o duty de CADA motor sozinho
(o outro freado) em degraus de 0 até o máximo, ~1,5s por degrau, e
imprime o duty a cada passo. Levante o carrinho do chão e anote o
duty onde cada roda sai do lugar (torque de arranque) — se os dois
valores forem bem diferentes, é evidência objetiva de motor/ponte
H/fiação desigual, não só impressão visual.

Conferido antes de suspeitar de hardware: `pwm_z42.c`/`motor.c`
escrevem `CnV`/`CnSC` pelo mesmo código pros dois canais TPM, só
endereçando índices diferentes — duty comandado é garantidamente
idêntico em registrador nos dois motores.

## EncoderCheck

Flasha no board do **Carrinho**. Mesmo duty fixo nos dois motores
(igual `AlinhamentoCheck`), mas agora conta pulsos dos dois encoders
IR HW-201 (um por roda) e imprime pulsos/segundo (PPS) de cada lado a
cada 1s — dá um número real de rotação em vez de só observação
visual. Levante o carrinho do chão antes de ligar.

Encoders ainda não instalados fisicamente — pinos reservados em
PTD1 (esquerda) / PTD3 (direita), ver `../Pinmap.md`. De propósito
NÃO em PORTB/PORTE: nesta subfamília KL25Z só PORTA e PORTC/PORTD têm
vetor de interrupção de pino — foi exatamente o problema relatado ao
tentar usar `gpioe` como IRQ antes.
