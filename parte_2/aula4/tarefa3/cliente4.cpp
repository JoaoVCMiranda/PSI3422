

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
// DETECTOR DA PLACA (o mesmo nas tarefas 1, 2 e 3)
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
// CONTROLE E PROGRAMA
//#########################################################################

//---- Parametros do controle (ajuste no carrinho real) ----
const double VEL=0.70;     //velocidade para frente com a placa centrada
const double GANHO=0.50;   //quanto o desvio da placa muda cada roda
const int LADO_PERTO=60;   //placa com lado >= isto (escalas 0 e 1): para

int main(int argc, char *argv[]) {
  //=======================================================================
  // 1) preparacao
  //=======================================================================
  ARGCLIENTE arg(argc,argv,"cliente4");      //[teclado.hpp] ip [video.avi] [t/c]

  //acha quadrado.png rodando de aula4/ ou de aula4/tarefa3/
  const char* lugares[2] = { "quadrado.png", "../quadrado.png" };
  string arqModelo = "";
  for (int i=0; i<2 && arqModelo==""; i++)
    if (!imread(lugares[i],1).empty()) arqModelo = lugares[i];   //[OpenCV] conseguiu ler?
  if (arqModelo=="") erro("Erro: nao achei quadrado.png (coloque-o em parte_2/aula4/)");   //[raspberry.hpp]

  PLACA placa(arqModelo, true);              //detector paralelo (roda o construtor)
  printf("cliente4: localizando com %d thread(s)\n", placa.threads());
  CLIENT client(arg.ip);                     //[projeto.hpp] conecta no servidor4

  namedWindow("cliente4");                   //[OpenCV]
  //so' cria o gravador se foi pedido um video (NULL = nao grava)
  GRAVADOR* gravador = arg.video.empty() ? NULL : new GRAVADOR(arg.video);   //[teclado.hpp]

  BYTE cmd[3]={'m',0,0};
  client.sendBytes(3,cmd);           //[projeto.hpp] "estou pronto para receber imagem"

  //=======================================================================
  // 2) laco: um quadro por volta
  //=======================================================================
  Mat_<COR> img;
  Mat_<FLT> imgCinza;
  int quadros=0; double t0=timeSinceEpoch();   //[raspberry.hpp] relogio
  while (true) {
    //---- a) recebe o quadro e localiza a placa ----
    client.receiveImgComp(img);                //[projeto.hpp] recebe JPEG e descomprime
    if (img.rows!=240 || img.cols!=320) resize(img, img, Size(320,240));   //[OpenCV]
    converte(img, imgCinza);                   //[raspberry.hpp] cor -> cinza 0..1
    placa.procura(imgCinza);                   //os 4 passos do detector

    //---- b) o CONTROLE: decide a velocidade de cada roda (-1..+1) ----
    double esq=0, dir=0;                       //comeca parado
    string estado;                             //texto mostrado na tela
    if (!placa.achou) {
      estado = "sem placa: parado";
    } else if (placa.melhor.lado >= LADO_PERTO) {
      estado = "perto: parado";                //placa grande = perto: chegou
    } else {
      //desvio (o "erro" da formula do topo): -1 com a placa na borda
      //esquerda, 0 no centro, +1 na borda direita
      double centro = img.cols/2.0;
      double desvio = (placa.melhor.c - centro)/centro;
      //placa a direita (desvio>0): esquerda acelera, direita freia -> vira a
      //direita. max/min cortam o resultado na faixa -1..+1.
      esq = max(-1.0, min(VEL + GANHO*desvio, 1.0));
      dir = max(-1.0, min(VEL - GANHO*desvio, 1.0));
      estado = "seguindo";
    }

    //---- c) monta a tela, mostra e grava ----
    //A imagem da camera com o debug por cima: quadrados da placa, uma barra
    //por roda (esquerda na borda esquerda, direita na borda direita) e o
    //estado embaixo.
    Mat_<COR> tela = img.clone();        //copia de verdade: img fica limpa para o modo 'c'
    placa.desenha(tela);
    const Scalar verde(0,255,0);         //B,G,R
    //Barra: a velocidade da roda como uma barra vertical que sai do meio da
    //altura: para cima = frente, para baixo = re (v=1 -> quase ate' o topo).
    double velocidade[2] = { esq, dir };
    int coluna[2] = { 2, tela.cols-10 }; //onde cada barra comeca
    for (int r=0; r<2; r++) {
      int meio = tela.rows/2;
      int altura = int(velocidade[r]*(tela.rows/2-10));
      //rectangle [OpenCV] com dois cantos opostos; FILLED = preenchido
      rectangle(tela, Point(coluna[r],meio), Point(coluna[r]+8,meio-altura), verde, FILLED);
    }
    putText(tela, estado, Point(14,tela.rows-6), FONT_HERSHEY_PLAIN, 1.0, Scalar(0,0,255), 1);  //[OpenCV]
    imshow("cliente4", tela);                  //[OpenCV]
    if (gravador) gravador->grava(arg.modo=='c' ? img : tela);   //'c': so' a camera

    //---- d) teclado: le TODAS as teclas pendentes e ve se alguma foi ESC ----
    //Esvaziar a fila evita que teclas acumuladas cheguem atrasadas. O
    //waitKeyEx tambem e' o que faz o imshow realmente desenhar a janela.
    bool esc=false;
    for (int i=0; i<100; i++) {
      int k = waitKeyEx(1);              //[OpenCV] espera 1 ms por uma tecla
      if (k<0) break;                    //fila vazia
      if ((k & 0xffff)==27) esc=true;    //27 = ESC (o GTK poe modificadores nos bits altos)
    }

    //---- e) comando ao servidor4: os 3 bytes do protocolo (ver servidor4.cpp) ----
    //   ESC   -> 's' 0 0
    //   senao -> 'm' esq dir, com esq e dir em % (-100..100) gravados como int8
    if (esc) {
      cmd[0]='s'; cmd[1]=0; cmd[2]=0;
    } else {
      cmd[0]='m';
      //(int8_t) = byte com sinal; (BYTE) = o mesmo byte visto sem sinal
      cmd[1]=(BYTE)(int8_t)round(100*esq);   //ex.: -0.5 -> -50 -> byte 0xCE
      cmd[2]=(BYTE)(int8_t)round(100*dir);
    }
    client.sendBytes(3,cmd);                   //[projeto.hpp]
    if (esc) break;

    //---- f) fps a cada 40 quadros ----
    quadros++;
    if (quadros%40==0) {
      double t=timeSinceEpoch();
      printf("%.1f fps\n", 40/(t-t0));
      t0=t;
    }
  }
  delete gravador;                   //fecha o video (delete de NULL nao faz nada)
}
