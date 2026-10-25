//client8.cpp - computador - Tarefa 2 da aula 3
//testa sendVb e receiveVb
//computador$ compila client8 -ocv -v3
#include "../projeto.hpp"

int main(int argc, char *argv[]) {
  if (argc!=2) erro("client8 servidorIpAddr\n");
  CLIENT client(argv[1]);
  vector<BYTE> vb;
  client.receiveVb(vb);
  if (testaVb(vb,111)) printf("Recebi corretamente %lu bytes %u\n",(unsigned long)vb.size(),111);
  else printf("Erro na recepcao de %lu bytes %u\n",(unsigned long)vb.size(),111);
  vb.assign(100000,222);
  client.sendVb(vb);
  printf("Enviei vector<BYTE> com 100000 bytes 222\n");
  client.receiveVb(vb);
  printf("Estou aguardando receber vector<BYTE> com 100000 bytes 1\n");
  //"Erro" proposital da apostila: o server manda 2, nao 1.
  if (testaVb(vb,1)) printf("Recebi corretamente 100000 bytes 1\n");
  else printf("Nao recebi 100000 bytes 1\n");
}
