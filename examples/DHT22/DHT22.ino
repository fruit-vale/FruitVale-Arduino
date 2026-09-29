// FruitVale — exemplo com o sensor DHT22 (ou DHT11): temperatura e umidade do ar.
// É só um dos sensores possíveis; a biblioteca FruitVale não depende dele.
//
// Ligação: VCC → 3V3, GND → GND, DATA → GPIO 4 (resistor de 10 kΩ entre DATA
// e 3V3, se o módulo não tiver). Requer a biblioteca "DHT sensor library"
// (Adafruit) e "Adafruit Unified Sensor".
//
// No app FruitVale, cadastre o sensor com "Usar temperatura e umidade"
// (chaves temperatura e umidade). Configuração do Wi-Fi e do sensor: pelo
// portal (veja o exemplo Basico).

#include <DHT.h>
#include <FruitVale.h>

constexpr uint8_t PINO_DHT = 4;
DHT dht(PINO_DHT, DHT22);  // DHT11 para o modelo azul
FruitVale fv;

void setup() {
  Serial.begin(115200);
  dht.begin();
  fv.intervalo(60);
  fv.begin();
}

void loop() {
  fv.loop();
  if (fv.horaDeLer()) {
    // leitura que falhar (NaN) fica de fora da mensagem automaticamente
    fv.valor("temperatura", dht.readTemperature(), 1);
    fv.valor("umidade", dht.readHumidity(), 1);
    fv.enviar();
  }
}
