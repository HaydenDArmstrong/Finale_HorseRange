#include "BluetoothSerial.h"

class BLEAngleReceiver {
private:
    BluetoothSerial SerialBT;
    float currentAngle = 0.0f;
    const char* deviceName = "ESP32_protoAngle"; // sender name

public:
    void init() {
        Serial.println("Starting Bluetooth client...");

        // Start Bluetooth as client
        if(!SerialBT.begin("M5_Display_Receiver", true)) {
            Serial.println("Failed to start BT");
        }

        Serial.println("Trying to connect...");

        if(SerialBT.connect(deviceName)) {
            Serial.println("Connected to sender!");
        } else {
            Serial.println("Failed to connect");
        }
    }

    void tick() {
        
        if (SerialBT.connected()) {
            while (SerialBT.available()) {

                String data = SerialBT.readStringUntil('\n');

                Serial.print("Received: ");
                Serial.println(data);

                currentAngle = data.toFloat();

                Serial.print("Parsed float: ");
                Serial.println(currentAngle);
            }
        }
        else {
            Serial.println("Not connected...");
        }
    }

    bool isConnected() {
        return SerialBT.connected();
    }

    float getAngle() {
        return currentAngle;
    }
};