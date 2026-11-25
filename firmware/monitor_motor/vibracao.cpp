#include "vibracao.h"

#include "barramento_i2c.h"
#include "config.h"

namespace vibracao {
namespace {

constexpr uint8_t REG_SMPLRT_DIV   = 0x19;
constexpr uint8_t REG_CONFIG       = 0x1A;
constexpr uint8_t REG_ACCEL_CONFIG = 0x1C;
constexpr uint8_t REG_ACCEL_XOUT_H = 0x3B;
constexpr uint8_t REG_PWR_MGMT_1   = 0x6B;
constexpr uint8_t REG_WHO_AM_I     = 0x75;

constexpr float LSB_POR_G = 8192.0f;  // escala ±4 g

bool configurado = false;  // se falhou no boot o chip fica em sleep e lê zeros
bool presente = false;
float ultimoModulo = NAN;
float rmsAtual = NAN;

// Acumuladores da janela (double: a variância é a diferença de dois números ~1,0)
double soma = 0, somaQuadrados = 0;
uint32_t n = 0;
uint32_t inicioJanelaMs = 0;

// Com mau contato o módulo perde energia e volta dormindo, lendo exatamente zero nos 3 eixos.
constexpr uint16_t ZERADAS_PARA_ACORDAR = 25;   // ~100 ms de leituras zeradas
constexpr uint32_t INTERVALO_ACORDAR_MS = 1000;
uint16_t leiturasZeradas = 0;
uint32_t ultimoAcordarMs = 0;

bool configurar() {
  return i2c::escrever8(ENDERECO_MPU6050, REG_PWR_MGMT_1, 0x01)     // acorda, clock do giroscópio X
      && i2c::escrever8(ENDERECO_MPU6050, REG_CONFIG, 0x02)         // filtro passa-baixa ~94 Hz
      && i2c::escrever8(ENDERECO_MPU6050, REG_SMPLRT_DIV, 3)        // 1 kHz / (1+3) = 250 Hz
      && i2c::escrever8(ENDERECO_MPU6050, REG_ACCEL_CONFIG, 0x08);  // ±4 g
}

}  // namespace

bool iniciar() {
  uint8_t quemSou = 0;
  if (!i2c::ler(ENDERECO_MPU6050, REG_WHO_AM_I, &quemSou, 1)) return presente = false;
  // Original responde 0x68; clones às vezes 0x70/0x72/0x98. Só avisa, não bloqueia.
  if (quemSou != 0x68) Serial.printf("MPU6050: WHO_AM_I=0x%02X (clone?)\n", quemSou);

  presente = configurar();
  configurado = presente;
  inicioJanelaMs = millis();
  return presente;
}

void amostrar() {
  if (!configurado) return;
  uint8_t b[6];
  if (!i2c::ler(ENDERECO_MPU6050, REG_ACCEL_XOUT_H, b, 6)) {
    presente = false;
    return;
  }
  presente = true;

  if ((b[0] | b[1] | b[2] | b[3] | b[4] | b[5]) == 0) {
    // Zero exato não é medida: não entra na vibração, para não parecer "motor sem vibrar".
    ultimoModulo = 0;
    rmsAtual = NAN;
    soma = somaQuadrados = 0;
    n = 0;
    inicioJanelaMs = millis();
    if (++leiturasZeradas >= ZERADAS_PARA_ACORDAR && millis() - ultimoAcordarMs >= INTERVALO_ACORDAR_MS) {
      ultimoAcordarMs = millis();
      leiturasZeradas = 0;
      configurar();
    }
    return;
  }
  leiturasZeradas = 0;

  const float ax = int16_t((b[0] << 8) | b[1]) / LSB_POR_G;
  const float ay = int16_t((b[2] << 8) | b[3]) / LSB_POR_G;
  const float az = int16_t((b[4] << 8) | b[5]) / LSB_POR_G;
  ultimoModulo = sqrtf(ax * ax + ay * ay + az * az);

  soma += ultimoModulo;
  somaQuadrados += double(ultimoModulo) * ultimoModulo;
  n++;

  const uint32_t agora = millis();
  if (agora - inicioJanelaMs >= VIBRACAO_JANELA_MS && n > 1) {
    const double media = soma / n;
    const double variancia = somaQuadrados / n - media * media;
    rmsAtual = variancia > 0 ? float(sqrt(variancia)) : 0.0f;
    soma = somaQuadrados = 0;
    n = 0;
    inicioJanelaMs = agora;
  }
}

bool ok() { return presente; }
float rmsG() { return rmsAtual; }
float moduloG() { return ultimoModulo; }

}  // namespace vibracao
