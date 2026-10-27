//fase3.cpp - computador - Licao de casa #1/3 da aula 4 (fase 3, sequencial)
//Le um video 240x320, localiza a placa quadrado.png em cada quadro e grava o
//video de saida com a localizacao (figura 4 da apostila). Imprime o fps.
//O detector (CC + NCC em 10 escalas, "don't care" no quadrado amarelo) esta'
//em ../placa.hpp, compartilhado com fase3p e cliente4.
//computador$ compila fase3 -ocv -v3
//computador$ fase3 capturado.avi ../quadrado.png seu_localiza.avi
#include "../placa.hpp"

int main(int argc, char** argv) {
  return rodaFase3(argc, argv, false);
}
