// Saídas de alarme: LEDs verde, amarelo e vermelho + buzzer ativo ligado direto no GPIO
// (nível alto = ligado).
// Lógica: limites com histerese e persistência. Temperatura, corrente ou vibração altas desligam
// o motor; erro de rotação só avisa. Verde = rodando normal, amarelo piscando = erro de rotação,
// vermelho = alarme crítico (fica aceso até a próxima referência ou o comando s).
#pragma once

#include <Arduino.h>

namespace alarmes {

enum : uint8_t {
  ALARME_TEMPERATURA = 1 << 0,
  ALARME_CORRENTE    = 1 << 1,
  ALARME_VIBRACAO    = 1 << 2,
  ALARME_ROTACAO     = 1 << 3,
};

void avaliar();          // chamar a cada PERIODO_ALARMES_MS
uint8_t ativos();        // bits dos alarmes ativos ou que pararam o motor
void limparParada();     // nova referência ou comando s: esquece o alarme que parou o motor
bool comandaSaidas();    // true enquanto os alarmes são donos dos LEDs e do buzzer

void iniciar();        // todas as saídas como OUTPUT, desligadas
void iniciarTeste();   // liga uma saída de cada vez, 1 s cada, sem bloquear o loop
void atualizar();      // chamar a cada volta do loop: avança o teste
bool emTeste();
void relatarBloqueios();      // avisa no Serial as saídas em curto, que ficam sem acionar
uint8_t bloqueios();          // bits das saídas bloqueadas no boot: 1 verde, 2 amarelo, 4 vermelho, 8 buzzer
uint8_t leds();               // nível lido agora nos pinos: 1 verde, 2 amarelo, 4 vermelho (diagnóstico)

// Simulação de falhas para demonstração (painel web). Os bits ALARME_* passam pelas mesmas
// regras dos alarmes reais (persistência, LEDs e, nos críticos, motor desligado). Acaba sozinha
// depois de SIMULACAO_MAX_MS ou com simular(0). Não vale durante o ensaio.
void simular(uint8_t bits);
uint8_t simulados();
void manterTudo(bool ligar);  // diagnóstico: deixa as 4 saídas ligadas para medir (desliga sozinho em 5 min)
void definir(bool verde, bool amarelo, bool vermelho, bool buzzer);  // liga/desliga direto

}  // namespace alarmes
