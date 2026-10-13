#include <opencv2/opencv.hpp>
#include <iostream>
#include <cstdlib>

using namespace std;
using namespace cv;

int main() {
    // Abre a webcam
    VideoCapture w(0);

    if (!w.isOpened()) {
        fprintf(stderr, "Erro: Abertura de webcam 0.\n");
        return 1;
    }

    // Define a resolucao da captura
    w.set(CAP_PROP_FRAME_WIDTH, 640);
    w.set(CAP_PROP_FRAME_HEIGHT, 480);

    // Cria o gravador de video com nome fixo
    VideoWriter vo(
        "video.avi",
        VideoWriter::fourcc('X', 'V', 'I', 'D'),
        30,
        Size(640, 480)
    );

    if (!vo.isOpened()) {
        cerr << "Erro: Nao foi possivel criar o video video.avi"
             << endl;
        return 1;
    }

    Mat a;
    namedWindow("janela");

    while (true) {
        w >> a;  // Captura um quadro da camera

        if (a.empty())
            break;

        // Ajusta o tamanho para o formato de saida
        if (a.size() != Size(640, 480))
            resize(a, a, Size(640, 480));

        imshow("janela", a); // Mostra o quadro na tela
        vo << a;             // Grava o quadro no video

        int ch = (signed char)(waitKey(30));

        if (ch >= 0)
            break;
    }

    // Fecha os arquivos e libera os recursos
    vo.release();
    w.release();
    destroyAllWindows();

    return 0;
}