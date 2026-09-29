// FruitVale — credenciais direto no código, sem usar o portal.
// Útil em bancada ou quando cada placa é gravada já com seus dados.
// ⚠️ Não publique este arquivo com as senhas preenchidas.

#include <FruitVale.h>

FruitVale fv;

void setup() {
  Serial.begin(115200);
  fv.wifi("NOME_DA_REDE", "SENHA_DA_REDE")
    .sensor("emp1-estufa-01", "SENHA_GERADA_NO_APP")  // a empresa sai do prefixo emp1-
    .intervalo(30)
    .botaoConfiguracao(-1);  // desliga o portal pelo botão
  fv.begin();
}

void loop() {
  fv.loop();
  if (fv.horaDeLer()) {
    fv.valor("temperatura_chip", temperatureRead(), 1);
    fv.enviar();
  }
}
