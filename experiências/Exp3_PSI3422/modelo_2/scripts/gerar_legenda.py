#!/usr/bin/env python3
"""Gera exports/legenda_cores.png a partir da paleta definida em
parts/legend.scad. Se mudar uma cor/nome lá, atualize a lista abaixo
também -- é a mesma lista, só que em Python (OpenSCAD nao tem um jeito
simples de exportar isso automaticamente)."""

from PIL import Image, ImageDraw, ImageFont
import os

ENTRIES = [
    ("BurlyWood",      "Chapa de acrílico (base)", "réplica do 7MS10"),
    ("Black",           "Rodas", ""),
    ("DimGray",         "Suporte do motor", ""),
    ("RoyalBlue",       "Placa FRDM-KL25Z", ""),
    ("Crimson",         "Ponte H (driver)", "único componente embaixo da chapa"),
    ("Purple",          "Sensor IR HW-201 (par)", "um por roda"),
    ("SeaGreen",        "Sensor ultrassônico HC-SR04", ""),
    ("Silver",          "Rodízio", ""),
    ("Orange",          "Baterias 9V", "opcional, potência extra"),
    ("SaddleBrown",     "Powerbank 5V", ""),
    ("LimeGreen",       "PCB própria (10x10cm)", ""),
    ("LightSlateGray",  "Pés impressos da PCB", ""),
]

W, H = 900, 60 + len(ENTRIES) * 70 + 40
SWATCH = 48
PAD_X = 40
LINE_H = 70

img = Image.new("RGB", (W, H), "white")
draw = ImageDraw.Draw(img)

def load_font(size, bold=False):
    candidates = [
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf" if bold else "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf" if bold else "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
    ]
    for c in candidates:
        if os.path.exists(c):
            return ImageFont.truetype(c, size)
    return ImageFont.load_default()

title_font = load_font(28, bold=True)
name_font = load_font(20, bold=True)
desc_font = load_font(16)

draw.text((PAD_X, 18), "Legenda de cores — Chassi 2WD (modelo_2)", fill="black", font=title_font)

y = 60
for color, name, desc in ENTRIES:
    draw.rounded_rectangle(
        [PAD_X, y, PAD_X + SWATCH, y + SWATCH],
        radius=8, fill=color, outline="black", width=2,
    )
    tx = PAD_X + SWATCH + 24
    draw.text((tx, y + 2), name, fill="black", font=name_font)
    if desc:
        draw.text((tx, y + 28), desc, fill="#555555", font=desc_font)
    y += LINE_H

out_path = os.path.join(os.path.dirname(__file__), "..", "exports", "legenda_cores.png")
os.makedirs(os.path.dirname(out_path), exist_ok=True)
img.save(out_path)
print(f"Legenda salva em {out_path}")
