// Versão falsa (só para teste no computador) de HTTPClient.h: guarda o endereço e os horários
// de cada pedido e devolve a resposta que o teste escolher, sem acessar a internet.
// Um pedido "lento" demora g_httpDemoraMs. Se o pedido saiu da tarefa de rede (g_naTarefaDeRede),
// o loop() do sketch continua rodando enquanto ele espera, como no ESP32. Se o pedido saísse de
// dentro do loop(), o tempo passa sem leitura nenhuma, e o teste das leituras a cada 5 s falha.
#pragma once
#include "Arduino.h"
#include <vector>
extern std::string g_url; extern std::vector<unsigned long> g_inicios, g_fins;
extern int g_httpCodigo; extern std::string g_httpResposta; extern unsigned long g_httpDemoraMs; extern bool g_beginOk; extern bool g_naTarefaDeRede; extern int g_pedidosForaDaRede;
extern float g_tempNoPedido; extern unsigned long g_tempMudaEm; extern float g_tempNova;  // muda o calor 1 s depois de um pedido começar
void loop();
struct HTTPClient { void setTimeout(uint16_t) {} void setConnectTimeout(int32_t) {}
  bool begin(const String& u) { if (!g_beginOk) return false; g_url = u.s; return true; }
  int GET() { if (!g_naTarefaDeRede) g_pedidosForaDaRede++;  // pedido saiu de dentro do loop(): não pode
    if (g_tempNoPedido > 0) { g_tempMudaEm = g_millis + 1000; g_tempNova = g_tempNoPedido; g_tempNoPedido = 0; }
    g_inicios.push_back(g_millis); unsigned long fim = g_millis + g_httpDemoraMs; bool daRede = g_naTarefaDeRede;
    while (g_millis < fim) { if (daRede) { g_naTarefaDeRede = false; loop(); g_naTarefaDeRede = true; } else g_millis += 10; }
    g_fins.push_back(g_millis); return g_httpCodigo; }
  String getString() { return String(g_httpCodigo == 200 ? g_httpResposta.c_str() : ""); } void end() {} };
