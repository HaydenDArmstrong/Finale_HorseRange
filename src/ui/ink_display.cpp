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

void InkDisplay::screenRefresh(IMUSensor &imu, SDHandler &sdhandle, float angle, float dartType, float distance, float* gauges, float* distances, int count)
{
    float rho = imu.airDensityCalc(imu);

    M5.Display.clear(WHITE);
    setTextStyle(1);
    M5.Display.setCursor(0, 0);
    M5.Display.printf("Battery: %d%%  SD: %s\n", M5.Power.getBatteryLevel(), sdhandle.getSDStatusStr());
    M5.Display.printf("Density: %.3f kg/m^3\n", rho);
    M5.Display.printf("Angle: %.2f deg  Dart: %.2fCC\n", angle, dartType);
    M5.Display.printf("Distance: %.2f Ft\n", distance);

    // --- Table layout ---
    int tableX     = 0;
    int tableY     = 80;  // start below the info text
    int colWidth   = 120;
    int rowHeight  = 20;
    int cols       = 2;
    int rows       = count + 1; // +1 for header row

    // Draw header background
    M5.Display.fillRect(tableX, tableY, colWidth * cols, rowHeight, BLACK);

    // Header text
    M5.Display.setTextColor(WHITE);
    setTextStyle(3);
    M5.Display.printf("Gauge (MPa)");
    M5.Display.printf("Distance (Ft)");
    M5.Display.setTextColor(BLACK);

    // Draw each data row
    for (int i = 0; i < count; i++)
    {
        // Cell text;
        M5.Display.printf("%.2f", gauges[i]);
        M5.Display.printf("%.2f", distances[i]);

    }


    M5.Display.display();
}
#include <math.h> // for atan2, sqrt, sin, cos

void InkDisplay::drawAngle(IMUSensor &imu)
{
    const AccelVector &a = imu.getAccel();
    const MagVector &m = imu.getMag();

    float angle = a.y * 90.0f;
    float rad = angle * PI / 180.0f;

    const int cx = 850;
    const int cy = 60;
    const int length = 40;
    const int radius = 45;

    // clear intended area

    M5.Display.fillRect(cx - radius - 5, cy - radius - 5, (radius + 5) * 2, (radius + 5) * 2, WHITE);

    // outer ring

    M5.Display.drawCircle(cx, cy, radius, BLACK);

    int endX = cx + (int)(length * sin(rad));
    int endY = cy - (int)(length * cos(rad));

    M5.Display.drawLine(cx, cy, endX, endY, BLACK);

    M5.Display.fillCircle(cx, cy, 4, BLACK);

    // 5. Angle value as text underneath
    M5.Display.setCursor(cx - 20, cy + radius + 8);
    setTextStyle(1);
    M5.Display.printf("%.1f deg", angle);

    M5.Display.display();
}

void InkDisplay::userInputStage(SDHandler &sdhandle, float &dartType, float &inputDist, bool &isDistanceInputted)
{

    const int MAX_dartType = 2;
    const int MIN_dartType = 1;

    const int MAX_DIST = 150;
    const int MIN_DIST = 10;

    const int y = 250;
    const int x = 80;

    bool changed = false;

    const int lineH = 50;
    // M5.Display.clear(WHITE);

    static bool firstRun = true;
    static bool dartTypeSelected = false;

    if (firstRun)
    {
        M5.Display.clear(WHITE);
        M5.Display.setCursor(0, 40);
        M5.Display.printf("Battery: %d%% %d mV\n", M5.Power.getBatteryLevel(), M5.Power.getBatteryVoltage());
        M5.Display.printf("LEFT = 2cc  RIGHT = 1cc\n");
        M5.Display.printf("Press DOWN when ready\n");
        // draw initial dartType value too
        // M5.Display.fillRect(0, y, M5.Display.width(), lineH, WHITE);
        M5.Display.setCursor(0, y);
        M5.Display.printf("Dart Type : %.1f  CC\n", dartType);
        M5.Display.printf("Distance : %.1f  Ft\n", inputDist);
        M5.Display.display();
        firstRun = false;
    }
    static int stage = 0; // persists between calls

    if (stage == 0)
    {
        // dart type selection
        if (M5.BtnA.wasPressed())
        {
            dartType += 0.5f;
            if (dartType > MAX_dartType)
            {
                dartType = MIN_dartType;
            }
            changed = true;
        }
        if (M5.BtnC.wasPressed())
        {
            dartType -= 0.5f;
            if (dartType < MIN_dartType)
            {
                dartType = MAX_dartType;
            }
            changed = true;
        }
        if (M5.BtnB.wasPressed() && !M5.BtnB.isHolding())
        {
            stage = 1;
            changed = true;
        } // advance
    }
    else if (stage == 1)
    {
        // distance selection

        if (M5.BtnA.wasPressed())
        {
            inputDist += 5.0f;
            if (inputDist > MAX_DIST)
            {
                inputDist = MIN_DIST;
            }
            changed = true;
        }
        if (M5.BtnC.wasPressed())
        {
            inputDist -= 5.0f;
            if (inputDist < MIN_DIST)
            {
                inputDist = MAX_DIST;
            }
            changed = true;
        }
        if (M5.BtnB.wasPressed() && !M5.BtnB.isHolding())
        {
            isDistanceInputted = true;
            changed = true;
        }
    }

    if (changed)
    {

        // M5.Display.fillRect(0, y, M5.Display.width(), lineH, WHITE);
        // M5.Display.fillRect(0, y-20, M5.Display.width(), lineH, WHITE);
        M5.Display.setCursor(0, y);
        M5.Display.printf("Dart Type : %.1f  CC\n", dartType);
        M5.Display.printf("Distance : %.1f  Ft\n", inputDist);
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