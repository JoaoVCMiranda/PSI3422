//client6c.cpp - computador - Tarefa 1 da aula 3
//testa sendUint e receiveUint (htonl/ntohl ficam em DEVICE, no projeto.hpp)
//computador$ compila client6c -ocv -v3
#include "../../projeto.hpp"

int main(int argc, char *argv[]) {
  if (argc!=2) erro("client6c servidorIpAddr\n");
  CLIENT client(argv[1]);
  uint32_t u;
  client.receiveUint(u);
  cout << "Recebi o numero " << u << " de Raspberry" << endl;
  client.sendUint(3333333333);
  cout << "Enviei numero 3333333333 para Raspberry" << endl;
}
