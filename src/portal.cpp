// Portal de configuração: a placa vira um ponto de acesso Wi-Fi com uma página
// para digitar o Wi-Fi do local e as credenciais do sensor. Ao conectar na
// rede "FruitVale-XXXX" o celular abre a página sozinho (portal cativo).

#include <DNSServer.h>
#include <Preferences.h>
#include <WebServer.h>

#include "FruitVale.h"

namespace {

// fecha o portal sozinho (e reinicia) se ninguém salvar nada
constexpr uint32_t PORTAL_TIMEOUT_MS = 10UL * 60 * 1000;
constexpr uint8_t PORTA_DNS = 53;

const char PAGINA_INICIO[] = R"HTML(<!doctype html><html lang="pt-BR"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>FruitVale — configurar sensor</title><style>
body{margin:0;font-family:Arial,Helvetica,sans-serif;background:#faf9fc;color:#4d392b}
main{max-width:420px;margin:0 auto;padding:24px 18px}
h1{margin:0 0 4px;font-size:22px;color:#08060d}.sub{margin:0 0 20px;color:#6b6375;font-size:14px}
fieldset{border:1px solid #e5e4e7;border-radius:12px;padding:14px;margin:0 0 16px;background:#fff}
legend{padding:0 6px;font-weight:700}
label{display:block;margin:10px 0 4px;font-size:13px;color:#6b6375}
input{box-sizing:border-box;width:100%;padding:11px 12px;border:1px solid #e5e4e7;border-radius:9px;font-size:15px}
small{display:block;margin-top:4px;color:#8a8490;font-size:12px}
button{width:100%;padding:13px;border:0;border-radius:10px;background:#4b24ba;color:#fff;font-size:15px;font-weight:700}
.ok{padding:14px;border-radius:10px;background:#e8f5ec;color:#2a6e3f}
.erro{padding:10px 12px;border-radius:9px;background:#fdecec;color:#a12a2a;font-size:13px}
</style></head><body><main>)HTML";

const char PAGINA_FIM[] = "</main></body></html>";

String escapar(const String& texto) {
  String saida;
  saida.reserve(texto.length() + 8);
  for (char c : texto) {
    switch (c) {
      case '&': saida += "&amp;"; break;
      case '<': saida += "&lt;"; break;
      case '>': saida += "&gt;"; break;
      case '"': saida += "&quot;"; break;
      default: saida += c;
    }
  }
  return saida;
}

/** Redes Wi-Fi próximas, para sugerir no campo SSID. */
String opcoesDeRedes() {
  String opcoes;
  const int total = WiFi.scanNetworks();
  for (int i = 0; i < total && i < 20; i++) {
    const String ssid = WiFi.SSID(i);
    if (ssid.length() && opcoes.indexOf("\"" + escapar(ssid) + "\"") < 0)
      opcoes += "<option value=\"" + escapar(ssid) + "\">";
  }
  WiFi.scanDelete();
  return opcoes;
}

}  // namespace

void FruitVale::abrirPortal() {
  // nome único pelo final do MAC, para distinguir várias placas lado a lado
  uint8_t mac[6];
  WiFi.macAddress(mac);
  char nomeRede[24];
  snprintf(nomeRede, sizeof(nomeRede), "FruitVale-%02X%02X", mac[4], mac[5]);

  WiFi.disconnect(true);
  WiFi.mode(WIFI_AP_STA);  // STA para poder listar as redes próximas
  const String redes = opcoesDeRedes();
  WiFi.softAP(nomeRede, _senhaPortal.c_str());
  const IPAddress ip = WiFi.softAPIP();

  log("modo de configuração: conecte o celular na rede \"%s\" (senha \"%s\") e abra http://%s",
      nomeRede, _senhaPortal.c_str(), ip.toString().c_str());

  DNSServer dns;
  dns.start(PORTA_DNS, "*", ip);  // todo endereço leva à página (portal cativo)
  WebServer servidor(80);

  servidor.on("/", HTTP_GET, [&]() {
    String html = PAGINA_INICIO;
    html += "<h1>Configurar sensor FruitVale</h1>";
    html += "<p class=\"sub\">Rede da placa: <b>" + String(nomeRede) + "</b></p>";
    html += "<form method=\"post\" action=\"/salvar\">";
    html += "<fieldset><legend>Wi-Fi do local</legend>";
    html += "<label for=\"ssid\">Nome da rede (2,4 GHz)</label>";
    html += "<input id=\"ssid\" name=\"ssid\" list=\"redes\" required value=\"" + escapar(_ssid) + "\">";
    html += "<datalist id=\"redes\">" + redes + "</datalist>";
    html += "<label for=\"wpass\">Senha do Wi-Fi</label>";
    html += "<input id=\"wpass\" name=\"wpass\" type=\"password\"" +
            String(_senhaWifi.length() ? " placeholder=\"(manter a atual)\"" : "") + ">";
    html += "</fieldset><fieldset><legend>Sensor no app FruitVale</legend>";
    html += "<label for=\"user\">Usuário (código do sensor)</label>";
    html += "<input id=\"user\" name=\"user\" required autocapitalize=\"off\" autocorrect=\"off\" spellcheck=\"false\" placeholder=\"emp1-estufa-01\" value=\"" +
            escapar(_usuario) + "\">";
    html += "<label for=\"mpass\">Senha do sensor</label>";
    html += "<input id=\"mpass\" name=\"mpass\" type=\"password\" autocapitalize=\"off\"" +
            String(_senhaMqtt.length() ? " placeholder=\"(manter a atual)\"" : " required") + ">";
    html += "<small>Os dois aparecem no app ao cadastrar o sensor (Sensores → Cadastrar sensor).</small>";
    html += "</fieldset><button type=\"submit\">Salvar e conectar</button></form>";
    html += PAGINA_FIM;
    servidor.send(200, "text/html; charset=utf-8", html);
  });

  servidor.on("/salvar", HTTP_POST, [&]() {
    // o teclado do celular costuma deixar espaço no fim (ao colar ou pelo autocompletar)
    String usuario = servidor.arg("user");
    String senhaMqtt = servidor.arg("mpass");
    usuario.trim();
    senhaMqtt.trim();
    const String ssid = servidor.arg("ssid");
    const String senhaWifi = servidor.arg("wpass");

    // campo de senha vazio = manter a senha já salva
    const bool temSenhaMqtt = senhaMqtt.length() || _senhaMqtt.length();
    if (!ssid.length() || !usuario.length() || !temSenhaMqtt) {
      String html = PAGINA_INICIO;
      html += "<p class=\"erro\">Preencha o nome da rede, o usuário e a senha do sensor.</p>";
      html += "<p><a href=\"/\">Voltar</a></p>";
      html += PAGINA_FIM;
      servidor.send(400, "text/html; charset=utf-8", html);
      return;
    }

    Preferences prefs;
    prefs.begin("fruitvale", false);
    prefs.putString("ssid", ssid);
    if (senhaWifi.length() || !_senhaWifi.length()) prefs.putString("wpass", senhaWifi);
    prefs.putString("user", usuario);
    if (senhaMqtt.length()) prefs.putString("mpass", senhaMqtt);
    prefs.end();

    String html = PAGINA_INICIO;
    html += "<h1>Configuração salva</h1>";
    html += "<p class=\"ok\">A placa vai reiniciar e conectar na rede <b>" + escapar(ssid) +
            "</b>. O LED pisca duas vezes quando o sensor estiver conectado à plataforma.</p>";
    html += "<p class=\"sub\">Se não conectar, desligue e ligue a placa, aperte o botão BOOT e "
            "confira os dados.</p>";
    html += PAGINA_FIM;
    servidor.send(200, "text/html; charset=utf-8", html);
    log("configuração salva; reiniciando");
    delay(1500);
    ESP.restart();
  });

  // celulares testam URLs próprias para detectar portal cativo: manda para a página
  servidor.onNotFound([&]() {
    servidor.sendHeader("Location", "http://" + ip.toString() + "/", true);
    servidor.send(302, "text/plain", "");
  });

  servidor.begin();
  const uint32_t inicio = millis();
  while (millis() - inicio < PORTAL_TIMEOUT_MS) {
    dns.processNextRequest();
    servidor.handleClient();
    // LED respirando devagar: está no modo de configuração
    acenderLed((millis() / 700) % 2);
    delay(2);
  }
  log("ninguém configurou em 10 minutos; reiniciando");
  ESP.restart();
}
