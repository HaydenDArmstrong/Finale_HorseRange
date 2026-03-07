#include "imu_sensor.hpp"
#include <M5Unified.h>
#include <M5UnitENV.h>


void IMUSensor::init() {
     // Try both common BMP280 addresses
    const uint8_t addresses[] = {0x76, 0x77};
    bool found = false;

    for (uint8_t addr : addresses) {
        if (bmp.begin(&Wire, addr, 25, 32, 400000U)) {
            found = true;
            Serial.print("BMP280 found at 0x");
            Serial.println(addr, HEX);
            break;
        }
    }

    if (!found) {
        Serial.println("Couldn't find BMP280 at 0x76 or 0x77!");
        while (1) delay(1);  // stop here
    }

    // Set default sampling as per datasheet
    bmp.setSampling(BMP280::MODE_NORMAL,     // Operating Mode
                    BMP280::SAMPLING_X2,     // Temp oversampling
                    BMP280::SAMPLING_X16,    // Pressure oversampling
                    BMP280::FILTER_X16,      // Filtering
                    BMP280::STANDBY_MS_500);// Standby time
}

void IMUSensor::update() {
    //update IMU Sensor variables
    M5.Imu.getAccel(&accel.x, &accel.y, &accel.z);
    M5.Imu.getGyro(&gyro.x, &gyro.y, &gyro.z);
    M5.Imu.getMag(&mag.x, &mag.y, &mag.z);
    M5.Imu.getTemp(&temp);
}

const AccelVector& IMUSensor::getAccel() const {
    return accel;
}

const GyroVector& IMUSensor::getGyro() const {
    return gyro;
}

const MagVector& IMUSensor::getMag() const {
    return mag;
}

float IMUSensor::getTemp() const {
    return temp;
}

void IMUSensor::printToSerial() {
  Serial.printf("ACC  %.3f  %.3f  %.3f\n", accel.x, accel.y, accel.z);
  Serial.printf("GYR  %.3f  %.3f  %.3f\n", gyro.x, gyro.y, gyro.z);
  Serial.printf("MAG  %.3f  %.3f  %.3f\n", mag.x, mag.y, mag.z);
  Serial.printf("TMP  %.2f C\n", temp);
  Serial.println("-------------------------");
}

Baro IMUSensor::getBaro() {
    bmp.update();  // update internal BMP280 values
    Baro data;
    data.pressure = bmp.pressure;      // Pa
    data.altitude = bmp.altitude;      // m
    data.cTemp     = bmp.cTemp;         // °C
    return data;
}


float IMUSensor::airDensityCalc(IMUSensor& imu) {
    //gas constant
    float R_d = 287.058;
    //pressure in Pa
    float P = imu.getBaro().pressure;

    //termperature in Kelvin
    float T1 = (imu.getBaro().cTemp + 273.15);

    float T2 = imu.getTemp() + 273.15;

    float Tavg = (T1 + T2) /2;

    float rho = P / (R_d * Tavg);

    return rho;

}