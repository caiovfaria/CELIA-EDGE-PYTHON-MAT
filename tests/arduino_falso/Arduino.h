// Versão falsa (só para teste no computador) de Arduino.h: imita o que o sketch usa.
#pragma once
#include <cstdint>
#include <cstdio>
#include <cmath>
#include <string>
#include <cstring>
#include <cstdlib>
#include <vector>
using std::isnan;
#define HIGH 1
#define LOW 0
#define OUTPUT 1
#define INPUT 0
extern unsigned long g_millis; extern unsigned long g_eco; extern int g_pinos[40];
inline void pinMode(int,int){} inline void digitalWrite(int p,int v){ g_pinos[p]=v; } inline void delayMicroseconds(unsigned){}
inline void delay(unsigned long ms){ g_millis += ms; }
extern std::vector<unsigned long> g_leituras;  // horário de cada leitura do HC-SR04 (uma por leitura do sketch)
inline unsigned long pulseIn(int,int,unsigned long){ g_leituras.push_back(g_millis); return g_eco; }
inline unsigned long millis(){ return g_millis; }
// String do Arduino, só com o que o sketch usa
struct String { std::string s; String(const char* c = "") : s(c) {} String(int v) : s(std::to_string(v)) {}
  String(double v, int d) { char b[32]; snprintf(b, 32, "%.*f", d, v); s = b; }
  String& operator+=(const char* c) { s += c; return *this; } String& operator+=(const String& o) { s += o.s; return *this; }
  long toInt() const { return atol(s.c_str()); } };
struct Print { std::string buf; void print(const String& v){ buf += v.s; } void print(const char* s){ buf += s; } void print(int v){ buf += std::to_string(v); } void print(unsigned long v){ buf += std::to_string(v); }
  void print(long v){ buf += std::to_string(v); } void print(unsigned v){ buf += std::to_string(v); } void print(double v,int d=2){ char b[32]; snprintf(b,32,"%.*f",d,v); buf += b; }
  void println(const char* s=""){ buf += s; buf += "\n"; } };
struct HardwareSerial : Print { void begin(long){} };
extern HardwareSerial Serial;
// FreeRTOS falso: fila de 1 lugar de verdade (copia os bytes) e tarefa que não roda sozinha.
// O teste chama cicloDeRede() no lugar da tarefa de rede, e só se a tarefa foi criada.
// g_filaFalha e g_tarefaFalha simulam falta de memória na criação.
extern bool g_filaFalha, g_tarefaFalha, g_tarefaCriada; extern unsigned g_folgaPilha;
struct FilaFalsa { std::vector<unsigned char> dados; bool cheia = false; };
typedef FilaFalsa* QueueHandle_t;
typedef void (*TaskFunction_t)(void*);
#define pdTRUE 1
#define pdFALSE 0
#define pdPASS 1
#define pdMS_TO_TICKS(ms) (ms)
inline QueueHandle_t xQueueCreate(int, size_t tam) { if (g_filaFalha) return nullptr; FilaFalsa* f = new FilaFalsa(); f->dados.resize(tam); return f; }
inline int xQueueOverwrite(QueueHandle_t f, const void* p) { memcpy(f->dados.data(), p, f->dados.size()); f->cheia = true; return pdTRUE; }
inline int xQueueReceive(QueueHandle_t f, void* p, unsigned long) { if (!f->cheia) return pdFALSE; memcpy(p, f->dados.data(), f->dados.size()); f->cheia = false; return pdTRUE; }
inline int xTaskCreate(TaskFunction_t, const char*, unsigned, void*, unsigned, void*) { if (g_tarefaFalha) return pdFALSE; g_tarefaCriada = true; return pdPASS; }
inline unsigned uxTaskGetStackHighWaterMark(void*) { return g_folgaPilha; }
inline void vTaskDelay(unsigned long) {}
inline void vQueueDelete(QueueHandle_t f) { delete f; }
