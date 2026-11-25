// Acesso ao barramento I2C compartilhado (MPU6050, OLED).
#pragma once

#include <Arduino.h>

namespace i2c {

void iniciar();
bool escrever8(uint8_t endereco, uint8_t registro, uint8_t valor);
bool escrever16(uint8_t endereco, uint8_t registro, uint16_t valor);  // MSB primeiro
bool escreverBytes(uint8_t endereco, const uint8_t* dados, uint8_t n);  // sem registro (LCD)
bool ler(uint8_t endereco, uint8_t registro, uint8_t* dados, uint8_t n);
bool ler16(uint8_t endereco, uint8_t registro, uint16_t& valor);      // MSB primeiro

// Lista no Serial os endereços que respondem (útil para confirmar o OLED).
void escanear(Print& saida);
bool responde(uint8_t endereco);

}  // namespace i2c
