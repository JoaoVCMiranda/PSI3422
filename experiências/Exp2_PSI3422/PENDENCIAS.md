# Pendências — Exp2_PSI3422

Snapshot do que falta, pra retomar sem precisar releer a conversa toda. `main` = entrega real (Atividade 2 já entregue, README Aula 2). `debug/exp2-validacoes` = bancada de validação, não mergeada.

## Hardware — bloqueando

- **Joystick (Controle) com defeito confirmado**: eixo travado perto de 3,3V/4095 (medido com multímetro e com `debug/JoystickCheck`). `joystick_read()` está desabilitado de novo no `main` (só UART funciona). Falta:
  - Rodar o teste de troca cruzada (trocar os fios X/Y) e o teste de continuidade no potenciômetro pra saber se é o pot ou a fiação do lado do KL25Z.
  - Depois de consertar, reativar a chamada comentada em `Controle/src/main.c` (`joystick_read(&ml, &mr, &brk);`) e revalidar com `debug/JoystickCheck`.

- **Carrinho anda torto** (uma roda com mais força/torque que a outra), confirmado com `debug/AlinhamentoCheck`. Já verificado que não é bug de firmware (`pwm_z42.c`/`motor.c` escrevem o mesmo duty nos dois canais TPM). Falta:
  - Rodar `debug/DutySweepCheck` (torque de arranque por motor) — construído, ainda não testado/reportado.
  - Rodar `debug/EncoderCheck` depois de instalar os HW-201 (pulsos/s por roda no mesmo duty) — construído, aguardando hardware.
  - Teste manual de troca cruzada dos motores (isola motor vs. ponte H/fiação) e teste com rodas no ar (isola elétrico vs. mecânico/atrito) — sugeridos, ainda não reportados.

## Branch debug/exp2-validacoes — não mergeada em main

- `JoystickCheck`, `AlinhamentoCheck`, `DutySweepCheck`, `EncoderCheck`, `monitor.py`: só existem aqui, não em `main`. Decidir quando (ou se) algo disso vale trazer pra `main` — provavelmente não os firmwares em si (são só bancada), mas:
- **Reserva de pinos dos encoders (PTD1/PTD3) no `Pinmap.md`** está só nesta branch. Levar pra `main` assim que os HW-201 forem instalados e `EncoderCheck` confirmar que os pinos funcionam de verdade.
- `debug/monitor.py`: comentário na linha ~73 ainda cita `DISTANCIA_MINIMA_M`, renomeada pra `DISTANCIA_FRENTE_M`/`DISTANCIA_CURVA_M`/`DISTANCIA_PARADA_M` no commit `4b3ff74` (main). Só comentário, não quebra nada — cosmético.

## auto_mode (control_fsm) — implementado, não testado com hardware real

Commit `4b3ff74` em `main` trocou o auto_mode binário por 4 faixas de distância (frente/vira direita/ré/para). Buildado com sucesso, mas **nunca rodado no carrinho de verdade** — validar:
- Os limiares (30/20/4cm) fazem sentido com o alcance/precisão real do HC-SR04 nesta montagem.
- O pivô de giro (`motor_l` frente + `motor_r` ré) realmente vira pra direita no sentido físico esperado (não foi conferido em bancada, só a lógica).
- Se `VELOCIDADE_AUTO` (~50%) é uma velocidade seguras pra virar/dar ré sem capotar ou perder tração.

## README — próximas aulas ainda não iniciadas

- **Aula 3** (`- [ ] Por Fazer`): projetar a PCB de verdade. `Pinmap.md` já cobre boa parte do "defina os pinos antes de criar a placa", mas falta o desenho da placa em si.
- **Aula 4** (sem checkbox — não iniciada): acoplar os HW-201, calibrar distância percorrida a partir da contagem de pulsos, fazer curvas de 90° calibradas. `debug/EncoderCheck` é um adiantamento de bancada pra isso, não a entrega.
- **Aula 5/6** (sem checkbox — não iniciada): comandos RUN/STOP remotos, travessia de labirinto, distância salva/apagável.
