/*
  Teste do sketch inteiro com sensores simulados (sem Wokwi).
  As peças (DHT22, HC-SR04, OLED, LED) são trocadas por versões falsas da pasta
  arduino_falso/, que deixam o teste escolher quando cada sensor funciona ou falha.
  Rodar na pasta do projeto:
      g++ -std=gnu++17 -I tests/arduino_falso -o teste_sketch tests/teste_sketch.cpp && ./teste_sketch
*/
#include "Arduino.h"
#include "Wire.h"
unsigned long g_millis = 0; unsigned long g_eco = 11760; int g_pinos[40] = {0};
bool g_dhtOk = true; float g_temp = 26;
HardwareSerial Serial; TwoWire Wire;
#include "../edge/sketch.ino"   // o mesmo arquivo que roda no Wokwi
int falhas = 0;
void confere(bool ok, const char* d) { printf("%s  %s\n", ok ? "ok    " : "FALHOU", d); if (!ok) falhas++; }
const char* led() { int r = g_pinos[25], g = g_pinos[26]; return r && g ? "amarelo" : r ? "vermelho" : g ? "verde" : "apagado"; }
// roda o loop() por "ms" milissegundos simulados (mais 10 ms, para incluir a leitura que cai bem no fim)
void rodar(unsigned long ms) { unsigned long fim = g_millis + ms + 10; while (g_millis < fim) { loop(); } }
int main() {
  printf("Teste do sketch com sensores simulados\n\n");
  // 1) Partida com os dois sensores falhando
  g_eco = 0; g_dhtOk = false; setup();
  confere(std::string(led()) == "apagado", "partida sem nenhum sensor: LED apagado");
  confere(tela.tela.find("SEM DADOS") != std::string::npos && tela.tela.find("HC-SR04: sem eco") != std::string::npos
          && tela.tela.find("DHT22: sem resposta") != std::string::npos, "partida sem nenhum sensor: SEM DADOS com os dois sensores sem resposta");
  // 2) HC-SR04 volta, DHT22 continua sem resposta (partida sem DHT22)
  g_eco = 11760; rodar(5000);
  confere(std::string(led()) == "apagado" && tela.tela.find("SEM DADOS") != std::string::npos, "partida sem DHT22: continua SEM DADOS e LED apagado");
  confere(tela.tela.find("HC-SR04: ok") != std::string::npos && tela.tela.find("DHT22: sem resposta") != std::string::npos, "partida sem DHT22: mostra HC-SR04 ok e DHT22 sem resposta");
  // 3) HC-SR04 falha de novo enquanto ainda está em SEM DADOS: a tela mostra a tentativa atual
  g_eco = 0; rodar(5000);
  confere(tela.tela.find("HC-SR04: sem eco") != std::string::npos, "SEM DADOS mostra a falha atual do HC-SR04 (mesmo depois de um eco valido)");
  // 4) os dois sensores respondem: decide
  g_eco = 11760; g_dhtOk = true; rodar(5000);
  confere(std::string(led()) == "verde" && tela.tela.find("ACESSIVEL") != std::string::npos, "sensores ok: verde, ACESSIVEL");
  // 5) carro parado (50 cm) por 20 s
  g_eco = 2940; rodar(20000);
  confere(std::string(led()) == "vermelho" && tela.tela.find("EVITE") != std::string::npos, "50 cm confirmado: vermelho, EVITE");
  // 6) um eco solto de calçada livre e depois falhas por 30 s
  g_eco = 11760; rodar(5000);
  confere(tela.tela.find("Confirmando") != std::string::npos, "eco livre solto: comeca a confirmar");
  g_eco = 0; rodar(30000);
  confere(std::string(led()) == "vermelho", "eco solto + 30 s de falhas: continua vermelho (nao libera)");
  confere(tela.tela.find("Aviso: falha HC-SR04") != std::string::npos, "OLED avisa a falha do HC-SR04");
  // 7) eco volta livre: precisa de 15 s completos para liberar
  g_eco = 11760; rodar(5000);
  confere(std::string(led()) == "vermelho", "eco voltou: recomeca a contar, ainda vermelho");
  rodar(15000);
  confere(std::string(led()) == "verde", "15 s de eco livre: verde de novo");
  // 8) calor em atenção (34 °C) e depois o DHT22 falha: mantém o índice anterior
  g_temp = 34; rodar(5000);
  confere(std::string(led()) == "amarelo" && tela.tela.find("Calor 34.0C atencao") != std::string::npos, "34 C: amarelo, calor em atencao");
  g_dhtOk = false; rodar(10000);
  confere(tela.tela.find("Aviso: falha DHT22") != std::string::npos, "DHT22 falhou: aviso no OLED");
  confere(tela.tela.find("Calor 34.0C atencao") != std::string::npos && std::string(led()) == "amarelo", "DHT22 falhou: mantem 34,0 C e o LED amarelo (nao zera o calor)");
  // 9) os dois falham juntos depois da partida
  g_eco = 0; rodar(5000);
  confere(tela.tela.find("Aviso: falha sensores") != std::string::npos && std::string(led()) == "amarelo", "dois sensores falhando: aviso e ultimo status mantido");
  printf("\nTela final: %s\n%d falha(s).\n", tela.tela.c_str(), falhas);
  return falhas == 0 ? 0 : 1;
}
