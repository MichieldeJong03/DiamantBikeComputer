#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_BMP280.h>
#include "sensordata.h"

// ── Hardware ────────────────────────────────────────────────
Adafruit_BMP280 bmp;

// ── FreeRTOS objects ────────────────────────────────────────
QueueHandle_t sensorQueue;

// ── Task: Sensor reading (Core 1, high priority) ────────────
void SensorTask(void *pvParameters) {
    Serial.println("[SensorTask] Started");

    // BMP280 init inside the task
    if (!bmp.begin(0x76)) {
        Serial.println("[SensorTask] BMP280 not found — halting");
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

    Serial.println("[SensorTask] BMP280 ready");

    SensorData data = {};  // zero-initialise

    while (1) {
        // Read BMP280
        data.temperature  = bmp.readTemperature();
        data.pressure     = bmp.readPressure() / 100.0F;
        data.altitude_raw = bmp.readAltitude(1013.25);
        data.altitude     = data.altitude_raw;  // filter added later
        data.bmp_valid    = true;
        data.timestamp    = millis();

        // Send to queue — overwrite if full (don't block)
        xQueueOverwrite(sensorQueue, &data);

        vTaskDelay(pdMS_TO_TICKS(10));  // 100Hz
    }
}

// ── Task: Display / Serial output (Core 1, medium priority) ─
void DisplayTask(void *pvParameters) {
    Serial.println("[DisplayTask] Started");

    SensorData data;

    while (1) {
        // Wait up to 500ms for new data
        if (xQueuePeek(sensorQueue, &data, pdMS_TO_TICKS(500)) == pdTRUE) {
            if (data.bmp_valid) {
                // Teleplot format
                Serial.print(">Temp:"); Serial.println(data.temperature);
                Serial.print(">Pressure:"); Serial.println(data.pressure);
                Serial.print(">Altitude:"); Serial.println(data.altitude);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(66));  // ~15Hz display refresh
    }
}

// ── Task: Logger (Core 1, low priority) ─────────────────────
void LogTask(void *pvParameters) {
    Serial.println("[LogTask] Started");

    SensorData data;
    uint32_t lastLog = 0;

    while (1) {
        if (xQueuePeek(sensorQueue, &data, pdMS_TO_TICKS(1000)) == pdTRUE) {
            uint32_t now = millis();
            if (now - lastLog >= 1000) {  // log once per second
                Serial.printf("[LOG] t=%lums | Temp=%.2f°C | Alt=%.1fm\n",
                    data.timestamp, data.temperature, data.altitude);
                lastLog = now;
                // SD card write goes here later
            }
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// ── Setup ────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    Wire.begin(21, 22);

    delay(500);
    Serial.println("\n[BOOT] DiamantBikeComputer starting...");

    // Create queue — holds 1 SensorData, overwrite mode
    sensorQueue = xQueueCreate(1, sizeof(SensorData));
    if (sensorQueue == NULL) {
        Serial.println("[BOOT] Queue creation failed — halting");
        while (1);
    }

    // Spawn tasks
    xTaskCreatePinnedToCore(SensorTask,  "SensorTask",  4096, NULL, 3, NULL, 1);
    xTaskCreatePinnedToCore(DisplayTask, "DisplayTask", 4096, NULL, 2, NULL, 1);
    xTaskCreatePinnedToCore(LogTask,     "LogTask",     4096, NULL, 1, NULL, 1);

    Serial.println("[BOOT] All tasks spawned");
}

// ── Loop is empty — FreeRTOS scheduler takes over ───────────
void loop() {
    vTaskDelay(pdMS_TO_TICKS(1000));  // idle — never put code here
}

/*
```

---

### Step 4 — Build, flash, observe

Upload and open Teleplot. You should see:
- `[BOOT]` messages on startup
- Each task announcing itself
- Teleplot graphs for Temp, Pressure, Altitude at 15Hz
- `[LOG]` lines every second in the Serial Monitor

If you open the Serial Monitor tab alongside Teleplot you'll see both simultaneously.

---

### Step 5 — Understanding what you just built
```
setup() creates queue + spawns 3 tasks → returns
FreeRTOS scheduler takes over
    SensorTask runs at 100Hz → writes to queue
    DisplayTask runs at 15Hz → reads queue → Teleplot
    LogTask runs at 1Hz → reads queue → Serial log
loop() does nothing — just sleeps
*/
