/*
  C.E.L.I.A. – câmera do poste: vídeo ao vivo + foto para a API a cada 20 s
  =========================================================================
  - Vídeo: abra no navegador o endereço que aparece no Monitor Serial. A página
    mostra só a imagem, no meio da tela. O vídeo sai em 320x240 para ficar liso.
  - API: a cada 20 s tira uma foto em 640x480 (mais detalhe para achar buraco) e
    manda para a API do João (POST com o JPEG puro, cabeçalhos X-Chave e X-Poste).
    Enquanto a chave estiver com o texto de exemplo, nada é enviado: só o vídeo funciona.

  Respostas da API: 201 deu certo · 401 chave errada · 400, 413 ou 415 foto vazia,
  grande demais ou fora do JPEG.

  Backlog de 15/11: HU-23 / RF22 (foto da calçada a cada 20 s para a API) e RNF11 (chave
  no cabeçalho). Quem decide se tem buraco é a API (RF09). Ela guarda a captura (RF23) e
  avisa o ESP32 do poste por MQTT (RF10), que fica vermelho com motivo 5 (RN02, HU-07).
  Esse ESP32 é o do Wokwi (pasta edge/), que lê o DHT22 e o LDR (RF14, RNF03).

  Placa: "AI Thinker ESP32-CAM". Tudo que o código usa já vem com a placa.
  Uma pessoa assistindo o vídeo por vez.
  NÃO salve no GitHub a senha do Wi-Fi nem a chave da câmera.
*/

#include "esp_camera.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "esp_http_server.h"

// ---------- Rede e API (cole os valores reais só na hora de gravar) ----------
const char* WIFI_NOME = "NOME_DA_REDE_2_4GHZ";   // o ESP32 só conecta em rede de 2,4 GHz
const char* WIFI_SENHA = "SENHA_DA_REDE";
// Endereço da API do João: o túnel muda quando ele reinicia a API (https também funciona)
const char* API_URL = "http://ENDERECO_DO_TUNEL/poste/captura";
const char* CHAVE_CAMERA = "COLE_A_CHAVE_DA_CAMERA_AQUI";   // a chave que o João mandou no privado
const char* CHAVE_DE_EXEMPLO = "COLE_A_CHAVE_DA_CAMERA_AQUI";
const char* ID_POSTE = "poste01";

// ---------- Tempos ----------
const unsigned long INTERVALO_FOTO_MS = 20000;    // uma foto a cada 20 s (contados do fim do envio anterior)
const int ESPERA_HTTP_MS = 10000;                 // limite para conectar e para a resposta da API
const unsigned long ESPERA_RECONEXAO_MS = 60000;  // 1 min sem Wi-Fi: força uma nova tentativa

// ---------- Imagem ----------
const framesize_t RESOLUCAO_VIDEO = FRAMESIZE_QVGA;  // 320x240: vídeo leve e liso
const framesize_t RESOLUCAO_FOTO = FRAMESIZE_VGA;    // 640x480: foto que vai para a API
const int LARGURA_FOTO = 640;                        // largura da RESOLUCAO_FOTO
const int ALTURA_FOTO = 480;                         // altura da RESOLUCAO_FOTO
const int QUADROS_PARA_DESCARTAR = 3;                // quadros antigos que ainda podem estar na fila
const int TENTATIVAS_FOTO = 6;                       // quadros novos que a foto tenta antes de desistir
const int ESPERA_CAMERA_MS = 3000;                   // quanto a foto espera o vídeo largar a câmera
const int QUALIDADE = 14;                            // 10 a 20: número maior = imagem mais leve
const int PINO_FLASH = 4;                            // flash da placa: fica apagado

// ---------- Pinos da câmera AI-Thinker ----------
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

#define LIMITE "celiaquadro"
static const char* TIPO_VIDEO = "multipart/x-mixed-replace;boundary=" LIMITE;
static const char* SEPARADOR = "\r\n--" LIMITE "\r\n";
static const char* CABECALHO_QUADRO = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

static const char PAGINA[] = R"rawliteral(<!DOCTYPE html>
<html lang="pt-br"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>C.E.L.I.A. - camera do poste</title>
<style>
html,body{margin:0;height:100%;background:#000}
body{display:flex;align-items:center;justify-content:center}
img{width:min(100vw,133vh);height:auto}
</style></head><body>
<img id="video" alt="Video ao vivo da camera do poste">
<script>document.getElementById('video').src = 'http://' + location.hostname + ':81/stream';</script>
</body></html>)rawliteral";

httpd_handle_t servidorPagina = NULL;
httpd_handle_t servidorVideo = NULL;
bool servidoresIniciados = false;
bool cameraOk = false;
bool wifiConectado = false;
unsigned long ultimaVezComWiFi = 0;
unsigned long fimUltimoEnvio = 0;
unsigned long numeroFoto = 0;

// O vídeo (servidor da porta 81) e a foto da API usam a mesma câmera. A trava garante que só
// um deles pega quadros por vez: quando a foto pede a câmera (fotoPedida), o vídeo termina o
// quadro que está mandando, larga a trava e espera a foto acabar.
SemaphoreHandle_t travaCamera = NULL;
volatile bool fotoPedida = false;

// Página: só a imagem, no meio da tela
static esp_err_t enviarPagina(httpd_req_t* req) {
  httpd_resp_set_type(req, "text/html; charset=utf-8");
  return httpd_resp_send(req, PAGINA, HTTPD_RESP_USE_STRLEN);
}

// Vídeo: um quadro atrás do outro enquanto a página estiver aberta
static esp_err_t enviarVideo(httpd_req_t* req) {
  if (travaCamera == NULL) return ESP_FAIL;   // sem câmera (ou sem memória para a trava)
  char cabecalho[64];
  esp_err_t res = httpd_resp_set_type(req, TIPO_VIDEO);
  if (res != ESP_OK) return res;
  while (true) {
    // A foto da API está usando a câmera: espera um pouco, sem pegar quadro.
    if (fotoPedida || xSemaphoreTake(travaCamera, pdMS_TO_TICKS(100)) != pdTRUE) {
      vTaskDelay(pdMS_TO_TICKS(10));
      continue;
    }
    camera_fb_t* quadro = esp_camera_fb_get();
    if (quadro == NULL) {
      xSemaphoreGive(travaCamera);
      Serial.println("Camera: falhou ao pegar o quadro do video.");
      return ESP_FAIL;
    }
    int tamanho = snprintf(cabecalho, sizeof(cabecalho), CABECALHO_QUADRO, (unsigned)quadro->len);
    res = httpd_resp_send_chunk(req, SEPARADOR, strlen(SEPARADOR));
    if (res == ESP_OK) res = httpd_resp_send_chunk(req, cabecalho, tamanho);
    if (res == ESP_OK) res = httpd_resp_send_chunk(req, (const char*)quadro->buf, quadro->len);
    esp_camera_fb_return(quadro);
    xSemaphoreGive(travaCamera);   // larga a câmera a cada quadro: a foto pode entrar
    if (res != ESP_OK) break;      // a página foi fechada
  }
  return res;
}

void iniciarServidores() {
  httpd_config_t configServidor = HTTPD_DEFAULT_CONFIG();
  configServidor.server_port = 80;
  httpd_uri_t rotaPagina = {};
  rotaPagina.uri = "/";
  rotaPagina.method = HTTP_GET;
  rotaPagina.handler = enviarPagina;
  rotaPagina.user_ctx = NULL;
  if (httpd_start(&servidorPagina, &configServidor) == ESP_OK) {
    httpd_register_uri_handler(servidorPagina, &rotaPagina);
  } else {
    Serial.println("Servidor da pagina: nao conseguiu iniciar.");
  }

  // O vídeo fica num servidor separado (porta 81), como no exemplo oficial
  configServidor.server_port = 81;
  configServidor.ctrl_port += 1;
  httpd_uri_t rotaVideo = {};
  rotaVideo.uri = "/stream";
  rotaVideo.method = HTTP_GET;
  rotaVideo.handler = enviarVideo;
  rotaVideo.user_ctx = NULL;
  if (httpd_start(&servidorVideo, &configServidor) == ESP_OK) {
    httpd_register_uri_handler(servidorVideo, &rotaVideo);
  } else {
    Serial.println("Servidor do video: nao conseguiu iniciar.");
  }
  servidoresIniciados = true;
}

// Avisa quando o Wi-Fi conecta ou cai; depois de 1 min sem conexão, força uma nova tentativa.
void acompanharWiFi() {
  bool conectado = (WiFi.status() == WL_CONNECTED);
  if (conectado && !wifiConectado) {
    if (!servidoresIniciados) iniciarServidores();
    Serial.printf("Wi-Fi conectado (sinal %d dBm). Video: http://", WiFi.RSSI());
    Serial.println(WiFi.localIP());
  } else if (!conectado && wifiConectado) {
    Serial.println("Wi-Fi caiu. O video e o envio voltam quando a rede voltar.");
  }
  wifiConectado = conectado;
  if (conectado) {
    ultimaVezComWiFi = millis();
  } else if (millis() - ultimaVezComWiFi >= ESPERA_RECONEXAO_MS) {
    Serial.println("Wi-Fi: 1 minuto sem conexao, tentando reconectar.");
    WiFi.reconnect();
    ultimaVezComWiFi = millis();
  }
}

// Lê a largura e a altura de dentro do próprio JPEG (no marcador SOF, que guarda o tamanho
// da imagem). Precisamos disso porque o driver da câmera preenche foto->width e foto->height
// com o tamanho CONFIGURADO na hora: um quadro antigo do vídeo (320x240) que ainda estava na
// fila sairia marcado como 640x480. Conferindo o JPEG, esse quadro velho não passa.
bool tamanhoDoJpeg(const uint8_t* buf, size_t len, int& largura, int& altura) {
  if (len < 4 || buf[0] != 0xFF || buf[1] != 0xD8) return false;   // não começa como JPEG
  size_t i = 2;
  while (i + 9 < len) {
    if (buf[i] != 0xFF) return false;                 // fora de um marcador: JPEG estranho
    uint8_t marcador = buf[i + 1];
    if (marcador == 0xFF) { i++; continue; }          // byte de preenchimento
    if (marcador == 0x01 || (marcador >= 0xD0 && marcador <= 0xD7)) { i += 2; continue; }   // marcadores sem tamanho
    size_t tamanhoSegmento = ((size_t)buf[i + 2] << 8) | buf[i + 3];
    bool ehSof = marcador >= 0xC0 && marcador <= 0xCF && marcador != 0xC4 && marcador != 0xC8 && marcador != 0xCC;
    if (ehSof) {
      altura = (buf[i + 5] << 8) | buf[i + 6];
      largura = (buf[i + 7] << 8) | buf[i + 8];
      return true;
    }
    if (marcador == 0xDA) return false;               // começou a imagem sem dizer o tamanho
    i += 2 + tamanhoSegmento;                         // pula para o próximo marcador
  }
  return false;
}

// Passa a câmera para 640x480 só durante a foto da API, joga fora os quadros antigos da fila
// e só aceita um quadro cujo JPEG seja mesmo 640x480. Quem chama devolve o quadro e volta o
// vídeo para 320x240. Só é chamada com a trava da câmera na mão (o vídeo está parado).
camera_fb_t* tirarFotoParaApi() {
  sensor_t* sensor = esp_camera_sensor_get();
  if (sensor == NULL) return NULL;
  if (sensor->set_framesize(sensor, RESOLUCAO_FOTO) != 0) {
    Serial.println("Camera: nao conseguiu mudar para 640x480.");
    return NULL;
  }
  delay(150);   // tempo para o sensor começar a mandar quadros no tamanho novo
  for (int i = 0; i < QUADROS_PARA_DESCARTAR; i++) {   // quadros que já estavam na fila: fora
    camera_fb_t* velho = esp_camera_fb_get();
    if (velho != NULL) esp_camera_fb_return(velho);
  }
  for (int tentativa = 0; tentativa < TENTATIVAS_FOTO; tentativa++) {
    camera_fb_t* foto = esp_camera_fb_get();
    if (foto == NULL) continue;
    int largura = 0, altura = 0;
    if (tamanhoDoJpeg(foto->buf, foto->len, largura, altura) && largura == LARGURA_FOTO && altura == ALTURA_FOTO) {
      return foto;                    // o JPEG é mesmo 640x480: é a foto nova
    }
    esp_camera_fb_return(foto);       // ainda é um quadro de outro tamanho: descarta
  }
  return NULL;
}

void voltarParaVideo() {
  sensor_t* sensor = esp_camera_sensor_get();
  if (sensor != NULL) sensor->set_framesize(sensor, RESOLUCAO_VIDEO);
}

// Manda a foto para a API: o corpo é o JPEG puro, o resto vai em cabeçalhos.
void enviarFoto(camera_fb_t* foto) {
  HTTPClient http;
  WiFiClientSecure clienteSeguro;   // usado só com https
  http.setConnectTimeout(ESPERA_HTTP_MS);
  http.setTimeout(ESPERA_HTTP_MS);
  bool comeco;
  if (strncmp(API_URL, "https", 5) == 0) {
    clienteSeguro.setInsecure();   // aceita o certificado do túnel sem conferir (só para a demonstração)
    comeco = http.begin(clienteSeguro, API_URL);
  } else {
    comeco = http.begin(API_URL);
  }
  if (!comeco) {
    Serial.println("Envio: endereco da API invalido. Confira o API_URL.");
    return;
  }
  http.addHeader("Content-Type", "image/jpeg");
  http.addHeader("X-Chave", CHAVE_CAMERA);   // a chave nunca aparece no serial
  http.addHeader("X-Poste", ID_POSTE);
  int codigo = http.POST(foto->buf, foto->len);
  String resposta = http.getString();
  http.end();

  if (codigo == 201 || codigo == 200) {
    Serial.printf("API respondeu %d: %s\n", codigo, resposta.substring(0, 300).c_str());
  } else if (codigo == 401) {
    Serial.println("API respondeu 401: chave errada. Confira a CHAVE_CAMERA.");
  } else if (codigo == 400 || codigo == 413 || codigo == 415) {
    Serial.printf("API respondeu %d: foto vazia, grande demais ou fora do JPEG.\n", codigo);
  } else if (codigo == 301 || codigo == 302 || codigo == 307 || codigo == 308) {
    Serial.printf("API respondeu %d: o endereco redirecionou. Troque http por https no API_URL.\n", codigo);
  } else if (codigo < 0) {
    Serial.printf("Envio: sem resposta da API (codigo %d). O tunel pode ter mudado de endereco.\n", codigo);
  } else {
    Serial.printf("API respondeu %d: %s\n", codigo, resposta.substring(0, 300).c_str());
  }
}

// A cada 20 s: foto em 640x480 para a API (se a chave e o Wi-Fi estiverem prontos)
void cicloDaApi() {
  if (strcmp(CHAVE_CAMERA, CHAVE_DE_EXEMPLO) == 0) return;   // sem chave: só o vídeo funciona
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Envio: sem Wi-Fi. Tenta de novo na proxima foto.");
    return;
  }
  numeroFoto++;
  fotoPedida = true;   // o vídeo para de pegar quadros
  if (xSemaphoreTake(travaCamera, pdMS_TO_TICKS(ESPERA_CAMERA_MS)) != pdTRUE) {
    fotoPedida = false;
    Serial.println("Camera: ocupada com o video. Tenta de novo em 20 s.");
    return;
  }
  camera_fb_t* foto = tirarFotoParaApi();
  voltarParaVideo();                 // a foto (se veio) já está guardada no quadro
  xSemaphoreGive(travaCamera);       // o vídeo volta enquanto a foto é enviada
  fotoPedida = false;
  if (foto == NULL) {
    Serial.println("Camera: nao conseguiu tirar a foto em 640x480. Tenta de novo em 20 s.");
    return;
  }
  Serial.printf("Foto %lu: %u bytes (%dx%d)\n", numeroFoto, (unsigned)foto->len, LARGURA_FOTO, ALTURA_FOTO);
  enviarFoto(foto);
  esp_camera_fb_return(foto);   // devolve o quadro para a câmera
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
  config.frame_size = RESOLUCAO_FOTO;   // reserva memória para a foto maior
  config.jpeg_quality = QUALIDADE;
  if (psramFound()) {
    config.fb_count = 2;                       // dois quadros na fila
    config.grab_mode = CAMERA_GRAB_LATEST;     // sempre o mais novo
    config.fb_location = CAMERA_FB_IN_PSRAM;
  } else {
    config.fb_count = 1;
    config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
    config.fb_location = CAMERA_FB_IN_DRAM;
  }
  esp_err_t erro = esp_camera_init(&config);
  if (erro != ESP_OK) {
    Serial.printf("Camera: falhou ao iniciar (erro 0x%x). Confira o cabo da camera.\n", erro);
    return false;
  }
  travaCamera = xSemaphoreCreateMutex();   // a trava que o vídeo e a foto dividem
  if (travaCamera == NULL) {
    Serial.println("Camera: sem memoria para a trava da camera.");
    return false;
  }
  sensor_t* sensor = esp_camera_sensor_get();
  if (sensor != NULL) {
    sensor->set_framesize(sensor, RESOLUCAO_VIDEO);   // o vídeo começa em 320x240
    sensor->set_quality(sensor, QUALIDADE);
    sensor->set_aec2(sensor, 0);   // sem o "modo noturno" da exposição, que deixa o vídeo lento no escuro
  }
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("C.E.L.I.A. - camera do poste (video + API)");
  pinMode(PINO_FLASH, OUTPUT);
  digitalWrite(PINO_FLASH, LOW);

  cameraOk = iniciarCamera();
  Serial.println(cameraOk ? "Camera: ok" : "Camera: sem camera.");
  if (strcmp(CHAVE_CAMERA, CHAVE_DE_EXEMPLO) == 0) {
    Serial.println("API: sem chave ainda. So o video vai funcionar.");
  }

  // Começa a conectar sem esperar; os servidores sobem quando o Wi-Fi conectar.
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);   // Wi-Fi sem economia de energia: vídeo e envios mais estáveis
  WiFi.begin(WIFI_NOME, WIFI_SENHA);
  ultimaVezComWiFi = millis();
  fimUltimoEnvio = millis() - INTERVALO_FOTO_MS + 8000;   // primeira foto uns 8 s depois de ligar
}

void loop() {
  acompanharWiFi();
  if (cameraOk && millis() - fimUltimoEnvio >= INTERVALO_FOTO_MS) {
    cicloDaApi();
    fimUltimoEnvio = millis();   // conta os 20 s a partir do fim do envio
  }
  delay(50);
}
