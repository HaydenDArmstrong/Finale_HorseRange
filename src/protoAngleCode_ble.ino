#include <Wire.h>
#include <Adafruit_LIS3DH.h>
#include <Adafruit_Sensor.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

Adafruit_LIS3DH lis = Adafruit_LIS3DH();

// BLE objects
BLECharacteristic *pCharacteristic;
bool deviceConnected = false;

float pitch_offset = 0;


#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// Handle connect/disconnect
class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
  };

  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
  }
};

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);

  // LIS3DH init
  if (!lis.begin(0x18)) {
    Serial.println("Could not start LIS3DH");
    while (1);
  }
  lis.setRange(LIS3DH_RANGE_2_G);

  // BLE init
  BLEDevice::init("ESP32_protoAngle");
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);

  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_NOTIFY |
                      BLECharacteristic::PROPERTY_READ
                    );

  pCharacteristic->addDescriptor(new BLE2902()); // required for notify

  pService->start();
  BLEDevice::getAdvertising()->start();

  Serial.println("BLE ready, waiting for connection...");

  // --- CALIBRATION ---
  Serial.println("Calibrating... keep level");
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

  Serial.print("Offset: ");
  Serial.println(pitch_offset);
}

void loop() {
  lis.read();

  float x = lis.x / 1000.0;
  float y = lis.y / 1000.0;
  float z = lis.z / 1000.0;

  float pitch = -1 * atan2(x, sqrt(y*y + z*z)) * 180.0 / PI;
  pitch -= pitch_offset;

  Serial.println(pitch, 2);

  if (deviceConnected) {
    char buffer[16];
    dtostrf(pitch, 6, 2, buffer);  // convert float → string

    pCharacteristic->setValue(buffer);
    pCharacteristic->notify();     
  }

  delay(200);
}