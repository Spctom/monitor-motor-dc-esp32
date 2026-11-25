#include "display.h"

#include "alarmes.h"
#include "barramento_i2c.h"
#include "config.h"
#include "controle.h"
#include "dados.h"
#include "remoto.h"

namespace display {
namespace {

// Ligação do módulo I2C comum: P0=RS, P1=RW, P2=E, P3=luz de fundo, P4..P7=D4..D7.
constexpr uint8_t BIT_RS  = 0x01;
constexpr uint8_t BIT_E   = 0x04;
constexpr uint8_t BIT_LUZ = 0x08;

constexpr uint8_t COLUNAS = 16;
constexpr uint8_t CELULAS = COLUNAS * 2;
constexpr uint8_t SEM_CURSOR = 0xFF;
constexpr uint8_t ERROS_PARA_DESISTIR = 3;

bool ativo = false;
uint8_t endereco = 0;
uint8_t erros = 0;
uint8_t posicao = 0;            // de onde continua a procura por caracteres diferentes
uint8_t cursor = SEM_CURSOR;    // onde o cursor do LCD está
uint32_t ultimaTelaMs = 0;
char desejado[CELULAS];
char naTela[CELULAS];

// O módulo usa 0x20–0x27 (PCF8574) ou 0x38–0x3F (PCF8574A), conforme a versão do chip.
uint8_t procurarEndereco() {
  for (uint8_t e = 0x20; e <= 0x27; e++) {
    if (i2c::responde(e)) return e;
  }
  for (uint8_t e = 0x38; e <= 0x3F; e++) {
    if (i2c::responde(e)) return e;
  }
  return 0;
}

bool enviarNibble(uint8_t nibble, uint8_t flags) {
  const uint8_t base = (nibble & 0xF0) | flags | BIT_LUZ;
  const uint8_t b[2] = {uint8_t(base | BIT_E), base};
  return i2c::escreverBytes(endereco, b, sizeof(b));
}

// Os dois meios bytes vão numa transação só, para ocupar o barramento o menor tempo possível.
bool enviar(uint8_t valor, uint8_t flags) {
  const uint8_t alto = (valor & 0xF0) | flags | BIT_LUZ;
  const uint8_t baixo = uint8_t(valor << 4) | flags | BIT_LUZ;
  const uint8_t b[4] = {uint8_t(alto | BIT_E), alto, uint8_t(baixo | BIT_E), baixo};
  return i2c::escreverBytes(endereco, b, sizeof(b));
}

void registrarResultado(bool sucesso) {
  if (sucesso) {
    erros = 0;
    return;
  }
  if (++erros >= ERROS_PARA_DESISTIR) {
    ativo = false;  // um display com defeito não pode atrasar o controle
    Serial.println("LCD: parou de responder; seguindo sem display");
  }
}

void numero(char* destino, size_t tam, float v, uint8_t casas) {
  if (isnan(v)) strlcpy(destino, "--", tam);
  else snprintf(destino, tam, "%.*f", casas, v);
}

void definirLinha(uint8_t linha, const char* texto) {
  char* destino = desejado + linha * COLUNAS;
  memset(destino, ' ', COLUNAS);
  memcpy(destino, texto, strnlen(texto, COLUNAS));
}

void montarTela(uint32_t agora) {
  const dados::Amostra a = dados::atual();
  char l1[40], l2[40], v1[16], v2[16];

  if ((agora / PERIODO_LCD_PAGINA_MS) % 2 == 0) {
    numero(v1, sizeof(v1), a.rpm, 0);
    numero(v2, sizeof(v2), a.rpmRef, 0);
    snprintf(l1, sizeof(l1), "RPM %s/%s", v1, v2);
    numero(v1, sizeof(v1), a.duty, 0);
    snprintf(l2, sizeof(l2), "D%s%% %.9s", v1, controle::nomeEstado(controle::Estado(a.estado)));
  } else {
    numero(v1, sizeof(v1), a.corrente, 0);
    numero(v2, sizeof(v2), a.temperatura, 1);
    snprintf(l1, sizeof(l1), "I %smA T %sC", v1, v2);
    numero(v1, sizeof(v1), a.vibracao, 3);
    snprintf(l2, sizeof(l2), "Vib %sg", v1);
  }

  if (a.alarmes != 0) {
    strlcpy(l2, "ALARME", sizeof(l2));
    if (a.alarmes & alarmes::ALARME_TEMPERATURA) strlcat(l2, " T", sizeof(l2));
    if (a.alarmes & alarmes::ALARME_CORRENTE) strlcat(l2, " I", sizeof(l2));
    if (a.alarmes & alarmes::ALARME_VIBRACAO) strlcat(l2, " V", sizeof(l2));
    if (a.alarmes & alarmes::ALARME_ROTACAO) strlcat(l2, " R", sizeof(l2));
  } else if (a.ensaio) {
    strlcpy(l2, "Ensaio rodando", sizeof(l2));
  }

  definirLinha(0, l1);
  definirLinha(1, l2);
  desejado[CELULAS - 1] = remoto::conectado() ? 'W' : ' ';
}

}  // namespace

bool iniciar() {
  if (!LCD_INSTALADO) return false;
  endereco = procurarEndereco();
  if (endereco == 0) return false;

  delay(50);  // o LCD precisa de tempo depois de energizar
  // Sequência do datasheet do HD44780 para entrar no modo de 4 bits.
  for (uint8_t nibble : {0x30, 0x30, 0x30, 0x20}) {
    if (!enviarNibble(nibble, 0)) return false;
    delay(5);
  }
  // 2 linhas, display desligado, limpa, cursor avança, display ligado sem cursor.
  for (uint8_t comando : {0x28, 0x08, 0x01, 0x06, 0x0C}) {
    if (!enviar(comando, 0)) return false;
    delay(3);
  }
  memset(desejado, ' ', CELULAS);
  memset(naTela, ' ', CELULAS);
  ativo = true;
  Serial.printf("LCD 16x2 em 0x%02X\n", endereco);
  return true;
}

void passo() {
  if (!ativo) return;
  const uint32_t agora = millis();
  if (agora - ultimaTelaMs >= PERIODO_LCD_TELA_MS) {
    ultimaTelaMs = agora;
    montarTela(agora);
  }

  for (uint8_t i = 0; i < CELULAS; i++) {
    const uint8_t p = (posicao + i) % CELULAS;
    if (desejado[p] == naTela[p]) continue;
    posicao = p;

    if (cursor != p) {  // posicionar e escrever ficam em chamadas separadas
      const uint8_t coluna = p % COLUNAS;
      const uint8_t ddram = p < COLUNAS ? coluna : uint8_t(0x40 + coluna);
      const bool sucesso = enviar(0x80 | ddram, 0);
      registrarResultado(sucesso);
      if (sucesso) cursor = p;
      return;
    }

    const bool sucesso = enviar(uint8_t(desejado[p]), BIT_RS);
    registrarResultado(sucesso);
    if (sucesso) {
      naTela[p] = desejado[p];
      // No fim da primeira linha o cursor do LCD não pula para a segunda: precisa reposicionar.
      cursor = (p == COLUNAS - 1 || p == CELULAS - 1) ? SEM_CURSOR : uint8_t(p + 1);
      posicao = (p + 1) % CELULAS;
    } else {
      cursor = SEM_CURSOR;
    }
    return;
  }
}

bool ok() { return ativo; }

}  // namespace display
