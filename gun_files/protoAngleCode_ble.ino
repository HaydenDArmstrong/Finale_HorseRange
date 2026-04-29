#include <Wire.h>
#include <Adafruit_LIS3DH.h>
#include <Adafruit_Sensor.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

Adafruit_LIS3DH lis = Adafruit_LIS3DH();
BLECharacteristic *pCharacteristic;
BLEAdvertising *pAdvertising;
bool deviceConnected = false;

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

float pitch_offset = 0;

// CONNECTION CALLBACKS
class MyServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        deviceConnected = true;
        Serial.println("[BLE] Client connected");
    }
    void onDisconnect(BLEServer* pServer) {
        deviceConnected = false;
        Serial.println("[BLE] Client disconnected - restarting advertising");
        
        // Restart advertising when client disconnects
        if (pAdvertising) {
            pAdvertising->start();
            Serial.println("[BLE] Advertising restarted");
        }
    }
};


// SETUP


void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n[SETUP] ===== ANGLE DEVICE STARTUP =====");
    
    // IMU INIT
    Serial.println("[IMU] Initializing LIS3DH...");
    Wire.begin(21, 22);
    
    if (!lis.begin(0x18)) {
        Serial.println("[ERROR] LIS3DH not found at 0x18!");
        Serial.println("[ERROR] Trying alternate address 0x19...");
        if (!lis.begin(0x19)) {
            Serial.println("[ERROR] LIS3DH not found anywhere - HALTING");
            while (1) delay(1000);
        }
    }
    
    lis.setRange(LIS3DH_RANGE_2_G);
    Serial.println("[IMU] LIS3DH initialized successfully");

    // BLE INIT - MORE ROBUST
    Serial.println("[BLE] Initializing BLE stack...");
    BLEDevice::init("ESP32_protoAngle");
    delay(100);
    
    // Create server
    Serial.println("[BLE] Creating BLE server...");
    BLEServer *pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    // Create service
    Serial.println("[BLE] Creating service: " SERVICE_UUID);
    BLEService *pService = pServer->createService(SERVICE_UUID);

    // Create characteristic
    Serial.println("[BLE] Creating characteristic: " CHARACTERISTIC_UUID);
    pCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_NOTIFY |
        BLECharacteristic::PROPERTY_READ
    );

    // Add descriptor for notifications
    pCharacteristic->addDescriptor(new BLE2902());
    
    // Start service
    pService->start();
    Serial.println("[BLE] Service started");

    // Setup advertising
    Serial.println("[BLE] Setting up advertising...");
    pAdvertising = BLEDevice::getAdvertising();
    
    // Add service UUID to advertisement
    pAdvertising->addServiceUUID(SERVICE_UUID);
    Serial.println("[BLE] ✓ Service UUID added to advertisement");
    
    // Set device name in advertisement
    pAdvertising->setAppearance(0x0000);  // Generic appearance
    
    // Start advertising
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);   // Short min interval
    pAdvertising->setMaxPreferred(0x12);   // Short max interval
    
    Serial.println("[BLE] Starting advertising...");
    pAdvertising->start();
    
    Serial.println("[BLE]  ADVERTISING STARTED ");
    Serial.println("[BLE] Service UUID: " SERVICE_UUID);
    Serial.println("[BLE] Device name: ESP32_protoAngle");
    Serial.println("[BLE] Waiting for connections...\n");

    // CALIBRATION
    Serial.println("[CAL] Keep device level for calibration...");
    float sum = 0;
    int samples = 50;
    
    for (int i = 0; i < samples; i++) {
        lis.read();
        float x = lis.x / 1000.0;
        float y = lis.y / 1000.0;
        float z = lis.z / 1000.0;
        float pitch = atan2(x, sqrt(y*y + z*z)) * 180.0 / PI;
        sum += pitch;
        delay(20);
    }
    
    pitch_offset = sum / samples;
    Serial.printf("[CAL] Offset = %.2f\n", pitch_offset);
    Serial.println("[SETUP] ===== STARTUP COMPLETE =====\n");
}


// LOOP


void loop() {
    lis.read();
    
    float x = lis.x / 1000.0;
    float y = lis.y / 1000.0;
    float z = lis.z / 1000.0;
    
    float pitch = -atan2(x, sqrt(y*y + z*z)) * 180.0 / PI;
    pitch -= pitch_offset;

    Serial.printf("[ANGLE] %.2f deg\n", pitch);

    // BLE SEND (ONLY IF CONNECTED)
    if (deviceConnected) {
        char buffer[16];
        snprintf(buffer, sizeof(buffer), "%.2f", pitch);
        
        pCharacteristic->setValue(buffer);
        pCharacteristic->notify();
        
        //Serial.printf("[BLE-SEND] %s\n", buffer);
    } else {
        // Optional: print advertising status every 5 seconds
        static unsigned long lastStatusPrint = 0;
        if (millis() - lastStatusPrint > 5000) {
            Serial.println("[BLE] Waiting for client connection (advertising)...");
            lastStatusPrint = millis();
        }
    }

    delay(100); // 10 Hz update rate
}