//raspvid.cpp - LICAO DE CASA #1/3 da aula 2 (vale 2,5)
//Captura da camera e grava num video, com a pre-visualizacao na tela
//podendo ser ligada ou desligada.
//
//raspberry$ compila raspvid -ocv
//raspberry$ ./raspvid saida.avi              (com tela, default)
//raspberry$ ./raspvid -n saida.avi           (sem tela)
//
//POR QUE A FLAG: transmitir a janela por VNC ou "ssh -X" consome banda,
//derruba quadros da captura e o video resultante sai "acelerado". Gravando
//sem tela, a captura roda na velocidade real da camera.
//
//Com tela:  qualquer tecla (com a janela em foco) encerra.
//Sem tela:  Ctrl+C encerra - o video e' fechado corretamente.
#include "../projeto.hpp"
#include <csignal>

static volatile sig_atomic_t parar=0;
static void trataSinal(int) { parar=1; }

int main(int argc, char** argv) {
  string nomeVideo="saida.avi";
  bool comTela=true;

  for (int i=1; i<argc; i++) {
    string a=argv[i];
    if      (a=="-n" || a=="--sem-tela") comTela=false;
    else if (a=="-t" || a=="--com-tela") comTela=true;
    else if (!a.empty() && a[0]=='-')
      erro("uso: raspvid [-n|--sem-tela] [-t|--com-tela] [arquivo.avi]");
    else nomeVideo=a;
  }

  //Pedir tela sem ter DISPLAY trava o programa num erro obscuro do HighGUI.
  if (comTela && getenv("DISPLAY")==NULL) {
    printf("Aviso: DISPLAY vazio, gravando sem tela. Ctrl+C para encerrar.\n");
    comTela=false;
  }

  if (!comTela) {
    signal(SIGINT, trataSinal);
    signal(SIGTERM, trataSinal);
  }

  VideoCapture w(0);
  if (!w.isOpened()) erro("Erro: Abertura de webcam 0.");
  w.set(CAP_PROP_FRAME_WIDTH,640);
  w.set(CAP_PROP_FRAME_HEIGHT,480);

  Mat_<COR> a;
  if (comTela) namedWindow("janela");

  //----------------------------------------------------------------------
  //Fase 1: mede o fps real antes de abrir o video.
  //A apostila avisa que, via wifi, capturamos bem menos que 30 fps - gravar
  //com fps=30 fixo faria o video sair acelerado. Entao medimos o fps de
  //verdade. A medicao roda no MESMO modo da gravacao (com ou sem tela),
  //senao o valor medido nao valeria para o laco seguinte.
  //----------------------------------------------------------------------
  //Os primeiros quadros saem rapido demais (buffer/aquecimento da camera) e
  //inflam a medicao - o video acabaria gravado com fps maior que o real, ou
  //seja, acelerado. Descartamos esses antes de cronometrar.
  const int nAquece=15;
  for (int i=0; i<nAquece && !parar; i++) {
    w >> a;
    if (a.empty()) erro("Erro: quadro vazio da camera");
    if (comTela) { imshow("janela",a); waitKey(1); }
  }

  const int nMedida=30;
  double t1=timeSinceEpoch();
  for (int i=0; i<nMedida && !parar; i++) {
    w >> a;
    if (a.empty()) erro("Erro: quadro vazio da camera");
    if (comTela) { imshow("janela",a); waitKey(1); }
  }
  double t2=timeSinceEpoch();
  double fps=nMedida/(t2-t1);
  printf("fps medido = %.2f  (tela %s)\n", fps, comTela?"ligada":"desligada");

  //Size(largura,altura) e Mat(linhas,colunas) usam convencoes invertidas.
  //Trocar isso gera um video nulo, sem quadros e sem mensagem de erro.
  VideoWriter vo(nomeVideo, VideoWriter::fourcc('X','V','I','D'),
                 fps, Size(a.cols,a.rows));
  if (!vo.isOpened()) erro("Erro: nao consegui abrir o video de saida "+nomeVideo);

  printf("Gravando em %s. %s\n", nomeVideo.c_str(),
         comTela ? "Qualquer tecla na janela encerra." : "Ctrl+C encerra.");

  //----------------------------------------------------------------------
  //Fase 2: captura, (opcionalmente) mostra, e grava.
  //----------------------------------------------------------------------
  int quadros=0;
  double t3=timeSinceEpoch();
  while (!parar) {
    w >> a;
    if (a.empty()) break;
    if (comTela) {
      imshow("janela",a);
      int ch=(signed char)(waitKey(1));
      if (ch>=0) break;
    }
    vo << a;
    quadros++;
  }
  double t4=timeSinceEpoch();

  vo.release();   //fecha o AVI antes de imprimir o resumo
  w.release();
  if (comTela) destroyAllWindows();

  double real = (t4>t3) ? quadros/(t4-t3) : 0.0;
  printf("Gravados %d quadros em %.2f s -> %.2f fps reais "
         "(gravado a %.2f fps)\n", quadros, t4-t3, real, fps);
  if (quadros>0 && fps>0 && (real/fps < 0.8 || real/fps > 1.25))
    printf("Aviso: fps real ficou %.0f%% do medido - o video vai parecer %s.\n",
           100*real/fps, (real<fps) ? "acelerado" : "lento");
}
