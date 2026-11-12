// Versão falsa (só para teste no computador) de Arduino.h: imita o que o sketch usa.
#pragma once
#include <cstdint>
#include <cstdio>
#include <cmath>
#include <string>
using std::isnan;
#define HIGH 1
#define LOW 0
#define OUTPUT 1
#define INPUT 0
extern unsigned long g_millis; extern unsigned long g_eco; extern int g_pinos[40];
inline void pinMode(int,int){} inline void digitalWrite(int p,int v){ g_pinos[p]=v; } inline void delayMicroseconds(unsigned){}
inline void delay(unsigned long ms){ g_millis += ms; }
inline unsigned long pulseIn(int,int,unsigned long){ return g_eco; }
inline unsigned long millis(){ return g_millis; }
struct Print { std::string buf; void print(const char* s){ buf += s; } void print(int v){ buf += std::to_string(v); } void print(unsigned long v){ buf += std::to_string(v); }
  void print(long v){ buf += std::to_string(v); } void print(unsigned v){ buf += std::to_string(v); } void print(double v,int d=2){ char b[32]; snprintf(b,32,"%.*f",d,v); buf += b; }
  void println(const char* s=""){ buf += s; buf += "\n"; } };
struct HardwareSerial : Print { void begin(long){} };
extern HardwareSerial Serial;
