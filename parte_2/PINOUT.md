# Pinout — PSI3422 parte 2 (carrinho Raspberry Pi)

Hardware: **Raspberry Pi 3 Model B Rev 1.2** · wiringPi 2.50 · Raspbian 10 (buster)
Ponte-H: **L298n** (módulo vermelho "para Arduino")
Câmera: **Logitech C925e** USB (`046d:085b`) em `/dev/video0`

Tabela conferida no hardware com `gpio readall` em 13/10/2026 — não transcrita do PDF.

---

## 1. GPIO → ponte-H

As três numerações coexistem e é fácil trocá-las. **wiringPi** é a que aparece
no código (`digitalWrite(0,...)`, `softPwmWrite(0,...)`); **física** é a que
você conta no conector.

| wiringPi | BCM | Físico | Ponte-H | Motor |
|:---:|:---:|:---:|:---:|---|
| **0** | 17 | **11** | entrada **8** | A |
| **1** | 18 | **12** | entrada **9** | A |
| **2** | 27 | **13** | entrada **10** | B |
| **3** | 22 | **15** | entrada **11** | B |
| — | — | **6 (0 V)** | **conector 5** | **terra comum** |

O terra comum é obrigatório: Raspberry e ponte-H têm fontes independentes e
precisam compartilhar a referência de 0 V, senão o nível lógico não é visto.

Trecho real do `gpio readall` (lado ímpar do conector):

```
 | BCM | wPi |   Name  | Mode | V | Physical |
 |   3 |   9 |   SCL.1 |   IN | 1 |  5 || 6  |  <- fisico 6 = 0v (terra comum)
 |  17 |   0 | GPIO. 0 |   IN | 0 | 11 || 12 |  <- wPi 0 (fis.11) e wPi 1 (fis.12)
 |  27 |   2 | GPIO. 2 |   IN | 0 | 13 || 14 |  <- wPi 2 (fis.13)
 |  22 |   3 | GPIO. 3 |   IN | 0 | 15 || 16 |  <- wPi 3 (fis.15)
```

Note que o físico **12** fica no lado par, em frente ao 11.

---

## 2. Conectores da ponte-H L298n

Numeração da figura 5 da apostila `raspicam_wiringpi.pdf`.

| Conector | Ligação |
|:---:|---|
| 1, 2 | saídas → **motor A** |
| 13, 14 | saídas → **motor B** |
| **3** | jumper do regulador 5 V — **manter no lugar** |
| **4** | **+5 V** (powerbank) |
| **5** | **0 V / GND** (powerbank **e** terra do Raspberry) |
| 6 | **nada ligado** |
| **7, 12** | jumpers ENA/ENB — **manter no lugar** (PWM é por software) |
| 8, 9 | entradas de controle do **motor A** |
| 10, 11 | entradas de controle do **motor B** |

---

## 3. Tabela de rotação

| Motor A (B) | conector 8 (10) | conector 9 (11) |
|---|:---:|:---:|
| horário | 5 V | GND |
| anti-horário | GND | 5 V |
| ponto morto | GND | GND |
| freio | 5 V | 5 V |

---

## 4. Alimentação

| Dispositivo | Fonte |
|---|---|
| Raspberry Pi 3B | powerbank, **5 V / 2 A** |
| Ponte-H + motores | powerbank, **5 V / 1 A** |

Powerbank de 10000 ou 20000 mAh, deitado na carroceria. Se tiver duas saídas
"smart", qualquer uma serve para qualquer um dos dois.

---

## 5. Avisos

- **O Raspberry é um dispositivo de 3,3 V.** Nunca injete 5 V num pino GPIO.
  Neste projeto os 4 GPIO são **saída** — nenhum é usado como entrada.
- Faça **todas** as ligações com Raspberry e ponte-H **desenergizados**.
- Para medir, use fios com conector fêmea. Não encoste a ponta do multímetro
  direto no pino do GPIO — risco de curto acidental.
- Desligue o Raspberry sempre de forma ordenada (`sudo poweroff`). Vários
  cartões SD morreram em 2024–2026 por desligamento indevido.

---

## 6. Ordem de montagem e teste

1. Saídas da ponte-H → motores (1-2 no motor A, 13-14 no motor B).
2. **Antes de ligar a ponte-H ao Raspberry**, rode `aula2/blink3` e meça os
   pinos físicos 11, 12, 13 e 15: devem alternar 3,3 V ↔ 0 V a cada 2 s.
   Isso separa "problema no GPIO" de "problema na fiação".
3. Os 4 fios GPIO → entradas 8, 9, 10, 11.
4. Terra comum: físico 6 → conector 5.
5. Energize (ponte-H 5 V/1 A, Raspberry 5 V/2 A).
6. `aula2/pwmroda4`: esperado ≈2,97 V (90%), ≈1,98 V (60%) e 0 V; onda
   retangular de 100 Hz no osciloscópio.
7. `aula2/paramotor` é a parada de emergência — deixe sempre compilado.
