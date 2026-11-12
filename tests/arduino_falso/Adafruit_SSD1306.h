// Versão falsa (só para teste no computador) de Adafruit_SSD1306.h: imita o que o sketch usa.
#pragma once
#include "Wire.h"
#define SSD1306_SWITCHCAPVCC 0x02
#define SSD1306_WHITE 1
struct Adafruit_SSD1306 : Print { std::string tela; Adafruit_SSD1306(int,int,TwoWire*,int){} bool begin(int,int){return true;} void clearDisplay(){ buf.clear(); } void setTextColor(int){} void setTextSize(int){} void setCursor(int,int y){ if (y) buf += " | "; } void display(){ tela = buf; } };
