#include "imu_sensor.hpp"
#include <M5Unified.h>
#include <M5UnitENV.h>

// ============================================================
// INITIALIZATION
// ============================================================

IMUInitStatus IMUSensor::init() {
    // Try to initialize BMP280 at common I2C addresses
    const uint8_t BMP280_ADDRESSES[] = {0x76, 0x77};
    bool found = false;

    for (uint8_t addr : BMP280_ADDRESSES) {
        if (bmp.begin(&Wire, addr, 25, 32, 400000U)) {
            found = true;
            Serial.printf("[IMU] BMP280 found at 0x%02X\n", addr);
            break;
        }
    }

    if (!found) {
        Serial.println("[IMU] ERROR: BMP280 not found at any known address!");
        Serial.println("[IMU] Checked: 0x76, 0x77");
        return IMUInitStatus::BMP280_NOT_FOUND;
    }

    // Configure BMP280 sampling parameters (from datasheet)
    bmp.setSampling(
        BMP280::MODE_NORMAL,      // Continuous measurement mode
        BMP280::SAMPLING_X2,      // Temperature oversampling ×2
        BMP280::SAMPLING_X16,     // Pressure oversampling ×16 (high precision)
        BMP280::FILTER_X16,       // IIR filter coefficient 16
        BMP280::STANDBY_MS_500    // 500ms standby between measurements
    );

    isInitialized = true;
    Serial.println("[IMU] BMP280 initialized successfully");
    return IMUInitStatus::SUCCESS;
}

// ============================================================
// CALIBRATION
// ============================================================

void IMUSensor::Calib() {
    // Set manual calibration offsets (placeholder values)
    // In a production system, you'd measure these values empirically
    M5.Imu.setCalibration(100, 100, 100);
    Serial.println("[IMU] Calibration offsets applied");
}

// ============================================================
// SENSOR DATA ACQUISITION
// ============================================================

void IMUSensor::update() {
    // Read current sensor values from hardware
    M5.Imu.getAccel(&accel.x, &accel.y, &accel.z);
    M5.Imu.getGyro(&gyro.x, &gyro.y, &gyro.z);
    M5.Imu.getMag(&mag.x, &mag.y, &mag.z);
    M5.Imu.getTemp(&temp);
}

// ============================================================
// DATA ACCESSORS (inline in .hpp for efficiency)
// ============================================================

Baro IMUSensor::getBaro() {
    // Trigger internal update of BMP280 measurements
    bmp.update();

    Baro data;
    data.pressure = bmp.pressure;  // Pa
    data.cTemp = bmp.cTemp;        // °C
    data.altitude = bmp.altitude;  // m

    return data;
}

// ============================================================
// THERMODYNAMICS CALCULATION
// ============================================================

float IMUSensor::airDensityCalc() {
    if (!isInitialized) {
        Serial.println("[IMU ERROR] airDensityCalc() called before initialized!");
        return 0.0f;
    }

    // Get barometric data
    Baro baro = getBaro();

    // Get IMU temperature
    float imuTemp = getTemp();

    // Calculate average temperature from two sources
    // T1: Barometric sensor (more stable)
    // T2: IMU sensor (faster response, potential thermal lag)
    float T1_Kelvin = celsiusToKelvin(baro.cTemp);
    float T2_Kelvin = celsiusToKelvin(imuTemp);
    float T_avg_Kelvin = (T1_Kelvin + T2_Kelvin) / 2.0f;

    // Calculate air density using ideal gas law
    // rho = P / (R_d * T)
    // where:
    //   P = pressure in Pa
    //   R_d = gas constant for dry air (287.058 J/(kg·K))
    //   T = temperature in Kelvin
    float rho = baro.pressure / (GAS_CONSTANT_DRY_AIR * T1_Kelvin);

    Serial.printf("[IMU] Density calc: P=%.0f Pa, T_baro=%.1f°C, T_imu=%.1f°C, rho=%.4f kg/m³\n",
                  baro.pressure, baro.cTemp, imuTemp, rho);

    return rho;
}

// ============================================================
// DEBUG OUTPUT
// ============================================================

void IMUSensor::printToSerial() {
    Serial.println("========== IMU SENSOR DATA ==========");
    Serial.printf("  Accel: X=%.3f  Y=%.3f  Z=%.3f (m/s²)\n", 
                  accel.x, accel.y, accel.z);
    Serial.printf("  Gyro:  X=%.3f  Y=%.3f  Z=%.3f (rad/s)\n", 
                  gyro.x, gyro.y, gyro.z);
    Serial.printf("  Mag:   X=%.3f  Y=%.3f  Z=%.3f (μT)\n", 
                  mag.x, mag.y, mag.z);

    Baro baro = getBaro();
    Serial.printf("  Baro:  Pressure=%.0f Pa, Temp=%.1f°C, Altitude=%.1f m\n",
                  baro.pressure, baro.cTemp, baro.altitude);
    Serial.printf("  IMU Temp: %.1f°C\n", temp);
    Serial.println("=====================================");
}