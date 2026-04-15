#pragma once

#include <M5Unified.h>
#include <BLEDevice.h>
#include <BLEClient.h>
#include <BLEScan.h>
#include <BLERemoteCharacteristic.h>

// MUST match sender
static BLEUUID SERVICE_UUID("4fafc201-1fb5-459e-8fcc-c5c9c331914b");
static BLEUUID CHAR_UUID("beb5483e-36e1-4688-b7f5-ea07361b26a8");

class BLEAngleReceiver {
private:
    BLEClient* client = nullptr;
    BLERemoteCharacteristic* remoteChar = nullptr;

    float currentAngle = 0.0f;
    bool connected = false;

    unsigned long lastScan = 0;
    const unsigned long scanInterval = 3000;

public:

    void init() {
        BLEDevice::init("M5_Receiver");
        Serial.println("[BLE] Ready");
    }

    void tick() {

        // If connected, read data
        if (connected && client && client->isConnected() && remoteChar) {

            std::string value = remoteChar->readValue();
            if (!value.empty()) {
                currentAngle = atof(value.c_str());
                Serial.printf("[BLE] Angle: %.2f\n", currentAngle);
            }

            return;
        }

        // reconnect delay
        if (millis() - lastScan < scanInterval) return;
        lastScan = millis();

        Serial.println("[BLE] Scanning...");

        BLEScan* scan = BLEDevice::getScan();
        BLEScanResults results = scan->start(2, false);

        for (int i = 0; i < results.getCount(); i++) {

            BLEAdvertisedDevice device = results.getDevice(i);

            if (device.haveServiceUUID() &&
                device.isAdvertisingService(SERVICE_UUID)) {

                Serial.println("[BLE] Found device → connecting");

                client = BLEDevice::createClient();

                if (!client->connect(&device)) {
                    Serial.println("[BLE] Connect failed");
                    return;
                }

                BLERemoteService* service = client->getService(SERVICE_UUID);
                if (!service) {
                    Serial.println("[BLE] No service");
                    return;
                }

                remoteChar = service->getCharacteristic(CHAR_UUID);
                if (!remoteChar) {
                    Serial.println("[BLE] No char");
                    return;
                }

                connected = true;
                Serial.println("[BLE] CONNECTED");

                return;
            }
        }

        Serial.println("[BLE] No device found");
    }

    bool isConnected() {
        return connected && client && client->isConnected();
    }

    float getAngle() {
        return currentAngle;
    }
};