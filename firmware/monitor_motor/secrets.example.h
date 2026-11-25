// Modelo de credenciais. Copie para "secrets.h" (mesma pasta) e preencha.
// O secrets.h não deve ir para o git nem ser enviado para ninguém.
#pragma once

// Wi-Fi com internet (necessário para o Thinger.io). Pode ficar vazio para usar só a rede própria.
#define WIFI_SSID     ""
#define WIFI_SENHA    ""

// Redes extras (opcional): o ESP32 conecta na de melhor sinal entre as que estiverem ao alcance.
// Precisa ser 2,4 GHz com senha simples. Rede com usuário e senha (como eduroam) não funciona.
#define WIFI_SSID_2   ""
#define WIFI_SENHA_2  ""
#define WIFI_SSID_3   ""
#define WIFI_SENHA_3  ""

// Thinger.io: console → Devices → seu dispositivo
#define THINGER_USUARIO     ""
#define THINGER_DEVICE_ID   ""
#define THINGER_CREDENCIAL  ""
#define THINGER_BUCKET      ""   // ID do Data Bucket

// Senha da rede própria do ESP32 ("MotorDC"). Mínimo de 8 caracteres.
#define AP_SENHA            ""
