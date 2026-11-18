// Versão falsa (só para teste no computador) de PubSubClient.h: imita o MQTT que o sketch usa.
// O teste escolhe se o broker está no ar (g_mqttOk) e coloca em g_mqttEntrada as mensagens
// que "chegam" no tópico. Como na biblioteca de verdade, cada mqtt.loop() entrega no
// máximo UMA mensagem. g_mqttAgendadas guarda mensagens que chegam num horário marcado.
// Também imita:
// - inscrição que falha (g_mqttAssinaturasFalhando);
// - mensagem retida no broker (g_mqttRetida), que chega g_mqttRetidaAtrasoMs depois da inscrição;
// - keepalive em duas fases, como a biblioteca: depois de "keepalive" segundos sem atividade,
//   manda um PINGREQ; se a resposta não vier em mais "keepalive" segundos, desiste.
//   Numa conexão viva, o broker responde na hora.
// - conexão morta sem aviso: se o Wi-Fi cair, a conexão parece viva (connected() continua
//   true), mas nada mais chega e o ping fica sem resposta. Broker fora do ar derruba na hora.
// - conexão fechada aqui no poste (clienteMqtt.stop()): connected() vira false na hora.
#pragma once
#include "Arduino.h"
#include "WiFi.h"
#include <vector>
#include <string>
#include <utility>
extern bool g_mqttOk; extern int g_mqttConexoes, g_mqttDesconexoes, g_mqttAssinaturasFalhando; extern std::vector<std::string> g_mqttInscricoes, g_mqttEntrada;
extern std::string g_mqttServidor; extern int g_mqttPorta, g_mqttKeepAlive; extern std::vector<std::string> g_mqttIds;
extern std::vector<std::pair<unsigned long, std::string>> g_mqttAgendadas; extern std::string g_mqttRetida; extern unsigned long g_mqttRetidaAtrasoMs;
extern int g_mqttPerdidas;  // mensagens que chegaram enquanto a conexão estava morta (perdidas)
inline int g_mqttQos = -1;  // QoS da última inscrição (-1 = inscrição simples, sem QoS)
typedef void (*CallbackMqtt)(char*, uint8_t*, unsigned int);
class PubSubClient { bool conectado = false; int quedasNaConexao = 0, paradasNaConexao = 0; bool morta = false, pingPendente = false; unsigned long ultimaAtividade = 0;
  WiFiClient* cli; CallbackMqtt cb = nullptr; public:
  PubSubClient(WiFiClient& c) : cli(&c) {}
  PubSubClient& setServer(const char* s, uint16_t p) { g_mqttServidor = s; g_mqttPorta = p; return *this; }
  PubSubClient& setCallback(CallbackMqtt c) { cb = c; return *this; }
  PubSubClient& setKeepAlive(uint16_t k) { g_mqttKeepAlive = k; return *this; }
  PubSubClient& setSocketTimeout(uint16_t) { return *this; }
  bool connect(const char* id) { g_mqttConexoes++; g_mqttIds.push_back(id); quedasNaConexao = g_quedasWifi; paradasNaConexao = cli->paradas; morta = false; pingPendente = false; ultimaAtividade = g_millis; conectado = g_mqttOk && g_wifiOk;
    // o que foi publicado enquanto o poste estava desconectado se perdeu (sessão limpa, QoS 0)
    g_mqttPerdidas += (int)g_mqttEntrada.size(); g_mqttEntrada.clear();
    for (size_t i = 0; i < g_mqttAgendadas.size();) { if (g_millis >= g_mqttAgendadas[i].first) { g_mqttPerdidas++; g_mqttAgendadas.erase(g_mqttAgendadas.begin() + i); } else i++; }
    return conectado; }
  void disconnect() { if (conectado) g_mqttDesconexoes++; conectado = false; morta = false; }
  bool connected() {
    if (!g_mqttOk) conectado = false;                       // broker fora: a conexão cai na hora
    if (cli->paradas != paradasNaConexao) conectado = false; // fechada aqui no poste
    if (conectado && !morta && g_quedasWifi != quedasNaConexao) morta = true;  // Wi-Fi caiu: conexão morta, sem aviso
    return conectado;
  }
  bool subscribe(const char* t) {
    if (g_mqttAssinaturasFalhando > 0) { g_mqttAssinaturasFalhando--; return false; }
    g_mqttInscricoes.push_back(t);
    if (!g_mqttRetida.empty()) { g_mqttAgendadas.push_back({g_millis + g_mqttRetidaAtrasoMs, g_mqttRetida}); }
    return true;
  }
  // inscrição com QoS: guarda o QoS pedido e faz o resto igual à inscrição simples
  bool subscribe(const char* t, uint8_t qos) { g_mqttQos = qos; return subscribe(t); }
  int state() { return conectado ? 0 : -2; }
  bool loop() {
    // mensagens com horário marcado que já chegaram vão para a entrada
    for (size_t i = 0; i < g_mqttAgendadas.size();) {
      if (g_millis >= g_mqttAgendadas[i].first) { g_mqttEntrada.push_back(g_mqttAgendadas[i].second); g_mqttAgendadas.erase(g_mqttAgendadas.begin() + i); }
      else i++;
    }
    if (!connected()) return false;
    unsigned long ka = (unsigned long)g_mqttKeepAlive * 1000UL;
    if (g_millis - ultimaAtividade > ka) {
      if (pingPendente) { conectado = false; return false; }   // o ping ficou sem resposta: desiste
      pingPendente = true; ultimaAtividade = g_millis;          // manda o PINGREQ
      if (!morta) pingPendente = false;                          // conexão viva: o broker responde na hora
    }
    if (morta) { g_mqttPerdidas += (int)g_mqttEntrada.size(); g_mqttEntrada.clear(); return true; }  // conexão morta: nada chega
    if (g_mqttEntrada.empty()) return true;
    std::string m = g_mqttEntrada.front(); g_mqttEntrada.erase(g_mqttEntrada.begin());  // uma por vez
    char topico[] = "celia/poste01/camera";
    ultimaAtividade = g_millis;
    cb(topico, (uint8_t*)m.data(), (unsigned int)m.size());
    return true;
  }
};
