# Aula 4 parte 2 — template matching, OpenMP e carrinho seguindo a placa

| Pasta | Raspberry | Computador | Lição |
|---|---|---|---|
| [`tarefa1`](tarefa1/) | — | `fase3` | **#1/3** (fase 3): localiza a placa nos quadros de um vídeo, sequencial |
| [`tarefa2`](tarefa2/) | — | `fase3p` | **#2/3**: o mesmo, com os template matchings em paralelo (OpenMP) |
| [`tarefa3`](tarefa3/) | `servidor4` | `cliente4` | **#3/3** (fase 4): o carrinho segue a placa |

Arquivos compartilhados, na raiz de `aula4/`:

- [`placa.hpp`](placa.hpp): o detector (classe `PLACA`) e o laço de vídeo da fase 3
  (`rodaFase3`). É usado pelas três lições, então uma melhoria no detector vale
  para todas.
- [`motores.hpp`](motores.hpp): mapeamento GPIO e calibração das rodas, os mesmos do
  `../aula3/tarefa6/servidor2.cpp`.
- `cliente4` reaproveita o `GRAVADOR` (vídeo com fps medido, fechado em qualquer
  saída) e o `ARGCLIENTE` de [`../aula3/teclado.hpp`](../aula3/teclado.hpp).

## Antes de tudo: arquivos do professor

Baixe do site da apostila (`www.lps.usp.br/hae/apostilaraspi/`) para dentro
de `parte_2/aula4/`:

- `quadrado.png` (a placa, 401×401): **obrigatório**, inclusive para o `cliente4`;
- `capturado.avi`, `capturado2.avi`, `capturado3.avi` (vídeos de teste da fase 3);
- `localiza.avi`, `localiza2.avi`, `localiza3.avi` (saídas de referência).

Os `.avi` não entram no git (`*.avi` está no `.gitignore`).

## Compilar e rodar

    ./aula4.sh build                # compila o que faz sentido para este local
    ./aula4.sh fps capturado2.avi   # roda fase3 e fase3p e imprime o fps dos dois

Ou na mão (de dentro de `aula4/`):

    computador$ (cd tarefa1 && compila fase3 -ocv -v3)
    computador$ (cd tarefa2 && compila fase3p -ocv -v3 -omp)
    computador$ (cd tarefa3 && compila cliente4 -ocv -v3 -omp)
    raspberry$  (cd tarefa3 && compila servidor4 -ocv -w)

    computador$ tarefa1/fase3  capturado.avi quadrado.png seu_localiza.avi
    computador$ tarefa2/fase3p capturado.avi quadrado.png seu_localiza.avi

    raspberry$  tarefa3/servidor4 [frenteEsq frenteDir reEsq reDir]
    computador$ tarefa3/cliente4 192.168.15.44 [videosaida.avi] [t/c]

## Lição #1/3: detector (fase 3)

Segue a sugestão da apostila (p. 8):

1. **10 escalas em progressão geométrica**, de 69×69 a 19×19 pixels (placa a
   ~30–150 cm). É geométrica porque o tamanho aparente é proporcional a
   1/distância: com razão constante, o "salto" relativo entre escalas vizinhas
   é o mesmo, perto ou longe. O redimensionamento é com `INTER_NEAREST`, para
   não criar tons intermediários que deixariam de valer exatamente 1.0.
2. **Don't care**: o interior do quadrado amarelo (pixels exatamente
   (255,255,255) → 1.0) vira zero no modelo, com `dcReject(T,1.0)`. Modelo CC:
   `somaAbsDois(dcReject(T,1.0))`. Modelo NCC: `dcReject(T,1.0)`.
   *Cuidado*: `dcReject` altera a imagem recebida (o `Mat` compartilha os
   dados), por isso cada modelo é feito sobre um `clone()`.
3. **CC propõe, NCC confirma**: os 20 maiores picos do máximo de CC entre as
   escalas, separados por ≥ 10 pixels, são os candidatos (ciano). Cada um
   leva o NCC **da mesma posição e escala**. Vence o de maior NCC (amarelo),
   desde que passe de **0,55**.
4. Os números ao lado de cada quadrado são, como na figura 4: escala (0 = maior),
   CC×100 e NCC×100.

Os parâmetros (escalas, número de candidatos, limiar) são constantes no topo
da classe `PLACA`.

## Lição #2/3: OpenMP

O trecho pesado é o laço dos 20 `matchTemplate` (10 CC + 10 NCC) de cada
quadro. As iterações são independentes (cada uma escreve só no seu
resultado), então basta:

    #pragma omp parallel for schedule(dynamic) if(paralelo)
    for (int i=0; i<2*NESC; i++) ...

- `schedule(dynamic)`: os modelos grandes demoram mais que os pequenos, e o
  núcleo que fica livre pega o próximo.
- `if(paralelo)`: `fase3` e `fase3p` usam **o mesmo código**. A única diferença
  é esse flag (e o `-omp` na compilação). Assim a comparação de fps é justa.
- `cv::setNumThreads(1)` nos dois: o OpenCV pode paralelizar o `matchTemplate`
  por dentro, o que faria o "sequencial" usar vários núcleos e brigaria com os
  threads do OpenMP. Assim, quem paraleliza é só o nosso laço.
- O resto (máximo entre escalas, picos, NCC dos candidatos) é leve e fica
  sequencial, como a apostila sugere (p. 18).

O fps é medido **sem `imshow`**. O programa imprime o fps só da localização e
também o fps com leitura e gravação do vídeo.

### fps medido

| Máquina | Sequencial (fps) | Paralelo (fps) |
|---|---|---|
Só a localização, sem `imshow`, num notebook com 2 núcleos físicos e 4 threads
(OpenCV 4.6, g++ -O2):

| Vídeo | Sequencial (fps) | Paralelo (fps) | Ganho |
|---|---|---|---|
| `capturado.avi` (1709 quadros) | 10,1 | 14,8 | 1,5× |
| `capturado2.avi` (575 quadros) | 10,3 | 14,2 | 1,4× |
| `capturado3.avi` (619 quadros) | 10,6 | 17,1 | 1,6× |
| Mint do projeto | _preencher_ | _preencher_ | |

Sim, o paralelo é mais rápido, mas fica longe de 4× porque são só 2 núcleos físicos. Os 4 threads
lógicos (hyper-threading) não rendem 4×, como a apostila avisa (p. 11). Para
conferir que todos os núcleos trabalham, rode o `htop` em outro terminal
durante o `./aula4.sh fps`.

## Lição #3/3: carrinho segue a placa (fase 4)

A parte pesada (localizar a placa) roda no computador. O Raspberry só
captura e move os motores.

**Protocolo** (trava-trava, como na aula 3): o `servidor4` só captura e manda
um quadro (JPEG, 240×320) depois de receber 3 bytes do `cliente4`:

| Bytes | Significado |
|---|---|
| `'m'` esq dir | move: `esq`/`dir` em −100..100 (int8, % da potência calibrada) |
| `'s'` – – | ESC: termina |

**Controle** (como o do professor): a diferença entre as rodas é proporcional
ao desvio horizontal da placa. Só o `x` é usado:

    erro = (x − 160)/160          −1 (toda à esquerda) .. +1 (toda à direita)
    esq  = VEL + GANHO·erro       placa à direita → roda esquerda mais rápida
    dir  = VEL − GANHO·erro       → o carrinho vira para a direita

com `VEL=0,70` e `GANHO=0,50` (topo do `cliente4.cpp`). O carrinho **para**
quando não acha a placa ou quando ela está perto demais (lado aparente ≥
`LADO_PERTO=60` px, as duas maiores escalas, ~30 cm).

Na janela do `cliente4`: os quadrados da fase 3, uma barra verde por roda
(esquerda/direita, para cima = frente) e o estado (`seguindo`, `sem placa:
parado`, `perto: parado`). ESC termina os dois programas.

Vídeo: `t` grava a tela com o debug. `c` grava só a câmera. A tarefa
2024/2025 pede ~1 min gravado com `c`, nomeado
`FulanoDeTal_CiclanoDeQual_Fase4.avi`, para o Drive da disciplina.

**Segurança**: igual ao `servidor2`. Os motores desligam em `'s'`, em queda da
conexão, em Ctrl+C e em `kill`. Se algo der errado: `../aula2/paramotor`.

**Ajuste no carrinho real**: se oscilar (zig-zag), diminua `GANHO`. Se
demorar a corrigir, aumente. Se as rodas não vencerem o atrito com a placa
quase centrada, aumente `VEL`. Se parar longe ou bater, ajuste `LADO_PERTO`.

## Resultados nos vídeos do professor

`./aula4.sh fps` gera `localiza_<video>_seq.avi` e `localiza_<video>_par.avi`:

| Vídeo | Quadros | Placa achada |
|---|---|---|
| `capturado.avi` | 1709 | 863 |
| `capturado2.avi` | 575 | 340 |
| `capturado3.avi` | 619 | 345 |

- Quadro 140 do `capturado2.avi`, a referência da figura 4: achamos escala 9,
  CC 12 e NCC 66. O professor obteve 9, 16 e 75.
- **Falsos positivos**: procuramos detecções "isoladas", que pulam mais de 40 px
  em relação às detecções dos quadros vizinhos (±3). Saiu **1 em 1548**
  (`capturado3`, quadro 129, NCC 0,57, logo acima do limiar). Pelo quadro, é a
  própria placa movida rápido e borrada. Por isso mantivemos `LIMIAR_NCC=0,55`.
- Os quadros sem detecção são a placa fora do quadro, borrada pelo movimento
  (como a figura 5) ou longe demais. Nesses casos o carrinho fica parado, que
  é o erro menos grave.

## Testes sem hardware (vídeo sintético)

Antes de termos os vídeos do professor, testamos com uma placa e um vídeo
sintéticos. A placa foi achada nos 180 quadros que a têm, e o `fase3` e o
`fase3p` geraram vídeos idênticos byte a byte. O fundo sintético tinha
retângulos vazados com formato de placa, que deram falso positivo (NCC ~0,65).
Nos vídeos reais isso não apareceu (ver acima).

### Fase 4 (servidor4/cliente4)

Teste por loopback, com wiringPi simulado e um vídeo no lugar da câmera:
placa à esquerda (x=54) dá esq 30% / dir 80%, placa à direita (x=267) dá
80% / 30%, e placa grande deixa o carrinho parado. Na queda da conexão, os 4
pinos recebem `softPwmStop`. O vídeo `t` saiu válido (~18 fps medidos).

Falta validar no carrinho: câmera, wifi e os ganhos do controle (`VEL`,
`GANHO`, `LADO_PERTO`).
