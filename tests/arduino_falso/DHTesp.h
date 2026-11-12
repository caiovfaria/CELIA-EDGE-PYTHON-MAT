// Versão falsa (só para teste no computador) de DHTesp.h: imita o que o sketch usa.
#pragma once
#include "Arduino.h"
struct TempAndHumidity { float temperature; float humidity; };
extern bool g_dhtOk; extern float g_temp;
extern unsigned long g_tempMudaEm; extern float g_tempNova;  // troca a temperatura num horário marcado
class DHTesp { public: enum DHT_MODEL_t { AUTO_DETECT, DHT11, DHT22, AM2302, RHT03 }; enum DHT_ERROR_t { ERROR_NONE = 0, ERROR_TIMEOUT, ERROR_CHECKSUM };
 void setup(uint8_t, DHT_MODEL_t){} TempAndHumidity getTempAndHumidity(){ if (g_tempMudaEm && g_millis >= g_tempMudaEm) { g_temp = g_tempNova; g_tempMudaEm = 0; } return g_dhtOk ? TempAndHumidity{g_temp,50} : TempAndHumidity{NAN,NAN}; }
 DHT_ERROR_t getStatus(){ return g_dhtOk ? ERROR_NONE : ERROR_TIMEOUT; } float computeHeatIndex(float t,float, bool=false){ return t; } };
