#include "alarmes.h"

#include "config.h"
#include "controle.h"
#include "corrente.h"
#include "ensaio.h"
#include "hall.h"
#include "motor.h"
#include "temperatura.h"
#include "vibracao.h"

namespace alarmes {
namespace {

struct Saida {
  uint8_t pino;
  const char* nome;
};

const Saida SAIDAS[] = {
  {PINO_LED_VERDE,    "LED verde"},
  {PINO_LED_AMARELO,  "LED amarelo"},
  {PINO_LED_VERMELHO, "LED vermelho"},
  {PINO_BUZZER,       "buzzer (ativo: apita; passivo: so um clique)"},
};
constexpr int NUM_SAIDAS = sizeof(SAIDAS) / sizeof(SAIDAS[0]);
constexpr uint32_t DURACAO_PASSO_MS = 1000;

bool bloqueada[NUM_SAIDAS] = {};  // pino puxado para o GND: acionar só forçaria a saída do ESP32

bool desativada(int i) { return SAIDAS[i].pino == PINO_BUZZER && !BUZZER_INSTALADO; }

void escrever(int i, bool ligar) {
  if (desativada(i)) ligar = false;  // buzzer fora: pino sempre baixo
  if (!bloqueada[i]) digitalWrite(SAIDAS[i].pino, ligar ? HIGH : LOW);
}

int passoTeste = -1;  // -1 = sem teste rodando
uint32_t inicioPassoMs = 0;
bool mantendo = false;
uint32_t inicioManterMs = 0;
constexpr uint32_t MANTER_MAX_MS = 300000;

void desligarTudo() {
  for (int i = 0; i < NUM_SAIDAS; i++) escrever(i, false);
}

// Um alarme com histerese e tempo de persistência.
struct Regra {
  bool ativo = false;
  uint32_t desde = 0;

  void avaliar(bool acima, bool limpou, uint32_t persistenciaMs, uint32_t agora) {
    const bool mudando = ativo ? limpou : acima;
    if (!mudando) {
      desde = 0;
      return;
    }
    if (desde == 0) desde = agora;
    if (agora - desde >= (ativo ? ALARME_LIMPAR_MS : persistenciaMs)) {
      ativo = !ativo;
      desde = 0;
    }
  }
};

Regra regraTemperatura, regraCorrente, regraVibracao, regraRotacao;
uint8_t simBits = 0;       // falhas simuladas pelo painel (bits ALARME_*)
uint32_t simDesde = 0;
uint8_t paradaPor = 0;  // alarmes que desligaram o motor; ficam até a próxima referência ou o comando s

uint8_t bitsAtivos() {
  return (regraTemperatura.ativo ? ALARME_TEMPERATURA : 0) | (regraCorrente.ativo ? ALARME_CORRENTE : 0) |
         (regraVibracao.ativo ? ALARME_VIBRACAO : 0) | (regraRotacao.ativo ? ALARME_ROTACAO : 0);
}

void indicar(uint32_t agora) {
  const uint8_t bits = bitsAtivos() | paradaPor;
  const bool critico = bits & (ALARME_TEMPERATURA | ALARME_CORRENTE | ALARME_VIBRACAO);
  const bool rotacao = bits & ALARME_ROTACAO;
  definir(!critico && !rotacao, rotacao && !critico && (agora % 1000) < 500, critico,
          (critico || rotacao) && (agora % 1000) < 200);
}

void ligarSo(int i) {
  desligarTudo();
  escrever(i, true);
  Serial.printf("teste: %s (GPIO %u)%s\n", SAIDAS[i].nome, SAIDAS[i].pino,
                desativada(i) ? ": DESATIVADO em config.h (BUZZER_INSTALADO = false)" : "");
}

}  // namespace

void iniciar() {
  for (const Saida& s : SAIDAS) {
    pinMode(s.pino, OUTPUT);
    digitalWrite(s.pino, LOW);
  }
  // Liga cada saída por um instante: se o pino continuar em nível baixo, há curto com o GND.
  for (int i = 0; i < NUM_SAIDAS; i++) {
    if (desativada(i)) continue;  // nem o pulso de teste: fica em nível baixo
    if (SAIDAS[i].pino != PINO_BUZZER && !LEDS_COM_RESISTOR) {
      bloqueada[i] = true;  // sem resistor: nem o teste rápido pode ligar
      pinMode(SAIDAS[i].pino, INPUT);  // pino solto: não fornece nem puxa corrente
      continue;
    }
    digitalWrite(SAIDAS[i].pino, HIGH);
    delayMicroseconds(100);
    bloqueada[i] = digitalRead(SAIDAS[i].pino) == LOW;
    digitalWrite(SAIDAS[i].pino, LOW);
    if (bloqueada[i]) pinMode(SAIDAS[i].pino, INPUT);
  }
}

void relatarBloqueios() {
  for (int i = 0; i < NUM_SAIDAS; i++) {
    if (!bloqueada[i]) continue;
    const bool semResistor = SAIDAS[i].pino != PINO_BUZZER && !LEDS_COM_RESISTOR;
    Serial.printf("ATENCAO: GPIO %u (%s) %s: saida bloqueada\n", SAIDAS[i].pino, SAIDAS[i].nome,
                  semResistor ? "sem resistor" : "em curto com o GND");
  }
}

uint8_t bloqueios() {
  uint8_t bits = 0;
  for (int i = 0; i < NUM_SAIDAS; i++)
    if (bloqueada[i]) bits |= 1 << i;
  return bits;
}

void iniciarTeste() {
  mantendo = false;
  passoTeste = 0;
  inicioPassoMs = millis();
  ligarSo(passoTeste);
}

void atualizar() {
  const uint32_t agora = millis();
  if (mantendo && agora - inicioManterMs >= MANTER_MAX_MS) manterTudo(false);
  if (passoTeste < 0) return;
  if (agora - inicioPassoMs < DURACAO_PASSO_MS) return;

  inicioPassoMs = agora;
  passoTeste++;
  if (passoTeste < NUM_SAIDAS) {
    ligarSo(passoTeste);
  } else {
    desligarTudo();
    passoTeste = -1;
    Serial.println("teste: fim, tudo desligado");
  }
}

bool emTeste() { return passoTeste >= 0 || mantendo; }

void manterTudo(bool ligar) {
  passoTeste = -1;
  mantendo = ligar;
  inicioManterMs = millis();
  for (int i = 0; i < NUM_SAIDAS; i++) escrever(i, ligar);
  delayMicroseconds(100);
  for (int i = 0; i < NUM_SAIDAS; i++) {
    Serial.printf("GPIO %u: %s, lido %s\n", SAIDAS[i].pino,
                  bloqueada[i] ? "BLOQUEADO" : (ligar ? "comandado ALTO" : "comandado BAIXO"),
                  digitalRead(SAIDAS[i].pino) ? "ALTO" : "BAIXO");
  }
}

void avaliar() {
  // O ensaio tem as próprias proteções e usa os LEDs para mostrar o andamento.
  if (ensaio::ativo()) {
    regraTemperatura = Regra();
    regraCorrente = Regra();
    regraVibracao = Regra();
    regraRotacao = Regra();
    paradaPor = 0;
    return;
  }

  const uint32_t agora = millis();
  if (simBits && agora - simDesde >= SIMULACAO_MAX_MS) {
    simBits = 0;
    Serial.println("SIMULACAO: encerrada (tempo maximo)");
  }
  // Falha simulada entra como se a medida tivesse passado do limite (e some ao limpar).
  const bool simTemp = simBits & ALARME_TEMPERATURA, simCor = simBits & ALARME_CORRENTE,
             simVib = simBits & ALARME_VIBRACAO, simRot = simBits & ALARME_ROTACAO;

  const float temp = temperatura::paraProtecao();
  regraTemperatura.avaliar(simTemp || (!isnan(temp) && temp > LIMITE_TEMPERATURA_C),
                           !simTemp && (isnan(temp) || temp < LIMITE_TEMPERATURA_C * ALARME_HISTERESE_TEMPERATURA),
                           ALARME_PERSISTENCIA_MS, agora);

  const float mA = corrente::ok() ? fabsf(corrente::miliamperes()) : NAN;
  const bool saturado = corrente::saturado();
  regraCorrente.avaliar(simCor || saturado || (!isnan(mA) && mA > LIMITE_CORRENTE_MA),
                        !simCor && !saturado && (isnan(mA) || mA < LIMITE_CORRENTE_MA * ALARME_HISTERESE_CORRENTE),
                        ALARME_PERSISTENCIA_MS, agora);

  const float vib = vibracao::ok() ? vibracao::rmsG() : NAN;
  regraVibracao.avaliar(simVib || (!isnan(vib) && vib > LIMITE_VIBRACAO_G),
                        !simVib && (isnan(vib) || vib < LIMITE_VIBRACAO_G * ALARME_HISTERESE_VIBRACAO),
                        ALARME_PERSISTENCIA_VIB_MS, agora);

  // Rotação: só compara depois do transitório de partida ou de mudança de referência.
  const bool regulando = controle::estado() == controle::Estado::Regulando &&
                         controle::msDesdeMudanca() >= ALARME_ROTACAO_TRANSITORIO_MS;
  const float ref = controle::referencia();
  const float erroPct = regulando && ref > 0 ? fabsf(ref - hall::rpm()) / ref * 100.0f : 0.0f;
  regraRotacao.avaliar(simRot || (regulando && erroPct > ALARME_ROTACAO_ERRO_PCT),
                       !simRot && (!regulando || erroPct < ALARME_ROTACAO_ERRO_PCT * 0.7f),
                       ALARME_ROTACAO_PERSISTENCIA_MS, agora);
  if (controle::estado() == controle::Estado::Falha) paradaPor |= ALARME_ROTACAO;

  const uint8_t criticos = bitsAtivos() & (ALARME_TEMPERATURA | ALARME_CORRENTE | ALARME_VIBRACAO);
  if (criticos && (controle::ativo() || motor::duty() > 0)) {
    controle::parar();
    motor::definirDuty(0);
    paradaPor |= criticos;
    Serial.printf("ALARME%s: motor desligado (bits %u: 1=temperatura, 2=corrente, 4=vibracao)\n",
                  (criticos & simBits) ? " SIMULADO" : "", criticos);
  }

  if (comandaSaidas() && !emTeste()) indicar(agora);
}

uint8_t ativos() { return bitsAtivos() | paradaPor; }

void simular(uint8_t bits) {
  if (ensaio::ativo()) return;
  simBits = bits & (ALARME_TEMPERATURA | ALARME_CORRENTE | ALARME_VIBRACAO | ALARME_ROTACAO);
  simDesde = millis();
  Serial.printf("SIMULACAO: %s (bits %u)\n", simBits ? "ligada" : "desligada", simBits);
}

uint8_t simulados() { return simBits; }

uint8_t leds() {
  uint8_t bits = 0;
  for (int i = 0; i < 3; i++)
    if (digitalRead(SAIDAS[i].pino)) bits |= 1 << i;  // saída: lê o nível do próprio pino
  return bits;
}

void limparParada() { paradaPor = 0; }

bool comandaSaidas() {
  return !ensaio::ativo() && (controle::ativo() || motor::duty() > 0 || ativos() != 0);
}

void definir(bool verde, bool amarelo, bool vermelho, bool buzzer) {
  escrever(0, verde);
  escrever(1, amarelo);
  escrever(2, vermelho);
  escrever(3, buzzer);
}

}  // namespace alarmes
