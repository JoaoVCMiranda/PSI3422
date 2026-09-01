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

Formato de saída esperado (a cada 1s), ainda **não** capturado numa
bancada de verdade — ilustrativo, pra saber o que procurar:

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
  do feixe gera uma borda de cada tipo) — o que importa é **qual
  nível o sensor assume enquanto o feixe está bloqueado**: se ele cai
  pra `LOW` ao bloquear, a borda de bloqueio é `FALLING`, o oposto do
  que `lib/encoder` assume hoje.
- `intervalo_min` na casa de poucas centenas de µs, repetindo a cada
  interrupção manual do feixe, indica bouncing — nesse caso considerar
  debounce por software antes de portar a lógica pro `lib/encoder`.
- Se `subidas/s`/`descidas/s` ficarem em 0 mesmo interrompendo o
  feixe, ajuste o trimpot do módulo até o LED dele reagir; se nada
  mudar, é falha do módulo, não do firmware.

Se a bancada confirmar que a polaridade é a oposta da assumida em
`lib/encoder/encoder.h`, atualizar `GPIO_INT_EDGE_RISING` (ou
inverter a leitura) lá antes de instalar os encoders de verdade no
CarrinhoBase.
