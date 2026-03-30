#ifndef SENSOR_DATA_H
#define SENSOR_DATA_H

#include <cstdint>

struct SensorData {
    // BMP280
    float temperature;      // °C
    float pressure;         // hPa
    float altitude;         // m above sea level
    float altitude_raw;     // unfiltered, for filter debugging
    
    // MPU6050 (populated later)
    float roll;             // degrees
    float pitch;            // degrees
    float heading;          // degrees (0-360)
    float accel_x;
    float accel_y;
    float accel_z;
    
    // GPS (populated later)
    float speed_gps;        // km/h
    float latitude;
    float longitude;
    float speed_wheel;      // km/h from reed switch
    float cadence;          // RPM
    
    // Fused (populated later)
    float speed_fused;      // km/h — best estimate
    float gradient;         // % grade
    float wattage;          // W estimate
    
    // Metadata
    uint32_t timestamp;     // millis()
    bool gps_valid;
    bool imu_valid;
    bool bmp_valid;
};

#endif