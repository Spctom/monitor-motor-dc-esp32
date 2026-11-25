// Temperatura da carcaça pelo LM35 (10 mV/°C).
#pragma once

#include <Arduino.h>

namespace temperatura {

void iniciar();
void atualizar();   // chamar a cada PERIODO_TEMPERATURA_MS
float celsius();    // com a correção de config.h; NAN sem leitura ou sem a fonte de 12 V
float paraProtecao();  // NAN enquanto o sensor não for confiável (LM35_NA_PROTECAO = false)

}  // namespace temperatura
