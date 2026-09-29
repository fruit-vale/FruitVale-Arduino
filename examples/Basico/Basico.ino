// FruitVale — exemplo básico, sem nenhum sensor ligado.
// Envia a temperatura interna do chip e o sinal do Wi-Fi: serve para testar a
// conexão com a plataforma antes de ligar um sensor de verdade.
//
// No app FruitVale, cadastre o sensor com estes dados (chave no JSON):
//   "Temperatura do chip"  →  temperatura_chip   (tipo Outro, unidade °C)
//   "Sinal Wi-Fi"          →  sinal_wifi         (tipo Outro, unidade dBm)
//
// Primeiro uso: a placa cria a rede "FruitVale-XXXX" (senha "fruitvale").
// Conecte o celular nela e preencha o Wi-Fi do local e o usuário/senha do
// sensor que o app mostrou. Para reconfigurar: ligue a placa e aperte o
// botão BOOT nos primeiros 3 segundos.

#include <FruitVale.h>

FruitVale fv;

void setup() {
  Serial.begin(115200);
  fv.intervalo(60);  // uma leitura por minuto
  fv.begin();
}

void loop() {
  fv.loop();
  if (fv.horaDeLer()) {
    fv.valor("temperatura_chip", temperatureRead(), 1);
    fv.valor("sinal_wifi", WiFi.RSSI(), 0);
    fv.enviar();
  }
}
