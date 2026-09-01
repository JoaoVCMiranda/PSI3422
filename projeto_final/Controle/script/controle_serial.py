#!/usr/bin/env python3
"""
Script de comunicação com a placa "Controle" do projeto_final (PSI3422).

Repassa cada tecla digitada no terminal, sem esperar Enter, direto
para a UART da placa (que já entende w/a/s/d/x/q/o/i/c — ver
src/main.c) e imprime de volta qualquer linha que a placa mandar (as
linhas de telemetria "dist=... dutyL=... dutyR=... percorrida=...").

Uso:
    uv run --with pyserial scripts/controle_serial.py --port /dev/ttyACM0
    # ou, com pyserial já instalado:
    python3 scripts/controle_serial.py --port /dev/ttyACM0

Sai com ESC ou Ctrl+C.

Só testado em Linux/macOS (usa termios para ler teclas sem Enter).
"""

import argparse
import sys
import termios
import threading
import tty

import serial

ESC = "\x1b"
CTRL_C = "\x03"


def read_serial_forever(ser: serial.Serial, stop: threading.Event) -> None:
    buf = b""
    while not stop.is_set():
        try:
            chunk = ser.read(64)
        except serial.SerialException:
            break
        if not chunk:
            continue
        buf += chunk
        while b"\n" in buf:
            line, buf = buf.split(b"\n", 1)
            text = line.decode(errors="replace").rstrip("\r")
            if text:
                print(f"\r[carrinho] {text}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True, help="ex.: /dev/ttyACM0")
    parser.add_argument("--baud", type=int, default=115200)
    args = parser.parse_args()

    ser = serial.Serial(args.port, args.baud, timeout=0.1)

    print("=== PSI3422 projeto_final -- controle_serial.py ===")
    print(f"Porta: {args.port} @ {args.baud}")
    print("w/a/s/d = mover, x/espaço = STOP, o = RUN (desvio autônomo)")
    print("q = ponto morto, i = mostrar distância percorrida, c = apagar distância")
    print("ESC ou Ctrl+C para sair.\n")

    stop = threading.Event()
    reader = threading.Thread(target=read_serial_forever, args=(ser, stop), daemon=True)
    reader.start()

    fd = sys.stdin.fileno()
    old_settings = termios.tcgetattr(fd)
    try:
        tty.setcbreak(fd)
        while True:
            key = sys.stdin.read(1)
            if key in (ESC, CTRL_C):
                break
            ser.write(key.encode())
    finally:
        termios.tcsetattr(fd, termios.TCSADRAIN, old_settings)
        stop.set()
        ser.close()

    print("\nEncerrado.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
