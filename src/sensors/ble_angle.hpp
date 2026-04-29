#pragma once

#include <M5Unified.h>
#include <BLEDevice.h>
#include <BLEClient.h>
#include <BLEScan.h>
#include <BLERemoteCharacteristic.h>
#include "utility/environmental_monitor.hpp"

#define SCAN_TIME 5
#define SCAN_TIME_RECONNECT 3

// MUST match sender (from gun side code)
static BLEUUID SERVICE_UUID("4fafc201-1fb5-459e-8fcc-c5c9c331914b");
static BLEUUID CHAR_UUID("beb5483e-36e1-4688-b7f5-ea07361b26a8");

// Forward declaration
class InkDisplay;

class BLEAngleReceiver {
private:
    BLEClient* client = nullptr;
    BLERemoteCharacteristic* remoteChar = nullptr;
    BLEScan* scan = nullptr;

    SignalQualityMonitor* signalMonitorPtr = nullptr;
    InkDisplay* displayPtr = nullptr;

    float currentAngle = 0.0f;
    bool connected = false;
    bool everConnected = false;

    unsigned long lastScan = 0;
    unsigned long scanInterval = 1000;
    
    static constexpr unsigned long STARTUP_SCAN_DURATION = 60000;
    unsigned long initTime = 0;
    int scanAttempts = 0;

public:

    void init() {
        Serial.println("\n[BLE] ===== BLE INIT START =====");
        initTime = millis();
        scanAttempts = 0;
        
        Serial.println("[BLE] Performing hard BLE reset...");
        BLEDevice::deinit(true);
        delay(200);
        
        Serial.println("[BLE] Re-initializing BLE stack...");
        BLEDevice::init("M5_Receiver");
        delay(100);

        scan = BLEDevice::getScan();
        if (!scan) {
            Serial.println("[BLE] ERROR: Failed to get scan object!");
            return;
        }
        
        scan->setActiveScan(true);
        scan->setInterval(100);
        scan->setWindow(99);
        
        Serial.println("[BLE] ===== BLE INIT COMPLETE =====");
        Serial.println("[BLE] Looking for service UUID:");
        Serial.println(SERVICE_UUID.toString().c_str());
        Serial.println("[BLE] Target: BLE device advertising 4fafc201-1fb5-459e-8fcc-c5c9c331914b");
        Serial.println("[BLE] Entering aggressive startup scan phase (60s)\n");
    }

    void attachSignalMonitor(SignalQualityMonitor& monitor) {
        signalMonitorPtr = &monitor;
    }

    void attachDisplay(InkDisplay& disp) {
        displayPtr = &disp;
    }

    void tick() {
        // =========================
        // IF CONNECTED: READ DATA
        // =========================
        if (connected && client && client->isConnected() && remoteChar) {
            int8_t rssi = client->getRssi();
            if (signalMonitorPtr) {
                signalMonitorPtr->updateRSSI(rssi);
            }

            std::string value = remoteChar->readValue();
            if (!value.empty()) {
                currentAngle = atof(value.c_str());
            }
            return;
        }

        // =========================
        // DETECT CONNECTION LOSS
        // =========================
        if (connected && (!client || !client->isConnected())) {
            Serial.println("[BLE] *** CONNECTION LOST ***");
            handleDisconnect();
        }

        // =========================
        // STARTUP PHASE (first 60 seconds)
        // =========================
        unsigned long timeSinceBoot = millis() - initTime;
        bool inStartupPhase = (timeSinceBoot < STARTUP_SCAN_DURATION);
        
        if (inStartupPhase) {
            if ((millis() - lastScan) < 500) {
                return;
            }
            Serial.printf("[BLE-STARTUP] Scan attempt #%d (%.1fs elapsed)\n", 
                          ++scanAttempts, timeSinceBoot / 1000.0f);
        } else {
            if (!everConnected) {
                if ((millis() - lastScan) < 2000) return;
                Serial.printf("[BLE] Still searching (attempt #%d)...\n", ++scanAttempts);
            } else {
                if ((millis() - lastScan) < scanInterval) return;
                Serial.println("[BLE] Reconnect scan...");
            }
        }
        
        lastScan = millis();

        if (displayPtr && !connected) {
            displayPtr->showBLEConnecting();
        }

        int currentScanTime = inStartupPhase ? SCAN_TIME : SCAN_TIME_RECONNECT;
        
        Serial.printf("[BLE] Starting %ds scan...\n", currentScanTime);
        BLEScanResults results = scan->start(currentScanTime, false);

        int count = results.getCount();
        Serial.printf("[BLE] Found %d devices\n", count);

        bool found = false;
        for (int i = 0; i < count; i++) {
            BLEAdvertisedDevice device = results.getDevice(i);
            
            const char* devName = device.getName().c_str();
            const char* devAddr = device.getAddress().toString().c_str();
            int rssi = device.getRSSI();
            
            // Print basic info
            Serial.printf("[BLE]   Device %d: Name=\"%s\" Addr=%s RSSI=%d\n", 
                          i, devName, devAddr, rssi);

            // ===== PRIMARY MATCH: Service UUID =====
            // Try to get the service UUID - if we find ours, connect to it
            if (device.haveServiceUUID()) {
                BLEUUID serviceUUID = device.getServiceUUID();
                Serial.printf("[BLE]   Services advertised: %s\n", serviceUUID.toString().c_str());
                
                // Check if this is our target service
                if (serviceUUID.equals(SERVICE_UUID)) {
                    Serial.printf("[BLE]  SERVICE UUID MATCH! \n");
                    Serial.printf("[BLE] This is your angle device at %s\n", devAddr);
                    
                    if (attemptConnection(device)) {
                        everConnected = true;
                        connected = true;
                        Serial.println("[BLE] *** CONNECTION SUCCESS ***\n");
                        
                        if (displayPtr) {
                            displayPtr->showBLEConnected();
                        }
                        found = true;
                        return;
                    } else {
                        Serial.println("[BLE] Connection attempt failed - will retry");
                        connected = false;
                        found = true;
                        break;
                    }
                }
            }
        }

        if (!found) {
            if (count == 0) {
                Serial.println("[BLE] No devices found in scan");
            } else {
                Serial.println("[BLE] Target service UUID not found in any device");
                Serial.printf("[BLE] Looking for: %s\n\n", SERVICE_UUID.toString().c_str());
            }
        }
        
        if (!inStartupPhase && !everConnected && scanAttempts % 10 == 0) {
            Serial.printf("[BLE] WARNING: Still searching after %d attempts\n", scanAttempts);
            Serial.println("[BLE] Make sure:");
            Serial.println("[BLE]   - Angle device is powered on");
            Serial.println("[BLE]   - Angle device is advertising (check its serial output)");
            Serial.println("[BLE]   - Both devices are in range\n");
        }
    }

    bool isConnected() {
        return connected && client && client->isConnected();
    }

    float getAngle() {
        return currentAngle;
    }

private:
    bool attemptConnection(BLEAdvertisedDevice& device) {
        // Clean up old connection
        if (client) {
            try {
                client->disconnect();
            } catch (...) {}
            delete client;
            client = nullptr;
        }

        Serial.println("[BLE] Creating new BLE client...");
        client = BLEDevice::createClient();
        if (!client) {
            Serial.println("[BLE] ERROR: Failed to create client!");
            return false;
        }

        Serial.printf("[BLE] Connecting to %s...\n", device.getAddress().toString().c_str());
        if (!client->connect(&device)) {
            Serial.println("[BLE] ERROR: Connection failed");
            delete client;
            client = nullptr;
            return false;
        }

        Serial.println("[BLE] Connected to device! Waiting for stability...");
        delay(150);

        Serial.printf("[BLE] Retrieving service: %s\n", SERVICE_UUID.toString().c_str());
        BLERemoteService* service = client->getService(SERVICE_UUID);
        if (!service) {
            Serial.println("[BLE] ERROR: Service not found on device");
            client->disconnect();
            delete client;
            client = nullptr;
            return false;
        }
        Serial.println("[BLE]  Service found");

        Serial.printf("[BLE] Retrieving characteristic: %s\n", CHAR_UUID.toString().c_str());
        remoteChar = service->getCharacteristic(CHAR_UUID);
        if (!remoteChar) {
            Serial.println("[BLE] ERROR: Characteristic not found on device");
            client->disconnect();
            delete client;
            client = nullptr;
            return false;
        }
        Serial.println("[BLE]  Characteristic found");
        
        Serial.println("[BLE] Full connection successful! \n");
        return true;
    }

    void handleDisconnect() {
        connected = false;
        lastScan = 0;  // Scan immediately
        
        if (client) {
            try {
                client->disconnect();
            } catch (...) {
                Serial.println("[BLE] Error during disconnect");
            }
            delete client;
            client = nullptr;
        }
        remoteChar = nullptr;
    }
};