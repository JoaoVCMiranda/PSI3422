#!/usr/bin/env python3
# /// script
# dependencies = ["pyserial", "rich"]
# ///
"""
Painel serial para os firmwares do Exp2_PSI3422 (Carrinho, Controle,
JoystickCheck, AlinhamentoCheck) -- FRDM-KL25Z / Zephyr.

Console UART @ 115200 8N1 -- NAO 9600 como diz (erradamente)
debug/README.md: `current-speed` no zephyr.dts gerado pelo build
(Carrinho, Controle e os dois firmwares de debug) é sempre
`0x1c200` = 115200, e é o mesmo valor default de
`../Controle/script/controle_serial.py`. `pio device monitor --baud`
sem essa opção também cai nesse default do board, então basta
`pio device monitor` sem `--baud 9600`.

Lê a saída bruta printk() de qualquer um dos 4 firmwares, classifica
cada linha por padrão (telemetria, comando, heartbeat, joystick bruto,
fase do AlinhamentoCheck, erro/aviso, boot) e mantém um painel ao vivo
com métricas por categoria + o log bruto. Painéis aparecem sob demanda:
só aparece o de telemetria se a placa conectada realmente mandar linhas
de telemetria, etc. -- então o mesmo script serve pras 4 placas sem
flag de "modo".

Só o Controle tem UART de comando: --control liga o forward de teclas
w/a/s/d/x/espaço/q/o pro firmware, igual controle_serial.py (mesmo
protocolo, só que aqui o log de volta vira métricas em vez de linhas
soltas).

Uso:
    uv run monitor.py [porta] [--baud 115200] [--control]

Exemplos:
    uv run monitor.py /dev/ttyACM0
    uv run monitor.py /dev/ttyACM1 --control

Sai com Ctrl+C (ou ESC se --control).
"""
from __future__ import annotations

import argparse
import re
import sys
import threading
import time
from collections import Counter, deque
from dataclasses import dataclass, field

import serial
from rich.console import Group
from rich.live import Live
from rich.panel import Panel
from rich.table import Table
from rich.text import Text

# ── Padrões de log conhecidos (ver Carrinho/src/main.c, Controle/src/main.c,
#    debug/JoystickCheck/src/main.c, debug/AlinhamentoCheck/src/main.c) ──
RE_TELEMETRY = re.compile(r"^dist=\s*(\d+)cm dutyL=\s*(-?\d+) dutyR=\s*(-?\d+)$")
RE_CMD_HEARTBEAT = re.compile(r"^heartbeat: tx_cmd = (\d) (\d) (-?\d+) (-?\d+)$")
RE_CARRINHO_RX = re.compile(r"^-> recebido: auto=(\d) freio=(\d) motL=(-?\d+) motR=(-?\d+)$")
RE_CARRINHO_HB = re.compile(
    r"^car heartbeat: dist=(\d+)cm cmd\.auto=(\d) outL=(-?\d+) outR=(-?\d+) \(sem_radio=(\d)\)$"
)
RE_JOYSTICK = re.compile(
    r"^X=\s*(-?\d+) Y=\s*(-?\d+) freio=(\d) -> motor_l=\s*(-?\d+) motor_r=\s*(-?\d+)$"
)
RE_ALIGN_PHASE = re.compile(r"^-> (andando|freando)$")
RE_RECONNECT = re.compile(r"^Reconectando modulo NRF24 do (\w+)")
RE_ERROR = re.compile(r"^ERRO")
RE_WARN = re.compile(r"^AVISO")
RE_BOOT = re.compile(r"^=+$|^=== .* ===$")

# limiar de obstáculo real do firmware -- ver DISTANCIA_MINIMA_M em
# Carrinho/lib/control_fsm/control_fsm.h (0.3 m)
DIST_OBSTACULO_CM = 30
DIST_ATENCAO_CM = 60

ESC = "\x1b"
CTRL_C = "\x03"
CONTROL_KEYS = set("wasdxq oWASDXQO")


@dataclass
class Stats:
    start_ts: float = field(default_factory=time.monotonic)
    total_lines: int = 0
    category_counts: Counter = field(default_factory=Counter)
    raw_log: deque = field(default_factory=lambda: deque(maxlen=14))

    telemetry: dict | None = None
    cmd_heartbeat: dict | None = None
    carrinho_rx: dict | None = None
    carrinho_hb: dict | None = None
    joystick: dict | None = None
    align_phase: str | None = None
    align_ts: float = 0.0
    align_cycles: int = 0

    events: deque = field(default_factory=lambda: deque(maxlen=6))


def classify_line(line: str, stats: Stats, lock: threading.Lock) -> None:
    now = time.monotonic()
    m: re.Match | None

    if (m := RE_TELEMETRY.match(line)) is not None:
        cat = "telemetry"
        with lock:
            stats.telemetry = {
                "dist_cm": int(m.group(1)),
                "duty_l": int(m.group(2)),
                "duty_r": int(m.group(3)),
                "ts": now,
            }
    elif (m := RE_CMD_HEARTBEAT.match(line)) is not None:
        cat = "cmd_heartbeat"
        with lock:
            stats.cmd_heartbeat = {
                "auto": int(m.group(1)),
                "freio": int(m.group(2)),
                "motor_l": int(m.group(3)),
                "motor_r": int(m.group(4)),
                "ts": now,
            }
    elif (m := RE_CARRINHO_RX.match(line)) is not None:
        cat = "carrinho_rx"
        with lock:
            stats.carrinho_rx = {
                "auto": int(m.group(1)),
                "freio": int(m.group(2)),
                "motor_l": int(m.group(3)),
                "motor_r": int(m.group(4)),
                "ts": now,
            }
    elif (m := RE_CARRINHO_HB.match(line)) is not None:
        cat = "carrinho_hb"
        with lock:
            stats.carrinho_hb = {
                "dist_cm": int(m.group(1)),
                "auto": int(m.group(2)),
                "out_l": int(m.group(3)),
                "out_r": int(m.group(4)),
                "sem_radio": int(m.group(5)),
                "ts": now,
            }
    elif (m := RE_JOYSTICK.match(line)) is not None:
        cat = "joystick"
        with lock:
            stats.joystick = {
                "x": int(m.group(1)),
                "y": int(m.group(2)),
                "freio": int(m.group(3)),
                "motor_l": int(m.group(4)),
                "motor_r": int(m.group(5)),
                "ts": now,
            }
    elif (m := RE_ALIGN_PHASE.match(line)) is not None:
        cat = "align"
        with lock:
            phase = m.group(1)
            if phase == "andando" and stats.align_phase == "freando":
                stats.align_cycles += 1
            stats.align_phase = phase
            stats.align_ts = now
    elif (m := RE_RECONNECT.match(line)) is not None:
        cat = "reconnect"
        with lock:
            stats.events.append((now, "warn", line))
    elif RE_ERROR.match(line):
        cat = "error"
        with lock:
            stats.events.append((now, "error", line))
    elif RE_WARN.match(line):
        cat = "warn"
        with lock:
            stats.events.append((now, "warn", line))
    elif RE_BOOT.match(line):
        cat = "boot"
    else:
        cat = "other"

    with lock:
        stats.total_lines += 1
        stats.category_counts[cat] += 1
        stats.raw_log.append((now, cat, line))


def serial_reader_loop(ser: serial.Serial, stats: Stats, lock: threading.Lock, stop: threading.Event) -> None:
    buf = b""
    while not stop.is_set():
        try:
            chunk = ser.read(256)
        except serial.SerialException:
            break
        if not chunk:
            continue
        buf += chunk
        while b"\n" in buf:
            raw, buf = buf.split(b"\n", 1)
            line = raw.decode(errors="replace").strip("\r")
            if line:
                classify_line(line, stats, lock)


def keyboard_forward_loop(ser: serial.Serial, stop: threading.Event) -> None:
    import termios
    import tty

    fd = sys.stdin.fileno()
    old_settings = termios.tcgetattr(fd)
    try:
        tty.setcbreak(fd)
        while not stop.is_set():
            key = sys.stdin.read(1)
            if key in (ESC, CTRL_C):
                stop.set()
                break
            if key in CONTROL_KEYS:
                ser.write(key.encode())
    finally:
        termios.tcsetattr(fd, termios.TCSADRAIN, old_settings)


def age_str(ts: float | None) -> str:
    if not ts:
        return "--"
    dt = time.monotonic() - ts
    return f"{dt:4.1f}s"


def dist_bar(dist_cm: int, width: int = 30) -> Text:
    if dist_cm <= DIST_OBSTACULO_CM:
        color = "red"
    elif dist_cm <= DIST_ATENCAO_CM:
        color = "yellow"
    else:
        color = "green"
    filled = min(dist_cm // 2, width)
    bar = "#" * filled + "-" * (width - filled)
    return Text(f"{dist_cm:4d} cm  {bar}", style=color)


def duty_text(label: str, duty: int) -> Text:
    color = "green" if duty > 0 else ("red" if duty < 0 else "grey58")
    return Text(f"{label}={duty:6d}", style=color)


def build_header(stats: Stats, port: str, baud: int, control: bool) -> Panel:
    elapsed = time.monotonic() - stats.start_ts
    rate = stats.total_lines / elapsed if elapsed > 0 else 0.0
    ctrl_txt = " [bold cyan]CONTROLE ATIVO[/]" if control else ""
    body = (
        f"porta=[bold]{port}[/] baud=[bold]{baud}[/] (nao 9600) | "
        f"linhas={stats.total_lines} ({rate:.1f}/s) | uptime={elapsed:5.0f}s{ctrl_txt}"
    )
    return Panel(body, title="Exp2_PSI3422 -- painel serial", border_style="blue")


def build_telemetry_panel(stats: Stats) -> Panel | None:
    t = stats.telemetry
    if t is None:
        return None
    body = Group(
        dist_bar(t["dist_cm"]),
        Text.assemble(duty_text("dutyL", t["duty_l"]), "   ", duty_text("dutyR", t["duty_r"])),
        Text(f"ultima leitura ha {age_str(t['ts'])}", style="grey58"),
    )
    return Panel(body, title="Telemetria (Carrinho -> Controle, via radio)", border_style="magenta")


def build_cmd_panel(stats: Stats) -> Panel | None:
    src = stats.cmd_heartbeat or stats.carrinho_rx
    if src is None:
        return None
    title = "Comando enviado (Controle)" if stats.cmd_heartbeat else "Comando recebido (Carrinho)"
    modo = "AUTO" if src["auto"] else "MANUAL"
    freio = "FREANDO" if src["freio"] else "livre"
    body = Group(
        Text(f"modo={modo}  freio={freio}", style="bold"),
        Text.assemble(duty_text("motor_l", src["motor_l"]), "   ", duty_text("motor_r", src["motor_r"])),
        Text(f"atualizado ha {age_str(src['ts'])}", style="grey58"),
    )
    return Panel(body, title=title, border_style="cyan")


def build_carrinho_hb_panel(stats: Stats) -> Panel | None:
    hb = stats.carrinho_hb
    if hb is None:
        return None
    radio = Text("SEM RADIO", style="bold red") if hb["sem_radio"] else Text("radio ok", style="green")
    body = Group(
        dist_bar(hb["dist_cm"]),
        Text.assemble(duty_text("outL", hb["out_l"]), "   ", duty_text("outR", hb["out_r"]), "   ", radio),
        Text(f"atualizado ha {age_str(hb['ts'])}", style="grey58"),
    )
    return Panel(body, title="Heartbeat do Carrinho (loop local)", border_style="magenta")


def build_joystick_panel(stats: Stats) -> Panel | None:
    j = stats.joystick
    if j is None:
        return None
    freio = "FREIO" if j["freio"] else "solto"
    body = Group(
        Text(f"X={j['x']:4d}  Y={j['y']:4d}  botao={freio}"),
        Text.assemble(duty_text("motor_l", j["motor_l"]), "   ", duty_text("motor_r", j["motor_r"])),
        Text(f"lido ha {age_str(j['ts'])}", style="grey58"),
    )
    return Panel(body, title="Joystick bruto (JoystickCheck)", border_style="yellow")


def build_align_panel(stats: Stats) -> Panel | None:
    if stats.align_phase is None:
        return None
    color = "green" if stats.align_phase == "andando" else "red"
    body = Group(
        Text(f"fase atual: {stats.align_phase}", style=f"bold {color}"),
        Text(f"ciclos completos: {stats.align_cycles}"),
        Text(f"desde ha {age_str(stats.align_ts)}", style="grey58"),
    )
    return Panel(body, title="AlinhamentoCheck", border_style="green")


def build_events_panel(stats: Stats) -> Panel:
    table = Table.grid(padding=(0, 1))
    table.add_column(justify="right", style="grey58")
    table.add_column()
    for name in ("error", "warn", "reconnect", "boot", "other"):
        n = stats.category_counts.get(name, 0)
        if n:
            table.add_row(name, str(n))
    lines = [table]
    if stats.events:
        lines.append(Text("---"))
        for ts, kind, line in stats.events:
            style = "red" if kind == "error" else "yellow"
            lines.append(Text(f"[{age_str(ts)}] {line}", style=style))
    return Panel(Group(*lines), title="Eventos (erro/aviso/reconexao)", border_style="red")


def build_raw_panel(stats: Stats) -> Panel:
    lines = [Text(line, style="grey70") for _, _cat, line in stats.raw_log]
    return Panel(Group(*lines) if lines else Text("(aguardando dados...)"), title="Log bruto", border_style="grey50")


def build_dashboard(stats: Stats, port: str, baud: int, control: bool) -> Group:
    panels = [build_header(stats, port, baud, control)]
    for builder in (
        build_telemetry_panel,
        build_cmd_panel,
        build_carrinho_hb_panel,
        build_joystick_panel,
        build_align_panel,
    ):
        p = builder(stats)
        if p is not None:
            panels.append(p)
    panels.append(build_events_panel(stats))
    panels.append(build_raw_panel(stats))
    return Group(*panels)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("port", nargs="?", default="/dev/ttyACM0", help="ex.: /dev/ttyACM0")
    parser.add_argument("--baud", type=int, default=115200, help="default 115200 -- ver docstring")
    parser.add_argument(
        "--control",
        action="store_true",
        help="repassa teclas w/a/s/d/x/espaco/q/o pra UART (só funciona falando com o Controle)",
    )
    args = parser.parse_args()

    try:
        ser = serial.Serial(args.port, args.baud, timeout=0.1)
    except serial.SerialException as exc:
        print(f"Erro abrindo a porta serial: {exc}")
        return 1

    stats = Stats()
    lock = threading.Lock()
    stop = threading.Event()

    reader = threading.Thread(target=serial_reader_loop, args=(ser, stats, lock, stop), daemon=True)
    reader.start()

    kb_thread = None
    if args.control:
        kb_thread = threading.Thread(target=keyboard_forward_loop, args=(ser, stop), daemon=True)
        kb_thread.start()

    try:
        with Live(build_dashboard(stats, args.port, args.baud, args.control), refresh_per_second=8, screen=True) as live:
            while not stop.is_set():
                with lock:
                    live.update(build_dashboard(stats, args.port, args.baud, args.control))
                time.sleep(0.125)
    except KeyboardInterrupt:
        pass
    finally:
        stop.set()
        ser.close()

    print("Encerrado.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
