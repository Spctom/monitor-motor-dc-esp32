# Monitoramento de Motor DC e Controle via Wi-Fi — material para a apresentação

DPEE1107 – Sistemas Microcontrolados (UFSM) · Pedro Felipe Hoffmann e Lucas Garcia Tommaselli Chuba Machado
Apresentação: 25/11/2026 · Documento montado em 24/11/2026 a partir dos ensaios e testes na bancada.

Cada seção abaixo pode virar um ou dois slides. Os números vêm dos logs em `firmware/logs_ensaios/`
(ensaios 1 a 28) e dos testes feitos pelo app. Quando algo **não** foi testado no hardware, está escrito.

---

## 1. Problema e objetivo

- Motores falham aos poucos: aquecimento, consumo de corrente e vibração mudam antes da parada.
- Proposta: uma bancada didática que **mede, alarma, controla e registra** um motor DC com um ESP32.
- Objetivo geral (relatório parcial): medir rotação, corrente, temperatura e vibração; alarmar
  localmente; controlar a rotação em malha fechada com a referência vinda de um app de celular; enviar
  os dados ao Thinger.io com registro a cada minuto; continuar funcionando sem Wi-Fi.

## 2. A bancada

- Motor DC 9 V (~60 Ω medidos) com hélice de 2 pás e **2 ímãs de neodímio**, sobre **base e suporte
  impressos em 3D** (`suporte.stl`, `helice.stl`).
- Fonte de **12 V** → ponte H **L298N** (PWM no ENA) → motor.
- **ESP32 DevKit V1** alimentado por um **LM2596** (12 V → 3,25 V no pino 3V3).
- Montagem final **soldada em placa perfurada**. Também foi projetada uma PCB de face simples no
  KiCad 10 (`hardware/placa_conexoes/`, ERC e DRC limpos), que não chegou a ser fabricada.

| Função | Componente | Ligação |
|---|---|---|
| Rotação | Hall 3144E + 2 ímãs | GPIO 27, interrupção (período entre pulsos) |
| Vibração | MPU6050 | I²C 0x68 (GPIO 21/22), registradores direto, 250 amostras/s |
| Corrente | ACS712 | ADC GPIO 35 (5 V do borne da L298N) |
| Temperatura | LM35 | ADC GPIO 34 (com defeito, ver seção 7) |
| Atuação | L298N | ENA GPIO 25 (PWM 5 kHz, 10 bits), IN1/IN2 GPIO 26/33 |
| Alarme local | LEDs verde / amarelo / vermelho | GPIO 18 / 17 / 16 |
| Remoto | Wi-Fi do ESP32 | Thinger.io, app MIT App Inventor, painel web |

## 3. Arquitetura do firmware

- ~3300 linhas em 34 arquivos (`firmware/monitor_motor/`), um módulo por função (`hall`, `vibracao`,
  `corrente`, `temperatura`, `motor`, `controle`, `alarmes`, `ensaio`, `registro`, `remoto`, `app_server`...).
- **Regra central: controle e alarmes nunca esperam a rede.** Sem `delay()` no loop.
  - **Core 1** (loop): sensores, controle (50 ms), alarmes (100 ms), ensaio, log.
  - **Core 0** (tarefa FreeRTOS): Wi-Fi, Thinger.io e servidor web. Os dois lados só trocam cópias
    das medidas; comandos do celular entram por uma fila e quem mexe no motor é sempre o loop.
- Watchdog de 3 s: se o loop travar, o ESP32 reinicia (testável com o comando `testewdt`).

| Tarefa | Período |
|---|---|
| Hall | interrupção (só registra `micros()`) |
| MPU6050 | 4 ms (RMS em janela de 1 s) |
| Controle PI | 50 ms |
| Corrente, temperatura | 100 ms |
| Alarmes | 100 ms |
| Data Bucket (Thinger.io) | 60 s (médias do minuto) |

### Medições
- **RPM** pelo período entre pulsos (melhor resolução em baixa rotação que contar pulsos numa janela),
  2 pulsos por volta, filtro contra pulsos com menos de 1 ms entre si, zero após 500 ms sem pulso.
- **Vibração** = RMS de |a| − média numa janela de 1 s (tira a gravidade).
- **Corrente** pelo ACS712, com o zero medido no boot e o valor entregue sempre positivo.

### Controle de rotação
- **PI com feedforward e anti-windup**, duty limitado entre **30 % e 100 %**.
- **Pulso de partida**: 100 % por 400 ms para vencer a inércia; se o motor não girar, tenta de novo,
  até 3 vezes, e então desliga e sinaliza falha.
- A referência vem do app ou do painel e é validada (faixa aceita: 500 a 6000 RPM).

### Alarmes
| Alarme | Limite | Persistência | Ação |
|---|---|---|---|
| Temperatura | 60 °C | 1 s | desliga o motor, LED vermelho |
| Corrente | 1500 mA | 1 s | desliga o motor, LED vermelho |
| Vibração | 0,50 g | 2 s | desliga o motor, LED vermelho |
| Rotação | erro > 25 % | 3 s (ignora os 4 s após partir ou mudar a referência) | LED amarelo piscando |

- Histerese para limpar (temperatura 92 %, corrente 85 %, vibração 80 % do limite; 2 s abaixo).
- Motor normal = **LED verde**.

## 4. Comunicação

- **Rede própria "MotorDC"**: o ESP32 cria o próprio Wi-Fi. O app Android e o painel web
  (`http://motordc.com` ou `http://192.168.4.1`) funcionam **sem roteador**.
- **Internet**: o ESP32 também entra em até 3 redes com internet (na UFSM, o hotspot de um celular)
  e envia os dados ao **Thinger.io**: dispositivo `motordc`, dashboard `motordc` (6 medidores,
  alarmes, histórico RPM × referência) e Data Bucket `motorminuto` (um registro por minuto, com médias).
- **App MIT App Inventor** (`app/MotorDC.aia`): mostra as medidas e alarmes e envia a rotação
  desejada e o comando de parar.
- **Painel web**: as mesmas medidas, mais os botões **Rodar ensaio** e **Simular falha** (seção 6).

## 5. Resultados

### 5.1 Curva do motor em malha aberta (ensaio 27, hélice balanceada)

| Duty | RPM | Vibração (g) |
|---|---|---|
| 30 % | 475 | 0,007 |
| 40 % | 1375 | 0,023 |
| 50 % | 1948 | 0,063 |
| 60 % | 2013 | 0,023 |
| 70 % | 3013 | 0,040 |
| 80 % | 3714 | 0,077 |
| 90 % | 4762 | 0,115 |
| 100 % | **5199** | **0,196** |

- O motor se mantém girando a partir de **30 %**; com o pulso de partida, até **25 %** (~300 RPM).
- A partida **sem ajuda** variou entre 30 % e 60 % de um ensaio para outro (atrito estático), o que
  justifica o pulso de partida no controle.
- Repetibilidade: a 80 %, 3721 RPM no ensaio 26 e 3714 no ensaio 27.

### 5.2 Vibração antes e depois do balanceamento da hélice

| Duty | Antes (ensaios 24/25) | Depois (ensaio 26) |
|---|---|---|
| 50 % | 0,10 g | 0,031 g |
| 70 % | 0,09 g | 0,040 g |
| 80 % | 0,21–0,24 g | **0,079 g** |
| 85 % | 0,47–0,63 g (**proteção desligou o motor**) | — |

- O MPU6050 detectou o desbalanceamento e a **proteção de vibração desligou o motor de verdade no
  ensaio 24** (0,63 g > 0,50 g). Depois de balancear, o motor passou a ir até 100 % com folga.
- O balanceamento também aumentou a rotação a 80 %: de ~2900 para ~3700 RPM.

### 5.3 Controle em malha fechada (pelo app, na fonte)

| Referência | Medido | Erro |
|---|---|---|
| 800 RPM | 840 | 5 % |
| 1500 RPM | 1472 | 1,9 % |
| 3000 RPM | 2964 | 1,2 % |
| 4500 RPM | 4472 | 0,6 % |
| 600 RPM ou menos | ~535 | duty no piso de 30 % |

- **Faixa útil: ~550 a ~5200 RPM.** Abaixo de ~550 o motor não gira estável; acima de ~5200 falta
  tensão (100 % de duty).
- O alarme de rotação funcionou: com o piso antigo de 35 %, a referência de 800 RPM ficava em
  1040 RPM, e o alarme acendeu. O piso foi baixado para 30 %.
- Os ganhos do PI ainda são os da simulação (Kp = 0,005 %/RPM, Ki = 0,01 %/(RPM·s)) e já deram
  erro abaixo de 2 % de 1500 a 4500 RPM.

### 5.4 Ensaio automático com log

- Segurando o BOOT (ou pelo botão do painel), o ESP32 testa LEDs, sensores e o motor em degraus
  (subida 30→100 %, descida, baixa rotação com pulso), com as proteções ativas, e **grava tudo na
  flash** (um CSV por ensaio, os 30 últimos guardados, sobrevive a desligar e a regravar).
- Resultado nos LEDs; o log é baixado depois pelo USB (`python3 firmware/baixar_logs.py`).
- Foram 28 ensaios. Eles revelaram a vibração alta, os pulsos falsos do Hall, a falta de sinal do
  LM35 e a falta de resolução do sensor de corrente.

## 6. Recursos para a demonstração

- **Painel web**: Rodar ensaio; Simular falha (temperatura, corrente, vibração, erro de rotação,
  limpar); indicador dos LEDs que o firmware está acendendo.
- **Simulação de falhas**: a falha passa pelas mesmas regras de um alarme real (persistência, LEDs; as
  críticas desligam o motor). Aparece como **"(SIMULADO)"** no painel e no app, e no Thinger.io vai em
  um campo próprio (`simulado`), para não se confundir com falha real no histórico. Acaba sozinha
  em 30 s. Testada pela serial; **os botões do painel ainda precisam ser testados no celular.**

## 7. Problemas encontrados e como foram tratados

| Problema | Diagnóstico | Solução |
|---|---|---|
| Fonte ligada com a polaridade invertida (21/10) | Inversão na adaptação dos cabos | Conferir polaridade sempre; a placa não tem diodo de proteção |
| INA219 com leituras sem sentido | Falha depois da inversão da fonte (causa não confirmada); o de reserva também deixou de funcionar (23/11) | Trocado pelo **ACS712** (18/11) |
| ACS712 não mostra a corrente do motor | Motor de ~60 Ω puxa ≤ 200 mA: a saída muda ~4 mV, menos que o ruído (4–7 mV) | **Limitação declarada**; corrente mostrada só como indicador |
| LM35 com leituras de 14 a 315 °C | Padrão de entrada solta: sem sinal chegando ao GPIO 34 | **Limitação declarada**; fora da proteção |
| Buzzer não toca | Buzzer de 5 V acionado direto pelo GPIO (3,3 V) | Removido (precisaria de transistor e solda) |
| LCD 16x2 I²C não responde | Testado a 400, 100, 50 e 10 kHz e com SDA/SCL invertidos: módulo com defeito | Removido; exibição pelo painel e app |
| OLED | Não chegou | — |
| Motor não partia com duty baixo | Zona morta de 30–60 % | Pulso de partida (100 % por 400 ms) e piso do PI em 30 % |
| Vibração alta acima de 80 % | Hélice desbalanceada (o ensaio abortou a 85 %) | Balanceamento: 0,22 → 0,079 g a 80 % |
| LED verde não acendia com o motor girando | O painel mostrou que o firmware acendia o "verde", mas acendia o vermelho: LEDs ligados trocados | Pinos trocados no firmware (verde 18, vermelho 16) |
| MPU6050 sumiu do I²C após a troca de base | Provável mau contato | Voltou a responder depois de reacomodar a fiação; conferir antes de apresentar |
| Pulsos falsos do Hall com o motor parado | Só na fonte, quando os LEDs chaveiam (provável queda no 3V3) | **Pendente**: o filtro de 1 ms do firmware não elimina; solução seria capacitor no 3V3 (solda). Sem o buzzer há menos chaveamento |
| Leitura do ADC errada no core 3.x | `analogSetPinAttenuation()` ignorada antes da 1ª leitura | Atenuação global com `analogSetAttenuation()` |
| Ensaio sem ver nada na fonte (USB e fonte não podem ficar juntos) | — | Ensaio automático com log na flash |

## 8. Mudanças em relação ao relatório parcial

| Planejado | Entregue | Motivo |
|---|---|---|
| INA219 (corrente, tensão, potência) | ACS712 (só corrente, aproximada) | INA219 com falha |
| Fonte de 9 V | Fonte de 12 V | Compensar a queda de ~2 V da L298N |
| ESP32 DevKitC V4 | ESP32 DevKit V1 | Placa disponível; mesmos GPIOs |
| Display OLED | Nenhum display (painel web e app) | OLED não chegou e o LCD substituto não respondeu no I²C |
| LEDs + buzzer | Só LEDs | Buzzer de 5 V sem transistor |
| Controle proporcional | PI com feedforward, anti-windup e pulso de partida | P puro deixa erro em regime |
| App via Wi-Fi | App e painel numa rede própria do ESP32 ("MotorDC") | Funciona sem roteador e na UFSM |
| — | Ensaio automático com log na flash | Testar tudo na fonte, sem USB |
| — | Simulação de falhas no painel | Demonstrar alarmes de sensores que não estão funcionando |

## 9. Limitações (declarar)

- **Sem medida de temperatura e sem proteção de temperatura** (LM35 sem sinal).
- **Corrente só aproximada**: o ACS712 não enxerga a corrente pequena deste motor; não há tensão nem
  potência.
- **Sem alarme sonoro e sem display local**: a sinalização local é pelos LEDs.
- A 100 % de duty o motor recebe ~10 V, um pouco acima dos 9 V nominais.
- O app aceita 500 a 6000 RPM, mas a faixa útil é ~550 a ~5200 RPM.
- Sem diodo contra inversão de polaridade e sem resistor de pull-down no ENA.

## 10. O que ainda não foi testado no hardware (fazer antes, se der)

- [ ] Botões **Rodar ensaio** e **Simular falha** pelo celular.
- [ ] LEDs na fonte depois da troca de pinos: **verde** com o motor girando normal, **vermelho** numa falha crítica.
- [ ] Thinger.io recebendo dados **com o motor girando na fonte** (até agora: online e gravando no
      bucket com o motor parado; dashboard ao vivo confirmado).
- [ ] **Queda do Wi-Fi** com o motor girando: o controle e os alarmes têm que continuar.
- [ ] Rodar o sistema por alguns minutos com o Thinger.io ligado, para ter histórico no bucket.

## 11. Roteiro da demonstração (sugestão)

1. **Montagem**: hotspot do iPhone ligado (Maximizar Compatibilidade, tela do Acesso Pessoal aberta);
   fonte de 12 V ligada **sem USB**; Android com dados móveis desligados, na rede **MotorDC**.
2. **Mostrar o sistema parado**: app com RPM 0; dashboard do Thinger.io online.
3. **Controle**: pedir 1500 → 3000 → 4500 RPM no app; mostrar que o motor chega e se mantém (LED
   verde) e o histórico RPM × referência no Thinger.io.
4. **Limite inferior**: pedir 800 RPM (chega a ~840); explicar que abaixo de ~550 RPM o motor não
   gira estável (pedindo 600, ele fica em ~535 com o duty no piso de 30 %).
5. **Falhas simuladas** pelo painel: "Vibração alta" → motor desliga, LED vermelho, "(SIMULADO)" no
   app; depois "Erro de rotação" com o motor girando → LED amarelo piscando.
6. **Ensaio automático** pelo painel (~1 min 45 s), mostrando os degraus e o resultado nos LEDs.
7. **Parar**: botão Parar no app/painel, ou um toque no BOOT, ou desligar a fonte.

## 12. Trabalhos futuros

- Trocar o LM35 e ligar a proteção de temperatura.
- Medir a corrente com um sensor adequado a ~200 mA (INA219 novo ou shunt + amplificador).
- Buzzer com transistor; display funcionando.
- Ajustar `RPM_REF_MAX` para ~5000 e gerar o app de novo; feedforward pela curva medida.
- Fabricar a PCB (atualizando para o ACS712) e incluir diodo de proteção e pull-down no ENA.
- Análise de vibração em frequência (FFT) para identificar a causa, não só o nível.

## Linha do tempo

| Data | Marco |
|---|---|
| 14/10 | Relatório parcial |
| 21/10 | Esqueleto do firmware; primeiro teste do motor; polaridade da fonte invertida |
| 04/11 | Troca para fonte de 12 V, DevKit V1 e PCB v2 no KiCad |
| 18/11 | L298N funcionando; INA219 trocado pelo ACS712 |
| 21/11 | Placa soldada; ensaio automático com log |
| 22/11 | PI, alarmes, Thinger.io (online e gravando), app Android e rede MotorDC |
| 23/11 | LM35 dado como defeituoso; LCD substituto sem resposta |
| 24/11 | Base nova impressa; hélice balanceada; teto de 100 %; controle validado de 800 a 4500 RPM; buzzer e LCD fora; LEDs corrigidos; simulação de falhas e ensaio pelo painel |
| 25/11 | Apresentação |
