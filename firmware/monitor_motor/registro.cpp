#include "registro.h"

#include <LittleFS.h>
#include <stdarg.h>

#include "config.h"

namespace registro {
namespace {

const char* const MARCA_EM_CURSO = "/em_curso";  // existe enquanto um ensaio está aberto

bool montado = false;
bool erroEscrita = false;
File arquivo;
uint32_t maiorSalvar = 0;

void caminho(char* destino, size_t tam, uint16_t numero) {
  snprintf(destino, tam, "/e%04u.csv", numero);
}

// Lê do diretório os números dos ensaios gravados, em ordem crescente.
size_t numeros(uint16_t* v, size_t maximo) {
  size_t n = 0;
  File raiz = LittleFS.open("/");
  if (!raiz) return 0;
  for (File f = raiz.openNextFile(); f; f = raiz.openNextFile()) {
    unsigned num = 0;
    char fim = 0;
    const char* nome = f.name();
    if (nome[0] == '/') nome++;
    if (sscanf(nome, "e%4u.cs%c", &num, &fim) == 2 && fim == 'v' && num > 0 && n < maximo) {
      size_t i = n++;
      for (; i > 0 && v[i - 1] > num; i--) v[i] = v[i - 1];
      v[i] = uint16_t(num);
    }
  }
  return n;
}

bool enviarUm(Print& destino, uint16_t numero) {
  char nome[16];
  caminho(nome, sizeof(nome), numero);
  File f = LittleFS.open(nome, "r");
  if (!f) return false;

  destino.printf("=== ARQUIVO e%04u.csv ===\n", numero);
  uint8_t bloco[128];
  uint32_t bytes = 0, soma = 0;
  bool terminouEmLinha = true;
  while (f.available()) {
    const size_t lidos = f.read(bloco, sizeof(bloco));
    if (lidos == 0) break;
    destino.write(bloco, lidos);
    feedLoopWDT();  // enviar o log pode levar mais que o tempo do watchdog
    bytes += lidos;
    for (size_t i = 0; i < lidos; i++) soma += bloco[i];
    terminouEmLinha = bloco[lidos - 1] == '\n';
  }
  f.close();
  // O marcador precisa começar numa linha nova mesmo se o log foi cortado no meio de uma linha.
  if (!terminouEmLinha) destino.println();
  destino.printf("=== ARQUIVO FIM %lu %lu ===\n", (unsigned long)bytes, (unsigned long)(soma & 0xFFFF));
  return true;
}

}  // namespace

bool iniciar() {
  montado = LittleFS.begin(true);
  return montado;
}

bool ok() { return montado; }

uint16_t novoArquivo() {
  if (!montado) return 0;
  fechar();

  uint16_t lista[ENSAIO_MAX_ARQUIVOS + 1];
  size_t n = numeros(lista, ENSAIO_MAX_ARQUIVOS + 1);
  const uint16_t proximo = n ? lista[n - 1] + 1 : 1;
  if (proximo > 9999) return 0;

  // Abre espaço apagando só os ensaios mais antigos.
  char nome[16];
  size_t i = 0;
  while (i < n && (n - i >= ENSAIO_MAX_ARQUIVOS ||
                   LittleFS.totalBytes() - LittleFS.usedBytes() < ENSAIO_FLASH_LIVRE_MIN)) {
    caminho(nome, sizeof(nome), lista[i++]);
    LittleFS.remove(nome);
  }

  caminho(nome, sizeof(nome), proximo);
  arquivo = LittleFS.open(nome, "w");
  if (!arquivo) return 0;

  File marca = LittleFS.open(MARCA_EM_CURSO, "w");
  if (marca) {
    marca.print(proximo);
    marca.close();
  }
  erroEscrita = false;
  maiorSalvar = 0;
  return proximo;
}

void escrever(const char* formato, ...) {
  if (!arquivo) return;
  char buf[200];
  va_list args;
  va_start(args, formato);
  const int tam = vsnprintf(buf, sizeof(buf), formato, args);
  va_end(args);
  if (tam <= 0) return;
  const size_t n = min(size_t(tam), sizeof(buf) - 1);
  if (arquivo.write(reinterpret_cast<const uint8_t*>(buf), n) != n) erroEscrita = true;
}

// File nulo: quem imprime nele sem ensaio aberto não escreve nada.
Print& saida() { return arquivo; }

void salvar() {
  if (!arquivo) return;
  const uint32_t inicio = millis();
  arquivo.flush();
  maiorSalvar = max(maiorSalvar, millis() - inicio);
}

void fechar() {
  if (!arquivo) return;
  arquivo.flush();
  arquivo.close();
  LittleFS.remove(MARCA_EM_CURSO);
}

uint32_t maiorSalvarMs() { return maiorSalvar; }
bool houveErroEscrita() { return erroEscrita; }

uint16_t anotarInterrupcao(const char* motivoReset) {
  if (!montado || !LittleFS.exists(MARCA_EM_CURSO)) return 0;

  File marca = LittleFS.open(MARCA_EM_CURSO, "r");
  const uint16_t numero = marca ? uint16_t(marca.parseInt()) : 0;
  if (marca) marca.close();

  if (numero > 0) {
    char nome[16];
    caminho(nome, sizeof(nome), numero);
    File f = LittleFS.open(nome, "a");
    if (f) {
      // Começa com \n porque o corte pode ter acontecido no meio de uma linha.
      f.printf("\n# INTERROMPIDO: o ESP32 reiniciou antes do fim do ensaio (motivo do reset: %s)\n",
               motivoReset);
      f.close();
    }
  }
  LittleFS.remove(MARCA_EM_CURSO);
  return numero;
}

void listar(Print& destino) {
  if (!montado) {
    destino.println("Flash indisponivel.");
    return;
  }
  uint16_t lista[ENSAIO_MAX_ARQUIVOS + 1];
  const size_t n = numeros(lista, ENSAIO_MAX_ARQUIVOS + 1);
  destino.printf("%u ensaio(s) na flash, %lu de %lu kB usados\n", unsigned(n),
                 (unsigned long)(LittleFS.usedBytes() / 1024),
                 (unsigned long)(LittleFS.totalBytes() / 1024));
  char nome[16];
  for (size_t i = 0; i < n; i++) {
    caminho(nome, sizeof(nome), lista[i]);
    File f = LittleFS.open(nome, "r");
    destino.printf("  ensaio %u: %lu bytes\n", lista[i], f ? (unsigned long)f.size() : 0UL);
    if (f) f.close();
  }
}

bool enviar(Print& destino, uint16_t numero) {
  if (!montado) return false;
  destino.println("=== LOGS INICIO ===");
  size_t enviados = 0;
  if (numero > 0) {
    enviados = enviarUm(destino, numero) ? 1 : 0;
  } else {
    uint16_t lista[ENSAIO_MAX_ARQUIVOS + 1];
    const size_t n = numeros(lista, ENSAIO_MAX_ARQUIVOS + 1);
    for (size_t i = 0; i < n; i++) enviados += enviarUm(destino, lista[i]) ? 1 : 0;
  }
  destino.printf("=== LOGS FIM %u ===\n", unsigned(enviados));
  return numero == 0 || enviados == 1;
}

bool apagarTudo() {
  if (!montado) return false;
  fechar();
  uint16_t lista[ENSAIO_MAX_ARQUIVOS + 1];
  const size_t n = numeros(lista, ENSAIO_MAX_ARQUIVOS + 1);
  char nome[16];
  bool tudo = true;
  for (size_t i = 0; i < n; i++) {
    caminho(nome, sizeof(nome), lista[i]);
    tudo = LittleFS.remove(nome) && tudo;
    feedLoopWDT();
  }
  return tudo;
}

}  // namespace registro
