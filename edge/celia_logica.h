/*
  C.E.L.I.A. – regras de decisão do poste (luz + calor)
  -----------------------------------------------------
  Este arquivo só tem "contas e decisões": não lê sensor nem acende LED.
  Por isso ele roda igual no ESP32 e no computador, e foi testado fora do
  Wokwi (tests/teste_logica.cpp) antes de ir para o circuito.

  Mudança de 18/11/2026: o HC-SR04 (distância na calçada) saiu e entrou o LDR
  (luminosidade). O calor (DHT22) e a câmera continuam iguais.

  Os limites ainda estão em validação pela equipe: para mudar, troque só as
  constantes abaixo.
*/
#pragma once
#include <stdint.h>   // uint32_t: inteiro sem sinal de 32 bits, o mesmo tamanho do millis() no ESP32
#include <math.h>     // pow()

// ---------- Limites (mudar aqui se a equipe decidir outros valores) ----------
const float CALOR_ATENCAO_C = 32.0;      // índice de calor a partir de 32 °C: atenção
const float CALOR_PERIGO_C  = 41.0;      // a partir de 41 °C: perigo
const long  LUZ_ESCURO_LUX  = 10;        // abaixo de 10 lux: rua escura (não dá para ver buraco, degrau, desnível)
const long  LUZ_SOL_LUX     = 25000;     // a partir de 25.000 lux: sol direto (calçada sem sombra)
const long  LUZ_MAXIMA_LUX  = 100000;    // teto: o módulo do Wokwi vai até 100.000 lux (sol forte)

// ---------- LDR: leitura do ADC -> lux ----------
// O módulo de luz tem o LDR em série com um resistor de 10 kΩ; o pino AO fica entre os
// dois. No escuro a resistência do LDR sobe e a tensão no AO também (ADC alto); no claro,
// a tensão cai (ADC baixo). O ESP32 lê de 0 a 4095 para 0 a 3,3 V.
// A conversão é a da documentação do Wokwi (wokwi-photoresistor-sensor), trocando 5 V
// por 3,3 V: rl10 = 50 kΩ (resistência com 10 lux) e gamma = 0,7.
const float LDR_VCC           = 3.3;
const float LDR_ADC_MAX       = 4095.0;
const float LDR_RESISTOR_FIXO = 10000.0;  // o resistor de 10 kΩ do módulo
const float LDR_RL10_KOHM     = 50.0;     // atributo rl10 do Wokwi
const float LDR_GAMMA         = 0.7;      // atributo gamma do Wokwi

// Lux inteiros (arredonda PARA BAIXO). O número que vai para o ThingSpeak é o mesmo
// que decide o status: o que aparece no gráfico é exatamente o que o poste usou.
inline long luxPeloAdc(int adc) {
  if (adc <= 0) return LUZ_MAXIMA_LUX;            // tensão zero: claro demais para medir
  if (adc >= (int)LDR_ADC_MAX) return 0;          // tensão máxima: escuro total
  float tensao = adc / LDR_ADC_MAX * LDR_VCC;
  float resistencia = LDR_RESISTOR_FIXO * tensao / (LDR_VCC - tensao);
  float lux = pow(LDR_RL10_KOHM * 1000.0 * pow(10.0, LDR_GAMMA) / resistencia, 1.0 / LDR_GAMMA);
  if (lux >= LUZ_MAXIMA_LUX) return LUZ_MAXIMA_LUX;
  return (long)lux;                                // (long) corta a parte decimal
}

// ---------- Situação da luz ----------
const int LUZ_NORMAL = 0;
const int LUZ_ESCURO = 1;
const int LUZ_SOL    = 2;

// ---------- Situação do calor ----------
const int CALOR_NORMAL  = 0;
const int CALOR_ATENCAO = 1;
const int CALOR_PERIGO  = 2;

// ---------- Status do trecho (é o número que vai para o field5 do ThingSpeak) ----------
const int STATUS_ACESSIVEL = 0;  // verde
const int STATUS_ATENCAO   = 1;  // amarelo
const int STATUS_EVITE     = 2;  // vermelho

// ---------- Motivo do status (field6 do ThingSpeak) ----------
// Desde 18/11, o motivo 1 é a LUZ (antes, com o HC-SR04, era a calçada).
const int MOTIVO_NENHUM = 0;
const int MOTIVO_LUZ    = 1;
const int MOTIVO_CALOR  = 2;
const int MOTIVO_AMBOS  = 3;
const int MOTIVO_BURACO = 5;  // buraco visto pela câmera

// Classifica a luz pelos lux medidos pelo LDR.
inline int classificarLuz(long lux) {
  if (lux < LUZ_ESCURO_LUX) return LUZ_ESCURO;
  if (lux >= LUZ_SOL_LUX) return LUZ_SOL;
  return LUZ_NORMAL;
}

// Classifica o calor pelo índice de calor (sensação térmica) em °C.
inline int classificarCalor(float indiceCalorC) {
  if (indiceCalorC >= CALOR_PERIGO_C) return CALOR_PERIGO;
  if (indiceCalorC >= CALOR_ATENCAO_C) return CALOR_ATENCAO;
  return CALOR_NORMAL;
}

// Junta luz e calor no status final:
//
//                 | calor normal | calor atenção | calor perigo
//   escuro        | ATENÇÃO (1)  | ATENÇÃO (3)   | ATENÇÃO (3)
//   luz normal    | ACESSÍVEL (0)| ATENÇÃO (2)   | ATENÇÃO (2)
//   sol direto    | ACESSÍVEL (0)| EVITE (3)     | EVITE (3)
//
// (entre parênteses, o motivo). O sol sozinho não piora nada; com calor, piora:
// sol direto + calor = EVITE. O calor sozinho fica em ATENÇÃO.
inline int calcularStatus(int luz, int calor) {
  if (calor == CALOR_NORMAL) {
    return luz == LUZ_ESCURO ? STATUS_ATENCAO : STATUS_ACESSIVEL;
  }
  return luz == LUZ_SOL ? STATUS_EVITE : STATUS_ATENCAO;
}

// Diz o que deixou o trecho pior que "acessível": a luz, o calor ou os dois.
// A luz só tem culpa quando ela piorou o status: escuro sempre; sol só junto com calor.
inline int calcularMotivo(int luz, int calor) {
  bool culpaCalor = (calor != CALOR_NORMAL);
  bool culpaLuz = (luz == LUZ_ESCURO) || (luz == LUZ_SOL && culpaCalor);
  if (culpaLuz && culpaCalor) return MOTIVO_AMBOS;
  if (culpaLuz) return MOTIVO_LUZ;
  if (culpaCalor) return MOTIVO_CALOR;
  return MOTIVO_NENHUM;
}

// ---------- Confirmação da luz ----------
// Uma mudança na luz só vale depois de aparecer em leituras seguidas por um tempo
// mínimo (1 minuto; 15 s no modo demonstração). Assim, a sombra de um ônibus ou o
// farol de um carro passando não mudam o trecho, e um poste de luz apagado muda.
// (Antes de 18/11, o mesmo confirmador servia para a calçada.)
struct Confirmador {
  bool iniciado = false;          // a primeira leitura vale na hora
  int confirmado = LUZ_NORMAL;    // situação que está valendo
  int candidato = LUZ_NORMAL;     // situação nova que está aparecendo
  uint32_t desde = 0;             // quando o candidato apareceu (ms)
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

// Quanto falta (ms) para confirmar a mudança em andamento; 0 se não há mudança.
inline uint32_t msParaConfirmar(const Confirmador &c, uint32_t agoraMs, uint32_t tempoConfirmacaoMs) {
  if (c.candidato == c.confirmado) return 0;
  uint32_t passou = agoraMs - c.desde;   // continua certo quando millis() volta a zero
  if (passou >= tempoConfirmacaoMs) return 0;
  return tempoConfirmacaoMs - passou;
}

// ---------- Câmera ----------
// A câmera do poste (ESP32-CAM, pasta camera/) manda uma foto a cada 20 s para a API de
// visão, que avisa por MQTT "livre" ou "buraco". Um "buraco" vale por um tempo (10 min;
// 1 min no modo demonstração). Enquanto vale, o trecho fica em EVITE com motivo 5,
// qualquer que seja a luz e o calor. Um "livre" não muda nada.

// O aviso de buraco ainda vale? "desdeMs" é quando ele chegou.
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

// ---------- Textos para o monitor serial (sem acento) ----------
inline const char* nomeStatus(int status) {
  if (status == STATUS_ACESSIVEL) return "ACESSIVEL";
  if (status == STATUS_ATENCAO) return "ATENCAO";
  return "EVITE";
}

inline const char* nomeMotivo(int motivo) {
  if (motivo == MOTIVO_LUZ) return "luz";
  if (motivo == MOTIVO_CALOR) return "calor";
  if (motivo == MOTIVO_AMBOS) return "luz+calor";
  if (motivo == MOTIVO_BURACO) return "buraco";
  return "nenhum";
}

inline const char* nomeLuz(int luz) {
  if (luz == LUZ_ESCURO) return "escuro";
  if (luz == LUZ_SOL) return "sol direto";
  return "normal";
}

inline const char* nomeCalor(int calor) {
  if (calor == CALOR_NORMAL) return "normal";
  if (calor == CALOR_ATENCAO) return "atencao";
  return "perigo";
}
