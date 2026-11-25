// Acionamento do motor pela ponte H L298N (sentido fixo).
// Nesta etapa é malha aberta: o duty vem de comando no Serial. O controle PI entra depois.
//
// Dois modos de PWM, trocáveis em funcionamento sem mexer na fiação:
//  - Rapido: PWM no ENA, IN1=1, IN2=0. No tempo desligado a ponte abre e a corrente do
//            motor volta para a fonte pelos diodos: cai rápido.
//  - Lento:  ENA=1, IN1=1, PWM invertido no IN2. No tempo desligado IN1=IN2=1 freia o motor
//            (terminais ligados pelos transistores de cima): a corrente cai devagar e o
//            motor costuma girar com duty menor. Com duty 0 o motor fica freado.
#pragma once

#include <Arduino.h>

namespace motor {

enum class Modo : uint8_t { Rapido, Lento };

void iniciar();                         // começa parado, modo rápido, MOTOR_PWM_FREQ_HZ
void definirDuty(float porcento);       // 0 a MOTOR_DUTY_MAX_PCT; fora da faixa é saturado
float duty();                           // duty atual em %
void definirFrequencia(long hz);        // saturada em MOTOR_PWM_FREQ_MIN_HZ..MAX_HZ
uint32_t frequencia();
void definirModo(Modo m);
Modo modo();
const char* nomeModo();

}  // namespace motor
