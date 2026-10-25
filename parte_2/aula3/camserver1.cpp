//camserver1.cpp - Raspberry - Tarefa 3 da aula 3
//Captura quadros coloridos 480x640 e envia SEM compressao ao camclient1.
//Depois de cada quadro, espera 1 byte do cliente: '0' = manda outro,
//'s' = termina (o usuario apertou ESC no computador).
//raspberry$ compila camserver1 -ocv
//raspberry$ camserver1
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
    server.sendImg(a);
    quadros++;
    BYTE cmd;
    server.receiveBytes(1,&cmd);
    if (cmd=='s') break;
  }
  printf("Enviei %d quadros %dx%d sem compressao.\n",quadros,a.rows,a.cols);
}
