// Ensaio automático da bancada: testa todos os componentes sem precisar do USB e grava o
// resultado na flash (ver registro.h).
//
// Na fonte de 12 V: LED verde piscando = pronto. Segure o botão BOOT por 1 s para começar.
// Depois de uma contagem de 5 s com bipes, o motor sobe e desce em degraus.
// Um toque no BOOT, a qualquer momento, para o motor e encerra o ensaio.
#pragma once

#include <Arduino.h>

namespace ensaio {

void iniciar();                     // chamar no setup, depois dos sensores
void atualizar();                   // chamar a cada volta do loop
void comecar();                     // o mesmo que segurar o BOOT (comando "e")
void abortar(const char* motivo);   // para o motor na hora e fecha o log
bool ativo();

}  // namespace ensaio
