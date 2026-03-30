#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_BMP280.h>

Adafruit_BMP280 bmp;

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);

  if (!bmp.begin(0x76)) {
    Serial.println("BMP280 not found, check wiring");
    while (1);
  }
  else {
    Serial.println("BMP280 found");
  }

  bmp.setSampling(
    Adafruit_BMP280::MODE_NORMAL,
    Adafruit_BMP280::SAMPLING_X2,   // temperature
    Adafruit_BMP280::SAMPLING_X16,  // pressure
    Adafruit_BMP280::FILTER_X16,
    Adafruit_BMP280::STANDBY_MS_500
  );

  Serial.println("BMP280 ready");
}

void loop() {
  float temperature = bmp.readTemperature();
  float pressure    = bmp.readPressure() / 100.0F;
  float altitude    = bmp.readAltitude(1013.25);

  Serial.print(">Temp:"); Serial.println(temperature);
  Serial.print(">Pressure:"); Serial.println(pressure);
  Serial.print(">Altitude:"); Serial.println(altitude);

  delay(500);
}
