# PSI3422 parte 2 — Aula 2 (Raspberry Pi)

Divisão: **João** faz a montagem física (wiring + chassi). **Código** já está aqui.

Raspberry: `192.168.15.44` (IP chega por notificação no ntfy, tópico `PSI3422_JVCM`).

---

## 1. Montagem física — instruções

> **Faça TODAS as ligações com o Raspberry e a ponte-H desenergizados.**
> O Raspberry é um dispositivo de **3,3 V**. Nunca injete 5 V num pino GPIO.
> Nenhum GPIO será usado como entrada neste projeto — os 4 são saída.

### 1.1 Ponte-H L298n (numeração da figura 5 da apostila)

| Conector | Ligação |
|---|---|
| 1 e 2 | motor **A** |
| 13 e 14 | motor **B** |
| 3 | **jumper do regulador 5 V — manter no lugar** |
| 4 | **+5 V** (powerbank) |
| 5 | **0 V / GND** (powerbank) |
| 6 | **nada** |
| 7 e 12 | **jumpers ENA/ENB — manter no lugar** (o PWM é por software) |
| 8 e 9 | entradas de controle do motor A |
| 10 e 11 | entradas de controle do motor B |

### 1.2 GPIO do Raspberry → entradas da ponte-H

| wiringPi | pino físico | → conector ponte-H | função |
|---|---|---|---|
| 0 | 11 | 8 | motor A |
| 1 | 12 | 9 | motor A |
| 2 | 13 | 10 | motor B |
| 3 | 15 | 11 | motor B |
| — | **6 (0 V)** | **5** | **terra comum — obrigatório** |

O terra comum não é opcional: Raspberry e ponte-H usam fontes independentes e
precisam compartilhar a referência de 0 V, senão o nível lógico não é visto.

### 1.3 Ordem de montagem

1. Saídas da ponte-H → motores (conectores 1-2 no motor A, 13-14 no motor B).
2. Os 4 fios GPIO (físicos 11, 12, 13, 15) → entradas 8, 9, 10, 11.
3. Terra comum: físico 6 → conector 5.
4. Só então energize: ponte-H em 5 V/1 A e Raspberry em 5 V/2 A do powerbank.
   Se o powerbank tiver duas saídas "smart", qualquer uma serve para qualquer um.

### 1.4 Tabela de rotação (para conferir o sentido dos motores)

| Motor A (B) | conector 8 (10) | conector 9 (11) |
|---|---|---|
| horário | 5 V | GND |
| anti-horário | GND | 5 V |
| ponto morto | GND | GND |
| freio | 5 V | 5 V |

### 1.5 Mecânica e câmera

- Montagem **caprichada**, tudo firmemente fixado, sem fios soltos — a
  qualidade da montagem **vale nota** nesta aula e nas próximas.
- Powerbank deitado na carroceria (o carrinho fica mais estável).
- Raspberry sempre dentro da caixa, com os conectores acessíveis.
- Powerbank **totalmente carregado** antes da aula (demora bastante).
- Câmera Logitech C925e: basta plugar na USB. (O `sudo modprobe bcm2835-v4l2`
  e o `Camera=enable` do raspi-config só valem para a raspicam de cabo flat.)

### 1.6 Segurança na medição

Não encoste pontas de multímetro diretamente nos pinos do GPIO — risco de
curto acidental. Use sempre fios com conector fêmea.

---

## 2. Sequência de teste (nesta ordem)

1. **`blink3` ANTES de conectar a ponte-H.** Meça os pinos físicos 11, 12, 13
   e 15 com multímetro (ou LED + resistor de 270 Ω). Devem alternar entre
   3,3 V e 0 V a cada 2 s. Isso separa "problema no GPIO" de "problema na fiação".
2. Faça a fiação da ponte-H (seção 1).
3. **`pwmroda4`.** Esperado no multímetro: ≈2,97 V (90%), ≈1,98 V (60%) e 0 V.
   No osciloscópio: onda retangular de 100 Hz. Com as rodas montadas, o carrinho
   gira horário e anti-horário a 90% e depois repete a 60%.
4. **`paramotor`** é a parada de emergência — deixe sempre compilado. Se algum
   programa terminar com os motores ligados, rode-o.

---

## 3. Arquivos

| Arquivo | O que é |
|---|---|
| `raspberry.hpp` | do professor, baixado de lps.usp.br (não editar) |
| `projeto.hpp` | **nosso**: classes `DEVICE`/`SERVER`/`CLIENT` + `testaBytes` |
| `aula2/raspcam2.cpp` | programa base da apostila (câmera na tela) |
| `aula2/raspvid.cpp` | **lição #1/3** — câmera na tela + gravação em vídeo |
| `aula2/blink3.cpp` | teste das 4 saídas GPIO |
| `aula2/pwmroda4.cpp` | **lição #2/3 (b)** — softPwm a 90% e 60% |
| `aula2/paramotor.cpp` | parada de emergência |
| `aula2/server6b.cpp` / `client6b.cpp` | **lição #3/3** — teste de `sendBytes`/`receiveBytes` |

## 3.1 Script de uso (`aula2/aula2.sh`)

Atalho para tudo que está nas seções 4 e 5. Detecta sozinho se está rodando no
Raspberry ou no computador e usa as flags certas.

    ./aula2.sh status           estado do ambiente (câmera, GPIO, IP, binários)
    ./aula2.sh build            compila o que faz sentido para este local
    ./aula2.sh blink            blink3 — teste dos 4 GPIO, ANTES de ligar a ponte-H
    ./aula2.sh pwm              pwmroda4 — lição #2/3(b)
    ./aula2.sh parar            paramotor — PARADA DE EMERGÊNCIA
    ./aula2.sh cam              raspcam2 — câmera na tela
    ./aula2.sh vid [arq.avi]    raspvid — lição #1/3 (default `saida.avi`)
    ./aula2.sh server           server6b — lição #3/3, lado Raspberry
    ./aula2.sh client [ip]      client6b — lição #3/3, lado computador

Ordem sugerida: `status` → `build` → `blink` → (fiação) → `pwm` → `vid` → `server`/`client`.

Ele recompila sozinho o que faltar antes de rodar, avisa quando `DISPLAY` está
vazio (programa com janela precisa de `ssh -X` ou VNC) e, se o Cekeikon não
estiver instalado, cai num `g++` equivalente — útil no notebook.

## 4. Compilação

No **Raspberry**:

    compila blink3 -w
    compila paramotor -w
    compila pwmroda4 -w -ocv
    compila raspcam2 -ocv
    compila raspvid -ocv
    compila server6b -ocv

No **computador**:

    compila client6b -ocv -v3

> **Atenção:** a apostila (p. 19) mostra `compila server6b` sem `-ocv`. Isso
> valia para a versão mínima de `raspberry.hpp` da p. 13. O arquivo oficial
> publicado pelo professor já inclui `<opencv2/opencv.hpp>` na linha 29, então
> **tudo que inclui `projeto.hpp` precisa de `-ocv`**.

### Qual computador

O computador do projeto é um **Linux Mint com Cekeikon instalado** — é lá que
vale o `compila` acima, e é o ambiente que a matéria pressupõe (os micros da
GD-06 também são Mint com Cekeikon). O OpenCV é o **3**, igual ao da Raspi.

O notebook Debian 13 (trixie) **não tem Cekeikon**, e não vale a pena instalar:
o pacote x86_64 é um binário de 443 MB feito para Mint 18.3 (adaptado até o
Mint 22) e o Debian trixie é mais novo que isso. Nesse notebook o `aula2.sh`
cai sozinho num `g++` equivalente, usando o **OpenCV 4** do sistema.

Esse fallback é **conveniência, não substituto**. O OpenCV 4 removeu nomes que
o `raspberry.hpp` usa; hoje o script contorna `CV_BGR2GRAY` e `CV_GRAY2BGR`,
mas a aula 3 traz outros (por exemplo `CV_IMWRITE_JPEG_QUALITY`, que virou
`IMWRITE_JPEG_QUALITY`). Entregue e demonstre sempre no Mint.

### Quando o `compila` falha

O `compila` do Cekeikon é um binário e pode abortar (SIGABRT, saída **134**) se
o ambiente for ativado pelo script errado. Cada arquitetura tem o seu:

| Máquina | Script de ativação |
|---|---|
| Mint / PC x86_64 | `source ~/cekeikon5/bin/ativa_cekcpu` |
| Raspberry (armv7l) | `source ~/cekeikon5/bin/inst-cek5-raspberry.sh` |

O `aula2.sh` escolhe sozinho pela arquitetura. E se o `compila` falhar mesmo
assim, ele avisa e **cai no `g++` automaticamente** — não trava o trabalho.

Para forçar ou ajustar o caminho do `g++`:

    SEM_CEKEIKON=1 ./aula2.sh build        # ignora o compila
    OCV_FLAGS="-I/usr/include/opencv4 -lopencv_core ..." ./aula2.sh build

`OCV_FLAGS` existe para o caso de o `pkg-config` não conhecer o OpenCV
instalado (ex.: o OpenCV 3 que vem dentro do Cekeikon) — aí você passa os
`-I`/`-L`/`-l` na mão.

## 5. Execução

    # lição #1/3
    raspberry$ ./raspvid saida.avi        # qualquer tecla encerra

    # lição #2/3
    raspberry$ ./pwmroda4

    # lição #3/3
    raspberry$  ./server6b
    computador$ ./client6b 192.168.15.44

Saída esperada da lição #3/3 (já validada aqui via loopback):

    server: Esperando conexao...
    server: recebi conexao de <ip>
    Recebeu corretamente 100000 bytes 214

    client: conectando a <ip>
    Recebeu corretamente 100000 bytes 111
    Erro na recepcao de 100000 bytes 2

A última linha é um **erro proposital** da apostila (o servidor manda 111, o
cliente testa contra 2).

## 6. Notas de implementação

- `sendBytes`/`receiveBytes` fazem laço até enviar/receber **todos** os bytes.
  É o ponto da lição: `send()` e `recv()` podem transferir menos que o pedido.
- `sendUint`/`receiveUint` usam `htonl`/`ntohl` (a rede é big endian; ARM e
  Intel são little endian).
- Fizemos a parte **opcional** da apostila (classe abstrata `DEVICE`), e um
  passo além: `sendBytes`/`receiveBytes` aparecem **uma única vez** em `DEVICE`,
  não duplicados em `SERVER` e `CLIENT`. O que varia entre as duas é só qual
  socket usar, exposto pelo virtual `fd()`. Isso responde à pergunta em aberto
  da apostila (p. 30, "É possível escrever sendBytes e receiveBytes também uma
  única vez?"). Se o professor preferir ver a estrutura literal da figura 11b,
  é trivial duplicar os dois métodos nas derivadas.
- `raspvid` **mede o fps real** antes de abrir o `VideoWriter`. A apostila avisa
  que, via wifi, captura-se bem menos que 30 fps e gravar com 30 fixo deixa o
  vídeo acelerado.
