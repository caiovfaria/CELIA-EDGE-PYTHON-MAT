// Versão falsa (só para teste no computador) de WiFi.h: o teste liga e desliga a rede.
#pragma once
#include "Arduino.h"
#define WL_CONNECTED 3
#define WL_DISCONNECTED 6
extern bool g_wifiOk; extern int g_reconexoes; extern int g_quedasWifi;  // conta as vezes que o Wi-Fi foi visto fora
struct WiFiClass { void begin(const char*, const char*, int = 0) {} int status() { if (!g_wifiOk) g_quedasWifi++; return g_wifiOk ? WL_CONNECTED : WL_DISCONNECTED; }
  bool reconnect() { g_reconexoes++; return true; } };
extern WiFiClass WiFi;
extern int g_mqttStops;  // vezes que a conexão do MQTT foi fechada aqui no poste (stop)
class WiFiClient { public: int paradas = 0; void stop() { paradas++; g_mqttStops++; } };  // conexão de internet do MQTT
