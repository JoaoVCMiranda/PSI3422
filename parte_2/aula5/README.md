# Aula 5 parte 2 — leitura do dígito manuscrito (fase 5)

| Pasta | Raspberry | Computador | Lição |
|---|---|---|---|
| [`tarefa1`](tarefa1/) | — | `fase5` | **#1/2** (fase 5): localiza a placa e lê o dígito escrito nela |

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
    computador$ tarefa1/fase5 capturado.avi quadrado.png locarec.avi

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
