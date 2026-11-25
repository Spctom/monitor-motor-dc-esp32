#include "remoto.h"

#include <DNSServer.h>
#include <ESPmDNS.h>
#include <WiFi.h>

#include "app_server.h"
#include "config.h"
#include "controle.h"
#include "credenciais.h"
#include "dados.h"

// Include direto (sem __has_include): a IDE só acha a biblioteca seguindo um #include real.
#if USAR_THINGER
#include <ThingerESP32.h>
#endif

namespace remoto {
namespace {

volatile bool wifiOk = false;

// Estado do Thinger.io e do teste de DNS, para o /status (diagnóstico pela rede, sem USB).
volatile const char* estadoThinger = "desativado";
char textoDns[48] = "nao testado";
volatile bool testarDns = false;  // pedido pela falha de conexão; feito no laço da tarefa, não no callback
bool dnsTestado = false;          // um teste por conexão Wi-Fi (hostByName pode demorar alguns segundos)
DNSServer dns;  // na rede própria, qualquer nome (motordc.com) aponta para o ESP32

void iniciarNomeLocal() {
  static bool iniciado = false;
  if (iniciado) return;
  iniciado = MDNS.begin(NOME_LOCAL);
  if (iniciado) {
    MDNS.addService("http", "tcp", 80);
    Serial.printf("REDE: no Wi-Fi comum, painel em http://%s.local\n", NOME_LOCAL);
  }
}

volatile uint8_t motivoQueda = 0;

// Redes com internet, na ordem do secrets.h. As vazias são ignoradas.
struct Rede {
  const char* ssid;
  const char* senha;
};
const Rede REDES[] = {
    {WIFI_SSID, WIFI_SENHA},
    {WIFI_SSID_2, WIFI_SENHA_2},
    {WIFI_SSID_3, WIFI_SENHA_3},
};
constexpr uint8_t TOTAL_REDES = sizeof(REDES) / sizeof(REDES[0]);

enum class EstadoSta : uint8_t { Aguardando, Buscando, Conectando };
EstadoSta estadoSta = EstadoSta::Aguardando;
bool buscarAgora = true, avisouSemRede = false;
uint32_t marcaRoteadorMs = 0;
uint8_t redesRecusadas = 0;   // bits das redes que falharam nesta rodada de tentativas
uint8_t redesEnterprise = 0;  // bits das redes já avisadas como não suportadas
int8_t redeAtual = -1;

bool temRede() {
  for (const Rede& r : REDES) {
    if (strlen(r.ssid) > 0) return true;
  }
  return false;
}

const char* explicarMotivo(uint8_t motivo) {
  switch (motivo) {
    case 201: case 210: case 211: case 212: return "rede nao encontrada: confira o nome e se ela e 2,4 GHz";
    case 2: case 15: case 202: case 204:    return "autenticacao falhou: senha, WPA3 ou sinal fraco";
    case 203: case 205:                     return "o roteador recusou a conexao";
    case 0:                                 return "sem resposta do roteador";
    default:                                return "erro de Wi-Fi";
  }
}

const char* nomeSeguranca(wifi_auth_mode_t modo) {
  switch (modo) {
    case WIFI_AUTH_OPEN:          return "aberta";
    case WIFI_AUTH_WEP:           return "WEP";
    case WIFI_AUTH_WPA_PSK:       return "WPA";
    case WIFI_AUTH_WPA2_PSK:      return "WPA2";
    case WIFI_AUTH_WPA_WPA2_PSK:  return "WPA/WPA2";
    case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2 Enterprise (usuario e senha: nao suportada)";
    case WIFI_AUTH_WPA3_PSK:      return "WPA3";
    case WIFI_AUTH_WPA2_WPA3_PSK: return "WPA2/WPA3";
    default:                      return "outra";
  }
}

// Entre as redes à vista, escolhe a de melhor sinal que ainda não falhou nesta rodada.
// Nos avisos a rede aparece pelo número (1 a 3), sem imprimir nome nem senha.
int8_t escolherRede(int16_t encontradas, int16_t& indiceBusca) {
  int8_t melhor = -1;
  int32_t melhorSinal = INT32_MIN;
  for (int16_t i = 0; i < encontradas; i++) {
    const String nome = WiFi.SSID(i);
    for (uint8_t r = 0; r < TOTAL_REDES; r++) {
      const uint8_t bitRede = 1 << r;
      if (strlen(REDES[r].ssid) == 0 || (redesRecusadas & bitRede) || nome != REDES[r].ssid) continue;
      if (WiFi.encryptionType(i) == WIFI_AUTH_WPA2_ENTERPRISE) {
        if (!(redesEnterprise & bitRede)) {
          redesEnterprise |= bitRede;
          Serial.printf("REDE: rede %u pede usuario e senha (Enterprise): nao suportada, use o hotspot\n", r + 1);
        }
        continue;
      }
      if (WiFi.RSSI(i) > melhorSinal) {
        melhorSinal = WiFi.RSSI(i);
        melhor = r;
        indiceBusca = i;
      }
    }
  }
  return melhor;
}

void aguardarProximaBusca(uint32_t agora) {
  redesRecusadas = 0;
  estadoSta = EstadoSta::Aguardando;
  marcaRoteadorMs = agora;
}

// Tudo sem esperar: a busca roda em segundo plano para o painel seguir respondendo.
void gerenciarRoteador(uint32_t agora) {
  if (WiFi.status() == WL_CONNECTED) {
    estadoSta = EstadoSta::Aguardando;
    buscarAgora = true;  // se cair, procura de novo na hora
    redesRecusadas = 0;
    avisouSemRede = false;
    return;
  }

  switch (estadoSta) {
    case EstadoSta::Aguardando:
      // Os 500 ms dão tempo de o Wi-Fi sair da tentativa anterior antes de buscar.
      if ((buscarAgora && agora - marcaRoteadorMs >= 500) || agora - marcaRoteadorMs >= ROTEADOR_PAUSA_MS) {
        buscarAgora = false;
        WiFi.scanDelete();
        WiFi.scanNetworks(true, false, false, ROTEADOR_MS_POR_CANAL);
        estadoSta = EstadoSta::Buscando;
        marcaRoteadorMs = agora;
      }
      break;

    case EstadoSta::Buscando: {
      const int16_t encontradas = WiFi.scanComplete();
      if (encontradas == WIFI_SCAN_RUNNING && agora - marcaRoteadorMs < ROTEADOR_BUSCA_MAX_MS) break;
      if (encontradas < 0) {
        WiFi.scanDelete();
        aguardarProximaBusca(agora);
        break;
      }
      int16_t indice = -1;
      const int8_t rede = escolherRede(encontradas, indice);
      if (rede < 0) {
        WiFi.scanDelete();
        if (!avisouSemRede) {
          avisouSemRede = true;
          Serial.printf("REDE: nenhuma rede cadastrada entre as %d vistas; nova busca a cada %lu s\n", encontradas,
                        (unsigned long)(ROTEADOR_PAUSA_MS / 1000));
        }
        aguardarProximaBusca(agora);
        break;
      }
      Serial.printf("REDE: tentando rede %d (sinal %ld dBm, canal %ld, %s)\n", rede + 1, long(WiFi.RSSI(indice)),
                    long(WiFi.channel(indice)), nomeSeguranca(WiFi.encryptionType(indice)));
      WiFi.scanDelete();
      redeAtual = rede;
      motivoQueda = 0;
      WiFi.begin(REDES[rede].ssid, REDES[rede].senha);
      estadoSta = EstadoSta::Conectando;
      marcaRoteadorMs = agora;
      break;
    }

    case EstadoSta::Conectando: {
      if (agora - marcaRoteadorMs < ROTEADOR_TENTATIVA_MS) break;
      const uint8_t motivo = motivoQueda;  // o disconnect abaixo gera outro evento
      WiFi.disconnect();
      Serial.printf("REDE: nao conectou na rede %d (%s, codigo %u)\n", redeAtual + 1, explicarMotivo(motivo), motivo);
      redesRecusadas |= 1 << redeAtual;  // na próxima busca tenta outra, se houver
      estadoSta = EstadoSta::Aguardando;
      buscarAgora = true;
      marcaRoteadorMs = agora;
      break;
    }
  }
}

#if USAR_THINGER
ThingerESP32* thing = nullptr;
dados::Amostra amostraBucket;

// Medida indisponível vai como null: o campo continua no dashboard, sem inventar um valor.
void campo(pson& saida, const char* nome, float v) {
  if (isnan(v)) saida[nome].set_null();
  else saida[nome] = v;
}

void preencher(pson& saida, const dados::Amostra& a) {
  campo(saida, "rpm", a.rpm);
  campo(saida, "rpm_ref", a.rpmRef);
  campo(saida, "temperatura", a.temperatura);
  campo(saida, "corrente", a.corrente);
  campo(saida, "vibracao", a.vibracao);
  campo(saida, "duty", a.duty);
  saida["alarmes"] = int(a.alarmes);
  saida["simulado"] = int(a.simulados);  // bits de alarme que são simulação do painel
  saida["estado"] = controle::nomeEstado(controle::Estado(a.estado));
}

// Mostra no Serial só as mudanças que importam (sem imprimir nenhuma credencial).
void aoMudarEstado(ThingerClient::THINGER_STATE estado) {
  switch (estado) {
    case ThingerClient::THINGER_AUTHENTICATED:
      estadoThinger = "online";
      Serial.println("REDE: Thinger.io online");
      break;
    case ThingerClient::THINGER_AUTH_FAILED:
      estadoThinger = "credenciais recusadas";
      Serial.println("REDE: Thinger.io recusou as credenciais (confira usuario, device e credencial)");
      break;
    case ThingerClient::SOCKET_CONNECTION_ERROR:
      estadoThinger = "servidor inalcancavel";
      testarDns = true;
      Serial.println("REDE: nao consegui alcancar o servidor do Thinger.io");
      break;
    case ThingerClient::SOCKET_DISCONNECTED:
      estadoThinger = "desconectado";
      Serial.println("REDE: Thinger.io desconectado");
      break;
    default:
      break;
  }
}

void iniciarThinger() {
  if (strlen(THINGER_USUARIO) == 0 || strlen(THINGER_DEVICE_ID) == 0 || strlen(THINGER_CREDENCIAL) == 0) {
    Serial.println("REDE: Thinger.io sem credenciais no secrets.h");
    return;
  }
  estadoThinger = "conectando";
  thing = new ThingerESP32(THINGER_USUARIO, THINGER_DEVICE_ID, THINGER_CREDENCIAL);
  thing->set_state_listener(aoMudarEstado);
  (*thing)["dados"] >> [](pson& saida) { preencher(saida, dados::atual()); };
  (*thing)["minuto"] >> [](pson& saida) { preencher(saida, amostraBucket); };
}

void atenderThinger(uint32_t agora) {
  static uint32_t ultimoBucketMs = agora;
  if (agora - ultimoBucketMs >= PERIODO_BUCKET_MS) {
    ultimoBucketMs = agora;
    amostraBucket = dados::mediaDoPeriodo();  // zera a média mesmo offline: cada registro vale pelo seu minuto
    if (thing != nullptr && thing->is_connected() && strlen(THINGER_BUCKET) > 0) {
      // Com confirmação: o aviso no Serial só diz "gravado" se o servidor respondeu.
      const bool gravou = thing->write_bucket(THINGER_BUCKET, "minuto", true);
      Serial.println(gravou ? "REDE: registro gravado no Data Bucket"
                            : "REDE: falha ao gravar no Data Bucket (confira o ID e a fonte From Write Call)");
    }
  }
  if (thing != nullptr && wifiOk) thing->handle();
}

// Alguns roteadores não respondem ao DNS do ESP32 (o Mac na mesma rede resolve normal).
// Se o nome do servidor não resolver, passa a usar DNS públicos, sem mudar o IP dado pelo roteador.
void verificarDns() {
  testarDns = false;
  if (dnsTestado || !wifiOk) return;
  dnsTestado = true;
  IPAddress ip;
  if (WiFi.hostByName(THINGER_SERVER, ip) == 1) {
    snprintf(textoDns, sizeof(textoDns), "roteador ok (%s)", ip.toString().c_str());
    Serial.printf("REDE: DNS ok, %s = %s; a conexao foi bloqueada depois disso\n", THINGER_SERVER,
                  ip.toString().c_str());
    return;
  }
  WiFi.STA.dnsIP(0, IPAddress(8, 8, 8, 8));
  WiFi.STA.dnsIP(1, IPAddress(1, 1, 1, 1));
  const bool ok = WiFi.hostByName(THINGER_SERVER, ip) == 1;
  snprintf(textoDns, sizeof(textoDns), "roteador falhou; publico %s", ok ? ip.toString().c_str() : "tambem falhou");
  Serial.printf("REDE: DNS do roteador falhou; usando 8.8.8.8 e 1.1.1.1 (%s)\n", ok ? "resolveu" : "tambem falhou");
}
#endif

void tarefaRede(void*) {
  const bool usaRoteador = temRede();
  const bool usaRedePropria = strlen(AP_SENHA) >= 8;
  WiFi.mode(usaRoteador && usaRedePropria ? WIFI_AP_STA : (usaRedePropria ? WIFI_AP : WIFI_STA));
  if (usaRedePropria) {
    if (WiFi.softAP(AP_NOME, AP_SENHA)) {
      app_server::iniciar();
      dns.setErrorReplyCode(DNSReplyCode::NoError);
      dns.start(53, "*", WiFi.softAPIP());
      iniciarNomeLocal();
      Serial.printf("REDE: rede propria %s ativa, painel em http://motordc.com ou http://%s\n", AP_NOME,
                    WiFi.softAPIP().toString().c_str());
    } else {
      Serial.println("REDE: nao consegui criar a rede propria");
    }
  }
  if (usaRoteador) {
    WiFi.setAutoReconnect(false);  // quem controla as tentativas é gerenciarRoteador()
    WiFi.onEvent([](WiFiEvent_t, WiFiEventInfo_t info) { motivoQueda = info.wifi_sta_disconnected.reason; },
                 ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
  }
#if USAR_THINGER
  iniciarThinger();
#else
  Serial.println("REDE: Thinger.io desativado em config.h; so o servidor do app fica ativo");
#endif

  bool estavaConectado = false;
  for (;;) {
    if (usaRoteador) gerenciarRoteador(millis());
    wifiOk = usaRoteador && WiFi.status() == WL_CONNECTED;
    if (wifiOk && !estavaConectado) {
#if USAR_THINGER
      dnsTestado = false;  // rede nova: o DNS dela pode ser diferente
#endif
      app_server::iniciar();
      iniciarNomeLocal();
      Serial.printf("REDE: conectado na rede %d, IP %s\n", redeAtual + 1, WiFi.localIP().toString().c_str());
    } else if (!wifiOk && estavaConectado) {
      Serial.println("REDE: Wi-Fi caiu; funcoes locais continuam");
    }
    estavaConectado = wifiOk;

    if (usaRedePropria) dns.processNextRequest();
    if (wifiOk || usaRedePropria) app_server::atender();
#if USAR_THINGER
    atenderThinger(millis());
    if (testarDns) verificarDns();
#endif
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

}  // namespace

void iniciar() {
  if (!temRede() && strlen(AP_SENHA) < 8) {
    Serial.println("REDE: nenhuma rede no secrets.h; rodando so com as funcoes locais");
    return;
  }
  xTaskCreatePinnedToCore(tarefaRede, "rede", 12288, nullptr, 1, nullptr, 0);
}

bool conectado() { return wifiOk; }

const char* thinger() { return (const char*)estadoThinger; }
const char* dns() { return textoDns; }

}  // namespace remoto
