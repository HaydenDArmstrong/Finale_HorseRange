#pragma once
#include "sensors/imu_sensor.hpp"
#include "utility/sdhandler.hpp"

class InkDisplay {
public:
    void initScreen();
    void screenRefresh(IMUSensor& imu, SDHandler& sdhandle, float angle, float mass, float distance);
    void drawIMUBox(IMUSensor& imu);
private:

};