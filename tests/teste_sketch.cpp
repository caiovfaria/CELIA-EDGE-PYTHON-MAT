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
#include "PubSubClient.h"
unsigned long g_millis = 0; unsigned long g_eco = 11760; int g_pinos[40] = {0}; unsigned long g_mudouEm[40] = {0};
bool g_dhtOk = true; float g_temp = 26;
bool g_wifiOk = true; int g_reconexoes = 0; int g_quedasWifi = 0;
std::vector<unsigned long> g_leituras;
std::string g_url; std::vector<unsigned long> g_inicios, g_fins; int g_httpCodigo = 200; std::string g_httpResposta = "1";
unsigned long g_httpDemoraMs = 0; bool g_beginOk = true; bool g_naTarefaDeRede = false; int g_pedidosForaDaRede = 0;
bool g_filaFalha = false, g_tarefaRede = false, g_tarefaCamera = false; const char* g_tarefaQueFalha = nullptr; unsigned g_folgaPilha = 4800;
unsigned long g_tempMudaEm = 0; float g_tempNova = 0; float g_tempNoPedido = 0; int g_filasAteFalhar = 0;
bool g_mqttOk = true; int g_mqttConexoes = 0, g_mqttDesconexoes = 0, g_mqttAssinaturasFalhando = 0, g_mqttPerdidas = 0, g_mqttStops = 0;
std::vector<std::string> g_mqttInscricoes, g_mqttEntrada, g_mqttIds;
std::string g_mqttServidor; int g_mqttPorta = 0, g_mqttKeepAlive = 0;
std::vector<std::pair<unsigned long, std::string>> g_mqttAgendadas; std::string g_mqttRetida; std::vector<std::string> g_mqttNoPedido; unsigned long g_mqttRetidaAtrasoMs = 0;
HardwareSerial Serial; TwoWire Wire; WiFiClass WiFi;
#include "../edge/sketch.ino"   // o mesmo arquivo que roda no Wokwi
int falhas = 0;
void confere(bool ok, const char* d) { printf("%s  %s\n", ok ? "ok    " : "FALHOU", d); fflush(stdout); if (!ok) falhas++; }
const char* led() { int r = g_pinos[25], g = g_pinos[26]; return r && g ? "amarelo" : r ? "vermelho" : g ? "verde" : "apagado"; }
// uma volta da tarefa de rede (só se ela foi criada no setup(), como no ESP32)
void rede() { if (!g_tarefaRede) return; g_naTarefaDeRede = true; cicloDeRede(); g_naTarefaDeRede = false; }
// uma volta da tarefa da câmera (só se ela foi criada), no máximo a cada 100 ms, como no ESP32
unsigned long g_ultimaCamera = 0;
void camera() { if (!g_tarefaCamera || (g_ultimaCamera != 0 && g_millis - g_ultimaCamera < 100)) return; g_ultimaCamera = g_millis; cicloDaCamera(); }
// roda o loop() e as duas tarefas por "ms" milissegundos simulados (mais 10 ms, para incluir a
// leitura que cai bem no fim). No ESP32 as três rodam ao mesmo tempo; aqui elas se revezam.
void rodar(unsigned long ms) { unsigned long fim = g_millis + ms + 10; while (g_millis < fim) { loop(); rede(); camera(); } }
// quantas vezes um trecho aparece no texto
int conta(const std::string& texto, const char* trecho) { int n = 0; for (size_t p = texto.find(trecho); p != std::string::npos; p = texto.find(trecho, p + 1)) n++; return n; }
bool tem(const std::string& texto, const char* trecho) { return texto.find(trecho) != std::string::npos; }
// tamanho do serial agora: para olhar só o que foi escrito depois deste ponto
size_t marca() { return Serial.buf.size(); }
std::string serialDesde(size_t m) { return Serial.buf.substr(m); }
int main() {
  printf("Teste do sketch com sensores simulados\n\n");
  // 1) Partida com os dois sensores falhando
  g_eco = 0; g_dhtOk = false; setup();
  Serial.registrar = true;   // daqui em diante, guarda cada escrita no serial (cenário 43)
  confere(std::string(led()) == "apagado", "partida sem nenhum sensor: LED apagado");
  confere(tela.tela.find("SEM DADOS") != std::string::npos && tela.tela.find("HC-SR04: sem eco") != std::string::npos
          && tela.tela.find("DHT22: sem resposta") != std::string::npos, "partida sem nenhum sensor: SEM DADOS com os dois sensores sem resposta");
  confere(g_tarefaRede && tem(Serial.buf, "Tarefa de rede ligada"), "setup criou a fila e a tarefa de rede e avisou no serial");
  confere(g_tarefaCamera && tem(Serial.buf, "Tarefa da camera ligada"), "setup criou a fila e a tarefa da camera (separada da de rede) e avisou no serial");
  // Etapa 2: com SEM DADOS não há status, então nada vai para o ThingSpeak (nem com chave e Wi-Fi)
  THINGSPEAK_CHAVE = "CHAVE_TESTE"; size_t m0 = marca(); enviarAoThingSpeak();
  confere(g_inicios.empty() && tem(serialDesde(m0), "ainda sem dados"), "SEM DADOS: nao envia nada ao ThingSpeak");
  THINGSPEAK_CHAVE = CHAVE_DE_EXEMPLO;  // volta para a chave de exemplo, como fica no GitHub
  // 1b) Etapa 4: buraco na partida, ainda sem leitura de nenhum sensor. O buraco não depende
  //     dos sensores: EVITE, LED vermelho e envio só dos campos 5 e 6.
  rodar(3500);   // a tarefa da câmera conecta, assina e passa a janela de 3 s
  size_t m1b = marca(); g_mqttEntrada.push_back("buraco"); rodar(200);
  confere(std::string(led()) == "vermelho" && tem(tela.tela, "EVITE") && tem(tela.tela, "Motivo: buraco") && tem(tela.tela, "Buraco (camera)"),
          "buraco na partida sem sensores: LED vermelho, EVITE e motivo buraco no OLED");
  confere(tem(tela.tela, "HC-SR04: sem eco") && tem(tela.tela, "DHT22: sem resposta") && tem(serialDesde(m1b), "camera viu um buraco"),
          "buraco na partida sem sensores: OLED continua mostrando os sensores sem resposta");
  THINGSPEAK_CHAVE = "CHAVE_TESTE"; g_naTarefaDeRede = true; enviarAoThingSpeak(); g_naTarefaDeRede = false;  // o teste faz o papel da tarefa de rede
  confere(g_inicios.size() == 1 && tem(g_url, "&field5=2&field6=5") && !tem(g_url, "field1") && !tem(g_url, "field4"),
          "buraco na partida sem sensores: envia status 2 e motivo 5, sem os campos dos sensores");
  THINGSPEAK_CHAVE = CHAVE_DE_EXEMPLO; g_inicios.clear(); g_fins.clear(); g_url.clear();
  // 1c) o buraco vence e os sensores continuam sem leitura: volta a SEM DADOS e não envia nada
  rodar(61000);
  confere(std::string(led()) == "apagado" && tem(tela.tela, "SEM DADOS"), "buraco venceu na partida sem sensores: volta a SEM DADOS, LED apagado");
  THINGSPEAK_CHAVE = "CHAVE_TESTE"; size_t m1c = marca(); guardarParaEnvio(); enviarAoThingSpeak(); THINGSPEAK_CHAVE = CHAVE_DE_EXEMPLO;
  confere(g_inicios.empty() && tem(serialDesde(m1c), "ainda sem dados"), "buraco venceu na partida sem sensores: nao envia mais nada");
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
  // 16b) Etapa 4: com os dois sensores falhando, chega um buraco: o LED fica vermelho e o envio
  //      volta, só com status 2 e motivo 5 (o buraco vem da câmera, não dos sensores)
  size_t m16b = marca(); g_mqttEntrada.push_back("buraco"); rodar(200);
  confere(std::string(led()) == "vermelho" && tem(tela.tela, "EVITE") && tem(tela.tela, "Motivo: buraco"), "dois sensores falhando + buraco: LED vermelho, EVITE, motivo buraco");
  rodar(25000);
  confere(g_inicios.size() > n16 && tem(g_url, "&field5=2&field6=5") && !tem(g_url, "field1") && !tem(g_url, "field4")
          && tem(serialDesde(m16b), "(status 2, motivo 5)"), "dois sensores falhando + buraco: envia status 2 e motivo 5, sem os campos dos sensores");
  rodar(40000);   // o buraco vence; os sensores continuam falhando
  size_t n16c = g_inicios.size(); size_t m16c = marca(); rodar(25000);
  confere(g_inicios.size() == n16c && tem(serialDesde(m16c), "os dois sensores falharam, envio pulado"), "buraco venceu com os dois sensores falhando: volta a nao enviar");
  g_dhtOk = true; g_eco = 11760; rodar(25000);
  confere(g_inicios.size() > n16 && tem(g_url, "&field1=26.0&field2=50.0&field3=26.0&field4=200&field5=0&field6=0"), "sensores voltaram: envia de novo, com os 6 campos");

  // 17) pedido lento (7 s) seguido de um rápido: as leituras não param e o intervalo conta do fim.
  //     1 s depois de o pedido começar, o calor sobe para 34 °C: o LED muda durante o pedido, e o
  //     registro lento leva o status da leitura que foi capturada antes dele (0); o seguinte leva o novo (1).
  size_t k = g_inicios.size(); size_t m17 = marca();
  g_httpDemoraMs = 7000; g_tempNoPedido = 34;
  while (g_inicios.size() == k) { loop(); rede(); camera(); }  // roda até sair o pedido lento (ele espera 7 s)
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
  while (g_inicios.size() == n21) { loop(); rede(); camera(); }  // roda até sair um envio normal
  fimUltimaTentativa = millis() - INTERVALO_ENVIO_MS; size_t n22 = g_inicios.size(); size_t m22 = marca();
  rede();  // tenta de novo logo em seguida, sem o loop() ter lido os sensores
  confere(g_inicios.size() == n22 && tem(serialDesde(m22), "sem leitura nova"), "sem leitura nova: nao reenvia o pacote antigo");

  // ---------- Etapa 4: câmera por MQTT ----------
  g_eco = 11760; g_temp = 26; g_dhtOk = true; rodar(20000);  // trecho verde de novo
  confere(std::string(led()) == "verde", "antes da camera: trecho verde");
  // 30) conectou no broker certo, assinou o tópico, e assinou de novo depois da queda do Wi-Fi (cenário 11)
  confere(g_mqttServidor == "broker.hivemq.com" && g_mqttPorta == 1883, "MQTT no broker.hivemq.com, porta 1883");
  int assinaturas = 0; for (auto& t : g_mqttInscricoes) if (t == "celia/poste01/camera") assinaturas++;
  confere(assinaturas >= 2 && assinaturas == (int)g_mqttInscricoes.size(), "assinou celia/poste01/camera (e de novo depois que o Wi-Fi voltou)");
  confere(tem(Serial.buf, "MQTT conectado: ouvindo a camera"), "serial avisa que o MQTT conectou");
  confere(g_mqttIds.size() >= 2 && g_mqttIds[0] != g_mqttIds[1] && g_mqttIds[0].rfind("celia-poste01-", 0) == 0, "cada conexao usa um nome diferente (celia-poste01-...)");

  // 31) chega "buraco": EVITE na hora (sem esperar a leitura de 5 s), motivo 5, contagem no OLED
  size_t m31 = marca(); g_mqttEntrada.push_back("buraco"); rodar(200);
  confere(std::string(led()) == "vermelho" && tem(tela.tela, "EVITE") && tem(tela.tela, "Motivo: buraco"), "buraco: LED vermelho, EVITE e motivo buraco na hora");
  confere(tem(tela.tela, "Buraco (camera) 60s"), "buraco: OLED mostra quanto falta (60 s no modo demonstracao)");
  confere(tem(serialDesde(m31), "Camera: buraco recebido") && tem(serialDesde(m31), "trecho em EVITE por 60 s"), "buraco: avisos no serial");
  rodar(20000);
  confere(tem(g_url, "&field5=2&field6=5"), "buraco: o ThingSpeak recebe status 2 e motivo 5");
  // 32) "livre" enquanto o buraco vale: não muda nada (não apaga o buraco)
  size_t m32 = marca(); g_mqttEntrada.push_back("livre"); rodar(200);
  confere(std::string(led()) == "vermelho" && tem(serialDesde(m32), "livre (nao muda nada)"), "livre durante o buraco: continua vermelho");
  // 33) o aviso vale 60 s: ainda vermelho perto do fim, verde depois
  rodar(38000);   // uns 58 s desde o buraco
  confere(std::string(led()) == "vermelho", "buraco com 58 s: ainda vermelho");
  size_t m33 = marca(); rodar(3000);
  confere(std::string(led()) == "verde" && tem(serialDesde(m33), "aviso de buraco venceu"), "buraco com 61 s: venceu, volta a regra dos sensores (verde)");
  rodar(20000);
  confere(tem(g_url, "&field5=0&field6=0"), "depois que venceu: o ThingSpeak volta a receber status 0, motivo 0");
  // 34) mensagens que não são "livre" nem "buraco" são ignoradas
  size_t m34 = marca();
  g_mqttEntrada.push_back("BURACO"); g_mqttEntrada.push_back("buraco!"); g_mqttEntrada.push_back("");
  g_mqttEntrada.push_back(std::string(40, 'x')); g_mqttEntrada.push_back("obstaculo");
  g_mqttEntrada.push_back(std::string("buraco\0x", 8)); g_mqttEntrada.push_back(std::string("livre\0", 6)); rodar(1000);
  int ignoradas = conta(serialDesde(m34), "mensagem desconhecida");
  confere(std::string(led()) == "verde" && ignoradas == 7, "BURACO, buraco!, vazio, texto grande, obstaculo, buraco\\0x e livre\\0: as 7 ignoradas");

  // 35) mensagem que chega junto com a inscrição (pode ser antiga, retida no broker): ignorada
  g_mqttOk = false; rodar(6000);  // broker cai: o MQTT desconecta
  confere(tem(Serial.buf, "MQTT caiu"), "broker caiu: aviso no serial");
  g_mqttOk = true; size_t ins = g_mqttInscricoes.size();
  while (g_mqttInscricoes.size() == ins) { loop(); rede(); camera(); }  // até assinar de novo
  size_t m35 = marca(); g_mqttEntrada.push_back("buraco"); rodar(200);
  confere(std::string(led()) == "verde" && tem(serialDesde(m35), "junto com a inscricao"), "buraco logo depois de assinar: ignorado (pode ser mensagem velha; custo conhecido da janela de 3 s)");
  rodar(3000); g_mqttEntrada.push_back("buraco"); rodar(200);
  confere(std::string(led()) == "vermelho", "buraco 3 s depois de assinar: aceito");

  // 36) um buraco novo recomeça a contagem; e, com o buraco valendo, o motivo é 5 mesmo com a calçada bloqueada
  rodar(30000); g_eco = 2940;      // 30 s depois, um carro para em 50 cm
  g_mqttEntrada.push_back("buraco"); rodar(200);
  rodar(40000);                    // 70 s desde o primeiro buraco, 40 s desde o segundo
  confere(std::string(led()) == "vermelho" && tem(tela.tela, "Motivo: buraco"), "buraco novo recomeca os 60 s; com calcada bloqueada, motivo continua buraco");
  rodar(25000);                    // o segundo buraco venceu; a calçada continua bloqueada
  confere(std::string(led()) == "vermelho" && tem(tela.tela, "Motivo: calcada"), "buraco venceu com a calcada bloqueada: continua EVITE, agora motivo calcada");
  g_eco = 11760; rodar(20000);

  // 37) broker fora do ar: tenta a cada 5 s, sem travar o poste
  g_mqttOk = false; rodar(6000); int c0 = g_mqttConexoes; size_t m37 = marca(); rodar(20000);
  int tentativas = g_mqttConexoes - c0;
  char d37[100]; snprintf(d37, sizeof d37, "broker fora: %d tentativas em 20 s (uma a cada 5 s)", tentativas);
  confere(tentativas >= 3 && tentativas <= 5 && tem(serialDesde(m37), "sem conexao com o broker"), d37);
  confere(tem(serialDesde(m37), "t="), "broker fora: as leituras continuam");
  g_mqttOk = true; rodar(10000);

  // ---------- Revisão 1 do Codex (Etapa 4) ----------
  // 38) mensagens chegam DURANTE um pedido HTTP lento (7 s): a tarefa da câmera não espera o HTTP.
  //     Três mensagens juntas, 2 s depois de o pedido começar: cada mqtt.loop() entrega uma,
  //     na ordem, e o LED fica vermelho bem antes de o pedido terminar.
  confere(std::string(led()) == "verde", "antes do pedido lento com mensagens: trecho verde");
  size_t k38 = g_inicios.size(); size_t m38 = marca(); g_httpDemoraMs = 7000;
  g_mqttNoPedido = {"livre", "xyz", "buraco"};   // chegam juntas, 2 s depois de o pedido começar
  while (g_inicios.size() == k38) { loop(); rede(); camera(); }   // a tarefa de rede fica 7 s no pedido
  g_httpDemoraMs = 0;
  {
    std::string s38 = serialDesde(m38);
    size_t pL = s38.find("livre (nao muda nada)"), pD = s38.find("mensagem desconhecida"), pB = s38.find("buraco recebido"), pR = s38.find("ThingSpeak: registro");
    unsigned long chegada = g_inicios[k38] + 2000, vermelhoEm = g_mudouEm[25];
    char d38[160]; snprintf(d38, sizeof d38, "buraco durante pedido lento de 7 s: LED vermelho %ld ms depois de chegar, antes do fim do pedido", (long)vermelhoEm - (long)chegada);
    confere(std::string(led()) == "vermelho" && vermelhoEm >= chegada && vermelhoEm - chegada <= 500 && vermelhoEm < g_fins[k38], d38);
    confere(pL != std::string::npos && pD != std::string::npos && pB != std::string::npos && pR != std::string::npos && pL < pD && pD < pB && pB < pR,
            "3 mensagens juntas durante o pedido: uma por volta, na ordem (livre, desconhecida, buraco), antes do fim do envio");
  }
  rodar(62000);   // o buraco vence
  confere(std::string(led()) == "verde", "depois do teste 38: buraco venceu, verde");

  // 39) mensagem RETIDA que chega 0,3 s depois da inscrição, com a tarefa de rede presa num pedido
  //     lento: a tarefa da câmera continua rodando, então ela cai dentro da janela de 3 s e é ignorada
  g_mqttOk = false; rodar(1000); g_mqttOk = true;
  g_mqttRetida = "buraco"; g_mqttRetidaAtrasoMs = 300; g_httpDemoraMs = 7000;
  fimUltimaTentativa = millis() - INTERVALO_ENVIO_MS;            // o envio sai agora
  ultimaTentativaMqtt = millis() - MQTT_ESPERA_RECONEXAO_MS;     // e a reconexão também
  size_t ins39 = g_mqttInscricoes.size(); size_t k39 = g_inicios.size(); size_t m39 = marca();
  while (g_inicios.size() == k39) { loop(); rede(); camera(); }  // a tarefa de rede fica 7 s no pedido
  g_httpDemoraMs = 0; g_mqttRetida.clear(); rodar(1000);
  confere(g_mqttInscricoes.size() > ins39 && std::string(led()) == "verde" && tem(serialDesde(m39), "junto com a inscricao"),
          "mensagem retida entregue logo depois da inscricao, com o HTTP lento: ignorada (trecho continua verde)");
  rodar(3000); g_mqttEntrada.push_back("buraco"); rodar(200);
  confere(std::string(led()) == "vermelho", "depois da janela de 3 s, um buraco novo e aceito");
  rodar(62000);

  // 40) o pedido de inscrição não sai (subscribe() devolve false): o poste encerra a conexão e tenta
  //     de novo em 5 s (não fica conectado sem ouvir). A confirmação do broker (SUBACK) é outra
  //     coisa: ela é conferida na rodada com o broker real, quando um aviso chega.
  g_mqttAssinaturasFalhando = 1; g_mqttOk = false; rodar(1000); g_mqttOk = true;
  int c40 = g_mqttConexoes, d40 = g_mqttDesconexoes; size_t m40 = marca(); rodar(12000);
  {
    std::string s40 = serialDesde(m40);
    size_t pF = s40.find("nao deu para assinar o topico"), pO = s40.find("MQTT conectado: ouvindo");
    confere(pF != std::string::npos && pO != std::string::npos && pF < pO && conta(s40, "MQTT conectado: ouvindo") == 1,
            "pedido de inscricao nao saiu: avisa, e so anuncia que esta ouvindo depois do pedido seguinte sair");
    confere(g_mqttConexoes - c40 == 2 && g_mqttDesconexoes - d40 == 1, "pedido de inscricao nao saiu: encerra a conexao e reconecta 5 s depois");
  }
  g_mqttEntrada.push_back("buraco"); rodar(200);
  confere(std::string(led()) == "vermelho", "depois da nova inscricao, o buraco chega");
  rodar(62000);

  // 41) o Wi-Fi cai por 2 s: a conexão do MQTT morreria sem aviso. O poste fecha a conexão aqui
  //     (stop, sem tentar mandar nada pela rede caída), conecta e assina de novo quando o Wi-Fi
  //     volta, e o buraco seguinte chega (nada se perde)
  int perdidas41 = g_mqttPerdidas, stops41 = g_mqttStops, desc41 = g_mqttDesconexoes; size_t m41 = marca();
  g_wifiOk = false; rodar(2000); g_wifiOk = true; rodar(9000);
  g_mqttEntrada.push_back("buraco"); rodar(300);
  confere(tem(serialDesde(m41), "MQTT: sem Wi-Fi, conexao encerrada") && std::string(led()) == "vermelho" && g_mqttPerdidas == perdidas41,
          "Wi-Fi caiu 2 s: o MQTT reconecta quando volta e o buraco seguinte chega");
  confere(g_mqttStops - stops41 == 1 && g_mqttDesconexoes == desc41, "Wi-Fi caiu: fecha a conexao local (stop), sem mandar DISCONNECT pela rede caida");
  rodar(62000);

  // 42) a conexão morre sem o Wi-Fi cair (por exemplo, o roteador esqueceu a conexão): com o
  //     keepalive de 15 s, o ping sai depois de até 15 s parado e a biblioteca desiste depois de
  //     mais 15 s sem resposta. O poste reconecta entre 15 e 30 s (com 60 s, levaria até 2 min)
  confere(g_mqttKeepAlive == 15, "keepalive do MQTT: 15 s (a tarefa da camera nao espera o HTTP)");
  g_quedasWifi++;   // a conexão morreu, mas o Wi-Fi continua no ar
  size_t ins42 = g_mqttInscricoes.size(); unsigned long t42 = g_millis;
  while (g_mqttInscricoes.size() == ins42 && g_millis - t42 < 120000) { loop(); rede(); camera(); }
  char d42[100]; snprintf(d42, sizeof d42, "conexao morta sem aviso: reconectou e assinou de novo em %lu s", (g_millis - t42) / 1000);
  confere(g_millis - t42 >= 15000 && g_millis - t42 <= 31000, d42);

  // 22) cada tarefa mede a sobra da pilha (no ESP32, em bytes) e avisa quando ela diminui
  confere(tem(Serial.buf, "Tarefa de rede: menor sobra de pilha ate agora: 4800 bytes"), "tarefa de rede mede e avisa a sobra da pilha");
  confere(tem(Serial.buf, "Tarefa da camera: menor sobra de pilha ate agora: 4800 bytes"), "tarefa da camera mede e avisa a sobra da pilha");

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

  // 26) falta de memória para a fila do envio na partida: avisa, não cria a tarefa de rede e segue sem envio
  g_tarefaRede = false; g_filasAteFalhar = 1; size_t n25 = g_inicios.size(); size_t m25 = marca();
  bool ok25 = iniciarEnvio(); guardarParaEnvio();
  g_eco = 11760; rodar(30000);
  confere(!ok25 && filaEnvio == NULL && !g_tarefaRede && tem(serialDesde(m25), "sem memoria para a fila do ThingSpeak"),
          "fila do envio nao criada: aviso no serial e nenhuma tarefa de rede");
  confere(g_inicios.size() == n25 && std::string(led()) == "verde" && tem(serialDesde(m25), "t="),
          "fila do envio nao criada: sem envio, mas sensores, OLED e LED continuam (liberou para verde)");
  // 27) a fila foi criada, mas a tarefa de rede não: avisa, libera a fila e segue sem envio
  g_tarefaQueFalha = "rede"; size_t n26 = g_inicios.size(); size_t m26 = marca();
  bool ok26 = iniciarEnvio(); g_eco = 2940; rodar(30000);
  confere(!ok26 && !g_tarefaRede && filaEnvio == NULL && tem(serialDesde(m26), "nao deu para criar a tarefa de rede"),
          "tarefa de rede nao criada: aviso no serial e a fila e liberada (volta a NULL)");
  confere(g_inicios.size() == n26 && std::string(led()) == "vermelho", "tarefa de rede nao criada: sem envio, mas o poste continua decidindo (50 cm, vermelho)");
  g_mqttEntrada.push_back("buraco"); rodar(200);
  confere(tem(tela.tela, "Motivo: buraco"), "tarefa de rede nao criada: a camera continua funcionando (o buraco chega)");
  g_tarefaQueFalha = nullptr;
  bool okRede = iniciarEnvio();   // a tarefa de rede volta, para conferir que o envio não depende da câmera
  // 28) falta de memória para a fila da câmera: avisa, não cria a tarefa da câmera e segue sem câmera
  g_tarefaCamera = false; g_filasAteFalhar = 1; size_t m28 = marca(); size_t n28 = g_inicios.size();
  bool ok28 = iniciarCamera(); rodar(30000);
  confere(!ok28 && filaCamera == NULL && !g_tarefaCamera && tem(serialDesde(m28), "sem memoria para a fila da camera"),
          "fila da camera nao criada: aviso no serial e nenhuma tarefa da camera");
  confere(tem(serialDesde(m28), "t="), "fila da camera nao criada: o poste continua lendo os sensores");
  confere(okRede && g_inicios.size() > n28, "fila da camera nao criada: o envio ao ThingSpeak continua");
  // 29) a fila da câmera foi criada, mas a tarefa não: avisa, libera a fila e segue sem câmera
  g_tarefaQueFalha = "camera"; size_t m29 = marca(); size_t n29 = g_inicios.size();
  bool ok29 = iniciarCamera(); rodar(30000);
  confere(!ok29 && !g_tarefaCamera && filaCamera == NULL && tem(serialDesde(m29), "nao deu para criar a tarefa da camera"),
          "tarefa da camera nao criada: aviso no serial e a fila e liberada (volta a NULL)");
  confere(tem(serialDesde(m29), "t=") && std::string(led()) == "vermelho" && g_inicios.size() > n29,
          "tarefa da camera nao criada: o poste continua decidindo (50 cm, vermelho) e enviando");
  g_tarefaQueFalha = nullptr;

  // 43) depois do setup(), toda escrita no serial é uma linha inteira: texto e quebra de linha
  //     numa escrita só. Assim, o loop() e as duas tarefas nunca cortam a linha uma da outra.
  int quebradas = 0;
  for (const std::string& e : Serial.escritas)
    if (e.size() < 2 || e.find("\r\n") != e.size() - 2) quebradas++;
  char d43[120]; snprintf(d43, sizeof d43, "serial: %zu escritas depois do setup, todas linhas inteiras (%d quebradas)", Serial.escritas.size(), quebradas);
  confere(Serial.escritas.size() > 100 && quebradas == 0, d43);

  printf("\nTela final: %s\nUltimo envio: %s\n%d falha(s).\n", tela.tela.c_str(), g_url.c_str(), falhas);
  return falhas == 0 ? 0 : 1;
}
