# Bike Computer — Project Roadmap

A standalone GPS cycling computer built from scratch on ESP32-S3.
Custom PCB, sensor fusion, offline map tiles, turn-by-turn navigation, automatic Strava upload.
No phone app. No cloud dependency. Full embedded system.

---

## Phase 1 — Sensor readout 
**Goal:** IMU + barometer live data with complementary filter

Hardware available from day one:
- ESP32 dev module
- MPU6050 (accelerometer + gyroscope)
- BMP280 (barometric pressure + temperature)

Tasks:
- [x] PlatformIO project setup
- [ ] I2C scan confirms both sensors (0x68, 0x76)
- [ ] MPU6050 raw accel + gyro readout
- [ ] BMP280 altitude + temperature readout
- [ ] Complementary filter for stable roll/pitch (alpha = 0.96)
- [ ] Clean JSON output on Serial at 100Hz
- [ ] Serial Plotter shows smooth stable graphs

**Unlock condition:** Heading and altitude stable on Serial Plotter after a short walk test outside
**Parts to order:** 1.8" TFT SPI display (ST7789, ~€6) + MicroSD SPI module (~€2)

---

## Phase 2 — Display + UI 
**Goal:** Garmin-style screen showing live ride data

Hardware added:
- 1.8" TFT SPI display (ST7789 / ST7735, 240x240)
- MicroSD SPI breakout

Tasks:
- [ ] TFT library setup (TFT_eSPI)
- [ ] Display shows speed, altitude, temperature in large readable font
- [ ] Heading shown as rotating arrow — smooth, not jittery
- [ ] UI layout: main metric large, secondary metrics small
- [ ] MicroSD mounts and read/write a test file
- [ ] Display refresh at stable 15fps

**Unlock condition:** UI looks clean, heading arrow rotates smoothly while turning the device
**Parts to order:** u-blox GPS module (LC86G or ATGM336H, ~€10)

---

## Phase 3 — GPS + Map tiles 
**Goal:** Live position dot on an OpenStreetMap tile

Hardware added:
- u-blox M10 / LC86G GPS module

Tasks:
- [ ] GPS NMEA parsing (TinyGPS++ library)
- [ ] Speed from GPS displayed (more accurate than IMU alone)
- [ ] GPS + IMU sensor fusion for smooth speed (simple Kalman filter)
- [ ] Download OSM raster tiles for Belgium at zoom level 15-16
- [ ] Store tiles on SD card as .bmp or .raw files
- [ ] Tile lookup from GPS coordinate
- [ ] Render correct tile on display
- [ ] Position dot moves correctly as you walk/ride

Note: This is the hardest phase technically. Budget 1.5-2 weeks.

**Unlock condition:** Position dot moves on a real map tile during an outdoor test
**Parts to order:** None — next phase is software only

---

## Phase 4 — Navigation + Strava 
**Goal:** Turn-by-turn navigation from GPX file + automatic Strava upload

Tasks:

Navigation:
- [ ] GPX file parser (reads route planned on Komoot / RideWithGPS)
- [ ] Waypoint distance + bearing calculations
- [ ] Turn detection (approaching waypoint within threshold)
- [ ] Turn arrow overlay on map tile (straight, left, right, U-turn)
- [ ] Distance to next turn displayed
- [ ] Route progress tracking

Strava integration:
- [ ] FIT file writer — logs GPS track, speed, altitude, cadence every second
- [ ] WiFi connection on return home
- [ ] OAuth2 token stored in ESP32 flash (one-time setup)
- [ ] One-time setup page hosted by ESP32 on first boot (captive portal)
- [ ] Automatic FIT file upload to Strava API on WiFi connect
- [ ] Activity appears in Strava with full track + metrics

**Unlock condition:** Complete a 2km test ride navigating only from the device, activity appears on Strava automatically afterward
**Parts to order:** PCB components — see Phase 5 BOM

---

## Phase 5 — Custom PCB 
**Goal:** All functionality on a single custom-designed board

Chip upgrade: ESP32 classic → ESP32-S3 (8MB PSRAM for map tile buffering)
IMU upgrade: MPU6050 → ICM-42688-P (what solar/EV teams actually use)

BOM:
| Component | Part | ~Cost |
|---|---|---|
| MCU | ESP32-S3 WROOM module | €4 |
| GPS | u-blox M10 SMD | €10 |
| IMU | ICM-42688-P | €5 |
| Baro | BMP390 | €3 |
| Display connector | FPC / pin header | €1 |
| Charger IC | BQ25895 | €3 |
| LiPo battery | 1000mAh | €6 |
| MicroSD socket | SPI | €1 |
| PCB (JLCPCB 4-layer) | 5x | €20 |
| Passives + misc | — | €10 |
| **Total** | | **~€63** |

Tasks:
- [ ] KiCad schematic from working devkit circuit
- [ ] Power tree: LiPo → BQ25895 → LDO → 3.3V rail
- [ ] GPS antenna keepout zone + impedance-controlled traces
- [ ] 4-layer stackup: signal / GND / PWR / signal
- [ ] Design review — decoupling caps, pull-ups, ESD protection
- [ ] Send to JLCPCB
- [ ] PCB assembly + bring-up
- [ ] Port all firmware to new hardware

**Unlock condition:** PCB boots, all sensors respond on I2C/SPI, display works
**Parts to order:** 3D printer filament for enclosure

---

## Phase 6 — Enclosure + Polish 
**Goal:** Finished device ready to ride

Tasks:
- [ ] 3D printed handlebar mount (Fusion 360 / FreeCAD)
- [ ] Weatherproofing (conformal coat or gasket)
- [ ] USB-C charging port
- [ ] Power button with sleep/wake
- [ ] Boot screen
- [ ] Low battery warning on display
- [ ] GitHub README with photos, architecture diagram, demo video
- [ ] Write-up: design decisions, challenges, what I'd do differently

---

## Budget tracker

| Phase | Spend | Cumulative |
|---|---|---|
| 1 — Sensors | €0 | €0 |
| 2 — Display | €12 | €12 |
| 3 — GPS | €18 | €30 |
| 4 — Navigation | €0 | €30 |
| 5 — PCB | €63 | €93 |
| 6 — Enclosure | €12 | €105 |

---

## Tech stack

| Layer | Technology |
|---|---|
| Firmware | C++ / Arduino framework / FreeRTOS |
| IDE | PlatformIO (VS Code) |
| IMU fusion | Complementary filter → Kalman filter |
| Map tiles | OpenStreetMap raster tiles (zoom 15-16) |
| Navigation | GPX parsing, waypoint bearing/distance |
| Activity logging | FIT file format (Garmin spec) |
| Cloud sync | Strava API v3 over WiFi (OAuth2) |
| PCB design | KiCad |
| Enclosure | FreeCAD / Fusion 360 → 3D print |

---

## Relevance to career goals

This project demonstrates:
- Custom PCB design (KiCad, 4-layer, power tree)
- Sensor fusion (IMU + GPS + barometer, Kalman filter)
- FreeRTOS multi-task firmware architecture
- Embedded power management (LiPo charging, sleep modes)
- RF considerations (GPS antenna design, BLE)
- Full product lifecycle: concept → prototype → PCB → enclosure
- Professional firmware structure (not Arduino hobby style)

All directly relevant to: Innoptus Solar Team electronics, Formula Electric Belgium, ASML, embedded systems roles.

---

*Started: March 2026*
*Target completion: May-June 2026***