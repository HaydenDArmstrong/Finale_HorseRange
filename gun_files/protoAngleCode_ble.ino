#include <Wire.h>
#include <Adafruit_LIS3DH.h>
#include <Adafruit_Sensor.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

Adafruit_LIS3DH lis = Adafruit_LIS3DH();



BLECharacteristic *pCharacteristic;
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
        Serial.println("[BLE] Client disconnected");
    }
};

// ============================================================
// SETUP
// ============================================================

void setup() {
    Serial.begin(115200);
    Wire.begin(21, 22);

    // IMU INIT
    if (!lis.begin(0x18)) {
        Serial.println("[ERROR] LIS3DH not found");
        while (1);
    }

    lis.setRange(LIS3DH_RANGE_2_G);

    // BLE INIT
    BLEDevice::init("ESP32_protoAngle");

    BLEServer *pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    BLEService *pService = pServer->createService(SERVICE_UUID);

    pCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_NOTIFY |
        BLECharacteristic::PROPERTY_READ
    );

    pCharacteristic->addDescriptor(new BLE2902());

    pService->start();

    // advertise SERVICE UUID explicitly
    BLEAdvertising *adv = BLEDevice::getAdvertising();
    adv->addServiceUUID(SERVICE_UUID);
    adv->setScanResponse(true);
    adv->start();

    Serial.println("[BLE] Advertising started");

    // CALIBRATION
    Serial.println("[CAL] Keep device level...");

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
}

// ============================================================
// LOOP
// ============================================================

void loop() {

    lis.read();

    float x = lis.x / 1000.0;
    float y = lis.y / 1000.0;
    float z = lis.z / 1000.0;

    float pitch = -atan2(x, sqrt(y*y + z*z)) * 180.0 / PI;
    pitch -= pitch_offset;

    Serial.printf("[ANGLE] %.2f\n", pitch);


    // BLE SEND (ONLY IF CONNECTED)
    if (deviceConnected) {

        char buffer[16];

        // stable formatting (no weird padding)
        snprintf(buffer, sizeof(buffer), "%.2f", pitch);

        pCharacteristic->setValue(buffer);
        pCharacteristic->notify();
    }

    delay(100); // stable update rate (10 Hz)
}