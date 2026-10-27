//fase3p.cpp - computador - Licao de casa #2/3 da aula 4 (fase 3, paralela)
//Igual ao fase3, mas os 20 template matchings de cada quadro (10 CC + 10 NCC)
//rodam em paralelo com OpenMP: o "#pragma omp parallel for" do
//PLACA::procura (../placa.hpp) so' vale com paralelo=true e compilando
//com -omp. Sem -omp o pragma e' ignorado e o programa fica sequencial.
//computador$ compila fase3p -ocv -v3 -omp
//computador$ fase3p capturado.avi ../quadrado.png seu_localiza.avi
#include "../placa.hpp"

int main(int argc, char** argv) {
#ifndef _OPENMP
  printf("AVISO: compilado sem -omp, vai rodar sequencial\n");
#endif
  return rodaFase3(argc, argv, true);
}
