//placa.hpp - Aula 4 (fases 3 e 4): localiza a placa quadrado.png num quadro
//240x320 usando template matching CC + NCC em varias escalas.
//Usado por fase3.cpp (sequencial), fase3p.cpp (OpenMP) e cliente4.cpp.
//
//Algoritmo (sugestao da apostila, p. 8):
// 1) 10 modelos em progressao geometrica, de 69x69 a 19x19 pixels (placa a
//    ~30 a ~150 cm da camera), reamostrados com vizinho mais proximo para nao
//    misturar a cor "don't care" 1.0 com as vizinhas.
// 2) Para cada escala, um modelo CC (somaAbsDois(dcReject(T,1.0))) e um NCC
//    (dcReject(T,1.0)): o interior do quadrado amarelo, onde vai o digito
//    escrito a mao, e' "don't care" nos dois.
// 3) Os 20 template matchings (10 CC + 10 NCC) sao independentes entre si: e'
//    o laco que o OpenMP paraleliza (licao #2/3).
// 4) Candidatos = 20 maiores picos de CC (maximo entre as escalas),
//    separados por pelo menos 10 pixels.
// 5) Entre eles, vence o de maior NCC, NA MESMA POSICAO E ESCALA. So' e'
//    placa se esse NCC passar de LIMIAR_NCC.
#ifndef PLACA_HPP
#define PLACA_HPP
#include "../projeto.hpp"
#ifdef _OPENMP
#include <omp.h>
#endif

//OpenCV 4 removeu os nomes CV_TM_*; os novos (TM_*) existem desde o 3.
#ifndef CV_TM_CCORR
  #define CV_TM_CCORR cv::TM_CCORR
  #define CV_TM_CCOEFF_NORMED cv::TM_CCOEFF_NORMED
#endif

struct CANDIDATO {
  int l, c;      //centro, no quadro
  int s;         //indice da escala (0 = maior modelo)
  int lado;      //lado do modelo nessa escala, em pixels
  FLT cc, ncc;
};

class PLACA {
public:
  static constexpr int NESC=10;            //numero de escalas
  static constexpr int LADO_MAX=69, LADO_MIN=19;
  static constexpr int NCAND=20;           //candidatos CC
  static constexpr int DIST_MIN=10;        //distancia minima entre candidatos
  static constexpr FLT LIMIAR_NCC=0.55;

private:
  vector<Mat_<FLT>> tcc, tncc;    //modelos pre-processados, um por escala
  vector<int> lado;
  vector<Mat_<FLT>> rcc, rncc;    //resultados do ultimo quadro
  bool paralelo;

public:
  vector<CANDIDATO> candidatos;   //do ultimo quadro, em ordem de CC
  CANDIDATO melhor;               //maior NCC entre os candidatos
  bool achou=false;

  //paralelo=false: os 20 matchTemplate rodam um depois do outro, num nucleo
  PLACA(const string& arqModelo, bool _paralelo): paralelo(_paralelo) {
    //Os dois programas usam cv::setNumThreads(1): o matchTemplate do OpenCV
    //pode se paralelizar sozinho por dentro, o que (a) faria a versao
    //"sequencial" usar varios nucleos e (b) brigaria com os threads do
    //OpenMP na versao paralela. Assim quem paraleliza e' so' o nosso laco.
    setNumThreads(1);

    Mat_<COR> temp=imread(arqModelo,1);
    if (temp.total()==0) erro("Erro leitura "+arqModelo);
    Mat_<FLT> T; converte(temp,T);   //401x401, 0=preto 1=branco

    //progressao geometrica: razao constante entre escalas vizinhas, porque
    //o tamanho aparente da placa e' inversamente proporcional a distancia
    double razao=pow(double(LADO_MIN)/LADO_MAX, 1.0/(NESC-1));
    for (int s=0; s<NESC; s++) {
      int n=int(round(LADO_MAX*pow(razao,s)));
      Mat_<FLT> Ts;
      resize(T, Ts, Size(n,n), 0, 0, INTER_NEAREST);
      //dcReject altera a imagem recebida (Mat compartilha os dados): clone
      tncc.push_back(dcReject(Ts.clone(),1.0));
      tcc.push_back(somaAbsDois(dcReject(Ts.clone(),1.0)));
      lado.push_back(n);
    }
    rcc.resize(NESC); rncc.resize(NESC);
  }

  int ladoEscala(int s) const { return lado[s]; }

  int threads() const {
#ifdef _OPENMP
    return paralelo ? omp_get_max_threads() : 1;
#else
    return 1;
#endif
  }

  //Procura a placa no quadro f (em niveis de cinza, 0..1). Devolve achou.
  bool procura(const Mat_<FLT>& f) {
    //Parte pesada: 2*NESC template matchings independentes. Cada iteracao
    //escreve so' no seu rcc[s]/rncc[s], entao nao ha' dependencia entre
    //iteracoes. schedule(dynamic): os modelos grandes demoram mais que os
    //pequenos, e o nucleo livre pega o proximo.
    #pragma omp parallel for schedule(dynamic) if(paralelo)
    for (int i=0; i<2*NESC; i++) {
      int s=i/2;
      if (i%2==0) rcc[s]=matchTemplateSame(f, tcc[s], CV_TM_CCORR, 0.0);
      else        rncc[s]=matchTemplateSame(f, tncc[s], CV_TM_CCOEFF_NORMED, 0.0);
    }

    //Daqui em diante e' leve: nao precisa paralelizar.
    //Maximo de CC entre as escalas, lembrando qual escala deu o maximo.
    Mat_<FLT> m=rcc[0].clone();
    Mat_<int> esc(f.rows, f.cols, 0);
    for (int s=1; s<NESC; s++)
      for (int l=0; l<f.rows; l++)
        for (int c=0; c<f.cols; c++)
          if (rcc[s](l,c)>m(l,c)) { m(l,c)=rcc[s](l,c); esc(l,c)=s; }

    //NCAND maiores picos, apagando a vizinhanca de cada um escolhido
    candidatos.clear();
    for (int k=0; k<NCAND; k++) {
      double vmax; Point p;
      minMaxLoc(m, NULL, &vmax, NULL, &p);
      if (vmax<=0) break;
      CANDIDATO cd;
      cd.l=p.y; cd.c=p.x; cd.s=esc(p.y,p.x); cd.lado=lado[cd.s];
      cd.cc=FLT(vmax); cd.ncc=rncc[cd.s](p.y,p.x);
      candidatos.push_back(cd);
      Rect viz(p.x-DIST_MIN, p.y-DIST_MIN, 2*DIST_MIN+1, 2*DIST_MIN+1);
      m(viz & Rect(0,0,m.cols,m.rows)).setTo(-1);
    }

    achou=false;
    for (auto& cd: candidatos)
      if (!achou || cd.ncc>melhor.ncc) { melhor=cd; achou=true; }
    if (achou && melhor.ncc<LIMIAR_NCC) achou=false;
    return achou;
  }

  //Desenha como na figura 4: candidatos em ciano com escala/CC/NCC (x100) e
  //a placa encontrada em amarelo.
  void desenha(Mat_<COR>& a) const {
    const Scalar ciano(255,255,0), amarelo(0,255,255);
    for (auto& cd: candidatos) {
      int h=cd.lado/2;
      rectangle(a, Rect(cd.c-h, cd.l-h, cd.lado, cd.lado), ciano, 1);
      int y=cd.l-h+8, x=cd.c+h+2;
      for (int v: {cd.s, int(round(cd.cc*100)), int(round(cd.ncc*100))}) {
        putText(a, to_string(v), Point(x,y), FONT_HERSHEY_PLAIN, 0.6, ciano, 1);
        y+=8;
      }
    }
    if (achou) {
      int h=melhor.lado/2;
      rectangle(a, Rect(melhor.c-h, melhor.l-h, melhor.lado, melhor.lado), amarelo, 2);
      string t=to_string(melhor.s)+" "+to_string(int(round(melhor.cc*100)))+
               " "+to_string(int(round(melhor.ncc*100)));
      putText(a, t, Point(melhor.c-h, melhor.l-h-3), FONT_HERSHEY_PLAIN, 0.8, amarelo, 1);
    }
  }
};

//---------------------------------------------------------------------------
// Fase 3: le o video, localiza a placa em cada quadro e grava o video de
// saida. fase3.cpp chama com paralelo=false e fase3p.cpp com true.
// O fps NAO inclui mostrar na tela (o programa nao usa imshow).
//   fase3 capturado.avi quadrado.png seu_localiza.avi
//---------------------------------------------------------------------------
int rodaFase3(int argc, char** argv, bool paralelo) {
  string prog=argv[0];
  if (argc!=4) erro(prog+" entrada.avi quadrado.png saida.avi");
  PLACA placa(argv[2], paralelo);
  printf("%s: %d thread(s)\n", prog.c_str(), placa.threads());

  VideoCapture w(argv[1]);
  if (!w.isOpened()) erro("Erro: Abertura de video "+string(argv[1]));
  int nl=240, nc=320;
  double fpsVideo=w.get(CAP_PROP_FPS);
  if (!(fpsVideo>0 && fpsVideo<100)) fpsVideo=20;
  VideoWriter vo(argv[3], VideoWriter::fourcc('X','V','I','D'), fpsVideo, Size(nc,nl));
  if (!vo.isOpened()) erro("Erro: nao consegui abrir "+string(argv[3]));

  Mat_<COR> a; Mat_<FLT> f;
  int quadros=0, achados=0;
  double tProc=0, t0=timeSinceEpoch();
  while (true) {
    w >> a;
    if (!a.data) break;
    if (a.rows!=nl || a.cols!=nc) resize(a,a,Size(nc,nl));
    double t1=timeSinceEpoch();
    converte(a,f);
    if (placa.procura(f)) achados++;
    tProc += timeSinceEpoch()-t1;
    placa.desenha(a);
    vo << a;
    quadros++;
  }
  double tTotal=timeSinceEpoch()-t0;
  if (quadros==0) erro("Erro: video sem quadros");
  printf("%d quadros, placa achada em %d\n", quadros, achados);
  printf("localizacao: %.1f fps  (%.2f ms/quadro)\n", quadros/tProc, 1000*tProc/quadros);
  printf("com leitura e gravacao do video: %.1f fps\n", quadros/tTotal);
  return 0;
}

#endif
