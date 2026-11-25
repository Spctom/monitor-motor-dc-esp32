#include "corrente.h"

#include "barramento_i2c.h"
#include "config.h"

// Registradores do INA219 acessados direto (sem biblioteca) para poder ligar a média
// interna de 128 amostras: com o PWM a corrente é pulsada, e uma conversão única
// pegaria um ponto qualquer da onda. Com a média, a leitura é a corrente média real.

namespace corrente {
namespace {

constexpr uint8_t REG_CONFIG = 0x00;
constexpr uint8_t REG_SHUNT  = 0x01;
constexpr uint8_t REG_BUS    = 0x02;

// BRNG=32 V | PGA=/8 (±320 mV) | BADC=32 amostras (17 ms) | SADC=128 amostras (68 ms) | contínuo
// Ciclo completo ≈ 85 ms, abaixo do período de leitura de 100 ms.
constexpr uint16_t CONFIG = 0x2000 | 0x1800 | 0x0680 | 0x0078 | 0x0007;  // = 0x3EFF

bool presente = false;
bool overflow = false;
float mA = NAN, V = NAN;

}  // namespace

bool iniciar() {
  presente = i2c::escrever16(ENDERECO_INA219, REG_CONFIG, CONFIG);
  uint16_t lido = 0;
  presente = presente && i2c::ler16(ENDERECO_INA219, REG_CONFIG, lido) && lido == CONFIG;
  return presente;
}

void atualizar() {
  uint16_t shunt, bus;
  if (!i2c::ler16(ENDERECO_INA219, REG_SHUNT, shunt) || !i2c::ler16(ENDERECO_INA219, REG_BUS, bus)) {
    presente = false;
    return;
  }
  presente = true;

  const float shuntMv = int16_t(shunt) * 0.01f;  // LSB = 10 µV
  mA = shuntMv / INA219_SHUNT_OHMS;
  V = (bus >> 3) * 0.004f;                       // LSB = 4 mV
  overflow = bus & 0x01;
}

bool ok() { return presente; }
float miliamperes() { return mA; }
float volts() { return V; }
float watts() { return V * mA / 1000.0f; }
bool saturado() { return overflow; }

}  // namespace corrente
