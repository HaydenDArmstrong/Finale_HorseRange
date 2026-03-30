#include <M5Unified.h>
#include "sensors/imu_sensor.hpp"
#include "ui/ink_display.hpp"
#include "utility/sdhandler.hpp"

bool tableLoaded = false;
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
  imu.Calib();
  display.initScreen();
  SDHandlr.initSDCard();

  
}

void loop() {

  const AccelVector& a = imu.getAccel();

  M5.update();

  if (!M5.Imu.isEnabled()) {
    Serial.println("IMU not detected");
    delay(1000);
    return;
  }

  imu.update();

   if (!tableLoaded) {
    float rho = imu.airDensityCalc(imu);
    Serial.printf("rho = %.4f\n", rho);
    SDHandlr.csvRead(rho);
    tableLoaded = true;
  }

  float angle = a.y*90; //from IMU;
  float mass = 6.0; //user input
float distance = SDHandlr.lookupDistance(angle, mass);
  
  imu.printToSerial();

  display.screenRefresh(imu, SDHandlr, angle, mass, distance);
  //refresh screen with new imu values
  delay(5000);

}