#!/usr/bin/env bash
# aula4.sh - compila e mede os programas da Aula 4 da PSI3422 parte 2.
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
  local wp=0 omp=0 f
  for f in "$@"; do [ "$f" = -w ] && wp=1; [ "$f" = -omp ] && omp=1; done
  local args=(-std=c++11 -O2 -o "$nome" "$nome.cpp")
  if [ -n "${OCV_FLAGS:-}" ]; then           args+=( ${OCV_FLAGS} )
  elif pkg-config --exists opencv4; then     args+=( $(pkg-config --cflags --libs opencv4) )
  elif pkg-config --exists opencv; then      args+=( $(pkg-config --cflags --libs opencv) )
  else echo "sem OpenCV: nem pkg-config nem OCV_FLAGS" >/tmp/aula4-build.log; return 1
  fi
  [ "$wp" = 1 ] && args+=(-lwiringPi -lpthread)
  [ "$omp" = 1 ] && args+=(-fopenmp)
  g++ "${args[@]}" >/tmp/aula4-build.log 2>&1
}

constroi() {
  # $1 = tarefaN/programa: compila dentro da pasta da tarefa
  local dir=${1%/*} nome=${1##*/}; shift
  printf '  %-20s ' "$dir/$nome"
  (
    cd "$dir"
    if [ "$TEM_COMPILA" = 1 ]; then
      if compila "$nome" "$@" >/tmp/aula4-build.log 2>&1; then verde OK; exit 0; fi
      printf '\033[33mcompila falhou, tentando g++...\033[0m '
    fi
    if constroi_gpp "$nome" "$@"; then verde "OK (g++)"
    else vermelho FALHOU; sed 's/^/     /' /tmp/aula4-build.log | head -12; exit 1
    fi
  )
}

cmd_build() {
  echo "Compilando em: $ONDE"
  if [ "$ONDE" = raspberry ]; then
    constroi tarefa3/servidor4 -ocv -w
  else
    constroi tarefa1/fase3 -ocv -v3
    constroi tarefa2/fase3p -ocv -v3 -omp
    constroi tarefa3/cliente4 -ocv -v3 -omp
  fi
}

# Roda fase3 e fase3p no mesmo video, para a tabela de fps da licao #2/3
cmd_fps() {
  local v=${1:-capturado.avi}
  [ -f "$v" ] || { vermelho "nao achei $v"; exit 1; }
  [ -f quadrado.png ] || { vermelho "nao achei quadrado.png (baixe do site da apostila)"; exit 1; }
  local b=$(basename "$v" .avi)
  tarefa1/fase3  "$v" quadrado.png "localiza_${b}_seq.avi"
  echo
  tarefa2/fase3p "$v" quadrado.png "localiza_${b}_par.avi"
}

case "${1:-ajuda}" in
  build) cmd_build ;;
  fps)   shift; cmd_fps "$@" ;;
  *) cat <<AJUDA
aula4.sh - Aula 4 (PSI3422 parte 2)    [local: $ONDE]

  ./aula4.sh build              compila os programas deste local
  ./aula4.sh fps [video.avi]    roda fase3 e fase3p no video (default capturado.avi)

Depois (ver README.md):
  tarefa1/fase3  capturado.avi quadrado.png saida.avi
  tarefa2/fase3p capturado.avi quadrado.png saida.avi
  tarefa3/servidor4 (Raspberry) | tarefa3/cliente4 <ip> [video.avi] [t/c]
AJUDA
  ;;
esac
