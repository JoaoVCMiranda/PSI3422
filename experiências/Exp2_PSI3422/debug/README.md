# debug/ — validações de bancada, fora da entrega

Dois firmwares Zephyr isolados pra validar hardware sem depender do
resto do Exp2_PSI3422 (rádio, control_fsm, etc.) — se algo não bate
aqui, o problema é de fiação/mecânica, não de lógica de controle.
Vivem na branch `debug/exp2-validacoes`, não em `main`.

## JoystickCheck

Flasha no board do **Controle**. Lê X/Y do joystick (ADC) e o botão
de freio a cada 200ms e imprime bruto + o `motor_l`/`motor_r` que
`../Controle/src/main.c` derivaria. Serial monitor 9600 8N1.

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
