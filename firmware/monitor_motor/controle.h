// Controle de rotação em malha fechada: PI com feedforward, anti-windup e pulso de partida.
//
// O motor não sai da inércia com duty baixo (só zumbe). Por isso a partida aplica um pulso
// curto de duty alto e só depois entrega para o PI, que trabalha entre o duty mínimo em que
// o motor continua girando e o máximo seguro.
#pragma once

#include <Arduino.h>

namespace controle {

enum class Estado : uint8_t { Parado, Partindo, Regulando, Falha };

void iniciar();
void atualizar();                        // chamar a cada PERIODO_CONTROLE_MS

// Define o RPM desejado e dá a partida se preciso. Devolve nullptr se aceitou,
// ou o motivo da recusa (fora da faixa, ensaio rodando, sem leitura de corrente).
const char* definirReferencia(float rpm);
void parar();

float referencia();
Estado estado();
bool ativo();                            // partindo ou regulando
uint32_t msDesdeMudanca();               // tempo desde a última mudança de referência ou partida
const char* nomeEstado(Estado e);

}  // namespace controle
