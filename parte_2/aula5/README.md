# Aula 5 parte 2 — leitura do dígito manuscrito (fase 5)

| Pasta | Raspberry | Computador | Lição |
|---|---|---|---|
| [`tarefa1`](tarefa1/) | — | `fase5` | **#1/2** (fase 5): localiza a placa e lê o dígito escrito nela |
| [`tarefa2`](tarefa2/) | `servidor6` | `cliente6` | **#2/2** (fase 6): o carrinho segue o caminho indicado pelos dígitos |

Como na aula 4, **cada programa é um arquivo único**, comentado passo a passo
com etiquetas de origem (`[OpenCV]`, `[raspberry.hpp]`, `[C]` …). O
`fase5.cpp` tem três partes:

1. **DETECTOR DA PLACA:** cópia exata do `../aula4/tarefa2/fase3p.cpp`
   (CC + NCC em 10 escalas, OpenMP). Se o detector mudar lá, copie de novo.
2. **LEITOR DO DÍGITO** (classe `DIGITO`): a parte nova, com os passos 1 e
   4a–4d da apostila, um método por passo.
3. **PROGRAMA** (`main`): os passos 1–6 da apostila (p. 4), na ordem.

## Arquivos necessários (em `parte_2/aula5/`)

- `capturado.avi` e `quadrado.png`, do site da apostila;
- os 4 arquivos do `mnist.zip`, com os **nomes originais**:
  `train-images.idx3-ubyte`, `train-labels.idx1-ubyte`,
  `t10k-images.idx3-ubyte`, `t10k-labels.idx1-ubyte`.

O MNIST (55 MB) e os `.avi` não entram no git. Se o site do professor estiver
fora, os mesmos arquivos estão no espelho público do Google
(`storage.googleapis.com/cvdf-datasets/mnist/`, como `train-images-idx3-ubyte.gz`).
Basta descompactar e trocar o primeiro `-` do nome por `.`.

## Compilar e rodar

    ./aula5.sh build      # compila o fase5
    ./aula5.sh fase5      # roda no capturado.avi e gera locarec.avi

Ou na mão (de dentro de `aula5/`):

    computador$ (cd tarefa1 && compila fase5 -ocv -v3 -omp)
    computador$ (cd tarefa2 && compila cliente6 -ocv -v3 -omp)
    raspberry$  (cd tarefa2 && compila servidor6 -ocv -w)

    computador$ tarefa1/fase5 capturado.avi quadrado.png locarec.avi

    raspberry$  tarefa2/servidor6 [frenteEsq frenteDir reEsq reDir]
    computador$ tarefa2/cliente6 100.70.100.51 [videosaida.avi] [t/c]

## Lição #1/2: como o dígito é lido

| Passo | Método | O que faz |
|---|---|---|
| 1 | construtor | lê as **60000 imagens de treino** do MNIST (14×14, cores invertidas, bbox) e monta a árvore do FLANN (~2 s) |
| 4 | `le` | só lê com a placa **perto** (escala 0 ou 1). Longe, o dígito tem poucos pixels |
| 4a | `recorta` | recorta o interior do quadrado amarelo, com 10% de folga de cada lado |
| 4b | `ajustaContraste` | estica os tons: tinta (mínimo) → 0, fundo (mediana) → 1 |
| 4c | `reduz` | `mnist.bbox`: tira as bordas brancas e reduz para 14×14 |
| 4d | `classifica` | FLANN: o rótulo da imagem de treino mais parecida |

Detalhes que importam:

- **Onde está o dígito:** o interior do quadrado amarelo é a única região do
  `quadrado.png` com pixels exatamente (255,255,255): linhas e colunas 121 a
  279, das 401. O programa mede isso no arquivo, em vez de usar números fixos,
  e converte para o quadro com a posição e a escala da placa encontrada.
- **Folga de 10%:** sem ela, a linha amarela ou uma localização 1–2 pixels
  fora põem uma borda escura no recorte. O bbox acharia a borda em vez do dígito.
- **Mediana como "branco":** a maioria dos pixels do recorte é fundo, então a
  mediana é um tom de fundo, qualquer que seja a iluminação. Tudo que for tão
  claro quanto o fundo vira 1.0, como no MNIST invertido.
- **Placa sem dígito:** se fundo − tinta < 0,15, não há tinta e o programa não
  tenta ler (só amplificaria ruído).
- **Por que só o treino:** o conjunto de teste serve para medir o erro de um
  algoritmo. O programa final não precisa dele (responde à pergunta da p. 3).
- **Sem bbox duplicado:** o `predictInterno` é chamado direto, porque o
  `MnistFlann::predict` faria o bbox de novo e o 4c já o fez (assim dá para
  mostrar a imagem 14×14 no vídeo).

**Vídeo de saída** (como a figura 1b): 440×240, o quadro com a placa e o dígito
em vermelho ao lado, mais uma coluna à direita com a imagem 14×14 que o FLANN
viu (em cima) e o recorte do quadro com o dígito lido (embaixo).

## Resultado no `capturado.avi`

    MNIST lido e FLANN treinado em 1.9 s
    1709 quadros, placa achada em 863, digito lido em 349
    localizacao + leitura: 13.3 fps        (notebook com 2 núcleos, 4 threads)

O vídeo mostra 10 placas, uma de cada vez: 3, 7, 9, 0, 4, 8, 2, 1, 5, 6.
Contando o dígito mais frequente em cada trecho como o certo:

| Placa | Quadros | Leituras | Erradas |
|---|---|---|---|
| 3 | 87–125 | 39 | 0 |
| 7 | 264–308 | 45 | 0 |
| 9 | 438–472 | 35 | 0 |
| 0 | 593–626 | 34 | 2 (um 5, um 6) |
| 4 | 739–769 | 31 | 1 (6) |
| 8 | 891–920 | 30 | 1 (3) |
| 2 | 1029–1063 | 35 | 0 |
| 1 | 1243–1279 | 37 | 0 |
| 5 | 1426–1451 | 26 | 0 |
| 6 | 1571–1607 | 37 | 0 |
| **total** | | **349** | **4 (1,1%)** |

As 10 placas são lidas certo, e os 4 erros são quadros isolados. Exemplo: um
"0" aberto em cima e de traço fino lido como "6". Para a fase 6, onde o
carrinho age a partir do dígito, vale decidir pela **maioria de alguns quadros
seguidos**, e não por um quadro só.

## Lição #2/2: máquina de estados (fase 6)

Toda a lógica fica no `cliente6`. O `servidor6` é o `servidor4` da aula 4 com
uma única novidade, o descarte de quadros velhos da câmera (`grab()` 2 vezes
antes de cada `read`, apostila p. 13). O protocolo é o mesmo: `'m' esq dir`
ou `'s'`.

O `cliente6.cpp` é um arquivo único com quatro partes:

1. **DETECTOR DA PLACA:** cópia exata do `cliente4`.
2. **LEITOR DO DÍGITO:** cópia exata do `fase5`.
3. **MÁQUINA DE ESTADOS:** a figura 6 da apostila, desenhada em ASCII no
   código, com um método por estado.
4. **TELA E PROGRAMA:** o laço do `cliente4`, mais os painéis do dígito.

| Estado | O que faz | Sai quando |
|---|---|---|
| **0 SEGUE** | segue a placa (controle da fase 4); sem placa, fica parado | placa grande (lado ≥ 60 px) → 1 |
| **1 PARA** | motores desligados | passou `TEMPO_PARADA` (0,8 s) → 2 |
| **2 LE** | lê o dígito com o carrinho parado; junta 5 leituras e vence a maioria | 0 ou 1 → 4 · 2–9 → 3 · 15 quadros sem conseguir → 0 |
| **3 MOVE** | faz o movimento do dígito durante o seu tempo | passou o tempo → 5 |
| **4 FIM** | fim do caminho: fica parado enquanto a placa 0/1 estiver à vista | placa sumiu por 1 s → 0 |
| **5 TERMINA** | motores desligados, espera o giro acabar | passou `TEMPO_PARADA` → 0 |

Comandos (2025): **0/1** fim · **2** 180° à esquerda · **3** 180° à direita
· **4/5** passa por baixo da placa · **6/7** 90° à esquerda · **8/9** 90° à
direita. Girar = uma roda para frente e a outra para trás.

Decisões de projeto:

- **Diferença da apostila no estado 4:** na apostila, o 0/1 termina os dois
  programas. Aqui o carrinho só para e espera a placa sair de vista. Depois
  volta ao estado 0 e procura a próxima, então dá para testar várias placas
  sem reiniciar nada. Os programas só terminam com **ESC**. Ele espera a
  placa *sumir*, e não só um tempo, porque voltar ao 0 com a placa 0 ainda na
  frente faria o carrinho ler o 0 de novo e entrar num vai-e-vem.
- **Um passo por quadro:** a apostila sugere uma variável `estado` com um
  `if` por estado dentro de um laço. O nosso laço é o do `main`, que roda uma
  vez por quadro, porque cada volta tem que devolver um comando ao servidor.
  O método `MAQUINA::passo()` executa o estado atual uma vez.
- **Tempo em segundos, não em quadros:** o fps muda com a rede. Cada estado
  guarda quando começou (`inicioEstado`) e compara com o relógio.
- **Rodas paradas por padrão:** `passo()` zera as rodas antes de rodar o
  estado. Só os estados 0 e 3 as ligam, então nenhum estado esquece o motor
  ligado.
- **Maioria de 5 leituras no estado 2:** uma leitura isolada erra ~1% das
  vezes (lição #1), e um erro faria o carrinho virar para o lado errado.
- **Cada transição é impressa no terminal**, com o motivo. Ex.:
  `estado 2 LE -> 3 MOVE (li o digito 7: 90 graus a esquerda)`.

**Calibre no carrinho** (constantes no topo da seção MÁQUINA DE ESTADOS):

- `TEMPO_90` (0,8 s) e `TEMPO_180` (1,6 s): mande girar e ajuste até dar 90° e 180°;
- `GIRO` (0,70): velocidade das rodas girando;
- `TEMPO_PASSA` (2,0 s) e `VEL_PASSA`: o bastante para passar sob a placa;
- `TEMPO_PARADA` (0,8 s): se o dígito sair borrado no estado 2, aumente.

A tela mostra, no alto, o estado, o dígito lido e a informação do estado
(`dirx=-11`, `lendo 3/5`, `90 graus a esquerda (0.4s)`).

### Teste sem o carrinho

O teste foi feito pela rede local deste PC, com wiringPi simulado e o
`capturado.avi` no lugar da câmera. A cópia de teste do servidor repete o
mesmo quadro enquanto as rodas estão paradas, imitando o carrinho parado na
frente de uma placa fixa. No vídeo cru, quem segura a placa a tira de cena
durante a espera do estado 1.

    estado 0 SEGUE   -> 1 PARA     (placa grande: para para ler)
    estado 1 PARA    -> 2 LE       (carrinho parado)
    estado 2 LE      -> 3 MOVE     (li o digito 3: 180 graus a direita)
    estado 3 MOVE    -> 5 TERMINA  (fim do movimento)
    estado 5 TERMINA -> 0 SEGUE    (carrinho parado)
    ...              (7: 90 graus a esquerda, 9: 90 graus a direita)
    estado 2 LE      -> 4 FIM      (li o digito 0: fim do caminho, parado)
    estado 4 FIM     -> 0 SEGUE    (placa saiu de vista: procura a proxima)
    ...              (4: passa por baixo, 8: 90 graus a direita, 2: 180 a esquerda)
    estado 2 LE      -> 4 FIM      (li o digito 1: fim do caminho, parado)
    estado 4 FIM     -> 0 SEGUE    (placa saiu de vista: procura a proxima)
    ...              (5: passa por baixo; o video acaba antes da placa 6)

Todas as placas foram lidas certo. Algumas aparecem duas vezes no log,
porque no vídeo o "carrinho" não gira de verdade e a placa continua na frente. Falta testar no carrinho
real e calibrar os tempos dos giros.
