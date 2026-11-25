// Corrente, tensão e potência pelo INA219 (lado alto, entre a fonte 9 V e a L298N).
#pragma once

#include <Arduino.h>

namespace corrente {

bool iniciar();       // false se o INA219 não responder
void atualizar();     // chamar a cada PERIODO_CORRENTE_MS
bool ok();
float miliamperes();
float volts();        // tensão na entrada da ponte H
float watts();
bool saturado();      // corrente acima da faixa do INA219 (~3,2 A com shunt 0,1 Ω)

}  // namespace corrente
