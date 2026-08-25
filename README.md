# Laboratório de Sistemas Eletrônicos

## Exp1 - Profª Yu

### Aula 1
- [X] Entregue, porém podemos fazer melhor

Objetivos:

    Programar o carrinho para: avançar, recuar e fazer curvas.
    Detectar obstáculos e desviar-se deles.

Roteiro:

    Conecte a ponte H aos motores e programe o carrinho para avançar/recuar;
    Controle a velocidade dos motores para fazer curvas.
    Programe o sensor ultrassom para detectar obstáculos e desviar-se deles. 

Última atualização: terça-feira, 4 ago. 2026, 08:45

### Aula 2
- [X] Entregue — ver `experiências/Exp2_PSI3422/` (Carrinho + Controle, nRF24L01+ via `lib/spi` e `lib/nrf24` compartilhadas)
Objetivos:

    Implementar a comunicação serial entre duas placas de desenvolvimento Freedom (KL25Z) utilizando os tranceivers nRF24L01+.
    Através do terminal do computador, controlar remotamente um LED na outra placa Freedom.

Roteiro:

    Implemente a comunicação serial entre duas placas de desenvolvimento Freedom (KL25Z) utilizando os tranceivers nRF24L01+.
    Conecte os dois transceptores às duas placas Freedom de acordo com as orientações da apresentação.
    OBS1: Lembre-se que a tensão máxima de alimentação do nRF24L01+ é de 3,6!
    OBS2: Verifique a versão do seu módulo nRF24L01, pois os pinos mudam dependendo da versão do componente.
    Programe os transceptores para receber/trasmitir dados.
    Conecte as duas placas no mesmo computador e utilize o Serial Monitor para verificar a comunicação.
    Através do terminal do computador, controle remotamente um LED na outra placa Freedom.

Observações:

    Os transceptores utilizam a comunicação SPI.
    Freedom KL25Z suporta a comunicação SPI.
    Além da comunicação SPI, os pinos CE e CSN (IRQ opcional) precisam ser configurados para transmitir/receber dados.
    Os comandos de operação estão listados no Datasheet do módulo.
    A apresentação mostra de forma sucinta como programar cada modo de operação do módulo.

Última atualização: terça-feira, 11 ago. 2026, 09:19 

### Aula 3 
- [ ] Por Fazer
Objetivos:

    Criar uma placa de circuito impresso para interconectar os módulos que serão usados no carrinho.
    Usando esta placa, o carrinho deverá receber um comando remoto para movimentar os motores.
    O carrinho deverá ser alimentado pela bateria.

Observações:

    Defina quais pinos serão utilizados por cada componente antes de criar a placa de circuito impresso.
    Tamanho recomendado para os furos das ilhas: 2 mm
    Tamanho recomendado para as trilhas: 1 mm

Componentes:

    Microcontrolador FRDM KL25Z
    Sensor ultrassônico HC-SR04
    Módulo de comunicação wireless nRF24L01+
    Motores (Ponte H)
    Encoder dos motores (consulte os objetivos da "Aula 4" para verificar o uso dos encoders neste projeto)

Última atualização: segunda-feira, 17 ago. 2026, 16:32
### Aula 4 
Objetivos:

    Acople um encoder(sensor de obstáculos IR HW-201) em cada motor.

    Calibre o encoder para estimar a distância percorrida pelo carrinho.
    Calibre o encoder para controlar o movimento do carrinho e fazer:
        Uma curva de 90° para direita;
        Uma curva de 90° para esquerda.
    OBS: Essa parece uma aplicação interessante de programação competitiva, um código que dados dois contadores de rotações para cada roda, calcule qual é a distância percorrida.

Última atualização: segunda-feira, 17 ago. 2026, 16:32

### Aula 5 e 6
Objetivos:

    Monte o Shield e os módulos do carrinho.
    Programação do carrinho para atravessar o percurso fechado por obstáculos e estimar a distância percorrida.

Comportamento esperado do carrinho na entrega final/apresentação:
Controle remoto pelo computador:

    Controle o carrinho de forma remota através da comunicação wireless:
        Adicione um comando para entrar no estado RUN.
        Adicione um comando para entrar no estado STOP.
        Adicione um comando para mostrar a distância percorrida no computador.
        Adicione um comando para apagar a distância percorrida salvo no carrinho.

Funções no estado RUN:

    Crie uma lógica para o carrinho atravessar um labirinto.
    O carrinho pode avançar, recuar e fazer curvas.
    Estime e salve a distância percorrida (dar meia-volta não reduz a distância percorrida).

Funções no estado STOP:

    O carrinho fica parado e aguarda novas instruções.

Última atualização: segunda-feira, 17 ago. 2026, 19:55
