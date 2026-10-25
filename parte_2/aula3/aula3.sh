#!/usr/bin/env bash
# aula3.sh - compila os programas da Aula 3 da PSI3422 parte 2.
# Detecta se esta no Raspberry ou no computador e usa as flags certas.
# Mesma logica de compilacao do ../aula2/aula2.sh (compila do Cekeikon, ou
# g++ equivalente se ele faltar/falhar).
set -euo pipefail

AQUI="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$AQUI"

if [ -e /usr/include/wiringPi.h ] && [ "$(uname -m)" = armv7l ]; then
  ONDE=raspberry
else
  ONDE=computador
fi

# O 'compila' so' existe em shell interativo (vem do .bashrc).
if [ "${SEM_CEKEIKON:-0}" != 1 ] && ! command -v compila >/dev/null 2>&1; then
  case "$(uname -m)" in
    arm*|aarch64) ATIVA=(~/cekeikon5/bin/inst-cek5-raspberry.sh) ;;
    *)            ATIVA=(~/cekeikon5/bin/ativa_cekcpu
                         ~/cekeikon5/bin/inst-cek5-cpu-pycv3.sh) ;;
  esac
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

constroi_gpp() {
  local nome=$1; shift
  local wp=0 f
  for f in "$@"; do [ "$f" = -w ] && wp=1; done
  local args=(-std=c++11 -O2 -o "$nome" "$nome.cpp")
  if [ -n "${OCV_FLAGS:-}" ]; then           args+=( ${OCV_FLAGS} )
  elif pkg-config --exists opencv4; then     args+=( $(pkg-config --cflags --libs opencv4) )
  elif pkg-config --exists opencv; then      args+=( $(pkg-config --cflags --libs opencv) )
  else echo "sem OpenCV: nem pkg-config nem OCV_FLAGS" >/tmp/aula3-build.log; return 1
  fi
  [ "$wp" = 1 ] && args+=(-lwiringPi -lpthread)
  g++ "${args[@]}" >/tmp/aula3-build.log 2>&1
}

constroi() {
  local nome=$1; shift
  printf '  %-12s ' "$nome"
  if [ "$TEM_COMPILA" = 1 ]; then
    if compila "$nome" "$@" >/tmp/aula3-build.log 2>&1; then verde OK; return 0; fi
    printf '\033[33mcompila falhou, tentando g++...\033[0m '
  fi
  if constroi_gpp "$nome" "$@"; then verde "OK (g++)"
  else vermelho FALHOU; sed 's/^/     /' /tmp/aula3-build.log | head -12; return 1
  fi
}

cmd_build() {
  echo "Compilando em: $ONDE"
  if [ "$ONDE" = raspberry ]; then
    for p in server6c server8 camserver1 camserver2 servidor1; do constroi $p -ocv; done
    constroi servidor2 -ocv -w
  else
    for p in client6c client8 camclient1 camclient2 cliente1 cliente2; do constroi $p -ocv -v3; done
  fi
}

case "${1:-ajuda}" in
  build) cmd_build ;;
  *) cat <<AJUDA
aula3.sh - Aula 3 (PSI3422 parte 2)    [local: $ONDE]

  ./aula3.sh build     compila os programas deste local

Depois, servidor no Raspberry e cliente no computador (ver README.md):
  tarefa 1  ./server6c    | ./client6c   <ip>
  tarefa 2  ./server8     | ./client8    <ip>
  tarefa 3  ./camserver1  | ./camclient1 <ip>
  tarefa 4  ./camserver2  | ./camclient2 <ip>
  tarefa 5  ./servidor1   | ./cliente1   <ip> [video.avi] [t/c]
  tarefa 6  ./servidor2   | ./cliente2   <ip> [video.avi] [t/c]
AJUDA
  ;;
esac
