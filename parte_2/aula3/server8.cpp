//server8.cpp - Raspberry - Tarefa 2 da aula 3
//testa sendVb e receiveVb
//raspberry$ compila server8 -ocv
#include "../projeto.hpp"

int main(void) {
  SERVER server;
  server.waitConnection();
  vector<BYTE> vb;
  vb.assign(100000,111);
  server.sendVb(vb);
  printf("Enviei vector<BYTE> com 100000 bytes 111\n");
  server.receiveVb(vb);
  if (testaVb(vb,222)) printf("Recebi corretamente %lu bytes %u\n",(unsigned long)vb.size(),222);
  else printf("Erro na recepcao de %lu bytes %u\n",(unsigned long)vb.size(),222);
  vb.assign(100000,2);
  printf("Enviei vector<BYTE> com 100000 bytes 2\n");
  server.sendVb(vb);
}
