/*
  C.E.L.I.A. – Módulo Edge (poste 01)
  ===================================
  Etapa 1: o ESP32 lê os sensores, decide sozinho o status do trecho
  e mostra o resultado no OLED e no LED.
  Etapa 2: manda as leituras, o status e o motivo para o canal público do
  ThingSpeak, pelo Wi-Fi, com 20 s de folga entre um envio e o seguinte.
  O envio roda numa tarefa separada (a "tarefa de rede"): se a internet
  cair ou ficar lenta, o loop() continua lendo os sensores e mostrando
  no OLED e no LED do mesmo jeito.
  O resultado da câmera (MQTT) entra na Etapa 4.

  Ligações (ver diagram.json):
    DHT22   -> temperatura e umidade ........ dados no pino 15
    HC-SR04 -> distância livre na calçada ... TRIG no 5, ECHO no 18
    OLED    -> SSD1306 128x64 (I2C) ......... SDA no 21, SCL no 22
    LED RGB -> catodo comum ................. vermelho no 25, verde no 26
*/

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHTesp.h>
#include <WiFi.h>           // Wi-Fi do ESP32 (já vem com a placa, não precisa instalar)
#include <HTTPClient.h>     // pedidos HTTP (também já vem com a placa)
#include "celia_logica.h"   // regras de decisão (limites, status e confirmação)

// ---------- Wi-Fi e ThingSpeak ----------
// "Wokwi-GUEST" é a rede aberta do simulador (sem senha).
const char* WIFI_NOME = "Wokwi-GUEST";
const char* WIFI_SENHA = "";
const int WIFI_CANAL = 6;  // o Wokwi recomenda o canal 6: a conexão fica mais rápida

// Write API Key do canal (aba "API Keys" do ThingSpeak).
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

// ---------- Pinos ----------
const int PINO_DHT = 15;
const int PINO_TRIG = 5;
const int PINO_ECHO = 18;
const int PINO_LED_VERMELHO = 25;
const int PINO_LED_VERDE = 26;

// ---------- Tempos ----------
// A documentação pede 1 minuto de confirmação. Na apresentação isso é lento,
// então o MODO_DEMONSTRACAO usa 15 segundos. Para o valor real, troque para false.
const bool MODO_DEMONSTRACAO = true;
const unsigned long INTERVALO_LEITURA_MS = 5000;  // lê os sensores a cada 5 s
const unsigned long TEMPO_CONFIRMACAO_MS = MODO_DEMONSTRACAO ? 15000UL : 60000UL;

// ---------- Objetos dos componentes ----------
Adafruit_SSD1306 tela(128, 64, &Wire, -1);  // OLED 128x64, sem pino de reset
DHTesp dht;
Confirmador calcada;                         // guarda a situação confirmada da calçada

// ---------- Últimas leituras válidas ----------
// Se um sensor falhar numa leitura, continuamos com o último valor bom
// e avisamos na linha de cima do OLED.
float temperatura = 0;
float umidade = 0;
float indiceCalor = 0;
float distanciaCm = 0;
bool temLeituraCalor = false;      // o DHT22 já deu pelo menos uma leitura válida?
bool temLeituraDistancia = false;  // o HC-SR04 já deu pelo menos um eco válido?
bool falhouCalor = false;          // a última tentativa de ler o DHT22 falhou?
bool falhouDistancia = false;      // a última tentativa de ler o HC-SR04 falhou?
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
  bool temStatus;        // false enquanto o OLED mostra SEM DADOS
  bool falhouCalor;      // o DHT22 falhou na última leitura?
  bool falhouDistancia;  // o HC-SR04 falhou na última leitura?
  float temperatura;
  float umidade;
  float indiceCalor;
  int distanciaCm;
  int status;
  int motivo;
};
QueueHandle_t filaEnvio;                 // a fila de 1 lugar (recurso do FreeRTOS, que já vem no ESP32)

// ---------- Usadas só pela tarefa de rede ----------
unsigned long fimUltimaTentativa = 0;    // quando terminou a última tentativa de envio
bool wifiConectado = false;              // só para avisar no serial quando o Wi-Fi conecta ou cai
unsigned long ultimaVezComWiFi = 0;      // última vez que o Wi-Fi estava conectado
unsigned int menorFolgaPilha = 0;        // menor sobra de memória da tarefa de rede (0 = ainda não medida)

// Mede a distância livre com o HC-SR04. Devolve -1 se não houver eco.
float medirDistanciaCm() {
  digitalWrite(PINO_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PINO_TRIG, HIGH);  // um pulso de 10 µs manda o sensor emitir o som
  delayMicroseconds(10);
  digitalWrite(PINO_TRIG, LOW);

  // ECHO fica em HIGH pelo tempo de ida e volta do som. Espera no máximo 30 ms (~5 m).
  unsigned long duracaoUs = pulseIn(PINO_ECHO, HIGH, 30000);
  if (duracaoUs == 0) return -1;

  return distanciaPeloEco(duracaoUs);  // conversão em celia_logica.h (340 m/s, cm inteiros)
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
    Serial.println("Aviso: falha ao ler o DHT22; mantendo a leitura anterior.");
  }

  float distancia = medirDistanciaCm();
  if (distancia >= 0) {
    distanciaCm = distancia;
    temLeituraDistancia = true;
    falhouDistancia = false;
  } else {
    falhouDistancia = true;
    Serial.println("Aviso: HC-SR04 sem eco; mantendo a leitura anterior.");
  }
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

// Mostra no OLED o status, o motivo e as duas leituras.
// A linha "Faixa" mostra o que o sensor leu por último; o status grande só muda
// depois da confirmação (enquanto isso, a linha de cima mostra a contagem).
void mostrarNaTela(int status, int motivo, int calcadaLida, int calor, unsigned long faltaMs) {
  tela.clearDisplay();
  tela.setTextColor(SSD1306_WHITE);

  // Linha de cima: aviso de falha, contagem da confirmação ou nome do poste
  tela.setTextSize(1);
  tela.setCursor(0, 0);
  if (falhouDistancia && falhouCalor) {
    tela.print("Aviso: falha sensores");
  } else if (falhouDistancia) {
    tela.print("Aviso: falha HC-SR04");
  } else if (falhouCalor) {
    tela.print("Aviso: falha DHT22");
  } else if (faltaMs > 0) {
    tela.print("Confirmando... ");
    tela.print((faltaMs + 999) / 1000);  // arredonda para cima, em segundos
    tela.print("s");
  } else {
    tela.print("C.E.L.I.A. poste 01");
  }

  // Status em letras grandes
  tela.setTextSize(2);
  tela.setCursor(0, 13);
  tela.print(nomeStatus(status));

  // Detalhes
  tela.setTextSize(1);
  tela.setCursor(0, 34);
  tela.print("Motivo: ");
  tela.print(nomeMotivo(motivo));

  tela.setCursor(0, 44);
  tela.print("Faixa ");
  tela.print((int)distanciaCm);
  tela.print("cm ");
  tela.print(nomeCalcada(calcadaLida));

  tela.setCursor(0, 54);
  tela.print("Calor ");
  tela.print(indiceCalor, 1);
  tela.print("C ");
  tela.print(nomeCalor(calor));

  tela.display();
}

// Tela da partida, enquanto algum sensor ainda não deu uma leitura válida.
void mostrarSemDados() {
  tela.clearDisplay();
  tela.setTextColor(SSD1306_WHITE);
  tela.setTextSize(1);
  tela.setCursor(0, 0);
  tela.print("C.E.L.I.A. poste 01");

  tela.setTextSize(2);
  tela.setCursor(0, 13);
  tela.print("SEM DADOS");

  tela.setTextSize(1);
  tela.setCursor(0, 34);
  tela.print("Aguardando sensores");
  // estado da tentativa ATUAL de cada sensor
  tela.setCursor(0, 44);
  tela.print("HC-SR04: ");
  tela.print(falhouDistancia ? "sem eco" : "ok");
  tela.setCursor(0, 54);
  tela.print("DHT22: ");
  tela.print(falhouCalor ? "sem resposta" : "ok");

  tela.display();
}

// Linha de registro no monitor serial (serve de evidência do funcionamento).
// A linha é montada inteira antes de ir para o serial (snprintf): como a tarefa
// de rede também escreve no serial, assim uma mensagem não corta a outra no meio.
void registrarNoSerial(unsigned long agora, int calcadaLida, int calcadaValida, int calor, int status, int motivo) {
  char linha[180];
  snprintf(linha, sizeof(linha),
           "t=%lus | T %.1fC U %.0f%% IC %.1fC (%s) | faixa %dcm lida %s / vale %s | STATUS %d %s | motivo %d %s",
           agora / 1000, temperatura, umidade, indiceCalor, nomeCalor(calor),
           (int)distanciaCm, nomeCalcada(calcadaLida), nomeCalcada(calcadaValida),
           status, nomeStatus(status), motivo, nomeMotivo(motivo));
  Serial.println(linha);
}

// O "cérebro" do poste: lê, decide e mostra.
void atualizarTrecho() {
  unsigned long agora = millis();
  lerSensores();

  // Calçada: só uma leitura válida conta para a confirmação.
  if (falhouDistancia) {
    cancelarConfirmacao(calcada);  // sem eco: a mudança em andamento recomeça do zero
  } else {
    atualizarConfirmacao(calcada, classificarCalcada(distanciaCm), agora, TEMPO_CONFIRMACAO_MS);
  }

  // Na partida, enquanto algum sensor nunca respondeu, o poste não decide nada:
  // mostra SEM DADOS e deixa o LED apagado (na dúvida, não é verde).
  if (!calcada.iniciado || !temLeituraCalor) {
    apagarLed();
    mostrarSemDados();
    Serial.println("Aguardando a primeira leitura valida dos dois sensores...");
    return;
  }

  int calcadaLida = classificarCalcada(distanciaCm);  // o que o sensor leu por último
  int calcadaValida = calcada.confirmado;             // o que já foi confirmado
  int calor = classificarCalor(indiceCalor);
  int status = calcularStatus(calcadaValida, calor);
  int motivo = calcularMotivo(calcadaValida, calor);
  unsigned long faltaMs = msParaConfirmar(calcada, agora, TEMPO_CONFIRMACAO_MS);

  mostrarNoLed(status);
  mostrarNaTela(status, motivo, calcadaLida, calor, faltaMs);
  registrarNoSerial(agora, calcadaLida, calcadaValida, calor, status, motivo);

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
  pacote.falhouDistancia = falhouDistancia;
  pacote.temperatura = temperatura;
  pacote.umidade = umidade;
  pacote.indiceCalor = indiceCalor;
  pacote.distanciaCm = (int)distanciaCm;
  pacote.status = ultimoStatus;
  pacote.motivo = ultimoMotivo;
  xQueueOverwrite(filaEnvio, &pacote);
}

// ========================= TAREFA DE REDE =========================
// Tudo daqui até o setup() roda na tarefa de rede, separada do loop().
// Se um pedido HTTP demorar, só esta tarefa espera.

// Acompanha o Wi-Fi: avisa no serial quando conecta ou cai. O ESP32 tenta
// reconectar sozinho; se passar 1 minuto sem conexão, o sketch força uma nova
// tentativa (e espera mais 1 minuto antes de forçar de novo).
void acompanharWiFi() {
  bool conectado = (WiFi.status() == WL_CONNECTED);
  if (conectado && !wifiConectado) {
    Serial.println("Wi-Fi conectado.");
  } else if (!conectado && wifiConectado) {
    Serial.println("Wi-Fi caiu. O poste continua decidindo sozinho.");
  }
  wifiConectado = conectado;

  if (conectado) {
    ultimaVezComWiFi = millis();
  } else if (millis() - ultimaVezComWiFi >= ESPERA_RECONEXAO_MS) {
    Serial.println("Wi-Fi: 1 minuto sem conexao, tentando reconectar.");
    WiFi.reconnect();
    ultimaVezComWiFi = millis();   // conta mais 1 minuto antes de tentar de novo
  }
}

// Uma tentativa de envio ao ThingSpeak.
// Campos: 1 temperatura, 2 umidade, 3 índice de calor, 4 distância, 5 status, 6 motivo.
void enviarAoThingSpeak() {
  // Pega o pacote mais novo. Se o loop() não deixou nenhum desde o último
  // envio, não há leitura nova: melhor não mandar dado velho.
  PacoteEnvio pacote;
  if (xQueueReceive(filaEnvio, &pacote, 0) != pdTRUE) {
    Serial.println("ThingSpeak: sem leitura nova desde o ultimo envio, envio pulado.");
    return;
  }
  // Na partida (SEM DADOS) ainda não há status. Com os dois sensores falhando,
  // o status seria só a repetição do antigo. Nos dois casos, não manda nada:
  // depois de 10 min sem envio, o app mostra o trecho em cinza (sem dados).
  if (!pacote.temStatus) {
    Serial.println("ThingSpeak: ainda sem dados dos sensores, envio pulado.");
    return;
  }
  if (pacote.falhouCalor && pacote.falhouDistancia) {
    Serial.println("ThingSpeak: os dois sensores falharam, envio pulado.");
    return;
  }
  if (strcmp(THINGSPEAK_CHAVE, CHAVE_DE_EXEMPLO) == 0) {
    Serial.println("ThingSpeak: cole a Write API Key no sketch para enviar.");
    return;
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("ThingSpeak: sem Wi-Fi, envio pulado.");
    return;
  }

  // Monta o endereço do pedido. Se um sensor falhou na última leitura, o campo
  // dele fica de fora: o gráfico mostra um buraco em vez de repetir um valor velho.
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
  if (!pacote.falhouDistancia) {
    url += "&field4=";
    url += String(pacote.distanciaCm);
  }
  url += "&field5=";
  url += String(pacote.status);
  url += "&field6=";
  url += String(pacote.motivo);

  HTTPClient http;
  http.setConnectTimeout(ESPERA_MAXIMA_HTTP_MS);  // limite para conectar
  http.setTimeout(ESPERA_MAXIMA_HTTP_MS);         // limite para esperar a resposta
  if (!http.begin(url)) {
    Serial.println("ThingSpeak: nao deu para preparar o pedido. Tenta de novo no proximo envio.");
    return;
  }
  int codigo = http.GET();             // 200 = o ThingSpeak recebeu o pedido
  String resposta = http.getString();  // número do registro gravado, ou "0" se recusou
  http.end();

  // A mensagem é montada inteira antes de ir para o serial. Ela nunca mostra a chave.
  char mensagem[120];
  if (codigo == 200 && resposta.toInt() > 0) {
    snprintf(mensagem, sizeof(mensagem), "ThingSpeak: registro %ld gravado (status %d, motivo %d)",
             resposta.toInt(), pacote.status, pacote.motivo);
  } else if (codigo == 200) {
    snprintf(mensagem, sizeof(mensagem), "ThingSpeak: o canal recusou o envio (chave errada ou envio rapido demais).");
  } else {
    snprintf(mensagem, sizeof(mensagem), "ThingSpeak: falha no envio (codigo %d). Tenta de novo no proximo envio.", codigo);
  }
  Serial.println(mensagem);
}

// Mede quanto sobrou, no pior momento até agora, da memória de 8 KB da tarefa
// de rede (a "pilha"). Avisa no serial só quando a sobra diminui: serve para
// conferir se 8 KB bastam para o HTTP. No ESP32, o valor vem em bytes.
void medirPilha() {
  unsigned int folga = uxTaskGetStackHighWaterMark(NULL);
  if (menorFolgaPilha == 0 || folga < menorFolgaPilha) {
    menorFolgaPilha = folga;
    char mensagem[80];
    snprintf(mensagem, sizeof(mensagem), "Tarefa de rede: menor sobra de pilha ate agora: %u bytes", folga);
    Serial.println(mensagem);
  }
}

// Uma volta da tarefa de rede: confere o Wi-Fi e, se já passaram 20 s desde o
// FIM da última tentativa, tenta enviar. O relógio é marcado depois da tentativa:
// um pedido lento empurra o próximo para frente, e dois registros nunca ficam
// a menos de 20 s um do outro.
void cicloDeRede() {
  acompanharWiFi();
  if (millis() - fimUltimaTentativa >= INTERVALO_ENVIO_MS) {
    enviarAoThingSpeak();
    fimUltimaTentativa = millis();
    medirPilha();
  }
}

// A tarefa de rede repete o ciclo para sempre, com uma pausa de 100 ms.
// vTaskDelay é a pausa do FreeRTOS: ela libera o processador para o loop().
void tarefaDeRede(void* parametro) {
  for (;;) {
    cicloDeRede();
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}
// ==================================================================

// Cria a fila e a tarefa de rede. Se faltar memória para alguma delas, avisa
// no serial e o poste segue sem envio: sensores, OLED e LED continuam funcionando.
bool iniciarEnvio() {
  filaEnvio = xQueueCreate(1, sizeof(PacoteEnvio));   // a fila de 1 lugar para o pacote
  if (filaEnvio == NULL) {
    Serial.println("Aviso: sem memoria para a fila do ThingSpeak. O poste segue sem envio.");
    return false;
  }
  fimUltimaTentativa = millis();   // o primeiro envio sai uns 20 s depois
  ultimaVezComWiFi = millis();
  // Tarefa de rede: 8 KB de memória (pilha), prioridade 1
  if (xTaskCreate(tarefaDeRede, "rede", 8192, NULL, 1, NULL) != pdPASS) {
    Serial.println("Aviso: nao deu para criar a tarefa de rede. O poste segue sem envio.");
    vQueueDelete(filaEnvio);   // sem a tarefa, ninguém leria a fila: devolve a memória
    filaEnvio = NULL;          // e o loop() para de guardar pacotes
    return false;
  }
  Serial.println("Tarefa de rede ligada: envio ao ThingSpeak a cada 20 s.");
  return true;
}

void setup() {
  Serial.begin(115200);
  pinMode(PINO_TRIG, OUTPUT);
  pinMode(PINO_ECHO, INPUT);
  pinMode(PINO_LED_VERMELHO, OUTPUT);
  pinMode(PINO_LED_VERDE, OUTPUT);
  apagarLed();

  dht.setup(PINO_DHT, DHTesp::DHT22);

  if (!tela.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {  // 0x3C = endereço I2C do OLED
    Serial.println("Aviso: OLED nao encontrado.");
  }
  tela.clearDisplay();
  tela.setTextColor(SSD1306_WHITE);
  tela.setTextSize(1);
  tela.setCursor(0, 0);
  tela.println("C.E.L.I.A.");
  tela.println("Iniciando sensores...");
  tela.display();

  Serial.println("C.E.L.I.A. - modulo Edge iniciado");
  Serial.print("Confirmacao da calcada: ");
  Serial.print(TEMPO_CONFIRMACAO_MS / 1000);
  Serial.println(MODO_DEMONSTRACAO ? " s (modo demonstracao)" : " s");

  // Começa a conectar no Wi-Fi sem esperar: se a rede demorar ou faltar,
  // os sensores, o OLED e o LED funcionam do mesmo jeito.
  Serial.print("Conectando ao Wi-Fi ");
  Serial.println(WIFI_NOME);
  WiFi.begin(WIFI_NOME, WIFI_SENHA, WIFI_CANAL);

  delay(2000);  // o DHT22 precisa de ~2 s depois de ligar para a primeira leitura
  atualizarTrecho();
  ultimaLeitura = millis();

  iniciarEnvio();      // cria a fila e liga a tarefa de rede
  guardarParaEnvio();  // primeiro pacote, com a leitura de agora
}

void loop() {
  // O loop() cuida só dos sensores, do OLED e do LED: confere o relógio e lê
  // a cada 5 s, sem delay() longo. O envio ao ThingSpeak fica com a tarefa de
  // rede, que roda ao mesmo tempo. Na Etapa 4, o MQTT entra do mesmo jeito.
  if (millis() - ultimaLeitura >= INTERVALO_LEITURA_MS) {
    ultimaLeitura = millis();
    atualizarTrecho();
    guardarParaEnvio();
  }
  delay(10);  // pequena pausa: deixa a simulação do Wokwi mais rápida
}
