/*
 * DESCOBRIR O TAMANHO REAL DA MEMORIA DO DISPLAY
 * -----------------------------------------------
 * A biblioteca acha que a tela e 240x320. Se o controlador
 * tiver memoria maior, sobra uma faixa que nunca e pintada.
 *
 * Este teste pinta a memoria bruta em varios tamanhos e
 * marca as bordas, para vermos qual cobre a tela inteira.
 *
 * Fiacao normal do display. O sensor pode ficar conectado.
 * Serial Monitor a 115200.
 */

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>

#define TFT_CS   15
#define TFT_DC    2
#define TFT_RST   4

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);

// Pinta uma area da memoria do controlador ignorando os
// limites que a biblioteca acredita ter.
void pintarBruto(uint16_t largura, uint16_t altura, uint16_t cor) {
  tft.startWrite();
  tft.setAddrWindow(0, 0, largura, altura);
  for (uint32_t i = 0; i < (uint32_t)largura * altura; i++) {
    tft.SPI_WRITE16(cor);
  }
  tft.endWrite();
}

void etapa(const char *nome, uint16_t largura, uint16_t altura, uint16_t cor) {
  Serial.print("Pintando ");
  Serial.print(largura);
  Serial.print(" x ");
  Serial.print(altura);
  Serial.print("  -> ");
  Serial.println(nome);

  pintarBruto(largura, altura, cor);

  // Escreve o tamanho no canto para identificar na foto
  tft.setRotation(0);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.print(largura);
  tft.print("x");
  tft.print(altura);

  delay(4000);
}

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println("\n\n=======================================");
  Serial.println("  TAMANHO REAL DA MEMORIA DO DISPLAY");
  Serial.println("=======================================");
  Serial.println("Observe qual tamanho pinta a TELA INTEIRA,");
  Serial.println("sem sobrar nenhuma faixa.\n");

  tft.begin();
  tft.setRotation(0);
}

void loop() {
  // Cada etapa usa uma cor diferente para dar para
  // enxergar exatamente ate onde cada tamanho chega.
  etapa("padrao da biblioteca", 240, 320, ILI9341_BLUE);
  etapa("mais largo",           320, 320, ILI9341_GREEN);
  etapa("mais alto",            240, 400, ILI9341_RED);
  etapa("bem maior",            320, 480, ILI9341_MAGENTA);

  Serial.println("--- Reiniciando o ciclo ---\n");
  delay(2000);
}
