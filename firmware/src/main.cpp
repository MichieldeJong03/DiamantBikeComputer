#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <Adafruit_BMP280.h>
#include "sensordata.h"

// ── Hardware ─────────────────────────────────────────────────
TFT_eSPI tft = TFT_eSPI();
Adafruit_BMP280 bmp;

// ── FreeRTOS ─────────────────────────────────────────────────
QueueHandle_t sensorQueue;

// ── Colours (RGB565) ─────────────────────────────────────────
#define CLR_BG       0x0820   // dark blue-grey background
#define CLR_PRIMARY  0xFFFF   // white
#define CLR_ACCENT   0x07E0   // green
#define CLR_MUTED    0x7BEF   // light grey
#define CLR_WARNING  0xFD20   // orange

// ── SensorTask ───────────────────────────────────────────────
void SensorTask(void *pvParameters) {
    if (!bmp.begin(0x76)) {
        Serial.println("[SensorTask] BMP280 not found");
        vTaskDelete(NULL);
        return;
    }
    bmp.setSampling(
        Adafruit_BMP280::MODE_NORMAL,
        Adafruit_BMP280::SAMPLING_X2,
        Adafruit_BMP280::SAMPLING_X16,
        Adafruit_BMP280::FILTER_X16,
        Adafruit_BMP280::STANDBY_MS_125
    );

    SensorData data = {};

    while (1) {
        data.temperature  = bmp.readTemperature();
        data.pressure     = bmp.readPressure() / 100.0F;
        data.altitude     = bmp.readAltitude(1013.25);
        data.bmp_valid    = true;
        data.timestamp    = millis();
        xQueueOverwrite(sensorQueue, &data);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// ── DisplayTask ──────────────────────────────────────────────
void DisplayTask(void *pvParameters) {
    tft.init();
    tft.setRotation(1);         // landscape
    tft.fillScreen(CLR_BG);

    // ── Static labels (drawn once) ──────────────────────────
    tft.setTextColor(CLR_MUTED, CLR_BG);
    tft.setTextSize(1);
    tft.drawString("ALTITUDE",  10, 10, 2);
    tft.drawString("TEMP",     170, 10, 2);
    tft.drawString("PRESSURE", 10, 110, 2);
    tft.drawString("DiamantBikeComputer v0.1", 10, 220, 1);

    // ── Divider lines ───────────────────────────────────────
    tft.drawFastHLine(0, 100, 320, CLR_MUTED);
    tft.drawFastHLine(0, 200, 320, CLR_MUTED);
    tft.drawFastVLine(160, 0,  100, CLR_MUTED);

    SensorData data;
    float last_alt  = -9999;
    float last_temp = -9999;
    float last_pres = -9999;

    while (1) {
        if (xQueuePeek(sensorQueue, &data, pdMS_TO_TICKS(500)) == pdTRUE) {

            // Only redraw if value changed significantly
            if (abs(data.altitude - last_alt) > 0.05) {
                tft.fillRect(10, 30, 140, 60, CLR_BG);
                tft.setTextColor(CLR_ACCENT, CLR_BG);
                tft.drawFloat(data.altitude, 1, 10, 35, 6);
                tft.setTextColor(CLR_MUTED, CLR_BG);
                tft.drawString("m", 115, 60, 2);
                last_alt = data.altitude;
            }

            if (abs(data.temperature - last_temp) > 0.05) {
                tft.fillRect(170, 30, 140, 60, CLR_BG);
                tft.setTextColor(CLR_WARNING, CLR_BG);
                tft.drawFloat(data.temperature, 1, 170, 35, 6);
                tft.setTextColor(CLR_MUTED, CLR_BG);
                tft.drawString("C", 290, 60, 2);
                last_temp = data.temperature;
            }

            if (abs(data.pressure - last_pres) > 0.1) {
                tft.fillRect(10, 125, 200, 60, CLR_BG);
                tft.setTextColor(CLR_PRIMARY, CLR_BG);
                tft.drawFloat(data.pressure, 1, 10, 130, 6);
                tft.setTextColor(CLR_MUTED, CLR_BG);
                tft.drawString("hPa", 190, 150, 2);
                last_pres = data.pressure;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(66));  // 15fps
    }
}

// ── Setup ─────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    Wire.begin(21, 22);
    delay(500);
    Serial.println("[BOOT] Starting...");

    sensorQueue = xQueueCreate(1, sizeof(SensorData));

    xTaskCreatePinnedToCore(SensorTask,  "SensorTask",  4096, NULL, 3, NULL, 1);
    xTaskCreatePinnedToCore(DisplayTask, "DisplayTask", 8192, NULL, 2, NULL, 1);
}

void loop() {
    vTaskDelay(pdMS_TO_TICKS(1000));
}