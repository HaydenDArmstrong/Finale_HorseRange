#include <M5Unified.h>
#include "sensors/imu_sensor.hpp"
#include "ui/ink_display.hpp"
#include "utility/sdhandler.hpp"
#include "esp_sleep.h"

// GPIO 37 = BtnA, 38 = BtnB, 39 = BtnC
#define WAKE_BTN_GPIO   GPIO_NUM_38
#define WAKE_BTN_LEVEL  0           // buttons are active-LOW 

// RTC_DATA_ATTR: survives deep sleep, stored in RTC SRAM (~8 KB)

RTC_DATA_ATTR static bool  isConfigured = false;
RTC_DATA_ATTR static bool  tableLoaded  = false;
RTC_DATA_ATTR static float gauge        = 2.0f;

// Sleep timeout: enter deep sleep after this many ms of inactivity
static const uint32_t SLEEP_TIMEOUT_MS = 6000;   // 1 minute
static uint32_t       lastActivityMs   = 0;

// system objects  (re-created each boot)
IMUSensor  imu;
InkDisplay display;
SDHandler  SDHandlr;

// Helper: put the device into deep sleep, wake on BtnB press
void enterDeepSleep()
{
    Serial.println("Entering deep sleep — press BtnB to wake");
    Serial.flush();

    // E-ink holds the image with no power, nothing extra needed

    esp_sleep_enable_ext0_wakeup(WAKE_BTN_GPIO, WAKE_BTN_LEVEL);
    esp_deep_sleep_start();
    // execution never reaches here
}


// void enterLightSleep()
// {
//     Serial.println("Light sleep — press BtnB to wake");
//     Serial.flush();

//     gpio_wakeup_enable(WAKE_BTN_GPIO, GPIO_INTR_LOW_LEVEL);
//     esp_sleep_enable_gpio_wakeup();
//     esp_light_sleep_start();

//     // Execution resumes HERE after wake, re-init anything that
//     // loses state (Serial, display driver, etc.)
//     Serial.begin(115200);
//     M5.Display.wakeup();
//     lastActivityMs = millis();
// }

void setup()
{
    Serial.begin(115200);
    delay(2000);
    Serial.println("=== BOOT START ===");

    auto cfg = M5.config();
    cfg.serial_baudrate = 115200;
    cfg.external_imu   = true;
    cfg.internal_imu   = false;
    M5.begin(cfg);
    Serial.println("M5 ready");

    // Wakeup reason
    esp_sleep_wakeup_cause_t reason = esp_sleep_get_wakeup_cause();
    switch (reason) {
        case ESP_SLEEP_WAKEUP_EXT0:
            Serial.println("WAKE: button press (ext0)");
            break;
        case ESP_SLEEP_WAKEUP_UNDEFINED:
            Serial.println("WAKE: cold boot");
            break;
        default:
            Serial.printf("WAKE: other reason %d\n", reason);
            break;
    }

    Serial.printf("isConfigured=%d tableLoaded=%d gauge=%.1f\n",
                   isConfigured, tableLoaded, gauge);

    Serial.println("Starting IMU init...");
    imu.init();

    if (!isConfigured) {
        Serial.println("Calibrating IMU...");
        imu.Calib();
    }

    Serial.println("Display init...");
    display.initScreen();

    Serial.println("SD init...");
    SDHandlr.initSDCard();

    if (tableLoaded) {
        SDHandlr.csvRead(imu.airDensityCalc(imu));
    }

    lastActivityMs = millis();
    Serial.println("SETUP DONE");
}

void loop()
{
    M5.update();

    //Configuration stage (skipped after deep-sleep wakes)
    if (!isConfigured)
    {
        while (!M5.BtnB.isPressed())
        {
            M5.update();
            display.userInputStage(SDHandlr, gauge);

            // Allow sleep even during config if user walks away
            if (millis() - lastActivityMs > SLEEP_TIMEOUT_MS) {
                enterDeepSleep();
            }
            delay(100);
        }
        isConfigured  = true;
        lastActivityMs = millis();
    }

    //IMU guard
    if (!M5.Imu.isEnabled()) {
        Serial.println("IMU not detected");
        delay(1000);
        return;
    }

    // Active measurement on BtnB
    if (M5.BtnB.isPressed())
    {
        lastActivityMs = millis();   // user is active, reset timer

        imu.update();
        const AccelVector &a = imu.getAccel();

        if (!tableLoaded) {
            float rho = imu.airDensityCalc(imu);
            Serial.printf("rho = %.4f\n", rho);
            SDHandlr.csvRead(rho);
            tableLoaded = true;
        }

        float angle    = a.y * 90.0f;
        float distance = SDHandlr.lookupDistance(angle, gauge);

        imu.printToSerial();
        display.screenRefresh(imu, SDHandlr, angle, gauge, distance);
        delay(500);
    }

    // Inactivity , then sleep
    if (millis() - lastActivityMs > SLEEP_TIMEOUT_MS)
    {
        enterDeepSleep(); 
    }
}