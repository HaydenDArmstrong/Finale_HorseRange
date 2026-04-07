#ifndef BLE_ANGLE_HPP
#define BLE_ANGLE_HPP

#include "BluetoothSerial.h"

class BLEAngleReceiver {
private:
    BluetoothSerial SerialBT;
    float currentAngle = 0.0f;
    const char* remoteName = "ESP32_IMU_SENDER"; // Must match the Sender's name

public:
    void init() {
        // Start as a master device
        SerialBT.begin("M5_Display_Receiver", true); 
        Serial.println("Bluetooth Serial started. Looking for sender...");
        
        // Try to connect to the sender by name
        bool connected = SerialBT.connect(remoteName);
        
        if(connected) {
            Serial.println("Connected to IMU successfully!");
        } else {
            Serial.println("Failed to connect. Will retry in loop.");
        }
    }

    void tick() {
        // If lost connection, try to reconnect
        if (!SerialBT.connected()) {
            if (SerialBT.connect(remoteName)) {
                Serial.println("Reconnected!");
            }
        }

        // Read incoming data
        if (SerialBT.available()) {
            // Reads the string until a newline character
            String data = SerialBT.readStringUntil('\n');
            currentAngle = data.toFloat();
        }
    }

    bool isConnected() {
        return SerialBT.connected();
    }

    float getAngle() {
        return currentAngle;
    }
};

#endif