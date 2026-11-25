# Monitoramento de Motor DC e Controle via Wi-Fi (ESP32)

Projeto da disciplina **DPEE1107 – Sistemas Microcontrolados** (UFSM, Depto. de Processamento de Energia Elétrica).
Equipe: Pedro Felipe Hoffmann e Lucas Garcia Tommaselli Chuba Machado.
Fonte do escopo: `RELATÓRIO PARCIAL - SISTEMAS MICROCONTROLADOS.pdf` (apresentado em 14/10/2026).

Responda e comente código em **português**.

## O que o sistema faz

Bancada didática: motor DC 9 V com hélice (2 ímãs de neodímio), monitorado e controlado por um ESP32.

- **Mede**: rotação (Hall 3144E), corrente (ACS712; sem tensão/potência desde a troca de 18/11), temperatura da carcaça (LM35), vibração (MPU6050).
- **Alarma localmente**: LEDs + buzzer quando temperatura, corrente, vibração ou rotação saem dos limites.
- **Mostra localmente**: display OLED (RPM, temperatura, corrente, vibração, estado dos alarmes).
- **Controla**: rotação em malha fechada; referência (RPM desejado) vem de um app MIT App Inventor via Wi-Fi; atuação por PWM na ponte H L298N.
- **Remoto**: Thinger.io — dashboards ao vivo + Data Bucket gravando **a cada 1 minuto**: data/hora, vibração, corrente, RPM, temperatura e estado dos alarmes.
- **Sem Wi-Fi**: aquisição, controle e alarmes locais **têm que continuar funcionando**. Buffer local para reenvio é opcional.

## Estado atual (21/10/2026)

- Materiais comprados; cada sensor testado **individualmente**. Thinger.io e App Inventor já usados em outras atividades da disciplina.
- **Nada integrado ainda.** Estrutura física não montada.
- Firmware: esqueleto em `firmware/monitor_motor/` com os 4 sensores + motor em malha aberta. Compila, **ainda não testado no hardware**.
- **Cronograma atrasado** — ver seção "Cronograma".
- **22/11**: firmware ganhou **controle PI com pulso de partida, lógica dos alarmes, servidor do app e tarefa de rede** (validados em simulação no computador; **PI ainda sem ajuste no motor real**). Thinger.io escrito e compilando (biblioteca `thinger.io` 2.49.0 instalada; com credenciais o firmware ocupa 95 % da flash do app; o Serial avisa `Thinger.io online` e `registro gravado no Data Bucket`), **testado na placa em 22/11: dispositivo `motordc` online e gravação no bucket `motorminuto` confirmada pelo servidor** (com o motor parado, só no USB). Dashboard `motordc` criado (6 medidores, alarmes e histórico de RPM × referência); **atualização ao vivo confirmada em 22/11** (medidor de vibração subiu ao balançar o MPU6050). Medida sem leitura vai como null e o medidor mostra 0: hoje 0 °C e 0 mA no dashboard podem significar "sem leitura". Na bancada: ESP32, LEDs, MPU6050 (0,82 g parado) e Hall OK; buzzer não toca; LM35 com defeito; OLED não chegou; **ACS712 ainda sem variação comprovada**. Motor parte perto de 60 % e se mantém até ~30–40 % (sem hélice, pela vibração).
- **22/11 (rede)**: ESP32 conectou no Wi-Fi do `secrets.h` e respondeu `/`, `/status`, `/parar`, (testado de verdade pelo Mac; painel aberto pelo usuário; `motordc.com` confirmado no celular na rede MotorDC). A rede própria **MotorDC** sobe junto. **Falta**: testar `/rpm` com a fonte ligada. Para a apresentação, usar o hotspot do celular em 2,4 GHz como Wi-Fi com internet (a rede de casa não existe na UFSM).
- **21/11**: placa soldada. Firmware ganhou **ensaio automático com log na flash** para testar tudo na fonte, sem USB. Lógica validada em simulação no computador e compilando; **ainda não rodou na placa**.
- **18/11**: L298N testada e girando o motor (a lógica da placa estava sem 5 V e o ESP32 sem alimentação nos testes anteriores). `MOTOR_DUTY_INICIAL_PCT` em `config.h` define o duty no boot (**0 desde 21/11**: o motor só liga por comando ou pelo ensaio). Sensor de corrente trocado de INA219 para **ACS712**: firmware já compila, **hardware ainda por ligar e testar**.

### Pendências da bancada (precisam de ferro de solda; anotado em 22/11)

1. **Corrente: o INA219 de reserva não funciona mais (23/11).** Fica o ACS712 como indicador aproximado, porque o motor (~60 Ω, até ~200 mA) é pequeno demais para ele medir bem. **Validado com os ensaios 23 e 24 (24/11):** está vivo (repouso em ~2589 mV no pino) mas a 85 % a saída cai só ~4 mV, menos que o ruído de 4–7 mV: **não mede a corrente deste motor**. Cair em vez de subir sugere IP+/IP− invertidos (a proteção usa o valor absoluto, então não afeta).
2. **LM35:** saída em 0 mV com 5 V presentes. VCC passado do ESP32 para o borne 5V da L298N em 23/11 (esse 5 V não pode encostar no VIN nem no 3V3). Com 5 V, 1 kΩ em série até o D34, capacitor no D34 e descarte de leituras no ADC, ainda marcou **14 °C em repouso (ambiente de 25 °C) e 52–56 °C com os dedos**, oscilando. **Decisão de 23/11: sensor com defeito e sem reserva, desativado** (`LM35_INSTALADO = false`): app e painel mostram `--`, não há proteção de temperatura e o valor não foi estimado. Declarar isso no relatório e na apresentação. Ao trocar: `LM35_INSTALADO = true` e `LM35_NA_PROTECAO = true`; o circuito já está pronto. **24/11: medida religada para testar de novo** (`LM35_INSTALADO = true`, ainda com `LM35_NA_PROTECAO = false`), gravada no ESP32; falta o ensaio na fonte. **24/11 (noite): sensor novo instalado e proteção religada** (`LM35_NA_PROTECAO = true`, alarme a 60 °C desliga o motor), gravado no ESP32. **Validado no ensaio 29: 29,5–31,2 °C o ensaio inteiro, estável, sem picos** (o antigo pulava até 315 °C): o sensor novo funciona. Se um dia ler errado, voltar a proteção para `false`. No mesmo dia o usuário passou um ímã para a outra pá da hélice, para equilibrar.
3. **Buzzer: fora do projeto (decisão de 24/11, sem ferro de solda).** Não toca com 220 Ω (teste `t` de 24/11, em que os 3 LEDs acenderam). `BUZZER_INSTALADO = false` em `config.h`: o D19 fica sempre em nível baixo, e alarmes, teste `t`, `h` e ensaio não o acionam. O alarme é só visual (LEDs). Para voltar: montar BC547 ou 2N2222 (ligação na tabela abaixo), com o + no borne 5V da L298N (na fonte o ESP32 não tem 5 V no VIN), mudar para `true` e testar com a fonte ligada.
4. **Hall:** VCC passado para o **3V3** em 23/11 (3,33 V medidos). Contagem conferida no ensaio 23 (até 3294 RPM). Aparecem pulsos falsos quando os LEDs e o buzzer chaveiam com o motor parado (ensaios 23 e 24, até "7897 RPM" com o motor parado), **mas só na fonte**: no USB, o mesmo teste (`t` e `h 1`) não gerou nenhum pulso (24/11). Provável queda no 3V3 do LM2596 quando as saídas ligam: capacitor de 100 µF + 100 nF entre 3V3 e GND perto do ESP32 e 100 nF no VCC do Hall; afastar os fios do Hall dos LEDs/buzzer.
8. **MPU6050 (ensaios 21–23):** não responde no I²C (scan vazio) depois da remontagem na base nova. Conferir SDA (D21), SCL (D22), VCC (3V3) e GND do módulo. **24/11, depois do ensaio:** voltou a responder em 0x68 (scan pela serial, duas vezes) e a ler vibração (0,0052 g parado); se sumir de novo, é mau contato.
5. **Motor:** arrumado em 23/11; ainda sem teste girando pelo app. Hélice leve e balanceada (motor fraco).
6. **ENA:** resistor de 10 kΩ para o GND, para o motor não ligar se o ESP32 perder a alimentação.
7. **Display:** o OLED não chegou (`OLED_INSTALADO = false`). Em 23/11 apareceu um **LCD 16x2 com módulo I²C de 4 pinos**, que vai no lugar. Código em `display.*`, validado em simulação, mas **desativado** (`LCD_INSTALADO = false`). **23/11: ligado no 3V3, com a continuidade de SDA, SCL e GND conferida nos pinos do módulo, a luz acende e ele não responde no I²C (o scan só acha 0x68)**. Provável defeito no PCF8574 ou incompatibilidade com 3,3 V. O módulo tem pull-ups de ~4,7 kΩ para o VCC dele (medido em 23/11): **ligar o VCC no 3V3, nunca em 5 V com SDA/SCL no ESP32** (limite 3,6 V). Em 3,3 V o contraste pode ficar fraco. Se não aparecer texto: conversor de nível, ou tirar os 2 pull-ups do módulo e alimentar com 5 V por um 1N4148 em série (~4,3 V), com o barramento puxado só para 3,3 V. **24/11: módulo dado como com defeito e LCD fora do projeto.** Programa de teste gravado no ESP32 buscou a 400, 100, 50 e 10 kHz e com SDA/SCL invertidos (e o usuário também inverteu os fios no módulo): nenhuma resposta, com SDA e SCL em nível alto em repouso e o MPU6050 respondendo em todas as velocidades. A exibição local fica só nos LEDs; painel web, app e Thinger.io mostram as medidas. Firmware principal regravado em 24/11 (buzzer desativado), logs preservados.

## Hardware

| Componente | Interface | Observações |
|---|---|---|
| ESP32 DevKit V1 (WROOM-32, 30 pinos) | — | Lógica 3,3 V. Trocado da DevKitC V4 (38 pinos) em 04/11; os GPIOs usados são os mesmos, o firmware não muda. Na montagem real (21/11) é alimentado pelo pino **3V3**, com o LM2596 em 3,25 V: **nunca subir essa tensão** |
| Motor DC 9 V + fonte **12 V** | via L298N | Hélice com 2 ímãs. **Resistência medida em 22/11: ~60 Ω** (59–67 Ω, com saltos até 300 Ω no contato das escovas): corrente máxima de ~200 mA, pequena demais para o ACS712 medir bem. Motor fraco: a hélice precisa ser leve e balanceada. Fonte trocada de 9 V para 12 V (04/11): com a queda da L298N o motor chegaria a ~10 V em 100 %. Teto único `MOTOR_DUTY_MAX_PCT` em `config.h` (vale para `d`, controle, pulso de partida e ensaio): foi 80 % por causa da vibração e **voltou a 100 % em 24/11 depois do balanceamento**, por decisão do usuário (~10 V no motor de 9 V, um pouco acima do nominal; gravado e conferido: `d 120` aplica 100 %) |
| Ponte H L298N | PWM (ENA) + IN1/IN2 | Queda de ~2 V nos transistores → motor recebe ~7 V no máximo. Tirar o jumper do ENA para usar PWM (o GPIO vai no pino do ENA que **não** tem 5 V). Jumper 5V-EN **fica**; o borne 5V alimenta **só o ACS712** (e o LM35 quando voltar) e nunca pode encostar no VIN nem no 3V3 do ESP32 |
| ACS712 (substituiu o INA219 em 18/11) | ADC (GPIO 35) | Efeito Hall, em série no **lado alto**: fonte + → IP+ e IP− → borne 12V da L298N. **Alimentar com 5 V** (mínimo 4,5 V: com 3,3 V não serve). Saída ≈ Vcc/2 sem corrente; 185 mV/A na versão 5 A, 100 na de 20 A, 66 na de 30 A. **Versão desconhecida (chip inacessível): `ACS712_MV_POR_A = 66`, o pior caso**, para a proteção nunca atuar tarde; a corrente exibida é um indicador e pode estar até 2,8× acima da real. A saída pode chegar a 5 V; o ideal é um divisor **10 kΩ (OUT→pino) + 20 kΩ (pino→GND) + 100 nF no pino** (também filtra o PWM), mas **desde 18/11 o OUT está direto no GPIO 35, sem divisor nem filtro** (`ACS712_DIV_R_CIMA_OHM = 0` em `config.h`; ao montar o divisor, voltar para 10000). Sem divisor a leitura é segura só até ~3 A (saída ≈ 3,05 V, `ACS712_SATURADO` acende perto de 2,7 A): motor travado pode passar dos 3,6 V do pino e danificá-lo. Proteção barata: resistor de 1 kΩ em série entre OUT e o pino. Só mede corrente, não tensão nem potência. O zero é medido no boot com o motor parado. A leitura inclui o consumo da própria ponte H. O INA219 (shunt R100 medido OK em 18/11) ficou de reserva; código antigo em `firmware/versoes_antigas/corrente_ina219/` |
| MPU6050 | I²C (0x68) | Fixado rígido na estrutura, perto da carcaça. Não mudar posição/orientação entre ensaios |
| LCD 16x2 com módulo PCF8574 (no lugar do OLED, 23/11) | I²C (0x20–0x27 ou 0x38–0x3F, detectado no boot) | VCC no **3V3** (pull-ups do módulo vão para o VCC; 5 V danificaria SDA/SCL do ESP32). O PCF8574 só garante 100 kHz: com `LCD_INSTALADO = true` o barramento inteiro cai de 400 para 100 kHz |
| OLED (provável SSD1306 128x64, não chegou) | I²C (0x3C) | Confirmar modelo/endereço com um scanner I²C |
| LM35 | ADC | Alimentar com 5 V (mínimo do LM35 é 4 V). Saída 10 mV/°C. Isolado eletricamente da carcaça. **23/11: com defeito e desativado** (`LM35_INSTALADO = false`): sem medida e **sem proteção de temperatura**. Circuito pronto para a troca: VCC no borne 5V, 1 kΩ em série até o D34 e capacitor no D34 |
| Hall 3144E | Entrada digital + interrupção | Alimentar com 5 V (mínimo 4,5 V). Saída open-collector → pull-up de 10 kΩ para **3,3 V** (nunca 5 V no pino do ESP32) |
| LEDs, buzzer | GPIO | LEDs com resistor em série (`LEDS_COM_RESISTOR` em `config.h`), confirmados acendendo em 24/11. **Buzzer desativado desde 24/11** (`BUZZER_INSTALADO = false`). No boot, saída lida em nível baixo fica bloqueada. Buzzer **TMB12A05** (ativo, 5 V, até ~30 mA): hoje com **220 Ω em série** no D19, sem transistor, e **não toca** (o D19 aciona normalmente). Ligação certa: D19 → 1 kΩ → base do BC547; emissor no GND; coletor no − do buzzer; + do buzzer em 5 V; 1N4148 em paralelo com a faixa no +. **Nunca pôr 5 V no + enquanto ele estiver ligado ao D19** |
| LM2596 (módulo step-down) | — | **Montagem real (21/11): 12 V → 3,25 V → pino 3V3 do ESP32. NÃO ajustar para 5 V: queima o ESP32.** Só passar para 5 V depois de mover o OUT+ soldado do 3V3 para o VIN (precisa de ferro de solda). Entrada puxada dos 12 V **antes** do ACS712 |
| Capacitores | — | Desacoplamento no motor e na alimentação dos sensores |

**Terra comum**: na placa de conexões v2 a fonte de 12 V entra no borne da placa e sai por outro borne para o motor (+ → IP+ do ACS712, − → borne GND da L298N), com a volta do motor numa trilha larga só entre os dois bornes. A barra de controle da L298N não tem GND: a ponte recebe o terra só pelo borne (um caminho único). **Não há diodo contra inversão**: conferir a polaridade da fonte antes de ligar (já houve uma inversão). Manter fios de potência do motor longe dos fios de sinal/I²C.

**Alimentação do ESP32 (montagem real, 21/11)**: LM2596 em **3,25 V soldado no pino 3V3**. Na fonte **não existe 5 V no VIN**: o pino só fica com ~3 V, vindo de volta pelo regulador da placa, e por isso o ACS712 ligado nele leu 1,5 V em vez de 2,5 V. ACS712 e LM35 precisam de 5 V de outra origem (opção sem solda: borne 5V da L298N, só para esses dois, sem ligar esse 5 V ao VIN nem ao 3V3 do ESP32). Hall (módulo com D0), MPU6050 e OLED no 3V3. Não ligar USB e fonte ao mesmo tempo: para gravar o firmware, desligar a fonte. Divisor resistivo **não** serve para alimentar o ESP32.

### Placa de conexões (PCB)

**A montagem real NÃO usa esta PCB**: foi soldada à mão em placa perfurada, com o ESP32 soldado direto (sem barras). Não assumir barras, JP1 nem trilhas do projeto KiCad: conferir nos pinos do ESP32.

`hardware/placa_conexoes/`: placa face simples para fabricação caseira (KiCad 10), gerada por
`gerar_placa.py` — editar o script e rodar de novo, não os `.kicad_*` à mão (a versão editada à mão
em 22/10 está em `versoes_antigas/`). **v2 (04/11)**: 94 × 80 mm, ESP32 DevKit V1 em barras fêmea,
barras para cada módulo na ordem dos pinos do módulo, bornes de 12 V (fonte e saída para o motor),
ligações do LM2596, LEDs/buzzer/pull-up/filtros na placa. Tem um **jumper de fio JP1 obrigatório**
(liga ao resto do plano a ilha de GND do INA219/MPU6050 cercada pela trilha de 3,3 V).
ERC e DRC limpos, **ainda não fabricada**. **Desatualizada desde 18/11**: ainda prevê o módulo INA219; com o
ACS712 faltam o módulo (5 V, GND, OUT), o divisor 10 kΩ/20 kΩ + 100 nF para o GPIO 35 e o terminal IP+/IP− em série
com o motor. Revisar o script antes de fabricar. Usa a pinagem abaixo; se mudar um pino aqui, mudar
também no script. Detalhes, lista de materiais e passo a passo em `hardware/placa_conexoes/LEIAME.md`.

Armadilhas do script (KiCad 10): não usar `fp.Remove()` pelo Python (corrompe a memória); carregar
todos os footprints antes de montar a placa; preencher a zona só depois de salvar e recarregar.

### Mapa de pinos (proposta — confirmar na montagem e atualizar aqui)

| Função | GPIO | Motivo |
|---|---|---|
| I²C SDA / SCL | 21 / 22 | Padrão do ESP32 Arduino. Endereços 0x68 e 0x3C não conflitam |
| LM35 | 34 | **Tem que ser ADC1** (GPIO 32–39). ADC2 não funciona com Wi-Fi ligado |
| ACS712 (corrente) | 35 | ADC1, só entrada. OUT do módulo direto (hoje) ou por divisor 10 kΩ/20 kΩ + 100 nF |
| Hall 3144E | 27 | Interrupção na borda de descida |
| L298N ENA (PWM) | 25 | Canal LEDC |
| L298N IN1 / IN2 | 26 / 33 | Sentido fixo: IN1=HIGH, IN2=LOW |
| LED verde / amarelo / vermelho | **18 / 17 / 16** | Na placa montada verde e vermelho ficaram invertidos em relação ao projeto (16/17/18); trocado no `config.h` em 24/11. A PCB do KiCad continua 16/17/18 |
| Buzzer | 19 | |

Evitar: GPIO 6–11 (flash), 0/2/12/15 (strapping), 34–39 como saída (só entrada, sem pull-up interno).

### Armadilhas conhecidas

- **3144E é unipolar**: só detecta o polo **sul**. Os dois ímãs precisam estar com o **mesmo polo voltado para o sensor**, senão só um pulso por volta é contado e o RPM sai pela metade.
- Hélice + ímãs desbalanceados geram vibração que mascara a medição — balancear antes de levantar o "normal" de referência.
- ADC do ESP32 é não linear: usar `analogReadMilliVolts()` (já calibrado) e média de várias amostras.
- Core 3.x: `analogSetPinAttenuation()` antes da primeira leitura é **ignorada** e a calibração do `analogReadMilliVolts()` usa a atenuação **global**. Usar `analogSetAttenuation()`. Como é global, LM35 e ACS712 usam a **mesma** atenuação (11 dB, `ADC_ATENUACAO` em `config.h`); o LM35 ficou um pouco menos preciso que com 0 dB.
- ACS712: com a saída ≈ Vcc/2 o pino do ESP32 chega a ~1,7 V sem corrente (com o divisor). Sem o divisor, o limite de 3,6 V no pino é ultrapassado em sobrecorrente e a leitura satura perto de 2,7 A (versão 5 A). Alimentado com 3,3 V a saída fica errada: precisa de 5 V.
- Durante reset/upload os GPIOs ficam soltos: manter a chave dos 9 V desligada ao gravar o firmware (ou pull-down de 10 kΩ no ENA).
- Ruído do motor/PWM pode gerar pulsos falsos no Hall: capacitor na alimentação do sensor e descartar pulsos mais próximos que o mínimo físico possível.

## Software

### Ferramentas

- **Arduino IDE** com core **esp32 3.3.8**, placa "ESP32 Dev Module" (FQBN `esp32:esp32:esp32`). O código também tem `#if` para o core 2.x no PWM.
- Compilar pela linha de comando (CLI que vem dentro do Arduino IDE):
  ```
  "/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli" compile --fqbn esp32:esp32:esp32 --warnings all firmware/monitor_motor
  ```
  Manter compilando **sem avisos** com `--warnings all`.
- MPU6050 é lido **direto pelos registradores** (sem biblioteca); o ACS712 pelo ADC, com média de 32 leituras + filtro RC no hardware por causa do PWM. Até agora nenhuma biblioteca externa é necessária. Instalada: `thinger.io` 2.49.0. Próximas: `Adafruit_SSD1306` + `Adafruit_GFX` (OLED); `WiFi` e `WebServer` já vêm no core.
- Credenciais (SSID, senha, usuário/device/token do Thinger) ficam em `secrets.h`, **fora do controle de versão** (já no `.gitignore`). Modelo em `secrets.example.h`.

### Arquitetura

Sketch em `firmware/monitor_motor/` (arquivos soltos na raiz da pasta para aparecerem como abas no Arduino IDE). Cada módulo é um `namespace` com `iniciar()` e `atualizar()`/`amostrar()`; um sensor que falha no boot não trava o resto.

| Arquivo | Conteúdo |
|---|---|
| `monitor_motor.ino` | `setup`/`loop`, agendamento, comandos e telemetria pelo Serial |
| `config.h` | Pinos, endereços, períodos, parâmetros, limites de alarme |
| `periodico.h` | `Periodico`: roda uma tarefa a cada N ms sem `delay()` |
| `barramento_i2c.*` | Leitura/escrita de registradores e scanner I²C |
| `hall.*` | RPM por período entre pulsos (interrupção) |
| `temperatura.*` | LM35 (ADC 11 dB, média + filtro) |
| `corrente.*` | ACS712 (ADC): corrente média, zero medido no boot com o motor parado |
| `vibracao.*` | MPU6050: desvio padrão de \|a\| em janela de 1 s |
| `motor.*` | L298N: PWM via LEDC, sentido fixo. Modo rápido (PWM no ENA) ou lento (ENA=1, PWM invertido no IN2, freia no tempo desligado); frequência e modo trocáveis em funcionamento |
| `alarmes.*` | LEDs + buzzer e lógica com histerese e persistência. Temperatura, corrente ou vibração altas desligam o motor; erro de rotação só avisa |
| `controle.*` | PI com feedforward, anti-windup, pulso de partida e até 3 tentativas se o motor não girar |
| `dados.*` | Cópia das medidas entre o loop (core 1) e a rede (core 0) + médias do minuto |
| `display.*` | LCD 16x2 I²C sem biblioteca: duas páginas (RPM/duty/estado e corrente/temperatura/vibração), alarme na 2ª linha, `W` = Wi-Fi com internet. Escreve um caractere por passo para não atrasar o controle e se desativa sozinho se parar de responder |
| `remoto.*` | Tarefa de rede no core 0: Wi-Fi e Thinger.io (`USAR_THINGER` em `config.h`; com 1, precisa da biblioteca `thinger.io`) |
| `app_server.*` | Painel web em `/` e `GET /status`, `/app`, `/rpm?valor=N`, `/parar` (também interrompe o ensaio), **`/ensaio`** (o mesmo que segurar o BOOT) e **`/simular?tipo=temperatura|corrente|vibracao|rotacao|nenhum`** (24/11; o painel tem botões para os dois e mostra os LEDs que o firmware está acendendo), **sem senha nas rotas** (decisão de 22/11: quem está na mesma rede comanda o motor); os comandos passam por fila até o loop |
| `credenciais.h` | Usa o `secrets.h` se existir; sem ele o firmware roda só local |
| `registro.*` | Log na flash (LittleFS): um `/eNNNN.csv` por ensaio, guarda os 30 últimos, sobrevive a desligar e a regravar o firmware |
| `ensaio.*` | Ensaio automático: testa todos os componentes, degraus de duty até 85 % (subida sem ajuda, para medir a partida natural), depois **degraus baixos de 25, 20 e 15 % com o pulso de partida do controle** (85 % por 400 ms, cada um começando do motor parado; 24/11), limites de proteção, resumo no fim do log com `DUTY_MINIMO com pulso de partida` |

**App do celular (22/11)**: `app/MotorDC.aia`, gerado por `app/gerar_aia.py` (editar o script e rodar de novo). Lê `GET /app`, uma linha separada por `;` (`rpm;ref;duty;corrente;vibracao;temperatura;bits;texto dos alarmes;estado`, com `--` sem leitura). Para a apresentação a equipe conseguiu um **Android**: instalar o `.apk` gerado em Build → Android App no site do App Inventor (o servidor deles compila; não dá para gerar aqui). Endereço padrão do app: `192.168.4.1`, na rede MotorDC, com os dados móveis desligados. Importado no App Inventor (114 blocos, 0 avisos, 0 erros) e `.apk` gerado em 22/11 (o QR code vale 2 horas; depois, gerar de novo em Build); **funcionando no Android (confirmado pelo usuário em 23/11)**; comando de rotação com o motor girando ainda sem teste. A rota `/app` está gravada e respondeu certo pelo Mac, junto com `/parar` e as recusas de `/rpm` (fora da faixa e fonte desligada). Em 23/11 o MPU6050 voltou a ler vibração (0,0024 g parado). Se ficar sem leitura de novo, suspeitar de mau contato no VCC/GND do módulo (ele perde energia e volta lendo zero).

Ainda falta: ajustar o PI no motor real. LCD e buzzer ficaram fora (24/11).

Comandos no Serial Monitor (115200, "Nova linha"): `r <rpm>` liga o controle, `a` mostra alarmes e estado, `d <0-100>` duty % em malha aberta, `s` para, `f <Hz>` frequência do PWM, `m` alterna modo rápido/lento do PWM, `i` scan I²C, `p` pulsos do Hall, `t` teste dos LEDs e do buzzer, `x <t|c|v|r|0>` simula falha (igual ao painel), `a` também mostra os bits simulados e os LEDs acesos, `h <1|0>` mantém LEDs e buzzer ligados para medir (desliga sozinho em 5 min), `testewdt` trava o loop para provar o watchdog (3 s), `e` roda o ensaio, `l` lista os logs, `b [n]` mostra log, `apagar` apaga os logs, `?` ajuda. A telemetria sai como `nome:valor,...` a cada 500 ms e abre direto no Serial Plotter.

**Ensaio sem USB**: gravar o firmware com a fonte desligada → tirar o USB → ligar a fonte → LED verde piscando → **segurar BOOT 1 s** → ~1 min 45 s (toque no BOOT para tudo). Resultado nos LEDs (verde = OK, amarelo = aviso, vermelho = falha, vermelho piscando = abortado). Para ler: desligar a fonte, ligar o USB, fechar o Serial Monitor e rodar `python3 firmware/baixar_logs.py` (salva em `firmware/logs_ensaios/`). Se o ESP32 reiniciar no meio (ex.: queda de tensão ao ligar o motor), o motivo fica anotado no log.

Regra central do relatório: **controle e alarmes nunca podem esperar pela rede ou pelo display.** Sem `delay()` no loop principal.

- Thinger.io: `thing.handle()` pode travar por segundos tentando reconectar quando o Wi-Fi cai. Rodar a parte de rede numa **task FreeRTOS no core 0**; aquisição, controle e alarmes no core 1 (loop do Arduino ou task própria). Compartilhar dados entre tasks com mutex ou cópia atômica de uma struct.
- Períodos sugeridos (ajustar nos testes):

| Tarefa | Período |
|---|---|
| Hall | interrupção (ISR curta: só registra `micros()`) |
| Amostragem MPU6050 | 2–5 ms |
| Controle de velocidade | 50 ms |
| LM35 / ACS712 (com média) | 100 ms |
| Verificação de alarmes | 100 ms |
| OLED | 500 ms |
| Registro no Data Bucket | 60 s (média do intervalo para as medidas, estado instantâneo para alarmes) |

### Medições

- **RPM**: medir o **período entre pulsos** (`micros()` na ISR) em vez de contar pulsos numa janela — dá resolução muito melhor em baixa rotação. `RPM = 60e6 / (periodo_us * 2)` com 2 ímãs. Se não houver pulso por ~500 ms, RPM = 0. Fazer média dos últimos N períodos.
- **Temperatura**: `T(°C) = mV / 10`, média de várias leituras.
- **Corrente**: ACS712 de versão desconhecida, calculado como 30 A (66 mV/A), `I = (V_saída − V_zero) / 0,066`, **entregue em módulo (sempre positiva) desde 24/11**: `corrente::miliamperes()` tira o módulo só depois da média e do filtro, para o ruído em volta do zero não virar corrente; `miliamperesComSinal()` fica para diagnóstico. Zero medido no boot (inclui o consumo parado da L298N, então o valor é a corrente do motor acima disso). Resolução esperada de algumas dezenas de mA; tensão e potência não são medidas (se precisar, estimar pela tensão da fonte ou ler os 12 V por um divisor em outro pino ADC1).
- **Vibração**: aceleração resultante `|a| = sqrt(ax²+ay²+az²)`; indicador = RMS de `|a| - média` (tira a gravidade) numa janela de ~1 s. Configurar o filtro passa-baixa interno (DLPF) e I²C em 400 kHz. Levantar o valor de referência com o motor em operação normal, em algumas rotações.

### Controle de velocidade

- O relatório prevê começar com controle **proporcional**. P puro deixa erro em regime; o passo natural é **PI** com anti-windup (saturar o PWM entre o mínimo que faz o motor girar e 100%).
- **Zona morta medida (21/10, fonte 9 V direto na L298N, sem INA219, PWM 5 kHz modo rápido, com hélice):** o motor só parte com duty ≥ 55 % e abaixo de ~50 % não se mantém girando. Testar `f` (frequências menores) e `m` (modo lento) para reduzir; registrar aqui o resultado e o duty mínimo final, que vira o limite inferior do PI.
- **Ensaio 23 (firmware de 23/11 11:46, baixado em 24/11, sem relógio no log):** parte com **30 %** e se mantém girando até 30 % na descida. RPM médio por degrau, subindo: 769 (30 %), 1151 (40 %), 1667 (50 %), 1848 (60 %), 2475 (70 %), 2684 (80 %), 3294 (85 %); descendo: 2272 (70 %), 1960 (60 %), 1887 (50 %), 1303 (40 %), 435 (30 %). Usar como curva de feedforward e limite inferior do PI.
- **Ensaio 24 (24/11, base nova, MPU6050 de volta):** **abortado por vibração alta a 85 %** (0,63 g, picos de |a| de 3,4 g; limite 0,50 g) — a proteção funcionou. Vibração média por degrau: 0,024 g (30 %), 0,039 (40 %), 0,10 (50 %), 0,069 (60 %), 0,092 (70 %), **0,24 g (80 %)**. RPM repetiu o ensaio 23: 870 (30 %), 1276 (40 %), 1600 (50 %), 1845 (60 %), 2278 (70 %), 2883 (80 %). A vibração dispara acima de 80 %: hélice desbalanceada, algo solto na base nova ou ressonância. Os 2 pulsos e o pico de 0,069 g "parado" foram em t = 8–11 s, no repouso: provavelmente mão na bancada.
- **Ensaio 25 (24/11, com degraus baixos e pulso de partida):** completo, sem abortar. **Partida sem ajuda subiu para 60 %** (nos ensaios 23 e 24 tinha sido 30 %): a partida natural varia muito, e por isso o controle usa o pulso. Na descida manteve a 30 % (~500 RPM, ainda caindo). **O pulso (85 % por 400 ms) faz o motor partir sempre** (pico de ~2500 RPM), mas a 25, 20 e 15 % ele perde rotação e para em ~3 s: **o mínimo que se mantém é ~30 %**, e `CONTROLE_DUTY_MIN = 35` tem margem. Vibração: 0,21 g a 80 %, **0,47 g a 85 %** (limite 0,50; no ensaio 24 passou e abortou). Logo depois de cada pulso a vibração mede 0,5–1,3 g por uma janela de 1 s: não dispara o alarme do controle, que exige 2 s. O resumo do log diz "nao girou em nenhum degrau baixo" porque a média é tirada no fim do degrau, quando o motor já parou.
- **Ensaio 26 (24/11, hélice balanceada, ímã passado para a outra pá):** vibração caiu para um terço: 0,031 g (50 %), 0,040 (70 %), **0,079 g a 80 %** (antes 0,21–0,24). RPM a 80 %: **3721** (antes ~2900). Partida sem ajuda 40 %; mantém a 30 %; com pulso, 25 % e abaixo ainda param. **LM35 religado leu de 14 a 315 °C**: padrão de entrada solta (o D34 mostra a sobra do ACS712 no capacitor do ADC), não de sensor medindo. Usuário decidiu deixar a medida ligada (só leitura, fora da proteção). Depois disso o teto subiu para 100 % e o ensaio ganhou os degraus de 80 e 90 % antes do máximo.
- **Ensaio 27 (24/11, teto 100 %, hélice balanceada):** completo, sem abortar. RPM e vibração por degrau: 30 % 475 RPM/0,007 g; 40 % 1375/0,023; 50 % 1948/0,063; 60 % 2013/0,023; 70 % 3013/0,040; 80 % 3714/0,077; **90 % 4762/0,115 g; 100 % 5199 RPM/0,196 g** (limite 0,50 g). Partida sem ajuda e mínimo girando: 30 %. **Com o pulso de 100 % por 400 ms o motor se mantém a 25 %** (~300 RPM); 20 % ainda para. LM35: 14,2 °C na maior parte do tempo, com picos até 235 °C (sem sinal, como no ensaio 26).
- **Ensaio 29 (24/11 à noite, LM35 novo com proteção):** completo, resultado geral AVISO (só o ACS712, que não mede este motor). RPM: 30 % 1127; 40 % 1283; 50 % 1897; 60 % 2253; 70 % 2783; 80 % 3733; 90 % 4774; **100 % 5204 RPM / 0,246 g**. Partida sem ajuda e mínimo girando: 30 %; com pulso, 25 %. Temperatura 29,5–31,2 °C (motor não aqueceu no ensaio curto).
- **Teste do controle pelo app (24/11, na fonte, PI com os ganhos de simulação):** referência → RPM medido: 1500 → 1472, 3000 → 2964, 4500 → 4472 (erro ≤ 2 %); **800 → 1040 e alarme de rotação**, porque o piso do PI (35 %) dava ~1000 RPM. Piso baixado para **30 %** (`CONTROLE_DUTY_MIN`) e gravado. Repetido na fonte: **800 → 840 RPM sem alarme**; de 600 para baixo o PI fica no piso de 30 % e o motor para em **~535 RPM**. **Faixa útil do controle: ~550 a ~5200 RPM** (o app aceita 500 a 6000). **LED verde não acendeu na fonte durante o controle** (só o amarelo, no alarme), mas acende no USB (`t`, `h 1`). Suspeitas: bloqueio no boot pela fonte ou LED verde de tensão alta (~3 V) com o ESP32 em 3,25 V. O log do ensaio agora tem a linha `SAIDAS bloqueadas no boot` para separar os dois casos. **Ensaio 28: nenhuma bloqueada, e o verde acende no ensaio na fonte (motor parado).** **Causa achada pelo painel (24/11): verde e vermelho estavam ligados trocados** (verde no GPIO 18, vermelho no 16). Corrigido no `config.h`; a hipótese de tensão do LED verde foi descartada.
- **Teto (24/11):** foi 80 % por vibração (0,47 g a 85 % antes do balanceamento) e voltou a 100 %. A rotação máxima medida a 80 % é ~3700 RPM, mas `RPM_REF_MAX` ainda é 6000 (o app também diz "500 a 6000"): referência acima de ~2800 deixa o PI no teto e acende o alarme de rotação. O feedforward linear usa essa faixa e fica longe da curva medida (2000 RPM pede ~62 %, o linear dá ~47 %). Baixar `RPM_REF_MAX` para ~2800 exige regerar o `.aia` e o `.apk`.
- Ajuda muito um feedforward: levantar a curva RPM × duty cycle em malha aberta e usar como ponto de partida do PWM.
- Validar a referência recebida (limites mínimo/máximo de RPM) antes de usar.
- PWM via LEDC (ex.: 5 kHz, 8–10 bits). A L298N é lenta; não usar frequência alta.

### Alarmes

- Limites em `config.h`, definidos a partir dos ensaios.
- Usar **histerese** e **tempo de persistência** (ex.: condição precisa durar >1 s) para não disparar com ruído.
- Alarme de rotação: erro entre referência e medida acima de X% por mais de N s, **ignorado durante o transitório** logo após mudar a referência.
- Estado dos alarmes como bitmask/struct: vai para LEDs, buzzer, OLED e Thinger.
- **Simulação de falhas (24/11, para a apresentação):** `alarmes::simular(bits)` faz a falha entrar nas mesmas regras dos alarmes reais (persistência, LEDs; temperatura, corrente e vibração desligam o motor). Acaba sozinha em `SIMULACAO_MAX_MS` (30 s) ou com `nenhum`/`x 0`; não vale durante o ensaio. Marcada como "(SIMULADO)" no painel e no `/app`, e no Thinger vai no campo `simulado` (bits). Testada pela serial em 24/11: temperatura → vermelho; rotação → amarelo piscando; corrente com o motor a 50 % → motor desligado + vermelho. Ao limpar, o alarme leva ~2 s para sumir (histerese).

### Comunicação

- **Rede própria (22/11)**: o ESP32 cria o Wi-Fi **MotorDC** (senha `AP_SENHA` no `secrets.h`), com painel em `http://motordc.com` ou `http://192.168.4.1` (o próprio ESP32 responde esse nome, só dentro da MotorDC; com dados móveis ligados o celular pode ir para a internet). No Wi-Fi comum: `http://motordc.local`. Funciona sem roteador e permite comandar o motor com a fonte ligada, quando o USB não pode estar conectado.
- **Várias redes (23/11)**: o `secrets.h` aceita até 3 Wi-Fi com internet (`WIFI_SSID`/`WIFI_SENHA`, `_2`, `_3`). O ESP32 busca em segundo plano e conecta na de melhor sinal; no Serial a rede aparece só pelo número. Precisa ser 2,4 GHz com senha simples: rede com usuário e senha (Enterprise, como eduroam) é recusada. **Testado na placa em 23/11: entrou no hotspot do iPhone (rede 3, canal 6), ficou online no Thinger.io em ~10 s e gravou no bucket.** O iPhone precisa estar com Maximizar Compatibilidade ligado e a tela de Acesso Pessoal aberta para o ESP32 achar a rede. O nome no `secrets.h` tem que ser idêntico ao da rede. Plano para a UFSM: hotspot do iPhone como rede com internet e o Android na rede MotorDC com o app. **25/11: a rede 2 do `secrets.h` bloqueia o Thinger.io**: o ESP32 conecta e resolve o DNS, mas a negociação TLS na porta 25202 é derrubada (o Mac na mesma rede sofre o mesmo; a 443 passa). No hotspot do iPhone (rede 3), no mesmo dia, ficou online e gravou no bucket em segundos. O firmware agora mostra `wifi`, `thinger` e `dns` no `/status` e troca para os DNS 8.8.8.8/1.1.1.1 se o DNS do roteador falhar. Se o hotspot falhar na UFSM, a saída é o Thinger sem TLS (porta 25200, credencial sem criptografia: decisão do usuário).
- **App → ESP32**: o relatório diz conexão Wi-Fi direta. Opção padrão: `WebServer` no ESP32 com rota tipo `GET /rpm?valor=1500`, chamada pelo componente Web do App Inventor (celular na mesma rede; mostrar o IP do ESP32 no OLED). Alternativa, se a rede da universidade bloquear acesso entre dispositivos: o app escreve num recurso de entrada do Thinger.io pela API REST.
- **ESP32 → Thinger.io**: um recurso de saída com todas as variáveis (`rpm`, `rpm_ref`, `temperatura`, `corrente`, `vibracao`, `alarmes`). O Data Bucket recebe a cada 60 s pelo firmware (`write_bucket`) com as médias do minuto; o timestamp vem do próprio Thinger.

## Cronograma

Encontros às quartas. Entrega e apresentação final: **25/11/2026**.

Plano original (relatório parcial):

| Data | Atividade |
|---|---|
| 14/10 | Relatório parcial; definir estrutura física |
| 21/10 | Fixar motor, definir posição dos sensores, suportes |
| 28/10 | Terminar montagem, instalar sensores, ligações elétricas |
| 04/11 | Código inicial: leitura de todos os sensores juntos |
| 11/11 | Corrigir falhas, validar leituras, monitoramento e alarmes locais |
| 18/11 | Thinger.io + app + controle de velocidade + registro |
| 25/11 | Testes finais e apresentação |

### Plano de recuperação

O original é sequencial demais (software só começa depois da montagem) e deixa Wi-Fi, app e controle todos para a última semana. Para recuperar: **escrever o firmware em paralelo com a montagem**, testando cada módulo na protoboard.

| Semana | Hardware | Software |
|---|---|---|
| 21–27/10 | Estrutura, fixar motor, suportes, balancear hélice | Esqueleto do projeto, `config.h`, módulos de cada sensor testados um a um na protoboard |
| 28/10–03/11 | Instalar sensores e fazer as ligações definitivas | Todos os sensores juntos + OLED; curva RPM × PWM em malha aberta |
| 04–10/11 | Ajustes de fixação e ruído | Controle PI, alarmes locais, levantar valores de referência e limites |
| 11–17/11 | — | Thinger.io (dashboard + bucket), app do celular, perda de Wi-Fi |
| 18–24/11 | — | Testes integrados, coletar histórico para o relatório final, ajustes |
| 25/11 | Apresentação | |

Coletar dados no bucket **o quanto antes** — o relatório final precisa de histórico de ensaios para discutir.

Atualize esta seção quando uma etapa for concluída ou o plano mudar.
