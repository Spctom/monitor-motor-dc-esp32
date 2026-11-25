#include "motor.h"

#include "config.h"

namespace motor {
namespace {

constexpr uint32_t DUTY_MAX = (1u << MOTOR_PWM_BITS) - 1;
#if ESP_ARDUINO_VERSION_MAJOR < 3
constexpr uint8_t CANAL = 0;
#endif

float dutyAtual = 0;
uint32_t freqHz = MOTOR_PWM_FREQ_HZ;
Modo modoAtual = Modo::Rapido;

uint8_t pinoPwm() { return modoAtual == Modo::Rapido ? PINO_MOTOR_PWM : PINO_MOTOR_IN2; }

void saidaFixa(uint8_t pino, uint8_t nivel) {
  pinMode(pino, OUTPUT);
  digitalWrite(pino, nivel);
}

void ligarPwm(uint8_t pino) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(pino, freqHz, MOTOR_PWM_BITS);
#else
  ledcSetup(CANAL, freqHz, MOTOR_PWM_BITS);
  ledcAttachPin(pino, CANAL);
#endif
}

void desligarPwm(uint8_t pino) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcDetach(pino);
#else
  ledcDetachPin(pino);
#endif
}

void escreverPwm(uint32_t valor) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(pinoPwm(), valor);
#else
  ledcWrite(CANAL, valor);
#endif
}

// Prepara os três pinos para o modo e a frequência atuais.
void configurar() {
  saidaFixa(PINO_MOTOR_IN1, HIGH);
  if (modoAtual == Modo::Rapido) {
    saidaFixa(PINO_MOTOR_IN2, LOW);
    ligarPwm(PINO_MOTOR_PWM);
  } else {
    saidaFixa(PINO_MOTOR_PWM, HIGH);
    ligarPwm(PINO_MOTOR_IN2);
  }
}

// Troca modo e/ou frequência com o motor em funcionamento, mantendo o duty.
void reconfigurar(Modo novoModo, uint32_t novaFreq) {
  desligarPwm(pinoPwm());
  modoAtual = novoModo;
  freqHz = novaFreq;
  configurar();
  definirDuty(dutyAtual);
}

}  // namespace

void iniciar() {
  configurar();
  definirDuty(0);
}

void definirDuty(float porcento) {
  dutyAtual = constrain(porcento, 0.0f, MOTOR_DUTY_MAX_PCT);  // teto único para tudo
  const uint32_t valor = lroundf(dutyAtual / 100.0f * DUTY_MAX);
  // No modo lento o IN2 é ativo em nível baixo: IN2=0 dá tração, IN2=1 freia.
  escreverPwm(modoAtual == Modo::Rapido ? valor : DUTY_MAX - valor);
}

float duty() { return dutyAtual; }

void definirFrequencia(long hz) {
  const long limitada = constrain(hz, long(MOTOR_PWM_FREQ_MIN_HZ), long(MOTOR_PWM_FREQ_MAX_HZ));
  reconfigurar(modoAtual, uint32_t(limitada));
}

uint32_t frequencia() { return freqHz; }

void definirModo(Modo m) {
  if (m != modoAtual) reconfigurar(m, freqHz);
}

Modo modo() { return modoAtual; }

const char* nomeModo() { return modoAtual == Modo::Rapido ? "rapido (PWM no ENA)" : "lento (PWM no IN2)"; }

}  // namespace motor
