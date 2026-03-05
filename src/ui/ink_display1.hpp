#pragma once
#include "sensors/imu_sensor.hpp"

class InkDisplay {
public:
    void initScreen();
    void screenRefresh(IMUSensor& imu);
    void drawIMUBox(IMUSensor& imu);
private:

};