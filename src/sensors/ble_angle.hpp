#pragma once
#include "BluetoothSerial.h"

class BLEAngleReceiver {
private:
    BluetoothSerial serialBT;
    float currentAngle = 0.0f;
    bool isConnectedFlag = false;

    const char* deviceName = "ESP32_protoAngle";
    const char* receiverName = "M5_Display_Receiver";

public:
    void init() {
        Serial.println("[BLE] Starting Bluetooth client...");

        if (!serialBT.begin(receiverName, true)) {
            Serial.println("[BLE] ERROR: Failed to initialize Bluetooth");
            isConnectedFlag = false;
            return;
        }

        Serial.printf("[BLE] Attempting connection to '%s'...\n", deviceName);

        if (serialBT.connect(deviceName)) {
            isConnectedFlag = true;
            Serial.println("[BLE] Connected");
        } else {
            isConnectedFlag = false;
            Serial.println("[BLE] Running in OFFLINE mode (no Bluetooth)");
        }
    }

    bool tick() {
        if (!isConnectedFlag) {
            return false; // just run offline
        }

        if (!serialBT.connected()) {
            isConnectedFlag = false;
            Serial.println("[BLE] Disconnected → switching to OFFLINE mode");
            return false;
        }

        if (serialBT.available()) {
            String data = serialBT.readStringUntil('\n');
            data.trim();

            float angle = data.toFloat();

            // reject garbage
            if (!(angle == 0.0f && data != "0" && data != "0.0")) {
                if (angle >= 0.0f && angle <= 90.0f) {
                    currentAngle = angle;
                    Serial.printf("[BLE] Angle: %.2f\n", currentAngle);
                }
            }
        }

        return true;
    }

    bool isConnected() const {
        return isConnectedFlag;
    }

    float getAngle() const {
        // KEY CHANGE: safe fallback
        if (!isConnectedFlag) {
            return 0.0f;
        }
        return currentAngle;
    }
};