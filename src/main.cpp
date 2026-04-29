#include <M5Unified.h>
#include "sensors/imu_sensor.hpp"
#include "ui/ink_display.hpp"
#include "utility/sdhandler.hpp"
#include "sensors/ble_angle.hpp"
#include "esp_sleep.h"

#define WAKE_BTN_GPIO GPIO_NUM_38
#define WAKE_TOUCH_GPIO GPIO_NUM_21
#define WAKE_BTN_LEVEL 0

#include "utility/environmental_monitor.hpp"

EnvironmentalMonitor envMonitor;
SignalQualityMonitor signalMonitor;
EnvironmentalSnapshot lastEnvironmental = {0};


// TIMING SYSTEM


#define ENABLE_TIMING 1

#if ENABLE_TIMING
#define TIMER_START(name) uint32_t name##_start = millis();
#define TIMER_END(name, label) \
    Serial.printf("[TIMING] %s: %lu ms\n", label, millis() - name##_start);
#else
#define TIMER_START(name)
#define TIMER_END(name, label)
#endif



enum class SystemState
{
    CONFIG = 0,
    RUNNING = 1
};

static constexpr uint32_t SLEEP_TIMEOUT_MS = 1048576; // 17.476 minutes
static constexpr uint32_t ANGLE_STALE_THRESHOLD_MS = 5000;

// RTC MEMORY
RTC_DATA_ATTR static GunType gunType = GunType::G2;
RTC_DATA_ATTR static float dartType = 2.0f;
RTC_DATA_ATTR static SystemState systemState = SystemState::CONFIG;
RTC_DATA_ATTR static bool configurationComplete = false;

// GLOBAL STATE
static uint32_t lastActivityMs = 0;
static uint32_t lastAngleUpdateMs = 0;
static float lastValidAngle = 0.0f;
static bool csvTableLoaded = false;
static float cachedAirDensity = 0.0f;

// OBJECTS
IMUSensor imu;
InkDisplay display;
SDHandler sdHandler;
BLEAngleReceiver bleAngle;


// HELPERS


void enterDeepSleep()
{
    Serial.println("[INFO] Entering deep sleep...");
    
    // Show visual warning before sleep
    display.showSleepWarning();
    
    Serial.flush();

    esp_sleep_enable_ext1_wakeup(
        (1ULL << WAKE_BTN_GPIO),
        ESP_EXT1_WAKEUP_ANY_HIGH);

    esp_deep_sleep_start();
}

float getAngleSafe()
{
    TIMER_START(angle);

    if (!bleAngle.isConnected())
    {
        Serial.printf("[DEBUG] BLE not connected, using last angle: %.1f\n", lastValidAngle);
        TIMER_END(angle, "Angle Read (BLE fallback)");
        return lastValidAngle;
    }

    float angle = bleAngle.getAngle();

    if (angle < -90.0f || angle > 90.0f)
    {
        Serial.printf("[ERROR] Invalid angle: %.1f\n", angle);
        TIMER_END(angle, "Angle Read (invalid)");
        return lastValidAngle;
    }

    lastValidAngle = angle;
    lastAngleUpdateMs = millis();

    TIMER_END(angle, "Angle Read");
    return angle;
}

bool isAngleReadingStale()
{
    return (millis() - lastAngleUpdateMs) > ANGLE_STALE_THRESHOLD_MS;
}

float loadAndCacheAirDensity()
{
    TIMER_START(rho);
    cachedAirDensity = imu.airDensityCalc();
    Serial.printf("[DEBUG] Air density: %.3f kg/m3\n", cachedAirDensity);
    TIMER_END(rho, "Air Density Calc");
    return cachedAirDensity;
}


// SETUP


void setup()
{
    TIMER_START(boot);

    Serial.begin(115200);
    delay(1000);

    Serial.println("=== SYSTEM BOOT START ===");

    auto cfg = M5.config();
    cfg.serial_baudrate = 115200;
    cfg.external_imu = true;
    cfg.internal_imu = false;
    M5.begin(cfg);

    auto wakeReason = esp_sleep_get_wakeup_cause();

    if (wakeReason == ESP_SLEEP_WAKEUP_EXT1)
    {
        uint64_t wakeupPin = esp_sleep_get_ext1_wakeup_status();

        if (wakeupPin & (1ULL << WAKE_BTN_GPIO))
            Serial.println("[INFO] Wake from button");
        else if (wakeupPin & (1ULL << WAKE_TOUCH_GPIO))
            Serial.println("[INFO] Wake from touch");
    }
    else
        Serial.println("[INFO] Cold boot");

    Serial.println("[INFO] Initializing IMU...");
    TIMER_START(imu);

    if (imu.init() != IMUInitStatus::SUCCESS)
        Serial.println("[ERROR] IMU init failed");

    TIMER_END(imu, "IMU Init");
    imu.Calib();

    display.initScreen();
    lastEnvironmental = envMonitor.captureSnapshot(imu);
    Serial.printf("[INIT] Initial env: %.1f°C, %.4f kg/m³\n",
                  lastEnvironmental.temperature,
                  lastEnvironmental.airDensity);

    TIMER_START(sd);
    sdHandler.initSDCard();
    TIMER_END(sd, "SD Init");

    Serial.println("[INFO] Starting BLE...");
    TIMER_START(ble);
    bleAngle.init();
    TIMER_END(ble, "BLE Init");

    bleAngle.attachSignalMonitor(signalMonitor);
    bleAngle.attachDisplay(display);

    // LOAD CSV AFTER IMU IS READY
    if (configurationComplete && !csvTableLoaded)
    {
        loadAndCacheAirDensity();

        TIMER_START(csv);

        if (cachedAirDensity <= 0.0f)
            display.showWarning("Air density calc failed");

        if (sdHandler.csvRead(cachedAirDensity, dartType))
        {
            csvTableLoaded = true;
            Serial.println("[INFO] CSV loaded");
        }
        else
        {
            Serial.println("[ERROR] CSV load failed");
            Serial.printf("[DEBUG] Looking for density: %.3f\n", cachedAirDensity);
        }

        TIMER_END(csv, "CSV Load");
    }

    lastActivityMs = millis();

    TIMER_END(boot, "Total Boot");
    Serial.println("=== BOOT COMPLETE ===");
}


// CONFIG STATE


void handleConfigurationState()
{
    display.userInputStage(sdHandler, dartType, gunType, configurationComplete);

    if (configurationComplete)
    {
        systemState = SystemState::RUNNING;
        csvTableLoaded = false;
        lastActivityMs = millis();

        Serial.println("[INFO] Config complete → RUNNING");
        Serial.printf("[DEBUG] Dart=%.1f Gun=%d\n",
                      dartType, (int)gunType);
    }
}


// RUN STATE


void handleRunningState()
{
    TIMER_START(loopTotal);

    M5.update();

    bool buttonPressed = M5.BtnB.wasPressed();
    bool touchPressed = M5.Touch.getDetail().wasPressed();

    if (!buttonPressed && !touchPressed)
        return;

    lastActivityMs = millis();

    if (!csvTableLoaded)
    {
        loadAndCacheAirDensity();

        if (!sdHandler.csvRead(cachedAirDensity, dartType))
        {
            display.showError("CSV failed");
            return;
        }

        csvTableLoaded = true;
    }

    float angle = getAngleSafe();

    if (isAngleReadingStale())
        Serial.println("[WARNING] Angle stale");

    float gauges[16];
    float distances[16];

    TIMER_START(lookup);
    int count = sdHandler.gaugesPossible(angle, gauges, distances, 16);
    TIMER_END(lookup, "CSV Lookup");

    Serial.printf("[DEBUG] Found %d entries for %.2f deg\n", count, angle);
    // ENVIRONMENTAL SNAPSHOT
    TIMER_START(envSnap);
    EnvironmentalSnapshot currEnv = envMonitor.captureSnapshot(imu);

    Serial.printf("[ENV] T=%.1f°C ρ=%.4f kg/m³ Health=%d%% Trend=%s\n",
                  currEnv.temperature,
                  currEnv.airDensity,
                  currEnv.sensorHealth,
                  envMonitor.getTempTrend(lastEnvironmental, currEnv));

    lastEnvironmental = currEnv;
    TIMER_END(envSnap, "Env Snapshot");

    // SIGNAL QUALITY
    SignalQuality sig = signalMonitor.getQuality();
    Serial.printf("[SIGNAL] %s (%d%% | RSSI=%d dBm)\n",
                   signalMonitor.getQualityString(),
                   sig.qualityPercent,
                   sig.rssi);
    TIMER_START(display);
    display.screenRefresh(imu, sdHandler, angle, dartType, gauges, distances, count);
    TIMER_END(display, "Display Refresh");

    TIMER_END(loopTotal, "Total Loop");
}


// LOOP


void loop()
{
    M5.update();
    bleAngle.tick();

    if (!M5.Imu.isEnabled())
    {
        Serial.println("[ERROR] IMU missing");
        delay(1000);
        return;
    }

    if (systemState == SystemState::CONFIG)
        handleConfigurationState();
    else
        handleRunningState();

    if (millis() - lastActivityMs > SLEEP_TIMEOUT_MS)
    {
        Serial.println("[INFO] Sleep timeout");
        enterDeepSleep();
    }
}