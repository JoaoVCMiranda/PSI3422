//camclient2.cpp - computador - Tarefa 4 da aula 3
//Recebe os quadros JPEG do camserver2, descompacta e mostra na tela.
//ESC (com a janela em foco) termina os dois programas.
//computador$ compila camclient2 -ocv -v3
//computador$ camclient2 192.168.0.110
#include "../projeto.hpp"

int main(int argc, char *argv[]) {
  if (argc!=2) erro("camclient2 servidorIpAddr\n");
  CLIENT client(argv[1]);

  Mat_<COR> a;
  namedWindow("camclient2");
  int quadros=0, nJanela=0;
  double t0=timeSinceEpoch(), tJanela=t0;
  while (true) {
    client.receiveImgComp(a);
    imshow("camclient2",a);
    quadros++; nJanela++;
    int ch=(signed char)(waitKey(1));
    BYTE cmd = (ch==27) ? 's' : '0';
    client.sendBytes(1,&cmd);
    if (cmd=='s') break;

    double t=timeSinceEpoch();
    if (t-tJanela>=2.0) {     //fps a cada 2 s
      printf("%dx%d  %.1f fps\n", a.rows, a.cols, nJanela/(t-tJanela));
      nJanela=0; tJanela=t;
    }
  }
  double t=timeSinceEpoch();
  printf("Recebi %d quadros em %.1f s: %.1f fps medios.\n",quadros,t-t0,quadros/(t-t0));
}
