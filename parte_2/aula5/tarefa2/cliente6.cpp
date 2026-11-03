//cliente6.cpp - computador - Licao de casa #2/2 da aula 5 (fase 6)
//O carrinho segue o caminho indicado pelos digitos escritos nas placas.
//Toda a logica fica aqui; o servidor6 so' captura e move as rodas.
//
//O arquivo tem quatro partes:
//  DETECTOR DA PLACA    copiado igual do cliente4 (aula 4)
//  LEITOR DO DIGITO     copiado igual do fase5 (licao #1/2)
//  MAQUINA DE ESTADOS   o que e' novo: a figura 6 da apostila
//  TELA E PROGRAMA      mostra, grava e conversa com o servidor6
//
//Arquivos no diretorio de onde o programa roda (parte_2/aula5/):
//  quadrado.png e os 4 arquivos do mnist.zip
//computador$ compila cliente6 -ocv -v3 -omp
//computador$ cliente6 192.168.0.110 [videosaida.avi] [t/c]
//  t = grava a tela (com o debug, default);  c = so' a camera
//ESC (com a janela em foco) termina os dois programas.
#include "../../projeto.hpp"   //inclui o raspberry.hpp (que inclui o OpenCV)
#ifdef _OPENMP
#include <omp.h>      //omp_get_max_threads (so' existe compilando com -fopenmp)
#endif

//OpenCV 4 removeu os nomes CV_TM_*; os novos (TM_*) existem desde o 3.
#ifndef CV_TM_CCORR
  #define CV_TM_CCORR cv::TM_CCORR
  #define CV_TM_CCOEFF_NORMED cv::TM_CCOEFF_NORMED
#endif
#include "../../aula3/teclado.hpp"   //GRAVADOR e ARGCLIENTE

//#########################################################################
// DETECTOR DA PLACA (copiado igual do ../../aula4/tarefa3/cliente4.cpp)
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

    Mat_<FLT> placaCinza = lePlaca(arqModelo);
    preparaModelos(placaCinza);

    //os mapas sao preenchidos pelo procura(); aqui so' criamos as NESC vagas
    mapaCC.resize(NESC);
    mapaNCC.resize(NESC);
  }

  int ladoEscala(int s) const { return lado[s]; }

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
    calculaMapas(f);                              //PASSO 1 (pesado)

    Mat_<FLT> ccMaximo;      //ccMaximo(l,c)   = maior CC entre as escalas
    Mat_<int> escalaMaximo;  //escalaMaximo(l,c) = escala que deu esse maior CC
    maximoEntreEscalas(f.rows, f.cols, ccMaximo, escalaMaximo);   //PASSO 2

    escolheCandidatos(ccMaximo, escalaMaximo);    //PASSO 3
    escolheMelhor();                              //PASSO 4
    return achou;
  }

  //=========================================================================
  // desenha: marca o resultado no quadro colorido a, como na figura 4:
  //   ciano   = cada candidato, com 3 numeros: escala, CC x100, NCC x100
  //   amarelo = a placa encontrada (so' se achou)
  //=========================================================================
  void desenha(Mat_<COR>& a) const {
    for (int k=0; k<(int)candidatos.size(); k++)
      desenhaCandidato(a, candidatos[k]);
    if (achou)
      desenhaMelhor(a);
  }

private:
  //=========================================================================
  // Preparacao (construtor)
  //=========================================================================

  //Le quadrado.png e converte para niveis de cinza 0..1.
  //A placa tem 401x401: preto, branco e o interior do quadrado amarelo,
  //que ao virar cinza fica exatamente 1.0 (branco puro).
  //("static": esta funcao nao usa nenhum campo da classe)
  static Mat_<FLT> lePlaca(const string& arqModelo) {
    Mat_<COR> placaCor = imread(arqModelo, 1);        //[OpenCV] 1 = ler colorida
    if (placaCor.empty()) erro("Erro leitura "+arqModelo);   //[raspberry.hpp]
    Mat_<FLT> placaCinza;
    converte(placaCor, placaCinza);                   //[raspberry.hpp] cor -> cinza 0..1
    return placaCinza;
  }

  //Gera os NESC tamanhos, de LADO_MAX a LADO_MIN, e para cada um os dois
  //modelos (CC e NCC). Ao final: modeloCC, modeloNCC e lado tem NESC elementos.
  void preparaModelos(const Mat_<FLT>& placaCinza) {
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
  }

  //=========================================================================
  // PASSO 1 - calculaMapas: os 2*NESC template matchings (parte pesada)
  //=========================================================================
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
  void calculaMapas(const Mat_<FLT>& f) {
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
  }

  //=========================================================================
  // PASSO 2 - maximoEntreEscalas: um mapa so', com o melhor CC de cada pixel
  //=========================================================================
  //Para cada pixel (l,c): qual escala deu o maior CC, e quanto foi.
  //Comeca supondo que e' a escala 0 e troca sempre que outra for maior.
  //(ccMaximo e escalaMaximo sao referencias: sao as variaveis do procura())
  void maximoEntreEscalas(int nl, int nc, Mat_<FLT>& ccMaximo, Mat_<int>& escalaMaximo) const {
    ccMaximo = mapaCC[0].clone();               //copia de verdade do mapa da escala 0
    escalaMaximo = Mat_<int>(nl, nc, 0);        //matriz nl x nc de int, toda com 0

    for (int s=1; s<NESC; s++)
      for (int l=0; l<nl; l++)
        for (int c=0; c<nc; c++)
          if (mapaCC[s](l,c) > ccMaximo(l,c)) {
            ccMaximo(l,c) = mapaCC[s](l,c);
            escalaMaximo(l,c) = s;
          }
  }

  //=========================================================================
  // PASSO 3 - escolheCandidatos: os NCAND maiores picos de CC
  //=========================================================================
  //Repete NCAND vezes: pega o maior valor do mapa e depois "apaga" (poe -1)
  //um quadrado de lado 2*DIST_MIN+1 em volta dele. Sem apagar, os 20
  //maiores valores seriam todos pixels vizinhos do MESMO pico.
  //ATENCAO: altera ccMaximo (por isso ele e' uma copia, feita no PASSO 2).
  void escolheCandidatos(Mat_<FLT>& ccMaximo, const Mat_<int>& escalaMaximo) {
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
  }

  //=========================================================================
  // PASSO 4 - escolheMelhor: o candidato de maior NCC, se passar do limiar
  //=========================================================================
  void escolheMelhor() {
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
  }

  //=========================================================================
  // Desenho
  //=========================================================================

  //Quadrado ciano + 3 numeros empilhados a direita: escala, CC x100, NCC x100.
  static void desenhaCandidato(Mat_<COR>& a, const CANDIDATO& cd) {
    const Scalar ciano(255,255,0);       //cor na ordem B,G,R
    int metade = cd.lado/2;
    //Rect(coluna do canto, linha do canto, largura, altura)
    rectangle(a, Rect(cd.c-metade, cd.l-metade, cd.lado, cd.lado), ciano, 1);   //[OpenCV]

    int x = cd.c+metade+2;               //coluna: logo a direita do quadrado
    int y = cd.l-metade+8;               //linha: alinhado com o topo do quadrado
    int numeros[3] = { cd.s, int(round(cd.cc*100)), int(round(cd.ncc*100)) };
    for (int k=0; k<3; k++) {
      //putText [OpenCV]: escreve o texto com o canto inferior esquerdo em (x,y)
      putText(a, to_string(numeros[k]), Point(x,y), FONT_HERSHEY_PLAIN, 0.6, ciano, 1);
      y += 8;                            //proxima linha
    }
  }

  //Quadrado amarelo (mais grosso) + "escala CC NCC" acima dele.
  void desenhaMelhor(Mat_<COR>& a) const {
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
};

//#########################################################################
// LEITOR DO DIGITO (copiado igual do ../tarefa1/fase5.cpp)
//#########################################################################

//---------------------------------------------------------------------------
// DIGITO: le o digito manuscrito dentro da placa. Uso:
//    DIGITO digito("quadrado.png", ".");      //le o MNIST e treina (uma vez)
//    if (digito.le(placa, quadro, quadroCinza)) ...   //a cada quadro
//    digito.digito                            //o digito lido, 0..9
//
// Os passos sao os da apostila (p. 4):
//   1)  treino: MNIST (60000 imagens de TREINO) + FLANN       (construtor)
//   4a) recorta o interior do quadrado amarelo                (recorta)
//   4b) ajusta brilho/contraste: fundo branco, tinta preta    (ajustaContraste)
//   4c) mnist.bbox: tira as bordas brancas e reduz a 14x14    (reduz)
//   4d) FLANN: o rotulo da imagem de treino mais parecida     (classifica)
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
    achaInterior(arqPlaca);
    treina(dirMnist);
  }

  //=========================================================================
  // le: os passos 4a-4d no quadro atual. Devolve true se leu um digito.
  //   placa:       o detector, ja' com o resultado deste quadro
  //   quadro:      o quadro colorido (para o recorte que vai no video)
  //   quadroCinza: o mesmo quadro em cinza 0..1 (para ler o digito)
  //=========================================================================
  bool le(const PLACA& placa, const Mat_<COR>& quadro, const Mat_<FLT>& quadroCinza) {
    leu = false;
    digito = -1;

    //PASSO 4: so' le se achou a placa e ela esta' perto (escala 0 ou 1).
    //Longe, o digito tem poucos pixels e sai borrado demais para ler.
    if (!placa.achou) return false;
    if (placa.melhor.s > ESCALA_PERTO) return false;

    Mat_<FLT> cinza;
    if (!recorta(placa.melhor, quadro, quadroCinza, cinza)) return false;   //4a
    if (!ajustaContraste(cinza)) return false;                              //4b
    if (!reduz()) return false;                                             //4c
    digito = classifica();                                                  //4d
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

    desenhaDigitoAoLado(saida, placa.melhor);
    desenhaPainelPequena(saida, quadro.cols, 0, lp);
    desenhaPainelRecorte(saida, quadro.cols, lp, lp);
    return saida;
  }

private:
  //=========================================================================
  // Preparacao (construtor)
  //=========================================================================

  //Acha o interior do quadrado amarelo, onde se escreve o digito.
  //E' a unica regiao do quadrado.png com pixels EXATAMENTE (255,255,255) -
  //a mesma que e' "don't care" no detector. Medir no arquivo evita numeros
  //magicos no codigo (no quadrado.png: linhas e colunas 121 a 279).
  void achaInterior(const string& arqPlaca) {
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
  }

  //PASSO 1: le o MNIST e treina o FLANN.
  //So' as 60000 imagens de TREINO (o 0 pede para nao ler as 10000 de teste):
  //o conjunto de teste serve para MEDIR o erro de um algoritmo, nao para o
  //programa final usar.
  //Depois do le(), mnist.ax tem uma imagem 14x14 por linha (196 colunas,
  //0=preto 1=branco) e mnist.ay o rotulo de cada uma (0..9).
  //"Treinar" o FLANN e' so' montar uma arvore de busca sobre as linhas de
  //mnist.ax, para achar rapido a mais parecida com uma imagem nova.
  void treina(const string& dirMnist) {
    double t0 = timeSinceEpoch();                     //[raspberry.hpp]
    mnist.le(dirMnist, 60000, 0);                     //[raspberry.hpp] le os arquivos
    mnist.train();                                    //[raspberry.hpp] arvore do FLANN
    printf("MNIST lido e FLANN treinado em %.1f s\n", timeSinceEpoch()-t0);
  }

  //=========================================================================
  // PASSO 4a - recorta: o interior do quadrado amarelo, no quadro
  //=========================================================================
  //O detector diz onde esta' o CENTRO da placa (cd.l, cd.c) e o lado do
  //modelo que casou (cd.lado). O modelo e' o quadrado.png (401x401)
  //reduzido para cd.lado x cd.lado, entao um pixel (y,x) do quadrado.png
  //cai, no quadro, em:
  //   canto + (y,x) * k,   com k = cd.lado/401 e canto = centro - (lado-1)/2
  //(o "(lado-1)/2" e' o mesmo que o matchTemplateSame usa para o centro).
  //Saida: recorte (colorido, para o video) e cinza (para os proximos passos).
  bool recorta(const CANDIDATO& cd, const Mat_<COR>& quadro,
               const Mat_<FLT>& quadroCinza, Mat_<FLT>& cinza) {
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
    cinza = quadroCinza(r).clone();                  //[OpenCV]
    return true;
  }

  //=========================================================================
  // PASSO 4b - ajustaContraste: fundo -> branco (1.0), tinta -> preto (0.0)
  //=========================================================================
  //No video, o fundo da placa e' cinza claro (nao 1.0) e a tinta cinza
  //escura (nao 0.0), e os dois mudam com a iluminacao. O MNIST invertido
  //tem fundo exatamente 1.0. Entao esticamos os tons:
  //   preto  = o pixel mais escuro do recorte       (a tinta)
  //   branco = a MEDIANA dos pixels do recorte      (o fundo: a maioria dos
  //            pixels e' fundo, entao o valor do meio e' um pixel de fundo)
  //   novo   = (velho - preto)/(branco - preto), cortado em 0..1
  //Tudo que e' tao claro quanto o fundo vira 1.0 (branco); a tinta vira
  //0.0; a borda do traco fica cinza, como no MNIST.
  bool ajustaContraste(const Mat_<FLT>& cinza) {
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
    return true;
  }

  //=========================================================================
  // PASSO 4c - reduz: bbox e 14x14, como o MNIST fez com as imagens de treino
  //=========================================================================
  //mnist.bbox [raspberry.hpp] (versao FLT):
  //  - acha as linhas/colunas com algum pixel <= 0.5 (tinta) e corta as
  //    bordas brancas em volta;
  //  - redimensiona o que sobrou para 14x14.
  //Sem isso, um "1" pequeno no canto e um "1" grande no meio seriam imagens
  //muito diferentes para o FLANN. Se nao achar tinta, mnist.localizou=false.
  bool reduz() {
    pequena = mnist.bbox(ajustada);                  //[raspberry.hpp]
    return mnist.localizou;
  }

  //=========================================================================
  // PASSO 4d - classifica: FLANN, o vizinho mais proximo
  //=========================================================================
  //A consulta tem que ter o mesmo formato de uma linha de mnist.ax: 1 linha
  //com os 196 pixels (14x14 lidos linha por linha). predictInterno
  //[raspberry.hpp] acha a imagem de treino mais parecida e devolve o
  //rotulo dela.
  //(MnistFlann::predict tambem existiria, mas ele mesmo chama o bbox de
  //novo; aqui o bbox ja' foi feito no 4c, para podermos mostrar a imagem.)
  int classifica() {
    Mat_<FLT> consulta(1, NLADO*NLADO);              //1 linha x 196 colunas
    for (int i=0; i<NLADO*NLADO; i++)
      consulta(i) = pequena(i);                      //pequena(i): i-esimo pixel
    return int(mnist.predictInterno(consulta));     //[raspberry.hpp]
  }

  //=========================================================================
  // Desenho
  //=========================================================================

  //O digito lido, grande e vermelho, a esquerda da placa.
  void desenhaDigitoAoLado(Mat_<COR>& saida, const CANDIDATO& cd) const {
    const Scalar vermelho(0,0,255);                  //cor na ordem B,G,R
    Point p(cd.c - cd.lado/2 - 32, cd.l + 16);       //(coluna, linha) do texto
    putText(saida, to_string(digito), p, FONT_HERSHEY_SIMPLEX, 1.4, vermelho, 3);   //[OpenCV]
  }

  //Painel de cima: a imagem 14x14 ampliada SEM interpolar (INTER_NEAREST),
  //para ver cada pixel como um quadradinho - e' o que o FLANN recebeu.
  void desenhaPainelPequena(Mat_<COR>& saida, int c, int l, int lp) const {
    Mat_<COR> painel;
    converte(pequena, painel);                       //[raspberry.hpp] FLT 0..1 -> cor
    resize(painel, painel, Size(lp,lp), 0, 0, INTER_NEAREST);    //[OpenCV]
    painel.copyTo(saida(Rect(c, l, lp, lp)));        //[OpenCV]
  }

  //Painel de baixo: o recorte original do quadro, ampliado, com o digito lido.
  void desenhaPainelRecorte(Mat_<COR>& saida, int c, int l, int lp) const {
    const Scalar vermelho(0,0,255);
    Mat_<COR> painel;
    resize(recorte, painel, Size(lp,lp), 0, 0, INTER_LINEAR);   //[OpenCV]
    putText(painel, to_string(digito), Point(lp/2+8, lp-14),
            FONT_HERSHEY_SIMPLEX, 1.6, vermelho, 3);              //[OpenCV]
    painel.copyTo(saida(Rect(c, l, lp, lp)));        //[OpenCV]
  }
};

//---------------------------------------------------------------------------
// achaMnist: o diretorio dos arquivos do MNIST, rodando de aula5/ ou de
// aula5/tarefa2/ ("." = diretorio atual, ".." = o de cima)
//---------------------------------------------------------------------------
string achaMnist() {
  const char* lugares[2] = { ".", ".." };
  for (int i=0; i<2; i++) {
    string arq = string(lugares[i]) + "/train-images.idx3-ubyte";
    FILE* f = fopen(arq.c_str(), "rb");              //[C] consegue abrir?
    if (f) { fclose(f); return lugares[i]; }
  }
  erro("Erro: nao achei train-images.idx3-ubyte (descompacte o mnist.zip em parte_2/aula5/)");   //[raspberry.hpp]
  return "";
}

//#########################################################################
// MAQUINA DE ESTADOS (novo na fase 6) - figura 6 da apostila
//#########################################################################
//
//            +-------------------------------+
//            |  0 SEGUE: detecta e segue a   |<------------------+
//  inicio -->|  placa (sem placa: parado)    |<---------+        |
//            +-------------------------------+          |        |
//                 | placa grande (perto)                |        |
//                 v                                     |        |
//            +-------------------------------+          |        |
//            |  1 PARA: motores desligados,  |          |        |
//            |  espera o carrinho parar      |          |        |
//            +-------------------------------+          |        |
//                 | passou TEMPO_PARADA                 |        |
//                 v                                     |        |
//            +-------------------------------+  nao conseguiu ler|
//            |  2 LE: le o digito (parado),  |----------+        |
//            |  VOTOS leituras, vence a      |          |        |
//            |  maioria                      |          |        |
//            +-------------------------------+          |        |
//                 |                  | 0 ou 1           |        |
//                 |                  v                  |        |
//                 |    +-------------------------+      |        |
//                 |    | 4 FIM: fim do caminho,  |------+        |
//                 |    | parado enquanto a placa | placa saiu    |
//                 |    | estiver a vista         | de vista      |
//                 |    +-------------------------+               |
//                 | 2..9: guarda o digito,                       |
//                 v       zera o cronometro                      |
//            +-------------------------------+                   |
//            |  3 MOVE: faz o movimento do   |                   |
//            |  digito durante o seu tempo   |                   |
//            +-------------------------------+                   |
//                 | passou o tempo do movimento                  |
//                 v                                              |
//            +-------------------------------+  passou           |
//            |  5 TERMINA: motores desligados|  TEMPO_PARADA     |
//            |  espera o giro parar (inercia)|-------------------+
//            +-------------------------------+
//
// Como vira codigo: a apostila (p. 8) sugere uma variavel "estado" e um
// if/else para cada estado, dentro de um laco. Aqui o laco e' o do main,
// que roda UMA VEZ POR QUADRO (cada volta recebe um quadro e tem que
// devolver um comando ao servidor6). Entao a maquina anda um "passo" por
// quadro: o metodo passo() executa o estado atual uma vez, define a
// velocidade das rodas e, se for o caso, muda de estado.
//
// Tempos em SEGUNDOS, nao em quadros (apostila, p. 8): o fps muda com a
// rede, e "girar 13 quadros" daria angulos diferentes a cada vez. Cada
// estado guarda quando comecou (inicioEstado) e compara com o relogio.
//
// Diferenca da apostila no estado 4: la' o 0/1 TERMINA os dois programas.
// Aqui o carrinho so' para e espera a placa sair de vista; entao volta ao
// estado 0 e procura a proxima. Assim da' para testar varias placas sem
// reiniciar cliente6 e servidor6. Os programas so' terminam com ESC.

//---- Parametros (ajuste no carrinho real) ----
//Seguir a placa (estado 0): o mesmo controle do cliente4
const double VEL=0.70;          //velocidade para frente com a placa centrada
const double GANHO=0.50;        //quanto o desvio da placa muda cada roda
const int LADO_PERTO=60;        //placa com lado >= isto (escalas 0 e 1): perto
//Parar e ler (estados 1, 2 e 5)
const double TEMPO_PARADA=0.8;  //s: tempo para a inercia acabar (estados 1 e 5)
const int VOTOS=5;              //leituras que o estado 2 junta antes de decidir
const int MAX_TENTATIVAS=15;    //quadros no estado 2 sem VOTOS leituras: desiste
const double TEMPO_SEM_PLACA=1.0;  //s sem ver placa para sair do 4 FIM e recomecar
//Movimentos (estado 3). Os tempos dependem do carrinho, da bateria e do chao:
//CALIBRE: mande girar e ajuste ate' dar 90 e 180 graus.
const double GIRO=0.70;         //velocidade de cada roda girando no proprio eixo
//Calibrado no carrinho em 03/11/2026, com GIRO=0.70 (testando e ajustando).
//O angulo NAO e' proporcional ao tempo: o carrinho perde um tempo vencendo o
//atrito antes de comecar a girar, por isso 180 graus nao e' o dobro de 90.
//Mudou a bateria, o chao ou o GIRO? Calibre de novo.
const double TEMPO_90=2.60;     //s: giro de 90 graus
const double TEMPO_180=4.80;    //s: giro de 180 graus
//Passar por baixo (4 e 5): anda reto por TEMPO_PASSA, sem olhar a placa, e
//volta ao estado 0. O carrinho para a ~30 cm da placa (LADO_PERTO), entao
//precisa andar esses ~30 cm MAIS o proprio comprimento.
//Calibrado no carrinho em 03/11/2026 (2 e 4 s nao bastavam: parava antes da
//placa, via o 5 de novo e relia). Se passar e ainda parar logo depois, ou
//bater em algo adiante, ajuste aqui.
const double VEL_PASSA=0.70;    //velocidade para frente passando sob a placa
const double TEMPO_PASSA=7.00;  //s: tempo andando reto para passar sob a placa

//---------------------------------------------------------------------------
// ESTADO: os 6 estados, com os numeros da figura 6.
// enum [C]: da' nome a numeros. SEGUE vale 0, PARA vale 1, etc.
//---------------------------------------------------------------------------
enum ESTADO { SEGUE=0, PARA=1, LE=2, MOVE=3, FIM=4, TERMINA=5 };

//nome de cada estado, para a tela e o terminal: NOME_ESTADO[MOVE] = "3 MOVE"
const char* NOME_ESTADO[6] = { "0 SEGUE", "1 PARA", "2 LE", "3 MOVE", "4 FIM", "5 TERMINA" };

//---------------------------------------------------------------------------
// MOVIMENTO: o que o carrinho faz no estado 3 (velocidade de cada roda
// durante "tempo" segundos).
//---------------------------------------------------------------------------
struct MOVIMENTO {
  double esq, dir;     //velocidade de cada roda, -1..+1
  double tempo;        //duracao, em segundos
  string nome;         //para a tela
};

//---------------------------------------------------------------------------
// movimentoDoDigito: a tabela de comandos da apostila (2025, p. 5).
//   0 ou 1  pare (nao chega aqui: o estado 2 vai direto para o 4 FIM)
//   2       vire 180 graus a esquerda      3       vire 180 graus a direita
//   4 ou 5  passe por baixo da placa e continue em frente
//   6 ou 7  vire 90 graus a esquerda       8 ou 9  vire 90 graus a direita
// Girar no proprio eixo: uma roda para frente e a outra para tras.
//   esquerda: esq = -GIRO, dir = +GIRO      direita: esq = +GIRO, dir = -GIRO
//---------------------------------------------------------------------------
MOVIMENTO movimentoDoDigito(int d) {
  MOVIMENTO m;
  if (d==2)             m = { -GIRO,  GIRO,     TEMPO_180,   "180 graus a esquerda" };
  else if (d==3)        m = {  GIRO, -GIRO,     TEMPO_180,   "180 graus a direita" };
  else if (d==4 || d==5) m = { VEL_PASSA, VEL_PASSA, TEMPO_PASSA, "passa por baixo" };
  else if (d==6 || d==7) m = { -GIRO,  GIRO,     TEMPO_90,    "90 graus a esquerda" };
  else                  m = {  GIRO, -GIRO,     TEMPO_90,    "90 graus a direita" };   //8 ou 9
  return m;
}

//---------------------------------------------------------------------------
// MAQUINA: a maquina de estados. Uso, a cada quadro:
//    maquina.passo(placa, digito);   //anda um passo
//    maquina.esq, maquina.dir        //velocidade para mandar ao servidor6
//    maquina.estado                  //o estado atual (para a tela)
//---------------------------------------------------------------------------
class MAQUINA {
public:
  //---- Saida de cada passo (o main le estes campos) ----
  ESTADO estado = SEGUE;   //o estado atual (o inicio e' o 0)
  double esq=0, dir=0;     //velocidade das rodas neste quadro
  string info;             //texto extra para a tela (ex.: "dirx=-11")
  int digitoLido = -1;     //o digito decidido no estado 2

private:
  double inicioEstado;     //quando entrou no estado atual [segundos]
  double ultimaPlaca;      //estado 4: quando viu a placa pela ultima vez [segundos]
  MOVIMENTO movimento;     //o movimento do estado 3
  int votos[10];           //estado 2: votos[d] = quantas leituras deram d
  int leituras;            //estado 2: quantas leituras ja' juntou
  int tentativas;          //estado 2: quantos quadros ja' tentou

public:
  MAQUINA() {
    inicioEstado = timeSinceEpoch();       //[raspberry.hpp]
  }

  //=========================================================================
  // passo: executa o estado atual UMA vez (um quadro).
  // Comeca com as rodas paradas: so' os estados 0 e 3 as ligam. Assim
  // nenhum estado esquece os motores ligados por engano.
  //=========================================================================
  void passo(const PLACA& placa, const DIGITO& digito) {
    esq = 0;
    dir = 0;
    if      (estado == SEGUE)   estado0_segue(placa);
    else if (estado == PARA)    estado1_para();
    else if (estado == LE)      estado2_le(digito);
    else if (estado == MOVE)    estado3_move();
    else if (estado == FIM)     estado4_fim(placa);
    else if (estado == TERMINA) estado5_termina();
  }

  //Segundos desde que entrou no estado atual (o "cronometro").
  double tempoNoEstado() const {
    return timeSinceEpoch() - inicioEstado;   //[raspberry.hpp]
  }

private:
  //Muda de estado, zera o cronometro e avisa no terminal (para depurar).
  void vaiPara(ESTADO novo, const string& motivo) {
    printf("estado %-9s -> %-9s  (%s)\n", NOME_ESTADO[estado], NOME_ESTADO[novo], motivo.c_str());
    estado = novo;
    inicioEstado = timeSinceEpoch();
  }

  //=========================================================================
  // Estado 0 - SEGUE: detecta e segue a placa (o mesmo controle da fase 4).
  //   sem placa      -> parado, continua no 0
  //   placa pequena  -> anda na direcao dela
  //   placa grande   -> vai para o 1 (parar antes de bater)
  //=========================================================================
  void estado0_segue(const PLACA& placa) {
    if (!placa.achou) {
      info = "sem placa";
      return;                              //rodas paradas
    }
    if (placa.melhor.lado >= LADO_PERTO) {
      info = "placa perto";
      vaiPara(PARA, "placa grande: para para ler");
      return;                              //rodas paradas
    }
    //desvio: -1 com a placa na borda esquerda, 0 no centro, +1 na direita
    double centro = 320/2.0;
    double desvio = (placa.melhor.c - centro)/centro;
    //placa a direita (desvio>0): esquerda acelera, direita freia -> vira a direita
    esq = max(-1.0, min(VEL + GANHO*desvio, 1.0));
    dir = max(-1.0, min(VEL - GANHO*desvio, 1.0));
    info = "dirx=" + to_string(int(round(100*desvio)));   //como na figura 7
  }

  //=========================================================================
  // Estado 1 - PARA: motores desligados ate' o carrinho parar de vez.
  // Pela inercia, ele ainda anda um pouco; ler o digito agora daria uma
  // imagem borrada (figura 8).
  //=========================================================================
  void estado1_para() {
    info = "parando";
    if (tempoNoEstado() >= TEMPO_PARADA) {
      comecaLeitura();
      vaiPara(LE, "carrinho parado");
    }
  }

  //=========================================================================
  // Estado 2 - LE: le o digito com o carrinho parado (figura 9).
  // Uma leitura erra de vez em quando (1% no capturado.avi, ver
  // ../tarefa1); um erro aqui faria o carrinho virar para o lado errado.
  // Entao juntamos VOTOS leituras e vence o digito mais lido (maioria).
  //   VOTOS leituras: 0 ou 1 -> 4 FIM (para);  2..9 -> 3 MOVE
  //   MAX_TENTATIVAS quadros sem conseguir (placa sumiu, sem digito...):
  //                  volta para o 0 (a apostila manda voltar ao 0)
  //=========================================================================
  void comecaLeitura() {
    for (int d=0; d<10; d++) votos[d] = 0;
    leituras = 0;
    tentativas = 0;
  }

  void estado2_le(const DIGITO& digito) {
    tentativas++;
    if (digito.leu) {                      //o DIGITO leu neste quadro?
      votos[digito.digito]++;
      leituras++;
    }
    info = "lendo " + to_string(leituras) + "/" + to_string(VOTOS);

    if (leituras >= VOTOS) {
      digitoLido = maisVotado();
      string lido = "li o digito " + to_string(digitoLido);
      if (digitoLido == 0 || digitoLido == 1) {
        ultimaPlaca = timeSinceEpoch();    //a placa esta' a vista agora
        vaiPara(FIM, lido + ": fim do caminho, parado");
      } else {
        movimento = movimentoDoDigito(digitoLido);
        vaiPara(MOVE, lido + ": " + movimento.nome);   //zera o cronometro
      }
    } else if (tentativas >= MAX_TENTATIVAS) {
      vaiPara(SEGUE, "nao consegui ler o digito");
    }
  }

  //o digito com mais votos (no empate, o menor)
  int maisVotado() const {
    int melhor = 0;
    for (int d=1; d<10; d++)
      if (votos[d] > votos[melhor]) melhor = d;
    return melhor;
  }

  //=========================================================================
  // Estado 3 - MOVE: faz o movimento do digito durante o seu tempo.
  // O servidor6 continua mandando quadros (borrados: figura 10), mas aqui
  // eles nao mudam nada - o movimento e' so' por tempo.
  //=========================================================================
  void estado3_move() {
    esq = movimento.esq;
    dir = movimento.dir;
    double falta = movimento.tempo - tempoNoEstado();
    char texto[64];
    snprintf(texto, sizeof texto, "%s (%.1fs)", movimento.nome.c_str(), max(0.0, falta));   //[C]
    info = texto;
    if (falta <= 0) {
      esq = 0;
      dir = 0;
      vaiPara(TERMINA, "fim do movimento");
    }
  }

  //=========================================================================
  // Estado 4 - FIM: fim do caminho (digito 0 ou 1). O carrinho fica parado
  // enquanto a placa estiver a vista. Quando ela some por TEMPO_SEM_PLACA
  // (alguem a tirou, ou o carrinho foi levado para outro lugar), volta ao
  // estado 0 e procura a proxima placa. (A apostila termina os programas
  // aqui; ver a "Diferenca da apostila" no topo desta secao.)
  // Por que esperar a placa SUMIR, e nao so' um tempo? Se voltasse ao 0 com
  // a placa 0 ainda na frente, leria o 0 de novo e ficaria nesse vai-e-vem.
  //=========================================================================
  void estado4_fim(const PLACA& placa) {
    if (placa.achou) ultimaPlaca = timeSinceEpoch();   //ainda vejo a placa
    double semPlaca = timeSinceEpoch() - ultimaPlaca;
    info = "fim do caminho: parado (tire a placa para recomecar)";
    if (semPlaca >= TEMPO_SEM_PLACA)
      vaiPara(SEGUE, "placa saiu de vista: procura a proxima");
  }

  //=========================================================================
  // Estado 5 - TERMINA: motores desligados ate' o giro parar (figura 12),
  // e volta ao 0 para procurar a proxima placa.
  //=========================================================================
  void estado5_termina() {
    info = "terminando movimento";
    if (tempoNoEstado() >= TEMPO_PARADA)
      vaiPara(SEGUE, "carrinho parado");
  }
};

//#########################################################################
// TELA E PROGRAMA
//#########################################################################

//---------------------------------------------------------------------------
// achaModelo: acha quadrado.png rodando de aula5/ ou de aula5/tarefa2/
//---------------------------------------------------------------------------
string achaModelo() {
  const char* lugares[2] = { "quadrado.png", "../quadrado.png" };
  for (int i=0; i<2; i++)
    if (!imread(lugares[i],1).empty()) return lugares[i];   //[OpenCV] conseguiu ler?
  erro("Erro: nao achei quadrado.png (coloque-o em parte_2/aula5/)");   //[raspberry.hpp]
  return "";
}

//---------------------------------------------------------------------------
// barra: desenha a velocidade de uma roda como uma barra vertical que sai
// do meio da altura: para cima = frente, para baixo = re.
//   c: coluna onde a barra comeca;  v: velocidade -1..+1
//---------------------------------------------------------------------------
void barra(Mat_<COR>& a, int c, double v, const Scalar& cor) {
  int meio = a.rows/2;
  int altura = int(v*(a.rows/2-10));   //v=1 -> quase ate' o topo
  //rectangle [OpenCV] com dois cantos opostos; FILLED = preenchido
  rectangle(a, Point(c,meio), Point(c+8,meio-altura), cor, FILLED);
}

//---------------------------------------------------------------------------
// montaTela: a imagem da camera com o debug por cima (figuras 7 a 12):
//   quadrados da placa, barras das rodas, os paineis do digito a direita
//   (DIGITO::desenha, como na fase 5) e, no alto, o estado e a info.
//---------------------------------------------------------------------------
Mat_<COR> montaTela(const Mat_<COR>& img, const PLACA& placa, const DIGITO& digito,
                    const MAQUINA& maquina) {
  Mat_<COR> camera = img.clone();      //copia de verdade: img fica limpa para o modo 'c'
  placa.desenha(camera);
  const Scalar verde(0,255,0);         //B,G,R
  barra(camera, 2, maquina.esq, verde);
  barra(camera, camera.cols-10, maquina.dir, verde);

  Mat_<COR> tela = digito.desenha(camera, placa);   //alarga: 320 + 120 colunas

  //faixa preta no alto, com o estado e a informacao do estado
  rectangle(tela, Rect(0,0,320,30), Scalar(0,0,0), FILLED);   //[OpenCV]
  string linha1 = "Estado " + string(NOME_ESTADO[maquina.estado]);
  if (maquina.digitoLido >= 0) linha1 += "   digito " + to_string(maquina.digitoLido);
  putText(tela, linha1, Point(4,12), FONT_HERSHEY_PLAIN, 0.9, Scalar(0,255,255), 1);   //[OpenCV]
  putText(tela, maquina.info, Point(4,26), FONT_HERSHEY_PLAIN, 0.9, Scalar(255,255,255), 1);
  return tela;
}

//---------------------------------------------------------------------------
// apertouEsc: le TODAS as teclas pendentes e diz se alguma foi ESC.
// Esvaziar a fila evita que teclas acumuladas cheguem atrasadas. O
// waitKeyEx tambem e' o que faz o imshow realmente desenhar a janela.
//---------------------------------------------------------------------------
bool apertouEsc() {
  bool esc=false;
  for (int i=0; i<100; i++) {
    int k = waitKeyEx(1);              //[OpenCV] espera 1 ms por uma tecla
    if (k<0) break;                    //fila vazia
    if ((k & 0xffff)==27) esc=true;    //27 = ESC (o GTK poe modificadores nos bits altos)
  }
  return esc;
}

//---------------------------------------------------------------------------
// montaComando: os 3 bytes do protocolo (ver servidor6.cpp)
//   termina -> 's' 0 0
//   senao   -> 'm' esq dir, com esq e dir em % (-100..100) gravados como int8
//---------------------------------------------------------------------------
void montaComando(bool termina, double esq, double dir, BYTE cmd[3]) {
  if (termina) {
    cmd[0]='s'; cmd[1]=0; cmd[2]=0;
  } else {
    cmd[0]='m';
    //(int8_t) = byte com sinal; (BYTE) = o mesmo byte visto sem sinal
    cmd[1]=(BYTE)(int8_t)round(100*esq);   //ex.: -0.5 -> -50 -> byte 0xCE
    cmd[2]=(BYTE)(int8_t)round(100*dir);
  }
}

int main(int argc, char *argv[]) {
  //---- 1) preparacao ----
  ARGCLIENTE arg(argc,argv,"cliente6");      //[teclado.hpp] ip [video.avi] [t/c]
  string arqPlaca = achaModelo();
  DIGITO digito(arqPlaca, achaMnist());      //le o MNIST e treina o FLANN (~2 s)
  PLACA placa(arqPlaca, true);               //detector paralelo
  printf("cliente6: localizando com %d thread(s)\n", placa.threads());
  //Mostra os tempos com que ESTE executavel foi compilado. Mudou uma
  //constante e o valor aqui nao mudou? Faltou recompilar (./aula5.sh build).
  printf("tempos: 90 graus %.2f s | 180 graus %.2f s | passa por baixo %.2f s | parada %.2f s\n",
         TEMPO_90, TEMPO_180, TEMPO_PASSA, TEMPO_PARADA);
  CLIENT client(arg.ip);                     //[projeto.hpp] conecta no servidor6

  namedWindow("cliente6");                   //[OpenCV]
  //so' cria o gravador se foi pedido um video (NULL = nao grava)
  GRAVADOR* gravador = arg.video.empty() ? NULL : new GRAVADOR(arg.video);   //[teclado.hpp]

  MAQUINA maquina;                           //comeca no estado 0 SEGUE
  BYTE cmd[3]={'m',0,0};
  client.sendBytes(3,cmd);           //[projeto.hpp] "estou pronto para receber imagem"

  //---- 2) laco: um quadro por volta = um passo da maquina de estados ----
  Mat_<COR> img;
  Mat_<FLT> imgCinza;
  while (true) {
    //a) recebe o quadro, localiza a placa e tenta ler o digito.
    //   O leitor roda em todo quadro (para mostrar na tela); a maquina so'
    //   usa o resultado no estado 2.
    client.receiveImgComp(img);                //[projeto.hpp] recebe JPEG e descomprime
    if (img.rows!=240 || img.cols!=320) resize(img, img, Size(320,240));   //[OpenCV]
    converte(img, imgCinza);                   //[raspberry.hpp] cor -> cinza 0..1
    placa.procura(imgCinza);
    digito.le(placa, img, imgCinza);

    //b) um passo da maquina de estados: decide as rodas
    maquina.passo(placa, digito);

    //c) mostra e grava
    Mat_<COR> tela = montaTela(img, placa, digito, maquina);
    imshow("cliente6", tela);                  //[OpenCV]
    if (gravador) gravador->grava(arg.modo=='c' ? img : tela);   //'c': so' a camera

    //d) comando ao servidor6. So' o ESC termina: o 's' tambem termina o
    //   servidor6. (O estado 4 FIM so' para o carrinho.)
    bool esc = apertouEsc();
    montaComando(esc, maquina.esq, maquina.dir, cmd);
    client.sendBytes(3,cmd);                   //[projeto.hpp]
    if (esc) {
      printf("ESC: fim\n");
      break;
    }
  }
  delete gravador;                   //fecha o video (delete de NULL nao faz nada)
}
