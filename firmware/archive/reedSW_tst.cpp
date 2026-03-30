#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>

#define REED_PIN 17

// Shared between ISR and task
volatile unsigned long lastPulseTime = 0;
SemaphoreHandle_t reedSemaphore;
QueueHandle_t speedQueue;

// ISR — keep it minimal
void IRAM_ATTR reedISR() {
  unsigned long now = millis();
  static unsigned long lastDebounce = 0;

  if (now - lastDebounce > 20) {
    lastDebounce = now;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(reedSemaphore, &xHigherPriorityTaskWoken);
    lastPulseTime = now;
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
  }
}

// Task 1: process reed pulses
void reedReaderTask(void *pvParameters) {
  unsigned long prevPulseTime = 0;
  const float wheelCircumference = 2.1; // meters — update when on bike

  while (true) {
    // Block until ISR signals a pulse
    if (xSemaphoreTake(reedSemaphore, pdMS_TO_TICKS(2000)) == pdTRUE) {
      unsigned long now = lastPulseTime;
      unsigned long interval = now - prevPulseTime;
      prevPulseTime = now;

      float speed_kmh = 0.0;
      if (interval > 0 && interval < 5000) { // sanity check
        speed_kmh = (wheelCircumference / (interval / 1000.0)) * 3.6;
      }

      xQueueOverwrite(speedQueue, &speed_kmh);

    } else {
      // Timeout = no pulse = speed is 0
      float speed_kmh = 0.0;
      xQueueOverwrite(speedQueue, &speed_kmh);
    }
  }
}

// Task 2: send to TelePlot
void telePlotTask(void *pvParameters) {
  float speed_kmh = 0.0;

  while (true) {
    if (xQueuePeek(speedQueue, &speed_kmh, pdMS_TO_TICKS(100)) == pdTRUE) {
      // TelePlot format: >varname:value\n
      Serial.print(">speed_kmh:");
      Serial.println(speed_kmh);
    }
    vTaskDelay(pdMS_TO_TICKS(100)); // 10 Hz update rate to TelePlot
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(REED_PIN, INPUT_PULLUP);

  reedSemaphore = xSemaphoreCreateBinary();
  speedQueue = xQueueCreate(1, sizeof(float)); // depth 1, always latest value

  attachInterrupt(digitalPinToInterrupt(REED_PIN), reedISR, FALLING);

  xTaskCreatePinnedToCore(reedReaderTask, "ReedReader", 2048, NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(telePlotTask,   "TelePlot",   2048, NULL, 1, NULL, 1);
}

void loop() {
  vTaskDelete(NULL); // loop() not used in RTOS setup
}