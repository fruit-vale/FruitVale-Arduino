#include "FruitVale.h"

#include <Preferences.h>
#include <stdarg.h>
#include <time.h>

#include "certificados.h"

namespace {

constexpr const char* HOST_PADRAO = "mqtt.fruitvale.com.br";
constexpr uint16_t PORTA_PADRAO = 8883;
constexpr const char* SENHA_PORTAL_PADRAO = "fruitvale";
constexpr uint32_t INTERVALO_PADRAO_MS = 60000;

constexpr uint32_t WIFI_TIMEOUT_MS = 20000;
constexpr uint32_t NTP_TIMEOUT_MS = 15000;
constexpr uint32_t MQTT_ESPERA_MIN_MS = 2000;
constexpr uint32_t MQTT_ESPERA_MAX_MS = 60000;
// sem conexão com o broker por tanto tempo, reinicia a placa (recupera travas)
constexpr uint32_t REINICIAR_SEM_CONEXAO_MS = 15UL * 60 * 1000;
// janela, logo após ligar, em que apertar o botão abre o portal
constexpr uint32_t JANELA_BOTAO_MS = 3000;

// leituras_valores.valor é NUMERIC(14, 4): um valor acima disso faz a plataforma
// recusar a mensagem inteira
constexpr float VALOR_LIMITE = 1e10f;

// 20 dados (o máximo por sensor na plataforma) com chaves de até 40 caracteres
constexpr size_t MENSAGEM_MAX = 1280;
// buffer do MQTT: a mensagem + tópico + cabeçalho do pacote
constexpr uint16_t BUFFER_MQTT = MENSAGEM_MAX + 256;

constexpr const char* NTP_1 = "a.st1.ntp.br";
constexpr const char* NTP_2 = "pool.ntp.org";

/** Já passou do instante? (seguro quando millis() volta a zero, a cada ~49 dias) */
bool passou(uint32_t instanteMs) { return static_cast<int32_t>(millis() - instanteMs) >= 0; }

bool relogioCerto() { return time(nullptr) > 1700000000; }  // depois de 2023

/** "emp12-estufa-01" → 12; -1 se não tiver o prefixo. */
int empresaDoUsuario(const String& usuario) {
  if (!usuario.startsWith("emp")) return -1;
  const int hifen = usuario.indexOf('-');
  if (hifen <= 3) return -1;
  for (int i = 3; i < hifen; i++)
    if (!isDigit(usuario[i])) return -1;
  return usuario.substring(3, hifen).toInt();
}

/** Mesma regra do app: minúsculas, números e _, começando por letra, até 40. */
bool chaveValida(const char* chave) {
  if (!chave || chave[0] < 'a' || chave[0] > 'z') return false;
  size_t i = 0;
  for (; chave[i]; i++) {
    const char c = chave[i];
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_') || i >= 40) return false;
  }
  return strcmp(chave, "data_hora") != 0;  // reservada para o horário da leitura
}

const char* explicarEstadoMqtt(int estado) {
  switch (estado) {
    case MQTT_CONNECTION_TIMEOUT: return "tempo esgotado (broker fora do ar?)";
    case MQTT_CONNECTION_LOST: return "conexão perdida";
    case MQTT_CONNECT_FAILED: return "falha de conexão TLS/TCP (certificado, rede ou firewall)";
    case MQTT_CONNECT_BAD_PROTOCOL: return "versão de protocolo recusada";
    case MQTT_CONNECT_BAD_CLIENT_ID: return "identificador recusado";
    case MQTT_CONNECT_UNAVAILABLE: return "broker indisponível";
    case MQTT_CONNECT_BAD_CREDENTIALS: return "usuário ou senha incorretos";
    case MQTT_CONNECT_UNAUTHORIZED: return "não autorizado (sensor desativado ou senha trocada?)";
    default: return "erro desconhecido";
  }
}

}  // namespace

FruitVale::FruitVale()
    : _host(HOST_PADRAO),
      _senhaPortal(SENHA_PORTAL_PADRAO),
      _porta(PORTA_PADRAO),
      _empresa(-1),
      _intervaloMs(INTERVALO_PADRAO_MS),
      _pinoLed(8),
      _pinoBotao(9),
      _ledEmLow(true),
      _potencia(WIFI_POWER_8_5dBm),
      _wifiNoCodigo(false),
      _sensorNoCodigo(false),
      _mqtt(_tls),
      _wifiIniciado(false),
      _inicioWifiMs(0),
      _proximaTentativaMqttMs(0),
      _esperaMqttMs(MQTT_ESPERA_MIN_MS),
      _proximaLeituraMs(0),
      _ultimaConexaoOkMs(0),
      _leituraPendente(false) {
  _topico[0] = '\0';
}

// ---- Configuração -------------------------------------------------------------

FruitVale& FruitVale::wifi(const char* ssid, const char* senha) {
  _ssid = ssid;
  _senhaWifi = senha;
  _wifiNoCodigo = true;
  return *this;
}

FruitVale& FruitVale::sensor(const char* usuario, const char* senha) {
  _usuario = usuario;
  _senhaMqtt = senha;
  _sensorNoCodigo = true;
  return *this;
}

FruitVale& FruitVale::empresa(int id) {
  _empresa = id;
  return *this;
}

FruitVale& FruitVale::host(const char* host, uint16_t porta) {
  _host = host;
  _porta = porta;
  return *this;
}

FruitVale& FruitVale::intervalo(uint32_t segundos) {
  // mínimo de 1 s: leituras mais frequentes só sobrecarregam a plataforma
  _intervaloMs = max<uint32_t>(segundos, 1) * 1000;
  return *this;
}

FruitVale& FruitVale::led(int pino, bool acendeEmLow) {
  _pinoLed = pino;
  _ledEmLow = acendeEmLow;
  return *this;
}

FruitVale& FruitVale::botaoConfiguracao(int pino) {
  _pinoBotao = pino;
  return *this;
}

FruitVale& FruitVale::senhaPortal(const char* senha) {
  if (strlen(senha) >= 8) _senhaPortal = senha;  // mínimo do WPA2
  return *this;
}

FruitVale& FruitVale::potenciaWifi(wifi_power_t potencia) {
  _potencia = potencia;
  return *this;
}

// ---- Utilitários ----------------------------------------------------------------

void FruitVale::log(const char* formato, ...) {
  char texto[192];
  va_list args;
  va_start(args, formato);
  vsnprintf(texto, sizeof(texto), formato, args);
  va_end(args);
  Serial.printf("[FruitVale] %s\n", texto);
}

void FruitVale::acenderLed(bool aceso) {
  if (_pinoLed >= 0) digitalWrite(_pinoLed, aceso == _ledEmLow ? LOW : HIGH);
}

void FruitVale::piscar(uint8_t vezes, uint16_t ms) {
  for (uint8_t i = 0; i < vezes; i++) {
    acenderLed(true);
    delay(ms);
    acenderLed(false);
    delay(ms);
  }
}

void FruitVale::carregarConfiguracao() {
  Preferences prefs;
  prefs.begin("fruitvale", true);
  // o que veio do código prevalece sobre o que o portal salvou
  if (!_wifiNoCodigo) {
    _ssid = prefs.getString("ssid", "");
    _senhaWifi = prefs.getString("wpass", "");
  }
  if (!_sensorNoCodigo) {
    _usuario = prefs.getString("user", "");
    _senhaMqtt = prefs.getString("mpass", "");
  }
  prefs.end();
  if (_empresa < 0) _empresa = empresaDoUsuario(_usuario);
}

bool FruitVale::configurado() const {
  return _ssid.length() && _usuario.length() && _senhaMqtt.length() && _empresa >= 0;
}

void FruitVale::apagarConfiguracao() {
  Preferences prefs;
  prefs.begin("fruitvale", false);
  prefs.clear();
  prefs.end();
  log("configuração apagada");
}

bool FruitVale::botaoPressionadoAoLigar() {
  if (_pinoBotao < 0) return false;
  pinMode(_pinoBotao, INPUT_PULLUP);
  log("aperte o botão BOOT agora para abrir a configuração...");
  const uint32_t inicio = millis();
  while (millis() - inicio < JANELA_BOTAO_MS) {
    acenderLed((millis() / 100) % 2);  // pisca rápido: janela aberta
    if (digitalRead(_pinoBotao) == LOW) {
      acenderLed(false);
      return true;
    }
    delay(10);
  }
  acenderLed(false);
  return false;
}

// ---- Ciclo principal --------------------------------------------------------------

void FruitVale::begin() {
  if (_pinoLed >= 0) pinMode(_pinoLed, OUTPUT);
  acenderLed(false);
  carregarConfiguracao();

  if (botaoPressionadoAoLigar()) abrirPortal();  // não retorna
  if (!configurado()) {
    if (_usuario.length() && _empresa < 0)
      log("o usuário \"%s\" não tem o prefixo empN-; use empresa(N) no código", _usuario.c_str());
    else
      log("sem configuração salva");
    abrirPortal();  // não retorna
  }

  snprintf(_topico, sizeof(_topico), "empresas/%d/sensores/%s/leituras", _empresa, _usuario.c_str());
  _tls.setCACert(fruitvale::CERTIFICADOS_RAIZ);  // valida o certificado do broker
  _mqtt.setServer(_host.c_str(), _porta);
  _mqtt.setBufferSize(BUFFER_MQTT);  // o padrão (256 bytes) só comporta poucos dados
  _mqtt.setKeepAlive(60);
  _mqtt.setSocketTimeout(15);
  _ultimaConexaoOkMs = millis();

  log("sensor %s → %s:%u, tópico %s", _usuario.c_str(), _host.c_str(), _porta, _topico);
}

bool FruitVale::conectarWifi() {
  if (WiFi.status() == WL_CONNECTED) return true;

  if (!_wifiIniciado || passou(_inicioWifiMs + WIFI_TIMEOUT_MS)) {
    if (_wifiIniciado) log("Wi-Fi não conectou (confira a rede: precisa ser 2,4 GHz); tentando de novo");
    log("conectando no Wi-Fi \"%s\"...", _ssid.c_str());
    WiFi.mode(WIFI_STA);
    WiFi.setHostname(_usuario.c_str());
    WiFi.setAutoReconnect(true);
    WiFi.begin(_ssid.c_str(), _senhaWifi.c_str());
    WiFi.setTxPower(_potencia);
    _wifiIniciado = true;
    _inicioWifiMs = millis();
  }
  acenderLed((millis() / 250) % 2);
  if (WiFi.status() != WL_CONNECTED) return false;

  acenderLed(false);
  log("Wi-Fi conectado, IP %s, sinal %d dBm", WiFi.localIP().toString().c_str(), WiFi.RSSI());
  return true;
}

bool FruitVale::sincronizarRelogio() {
  if (relogioCerto()) return true;
  log("acertando o relógio (NTP)...");
  configTime(0, 0, NTP_1, NTP_2);  // UTC
  const uint32_t inicio = millis();
  while (!relogioCerto() && millis() - inicio < NTP_TIMEOUT_MS) delay(200);
  if (!relogioCerto()) {
    log("sem resposta dos servidores de hora");
    return false;
  }
  return true;
}

bool FruitVale::conectarMqtt() {
  if (_mqtt.connected()) return true;
  if (!passou(_proximaTentativaMqttMs)) return false;

  // entre aspas e com o tamanho da senha: denuncia espaço sobrando ou senha cortada
  log("conectando em %s:%u como \"%s\" (senha com %u caracteres)...", _host.c_str(), _porta, _usuario.c_str(),
      static_cast<unsigned>(_senhaMqtt.length()));
  // client id = código do sensor: se a placa reiniciar, o broker troca a sessão antiga
  if (_mqtt.connect(_usuario.c_str(), _usuario.c_str(), _senhaMqtt.c_str())) {
    log("conectado à plataforma");
    _esperaMqttMs = MQTT_ESPERA_MIN_MS;
    _leituraPendente = true;  // já lê ao conectar
    piscar(2, 120);
    return true;
  }

  const int estado = _mqtt.state();
  // o erro TLS só interessa quando o TLS falhou; com CONNACK recebido é resto de antes
  char erroTls[100] = "";
  if (estado == MQTT_CONNECT_FAILED) _tls.lastError(erroTls, sizeof(erroTls));
  log("falha ao conectar (estado %d: %s)%s%s — nova tentativa em %lu s", estado, explicarEstadoMqtt(estado),
      erroTls[0] ? " | TLS: " : "", erroTls, static_cast<unsigned long>(_esperaMqttMs / 1000));
  _proximaTentativaMqttMs = millis() + _esperaMqttMs;
  _esperaMqttMs = min(_esperaMqttMs * 2, MQTT_ESPERA_MAX_MS);
  return false;
}

void FruitVale::loop() {
  if (conectarWifi() && sincronizarRelogio() && conectarMqtt()) {
    _mqtt.loop();
    _ultimaConexaoOkMs = millis();
  }
  if (millis() - _ultimaConexaoOkMs > REINICIAR_SEM_CONEXAO_MS) {
    log("15 minutos sem conexão com a plataforma; reiniciando a placa");
    delay(200);
    ESP.restart();
  }
}

bool FruitVale::conectado() { return _mqtt.connected(); }

bool FruitVale::horaDeLer() {
  if (!conectado()) return false;
  if (!_leituraPendente && !passou(_proximaLeituraMs)) return false;
  _leituraPendente = false;
  _proximaLeituraMs = millis() + _intervaloMs;
  return true;
}

// ---- Mensagens ----------------------------------------------------------------------

void FruitVale::valor(const char* chave, float valor, uint8_t casas) {
  if (isnan(valor) || isinf(valor)) return;  // leitura falha: fica de fora
  // a plataforma ignoraria a chave inválida (ou recusaria o valor) sem a placa saber
  if (!chaveValida(chave)) {
    log("chave \"%s\" inválida: use a chave do dado no app (minúsculas, números e _)", chave ? chave : "");
    return;
  }
  if (fabsf(valor) >= VALOR_LIMITE) {
    log("valor de \"%s\" fora do intervalo aceito; ficou de fora", chave);
    return;
  }
  const float fator = powf(10, casas);
  _mensagem[chave] = roundf(valor * fator) / fator;
}

bool FruitVale::enviar() {
  if (_mensagem.size() == 0) {
    log("nada para enviar (nenhum valor válido lido)");
    return false;
  }
  if (!conectado()) {
    log("sem conexão; mensagem descartada");
    _mensagem.clear();
    return false;
  }

  // horário da leitura em ISO 8601 (UTC)
  char dataHora[25];
  const time_t agora = time(nullptr);
  struct tm utc;
  gmtime_r(&agora, &utc);
  strftime(dataHora, sizeof(dataHora), "%Y-%m-%dT%H:%M:%SZ", &utc);
  _mensagem["data_hora"] = dataHora;

  // mensagem cortada viraria JSON inválido e seria descartada pela plataforma
  if (measureJson(_mensagem) >= MENSAGEM_MAX) {
    log("mensagem grande demais (%u bytes, máximo %u): use menos dados ou chaves mais curtas",
        static_cast<unsigned>(measureJson(_mensagem)), static_cast<unsigned>(MENSAGEM_MAX));
    _mensagem.clear();
    return false;
  }
  static char texto[MENSAGEM_MAX];  // fora da pilha: 1,3 KB na pilha do loop() é arriscado
  const size_t tamanho = serializeJson(_mensagem, texto, sizeof(texto));
  _mensagem.clear();

  if (!_mqtt.publish(_topico, reinterpret_cast<const uint8_t*>(texto), tamanho, false)) {
    log("falha ao publicar (a conexão caiu?)");
    return false;
  }
  Serial.printf("[FruitVale] enviado: %s\n", texto);  // sem o limite de tamanho do log()
  piscar(1, 60);
  return true;
}
