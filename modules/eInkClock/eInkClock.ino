// Analog clock on an eInk display, driven by the Bluepill's hardware RTC
// Display module: **MH-ET Live 1.54-inch** e-paper module (200 x 200 pixels)

// Board: STM32F103C8T6

// Required libraries:
// arduino-cli lib install "GxEPD2"          (pulls in Adafruit GFX + BusIO)
//
// The face is redrawn once a minute with a partial update (no flashing), and
// once an hour with a full refresh to clear the ghosting those partial updates
// accumulate. That matches the panel care notes in ../eInk/README.md: at 60
// partial updates and 1 full refresh per hour this is gentle enough to leave
// running, unlike the eInk demo next door.
//
// Set the time by sending "HH:MM" (or "HH:MM:SS") over Serial at 115200 --
// which is USART1 on PA9/PA10, not the onboard USB port, so it needs a
// USB-TTL adapter. Failing that, the RTC is seeded from the build time, which
// is a minute or two behind by the time flashing finishes.

#include <SPI.h>
#include <GxEPD2_BW.h>
#include <Fonts/FreeSansBold9pt7b.h>

// ---------------------------------------------------------------------------
// Panel selection
// ---------------------------------------------------------------------------
// Current MH-ET Live 1.54" modules use an SSD1681 controller (GxEPD2_154_D67).
// Older ones shipped with an IL3829/SSD1608 (GxEPD2_154). See ../eInk/eInk.ino.
#define PANEL_IL3829 0

#if PANEL_IL3829
  #define PANEL_CLASS GxEPD2_154
#else
  #define PANEL_CLASS GxEPD2_154_D67
#endif

// ---------------------------------------------------------------------------
// Display connection (SPI1) -- identical to ../eInk/eInk.ino
// ---------------------------------------------------------------------------
// CLK -> PA5, SDI/DIN -> PA7, CS -> PA4, DC -> PA3, RST -> PA2, BUSY -> PA1
// GND -> GND, VCC -> 3.3V (never 5V: PA0-PA7 are not 5V tolerant on the F103).

#define EPD_CS   PA4
#define EPD_DC   PA3
#define EPD_RST  PA2
#define EPD_BUSY PA1

#define PAGE_HEIGHT (PANEL_CLASS::HEIGHT / 4)

GxEPD2_BW<PANEL_CLASS, PAGE_HEIGHT> display(
    PANEL_CLASS(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY));

// ---------------------------------------------------------------------------
// Clock face geometry
// ---------------------------------------------------------------------------
// Radii are measured from the centre outwards, so the face reads from the
// outside in: bezel ring, minute track, numerals, then the hands.
#define CENTER_X       100
#define CENTER_Y       100

#define BEZEL_OUTER_R   99
#define BEZEL_INNER_R   96   // the ring is the band between these two

#define TICK_OUTER_R    92
#define MINUTE_TICK_R   87   // minute ticks are short
#define HOUR_TICK_R     80   // hour ticks reach further in, and are wider
#define HOUR_TICK_HALFW  2

#define NUMERAL_R       66   // centre of each numeral's bounding box

#define HOUR_HAND_LEN   54
#define HOUR_HAND_HALFW  5
#define HOUR_HAND_TAIL  14

#define MIN_HAND_LEN    84
#define MIN_HAND_HALFW   4
#define MIN_HAND_TAIL   16

#define HUB_R            5   // the pivot the hands turn on

// ---------------------------------------------------------------------------
// Hardware RTC
// ---------------------------------------------------------------------------
// The F103's RTC is a plain 32-bit second counter in the backup domain, so it
// keeps running across a reset (and across power loss, if VBAT is battery
// backed). A magic value in a backup register marks it as already configured,
// so re-flashing does not silently reset the clock back to the build time.
#define RTC_CONFIGURED_MAGIC 0x4B10  // arbitrary; just has to be recognisable

static RTC_HandleTypeDef hrtc;
static bool rtcReady = false;
static bool rtcOnLSE = false;

// Parses the build time ("HH:MM:SS") so a freshly flashed board shows
// something plausible before anyone sets it over Serial.
static void seedFromBuildTime(RTC_TimeTypeDef *t) {
  const char *b = __TIME__;
  t->Hours   = uint8_t((b[0] - '0') * 10 + (b[1] - '0'));
  t->Minutes = uint8_t((b[3] - '0') * 10 + (b[4] - '0'));
  t->Seconds = uint8_t((b[6] - '0') * 10 + (b[7] - '0'));
}

static bool rtcBegin() {
  // The backup domain is write protected out of reset; PWR and BKP must be
  // clocked and DBP set before anything here will stick.
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_RCC_BKP_CLK_ENABLE();
  HAL_PWR_EnableBkUpAccess();

  hrtc.Instance = RTC;
  hrtc.Init.AsynchPrediv = RTC_AUTO_1_SECOND;  // HAL derives the prescaler
  hrtc.Init.OutPut = RTC_OUTPUTSOURCE_NONE;    // no tamper/calibration output

  bool alreadySet =
      (HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR1) == RTC_CONFIGURED_MAGIC);

  if (!alreadySet) {
    // Selecting the RTC source is only possible once per backup-domain reset,
    // so clear the domain first. This is also what wipes a previously set time,
    // hence doing it only when the magic value is absent.
    __HAL_RCC_BACKUPRESET_FORCE();
    __HAL_RCC_BACKUPRESET_RELEASE();

    RCC_OscInitTypeDef osc = {};
    osc.OscillatorType = RCC_OSCILLATORTYPE_LSE;
    osc.PLL.PLLState = RCC_PLL_NONE;  // leave the system clock alone
    osc.LSEState = RCC_LSE_ON;
    rtcOnLSE = (HAL_RCC_OscConfig(&osc) == HAL_OK);

    if (!rtcOnLSE) {
      // Plenty of Bluepill clones ship without the 32.768 kHz crystal, or with
      // one that never starts. LSI keeps the demo running, but it is an RC
      // oscillator good to only a few percent -- minutes of drift per day.
      osc.OscillatorType = RCC_OSCILLATORTYPE_LSI;
      osc.LSEState = RCC_LSE_OFF;
      osc.LSIState = RCC_LSI_ON;
      if (HAL_RCC_OscConfig(&osc) != HAL_OK) return false;
    }

    RCC_PeriphCLKInitTypeDef pk = {};
    pk.PeriphClockSelection = RCC_PERIPHCLK_RTC;
    pk.RTCClockSelection =
        rtcOnLSE ? RCC_RTCCLKSOURCE_LSE : RCC_RTCCLKSOURCE_LSI;
    if (HAL_RCCEx_PeriphCLKConfig(&pk) != HAL_OK) return false;
  } else {
    // The source selection lives in BDCR and survived the reset; just note
    // which one is running so the startup banner is accurate.
    rtcOnLSE = (__HAL_RCC_GET_RTC_SOURCE() == RCC_RTCCLKSOURCE_LSE);
  }

  __HAL_RCC_RTC_ENABLE();
  if (HAL_RTC_Init(&hrtc) != HAL_OK) return false;

  if (!alreadySet) {
    RTC_TimeTypeDef t = {};
    seedFromBuildTime(&t);
    if (HAL_RTC_SetTime(&hrtc, &t, RTC_FORMAT_BIN) != HAL_OK) return false;
    HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR1, RTC_CONFIGURED_MAGIC);
  }
  return true;
}

static bool rtcGet(uint8_t *hh, uint8_t *mm, uint8_t *ss) {
  RTC_TimeTypeDef t = {};
  if (HAL_RTC_GetTime(&hrtc, &t, RTC_FORMAT_BIN) != HAL_OK) return false;
  *hh = t.Hours;
  *mm = t.Minutes;
  *ss = t.Seconds;
  return true;
}

static void rtcSet(uint8_t hh, uint8_t mm, uint8_t ss) {
  RTC_TimeTypeDef t = {};
  t.Hours = hh;
  t.Minutes = mm;
  t.Seconds = ss;
  HAL_RTC_SetTime(&hrtc, &t, RTC_FORMAT_BIN);
}

// Accepts "HH:MM" or "HH:MM:SS" on Serial. Anything else is ignored rather
// than reported, so line noise on a floating RX pin cannot set the clock.
static void pollSerialTimeSet() {
  static char buf[16];
  static uint8_t len = 0;

  while (Serial.available()) {
    char c = char(Serial.read());
    if (c == '\n' || c == '\r') {
      buf[len] = '\0';
      int hh, mm, ss = 0;
      int n = sscanf(buf, "%d:%d:%d", &hh, &mm, &ss);
      if ((n == 2 || n == 3) && hh >= 0 && hh < 24 && mm >= 0 && mm < 60 &&
          ss >= 0 && ss < 60) {
        rtcSet(uint8_t(hh), uint8_t(mm), uint8_t(ss));
        Serial.print(F("clock set to "));
        Serial.println(buf);
      }
      len = 0;
    } else if (len < sizeof(buf) - 1) {
      buf[len++] = c;
    } else {
      len = 0;  // overlong line: drop it rather than truncate into a valid time
    }
  }
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------

// The time being drawn. Captured once per update, because GxEPD2's paged
// drawing runs the draw code once per page -- if these were read from the RTC
// inside the callback, a minute rolling over mid-draw would leave the hands in
// different positions on different bands of the screen.
static uint8_t drawHour = 0;
static uint8_t drawMinute = 0;

// 0 degrees is 12 o'clock, angles increase clockwise.
static void polar(float deg, float r, int16_t *x, int16_t *y) {
  float a = deg * float(PI) / 180.0f;
  *x = int16_t(CENTER_X + lroundf(r * sinf(a)));
  *y = int16_t(CENTER_Y - lroundf(r * cosf(a)));
}

// A radial bar of a given width, as two triangles. Used for the hour ticks and
// both hands; Adafruit_GFX has no thick-line primitive.
static void fillRadialBar(float deg, float rInner, float rOuter,
                          float halfWidth, uint16_t color) {
  float a = deg * float(PI) / 180.0f;
  float ux = sinf(a), uy = -cosf(a);   // outward along the radius
  float px = -uy, py = ux;             // perpendicular to it

  int16_t x0 = int16_t(CENTER_X + lroundf(ux * rInner + px * halfWidth));
  int16_t y0 = int16_t(CENTER_Y + lroundf(uy * rInner + py * halfWidth));
  int16_t x1 = int16_t(CENTER_X + lroundf(ux * rOuter + px * halfWidth));
  int16_t y1 = int16_t(CENTER_Y + lroundf(uy * rOuter + py * halfWidth));
  int16_t x2 = int16_t(CENTER_X + lroundf(ux * rOuter - px * halfWidth));
  int16_t y2 = int16_t(CENTER_Y + lroundf(uy * rOuter - py * halfWidth));
  int16_t x3 = int16_t(CENTER_X + lroundf(ux * rInner - px * halfWidth));
  int16_t y3 = int16_t(CENTER_Y + lroundf(uy * rInner - py * halfWidth));

  display.fillTriangle(x0, y0, x1, y1, x2, y2, color);
  display.fillTriangle(x0, y0, x2, y2, x3, y3, color);
}

// Centres the text's bounding box on (cx, cy) rather than on the cursor,
// which is what makes the numerals sit evenly around the dial.
static void drawTextCenteredAt(const char *text, int16_t cx, int16_t cy) {
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  display.setCursor(cx - (x1 + int16_t(w) / 2), cy - (y1 + int16_t(h) / 2));
  display.print(text);
}

// A tapered hand: a long point one way, a short counterweight the other.
static void drawHand(float deg, int16_t length, int16_t halfWidth,
                     int16_t tail) {
  int16_t tipX, tipY, leftX, leftY, rightX, rightY, tailX, tailY;
  polar(deg, length, &tipX, &tipY);
  polar(deg - 90.0f, halfWidth, &leftX, &leftY);
  polar(deg + 90.0f, halfWidth, &rightX, &rightY);
  polar(deg + 180.0f, tail, &tailX, &tailY);

  display.fillTriangle(tipX, tipY, leftX, leftY, rightX, rightY, GxEPD_BLACK);
  display.fillTriangle(leftX, leftY, rightX, rightY, tailX, tailY, GxEPD_BLACK);
}

static void drawClockFace() {
  display.fillScreen(GxEPD_WHITE);
  display.setTextColor(GxEPD_BLACK);

  // Bezel: concentric circles rather than one thick stroke, since GFX only
  // draws single-pixel outlines.
  for (int16_t r = BEZEL_INNER_R; r <= BEZEL_OUTER_R; r++) {
    display.drawCircle(CENTER_X, CENTER_Y, r, GxEPD_BLACK);
  }

  // Minute track: 60 ticks, every fifth one long and wide.
  for (uint8_t i = 0; i < 60; i++) {
    float a = i * 6.0f;
    if (i % 5 == 0) {
      fillRadialBar(a, HOUR_TICK_R, TICK_OUTER_R, HOUR_TICK_HALFW, GxEPD_BLACK);
    } else {
      int16_t x0, y0, x1, y1;
      polar(a, MINUTE_TICK_R, &x0, &y0);
      polar(a, TICK_OUTER_R, &x1, &y1);
      display.drawLine(x0, y0, x1, y1, GxEPD_BLACK);
    }
  }

  // Numerals 1..12, each centred on its own hour angle.
  display.setFont(&FreeSansBold9pt7b);
  for (uint8_t n = 1; n <= 12; n++) {
    int16_t x, y;
    polar(n * 30.0f, NUMERAL_R, &x, &y);
    char label[3];
    snprintf(label, sizeof(label), "%u", n);
    drawTextCenteredAt(label, x, y);
  }

  // Hands. The hour hand advances smoothly through the hour (0.5 deg/min)
  // instead of jumping, which is what stops it looking wrong at, say, 5:59.
  float minuteAngle = drawMinute * 6.0f;
  float hourAngle = (drawHour % 12) * 30.0f + drawMinute * 0.5f;

  drawHand(hourAngle, HOUR_HAND_LEN, HOUR_HAND_HALFW, HOUR_HAND_TAIL);
  drawHand(minuteAngle, MIN_HAND_LEN, MIN_HAND_HALFW, MIN_HAND_TAIL);

  // Pivot, drawn last so it sits on top of both hands.
  display.fillCircle(CENTER_X, CENTER_Y, HUB_R, GxEPD_BLACK);
  display.fillCircle(CENTER_X, CENTER_Y, HUB_R - 3, GxEPD_WHITE);
}

// ---------------------------------------------------------------------------
// Refresh
// ---------------------------------------------------------------------------

static void showClock(uint8_t hh, uint8_t mm, bool fullRefresh) {
  drawHour = hh;
  drawMinute = mm;

  if (fullRefresh) {
    display.setFullWindow();
  } else {
    // The hands sweep the whole dial, so the partial window is the whole
    // screen. It is still a partial update: no flashing, just slower ghosting.
    display.setPartialWindow(0, 0, display.width(), display.height());
  }

  display.firstPage();
  do {
    drawClockFace();
  } while (display.nextPage());

  // The image is bistable, so the panel holds it with no power at all between
  // updates. hibernate() also drops the controller into deep sleep; GxEPD2
  // re-initialises it on the next draw, which is why RST has to be wired.
  display.hibernate();
}

// ---------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);

  // Workaround for STM32 core 3.0.0 never initialising SPI at default
  // settings: SPIClass::configSpi() only calls spi_init() when the requested
  // settings differ from the stored ones, and those start out as exactly what
  // SPI.begin() and GxEPD2 both ask for (4 MHz / MODE0 / MSB first). Without
  // this the SPI1 clock is never gated on, PA5/PA6/PA7 stay floating inputs,
  // and the first transfer spins forever in spi_com.c's untimed
  // while (!LL_SPI_IsActiveFlag_TXE(...)). See ../eInk/README.md.
  SPI.begin();
  SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE1));
  SPI.endTransaction();

  rtcReady = rtcBegin();

  display.init(115200);
  display.setRotation(0);

  Serial.println();
  Serial.print(F("eInk clock, RTC "));
  if (!rtcReady) {
    Serial.println(F("FAILED to start"));
  } else {
    Serial.println(rtcOnLSE ? F("on LSE (32.768 kHz crystal)")
                            : F("on LSI -- expect drift, no LSE crystal"));
    Serial.println(F("send HH:MM or HH:MM:SS to set the time"));
  }
}

void loop() {
  if (!rtcReady) {
    Serial.println(F("RTC unavailable"));
    delay(5000);
    return;
  }

  pollSerialTimeSet();

  uint8_t hh, mm, ss;
  if (!rtcGet(&hh, &mm, &ss)) {
    delay(1000);
    return;
  }

  // 0xFF means "nothing on screen yet", which forces the first update to be a
  // full refresh -- the panel may be holding an arbitrary stored image.
  static uint8_t shownMinute = 0xFF;

  if (mm != shownMinute) {
    // Top of the hour gets the full refresh that clears the ghosting left by
    // the preceding 59 partial updates.
    bool full = (shownMinute == 0xFF) || (mm == 0);
    showClock(hh, mm, full);
    shownMinute = mm;
  }

  // Polling once a second is precise enough to land each update within a
  // second of the minute boundary, and leaves the panel alone in between.
  delay(1000);
}
