#pragma once
#include "sensors/imu_sensor.hpp"
#include "utility/sdhandler.hpp"

// ============================================================
// ENUMERATIONS
// ============================================================

enum class GunType {
    G2 = 0,
    Model389 = 1
};


// ============================================================
// INK DISPLAY CLASS
// ============================================================

class InkDisplay {
public:
    // Initialize display hardware and clear screen
    void initScreen();

    // Display ballistics calculation results
    void screenRefresh(
        IMUSensor& imu,
        SDHandler& sdhandler,
        float angle,
        float dartType,
        const float* gauges,
        const float* distances,
        int gaugeCount
    );

    // Configuration UI - user selects dart type and gun type
    void userInputStage(
        SDHandler& sdhandler,
        float& dartType,
        GunType& gunType,
        bool& configComplete
    );

    void showWarning(const char* message);
    void showError(const char* message);
    void drawAngle(IMUSensor& imu);



private:
    void setTextStyle(uint8_t size);
    const char* getGunTypeString(GunType gunType) const;

    static constexpr int HEADER_HEIGHT = 75;
    static constexpr int TABLE_START_Y = 90;
    static constexpr int TABLE_START_X = 250;
    static constexpr int COL_WIDTH = 240;

};