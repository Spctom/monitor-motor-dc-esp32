#include "ensaio.h"

#include <esp_system.h>
#include <stdarg.h>

#include "alarmes.h"
#include "barramento_i2c.h"
#include "config.h"
#include "controle.h"
#include "corrente.h"
#include "hall.h"
#include "motor.h"
#include "registro.h"
#include "temperatura.h"
#include "vibracao.h"

namespace ensaio {
namespace {

// ---------------- Roteiro do ensaio ----------------
enum class Tipo : uint8_t { Saida, Oled, Repouso, Contagem, Motor, Pausa, Parada };

struct Passo {
  Tipo tipo;
  const char* nome;
  float duty;
  uint32_t duracaoMs;
  uint8_t saida;  // só para Tipo::Saida: 0 verde, 1 amarelo, 2 vermelho, 3 buzzer
  bool pulso;     // só para Tipo::Motor: se o motor estiver parado, dá o pulso de partida antes
};

constexpr uint32_t T_SOBE_MS = 5000;
constexpr uint32_t T_DESCE_MS = 4000;
constexpr uint32_t T_BAIXA_MS = 5000;
constexpr uint32_t T_PAUSA_MS = 2500;  // motor desligado até parar antes de cada degrau com pulso

const Passo PASSOS[] = {
  {Tipo::Saida,    "LED_VERDE",    0, 1000, 0, false},
  {Tipo::Saida,    "LED_AMARELO",  0, 1000, 1, false},
  {Tipo::Saida,    "LED_VERMELHO", 0, 1000, 2, false},
  {Tipo::Saida,    "BUZZER",       0,  600, 3, false},
  {Tipo::Oled,     "OLED",         0, 3000, 0, false},
  {Tipo::Repouso,  "REPOUSO",      0, 6000, 0, false},
  {Tipo::Contagem, "CONTAGEM",     0, 5000, 0, false},
  {Tipo::Motor, "SOBE_30", 30, T_SOBE_MS, 0, false},
  {Tipo::Motor, "SOBE_40", 40, T_SOBE_MS, 0, false},
  {Tipo::Motor, "SOBE_50", 50, T_SOBE_MS, 0, false},
  {Tipo::Motor, "SOBE_60", 60, T_SOBE_MS, 0, false},
  {Tipo::Motor, "SOBE_70", 70, T_SOBE_MS, 0, false},
  {Tipo::Motor, "SOBE_80", 80, T_SOBE_MS, 0, false},
  {Tipo::Motor, "SOBE_90", 90, T_SOBE_MS, 0, false},
  {Tipo::Motor, "SOBE_MAX", ENSAIO_DUTY_MAX_PCT, T_SOBE_MS, 0, false},
  {Tipo::Motor, "DESCE_70", 70, T_DESCE_MS, 0, false},
  {Tipo::Motor, "DESCE_60", 60, T_DESCE_MS, 0, false},
  {Tipo::Motor, "DESCE_50", 50, T_DESCE_MS, 0, false},
  {Tipo::Motor, "DESCE_40", 40, T_DESCE_MS, 0, false},
  {Tipo::Motor, "DESCE_30", 30, T_DESCE_MS, 0, false},
  // Baixa rotação com o pulso de partida do controle: cada degrau começa com o motor parado.
  {Tipo::Pausa, "PAUSA", 0, T_PAUSA_MS, 0, false},
  {Tipo::Motor, "BAIXA_25", 25, T_BAIXA_MS, 0, true},
  {Tipo::Pausa, "PAUSA", 0, T_PAUSA_MS, 0, false},
  {Tipo::Motor, "BAIXA_20", 20, T_BAIXA_MS, 0, true},
  {Tipo::Pausa, "PAUSA", 0, T_PAUSA_MS, 0, false},
  {Tipo::Motor, "BAIXA_15", 15, T_BAIXA_MS, 0, true},
  {Tipo::Parada, "PARADA", 0, 6000, 0, false},
};
constexpr uint8_t NUM_PASSOS = sizeof(PASSOS) / sizeof(PASSOS[0]);
constexpr uint8_t MAX_DEGRAUS = 20;
constexpr float RPM_GIRANDO = 100.0f;          // abaixo disso considera parado
constexpr float DELTA_CORRENTE_MA = 150.0f;    // ~10 mV no pior caso (30 A): acima do ruído do ACS712
constexpr float VIB_GIRANDO_G = 0.02f;         // vibração que mostra o motor girando mesmo sem o Hall

// ---------------- Resultado ----------------
enum class Res : uint8_t { NaoTestado, Ok, Aviso, Falha };
enum Item : uint8_t { LM35, ACS712, MPU6050, OLED, HALL, MOTOR, NUM_ITENS };
const char* const NOME_ITEM[NUM_ITENS] = {"LM35", "ACS712", "MPU6050", "OLED", "HALL", "MOTOR"};

struct Resultado {
  Res res;
  char texto[110];
};

struct Media {
  float soma = 0;
  uint16_t n = 0;
  void somar(float v) {
    if (!isnan(v)) {
      soma += v;
      n++;
    }
  }
  float valor() const { return n ? soma / n : NAN; }
};

struct Degrau {
  const char* nome;
  float duty, rpm, mA, vib;
};

// ---------------- Estado ----------------
bool rodando = false;
uint16_t numeroEnsaio = 0;
int8_t passoAtual = -1;
uint32_t inicioEnsaioMs = 0, inicioPassoMs = 0, ultimoLogMs = 0, ultimoSalvarMs = 0;
const char* motivoReset = "?";

Resultado resultados[NUM_ITENS];
Degrau degraus[MAX_DEGRAUS];
uint8_t numDegraus = 0;
Media mRpm, mCorrente, mVib, mTemp, mPino, mAcel;
float vibMaxRepouso = 0;
uint32_t pulsosInicioPasso = 0;
bool emPulso = false;          // pulso de partida em andamento no degrau atual
uint32_t fimPulsoMs = 0;
bool zeroMedido = false;
float correnteRepousoMa = NAN;
uint32_t limiteDesde[4] = {0, 0, 0, 0};

Res resultadoFinal = Res::NaoTestado;
bool abortado = false;
bool semLog = false;
uint32_t fimEnsaioMs = 0;

// Botão BOOT (GPIO 0, nível baixo = apertado)
bool botaoEstavel = HIGH, botaoLeitura = HIGH, inicioPorBotaoFeito = false;
uint32_t botaoMudouMs = 0;
constexpr uint32_t BOTAO_DEBOUNCE_MS = 30;
constexpr uint32_t BOTAO_SEGURAR_MS = 1000;

const char* nomeRes(Res r) {
  switch (r) {
    case Res::Ok:    return "OK";
    case Res::Aviso: return "AVISO";
    case Res::Falha: return "FALHA";
    default:         return "NAO_TESTADO";
  }
}

const char* nomeResetBoot() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON:   return "LIGOU_ENERGIA";
    case ESP_RST_EXT:       return "PINO_EN";
    case ESP_RST_SW:        return "SOFTWARE";
    case ESP_RST_PANIC:     return "ERRO_FIRMWARE";
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT:       return "WATCHDOG";
    case ESP_RST_BROWNOUT:  return "QUEDA_DE_TENSAO";
    default:                return "DESCONHECIDO";
  }
}

void marcar(Item item, Res res, const char* formato, ...) __attribute__((format(printf, 3, 4)));
void marcar(Item item, Res res, const char* formato, ...) {
  resultados[item].res = res;
  va_list args;
  va_start(args, formato);
  vsnprintf(resultados[item].texto, sizeof(resultados[item].texto), formato, args);
  va_end(args);
}

void zerarMedias() {
  mRpm = Media();
  mCorrente = Media();
  mVib = Media();
  mTemp = Media();
  mPino = Media();
  mAcel = Media();
}

// ---------------- OLED sem biblioteca: acende todos os pixels para conferir a olho ----------------
bool oledComando(uint8_t c) { return i2c::escrever8(ENDERECO_OLED, 0x00, c); }

bool oledAcender() {
  static const uint8_t INICIO[] = {0xAE, 0xD5, 0x80, 0xA8, 0x3F, 0xD3, 0x00, 0x40, 0x8D, 0x14,
                                   0xA1, 0xC8, 0xDA, 0x12, 0x81, 0xCF, 0xD9, 0xF1, 0xDB, 0x40,
                                   0xA6, 0xA5, 0xAF};  // 0xA5 = todos os pixels ligados
  for (uint8_t c : INICIO) {
    if (!oledComando(c)) return false;
  }
  return true;
}

void oledApagar() {
  oledComando(0xA4);
  oledComando(0xAE);
}

// ---------------- Avaliação ----------------
void avaliarRepouso() {
  const float temp = mTemp.valor();
  if (!LM35_INSTALADO) marcar(LM35, Res::NaoTestado, "desativado em config.h (LM35_INSTALADO = false)");
  else if (isnan(temp)) marcar(LM35, Res::Falha, "sem leitura");
  else if (!LM35_NA_PROTECAO) marcar(LM35, Res::Aviso, "%.1f C: estimativa, sensor com erro e fora da protecao", temp);
  else if (temp < 5 || temp > LIMITE_TEMPERATURA_C)
    marcar(LM35, Res::Falha, "%.1f C fora de 5..%.0f C: conferir fio de sinal e 5 V", temp, LIMITE_TEMPERATURA_C);
  else if (temp < 15)
    marcar(LM35, Res::Aviso, "%.1f C: abaixo de 15 C o ADC em 11 dB e impreciso", temp);
  else marcar(LM35, Res::Ok, "%.1f C (conferir se parece a temperatura ambiente)", temp);

  const float pino = mPino.valor();
  correnteRepousoMa = mCorrente.valor();
  if (corrente::ok())
    marcar(ACS712, Res::Ok, "zero %.0f mV no modulo; variacao ainda nao avaliada", corrente::zeroMv());
  else if (!isnan(pino) && pino >= 1450 && pino < 1950)
    marcar(ACS712, Res::Falha, "pino em %.0f mV: VCC do modulo sem 5 V (ou ha divisor: ajustar ACS712_DIV_R_CIMA_OHM)", pino);
  else if (!isnan(pino) && pino < 200)
    marcar(ACS712, Res::Falha, "pino em %.0f mV: OUT desligado ou modulo sem 5 V", pino);
  else marcar(ACS712, Res::Falha, "pino em %.0f mV: esperado ~2500 (conferir 5 V e OUT)", pino);

  const float acel = mAcel.valor();
  if (!vibracao::ok() || isnan(acel)) {
    marcar(MPU6050, Res::Falha, "nao responde em 0x%02X (conferir SDA, SCL, VCC, GND)", ENDERECO_MPU6050);
  } else if (acel < 0.6f || acel > 1.4f) {
    marcar(MPU6050, Res::Falha, "|a| = %.3f g parado; esperado ~1,0 g", acel);
  } else if (acel < 0.9f || acel > 1.1f) {
    marcar(MPU6050, Res::Aviso, "|a| = %.3f g parado: desvio do sensor (esperado ~1,0 g)", acel);
  } else if (vibMaxRepouso > 0.05f) {
    marcar(MPU6050, Res::Aviso, "|a| = %.3f g, mas vibrou %.4f g parado", acel, vibMaxRepouso);
  } else {
    marcar(MPU6050, Res::Ok, "|a| = %.3f g, vibracao parado %.4f g", acel, vibMaxRepouso);
  }

  const uint32_t ruido = hall::pulsosTotais() - pulsosInicioPasso;
  if (ruido > 0) marcar(HALL, Res::Aviso, "%lu pulso(s) com o motor parado: ruido ou mao no ima", (unsigned long)ruido);
}

void avaliarMotor() {
  if (numDegraus == 0) return;
  float rpmMax = 0, correnteMax = NAN;
  for (uint8_t i = 0; i < numDegraus; i++) {
    if (!isnan(degraus[i].rpm)) rpmMax = max(rpmMax, degraus[i].rpm);
    if (!isnan(degraus[i].mA) && (isnan(correnteMax) || fabsf(degraus[i].mA) > fabsf(correnteMax)))
      correnteMax = degraus[i].mA;
  }
  const float delta = isnan(correnteMax) ? NAN : correnteMax - (isnan(correnteRepousoMa) ? 0 : correnteRepousoMa);
  const bool correnteMudou = !isnan(delta) && fabsf(delta) >= DELTA_CORRENTE_MA;
  const bool hallTinhaAviso = resultados[HALL].res == Res::Aviso;
  float vibMax = 0;
  for (uint8_t i = 0; i < numDegraus; i++) {
    if (!isnan(degraus[i].vib)) vibMax = max(vibMax, degraus[i].vib);
  }
  const bool girouPorVibracao = vibMax >= max(VIB_GIRANDO_G, vibMaxRepouso * 3.0f);
  const bool girou = rpmMax >= RPM_GIRANDO || girouPorVibracao;

  if (rpmMax >= RPM_GIRANDO) {
    marcar(MOTOR, Res::Ok, "girou; maximo %.0f RPM", rpmMax);
    if (hallTinhaAviso) {
      // Mantém o aviso de ruído, acrescentando que os pulsos com motor funcionam.
      char antes[80];
      strlcpy(antes, resultados[HALL].texto, sizeof(antes));
      marcar(HALL, Res::Aviso, "le ate %.0f RPM; %s", rpmMax, antes);
    } else {
      marcar(HALL, Res::Ok, "le ate %.0f RPM", rpmMax);
    }
  } else if (girouPorVibracao) {
    marcar(MOTOR, Res::Ok, "girou (vibracao ate %.3f g), sem pulsos do Hall", vibMax);
    marcar(HALL, Res::Falha, "motor girou e nao houve pulsos: conferir VCC, distancia e polo do ima");
  } else if (correnteMudou) {
    marcar(MOTOR, Res::Aviso, "consumiu %.0f mA, sem pulsos: confirmar se girou", correnteMax);
    marcar(HALL, Res::Falha, "sem pulsos: conferir distancia, polo do ima e pull-up");
  } else if (corrente::ok()) {
    marcar(MOTOR, Res::Falha, "sem giro e sem corrente: fonte de 12 V ligada? ENA/IN1/IN2?");
    marcar(HALL, Res::NaoTestado, "motor nao girou");
  } else {
    marcar(MOTOR, Res::Falha, "sem pulsos do Hall e ACS712 sem leitura: nao deu para confirmar o giro");
    marcar(HALL, Res::Falha, "sem pulsos");
  }

  if (corrente::ok()) {
    if (correnteMudou && delta > 0)
      marcar(ACS712, Res::Ok, "zero %.0f mV; pico medio %.0f mA", corrente::zeroMv(), correnteMax);
    else if (correnteMudou)
      marcar(ACS712, Res::Aviso, "corrente negativa (%.0f mA): inverter IP+ e IP-", correnteMax);
    else if (girou)
      marcar(ACS712, Res::Aviso, "motor girou e a leitura quase nao mudou: confirmar com a helice (carga)");
  }
}

void escreverResumo(const char* motivoAbort) {
  registro::escrever("# === RESUMO ===\n");
  if (motivoAbort) registro::escrever("# ABORTADO: %s\n", motivoAbort);
  for (uint8_t i = 0; i < NUM_ITENS; i++) {
    registro::escrever("# RESULTADO %s %s: %s\n", NOME_ITEM[i], nomeRes(resultados[i].res), resultados[i].texto);
  }
  registro::escrever(BUZZER_INSTALADO
                         ? "# CONFERIR A OLHO: 3 LEDs e buzzer acenderam um de cada vez; OLED ficou todo branco por 3 s\n"
                         : "# CONFERIR A OLHO: 3 LEDs acenderam um de cada vez (buzzer desativado); OLED ficou todo branco por 3 s\n");

  registro::escrever("# CURVA degrau,duty_pct,rpm,corrente_mA,vib_g\n");
  float partida = NAN, sustenta = NAN, comPulso = NAN;
  for (uint8_t i = 0; i < numDegraus; i++) {
    const Degrau& d = degraus[i];
    registro::escrever("# CURVA %s,%.0f,%.0f,%.0f,%.4f\n", d.nome, d.duty, d.rpm, d.mA, d.vib);
    if (d.rpm >= RPM_GIRANDO) {
      if (isnan(partida) && d.nome[0] == 'S') partida = d.duty;
      if (d.nome[0] == 'D') sustenta = d.duty;
      if (d.nome[0] == 'B' && (isnan(comPulso) || d.duty < comPulso)) comPulso = d.duty;
    }
  }
  if (!isnan(partida)) registro::escrever("# DUTY_MINIMO partida (subindo): %.0f %%\n", partida);
  if (!isnan(sustenta)) registro::escrever("# DUTY_MINIMO mantem girando (descendo): %.0f %%\n", sustenta);
  if (!isnan(comPulso))
    registro::escrever("# DUTY_MINIMO com pulso de partida (%.0f %% por %lu ms): %.0f %%\n", CONTROLE_DUTY_PARTIDA,
                       (unsigned long)CONTROLE_PARTIDA_MS, comPulso);
  else
    registro::escrever("# DUTY_MINIMO com pulso de partida: nao girou em nenhum degrau baixo\n");
  registro::escrever("# FLASH maior gravacao: %lu ms; erro de escrita: %s\n",
                     (unsigned long)registro::maiorSalvarMs(), registro::houveErroEscrita() ? "SIM" : "nao");
}

void concluir(const char* motivoAbort) {
  motor::definirDuty(0);
  emPulso = false;
  alarmes::definir(false, false, false, false);
  if (OLED_INSTALADO && passoAtual >= 0 && PASSOS[passoAtual].tipo == Tipo::Oled) oledApagar();

  abortado = motivoAbort != nullptr;
  avaliarMotor();

  resultadoFinal = abortado ? Res::Falha : Res::Ok;
  for (const Resultado& r : resultados) {
    if (r.res == Res::Falha) resultadoFinal = Res::Falha;
    else if (r.res == Res::Aviso && resultadoFinal == Res::Ok) resultadoFinal = Res::Aviso;
  }

  escreverResumo(motivoAbort);
  registro::escrever("# GERAL %s\n", nomeRes(resultadoFinal));
  registro::escrever("# FIM\n");
  registro::fechar();

  rodando = false;
  passoAtual = -1;
  fimEnsaioMs = millis();
  // Quem parou o ensaio segurando o BOOT não pode religar o motor sem soltar o botão antes.
  if (botaoEstavel == LOW) inicioPorBotaoFeito = true;
  Serial.printf("Ensaio %u terminou: %s%s%s. Comando b %u mostra o log.\n", numeroEnsaio, nomeRes(resultadoFinal),
                motivoAbort ? ", abortado por " : "", motivoAbort ? motivoAbort : "", numeroEnsaio);
}

// ---------------- Andamento ----------------
void amostrar(uint32_t agora) {
  const Passo& p = PASSOS[passoAtual];
  const uint32_t noPasso = agora - inicioPassoMs;
  const float rpm = hall::rpm(), mA = corrente::miliamperes(), pino = corrente::pinoMv();
  const float temp = temperatura::celsius();
  const bool mpu = vibracao::ok();
  const float vib = mpu ? vibracao::rmsG() : NAN, acel = mpu ? vibracao::moduloG() : NAN;

  registro::escrever("%lu,%s,%.0f,%.0f,%lu,%.0f,%.0f,%.1f,%.4f,%.3f\n", (unsigned long)(agora - inicioEnsaioMs),
                     p.nome, motor::duty(), rpm, (unsigned long)hall::pulsosTotais(), mA, pino, temp, vib, acel);

  if (p.tipo == Tipo::Repouso && noPasso >= 2000) {
    mTemp.somar(temp);
    mPino.somar(pino);
    mCorrente.somar(mA);
    mAcel.somar(acel);
    if (!isnan(vib)) vibMaxRepouso = max(vibMaxRepouso, vib);
  } else if (p.tipo == Tipo::Motor && noPasso >= p.duracaoMs * 6 / 10) {  // só depois de estabilizar
    mRpm.somar(rpm);
    mCorrente.somar(mA);
    mVib.somar(vib);
  }
}

void entrarPasso(uint8_t i) {
  passoAtual = i;
  inicioPassoMs = millis();
  pulsosInicioPasso = hall::pulsosTotais();
  zerarMedias();

  const Passo& p = PASSOS[i];
  emPulso = p.tipo == Tipo::Motor && p.pulso && hall::rpm() < RPM_GIRANDO;
  if (emPulso) {
    // Mesmo pulso do controle: rompe a inércia e depois cai para o duty do degrau.
    motor::definirDuty(CONTROLE_DUTY_PARTIDA);
    fimPulsoMs = inicioPassoMs + CONTROLE_PARTIDA_MS;
    registro::escrever("# PASSO %s duty=%.0f (pulso de %.0f %% por %lu ms antes)\n", p.nome, p.duty,
                       CONTROLE_DUTY_PARTIDA, (unsigned long)CONTROLE_PARTIDA_MS);
  } else {
    motor::definirDuty(p.tipo == Tipo::Motor ? p.duty : 0.0f);
    registro::escrever("# PASSO %s duty=%.0f\n", p.nome, motor::duty());
  }

  switch (p.tipo) {
    case Tipo::Saida:
      alarmes::definir(p.saida == 0, p.saida == 1, p.saida == 2, p.saida == 3);
      break;
    case Tipo::Oled:
      alarmes::definir(false, false, false, false);
      if (!OLED_INSTALADO)
        marcar(OLED, Res::NaoTestado, "ainda nao instalado");
      else if (!i2c::responde(ENDERECO_OLED))
        marcar(OLED, Res::Falha, "nao responde em 0x%02X (conferir SDA, SCL, VCC, GND)", ENDERECO_OLED);
      else if (oledAcender())
        marcar(OLED, Res::Ok, "responde em 0x%02X (conferir se a tela ficou branca)", ENDERECO_OLED);
      else
        marcar(OLED, Res::Falha, "respondeu em 0x%02X, mas falhou ao receber comandos", ENDERECO_OLED);
      break;
    case Tipo::Repouso:
      alarmes::definir(false, false, false, false);
      zeroMedido = false;
      break;
    case Tipo::Contagem:
      alarmes::definir(false, true, false, false);
      break;
    case Tipo::Motor:
      alarmes::definir(false, true, true, false);  // amarelo + vermelho = motor ligado
      break;
    case Tipo::Pausa:
    case Tipo::Parada:
      alarmes::definir(false, true, false, false);
      break;
  }
}

void sairPasso() {
  const Passo& p = PASSOS[passoAtual];
  if (p.tipo == Tipo::Oled && OLED_INSTALADO) oledApagar();
  if (p.tipo == Tipo::Repouso) avaliarRepouso();
  if (p.tipo == Tipo::Motor && numDegraus < MAX_DEGRAUS) {
    degraus[numDegraus++] = {p.nome, p.duty, mRpm.valor(), mCorrente.valor(), mVib.valor()};
  }
  if (p.tipo == Tipo::Parada && hall::rpm() >= RPM_GIRANDO) {
    registro::escrever("# AVISO: ainda havia pulsos %lu ms depois de desligar\n", (unsigned long)p.duracaoMs);
  }
}

bool persistiu(uint8_t i, bool condicao, uint32_t tempoMs, uint32_t agora) {
  if (!condicao) {
    limiteDesde[i] = 0;
    return false;
  }
  if (limiteDesde[i] == 0) limiteDesde[i] = agora;
  return agora - limiteDesde[i] >= tempoMs;
}

// Protege motor, ponte e hélice enquanto ninguém está olhando o Serial.
const char* limiteEstourado(uint32_t agora) {
  const float mA = corrente::miliamperes(), temp = temperatura::paraProtecao();
  const float vib = vibracao::ok() ? vibracao::rmsG() : NAN;
  if (persistiu(0, corrente::saturado(), 300, agora)) return "ACS712_SATURADO";
  if (persistiu(1, corrente::ok() && !isnan(mA) && fabsf(mA) > LIMITE_CORRENTE_MA, 1000, agora)) return "CORRENTE_ALTA";
  if (persistiu(2, !isnan(temp) && temp > LIMITE_TEMPERATURA_C, 1000, agora)) return "TEMPERATURA_ALTA";
  if (persistiu(3, !isnan(vib) && vib > LIMITE_VIBRACAO_G, 2000, agora)) return "VIBRACAO_ALTA";
  return nullptr;
}

// Devolve true uma vez por toque (para abortar) e cuida do "segurar 1 s" (para começar).
bool tocouBotao(uint32_t agora) {
  const bool leitura = digitalRead(PINO_BOTAO_BOOT);
  if (leitura != botaoLeitura) {
    botaoLeitura = leitura;
    botaoMudouMs = agora;
  }
  bool toque = false;
  if (agora - botaoMudouMs >= BOTAO_DEBOUNCE_MS && leitura != botaoEstavel) {
    botaoEstavel = leitura;
    toque = botaoEstavel == LOW;
    if (botaoEstavel == HIGH) inicioPorBotaoFeito = false;
  }
  return toque;
}

// LEDs e buzzer enquanto espera: mostram se está pronto e como foi o último ensaio.
void indicarEspera(uint32_t agora) {
  if (alarmes::emTeste()) return;
  const uint32_t t = agora - fimEnsaioMs;
  bool verde = false, amarelo = false, vermelho = false, buzzer = false;
  if (semLog) {
    vermelho = (agora % 200) < 100;
  } else {
    switch (resultadoFinal) {
      case Res::Ok:
        verde = true;
        buzzer = t < 800;
        break;
      case Res::Aviso:
        amarelo = true;
        buzzer = t < 800 && (t % 400) < 200;
        break;
      case Res::Falha:
        vermelho = abortado ? (agora % 1000) < 500 : true;
        buzzer = t < 1200 && (t % 400) < 200;
        break;
      default:
        verde = (agora % 1000) < 150;  // pronto: pisca curto
    }
  }
  alarmes::definir(verde, amarelo, vermelho, buzzer);
}

}  // namespace

void iniciar() {
  pinMode(PINO_BOTAO_BOOT, INPUT_PULLUP);
  motivoReset = nomeResetBoot();
  if (!registro::iniciar()) {
    semLog = true;
    Serial.println("ERRO: flash indisponivel, os ensaios nao serao gravados.");
    return;
  }
  const uint16_t interrompido = registro::anotarInterrupcao(motivoReset);
  if (interrompido) Serial.printf("Ensaio %u foi interrompido (reset: %s).\n", interrompido, motivoReset);
  Serial.printf("Ultimo reset: %s\n", motivoReset);
  registro::listar(Serial);
}

void comecar() {
  if (rodando) return;
  controle::parar();
  alarmes::limparParada();
  motor::definirDuty(0);
  if (alarmes::emTeste()) alarmes::manterTudo(false);

  numeroEnsaio = registro::novoArquivo();
  if (numeroEnsaio == 0) {
    semLog = true;
    Serial.println("ERRO: nao consegui criar o log. Ensaio cancelado, motor continua parado.");
    return;
  }
  semLog = false;

  for (Resultado& r : resultados) {
    r.res = Res::NaoTestado;
    strlcpy(r.texto, "nao chegou a ser avaliado", sizeof(r.texto));
  }
  numDegraus = 0;
  vibMaxRepouso = 0;
  correnteRepousoMa = NAN;
  abortado = false;
  resultadoFinal = Res::NaoTestado;
  memset(limiteDesde, 0, sizeof(limiteDesde));
  if (!vibracao::ok() && i2c::responde(ENDERECO_MPU6050)) vibracao::iniciar();

  registro::escrever("# ENSAIO %u\n", numeroEnsaio);
  registro::escrever("# firmware %s %s | reset no boot: %s | ligado ha %lu s\n", __DATE__, __TIME__, motivoReset,
                     (unsigned long)(millis() / 1000));
  registro::escrever("# PWM %lu Hz, modo %s, duty maximo %.0f %%\n", (unsigned long)motor::frequencia(),
                     motor::nomeModo(), ENSAIO_DUTY_MAX_PCT);
  registro::escrever("# ACS712 %.0f mV/A, divisor %.3f | limites: %.0f mA, %.0f C, %.2f g\n", ACS712_MV_POR_A,
                     ACS712_DIVISOR_FATOR, LIMITE_CORRENTE_MA, LIMITE_TEMPERATURA_C, LIMITE_VIBRACAO_G);
  // Na fonte não há Serial: o log é o único lugar para saber se o boot bloqueou algum LED.
  const uint8_t bl = alarmes::bloqueios();
  registro::escrever("# SAIDAS bloqueadas no boot (pino lido em nivel baixo):%s%s%s%s\n", bl ? "" : " nenhuma",
                     (bl & 1) ? " verde" : "", (bl & 2) ? " amarelo" : "", (bl & 4) ? " vermelho" : "");
  registro::escrever("# ");
  i2c::escanear(registro::saida());
  registro::escrever("t_ms,fase,duty_pct,rpm,pulsos,corrente_mA,acs_pino_mV,temp_C,vib_g,acel_g\n");
  registro::salvar();

  rodando = true;
  inicioEnsaioMs = millis();
  ultimoLogMs = inicioEnsaioMs;
  ultimoSalvarMs = inicioEnsaioMs;
  entrarPasso(0);
  Serial.printf("Ensaio %u iniciado. Um toque no BOOT (ou o comando s) para tudo.\n", numeroEnsaio);
}

void abortar(const char* motivo) {
  if (!rodando) return;
  motor::definirDuty(0);  // primeiro o motor, depois a papelada
  registro::escrever("# ABORTANDO em t=%lu ms: %s\n", (unsigned long)(millis() - inicioEnsaioMs), motivo);
  concluir(motivo);
}

bool ativo() { return rodando; }

void atualizar() {
  const uint32_t agora = millis();
  const bool toque = tocouBotao(agora);

  if (!rodando) {
    // Com o motor ligado, um toque no BOOT sempre para, e segurar não emenda um ensaio.
    if (toque && (controle::ativo() || motor::duty() > 0)) {
      controle::parar();
      motor::definirDuty(0);
      inicioPorBotaoFeito = true;
      Serial.println("Motor parado pelo botao BOOT.");
    }
    if (botaoEstavel == LOW && !inicioPorBotaoFeito && agora - botaoMudouMs >= BOTAO_SEGURAR_MS) {
      inicioPorBotaoFeito = true;  // só vale de novo depois de soltar
      comecar();
      if (rodando) return;
    }
    if (!alarmes::comandaSaidas()) indicarEspera(agora);
    return;
  }

  if (toque) {
    abortar("BOTAO_BOOT");
    return;
  }

  const Passo& p = PASSOS[passoAtual];
  if (p.tipo == Tipo::Motor) {
    if (const char* motivo = limiteEstourado(agora)) {
      abortar(motivo);
      return;
    }
  }

  const uint32_t noPasso = agora - inicioPassoMs;
  if (emPulso && (int32_t)(agora - fimPulsoMs) >= 0) {
    emPulso = false;
    motor::definirDuty(p.duty);
  }
  if (p.tipo == Tipo::Contagem) {
    alarmes::definir(false, true, false, (noPasso % 1000) < 120);  // bipe por segundo: afaste a mão
  } else if (p.tipo == Tipo::Repouso && !zeroMedido && noPasso >= 1000) {
    corrente::medirZero();  // motor parado há pelo menos 1 s, já na fonte que vai ser usada
    zeroMedido = true;
  }

  if (agora - ultimoLogMs >= ENSAIO_PERIODO_LOG_MS) {
    ultimoLogMs = agora;
    amostrar(agora);
  }
  if (agora - ultimoSalvarMs >= ENSAIO_PERIODO_SALVAR_MS) {
    ultimoSalvarMs = agora;
    registro::salvar();
  }

  if (noPasso >= p.duracaoMs) {
    sairPasso();
    // Sem leitura de corrente não há proteção: o motor não chega a ligar.
    if (p.tipo == Tipo::Repouso && !corrente::ok()) {
      abortar("ACS712_SEM_LEITURA");
      return;
    }
    if (passoAtual + 1 < NUM_PASSOS) {
      entrarPasso(passoAtual + 1);
    } else {
      concluir(nullptr);
    }
  }
}

}  // namespace ensaio
