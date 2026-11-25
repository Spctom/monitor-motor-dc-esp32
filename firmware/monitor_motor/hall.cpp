#include "hall.h"

#include "config.h"

namespace hall {
namespace {

portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

// Compartilhadas com a interrupção: só acessar dentro de portENTER/EXIT_CRITICAL.
uint32_t ultimoPulsoUs = 0;
uint32_t somaPeriodosUs = 0;
uint32_t numPeriodos = 0;
uint32_t totalPulsos = 0;

float rpmAtual = 0;

void IRAM_ATTR aoPulso() {
  const uint32_t agora = micros();

  portENTER_CRITICAL_ISR(&mux);
  const uint32_t periodo = agora - ultimoPulsoUs;
  if (periodo >= HALL_PERIODO_MIN_US) {  // abaixo disso é ruído do motor/PWM
    // Período maior que o timeout é o primeiro pulso depois de parado: não entra na média.
    if (ultimoPulsoUs != 0 && periodo < HALL_TIMEOUT_MS * 1000UL) {
      somaPeriodosUs += periodo;
      numPeriodos++;
    }
    ultimoPulsoUs = agora;
    totalPulsos++;
  }
  portEXIT_CRITICAL_ISR(&mux);
}

}  // namespace

void iniciar() {
  // Pull-up interno como reserva; o externo de 10 kΩ para 3,3 V continua recomendado.
  pinMode(PINO_HALL, INPUT_PULLUP);
  // 3144E vai a nível baixo quando o polo sul do ímã passa.
  attachInterrupt(digitalPinToInterrupt(PINO_HALL), aoPulso, FALLING);
}

void atualizar() {
  portENTER_CRITICAL(&mux);
  const uint32_t soma = somaPeriodosUs;
  const uint32_t n = numPeriodos;
  const uint32_t ultimo = ultimoPulsoUs;
  somaPeriodosUs = 0;
  numPeriodos = 0;
  portEXIT_CRITICAL(&mux);

  if (n > 0) {
    const float periodoMedioUs = float(soma) / n;
    rpmAtual = 60e6f / (periodoMedioUs * HALL_PULSOS_POR_VOLTA);
  } else if (micros() - ultimo > HALL_TIMEOUT_MS * 1000UL) {
    rpmAtual = 0;
  }
  // n == 0 dentro do timeout: rotação baixa, nenhum pulso nesta janela; mantém o último valor.
}

float rpm() { return rpmAtual; }

uint32_t pulsosTotais() {
  portENTER_CRITICAL(&mux);
  const uint32_t total = totalPulsos;
  portEXIT_CRITICAL(&mux);
  return total;
}

}  // namespace hall
