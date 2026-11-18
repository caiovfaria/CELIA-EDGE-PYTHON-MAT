// Versão falsa (só para teste no computador) de Arduino.h: imita o que o sketch usa.
#pragma once
#include <cstdint>
#include <cstdio>
#include <cmath>
#include <string>
#include <cstring>
#include <cstdlib>
#include <vector>
typedef uint8_t byte;
// conta em 32 bits sem sinal: dá a mesma sequência de antes, sem estourar o long de 32 bits (Windows)
inline long random(long maximo) { static uint32_t semente = 12345; semente = (semente * 1103515245u + 12345u) & 0x7fffffffu; return (long)(semente % (uint32_t)maximo); }
using std::isnan;
#define HIGH 1
#define LOW 0
#define OUTPUT 1
#define INPUT 0
extern unsigned long g_millis; extern unsigned long g_eco; extern int g_pinos[40]; extern unsigned long g_mudouEm[40];  // quando cada pino mudou pela última vez
inline void pinMode(int,int){} inline void digitalWrite(int p,int v){ if (g_pinos[p] != v) g_mudouEm[p] = g_millis; g_pinos[p]=v; } inline void delayMicroseconds(unsigned){}
inline void delay(unsigned long ms){ g_millis += ms; }
extern std::vector<unsigned long> g_leituras;  // horário de cada pulseIn() (o poste não deve chamar nenhum)
inline unsigned long pulseIn(int,int,unsigned long){ g_leituras.push_back(g_millis); return g_eco; }
inline unsigned long millis(){ return g_millis; }
// LDR: analogRead devolve o valor que o teste escolher em g_adc (0 a 4095) e guarda o
// horário de cada leitura em g_leiturasAdc ("inline", do C++17: o teste não precisa declarar).
inline int g_adc = 2000; inline std::vector<unsigned long> g_leiturasAdc;
inline int analogRead(int){ g_leiturasAdc.push_back(g_millis); return g_adc; }
// String do Arduino, só com o que o sketch usa
struct String { std::string s; String(const char* c = "") : s(c) {} String(int v) : s(std::to_string(v)) {}
  String(double v, int d) { char b[32]; snprintf(b, 32, "%.*f", d, v); s = b; }
  String& operator+=(const char* c) { s += c; return *this; } String& operator+=(const String& o) { s += o.s; return *this; }
  long toInt() const { return atol(s.c_str()); } };
// Print falso: guarda tudo em "buf". Com "registrar" ligado, guarda também cada escrita
// separada em "escritas" (println faz duas: o texto e depois a quebra de linha, como no ESP32).
struct Print { std::string buf; bool registrar = false; std::vector<std::string> escritas;
  void escrever(const std::string& s){ buf += s; if (registrar) escritas.push_back(s); }
  void print(const String& v){ escrever(v.s); } void print(const char* s){ escrever(s); } void print(int v){ escrever(std::to_string(v)); } void print(unsigned long v){ escrever(std::to_string(v)); }
  void print(long v){ escrever(std::to_string(v)); } void print(unsigned v){ escrever(std::to_string(v)); } void print(double v,int d=2){ char b[32]; snprintf(b,32,"%.*f",d,v); escrever(b); }
  void println(const char* s=""){ escrever(s); escrever("\r\n"); } };
struct HardwareSerial : Print { void begin(long){} };
extern HardwareSerial Serial;
// FreeRTOS falso: fila de 1 lugar de verdade (copia os bytes) e tarefas que não rodam sozinhas.
// O teste chama cicloDeRede() e cicloDaCamera() no lugar das tarefas, e só se cada uma foi criada.
// g_filaFalha, g_filasAteFalhar e g_tarefaQueFalha simulam falta de memória na criação.
extern bool g_filaFalha, g_tarefaRede, g_tarefaCamera; extern const char* g_tarefaQueFalha;
extern unsigned g_folgaPilha; extern int g_filasAteFalhar;  // g_filasAteFalhar = n: a n-ésima fila criada falha
struct FilaFalsa { std::vector<unsigned char> dados; bool cheia = false; };
typedef FilaFalsa* QueueHandle_t;
typedef void (*TaskFunction_t)(void*);
#define pdTRUE 1
#define pdFALSE 0
#define pdPASS 1
#define pdMS_TO_TICKS(ms) (ms)
inline QueueHandle_t xQueueCreate(int, size_t tam) { if (g_filaFalha) return nullptr; if (g_filasAteFalhar > 0 && --g_filasAteFalhar == 0) return nullptr; FilaFalsa* f = new FilaFalsa(); f->dados.resize(tam); return f; }
inline int xQueueOverwrite(QueueHandle_t f, const void* p) { memcpy(f->dados.data(), p, f->dados.size()); f->cheia = true; return pdTRUE; }
inline int xQueueReceive(QueueHandle_t f, void* p, unsigned long) { if (!f->cheia) return pdFALSE; memcpy(p, f->dados.data(), f->dados.size()); f->cheia = false; return pdTRUE; }
inline int xTaskCreate(TaskFunction_t, const char* nome, unsigned, void*, unsigned, void*) {
  if (g_tarefaQueFalha && strcmp(nome, g_tarefaQueFalha) == 0) return pdFALSE;
  if (strcmp(nome, "rede") == 0) g_tarefaRede = true;
  if (strcmp(nome, "camera") == 0) g_tarefaCamera = true;
  return pdPASS; }
inline unsigned uxTaskGetStackHighWaterMark(void*) { return g_folgaPilha; }
inline void vTaskDelay(unsigned long) {}
inline void vQueueDelete(QueueHandle_t f) { delete f; }
