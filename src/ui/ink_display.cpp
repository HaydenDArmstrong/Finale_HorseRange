#include "ink_display.hpp"
#include <string.h>
#include <M5Unified.h>
#include <math.h>

// ============================================================
// INPUT HELPERS (shared for buttons + touch)
// ============================================================

void applyStep(float &value, float step, float min, float max, int dir)
{
    value += dir * step;

    if (value > max)
        value = min; // wrap
    if (value < min)
        value = max; // wrap
}

int getDirection()
{
    auto touch = M5.Touch.getDetail();

    if (M5.BtnA.wasPressed())
        return +1;
    if (M5.BtnC.wasPressed())
        return -1;

    if (touch.wasPressed())
    {
        return (touch.x < M5.Display.width() / 2) ? -1 : +1;
    }

    return 0;
}

// ============================================================
// INITIALIZATION
// ============================================================

void InkDisplay::initScreen()
{
    M5.Display.clear(WHITE);
    M5.Display.display();

    M5.Display.setRotation(1);
    M5.Display.setEpdMode(epd_fast);

    M5.Display.setFont(&fonts::DejaVu24);
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(BLACK, WHITE);

    M5.Display.display();
    Serial.println("[DISPLAY] Screen initialized");
}

// ============================================================
// BALLISTICS DISPLAY
// ============================================================

void InkDisplay::screenRefresh(
    IMUSensor &imu,
    SDHandler &sdhandler,
    float angle,
    float dartType,
    const float *gauges,
    const float *distances,
    int gaugeCount)
{

    // ============================================================
    // SAFETY CHECKS
    // ============================================================
    if (!gauges || !distances || gaugeCount < 0)
    {
        showError("Invalid gauge/distance data");
        return;
    }

    // ============================================================
    // SENSOR COMPUTATION
    // ============================================================
    // Calculate air density from IMU/environment sensor
    float rho = imu.airDensityCalc();

    // ============================================================
    // SCREEN RESET
    // ============================================================
    M5.Display.clear(WHITE);
    setTextStyle(1); // small font for header

    // ============================================================
    // HEADER INFO SECTION
    // ============================================================
    M5.Display.setCursor(0, 0);
    M5.Display.printf("Batt: %d%%  SD: %s",
                      M5.Power.getBatteryLevel(),
                      sdhandler.getSDStatusStr());

    M5.Display.setCursor(0, 25);
    M5.Display.printf("Density: %.3f kg/m^3  Angle: %.1fdeg",
                      rho,
                      angle);

    M5.Display.setCursor(0, 50);
    M5.Display.printf("Dart: %.1f CC", dartType);

    // ============================================================
    // TABLE LAYOUT SETUP
    // ============================================================
    const int tableStartY = TABLE_START_Y;
    const int tableStartX = TABLE_START_X;
    const int colWidth = COL_WIDTH;

    const int screenHeight = M5.Display.height();
    const int availableHeight = screenHeight - tableStartY - 20;

    // Compute row height dynamically based on number of entries
    int rowHeight = (availableHeight / (gaugeCount + 1));

    // Minimum row height guard
    rowHeight = (rowHeight < 20) ? 20 : rowHeight;

    const int tableWidth = colWidth * 2;
    const int tableHeight = (gaugeCount + 1) * rowHeight;

    Serial.printf("[DISPLAY] Screen %dpx | Available %dpx | %d rows × %dpx height\n",
                  screenHeight,
                  availableHeight,
                  gaugeCount,
                  rowHeight);

    // ============================================================
    // TABLE BORDER
    // ============================================================
    M5.Display.drawRect(tableStartX, tableStartY, tableWidth, tableHeight, BLACK);

    // ============================================================
    // TABLE HEADER ROW
    // ============================================================
    M5.Display.fillRect(tableStartX, tableStartY, tableWidth, rowHeight, BLACK);
    M5.Display.setTextColor(WHITE, BLACK);
    setTextStyle(1);

    M5.Display.setCursor(tableStartX + 10, tableStartY + (rowHeight / 2) - 5);
    M5.Display.printf("Gauge (MPa)");

    M5.Display.setCursor(tableStartX + colWidth + 10, tableStartY + (rowHeight / 2) - 5);
    M5.Display.printf("Distance (Ft)");

    // ============================================================
    // DATA ROWS
    // ============================================================
    M5.Display.setTextColor(BLACK, WHITE);

    for (int i = 0; i < gaugeCount; i++)
    {

        const int rowY = tableStartY + (i + 1) * rowHeight;

        // Row separator
        M5.Display.drawLine(
            tableStartX,
            rowY,
            tableStartX + tableWidth,
            rowY,
            BLACK);

        // Column separator
        M5.Display.drawLine(
            tableStartX + colWidth,
            tableStartY + (i + 1) * rowHeight - rowHeight,
            tableStartX + colWidth,
            rowY,
            BLACK);

        // Left column: gauge values
        M5.Display.setCursor(tableStartX + 10, rowY + (rowHeight / 2) - 5);
        M5.Display.printf("%.2f", gauges[i]);

        // Right column: distance values
        M5.Display.setCursor(tableStartX + colWidth + 10, rowY + (rowHeight / 2) - 5);
        M5.Display.printf("%.2f", distances[i]);
    }

    // ============================================================
    // BOTTOM BORDER
    // ============================================================
    M5.Display.drawLine(
        tableStartX,
        tableStartY + tableHeight,
        tableStartX + tableWidth,
        tableStartY + tableHeight,
        BLACK);

    // ============================================================
    // PUSH TO DISPLAY
    // ============================================================
    M5.Display.display();
}
// ============================================================
// CONFIGURATION INPUT STAGE
// ============================================================

void InkDisplay::userInputStage(
    SDHandler &sdhandler,
    float &dartType,
    GunType &gunType,
    bool &configComplete)
{

    const float MAX_DART_TYPE = 2.0f;
    const float MIN_DART_TYPE = 1.0f;

    const int Y_POS = 250;

    static bool firstRun = true;
    static int stage = 0;

    bool changed = false;

    // =========================
    // INITIAL SCREEN DRAW
    // =========================
    if (firstRun)
    {
        M5.Display.clear(WHITE);
        setTextStyle(1);

        M5.Display.setCursor(0, 40);
        M5.Display.printf("Battery: %d%% (%d mV)",
                          M5.Power.getBatteryLevel(),
                          M5.Power.getBatteryVoltage());

        M5.Display.setCursor(0, 80);
        M5.Display.printf("LEFT/RIGHT or TOUCH = change");
        M5.Display.setCursor(0, 110);
        M5.Display.printf("DOWN = confirm");

        M5.Display.display();
        firstRun = false;
    }

    // =========================
    // INPUT HANDLING (UNIFIED)
    // =========================
    int dir = getDirection();

    if (stage == 0)
    {
        if (dir != 0)
        {
            applyStep(dartType, 0.5f, MIN_DART_TYPE, MAX_DART_TYPE, dir);
            changed = true;
        }

        if (M5.BtnB.wasPressed() && !M5.BtnB.isHolding())
        {
            stage = 1;
            changed = true;
        }
    }
    else if (stage == 1)
    {
        if (dir != 0)
        {
            gunType = (gunType == GunType::G2)
                          ? GunType::Model389
                          : GunType::G2;
            changed = true;
        }

        if (M5.BtnB.wasPressed() && !M5.BtnB.isHolding())
        {
            configComplete = true;
            stage = 0;
            changed = true;
        }
    }

    // =========================
    // DISPLAY UPDATE
    // =========================
    if (changed)
    {
        M5.Display.fillRect(0, Y_POS, M5.Display.width(), 100, WHITE);

        M5.Display.setCursor(0, Y_POS);
        setTextStyle(1);

        M5.Display.printf("Dart Type : %.1f CC\n", dartType);
        M5.Display.printf("Gun Type  : %s\n", getGunTypeString(gunType));
        M5.Display.printf("Stage     : %s\n",
                          (stage == 0) ? "Dart selection" : "Gun selection");

        M5.Display.display();
    }
}

// ============================================================
// ERROR / WARNING
// ============================================================

void InkDisplay::showWarning(const char *message)
{
    if (!message)
        return;

    M5.Display.clear(WHITE);
    setTextStyle(2);

    M5.Display.setCursor(20, 100);
    M5.Display.printf("WARNING");

    setTextStyle(1);
    M5.Display.setCursor(20, 160);
    M5.Display.printf(message);

    M5.Display.display();
    delay(2000);
}

void InkDisplay::showError(const char *message)
{
    if (!message)
        return;

    M5.Display.clear(WHITE);
    setTextStyle(2);

    M5.Display.setCursor(20, 100);
    M5.Display.printf("ERROR");

    setTextStyle(1);
    M5.Display.setCursor(20, 160);
    M5.Display.printf(message);

    M5.Display.display();
    delay(3000);
}

// ============================================================
// ANGLE VISUALIZATION
// ============================================================

void InkDisplay::drawAngle(IMUSensor &imu)
{
    const AccelVector &a = imu.getAccel();

    float angle = a.y * 90.0f;
    float rad = angle * M_PI / 180.0f;

    const int cx = 850;
    const int cy = 60;
    const int length = 40;
    const int radius = 45;

    M5.Display.fillRect(cx - 50, cy - 50, 120, 120, WHITE);
    M5.Display.drawCircle(cx, cy, radius, BLACK);

    int endX = cx + (int)(length * sin(rad));
    int endY = cy - (int)(length * cos(rad));

    M5.Display.drawLine(cx, cy, endX, endY, BLACK);
    M5.Display.fillCircle(cx, cy, 4, BLACK);

    M5.Display.setCursor(cx - 20, cy + radius + 8);
    setTextStyle(1);
    M5.Display.printf("%.1f deg", angle);

    M5.Display.display();
}

// ============================================================
// HELPERS
// ============================================================

void InkDisplay::setTextStyle(uint8_t size)
{
    switch (size)
    {
    case 1:
        M5.Display.setFont(&fonts::DejaVu24);
        break;
    case 2:
        M5.Display.setFont(&fonts::DejaVu40);
        break;
    case 3:
        M5.Display.setFont(&fonts::DejaVu56);
        break;
    case 4:
        M5.Display.setFont(&fonts::DejaVu72);
        break;
    default:
        M5.Display.setFont(&fonts::DejaVu24);
        break;
    }

    M5.Display.setTextSize(1);
    M5.Display.setTextColor(BLACK, WHITE);
}

const char *InkDisplay::getGunTypeString(GunType gunType) const
{
    switch (gunType)
    {
    case GunType::G2:
        return "G2";
    case GunType::Model389:
        return "Model389";
    default:
        return "UNKNOWN";
    }
}