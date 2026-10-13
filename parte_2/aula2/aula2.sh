#!/usr/bin/env bash
# aula2.sh - roteiro da Aula 2 da PSI3422 parte 2.
# Detecta sozinho se esta rodando no Raspberry ou no computador e usa as
# flags certas de compilacao. Rode sem argumento para ver a ajuda.
set -euo pipefail

AQUI="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$AQUI"

RASPI_PADRAO=192.168.15.44

# --- onde estamos? --------------------------------------------------------
if [ -e /usr/include/wiringPi.h ] && [ "$(uname -m)" = armv7l ]; then
  ONDE=raspberry
else
  ONDE=computador
fi

# --- o 'compila' do Cekeikon so' existe em shell interativo (vem do .bashrc),
# --- entao por ssh precisamos carregar o ambiente na mao.
# Cada arquitetura tem o SEU script de ativacao:
#   x86_64 (Mint)      -> ativa_cekcpu          (e' o que a apostila manda)
#   armv7l (Raspberry) -> inst-cek5-raspberry.sh
# Carregar o do Raspberry num PC monta PATH/LD_LIBRARY_PATH errados, e o
# 'compila' morre com SIGABRT (saida 134).
if [ "${SEM_CEKEIKON:-0}" != 1 ] && ! command -v compila >/dev/null 2>&1; then
  case "$(uname -m)" in
    arm*|aarch64) ATIVA=(~/cekeikon5/bin/inst-cek5-raspberry.sh) ;;
    *)            ATIVA=(~/cekeikon5/bin/ativa_cekcpu
                         ~/cekeikon5/bin/inst-cek5-cpu-pycv3.sh) ;;
  esac
  # Os scripts do Cekeikon fazem "export LD_LIBRARY_PATH=...:$LD_LIBRARY_PATH"
  # sem guarda; com 'set -u' isso derruba o shell. Relaxamos so' no source.
  set +u +e
  for s in "${ATIVA[@]}"; do
    [ -f "$s" ] && { source "$s" >/dev/null 2>&1; break; }
  done
  set -u -e
fi
TEM_COMPILA=0
if [ "${SEM_CEKEIKON:-0}" != 1 ] && command -v compila >/dev/null 2>&1; then
  TEM_COMPILA=1
fi

vermelho() { printf '\033[31m%s\033[0m\n' "$*"; }
verde()    { printf '\033[32m%s\033[0m\n' "$*"; }

# --- compilacao -----------------------------------------------------------
# Se o Cekeikon estiver instalado usa 'compila' (o que a materia pede).
# Senao cai num g++ equivalente - util no notebook, que nao tem Cekeikon.
# Monta e roda a linha de g++ equivalente a "compila <nome> <flags>".
# OCV_FLAGS permite passar na mao os -I/-L/-l do OpenCV, para o caso de o
# pkg-config nao conhecer o OpenCV (ex.: o OpenCV 3 que vem no Cekeikon):
#   OCV_FLAGS="-I$HOME/cekeikon5/cekeikon/include -L... -lopencv_core ..." ./aula2.sh build
constroi_gpp() {
  local nome=$1; shift
  local cv=0 wp=0 f
  for f in "$@"; do
    case $f in
      -ocv) cv=1 ;;
      -w)   wp=1 ;;
    esac
  done

  local args=(-std=c++11 -O2 -o "$nome" "$nome.cpp")
  if [ "$cv" = 1 ]; then
    if [ -n "${OCV_FLAGS:-}" ]; then
      args+=( ${OCV_FLAGS} )
    elif pkg-config --exists opencv4; then
      args+=( $(pkg-config --cflags --libs opencv4) )
      # raspberry.hpp usa CV_BGR2GRAY/CV_GRAY2BGR, removidos no OpenCV 4
      args+=(-DCV_BGR2GRAY=cv::COLOR_BGR2GRAY -DCV_GRAY2BGR=cv::COLOR_GRAY2BGR)
    elif pkg-config --exists opencv; then
      args+=( $(pkg-config --cflags --libs opencv) )
    else
      echo "sem OpenCV: nem pkg-config nem OCV_FLAGS" >/tmp/aula2-build.log
      return 1
    fi
  fi
  [ "$wp" = 1 ] && args+=(-lwiringPi -lpthread)

  g++ "${args[@]}" >/tmp/aula2-build.log 2>&1
}

constroi() {
  local nome=$1; shift
  printf '  %-12s ' "$nome"

  if [ "$TEM_COMPILA" = 1 ]; then
    local st=0
    compila "$nome" "$@" >/tmp/aula2-build.log 2>&1 || st=$?
    if [ "$st" = 0 ]; then verde OK; return 0; fi

    # 134 = SIGABRT: tipicamente ambiente do Cekeikon montado errado.
    local extra=""
    [ "$st" = 134 ] && extra=" (SIGABRT - ambiente do Cekeikon)"
    printf '\033[33mcompila falhou [%s]%s, tentando g++...\033[0m ' "$st" "$extra"
    [ -s /tmp/aula2-build.log ] && sed 's/^/     /' /tmp/aula2-build.log | head -6
  fi

  if constroi_gpp "$nome" "$@"; then
    verde "OK (g++)"
  else
    vermelho FALHOU; sed 's/^/     /' /tmp/aula2-build.log | head -12; return 1
  fi
}

cmd_build() {
  echo "Compilando em: $ONDE  ($([ "$TEM_COMPILA" = 1 ] && echo 'compila/Cekeikon' || echo 'g++ (Cekeikon ausente)'))"
  if [ "$ONDE" = raspberry ]; then
    constroi blink3    -w
    constroi paramotor -w
    constroi pwmroda4  -w -ocv
    constroi raspcam2  -ocv
    constroi raspvid   -ocv
    constroi server6b  -ocv
  else
    constroi client6b  -ocv -v3
  fi
}

# --- checagens ------------------------------------------------------------
checa_raspberry() {
  [ "$ONDE" = raspberry ] || { vermelho "Isto roda no Raspberry, nao no computador."; exit 1; }
}

checa_display() {
  if [ -z "${DISPLAY:-}" ]; then
    vermelho "DISPLAY vazio: um programa com janela nao vai abrir."
    echo     "Conecte com 'ssh -X pi@<ip>' ou use o VNC."
    exit 1
  fi
}

garante() { [ -x "./$1" ] || constroi "$@"; }

# --- comandos -------------------------------------------------------------
cmd_status() {
  echo "local .............. $ONDE ($(uname -m))"
  echo "compila ............ $(command -v compila || echo 'AUSENTE')"
  echo "CEK5_PATH .......... ${CEK5_PATH:-'(nao definido)'}"
  echo "OpenCV (pkg-config)  $(pkg-config --modversion opencv4 2>/dev/null \
                               || pkg-config --modversion opencv 2>/dev/null \
                               || echo 'nenhum - use OCV_FLAGS')"
  echo "DISPLAY ............ ${DISPLAY:-'(vazio - sem janela)'}"
  if [ "$ONDE" = raspberry ]; then
    echo "camera ............. $(ls /dev/video0 2>/dev/null || echo 'AUSENTE')"
    echo "                     $(lsusb 2>/dev/null | grep -i logi || echo '(nenhuma Logitech no USB)')"
    echo "wiringPi ........... $(gpio -v 2>/dev/null | head -1 || echo AUSENTE)"
    echo "IP ................. $(hostname -I)"
  else
    echo "Raspberry padrao ... $RASPI_PADRAO"
    printf "ping ............... "
    ping -c1 -W2 "$RASPI_PADRAO" >/dev/null 2>&1 && verde alcancavel || vermelho inalcancavel
  fi
  echo "binarios ........... $(ls -1 2>/dev/null | grep -vE '\.(cpp|sh|md)$' | tr '\n' ' ')"
}

cmd_blink()  { checa_raspberry; garante blink3 -w
               echo "Meça os pinos fisicos 11, 12, 13 e 15: devem alternar 3,3V <-> 0V a cada 2s."
               echo "FACA ISTO ANTES DE LIGAR A PONTE-H."
               ./blink3; }

cmd_pwm()    { checa_raspberry; garante pwmroda4 -w -ocv
               echo "Licao #2/3(b): carrinho gira horario/anti-horario a 90% e depois a 60%."
               echo "Esperado nos pinos: ~2,97V (90%), ~1,98V (60%), 0V - 100Hz."
               ./pwmroda4; }

cmd_parar()  { checa_raspberry; garante paramotor -w
               ./paramotor; verde "motores desligados"; }

cmd_cam()    { checa_raspberry; checa_display; garante raspcam2 -ocv; ./raspcam2; }

cmd_vid()    { checa_raspberry; checa_display; garante raspvid -ocv
               local saida=${1:-saida.avi}
               echo "Licao #1/3: gravando em $saida. Qualquer tecla (na janela) encerra."
               ./raspvid "$saida"
               ls -lh "$saida"; }

cmd_server() { checa_raspberry; garante server6b -ocv
               echo "Licao #3/3: aguardando o cliente. No computador rode:"
               echo "    ./aula2.sh client $(hostname -I | awk '{print $1}')"
               ./server6b; }

cmd_client() { [ "$ONDE" = computador ] || { vermelho "Isto roda no computador."; exit 1; }
               garante client6b -ocv -v3
               ./client6b "${1:-$RASPI_PADRAO}"; }

ajuda() {
  cat <<AJUDA
aula2.sh - roteiro da Aula 2 (PSI3422 parte 2)    [local: $ONDE]

  ./aula2.sh status           estado do ambiente (camera, GPIO, IP, binarios)
  ./aula2.sh build            compila o que faz sentido para este local

 Raspberry:
  ./aula2.sh blink            blink3  - teste dos 4 GPIO ANTES de ligar a ponte-H
  ./aula2.sh pwm              pwmroda4 - licao #2/3(b), PWM 90% e 60%
  ./aula2.sh parar            paramotor - PARADA DE EMERGENCIA dos motores
  ./aula2.sh cam              raspcam2 - camera na tela (precisa de ssh -X ou VNC)
  ./aula2.sh vid [arq.avi]    raspvid  - licao #1/3, grava video (default saida.avi)
  ./aula2.sh server           server6b - licao #3/3, lado Raspberry

 Computador:
  ./aula2.sh client [ip]      client6b - licao #3/3 (default $RASPI_PADRAO)

Ordem sugerida: status -> build -> blink -> (fiacao) -> pwm -> vid -> server/client

Variaveis de ambiente:
  SEM_CEKEIKON=1   ignora o 'compila' e vai direto de g++
  OCV_FLAGS="..."  passa na mao os -I/-L/-l do OpenCV, quando o pkg-config
                   nao conhece o OpenCV instalado. Exemplo:
                   OCV_FLAGS="-I/usr/include/opencv4 -lopencv_core ..." ./aula2.sh build

Se o 'compila' falhar, o script avisa e tenta o g++ sozinho - nao precisa
fazer nada.
AJUDA
}

case "${1:-ajuda}" in
  status) cmd_status ;;
  build)  cmd_build ;;
  blink)  cmd_blink ;;
  pwm)    cmd_pwm ;;
  parar|stop) cmd_parar ;;
  cam)    cmd_cam ;;
  vid)    shift; cmd_vid "$@" ;;
  server) cmd_server ;;
  client) shift; cmd_client "$@" ;;
  ajuda|-h|--help) ajuda ;;
  *) vermelho "comando desconhecido: $1"; echo; ajuda; exit 1 ;;
esac
