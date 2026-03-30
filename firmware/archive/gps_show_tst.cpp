#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <TFT_eSPI.h> // Zorg dat je User_Setup.h goed is ingesteld voor jouw pinnen!

// ── Hardware & Instellingen ─────────────────────────────────
TFT_eSPI tft = TFT_eSPI(); 
const int SD_CS_PIN = 5; // Verander dit naar jouw SD Chip Select pin
const int ZOOM_LEVEL = 15;

// Buffer voor 1 rij pixels (240 pixels max * 2 bytes = 480 bytes)
uint16_t rowBuffer[240];

// ── Wiskunde Hulpscripts ────────────────────────────────────
// Zet Longitude om naar de absolute X-pixel op de wereldkaart
double lonToGlobalPixelX(double lon, int zoom) {
    double n = pow(2.0, zoom);
    double tileX = (lon + 180.0) / 360.0 * n;
    return tileX * 256.0;
}

// Zet Latitude om naar de absolute Y-pixel op de wereldkaart
double latToGlobalPixelY(double lat, int zoom) {
    double n = pow(2.0, zoom);
    double latRad = lat * PI / 180.0;
    double tileY = (1.0 - log(tan(latRad) + 1.0 / cos(latRad)) / PI) / 2.0 * n;
    return tileY * 256.0;
}

// ── Teken Functie ───────────────────────────────────────────
void drawMapArea(double lat, double lon) {
    Serial.printf("\n[MAP] Berekenen voor Lat: %f, Lon: %f\n", lat, lon);

    // 1. Bereken de globale pixel in het midden van het scherm
    long globalCenterPx = (long)lonToGlobalPixelX(lon, ZOOM_LEVEL);
    long globalCenterPy = (long)latToGlobalPixelY(lat, ZOOM_LEVEL);

    // 2. Bereken de randen van het 240x320 scherm in de wereld
    long screenLeft = globalCenterPx - 120; // 240 / 2
    long screenTop  = globalCenterPy - 160; // 320 / 2
    long screenRight = screenLeft + 239;
    long screenBottom = screenTop + 319;

    // 3. Bereken welke map-tegels we nodig hebben
    int tileXStart = screenLeft / 256;
    int tileXEnd   = screenRight / 256;
    int tileYStart = screenTop / 256;
    int tileYEnd   = screenBottom / 256;

    // 4. Loop door alle benodigde tegels
    for (int ty = tileYStart; ty <= tileYEnd; ty++) {
        for (int tx = tileXStart; tx <= tileXEnd; tx++) {
            
            // Bouw de bestandsnaam: /tiles/15/tx/ty.bin
            char filepath[64];
            snprintf(filepath, sizeof(filepath), "/tiles/%d/%d/%d.bin", ZOOM_LEVEL, tx, ty);
            
            // Bepaal het overlap-gebied tussen deze tegel en ons scherm
            long tileLeft = tx * 256;
            long tileTop  = ty * 256;
            long tileRight = tileLeft + 255;
            long tileBottom = tileTop + 255;

            long drawLeft   = max(screenLeft, tileLeft);
            long drawRight  = min(screenRight, tileRight);
            long drawTop    = max(screenTop, tileTop);
            long drawBottom = min(screenBottom, tileBottom);

            int drawWidth  = drawRight - drawLeft + 1;
            int drawHeight = drawBottom - drawTop + 1;

            if (drawWidth > 0 && drawHeight > 0) {
                // Open bestand op SD kaart
                File tileFile = SD.open(filepath, FILE_READ);
                if (!tileFile) {
                    Serial.printf("[WAARSCHUWING] Tegel %s niet gevonden!\n", filepath);
                    // Vul het missende stuk met grijs
                    tft.fillRect(drawLeft - screenLeft, drawTop - screenTop, drawWidth, drawHeight, TFT_DARKGREY);
                    continue;
                }

                // Waar moeten we dit op het TFT tekenen?
                int screenXOffset = drawLeft - screenLeft;
                int screenYOffset = drawTop - screenTop;

                // Welk stukje uit het bestand hebben we nodig?
                int fileXOffset = drawLeft - tileLeft;
                int fileYOffset = drawTop - tileTop;

                // Lees de tegel rij voor rij
                for (int r = 0; r < drawHeight; r++) {
                    // Bereken positie in .bin bestand (2 bytes per pixel)
                    uint32_t seekPos = ((fileYOffset + r) * 256 + fileXOffset) * 2;
                    tileFile.seek(seekPos);
                    
                    // Lees exact het aantal bytes voor deze rij
                    tileFile.read((uint8_t*)rowBuffer, drawWidth * 2);

                    // Push naar het TFT scherm
                    // Let op de endianness: eSPI vereist soms swapBytes afhankelijk van hoe de Python code opgeslagen heeft
                    tft.pushImage(screenXOffset, screenYOffset + r, drawWidth, 1, rowBuffer);
                }
                tileFile.close();
            }
        }
    }
    
    // Teken een rood kruisje/stip in het midden om je locatie aan te geven
    tft.fillCircle(120, 160, 5, TFT_RED);
    Serial.println("[MAP] Render voltooid!");
}

// ── Setup ────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n[BOOT] Map Renderer Test Gestart");

    // Init TFT
    tft.begin();
    tft.setRotation(0); // 0 = Portrait (240x320)
    tft.fillScreen(TFT_BLACK);
    
    // Init SD (Let op: Zorg dat je pinnen kloppen voor jouw bord!)
    if (!SD.begin(SD_CS_PIN)) {
        Serial.println("[FOUT] SD Kaart niet gevonden of defect.");
        tft.drawString("SD Fout!", 10, 10, 4);
        while (1) delay(100);
    }
    Serial.println("[SD] SD Kaart OK");

    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("Wachten op GPS via Serial...", 10, 160, 2);

    Serial.println("\nTyp coördinaten in de Serial Monitor om te testen.");
    Serial.println("Formaat: LAT,LON (bijv: 50.8503,4.3517)");
}

// ── Loop (Wacht op Serial input) ─────────────────────────────
void loop() {
    if (Serial.available()) {
        String input = Serial.readStringUntil('\n');
        input.trim();
        
        int commaIndex = input.indexOf(',');
        if (commaIndex > 0) {
            String latStr = input.substring(0, commaIndex);
            String lonStr = input.substring(commaIndex + 1);
            
            double testLat = latStr.toDouble();
            double testLon = lonStr.toDouble();
            
            if (testLat != 0.0 && testLon != 0.0) {
                drawMapArea(testLat, testLon);
            }
        }
    }
}