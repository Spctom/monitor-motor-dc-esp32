// Rotação pelo sensor Hall 3144E (2 ímãs na hélice).
// Mede o período entre pulsos por interrupção, o que dá boa resolução mesmo em baixa rotação.
#pragma once

#include <Arduino.h>

namespace hall {

void iniciar();
void atualizar();          // chamar a cada PERIODO_RPM_MS
float rpm();
uint32_t pulsosTotais();   // desde o boot; útil para conferir se os dois ímãs são detectados

}  // namespace hall
