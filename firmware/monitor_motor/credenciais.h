// Usa o secrets.h se ele existir. Sem ele, o firmware compila e roda só com as funções locais.
#pragma once

#if __has_include("secrets.h")
#include "secrets.h"
#endif

#ifndef WIFI_SSID
#define WIFI_SSID ""
#endif
#ifndef WIFI_SENHA
#define WIFI_SENHA ""
#endif
#ifndef WIFI_SSID_2
#define WIFI_SSID_2 ""
#endif
#ifndef WIFI_SENHA_2
#define WIFI_SENHA_2 ""
#endif
#ifndef WIFI_SSID_3
#define WIFI_SSID_3 ""
#endif
#ifndef WIFI_SENHA_3
#define WIFI_SENHA_3 ""
#endif
#ifndef THINGER_USUARIO
#define THINGER_USUARIO ""
#endif
#ifndef THINGER_DEVICE_ID
#define THINGER_DEVICE_ID ""
#endif
#ifndef THINGER_CREDENCIAL
#define THINGER_CREDENCIAL ""
#endif
#ifndef THINGER_BUCKET
#define THINGER_BUCKET ""
#endif
#ifndef AP_SENHA
#define AP_SENHA ""
#endif
