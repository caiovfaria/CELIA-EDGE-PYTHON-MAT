// Versão falsa (só para teste no computador) de DHTesp.h: imita o que o sketch usa.
#pragma once
#include "Arduino.h"
struct TempAndHumidity { float temperature; float humidity; };
extern bool g_dhtOk; extern float g_temp;
class DHTesp { public: enum DHT_MODEL_t { AUTO_DETECT, DHT11, DHT22, AM2302, RHT03 }; enum DHT_ERROR_t { ERROR_NONE = 0, ERROR_TIMEOUT, ERROR_CHECKSUM };
 void setup(uint8_t, DHT_MODEL_t){} TempAndHumidity getTempAndHumidity(){ return g_dhtOk ? TempAndHumidity{g_temp,50} : TempAndHumidity{NAN,NAN}; }
 DHT_ERROR_t getStatus(){ return g_dhtOk ? ERROR_NONE : ERROR_TIMEOUT; } float computeHeatIndex(float t,float, bool=false){ return t; } };
