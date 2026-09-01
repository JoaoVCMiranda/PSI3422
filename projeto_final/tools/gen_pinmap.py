#!/usr/bin/env python3
"""
Gera pinmap.h, Pinmap.md e kicad/PSI3422_shield.kicad_sym a partir de
pinmap.yaml — fonte única do portmap da FRDM-KL25Z (Carrinho +
Controle), pra código e PCB nunca ficarem fora de sincronia entre si
(motivo: os dois `Pinmap.md`/`pinmap.md` de Exp2/Exp4 já eram
quase-duplicados, mesma informação mantida à mão em dois lugares).

Uso (de dentro de projeto_final/):
    python3 tools/gen_pinmap.py

Sem dependência externa (nem PyYAML) de propósito: quem rodar isso
não precisa ter nada instalado além do Python 3 padrão. Por isso o
parser abaixo NÃO é um parser de YAML genérico — só entende o
subconjunto usado em pinmap.yaml (uma lista `pins:` de mapas com
chave: valor simples, listas inline `[a, b]` e comentários `#`). Se
pinmap.yaml crescer pra usar YAML mais complexo, é hora de trocar por
PyYAML de verdade.

Arquivos gerados (não editar à mão, editar pinmap.yaml e rodar de
novo):
    pinmap.h
    Pinmap.md
    kicad/PSI3422_shield.kicad_sym

Ressalva sobre o .kicad_sym: gerado seguindo o formato de arquivo
(S-expression) do KiCad 6/7 por inspeção do formato, sem KiCad
instalado nesta máquina pra abrir/validar — ver kicad/README.md pro
primeiro passo de verificação real.
"""

import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent  # projeto_final/
YAML_PATH = ROOT / "pinmap.yaml"


def parse_pinmap_yaml(path):
    """Parser mínimo pro subconjunto de YAML usado em pinmap.yaml.

    Entende: `pins:` seguido de itens `- chave: valor` (um item por
    `- nome: ...`), linhas de continuação `  chave: valor` indentadas
    dentro do item, listas inline `[a, b]`, comentários `#` (fora de
    string), e strings sem aspas (valor é sempre texto ou número).
    """
    pins = []
    current = None

    for raw_line in path.read_text(encoding="utf-8").splitlines():
        line = raw_line.split(" #", 1)[0].rstrip()
        if not line.strip() or line.strip().startswith("#"):
            continue
        if line.strip() == "pins:":
            continue

        stripped = line.strip()
        if stripped.startswith("- "):
            if current is not None:
                pins.append(current)
            current = {}
            stripped = stripped[2:]

        if current is None:
            continue

        if ":" not in stripped:
            continue
        key, _, value = stripped.partition(":")
        key = key.strip()
        value = value.strip()

        if value.startswith("[") and value.endswith("]"):
            items = [v.strip() for v in value[1:-1].split(",") if v.strip()]
            current[key] = items
        elif value.isdigit():
            current[key] = int(value)
        else:
            current[key] = value

    if current is not None:
        pins.append(current)

    return pins


def gpio_port_macro(porta):
    return f"DEVICE_DT_GET(DT_NODELABEL(gpio{porta.lower()}))"


def cmsis_gpio_macro(porta):
    return f"GPIO{porta.upper()}"


def gen_pinmap_h(pins):
    lines = [
        "#ifndef PROJETO_FINAL_PINMAP_H_",
        "#define PROJETO_FINAL_PINMAP_H_",
        "",
        "/*",
        " * GERADO por tools/gen_pinmap.py a partir de pinmap.yaml — não",
        " * editar à mão. Editar pinmap.yaml e rodar o gerador de novo.",
        " *",
        " * Inclui #define de todo pino usado por Carrinho e/ou Controle;",
        " * cada projeto usa só o subconjunto que precisa (ver pinmap.yaml,",
        " * campo `boards`, e Pinmap.md pra tabela por board). Pinos",
        " * tipo=doc (SPI/UART fixos) não geram define, só documentação.",
        " */",
        "",
        "#include <device.h>",
        "",
    ]

    for pin in pins:
        nome = pin["nome"]
        tipo = pin.get("tipo", "gpio")
        porta = pin.get("porta")
        pino = pin.get("pino")
        obs = pin.get("obs", "")
        boards = "+".join(pin.get("boards", []))

        if tipo == "doc":
            continue

        lines.append(f"/* {nome} — {obs} [{boards}] */" if obs else f"/* {nome} [{boards}] */")

        if tipo == "gpio":
            lines.append(f"#define {nome}_PORT {gpio_port_macro(porta)}")
            lines.append(f"#define {nome}_PIN  {pino}   /* PT{porta}{pino} */")
        elif tipo == "pwm":
            canal = pin["canal_tpm"]
            lines.append(f"#define {nome}_GPIO {cmsis_gpio_macro(porta)}")
            lines.append(f"#define {nome}_PIN  {pino}   /* PT{porta}{pino} = TPM0_CH{canal} */")
            lines.append(f"#define {nome}_CH   {canal}")
        elif tipo == "adc":
            canal = pin["canal_adc"]
            lines.append(f"#define {nome}_CHANNEL {canal}   /* PT{porta}{pino} = ADC0_SE{canal} */")
        else:
            raise ValueError(f"tipo desconhecido: {tipo} (pino {nome})")

        lines.append("")

    lines.append("#endif /* PROJETO_FINAL_PINMAP_H_ */")
    lines.append("")
    return "\n".join(lines)


def gen_pinmap_md(pins):
    def tabela(board):
        rows = [p for p in pins if board in p.get("boards", [])]
        out = ["| Sinal | Pino | Tipo/Direção | Observação |", "|---|---|---|---|"]
        for p in rows:
            pino_fmt = f"PT{p['porta']}{p['pino']}"
            out.append(f"| {p['nome']} | {pino_fmt} | {p.get('direcao', '')} | {p.get('obs', '')} |")
        return "\n".join(out)

    return f"""# Pinmap — projeto_final (FRDM-KL25Z)

GERADO por `tools/gen_pinmap.py` a partir de `pinmap.yaml` — não
editar à mão, editar o YAML e rodar o gerador de novo. Substitui a
manutenção manual de `experiências/Exp2_PSI3422/Pinmap.md` e
`experiências/Exp4_PSI3422/pinmap.md` (quase duplicados) por uma
fonte única, que também gera `pinmap.h` e o símbolo KiCad.

## Carrinho

{tabela("carrinho")}

## Controle

{tabela("controle")}
"""


def fmt_mm(value):
    """Corta erro de ponto flutuante (ex.: 24.130000000000003) do
    S-expression gerado — KiCad grava coordenadas com poucas casas."""
    return f"{value:.2f}".rstrip("0").rstrip(".") or "0"


def kicad_pin_block(pin, y, running_number):
    nome = pin["nome"]
    porta = pin.get("porta", "")
    pino_num = pin.get("pino", "")
    numero_fisico = f"PT{porta}{pino_num}" if porta != "" else str(running_number)
    return f"""      (pin passive line (at 30 {fmt_mm(y)} 180)
        (length 5)
        (name "{nome}" (effects (font (size 1.27 1.27))))
        (number "{numero_fisico}" (effects (font (size 1.27 1.27))))
      )"""


def gen_kicad_sym(pins):
    """
    Best-effort: um símbolo único "PSI3422_shield" com um pino por
    sinal do YAML (incluindo tipo=doc, pra representar visualmente
    SPI/UART fixos também). Layout: todos os pinos numa coluna à
    direita, espaçados de 2.54mm (grid padrão do KiCad), corpo
    retangular à esquerda deles.

    Gerado por inspeção do formato de arquivo do KiCad 6/7
    (S-expression) — NÃO verificado abrindo no KiCad de verdade
    (não instalado nesta máquina). Ver kicad/README.md.
    """
    n = len(pins)
    step = 2.54
    top_y = (n - 1) / 2 * step
    body_top = top_y + step
    body_bottom = -top_y - step

    pin_blocks = []
    for i, pin in enumerate(pins):
        y = top_y - i * step
        pin_blocks.append(kicad_pin_block(pin, y, i + 1))

    pins_sexpr = "\n".join(pin_blocks)

    return f"""(kicad_symbol_lib (version 20211014) (generator gen_pinmap.py)
  (symbol "PSI3422_shield" (in_bom yes) (on_board yes)
    (property "Reference" "U" (at 0 {fmt_mm(body_top + 2.54)} 0) (effects (font (size 1.27 1.27))))
    (property "Value" "PSI3422_shield" (at 0 {fmt_mm(body_top + 5.08)} 0) (effects (font (size 1.27 1.27))))
    (property "Footprint" "" (at 0 0 0) (effects (font (size 1.27 1.27)) hide))
    (property "Datasheet" "" (at 0 0 0) (effects (font (size 1.27 1.27)) hide))
    (symbol "PSI3422_shield_0_1"
      (rectangle (start -5 {fmt_mm(body_top)}) (end 25 {fmt_mm(body_bottom)})
        (stroke (width 0.254) (type default))
        (fill (type background))
      )
    )
    (symbol "PSI3422_shield_1_1"
{pins_sexpr}
    )
  )
)
"""


def main():
    pins = parse_pinmap_yaml(YAML_PATH)
    if not pins:
        print(f"ERRO: nenhum pino lido de {YAML_PATH}", file=sys.stderr)
        return 1

    for pin in pins:
        for campo in ("nome", "porta", "pino", "tipo", "boards"):
            if campo not in pin:
                print(f"ERRO: pino sem campo obrigatório '{campo}': {pin}", file=sys.stderr)
                return 1

    (ROOT / "pinmap.h").write_text(gen_pinmap_h(pins), encoding="utf-8")
    (ROOT / "Pinmap.md").write_text(gen_pinmap_md(pins), encoding="utf-8")

    kicad_dir = ROOT / "kicad"
    kicad_dir.mkdir(exist_ok=True)
    (kicad_dir / "PSI3422_shield.kicad_sym").write_text(gen_kicad_sym(pins), encoding="utf-8")

    print(f"OK: {len(pins)} pinos -> pinmap.h, Pinmap.md, kicad/PSI3422_shield.kicad_sym")
    return 0


if __name__ == "__main__":
    sys.exit(main())
