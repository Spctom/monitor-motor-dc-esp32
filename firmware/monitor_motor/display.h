// LCD 16x2 com módulo I2C (PCF8574), sem biblioteca.
//
// A tela alterna entre duas páginas:
//   RPM 1980/2000        I 120mA T 25.1C
//   D62% regulando  W    Vib 0.012g      W
// Com alarme, a segunda linha vira "ALARME T I V R". O "W" no canto indica Wi-Fi com internet.
//
// Controle e alarmes não podem esperar pelo display: passo() escreve no máximo um caractere por chamada.
#pragma once

#include <Arduino.h>

namespace display {

bool iniciar();   // no setup: é o único trecho que usa delay()
void passo();     // no loop, a cada PERIODO_LCD_PASSO_MS
bool ok();

}  // namespace display
