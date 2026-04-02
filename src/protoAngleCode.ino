#include "BluetoothSerial.h"
#include <Adafruit_LIS3DH.h>
#include <Adafruit_Sensor.h>

Adafruit_LIS3DH lis = Adafruit_LIS3DH();
BluetoothSerial SerialBT;
float pitch_offset = 0; //for calibration

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);
  SerialBT.begin("ESP32_protoAngle");

  if (!lis.begin(0x18)) {
    Serial.println("Could not start LIS3DH");
    while (1);
  }

  lis.setRange(LIS3DH_RANGE_2_G);

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

  Serial.println("Angle via Bluetooth ready");
}
void loop() {
  lis.read();

  float x = lis.x;
  float y = lis.y;
  float z = lis.z;

  //convert to g's
  x /= 1000.0;
  y /= 1000.0;
  z /= 1000.0;

  //tilt angles
  float pitch = atan2(x, sqrt(y*y + z*z)) * 180.0 / PI;

  //apply calibration
  pitch -= pitch_offset;

  //send via bluetooth
  SerialBT.print("Pitch: ");
  SerialBT.print(pitch);
  SerialBT.println(" deg");

  delay(200);
}

