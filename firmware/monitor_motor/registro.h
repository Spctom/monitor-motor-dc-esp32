// Log dos ensaios na memória flash do ESP32 (LittleFS). Sobrevive a desligar a fonte e a gravar
// um firmware novo pela IDE. Cada ensaio vira um arquivo /e0001.csv, /e0002.csv...
#pragma once

#include <Arduino.h>

namespace registro {

bool iniciar();                    // monta a flash (formata só se ainda não tiver sistema de arquivos)
bool ok();

uint16_t novoArquivo();            // abre o log do próximo ensaio; devolve o número (0 = falhou)
void escrever(const char* formato, ...) __attribute__((format(printf, 1, 2)));
Print& saida();                    // o arquivo aberto, para quem imprime direto nele
void salvar();                     // garante na flash o que foi escrito até agora
void fechar();
uint32_t maiorSalvarMs();          // maior tempo gasto num salvar() do ensaio atual
bool houveErroEscrita();

// Se o ESP32 reiniciou com um ensaio aberto, anota isso no fim do log daquele ensaio.
// Devolve o número do ensaio interrompido (0 = nenhum).
uint16_t anotarInterrupcao(const char* motivoReset);

void listar(Print& destino);
bool enviar(Print& destino, uint16_t numero);  // 0 = todos os ensaios
bool apagarTudo();

}  // namespace registro
