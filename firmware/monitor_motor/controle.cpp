#include "controle.h"

#include "config.h"
#include "corrente.h"
#include "ensaio.h"
#include "hall.h"
#include "motor.h"

namespace controle {
namespace {

Estado estadoAtual = Estado::Parado;
float ref = 0;
float integral = 0;
uint32_t inicioEstadoMs = 0, ultimoMs = 0, mudouRefMs = 0;
uint32_t semRotacaoDesde = 0, girandoDesde = 0;
uint8_t tentativas = 0;

// Ponto de partida do PWM. Linear até existir a curva RPM × duty medida com a hélice.
float feedforward(float rpm) {
  const float fracao = (rpm - RPM_REF_MIN) / (RPM_REF_MAX - RPM_REF_MIN);
  return CONTROLE_DUTY_MIN + constrain(fracao, 0.0f, 1.0f) * (CONTROLE_DUTY_MAX - CONTROLE_DUTY_MIN);
}

void darPartida(uint32_t agora) {
  estadoAtual = Estado::Partindo;
  inicioEstadoMs = agora;
  mudouRefMs = agora;
  semRotacaoDesde = 0;
  girandoDesde = 0;
  integral = 0;
  motor::definirDuty(CONTROLE_DUTY_PARTIDA);
}

void falhar() {
  motor::definirDuty(0);
  estadoAtual = Estado::Falha;
  Serial.println("CONTROLE: motor nao girou depois das tentativas de partida. Motor desligado.");
}

}  // namespace

void iniciar() {
  estadoAtual = Estado::Parado;
  ref = 0;
  integral = 0;
}

const char* definirReferencia(float rpm) {
  if (ensaio::ativo()) return "ensaio em andamento";
  if (isnan(rpm) || rpm < RPM_REF_MIN || rpm > RPM_REF_MAX) return "rpm fora da faixa";
  if (!corrente::ok()) return "sem leitura de corrente";  // sem ela não há proteção

  const uint32_t agora = millis();
  ref = rpm;
  mudouRefMs = agora;
  if (!ativo()) {
    tentativas = 0;
    darPartida(agora);
  }
  return nullptr;
}

void parar() {
  motor::definirDuty(0);
  estadoAtual = Estado::Parado;
  ref = 0;
  integral = 0;
  tentativas = 0;
}

void atualizar() {
  if (!ativo()) return;
  const uint32_t agora = millis();

  if (estadoAtual == Estado::Partindo) {
    if (agora - inicioEstadoMs < CONTROLE_PARTIDA_MS) return;
    estadoAtual = Estado::Regulando;
    inicioEstadoMs = agora;
    ultimoMs = agora;
    motor::definirDuty(feedforward(ref));
    return;
  }

  const float rpm = hall::rpm();

  // Parou ou o Hall não vê pulsos: tenta partir de novo algumas vezes e depois desiste.
  if (rpm < CONTROLE_RPM_GIRANDO) {
    girandoDesde = 0;
    if (semRotacaoDesde == 0) semRotacaoDesde = agora;
    if (agora - semRotacaoDesde >= CONTROLE_SEM_ROTACAO_MS) {
      if (++tentativas >= CONTROLE_TENTATIVAS_PARTIDA) falhar();
      else darPartida(agora);
      return;
    }
  } else {
    semRotacaoDesde = 0;
    if (girandoDesde == 0) girandoDesde = agora;
    if (agora - girandoDesde >= CONTROLE_ESTAVEL_MS) tentativas = 0;
  }

  const float dt = (agora - ultimoMs) / 1000.0f;
  ultimoMs = agora;

  const float erro = ref - rpm;
  const float semIntegral = feedforward(ref) + CONTROLE_KP * erro;
  const float novaIntegral = integral + CONTROLE_KI * erro * dt;
  const float saida = semIntegral + novaIntegral;

  // Anti-windup: não acumula erro empurrando ainda mais uma saída já saturada.
  const bool presoNoMaximo = saida > CONTROLE_DUTY_MAX && erro > 0;
  const bool presoNoMinimo = saida < CONTROLE_DUTY_MIN && erro < 0;
  if (!presoNoMaximo && !presoNoMinimo) integral = novaIntegral;

  motor::definirDuty(constrain(semIntegral + integral, CONTROLE_DUTY_MIN, CONTROLE_DUTY_MAX));
}

float referencia() { return ref; }
Estado estado() { return estadoAtual; }
bool ativo() { return estadoAtual == Estado::Partindo || estadoAtual == Estado::Regulando; }
uint32_t msDesdeMudanca() { return millis() - mudouRefMs; }

const char* nomeEstado(Estado e) {
  switch (e) {
    case Estado::Partindo:  return "partindo";
    case Estado::Regulando: return "regulando";
    case Estado::Falha:     return "falha";
    default:                return "parado";
  }
}

}  // namespace controle
