#include <SparkFunLSM6DS3.h>
#include <Wire.h>

// Create an instance specifying I2C mode and the new address
LSM6DS3 myIMU(I2C_MODE, 0x6A);

void setup() {
  Serial.begin(9600);
  if (myIMU.begin() != 0) {
    Serial.println("Device error or address mismatch!");
  } else {
    Serial.println("Device initialized at 0x6A");
  }
}
