#pragma once
#include <M5UnitENV.h> // Defines BMP280 class

// ============================================================
// DATA STRUCTURES
// ============================================================

struct Baro {
    float pressure;  // Pa (Pascal)
    float cTemp;     // °C (Celsius)
    float altitude;  // m (meters)
};

struct AccelVector {
    float x, y, z;   // m/s² (acceleration)
};

struct GyroVector {
    float x, y, z;   // rad/s (angular velocity)
};

struct MagVector {
    float x, y, z;   // μT (microtesla) - magnetic field
};

// ============================================================
// INITIALIZATION STATUS
// ============================================================

enum class IMUInitStatus {
    SUCCESS = 0,
    BMP280_NOT_FOUND = 1,
    INVALID_STATE = 2,
    UNKNOWN_ERROR = 3
};

// ============================================================
// IMU SENSOR CLASS
// ============================================================

class IMUSensor {
public:
    // Initialize all sensors and configure settings
    // Returns status code indicating success/failure
    IMUInitStatus init();

    // Calibrate IMU (call after device is level and stable)
    void Calib();

    // Update sensor readings from hardware
    void update();

    // Accessors for sensor data (const references for efficiency)
    const AccelVector& getAccel() const { return accel; }
    const GyroVector& getGyro() const { return gyro; }
    const MagVector& getMag() const { return mag; }
    float getTemp() const { return temp; }

    // Get barometric data (pressure, temperature, altitude)
    Baro getBaro();

    // Calculate air density from environmental conditions
    // Formula: rho = P / (R_d * T_avg)
    // Returns air density in kg/m³
    float airDensityCalc();

    // Print sensor readings to serial console (for debugging)
    void printToSerial();

private:
    AccelVector accel{0, 0, 0};
    GyroVector gyro{0, 0, 0};
    MagVector mag{0, 0, 0};
    float temp = 0.0f;
    BMP280 bmp;
    bool isInitialized = false;

    // Gas constant for dry air (J/(kg·K))
    static constexpr float GAS_CONSTANT_DRY_AIR = 287.058f;

    // Helper: Convert temperature to Kelvin
    static float celsiusToKelvin(float celsius) {
        return celsius + 273.15f;
    }
};