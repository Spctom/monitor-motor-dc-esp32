// Configuração central: pinos, endereços, períodos e parâmetros dos sensores.
// Mapa de pinos é proposta — confirmar na montagem e manter igual ao CLAUDE.md.
#pragma once

#include <Arduino.h>

// ---------------- Pinos ----------------
constexpr uint8_t PINO_I2C_SDA     = 21;
constexpr uint8_t PINO_I2C_SCL     = 22;
constexpr uint8_t PINO_LM35        = 34;  // precisa ser ADC1 (GPIO 32–39): ADC2 não funciona com Wi-Fi
constexpr uint8_t PINO_ACS712      = 35;  // ADC1, só entrada; OUT do módulo direto (sem divisor/filtro, decisão de 18/11)
constexpr uint8_t PINO_HALL        = 27;  // saída open-collector do 3144E, pull-up 10 kΩ para 3,3 V
constexpr uint8_t PINO_MOTOR_PWM   = 25;  // ENA da L298N (tirar o jumper)
constexpr uint8_t PINO_MOTOR_IN1   = 26;
constexpr uint8_t PINO_MOTOR_IN2   = 33;
// Na placa montada o verde ficou no 18 e o vermelho no 16 (visto pelo painel em 24/11): trocado aqui
// em vez de ressoldar. A PCB do projeto KiCad continua com verde 16 / vermelho 18.
constexpr uint8_t PINO_LED_VERDE   = 18;
constexpr uint8_t PINO_LED_AMARELO = 17;
constexpr uint8_t PINO_LED_VERMELHO = 16;
constexpr uint8_t PINO_BUZZER      = 19;
// LEDs precisam de resistor em série. Com false, o firmware não os aciona.
constexpr bool     LEDS_COM_RESISTOR = true;
// Buzzer: o TMB12A05 é de 5 V e não toca pelo GPIO (3,3 V com 220 Ω). Sem ferro de solda para
// montar o transistor, fica fora (24/11): o pino 19 fica sempre em nível baixo. Ao montar o
// transistor (D19 → 1 kΩ → base, + do buzzer no borne 5V da L298N), mudar para true.
constexpr bool     BUZZER_INSTALADO = false;

// ---------------- I2C ----------------
// LCD 16x2 com módulo I2C. O módulo tem pull-ups de 4,7 kΩ para o VCC dele (medido em 23/11):
// ligar o VCC no 3V3. Com VCC em 5 V, SDA e SCL chegariam a 5 V no ESP32 (limite: 3,6 V).
constexpr bool     LCD_INSTALADO    = false;  // 23/11: ligação conferida, mas o módulo não responde no I2C
constexpr uint32_t I2C_FREQ_HZ  = LCD_INSTALADO ? 100000 : 400000;  // o PCF8574 do LCD só garante 100 kHz
constexpr uint8_t  ENDERECO_MPU6050 = 0x68;  // AD0 em GND
constexpr uint8_t  ENDERECO_OLED    = 0x3C;  // confirmar com o scanner I2C do boot
constexpr bool     OLED_INSTALADO   = false; // mudar para true quando o display chegar

// ---------------- Períodos (ms) ----------------
constexpr uint32_t PERIODO_VIBRACAO_MS    = 4;    // amostragem do MPU6050 (250 Hz)
constexpr uint32_t PERIODO_RPM_MS         = 50;
constexpr uint32_t PERIODO_TEMPERATURA_MS = 100;
constexpr uint32_t PERIODO_CORRENTE_MS    = 100;
constexpr uint32_t PERIODO_SERIAL_MS      = 500;  // impressão no Serial Monitor/Plotter
constexpr uint32_t PERIODO_CONTROLE_MS    = 50;
constexpr uint32_t PERIODO_ALARMES_MS     = 100;
constexpr uint32_t PERIODO_DADOS_MS       = 500;  // cópia das medidas para a tarefa de rede
constexpr uint32_t PERIODO_BUCKET_MS      = 60000; // um registro por minuto no Thinger.io
constexpr uint32_t PERIODO_LCD_PASSO_MS   = 3;     // escreve no máximo um caractere por passo
constexpr uint32_t PERIODO_LCD_TELA_MS    = 500;   // atualiza os valores da tela
constexpr uint32_t PERIODO_LCD_PAGINA_MS  = 3000;  // troca entre as duas páginas

// ---------------- Hall / RPM ----------------
constexpr uint8_t  HALL_PULSOS_POR_VOLTA = 2;      // dois ímãs na hélice, mesmo polo (sul) voltado ao sensor
constexpr uint32_t HALL_PERIODO_MIN_US   = 1000;   // pulsos mais próximos que isso são ruído (equivale a 30000 RPM)
constexpr uint32_t HALL_TIMEOUT_MS       = 500;    // sem pulso por esse tempo → motor parado

// ---------------- LM35 ----------------
// LM35 com defeito e sem reserva (23/11: 14 °C em repouso com 25 °C no ambiente, 52–56 °C com os dedos).
// 24/11: sensor trocado por um novo; medida e proteção religadas (alarme acima de LIMITE_TEMPERATURA_C).
// Se o novo também ler errado, voltar LM35_NA_PROTECAO para false, senão o motor desliga sem motivo.
constexpr bool    LM35_INSTALADO  = true;
constexpr bool    LM35_NA_PROTECAO = true;
// Somado à leitura, se o sensor novo precisar de ajuste: temperatura ambiente − leitura em repouso.
constexpr float   LM35_CORRECAO_C = 0.0f;
constexpr uint8_t LM35_AMOSTRAS   = 16;     // leituras do ADC por atualização
constexpr uint8_t LM35_DESCARTES  = 4;      // leituras jogadas fora ao trocar de canal no ADC
constexpr float   LM35_MV_POR_C   = 10.0f;
constexpr float   LM35_FILTRO_ALFA = 0.2f;  // média móvel exponencial (0..1, menor = mais suave)

// ---------------- ADC ----------------
// Atenuação GLOBAL do ADC (core 3.x): vale para o LM35 e o ACS712 ao mesmo tempo.
// 11 dB lê até ~3,1 V; o ACS712 precisa dessa faixa (saída em torno de 1,7 V com o divisor).
constexpr adc_attenuation_t ADC_ATENUACAO = ADC_11db;
constexpr float ADC_SATURACAO_MV = 3000.0f;  // acima disso a leitura está no fim da faixa

// ---------------- ACS712 (sensor de corrente, 5 V) ----------------
// Versão do módulo desconhecida (chip inacessível): usa o PIOR CASO, 30 A. Se ele for de 5 ou 20 A, a corrente
// aparece maior que a real e a proteção atua mais cedo, nunca mais tarde. Valores: 5 A = 185, 20 A = 100, 30 A = 66.
constexpr float   ACS712_MV_POR_A = 66.0f;
// TESTE SEM DIVISOR (OUT direto no pino): CIMA = 0 dá fator 1,0. Com o divisor montado, voltar CIMA para 10000.
constexpr float   ACS712_DIV_R_CIMA_OHM  = 0.0f;      // OUT → pino do ESP32 (com divisor: 10000)
constexpr float   ACS712_DIV_R_BAIXO_OHM = 20000.0f;  // pino do ESP32 → GND (não pode ser 0; sem efeito se CIMA = 0)
constexpr float   ACS712_DIVISOR_FATOR = ACS712_DIV_R_BAIXO_OHM / (ACS712_DIV_R_CIMA_OHM + ACS712_DIV_R_BAIXO_OHM);
constexpr uint8_t ACS712_AMOSTRAS = 32;       // leituras do ADC por atualização
constexpr uint16_t ACS712_AMOSTRAS_ZERO = 256;  // leituras para achar o zero no boot (motor parado)
constexpr float   ACS712_FILTRO_ALFA = 0.3f;  // média móvel exponencial (0..1, menor = mais suave)
// Saída sem corrente esperada: Vcc/2 = 2500 mV (Vcc = 5 V). Fora disso o sensor está desligado ou sem 5 V.
constexpr float   ACS712_ZERO_MIN_MV = 2000.0f;
constexpr float   ACS712_ZERO_MAX_MV = 3000.0f;

// ---------------- Vibração ----------------
constexpr uint32_t VIBRACAO_JANELA_MS = 1000;  // janela do RMS

// ---------------- Motor / PWM ----------------
constexpr uint32_t MOTOR_PWM_FREQ_HZ  = 5000;  // L298N é lenta: não subir muito
constexpr uint32_t MOTOR_PWM_FREQ_MIN_HZ = 50;     // faixa aceita pelo comando "f"
constexpr uint32_t MOTOR_PWM_FREQ_MAX_HZ = 20000;
constexpr uint8_t  MOTOR_PWM_BITS     = 10;    // duty de 0 a 1023
// Teto do duty para tudo (comando d, controle, pulso de partida, ensaio). Foi 80 % enquanto a hélice
// vibrava demais (0,47 g a 85 % no ensaio 25). Depois do balanceamento (ensaio 26: 0,079 g a 80 %),
// 100 % por decisão de 24/11. Com a fonte de 12 V menos ~2 V da L298N, 100 % dá ~10 V no motor de
// 9 V: um pouco acima do nominal. A proteção de vibração (0,5 g) continua valendo.
constexpr float    MOTOR_DUTY_MAX_PCT = 100.0f;
// Duty aplicado no fim do setup(). Fica em 0: o motor só liga por comando ou pelo ensaio (botão BOOT).
constexpr float    MOTOR_DUTY_INICIAL_PCT = 0.0f;
// Se o loop travar por esse tempo, o ESP32 reinicia e o motor volta para 0.
constexpr uint32_t WATCHDOG_MS = 3000;

// ---------------- Ensaio automático e log na flash ----------------
constexpr uint8_t  PINO_BOTAO_BOOT = 0;              // botão BOOT da placa: só ler depois do boot
constexpr float    ENSAIO_DUTY_MAX_PCT = MOTOR_DUTY_MAX_PCT;
constexpr uint32_t ENSAIO_PERIODO_LOG_MS = 500;      // uma linha no log
constexpr uint32_t ENSAIO_PERIODO_SALVAR_MS = 1000;  // grava na flash: se a energia cair, perde no máximo isso
constexpr uint8_t  ENSAIO_MAX_ARQUIVOS = 30;         // passou disso, apaga o ensaio mais antigo
constexpr size_t   ENSAIO_FLASH_LIVRE_MIN = 64 * 1024;

// ---------------- Rede ----------------
// 1 = compila o Thinger.io (precisa da biblioteca "thinger.io" instalada). 0 = só o servidor do app.
#define USAR_THINGER 1
// Rede própria do ESP32: funciona sem roteador. A senha fica no secrets.h (AP_SENHA, mínimo 8 caracteres).
constexpr const char* AP_NOME = "MotorDC";
constexpr const char* NOME_LOCAL = "motordc";  // endereço http://motordc.local no Wi-Fi comum
// Procurar o roteador troca o canal do rádio e atrapalha a rede própria: tenta, descansa e tenta de novo.
constexpr uint32_t ROTEADOR_TENTATIVA_MS = 20000;
constexpr uint32_t ROTEADOR_PAUSA_MS     = 60000;
constexpr uint32_t ROTEADOR_BUSCA_MAX_MS = 10000;  // limite de uma busca de redes
constexpr uint32_t ROTEADOR_MS_POR_CANAL = 120;    // busca curta para não atrapalhar a rede MotorDC

// ---------------- Controle de rotação (TODO: ajustar com a hélice e o suporte) ----------------
constexpr float    RPM_REF_MIN = 500.0f;             // faixa aceita para a referência
constexpr float    RPM_REF_MAX = 6000.0f;
constexpr float    CONTROLE_KP = 0.005f;             // % de duty por RPM de erro
constexpr float    CONTROLE_KI = 0.01f;              // % de duty por (RPM · s) de erro
// Piso do PI. 35 % deixava o motor em ~1000 RPM e 800 RPM era inalcançável (teste do app, 24/11).
// Nos ensaios 25–27 o motor se manteve girando a 30 % (~450–500 RPM); se parar, o controle repete a partida.
constexpr float    CONTROLE_DUTY_MIN = 30.0f;
constexpr float    CONTROLE_DUTY_MAX = ENSAIO_DUTY_MAX_PCT;
constexpr float    CONTROLE_DUTY_PARTIDA = MOTOR_DUTY_MAX_PCT;  // pulso para romper a inércia
constexpr uint32_t CONTROLE_PARTIDA_MS = 400;
constexpr float    CONTROLE_RPM_GIRANDO = 100.0f;    // abaixo disso considera parado
constexpr uint32_t CONTROLE_SEM_ROTACAO_MS = 2000;   // parado por esse tempo: tenta partir de novo
constexpr uint8_t  CONTROLE_TENTATIVAS_PARTIDA = 3;  // depois disso desliga o motor
constexpr uint32_t CONTROLE_ESTAVEL_MS = 2000;       // girando por esse tempo: zera as tentativas

// ---------------- Limites de alarme (TODO: definir nos ensaios) ----------------
constexpr float LIMITE_TEMPERATURA_C = 60.0f;
constexpr float LIMITE_CORRENTE_MA   = 1500.0f;
constexpr float LIMITE_VIBRACAO_G    = 0.5f;
// Falha simulada pelo painel web (demonstração): acaba sozinha depois disso.
constexpr uint32_t SIMULACAO_MAX_MS = 30000;
// Histerese: o alarme só limpa abaixo de limite × fator.
constexpr float ALARME_HISTERESE_TEMPERATURA = 0.92f;
constexpr float ALARME_HISTERESE_CORRENTE    = 0.85f;
constexpr float ALARME_HISTERESE_VIBRACAO    = 0.80f;
constexpr uint32_t ALARME_PERSISTENCIA_MS       = 1000;  // condição precisa durar isso para disparar
constexpr uint32_t ALARME_PERSISTENCIA_VIB_MS   = 2000;  // a vibração é medida em janelas de 1 s
constexpr uint32_t ALARME_LIMPAR_MS             = 2000;  // tempo abaixo do limite para limpar
constexpr float    ALARME_ROTACAO_ERRO_PCT      = 25.0f; // erro entre referência e medida
constexpr uint32_t ALARME_ROTACAO_TRANSITORIO_MS = 4000; // ignora logo após partir ou mudar a referência
constexpr uint32_t ALARME_ROTACAO_PERSISTENCIA_MS = 3000;
