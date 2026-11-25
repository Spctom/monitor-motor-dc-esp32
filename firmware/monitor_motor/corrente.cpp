#include "corrente.h"

#include "config.h"

// O ACS712 entrega Vcc/2 sem corrente e varia ACS712_MV_POR_A a cada ampère. Como a saída é
// proporcional a Vcc, o "zero" é medido no boot (motor parado) em vez de assumido em 2500 mV.
// O PWM faz a corrente pulsar: o filtro RC do divisor (10 kΩ // 20 kΩ + 100 nF, ~240 Hz) e a
// média de várias leituras entregam a corrente média, como a média interna do INA219 fazia.

namespace corrente {
namespace {

bool presente = false;
bool encostou = false;
float zero = NAN;  // mV na saída do módulo (antes do divisor) sem corrente
float mA = NAN;
float pino = NAN;

// Média de leituras do ADC em mV no pino (já usa a calibração de fábrica).
float lerPinoMv(uint16_t amostras) {
  uint32_t soma = 0;
  for (uint16_t i = 0; i < amostras; i++) soma += analogReadMilliVolts(PINO_ACS712);
  return float(soma) / amostras;
}

}  // namespace

bool iniciar() {
  analogReadResolution(12);
  analogSetAttenuation(ADC_ATENUACAO);  // global: vale também para o LM35
  return medirZero();
}

bool medirZero() {
  lerPinoMv(16);  // descarta as primeiras leituras, que podem sair fora
  pino = lerPinoMv(ACS712_AMOSTRAS_ZERO);
  zero = pino / ACS712_DIVISOR_FATOR;
  presente = zero >= ACS712_ZERO_MIN_MV && zero <= ACS712_ZERO_MAX_MV;
  mA = NAN;  // o filtro recomeça a partir do novo zero
  encostou = false;
  return presente;
}

void atualizar() {
  // Lê mesmo com o sensor reprovado: a tensão crua ajuda no diagnóstico e na proteção.
  pino = lerPinoMv(ACS712_AMOSTRAS);
  encostou = pino >= ADC_SATURACAO_MV;
  if (!presente) return;
  const float c = (pino / ACS712_DIVISOR_FATOR - zero) / ACS712_MV_POR_A * 1000.0f;
  mA = isnan(mA) ? c : mA + ACS712_FILTRO_ALFA * (c - mA);
}

bool ok() { return presente; }
// O módulo é tirado só aqui, depois da média e do filtro: assim o ruído em volta do zero se
// cancela antes. Tirar o módulo de cada leitura viraria o ruído numa corrente positiva falsa.
float miliamperes() { return fabsf(mA); }
float miliamperesComSinal() { return mA; }
float zeroMv() { return zero; }
float pinoMv() { return pino; }
bool saturado() { return encostou; }

}  // namespace corrente
