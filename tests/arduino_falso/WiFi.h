// Versão falsa (só para teste no computador) de WiFi.h: o teste liga e desliga a rede.
#pragma once
#include "Arduino.h"
#define WL_CONNECTED 3
#define WL_DISCONNECTED 6
extern bool g_wifiOk; extern int g_reconexoes;
struct WiFiClass { void begin(const char*, const char*, int = 0) {} int status() { return g_wifiOk ? WL_CONNECTED : WL_DISCONNECTED; }
  bool reconnect() { g_reconexoes++; return true; } };
extern WiFiClass WiFi;
