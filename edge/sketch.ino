/*
  C.E.L.I.A. – Módulo Edge (poste 01)
  ===================================
  Etapa 1: o ESP32 lê os sensores, decide sozinho o status do trecho
  e mostra o resultado no OLED e no LED. Ainda sem internet:
  o envio ao ThingSpeak entra na Etapa 2 e o resultado da câmera
  (MQTT) entra na Etapa 4.

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
#include "celia_logica.h"   // regras de decisão (limites, status e confirmação)

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
void registrarNoSerial(unsigned long agora, int calcadaLida, int calcadaValida, int calor, int status, int motivo) {
  Serial.print("t=");
  Serial.print(agora / 1000);
  Serial.print("s | T ");
  Serial.print(temperatura, 1);
  Serial.print("C U ");
  Serial.print(umidade, 0);
  Serial.print("% IC ");
  Serial.print(indiceCalor, 1);
  Serial.print("C (");
  Serial.print(nomeCalor(calor));
  Serial.print(") | faixa ");
  Serial.print(distanciaCm, 0);
  Serial.print("cm lida ");
  Serial.print(nomeCalcada(calcadaLida));
  Serial.print(" / vale ");
  Serial.print(nomeCalcada(calcadaValida));
  Serial.print(" | STATUS ");
  Serial.print(status);
  Serial.print(" ");
  Serial.print(nomeStatus(status));
  Serial.print(" | motivo ");
  Serial.print(motivo);
  Serial.print(" ");
  Serial.println(nomeMotivo(motivo));
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

  delay(2000);  // o DHT22 precisa de ~2 s depois de ligar para a primeira leitura
  atualizarTrecho();
  ultimaLeitura = millis();
}

void loop() {
  // Em vez de ficar parado num delay() longo, o ESP32 confere o relógio e só lê
  // quando passou o intervalo. Assim, nas próximas etapas, ele consegue cuidar
  // do Wi-Fi e do MQTT ao mesmo tempo.
  if (millis() - ultimaLeitura >= INTERVALO_LEITURA_MS) {
    ultimaLeitura = millis();
    atualizarTrecho();
  }
  delay(10);  // pequena pausa: deixa a simulação do Wokwi mais rápida
}
