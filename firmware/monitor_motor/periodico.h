// Executa uma tarefa a cada N ms sem bloquear o loop (substitui delay()).
#pragma once

#include <Arduino.h>

struct Periodico {
  uint32_t periodoMs;
  uint32_t ultimo = 0;

  bool pronto(uint32_t agora) {
    if (agora - ultimo < periodoMs) return false;
    ultimo = agora;
    return true;
  }
};
