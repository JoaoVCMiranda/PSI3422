//camserver2.cpp - Raspberry - Tarefa 4 da aula 3
//Igual ao camserver1, mas envia os quadros 480x640 COM compressao JPEG
//(sendImgComp do projeto.hpp).
//raspberry$ compila camserver2 -ocv
//raspberry$ camserver2
#include "../projeto.hpp"

int main(void) {
  VideoCapture w(0);
  if (!w.isOpened()) erro("Erro: Abertura de webcam 0.");
  w.set(CAP_PROP_FRAME_WIDTH,640);
  w.set(CAP_PROP_FRAME_HEIGHT,480);

  SERVER server;
  server.waitConnection();

  Mat_<COR> a;
  int quadros=0;
  while (true) {
    w >> a;
    if (a.empty()) erro("Erro: quadro vazio da camera");
    server.sendImgComp(a);
    quadros++;
    BYTE cmd;
    server.receiveBytes(1,&cmd);
    if (cmd=='s') break;
  }
  printf("Enviei %d quadros %dx%d com compressao JPEG.\n",quadros,a.rows,a.cols);
}
