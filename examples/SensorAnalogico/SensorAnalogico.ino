// FruitVale — qualquer sensor de saída analógica.
// O exemplo usa um sensor capacitivo de umidade do solo, mas o mesmo vale para
// luminosidade (LDR), nível, pressão etc.: leia a tensão e converta para a
// grandeza do seu sensor.
//
// Ligação: VCC → 3V3, GND → GND, saída analógica → GPIO 3 (entrada ADC).
//
// Calibração (umidade do solo): anote o valor bruto com o sensor no ar
// (LEITURA_SECO) e mergulhado em água (LEITURA_MOLHADO) — o Serial mostra
// esse valor a cada envio.
//
// No app FruitVale, cadastre o dado "Umidade solo" (tipo Umidade do solo,
// unidade %; o app gera a chave umidade_solo a partir do nome). Configuração do Wi-Fi e do sensor: pelo
// portal (veja o exemplo Basico).

#include <FruitVale.h>

constexpr uint8_t PINO_SENSOR = 3;
constexpr int LEITURA_SECO = 3000;     // ajuste na calibração
constexpr int LEITURA_MOLHADO = 1300;  // ajuste na calibração
constexpr uint8_t AMOSTRAS = 16;       // média para reduzir o ruído do ADC

FruitVale fv;

float lerUmidadeDoSolo() {
  uint32_t soma = 0;
  for (uint8_t i = 0; i < AMOSTRAS; i++) {
    soma += analogRead(PINO_SENSOR);
    delay(5);
  }
  const int bruto = soma / AMOSTRAS;
  Serial.printf("leitura bruta do sensor: %d\n", bruto);

  // seco → 0 %, molhado → 100 %
  const float umidade = 100.0f * (LEITURA_SECO - bruto) / (LEITURA_SECO - LEITURA_MOLHADO);
  return constrain(umidade, 0.0f, 100.0f);
}

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);  // 0–4095
  fv.intervalo(60);
  fv.begin();
}

void loop() {
  fv.loop();
  if (fv.horaDeLer()) {
    fv.valor("umidade_solo", lerUmidadeDoSolo(), 1);
    fv.enviar();
  }
}
