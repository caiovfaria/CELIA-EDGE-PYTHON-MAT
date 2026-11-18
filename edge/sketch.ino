/*
  C.E.L.I.A. – Módulo Edge (poste 01): LUZ + CALOR
  =================================================
  O ESP32 lê os sensores, decide sozinho o status do trecho e mostra o resultado no LED
  (e numa linha do monitor serial a cada leitura). Uma tarefa de rede manda as leituras,
  o status e o motivo ao ThingSpeak a cada 20 s. Uma tarefa da câmera escuta a API de
  visão por MQTT: "buraco" deixa o trecho em EVITE (motivo 5) por um tempo.

  Mudança de 18/11/2026: saíram o HC-SR04 (distância na calçada) e o OLED, e entrou o
  LDR (luminosidade). A luz vai num campo novo do canal, o field7. O field4 (distância)
  fica vazio, então o Python da equipe não confunde luz com distância. A versão com
  HC-SR04 e OLED continua no histórico do git (até o commit b47342a).

  Como o poste decide (regras em celia_logica.h):
    rua escura (< 10 lux) ................... ATENÇÃO, motivo 1 (luz)
    calor (índice de calor >= 32 °C) ......... ATENÇÃO, motivo 2 (calor)
    sol direto (>= 25.000 lux) + calor ....... EVITE,   motivo 3 (luz + calor)
    buraco visto pela câmera ................. EVITE,   motivo 5 (vale 1 min na demonstração)

  Ligações (ver diagram.json):
    DHT22   -> temperatura e umidade ........ dados no pino 15
    LDR     -> módulo de luz (saída AO) ..... pino 34 (entrada analógica do ADC1)
    LED RGB -> catodo comum ................. vermelho no 25, verde no 26
  O LDR fica no pino 34 porque o ADC2 do ESP32 não funciona com o Wi-Fi ligado.
*/

#include <DHTesp.h>
#include <WiFi.h>           // Wi-Fi do ESP32 (já vem com a placa, não precisa instalar)
#include <HTTPClient.h>     // pedidos HTTP (também já vem com a placa)
#include <PubSubClient.h>   // MQTT (biblioteca PubSubClient, listada no libraries.txt)
#include "celia_logica.h"   // regras de decisão (limites, status e confirmação)

// ---------- Wi-Fi e ThingSpeak ----------
// "Wokwi-GUEST" é a rede aberta do simulador (sem senha).
const char* WIFI_NOME = "Wokwi-GUEST";
const char* WIFI_SENHA = "";
const int WIFI_CANAL = 6;  // o Wokwi recomenda o canal 6: a conexão fica mais rápida

// Write API Key do canal GS-EDGE-C.E.L.I.A. (aba "API Keys" do ThingSpeak).
// Cole a chave só na hora de rodar no Wokwi e NÃO salve o projeto com ela:
// o link do Wokwi é público. No GitHub e no projeto salvo, fica COLE_SUA_CHAVE_AQUI.
const char* THINGSPEAK_CHAVE = "COLE_SUA_CHAVE_AQUI";
const char* CHAVE_DE_EXEMPLO = "COLE_SUA_CHAVE_AQUI";

// O plano gratuito do ThingSpeak aceita no máximo um envio a cada 15 s.
// Usamos 20 s, contados a partir do FIM da tentativa anterior: assim, mesmo
// que um pedido demore, dois registros nunca ficam a menos de 20 s.
const unsigned long INTERVALO_ENVIO_MS = 20000;
const int ESPERA_MAXIMA_HTTP_MS = 5000;  // limite para conectar e para esperar a resposta
// Quando o Wi-Fi cai, o próprio ESP32 tenta reconectar. Só depois de 1 minuto
// sem conexão o sketch força uma nova tentativa (para não atrapalhar a do ESP32).
const unsigned long ESPERA_RECONEXAO_MS = 60000;

// ---------- Câmera por MQTT ----------
// broker.hivemq.com é um servidor MQTT público e gratuito: serve para o protótipo,
// mas qualquer pessoa pode publicar no tópico (limitação conhecida).
// O tópico é o combinado com o João no INTEGRACAO.md.
const char* MQTT_SERVIDOR = "broker.hivemq.com";
const int MQTT_PORTA = 1883;
const char* MQTT_TOPICO = "celia/poste01/camera";
const int MQTT_QOS = 1;  // inscrição com QoS 1: se a API publicar com QoS 1, o broker reenvia até o poste confirmar
const unsigned long MQTT_ESPERA_RECONEXAO_MS = 5000;  // sem conexão com o broker: tenta de novo a cada 5 s
// Filtro da demonstração: todo aviso que o poste processa nos primeiros 3 s depois
// de assinar o tópico é ignorado. A ideia é descartar mensagens antigas (guardadas no
// broker com "retain"), porque a mensagem não traz horário. Limites conhecidos:
// - um buraco NOVO processado nesses 3 s também é ignorado (só na partida e depois de
//   uma queda);
// - um aviso antigo processado depois dos 3 s seria aceito. Como a tarefa da câmera
//   confere o MQTT a cada 0,1 s, uma mensagem retida costuma ser processada dentro da
//   janela, mas isso não é garantido.
// A equipe aceitou essa limitação na demonstração (13/11). A solução definitiva seria
// a API mandar o horário junto (melhoria futura, a combinar com o João).
const unsigned long IGNORAR_APOS_ASSINAR_MS = 3000;
// Sem mensagens, o MQTT manda um "ping" ao broker depois de 15 s parado e espera a
// resposta por mais 15 s. Se a conexão morrer sem aviso, o poste percebe entre 15 e
// 30 s e então tenta reconectar (quanto a reconexão demora depende da rede e do broker).
const int MQTT_KEEPALIVE_S = 15;

// ---------- Pinos ----------
const int PINO_DHT = 15;
const int PINO_LDR = 34;
const int PINO_LED_VERMELHO = 25;
const int PINO_LED_VERDE = 26;

// ---------- Tempos ----------
// A documentação pede 1 minuto de confirmação. Na apresentação isso é lento,
// então o MODO_DEMONSTRACAO usa 15 segundos. Para o valor real, troque para false.
const bool MODO_DEMONSTRACAO = true;
const unsigned long INTERVALO_LEITURA_MS = 5000;  // lê os sensores a cada 5 s
const unsigned long TEMPO_CONFIRMACAO_MS = MODO_DEMONSTRACAO ? 15000UL : 60000UL;
// Um aviso de buraco vale 10 min (proposta da documentação). Na demonstração, 1 min.
const unsigned long TEMPO_BURACO_MS = MODO_DEMONSTRACAO ? 60000UL : 600000UL;
// O LDR é lido 8 vezes seguidas e o poste usa a média: tira o "chiado" do ADC.
const int LEITURAS_LDR = 8;

// ---------- Objetos dos componentes ----------
DHTesp dht;
Confirmador luz;                          // guarda a situação confirmada da luz

// ---------- Últimas leituras ----------
// Se o DHT22 falhar numa leitura, continuamos com o último valor bom e avisamos no serial.
// O LDR sempre devolve um número (não tem como "falhar" como o DHT22).
float temperatura = 0;
float umidade = 0;
float indiceCalor = 0;
int adcLuz = 0;                    // média das leituras do ADC (0 a 4095)
long luxLidos = 0;                 // a mesma leitura convertida em lux
bool temLeituraCalor = false;      // o DHT22 já deu pelo menos uma leitura válida?
bool falhouCalor = false;          // a última tentativa de ler o DHT22 falhou?
unsigned long ultimaLeitura = 0;

// ---------- Último status decidido ----------
bool temStatus = false;            // o poste já decidiu algum status? (com SEM DADOS, ainda não)
int ultimoStatus = STATUS_ACESSIVEL;
int ultimoMotivo = MOTIVO_NENHUM;

// ---------- Pacote para o ThingSpeak ----------
// Depois de cada leitura, o loop() copia tudo o que o envio precisa para um
// "pacote" e deixa esse pacote numa fila de 1 lugar. A tarefa de rede pega o
// pacote quando chega a hora de enviar. Como cada um mexe só na sua cópia,
// o loop() e a tarefa de rede nunca leem uma variável pela metade.
struct PacoteEnvio {
  bool temStatus;        // false enquanto o poste está em SEM DADOS
  bool falhouCalor;      // o DHT22 falhou na última leitura?
  float temperatura;
  float umidade;
  float indiceCalor;
  long lux;
  int status;
  int motivo;
};
QueueHandle_t filaEnvio;                 // a fila de 1 lugar (recurso do FreeRTOS, que já vem no ESP32)

// ---------- Câmera ----------
// A tarefa da câmera recebe o "buraco" e deixa um aviso na fila da câmera (também de
// 1 lugar). O loop() pega o aviso e marca quando ele chegou.
QueueHandle_t filaCamera;
bool temBuraco = false;                  // (só o loop() mexe) recebeu um aviso de buraco?
uint32_t buracoDesde = 0;                // (só o loop() mexe) quando o aviso chegou

// ---------- Usadas só pela tarefa de rede (ThingSpeak) ----------
unsigned long fimUltimaTentativa = 0;    // quando terminou a última tentativa de envio
bool wifiConectado = false;              // só para avisar no serial quando o Wi-Fi conecta ou cai
unsigned long ultimaVezComWiFi = 0;      // última vez que o Wi-Fi estava conectado
unsigned int menorFolgaRede = 0;         // menor sobra de memória da tarefa de rede (0 = ainda não medida)

// ---------- Usadas só pela tarefa da câmera (MQTT) ----------
WiFiClient clienteMqtt;                  // conexão de internet usada só pelo MQTT
PubSubClient mqtt(clienteMqtt);
unsigned long ultimaTentativaMqtt = 0;   // última tentativa de conectar no broker
unsigned long assinouEm = 0;             // quando assinou o tópico da câmera
bool mqttConectado = false;              // só para avisar no serial quando conecta ou cai
unsigned int menorFolgaCamera = 0;       // menor sobra de memória da tarefa da câmera

// Escreve uma linha no monitor serial numa escrita só: o texto e a quebra de
// linha ("\r\n") saem juntos. Como o loop() e as duas tarefas usam o serial ao
// mesmo tempo, assim uma linha nunca é cortada por outra no meio.
// (Por isso não usamos Serial.println() depois do setup(): ele faz duas escritas, o
// texto e depois a quebra de linha, e outra tarefa poderia escrever entre as duas.)
void linhaNoSerial(const char* texto) {
  char linha[260];
  snprintf(linha, sizeof(linha), "%s\r\n", texto);
  Serial.print(linha);
}

// Lê o LDR: média de 8 leituras do ADC, convertida em lux (conta em celia_logica.h).
void lerLuz() {
  long soma = 0;
  for (int i = 0; i < LEITURAS_LDR; i++) soma += analogRead(PINO_LDR);
  adcLuz = (int)(soma / LEITURAS_LDR);
  luxLidos = luxPeloAdc(adcLuz);
}

// Lê os dois sensores e calcula o índice de calor.
void lerSensores() {
  TempAndHumidity leitura = dht.getTempAndHumidity();
  if (dht.getStatus() == DHTesp::ERROR_NONE && !isnan(leitura.temperature) && !isnan(leitura.humidity)) {
    temperatura = leitura.temperature;
    umidade = leitura.humidity;
    // Índice de calor = sensação térmica, que junta temperatura e umidade (false = em °C)
    indiceCalor = dht.computeHeatIndex(temperatura, umidade, false);
    temLeituraCalor = true;
    falhouCalor = false;
  } else {
    falhouCalor = true;
    linhaNoSerial("Aviso: falha ao ler o DHT22; mantendo a leitura anterior.");
  }
  lerLuz();
}

// Verde = acessível, amarelo (vermelho + verde) = atenção, vermelho = evite.
void mostrarNoLed(int status) {
  if (status == STATUS_ACESSIVEL) {
    digitalWrite(PINO_LED_VERMELHO, LOW);
    digitalWrite(PINO_LED_VERDE, HIGH);
  } else if (status == STATUS_ATENCAO) {
    digitalWrite(PINO_LED_VERMELHO, HIGH);
    digitalWrite(PINO_LED_VERDE, HIGH);
  } else {
    digitalWrite(PINO_LED_VERMELHO, HIGH);
    digitalWrite(PINO_LED_VERDE, LOW);
  }
}

// LED apagado = o poste ainda não tem dados para decidir.
void apagarLed() {
  digitalWrite(PINO_LED_VERMELHO, LOW);
  digitalWrite(PINO_LED_VERDE, LOW);
}

// Linha de registro no monitor serial (serve de evidência do funcionamento).
// "lida" é o que o LDR leu agora; "vale" é a situação da luz já confirmada.
// No fim, quando for o caso: a contagem da confirmação e do aviso de buraco.
void registrarNoSerial(unsigned long agora, int luzLida, int luzValida, int calor, int status, int motivo,
                       unsigned long faltaMs, unsigned long buracoRestaMs) {
  char linha[240];
  int n = snprintf(linha, sizeof(linha),
                   "t=%lus | T %.1fC U %.0f%% IC %.1fC (%s) | luz %ld lux (adc %d) lida %s / vale %s | STATUS %d %s | motivo %d %s",
                   agora / 1000, temperatura, umidade, indiceCalor, nomeCalor(calor),
                   luxLidos, adcLuz, nomeLuz(luzLida), nomeLuz(luzValida),
                   status, nomeStatus(status), motivo, nomeMotivo(motivo));
  if (faltaMs > 0 && n > 0 && n < (int)sizeof(linha)) {
    n += snprintf(linha + n, sizeof(linha) - n, " | confirmando luz %lus", (faltaMs + 999) / 1000);
  }
  if (buracoRestaMs > 0 && n > 0 && n < (int)sizeof(linha)) {
    snprintf(linha + n, sizeof(linha) - n, " | buraco (camera) %lus", (buracoRestaMs + 999) / 1000);
  }
  linhaNoSerial(linha);
}

// O "cérebro" do poste: lê, decide e mostra.
void atualizarTrecho() {
  unsigned long agora = millis();
  lerSensores();

  // Luz: o LDR sempre dá uma leitura, então ela sempre conta para a confirmação.
  int luzLida = classificarLuz(luxLidos);
  atualizarConfirmacao(luz, luzLida, agora, TEMPO_CONFIRMACAO_MS);

  // Câmera: com um buraco valendo, EVITE e motivo 5 (regras em celia_logica.h).
  // O buraco vale sozinho: não precisa de leitura dos sensores.
  bool buraco = buracoValendo(temBuraco, buracoDesde, agora, TEMPO_BURACO_MS);
  unsigned long buracoRestaMs = buraco ? TEMPO_BURACO_MS - (uint32_t)(agora - buracoDesde) : 0;

  // Na partida, enquanto o DHT22 nunca respondeu, os sensores não decidem nada:
  // sem buraco, o LED fica apagado (na dúvida, não é verde) e nada vai para o ThingSpeak.
  if (!temLeituraCalor) {
    char linha[160];
    if (buraco) {
      mostrarNoLed(STATUS_EVITE);
      snprintf(linha, sizeof(linha), "DHT22 ainda sem leitura valida, mas a camera viu um buraco: EVITE (motivo buraco) | luz %ld lux | buraco (camera) %lus",
               luxLidos, (buracoRestaMs + 999) / 1000);
      temStatus = true;              // o status vem da câmera: vai para o ThingSpeak
      ultimoStatus = STATUS_EVITE;
      ultimoMotivo = MOTIVO_BURACO;
    } else {
      apagarLed();
      snprintf(linha, sizeof(linha), "SEM DADOS: aguardando a primeira leitura valida do DHT22 (luz agora: %ld lux)", luxLidos);
      temStatus = false;             // sem DHT22 e sem buraco: ainda não há status
    }
    linhaNoSerial(linha);
    return;
  }

  int luzValida = luz.confirmado;                   // o que já foi confirmado
  int calor = classificarCalor(indiceCalor);
  int status = statusComCamera(calcularStatus(luzValida, calor), buraco);
  int motivo = motivoComCamera(calcularMotivo(luzValida, calor), buraco);
  unsigned long faltaMs = msParaConfirmar(luz, agora, TEMPO_CONFIRMACAO_MS);

  mostrarNoLed(status);
  registrarNoSerial(agora, luzLida, luzValida, calor, status, motivo, faltaMs, buracoRestaMs);

  // Guarda o status: ele vai no pacote do próximo envio ao ThingSpeak
  temStatus = true;
  ultimoStatus = status;
  ultimoMotivo = motivo;
}

// Copia a situação atual para o pacote e deixa na fila de 1 lugar.
// Se ainda havia um pacote antigo esperando, o novo toma o lugar dele.
void guardarParaEnvio() {
  if (filaEnvio == NULL) return;   // sem fila (faltou memória na partida): segue sem envio
  PacoteEnvio pacote;
  pacote.temStatus = temStatus;
  pacote.falhouCalor = falhouCalor;
  pacote.temperatura = temperatura;
  pacote.umidade = umidade;
  pacote.indiceCalor = indiceCalor;
  pacote.lux = luxLidos;
  pacote.status = ultimoStatus;
  pacote.motivo = ultimoMotivo;
  xQueueOverwrite(filaEnvio, &pacote);
}

// Confere a câmera: se a tarefa da câmera deixou um aviso de buraco, ou se o aviso
// venceu, decide e mostra na hora, sem esperar a próxima leitura de 5 s.
void verCamera() {
  if (filaCamera == NULL) return;   // sem fila da câmera (faltou memória): segue sem câmera
  bool chegouBuraco = false;
  if (xQueueReceive(filaCamera, &chegouBuraco, 0) == pdTRUE && chegouBuraco) {
    temBuraco = true;
    buracoDesde = millis();          // um buraco novo recomeça a contagem
    char mensagem[80];
    snprintf(mensagem, sizeof(mensagem), "Camera: trecho em EVITE por %lu s (motivo buraco).", TEMPO_BURACO_MS / 1000);
    linhaNoSerial(mensagem);
  } else if (temBuraco && !buracoValendo(temBuraco, buracoDesde, millis(), TEMPO_BURACO_MS)) {
    temBuraco = false;
    linhaNoSerial("Camera: o aviso de buraco venceu. Volta a valer a regra dos sensores.");
  } else {
    return;                          // nada mudou
  }
  atualizarTrecho();
  guardarParaEnvio();
  ultimaLeitura = millis();
}

// Mede quanto sobrou, no pior momento até agora, da memória (a "pilha") da tarefa
// que chamou. Avisa no serial só quando a sobra diminui. No ESP32, o valor vem em bytes.
unsigned int medirPilha(const char* tarefa, unsigned int menorAteAgora) {
  unsigned int folga = uxTaskGetStackHighWaterMark(NULL);
  if (menorAteAgora == 0 || folga < menorAteAgora) {
    char mensagem[80];
    snprintf(mensagem, sizeof(mensagem), "%s: menor sobra de pilha ate agora: %u bytes", tarefa, folga);
    linhaNoSerial(mensagem);
    return folga;
  }
  return menorAteAgora;
}

// ========================= TAREFA DE REDE =========================
// Envia ao ThingSpeak e acompanha o Wi-Fi. Roda separada do loop() e da câmera:
// se um pedido HTTP demorar, só esta tarefa espera.

// Acompanha o Wi-Fi: avisa no serial quando conecta ou cai. O ESP32 tenta
// reconectar sozinho; se passar 1 minuto sem conexão, o sketch força uma nova tentativa.
void acompanharWiFi() {
  bool conectado = (WiFi.status() == WL_CONNECTED);
  if (conectado && !wifiConectado) {
    linhaNoSerial("Wi-Fi conectado.");
  } else if (!conectado && wifiConectado) {
    linhaNoSerial("Wi-Fi caiu. O poste continua decidindo sozinho.");
  }
  wifiConectado = conectado;

  if (conectado) {
    ultimaVezComWiFi = millis();
  } else if (millis() - ultimaVezComWiFi >= ESPERA_RECONEXAO_MS) {
    linhaNoSerial("Wi-Fi: 1 minuto sem conexao, tentando reconectar.");
    WiFi.reconnect();
    ultimaVezComWiFi = millis();   // conta mais 1 minuto antes de tentar de novo
  }
}

// Uma tentativa de envio ao ThingSpeak.
// Campos: 1 temperatura, 2 umidade, 3 índice de calor, 5 status, 6 motivo, 7 luminosidade (lux).
// O field4 (distância) não vai: desde 18/11 o poste não tem HC-SR04.
void enviarAoThingSpeak() {
  // Pega o pacote mais novo. Se o loop() não deixou nenhum desde o último
  // envio, não há leitura nova: melhor não mandar dado velho.
  PacoteEnvio pacote;
  if (xQueueReceive(filaEnvio, &pacote, 0) != pdTRUE) {
    linhaNoSerial("ThingSpeak: sem leitura nova desde o ultimo envio, envio pulado.");
    return;
  }
  // Na partida (SEM DADOS) ainda não há status: não manda nada. A exceção é o buraco
  // (motivo 5): ele vem da câmera, então vai mesmo assim (sem os campos do DHT22).
  if (!pacote.temStatus) {
    linhaNoSerial("ThingSpeak: ainda sem dados do DHT22, envio pulado.");
    return;
  }
  if (strcmp(THINGSPEAK_CHAVE, CHAVE_DE_EXEMPLO) == 0) {
    linhaNoSerial("ThingSpeak: cole a Write API Key no sketch para enviar.");
    return;
  }
  if (WiFi.status() != WL_CONNECTED) {
    linhaNoSerial("ThingSpeak: sem Wi-Fi, envio pulado.");
    return;
  }

  // Monta o endereço do pedido. Se o DHT22 falhou na última leitura, os campos dele
  // ficam de fora: o gráfico mostra um buraco em vez de repetir um valor velho.
  String url = "http://api.thingspeak.com/update?api_key=";
  url += THINGSPEAK_CHAVE;
  if (!pacote.falhouCalor) {
    url += "&field1=";
    url += String(pacote.temperatura, 1);
    url += "&field2=";
    url += String(pacote.umidade, 1);
    url += "&field3=";
    url += String(pacote.indiceCalor, 1);
  }
  url += "&field5=";
  url += String(pacote.status);
  url += "&field6=";
  url += String(pacote.motivo);
  url += "&field7=";
  url += String(pacote.lux);

  HTTPClient http;
  http.setConnectTimeout(ESPERA_MAXIMA_HTTP_MS);  // limite para conectar
  http.setTimeout(ESPERA_MAXIMA_HTTP_MS);         // limite para esperar a resposta
  if (!http.begin(url)) {
    linhaNoSerial("ThingSpeak: nao deu para preparar o pedido. Tenta de novo no proximo envio.");
    return;
  }
  int codigo = http.GET();             // 200 = o ThingSpeak recebeu o pedido
  String resposta = http.getString();  // número do registro gravado, ou "0" se recusou
  http.end();

  // A mensagem é montada inteira antes de ir para o serial. Ela nunca mostra a chave.
  char mensagem[120];
  if (codigo == 200 && resposta.toInt() > 0) {
    snprintf(mensagem, sizeof(mensagem), "ThingSpeak: registro %ld gravado (status %d, motivo %d, %ld lux)",
             resposta.toInt(), pacote.status, pacote.motivo, pacote.lux);
  } else if (codigo == 200) {
    snprintf(mensagem, sizeof(mensagem), "ThingSpeak: o canal recusou o envio (chave errada ou envio rapido demais).");
  } else {
    snprintf(mensagem, sizeof(mensagem), "ThingSpeak: falha no envio (codigo %d). Tenta de novo no proximo envio.", codigo);
  }
  linhaNoSerial(mensagem);
}

// Uma volta da tarefa de rede: confere o Wi-Fi e, se já passaram 20 s desde o
// FIM da última tentativa, tenta enviar.
void cicloDeRede() {
  acompanharWiFi();
  if (millis() - fimUltimaTentativa >= INTERVALO_ENVIO_MS) {
    enviarAoThingSpeak();
    fimUltimaTentativa = millis();
    menorFolgaRede = medirPilha("Tarefa de rede", menorFolgaRede);
  }
}

// A tarefa de rede repete o ciclo para sempre, com uma pausa de 100 ms.
void tarefaDeRede(void* parametro) {
  for (;;) {
    cicloDeRede();
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

// ======================== TAREFA DA CÂMERA ========================
// Escuta a API de visão por MQTT. Fica separada da tarefa de rede para que um
// pedido HTTP lento não atrase o aviso de buraco.

// A mensagem é exatamente este texto? Confere o tamanho e cada letra.
bool mensagemIgual(const byte* conteudo, unsigned int tamanho, const char* texto) {
  return tamanho == strlen(texto) && memcmp(conteudo, texto, tamanho) == 0;
}

// Chamada pela biblioteca MQTT quando chega uma mensagem no tópico da câmera.
// Só "livre" e "buraco" valem: no broker público, qualquer um pode publicar.
void aoReceberMensagem(char* topico, byte* conteudo, unsigned int tamanho) {
  bool buraco = mensagemIgual(conteudo, tamanho, "buraco");
  bool livre = mensagemIgual(conteudo, tamanho, "livre");
  if (!buraco && !livre) {
    linhaNoSerial("Camera: mensagem desconhecida, ignorada.");
    return;
  }
  if (millis() - assinouEm < IGNORAR_APOS_ASSINAR_MS) {
    linhaNoSerial("Camera: mensagem chegou junto com a inscricao (pode ser antiga), ignorada.");
    return;
  }
  if (buraco) {
    xQueueOverwrite(filaCamera, &buraco);   // o loop() pega e deixa o trecho em EVITE
    linhaNoSerial("Camera: buraco recebido.");
  } else {
    linhaNoSerial("Camera: livre (nao muda nada).");
  }
}

// Mantém a conexão com o broker MQTT e recebe as mensagens da câmera.
void manterMqtt() {
  if (WiFi.status() != WL_CONNECTED) {
    // Sem Wi-Fi, a conexão do MQTT já era. Fecha a conexão aqui no poste (stop),
    // sem tentar mandar nada ao broker. Quando o Wi-Fi voltar, conecta e assina de novo.
    if (mqttConectado) {
      clienteMqtt.stop();
      mqttConectado = false;
      linhaNoSerial("MQTT: sem Wi-Fi, conexao encerrada. Conecta de novo quando o Wi-Fi voltar.");
    }
    return;
  }
  bool conectado = mqtt.connected();
  if (!conectado && mqttConectado) {
    linhaNoSerial("MQTT caiu. Tenta conectar de novo em 5 s.");
  }
  mqttConectado = conectado;
  if (!conectado) {
    if (millis() - ultimaTentativaMqtt < MQTT_ESPERA_RECONEXAO_MS) return;
    ultimaTentativaMqtt = millis();
    char id[32];   // cada conexão com um nome diferente, para não derrubar outra no broker público
    snprintf(id, sizeof(id), "celia-poste01-%04lx", (long)random(0x10000));
    if (!mqtt.connect(id)) {
      char mensagem[80];
      snprintf(mensagem, sizeof(mensagem), "MQTT: sem conexao com o broker (estado %d). Tenta de novo em 5 s.", mqtt.state());
      linhaNoSerial(mensagem);
      return;
    }
    if (!mqtt.subscribe(MQTT_TOPICO, MQTT_QOS)) {
      // Conectado, mas sem inscrição, o poste não ouviria nada: encerra e tenta de novo.
      mqtt.disconnect();
      linhaNoSerial("MQTT: nao deu para assinar o topico da camera. Tenta de novo em 5 s.");
      return;
    }
    assinouEm = millis();
    mqttConectado = true;
    linhaNoSerial("MQTT conectado: ouvindo a camera em celia/poste01/camera.");
  }
  mqtt.loop();   // recebe o que chegou (chama aoReceberMensagem)
}

// Uma volta da tarefa da câmera.
void cicloDaCamera() {
  manterMqtt();
  menorFolgaCamera = medirPilha("Tarefa da camera", menorFolgaCamera);
}

// A tarefa da câmera repete o ciclo para sempre, com uma pausa de 100 ms.
void tarefaDaCamera(void* parametro) {
  for (;;) {
    cicloDaCamera();
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}
// ==================================================================

// Cria a fila do pacote e a tarefa de rede. Se faltar memória para alguma delas,
// avisa no serial e o poste segue sem envio: sensores, LED e câmera continuam.
bool iniciarEnvio() {
  filaEnvio = xQueueCreate(1, sizeof(PacoteEnvio));
  if (filaEnvio == NULL) {
    linhaNoSerial("Aviso: sem memoria para a fila do ThingSpeak. O poste segue sem envio.");
    return false;
  }
  fimUltimaTentativa = millis();   // o primeiro envio sai uns 20 s depois
  ultimaVezComWiFi = millis();
  // Tarefa de rede: 8 KB de memória (pilha), prioridade 1
  if (xTaskCreate(tarefaDeRede, "rede", 8192, NULL, 1, NULL) != pdPASS) {
    linhaNoSerial("Aviso: nao deu para criar a tarefa de rede. O poste segue sem envio.");
    vQueueDelete(filaEnvio);
    filaEnvio = NULL;
    return false;
  }
  linhaNoSerial("Tarefa de rede ligada: envio ao ThingSpeak a cada 20 s.");
  return true;
}

// Cria a fila do aviso de buraco e a tarefa da câmera. Se faltar memória, avisa
// no serial e o poste segue sem câmera: sensores, LED e envio continuam.
bool iniciarCamera() {
  filaCamera = xQueueCreate(1, sizeof(bool));
  if (filaCamera == NULL) {
    linhaNoSerial("Aviso: sem memoria para a fila da camera. O poste segue sem camera.");
    return false;
  }
  ultimaTentativaMqtt = millis() - MQTT_ESPERA_RECONEXAO_MS;   // tenta o MQTT assim que tiver Wi-Fi
  mqtt.setServer(MQTT_SERVIDOR, MQTT_PORTA);
  mqtt.setCallback(aoReceberMensagem);
  mqtt.setKeepAlive(MQTT_KEEPALIVE_S);
  mqtt.setSocketTimeout(5);    // espera no máximo 5 s pelo broker
  // Tarefa da câmera: 8 KB de memória (pilha), prioridade 1
  if (xTaskCreate(tarefaDaCamera, "camera", 8192, NULL, 1, NULL) != pdPASS) {
    linhaNoSerial("Aviso: nao deu para criar a tarefa da camera. O poste segue sem camera.");
    vQueueDelete(filaCamera);
    filaCamera = NULL;
    return false;
  }
  linhaNoSerial("Tarefa da camera ligada: ouvindo a API de visao por MQTT.");
  return true;
}

void setup() {
  Serial.begin(115200);
  pinMode(PINO_LDR, INPUT);
  pinMode(PINO_LED_VERMELHO, OUTPUT);
  pinMode(PINO_LED_VERDE, OUTPUT);
  apagarLed();

  dht.setup(PINO_DHT, DHTesp::DHT22);

  // Aqui ainda não há outras tarefas usando o serial: pode escrever aos pedaços.
  Serial.println("C.E.L.I.A. - modulo Edge (luz + calor) iniciado");
  Serial.print("Confirmacao da luz: ");
  Serial.print(TEMPO_CONFIRMACAO_MS / 1000);
  Serial.println(MODO_DEMONSTRACAO ? " s (modo demonstracao)" : " s");

  // Começa a conectar no Wi-Fi sem esperar: se a rede demorar ou faltar,
  // os sensores e o LED funcionam do mesmo jeito.
  Serial.print("Conectando ao Wi-Fi ");
  Serial.println(WIFI_NOME);
  WiFi.begin(WIFI_NOME, WIFI_SENHA, WIFI_CANAL);

  delay(2000);  // o DHT22 precisa de ~2 s depois de ligar para a primeira leitura
  atualizarTrecho();
  ultimaLeitura = millis();

  iniciarEnvio();      // fila do pacote e tarefa de rede (ThingSpeak)
  iniciarCamera();     // fila do aviso e tarefa da câmera (MQTT)
  guardarParaEnvio();  // primeiro pacote, com a leitura de agora
}

void loop() {
  // O loop() cuida só dos sensores e do LED: confere o relógio e lê a cada 5 s,
  // sem delay() longo. O envio ao ThingSpeak fica com a tarefa de rede, e o MQTT
  // com a tarefa da câmera. As três rodam ao mesmo tempo.
  if (millis() - ultimaLeitura >= INTERVALO_LEITURA_MS) {
    ultimaLeitura = millis();
    atualizarTrecho();
    guardarParaEnvio();
  }
  verCamera();   // aviso de buraco chegou ou venceu? mostra na hora
  delay(10);     // pequena pausa: deixa a simulação do Wokwi mais rápida
}
