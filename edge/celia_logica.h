/*
  C.E.L.I.A. – regras de decisão do poste
  -----------------------------------------
  Este arquivo só tem "contas e decisões": não lê sensor nem acende LED.
  Por isso ele roda igual no ESP32 e no computador, e foi testado fora do
  Wokwi (tests/teste_logica.cpp) antes de ir para o circuito.

  Os limites vêm da Documentação do Módulo Edge e ainda estão em validação
  pela equipe: para mudar, troque só as constantes abaixo.
*/
#pragma once
#include <stdint.h>   // uint32_t: inteiro sem sinal de 32 bits, o mesmo tamanho do millis() no ESP32

// ---------- Limites (mudar aqui se a equipe decidir outros valores) ----------
const float CALCADA_LIVRE_CM    = 120.0;  // a partir de 120 cm: calçada livre (NBR 9050: faixa livre de 1,20 m)
const float CALCADA_ESTREITA_CM = 90.0;   // de 90 a 119 cm: estreita; abaixo de 90 cm: bloqueada
const float CALOR_ATENCAO_C     = 32.0;   // índice de calor a partir de 32 °C: atenção
const float CALOR_PERIGO_C      = 41.0;   // a partir de 41 °C: perigo (a equipe pode trocar para 39)

// ---------- Situação da calçada ----------
const int CALCADA_LIVRE     = 0;
const int CALCADA_ESTREITA  = 1;
const int CALCADA_BLOQUEADA = 2;

// ---------- Situação do calor ----------
const int CALOR_NORMAL  = 0;
const int CALOR_ATENCAO = 1;
const int CALOR_PERIGO  = 2;

// ---------- Status do trecho (é o número que vai para o field5 do ThingSpeak) ----------
const int STATUS_ACESSIVEL = 0;  // verde
const int STATUS_ATENCAO   = 1;  // amarelo
const int STATUS_EVITE     = 2;  // vermelho

// ---------- Motivo do status (field6 do ThingSpeak) ----------
const int MOTIVO_NENHUM  = 0;
const int MOTIVO_CALCADA = 1;
const int MOTIVO_CALOR   = 2;
const int MOTIVO_AMBOS   = 3;
const int MOTIVO_BURACO  = 5;  // buraco visto pela câmera (Etapa 4)
// O 4 (veículo) foi retirado pela equipe em 12/11: o modelo marcava carro estacionado.

// ---------- HC-SR04: tempo do eco -> distância ----------
// O som vai até o obstáculo e volta; o tempo do eco (ida e volta) é proporcional à
// distância. Medimos no Wokwi 28 ecos de 50 a 400 cm (edge/evidencias/calibracao_hc_sr04.md):
// em média 58,8 µs por centímetro (som a ~340 m/s), com variação de até ±3 µs.
// A regra da calçada vale para centímetros INTEIROS e arredonda PARA BAIXO: só conta
// 120 cm quando a medida chega a 120 cm completos. Na dúvida, o trecho fica na faixa pior.
// A folga de 0,08 cm (0,8 mm) cobre a maior falta medida no simulador (0,054 cm, ~3 µs)
// com um pouco de sobra: sem ela, 119 cm no Wokwi às vezes virava 118.
const float US_POR_CM = 58.8;
const float FOLGA_ECO_CM = 0.08;

inline float distanciaPeloEco(uint32_t duracaoUs) {
  float cm = duracaoUs / US_POR_CM;
  return (float)(long)(cm + FOLGA_ECO_CM);   // (long) corta a parte decimal = arredonda para baixo
}

// Classifica a calçada pela distância livre medida pelo HC-SR04.
inline int classificarCalcada(float distanciaCm) {
  if (distanciaCm >= CALCADA_LIVRE_CM) return CALCADA_LIVRE;
  if (distanciaCm >= CALCADA_ESTREITA_CM) return CALCADA_ESTREITA;
  return CALCADA_BLOQUEADA;
}

// Classifica o calor pelo índice de calor (sensação térmica) em °C.
inline int classificarCalor(float indiceCalorC) {
  if (indiceCalorC >= CALOR_PERIGO_C) return CALOR_PERIGO;
  if (indiceCalorC >= CALOR_ATENCAO_C) return CALOR_ATENCAO;
  return CALOR_NORMAL;
}

// Junta calçada e calor no status final (tabela "Status final do trecho" da documentação).
inline int calcularStatus(int calcada, int calor) {
  if (calcada == CALCADA_BLOQUEADA) return STATUS_EVITE;           // bloqueada + qualquer calor
  if (calcada == CALCADA_ESTREITA) {
    if (calor == CALOR_PERIGO) return STATUS_EVITE;                // estreita + perigo
    return STATUS_ATENCAO;                                         // estreita + normal ou atenção
  }
  // calçada livre
  if (calor == CALOR_NORMAL) return STATUS_ACESSIVEL;              // livre + normal
  return STATUS_ATENCAO;                                           // livre + atenção ou perigo
}

// Diz o que deixou o trecho pior que "acessível": a calçada, o calor ou os dois.
inline int calcularMotivo(int calcada, int calor) {
  bool culpaCalcada = (calcada != CALCADA_LIVRE);
  bool culpaCalor = (calor != CALOR_NORMAL);
  if (culpaCalcada && culpaCalor) return MOTIVO_AMBOS;
  if (culpaCalcada) return MOTIVO_CALCADA;
  if (culpaCalor) return MOTIVO_CALOR;
  return MOTIVO_NENHUM;
}

// ---------- Confirmação da calçada ----------
// Uma mudança na calçada só vale depois de aparecer em leituras seguidas por um
// tempo mínimo (1 minuto na documentação). Assim, uma pessoa passando na frente
// do sensor não deixa o trecho vermelho, e um carro parado deixa.
struct Confirmador {
  bool iniciado = false;            // a primeira leitura vale na hora
  int confirmado = CALCADA_LIVRE;   // situação que está valendo
  int candidato = CALCADA_LIVRE;    // situação nova que está aparecendo
  uint32_t desde = 0;               // quando o candidato apareceu (ms)
};

// Recebe a leitura atual e devolve a situação que deve valer.
inline int atualizarConfirmacao(Confirmador &c, int leitura, uint32_t agoraMs, uint32_t tempoConfirmacaoMs) {
  if (!c.iniciado) {
    c.iniciado = true;
    c.confirmado = leitura;
    c.candidato = leitura;
    c.desde = agoraMs;
    return c.confirmado;
  }
  if (leitura == c.confirmado) {
    c.candidato = leitura;          // voltou ao normal antes do tempo: cancela a mudança
  } else if (leitura != c.candidato) {
    c.candidato = leitura;          // apareceu uma situação nova: começa a contar
    c.desde = agoraMs;
  } else if (agoraMs - c.desde >= tempoConfirmacaoMs) {
    c.confirmado = leitura;         // ficou igual pelo tempo todo: confirma
  }
  return c.confirmado;
}

// Se o sensor falhou nesta leitura, a mudança em andamento é cancelada: a contagem
// só recomeça na próxima leitura válida. Assim, um eco solto seguido de falhas
// nunca confirma uma mudança (por exemplo, liberar um trecho bloqueado).
inline void cancelarConfirmacao(Confirmador &c) {
  c.candidato = c.confirmado;
}

// Quanto falta (ms) para confirmar a mudança em andamento; 0 se não há mudança.
inline uint32_t msParaConfirmar(const Confirmador &c, uint32_t agoraMs, uint32_t tempoConfirmacaoMs) {
  if (c.candidato == c.confirmado) return 0;
  uint32_t passou = agoraMs - c.desde;   // continua certo quando millis() volta a zero
  if (passou >= tempoConfirmacaoMs) return 0;
  return tempoConfirmacaoMs - passou;
}

// ---------- Câmera (Etapa 4) ----------
// A foto de um usuário vai para a API de visão, que avisa por MQTT "livre" ou "buraco".
// Um "buraco" vale por um tempo (10 min na documentação, ainda em proposta). Enquanto
// vale, o trecho fica em EVITE com motivo 5, qualquer que seja a calçada e o calor.
// Um "livre" não muda nada: vale a regra dos sensores (e não apaga um buraco que ainda vale).

// O aviso de buraco ainda vale? "desdeMs" é quando ele chegou.
// Continua certo quando millis() volta a zero, como no confirmador.
inline bool buracoValendo(bool recebeuBuraco, uint32_t desdeMs, uint32_t agoraMs, uint32_t validadeMs) {
  return recebeuBuraco && (agoraMs - desdeMs < validadeMs);
}

// Status e motivo finais, juntando os sensores com a câmera.
inline int statusComCamera(int statusSensores, bool buraco) {
  return buraco ? STATUS_EVITE : statusSensores;
}
inline int motivoComCamera(int motivoSensores, bool buraco) {
  return buraco ? MOTIVO_BURACO : motivoSensores;
}

// ---------- Textos para o OLED e o Serial (sem acento: a fonte do OLED não tem) ----------
inline const char* nomeStatus(int status) {
  if (status == STATUS_ACESSIVEL) return "ACESSIVEL";
  if (status == STATUS_ATENCAO) return "ATENCAO";
  return "EVITE";
}

inline const char* nomeMotivo(int motivo) {
  if (motivo == MOTIVO_CALCADA) return "calcada";
  if (motivo == MOTIVO_CALOR) return "calor";
  if (motivo == MOTIVO_AMBOS) return "calcada+calor";
  if (motivo == MOTIVO_BURACO) return "buraco";
  return "nenhum";
}

inline const char* nomeCalcada(int calcada) {
  if (calcada == CALCADA_LIVRE) return "livre";
  if (calcada == CALCADA_ESTREITA) return "estreita";
  return "bloqueada";
}

inline const char* nomeCalor(int calor) {
  if (calor == CALOR_NORMAL) return "normal";
  if (calor == CALOR_ATENCAO) return "atencao";
  return "perigo";
}
