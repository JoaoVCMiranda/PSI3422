# debug/ — validações de bancada, fora da entrega

Dois firmwares Zephyr isolados pra validar hardware sem depender do
resto do Exp2_PSI3422 (rádio, control_fsm, etc.) — se algo não bate
aqui, o problema é de fiação/mecânica, não de lógica de controle.
Vivem na branch `debug/exp2-validacoes`, não em `main`.

Serial monitor: **115200 8N1**, não 9600 — `current-speed` no
`zephyr.dts` gerado pelo build (Carrinho, Controle e os dois
firmwares daqui) é sempre `0x1c200` = 115200; `pio device monitor`
sem `--baud` já cai nesse default do board.

## monitor.py

Painel serial (`uv run monitor.py [porta] [--control]`) que lê a
saída de qualquer um dos 4 firmwares (Carrinho, Controle,
JoystickCheck, AlinhamentoCheck), separa os tipos de log em métricas
ao vivo (telemetria, comando, joystick bruto, fase do
AlinhamentoCheck, erros/avisos) e, com `--control`, repassa
w/a/s/d/x/espaço/q/o pra UART do Controle — mesmo protocolo de
`../Controle/script/controle_serial.py`. Ver docstring do script pra
detalhes.

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
