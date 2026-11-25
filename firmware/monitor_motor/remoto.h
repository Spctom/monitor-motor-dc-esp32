// Rede: Wi-Fi, servidor do app e Thinger.io numa tarefa própria no core 0.
// Se o Wi-Fi cair ou demorar, só esta tarefa espera: aquisição, controle e alarmes seguem no core 1.
#pragma once

#include <Arduino.h>

namespace remoto {

void iniciar();     // cria a tarefa; sem nenhuma rede no secrets.h, o sistema roda só local
bool conectado();
const char* thinger();  // estado do Thinger.io, em texto (para o /status)
const char* dns();      // resultado do último teste de DNS

}  // namespace remoto
