#include <M5Unified.h>
#include "sensors/imu_sensor.hpp"
#include "ui/ink_display.hpp"
#include "utility/sdhandler.hpp"
#include "sensors/ble_angle.hpp" // Ensure this contains the BluetoothSerial logic
#include "esp_sleep.h"

// GPIO 37 = BtnA, 38 = BtnB, 39 = BtnC
#define WAKE_BTN_GPIO GPIO_NUM_38
#define WAKE_BTN_LEVEL 0 // buttons are active-LOW

// RTC_DATA_ATTR: survives deep sleep
RTC_DATA_ATTR static bool isConfigured = false;
RTC_DATA_ATTR static bool tableLoaded = false;
RTC_DATA_ATTR static float dartType = 2.0f;
RTC_DATA_ATTR static float inputDistance = 0.0f;
RTC_DATA_ATTR static bool isDistanceInputted = false;

static const uint32_t SLEEP_TIMEOUT_MS = 90000; 
static uint32_t lastActivityMs = 0;

// System objects
IMUSensor imu;
InkDisplay display;
SDHandler SDHandlr;
BLEAngleReceiver bleAngle; // The Bluetooth Serial object

void enterDeepSleep()
{
    Serial.println("Entering deep sleep — press BtnB to wake");
    Serial.flush();
    esp_sleep_enable_ext0_wakeup(WAKE_BTN_GPIO, WAKE_BTN_LEVEL);
    esp_deep_sleep_start();
}

void setup()
{
    Serial.begin(115200);
    delay(1000);
    Serial.println("=== BOOT START ===");

    auto cfg = M5.config();
    cfg.serial_baudrate = 115200;
    cfg.external_imu = true;
    cfg.internal_imu = false;
    M5.begin(cfg);

    // Check wakeup reason
    esp_sleep_wakeup_cause_t reason = esp_sleep_get_wakeup_cause();
    if (reason == ESP_SLEEP_WAKEUP_EXT0) {
        Serial.println("WAKE: button press");
    }

    // Init Sensors & Display
    imu.init();
    if (!isConfigured) {
        imu.Calib();
        isConfigured = true; // Mark as calibrated for next wake
    }

    display.initScreen();
    SDHandlr.initSDCard();

    // Start Bluetooth Connection
    Serial.println("Bluetooth init...");
    bleAngle.init(); 

    if (tableLoaded) {
        SDHandlr.csvRead(imu.airDensityCalc(imu), dartType);
    }

    lastActivityMs = millis();
    Serial.println("SETUP DONE");
}

void loop()
{
    M5.update();
    
    // Maintain Bluetooth connection and parse incoming strings
    bleAngle.tick(); 

    // Guard: Ensure the BMP280 is alive for air density
    if (!M5.Imu.isEnabled()) {
        Serial.println("IMU (BMP280) not detected");
        delay(1000);
        return;
    }

    // Trigger calculation and display update on Button B
    if (M5.BtnB.isPressed())
    {
        lastActivityMs = millis();

        // Verify Bluetooth connection before proceeding
        if (!bleAngle.isConnected())
        {
            Serial.println("Bluetooth not connected — looking for sender...");
            // Optional: display.drawError("No BT Connection");
            delay(500);
            return;
        }

        // Get the angle received via Bluetooth Serial
        float angle = bleAngle.getAngle(); 

        // Load CSV if not done yet
        if (!tableLoaded)
        {
            float rho = imu.airDensityCalc(imu); 
            SDHandlr.csvRead(rho, dartType);
            tableLoaded = true;
        }

        // Logic for ballistic calculations
        float gauges[16];
        float distances[16];
        int found = SDHandlr.gaugesPossible(angle, gauges, distances, 16);

        Serial.printf("[ANGLE from BT] %.2f°\n", angle);

        // Update the E-Ink Display
        display.screenRefresh(imu, SDHandlr, angle, dartType, inputDistance,
                              gauges, distances, found);
        
        delay(500); // Debounce/throttle
    }

    // Sleep Timer
    if (millis() - lastActivityMs > SLEEP_TIMEOUT_MS)
    {
        enterDeepSleep();
    }
}