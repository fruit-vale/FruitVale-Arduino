// FruitVale — cliente para sensores IoT da plataforma FruitVale (ESP32).
//
// Cuida de tudo entre o sensor e a plataforma: Wi-Fi, relógio (NTP), conexão
// MQTT com TLS validado, reconexão e o formato das mensagens. O sketch só lê
// o sensor e chama valor() / enviar().
//
// As credenciais podem vir do código (wifi(), sensor()) ou do portal de
// configuração: sem configuração salva — ou apertando o botão BOOT logo após
// ligar — a placa cria a rede "FruitVale-XXXX" com uma página para digitar o
// Wi-Fi do local e o usuário/senha do sensor.
#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

class FruitVale {
 public:
  FruitVale();

  // ---- Configuração (antes de begin(); todas opcionais) --------------------

  /** Wi-Fi do local. Sem chamar, usa o que foi salvo pelo portal. */
  FruitVale& wifi(const char* ssid, const char* senha);
  /** Usuário (código do sensor, ex.: "emp1-estufa-01") e senha MQTT gerados no app. */
  FruitVale& sensor(const char* usuario, const char* senha);
  /** Só se o usuário não tiver o prefixo "empN-" (a empresa normalmente vem dele). */
  FruitVale& empresa(int id);
  /** Broker; o padrão é mqtt.fruitvale.com.br:8883. */
  FruitVale& host(const char* host, uint16_t porta = 8883);
  /** Segundos entre leituras (horaDeLer()); padrão 60, mínimo 1. Respeite também o
   *  intervalo mínimo do seu sensor (ex.: 2 s no DHT22). */
  FruitVale& intervalo(uint32_t segundos);
  /** LED de status; padrão GPIO 8 aceso em LOW (Super Mini). -1 desliga. */
  FruitVale& led(int pino, bool acendeEmLow = true);
  /** Botão que abre o portal logo após ligar; padrão GPIO 9 (BOOT). -1 desliga. */
  FruitVale& botaoConfiguracao(int pino);
  /** Senha da rede do portal (mínimo 8 caracteres); padrão "fruitvale". */
  FruitVale& senhaPortal(const char* senha);
  /** Potência do rádio; padrão 8,5 dBm (várias ESP32-C3 Super Mini não conectam no máximo). */
  FruitVale& potenciaWifi(wifi_power_t potencia);

  // ---- Uso -----------------------------------------------------------------

  /** Inicia: abre o portal se preciso, conecta no Wi-Fi e no broker. */
  void begin();
  /** Chame em todo loop(): mantém Wi-Fi, relógio e MQTT conectados. */
  void loop();

  /** Conectado ao broker e pronto para enviar. */
  bool conectado();
  /** true uma vez a cada intervalo (e logo ao conectar): hora de ler o sensor. */
  bool horaDeLer();
  /** Acrescenta um dado à próxima mensagem; NaN (leitura falha) é ignorado. */
  void valor(const char* chave, float valor, uint8_t casas = 2);
  /** Publica os dados acumulados (com data_hora) e esvazia a mensagem. */
  bool enviar();

  /** Abre o portal de configuração agora (não retorna: reinicia ao salvar). */
  void abrirPortal();
  /** Apaga Wi-Fi e credenciais salvos pelo portal. */
  void apagarConfiguracao();

  const char* usuario() const { return _usuario.c_str(); }
  int empresaId() const { return _empresa; }

 private:
  void carregarConfiguracao();
  bool configurado() const;
  bool conectarWifi();
  bool sincronizarRelogio();
  bool conectarMqtt();
  bool botaoPressionadoAoLigar();
  void acenderLed(bool aceso);
  void piscar(uint8_t vezes, uint16_t ms);
  void log(const char* formato, ...);

  String _ssid, _senhaWifi, _usuario, _senhaMqtt, _host, _senhaPortal;
  uint16_t _porta;
  int _empresa;
  uint32_t _intervaloMs;
  int _pinoLed, _pinoBotao;
  bool _ledEmLow;
  wifi_power_t _potencia;
  // o que veio do código tem prioridade sobre o salvo pelo portal
  bool _wifiNoCodigo, _sensorNoCodigo;

  WiFiClientSecure _tls;
  PubSubClient _mqtt;
  JsonDocument _mensagem;
  char _topico[128];

  bool _wifiIniciado;
  uint32_t _inicioWifiMs, _proximaTentativaMqttMs, _esperaMqttMs;
  uint32_t _proximaLeituraMs, _ultimaConexaoOkMs;
  bool _leituraPendente;
};
