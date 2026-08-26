#include <Wire.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <DFRobot_BMI160.h>

// Board: STM32F103C8T6

// Required libraries:
// arduino-cli lib install "DFRobot_BMI160"
// arduino-cli lib install "TFT_eSPI"
//
// The board wired here was sold/labeled as an LSM6DS3 breakout, but the
// LSM6DS3, MPU6050 and ICM42688 WHO_AM_I locations all came back wrong.
// A raw register dump of 0x00-0x1F showed register 0x00 = 0xD1, which is
// Bosch's documented CHIP_ID for the BMI160 -- confirmed by SENSORTIME
// (0x18-0x1A) being a live, nonzero free-running counter. It's a BMI160.
//
// TFT_eSPI display/pin configuration is passed at compile time via
// build-flags.txt (USER_SETUP_LOADED) instead of editing the library.

// Display connection:
// CS  -> PA4        SPI Chip Select (Default hardware pin)
// RES -> PA2        Display Reset (Can map to any GPIO)
// DC  -> PA3        Data / Command Select (Can map to any GPIO)
// SDA -> PA7 = SPI1 Master Out Slave In (Mandatory Hardware Pin)
// SCL -> PA5SPI1 Serial Clock (Mandatory Hardware Pin)

// IMU connection (I2C)
// PB7 -> SDA
// PB6 -> SCL
// SDIO/SA0 -> VCC (I2C address 0x69; tie to GND instead for 0x68)

#define IMU_I2C_ADDR 0x69

// Default power-on full-scale ranges: accel +/-2g, gyro +/-2000 dps.
#define IMU_ACCEL_LSB_PER_G   16384.0
#define IMU_GYRO_LSB_PER_DPS  16.4

TFT_eSPI tft = TFT_eSPI();
DFRobot_BMI160 myIMU;

// Set once in setup(); checked every loop() so a failed/lost sensor is
// reported instead of silently drawing stale readings.
bool imuReady = false;

// Filled once in setup() by i2cScan() so the "sensor not detected" screen
// can keep showing what's actually on the bus, without needing Serial.
String i2cScanResult;

void i2cScan() {
    Wire.begin();
    int found = 0;
    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            if (found > 0) i2cScanResult += ", ";
            if (addr < 16) i2cScanResult += "0";
            i2cScanResult += String(addr, HEX);
            found++;
        }
    }
    if (found == 0) {
        i2cScanResult = "no devices found";
    }
}

void setup() {

    // Initialize display
    tft.init();
    tft.setRotation(3);  // -90 degrees from the default portrait orientation
    tft.fillScreen(TFT_BLACK);

    tft.setCursor(10, 10);
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(1);
    tft.print("IMU Demo");

    i2cScan();

    // Initialize sensor. I2cInit() checks CHIP_ID (must be 0xD1) and does
    // its own internal reset -- calling softReset() first would target the
    // library's hardcoded default address (0x68) instead of ours (0x69).
    imuReady = (myIMU.I2cInit(IMU_I2C_ADDR) == BMI160_OK);

    tft.setCursor(10, 25);
    if (!imuReady) {
        tft.setTextColor(TFT_RED);
        tft.print("IMU Error!");
    } else {
        tft.setTextColor(TFT_GREEN);
        tft.print("IMU Ready");
    }

    delay(1000);
}

void loop() {
    static uint32_t frame = 0;
    frame++;

    // Clear display for next frame
    tft.fillScreen(TFT_BLACK);

    // Frame counter -- proves the loop/display are actually refreshing,
    // independent of whether the sensor is reporting real data.
    tft.setCursor(5, 5);
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(1);
    tft.print("Frame: ");
    tft.print(frame);

    if (!imuReady) {
        tft.setCursor(5, 20);
        tft.setTextColor(TFT_RED);
        tft.print("Sensor not detected");
        tft.setCursor(5, 30);
        tft.setTextColor(TFT_YELLOW);
        tft.print("I2C scan:");
        tft.setCursor(5, 40);
        tft.print(i2cScanResult);
        delay(500);
        return;
    }

    // Read sensor values: index 0-2 gyro, index 3-5 accel (raw counts)
    int16_t raw[6] = {0};
    bool ok = myIMU.getAccelGyroData(raw) == 0;

    if (!ok) {
        tft.setCursor(5, 20);
        tft.setTextColor(TFT_RED);
        tft.print("Read error");
        delay(500);
        return;
    }

    float gyroX = raw[0] / IMU_GYRO_LSB_PER_DPS;
    float gyroY = raw[1] / IMU_GYRO_LSB_PER_DPS;
    float gyroZ = raw[2] / IMU_GYRO_LSB_PER_DPS;
    float accelX = raw[3] / IMU_ACCEL_LSB_PER_G;
    float accelY = raw[4] / IMU_ACCEL_LSB_PER_G;
    float accelZ = raw[5] / IMU_ACCEL_LSB_PER_G;

    // Draw title
    tft.setCursor(10, 15);
    tft.setTextColor(TFT_WHITE);
    tft.print("Accel (g)  Gyro (dps)");

    // Accelerometer data
    tft.setCursor(5, 30);
    tft.setTextColor(TFT_CYAN);
    tft.print("X:");
    tft.print(accelX, 2);
    tft.print("   ");
    tft.print(gyroX, 1);

    tft.setCursor(5, 40);
    tft.setTextColor(TFT_CYAN);
    tft.print("Y:");
    tft.print(accelY, 2);
    tft.print("   ");
    tft.print(gyroY, 1);

    tft.setCursor(5, 50);
    tft.setTextColor(TFT_CYAN);
    tft.print("Z:");
    tft.print(accelZ, 2);
    tft.print("   ");
    tft.print(gyroZ, 1);

    delay(500);
}
