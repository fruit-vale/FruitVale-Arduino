# FruitVale para Arduino (ESP32)

Conecta placas **ESP32** (testado no **ESP32-C3 Super Mini**) à plataforma agrícola
**FruitVale**. A biblioteca cuida de tudo entre o sensor e a plataforma — Wi-Fi,
relógio (NTP), conexão MQTT com **TLS validado**, reconexão e o formato das
mensagens.

**Ela não depende de nenhum sensor específico.** Você lê o sensor que quiser, com
a biblioteca dele, e entrega os números com `fv.valor("chave", numero)`:

```cpp
#include <FruitVale.h>

FruitVale fv;

void setup() {
  Serial.begin(115200);
  // inicialize aqui o seu sensor (DHT, BME280, DS18B20, analógico...)
  fv.begin();            // Wi-Fi e credenciais vêm do portal de configuração
}

void loop() {
  fv.loop();
  if (fv.horaDeLer()) {  // a cada 60 s
    fv.valor("temperatura", lerTemperatura());   // qualquer função que devolva um número
    fv.valor("umidade_solo", lerUmidadeDoSolo());
    fv.enviar();
  }
}
```

## Instalação

Na Arduino IDE: **Ferramentas → Gerenciar Bibliotecas → "FruitVale"**. O
PubSubClient e o ArduinoJson são instalados junto.

Placa: instale o core **esp32 (Espressif)** e escolha **ESP32C3 Dev Module**, com
**USB CDC On Boot: Enabled** para ver as mensagens no Monitor Serial (115200).

## Primeiro uso: portal de configuração

1. No app FruitVale, cadastre o sensor (**Sensores → Cadastrar sensor**) com os
   dados que ele envia. A **chave no JSON** de cada dado é o nome que você usa em
   `fv.valor("chave", ...)`. Guarde o **usuário** (ex.: `emp1-estufa-01`) e a
   **senha** que o app mostra — a senha só aparece uma vez.
2. Grave o sketch e ligue a placa. Sem configuração, ela cria a rede
   **`FruitVale-XXXX`** (senha **`fruitvale`**).
3. Conecte o celular nessa rede; a página de configuração abre sozinha (se não
   abrir, acesse `http://192.168.4.1`). Preencha o Wi-Fi do local e o usuário e
   a senha do sensor e toque em **Salvar e conectar**.
4. A placa reinicia e conecta. O LED pisca **duas vezes** ao conectar à
   plataforma e **uma vez** a cada envio.

Os dados ficam salvos na flash e sobrevivem a regravações do firmware.

**Para reconfigurar:** ligue a placa e **aperte o botão BOOT nos primeiros
3 segundos** (o LED pisca rápido nessa janela). Aperte *depois* de ligar —
segurar o BOOT durante o reset coloca a placa em modo de gravação.

Uma queda do Wi-Fi **não** abre o portal: a placa continua tentando reconectar
sozinha, e reinicia se ficar 15 minutos sem falar com a plataforma.

## Usando o seu sensor

A FruitVale só transporta números: qualquer sensor que o ESP32 consiga ler serve.

1. **No app**, cadastre o sensor com um *dado* para cada grandeza que ele mede
   (nome, tipo, unidade). A **chave no JSON** é o nome que você usa no código.
2. **No sketch**, leia o sensor com a biblioteca do fabricante e passe cada
   valor com `fv.valor("chave", valor, casas)`. Chame `valor()` quantas vezes
   precisar (até 20 dados por mensagem) e depois `fv.enviar()`.
3. Uma leitura que falhar pode ser passada como `NAN`: ela fica de fora da
   mensagem e os outros dados seguem.

| Exemplo | Sensor | Bibliotecas extras |
|---|---|---|
| **Basico** | nenhum (temperatura do chip e sinal Wi-Fi) — bom para testar a conexão | — |
| **SensorAnalogico** | qualquer sensor de saída analógica (ex.: umidade do solo capacitivo) | — |
| **DHT22** | DHT22/DHT11 (temperatura e umidade do ar) | DHT sensor library, Adafruit Unified Sensor |
| **CredenciaisNoCodigo** | nenhum — mostra Wi-Fi e credenciais no código, sem portal | — |

Para outros sensores (BME280, DS18B20, SHT31, pluviômetro, pH...) o padrão é o
mesmo: inicialize no `setup()` e passe as leituras com `fv.valor()`.

## Referência

| Função | O que faz |
|---|---|
| `fv.begin()` | Inicia; abre o portal se não houver configuração ou se o BOOT for apertado. |
| `fv.loop()` | Chame em todo `loop()`: mantém Wi-Fi, relógio e MQTT conectados. |
| `fv.horaDeLer()` | `true` uma vez a cada intervalo (e logo ao conectar). |
| `fv.valor(chave, valor, casas = 2)` | Acrescenta um dado à mensagem; leitura inválida (NaN) é ignorada. |
| `fv.enviar()` | Publica os dados acumulados, com `data_hora`, e esvazia a mensagem. |
| `fv.conectado()` | Conectado à plataforma. |
| `fv.abrirPortal()` | Abre o portal agora (reinicia ao salvar). |
| `fv.apagarConfiguracao()` | Apaga Wi-Fi e credenciais salvos. |

Configuração opcional, **antes** de `begin()` (encadeável):

| Função | Padrão |
|---|---|
| `wifi(ssid, senha)` | do portal |
| `sensor(usuario, senha)` | do portal |
| `empresa(id)` | tirado do prefixo `empN-` do usuário |
| `intervalo(segundos)` | 60 |
| `host(host, porta)` | `mqtt.fruitvale.com.br`, 8883 |
| `led(pino, acendeEmLow)` | GPIO 8, `true` (Super Mini); `-1` desliga |
| `botaoConfiguracao(pino)` | GPIO 9 (BOOT); `-1` desliga |
| `senhaPortal(senha)` | `fruitvale` (mínimo 8 caracteres) |
| `potenciaWifi(potencia)` | `WIFI_POWER_8_5dBm` |

`wifi()` e `sensor()` no código têm prioridade sobre o que o portal salvou (veja o
exemplo **CredenciaisNoCodigo**).

## O que é enviado

Tópico `empresas/{empresa}/sensores/{usuario}/leituras`, com um JSON como:

```json
{"temperatura":25.3,"umidade":61.2,"data_hora":"2026-09-29T14:05:00Z"}
```

Só entram na plataforma as chaves cadastradas para o sensor no app; as demais
são ignoradas.

## Segurança

- A conexão usa TLS e **valida o certificado** do broker (raízes ISRG Root X1 e
  X2 do Let's Encrypt, embutidas na biblioteca).
- A rede do portal usa WPA2; troque a senha padrão com `senhaPortal()` se as
  placas ficarem em local de acesso público.
- Não publique sketches com senhas preenchidas.

## Solução de problemas

Mensagens no Monitor Serial (115200), prefixadas com `[FruitVale]`:

| Mensagem | Causa provável |
|---|---|
| `Wi-Fi não conectou` | Senha errada ou rede de 5 GHz (o ESP32-C3 só usa 2,4 GHz). |
| `falha de conexão TLS/TCP` | Relógio errado, certificado do broker inválido ou porta 8883 bloqueada. |
| `usuário ou senha incorretos` | Credenciais diferentes das do app (gere uma nova senha no app). |
| `não autorizado` | Sensor desativado no app. |

## Licença

MIT — veja [LICENSE](LICENSE).
