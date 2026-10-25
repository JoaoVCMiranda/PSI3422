//server6c.cpp - Raspberry - Tarefa 1 da aula 3
//testa sendUint e receiveUint (htonl/ntohl ficam em DEVICE, no projeto.hpp)
//raspberry$ compila server6c -ocv
#include "../../projeto.hpp"

int main(void) {
  SERVER server;
  server.waitConnection();
  server.sendUint(1234567890);
  cout << "Enviei numero 1234567890 para Computador." << endl;
  uint32_t u;
  server.receiveUint(u);
  cout << "Recebi o numero " << u << " de Computador." << endl;
}
