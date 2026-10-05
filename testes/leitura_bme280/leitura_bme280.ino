/*
 * LEITURA DO BME280
 * -----------------
 * Le temperatura, umidade, pressao e altitude e imprime no Serial.
 * Sem display ainda. So para confirmar que os valores fazem sentido.
 *
 * BIBLIOTECAS (Gerenciador de Bibliotecas):
 *   - "Adafruit BME280 Library"
 *   - "Adafruit Unified Sensor"   (vem junto como dependencia)
 *
 * LIGACAO:
 *   VIN -> 3V3      GND -> GND
 *   SCL -> D22      SDA -> D21
 *
 * Serial Monitor a 115200.
 */

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

#define I2C_SDA 21
#define I2C_SCL 22

// Pressao ao nivel do mar, em hPa. Ajuste depois se quiser
// a altitude mais precisa para a sua regiao.
#define PRESSAO_NIVEL_MAR 1013.25f

Adafruit_BME280 bme;
bool sensorOk = false;

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println("\n\n=================================");
  Serial.println("  LEITURA DO BME280");
  Serial.println("=================================");

  Wire.begin(I2C_SDA, I2C_SCL);

  sensorOk = bme.begin(0x76);
  if (!sensorOk) {
    Serial.println("ERRO: sensor nao respondeu em 0x76.");
    return;
  }

  Serial.println("Sensor inicializado.\n");

  // Perfil recomendado pela Bosch para monitoramento
  // de clima: leitura sob demanda, consumo minimo.
  bme.setSampling(Adafruit_BME280::MODE_FORCED,
                  Adafruit_BME280::SAMPLING_X1,   // temperatura
                  Adafruit_BME280::SAMPLING_X1,   // pressao
                  Adafruit_BME280::SAMPLING_X1,   // umidade
                  Adafruit_BME280::FILTER_OFF);
}

void loop() {
  if (!sensorOk) {
    Serial.println("Sensor indisponivel.");
    delay(3000);
    return;
  }

  bme.takeForcedMeasurement();

  float temperatura = bme.readTemperature();
  float umidade     = bme.readHumidity();
  float pressao     = bme.readPressure() / 100.0F;
  float altitude    = bme.readAltitude(PRESSAO_NIVEL_MAR);

  Serial.print("Temperatura: ");
  Serial.print(temperatura, 1);
  Serial.println(" C");

  Serial.print("Umidade    : ");
  Serial.print(umidade, 1);
  Serial.println(" %");

  Serial.print("Pressao    : ");
  Serial.print(pressao, 1);
  Serial.println(" hPa");

  Serial.print("Altitude   : ");
  Serial.print(altitude, 0);
  Serial.println(" m");

  Serial.println("---");

  delay(2000);
}
