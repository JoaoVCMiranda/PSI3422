//fase5.cpp - computador - Licao de casa #1/2 da aula 5 (fase 5)
//Igual ao fase3p da aula 4, mas alem de localizar a placa LE O DIGITO
//manuscrito dentro dela, quando a placa esta' perto (escala 0 ou 1), e o
//escreve no video de saida (figura 1b da apostila).
//
//O arquivo tem tres partes:
//  DETECTOR DA PLACA   copiado igual do fase3p (CC + NCC, OpenMP)
//  LEITOR DO DIGITO    o que e' novo: MNIST + FLANN (vizinho mais proximo)
//  PROGRAMA            os passos 1-6 da apostila (p. 4), na ordem
//
//Arquivos no diretorio de onde o programa roda (parte_2/aula5/):
//  capturado.avi, quadrado.png e os 4 arquivos do mnist.zip
//  (train-images.idx3-ubyte, train-labels.idx1-ubyte, t10k-...)
//computador$ compila fase5 -ocv -v3 -omp
//computador$ fase5 capturado.avi quadrado.png locarec.avi
#include "../../projeto.hpp"   //inclui o raspberry.hpp (que inclui o OpenCV)
#ifdef _OPENMP
#include <omp.h>      //omp_get_max_threads (so' existe compilando com -fopenmp)
#endif

//OpenCV 4 removeu os nomes CV_TM_*; os novos (TM_*) existem desde o 3.
#ifndef CV_TM_CCORR
  #define CV_TM_CCORR cv::TM_CCORR
  #define CV_TM_CCOEFF_NORMED cv::TM_CCOEFF_NORMED
#endif

//#########################################################################
// DETECTOR DA PLACA (copiado igual do ../../aula4/tarefa2/fase3p.cpp)
//#########################################################################

//---------------------------------------------------------------------------
// CANDIDATO: um lugar do quadro onde PODE estar a placa.
// (struct normal de C; em C++ nao precisa de typedef para usar so' "CANDIDATO")
//---------------------------------------------------------------------------
struct CANDIDATO {
  int l, c;      //linha e coluna do CENTRO do candidato, no quadro
  int s;         //indice da escala que deu o maior CC ali (0 = maior modelo)
  int lado;      //lado do modelo dessa escala, em pixels (= tamanho aparente)
  FLT cc;        //valor da correlacao cruzada (CC) nesse ponto
  FLT ncc;       //valor da correlacao normalizada (NCC), mesma posicao e escala
};

//---------------------------------------------------------------------------
// PLACA: o detector. Uso:
//    PLACA placa("quadrado.png", paralelo);   //prepara os modelos (uma vez)
//    if (placa.procura(quadroCinza)) ...      //a cada quadro
//    placa.melhor.c, placa.melhor.l, ...      //onde achou
//    placa.desenha(quadro);                   //marca o resultado no quadro
//---------------------------------------------------------------------------
class PLACA {
public:
  //---- Parametros do algoritmo (ajuste aqui) ----
  static constexpr int NESC=10;            //numero de escalas (tamanhos de modelo)
  static constexpr int LADO_MAX=69;        //lado do maior modelo (placa a ~30 cm)
  static constexpr int LADO_MIN=19;        //lado do menor modelo (placa a ~150 cm)
  static constexpr int NCAND=20;           //quantos candidatos o CC propoe
  static constexpr int DIST_MIN=10;        //distancia minima entre candidatos (pixels)
  static constexpr FLT LIMIAR_NCC=0.55;    //NCC minimo para aceitar como placa

private:
  //---- Modelos: calculados UMA vez, no construtor ----
  //Cada um e' um array de NESC imagens, uma por escala (s = 0..NESC-1).
  vector<Mat_<FLT>> modeloCC;    //modeloCC[s]:  imagem do modelo da escala s, preparada para CC
  vector<Mat_<FLT>> modeloNCC;   //modeloNCC[s]: imagem do modelo da escala s, preparada para NCC
  vector<int> lado;              //lado[s]: lado do modelo da escala s, em pixels

  //---- Mapas de correlacao: recalculados A CADA quadro ----
  //Tambem arrays de NESC imagens. mapaCC[s](l,c) = a nota CC do modelo da
  //escala s centrado no pixel (l,c) do quadro. Mesmo tamanho do quadro (240x320).
  vector<Mat_<FLT>> mapaCC;
  vector<Mat_<FLT>> mapaNCC;

  bool paralelo;                 //true = usa OpenMP no PASSO 1

public:
  //---- Resultados do ultimo quadro procurado (o main le estes campos) ----
  vector<CANDIDATO> candidatos;  //array com os ate' NCAND candidatos, em ordem de CC
  CANDIDATO melhor;              //o candidato de maior NCC
  bool achou=false;              //true se melhor.ncc >= LIMIAR_NCC (comeca false)

  //=========================================================================
  // Construtor: roda sozinho em "PLACA placa(arq, paralelo);".
  // Le a placa e prepara os 2*NESC modelos.
  //   arqModelo: caminho do quadrado.png
  //   _paralelo: false = os 20 matchTemplate rodam um depois do outro;
  //              true  = rodam em paralelo (OpenMP), se compilado com -omp
  //=========================================================================
  PLACA(const string& arqModelo, bool _paralelo) {
    paralelo = _paralelo;

    //Os dois programas usam cv::setNumThreads(1): o matchTemplate do OpenCV
    //pode se paralelizar sozinho por dentro, o que (a) faria a versao
    //"sequencial" usar varios nucleos e (b) brigaria com os threads do
    //OpenMP na versao paralela. Assim quem paraleliza e' so' o nosso laco.
    setNumThreads(1);                                 //[OpenCV]

    //---- Le quadrado.png e converte para niveis de cinza 0..1 ----
    //A placa tem 401x401: preto, branco e o interior do quadrado amarelo,
    //que ao virar cinza fica exatamente 1.0 (branco puro).
    Mat_<COR> placaCor = imread(arqModelo, 1);        //[OpenCV] 1 = ler colorida
    if (placaCor.empty()) erro("Erro leitura "+arqModelo);   //[raspberry.hpp]
    Mat_<FLT> placaCinza;
    converte(placaCor, placaCinza);                   //[raspberry.hpp] cor -> cinza 0..1

    //---- Gera os NESC tamanhos, de LADO_MAX a LADO_MIN, e para cada um os
    //dois modelos (CC e NCC). Ao final: modeloCC, modeloNCC e lado tem NESC
    //elementos. ----
    //PROGRESSAO GEOMETRICA: cada escala e' a anterior vezes "razao".
    //Por que nao aritmetica? O tamanho aparente da placa e' inversamente
    //proporcional a distancia. Com razao constante, o salto RELATIVO entre
    //escalas vizinhas e' o mesmo para a placa perto ou longe.
    //  razao^(NESC-1) = LADO_MIN/LADO_MAX  =>  razao = (19/69)^(1/9) ~ 0,866
    double razao = pow(double(LADO_MIN)/LADO_MAX, 1.0/(NESC-1));   //[C]

    for (int s=0; s<NESC; s++) {
      //lados: 69, 60, 52, 45, 39, 34, 29, 25, 22, 19
      int n = int(round(LADO_MAX*pow(razao,s)));     //[C]

      //INTER_NEAREST (vizinho mais proximo) e nao interpolacao: interpolar
      //criaria tons como 0.97 na borda do quadrado amarelo, que deixariam
      //de ser exatamente 1.0 e o "don't care" abaixo nao os pegaria.
      Mat_<FLT> placaEscalada;
      resize(placaCinza, placaEscalada, Size(n,n), 0, 0, INTER_NEAREST);  //[OpenCV] 401x401 -> nxn

      //"DON'T CARE": o interior do quadrado amarelo (onde vai um digito
      //escrito a mao) vale 1.0 e nao deve influir na comparacao.
      //dcReject(T, 1.0) faz duas coisas (ver ../../raspberry.hpp):
      //  - subtrai a media dos pixels que NAO sao 1.0 (tira o nivel DC);
      //  - zera os pixels que sao 1.0 (zero nao soma nada na correlacao).
      //CUIDADO: dcReject ALTERA a imagem recebida (um Mat compartilha os
      //dados), por isso cada modelo e' feito sobre um clone().
      Mat_<FLT> semDC = dcReject(placaEscalada.clone(), 1.0);   //[raspberry.hpp]

      //Modelo NCC: so' sem DC. O TM_CCOEFF_NORMED ja' normaliza sozinho.
      modeloNCC.push_back(semDC);                     //acrescenta no fim do array

      //Modelo CC: sem DC e com soma dos valores absolutos = 2 (somaAbsDois).
      //Os pixels brancos somam +1 e os pretos -1, entao a nota CC vira
      //"media sob o branco - media sob o preto", na mesma faixa para modelos
      //grandes e pequenos: o maximo entre escalas (PASSO 2) e' justo.
      modeloCC.push_back(somaAbsDois(dcReject(placaEscalada.clone(), 1.0)));  //[raspberry.hpp]

      lado.push_back(n);
    }

    //os mapas sao preenchidos pelo procura(); aqui so' criamos as NESC vagas
    mapaCC.resize(NESC);
    mapaNCC.resize(NESC);
  }

  //Quantos threads o PASSO 1 usa (so' para imprimir na tela).
  int threads() const {
#ifdef _OPENMP
    if (paralelo) return omp_get_max_threads();      //[OpenMP] numero de nucleos
    else return 1;
#else
    return 1;   //compilado sem -omp: o #pragma omp e' ignorado
#endif
  }

  //=========================================================================
  // procura: roda os 4 passos no quadro f (cinza, 0..1). Devolve achou.
  //=========================================================================
  bool procura(const Mat_<FLT>& f) {
    //---------------------------------------------------------------------
    // PASSO 1 - os 2*NESC template matchings (parte pesada)
    //---------------------------------------------------------------------
    //Sao 20 tarefas: i = 0..19. A tarefa i trabalha na escala i/2 e:
    //   i par   -> CC  da escala i/2   (i=0: CC escala 0,  i=2: CC escala 1, ...)
    //   i impar -> NCC da escala i/2   (i=1: NCC escala 0, i=3: NCC escala 1, ...)
    //
    //As 20 tarefas sao INDEPENDENTES: cada uma escreve so' no seu mapa
    //(mapaCC[s] ou mapaNCC[s]) e so' le o quadro f. Entao podem rodar em
    //qualquer ordem, ao mesmo tempo, em nucleos diferentes - e' o que o
    //#pragma abaixo pede ao OpenMP:
    //   parallel for      divide as iteracoes do for entre os threads
    //   schedule(dynamic) o thread que termina pega a proxima iteracao livre
    //                     (os modelos grandes demoram mais que os pequenos;
    //                     dividir em blocos fixos deixaria nucleos ociosos)
    //   if(paralelo)      so' paraleliza se paralelo==true. Assim fase3 e
    //                     fase3p usam EXATAMENTE o mesmo codigo.
    //Sem -omp na compilacao, o #pragma e' ignorado e o for roda normal.
    #pragma omp parallel for schedule(dynamic) if(paralelo)
    for (int i=0; i<2*NESC; i++) {
      int s = i/2;                  //escala desta tarefa
      bool ehCC = (i%2==0);         //par = CC, impar = NCC

      //matchTemplateSame [raspberry.hpp]: chama o matchTemplate do OpenCV,
      //mas devolve um mapa do MESMO tamanho do quadro, com o valor de cada
      //posicao no CENTRO do modelo. As bordas, onde o modelo nao cabe
      //inteiro, ficam com 0.0.
      //  CV_TM_CCORR         = correlacao cruzada (CC): soma de modelo x imagem
      //  CV_TM_CCOEFF_NORMED = coeficiente de correlacao (NCC), -1..+1
      if (ehCC) mapaCC[s]  = matchTemplateSame(f, modeloCC[s],  CV_TM_CCORR,         0.0);
      else      mapaNCC[s] = matchTemplateSame(f, modeloNCC[s], CV_TM_CCOEFF_NORMED, 0.0);
    }

    //---------------------------------------------------------------------
    // PASSO 2 - um mapa so', com o melhor CC de cada pixel
    //---------------------------------------------------------------------
    //Para cada pixel (l,c): qual escala deu o maior CC, e quanto foi.
    //Comeca supondo que e' a escala 0 e troca sempre que outra for maior.
    Mat_<FLT> ccMaximo = mapaCC[0].clone();         //ccMaximo(l,c) = maior CC entre as escalas
    Mat_<int> escalaMaximo(f.rows, f.cols, 0);      //escalaMaximo(l,c) = escala desse maior CC
    for (int s=1; s<NESC; s++)
      for (int l=0; l<f.rows; l++)
        for (int c=0; c<f.cols; c++)
          if (mapaCC[s](l,c) > ccMaximo(l,c)) {
            ccMaximo(l,c) = mapaCC[s](l,c);
            escalaMaximo(l,c) = s;
          }

    //---------------------------------------------------------------------
    // PASSO 3 - os NCAND maiores picos de CC
    //---------------------------------------------------------------------
    //Repete NCAND vezes: pega o maior valor do mapa e depois "apaga" (poe -1)
    //um quadrado de lado 2*DIST_MIN+1 em volta dele. Sem apagar, os 20
    //maiores valores seriam todos pixels vizinhos do MESMO pico.
    //(Por isso o ccMaximo e' uma copia, feita no PASSO 2: aqui ele e' alterado.)
    candidatos.clear();                         //esvazia o array do quadro anterior
    for (int k=0; k<NCAND; k++) {
      //minMaxLoc [OpenCV]: varre a imagem e devolve o maior valor (vmax) e
      //onde ele esta' (p). Os NULL sao o menor valor e onde, que nao usamos.
      double vmax; Point p;
      minMaxLoc(ccMaximo, NULL, &vmax, NULL, &p);
      if (vmax<=0) break;    //so' sobrou fundo (ou area ja' apagada): acabou

      CANDIDATO cd;
      cd.l = p.y;                       //Point usa (x,y) = (coluna,linha)
      cd.c = p.x;
      cd.s = escalaMaximo(p.y, p.x);
      cd.lado = lado[cd.s];
      cd.cc = (FLT)vmax;
      cd.ncc = mapaNCC[cd.s](p.y, p.x); //NCC na MESMA posicao e escala
      candidatos.push_back(cd);         //acrescenta no fim do array

      //apaga a vizinhanca: o quadrado de linhas l-DIST_MIN..l+DIST_MIN e
      //colunas c-DIST_MIN..c+DIST_MIN, cortado nas bordas da imagem
      int lIni = max(cd.l-DIST_MIN, 0), lFim = min(cd.l+DIST_MIN, ccMaximo.rows-1);
      int cIni = max(cd.c-DIST_MIN, 0), cFim = min(cd.c+DIST_MIN, ccMaximo.cols-1);
      for (int l=lIni; l<=lFim; l++)
        for (int c=cIni; c<=cFim; c++)
          ccMaximo(l,c) = -1;
    }

    //---------------------------------------------------------------------
    // PASSO 4 - o candidato de maior NCC, se passar do limiar
    //---------------------------------------------------------------------
    achou = false;
    for (int k=0; k<(int)candidatos.size(); k++) {
      bool primeiro = !achou;
      if (primeiro || candidatos[k].ncc > melhor.ncc) {
        melhor = candidatos[k];         //copia a struct inteira, como em C
        achou = true;
      }
    }
    //Ate' aqui "achou" so' quer dizer "havia pelo menos um candidato".
    //Agora a decisao de verdade: o NCC tem que ser alto o bastante.
    if (achou && melhor.ncc < LIMIAR_NCC) achou = false;
    return achou;
  }

  //=========================================================================
  // desenha: marca o resultado no quadro colorido a, como na figura 4:
  //   ciano   = cada candidato, com 3 numeros: escala, CC x100, NCC x100
  //   amarelo = a placa encontrada (so' se achou)
  //=========================================================================
  void desenha(Mat_<COR>& a) const {
    //---- Cada candidato: quadrado ciano + 3 numeros empilhados a direita ----
    const Scalar ciano(255,255,0);         //cor na ordem B,G,R
    for (int k=0; k<(int)candidatos.size(); k++) {
      const CANDIDATO& cd = candidatos[k];
      int metade = cd.lado/2;
      //Rect(coluna do canto, linha do canto, largura, altura)
      rectangle(a, Rect(cd.c-metade, cd.l-metade, cd.lado, cd.lado), ciano, 1);   //[OpenCV]

      int x = cd.c+metade+2;               //coluna: logo a direita do quadrado
      int y = cd.l-metade+8;               //linha: alinhado com o topo do quadrado
      int numeros[3] = { cd.s, int(round(cd.cc*100)), int(round(cd.ncc*100)) };
      for (int j=0; j<3; j++) {
        //putText [OpenCV]: escreve o texto com o canto inferior esquerdo em (x,y)
        putText(a, to_string(numeros[j]), Point(x,y), FONT_HERSHEY_PLAIN, 0.6, ciano, 1);
        y += 8;                            //proxima linha
      }
    }

    //---- A placa encontrada: quadrado amarelo (mais grosso) + "escala CC NCC" ----
    if (achou) {
      const Scalar amarelo(0,255,255);     //cor na ordem B,G,R
      int metade = melhor.lado/2;
      rectangle(a, Rect(melhor.c-metade, melhor.l-metade, melhor.lado, melhor.lado), amarelo, 2);  //[OpenCV]

      //monta o texto "escala CC NCC", ex.: "5 20 70" ("+" junta strings)
      string texto = to_string(melhor.s) + " " +
                     to_string(int(round(melhor.cc*100))) + " " +
                     to_string(int(round(melhor.ncc*100)));
      putText(a, texto, Point(melhor.c-metade, melhor.l-metade-3),
              FONT_HERSHEY_PLAIN, 0.8, amarelo, 1);                                   //[OpenCV]
    }
  }
};

//#########################################################################
// LEITOR DO DIGITO (novo na fase 5)
//#########################################################################

//---------------------------------------------------------------------------
// DIGITO: le o digito manuscrito dentro da placa. Uso:
//    DIGITO digito("quadrado.png", ".");      //le o MNIST e treina (uma vez)
//    if (digito.le(placa, quadro, quadroCinza)) ...   //a cada quadro
//    digito.digito                            //o digito lido, 0..9
//    quadroSaida = digito.desenha(quadro, placa);     //quadro + paineis
//
// Os passos sao os da apostila (p. 4):
//   1)  treino: MNIST (60000 imagens de TREINO) + FLANN       (construtor)
//   4a) recorta o interior do quadrado amarelo                (le)
//   4b) ajusta brilho/contraste: fundo branco, tinta preta    (le)
//   4c) mnist.bbox: tira as bordas brancas e reduz a 14x14    (le)
//   4d) FLANN: o rotulo da imagem de treino mais parecida     (le)
//---------------------------------------------------------------------------
class DIGITO {
public:
  //---- Parametros (ajuste aqui) ----
  static constexpr int NLADO=14;           //lado das imagens do MNIST (o original e' 28)
  static constexpr int ESCALA_PERTO=1;     //so' le com a placa na escala 0 ou 1 (perto)
  //Folga tirada de cada lado do recorte (fracao do lado do quadrado amarelo).
  //Sem ela, a linha amarela ou uma localizacao 1-2 pixels fora poem uma
  //borda escura no recorte, e o bbox (4c) acharia a borda em vez do digito.
  static constexpr double FOLGA=0.10;
  //Contraste minimo (fundo - tinta) para dizer que ha' um digito escrito.
  //Abaixo disso o recorte e' so' fundo (placa sem digito) e o ajuste de
  //contraste (4b) so' amplificaria ruido.
  static constexpr FLT CONTRASTE_MIN=0.15;

private:
  //MnistFlann [raspberry.hpp]: a classe MNIST (le os arquivos) + o FLANN.
  //E' um objeto dentro de outro: e' construido antes do corpo do construtor
  //do DIGITO, com os argumentos dados em ": mnist(...)" (ver construtor).
  MnistFlann mnist;

  Rect interior;      //interior do quadrado amarelo, em pixels do quadrado.png
  int ladoPlaca;      //lado do quadrado.png (401)

public:
  //---- Resultado da ultima leitura (o main e o desenha leem estes campos) ----
  bool leu=false;          //true se conseguiu ler um digito neste quadro
  int digito=-1;           //o digito lido, 0..9 (-1 = nenhum)
  //As imagens intermediarias, para o video (a apostila recomenda olha-las:
  //e' assim que se descobre ONDE o programa erra):
  Mat_<COR> recorte;       //4a: o pedaco do quadro com o digito, colorido
  Mat_<FLT> ajustada;      //4b: o mesmo pedaco, cinza, depois do contraste
  Mat_<FLT> pequena;       //4c: 14x14, exatamente o que o FLANN compara

  //=========================================================================
  // Construtor: acha o interior do quadrado amarelo e faz o PASSO 1.
  //   arqPlaca: caminho do quadrado.png
  //   dirMnist: diretorio dos arquivos do MNIST
  // ": mnist(NLADO, true, true)" constroi o objeto mnist com:
  //   NLADO=14  as imagens sao reduzidas de 28x28 para 14x14
  //   true      inverte as cores: fundo branco e digito preto, como na placa
  //   true      ajusta o bbox: tira as linhas/colunas brancas da borda
  //=========================================================================
  DIGITO(const string& arqPlaca, const string& dirMnist) : mnist(NLADO, true, true) {
    //---- Acha o interior do quadrado amarelo, onde se escreve o digito ----
    //E' a unica regiao do quadrado.png com pixels EXATAMENTE (255,255,255) -
    //a mesma que e' "don't care" no detector. Medir no arquivo evita numeros
    //magicos no codigo (no quadrado.png: linhas e colunas 121 a 279).
    Mat_<COR> placaCor = imread(arqPlaca, 1);         //[OpenCV] 1 = ler colorida
    if (placaCor.empty()) erro("Erro leitura "+arqPlaca);   //[raspberry.hpp]

    int cima=placaCor.rows, baixo=-1, esq=placaCor.cols, dir=-1;
    for (int l=0; l<placaCor.rows; l++)
      for (int c=0; c<placaCor.cols; c++)
        if (placaCor(l,c) == COR(255,255,255)) {      //branco puro
          cima = min(cima, l);  baixo = max(baixo, l);
          esq  = min(esq, c);   dir   = max(dir, c);
        }
    if (baixo<0) erro("Erro: "+arqPlaca+" sem o interior branco (255,255,255)");

    //Rect(coluna do canto, linha do canto, largura, altura)
    interior = Rect(esq, cima, dir-esq+1, baixo-cima+1);
    ladoPlaca = placaCor.rows;

    //---- PASSO 1: le o MNIST e treina o FLANN ----
    //So' as 60000 imagens de TREINO (o 0 pede para nao ler as 10000 de teste):
    //o conjunto de teste serve para MEDIR o erro de um algoritmo, nao para o
    //programa final usar.
    //Depois do le(), mnist.ax tem uma imagem 14x14 por linha (196 colunas,
    //0=preto 1=branco) e mnist.ay o rotulo de cada uma (0..9).
    //"Treinar" o FLANN e' so' montar uma arvore de busca sobre as linhas de
    //mnist.ax, para achar rapido a mais parecida com uma imagem nova.
    double t0 = timeSinceEpoch();                     //[raspberry.hpp]
    mnist.le(dirMnist, 60000, 0);                     //[raspberry.hpp] le os arquivos
    mnist.train();                                    //[raspberry.hpp] arvore do FLANN
    printf("MNIST lido e FLANN treinado em %.1f s\n", timeSinceEpoch()-t0);
  }

  //=========================================================================
  // le: os passos 4a-4d no quadro atual. Devolve true se leu um digito.
  //   placa:       o detector, ja' com o resultado deste quadro
  //   quadro:      o quadro colorido (para o recorte que vai no video)
  //   quadroCinza: o mesmo quadro em cinza 0..1 (para ler o digito)
  // Se um passo falha, a funcao devolve false ali mesmo, e as imagens dos
  // passos seguintes (recorte, ajustada, pequena) ficam como estavam.
  //=========================================================================
  bool le(const PLACA& placa, const Mat_<COR>& quadro, const Mat_<FLT>& quadroCinza) {
    leu = false;
    digito = -1;

    //PASSO 4: so' le se achou a placa e ela esta' perto (escala 0 ou 1).
    //Longe, o digito tem poucos pixels e sai borrado demais para ler.
    if (!placa.achou) return false;
    if (placa.melhor.s > ESCALA_PERTO) return false;

    //---------------------------------------------------------------------
    // PASSO 4a - recorta o interior do quadrado amarelo, no quadro
    //---------------------------------------------------------------------
    //O detector diz onde esta' o CENTRO da placa (cd.l, cd.c) e o lado do
    //modelo que casou (cd.lado). O modelo e' o quadrado.png (401x401)
    //reduzido para cd.lado x cd.lado, entao um pixel (y,x) do quadrado.png
    //cai, no quadro, em:
    //   canto + (y,x) * k,   com k = cd.lado/401 e canto = centro - (lado-1)/2
    //(o "(lado-1)/2" e' o mesmo que o matchTemplateSame usa para o centro).
    //Saida: recorte (colorido, para o video) e cinza (para os proximos passos).
    const CANDIDATO& cd = placa.melhor;
    double k = double(cd.lado)/ladoPlaca;            //escala placa -> quadro
    int cantoL = cd.l - (cd.lado-1)/2;               //canto superior esquerdo
    int cantoC = cd.c - (cd.lado-1)/2;               //do modelo, no quadro
    double folga = FOLGA*interior.width;             //em pixels do quadrado.png

    //cantos do interior (menos a folga), convertidos para o quadro
    int l0 = cantoL + int(round((interior.y + folga)*k));
    int c0 = cantoC + int(round((interior.x + folga)*k));
    int l1 = cantoL + int(round((interior.y + interior.height - folga)*k));
    int c1 = cantoC + int(round((interior.x + interior.width  - folga)*k));
    Rect r(c0, l0, c1-c0, l1-l0);

    //o recorte tem que estar inteiro dentro do quadro ("&" = intersecao)
    Rect quadroTodo(0, 0, quadroCinza.cols, quadroCinza.rows);
    if ((r & quadroTodo) != r) return false;
    if (r.width<4 || r.height<4) return false;      //pequeno demais para ler

    //clone(): copia de verdade. Sem ele, recorte seria uma "janela" que
    //aponta para os pixels do quadro, e mudaria quando o quadro mudasse.
    recorte = quadro(r).clone();                     //[OpenCV]
    Mat_<FLT> cinza = quadroCinza(r).clone();        //[OpenCV]

    //---------------------------------------------------------------------
    // PASSO 4b - brilho/contraste: fundo -> branco (1.0), tinta -> preto (0.0)
    //---------------------------------------------------------------------
    //No video, o fundo da placa e' cinza claro (nao 1.0) e a tinta cinza
    //escura (nao 0.0), e os dois mudam com a iluminacao. O MNIST invertido
    //tem fundo exatamente 1.0. Entao esticamos os tons:
    //   preto  = o pixel mais escuro do recorte       (a tinta)
    //   branco = a MEDIANA dos pixels do recorte      (o fundo: a maioria dos
    //            pixels e' fundo, entao o valor do meio e' um pixel de fundo)
    //   novo   = (velho - preto)/(branco - preto), cortado em 0..1
    //Tudo que e' tao claro quanto o fundo vira 1.0 (branco); a tinta vira
    //0.0; a borda do traco fica cinza, como no MNIST.
    //mediana: nth_element [C++] poe no meio do vetor o valor que estaria
    //ali se o vetor fosse ordenado (sem ordenar tudo, mais rapido)
    vector<FLT> v(cinza.begin(), cinza.end());       //copia os pixels num vetor
    int meio = int(v.size())/2;
    nth_element(v.begin(), v.begin()+meio, v.end());
    FLT branco = v[meio];

    double preto;
    minMaxLoc(cinza, &preto, NULL);                  //[OpenCV] o menor valor

    if (branco - preto < CONTRASTE_MIN) return false;   //nao tem tinta: placa vazia

    ajustada.create(cinza.rows, cinza.cols);         //[OpenCV] aloca do mesmo tamanho
    for (int l=0; l<cinza.rows; l++)
      for (int c=0; c<cinza.cols; c++) {
        FLT novo = FLT((cinza(l,c) - preto)/(branco - preto));
        ajustada(l,c) = min(FLT(1), max(FLT(0), novo));   //corta em 0..1
      }

    //---------------------------------------------------------------------
    // PASSO 4c - bbox e 14x14, como o MNIST fez com as imagens de treino
    //---------------------------------------------------------------------
    //mnist.bbox [raspberry.hpp] (versao FLT):
    //  - acha as linhas/colunas com algum pixel <= 0.5 (tinta) e corta as
    //    bordas brancas em volta;
    //  - redimensiona o que sobrou para 14x14.
    //Sem isso, um "1" pequeno no canto e um "1" grande no meio seriam imagens
    //muito diferentes para o FLANN. Se nao achar tinta, mnist.localizou=false.
    pequena = mnist.bbox(ajustada);                  //[raspberry.hpp]
    if (!mnist.localizou) return false;

    //---------------------------------------------------------------------
    // PASSO 4d - FLANN: o vizinho mais proximo
    //---------------------------------------------------------------------
    //A consulta tem que ter o mesmo formato de uma linha de mnist.ax: 1 linha
    //com os 196 pixels (14x14 lidos linha por linha). predictInterno
    //[raspberry.hpp] acha a imagem de treino mais parecida e devolve o
    //rotulo dela.
    //(MnistFlann::predict tambem existiria, mas ele mesmo chama o bbox de
    //novo; aqui o bbox ja' foi feito no 4c, para podermos mostrar a imagem.)
    Mat_<FLT> consulta(1, NLADO*NLADO);              //1 linha x 196 colunas
    for (int i=0; i<NLADO*NLADO; i++)
      consulta(i) = pequena(i);                      //pequena(i): i-esimo pixel
    digito = int(mnist.predictInterno(consulta));    //[raspberry.hpp]
    leu = true;
    return true;
  }

  //=========================================================================
  // desenha: monta o quadro de saida, como na figura 1b. Devolve uma imagem
  // MAIS LARGA que o quadro: a direita, uma coluna com dois paineis.
  //
  //   +---------------------------+--------+
  //   |                           | 14x14  |  em cima: o que o FLANN ve' (4c)
  //   |   quadro (com a placa     |        |
  //   |   marcada e o digito em   +--------+
  //   |   vermelho ao lado dela)  | recorte|  embaixo: o recorte do quadro
  //   |                           |   + d  |  (4a) com o digito lido
  //   +---------------------------+--------+
  //   240 linhas x (320 + 120) colunas
  //=========================================================================
  Mat_<COR> desenha(const Mat_<COR>& quadro, const PLACA& placa) const {
    int lp = quadro.rows/2;             //lado de cada painel: 120
    Mat_<COR> saida(quadro.rows, quadro.cols+lp, COR(128,128,128));   //fundo cinza
    //copia o quadro para a parte esquerda da saida
    quadro.copyTo(saida(Rect(0, 0, quadro.cols, quadro.rows)));      //[OpenCV]
    if (!leu) return saida;             //nao leu: os paineis ficam cinzas

    const Scalar vermelho(0,0,255);     //cor na ordem B,G,R

    //---- O digito lido, grande e vermelho, a esquerda da placa ----
    const CANDIDATO& cd = placa.melhor;
    Point p(cd.c - cd.lado/2 - 32, cd.l + 16);       //(coluna, linha) do texto
    putText(saida, to_string(digito), p, FONT_HERSHEY_SIMPLEX, 1.4, vermelho, 3);   //[OpenCV]

    //---- Painel de cima: a imagem 14x14 ampliada SEM interpolar
    //(INTER_NEAREST), para ver cada pixel como um quadradinho - e' o que o
    //FLANN recebeu ----
    Mat_<COR> painel;
    converte(pequena, painel);                       //[raspberry.hpp] FLT 0..1 -> cor
    resize(painel, painel, Size(lp,lp), 0, 0, INTER_NEAREST);    //[OpenCV]
    painel.copyTo(saida(Rect(quadro.cols, 0, lp, lp)));          //[OpenCV]

    //---- Painel de baixo: o recorte original do quadro, ampliado, com o
    //digito lido ----
    resize(recorte, painel, Size(lp,lp), 0, 0, INTER_LINEAR);   //[OpenCV]
    putText(painel, to_string(digito), Point(lp/2+8, lp-14),
            FONT_HERSHEY_SIMPLEX, 1.6, vermelho, 3);              //[OpenCV]
    painel.copyTo(saida(Rect(quadro.cols, lp, lp, lp)));         //[OpenCV]
    return saida;
  }
};

//#########################################################################
// PROGRAMA: os passos da apostila (p. 4)
//   fase5 capturado.avi quadrado.png locarec.avi
//#########################################################################
int main(int argc, char** argv) {
#ifndef _OPENMP
  //_OPENMP so' existe quando se compila com -fopenmp
  printf("AVISO: compilado sem -omp, a localizacao vai rodar sequencial\n");
#endif
  bool paralelo = true;    //PARALELO: os 20 template matchings divididos entre os nucleos

  //---- argumentos ----
  string prog = argv[0];
  if (argc!=4) erro(prog+" capturado.avi quadrado.png locarec.avi");   //[raspberry.hpp]
  string arqEntrada = argv[1], arqPlaca = argv[2], arqSaida = argv[3];

  //---- PASSO 1: le o MNIST e treina o FLANN; prepara os modelos da placa ----
  //acha o diretorio dos arquivos do MNIST, rodando de aula5/ ou de
  //aula5/tarefa1/ ("." = diretorio atual, ".." = o de cima)
  const char* lugares[2] = { ".", ".." };
  string dirMnist = "";
  for (int i=0; i<2 && dirMnist==""; i++) {
    string arq = string(lugares[i]) + "/train-images.idx3-ubyte";
    FILE* f = fopen(arq.c_str(), "rb");              //[C] consegue abrir?
    if (f) { fclose(f); dirMnist = lugares[i]; }
  }
  if (dirMnist=="") erro("Erro: nao achei train-images.idx3-ubyte (descompacte o mnist.zip em parte_2/aula5/)");   //[raspberry.hpp]
  DIGITO digito(arqPlaca, dirMnist);
  PLACA placa(arqPlaca, paralelo);
  printf("%s: localizando com %d thread(s)\n", prog.c_str(), placa.threads());

  //---- abre o video de entrada e o de saida ----
  VideoCapture entrada(arqEntrada);                  //[OpenCV]
  if (!entrada.isOpened()) erro("Erro: Abertura de video "+arqEntrada);

  const int nl=240, nc=320;                          //tamanho de trabalho
  double fpsVideo = entrada.get(CAP_PROP_FPS);       //[OpenCV] fps gravado no arquivo
  if (!(fpsVideo>0 && fpsVideo<100)) fpsVideo=20;   //fps invalido: usa 20
  //O video de saida e' MAIS LARGO que o quadro: 320 + 120 colunas, por
  //causa da coluna com as imagens intermediarias (ver DIGITO::desenha).
  //Size(largura,altura) = Size(colunas,linhas)
  VideoWriter saida(arqSaida, VideoWriter::fourcc('X','V','I','D'), fpsVideo,
                    Size(nc + nl/2, nl));            //[OpenCV]
  if (!saida.isOpened()) erro("Erro: nao consegui abrir "+arqSaida);

  //---- laco: um quadro por vez ----
  Mat_<COR> quadro;          //colorido, para desenhar e gravar
  Mat_<FLT> quadroCinza;     //cinza 0..1, para procurar e ler
  int quadros=0, achados=0, lidos=0;
  int contagem[10] = {0};    //contagem[d] = em quantos quadros leu o digito d
  double tempoProcessamento=0;           //soma so' do converte+procura+le
  double inicio = timeSinceEpoch();      //[raspberry.hpp]
  while (true) {
    //PASSO 2: pega um quadro; se acabou o video, termina
    entrada.read(quadro);                //[OpenCV]
    if (quadro.empty()) break;
    if (quadro.rows!=nl || quadro.cols!=nc) resize(quadro, quadro, Size(nc,nl));   //[OpenCV]

    double t1 = timeSinceEpoch();
    converte(quadro, quadroCinza);       //[raspberry.hpp] cor -> cinza 0..1

    //PASSO 3: localiza a placa
    if (placa.procura(quadroCinza)) achados++;

    //PASSO 4: se a placa esta' perto, le o digito (4a-4d dentro do le)
    if (digito.le(placa, quadro, quadroCinza)) {
      lidos++;
      contagem[digito.digito]++;
    }
    tempoProcessamento += timeSinceEpoch()-t1;

    //PASSOS 3 e 4e: marca a placa e escreve o digito lido no quadro
    placa.desenha(quadro);               //quadrados ciano e amarelo
    Mat_<COR> quadroSaida = digito.desenha(quadro, placa);

    //PASSO 5: grava o quadro no video de saida
    saida.write(quadroSaida);            //[OpenCV]
    quadros++;
  }   //PASSO 6: volta ao PASSO 2
  double tempoTotal = timeSinceEpoch()-inicio;

  //---- relatorio ----
  if (quadros==0) erro("Erro: video sem quadros");
  printf("%d quadros, placa achada em %d, digito lido em %d\n", quadros, achados, lidos);
  printf("digitos lidos (digito:quadros):");
  for (int d=0; d<10; d++)
    if (contagem[d]>0) printf("  %d:%d", d, contagem[d]);
  printf("\n");
  printf("localizacao + leitura: %.1f fps\n", quadros/tempoProcessamento);
  printf("com leitura e gravacao do video: %.1f fps\n", quadros/tempoTotal);
  return 0;
}
