/*
  C.E.L.I.A. – câmera da cadeira de rodas (ESP32-CAM)
  ===================================================
  Coleta de fotos de calçadas do Rio para treinar o modelo de buracos (HU-06: "fotos de
  calçadas do Rio tiradas pela equipe"). A câmera vai presa na cadeira, apontada para o chão,
  e salva uma foto a cada INTERVALO_FOTO_MS no cartão microSD. Não precisa de internet.

  - Se a rede do celular estiver disponível na partida, a câmera acerta o relógio pela
    internet e põe data e hora no nome das fotos (para cruzar com o GPS do celular depois).
    Sem rede, as fotos são numeradas.
  - Cada vez que liga, cria uma pasta nova no cartão (sessao_001, sessao_002...) com as
    fotos e um registro.csv (número, segundos desde que ligou, data e hora, tamanho).
  - O LED vermelho pequeno da placa pisca a cada foto. Piscando rápido sem parar: sem cartão.

  Placa: "AI Thinker ESP32-CAM". Tudo que o código usa já vem com a placa.
  Alimentação: power bank no micro-USB da base (a CAM fica encaixada na base, sem fios).

  Privacidade (RNF04 e HU-08): aponte a câmera para o chão e apague, antes de usar no
  treino, qualquer foto que mostre rosto ou placa.
*/

#include <WiFi.h>
#include <time.h>
#include "FS.h"
#include "SD_MMC.h"
#include "esp_camera.h"

// ---------- Ajustes ----------
const unsigned long INTERVALO_FOTO_MS = 5000;     // uma foto a cada 5 s (a ~1 m/s, uma a cada 5 m)
const framesize_t TAMANHO_FOTO = FRAMESIZE_VGA;   // 640x480
const int QUALIDADE_JPEG = 10;                    // 10 a 15 (número menor = foto melhor e maior)
const bool VIRAR_IMAGEM = false;
const bool ESPELHAR_IMAGEM = false;

// Rede do celular (opcional), só para acertar o relógio. Deixe como está para não usar.
const char* WIFI_NOME = "NOME_DA_REDE_2_4GHZ";
const char* WIFI_SENHA = "SENHA_DA_REDE";
const unsigned long ESPERA_WIFI_MS = 15000;       // tenta a rede por até 15 s na partida
const long FUSO_SEGUNDOS = -3 * 3600;             // horário de Brasília

const int PINO_LED = 33;     // LED vermelho pequeno da placa (acende com LOW)
const int PINO_FLASH = 4;    // flash: fica apagado

// ---------- Pinos da câmera AI-Thinker (do exemplo oficial CameraWebServer) ----------
#define PWDN_GPIO_NUM  32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM  0
#define SIOD_GPIO_NUM  26
#define SIOC_GPIO_NUM  27
#define Y9_GPIO_NUM    35
#define Y8_GPIO_NUM    34
#define Y7_GPIO_NUM    39
#define Y6_GPIO_NUM    36
#define Y5_GPIO_NUM    21
#define Y4_GPIO_NUM    19
#define Y3_GPIO_NUM    18
#define Y2_GPIO_NUM    5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM  23
#define PCLK_GPIO_NUM  22

bool relogioCerto = false;
char pastaSessao[24] = "";
unsigned long numeroFoto = 0;
unsigned long ultimaFoto = 0;

void piscar(int vezes, int duracaoMs) {
  for (int i = 0; i < vezes; i++) {
    digitalWrite(PINO_LED, LOW);
    delay(duracaoMs);
    digitalWrite(PINO_LED, HIGH);
    delay(duracaoMs);
  }
}

// Se a rede do celular estiver disponível, acerta o relógio pela internet e desliga o Wi-Fi.
void acertarRelogio() {
  if (strcmp(WIFI_NOME, "NOME_DA_REDE_2_4GHZ") == 0) {
    Serial.println("Relogio: sem rede configurada, as fotos serao numeradas.");
    return;
  }
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_NOME, WIFI_SENHA);
  unsigned long inicio = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - inicio < ESPERA_WIFI_MS) delay(250);
  if (WiFi.status() == WL_CONNECTED) {
    configTime(FUSO_SEGUNDOS, 0, "pool.ntp.org", "a.st1.ntp.br");
    struct tm agora;
    relogioCerto = getLocalTime(&agora, 10000);
    Serial.println(relogioCerto ? "Relogio: acertado pela internet." : "Relogio: sem resposta do servidor de hora.");
  } else {
    Serial.println("Relogio: rede nao encontrada, as fotos serao numeradas.");
  }
  WiFi.disconnect(true);   // a partir daqui a câmera não precisa de rede: economiza bateria
  WiFi.mode(WIFI_OFF);
}

// Cria a pasta da sessão (sessao_001, sessao_002...) e o cabeçalho do registro.csv.
bool criarSessao() {
  for (int n = 1; n < 1000; n++) {
    snprintf(pastaSessao, sizeof(pastaSessao), "/sessao_%03d", n);
    if (!SD_MMC.exists(pastaSessao)) {
      if (!SD_MMC.mkdir(pastaSessao)) return false;
      char caminho[48];
      snprintf(caminho, sizeof(caminho), "%s/registro.csv", pastaSessao);
      File registro = SD_MMC.open(caminho, FILE_WRITE);
      if (!registro) return false;
      registro.println("foto,segundos_desde_que_ligou,data_hora,bytes");
      registro.close();
      Serial.printf("Cartao: salvando em %s\n", pastaSessao);
      return true;
    }
  }
  return false;
}

void tirarFoto() {
  // Descarta a imagem guardada desde a foto anterior: a foto salva é a de agora.
  camera_fb_t* velha = esp_camera_fb_get();
  if (velha != NULL) esp_camera_fb_return(velha);
  camera_fb_t* foto = esp_camera_fb_get();
  if (foto == NULL) {
    Serial.println("Camera: nao conseguiu tirar a foto.");
    return;
  }
  numeroFoto++;
  char dataHora[24] = "";
  char nome[64];
  struct tm agora;
  if (relogioCerto && getLocalTime(&agora, 100)) {
    strftime(dataHora, sizeof(dataHora), "%Y-%m-%d %H:%M:%S", &agora);
    char carimbo[20];
    strftime(carimbo, sizeof(carimbo), "%Y%m%d_%H%M%S", &agora);
    snprintf(nome, sizeof(nome), "%s/foto_%05lu_%s.jpg", pastaSessao, numeroFoto, carimbo);
  } else {
    snprintf(nome, sizeof(nome), "%s/foto_%05lu.jpg", pastaSessao, numeroFoto);
  }

  File arquivo = SD_MMC.open(nome, FILE_WRITE);
  bool salvou = arquivo && arquivo.write(foto->buf, foto->len) == foto->len;
  if (arquivo) arquivo.close();
  size_t bytes = foto->len;
  esp_camera_fb_return(foto);

  if (!salvou) {
    Serial.printf("Cartao: falhou ao salvar %s (cartao cheio ou solto?)\n", nome);
    piscar(5, 60);
    return;
  }
  char caminho[48];
  snprintf(caminho, sizeof(caminho), "%s/registro.csv", pastaSessao);
  File registro = SD_MMC.open(caminho, FILE_APPEND);
  if (registro) {
    registro.printf("%lu,%lu,%s,%u\n", numeroFoto, millis() / 1000, dataHora, (unsigned)bytes);
    registro.close();
  }
  Serial.printf("Foto %lu salva: %s (%u bytes)\n", numeroFoto, nome, (unsigned)bytes);
  piscar(1, 80);
}

bool iniciarCamera() {
  camera_config_t config = {};
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = TAMANHO_FOTO;
  config.jpeg_quality = QUALIDADE_JPEG;
  config.fb_count = 1;
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = psramFound() ? CAMERA_FB_IN_PSRAM : CAMERA_FB_IN_DRAM;
  esp_err_t erro = esp_camera_init(&config);
  if (erro != ESP_OK) {
    Serial.printf("Camera: falhou ao iniciar (erro 0x%x). Confira o cabo da camera.\n", erro);
    return false;
  }
  sensor_t* sensor = esp_camera_sensor_get();
  if (sensor != NULL) {
    sensor->set_vflip(sensor, VIRAR_IMAGEM ? 1 : 0);
    sensor->set_hmirror(sensor, ESPELHAR_IMAGEM ? 1 : 0);
  }
  return true;
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("C.E.L.I.A. - camera da cadeira (coleta de fotos, HU-06)");
  pinMode(PINO_LED, OUTPUT);
  digitalWrite(PINO_LED, HIGH);   // apagado
  pinMode(PINO_FLASH, OUTPUT);
  digitalWrite(PINO_FLASH, LOW);

  if (!iniciarCamera()) {
    while (true) piscar(3, 300);   // sem câmera: pisca devagar para sempre
  }
  // Modo de 1 fio do cartão: deixa o flash (IO4) apagado e libera o IO12 e o IO13.
  while (!SD_MMC.begin("/sdcard", true) || SD_MMC.cardType() == CARD_NONE || !criarSessao()) {
    Serial.println("Cartao: nao encontrado. Coloque um microSD (FAT32) e espere.");
    SD_MMC.end();
    piscar(10, 50);
    delay(4000);
  }
  acertarRelogio();
  ultimaFoto = millis() - INTERVALO_FOTO_MS;   // primeira foto logo de cara
}

void loop() {
  if (millis() - ultimaFoto >= INTERVALO_FOTO_MS) {
    ultimaFoto = millis();
    tirarFoto();
  }
  delay(20);
}
