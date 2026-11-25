# Monitoramento de motor DC e controle via Wi-Fi (ESP32)

Projeto da disciplina **DPEE1107 – Sistemas Microcontrolados** (UFSM), apresentado em 25/11/2026.
Equipe: Pedro Felipe Hoffmann e Lucas Garcia Tommaselli Chuba Machado.

Bancada didática com um motor DC de 9 V e hélice, monitorado e controlado por um ESP32:

- **Medidas**: rotação (Hall 3144E), temperatura da carcaça (LM35), vibração (MPU6050) e corrente (ACS712, só como proteção).
- **Controle PI** da rotação por PWM na ponte H L298N, com feedforward, anti-windup e pulso de partida (faixa útil de ~550 a ~5200 RPM, erro ≤ 2 % acima de 1500 RPM).
- **Alarmes locais** com histerese e persistência: temperatura, corrente ou vibração altas desligam o motor; erro de rotação avisa.
- **App Android** (MIT App Inventor) e **painel web** servidos pelo próprio ESP32, inclusive na rede Wi-Fi própria `MotorDC`.
- **Thinger.io**: dashboard ao vivo e Data Bucket com as médias de cada minuto.
- Sem Wi-Fi, aquisição, controle e alarmes continuam: a rede roda numa tarefa no core 0 e o controle no core 1.

## Estrutura

| Pasta | Conteúdo |
|---|---|
| `firmware/monitor_motor/` | Sketch do Arduino IDE (core esp32 3.3.8), um módulo por sensor/função |
| `firmware/logs_ensaios/` | Logs dos ensaios automáticos (CSV), baixados com `firmware/baixar_logs.py` |
| `app/` | App do celular (`MotorDC.aia`), gerado por `gerar_aia.py` |
| `hardware/placa_conexoes/` | Placa de conexões em KiCad (não usada na montagem final, feita em placa perfurada) |
| `*.stl`, `*.SLDPRT` | Hélice e suporte do motor |
| `APRESENTACAO.md` | Roteiro da apresentação |
| `CLAUDE.md` | Notas técnicas completas: ligações, decisões, resultados dos ensaios e problemas encontrados |

## Como compilar

1. Copie `firmware/monitor_motor/secrets.example.h` para `secrets.h` e preencha as redes Wi-Fi e as credenciais do Thinger.io (sem o arquivo, o firmware roda só com as funções locais).
2. Instale a biblioteca `thinger.io` (2.49.0) e compile para a placa "ESP32 Dev Module".

## Limitações conhecidas

- O ACS712 não tem resolução para a corrente deste motor (~100–170 mA): serve só como proteção contra sobrecorrente.
- Buzzer e display LCD ficaram de fora por defeito nos componentes: o alarme local é visual (LEDs).
- Algumas redes bloqueiam a conexão TLS do Thinger.io (porta 25202); na apresentação foi usado o hotspot do celular.
