#include "app_server.h"

#include <WebServer.h>
#include <WiFi.h>

#include "alarmes.h"
#include "config.h"
#include "controle.h"
#include "dados.h"
#include "ensaio.h"
#include "motor.h"
#include "remoto.h"

namespace app_server {
namespace {

struct Comando {
  enum Tipo : uint8_t { Rpm, Parar, Ensaio, Simular } tipo;
  float rpm;      // Rpm
  uint8_t bits;   // Simular: bits alarmes::ALARME_* (0 = limpar)
};

const char PAGINA[] PROGMEM = R"html(<!doctype html><html lang="pt-BR"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Motor DC</title>
<style>body{margin:0;padding:16px;font:16px system-ui,sans-serif;background:#f4f4f2;color:#1f1f1d}h1{font-size:22px;margin:0 0 4px}h2{font-size:17px;margin:22px 0 4px}#est{margin:0 0 14px;color:#555}.g{display:grid;grid-template-columns:repeat(2,1fr);gap:8px}.c{background:#fff;border:1px solid #ddd;border-radius:8px;padding:10px}.c small{display:block;color:#666}.c b{font-size:22px;font-weight:600}#al{margin:12px 0;padding:10px;border-radius:8px;background:#e4f3e4;color:#1d5c1d}#al.x{background:#fbe3e1;color:#8f1d15}label{display:block;margin-top:10px;color:#444}input{width:100%;box-sizing:border-box;padding:10px;font-size:16px;border:1px solid #bbb;border-radius:8px}.b{display:grid;grid-template-columns:1fr 1fr;gap:8px;margin-top:12px}button{padding:14px;font-size:17px;font-weight:600;border:0;border-radius:8px;color:#fff;background:#1f6f43}#p{background:#b3261e}.s button{background:#7a4d00;font-size:15px;padding:12px}.s #sl{background:#555}#e{background:#2c4f8a;width:100%;margin-top:8px}#m{min-height:22px;color:#555}.n{color:#666;font-size:14px;margin:4px 0}#leds span{display:inline-block;margin-right:12px}#leds i{display:inline-block;width:12px;height:12px;border-radius:50%;margin-right:4px;vertical-align:-1px;background:#ccc}</style>
<h1>Motor DC</h1><p id="est">conectando...</p><div class="g"><div class="c"><small>Rotação (RPM)</small><b id="rpm">-</b></div><div class="c"><small>Referência (RPM)</small><b id="ref">-</b></div><div class="c"><small>Duty (%)</small><b id="duty">-</b></div><div class="c"><small>Corrente (mA, aprox.)</small><b id="cor">-</b></div><div class="c"><small>Vibração (g)</small><b id="vib">-</b></div><div class="c"><small>Temperatura (°C)</small><b id="tem">-</b></div></div><p id="al">sem alarmes</p>
<p class="n" id="leds">LEDs (o que o firmware acende): -</p>
<label>RPM desejado<input id="r" type="number" inputmode="numeric" value="2000"></label><div class="b"><button id="l">Ligar</button><button id="p">Parar</button></div><p id="m"></p>
<h2>Ensaio automático</h2><p class="n">O mesmo que segurar o BOOT: testa tudo e grava o log. Precisa da fonte ligada. Parar interrompe.</p><button id="e">Rodar ensaio</button>
<h2>Simular falha</h2><p class="n">Demonstração: a falha entra nas mesmas regras dos alarmes reais (temperatura, corrente e vibração desligam o motor). Acaba sozinha em 30 s.</p><div class="b s"><button data-t="temperatura">Temperatura alta</button><button data-t="corrente">Corrente alta</button><button data-t="vibracao">Vibração alta</button><button data-t="rotacao">Erro de rotação</button><button id="sl" data-t="nenhum">Limpar simulação</button></div>
<script>const $=i=>document.getElementById(i),N=["temperatura","corrente","vibração","rotação"],F=(v,c)=>v==null?"-":v.toFixed(c),L=[["verde","#2e9a4b"],["amarelo","#e0a400"],["vermelho","#d3302a"]];
async function s(){try{const d=await(await fetch("/status",{cache:"no-store"})).json();$("est").textContent="Controle: "+d.estado+(d.ensaio?" (ensaio rodando)":"");$("rpm").textContent=F(d.rpm,0);$("ref").textContent=F(d.rpm_ref,0);$("duty").textContent=F(d.duty,0);$("cor").textContent=F(d.corrente,0);$("vib").textContent=F(d.vibracao,3);$("tem").textContent=F(d.temperatura,1);$("r").min=d.rpm_min;$("r").max=d.rpm_max;const a=N.filter((_,i)=>d.alarmes&(1<<i));$("al").textContent=a.length?"ALARME: "+a.join(", ")+(d.simulado?" (SIMULADO)":""):(d.simulado?"simulação ligada, aguardando o alarme":"sem alarmes");$("al").className=a.length?"x":"";$("leds").innerHTML="LEDs (o que o firmware acende): "+L.map((l,i)=>'<span><i style="background:'+(d.leds&(1<<i)?l[1]:"#ccc")+'"></i>'+l[0]+"</span>").join("")}catch(e){$("est").textContent="sem conexão com o ESP32"}}
async function c(u){try{const d=await(await fetch(u,{cache:"no-store"})).json();$("m").textContent=d.ok?"Comando enviado":d.erro}catch(e){$("m").textContent="sem conexão com o ESP32"}s()}
$("l").onclick=()=>c("/rpm?valor="+encodeURIComponent($("r").value));$("p").onclick=()=>c("/parar");$("e").onclick=()=>c("/ensaio");document.querySelectorAll(".s button").forEach(b=>b.onclick=()=>c("/simular?tipo="+b.dataset.t));s();setInterval(s,1000)</script></html>)html";

WebServer servidor(80);
QueueHandle_t fila = nullptr;
bool iniciado = false;

void responder(int codigo, const char* json) { servidor.send(codigo, "application/json", json); }

// JSON não tem NaN: medida indisponível vira null.
void numero(char* destino, size_t tam, float v, uint8_t casas) {
  if (isnan(v)) strlcpy(destino, "null", tam);
  else snprintf(destino, tam, "%.*f", casas, v);
}

void rotaStatus() {
  const dados::Amostra a = dados::atual();
  char rpm[16], ref[16], temp[16], mA[16], vib[16], duty[16], json[480];
  numero(rpm, sizeof(rpm), a.rpm, 0);
  numero(ref, sizeof(ref), a.rpmRef, 0);
  numero(temp, sizeof(temp), a.temperatura, 1);
  numero(mA, sizeof(mA), a.corrente, 0);
  numero(vib, sizeof(vib), a.vibracao, 4);
  numero(duty, sizeof(duty), a.duty, 1);
  snprintf(json, sizeof(json),
           "{\"rpm\":%s,\"rpm_ref\":%s,\"temperatura\":%s,\"corrente\":%s,\"vibracao\":%s,\"duty\":%s,"
           "\"alarmes\":%u,\"simulado\":%u,\"leds\":%u,\"estado\":\"%s\",\"ensaio\":%s,"
           "\"rpm_min\":%.0f,\"rpm_max\":%.0f,\"wifi\":%s,\"thinger\":\"%s\",\"dns\":\"%s\"}",
           rpm, ref, temp, mA, vib, duty, a.alarmes, a.simulados, a.leds,
           controle::nomeEstado(controle::Estado(a.estado)), a.ensaio ? "true" : "false", RPM_REF_MIN, RPM_REF_MAX,
           remoto::conectado() ? "true" : "false", remoto::thinger(), remoto::dns());
  responder(200, json);
}

// Resposta simples para o App Inventor: campos separados por ";" e "--" quando não há leitura.
// rpm;referencia;duty;corrente;vibracao;temperatura;bits dos alarmes;texto dos alarmes;estado
void campoApp(char* destino, size_t tam, float v, uint8_t casas) {
  if (isnan(v)) strlcpy(destino, "--", tam);
  else snprintf(destino, tam, "%.*f", casas, v);
}

void somarAlarme(char* texto, size_t tam, const char* nome) {
  strlcat(texto, strlen(texto) > 7 ? ", " : " ", tam);  // 7 = tamanho de "ALARME:"
  strlcat(texto, nome, tam);
}

void rotaApp() {
  const dados::Amostra a = dados::atual();
  char rpm[16], ref[16], temp[16], mA[16], vib[16], duty[16], texto[96], linha[280];
  campoApp(rpm, sizeof(rpm), a.rpm, 0);
  campoApp(ref, sizeof(ref), a.rpmRef, 0);
  campoApp(duty, sizeof(duty), a.duty, 0);
  campoApp(mA, sizeof(mA), a.corrente, 0);
  campoApp(vib, sizeof(vib), a.vibracao, 3);
  campoApp(temp, sizeof(temp), a.temperatura, 1);
  if (a.alarmes == 0) {
    strlcpy(texto, "sem alarmes", sizeof(texto));
  } else {
    strlcpy(texto, "ALARME:", sizeof(texto));
    if (a.alarmes & alarmes::ALARME_TEMPERATURA) somarAlarme(texto, sizeof(texto), "temperatura");
    if (a.alarmes & alarmes::ALARME_CORRENTE) somarAlarme(texto, sizeof(texto), "corrente");
    if (a.alarmes & alarmes::ALARME_VIBRACAO) somarAlarme(texto, sizeof(texto), "vibração");
    if (a.alarmes & alarmes::ALARME_ROTACAO) somarAlarme(texto, sizeof(texto), "rotação");
    if (a.simulados) strlcat(texto, " (SIMULADO)", sizeof(texto));
  }
  snprintf(linha, sizeof(linha), "%s;%s;%s;%s;%s;%s;%u;%s;%s%s", rpm, ref, duty, mA, vib, temp, a.alarmes, texto,
           controle::nomeEstado(controle::Estado(a.estado)), a.ensaio ? " (ensaio)" : "");
  servidor.send(200, "text/plain; charset=utf-8", linha);
}

void rotaRpm() {
  const String texto = servidor.arg("valor");
  const float rpm = texto.toFloat();
  if (texto.length() == 0 || rpm < RPM_REF_MIN || rpm > RPM_REF_MAX) {
    responder(400, "{\"ok\":false,\"erro\":\"rpm fora da faixa\"}");
    return;
  }
  const dados::Amostra agora = dados::atual();
  if (agora.ensaio) {
    responder(409, "{\"ok\":false,\"erro\":\"ensaio em andamento\"}");
    return;
  }
  if (isnan(agora.corrente)) {  // o controle recusaria: avisa aqui em vez de dizer "comando enviado"
    responder(409, "{\"ok\":false,\"erro\":\"sem leitura de corrente: ligue a fonte de 12 V\"}");
    return;
  }
  const Comando c{Comando::Rpm, rpm, 0};
  if (xQueueSend(fila, &c, 0) != pdTRUE) {
    responder(503, "{\"ok\":false,\"erro\":\"fila cheia\"}");
    return;
  }
  responder(200, "{\"ok\":true}");  // quem confirma se o motor aceitou é o campo "estado" do /status
}

void rotaParar() {
  const Comando c{Comando::Parar, 0, 0};
  xQueueSend(fila, &c, 0);
  responder(200, "{\"ok\":true}");
}

void rotaEnsaio() {
  const dados::Amostra agora = dados::atual();
  if (agora.ensaio) {
    responder(409, "{\"ok\":false,\"erro\":\"ensaio ja em andamento\"}");
    return;
  }
  if (isnan(agora.corrente)) {  // sem os 5 V da fonte o ensaio aborta no repouso: avisa logo
    responder(409, "{\"ok\":false,\"erro\":\"sem leitura de corrente: ligue a fonte de 12 V\"}");
    return;
  }
  const Comando c{Comando::Ensaio, 0, 0};
  xQueueSend(fila, &c, 0);
  responder(200, "{\"ok\":true}");
}

void rotaSimular() {
  const String tipo = servidor.arg("tipo");
  uint8_t bits;
  if (tipo == "temperatura") bits = alarmes::ALARME_TEMPERATURA;
  else if (tipo == "corrente") bits = alarmes::ALARME_CORRENTE;
  else if (tipo == "vibracao") bits = alarmes::ALARME_VIBRACAO;
  else if (tipo == "rotacao") bits = alarmes::ALARME_ROTACAO;
  else if (tipo == "nenhum") bits = 0;
  else {
    responder(400, "{\"ok\":false,\"erro\":\"tipo: temperatura, corrente, vibracao, rotacao ou nenhum\"}");
    return;
  }
  if (dados::atual().ensaio) {
    responder(409, "{\"ok\":false,\"erro\":\"ensaio em andamento\"}");
    return;
  }
  const Comando c{Comando::Simular, 0, bits};
  xQueueSend(fila, &c, 0);
  responder(200, "{\"ok\":true}");
}

}  // namespace

void iniciarFila() { fila = xQueueCreate(4, sizeof(Comando)); }

void aplicarComandos() {
  if (fila == nullptr) return;
  Comando c;
  while (xQueueReceive(fila, &c, 0) == pdTRUE) {
    if (c.tipo == Comando::Parar) {
      if (ensaio::ativo()) ensaio::abortar("APP_PARAR");
      controle::parar();
      alarmes::limparParada();
      Serial.println("APP: parar");
    } else if (c.tipo == Comando::Ensaio) {
      if (!ensaio::ativo()) ensaio::comecar();
      Serial.println("APP: ensaio");
    } else if (c.tipo == Comando::Simular) {
      alarmes::simular(c.bits);
    } else if (const char* erro = controle::definirReferencia(c.rpm)) {
      Serial.printf("APP: rpm %.0f recusado (%s)\n", c.rpm, erro);
    } else {
      alarmes::limparParada();  // o alarme antigo não pode ficar aceso com o motor rodando de novo
      Serial.printf("APP: rpm = %.0f\n", c.rpm);
    }
  }
}

void iniciar() {
  if (iniciado) return;
  servidor.on("/", HTTP_GET, [] { servidor.send_P(200, "text/html; charset=utf-8", PAGINA); });
  servidor.on("/status", HTTP_GET, rotaStatus);
  servidor.on("/app", HTTP_GET, rotaApp);
  servidor.on("/rpm", HTTP_GET, rotaRpm);
  servidor.on("/parar", HTTP_GET, rotaParar);
  servidor.on("/ensaio", HTTP_GET, rotaEnsaio);
  servidor.on("/simular", HTTP_GET, rotaSimular);
  servidor.onNotFound([] {
    // Na rede própria, o celular testa a internet em endereços como /generate_204.
    // Responder com o painel faz o sistema abrir a página sozinho ao conectar.
    if (servidor.client().localIP() == WiFi.softAPIP()) {
      servidor.sendHeader("Location", String("http://") + WiFi.softAPIP().toString() + "/", true);
      servidor.send(302, "text/plain", "");
      return;
    }
    responder(404, "{\"ok\":false,\"erro\":\"rota inexistente\"}");
  });
  servidor.begin();
  iniciado = true;
}

void atender() {
  if (iniciado) servidor.handleClient();
}

}  // namespace app_server
