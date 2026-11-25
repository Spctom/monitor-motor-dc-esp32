#!/usr/bin/env python3
"""Gera o projeto KiCad da placa de conexões (esquemático + PCB face simples).

Rodar com o Python que vem dentro do KiCad (é o único que tem o módulo pcbnew):

  "/Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3" gerar_placa.py

Esquemático e placa saem da MESMA tabela (COMPONENTES + TRILHAS), então não ficam
diferentes um do outro. Para mudar a placa, edite aqui e rode de novo — não edite os
arquivos .kicad_* à mão (seriam sobrescritos).

Placa feita para fabricação caseira: só cobre embaixo (B.Cu), componentes PTH em cima,
trilhas largas, isolação >= 0,5 mm, pads maiores e plano de GND.

v1 (21/10): ESP32 DevKitC V4 (38 pinos), buzzer com BC547, sem fonte na placa.
v2 (04/11): ESP32 DevKit V1 (30 pinos), borne de 12 V, LM2596 (5 V), buzzer direto no GPIO.
"""

import os
import uuid

import pcbnew

AQUI = os.path.dirname(os.path.abspath(__file__))
NOME = "placa_conexoes"
KICAD = "/Applications/KiCad/KiCad.app/Contents/SharedSupport"

# ======================= Parâmetros (mm) =======================
# Placa v2 (04/11): ESP32 DevKit V1 (30 pinos), fonte 12 V por borne, LM2596 gerando 5 V,
# buzzer direto no GPIO (sem transistor nem diodo).
ESP_X = 34.0        # fileira esquerda do ESP32 (lado do EN/VIN)
ESP_DIST = 25.4     # distância entre as fileiras: MEDIR NA SUA PLACA antes de imprimir
ESP_Y1 = 13.0       # pino 1 das fileiras (lado da antena)
ESP_PINOS = 15
D = ESP_X + ESP_DIST  # fileira direita (lado do D23/3V3)

LARGURA = 94.0
ALTURA = 80.0
ORIGEM = (40.0, 40.0)  # posição da placa na folha do KiCad

TRILHA = 1.2        # largura das trilhas de sinal
TRILHA_POT = 2.0    # 12 V do motor
TRILHA_GND_POT = 2.5  # volta do motor entre os bornes
ISOLACAO = 0.5      # distância mínima entre cobres de nets diferentes
PAD_BARRA = 1.9     # pads das barras e de peças com passo 2,54 mm
PAD_OUTROS = 2.3    # pads de resistores, capacitores, buzzer


def P(n):
    """Coordenada y do pino n (1..15) das fileiras do ESP32 (aceita 0 e -1 acima delas)."""
    return ESP_Y1 + (n - 1) * 2.54


Y_LED = {"VM": 34.0, "AM": 41.0, "VD": 48.0}
Y_5V = 53.0       # trilha de 5 V embaixo do ESP32
X_5V = 5.0        # trilha de 5 V na lateral esquerda
X_3V3 = ESP_X + 20.0  # 3,3 V sobe entre as fileiras, embaixo do ESP32
Y_3V3_TOPO = 4.5  # e passa por cima do ESP32 até as barras I2C
X_BORNE = LARGURA - 6.0
Y_FONTE = 63.08   # pad 1 (GND) do borne da fonte; o +12V fica 5,08 mm acima
Y_MOTOR = 75.08   # pad 1 (GND) do borne de saída para o motor
X_12V = X_BORNE - 6.0
X_GND_POT = X_BORNE + 3.5
LM2596 = (20.0, 56.0, 63.0, 77.0)  # contorno do módulo (x1, y1, x2, y2), preso com fita dupla-face

# ======================= Componentes =======================
# ref, símbolo, valor, footprint, (x, y) do pad 1, rotação, {pad: net}, (x, y) no esquemático
# Rotação do footprint: 0 = pads seguem para a direita (+x) / barras para baixo (+y);
# 90 = para cima; 180 = para a esquerda; 270 = para baixo.

FP_BARRA = "Connector_PinSocket_2.54mm:PinSocket_1x{:02d}_P2.54mm_Vertical"
FP_PINOS = "Connector_PinHeader_2.54mm:PinHeader_1x02_P2.54mm_Vertical"
FP_BORNE = "TerminalBlock_Phoenix:TerminalBlock_Phoenix_MKDS-1,5-2-5.08_1x02_P5.08mm_Horizontal"
FP_R = "Resistor_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P7.62mm_Horizontal"
FP_C = "Capacitor_THT:C_Disc_D5.0mm_W2.5mm_P5.00mm"
FP_LED = "LED_THT:LED_D5.0mm"
FP_FURO = "MountingHole:MountingHole_3.2mm_M3"

ESP_ESQ = {  # DevKit V1, fileira esquerda (antena para cima): EN ... VIN
    4: "IO34", 7: "IN2", 8: "ENA", 9: "IN1", 10: "HALL", 14: "GND", 15: "+5V"}
ESP_DIR = {  # DevKit V1, fileira direita: D23 ... 3V3
    2: "SCL", 5: "SDA", 6: "IO19", 7: "IO18", 9: "IO17", 10: "IO16", 14: "GND", 15: "+3V3"}

# Nomes dos pinos da DevKit V1 (só para o esquemático ficar legível)
NOMES_ESQ = ["EN", "VP/IO36", "VN/IO39", "IO34", "IO35", "IO32", "IO33", "IO25", "IO26", "IO27",
             "IO14", "IO12", "IO13", "GND", "VIN"]
NOMES_DIR = ["IO23", "IO22", "TX0/IO1", "RX0/IO3", "IO21", "IO19", "IO18", "IO5", "TX2/IO17",
             "RX2/IO16", "IO4", "IO2", "IO15", "GND", "3V3"]

COMPONENTES = [
    # --- ESP32 DevKit V1 encaixado em duas barras fêmea de 15 pinos ---
    ("J1", "Connector_Generic:Conn_01x15", "ESP32 fileira esq.", FP_BARRA.format(15),
     (ESP_X, P(1)), 0, {str(k): v for k, v in ESP_ESQ.items()}, (101.6, 101.6)),
    ("J2", "Connector_Generic:Conn_01x15", "ESP32 fileira dir.", FP_BARRA.format(15),
     (D, P(1)), 0, {str(k): v for k, v in ESP_DIR.items()}, (165.1, 101.6)),

    # --- Barras para os módulos (ordem igual à do módulo) ---
    ("J3", "Connector_Generic:Conn_01x04", "INA219", FP_BARRA.format(4),
     (D + 9.6, P(-1)), 0, {"1": "+3V3", "2": "GND", "3": "SCL", "4": "SDA"}, (241.3, 63.5)),
    ("J4", "Connector_Generic:Conn_01x04", "MPU6050", FP_BARRA.format(4),
     (D + 17.6, P(-1)), 0, {"1": "+3V3", "2": "GND", "3": "SCL", "4": "SDA"}, (279.4, 63.5)),
    ("J5", "Connector_Generic:Conn_01x04", "OLED", FP_BARRA.format(4),
     (D + 25.6, P(-1)), 0, {"1": "GND", "2": "+3V3", "3": "SCL", "4": "SDA"}, (317.5, 63.5)),
    ("J6", "Connector_Generic:Conn_01x03", "LM35", FP_BARRA.format(3),
     (ESP_X - 22, P(3)), 0, {"1": "+5V", "2": "LM35", "3": "GND"}, (241.3, 106.68)),
    ("J7", "Connector_Generic:Conn_01x03", "Hall 3144E", FP_BARRA.format(3),
     (ESP_X - 22, P(12)), 0, {"1": "+5V", "2": "GND", "3": "HALL"}, (279.4, 106.68)),
    # L298N: o GND vem pelo borne do motor (um caminho só de terra até a ponte)
    ("J8", "Connector_Generic:Conn_01x03", "L298N", FP_BARRA.format(3),
     (ESP_X - 17, P(8)), 0, {"1": "ENA", "2": "IN1", "3": "IN2"}, (317.5, 106.68)),

    # --- Alimentação: fonte 12 V, saída para o motor, LM2596 (12 V -> 5 V) ---
    ("J9", "Connector:Screw_Terminal_01x02", "Fonte 12V", FP_BORNE,
     (X_BORNE, Y_FONTE), 90, {"1": "GND", "2": "+12V"}, (241.3, 149.86)),
    ("J10", "Connector:Screw_Terminal_01x02", "Saida 12V motor", FP_BORNE,
     (X_BORNE, Y_MOTOR), 90, {"1": "GND", "2": "+12V"}, (279.4, 149.86)),
    ("J11", "Connector_Generic:Conn_01x02", "LM2596 entrada", FP_PINOS,
     (LM2596[2] + 3.0, 60.0), 0, {"1": "+12V", "2": "GND"}, (317.5, 149.86)),
    ("J12", "Connector_Generic:Conn_01x02", "LM2596 saida 5V", FP_PINOS,
     (LM2596[0] - 3.0, 60.0), 0, {"1": "+5V", "2": "GND"}, (355.6, 149.86)),

    # --- Sensores: pull-up do Hall, filtro e desacoplamento ---
    ("R1", "Device:R", "10k", FP_R, (ESP_X + 4, P(13)), 90, {"1": "+3V3", "2": "HALL"}, (50.8, 182.88)),
    ("R2", "Device:R", "1k", FP_R, (ESP_X - 17.62, P(4)), 0, {"1": "LM35", "2": "IO34"}, (76.2, 182.88)),
    ("C1", "Device:C", "100nF", FP_C, (ESP_X - 6, P(4)), 270, {"1": "IO34", "2": "GND"}, (101.6, 182.88)),
    ("C2", "Device:C", "100nF", FP_C, (ESP_X - 25.5, P(3)), 90, {"1": "+5V", "2": "GND"}, (127.0, 182.88)),
    ("C3", "Device:C", "100nF", FP_C, (ESP_X - 25.5, P(12)), 270, {"1": "+5V", "2": "GND"}, (152.4, 182.88)),

    # --- LEDs de alarme ---
    ("R3", "Device:R", "220", FP_R, (D + 11.6, Y_LED["VD"]), 0, {"1": "IO16", "2": "LED_VD"}, (50.8, 220.98)),
    ("R4", "Device:R", "220", FP_R, (D + 11.6, Y_LED["AM"]), 0, {"1": "IO17", "2": "LED_AM"}, (76.2, 220.98)),
    ("R5", "Device:R", "220", FP_R, (D + 11.6, Y_LED["VM"]), 0, {"1": "IO18", "2": "LED_VM"}, (101.6, 220.98)),
    ("D1", "Device:LED", "verde", FP_LED, (D + 25.44, Y_LED["VD"]), 180, {"1": "GND", "2": "LED_VD"}, (50.8, 246.38)),
    ("D2", "Device:LED", "amarelo", FP_LED, (D + 25.44, Y_LED["AM"]), 180, {"1": "GND", "2": "LED_AM"}, (76.2, 246.38)),
    ("D3", "Device:LED", "vermelho", FP_LED, (D + 25.44, Y_LED["VM"]), 180, {"1": "GND", "2": "LED_VM"}, (101.6, 246.38)),

    # --- Buzzer ligado direto no GPIO 19 (nível alto = liga) ---
    ("BZ1", "Device:Buzzer", "Buzzer ativo", "Buzzer_Beeper:Buzzer_12x9.5RM7.6",
     (D + 9.0, P(6)), 0, {"1": "IO19", "2": "GND"}, (152.4, 220.98)),

    # --- Jumper de fio (por cima da placa): a trilha de 3,3 V que passa por cima do ESP32
    # cerca uma área do plano de GND (GND do INA219, do MPU6050 e o GND direito do ESP32).
    # Este fio liga essa área ao resto do plano, passando por cima da trilha de 3,3 V.
    ("JP1", "Jumper:Jumper_2_Bridged", "fio jumper", FP_R, (D + 0.6, 1.9), 270,
     {"1": "GND", "2": "GND"}, (190.5, 220.98)),

    # --- Furos de fixação M3 ---
    ("H1", "Mechanical:MountingHole", "M3", FP_FURO, (4.0, 4.0), 0, {}, (50.8, 271.78)),
    ("H2", "Mechanical:MountingHole", "M3", FP_FURO, (LARGURA - 4, 4.0), 0, {}, (63.5, 271.78)),
    ("H3", "Mechanical:MountingHole", "M3", FP_FURO, (4.0, ALTURA - 4), 0, {}, (76.2, 271.78)),
    ("H4", "Mechanical:MountingHole", "M3", FP_FURO, (LARGURA - 16, ALTURA - 4), 0, {}, (88.9, 271.78)),
]

# ======================= Trilhas (B.Cu) =======================
# Cada item: net, sequência de pontos (no centro dos pads) e, opcional, largura.
# GND não tem trilha (vai pelo plano de cobre), exceto a volta do motor entre os bornes.
LED_A_X = D + 25.44 - 2.54
J11_IN = (LM2596[2] + 3.0, 60.0)
J12_OUT = (LM2596[0] - 3.0, 60.0)

TRILHAS = [
    # 3,3 V: sai do pino 3V3 (embaixo, à direita), sobe entre as fileiras e passa por cima do
    # ESP32 até as barras I2C; também vai para o pull-up do Hall
    ("+3V3", [(D, P(15)), (X_3V3, P(15)), (X_3V3, Y_3V3_TOPO), (D + 5.6, Y_3V3_TOPO),
              (D + 5.6, P(-1)), (D + 17.6, P(-1)), (D + 21.1, P(-1)), (D + 23.1, P(0)), (D + 25.6, P(0))]),
    ("+3V3", [(X_3V3, P(15)), (ESP_X + 4, P(15)), (ESP_X + 4, P(13))]),
    # I2C
    ("SCL", [(D, P(2)), (D + 4.0, P(2)), (D + 4.0, P(1)), (D + 25.6, P(1))]),
    ("SDA", [(D, P(5)), (D + 6.6, P(5)), (D + 6.6, P(2)), (D + 25.6, P(2))]),
    # 5 V (do LM2596): VIN do ESP32, LM35 e Hall
    ("+5V", [(ESP_X, P(15)), (ESP_X, Y_5V), (X_5V, Y_5V), (X_5V, P(3)), (ESP_X - 22, P(3))]),
    ("+5V", [(X_5V, P(12)), (ESP_X - 22, P(12))]),
    ("+5V", [J12_OUT, (J12_OUT[0], Y_5V)]),
    # LM35 com filtro RC
    ("LM35", [(ESP_X - 22, P(4)), (ESP_X - 17.62, P(4))]),
    ("IO34", [(ESP_X - 10, P(4)), (ESP_X, P(4))]),
    # L298N: IN2 contorna a barra pela esquerda para não cruzar ENA/IN1
    ("ENA", [(ESP_X, P(8)), (ESP_X - 17, P(8))]),
    ("IN1", [(ESP_X, P(9)), (ESP_X - 17, P(9))]),
    ("IN2", [(ESP_X, P(7)), (ESP_X - 22, P(7)), (ESP_X - 22, P(10)), (ESP_X - 17, P(10))]),
    # Hall: desce pela esquerda do ESP32 até a barra; pull-up entre as fileiras
    ("HALL", [(ESP_X, P(10)), (ESP_X - 4, P(10)), (ESP_X - 4, P(14)), (ESP_X - 22, P(14))]),
    ("HALL", [(ESP_X, P(10)), (ESP_X + 4, P(10))]),
    # Buzzer direto
    ("IO19", [(D, P(6)), (D + 9.0, P(6))]),
    # LEDs: leque sem cruzamento (quem sai mais alto vai mais longe para a direita)
    ("IO18", [(D, P(7)), (D + 7.6, P(7)), (D + 7.6, Y_LED["VM"]), (D + 11.6, Y_LED["VM"])]),
    ("IO17", [(D, P(9)), (D + 5.1, P(9)), (D + 5.1, Y_LED["AM"]), (D + 11.6, Y_LED["AM"])]),
    ("IO16", [(D, P(10)), (D + 2.6, P(10)), (D + 2.6, Y_LED["VD"]), (D + 11.6, Y_LED["VD"])]),
    ("LED_VM", [(D + 19.22, Y_LED["VM"]), (LED_A_X, Y_LED["VM"])]),
    ("LED_AM", [(D + 19.22, Y_LED["AM"]), (LED_A_X, Y_LED["AM"])]),
    ("LED_VD", [(D + 19.22, Y_LED["VD"]), (LED_A_X, Y_LED["VD"])]),
    # 12 V: borne da fonte -> borne do motor, com derivação para o LM2596
    ("+12V", [(X_BORNE, Y_FONTE - 5.08), (X_12V, Y_FONTE - 5.08), (X_12V, Y_MOTOR - 5.08),
              (X_BORNE, Y_MOTOR - 5.08)], TRILHA_POT),
    ("+12V", [J11_IN, (X_12V, J11_IN[1])], TRILHA_POT),
    # Volta do motor direto entre os bornes, pela borda (não passa perto dos sensores)
    ("GND", [(X_BORNE, Y_FONTE), (X_GND_POT, Y_FONTE), (X_GND_POT, Y_MOTOR), (X_BORNE, Y_MOTOR)],
     TRILHA_GND_POT),
]

# Textos na serigrafia (guia de montagem; na placa caseira vão só no PDF de montagem)
TEXTOS_SILK = [
    ("ESP32 DevKit V1", (ESP_X + ESP_DIST / 2, 26.0), 1.2),
    ("ANTENA", (ESP_X + ESP_DIST / 2, 9.5), 1.0),
    ("USB", (ESP_X + ESP_DIST / 2, 52.0), 1.0),
    ("INA219", (D + 9.6, 4.6), 1.0),
    ("MPU6050", (D + 17.6, 4.6), 1.0),
    ("OLED", (D + 25.6, 4.6), 1.0),
    ("LM35: 5V OUT GND", (13.0, 25.9), 0.9),
    ("L298N: ENA IN1 IN2", (17.0, 28.0), 0.9),
    ("HALL: 5V GND OUT", (13.0, 49.2), 0.9),
    ("BUZZER", (D + 12.8, 17.9), 0.9),
    ("VM", (D + 28.9, Y_LED["VM"]), 1.0),
    ("AM", (D + 28.9, Y_LED["AM"]), 1.0),
    ("VD", (D + 28.9, Y_LED["VD"]), 1.0),
    ("FONTE 12V", (X_BORNE - 12.0, 56.5), 1.0),
    ("SAIDA 12V", (X_BORNE - 12.0, 68.5), 1.0),
    ("+", (X_BORNE - 6.5, Y_FONTE - 5.08), 1.2),
    ("-", (X_BORNE - 6.5, Y_FONTE), 1.2),
    ("+", (X_BORNE - 6.5, Y_MOTOR - 5.08), 1.2),
    ("-", (X_BORNE - 6.5, Y_MOTOR), 1.2),
    ("JP1: fio", (D + 4.6, 1.9), 0.9),
    ("LM2596", ((LM2596[0] + LM2596[2]) / 2, 63.5), 1.5),
    ("ajustar 5,0 V antes de ligar", ((LM2596[0] + LM2596[2]) / 2, 66.5), 1.0),
    ("OUT+", (J12_OUT[0] - 3.6, J12_OUT[1]), 0.9),
    ("OUT-", (J12_OUT[0] - 3.6, J12_OUT[1] + 2.54), 0.9),
    ("IN+", (J11_IN[0] + 3.2, J11_IN[1]), 0.9),
    ("IN-", (J11_IN[0] + 3.2, J11_IN[1] + 2.54), 0.9),
]
# Legenda dos pinos das barras I2C (à esquerda do INA219 e à direita do OLED)
for i, nome in enumerate(["VCC", "GND", "SCL", "SDA"]):
    TEXTOS_SILK.append((nome, (D + 6.7, P(-1 + i)), 0.9))
for i, nome in enumerate(["GND", "VCC", "SCL", "SDA"]):
    TEXTOS_SILK.append((nome, (D + 28.9, P(-1 + i)), 0.9))

# Contornos desenhados na serigrafia (x1, y1, x2, y2)
RETANGULOS_SILK = [LM2596]

# Textos no cobre (lidos do lado do cobre; ficam espelhados na vista de cima)
TEXTOS_COBRE = [
    ("+12V", (X_BORNE, Y_FONTE - 9.0), 1.3),
    ("+12V", (X_BORNE - 1.0, Y_MOTOR - 8.55), 1.3),
    ("FONTE", (X_BORNE - 13.0, 64.5), 1.3),
    ("MOTOR", (X_BORNE - 13.0, 72.5), 1.3),
    ("DPEE1107 UFSM v2", ((LM2596[0] + LM2596[2]) / 2, 71.0), 1.5),
]


NETS_ALIMENTACAO = ("GND", "+3V3", "+5V", "+12V")  # viram símbolos de alimentação no esquemático


# ======================= S-expressions (arquivos do KiCad) =======================
class Str(str):
    """String que vai entre aspas no arquivo."""


def ler_sexpr(texto):
    pos = 0
    pilha = [[]]
    n = len(texto)
    while pos < n:
        c = texto[pos]
        if c == "(":
            pilha.append([])
            pos += 1
        elif c == ")":
            item = pilha.pop()
            pilha[-1].append(item)
            pos += 1
        elif c == '"':
            pos += 1
            buf = []
            while texto[pos] != '"':
                if texto[pos] == "\\":
                    pos += 1
                    buf.append({"n": "\n", "t": "\t"}.get(texto[pos], texto[pos]))
                else:
                    buf.append(texto[pos])
                pos += 1
            pos += 1
            pilha[-1].append(Str("".join(buf)))
        elif c.isspace():
            pos += 1
        else:
            ini = pos
            while pos < n and not texto[pos].isspace() and texto[pos] not in '()"':
                pos += 1
            pilha[-1].append(texto[ini:pos])
    return pilha[0]


def escrever_sexpr(no, nivel=0):
    if isinstance(no, Str):
        return '"' + no.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n") + '"'
    if isinstance(no, list):
        if all(not isinstance(x, list) for x in no):
            return "(" + " ".join(escrever_sexpr(x) for x in no) + ")"
        tab = "\t" * (nivel + 1)
        partes = []
        for x in no:
            partes.append(escrever_sexpr(x, nivel + 1))
        cabeca = []
        while partes and not isinstance(no[len(cabeca)], list):
            cabeca.append(partes.pop(0))
        return "(" + " ".join(cabeca) + "".join("\n" + tab + p for p in partes) + "\n" + "\t" * nivel + ")"
    if isinstance(no, float):
        return ("%.4f" % no).rstrip("0").rstrip(".")
    return str(no)


def achar(no, chave):
    return [x for x in no if isinstance(x, list) and x and x[0] == chave]


_cache_libs = {}


def simbolo_da_biblioteca(lib_id):
    """Extrai a definição de um símbolo do arquivo .kicad_sym (sem ler o arquivo inteiro)."""
    lib, nome = lib_id.split(":")
    if lib not in _cache_libs:
        with open(os.path.join(KICAD, "symbols", lib + ".kicad_sym"), encoding="utf-8") as f:
            _cache_libs[lib] = f.read()
    texto = _cache_libs[lib]
    ini = texto.index('\n\t(symbol "%s"' % nome) + 1
    prof, pos, em_str = 0, ini, False
    while True:
        c = texto[pos]
        if em_str:
            if c == "\\":
                pos += 1
            elif c == '"':
                em_str = False
        elif c == '"':
            em_str = True
        elif c == "(":
            prof += 1
        elif c == ")":
            prof -= 1
            if prof == 0:
                break
        pos += 1
    sym = ler_sexpr(texto[ini:pos + 1])[0]
    assert not achar(sym, "extends"), lib_id + " herda de outro símbolo; use o símbolo base"
    sym[1] = Str(lib_id)
    return sym


def pinos_do_simbolo(sym):
    """{numero: (x, y, angulo)} em coordenadas da biblioteca (y para cima)."""
    pinos = {}

    def visitar(no):
        for x in no:
            if isinstance(x, list) and x:
                if x[0] == "pin":
                    at = achar(x, "at")[0]
                    num = achar(x, "number")[0][1]
                    pinos[str(num)] = (float(at[1]), float(at[2]), int(float(at[3])))
                else:
                    visitar(x)
    visitar(sym)
    return pinos


def nova_uuid():
    return Str(str(uuid.uuid4()))


def fonte(tam=1.27):
    return ["effects", ["font", ["size", tam, tam]]]


# ======================= Esquemático =======================
def gerar_esquematico(uuids):
    raiz = nova_uuid()
    libs = {}
    itens = []
    n_pwr = [0]

    def instancia(lib_id, ref, valor, pos, rot=0, footprint="", sym_uuid=None, pinos=(),
                  valor_horizontal=None):
        """valor_horizontal: 0 ou 180 = símbolo de alimentação deitado; o nome da net fica
        na horizontal, logo depois do desenho, para não encavalar com os pinos vizinhos."""
        sym = libs.setdefault(lib_id, simbolo_da_biblioteca(lib_id))
        props_lib = {p[1]: p for p in achar(sym, "property")}
        x, y = pos
        no = ["symbol", ["lib_id", Str(lib_id)], ["at", x, y, rot], ["unit", 1],
              ["exclude_from_sim", "no"], achar(sym, "in_bom")[0], achar(sym, "on_board")[0],
              ["dnp", "no"],
              ["uuid", sym_uuid or nova_uuid()]]
        cos_r = {0: 1, 90: 0, 180: -1, 270: 0}[rot % 360]
        sen_r = {0: 0, 90: 1, 180: 0, 270: -1}[rot % 360]
        for nome, val in (("Reference", ref), ("Value", valor), ("Footprint", footprint),
                          ("Datasheet", props_lib.get("Datasheet", [0, 0, ""])[2])):
            p = props_lib.get(nome)
            if p is not None:
                at = achar(p, "at")[0]
                px, py, ang = float(at[1]), float(at[2]), int(float(at[3]))
                efeitos = achar(p, "effects")[0] if achar(p, "effects") else fonte()
                oculto = bool(achar(p, "hide"))
            else:
                px, py, ang, efeitos, oculto = 0.0, 0.0, 0, fonte(), True
            # Gira a posição do texto junto com o símbolo; símbolo girado fica com texto na horizontal
            rx, ry = px * cos_r - py * sen_r, px * sen_r + py * cos_r
            if rot % 360:
                ang = 0
            if nome == "Value" and valor_horizontal is not None:
                # O KiCad soma o ângulo do campo à rotação do símbolo: -rot deixa o texto deitado
                lado = -1 if valor_horizontal == 180 else 1
                rx, ry, ang = lado * 3.3, 0.0, (-rot) % 360
                efeitos = ["effects", ["font", ["size", 1.27, 1.27]],
                           ["justify", "right" if lado < 0 else "left"]]
            prop = ["property", Str(nome), Str(val), ["at", x + rx, y - ry, ang]]
            if oculto or nome in ("Footprint", "Datasheet"):
                prop.append(["hide", "yes"])
            prop.append(efeitos)
            no.append(prop)
        for num in pinos:
            no.append(["pin", Str(num), ["uuid", nova_uuid()]])
        no.append(["instances", ["project", Str(NOME), ["path", Str("/" + raiz),
                                                         ["reference", Str(ref)], ["unit", 1]]]])
        itens.append(no)
        return sym

    def ponta(pos, pino):
        px, py, ang = pino
        return (pos[0] + px, pos[1] - py), (ang + 180) % 360

    def ligar(ponto, direcao, net):
        """Coloca um rótulo, símbolo de alimentação ou X de não-conectado na ponta do pino."""
        x, y = ponto
        if net is None:
            itens.append(["no_connect", ["at", x, y], ["uuid", nova_uuid()]])
        elif net in NETS_ALIMENTACAO:
            n_pwr[0] += 1
            # GND aponta para baixo (270) na biblioteca; +3V3/+5V/+12V para cima (90)
            rot = (direcao - (270 if net == "GND" else 90)) % 360
            instancia("power:" + net, "#PWR%02d" % n_pwr[0], net, (x, y), rot, pinos=["1"],
                      valor_horizontal=direcao if direcao in (0, 180) else None)
        else:
            efe = ["effects", ["font", ["size", 1.27, 1.27]],
                   ["justify", "right" if direcao in (180, 270) else "left", "bottom"]]
            itens.append(["label", Str(net), ["at", x, y, direcao], efe, ["uuid", nova_uuid()]])

    # PWR_FLAG avisa ao ERC que a net é alimentada: 12 V e GND pelo borne da fonte, 5 V pelo
    # LM2596, 3,3 V pelo regulador do ESP32. Vai num pino da ponta do conector: toco de fio
    # até o símbolo de alimentação e a flag apontando para fora.
    flags = {("J9", "1"): 90, ("J9", "2"): 270, ("J12", "1"): 90, ("J2", "15"): 270}

    for ref, lib_id, valor, fp, _pos, _rot, nets, pos_sch in COMPONENTES:
        sym = libs.setdefault(lib_id, simbolo_da_biblioteca(lib_id))
        pinos = pinos_do_simbolo(sym)
        instancia(lib_id, ref, valor, pos_sch, 0, fp, uuids[ref], sorted(pinos, key=int))
        for num, pino in pinos.items():
            ponto, direcao = ponta(pos_sch, pino)
            if (ref, num) in flags:
                x, y = ponto
                x2 = x - 5.08 if direcao == 180 else x + 5.08
                itens.append(["wire", ["pts", ["xy", x, y], ["xy", x2, y]],
                              ["stroke", ["width", 0], ["type", "default"]], ["uuid", nova_uuid()]])
                n_pwr[0] += 1
                instancia("power:PWR_FLAG", "#FLG%02d" % n_pwr[0], "PWR_FLAG", (x, y),
                          (flags[(ref, num)] - 90) % 360, pinos=["1"])
                ligar((x2, y), direcao, nets.get(num))
            else:
                ligar(ponto, direcao, nets.get(num))

    # Nomes dos pinos do ESP32 ao lado das barras
    for ref, nomes in (("J1", NOMES_ESQ), ("J2", NOMES_DIR)):
        comp = next(c for c in COMPONENTES if c[0] == ref)
        x, y = comp[7]
        for i, nome in enumerate(nomes):
            yy = y - (len(nomes) - 1) * 1.27 + i * 2.54
            itens.append(["text", Str(nome), ["exclude_from_sim", "no"], ["at", x + 3.81, yy, 0],
                          ["effects", ["font", ["size", 1.0, 1.0]], ["justify", "left"]],
                          ["uuid", nova_uuid()]])

    textos = [
        ("ESP32 DevKit V1 (encaixado em barras femea)", (82.0, 73.66)),
        ("Barras para os modulos (mesma ordem de pinos do modulo)", (230.0, 50.8)),
        ("Alimentacao: fonte 12V, saida para INA219/L298N, LM2596 (12V -> 5V no VIN)", (230.0, 134.62)),
        ("Sensores: pull-up do Hall, filtro RC do LM35, desacoplamento", (40.0, 170.18)),
        ("Alarmes: LEDs e buzzer direto no GPIO19 (nivel alto = liga)", (40.0, 208.28)),
        ("Furos M3", (40.0, 264.16)),
    ]
    for t, (x, y) in textos:
        itens.append(["text", Str(t), ["exclude_from_sim", "no"], ["at", x, y, 0],
                      ["effects", ["font", ["size", 2.0, 2.0]], ["justify", "left"]],
                      ["uuid", nova_uuid()]])

    doc = ["kicad_sch", ["version", "20251024"], ["generator", Str("eeschema")],
           ["generator_version", Str("10.0")], ["uuid", raiz], ["paper", Str("A3")],
           ["title_block", ["title", Str("Placa de conexoes - monitor de motor DC")],
            ["date", Str("2026-11-04")], ["rev", Str("2")],
            ["company", Str("UFSM - DPEE1107 Sistemas Microcontrolados")],
            ["comment", 1, Str("Gerado por gerar_placa.py - nao editar a mao")]],
           ["lib_symbols"] + list(libs.values())]
    doc += itens
    doc += [["sheet_instances", ["path", Str("/"), ["page", Str("1")]]], ["embedded_fonts", "no"]]
    with open(os.path.join(AQUI, NOME + ".kicad_sch"), "w", encoding="utf-8") as f:
        f.write(escrever_sexpr(doc) + "\n")


# ======================= Placa =======================
def mm(x, y):
    return pcbnew.VECTOR2I_MM(ORIGEM[0] + x, ORIGEM[1] + y)


def remover_silk_sobre_pads(fp, folga=0.25):
    def dist_seg(p, a, b):
        ax, ay, bx, by, px, py = a.x, a.y, b.x, b.y, p.x, p.y
        dx, dy = bx - ax, by - ay
        t = 0.0 if dx == dy == 0 else max(0.0, min(1.0, ((px - ax) * dx + (py - ay) * dy) / (dx * dx + dy * dy)))
        return ((ax + t * dx - px) ** 2 + (ay + t * dy - py) ** 2) ** 0.5

    pads = [(p.GetPosition(), p.GetSize(pcbnew.F_Cu).x / 2) for p in fp.Pads()]
    for item in list(fp.GraphicalItems()):
        if item.GetLayer() != pcbnew.F_SilkS or not isinstance(item, pcbnew.PCB_SHAPE):
            continue
        if item.GetShape() != pcbnew.SHAPE_T_SEGMENT:
            continue
        meia = item.GetWidth() / 2 + pcbnew.FromMM(folga)
        if any(dist_seg(c, item.GetStart(), item.GetEnd()) < r + meia for c, r in pads):
            # Move para a camada de comentários (não sai em PDF/Gerber). Não usar
            # fp.Remove(): pelo Python isso corrompe a memória do KiCad 10.
            item.SetLayer(pcbnew.Cmts_User)


def gerar_placa(uuids):
    caminho = os.path.join(AQUI, NOME + ".kicad_pcb")
    # Carrega todos os footprints ANTES de criar a placa. No KiCad 10, carregar footprints
    # depois de já ter adicionado outros à placa às vezes devolve um objeto sem tipo
    # (erro "SwigPyObject has no attribute ..."). Um carregador só, reaproveitado.
    carregador = pcbnew.PCB_IO_MGR.FindPlugin(pcbnew.PCB_IO_MGR.KICAD_SEXP)
    footprints = {}
    for ref, _sym, _valor, fp_id, *_resto in COMPONENTES:
        lib, nome = fp_id.split(":")
        footprints[ref] = carregador.FootprintLoad(os.path.join(KICAD, "footprints", lib + ".pretty"), nome)
        assert isinstance(footprints[ref], pcbnew.FOOTPRINT), "falha ao carregar " + fp_id

    board = pcbnew.NewBoard(caminho)

    ds = board.GetDesignSettings()
    ds.m_MinClearance = pcbnew.FromMM(ISOLACAO)
    ds.m_CopperEdgeClearance = pcbnew.FromMM(0.5)
    nc = ds.m_NetSettings.GetDefaultNetclass()
    nc.SetClearance(pcbnew.FromMM(ISOLACAO))
    nc.SetTrackWidth(pcbnew.FromMM(TRILHA))

    nets = {}

    def net(nome):
        nome_pcb = nome if nome in NETS_ALIMENTACAO else "/" + nome
        if nome_pcb not in nets:
            n = pcbnew.NETINFO_ITEM(board, nome_pcb)
            board.Add(n)
            nets[nome_pcb] = n
        return nets[nome_pcb]

    # Contorno
    ret = pcbnew.PCB_SHAPE(board)
    ret.SetShape(pcbnew.SHAPE_T_RECT)
    ret.SetStart(mm(0, 0))
    ret.SetEnd(mm(LARGURA, ALTURA))
    ret.SetLayer(pcbnew.Edge_Cuts)
    ret.SetWidth(pcbnew.FromMM(0.1))
    board.Add(ret)

    # Componentes
    for ref, _sym, valor, fp_id, pos, rot, nets_pads, _sch in COMPONENTES:
        lib, nome = fp_id.split(":")
        fp = footprints[ref]
        fp.SetFPID(pcbnew.LIB_ID(lib, nome))
        fp.SetReference(ref)
        fp.SetValue(valor)
        fp.SetPath(pcbnew.KIID_PATH("/" + uuids[ref]))
        fp.SetSheetname("/")
        fp.SetSheetfile(NOME + ".kicad_sch")
        board.Add(fp)
        fp.SetPosition(mm(*pos))
        fp.SetOrientationDegrees(rot)
        for pad in fp.Pads():
            if pad.GetAttribute() == pcbnew.PAD_ATTRIB_PTH and "TerminalBlock" not in nome:
                passo_curto = any(k in nome for k in ("PinSocket", "PinHeader", "LED", "TO-92"))
                d = PAD_BARRA if passo_curto else PAD_OUTROS
                pad.SetSize(pcbnew.F_Cu, pcbnew.VECTOR2I_MM(d, d))
            n = nets_pads.get(pad.GetNumber())
            if n:
                pad.SetNet(net(n))
            elif pad.GetNumber():
                livre = pcbnew.NETINFO_ITEM(board, "unconnected-(%s-Pin_%s-Pad%s)"
                                            % (ref, pad.GetNumber(), pad.GetNumber()))
                board.Add(livre)
                pad.SetNet(livre)
        # Referência só no desenho de montagem (F.Fab), para não cair em cima dos pads
        fp.Reference().SetVisible(False)
        # Barras e LEDs já têm nome na serigrafia: esconde o valor repetido do desenho
        if (ref[0] == "J" and ref not in ("J1", "J2")) or ref[0] == "H" \
                or ref in ("D1", "D2", "D3", "BZ1"):
            fp.Value().SetVisible(False)
        # Com os pads maiores, alguns traços de serigrafia da biblioteca encostam neles: tira
        remover_silk_sobre_pads(fp)

    # Trilhas
    for nome, pontos, *largura in TRILHAS:
        for (x1, y1), (x2, y2) in zip(pontos, pontos[1:]):
            t = pcbnew.PCB_TRACK(board)
            t.SetStart(mm(x1, y1))
            t.SetEnd(mm(x2, y2))
            t.SetWidth(pcbnew.FromMM(largura[0] if largura else TRILHA))
            t.SetLayer(pcbnew.B_Cu)
            t.SetNet(net(nome))
            board.Add(t)

    # Plano de GND embaixo
    z = pcbnew.ZONE(board)
    z.SetLayer(pcbnew.B_Cu)
    z.SetNet(net("GND"))
    contorno = z.Outline()
    contorno.NewOutline()
    for x, y in ((0, 0), (LARGURA, 0), (LARGURA, ALTURA), (0, ALTURA)):
        contorno.Append(mm(x, y))
    z.SetLocalClearance(pcbnew.FromMM(0.6))
    z.SetMinThickness(pcbnew.FromMM(0.5))
    z.SetThermalReliefGap(pcbnew.FromMM(0.6))
    z.SetThermalReliefSpokeWidth(pcbnew.FromMM(1.0))
    z.SetPadConnection(pcbnew.ZONE_CONNECTION_THERMAL)
    z.SetIslandRemovalMode(pcbnew.ISLAND_REMOVAL_MODE_ALWAYS)
    board.Add(z)

    # Textos
    for texto, (x, y), tam in TEXTOS_SILK:
        t = pcbnew.PCB_TEXT(board)
        t.SetText(texto)
        t.SetPosition(mm(x, y))
        t.SetLayer(pcbnew.F_SilkS)
        t.SetTextSize(pcbnew.VECTOR2I_MM(tam, tam))
        t.SetTextThickness(pcbnew.FromMM(tam * 0.15))
        board.Add(t)
    for texto, (x, y), tam in TEXTOS_COBRE:
        t = pcbnew.PCB_TEXT(board)
        t.SetText(texto)
        t.SetPosition(mm(x, y))
        t.SetLayer(pcbnew.B_Cu)
        t.SetMirrored(True)
        t.SetTextSize(pcbnew.VECTOR2I_MM(tam, tam))
        t.SetTextThickness(pcbnew.FromMM(tam * 0.2))
        board.Add(t)
    for x1, y1, x2, y2 in RETANGULOS_SILK:
        r = pcbnew.PCB_SHAPE(board)
        r.SetShape(pcbnew.SHAPE_T_RECT)
        r.SetStart(mm(x1, y1))
        r.SetEnd(mm(x2, y2))
        r.SetLayer(pcbnew.F_SilkS)
        r.SetWidth(pcbnew.FromMM(0.15))
        board.Add(r)

    pcbnew.SaveBoard(caminho, board)

    # Preenche o plano de GND e salva de novo (sem isso os PDFs/Gerbers saem sem o plano e
    # todos os pads de GND ficariam soltos). Precisa recarregar do arquivo: preenchendo a
    # placa recém-montada em memória, o KiCad ignora a borda e os furos de fixação.
    board = pcbnew.LoadBoard(caminho)
    board.BuildConnectivity()
    zonas = board.Zones()
    pcbnew.ZONE_FILLER(board).Fill(zonas)
    assert all(zz.GetFilledPolysList(pcbnew.B_Cu).OutlineCount() > 0 for zz in zonas), \
        "plano de GND não foi preenchido"
    pcbnew.SaveBoard(caminho, board)
    marcar_jumpers(caminho, ["JP1"])
    return board


def marcar_jumpers(caminho, refs):
    """Diz ao KiCad que os dois furos de um jumper de fio estão ligados (o fio não é cobre).
    Sem isso o DRC acusa a área de GND do outro lado do fio como desconectada. Não há
    função para isso no Python do KiCad 10, então a propriedade vai direto no arquivo."""
    with open(caminho, encoding="utf-8") as f:
        texto = f.read()
    for ref in refs:
        i = texto.index('(property "Reference" "%s"' % ref)
        ini = texto.rfind("\n\t(footprint ", 0, i)
        j = texto.index("(duplicate_pad_numbers_are_jumpers", ini)
        proximo = texto.find("\n\t(footprint ", ini + 1)
        assert proximo == -1 or j < proximo, "footprint de %s sem duplicate_pad_numbers_are_jumpers" % ref
        fim = texto.index("\n", j)
        texto = texto[:fim] + '\n\t\t(jumper_pad_groups ("1" "2"))' + texto[fim:]
    with open(caminho, "w", encoding="utf-8") as f:
        f.write(texto)


def ajustar_projeto():
    """Os pads foram aumentados de propósito (furação manual): não tratar como erro."""
    import json
    caminho = os.path.join(AQUI, NOME + ".kicad_pro")
    with open(caminho, encoding="utf-8") as f:
        pro = json.load(f)
    sev = pro.setdefault("board", {}).setdefault("design_settings", {}).setdefault("rule_severities", {})
    sev["lib_footprint_mismatch"] = "ignore"
    sev["lib_footprint_issues"] = "ignore"
    with open(caminho, "w", encoding="utf-8") as f:
        json.dump(pro, f, indent=2)


def main():
    uuids = {c[0]: str(uuid.uuid4()) for c in COMPONENTES}
    gerar_esquematico(uuids)
    gerar_placa(uuids)
    with open(os.path.join(AQUI, NOME + ".kicad_dru"), "w", encoding="utf-8") as f:
        f.write('(version 1)\n'
                '(rule "fabricacao caseira: isolacao"\n'
                '  (constraint clearance (min %.1fmm)))\n'
                '(rule "fabricacao caseira: trilha"\n'
                '  (condition "A.Type == \'track\'")\n'
                '  (constraint track_width (min 1.0mm)))\n' % ISOLACAO)
    ajustar_projeto()
    print("ok:", os.path.join(AQUI, NOME))


if __name__ == "__main__":
    main()
