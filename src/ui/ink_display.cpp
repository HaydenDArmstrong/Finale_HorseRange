#include "ink_display.hpp"
#include <string.h>
#include <M5Unified.h>
#include <math.h>

//helper fcns

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
    int third = M5.Display.width() / 3;

    
    // TOUCH INPUT
    
    if (touch.wasPressed())
    {
        Serial.println(touch.x);
        if (touch.x < third)              // LEFT
            return +1;
        else if (touch.x < 2 * third)     // MIDDLE (SELECT)
            return 2;
        else                              // RIGHT
            return -1;
    }

    
    // BUTTON INPUT
    
    if (M5.BtnA.wasPressed())  // LEFT button
        return +1;
    
    if (M5.BtnB.wasPressed())  // MIDDLE button (SELECT)
        return 2;
    
    if (M5.BtnC.wasPressed())  // RIGHT button
        return -1;

    return 0;  // No input
}


void drawBatteryBar(int x, int y, int width, int height, int percentage)
{
    // Draw outer box
    M5.Display.drawRect(x, y, width, height, BLACK);
    
    // Draw filled portion based on percentage
    int fillWidth = (width * percentage) / 100;
    if (fillWidth > 2) {
        M5.Display.fillRect(x + 1, y + 1, fillWidth - 2, height - 2, BLACK);
    }
    
    // Draw battery terminal 
    M5.Display.fillRect(x + width + 2, y + (height / 4), 3, height / 2, BLACK);
}




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

//screen refresh logic

void InkDisplay::screenRefresh(
    IMUSensor &imu,
    SDHandler &sdhandler,
    float angle,
    float dartType,
    const float *gauges,
    const float *distances,
    int gaugeCount)
{

    // SAFETY CHECKS
    if (!gauges || !distances || gaugeCount < 0)
    {
        showError("Invalid gauge/distance data");
        return;
    }

    // Calculate air density from IMU/environment sensor
    float rho = imu.airDensityCalc();

    //reset screen
    M5.Display.clear(WHITE);
    setTextStyle(1); // small font for header


    // HEADER INFO SECTION
    
    // Get temperature and pressure from IMU
    Baro baro = imu.getBaro();
    float temp = baro.cTemp;
    float pressure = baro.pressure;
    float altitude = baro.altitude;
    
    // Get time since boot (HH:MM:SS)
    unsigned long bootTime = millis();
    unsigned int seconds = (bootTime / 1000) % 60;
    unsigned int minutes = (bootTime / 60000) % 60;
    unsigned int hours = (bootTime / 3600000);
    
    int screenWidth = M5.Display.width();
    int screenHeight = M5.Display.height();
    int batteryPercent = M5.Power.getBatteryLevel();
    
 
    // HEADER - LEFT SIDE

    M5.Display.setCursor(0, 0);
    M5.Display.printf("Battery:");

    // Draw battery bar (small, inline)
    drawBatteryBar(100, 0, 40, 18, batteryPercent);
    
    M5.Display.setCursor(160, 0);
    M5.Display.printf("%d%%", batteryPercent);
    
    M5.Display.setCursor(0, 25);
    M5.Display.printf("Density: %.3f kg/m^3", rho);

    M5.Display.setCursor(0, 50);
    M5.Display.printf("Dart: %.1f CC", dartType);
    

    //  HEADER - RIGHT SIDE

    setTextStyle(1);  // Small font
    
    // SD Status
    M5.Display.setCursor(screenWidth - 150, 0);
    M5.Display.printf("SD: %s", sdhandler.getSDStatusStr());
    
    // Time
    M5.Display.setCursor(screenWidth - 200, 25);
    M5.Display.printf("Time: %02u:%02u:%02u", hours, minutes, seconds);

  
    // ANGLE HIGHLIGHT - CENTER TOP (BOXED)

    int angleBoxX = (screenWidth / 2) - 80;
    int angleBoxY = 5;
    M5.Display.drawRect(angleBoxX, angleBoxY, 180, 70, BLACK);
    M5.Display.fillRect(angleBoxX + 1, angleBoxY + 1, 178, 18, BLACK);
    
    M5.Display.setTextColor(WHITE, BLACK);
    setTextStyle(1);
    M5.Display.setCursor(angleBoxX + 40, angleBoxY + 3);
    M5.Display.printf("ANGLE");
    
    M5.Display.setTextColor(BLACK, WHITE);
    setTextStyle(2);  // Larger font
    M5.Display.setCursor(angleBoxX + 30, angleBoxY + 30);
    M5.Display.printf("%.1f *", angle);

    // DIVIDER LINE FOR HEADER AND BODY

    M5.Display.drawLine(0, 75, screenWidth, 75, BLACK);

    // TABLE LAYOUT SETUP
   
    const int tableStartY = 90;  // Below divider
    const int tableStartX = TABLE_START_X;
    const int colWidth = COL_WIDTH;

    const int availableHeight = screenHeight - tableStartY - 20;

    // Compute row height  based on number of entries
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

    
    // TABLE BORDER with explicit left/right lines
    
    M5.Display.drawRect(tableStartX, tableStartY, tableWidth, tableHeight, BLACK);
    M5.Display.drawLine(tableStartX, tableStartY, tableStartX, tableStartY + tableHeight, BLACK);
    M5.Display.drawLine(tableStartX + tableWidth, tableStartY, tableStartX + tableWidth, tableStartY + tableHeight, BLACK);

    
    // TABLE HEADER ROW (bold)
    
    M5.Display.fillRect(tableStartX, tableStartY, tableWidth, rowHeight, BLACK);
    M5.Display.setTextColor(WHITE, BLACK);
    setTextStyle(1);

    M5.Display.setCursor(tableStartX + 10, tableStartY + (rowHeight / 2) - 5);
    M5.Display.printf("Gauge (MPa)");

    M5.Display.setCursor(tableStartX + colWidth + 10, tableStartY + (rowHeight / 2) - 5);
    M5.Display.printf("Distance (Ft)");

    
    // DATA ROWS
    
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
            rowY + 50,
            BLACK);

        // Left column: gauge values
        M5.Display.setCursor(tableStartX + 10, rowY + (rowHeight / 2) - 5);
        M5.Display.printf("%.2f", gauges[i]);

        // Right column: distance values
        M5.Display.setCursor(tableStartX + colWidth + 10, rowY + (rowHeight / 2) - 5);
        M5.Display.printf("%.2f", distances[i]);
    }

    
    // BOTTOM BORDER
    
    M5.Display.drawLine(
        tableStartX,
        tableStartY + tableHeight,
        tableStartX + tableWidth,
        tableStartY + tableHeight,
        BLACK);

    
    // TEMPERATURE BOX (right side of table)
    
    float tempF = (temp * 9.0f / 5.0f) + 32.0f;
    
    int tempBoxX = tableStartX + tableWidth + 40;
    int tempBoxY = tableStartY + 20;
    
    // Draw box with header
    M5.Display.drawRect(tempBoxX, tempBoxY, 180, 90, BLACK);
    M5.Display.fillRect(tempBoxX + 1, tempBoxY + 1, 178, 18, BLACK);
    
    M5.Display.setTextColor(WHITE, BLACK);
    setTextStyle(1);
    M5.Display.setCursor(tempBoxX + 55, tempBoxY + 3);
    M5.Display.printf("TEMP");

    
    M5.Display.setCursor(tempBoxX + 15, tempBoxY + 40);
    setTextStyle(2);
    M5.Display.printf("%.1f *F", tempF);

    
    // PRESSURE/ALTITUDE BOX (below temperature)
    
    int pressBoxX = tempBoxX;
    int pressBoxY = tempBoxY + 105;
    
    M5.Display.drawRect(pressBoxX, pressBoxY, 180, 90, BLACK);
    M5.Display.fillRect(pressBoxX + 1, pressBoxY + 1, 178, 18, BLACK);
    
    M5.Display.setTextColor(WHITE, BLACK);
    setTextStyle(1);
    M5.Display.setCursor(pressBoxX + 30, pressBoxY + 2);
    M5.Display.printf("PRESSURE");
    
    M5.Display.setTextColor(BLACK, WHITE);
    setTextStyle(1);
    M5.Display.setCursor(pressBoxX + 10, pressBoxY + 25);
    M5.Display.printf("%.0f Pa", pressure);
    
    M5.Display.setCursor(pressBoxX + 10, pressBoxY + 50);
    M5.Display.printf("Alt: %.1f f", altitude*3.280839895);

    

    
    // PUSH TO DISPLAY
    
    M5.Display.display();
}


// CONFIGURATION INPUT STAGE


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



  
    // INPUT HANDLING 
    
    int dir = getDirection();

    if (stage == 0)
    {
        if (dir == 1 || dir == -1)
        {
            applyStep(dartType, 0.5f, MIN_DART_TYPE, MAX_DART_TYPE, dir);
            changed = true;
        }

        if (dir == 2)
        {
            stage = 1;
            changed = true;
        }
    }
    else if (stage == 1)
    {
        if (dir == 1 || dir == -1)
        {
            gunType = (gunType == GunType::G2)
                          ? GunType::Model389
                          : GunType::G2;
            changed = true;
        }

        if (dir == 2)
        {
            configComplete = true;
            stage = 0;
            changed = true;
        }
    }

  
    // DISPLAY UPDATE
  
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


// ERROR / WARNING


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


// BLE CONNECTION STATE DISPLAYS


void InkDisplay::showBLEConnecting()
{
    M5.Display.clear(WHITE);
    setTextStyle(2);  // Large font
    M5.Display.setTextColor(BLACK, WHITE);

    // Draw title
    M5.Display.setCursor(M5.Display.width() / 2 - 200, 80);
    M5.Display.printf("BLE CONNECTING");

    setTextStyle(1);
    M5.Display.setCursor(M5.Display.width() / 2 - 200, 140);
    M5.Display.printf("Searching for angle sensor...");
    
    // Draw a simple circle to indicate activity
    int cx = M5.Display.width() / 2;
    int cy = 220;
    int radius = 30;
    
    // Draw animated spinner indicator
    static int spinFrame = 0;
    float angle = (spinFrame % 8) * 45.0f * M_PI / 180.0f;
    int x = cx + (int)(20 * cos(angle));
    int y = cy + (int)(20 * sin(angle));
    
    M5.Display.drawCircle(cx, cy, radius, BLACK);
    M5.Display.fillCircle(x, y, 5, BLACK);
    
    spinFrame++;
    
    // Footer warning
    M5.Display.setTextColor(BLACK, WHITE);
    setTextStyle(1);
    M5.Display.setCursor(20, M5.Display.height() - 30);
    M5.Display.printf("Do not power off device");

    M5.Display.display();
}

void InkDisplay::showBLEConnected()
{
    M5.Display.clear(WHITE);
    setTextStyle(2);
    M5.Display.setTextColor(BLACK, WHITE);

    // Draw title
    M5.Display.setCursor(M5.Display.width() / 2 - 200, 80);
    M5.Display.printf("BLE CONNECTED");

    setTextStyle(1);
    M5.Display.setCursor(M5.Display.width() / 2 - 200, 140);
    M5.Display.printf("Angle sensor ready");

    // checkmark symbol
    int cx = M5.Display.width() / 2;
    int cy = 240;
    M5.Display.drawLine(cx - 20, cy, cx - 5, cy + 15, BLACK);
    M5.Display.drawLine(cx - 5, cy + 15, cx + 30, cy - 20, BLACK);

    M5.Display.display();
    delay(1500);  // Brief confirmation display
    M5.Display.clearDisplay();

    //header
    M5.Display.setCursor(0, 40);
        M5.Display.printf("Battery: %d%% (%d mV)",
                          M5.Power.getBatteryLevel(),
                          M5.Power.getBatteryVoltage());

        M5.Display.setCursor(0, 80);
        M5.Display.printf("          LEFT: DECREMENT       MIDDLE: SELECT        RIGHT: INCREMENT         ");

        M5.Display.display();
}

void InkDisplay::showSleepWarning()
{
    // First flash - attention grabber
    M5.Display.clear(WHITE);
    setTextStyle(3);  // Extra large
    M5.Display.setTextColor(BLACK, WHITE);

    M5.Display.setCursor(M5.Display.width() / 2 - 200, 80);
    M5.Display.printf("SLEEP MODE");

    setTextStyle(2);
    M5.Display.setCursor(M5.Display.width() / 2 - 250, 180);
    M5.Display.printf("Device entering sleep");

    setTextStyle(1);
    M5.Display.setCursor(20, 260);
    M5.Display.printf("Press button to wake");

    M5.Display.display();
    delay(500);
    
    // Flash off
    M5.Display.clear(WHITE);
    M5.Display.display();
    delay(300);
    
    // Show again
    M5.Display.setTextColor(BLACK, WHITE);
    setTextStyle(3);
    M5.Display.setCursor(M5.Display.width() / 2 - 200, 80);
    M5.Display.printf("SLEEP MODE");
    
    setTextStyle(1);
    M5.Display.setCursor(20, 260);
    M5.Display.printf("Press button to wake");
    
    M5.Display.display();
    delay(2000);  // Give user time to read
}


// ANGLE VISUALIZATION


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


// HELPERS


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