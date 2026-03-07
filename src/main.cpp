#include <M5Unified.h>
#include "sensors/imu_sensor.hpp"
#include "ui/ink_display.hpp"
#include "utility/sdhandler.hpp"

//system objects
//IMU Sensor
IMUSensor imu;
InkDisplay display;
SDHandler SDHandlr;
//Air Density Calculator
//Display Manager
//Power state manager

void setup() {
  //config settings
  auto cfg = M5.config();
  cfg.serial_baudrate = 115200;
  cfg.external_imu = true;
  cfg.internal_imu = false;

  M5.begin(cfg);

  Serial.println("IMU Ready");
  //init screen here
  imu.init();
  display.initScreen();

  
}

void loop() {
  M5.update();

  if (!M5.Imu.isEnabled()) {
    Serial.println("IMU not detected");
    delay(1000);
    return;
  }

  imu.update();
  SDHandlr.initSDCard();
  
  imu.printToSerial();

  display.screenRefresh(imu, SDHandlr);
  //refresh screen with new imu values
  delay(20);

}