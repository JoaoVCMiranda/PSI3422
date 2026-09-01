# debug/ — validações de bancada, fora da entrega

Firmwares isolados pra validar hardware sem depender do resto do
Exp4_PSI3422 — se algo não bate aqui, o problema é do sensor/fiação,
não da lib compartilhada (`../../../lib/encoder`,
`../../../lib/odometria`).

## IRHW201_ESP32

Flasha numa ESP32 avulsa (`upesy_wroom`) numa protoboard — não no
board do CarrinhoBase (KL25Z). Serial monitor: **115200 8N1**
(`monitor_speed` já no `platformio.ini`).

Objetivo não é medir PPS com motor girando (isso já foi validado em
`../../Exp2_PSI3422/debug/EncoderCheck`); é caracterizar o sensor
HW-201 isolado, na mão, antes de confiar no `GPIO_INT_EDGE_RISING`
assumido em `../../../lib/encoder/encoder.h`:

- nível de repouso do `OUT` com o feixe livre;
- qual borda (subida ou descida) corresponde a "feixe interrompido";
- se o sinal é limpo (sem bouncing) ao interromper o feixe manualmente
  (cartão/dedo no gap do sensor).

Fiação (HW-201, 3 pinos):

| HW-201 | ESP32 |
|---|---|
| VCC | 3V3 (**não** 5V — GPIO da ESP32 não é tolerante a 5V) |
| GND | GND |
| OUT | GPIO4 |

Rodar: `pio run -t upload -t monitor` dentro de `IRHW201_ESP32/`.

**Resultado confirmado em bancada** (protoboard, ESP32 + HW-201,
2026-09-01): `HIGH` = feixe livre, `LOW` = feixe interrompido. A
transição que marca "acabou de bloquear" é `FALLING`; a marca
"acabou de liberar" é `RISING`.

Saída de exemplo (feixe livre, depois interrompido à mão):

```
=== IRHW201_ESP32 -- debug de bancada do sensor IR HW-201 ===
Nivel de repouso (feixe livre) em GPIO4: HIGH
Interrompa o feixe com a mao/cartao e veja qual contagem sobe.
'intervalo_min' baixo (poucos us) e repetido = sinal de bouncing.

nivel=HIGH  subidas/s=  0  descidas/s=  0  intervalo_min=0 us
nivel=HIGH  subidas/s=  3  descidas/s=  3  intervalo_min=41207 us
nivel=HIGH  subidas/s=  0  descidas/s=  0  intervalo_min=0 us
```

Como ler:
- `subidas/s` e `descidas/s` sempre variam juntos (cada interrupção
  do feixe gera uma borda de cada tipo) — cada bloqueio soma 1 em
  ambos, então a contagem por segundo em si não é afetada pela
  polaridade.
- `intervalo_min` na casa de poucas centenas de µs, repetindo a cada
  interrupção manual do feixe, indicaria bouncing — não observado
  nesta bancada.
- Se `subidas/s`/`descidas/s` ficarem em 0 mesmo interrompendo o
  feixe, ajuste o trimpot do módulo até o LED dele reagir; se nada
  mudar, é falha do módulo, não do firmware.

**Sobre `lib/encoder/encoder.h`:** o código lá usa
`GPIO_INT_EDGE_RISING` — como cada bloqueio do feixe gera exatamente
uma borda de subida e uma de descida (mesma contagem por revolução
nos dois casos), a polaridade *não* muda quantos pulsos são contados,
só a fase (se o pulso é contado no instante em que bloqueia ou no
instante em que libera). Não é necessário alterar o código por causa
deste resultado — troca só faria sentido se algum dia a lib precisar
disparar exatamente no instante do bloqueio (ex.: medir duração do
bloqueio), o que a Aula 4 não pede.
