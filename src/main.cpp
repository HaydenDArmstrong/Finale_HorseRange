#include <M5Unified.h>
#include "sensors/imu_sensor.hpp"
#include "ui/ink_display.hpp"
#include "utility/sdhandler.hpp"
#include "sensors/ble_angle.hpp"
#include "esp_sleep.h"

// ============================================================
// CONFIG
// ============================================================

#define WAKE_BTN_GPIO GPIO_NUM_38
#define WAKE_BTN_LEVEL 0

enum class SystemState {
    CONFIG = 0,
    RUNNING = 1
};

static constexpr uint32_t SLEEP_TIMEOUT_MS = 90000;
static constexpr uint32_t ANGLE_STALE_THRESHOLD_MS = 5000;

// ============================================================
// RTC MEMORY
// ============================================================

RTC_DATA_ATTR static GunType gunType = GunType::G2;
RTC_DATA_ATTR static float dartType = 2.0f;
RTC_DATA_ATTR static SystemState systemState = SystemState::CONFIG;
RTC_DATA_ATTR static bool configurationComplete = false;

// ============================================================
// GLOBAL STATE
// ============================================================

static uint32_t lastActivityMs = 0;
static uint32_t lastAngleUpdateMs = 0;
static float lastValidAngle = 0.0f;
static bool csvTableLoaded = false;
static float cachedAirDensity = 0.0f;

// ============================================================
// OBJECTS
// ============================================================

IMUSensor imu;
InkDisplay display;
SDHandler sdHandler;
BLEAngleReceiver bleAngle;

// ============================================================
// HELPERS
// ============================================================

void enterDeepSleep() {
    Serial.println("[INFO] Entering deep sleep...");
    Serial.flush();
    esp_sleep_enable_ext0_wakeup(WAKE_BTN_GPIO, WAKE_BTN_LEVEL);
    esp_deep_sleep_start();
}

float getAngleSafe() {
    if (!bleAngle.isConnected()) {
        Serial.printf("[DEBUG] BLE not connected, using last angle: %.1f\n", lastValidAngle);
        return lastValidAngle;
    }

    float angle = bleAngle.getAngle();

    if (angle < 0.0f || angle > 90.0f) {
        Serial.printf("[ERROR] Invalid angle: %.1f\n", angle);
        return lastValidAngle;
    }

    lastValidAngle = angle;
    lastAngleUpdateMs = millis();

    return angle;
}

bool isAngleReadingStale() {
    return (millis() - lastAngleUpdateMs) > ANGLE_STALE_THRESHOLD_MS;
}

float loadAndCacheAirDensity() {
    cachedAirDensity = imu.airDensityCalc();
    Serial.printf("[DEBUG] Air density: %.3f kg/m3\n", cachedAirDensity);
    return cachedAirDensity;
}

// ============================================================
// SETUP
// ============================================================

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("=== SYSTEM BOOT START ===");

    auto cfg = M5.config();
    cfg.serial_baudrate = 115200;
    cfg.external_imu = true;
    cfg.internal_imu = false;
    M5.begin(cfg);

    auto wakeReason = esp_sleep_get_wakeup_cause();
    if (wakeReason == ESP_SLEEP_WAKEUP_EXT0)
        Serial.println("[INFO] Wake from deep sleep");
    else
        Serial.println("[INFO] Cold boot");

    Serial.println("[INFO] Initializing IMU...");
    if (imu.init() != IMUInitStatus::SUCCESS) {
        Serial.println("[ERROR] IMU init failed");
    }

    display.initScreen();
    sdHandler.initSDCard();
    imu.Calib();

    Serial.println("[INFO] Starting BLE...");
    bleAngle.init();

    if (configurationComplete && !csvTableLoaded) {
        loadAndCacheAirDensity();

        if (sdHandler.csvRead(cachedAirDensity, dartType)) {
            csvTableLoaded = true;
            Serial.println("[INFO] CSV loaded");
        } else {
            Serial.println("[ERROR] CSV load failed");
        }
    }

    lastActivityMs = millis();
    Serial.println("=== BOOT COMPLETE ===");
}

// ============================================================
// CONFIG STATE
// ============================================================

void handleConfigurationState() {
    display.userInputStage(sdHandler, dartType, gunType, configurationComplete);

    if (configurationComplete) {
        systemState = SystemState::RUNNING;
        csvTableLoaded = false;
        lastActivityMs = millis();

        Serial.println("[INFO] Config complete → RUNNING");
        Serial.printf("[DEBUG] Dart=%.1f Gun=%d\n",
            dartType, (int)gunType);
    }
}

// ============================================================
// RUN STATE
// ============================================================

void handleRunningState() {
    if (!M5.BtnB.isPressed()) return;

    lastActivityMs = millis();

    if (!csvTableLoaded) {
        loadAndCacheAirDensity();

        if (!sdHandler.csvRead(cachedAirDensity, dartType)) {
            Serial.println("[ERROR] CSV load failed");
            display.showError("CSV failed");
            return;
        }

        csvTableLoaded = true;
        Serial.println("[INFO] CSV loaded");
    }

    float angle = getAngleSafe();

    if (isAngleReadingStale()) {
        Serial.println("[WARNING Angle stale");
        //display.showWarning("Angle stale");
    }

    float gauges[16];
    float distances[16];

    int count = sdHandler.gaugesPossible(angle, gauges, distances, 16);

    Serial.printf("[DEBUG] Found %d entries for %.2f deg\n", count, angle);

    display.screenRefresh(imu, sdHandler, angle, dartType, gauges, distances, count);

    delay(500);
}

// ============================================================
// LOOP
// ============================================================

void loop() {
    M5.update();
    bleAngle.tick();

    

    if (!M5.Imu.isEnabled()) {
        Serial.println("[ERROR] IMU missing");
        delay(1000);
        return;
    }

    if (systemState == SystemState::CONFIG) {
        handleConfigurationState();
    } else {
        handleRunningState();
    }

    if (millis() - lastActivityMs > SLEEP_TIMEOUT_MS) {
        Serial.println("[INFO] Sleep timeout");
        enterDeepSleep();
    }
}