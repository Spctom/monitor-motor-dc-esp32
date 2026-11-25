#include "temperatura.h"

#include "config.h"
#include "corrente.h"

namespace temperatura {
namespace {
float valor = NAN;
}

void iniciar() {
  analogReadResolution(12);
  // Atenuação 11 dB (ADC_ATENUACAO, em config.h), a mesma do ACS712: no core 3.x a calibração
  // do analogReadMilliVolts() usa sempre a atenuação GLOBAL, e analogSetPinAttenuation() antes
  // da primeira leitura é ignorada. Com o ACS712 no ADC não dá mais para usar 0 dB só para o LM35.
  // Perde-se um pouco de precisão em 30–60 °C (~300–600 mV): conferir com um termômetro.
  analogSetAttenuation(ADC_ATENUACAO);
}

void atualizar() {
  // O ADC tem um só capacitor de amostragem. Ele chega carregado com a tensão do ACS712 (~2,5 V),
  // e o LM35 quase não consegue puxar a tensão para baixo: as primeiras leituras saem altas.
  for (uint8_t i = 0; i < LM35_DESCARTES; i++) analogRead(PINO_LM35);

  uint32_t somaMv = 0;
  for (uint8_t i = 0; i < LM35_AMOSTRAS; i++) {
    somaMv += analogReadMilliVolts(PINO_LM35);  // já usa a calibração de fábrica do ADC
  }
  const float c = (float(somaMv) / LM35_AMOSTRAS) / LM35_MV_POR_C;
  valor = isnan(valor) ? c : valor + LM35_FILTRO_ALFA * (c - valor);
}

float celsius() {
  // O LM35 usa o mesmo 5 V da L298N que o ACS712: só no USB, o pino não tem medida de verdade.
  if (!LM35_INSTALADO || !corrente::ok() || isnan(valor)) return NAN;
  return valor + LM35_CORRECAO_C;
}

float paraProtecao() { return LM35_NA_PROTECAO ? celsius() : NAN; }

}  // namespace temperatura
