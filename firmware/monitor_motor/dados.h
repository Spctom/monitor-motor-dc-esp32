// Última medida, compartilhada entre o loop (core 1) e a tarefa de rede (core 0).
// Os dois lados só trocam cópias: a rede nunca segura o controle nem os alarmes.
#pragma once

#include <Arduino.h>

namespace dados {

struct Amostra {
  float rpm = NAN;
  float rpmRef = 0;
  float temperatura = NAN;
  float corrente = NAN;
  float vibracao = NAN;
  float duty = 0;
  uint8_t alarmes = 0;   // bits de alarmes::ALARME_*
  uint8_t simulados = 0; // bits de alarmes que são simulação (painel web)
  uint8_t leds = 0;      // nível nos pinos dos LEDs: 1 verde, 2 amarelo, 4 vermelho
  uint8_t estado = 0;    // controle::Estado
  bool ensaio = false;
};

void publicar(const Amostra& a);   // chamada pelo loop
Amostra atual();
// Médias desde a chamada anterior (para o Data Bucket); alarmes e estado são os do instante.
Amostra mediaDoPeriodo();

}  // namespace dados
