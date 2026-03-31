#include "ink_display.hpp"
#include <string.h>
#include <M5Unified.h>

void InkDisplay::initScreen()
{
    // make sure display is completely white to strart
    M5.Display.clear(WHITE);
    M5.Display.display();
    M5.Display.setRotation(1);       // landscape, makes coords correct
    M5.Display.setEpdMode(epd_fast); // reliable full refresh

    M5.Display.setTextSize(6);
    M5.Display.setTextColor(BLACK, WHITE); //  explicit fg/bg

    M5.Display.setFont(&fonts::DejaVu18);
    M5.Display.setTextSize(2);
    // M5.Display.setCursor(0, 10);
    M5.Display.display();
}

void InkDisplay::screenRefresh(IMUSensor &imu, SDHandler &sdhandle, float angle, float gauge, float distance)
{
    // primitive distance passed by value
    const AccelVector &a = imu.getAccel();
    const GyroVector &g = imu.getGyro();
    const MagVector &m = imu.getMag();
    Baro baro = imu.getBaro();

    float t = imu.getTemp();

    float rho = imu.airDensityCalc(imu);

    // Clear only text area
    M5.Display.clear(WHITE);
    setTextStyle(2);
    M5.Display.setCursor(0, 40);
    M5.Display.printf("Current Battery Status:\n %d %% %d mV \n", M5.Power.getBatteryLevel(), M5.Power.getBatteryVoltage());
    M5.Display.printf("SD Status: %s\n", sdhandle.getSDStatusStr());
    // M5.Display.println("Current Read:");
    //  M5.Display.printf("ACC  %.2fX  %.2fY  %.2fZ\n", a.x, a.y, a.z);
    //  M5.Display.printf("YDEG: %.2F\n", a.y*90);
    //  M5.Display.printf("GYR  %.2fX  %.2fY %.2f\nZ", g.x, g.y, g.z);
    //  M5.Display.printf("MAG  %.2fX  %.2fY  %.2f\nZ", m.x, m.y, m.z);
    //  M5.Display.printf("TMP  %.2f C\n", t);
    //  M5.Display.printf("ALT  %.1f m  P %.0f Pa\n", baro.altitude, baro.pressure);

    M5.Display.printf("Air Density: %.3f kg/m^3\n", rho);

    // return function result
    // if we do SDHandler::getSDStatusStr we return the address of the function

    M5.Display.printf("Current Angle  %.2f deg\n", angle);
    M5.Display.printf("Current Gauge %.2f MPa\n", gauge);

    setTextStyle(4);

    M5.Display.printf("Scope Distance: %.2f m\n", distance);
    M5.Display.display();
}

#include <math.h> // for atan2, sqrt, sin, cos

void InkDisplay::drawAngle(IMUSensor &imu)
{
    const AccelVector &a = imu.getAccel();
    const MagVector &m = imu.getMag();

    float angle = a.y*90.0f;
    float rad = angle * PI / 180.0f;

    const int cx     = 850;
    const int cy     = 60;
    const int length = 40;
    const int radius = 45;

    //clear intended area

    M5.Display.fillRect(cx - radius - 5, cy - radius - 5, (radius + 5) * 2, (radius +5) *2, WHITE);

    //outer ring

    M5.Display.drawCircle(cx, cy, radius, BLACK);

    int endX = cx + (int)(length * sin(rad));
    int endY = cy - (int)(length * cos(rad));

    M5.Display.drawLine(cx, cy, endX, endY, BLACK);

      M5.Display.fillCircle(cx, cy, 4, BLACK);

    // 5. Angle value as text underneath
    M5.Display.setCursor(cx - 20, cy + radius + 8);
    setTextStyle(1);
    M5.Display.printf("%.1f", angle);

    M5.Display.display();

}

void InkDisplay::userInputStage(SDHandler &sdhandle, float &gauge)
{

    const float MAX_GAUGE = 13.0;
    const float MIN_GAUGE = 2.0;

    const int y = 250;
    const int x = 80;

    bool changed = false;

    const int lineH = 50;
    // M5.Display.clear(WHITE);

    static bool firstRun = true;
    if (firstRun)
    {
        M5.Display.clear(WHITE);
        M5.Display.setCursor(0, 40);
        M5.Display.printf("Battery: %d%% %d mV\n", M5.Power.getBatteryLevel(), M5.Power.getBatteryVoltage());
        M5.Display.printf("LEFT = +0.5  RIGHT = -0.5\n");
        M5.Display.printf("Press DOWN when ready\n");
        // draw initial gauge value too
        M5.Display.fillRect(0, y, M5.Display.width(), lineH, WHITE);
        M5.Display.setCursor(0, y);
        M5.Display.printf("Gauge Value : %.1f\n", gauge);
        M5.Display.display();
        firstRun = false;
    }

    if (M5.BtnA.isPressed() && !M5.BtnA.isHolding())
    {

        gauge = gauge + 0.5;
        if (gauge > MAX_GAUGE)
        {
            gauge = MIN_GAUGE;
        }
        changed = true;
    }

    if (M5.BtnC.isPressed() && !M5.BtnC.isHolding())
    {

        gauge = gauge - 0.5;
        if (gauge < MIN_GAUGE)
        {
            gauge = MAX_GAUGE;
        }
        changed = true;
    }
    if (changed)
    {

        M5.Display.fillRect(0, y, M5.Display.width(), lineH, WHITE);
        M5.Display.setCursor(0, y);
        M5.Display.printf("Gauge Value : %.1f\n", gauge);
        M5.Display.display();
    }
}

void InkDisplay::setTextStyle(uint8_t size)
{
    switch (size)
    {
    case 1: // small
        M5.Display.setFont(&fonts::DejaVu24);
        break;
    case 2: // medium
        M5.Display.setFont(&fonts::DejaVu40);
        break;
    case 3: // large
        M5.Display.setFont(&fonts::DejaVu56);
        break;
    case 4: // XL
        M5.Display.setFont(&fonts::DejaVu72);
        break;
    }
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(BLACK, WHITE);
}