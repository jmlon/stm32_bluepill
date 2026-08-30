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

// Board-mount-dependent axis mapping. The BMI160's physical orientation on
// this breakout relative to the display was not known ahead of flashing --
// if the horizon moves opposite to the actual tilt, flip the matching SIGN
// below (or swap which raw axis feeds pitch vs roll) rather than rewriting
// the filter.
#define PITCH_SIGN  1.0f
#define ROLL_SIGN   1.0f

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite horizon = TFT_eSprite(&tft);
DFRobot_BMI160 myIMU;

// Set once in setup(); checked every loop() so a failed/lost sensor is
// reported instead of silently drawing stale readings.
bool imuReady = false;

// Filled once in setup() by i2cScan() so the "sensor not detected" screen
// can keep showing what's actually on the bus, without needing Serial.
String i2cScanResult;

// Averaged at rest in setup() and subtracted from every gyro reading so a
// static sensor offset doesn't show up as slow horizon drift. Only the X and
// Y axes are needed here (pitch/roll); Z (yaw) is not integrated into the
// display, so no bias is collected for it.
float gyroBiasX = 0, gyroBiasY = 0;

// Complementary-filter state, updated every loop().
float pitchDeg = 0, rollDeg = 0;
uint32_t lastMicros = 0;

// Horizon sprite geometry: a square disk big enough to cover the display's
// 128px width/160px height with room to spare, palette-indexed (4bpp) to
// keep it small on a 20KB-SRAM part -- a 16-bit full-screen framebuffer
// would not fit.
#define DISK_SIZE   100
// Centre the 100px disk on the 128px-wide panel: CENTER_X + DISK_SIZE/2 must
// stay <= 127, and the true horizontal centre of the display is 64. Picking
// anything larger leaves the disk's right edge clipped off-screen.
#define CENTER_X    64
#define CENTER_Y    64
#define PX_PER_DEG  1.8f

#define PAL_BG      0
#define PAL_SKY     1
#define PAL_GROUND  2
#define PAL_LINE    3

// Per-row circle clip, precomputed once in setup(): the sprite is a square
// buffer, but only pixels within radius DISK_SIZE/2 of its centre are ever
// drawn, so a rotated copy always fills the same round footprint on the
// TFT -- rotation never exposes/uncovers square corners, so there's nothing
// to erase between frames.
int16_t rowX0[DISK_SIZE];
int16_t rowW[DISK_SIZE];

void drawRollIndicator() {
    const int16_t r = DISK_SIZE / 2;
    // This mark slightly overlaps the disk at its lower edge. Draw it after
    // pushRotated(), otherwise the sprite's background can erase that edge.
    tft.fillTriangle(CENTER_X, CENTER_Y - r - 6,
                     CENTER_X - 5, CENTER_Y - r + 2,
                     CENTER_X + 5, CENTER_Y - r + 2,
                     TFT_YELLOW);
}

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

// Averages gyro readings while the board is assumed still, so a fixed
// sensor offset doesn't get integrated into slow horizon drift.
void calibrateGyroBias() {
    const int samples = 200;
    double sumX = 0, sumY = 0;
    int count = 0;
    for (int i = 0; i < samples; i++) {
        int16_t raw[6] = {0};
        if (myIMU.getAccelGyroData(raw) == 0) {
            // Only average successful reads; a failed sample must not drag
            // the mean down by being counted as zero.
            sumX += raw[0] / IMU_GYRO_LSB_PER_DPS;
            sumY += raw[1] / IMU_GYRO_LSB_PER_DPS;
            count++;
        }
        delay(5);
    }
    if (count == 0) return;   // no valid samples: leave bias at rest (zero)
    gyroBiasX = sumX / count;
    gyroBiasY = sumY / count;
}

void setup() {

    // Initialize display
    tft.init();
    tft.setRotation(3);  // -90 degrees from the default portrait orientation
    tft.fillScreen(TFT_BLACK);

    tft.setCursor(10, 10);
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(1);
    tft.print("Attitude Indicator");

    i2cScan();

    // Initialize sensor. I2cInit() checks CHIP_ID (must be 0xD1) and does
    // its own internal reset -- calling softReset() first would target the
    // library's hardcoded default address (0x68) instead of ours (0x69).
    imuReady = (myIMU.I2cInit(IMU_I2C_ADDR) == BMI160_OK);

    tft.setCursor(10, 25);
    if (!imuReady) {
        tft.setTextColor(TFT_RED);
        tft.print("IMU Error!");
        delay(1000);
        return;
    }

    tft.setTextColor(TFT_GREEN);
    tft.print("IMU Ready");
    tft.setCursor(10, 35);
    tft.print("Calibrating, keep still...");
    calibrateGyroBias();

    // One-time clear: wipes the setup banner/status text above so none of
    // it lingers as garbage once the main loop starts doing partial redraws.
    tft.fillScreen(TFT_BLACK);

    // Palette-indexed sprite: index 0 is screen-background black (also used
    // to mask the sprite's square corners into a circle), 1-3 are the
    // horizon's sky, ground and horizon-line colours.
    horizon.setColorDepth(4);
    horizon.createSprite(DISK_SIZE, DISK_SIZE);
    horizon.createPalette((uint16_t *)nullptr);
    horizon.setPaletteColor(PAL_BG, TFT_BLACK);
    horizon.setPaletteColor(PAL_SKY, TFT_CYAN);
    horizon.setPaletteColor(PAL_GROUND, 0x6A00); // brown (RGB565)
    horizon.setPaletteColor(PAL_LINE, TFT_WHITE);

    // Sprite rotates about its own centre; pushRotated() places that centre
    // at the TFT's pivot point, set once here.
    tft.setPivot(CENTER_X, CENTER_Y);

    const int16_t r = DISK_SIZE / 2;
    for (int16_t y = 0; y < DISK_SIZE; y++) {
        int16_t dy = y - r;
        int32_t dxSq = (int32_t)r * r - (int32_t)dy * dy;
        if (dxSq < 0) { rowW[y] = 0; continue; }
        int16_t dx = (int16_t)sqrt((float)dxSq);
        rowX0[y] = r - dx;
        rowW[y] = 2 * dx;
    }

    // Roll-index triangle at the top of the dial. It overlaps the disk by a
    // few pixels, so it is also redrawn after each rotated sprite push.
    drawRollIndicator();

    lastMicros = micros();

    delay(500);
}

void loop() {
    if (!imuReady) {
        tft.fillScreen(TFT_BLACK);
        tft.setCursor(5, 10);
        tft.setTextColor(TFT_RED);
        tft.setTextSize(1);
        tft.print("Sensor not detected");
        tft.setCursor(5, 20);
        tft.setTextColor(TFT_YELLOW);
        tft.print("I2C scan:");
        tft.setCursor(5, 30);
        tft.print(i2cScanResult);
        delay(500);
        return;
    }

    // Read sensor values: index 0-2 gyro, index 3-5 accel (raw counts)
    int16_t raw[6] = {0};
    bool ok = myIMU.getAccelGyroData(raw) == 0;

    if (!ok) {
        tft.fillScreen(TFT_BLACK);
        tft.setCursor(5, 10);
        tft.setTextColor(TFT_RED);
        tft.setTextSize(1);
        tft.print("Read error");
        delay(500);
        return;
    }

    float gyroX = raw[0] / IMU_GYRO_LSB_PER_DPS - gyroBiasX;
    float gyroY = raw[1] / IMU_GYRO_LSB_PER_DPS - gyroBiasY;
    float accelX = raw[3] / IMU_ACCEL_LSB_PER_G;
    float accelY = raw[4] / IMU_ACCEL_LSB_PER_G;
    float accelZ = raw[5] / IMU_ACCEL_LSB_PER_G;

    uint32_t now = micros();
    float dt = (now - lastMicros) / 1000000.0f;
    lastMicros = now;

    // Accel-derived leveled angles (degrees), used to correct gyro drift.
    float pitchAcc = atan2(-accelX, sqrt(accelY * accelY + accelZ * accelZ)) * RAD_TO_DEG;
    float rollAcc  = atan2(accelY, accelZ) * RAD_TO_DEG;

    // Complementary filter: mostly trust the integrated gyro rate (smooth,
    // responsive), slowly pulled back toward the accelerometer's leveled
    // estimate (drift-free, but noisy under motion).
    pitchDeg = 0.98f * (pitchDeg + PITCH_SIGN * gyroY * dt) + 0.02f * pitchAcc;
    rollDeg  = 0.98f * (rollDeg  + ROLL_SIGN  * gyroX * dt) + 0.02f * rollAcc;

    // Redraw the horizon disk: sky above the pitch-shifted horizon line,
    // ground below it, a couple of pitch-ladder rungs either side of centre.
    // Every row is drawn via the precomputed circle clip (rowX0/rowW), so
    // the square corners around the disk always come out background-black
    // -- same as last frame -- instead of needing an erase pass.
    int16_t horizonY = (int16_t)(DISK_SIZE / 2.0f + pitchDeg * PX_PER_DEG);

    for (int16_t y = 0; y < DISK_SIZE; y++) {
        if (rowW[y] <= 0) continue;
        horizon.drawFastHLine(rowX0[y], y, rowW[y], (y < horizonY) ? PAL_SKY : PAL_GROUND);
    }
    if (horizonY >= 0 && horizonY < DISK_SIZE && rowW[horizonY] > 0) {
        horizon.drawFastHLine(rowX0[horizonY], horizonY, rowW[horizonY], PAL_LINE);
    }

    for (int rung = -20; rung <= 20; rung += 10) {
        if (rung == 0) continue;
        int16_t y = (int16_t)(horizonY - rung * PX_PER_DEG);
        if (y < 0 || y >= DISK_SIZE || rowW[y] <= 0) continue;
        int16_t half = min((int16_t)15, (int16_t)(rowW[y] / 2));
        horizon.drawFastHLine(DISK_SIZE / 2 - half, y, 2 * half, PAL_LINE);
    }

    horizon.pushRotated(-rollDeg);

    // Fixed (non-rotating) reference symbols drawn on top of the horizon
    // every frame, since the sprite push above can overwrite their area.
    tft.drawFastHLine(CENTER_X - 25, CENTER_Y, 18, TFT_YELLOW);
    tft.drawFastHLine(CENTER_X + 7, CENTER_Y, 18, TFT_YELLOW);
    tft.fillCircle(CENTER_X, CENTER_Y, 2, TFT_YELLOW);
    drawRollIndicator();
}
