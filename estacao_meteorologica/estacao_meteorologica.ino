/*
 * ESTACAO METEOROLOGICA - ESP32 + BME280 + TFT 2.4" + WiFi
 * ---------------------------------------------------------
 * Mostra os valores no display e numa pagina web, agora com
 * historico das ultimas horas em graficos.
 *
 * O historico fica na memoria RAM: e perdido se a placa
 * reiniciar ou faltar energia.
 *
 * BIBLIOTECAS:
 *   - Adafruit BME280 Library
 *   - Adafruit ILI9341
 *   - Adafruit GFX Library
 *
 * LIGACAO DO SENSOR:
 *   VIN -> 3V3      GND -> GND
 *   SCL -> D22      SDA -> D21
 *
 * LIGACAO DO DISPLAY:
 *   VCC -> 3V3      LED -> 3V3      GND -> GND
 *   CS  -> D15      RESET -> D4     DC  -> D2
 *   SDI (MOSI) -> D23    SCK -> D18    SDO (MISO) -> D19
 */

#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>

enum Previsao { SOL, SOL_NUVEM, NUBLADO, CHUVA };

// ---------------- CONFIGURACAO ----------------
const char *WIFI_SSID = "SEU WIFI AQUI AMIGO";
const char *WIFI_SENHA = "SUA SENHA DO WIFI AQUI AMIGO";

#define I2C_SDA 21
#define I2C_SCL 22

#define TFT_CS   15
#define TFT_DC    2
#define TFT_RST   4

#define TELA_LARGURA 320
#define TELA_ALTURA  240

#define ALTITUDE_LOCAL 617.0f
#define TROCAR_RB false

#define INTERVALO_LEITURA   2000    // ms  - atualiza tela
#define INTERVALO_HISTORICO 60000   // ms  - grava um ponto no historico

#define MAX_HISTORICO 180           // 180 pontos x 1 min = 3 horas
// ----------------------------------------------

class TelaCorrigida : public Adafruit_ILI9341 {
public:
  TelaCorrigida(int8_t cs, int8_t dc, int8_t rst)
    : Adafruit_ILI9341(cs, dc, rst) {}

  void definirTamanhoReal(int16_t largura, int16_t altura) {
    _width  = largura;
    _height = altura;
  }

  void limparTudo(uint16_t cor) {
    startWrite();
    setAddrWindow(0, 0, 320, 320);
    for (uint32_t i = 0; i < 320UL * 320UL; i++) SPI_WRITE16(cor);
    endWrite();
  }
};

Adafruit_BME280 bme;
TelaCorrigida tft(TFT_CS, TFT_DC, TFT_RST);
WebServer servidor(80);

bool sensorOk = false;
unsigned long ultimaLeitura = 0;
unsigned long ultimoHistorico = 0;

float ultimaTemperatura = 0;
float ultimaUmidade     = 0;
float ultimaPressao     = 0;

// ---------- historico ----------

float histTemp[MAX_HISTORICO];
float histUmid[MAX_HISTORICO];
float histPres[MAX_HISTORICO];
int   histTotal = 0;

void gravarHistorico(float t, float u, float p) {
  if (histTotal == MAX_HISTORICO) {
    // Descarta o mais antigo, empurrando tudo uma posicao
    for (int i = 0; i < MAX_HISTORICO - 1; i++) {
      histTemp[i] = histTemp[i + 1];
      histUmid[i] = histUmid[i + 1];
      histPres[i] = histPres[i + 1];
    }
    histTotal = MAX_HISTORICO - 1;
  }

  histTemp[histTotal] = t;
  histUmid[histTotal] = u;
  histPres[histTotal] = p;
  histTotal++;
}

// ---------- cores ----------

uint16_t cor(uint8_t r, uint8_t g, uint8_t b) {
  if (TROCAR_RB) { uint8_t t = r; r = b; b = t; }
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

uint16_t COR_FUNDO, COR_CARTAO, COR_ROTULO;
uint16_t COR_TEMP, COR_UMID, COR_PRES;
uint16_t COR_SOL, COR_NUVEM, COR_CHUVA;

void definirCores() {
  COR_FUNDO  = cor(  6,  10,  16);
  COR_CARTAO = cor( 26,  34,  46);
  COR_ROTULO = cor(122, 136, 153);

  COR_TEMP = cor(255, 145,  50);
  COR_UMID = cor( 70, 200, 255);
  COR_PRES = cor(100, 230, 140);

  COR_SOL   = cor(255, 200,  60);
  COR_NUVEM = cor(210, 220, 232);
  COR_CHUVA = cor( 90, 160, 240);
}

// ---------- layout do display ----------

const int MARGEM = 8;
const int ESPACO = 8;
const int RODAPE = 16;
int larguraCartao, alturaCartao;
int col1X, col2X, lin1Y, lin2Y;

void calcularLayout() {
  larguraCartao = (tft.width()  - MARGEM * 2 - ESPACO) / 2;
  alturaCartao  = (tft.height() - MARGEM * 2 - ESPACO - RODAPE) / 2;
  col1X = MARGEM;
  col2X = MARGEM + larguraCartao + ESPACO;
  lin1Y = MARGEM;
  lin2Y = MARGEM + alturaCartao + ESPACO;
}

void desenharCartaoVazio(int x, int y, const char *rotulo) {
  tft.fillRoundRect(x, y, larguraCartao, alturaCartao, 6, COR_CARTAO);
  tft.setTextColor(COR_ROTULO);
  tft.setTextSize(1);
  tft.setCursor(x + 12, y + 12);
  tft.print(rotulo);
}

void escreverValor(int x, int y, const String &valor, const char *unidade, uint16_t corValor) {
  tft.fillRect(x + 8, y + 30, larguraCartao - 16, 44, COR_CARTAO);

  tft.setTextColor(corValor);
  tft.setTextSize(4);
  tft.setCursor(x + 16, y + 40);
  tft.print(valor);

  int larguraNumero = valor.length() * 24;
  tft.setTextSize(2);
  tft.setTextColor(COR_ROTULO);
  tft.setCursor(x + 22 + larguraNumero, y + 54);
  tft.print(unidade);
}

void escreverRodape(const String &texto) {
  int y = tft.height() - RODAPE + 2;
  tft.fillRect(0, y, tft.width(), RODAPE - 2, COR_FUNDO);
  tft.setTextColor(COR_ROTULO);
  tft.setTextSize(1);
  tft.setCursor(MARGEM + 4, y + 2);
  tft.print(texto);
}

// ---------- icones ----------

void desenharSol(int cx, int cy, int raio, uint16_t c) {
  tft.fillCircle(cx, cy, raio, c);
  for (int i = 0; i < 8; i++) {
    float ang = i * PI / 4.0;
    int x1 = cx + cos(ang) * (raio + 5);
    int y1 = cy + sin(ang) * (raio + 5);
    int x2 = cx + cos(ang) * (raio + 12);
    int y2 = cy + sin(ang) * (raio + 12);
    tft.drawLine(x1, y1, x2, y2, c);
    tft.drawLine(x1 + 1, y1, x2 + 1, y2, c);
  }
}

void desenharNuvem(int cx, int cy, int escala, uint16_t c) {
  tft.fillCircle(cx - escala, cy, escala, c);
  tft.fillCircle(cx, cy - escala / 2, escala * 3 / 4, c);
  tft.fillCircle(cx + escala, cy, escala * 4 / 5, c);
  tft.fillRect(cx - escala, cy, escala * 2, escala, c);
}

void desenharGotas(int cx, int cy, uint16_t c) {
  for (int i = -1; i <= 1; i++) {
    int x = cx + i * 14;
    tft.drawLine(x, cy, x - 4, cy + 12, c);
    tft.drawLine(x + 1, cy, x - 3, cy + 12, c);
  }
}

void desenharIcone(Previsao p) {
  int x = col1X, y = lin1Y;
  tft.fillRect(x + 6, y + 26, larguraCartao - 12, alturaCartao - 32, COR_CARTAO);

  int cx = x + larguraCartao / 2;
  int cy = y + alturaCartao / 2 + 6;

  switch (p) {
    case SOL:
      desenharSol(cx, cy, 22, COR_SOL);
      break;
    case SOL_NUVEM:
      desenharSol(cx - 20, cy - 12, 15, COR_SOL);
      desenharNuvem(cx + 10, cy + 10, 18, COR_NUVEM);
      break;
    case NUBLADO:
      desenharNuvem(cx - 12, cy - 8, 15, cor(150, 160, 175));
      desenharNuvem(cx + 10, cy + 8, 20, COR_NUVEM);
      break;
    case CHUVA:
      desenharNuvem(cx, cy - 10, 20, COR_NUVEM);
      desenharGotas(cx, cy + 14, COR_CHUVA);
      break;
  }
}

float pressaoAoNivelDoMar(float pressaoLocal) {
  return pressaoLocal / pow(1.0 - (ALTITUDE_LOCAL / 44330.0), 5.255);
}

Previsao preverTempo(float pressaoLocal, float umidade) {
  float pressaoMar = pressaoAoNivelDoMar(pressaoLocal);

  if (pressaoMar >= 1022) return SOL;
  if (pressaoMar >= 1012) return umidade > 75 ? SOL_NUVEM : SOL;
  if (pressaoMar >= 1005) return umidade > 80 ? CHUVA : SOL_NUVEM;
  return umidade > 70 ? CHUVA : NUBLADO;
}

const char *nomePrevisao(Previsao p) {
  switch (p) {
    case SOL:       return "Ensolarado";
    case SOL_NUVEM: return "Parcialmente nublado";
    case NUBLADO:   return "Nublado";
    case CHUVA:     return "Chuva provavel";
  }
  return "";
}

// ---------- grafico em SVG ----------

// Monta um grafico de linha a partir do historico.
// O eixo vertical se ajusta sozinho aos valores.
String montarGrafico(float *dados, int n, const char *corHex, int casas) {
  const int L = 600, A = 140;          // area do desenho
  const int MARG_E = 46, MARG_D = 8;
  const int MARG_T = 10, MARG_B = 18;

  if (n < 2) {
    return F("<div class='semdados'>Aguardando leituras para montar o grafico...</div>");
  }

  float minimo = dados[0], maximo = dados[0];
  for (int i = 1; i < n; i++) {
    if (dados[i] < minimo) minimo = dados[i];
    if (dados[i] > maximo) maximo = dados[i];
  }

  // Evita divisao por zero quando todos os valores sao iguais
  float faixa = maximo - minimo;
  if (faixa < 0.1) {
    minimo -= 0.5;
    maximo += 0.5;
    faixa = maximo - minimo;
  } else {
    minimo -= faixa * 0.12;
    maximo += faixa * 0.12;
    faixa = maximo - minimo;
  }

  int larguraUtil = L - MARG_E - MARG_D;
  int alturaUtil  = A - MARG_T - MARG_B;

  String pontos = "";
  for (int i = 0; i < n; i++) {
    float x = MARG_E + (larguraUtil * (float)i / (n - 1));
    float y = MARG_T + alturaUtil * (1.0 - (dados[i] - minimo) / faixa);
    pontos += String(x, 1) + "," + String(y, 1) + " ";
  }

  // Area preenchida sob a linha
  String area = String(MARG_E) + "," + String(MARG_T + alturaUtil) + " "
              + pontos
              + String(MARG_E + larguraUtil) + "," + String(MARG_T + alturaUtil);

  String svg = "<svg viewBox='0 0 " + String(L) + " " + String(A) + "' class='gr'>";

  // Linhas de grade horizontais com os rotulos
  for (int i = 0; i <= 2; i++) {
    float valor = maximo - (faixa * i / 2.0);
    int y = MARG_T + alturaUtil * i / 2;
    svg += "<line x1='" + String(MARG_E) + "' y1='" + String(y) +
           "' x2='" + String(MARG_E + larguraUtil) + "' y2='" + String(y) +
           "' class='grade'/>";
    svg += "<text x='" + String(MARG_E - 6) + "' y='" + String(y + 4) +
           "' class='eixo'>" + String(valor, casas) + "</text>";
  }

  svg += "<polygon points='" + area + "' fill='" + String(corHex) + "' opacity='0.16'/>";
  svg += "<polyline points='" + pontos + "' fill='none' stroke='" + String(corHex) +
         "' stroke-width='2.5' stroke-linejoin='round' stroke-linecap='round'/>";

  // Marca o valor mais recente
  float xUlt = MARG_E + larguraUtil;
  float yUlt = MARG_T + alturaUtil * (1.0 - (dados[n - 1] - minimo) / faixa);
  svg += "<circle cx='" + String(xUlt, 1) + "' cy='" + String(yUlt, 1) +
         "' r='4' fill='" + String(corHex) + "'/>";

  // Rotulo de tempo
  svg += "<text x='" + String(MARG_E) + "' y='" + String(A - 4) +
         "' class='eixo tempo'>" + String(n) + " min atras</text>";
  svg += "<text x='" + String(MARG_E + larguraUtil) + "' y='" + String(A - 4) +
         "' class='eixo tempo fim'>agora</text>";

  svg += "</svg>";
  return svg;
}

// ---------- pagina web ----------

String montarPagina() {
  Previsao p = preverTempo(ultimaPressao, ultimaUmidade);

  String html = F("<!DOCTYPE html><html lang='pt-BR'><head>"
    "<meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<meta http-equiv='refresh' content='30'>"
    "<title>Estacao Meteorologica</title><style>"
    "*{box-sizing:border-box;margin:0;padding:0}"
    "body{font-family:system-ui,-apple-system,sans-serif;background:#0a0f16;"
    "color:#e8edf2;min-height:100vh;padding:22px 16px;display:flex;"
    "flex-direction:column;align-items:center;gap:14px}"
    "h1{font-size:.78rem;font-weight:600;letter-spacing:.14em;"
    "text-transform:uppercase;color:#7a8899}"
    ".previsao{font-size:1.4rem;font-weight:600;color:#ffc83c}"
    ".painel{width:100%;max-width:620px;display:flex;flex-direction:column;gap:14px}"
    ".cartao{background:#1a222e;border:1px solid #26313f;border-radius:14px;padding:16px 18px}"
    ".topo{display:flex;align-items:baseline;justify-content:space-between}"
    ".rot{font-size:.72rem;letter-spacing:.1em;text-transform:uppercase;color:#7a8899}"
    ".val{font-size:2rem;font-weight:700;font-variant-numeric:tabular-nums;line-height:1}"
    ".un{font-size:.9rem;color:#7a8899;margin-left:4px;font-weight:400}"
    ".gr{width:100%;height:auto;margin-top:10px;display:block}"
    ".grade{stroke:#26313f;stroke-width:1}"
    ".eixo{fill:#5c6b7d;font-size:13px;text-anchor:end;font-family:system-ui,sans-serif}"
    ".tempo{text-anchor:start;font-size:12px}"
    ".fim{text-anchor:end}"
    ".semdados{color:#5c6b7d;font-size:.8rem;padding:26px 0;text-align:center}"
    "footer{font-size:.7rem;color:#4c5866;text-align:center;margin-top:4px}"
    "</style></head><body>"
    "<h1>Estacao Meteorologica</h1>");

  html += "<div class='previsao'>";
  html += nomePrevisao(p);
  html += "</div><div class='painel'>";

  auto bloco = [&](const char *rotulo, float valor, const char *unidade,
                   int casas, const char *corHex, float *dados) {
    html += "<div class='cartao'><div class='topo'><span class='rot'>";
    html += rotulo;
    html += "</span><span class='val' style='color:";
    html += corHex;
    html += "'>";
    html += String(valor, casas);
    html += "<span class='un'>";
    html += unidade;
    html += "</span></span></div>";
    html += montarGrafico(dados, histTotal, corHex, casas);
    html += "</div>";
  };

  bloco("Temperatura", ultimaTemperatura, "&deg;C", 1, "#ff9132", histTemp);
  bloco("Umidade",     ultimaUmidade,     "%",      1, "#46c8ff", histUmid);
  bloco("Pressao",     ultimaPressao,     "hPa",    1, "#64e68c", histPres);

  html += F("</div><footer>Historico de ate 3 horas, um ponto por minuto"
            "<br>A pagina se atualiza a cada 30 segundos</footer></body></html>");
  return html;
}

void tratarRaiz() {
  if (!sensorOk) {
    servidor.send(503, "text/plain", "Sensor indisponivel");
    return;
  }
  servidor.send(200, "text/html; charset=utf-8", montarPagina());
}

void conectarWiFi() {
  Serial.print("Conectando a ");
  Serial.println(WIFI_SSID);

  escreverRodape("Conectando ao WiFi...");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_SENHA);

  unsigned long inicio = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - inicio < 20000) {
    delay(400);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    String ip = WiFi.localIP().toString();
    Serial.print("Conectado. Acesse: http://");
    Serial.println(ip);
    escreverRodape("http://" + ip);
  } else {
    Serial.println("Falha ao conectar no WiFi.");
    escreverRodape("WiFi indisponivel");
  }
}

// ---------- estado ----------

String tempAnterior = "", umidAnterior = "", presAnterior = "";
Previsao previsaoAnterior = (Previsao)-1;

void desenharEstrutura() {
  tft.fillScreen(COR_FUNDO);
  desenharCartaoVazio(col1X, lin1Y, "PREVISAO");
  desenharCartaoVazio(col2X, lin1Y, "TEMPERATURA");
  desenharCartaoVazio(col1X, lin2Y, "PRESSAO");
  desenharCartaoVazio(col2X, lin2Y, "UMIDADE");
}

// ---------- setup / loop ----------

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n\nEstacao meteorologica iniciando...");

  definirCores();

  tft.begin();
  tft.setRotation(0);
  tft.limparTudo(COR_FUNDO);
  tft.definirTamanhoReal(TELA_LARGURA, TELA_ALTURA);

  calcularLayout();

  Wire.begin(I2C_SDA, I2C_SCL);
  sensorOk = bme.begin(0x76);

  if (!sensorOk) {
    Serial.println("ERRO: BME280 nao encontrado em 0x76.");
    tft.fillScreen(COR_FUNDO);
    tft.setTextColor(cor(255, 90, 90));
    tft.setTextSize(2);
    tft.setCursor(30, 110);
    tft.println("Sensor nao responde");
    return;
  }

  bme.setSampling(Adafruit_BME280::MODE_FORCED,
                  Adafruit_BME280::SAMPLING_X1,
                  Adafruit_BME280::SAMPLING_X1,
                  Adafruit_BME280::SAMPLING_X1,
                  Adafruit_BME280::FILTER_OFF);

  desenharEstrutura();
  conectarWiFi();

  servidor.on("/", tratarRaiz);
  servidor.onNotFound([]() { servidor.send(404, "text/plain", "Nao encontrado"); });
  servidor.begin();
  Serial.println("Servidor HTTP iniciado.");

  // Primeira leitura ja entra no historico
  bme.takeForcedMeasurement();
  ultimaTemperatura = bme.readTemperature();
  ultimaUmidade     = bme.readHumidity();
  ultimaPressao     = bme.readPressure() / 100.0F;
  gravarHistorico(ultimaTemperatura, ultimaUmidade, ultimaPressao);
  ultimoHistorico = millis();
}

void loop() {
  servidor.handleClient();

  if (!sensorOk) return;
  if (millis() - ultimaLeitura < INTERVALO_LEITURA) return;
  ultimaLeitura = millis();

  bme.takeForcedMeasurement();

  ultimaTemperatura = bme.readTemperature();
  ultimaUmidade     = bme.readHumidity();
  ultimaPressao     = bme.readPressure() / 100.0F;

  if (millis() - ultimoHistorico >= INTERVALO_HISTORICO) {
    gravarHistorico(ultimaTemperatura, ultimaUmidade, ultimaPressao);
    ultimoHistorico = millis();
    Serial.print("[historico] ");
    Serial.print(histTotal);
    Serial.println(" pontos");
  }

  String textoTemp = String(ultimaTemperatura, 1);
  String textoUmid = String(ultimaUmidade, 1);
  String textoPres = String(ultimaPressao, 0);

  if (textoTemp != tempAnterior) {
    escreverValor(col2X, lin1Y, textoTemp, "C", COR_TEMP);
    tempAnterior = textoTemp;
  }
  if (textoPres != presAnterior) {
    escreverValor(col1X, lin2Y, textoPres, "hPa", COR_PRES);
    presAnterior = textoPres;
  }
  if (textoUmid != umidAnterior) {
    escreverValor(col2X, lin2Y, textoUmid, "%", COR_UMID);
    umidAnterior = textoUmid;
  }

  Previsao p = preverTempo(ultimaPressao, ultimaUmidade);
  if (p != previsaoAnterior) {
    desenharIcone(p);
    previsaoAnterior = p;
  }
}
