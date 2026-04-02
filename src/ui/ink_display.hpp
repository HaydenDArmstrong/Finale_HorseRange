#pragma once
#include "sensors/imu_sensor.hpp"
#include "utility/sdhandler.hpp"

class InkDisplay
{
public:
    void initScreen();
    void screenRefresh(IMUSensor &imu, SDHandler &sdhandle, float angle, float dartType, float distance, float* gauges, float* distances, int count);
    void drawAngle(IMUSensor &imu);
    void userInputStage(SDHandler &sdhandle, float &gauge, float &inputDistance, bool &isDistanceInputted);

private:
    void setTextStyle(uint8_t size);
};