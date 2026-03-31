#pragma once
#include "sensors/imu_sensor.hpp"
#include "utility/sdhandler.hpp"

class InkDisplay
{
public:
    void initScreen();
    void screenRefresh(IMUSensor &imu, SDHandler &sdhandle, float angle, float mass, float distance);
    void drawIMUBox(IMUSensor &imu);
    void userInputStage(SDHandler &sdhandle, float &gauge);

private:
    void setTextStyle(uint8_t size);
};