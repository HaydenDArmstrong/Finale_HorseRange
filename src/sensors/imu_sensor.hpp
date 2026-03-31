#pragma once
#include <M5UnitENV.h> // this defines BMP280 class

struct Baro
{
    float pressure;
    float cTemp;
    float altitude;
};

struct AccelVector
{
    // accelerometer vector
    float x;
    float y;
    float z;
};
struct GyroVector
{
    // gyro vector
    float x;
    float y;
    float z;
};
struct MagVector
{
    // magnetomter vector
    float x;
    float y;
    float z;
};
class IMUSensor
{
public:
    void Calib();
    void init();
    void update();
    // const vector& getvector(); -> return a reference to the original box/vector
    // vector getvector(); -> copy the data return value
    const AccelVector &getAccel() const;
    const GyroVector &getGyro() const;
    const MagVector &getMag() const;
    float getTemp() const;

    void printToSerial();
    float airDensityCalc(IMUSensor &imu);

    Baro getBaro();

private:
    AccelVector accel{0, 0, 0};
    GyroVector gyro{0, 0, 0};
    MagVector mag{0, 0, 0};
    // temp sensor
    float temp;
    BMP280 bmp;
};