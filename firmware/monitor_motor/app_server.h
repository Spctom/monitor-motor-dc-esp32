// Servidor HTTP para o app do MIT App Inventor (celular na mesma rede do ESP32).
//
//   GET /                          painel para abrir no navegador do celular
//   GET /status                    medidas em JSON
//   GET /app                       medidas em uma linha separada por ";" (App Inventor)
//   GET /rpm?valor=1500            define a rotação
//   GET /parar                     para o motor (e interrompe o ensaio)
//   GET /ensaio                    roda o ensaio automático (o mesmo que segurar o BOOT)
//   GET /simular?tipo=temperatura  simula uma falha: temperatura, corrente, vibracao, rotacao, nenhum
//
// Sem senha nas rotas (decisão de 22/11): quem está na mesma rede consegue comandar o motor.
//
// O servidor roda na tarefa de rede (core 0). Ele só coloca pedidos numa fila, e quem
// mexe no motor é o loop (core 1), em aplicarComandos().
#pragma once

#include <Arduino.h>

namespace app_server {

void iniciarFila();        // no setup, antes da tarefa de rede
void aplicarComandos();    // no loop
void iniciar();            // na tarefa de rede, depois de conectar no Wi-Fi
void atender();            // na tarefa de rede

}  // namespace app_server
