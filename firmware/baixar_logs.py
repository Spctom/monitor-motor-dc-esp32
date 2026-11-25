#!/usr/bin/env python3
"""Baixa do ESP32 para o computador os logs dos ensaios gravados na flash.

Como usar:
  1. Desligue a fonte de 12 V.
  2. Ligue o ESP32 no USB e feche o Serial Monitor da Arduino IDE.
  3. Rode:  python3 firmware/baixar_logs.py

Os arquivos vão para firmware/logs_ensaios/. Nada é apagado do ESP32.
Só usa a biblioteca padrão do Python (macOS/Linux).
"""

import argparse
import glob
import os
import re
import select
import sys
import termios
import time
import tty

PADROES_PORTA = ("/dev/cu.usbserial*", "/dev/cu.SLAB_USBtoUART*", "/dev/cu.wchusbserial*", "/dev/cu.usbmodem*",
                 "/dev/ttyUSB*", "/dev/ttyACM*")
PASTA_PADRAO = os.path.join(os.path.dirname(os.path.abspath(__file__)), "logs_ensaios")
RE_INICIO = re.compile(rb"=== LOGS INICIO ===\r?\n")  # o println do Arduino termina em \r\n
RE_FIM = re.compile(rb"=== LOGS FIM (\d+) ===\r?\n")
RE_ARQUIVO = re.compile(rb"=== ARQUIVO (e\d{4}\.csv) ===\r?\n")
RE_FIM_ARQUIVO = re.compile(rb"=== ARQUIVO FIM (\d+) (\d+) ===\r?\n")
PREFIXOS_RESUMO = ("# ABORTADO", "# INTERROMPIDO", "# RESULTADO", "# DUTY_MINIMO", "# GERAL")


def achar_porta():
    portas = sorted({p for padrao in PADROES_PORTA for p in glob.glob(padrao)})
    if len(portas) == 1:
        return portas[0]
    if not portas:
        sys.exit("ESP32 não encontrado. Confira o cabo USB (precisa ser de dados) e o driver CP210x/CH340.")
    sys.exit("Mais de uma porta encontrada, escolha com --porta:\n  " + "\n  ".join(portas))


def abrir(porta):
    try:
        fd = os.open(porta, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    except OSError as erro:
        sys.exit(f"Não consegui abrir {porta}: {erro}\nO Serial Monitor da Arduino IDE está aberto? Feche e tente de novo.")
    tty.setraw(fd)
    atributos = termios.tcgetattr(fd)
    atributos[4] = atributos[5] = termios.B115200
    atributos[2] |= termios.CLOCAL | termios.CREAD
    termios.tcsetattr(fd, termios.TCSANOW, atributos)
    return fd


def ler_ate(fd, buffer, condicao, sem_dados_s, total_s):
    """Acumula bytes até condicao(buffer), até ficar sem_dados_s sem receber nada ou até total_s.

    O limite total é necessário porque a telemetria do ESP32 chega a cada 0,5 s e nunca deixa
    o tempo "sem dados" estourar.
    """
    ultimo = time.monotonic()
    fim = ultimo + total_s
    while not condicao(buffer):
        agora = time.monotonic()
        restante = min(sem_dados_s - (agora - ultimo), fim - agora)
        if restante <= 0:
            return False
        pronto, _, _ = select.select([fd], [], [], min(restante, 0.2))
        if pronto:
            try:
                dados = os.read(fd, 4096)
            except BlockingIOError:
                continue
            if dados:
                buffer.extend(dados)
                ultimo = time.monotonic()
    return True


def pedir_logs(fd):
    """O ESP32 pode reiniciar ao abrir a porta: tenta algumas vezes até ele responder."""
    time.sleep(0.3)
    for _ in range(6):
        buffer = bytearray()
        ler_ate(fd, buffer, lambda _b: False, 1.5, 1.5)  # espera o boot e descarta a telemetria
        termios.tcflush(fd, termios.TCIFLUSH)
        buffer.clear()
        os.write(fd, b"b\n")
        if not ler_ate(fd, buffer, lambda b: RE_INICIO.search(b) is not None, 3.0, 3.0):
            continue
        del buffer[: RE_INICIO.search(buffer).end()]
        # 180 s cobre a flash inteira a 115200 baud.
        if not ler_ate(fd, buffer, lambda b: RE_FIM.search(b) is not None, 6.0, 180.0):
            sys.exit("A transferência parou no meio. Rode de novo; os logs continuam no ESP32.")
        return bytes(buffer[: RE_FIM.search(buffer).end()])
    sys.exit("O ESP32 não respondeu. Ele está com o firmware novo? Aperte EN e tente de novo.")


def separar(dados):
    """Devolve [(nome, conteúdo)] conferindo tamanho e soma de cada arquivo."""
    arquivos = []
    pos = 0
    while True:
        cabecalho = RE_ARQUIVO.search(dados, pos)
        if not cabecalho:
            break
        fim = RE_FIM_ARQUIVO.search(dados, cabecalho.end())
        if not fim:
            sys.exit(f"{cabecalho.group(1).decode()} chegou incompleto. Rode de novo.")
        tamanho, soma = int(fim.group(1)), int(fim.group(2))
        conteudo = dados[cabecalho.end():cabecalho.end() + tamanho]
        if len(conteudo) != tamanho or (sum(conteudo) & 0xFFFF) != soma:
            sys.exit(f"{cabecalho.group(1).decode()} chegou com erro. Rode de novo; os logs continuam no ESP32.")
        arquivos.append((cabecalho.group(1).decode(), conteudo))
        pos = fim.end()
    esperado = int(RE_FIM.search(dados).group(1))
    if len(arquivos) != esperado:
        sys.exit(f"Esperava {esperado} arquivo(s) e chegaram {len(arquivos)}. Rode de novo.")
    return arquivos


def salvar(pasta, nome_esp, conteudo):
    """Nunca sobrescreve: se o número repetiu com outro conteúdo, salva como _2, _3..."""
    os.makedirs(pasta, exist_ok=True)
    base = "ensaio_" + nome_esp[1:5]
    for i in range(1, 1000):
        nome = base + (f"_{i}" if i > 1 else "") + ".csv"
        caminho = os.path.join(pasta, nome)
        if not os.path.exists(caminho):
            with open(caminho, "wb") as f:
                f.write(conteudo)
            return caminho, True
        with open(caminho, "rb") as f:
            if f.read() == conteudo:
                return caminho, False
    sys.exit("Arquivos demais com o mesmo número em " + pasta)


def main():
    parser = argparse.ArgumentParser(description="Baixa os logs dos ensaios do ESP32.")
    parser.add_argument("--porta", help="porta serial (padrão: detecta sozinho)")
    parser.add_argument("--pasta", default=PASTA_PADRAO, help="onde salvar")
    args = parser.parse_args()

    porta = args.porta or achar_porta()
    print(f"Lendo {porta}...")
    fd = abrir(porta)
    try:
        arquivos = separar(pedir_logs(fd))
    finally:
        os.close(fd)

    if not arquivos:
        print("Nenhum ensaio gravado no ESP32.")
        return
    novos = 0
    ultimo = None
    for nome, conteudo in arquivos:
        caminho, novo = salvar(args.pasta, nome, conteudo)
        novos += novo
        ultimo = (caminho, conteudo)
        print(f"  {'novo ' if novo else 'já tinha'} {os.path.relpath(caminho)}")
    print(f"{len(arquivos)} ensaio(s) no ESP32, {novos} novo(s).\n")

    print(f"Resumo de {os.path.basename(ultimo[0])}:")
    linhas = ultimo[1].decode("utf-8", "replace").splitlines()
    resumo = [l[2:] for l in linhas if l.startswith(PREFIXOS_RESUMO)]
    print("  " + "\n  ".join(resumo) if resumo else "  (o ensaio não chegou ao resumo)")


if __name__ == "__main__":
    main()
