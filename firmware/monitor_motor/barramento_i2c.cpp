#include "barramento_i2c.h"

#include <Wire.h>

#include "config.h"

extern bool loopTaskWDTEnabled;  // definido no core: só alimenta o watchdog depois de ele ser ligado

namespace i2c {

void iniciar() {
  Wire.begin(PINO_I2C_SDA, PINO_I2C_SCL, I2C_FREQ_HZ);
}

bool escrever8(uint8_t endereco, uint8_t registro, uint8_t valor) {
  Wire.beginTransmission(endereco);
  Wire.write(registro);
  Wire.write(valor);
  return Wire.endTransmission() == 0;
}

bool escrever16(uint8_t endereco, uint8_t registro, uint16_t valor) {
  Wire.beginTransmission(endereco);
  Wire.write(registro);
  Wire.write(uint8_t(valor >> 8));
  Wire.write(uint8_t(valor & 0xFF));
  return Wire.endTransmission() == 0;
}

bool escreverBytes(uint8_t endereco, const uint8_t* dados, uint8_t n) {
  Wire.beginTransmission(endereco);
  Wire.write(dados, n);
  return Wire.endTransmission() == 0;
}

bool ler(uint8_t endereco, uint8_t registro, uint8_t* dados, uint8_t n) {
  Wire.beginTransmission(endereco);
  Wire.write(registro);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(endereco, n) != n) return false;
  for (uint8_t i = 0; i < n; i++) dados[i] = Wire.read();
  return true;
}

bool ler16(uint8_t endereco, uint8_t registro, uint16_t& valor) {
  uint8_t b[2];
  if (!ler(endereco, registro, b, 2)) return false;
  valor = (uint16_t(b[0]) << 8) | b[1];
  return true;
}

bool responde(uint8_t endereco) {
  Wire.beginTransmission(endereco);
  return Wire.endTransmission() == 0;
}

void escanear(Print& saida) {
  saida.print("I2C encontrados:");
  uint8_t total = 0;
  for (uint8_t end = 1; end < 127; end++) {
    if (loopTaskWDTEnabled) feedLoopWDT();  // com o barramento travado, cada endereço pode demorar
    Wire.beginTransmission(end);
    if (Wire.endTransmission() == 0) {
      saida.printf(" 0x%02X", end);
      total++;
    }
  }
  if (total == 0) saida.print(" nenhum (verificar SDA/SCL/alimentação)");
  saida.println();
}

}  // namespace i2c
