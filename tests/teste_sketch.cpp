/*
  Teste do sketch inteiro (edge/sketch.ino: luz + calor) com sensores simulados, sem Wokwi.
  As peças (DHT22, LDR, LED, Wi-Fi, ThingSpeak e MQTT) são trocadas pelas versões falsas da
  pasta arduino_falso/. O LDR é o analogRead falso: o teste escolhe o valor do ADC em g_adc.
  Rodar na pasta do projeto:
      g++ -std=gnu++17 -I tests/arduino_falso -o teste_sketch tests/teste_sketch.cpp && ./teste_sketch
*/
#include "Arduino.h"
#include "WiFi.h"
#include "HTTPClient.h"
#include "PubSubClient.h"
unsigned long g_millis = 0; unsigned long g_eco = 0; int g_pinos[40] = {0}; unsigned long g_mudouEm[40] = {0};
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
HardwareSerial Serial; WiFiClass WiFi;
#include "../edge/sketch.ino"   // o mesmo arquivo que roda no Wokwi
int falhas = 0;
void confere(bool ok, const char* d) { printf("%s  %s\n", ok ? "ok    " : "FALHOU", d); fflush(stdout); if (!ok) falhas++; }
const char* led() { int r = g_pinos[25], g = g_pinos[26]; return r && g ? "amarelo" : r ? "vermelho" : g ? "verde" : "apagado"; }
void rede() { if (!g_tarefaRede) return; g_naTarefaDeRede = true; cicloDeRede(); g_naTarefaDeRede = false; }
unsigned long g_ultimaCamera = 0;
void camera() { if (!g_tarefaCamera || (g_ultimaCamera != 0 && g_millis - g_ultimaCamera < 100)) return; g_ultimaCamera = g_millis; cicloDaCamera(); }
void rodar(unsigned long ms) { unsigned long fim = g_millis + ms + 10; while (g_millis < fim) { loop(); rede(); camera(); } }
int conta(const std::string& texto, const char* trecho) { int n = 0; for (size_t p = texto.find(trecho); p != std::string::npos; p = texto.find(trecho, p + 1)) n++; return n; }
bool tem(const std::string& texto, const char* trecho) { return texto.find(trecho) != std::string::npos; }
size_t marca() { return Serial.buf.size(); }
std::string serialDesde(size_t m) { return Serial.buf.substr(m); }
// última linha do serial que contém um trecho
std::string ultimaLinhaCom(const char* trecho) {
  size_t p = Serial.buf.rfind(trecho); if (p == std::string::npos) return "";
  size_t ini = Serial.buf.rfind("\r\n", p); ini = (ini == std::string::npos) ? 0 : ini + 2;
  size_t fim = Serial.buf.find("\r\n", p); return Serial.buf.substr(ini, fim - ini);
}
// O que o Wokwi faz com o "lux" do módulo de luz (LDR + 10 kΩ em 3,3 V, ADC de 0 a 4095)
int adcDoWokwi(double lux) { double r = 50000.0 * pow(10.0, 0.7) / pow(lux, 0.7); return (int)lround(3.3 * r / (r + 10000.0) / 3.3 * 4095.0); }
const int ADC_ESCURO = adcDoWokwi(5), ADC_NORMAL = adcDoWokwi(500), ADC_SOL = adcDoWokwi(50000);

int main() {
  printf("Teste do sketch (luz + calor) com sensores simulados\n\n");
  char d[200];
  const long LUX_NORMAL = luxPeloAdc(ADC_NORMAL), LUX_ESCURO = luxPeloAdc(ADC_ESCURO), LUX_SOL = luxPeloAdc(ADC_SOL);
  snprintf(d, sizeof d, "LDR no Wokwi: 5 lux -> ADC %d -> %ld lux; 500 -> %d -> %ld; 50000 -> %d -> %ld", ADC_ESCURO, LUX_ESCURO, ADC_NORMAL, LUX_NORMAL, ADC_SOL, LUX_SOL);
  confere(classificarLuz(LUX_ESCURO) == LUZ_ESCURO && classificarLuz(LUX_NORMAL) == LUZ_NORMAL && classificarLuz(LUX_SOL) == LUZ_SOL, d);

  // 1) Partida com o DHT22 sem responder e luz normal: SEM DADOS, LED apagado, nada vai para o ThingSpeak
  g_adc = ADC_NORMAL; g_dhtOk = false; setup();
  Serial.registrar = true;   // daqui em diante, guarda cada escrita no serial (cenário 40)
  confere(std::string(led()) == "apagado" && tem(Serial.buf, "SEM DADOS: aguardando a primeira leitura valida do DHT22"), "partida sem DHT22: LED apagado e SEM DADOS no serial");
  snprintf(d, sizeof d, "(luz agora: %ld lux)", LUX_NORMAL);
  confere(tem(Serial.buf, d), "SEM DADOS mostra a luz lida, mesmo sem o DHT22");
  confere(g_leiturasAdc.size() == (size_t)LEITURAS_LDR, "cada leitura do LDR faz 8 leituras do ADC (media)");
  confere(g_tarefaRede && tem(Serial.buf, "Tarefa de rede ligada"), "setup criou a fila e a tarefa de rede");
  confere(g_tarefaCamera && tem(Serial.buf, "Tarefa da camera ligada"), "setup criou a fila e a tarefa da camera");
  confere(!tem(Serial.buf, "OLED") && tem(Serial.buf, "modulo Edge (luz + calor) iniciado"), "sem OLED: o serial anuncia o modulo de luz + calor");
  THINGSPEAK_CHAVE = "CHAVE_TESTE"; size_t m0 = marca(); enviarAoThingSpeak();
  confere(g_inicios.empty() && tem(serialDesde(m0), "ainda sem dados do DHT22"), "SEM DADOS: nao envia nada ao ThingSpeak");
  THINGSPEAK_CHAVE = CHAVE_DE_EXEMPLO;

  // 1b) buraco na partida, ainda sem DHT22: EVITE, LED vermelho, envio com luz, status 2 e motivo 5
  rodar(3500);   // a tarefa da câmera conecta, assina e passa a janela de 3 s
  size_t m1b = marca(); g_mqttEntrada.push_back("buraco"); rodar(200);
  confere(std::string(led()) == "vermelho" && tem(serialDesde(m1b), "camera viu um buraco: EVITE (motivo buraco)"), "buraco na partida sem DHT22: LED vermelho e EVITE");
  THINGSPEAK_CHAVE = "CHAVE_TESTE"; g_naTarefaDeRede = true; enviarAoThingSpeak(); g_naTarefaDeRede = false;
  snprintf(d, sizeof d, "&field5=2&field6=5&field7=%ld", LUX_NORMAL);
  confere(g_inicios.size() == 1 && tem(g_url, d) && !tem(g_url, "field1"), "buraco na partida sem DHT22: envia luz, status 2 e motivo 5, sem os campos do DHT22");
  THINGSPEAK_CHAVE = CHAVE_DE_EXEMPLO; g_inicios.clear(); g_fins.clear(); g_url.clear();
  // 1c) o buraco vence e o DHT22 continua sem leitura: volta a SEM DADOS e não envia
  size_t m1c = marca(); rodar(61000);
  confere(std::string(led()) == "apagado" && tem(serialDesde(m1c), "aviso de buraco venceu"), "buraco venceu na partida sem DHT22: LED apagado de novo");
  THINGSPEAK_CHAVE = "CHAVE_TESTE"; size_t m1d = marca(); guardarParaEnvio(); enviarAoThingSpeak(); THINGSPEAK_CHAVE = CHAVE_DE_EXEMPLO;
  confere(g_inicios.empty() && tem(serialDesde(m1d), "ainda sem dados do DHT22"), "buraco venceu na partida sem DHT22: nao envia mais nada");

  // 2) o DHT22 responde (26 C), luz normal: ACESSIVEL, verde
  g_dhtOk = true; size_t m2 = marca(); rodar(5000);
  confere(std::string(led()) == "verde", "DHT22 ok, 26 C e luz normal: verde");
  snprintf(d, sizeof d, "T 26.0C U 50%% IC 26.0C (normal) | luz %ld lux (adc %d) lida normal / vale normal | STATUS 0 ACESSIVEL | motivo 0 nenhum", LUX_NORMAL, ADC_NORMAL);
  confere(tem(serialDesde(m2), d), "linha do serial: temperatura, luz em lux e ADC, status e motivo");

  // 3) escurece (5 lux): precisa de 15 s seguidos para valer
  g_adc = ADC_ESCURO; rodar(5000);
  confere(std::string(led()) == "verde" && tem(ultimaLinhaCom("t="), "lida escuro / vale normal") && tem(ultimaLinhaCom("t="), "confirmando luz 15s"),
          "escureceu: ainda verde, serial mostra 'confirmando luz 15s'");
  rodar(10000);
  confere(std::string(led()) == "verde" && tem(ultimaLinhaCom("t="), "confirmando luz 5s"), "10 s no escuro: ainda verde, faltam 5 s");
  rodar(5000);
  confere(std::string(led()) == "amarelo" && tem(ultimaLinhaCom("t="), "STATUS 1 ATENCAO | motivo 1 luz"), "15 s no escuro: amarelo, ATENCAO, motivo luz");

  // 4) farol de carro passando: uma leitura com luz normal e volta o escuro. Não muda nada
  g_adc = ADC_NORMAL; rodar(5000); g_adc = ADC_ESCURO; rodar(5000);
  confere(std::string(led()) == "amarelo" && !tem(ultimaLinhaCom("t="), "confirmando"), "farol passou (uma leitura clara): continua amarelo e a contagem e cancelada");

  // 5) ThingSpeak: com a chave de exemplo (como fica no GitHub), nada é enviado
  confere(g_inicios.empty() && tem(Serial.buf, "cole a Write API Key"), "chave de exemplo: nenhum envio, aviso no serial");
  // chave colada: envia 6 campos (1, 2, 3, 5, 6 e a luz no 7), com o status igual à cor do LED
  THINGSPEAK_CHAVE = "CHAVE_TESTE"; size_t m5 = marca(); rodar(20000);
  confere(g_inicios.size() == 1 && tem(g_url, "http://api.thingspeak.com/update?api_key=CHAVE_TESTE"), "chave colada: 1 envio em 20 s, com a chave");
  snprintf(d, sizeof d, "&field1=26.0&field2=50.0&field3=26.0&field5=1&field6=1&field7=%ld", LUX_ESCURO);
  confere(tem(g_url, d) && !tem(g_url, "field4"), "campos: T 26.0, U 50.0, IC 26.0, status 1 (amarelo), motivo 1 (luz), luz em lux no field7; sem o field4 (distancia da v1)");
  snprintf(d, sizeof d, "registro 1 gravado (status 1, motivo 1, %ld lux)", LUX_ESCURO);
  confere(tem(serialDesde(m5), d), "serial confirma o registro gravado, com a luz");

  // 6) calor em atenção (33 C) no escuro: ATENCAO com motivo luz+calor, na hora (o calor não espera confirmação)
  g_temp = 33; rodar(5000);
  confere(std::string(led()) == "amarelo" && tem(ultimaLinhaCom("t="), "IC 33.0C (atencao)") && tem(ultimaLinhaCom("t="), "STATUS 1 ATENCAO | motivo 3 luz+calor"),
          "escuro + 33 C: amarelo, motivo luz+calor");
  // 7) a luz volta ao normal: 15 s depois, fica só o calor (motivo 2)
  g_adc = ADC_NORMAL; rodar(20000);
  confere(std::string(led()) == "amarelo" && tem(ultimaLinhaCom("t="), "vale normal | STATUS 1 ATENCAO | motivo 2 calor"), "luz normal + 33 C: amarelo, motivo calor");
  // 8) sol direto (50.000 lux) + calor: EVITE (motivo 3) depois da confirmação
  g_adc = ADC_SOL; rodar(5000);
  confere(std::string(led()) == "amarelo" && tem(ultimaLinhaCom("t="), "lida sol direto / vale normal"), "sol direto: ainda confirmando, continua amarelo");
  rodar(15000);
  confere(std::string(led()) == "vermelho" && tem(ultimaLinhaCom("t="), "STATUS 2 EVITE | motivo 3 luz+calor"), "15 s de sol direto + 33 C: vermelho, EVITE, motivo luz+calor");
  // 9) calor em perigo com sol: continua EVITE (no DHT22 falso, o índice de calor é a própria temperatura)
  g_temp = 42; rodar(5000);
  confere(std::string(led()) == "vermelho" && tem(ultimaLinhaCom("t="), "IC 42.0C (perigo)"), "sol direto + IC 42 C (perigo): vermelho");
  // 10) o calor passa (26 C) e o sol continua: sol sozinho não piora nada (verde)
  g_temp = 26; rodar(5000);
  confere(std::string(led()) == "verde" && tem(ultimaLinhaCom("t="), "vale sol direto | STATUS 0 ACESSIVEL | motivo 0 nenhum"), "sol direto sem calor: verde, motivo nenhum");
  rodar(20000);
  snprintf(d, sizeof d, "&field5=0&field6=0&field7=%ld", LUX_SOL);
  confere(tem(g_url, d), "o envio leva os lux do sol e status 0");

  // 11) o DHT22 falha: mantém o último calor, avisa e envia sem os campos 1, 2 e 3 (a luz vai)
  g_temp = 33; rodar(5000);   // calor volta para atenção: sol + calor = EVITE
  g_dhtOk = false; size_t m11 = marca(); rodar(25000);   // 25 s: garante um envio com a leitura que falhou
  confere(tem(serialDesde(m11), "falha ao ler o DHT22; mantendo a leitura anterior") && std::string(led()) == "vermelho", "DHT22 falhou: aviso no serial e mantem o ultimo calor (vermelho)");
  snprintf(d, sizeof d, "&field5=2&field6=3&field7=%ld", LUX_SOL);
  confere(!tem(g_url, "field1") && !tem(g_url, "field2") && !tem(g_url, "field3") && tem(g_url, d), "DHT22 falhando: envia so luz, status e motivo");
  g_dhtOk = true; g_temp = 26; g_adc = ADC_NORMAL; rodar(20000);
  confere(std::string(led()) == "verde", "tudo normal de novo: verde");

  // 12) Wi-Fi cai: não envia, continua decidindo; depois de 1 min força uma reconexão
  size_t n12 = g_inicios.size(); size_t m12 = marca(); g_wifiOk = false; g_adc = ADC_ESCURO; rodar(25000);
  confere(g_inicios.size() == n12 && tem(serialDesde(m12), "sem Wi-Fi, envio pulado") && tem(serialDesde(m12), "Wi-Fi caiu"), "sem Wi-Fi: nao envia e avisa");
  confere(std::string(led()) == "amarelo" && g_reconexoes == 0, "sem Wi-Fi: o poste continua decidindo (escureceu: amarelo) e ainda nao forca reconexao");
  rodar(40000);
  confere(g_reconexoes == 1 && tem(serialDesde(m12), "1 minuto sem conexao"), "sem Wi-Fi ha 1 min: forca uma reconexao");
  g_wifiOk = true; size_t m12b = marca(); rodar(20000);
  confere(g_inicios.size() == n12 + 1 && tem(serialDesde(m12b), "Wi-Fi conectado"), "Wi-Fi voltou: envia de novo");
  g_adc = ADC_NORMAL; rodar(20000);

  // 13) cadência: 20 s contados do FIM de uma tentativa até o início da seguinte
  size_t k13 = g_inicios.size(); rodar(60000);
  bool cadenciaOk = g_inicios.size() >= k13 + 3;
  for (size_t i = k13 > 0 ? k13 : 1; i < g_inicios.size(); i++) { unsigned long dd = g_inicios[i] - g_fins[i - 1]; if (dd < 20000 || dd > 20100) cadenciaOk = false; }
  confere(cadenciaOk, "20 s entre o fim de um envio e o inicio do seguinte");

  // 14) pedido lento (7 s): as leituras continuam e o seguinte sai 20 s depois do FIM dele
  size_t k14 = g_inicios.size(); g_httpDemoraMs = 7000;
  while (g_inicios.size() == k14) { loop(); rede(); camera(); }
  g_httpDemoraMs = 0;
  bool leuDurante = false;
  for (unsigned long t : g_leiturasAdc) if (t > g_inicios[k14] && t < g_fins[k14]) leuDurante = true;
  confere(g_fins[k14] - g_inicios[k14] >= 7000 && leuDurante, "pedido lento de 7 s: o loop leu os sensores enquanto o pedido esperava");
  rodar(25000);
  confere(g_inicios.size() == k14 + 2 && g_inicios[k14 + 1] - g_fins[k14] >= 20000, "depois do pedido lento, o seguinte so sai 20 s apos o FIM dele");

  // 15) begin falha, HTTP 400, resposta "0" e sem resposta (-1): avisa e não trava
  g_beginOk = false; size_t m15 = marca(); rodar(20000); g_beginOk = true;
  confere(tem(serialDesde(m15), "nao deu para preparar o pedido"), "begin falhou: aviso no serial");
  g_httpCodigo = 400; size_t m15b = marca(); rodar(20000);
  confere(tem(serialDesde(m15b), "falha no envio (codigo 400)"), "HTTP 400: aviso de falha com o codigo");
  g_httpCodigo = 200; g_httpResposta = "0"; size_t m15c = marca(); rodar(20000);
  confere(tem(serialDesde(m15c), "o canal recusou"), "ThingSpeak recusou (resposta 0): aviso no serial");
  g_httpResposta = "1"; g_httpCodigo = -1; g_httpDemoraMs = 5000; size_t m15d = marca(); g_adc = ADC_ESCURO; rodar(45000);
  confere(tem(serialDesde(m15d), "falha no envio (codigo -1)") && std::string(led()) == "amarelo", "sem resposta do ThingSpeak: aviso e o poste nao trava (escureceu: amarelo)");
  g_httpCodigo = 200; g_httpDemoraMs = 0; g_adc = ADC_NORMAL; rodar(20000);

  // 16) sem leitura nova desde o último envio: não manda dado velho
  size_t n16 = g_inicios.size();
  while (g_inicios.size() == n16) { loop(); rede(); camera(); }
  fimUltimaTentativa = millis() - INTERVALO_ENVIO_MS; size_t n16b = g_inicios.size(); size_t m16 = marca();
  rede();
  confere(g_inicios.size() == n16b && tem(serialDesde(m16), "sem leitura nova"), "sem leitura nova: nao reenvia o pacote antigo");

  // ---------- Câmera por MQTT ----------
  rodar(5000);
  confere(std::string(led()) == "verde", "antes da camera: trecho verde");
  // 17) broker certo, tópico certo, inscrição com QoS 1, um nome de conexão diferente a cada vez
  confere(g_mqttServidor == "broker.hivemq.com" && g_mqttPorta == 1883 && g_mqttKeepAlive == 15, "MQTT no broker.hivemq.com, porta 1883, keepalive 15 s");
  int assinaturas = 0; for (auto& t : g_mqttInscricoes) if (t == "celia/poste01/camera") assinaturas++;
  confere(assinaturas >= 2 && assinaturas == (int)g_mqttInscricoes.size(), "assinou celia/poste01/camera (e de novo depois que o Wi-Fi voltou)");
  confere(g_mqttQos == 1, "inscricao com QoS 1");
  confere(g_mqttIds.size() >= 2 && g_mqttIds[0] != g_mqttIds[1] && g_mqttIds[0].rfind("celia-poste01-", 0) == 0 && g_mqttIds[0].size() < 32,
          "cada conexao usa um nome diferente (celia-poste01-...)");
  // 18) chega "buraco": EVITE na hora, motivo 5, contagem no serial; o ThingSpeak recebe 2 e 5
  size_t m18 = marca(); g_mqttEntrada.push_back("buraco"); rodar(200);
  confere(std::string(led()) == "vermelho" && tem(serialDesde(m18), "Camera: buraco recebido") && tem(serialDesde(m18), "trecho em EVITE por 60 s"), "buraco: LED vermelho na hora e avisos no serial");
  confere(tem(ultimaLinhaCom("t="), "STATUS 2 EVITE | motivo 5 buraco | buraco (camera) 60s"), "buraco: a linha do serial mostra motivo buraco e quanto falta (60 s)");
  rodar(20000);
  snprintf(d, sizeof d, "&field5=2&field6=5&field7=%ld", LUX_NORMAL);
  confere(tem(g_url, d), "buraco: o ThingSpeak recebe a luz, status 2 e motivo 5");
  // 19) "livre" não apaga o buraco; com o buraco valendo, a luz pode mudar (as duas contagens aparecem)
  size_t m19 = marca(); g_mqttEntrada.push_back("livre"); g_adc = ADC_ESCURO; rodar(5000);
  confere(std::string(led()) == "vermelho" && tem(serialDesde(m19), "livre (nao muda nada)"), "livre durante o buraco: continua vermelho");
  confere(tem(ultimaLinhaCom("t="), "confirmando luz") && tem(ultimaLinhaCom("t="), "buraco (camera)"), "linha mais comprida (confirmando luz + buraco) sai inteira no serial");
  // 20) o buraco vence: volta a regra dos sensores (escuro confirmado: amarelo)
  size_t m20 = marca(); rodar(40000);
  confere(std::string(led()) == "amarelo" && tem(serialDesde(m20), "aviso de buraco venceu") && tem(ultimaLinhaCom("t="), "motivo 1 luz"), "buraco venceu: volta a regra dos sensores (escuro: amarelo, motivo luz)");
  g_adc = ADC_NORMAL; rodar(20000);
  // 21) mensagens que não são "livre" nem "buraco" são ignoradas
  size_t m21 = marca();
  g_mqttEntrada.push_back("BURACO"); g_mqttEntrada.push_back("buraco!"); g_mqttEntrada.push_back(""); g_mqttEntrada.push_back(std::string("buraco\0x", 8)); rodar(1000);
  confere(std::string(led()) == "verde" && conta(serialDesde(m21), "mensagem desconhecida") == 4, "BURACO, buraco!, vazio e buraco\\0x: as 4 ignoradas");
  // 22) mensagem que chega junto com a inscrição (pode ser antiga): ignorada; 3 s depois, aceita
  g_mqttOk = false; rodar(6000); g_mqttOk = true; size_t ins = g_mqttInscricoes.size();
  while (g_mqttInscricoes.size() == ins) { loop(); rede(); camera(); }
  size_t m22 = marca(); g_mqttEntrada.push_back("buraco"); rodar(200);
  confere(std::string(led()) == "verde" && tem(serialDesde(m22), "junto com a inscricao"), "buraco logo depois de assinar: ignorado (janela de 3 s)");
  rodar(3000); g_mqttEntrada.push_back("buraco"); rodar(200);
  confere(std::string(led()) == "vermelho", "buraco 3 s depois de assinar: aceito");
  rodar(62000);
  // 23) broker fora do ar: tenta a cada 5 s, sem travar as leituras
  g_mqttOk = false; rodar(6000); int c0 = g_mqttConexoes; size_t m23 = marca(); rodar(20000);
  int tentativas = g_mqttConexoes - c0;
  confere(tentativas >= 3 && tentativas <= 5 && tem(serialDesde(m23), "sem conexao com o broker") && tem(serialDesde(m23), "t="), "broker fora: tenta a cada 5 s e as leituras continuam");
  g_mqttOk = true; rodar(10000);
  // 24) buraco durante um pedido HTTP lento: a tarefa da câmera não espera o HTTP
  size_t k24 = g_inicios.size(); g_httpDemoraMs = 7000; g_mqttNoPedido = {"buraco"};
  while (g_inicios.size() == k24) { loop(); rede(); camera(); }
  g_httpDemoraMs = 0;
  unsigned long chegada = g_inicios[k24] + 2000, vermelhoEm = g_mudouEm[25];
  confere(std::string(led()) == "vermelho" && vermelhoEm >= chegada && vermelhoEm - chegada <= 500 && vermelhoEm < g_fins[k24], "buraco durante pedido lento: LED vermelho antes do fim do pedido");
  rodar(62000);
  // 25) Wi-Fi cai 2 s: o MQTT fecha a conexão aqui (stop), reconecta quando volta e o buraco seguinte chega
  int stops25 = g_mqttStops; size_t m25 = marca();
  g_wifiOk = false; rodar(2000); g_wifiOk = true; rodar(9000);
  g_mqttEntrada.push_back("buraco"); rodar(300);
  confere(tem(serialDesde(m25), "MQTT: sem Wi-Fi, conexao encerrada") && g_mqttStops - stops25 == 1 && std::string(led()) == "vermelho", "Wi-Fi caiu 2 s: o MQTT reconecta e o buraco seguinte chega");
  rodar(62000);
  // 26) o pedido de inscrição não sai: encerra a conexão e tenta de novo em 5 s
  g_mqttAssinaturasFalhando = 1; g_mqttOk = false; rodar(1000); g_mqttOk = true;
  size_t m26 = marca(); rodar(12000);
  confere(tem(serialDesde(m26), "nao deu para assinar o topico") && conta(serialDesde(m26), "MQTT conectado: ouvindo") == 1, "inscricao nao saiu: avisa e so anuncia que ouve depois da inscricao seguinte");
  // 26b) a conexão morre sem o Wi-Fi cair (por exemplo, o roteador esqueceu a conexão): com o
  //      keepalive de 15 s, o ping sai depois de até 15 s parado e a biblioteca desiste depois de
  //      mais 15 s sem resposta. O poste reconecta e assina de novo entre 15 e 31 s
  g_quedasWifi++;   // a conexão morreu, mas o Wi-Fi continua no ar
  size_t ins26b = g_mqttInscricoes.size(); unsigned long t26b = g_millis;
  while (g_mqttInscricoes.size() == ins26b && g_millis - t26b < 120000) { loop(); rede(); camera(); }
  snprintf(d, sizeof d, "conexao morta sem aviso: reconectou e assinou de novo em %lu s (keepalive de 15 s)", (g_millis - t26b) / 1000);
  confere(g_millis - t26b >= 15000 && g_millis - t26b <= 31000, d);
  rodar(3500); g_mqttEntrada.push_back("buraco"); rodar(300);   // passa a janela de 3 s depois da inscrição
  confere(std::string(led()) == "vermelho", "depois de reconectar, o buraco seguinte chega");
  rodar(62000);

  // ---------- Partida sem memória (fila ou tarefa) ----------
  // 27) sem fila do envio: avisa, segue sem envio, sensores e LED continuam
  g_tarefaRede = false; g_filasAteFalhar = 1; size_t n27 = g_inicios.size(); size_t m27 = marca();
  bool ok27 = iniciarEnvio(); guardarParaEnvio(); g_adc = ADC_ESCURO; rodar(30000);
  confere(!ok27 && filaEnvio == NULL && !g_tarefaRede && tem(serialDesde(m27), "sem memoria para a fila do ThingSpeak"), "fila do envio nao criada: aviso e nenhuma tarefa de rede");
  confere(g_inicios.size() == n27 && std::string(led()) == "amarelo", "fila do envio nao criada: sem envio, mas o poste continua decidindo (escuro: amarelo)");
  // 28) a fila foi criada, mas a tarefa de rede não: libera a fila e segue sem envio
  g_tarefaQueFalha = "rede"; size_t m28 = marca(); bool ok28 = iniciarEnvio(); rodar(10000);
  confere(!ok28 && filaEnvio == NULL && tem(serialDesde(m28), "nao deu para criar a tarefa de rede"), "tarefa de rede nao criada: aviso e a fila volta a NULL");
  g_tarefaQueFalha = nullptr; bool okRede = iniciarEnvio();
  // 29) sem fila da câmera: avisa, segue sem câmera; o envio continua
  g_tarefaCamera = false; g_filasAteFalhar = 1; size_t m29 = marca(); size_t n29 = g_inicios.size();
  bool ok29 = iniciarCamera(); rodar(30000);
  confere(!ok29 && filaCamera == NULL && !g_tarefaCamera && tem(serialDesde(m29), "sem memoria para a fila da camera"), "fila da camera nao criada: aviso e nenhuma tarefa da camera");
  confere(okRede && g_inicios.size() > n29 && tem(serialDesde(m29), "t="), "fila da camera nao criada: o poste continua lendo e enviando");
  // 30) a fila da câmera foi criada, mas a tarefa não: libera a fila e segue sem câmera
  g_tarefaQueFalha = "camera"; size_t m30 = marca(); bool ok30 = iniciarCamera(); rodar(10000);
  confere(!ok30 && filaCamera == NULL && tem(serialDesde(m30), "nao deu para criar a tarefa da camera"), "tarefa da camera nao criada: aviso e a fila volta a NULL");
  g_tarefaQueFalha = nullptr;

  // ---------- Conferências do teste inteiro ----------
  // 31) cada tarefa mede a sobra da pilha
  confere(tem(Serial.buf, "Tarefa de rede: menor sobra de pilha ate agora: 4800 bytes") && tem(Serial.buf, "Tarefa da camera: menor sobra de pilha ate agora: 4800 bytes"),
          "as duas tarefas medem e avisam a sobra da pilha");
  // 32) nenhuma pausa maior que 5 s entre duas leituras (nem com pedido lento)
  std::vector<unsigned long> horas;
  for (unsigned long t : g_leiturasAdc) if (horas.empty() || horas.back() != t) horas.push_back(t);
  unsigned long maiorPausa = 0;
  for (size_t i = 1; i < horas.size(); i++) if (horas[i] - horas[i - 1] > maiorPausa) maiorPausa = horas[i] - horas[i - 1];
  snprintf(d, sizeof d, "leituras a cada 5 s durante todo o teste (%zu leituras, maior pausa: %lu ms)", horas.size(), maiorPausa);
  confere(maiorPausa <= 5010 && horas.size() > 100, d);
  // 33) nenhum pedido HTTP saiu de dentro do loop()
  snprintf(d, sizeof d, "todos os %zu pedidos sairam da tarefa de rede, nenhum do loop()", g_inicios.size());
  confere(g_pedidosForaDaRede == 0 && !g_inicios.empty(), d);
  // 34) a chave nunca aparece no serial
  confere(!tem(Serial.buf, "CHAVE_TESTE"), "a chave nunca aparece no monitor serial");
  // 35) o sketch não usa mais o HC-SR04 (o OLED também saiu)
  confere(g_leituras.empty(), "nenhuma leitura de HC-SR04 (pulseIn): o sensor saiu em 18/11");
  // 36) depois do setup(), toda escrita no serial é uma linha inteira
  int quebradas = 0;
  for (const std::string& e : Serial.escritas) if (e.size() < 2 || e.find("\r\n") != e.size() - 2) quebradas++;
  snprintf(d, sizeof d, "serial: %zu escritas depois do setup, todas linhas inteiras (%d quebradas)", Serial.escritas.size(), quebradas);
  confere(Serial.escritas.size() > 100 && quebradas == 0, d);
  // 37) a maior linha do serial cabe com folga no buffer de 240 caracteres
  size_t maior = 0; for (const std::string& e : Serial.escritas) if (e.size() > maior) maior = e.size();
  snprintf(d, sizeof d, "maior linha do serial: %zu caracteres (cabe no buffer de 240)", maior);
  confere(maior < 238, d);

  printf("\nUltima linha: %s\nUltimo envio: %s\n%d falha(s).\n", ultimaLinhaCom("t=").c_str(), g_url.c_str(), falhas);
  return falhas == 0 ? 0 : 1;
}
