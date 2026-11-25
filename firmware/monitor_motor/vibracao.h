// Vibração pelo acelerômetro do MPU6050.
// Indicador = desvio padrão (RMS sem a média) do módulo da aceleração numa janela.
// Tirar a média remove a gravidade e deixa só a oscilação.
#pragma once

#include <Arduino.h>

namespace vibracao {

bool iniciar();        // false se o MPU6050 não responder
void amostrar();       // chamar a cada PERIODO_VIBRACAO_MS
bool ok();
float rmsG();          // indicador da última janela completa, em g
float moduloG();       // |a| da última amostra, em g (parado deve dar ~1,0)

}  // namespace vibracao
