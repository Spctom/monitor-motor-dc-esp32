# Placa de conexões v2 (face simples, fabricação caseira)

Placa de **94 × 80 mm** (cabe numa fenolite de 10 × 10 cm). O ESP32 DevKit V1 encaixa em barras
fêmea, e cada módulo tem a sua barra fêmea na mesma ordem de pinos do módulo, para os jumpers
irem retos. A fonte de 12 V entra por um borne de parafuso, e um módulo LM2596 gera os 5 V do
ESP32 e dos sensores.

Na própria placa ficam: borne da fonte, borne de saída para o motor, ligações do LM2596, pull-up
do Hall, filtro do LM35, capacitores de desacoplamento, LEDs com resistores e buzzer. INA219,
L298N e LM2596 continuam como módulos, ligados por fio.

Validação (KiCad 10.0.6): ERC 0 violações, DRC 0 violações (isolação mínima 0,5 mm, trilha
mínima 1,0 mm), 0 pads desconectados, placa idêntica ao esquemático.
**Ainda não foi fabricada nem testada no hardware.**

O que mudou da v1 (21/10): ESP32 DevKit V1 de 30 pinos no lugar da DevKitC de 38; fonte de 12 V
e LM2596 na placa; buzzer direto no GPIO 19 (sem transistor e sem diodo); barra da L298N sem GND
(o GND da ponte vem pelo borne do motor). A versão editada à mão em 22/10 está guardada em
`versoes_antigas/`.

## Arquivos

| Arquivo | Para que serve |
|---|---|
| `fabricacao/transferencia_termica_cobre.pdf` | Imprimir para transferência térmica (cobre, 1:1, já na orientação certa) |
| `fabricacao/montagem_componentes.pdf` | Onde vai cada componente, visto por cima |
| `fabricacao/gerber/` | Gerber + furação (fresadora ou empresa) |
| `fabricacao/render_3d.png` | Como a placa fica montada |
| `esquematico.pdf` | Esquemático (serve para o relatório) |
| `placa_conexoes.kicad_pro` | Abrir no KiCad |
| `gerar_placa.py` | Gera tudo acima. Para mudar a placa, edite e rode de novo |

## Antes de imprimir: 2 conferências

1. **Distância entre as fileiras do seu ESP32.** Meça de centro a centro de pino, de uma
   fileira à outra. A placa usa **25,4 mm**. Se a sua der outro valor, mude `ESP_DIST` em
   `gerar_placa.py` e gere de novo.
2. **Pinagem da sua DevKit V1** (antena para cima, USB para baixo).
   Fileira esquerda, de cima para baixo: EN, VP, VN, D34, D35, D32, D33, D25, D26, D27, D14,
   D12, D13, GND, **VIN**.
   Fileira direita: D23, D22, TX0, RX0, D21, D19, D18, D5, TX2, RX2, D4, D2, D15, GND, **3V3**.
   Se a serigrafia da sua placa estiver diferente, **não fabrique** sem ajustar.

## Fabricação (transferência térmica)

1. Imprima `transferencia_termica_cobre.pdf` em impressora **a laser**, em papel couché/glossy,
   com escala **100 %** ("tamanho real", sem "ajustar à página").
2. Confira com uma régua: a placa no papel deve medir **94 × 80 mm**.
3. **Não espelhe.** Os textos em cobre ("+12V", "FONTE", "MOTOR", "DPEE1107 UFSM v2") aparecem
   **espelhados no papel**: está certo. Depois de transferidos, ficam legíveis olhando o lado do cobre.
4. Placa de fenolite ou fibra, **face simples**. Lixe com palha de aço, limpe com álcool, passe o
   ferro e corroa normalmente.
5. Furação: broca de **1 mm** em quase todos os pads (o pontinho no centro guia a broca),
   **1,3 mm** nos 4 furos dos bornes de parafuso e **3 mm** nos 4 furos de fixação (M3).
6. Antes de soldar, olhe a placa contra a luz e passe o multímetro em continuidade entre trilhas
   vizinhas: nenhuma pode apitar com outra.

## Lista de materiais

- 2 barras fêmea 1×15, passo 2,54 mm (J1, J2: ESP32)
- 3 barras fêmea 1×4 (J3 INA219, J4 MPU6050, J5 OLED)
- 3 barras fêmea 1×3 (J6 LM35, J7 Hall, J8 L298N)
- 2 bornes de parafuso de 2 vias, passo 5,08 mm, tipo KF301 (J9 fonte, J10 saída para o motor)
- 1 módulo regulador step-down LM2596 (o azul, com trimpot)
- 4 pedaços de barra de pinos macho ou fio, para ligar o LM2596 (J11, J12)
- 1 resistor 10 kΩ (R1, pull-up do Hall)
- 1 resistor 1 kΩ (R2, filtro do LM35)
- 3 resistores 220 Ω (R3, R4, R5, LEDs)
- 3 capacitores cerâmicos 100 nF, marcados 104 (C1, C2, C3)
- 3 LEDs 5 mm: verde, amarelo, vermelho (D1, D2, D3)
- 1 buzzer ativo 12 mm (BZ1)
- 1 pedaço de fio para o jumper JP1 (perna cortada de resistor serve)
- 4 parafusos M3 com espaçador

Se faltar o resistor de 1 kΩ do filtro (R2), solde um fio no lugar e deixe C1 de fora.

## Montagem (componentes por cima, solda no lado do cobre)

Solde dos mais baixos para os mais altos: **JP1 primeiro**, depois R1 (fica embaixo do ESP32),
os outros resistores, capacitores, LEDs, buzzer, bornes e, por último, as barras fêmea.

- **JP1 (fio) é obrigatório.** A trilha de 3,3 V que passa por cima do ESP32 deixa o GND do
  INA219, do MPU6050 e o GND direito do ESP32 numa ilha de cobre separada. Esse fio, por cima
  da placa, liga essa ilha ao resto do GND. Sem ele, o INA219 e o MPU6050 ficam sem terra.
- **LEDs:** perna curta (lado chato) no pad **quadrado**, que é o GND.
- **Buzzer:** o "+" do buzzer no pad quadrado (marcado "+"). Ele fica ligado direto no GPIO 19:
  antes, meça a corrente dele em 3,3 V; se passar de uns 15 mA, ele força a saída do ESP32.
- **Bornes:** a abertura do fio fica virada para a borda da placa. Em cada borne, o **+ fica em
  cima**, como marcado no cobre ("+12V").
- **LM2596:** prenda o módulo com fita dupla-face dentro do retângulo e ligue 4 fios curtos:
  IN+ e IN− do módulo nos pads "IN+ IN−" (lado direito), OUT+ e OUT− nos pads "OUT+ OUT−"
  (lado esquerdo).

## Ligações externas

| Na placa | Vai para |
|---|---|
| Borne FONTE 12V | + e − da fonte de 12 V. **Confira a polaridade antes de ligar**: a placa não tem diodo de proteção |
| Borne SAIDA 12V, + | VIN+ (terminal verde) do INA219. O VIN− do INA219 vai no borne 12V da L298N |
| Borne SAIDA 12V, − | Borne GND da L298N |
| Barra L298N (ENA, IN1, IN2) | Pinos ENA, IN1, IN2 da L298N (o GND da ponte já vem pelo borne) |
| INA219 (VCC, GND, SCL, SDA) | Mesmos pinos do INA219 |
| MPU6050 (VCC, GND, SCL, SDA) | Mesmos pinos do MPU6050 (AD0 pode ficar solto) |
| OLED (**GND, VCC**, SCL, SDA) | Ordem do OLED comum. Confira o seu: alguns vêm com VCC primeiro |
| LM35 (5V, OUT, GND) | Pinos 1, 2, 3 do LM35 (face plana para você) |
| HALL (5V, GND, OUT) | Pinos 1, 2, 3 do 3144E (face escrita para você) |

## Antes de ligar pela primeira vez

1. **Ajuste o LM2596 sozinho, fora da placa:** ligue só ele nos 12 V, sem nada na saída, e gire o
   trimpot até a saída marcar **5,0 V**. Muitos vêm de fábrica com a saída perto de 12 V, e isso
   queima o ESP32. Depois, pingue esmalte no trimpot.
2. Com a placa vazia (sem ESP32 nem módulos), meça em continuidade:
   - +12V × GND, 5V × GND, 3V3 × GND e 3V3 × 5V: **nenhum pode apitar**;
   - GND do borne × pino GND de cada barra (inclusive INA219 e MPU6050): **tem que apitar**.
     Se o do INA219 ou do MPU6050 não apitar, falta o fio JP1.
3. Ligue a fonte **sem o ESP32** e meça ~5 V no pino 1 das barras LM35 e HALL.
4. Encaixe o ESP32 (antena para o lado "ANTENA"), ligue e meça ~3,3 V no VCC das barras I2C.
5. Só então ligue os módulos. O comando `t` do firmware testa os LEDs e o buzzer.

Não ligue o USB do ESP32 com a fonte de 12 V ligada ao mesmo tempo: para gravar o firmware,
desligue a fonte.

## Gerar de novo

```bash
"/Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3" gerar_placa.py
```

Depois, para conferir e exportar (troque `kicad-cli` pelo caminho completo
`/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli`):

```bash
kicad-cli sch erc placa_conexoes.kicad_sch
kicad-cli pcb drc --schematic-parity --severity-all placa_conexoes.kicad_pcb
kicad-cli pcb export pdf --mode-single --layers "B.Cu,Edge.Cuts" --black-and-white --drill-shape-opt 1 --scale 1 -o fabricacao/transferencia_termica_cobre.pdf placa_conexoes.kicad_pcb
```

Editar à mão no KiCad também funciona, mas a próxima execução do script sobrescreve. Se editar
à mão, guarde uma cópia antes de rodar o script.
