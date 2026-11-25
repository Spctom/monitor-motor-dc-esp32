// Corrente do motor pelo ACS712 (efeito Hall, saída analógica ≈ Vcc/2 sem corrente).
// Fica em série no lado alto, entre a fonte e a L298N. Mede só corrente: não há tensão nem potência.
#pragma once

#include <Arduino.h>

namespace corrente {

bool iniciar();       // configura o ADC e mede o zero: chamar com o motor PARADO
bool medirZero();     // refaz o zero (motor PARADO). false se o valor for implausível
void atualizar();     // chamar a cada PERIODO_CORRENTE_MS
bool ok();
float miliamperes();  // sempre positiva (módulo): não depende do sentido de IP+/IP− (decisão de 24/11)
float miliamperesComSinal();  // com sinal, só para diagnóstico: negativa = IP+/IP− invertidos
float zeroMv();       // saída do módulo sem corrente, medida no boot (esperado ≈ 2500 mV com Vcc = 5 V)
float pinoMv();       // tensão crua no GPIO, mesmo com o sensor reprovado (para diagnóstico)
bool saturado();      // ADC no fim da faixa: corrente fora do que a medida consegue ler

}  // namespace corrente
