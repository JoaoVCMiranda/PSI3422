#include <Arduino.h>
#include <stdint.h>

/*
 * PSI3422 — Exp4_PSI3422 / debug / IRHW201_ESP32
 *
 * Bancada isolada do sensor IR HW-201 na ESP32 (protoboard), antes de
 * decidir a fiação definitiva no CarrinhoBase (KL25Z, ver ../../pinmap.md).
 * O objetivo aqui não é medir PPS com motor girando — isso já foi
 * validado em ../../../Exp2_PSI3422/debug/EncoderCheck. É caracterizar
 * o sensor isolado, na mão:
 *   - nível de repouso do OUT (idle) com o feixe livre;
 *   - qual borda (subida ou descida) corresponde a "feixe interrompido";
 *   - se o sinal é limpo (sem bouncing) ao interromper o feixe manualmente
 *     (cartão/dedo no gap do sensor).
 *
 * Fiação (HW-201, 3 pinos):
 *   VCC -> 3V3 da ESP32 (NÃO 5V — mantém o OUT dentro da faixa segura de
 *          3,3 V da GPIO; a ESP32 não é tolerante a 5V)
 *   GND -> GND da ESP32
 *   OUT -> GPIO4 (PINO_SENSOR abaixo)
 *
 * Ajuste o trimpot do módulo até o LED indicador do próprio HW-201 mudar
 * de estado ao interromper o feixe — se o OUT não mudar mesmo ajustando
 * o trimpot, é sinal de falha do módulo, não do código.
 */

#define PINO_SENSOR 4

volatile uint32_t contagem_subida = 0;
volatile uint32_t contagem_descida = 0;
volatile uint32_t ultima_borda_us = 0;
volatile uint32_t menor_intervalo_us = UINT32_MAX;

void IRAM_ATTR isr_sensor() {
  uint32_t agora = micros();
  uint32_t intervalo = agora - ultima_borda_us;
  ultima_borda_us = agora;

  if (intervalo < menor_intervalo_us) {
    menor_intervalo_us = intervalo;
  }

  if (digitalRead(PINO_SENSOR) == HIGH) {
    contagem_subida++;
  } else {
    contagem_descida++;
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(PINO_SENSOR, INPUT);
  attachInterrupt(digitalPinToInterrupt(PINO_SENSOR), isr_sensor, CHANGE);

  delay(200); // tempo pro Serial Monitor conectar
  Serial.println();
  Serial.println("=== IRHW201_ESP32 -- debug de bancada do sensor IR HW-201 ===");
  Serial.printf("Nivel de repouso (feixe livre) em GPIO%d: %s\n",
                PINO_SENSOR, digitalRead(PINO_SENSOR) == HIGH ? "HIGH" : "LOW");
  Serial.println("Interrompa o feixe com a mao/cartao e veja qual contagem sobe.");
  Serial.println("'intervalo_min' baixo (poucos us) e repetido = sinal de bouncing.\n");
}

void loop() {
  static uint32_t proxima_impressao = 0;
  uint32_t agora = millis();

  if (agora >= proxima_impressao) {
    proxima_impressao = agora + 1000;

    noInterrupts();
    uint32_t subidas = contagem_subida;
    uint32_t descidas = contagem_descida;
    uint32_t menor_us = menor_intervalo_us;
    contagem_subida = 0;
    contagem_descida = 0;
    menor_intervalo_us = UINT32_MAX;
    interrupts();

    Serial.printf("nivel=%-4s  subidas/s=%3u  descidas/s=%3u  intervalo_min=%lu us\n",
                  digitalRead(PINO_SENSOR) == HIGH ? "HIGH" : "LOW",
                  (unsigned)subidas, (unsigned)descidas,
                  (unsigned long)(menor_us == UINT32_MAX ? 0 : menor_us));
  }
}
