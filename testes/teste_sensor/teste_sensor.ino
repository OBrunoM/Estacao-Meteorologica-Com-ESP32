/*
 * TESTE DO SENSOR - Scanner I2C + Identificacao do chip
 * ------------------------------------------------------
 * Faz duas coisas:
 *   1. Varre o barramento I2C e lista todos os enderecos que respondem
 *   2. Le o registrador de Chip ID (0xD0) e diz se e BME280 ou BMP280
 *
 * LIGACAO (modulo de 4 pinos):
 *   VIN -> 3V3
 *   GND -> GND
 *   SCL -> D22
 *   SDA -> D21
 *
 * Abra o Serial Monitor a 115200.
 */

#include <Wire.h>

#define I2C_SDA 21
#define I2C_SCL 22

#define REG_CHIP_ID 0xD0

// Chip IDs oficiais da Bosch
#define ID_BMP280   0x58
#define ID_BME280   0x60
#define ID_BMP180   0x55
#define ID_BME680   0x61

uint8_t lerRegistrador(uint8_t endereco, uint8_t reg) {
  Wire.beginTransmission(endereco);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return 0x00;

  Wire.requestFrom(endereco, (uint8_t)1);
  if (Wire.available()) return Wire.read();
  return 0x00;
}

void escanearBarramento() {
  Serial.println("\n[1] Escaneando barramento I2C...");

  int encontrados = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.print("    Dispositivo encontrado em 0x");
      if (addr < 16) Serial.print("0");
      Serial.println(addr, HEX);
      encontrados++;
    }
  }

  if (encontrados == 0) {
    Serial.println("    NENHUM dispositivo respondeu.");
    Serial.println("    Verifique: VIN em 3V3, GND, SDA no D21, SCL no D22.");
  } else {
    Serial.print("    Total: ");
    Serial.print(encontrados);
    Serial.println(" dispositivo(s).");
  }
}

void identificarChip(uint8_t endereco) {
  uint8_t id = lerRegistrador(endereco, REG_CHIP_ID);

  Serial.print("\n[2] Chip ID em 0x");
  Serial.print(endereco, HEX);
  Serial.print(" -> 0x");
  Serial.println(id, HEX);

  switch (id) {
    case ID_BME280:
      Serial.println("    >>> BME280 CONFIRMADO <<<");
      Serial.println("    Mede temperatura, pressao E UMIDADE.");
      Serial.println("    E o chip certo para o projeto.");
      break;
    case ID_BMP280:
      Serial.println("    >>> BMP280 <<<");
      Serial.println("    ATENCAO: este chip NAO mede umidade.");
      Serial.println("    Não é BME280 .");
      break;
    case ID_BME680:
      Serial.println("    >>> BME680 <<<");
      Serial.println("    Mede tudo do BME280 + qualidade do ar. Bonus!");
      break;
    case ID_BMP180:
      Serial.println("    >>> BMP180 <<< (sem umidade)");
      break;
    case 0x00:
      Serial.println("    Sem resposta valida. Comunicacao falhou.");
      break;
    default:
      Serial.println("    ID desconhecido. Pode ser um clone.");
      break;
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println("\n\n==========================================");
  Serial.println("  TESTE DO SENSOR - Scanner I2C");
  Serial.println("==========================================");

  Wire.begin(I2C_SDA, I2C_SCL);
  delay(100);
}

void loop() {
  escanearBarramento();

  // Testa os dois enderecos possiveis do BME/BMP280
  for (uint8_t addr : { 0x76, 0x77 }) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      identificarChip(addr);
    }
  }

  Serial.println("\n--- Repetindo em 5s ---");
  delay(5000);
}
