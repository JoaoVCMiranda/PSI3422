//raspcam2.cpp - programa base da apostila (secao 3)
//Mostra na tela os quadros capturados pela camera.
//raspberry$ compila raspcam2 -ocv
#include <opencv2/opencv.hpp>
using namespace std;
using namespace cv;

int main() {
  VideoCapture w(0);
  if (!w.isOpened()) {
    fprintf(stderr,"Erro: Abertura de webcam 0.");
    exit(1);
  }
  w.set(CAP_PROP_FRAME_WIDTH,640);
  w.set(CAP_PROP_FRAME_HEIGHT,480);
  Mat_<Vec3b> a;
  namedWindow("janela");
  while (true) {
    // w.grab(); //Esvazia o buffer da camera: diminui latencia, mas diminui fps
    w >> a;      //Pega um novo quadro da camera
    imshow("janela",a);
    int ch=(signed char)(waitKey(30)); //E' necessario (signed char)
    if (ch>=0) break;
  }
}
