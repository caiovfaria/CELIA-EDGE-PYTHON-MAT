/*
  Teste do sketch inteiro com sensores simulados (sem Wokwi).
  As peças (DHT22, HC-SR04, OLED, LED, Wi-Fi e ThingSpeak) são trocadas por versões
  falsas da pasta arduino_falso/, que deixam o teste escolher quando cada sensor
  funciona ou falha, se o Wi-Fi está no ar e o que o ThingSpeak responde.
  Rodar na pasta do projeto:
      g++ -std=gnu++17 -I tests/arduino_falso -o teste_sketch tests/teste_sketch.cpp && ./teste_sketch
*/
#include "Arduino.h"
#include "Wire.h"
#include "WiFi.h"
#include "HTTPClient.h"
unsigned long g_millis = 0; unsigned long g_eco = 11760; int g_pinos[40] = {0};
bool g_dhtOk = true; float g_temp = 26;
bool g_wifiOk = true; int g_reconexoes = 0;
std::vector<unsigned long> g_leituras;
std::string g_url; std::vector<unsigned long> g_inicios, g_fins; int g_httpCodigo = 200; std::string g_httpResposta = "1";
unsigned long g_httpDemoraMs = 0; bool g_beginOk = true; bool g_naTarefaDeRede = false; int g_pedidosForaDaRede = 0;
bool g_filaFalha = false, g_tarefaFalha = false, g_tarefaCriada = false; unsigned g_folgaPilha = 4800;
unsigned long g_tempMudaEm = 0; float g_tempNova = 0; float g_tempNoPedido = 0;
HardwareSerial Serial; TwoWire Wire; WiFiClass WiFi;
#include "../edge/sketch.ino"   // o mesmo arquivo que roda no Wokwi
int falhas = 0;
void confere(bool ok, const char* d) { printf("%s  %s\n", ok ? "ok    " : "FALHOU", d); fflush(stdout); if (!ok) falhas++; }
const char* led() { int r = g_pinos[25], g = g_pinos[26]; return r && g ? "amarelo" : r ? "vermelho" : g ? "verde" : "apagado"; }
// roda o loop() e a tarefa de rede por "ms" milissegundos simulados (mais 10 ms, para incluir a
// leitura que cai bem no fim). No ESP32 as duas rodam ao mesmo tempo; aqui elas se revezam.
// uma volta da tarefa de rede (só se ela foi criada no setup(), como no ESP32)
void rede() { if (!g_tarefaCriada) return; g_naTarefaDeRede = true; cicloDeRede(); g_naTarefaDeRede = false; }
void rodar(unsigned long ms) { unsigned long fim = g_millis + ms + 10; while (g_millis < fim) { loop(); rede(); } }
bool tem(const std::string& texto, const char* trecho) { return texto.find(trecho) != std::string::npos; }
// tamanho do serial agora: para olhar só o que foi escrito depois deste ponto
size_t marca() { return Serial.buf.size(); }
std::string serialDesde(size_t m) { return Serial.buf.substr(m); }
int main() {
  printf("Teste do sketch com sensores simulados\n\n");
  // 1) Partida com os dois sensores falhando
  g_eco = 0; g_dhtOk = false; setup();
  confere(std::string(led()) == "apagado", "partida sem nenhum sensor: LED apagado");
  confere(tela.tela.find("SEM DADOS") != std::string::npos && tela.tela.find("HC-SR04: sem eco") != std::string::npos
          && tela.tela.find("DHT22: sem resposta") != std::string::npos, "partida sem nenhum sensor: SEM DADOS com os dois sensores sem resposta");
  confere(g_tarefaCriada && tem(Serial.buf, "Tarefa de rede ligada"), "setup criou a fila e a tarefa de rede e avisou no serial");
  // Etapa 2: com SEM DADOS não há status, então nada vai para o ThingSpeak (nem com chave e Wi-Fi)
  THINGSPEAK_CHAVE = "CHAVE_TESTE"; size_t m0 = marca(); enviarAoThingSpeak();
  confere(g_inicios.empty() && tem(serialDesde(m0), "ainda sem dados"), "SEM DADOS: nao envia nada ao ThingSpeak");
  THINGSPEAK_CHAVE = CHAVE_DE_EXEMPLO;  // volta para a chave de exemplo, como fica no GitHub
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

  // ---------- Etapa 2: envio ao ThingSpeak ----------
  // 10) com a chave de exemplo (como está no GitHub), o poste funciona mas não envia nada
  confere(g_inicios.empty() && tem(Serial.buf, "cole a Write API Key"), "chave de exemplo: nenhum envio, aviso no serial");
  // sensores de volta ao normal (26 C, 200 cm)
  g_eco = 11760; g_dhtOk = true; g_temp = 26; rodar(5000);
  confere(std::string(led()) == "verde", "sensores de volta: verde");

  // 11) chave colada, mas sem Wi-Fi: não envia e o poste segue decidindo. Durante 1 minuto, deixa o
  //     próprio ESP32 tentar reconectar; só depois disso o sketch força uma nova tentativa.
  THINGSPEAK_CHAVE = "CHAVE_TESTE"; g_wifiOk = false; size_t m1 = marca();
  g_eco = 2940; rodar(25000);  // carro parado em 50 cm durante a queda do Wi-Fi
  confere(g_inicios.empty() && tem(serialDesde(m1), "sem Wi-Fi, envio pulado"), "sem Wi-Fi: nao envia");
  confere(g_reconexoes == 0, "sem Wi-Fi ha 25 s: ainda nao forca reconexao (deixa o ESP32 tentar sozinho)");
  confere(std::string(led()) == "vermelho", "sem Wi-Fi: o poste continua decidindo (50 cm confirmado, vermelho)");
  confere(tem(serialDesde(m1), "Wi-Fi caiu"), "sem Wi-Fi: avisa no serial que o Wi-Fi caiu");
  rodar(40000);  // 65 s sem Wi-Fi no total
  confere(g_reconexoes == 1 && tem(serialDesde(m1), "1 minuto sem conexao"), "sem Wi-Fi ha 1 min: forca uma reconexao (so uma em 65 s)");

  // 12) Wi-Fi volta: envia os 6 campos, com o status igual à cor do LED
  g_wifiOk = true; size_t m2 = marca(); rodar(20000);
  confere(g_inicios.size() == 1, "Wi-Fi voltou: 1 envio em 20 s");
  confere(tem(serialDesde(m2), "Wi-Fi conectado"), "Wi-Fi voltou: avisa no serial");
  confere(tem(g_url, "http://api.thingspeak.com/update?api_key=CHAVE_TESTE"), "endereco do ThingSpeak com a chave");
  confere(tem(g_url, "&field1=26.0&field2=50.0&field3=26.0&field4=50&field5=2&field6=1"),
          "6 campos na ordem: T 26.0, U 50.0, IC 26.0, 50 cm, status 2 (vermelho), motivo 1 (calcada)");
  confere(tem(serialDesde(m2), "registro 1 gravado (status 2, motivo 1)"), "serial confirma o registro gravado");

  // 13) cadência: 20 s contados do FIM de uma tentativa até o início da seguinte
  g_eco = 11760; rodar(60000);
  bool cadenciaOk = g_inicios.size() >= 4;
  for (size_t i = 1; i < g_inicios.size(); i++) {
    unsigned long d = g_inicios[i] - g_fins[i - 1];
    if (d < 20000 || d > 20100) cadenciaOk = false;
  }
  confere(cadenciaOk, "20 s entre o fim de um envio e o inicio do seguinte");
  confere(std::string(led()) == "verde" && tem(g_url, "&field4=200&field5=0&field6=0"), "trecho liberado: o envio acompanha o LED verde (status 0)");

  // 14) DHT22 falhando: os campos 1, 2 e 3 ficam de fora (o gráfico mostra o buraco)
  g_dhtOk = false; rodar(20000);
  confere(!tem(g_url, "field1") && !tem(g_url, "field2") && !tem(g_url, "field3") && tem(g_url, "&field4=200&field5=0&field6=0"),
          "DHT22 falhando: envia so distancia, status e motivo");
  // 15) HC-SR04 falhando: o campo 4 fica de fora
  g_dhtOk = true; g_eco = 0; rodar(20000);
  confere(tem(g_url, "&field1=26.0") && !tem(g_url, "field4") && tem(g_url, "&field5=0&field6=0"), "HC-SR04 falhando: envia sem o campo 4");

  // 16) os dois sensores falhando: nenhum envio (o status seria só repetição do antigo)
  g_dhtOk = false; size_t n16 = g_inicios.size(); size_t m16 = marca(); rodar(45000);
  confere(g_inicios.size() == n16 && tem(serialDesde(m16), "os dois sensores falharam, envio pulado"), "dois sensores falhando: nenhum envio em 45 s");
  g_dhtOk = true; g_eco = 11760; rodar(25000);
  confere(g_inicios.size() > n16 && tem(g_url, "&field1=26.0&field2=50.0&field3=26.0&field4=200&field5=0&field6=0"), "sensores voltaram: envia de novo, com os 6 campos");

  // 17) pedido lento (7 s) seguido de um rápido: as leituras não param e o intervalo conta do fim.
  //     1 s depois de o pedido começar, o calor sobe para 34 °C: o LED muda durante o pedido, e o
  //     registro lento leva o status da leitura que foi capturada antes dele (0); o seguinte leva o novo (1).
  size_t k = g_inicios.size(); size_t m17 = marca();
  g_httpDemoraMs = 7000; g_tempNoPedido = 34;
  while (g_inicios.size() == k) { loop(); rede(); }  // roda até sair o pedido lento (ele espera 7 s)
  g_httpDemoraMs = 0;
  confere(std::string(led()) == "amarelo" && tem(g_url, "&field5=0&field6=0"),
          "calor subiu durante o pedido lento: LED ja amarelo, registro lento com a leitura capturada (status 0)");
  unsigned long ini17 = g_inicios[k], fim17 = g_fins[k];
  bool leuDurante = false;
  for (unsigned long t : g_leituras) if (t > ini17 && t < fim17) leuDurante = true;
  confere(fim17 - ini17 >= 7000 && leuDurante, "pedido lento de 7 s: o loop leu os sensores enquanto o pedido esperava");
  confere(tem(serialDesde(m17), "t="), "pedido lento: linhas de leitura continuaram saindo no serial");
  rodar(25000);
  confere(g_inicios.size() == k + 2 && g_inicios[k + 1] - fim17 >= 20000, "depois do pedido lento, o seguinte so sai 20 s apos o FIM dele");
  confere(tem(g_url, "&field1=34.0") && tem(g_url, "&field5=1&field6=2"), "envio seguinte leva o status novo (1, motivo calor)");
  g_temp = 26; rodar(5000);

  // 18) não dá para preparar o pedido (begin falha): avisa e tenta de novo depois
  g_beginOk = false; size_t n18 = g_inicios.size(); size_t m18 = marca(); rodar(20000);
  confere(g_inicios.size() == n18 && tem(serialDesde(m18), "nao deu para preparar o pedido"), "begin falhou: nenhum pedido, aviso no serial");
  g_beginOk = true;

  // 19) resposta diferente de 200 (por exemplo, 400) e resposta "0" (canal recusou)
  g_httpCodigo = 400; size_t m19 = marca(); rodar(20000);
  confere(tem(serialDesde(m19), "falha no envio (codigo 400)"), "HTTP 400: aviso de falha com o codigo");
  g_httpCodigo = 200; g_httpResposta = "0"; size_t m20 = marca(); rodar(20000);
  confere(tem(serialDesde(m20), "o canal recusou"), "ThingSpeak recusou (resposta 0): aviso no serial");

  // 20) sem resposta (codigo -1) depois de esperar os 5 s do limite: avisa e o poste não trava
  g_httpResposta = "1"; g_httpCodigo = -1; g_httpDemoraMs = 5000; size_t m21 = marca();
  g_eco = 2940; rodar(45000);
  confere(tem(serialDesde(m21), "falha no envio (codigo -1)"), "sem resposta do ThingSpeak: aviso de falha no serial");
  confere(std::string(led()) == "vermelho", "sem resposta do ThingSpeak: o poste nao trava (50 cm confirmado, vermelho)");
  g_httpCodigo = 200; g_httpDemoraMs = 0;

  // 21) sem leitura nova desde o último envio: não manda dado velho
  size_t n21 = g_inicios.size();
  while (g_inicios.size() == n21) { loop(); rede(); }  // roda até sair um envio normal
  fimUltimaTentativa = millis() - INTERVALO_ENVIO_MS; size_t n22 = g_inicios.size(); size_t m22 = marca();
  rede();  // tenta de novo logo em seguida, sem o loop() ter lido os sensores
  confere(g_inicios.size() == n22 && tem(serialDesde(m22), "sem leitura nova"), "sem leitura nova: nao reenvia o pacote antigo");

  // 22) a tarefa de rede mede a sobra da pilha (no ESP32, em bytes) e avisa quando ela diminui
  confere(tem(Serial.buf, "menor sobra de pilha ate agora: 4800 bytes"), "tarefa de rede mede e avisa a sobra da pilha");

  // 23) até aqui: nenhuma pausa maior que 5 s entre duas leituras (nem com pedido lento)
  unsigned long maiorPausa = 0;
  for (size_t i = 1; i < g_leituras.size(); i++) if (g_leituras[i] - g_leituras[i - 1] > maiorPausa) maiorPausa = g_leituras[i] - g_leituras[i - 1];
  char d22[100]; snprintf(d22, sizeof d22, "leituras a cada 5 s durante todo o teste (maior pausa: %lu ms)", maiorPausa);
  confere(maiorPausa <= 5010, d22);

  // 24) nenhum pedido HTTP saiu de dentro do loop(): todos vieram da tarefa de rede
  char d23[100]; snprintf(d23, sizeof d23, "todos os %zu pedidos sairam da tarefa de rede, nenhum do loop()", g_inicios.size());
  confere(g_pedidosForaDaRede == 0 && !g_inicios.empty(), d23);

  // 25) a chave nunca aparece no serial (os prints de evidência podem ir para o README)
  confere(!tem(Serial.buf, "CHAVE_TESTE"), "a chave nunca aparece no monitor serial");

  // 26) falta de memória para a fila na partida: avisa, não cria a tarefa e o poste segue sem envio
  g_filaFalha = true; g_tarefaCriada = false; size_t n25 = g_inicios.size(); size_t m25 = marca();
  bool ok25 = iniciarEnvio(); guardarParaEnvio();
  g_eco = 11760; rodar(30000);
  confere(!ok25 && filaEnvio == NULL && !g_tarefaCriada && tem(serialDesde(m25), "sem memoria para a fila"),
          "fila nao criada: aviso no serial e nenhuma tarefa de rede");
  confere(g_inicios.size() == n25 && std::string(led()) == "verde" && tem(serialDesde(m25), "t="),
          "fila nao criada: sem envio, mas sensores, OLED e LED continuam (liberou para verde)");
  // 27) a fila foi criada, mas a tarefa de rede não: avisa e segue sem envio
  g_filaFalha = false; g_tarefaFalha = true; size_t n26 = g_inicios.size(); size_t m26 = marca();
  bool ok26 = iniciarEnvio(); g_eco = 2940; rodar(30000);
  confere(!ok26 && !g_tarefaCriada && tem(serialDesde(m26), "nao deu para criar a tarefa de rede"), "tarefa nao criada: aviso no serial");
  confere(filaEnvio == NULL, "tarefa nao criada: a fila e liberada (filaEnvio volta a NULL) e o loop() para de guardar pacotes");
  confere(g_inicios.size() == n26 && std::string(led()) == "vermelho", "tarefa nao criada: sem envio, mas o poste continua decidindo (50 cm, vermelho)");

  printf("\nTela final: %s\nUltimo envio: %s\n%d falha(s).\n", tela.tela.c_str(), g_url.c_str(), falhas);
  return falhas == 0 ? 0 : 1;
}
