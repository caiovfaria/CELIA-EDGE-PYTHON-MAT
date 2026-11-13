/*
  Testes das regras de decisão do poste (edge/celia_logica.h).
  Roda no computador, sem Wokwi:
      g++ -std=c++17 -Wall -o teste_logica tests/teste_logica.cpp && ./teste_logica
*/
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include "../edge/celia_logica.h"

int passou = 0, falhou = 0;

void confere(bool ok, const char* descricao) {
  if (ok) { passou++; }
  else { falhou++; printf("FALHOU: %s\n", descricao); }
}

// Cópia da fórmula de índice de calor da biblioteca DHTesp (computeHeatIndex),
// usada só para conferir os valores do roteiro de demonstração.
float indiceCalorDHTesp(float tempC, float umidade) {
  float t = tempC * 1.8f + 32;
  float hi = 0.5f * (t + 61.0f + ((t - 68.0f) * 1.2f) + (umidade * 0.094f));
  if (hi > 79) {
    hi = -42.379f + 2.04901523f * t + 10.14333127f * umidade - 0.22475541f * t * umidade
         - 0.00683783f * t * t - 0.05481717f * umidade * umidade
         + 0.00122874f * t * t * umidade + 0.00085282f * t * umidade * umidade
         - 0.00000199f * t * t * umidade * umidade;
    if (umidade < 13 && t >= 80 && t <= 112)
      hi -= ((13 - umidade) * 0.25f) * std::sqrt((17 - std::fabs(t - 95)) * 0.05882f);
    else if (umidade > 85 && t >= 80 && t <= 87)
      hi += ((umidade - 85) * 0.1f) * ((87 - t) * 0.2f);
  }
  return (hi - 32) / 1.8f;
}

int main() {
  // 1. Faixas da calçada, incluindo as bordas
  confere(classificarCalcada(400) == CALCADA_LIVRE, "400 cm = livre");
  confere(classificarCalcada(120) == CALCADA_LIVRE, "120 cm = livre (borda)");
  confere(classificarCalcada(119.9f) == CALCADA_ESTREITA, "119,9 cm = estreita");
  confere(classificarCalcada(90) == CALCADA_ESTREITA, "90 cm = estreita (borda)");
  confere(classificarCalcada(89.9f) == CALCADA_BLOQUEADA, "89,9 cm = bloqueada");
  confere(classificarCalcada(2) == CALCADA_BLOQUEADA, "2 cm = bloqueada");

  // 2. Faixas do calor, incluindo as bordas e o antigo intervalo 40–41 °C
  confere(classificarCalor(25) == CALOR_NORMAL, "25 °C = normal");
  confere(classificarCalor(31.9f) == CALOR_NORMAL, "31,9 °C = normal");
  confere(classificarCalor(32) == CALOR_ATENCAO, "32 °C = atenção (borda)");
  confere(classificarCalor(40.5f) == CALOR_ATENCAO, "40,5 °C = atenção (sem buraco entre 40 e 41)");
  confere(classificarCalor(40.99f) == CALOR_ATENCAO, "40,99 °C = atenção");
  confere(classificarCalor(41) == CALOR_PERIGO, "41 °C = perigo (borda)");
  confere(classificarCalor(55) == CALOR_PERIGO, "55 °C = perigo");

  // 3. Tabela "Status final do trecho" da documentação (as 9 combinações)
  const int esperado[3][3] = {
    // calor:     normal            atenção           perigo
    /* livre */  {STATUS_ACESSIVEL, STATUS_ATENCAO,   STATUS_ATENCAO},
    /* estreita*/{STATUS_ATENCAO,   STATUS_ATENCAO,   STATUS_EVITE},
    /* bloq.  */ {STATUS_EVITE,     STATUS_EVITE,     STATUS_EVITE},
  };
  for (int c = 0; c < 3; c++)
    for (int k = 0; k < 3; k++) {
      char d[80];
      snprintf(d, sizeof d, "status calçada %s + calor %s", nomeCalcada(c), nomeCalor(k));
      confere(calcularStatus(c, k) == esperado[c][k], d);
    }

  // 4. Motivo
  confere(calcularMotivo(CALCADA_LIVRE, CALOR_NORMAL) == MOTIVO_NENHUM, "motivo: nenhum");
  confere(calcularMotivo(CALCADA_BLOQUEADA, CALOR_NORMAL) == MOTIVO_CALCADA, "motivo: calçada");
  confere(calcularMotivo(CALCADA_LIVRE, CALOR_PERIGO) == MOTIVO_CALOR, "motivo: calor");
  confere(calcularMotivo(CALCADA_ESTREITA, CALOR_ATENCAO) == MOTIVO_AMBOS, "motivo: calçada e calor");
  for (int c = 0; c < 3; c++)
    for (int k = 0; k < 3; k++) {
      bool acessivel = calcularStatus(c, k) == STATUS_ACESSIVEL;
      bool semMotivo = calcularMotivo(c, k) == MOTIVO_NENHUM;
      confere(acessivel == semMotivo, "motivo 'nenhum' só quando o trecho é acessível");
    }

  // 5. Confirmação de 1 minuto (leituras a cada 5 s)
  {
    const uint32_t T = 60000;
    Confirmador c;
    confere(atualizarConfirmacao(c, CALCADA_LIVRE, 0, T) == CALCADA_LIVRE, "primeira leitura vale na hora");
    // pessoa passando: bloqueada numa leitura e livre na seguinte
    confere(atualizarConfirmacao(c, CALCADA_BLOQUEADA, 5000, T) == CALCADA_LIVRE, "pessoa passando não muda (5 s)");
    confere(atualizarConfirmacao(c, CALCADA_LIVRE, 10000, T) == CALCADA_LIVRE, "pessoa passou: continua livre");
    confere(msParaConfirmar(c, 10000, T) == 0, "sem mudança pendente depois que a pessoa passou");
    // carro estaciona em t = 15 s
    uint32_t t = 15000;
    confere(atualizarConfirmacao(c, CALCADA_BLOQUEADA, t, T) == CALCADA_LIVRE, "carro: ainda livre no início");
    confere(msParaConfirmar(c, t, T) == 60000, "faltam 60 s");
    for (t = 20000; t < 75000; t += 5000)
      confere(atualizarConfirmacao(c, CALCADA_BLOQUEADA, t, T) == CALCADA_LIVRE, "carro: ainda livre antes de 1 min");
    confere(msParaConfirmar(c, 70000, T) == 5000, "faltam 5 s em t = 70 s");
    confere(atualizarConfirmacao(c, CALCADA_BLOQUEADA, 75000, T) == CALCADA_BLOQUEADA, "carro: bloqueada após 1 min");
    // carro sai: precisa de 1 min livre para liberar
    confere(atualizarConfirmacao(c, CALCADA_LIVRE, 80000, T) == CALCADA_BLOQUEADA, "carro saiu: ainda bloqueada");
    confere(atualizarConfirmacao(c, CALCADA_LIVRE, 135000, T) == CALCADA_BLOQUEADA, "ainda bloqueada aos 55 s");
    confere(atualizarConfirmacao(c, CALCADA_LIVRE, 140000, T) == CALCADA_LIVRE, "liberada após 1 min livre");
  }
  {
    // Modo demonstração (15 s) e troca de candidato no meio da contagem
    const uint32_t T = 15000;
    Confirmador c;
    atualizarConfirmacao(c, CALCADA_LIVRE, 0, T);
    atualizarConfirmacao(c, CALCADA_ESTREITA, 5000, T);
    atualizarConfirmacao(c, CALCADA_BLOQUEADA, 10000, T);  // mudou de novo: recomeça a contar
    confere(atualizarConfirmacao(c, CALCADA_BLOQUEADA, 20000, T) == CALCADA_LIVRE, "demo: recomeçou a contar em t = 10 s");
    confere(atualizarConfirmacao(c, CALCADA_BLOQUEADA, 25000, T) == CALCADA_BLOQUEADA, "demo: confirma 15 s depois");
  }
  {
    // O relógio do ESP32 (millis) volta a zero depois de ~49 dias: a conta continua certa.
    // Os tempos são uint32_t (32 bits, como no ESP32), então a soma abaixo passa do
    // limite e volta a contar do zero de verdade, também no computador.
    const uint32_t T = 60000;
    Confirmador c;
    const uint32_t quaseFim = 0xFFFFFFFFu - 20000;
    atualizarConfirmacao(c, CALCADA_LIVRE, quaseFim, T);
    const uint32_t inicio = quaseFim + 5000;               // ainda antes do limite
    atualizarConfirmacao(c, CALCADA_BLOQUEADA, inicio, T);
    const uint32_t meio = inicio + 30000;                  // já passou do limite
    confere(meio < inicio, "o relógio realmente voltou a zero no teste");
    confere(msParaConfirmar(c, meio, T) == 30000, "tempo restante certo depois de voltar a zero");
    confere(atualizarConfirmacao(c, CALCADA_BLOQUEADA, meio, T) == CALCADA_LIVRE, "ainda não confirmou na metade");
    confere(atualizarConfirmacao(c, CALCADA_BLOQUEADA, inicio + 59999, T) == CALCADA_LIVRE, "1 ms antes do tempo: não confirma");
    confere(atualizarConfirmacao(c, CALCADA_BLOQUEADA, inicio + 60000, T) == CALCADA_BLOQUEADA, "no instante exato: confirma (depois do zero)");
  }
  {
    // Sensor sem eco: a mudança em andamento é cancelada e recomeça na próxima leitura válida
    const uint32_t T = 15000;
    Confirmador c;
    atualizarConfirmacao(c, CALCADA_LIVRE, 0, T);
    for (uint32_t t = 5000; t <= 20000; t += 5000) atualizarConfirmacao(c, CALCADA_BLOQUEADA, t, T);
    confere(c.confirmado == CALCADA_BLOQUEADA, "falha: trecho começa bloqueado");
    // um eco solto de calçada livre, seguido de falhas por 30 s
    atualizarConfirmacao(c, CALCADA_LIVRE, 25000, T);
    confere(msParaConfirmar(c, 25000, T) == 15000, "falha: eco livre começa a contar");
    for (int i = 0; i < 6; i++) cancelarConfirmacao(c);   // 6 leituras sem eco (30 s)
    confere(c.confirmado == CALCADA_BLOQUEADA, "falha: eco solto + falhas não liberam o trecho");
    confere(msParaConfirmar(c, 55000, T) == 0, "falha: nenhuma mudança pendente depois das falhas");
    // o eco volta: a contagem recomeça do zero
    confere(atualizarConfirmacao(c, CALCADA_LIVRE, 60000, T) == CALCADA_BLOQUEADA, "falha: eco voltou, recomeça a contar");
    confere(atualizarConfirmacao(c, CALCADA_LIVRE, 70000, T) == CALCADA_BLOQUEADA, "falha: ainda não liberou 10 s depois");
    confere(atualizarConfirmacao(c, CALCADA_LIVRE, 75000, T) == CALCADA_LIVRE, "falha: libera 15 s depois do eco voltar");
  }

  // Conversão do eco do HC-SR04 (58,8 µs por cm, medido no Wokwi; cm inteiros, para baixo, folga de 0,08 cm)
  {
    // os 28 ecos medidos no Wokwi (edge/evidencias/calibracao_hc_sr04.md) dão a distância escolhida
    struct Eco { int cm; uint32_t us; };
    const Eco medidos[] = {
      {50, 2941}, {50, 2939}, {50, 2941}, {50, 2939},
      {89, 5231}, {89, 5236}, {89, 5231}, {89, 5236},
      {90, 5291}, {90, 5293}, {90, 5291},
      {119, 6994}, {119, 6996}, {119, 6997}, {119, 6996}, {119, 6997}, {119, 6996},
      {120, 7058}, {120, 7059}, {120, 7058}, {120, 7056},
      {200, 11763}, {200, 11761}, {200, 11763},
      {400, 23525}, {400, 23523}, {400, 23525}, {400, 23523},
    };
    int erros = 0;
    for (const Eco& e : medidos) if ((int)distanciaPeloEco(e.us) != e.cm) erros++;
    confere(erros == 0, "os 28 ecos medidos no Wokwi dão a distância escolhida no simulador");
  }
  // pulsos consecutivos na borda dos 120 cm (120 cm = 7056 µs)
  confere(distanciaPeloEco(7050) == 119, "7050 µs (119,898 cm) = 119");
  confere(distanciaPeloEco(7051) == 119, "7051 µs (119,915 cm) = 119");
  confere(distanciaPeloEco(7052) == 120, "7052 µs (119,932 cm, dentro da folga de 0,08) = 120");
  confere(distanciaPeloEco(7056) == 120, "7056 µs (120,000 cm) = 120");
  confere(classificarCalcada(distanciaPeloEco(7051)) == CALCADA_ESTREITA, "7051 µs = estreita");
  confere(classificarCalcada(distanciaPeloEco(7052)) == CALCADA_LIVRE, "7052 µs = livre");
  confere(classificarCalcada(distanciaPeloEco(7032)) == CALCADA_ESTREITA, "119,6 cm (7032 µs) = estreita: só 120 completos é livre");
  // pulsos consecutivos na borda dos 90 cm (90 cm = 5292 µs)
  confere(distanciaPeloEco(5286) == 89, "5286 µs (89,898 cm) = 89");
  confere(distanciaPeloEco(5287) == 89, "5287 µs (89,915 cm) = 89");
  confere(distanciaPeloEco(5288) == 90, "5288 µs (89,932 cm, dentro da folga de 0,08) = 90");
  confere(distanciaPeloEco(5292) == 90, "5292 µs (90,000 cm) = 90");
  confere(classificarCalcada(distanciaPeloEco(5287)) == CALCADA_BLOQUEADA, "5287 µs = bloqueada");
  confere(classificarCalcada(distanciaPeloEco(5288)) == CALCADA_ESTREITA, "5288 µs = estreita");
  confere(classificarCalcada(distanciaPeloEco(5268)) == CALCADA_BLOQUEADA, "89,6 cm (5268 µs) = bloqueada: só 90 completos é estreita");
  // todos os centímetros do simulador (2 a 400 cm), com o eco até 3 µs abaixo ou acima do esperado
  {
    int erros = 0;
    for (int cm = 2; cm <= 400; cm++)
      for (int d = -3; d <= 3; d++) {
        uint32_t us = (uint32_t)(cm * 58.8 + 0.5) + d;
        if ((int)distanciaPeloEco(us) != cm) erros++;
      }
    confere(erros == 0, "2 a 400 cm com o eco variando ±3 µs: sempre o cm exato");
  }

  // 6. Valores do roteiro de demonstração (umidade fixa em 50%)
  struct Caso { float temp, dist; int calcada, calor, status, motivo; const char* nome; };
  const Caso roteiro[] = {
    {26, 200, CALCADA_LIVRE,     CALOR_NORMAL,  STATUS_ACESSIVEL, MOTIVO_NENHUM,  "passo 1: livre + normal"},
    {32, 200, CALCADA_LIVRE,     CALOR_ATENCAO, STATUS_ATENCAO,   MOTIVO_CALOR,   "passo 2: livre + atenção"},
    {38, 200, CALCADA_LIVRE,     CALOR_PERIGO,  STATUS_ATENCAO,   MOTIVO_CALOR,   "passo 3: livre + perigo"},
    {26, 100, CALCADA_ESTREITA,  CALOR_NORMAL,  STATUS_ATENCAO,   MOTIVO_CALCADA, "passo 4: estreita + normal"},
    {38, 100, CALCADA_ESTREITA,  CALOR_PERIGO,  STATUS_EVITE,     MOTIVO_AMBOS,   "passo 5: estreita + perigo"},
    {26,  50, CALCADA_BLOQUEADA, CALOR_NORMAL,  STATUS_EVITE,     MOTIVO_CALCADA, "passo 6: bloqueada + normal"},
  };
  for (const Caso& r : roteiro) {
    float ic = indiceCalorDHTesp(r.temp, 50);
    int calcada = classificarCalcada(r.dist), calor = classificarCalor(ic);
    char d[120];
    snprintf(d, sizeof d, "%s (índice de calor %.1f °C)", r.nome, ic);
    confere(calcada == r.calcada && calor == r.calor &&
            calcularStatus(calcada, calor) == r.status && calcularMotivo(calcada, calor) == r.motivo, d);
    printf("  %-28s  T %2.0f °C  U 50%%  IC %4.1f °C  faixa %3.0f cm  ->  %s, motivo %s\n",
           r.nome, r.temp, ic, r.dist, nomeStatus(calcularStatus(calcada, calor)), nomeMotivo(calcularMotivo(calcada, calor)));
  }
  // os valores de perigo do roteiro continuam em perigo se a equipe trocar o limite para 39 °C
  confere(indiceCalorDHTesp(38, 50) >= 41 && indiceCalorDHTesp(32, 50) < 39, "roteiro funciona com limite de 39 ou 41 °C");

  // 6b. Câmera (Etapa 4): buraco deixa em EVITE com motivo 5 enquanto vale; depois, voltam os sensores
  {
    const uint32_t VALIDADE = 600000;  // 10 min
    confere(!buracoValendo(false, 0, 1000, VALIDADE), "sem aviso de buraco: nao vale");
    confere(buracoValendo(true, 5000, 5000, VALIDADE), "buraco acabou de chegar: vale");
    confere(buracoValendo(true, 5000, 5000 + VALIDADE - 1, VALIDADE), "buraco 1 ms antes de vencer: ainda vale");
    confere(!buracoValendo(true, 5000, 5000 + VALIDADE, VALIDADE), "buraco com 10 min completos: venceu");
    uint32_t perto = 0xFFFFFFFFu - 100000;  // 100 s antes de o millis() voltar a zero
    uint32_t depois = perto + 300000;        // 5 min depois (já passou do zero)
    confere(depois < perto, "teste do zero: o relogio voltou a zero de verdade");
    confere(buracoValendo(true, perto, depois, VALIDADE), "buraco continua valendo quando o millis() volta a zero");
    confere(!buracoValendo(true, perto, perto + VALIDADE, VALIDADE), "buraco vence na hora certa mesmo passando pelo zero");
    bool todosOk = true;
    for (int calcada = 0; calcada <= 2; calcada++)
      for (int calor = 0; calor <= 2; calor++) {
        int s = calcularStatus(calcada, calor), m = calcularMotivo(calcada, calor);
        if (statusComCamera(s, true) != STATUS_EVITE || motivoComCamera(m, true) != MOTIVO_BURACO) todosOk = false;
        if (statusComCamera(s, false) != s || motivoComCamera(m, false) != m) todosOk = false;
      }
    confere(todosOk, "nas 9 combinacoes: com buraco, EVITE e motivo 5; sem buraco, fica a regra dos sensores");
    confere(MOTIVO_BURACO == 5 && strcmp(nomeMotivo(MOTIVO_BURACO), "buraco") == 0 && strcmp(nomeMotivo(4), "nenhum") == 0,
            "motivo 5 = buraco; o 4 (veiculo) nao existe mais");
  }

  // 7. Textos do OLED cabem na linha (21 caracteres)
  char linha[40];
  snprintf(linha, sizeof linha, "Faixa %dcm %s", 400, nomeCalcada(CALCADA_BLOQUEADA));
  confere(strlen(linha) <= 21, "linha da faixa cabe no OLED");
  snprintf(linha, sizeof linha, "Motivo: %s", nomeMotivo(MOTIVO_AMBOS));
  confere(strlen(linha) <= 21, "linha do motivo cabe no OLED");
  snprintf(linha, sizeof linha, "Buraco (camera) %ds", 600);
  confere(strlen(linha) <= 21, "linha da contagem do buraco cabe no OLED");
  confere(strlen(nomeStatus(STATUS_ACESSIVEL)) * 12 <= 128, "status grande cabe no OLED");

  printf("\n%d verificações passaram, %d falharam.\n", passou, falhou);
  return falhou == 0 ? 0 : 1;
}
