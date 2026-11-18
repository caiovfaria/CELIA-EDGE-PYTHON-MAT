/*
  Testes das regras de decisão do poste (edge/celia_logica.h): luz + calor.
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

// O que o Wokwi faz: com "lux" no módulo, calcula a resistência do LDR e a tensão no AO
// (LDR em série com 10 kΩ, módulo ligado em 3,3 V), e o ESP32 lê de 0 a 4095.
int adcDoWokwi(double lux) {
  double r = 50000.0 * pow(10.0, 0.7) / pow(lux, 0.7);
  double v = 3.3 * r / (r + 10000.0);
  return (int)lround(v / 3.3 * 4095.0);
}

// Cópia da fórmula de índice de calor da biblioteca DHTesp (computeHeatIndex),
// usada só para conferir os valores do roteiro de demonstração do README.
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
  char d[160];

  // 1. Conversão ADC -> lux: pontas
  confere(luxPeloAdc(0) == LUZ_MAXIMA_LUX, "ADC 0 (tensao zero) = 100000 lux (teto)");
  confere(luxPeloAdc(-5) == LUZ_MAXIMA_LUX, "ADC negativo = teto (nao quebra a conta)");
  confere(luxPeloAdc(4095) == 0, "ADC 4095 (tensao maxima) = 0 lux (escuro total)");
  confere(luxPeloAdc(5000) == 0, "ADC acima de 4095 = 0 lux (nao quebra a conta)");
  confere(luxPeloAdc(1) == LUZ_MAXIMA_LUX, "ADC 1 = teto de 100000 lux");

  // 2. Ida e volta: o lux escolhido no Wokwi volta perto do mesmo valor depois do ADC.
  //    Tolerância: até 1 lux abaixo de 100 lux (o valor é inteiro); 2% até 10.000 lux; 3% acima
  //    disso, porque no sol cada passo do ADC vale centenas de lux.
  const double luxes[] = {1, 5, 9, 11, 50, 100, 500, 1000, 5000, 10000, 20000, 30000, 50000, 90000};
  for (double l : luxes) {
    long volta = luxPeloAdc(adcDoWokwi(l));
    // abaixo de 100 lux o valor inteiro pode perder até 1 lux (arredonda para baixo)
    bool perto = l < 100 ? fabs(volta - l) <= 1.0 : fabs(volta - l) / l <= (l < 10000 ? 0.02 : 0.03);
    snprintf(d, sizeof d, "%g lux no Wokwi -> ADC %d -> %ld lux", l, adcDoWokwi(l), volta);
    confere(perto, d);
  }
  confere(luxPeloAdc(adcDoWokwi(100000)) == 100000, "100000 lux no Wokwi -> 100000 lux (teto)");

  // 3. Quanto mais alto o ADC, mais escuro: a conversão nunca "volta" (nunca aumenta)
  bool nuncaAumenta = true;
  for (int adc = 1; adc < 4095; adc++) if (luxPeloAdc(adc + 1) > luxPeloAdc(adc)) nuncaAumenta = false;
  confere(nuncaAumenta, "ADC maior = lux menor ou igual, em toda a faixa 1..4095");

  // 4. Faixas da luz, com as bordas
  confere(classificarLuz(0) == LUZ_ESCURO, "0 lux = escuro");
  confere(classificarLuz(9) == LUZ_ESCURO, "9 lux = escuro");
  confere(classificarLuz(10) == LUZ_NORMAL, "10 lux = normal (borda)");
  confere(classificarLuz(500) == LUZ_NORMAL, "500 lux = normal");
  confere(classificarLuz(24999) == LUZ_NORMAL, "24999 lux = normal");
  confere(classificarLuz(25000) == LUZ_SOL, "25000 lux = sol direto (borda)");
  confere(classificarLuz(100000) == LUZ_SOL, "100000 lux = sol direto");

  // 5. Bordas passando pelo ADC: o número que decide é o mesmo que vai para o ThingSpeak
  confere(luxPeloAdc(3412) == 10 && classificarLuz(luxPeloAdc(3412)) == LUZ_NORMAL, "ADC 3412 = 10 lux = normal");
  confere(luxPeloAdc(3413) == 9 && classificarLuz(luxPeloAdc(3413)) == LUZ_ESCURO, "ADC 3413 = 9 lux = escuro (um passo do ADC acima)");
  confere(classificarLuz(luxPeloAdc(84)) == LUZ_NORMAL && classificarLuz(luxPeloAdc(83)) == LUZ_SOL,
          "perto de 25000 lux: ADC 84 = normal, ADC 83 = sol direto");
  snprintf(d, sizeof d, "um passo do ADC perto de 25000 lux vale %ld lux (por isso a demonstracao usa 50000)", luxPeloAdc(83) - luxPeloAdc(84));
  confere(luxPeloAdc(83) - luxPeloAdc(84) < 1000, d);

  // 6. Valores do roteiro de demonstração (README): cada um longe da borda
  confere(classificarLuz(luxPeloAdc(adcDoWokwi(5))) == LUZ_ESCURO, "roteiro: 5 lux no Wokwi = escuro");
  confere(classificarLuz(luxPeloAdc(adcDoWokwi(500))) == LUZ_NORMAL, "roteiro: 500 lux no Wokwi = normal");
  confere(classificarLuz(luxPeloAdc(adcDoWokwi(50000))) == LUZ_SOL, "roteiro: 50000 lux no Wokwi = sol direto");
  confere(classificarCalor(indiceCalorDHTesp(26, 50)) == CALOR_NORMAL, "roteiro: 26 C e 50% = calor normal");
  confere(classificarCalor(indiceCalorDHTesp(33, 50)) == CALOR_ATENCAO, "roteiro: 33 C e 50% = calor em atencao (IC ~36 C)");
  confere(classificarCalor(indiceCalorDHTesp(36, 50)) == CALOR_PERIGO, "roteiro: 36 C e 50% = calor em perigo (IC ~43 C)");

  // 7. Faixas do calor
  confere(classificarCalor(31.9) == CALOR_NORMAL, "31,9 C = normal");
  confere(classificarCalor(32.0) == CALOR_ATENCAO, "32 C = atencao (borda)");
  confere(classificarCalor(40.9) == CALOR_ATENCAO, "40,9 C = atencao");
  confere(classificarCalor(41.0) == CALOR_PERIGO, "41 C = perigo (borda)");

  // 8. Tabela inteira: 3 situações de luz x 3 de calor (status e motivo)
  struct Linha { int luz, calor, status, motivo; const char* nome; };
  const Linha tabela[] = {
    {LUZ_ESCURO, CALOR_NORMAL,  STATUS_ATENCAO,   MOTIVO_LUZ,    "escuro + calor normal"},
    {LUZ_ESCURO, CALOR_ATENCAO, STATUS_ATENCAO,   MOTIVO_AMBOS,  "escuro + calor atencao"},
    {LUZ_ESCURO, CALOR_PERIGO,  STATUS_ATENCAO,   MOTIVO_AMBOS,  "escuro + calor perigo"},
    {LUZ_NORMAL, CALOR_NORMAL,  STATUS_ACESSIVEL, MOTIVO_NENHUM, "luz normal + calor normal"},
    {LUZ_NORMAL, CALOR_ATENCAO, STATUS_ATENCAO,   MOTIVO_CALOR,  "luz normal + calor atencao"},
    {LUZ_NORMAL, CALOR_PERIGO,  STATUS_ATENCAO,   MOTIVO_CALOR,  "luz normal + calor perigo"},
    {LUZ_SOL,    CALOR_NORMAL,  STATUS_ACESSIVEL, MOTIVO_NENHUM, "sol direto + calor normal"},
    {LUZ_SOL,    CALOR_ATENCAO, STATUS_EVITE,     MOTIVO_AMBOS,  "sol direto + calor atencao"},
    {LUZ_SOL,    CALOR_PERIGO,  STATUS_EVITE,     MOTIVO_AMBOS,  "sol direto + calor perigo"},
  };
  for (const Linha& t : tabela) {
    snprintf(d, sizeof d, "%s = status %d, motivo %d", t.nome, t.status, t.motivo);
    confere(calcularStatus(t.luz, t.calor) == t.status && calcularMotivo(t.luz, t.calor) == t.motivo, d);
  }
  // o motivo só é 0 quando o status é acessível (e vice-versa)
  bool coerente = true;
  for (const Linha& t : tabela)
    if ((calcularStatus(t.luz, t.calor) == STATUS_ACESSIVEL) != (calcularMotivo(t.luz, t.calor) == MOTIVO_NENHUM)) coerente = false;
  confere(coerente, "motivo 0 se e somente se o status e acessivel");
  // os números de status e motivo continuam dentro do que a API aceita (status 0-2, motivo 0-5)
  confere(STATUS_ACESSIVEL == 0 && STATUS_ATENCAO == 1 && STATUS_EVITE == 2, "status 0, 1, 2 (os mesmos do contrato com o app)");
  confere(MOTIVO_LUZ == 1 && MOTIVO_CALOR == 2 && MOTIVO_AMBOS == 3 && MOTIVO_BURACO == 5, "motivos 1 luz, 2 calor, 3 ambos, 5 buraco");

  // 9. Buraco da câmera: manda em tudo; um aviso velho não vale mais
  for (const Linha& t : tabela) {
    snprintf(d, sizeof d, "buraco + %s = EVITE, motivo 5", t.nome);
    confere(statusComCamera(calcularStatus(t.luz, t.calor), true) == STATUS_EVITE
            && motivoComCamera(calcularMotivo(t.luz, t.calor), true) == MOTIVO_BURACO, d);
  }
  confere(statusComCamera(STATUS_ATENCAO, false) == STATUS_ATENCAO && motivoComCamera(MOTIVO_LUZ, false) == MOTIVO_LUZ, "sem buraco: vale a regra dos sensores");
  confere(buracoValendo(true, 1000, 60999, 60000), "buraco com 59,999 s: ainda vale");
  confere(!buracoValendo(true, 1000, 61000, 60000), "buraco com 60 s: venceu");
  confere(!buracoValendo(false, 1000, 2000, 60000), "sem aviso: nao vale");
  confere(buracoValendo(true, 0xFFFFF000u, 0x00001000u, 60000), "buraco vale certo quando millis() volta a zero");

  // 10. Confirmação da luz
  Confirmador c;
  confere(atualizarConfirmacao(c, LUZ_NORMAL, 0, 15000) == LUZ_NORMAL, "primeira leitura vale na hora");
  confere(atualizarConfirmacao(c, LUZ_ESCURO, 5000, 15000) == LUZ_NORMAL, "escureceu: ainda nao confirmou");
  confere(msParaConfirmar(c, 10000, 15000) == 10000, "faltam 10 s para confirmar");
  confere(atualizarConfirmacao(c, LUZ_ESCURO, 15000, 15000) == LUZ_NORMAL, "10 s escuro: ainda normal");
  confere(atualizarConfirmacao(c, LUZ_ESCURO, 20000, 15000) == LUZ_ESCURO, "15 s escuro: confirmou escuro");
  confere(msParaConfirmar(c, 20000, 15000) == 0, "confirmado: nao falta nada");
  confere(atualizarConfirmacao(c, LUZ_NORMAL, 25000, 15000) == LUZ_ESCURO, "farol passou (luz normal): ainda escuro");
  confere(atualizarConfirmacao(c, LUZ_ESCURO, 30000, 15000) == LUZ_ESCURO && msParaConfirmar(c, 30000, 15000) == 0,
          "o farol foi embora antes do tempo: a mudanca e cancelada");
  confere(atualizarConfirmacao(c, LUZ_NORMAL, 35000, 15000) == LUZ_ESCURO, "poste acendeu: comeca a contar");
  confere(atualizarConfirmacao(c, LUZ_SOL, 40000, 15000) == LUZ_ESCURO, "trocou de novo (sol): recomeca a contar");
  confere(msParaConfirmar(c, 40000, 15000) == 15000, "a contagem recomecou do zero");
  Confirmador v;
  atualizarConfirmacao(v, LUZ_NORMAL, 0xFFFFE000u, 15000);
  atualizarConfirmacao(v, LUZ_ESCURO, 0xFFFFF000u, 15000);
  confere(atualizarConfirmacao(v, LUZ_ESCURO, 0x00003000u, 15000) == LUZ_ESCURO, "confirmacao certa quando millis() volta a zero");

  // 11. Textos do serial
  confere(strcmp(nomeLuz(LUZ_ESCURO), "escuro") == 0 && strcmp(nomeLuz(LUZ_SOL), "sol direto") == 0 && strcmp(nomeLuz(LUZ_NORMAL), "normal") == 0, "nomes da luz");
  confere(strcmp(nomeMotivo(MOTIVO_LUZ), "luz") == 0 && strcmp(nomeMotivo(MOTIVO_AMBOS), "luz+calor") == 0 && strcmp(nomeMotivo(MOTIVO_BURACO), "buraco") == 0
          && strcmp(nomeMotivo(MOTIVO_NENHUM), "nenhum") == 0 && strcmp(nomeMotivo(MOTIVO_CALOR), "calor") == 0, "nomes dos motivos");
  confere(strcmp(nomeStatus(STATUS_ACESSIVEL), "ACESSIVEL") == 0 && strcmp(nomeStatus(STATUS_ATENCAO), "ATENCAO") == 0 && strcmp(nomeStatus(STATUS_EVITE), "EVITE") == 0, "nomes dos status");

  printf("\n%d verificações passaram, %d falharam.\n", passou, falhou);
  return falhou == 0 ? 0 : 1;
}
