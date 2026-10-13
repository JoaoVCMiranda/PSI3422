//raspvid.cpp - LICAO DE CASA #1/3 da aula 2 (vale 2,5)
//Mostra na tela os quadros capturados pela camera E grava-os num video.
//raspberry$ compila raspvid -ocv
//raspberry$ ./raspvid saida.avi
//Aperte qualquer tecla (com a janela em foco) para encerrar a gravacao.
#include "../projeto.hpp"

int main(int argc, char** argv) {
  if (argc!=2) erro("uso: raspvid nomevideo.avi");

  VideoCapture w(0);
  if (!w.isOpened()) erro("Erro: Abertura de webcam 0.");
  w.set(CAP_PROP_FRAME_WIDTH,640);
  w.set(CAP_PROP_FRAME_HEIGHT,480);

  Mat_<COR> a;
  namedWindow("janela");

  //----------------------------------------------------------------------
  //Fase 1: mede o fps real antes de abrir o video.
  //A apostila avisa que, via wifi, capturamos bem menos que 30 fps - gravar
  //com fps=30 fixo faria o video sair "acelerado". Entao medimos o fps de
  //verdade e usamos esse valor no VideoWriter.
  //----------------------------------------------------------------------
  const int nMedida=30;
  double t1=timeSinceEpoch();
  for (int i=0; i<nMedida; i++) {
    w >> a;
    if (a.empty()) erro("Erro: quadro vazio da camera");
    imshow("janela",a);
    waitKey(1);
  }
  double t2=timeSinceEpoch();
  double fps=nMedida/(t2-t1);
  printf("fps medido = %.2f\n",fps);

  //Size(largura,altura) e Mat(linhas,colunas) usam convencoes invertidas.
  //Trocar isso gera um video nulo, sem quadros e sem mensagem de erro.
  VideoWriter vo(argv[1], VideoWriter::fourcc('X','V','I','D'),
                 fps, Size(a.cols,a.rows));
  if (!vo.isOpened()) erro("Erro: nao consegui abrir o video de saida "+string(argv[1]));

  //----------------------------------------------------------------------
  //Fase 2: captura, mostra e grava.
  //waitKey(1) (e nao 30) para o laco rodar no mesmo ritmo da medicao acima.
  //----------------------------------------------------------------------
  int quadros=0;
  double t3=timeSinceEpoch();
  while (true) {
    w >> a;
    if (a.empty()) break;
    imshow("janela",a);
    vo << a;
    quadros++;
    int ch=(signed char)(waitKey(1));
    if (ch>=0) break;
  }
  double t4=timeSinceEpoch();

  printf("Gravados %d quadros em %.2f s (%.2f fps) em %s\n",
         quadros, t4-t3, quadros/(t4-t3), argv[1]);
}
