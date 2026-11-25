#!/usr/bin/env python3
"""Gera MotorDC.aia, o projeto do app para importar no MIT App Inventor.

Editar este script e rodar de novo; não editar o .aia à mão.
Usa versões antigas de propósito: o App Inventor atualiza projetos antigos ao importar,
mas recusa projetos salvos numa versão mais nova que a do servidor.
"""
import json
import pathlib
import xml.etree.ElementTree as ET
import zipfile
from xml.sax.saxutils import escape

PASTA = pathlib.Path(__file__).resolve().parent
SAIDA = PASTA / "MotorDC.aia"
ENDERECO_PADRAO = "192.168.4.1"  # rede própria MotorDC: o .apk no Android funciona sem internet

YA_VERSION = "208"
BLOCKS_VERSION = "33"
VERSOES = {
    "Form": "27", "Button": "6", "Clock": "3", "Label": "5", "TextBox": "6",
    "HorizontalArrangement": "3", "VerticalArrangement": "3", "Web": "7",
}

COR_FUNDO = "&HFFF4F4F2"
COR_CARTAO = "&HFFFFFFFF"
COR_VERDE = "&HFF1F6F43"
COR_VERMELHO = "&HFFB3261E"
COR_OK = 0xFFE4F3E4 - (1 << 32)      # o bloco de número recebe o ARGB com sinal
COR_ALARME = 0xFFFBE3E1 - (1 << 32)

# ---------- Componentes da tela ----------
_uuid = 1000


def comp(tipo, nome, filhos=None, **props):
    global _uuid
    _uuid += 1
    c = {"$Name": nome, "$Type": tipo, "$Version": VERSOES[tipo], "Uuid": str(_uuid)}
    c.update({k: str(v) for k, v in props.items()})
    if filhos is not None:
        c["$Components"] = filhos
    return c


def espaco(nome):
    return comp("Label", nome, Height=8, Width=-2, Text="")


def cartao(nome, titulo, rotulo):
    return comp("VerticalArrangement", f"Cartao{nome}", [
        comp("Label", f"Tit{nome}", Text=titulo, FontSize=12, TextColor="&HFF666666", Width=-2, TextAlignment=1),
        comp("Label", rotulo, Text="-", FontSize=24, FontBold="True", Width=-2, TextAlignment=1),
    ], Width=-2, AlignHorizontal=3, BackgroundColor=COR_CARTAO)


def linha(n, a, b):
    return comp("HorizontalArrangement", f"Linha{n}", [
        a, comp("Label", f"Vao{n}", Width=8, Text=""), b,
    ], Width=-2)


TELA = [
    comp("Label", "LblConexao", Text="Conectando...", FontSize=15, Width=-2, TextAlignment=1),
    espaco("Esp1"),
    linha(1, cartao("Rpm", "Rotação (RPM)", "LblRpm"), cartao("Ref", "Referência (RPM)", "LblRef")),
    espaco("Esp2"),
    linha(2, cartao("Duty", "Duty (%)", "LblDuty"), cartao("Corrente", "Corrente (mA, aprox.)", "LblCorrente")),
    espaco("Esp3"),
    linha(3, cartao("Vib", "Vibração (g)", "LblVibracao"), cartao("Temp", "Temperatura (°C)", "LblTemperatura")),
    espaco("Esp4"),
    comp("Label", "LblAlarmes", Text="sem alarmes", FontSize=16, FontBold="True", Width=-2, TextAlignment=1,
         BackgroundColor="&HFFE4F3E4"),
    espaco("Esp5"),
    comp("Label", "TitRpmDesejado", Text="RPM desejado (500 a 6000)", FontSize=14, Width=-2),
    comp("TextBox", "TxtRpm", Text="2000", NumbersOnly="True", Hint="RPM", Width=-2, FontSize=18),
    espaco("Esp6"),
    comp("HorizontalArrangement", "LinhaBotoes", [
        comp("Button", "BtnLigar", Text="Ligar", Width=-2, FontSize=18, FontBold="True", Shape=1,
             BackgroundColor=COR_VERDE, TextColor="&HFFFFFFFF"),
        comp("Label", "VaoBotoes", Width=8, Text=""),
        comp("Button", "BtnParar", Text="Parar", Width=-2, FontSize=18, FontBold="True", Shape=1,
             BackgroundColor=COR_VERMELHO, TextColor="&HFFFFFFFF"),
    ], Width=-2),
    comp("Label", "LblMsg", Text="", FontSize=14, Width=-2, TextAlignment=1),
    espaco("Esp7"),
    comp("Label", "TitEndereco", Text="Endereço do ESP32", FontSize=12, TextColor="&HFF666666", Width=-2),
    comp("TextBox", "TxtEndereco", Text=ENDERECO_PADRAO, Hint=ENDERECO_PADRAO, Width=-2),
    comp("Clock", "Relogio", TimerInterval=1000),
    comp("Web", "WebStatus", Timeout=2500),
    comp("Web", "WebCmd", Timeout=3000),
]

FORM = {
    "authURL": ["ai2.appinventor.mit.edu"],
    "YaVersion": YA_VERSION,
    "Source": "Form",
    "Properties": {
        "$Name": "Screen1", "$Type": "Form", "$Version": VERSOES["Form"], "Uuid": "0",
        "AppName": "MotorDC", "Title": "Motor DC", "Scrollable": "True", "AlignHorizontal": "3",
        "BackgroundColor": COR_FUNDO, "PrimaryColor": COR_VERDE, "PrimaryColorDark": "&HFF14492C",
        "AccentColor": COR_VERDE, "Sizing": "Responsive", "ActionBar": "True",
        "$Components": TELA,
    },
}


def tipos_por_nome(lista, saida):
    for c in lista:
        saida[c["$Name"]] = c["$Type"]
        tipos_por_nome(c.get("$Components", []), saida)
    return saida


TIPOS = tipos_por_nome(TELA, {"Screen1": "Form"})


# ---------- Blocos ----------
def texto(s):
    return f'<block type="text"><field name="TEXT">{escape(s)}</field></block>'


def numero(n):
    return f'<block type="math_number"><field name="NUM">{n}</field></block>'


def parametro(nome):
    return (f'<block type="lexical_variable_get"><mutation><eventparam name="{nome}"></eventparam></mutation>'
            f'<field name="VAR">{nome}</field></block>')


def campos():
    return '<block type="lexical_variable_get"><field name="VAR">global campos</field></block>'


def item(n):
    return (f'<block type="lists_select_item"><value name="LIST">{campos()}</value>'
            f'<value name="NUM">{numero(n)}</value></block>')


def juntar(*partes):
    v = "".join(f'<value name="ADD{i}">{p}</value>' for i, p in enumerate(partes))
    return f'<block type="text_join"><mutation items="{len(partes)}"></mutation>{v}</block>'


def comparar(op, a, b):
    return (f'<block type="math_compare"><field name="OP">{op}</field><value name="A">{a}</value>'
            f'<value name="B">{b}</value></block>')


def ler(nome, prop):
    return (f'<block type="component_set_get"><mutation component_type="{TIPOS[nome]}" set_or_get="get" '
            f'property_name="{prop}" is_generic="false" instance_name="{nome}"></mutation>'
            f'<field name="COMPONENT_SELECTOR">{nome}</field><field name="PROP">{prop}</field></block>')


# Comandos em sequência: cada função devolve (abertura, fechamento) e encadear() monta os <next>.
def definir(nome, prop, valor):
    return (f'<block type="component_set_get"><mutation component_type="{TIPOS[nome]}" set_or_get="set" '
            f'property_name="{prop}" is_generic="false" instance_name="{nome}"></mutation>'
            f'<field name="COMPONENT_SELECTOR">{nome}</field><field name="PROP">{prop}</field>'
            f'<value name="VALUE">{valor}</value>', "</block>")


def chamar_get(nome):
    return (f'<block type="component_method"><mutation component_type="Web" method_name="Get" '
            f'is_generic="false" instance_name="{nome}"></mutation>'
            f'<field name="COMPONENT_SELECTOR">{nome}</field>', "</block>")


def guardar_campos(valor):
    return (f'<block type="lexical_variable_set"><field name="VAR">global campos</field>'
            f'<value name="VALUE">{valor}</value>', "</block>")


def se(condicao, entao, senao):
    return (f'<block type="controls_if"><mutation else="1"></mutation><value name="IF0">{condicao}</value>'
            f'<statement name="DO0">{encadear(entao)}</statement><statement name="ELSE">{encadear(senao)}</statement>',
            "</block>")


def encadear(comandos):
    xml = ""
    for abre, fecha in reversed(comandos):
        prox = f"<next>{xml}</next>" if xml else ""
        xml = abre + prox + fecha
    return xml


def evento(y, nome, ev, comandos):
    return (f'<block type="component_event" x="20" y="{y}"><mutation component_type="{TIPOS[nome]}" '
            f'is_generic="false" instance_name="{nome}" event_name="{ev}"></mutation>'
            f'<field name="COMPONENT_SELECTOR">{nome}</field>'
            f'<statement name="DO">{encadear(comandos)}</statement></block>')


def url(rota, *resto):
    return juntar(texto("http://"), ler("TxtEndereco", "Text"), texto(rota), *resto)


SEM_CONEXAO = "Sem conexão com o ESP32. Entre na rede MotorDC e desligue os dados móveis."

atualizar_tela = [
    definir("LblRpm", "Text", item(1)),
    definir("LblRef", "Text", item(2)),
    definir("LblDuty", "Text", item(3)),
    definir("LblCorrente", "Text", item(4)),
    definir("LblVibracao", "Text", item(5)),
    definir("LblTemperatura", "Text", item(6)),
    definir("LblAlarmes", "Text", item(8)),
    se(comparar("EQ", item(7), numero(0)),
       [definir("LblAlarmes", "BackgroundColor", numero(COR_OK))],
       [definir("LblAlarmes", "BackgroundColor", numero(COR_ALARME))]),
    definir("LblConexao", "Text", juntar(texto("Controle: "), item(9))),
]

divide = ('<block type="text_split"><mutation mode="SPLIT"></mutation><field name="OP">SPLIT</field>'
          f'<value name="TEXT">{parametro("responseContent")}</value><value name="AT">{texto(";")}</value></block>')
tamanho = f'<block type="lists_length"><value name="LIST">{campos()}</value></block>'
FIRMWARE_ANTIGO = "Resposta inesperada: confira o firmware do ESP32."

BLOCOS = [
    ('<block type="global_declaration" x="20" y="20"><field name="NAME">campos</field><value name="VALUE">'
     '<block type="lists_create_with"><mutation items="0"></mutation></block></value></block>'),
    evento(100, "Relogio", "Timer", [
        definir("WebStatus", "Url", url("/app")),
        chamar_get("WebStatus"),
    ]),
    evento(260, "WebStatus", "GotText", [
        se(comparar("EQ", parametro("responseCode"), numero(200)),
           [guardar_campos(divide),
            se(comparar("GTE", tamanho, numero(9)), atualizar_tela,
               [definir("LblConexao", "Text", texto(FIRMWARE_ANTIGO))])],
           [definir("LblConexao", "Text", texto(FIRMWARE_ANTIGO))]),
    ]),
    evento(1100, "WebStatus", "TimedOut", [definir("LblConexao", "Text", texto(SEM_CONEXAO))]),
    evento(1200, "BtnLigar", "Click", [
        definir("LblMsg", "Text", texto("Enviando...")),
        definir("WebCmd", "Url", url("/rpm?valor=", ler("TxtRpm", "Text"))),
        chamar_get("WebCmd"),
    ]),
    evento(1400, "BtnParar", "Click", [
        definir("LblMsg", "Text", texto("Parando...")),
        definir("WebCmd", "Url", url("/parar")),
        chamar_get("WebCmd"),
    ]),
    evento(1560, "WebCmd", "GotText", [
        se(comparar("EQ", parametro("responseCode"), numero(200)),
           [definir("LblMsg", "Text", texto("Comando enviado"))],
           [se(comparar("EQ", parametro("responseCode"), numero(400)),
               [definir("LblMsg", "Text", texto("RPM fora da faixa (500 a 6000)"))],
               [definir("LblMsg", "Text", texto("Recusado: ligue a fonte de 12 V ou aguarde o ensaio"))])]),
    ]),
    evento(1860, "WebCmd", "TimedOut", [definir("LblMsg", "Text", texto("Comando sem resposta do ESP32"))]),
    evento(1960, "Screen1", "ErrorOccurred", [definir("LblConexao", "Text", texto(SEM_CONEXAO))]),
]

BKY = ('<xml xmlns="http://www.w3.org/1999/xhtml">' + "".join(BLOCOS) +
       f'<yacodeblocks ya-version="{YA_VERSION}" language-version="{BLOCKS_VERSION}"></yacodeblocks></xml>')
SCM = "#|\n$JSON\n" + json.dumps(FORM, ensure_ascii=False, separators=(",", ":")) + "\n|#"
PROPRIEDADES = """main=appinventor.ai_motordc.MotorDC.Screen1
name=MotorDC
assets=../assets
source=../src
build=../build
versioncode=1
versionname=1.0
useslocation=False
aname=MotorDC
"""


def validar():
    raiz = ET.fromstring(BKY)
    ns = "{http://www.w3.org/1999/xhtml}"
    usados = set()
    for m in raiz.iter(f"{ns}mutation"):
        nome = m.get("instance_name")
        if nome:
            assert TIPOS[nome] == m.get("component_type"), nome
            usados.add(nome)
    nomes = list(tipos_por_nome(TELA, {}))
    assert len(nomes) == len(set(nomes)), "nome de componente repetido"
    json.loads(SCM.split("\n")[2])
    return len(list(raiz.iter(f"{ns}block"))), len(nomes), len(usados)


if __name__ == "__main__":
    blocos, componentes, referenciados = validar()
    base = "src/appinventor/ai_motordc/MotorDC/Screen1"
    with zipfile.ZipFile(SAIDA, "w", zipfile.ZIP_DEFLATED) as z:
        z.writestr(base + ".scm", SCM)
        z.writestr(base + ".bky", BKY)
        z.writestr("youngandroidproject/project.properties", PROPRIEDADES)
    with zipfile.ZipFile(SAIDA) as z:
        assert z.testzip() is None
    print(f"{SAIDA.name}: {blocos} blocos, {componentes} componentes, {referenciados} usados nos blocos, "
          f"{SAIDA.stat().st_size} bytes")
