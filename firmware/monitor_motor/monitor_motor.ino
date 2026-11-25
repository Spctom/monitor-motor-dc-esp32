// Monitoramento de motor DC e controle via Wi-Fi — DPEE1107 / UFSM
//
// Aquisição, controle PI, alarmes locais e rede (Wi-Fi, app e Thinger.io numa tarefa no core 0).
// Ensaio sem USB: na fonte, segure o botão BOOT por 1 s. O log fica na flash (ver ensaio.h).
// Display: LCD 16x2 I2C, ativado por LCD_INSTALADO em config.h.
//
// Serial Monitor a 115200, terminação "Nova linha". Comandos:
//   r <rpm>    liga o controle de rotação nesse RPM   (ex.: r 2000)
//   d <0-100>  malha aberta: define o duty em % (teto MOTOR_DUTY_MAX_PCT; ex.: d 60)
//   s          para o motor
//   f <Hz>     frequência do PWM (ex.: f 1000)
//   m          alterna o modo do PWM: rápido (PWM no ENA) / lento (PWM no IN2)
//   a          mostra os alarmes e o estado do controle
//   i          escaneia o barramento I2C
//   p          mostra o total de pulsos do Hall
//   t          testa LEDs e buzzer (um de cada vez, 1 s cada)
//   h <1|0>    mantém LEDs e buzzer ligados (1) ou desliga (0), para medir com multímetro
//   e          roda o ensaio automático (o mesmo que segurar o BOOT)
//   l          lista os ensaios gravados na flash
//   b [n]      mostra o log do ensaio n (sem número: todos)
//   apagar     apaga todos os logs da flash
//   ?          ajuda
// A saída "nome:valor,..." também funciona no Serial Plotter.

#include <esp_task_wdt.h>

#include "alarmes.h"
#include "app_server.h"
#include "barramento_i2c.h"
#include "config.h"
#include "controle.h"
#include "corrente.h"
#include "dados.h"
#include "display.h"
#include "ensaio.h"
#include "hall.h"
#include "motor.h"
#include "periodico.h"
#include "registro.h"
#include "remoto.h"
#include "temperatura.h"
#include "vibracao.h"

Periodico tarefaVibracao{PERIODO_VIBRACAO_MS};
Periodico tarefaRpm{PERIODO_RPM_MS};
Periodico tarefaTemperatura{PERIODO_TEMPERATURA_MS};
Periodico tarefaCorrente{PERIODO_CORRENTE_MS};
Periodico tarefaSerial{PERIODO_SERIAL_MS};
Periodico tarefaControle{PERIODO_CONTROLE_MS};
Periodico tarefaAlarmes{PERIODO_ALARMES_MS};
Periodico tarefaDados{PERIODO_DADOS_MS};
Periodico tarefaLcd{PERIODO_LCD_PASSO_MS};

void imprimirAjuda() {
  Serial.println("Comandos: r <rpm> (controle), a (alarmes), d <0-100> (duty %), s (parar), f <Hz> (freq. PWM), m (modo PWM), "
                 "i (scan I2C), p (pulsos Hall), t (teste LEDs/buzzer), x <t|c|v|r|0> (simula falha), h <1|0> (manter saidas), e (ensaio), l (lista logs), "
                 "b [n] (mostra log), apagar, ? (ajuda)");
}

void imprimirPwm() {
  Serial.printf("PWM: %lu Hz, modo %s\n", (unsigned long)motor::frequencia(), motor::nomeModo());
}

void imprimirStatus(const char* nome, bool ok) {
  Serial.printf("%-12s %s\n", nome, ok ? "OK" : "FALHOU");
}

void processarComando(char* linha) {
  // Durante o ensaio nada pode mexer no motor, e mostrar o log travaria o loop.
  if (ensaio::ativo()) {
    if (linha[0] == 's') ensaio::abortar("COMANDO_SERIAL");
    else Serial.println("Ensaio em andamento: so o comando s (parar) funciona.");
    return;
  }
  if (strcmp(linha, "testewdt") == 0) {  // trava o loop de propósito para provar que o watchdog reinicia
    if (motor::duty() > 0) {
      Serial.println("Pare o motor antes.");
      return;
    }
    Serial.println("Travando o loop...");
    Serial.flush();
    while (true) {}
  }
  if (strcmp(linha, "apagar") == 0) {
    Serial.println(registro::apagarTudo() ? "Logs apagados." : "ERRO ao apagar os logs.");
    return;
  }

  switch (linha[0]) {
    case 'r':
      alarmes::limparParada();
      if (const char* erro = controle::definirReferencia(atof(linha + 1))) {
        Serial.printf("referencia recusada: %s (faixa %.0f a %.0f RPM)\n", erro, RPM_REF_MIN, RPM_REF_MAX);
      } else {
        Serial.printf("referencia = %.0f RPM\n", controle::referencia());
      }
      break;
    case 'a':
      Serial.printf("controle: %s, ref %.0f RPM | alarmes: %u (1=temperatura, 2=corrente, 4=vibracao, 8=rotacao)"
                    " | simulados: %u | LEDs: %u (1=verde, 2=amarelo, 4=vermelho)\n",
                    controle::nomeEstado(controle::estado()), controle::referencia(), alarmes::ativos(),
                    alarmes::simulados(), alarmes::leds());
      break;
    case 'x': {  // simula falha, como o painel: x t, x c, x v, x r, x 0
      const char t = linha[1] == ' ' ? linha[2] : linha[1];
      alarmes::simular(t == 't' ? alarmes::ALARME_TEMPERATURA : t == 'c' ? alarmes::ALARME_CORRENTE
                       : t == 'v' ? alarmes::ALARME_VIBRACAO : t == 'r' ? alarmes::ALARME_ROTACAO : 0);
      break;
    }
    case 'd':
      controle::parar();  // malha aberta: o PI não pode disputar o duty
      alarmes::limparParada();
      motor::definirDuty(atof(linha + 1));
      Serial.printf("duty = %.1f %%\n", motor::duty());
      break;
    case 's':
      controle::parar();
      alarmes::limparParada();
      motor::definirDuty(0);
      Serial.println("motor parado");
      break;
    case 'f':
      motor::definirFrequencia(atol(linha + 1));
      imprimirPwm();
      break;
    case 'm':
      motor::definirModo(motor::modo() == motor::Modo::Rapido ? motor::Modo::Lento : motor::Modo::Rapido);
      imprimirPwm();
      break;
    case 'i':
      i2c::escanear(Serial);
      break;
    case 'p':
      Serial.printf("pulsos Hall = %lu\n", (unsigned long)hall::pulsosTotais());
      break;
    case 't':
      alarmes::iniciarTeste();
      break;
    case 'h':
      alarmes::manterTudo(atoi(linha + 1) != 0);
      break;
    case 'e':
      ensaio::comecar();
      break;
    case 'l':
      registro::listar(Serial);
      break;
    case 'b':
      if (motor::duty() > 0) {
        Serial.println("Pare o motor antes de ler o log (comando s).");
      } else if (!registro::enviar(Serial, uint16_t(atoi(linha + 1)))) {
        Serial.println("Ensaio nao encontrado.");
      }
      break;
    default:
      imprimirAjuda();
  }
}

// Lê uma linha do Serial sem bloquear.
void lerSerial() {
  static char buf[32];
  static uint8_t tam = 0;
  while (Serial.available()) {
    const char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (tam > 0) {
        buf[tam] = '\0';
        processarComando(buf);
        tam = 0;
      }
    } else if (tam < sizeof(buf) - 1) {
      buf[tam++] = c;
    }
  }
}

void imprimirMedidas() {
  Serial.printf("rpm:%.0f,ref:%.0f,duty:%.0f", hall::rpm(), controle::referencia(), motor::duty());
  if (!isnan(temperatura::celsius())) {
    Serial.printf(",temp_C:%.1f", temperatura::celsius());
  }
  if (corrente::ok()) {
    Serial.printf(",corrente_mA:%.0f", corrente::miliamperes());
    if (corrente::saturado()) Serial.print(",ACS712_SATURADO:1");
  }
  if (vibracao::ok() && !isnan(vibracao::rmsG())) {
    Serial.printf(",vib_g:%.4f", vibracao::rmsG());
  }
  if (alarmes::ativos()) Serial.printf(",alarmes:%u", alarmes::ativos());
  Serial.println();
}

// Cópia das medidas para a tarefa de rede: ela nunca lê os sensores direto.
void publicarDados() {
  dados::Amostra a;
  a.rpm = hall::rpm();
  a.rpmRef = controle::referencia();
  a.temperatura = temperatura::celsius();
  a.corrente = corrente::ok() ? corrente::miliamperes() : NAN;
  a.vibracao = vibracao::ok() ? vibracao::rmsG() : NAN;
  a.duty = motor::duty();
  a.alarmes = alarmes::ativos();
  a.simulados = alarmes::simulados();
  a.leds = alarmes::leds();
  a.estado = uint8_t(controle::estado());
  a.ensaio = ensaio::ativo();
  dados::publicar(a);
}

void setup() {
  // Antes de tudo: até aqui os pinos da L298N, LEDs e buzzer estão soltos (flutuando).
  motor::iniciar();    // ENA em 0, IN1/IN2 definidos: motor parado
  alarmes::iniciar();  // LEDs e buzzer desligados

  Serial.setTxBufferSize(1024);  // evita que o print segure o loop
  Serial.begin(115200);
  delay(500);  // só no boot: dá tempo do Serial Monitor conectar
  Serial.println("\n=== Monitor de motor DC ===");

  i2c::iniciar();
  i2c::escanear(Serial);

  hall::iniciar();
  temperatura::iniciar();
  // O zero do ACS712 é medido aqui, com o motor ainda parado (o duty inicial só entra no fim do setup).
  imprimirStatus("ACS712", corrente::iniciar());
  Serial.printf("ACS712 zero = %.0f mV (esperado ~2500 com Vcc = 5 V)\n", corrente::zeroMv());
  imprimirStatus("MPU6050", vibracao::iniciar());
  if (LCD_INSTALADO) imprimirStatus("LCD 16x2", display::iniciar());
  imprimirPwm();
  alarmes::relatarBloqueios();
  ensaio::iniciar();  // monta a flash e registra se o ensaio anterior foi interrompido
  controle::iniciar();
  app_server::iniciarFila();
  publicarDados();     // a rede já começa com uma medida válida
  remoto::iniciar();   // sem secrets.h, segue só com as funções locais

  imprimirAjuda();

  // Daqui em diante, se o loop travar, o ESP32 reinicia e o motor volta para 0.
  // Só o loop é vigiado (máscara 0): a tarefa de rede pode ficar presa no core 0 sem reiniciar o sistema.
  const esp_task_wdt_config_t configWdt = {WATCHDOG_MS, 0, true};
  esp_task_wdt_reconfigure(&configWdt);
  enableLoopWDT();

  // Duty inicial (config.h): liga o motor sem depender do Serial. Fica por último para os sensores já estarem prontos.
  motor::definirDuty(MOTOR_DUTY_INICIAL_PCT);
  Serial.printf("duty inicial = %.1f %%\n", motor::duty());
}

void loop() {
  const uint32_t agora = millis();

  lerSerial();
  alarmes::atualizar();
  ensaio::atualizar();
  app_server::aplicarComandos();

  if (tarefaVibracao.pronto(agora))    vibracao::amostrar();
  if (tarefaRpm.pronto(agora))         hall::atualizar();
  if (tarefaTemperatura.pronto(agora)) temperatura::atualizar();
  if (tarefaCorrente.pronto(agora))    corrente::atualizar();
  if (tarefaControle.pronto(agora))    controle::atualizar();
  if (tarefaAlarmes.pronto(agora))     alarmes::avaliar();
  if (tarefaDados.pronto(agora))       publicarDados();
  if (tarefaSerial.pronto(agora))      imprimirMedidas();
  if (LCD_INSTALADO && tarefaLcd.pronto(agora)) display::passo();
}
