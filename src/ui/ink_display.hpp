#pragma once
#include "sensors/imu_sensor.hpp"
#include "utility/sdhandler.hpp"

class InkDisplay {
public:
    void initScreen();
    void screenRefresh(IMUSensor& imu, SDHandler& sdhandle);
    void drawIMUBox(IMUSensor& imu);
private:

};