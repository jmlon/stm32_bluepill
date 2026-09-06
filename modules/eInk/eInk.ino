// Demo program for eInk display on STM32 Bluepill
// Display module: **MH-ET Live 1.54-inch** e-paper module (200 × 200 pixels)

// Board: STM32F103C8T6

// Required libraries:
// arduino-cli lib install "GxEPD2"          (pulls in Adafruit GFX + BusIO)
//
// Cycles through four screens to exercise the panel: an info splash, GFX
// primitives, text/fonts, and a partial-update counter. See README.md in
// this directory for panel care notes -- in particular, this demo is meant
// to be run for a while and then stopped, not left refreshing for days.

#include <GxEPD2_BW.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>

// ---------------------------------------------------------------------------
// Panel selection
// ---------------------------------------------------------------------------
// Current MH-ET Live 1.54" modules use an SSD1681 controller (GxEPD2_154_D67).
// Older ones shipped with an IL3829/SSD1608 (GxEPD2_154), which needs a
// different init sequence: the wiring is identical, but picking the wrong
// class gives a blank or garbled screen. If nothing appears with the default
// below, switch PANEL_IL3829 to 1 before suspecting the wiring.
#define PANEL_IL3829 0

#if PANEL_IL3829
  #define PANEL_CLASS GxEPD2_154
  #define PANEL_NAME  "IL3829"
#else
  #define PANEL_CLASS GxEPD2_154_D67
  #define PANEL_NAME  "SSD1681"
#endif

// ---------------------------------------------------------------------------
// Display connection (SPI1)
// ---------------------------------------------------------------------------
// CLK  -> PA5       SPI1 Serial Clock (mandatory hardware pin)
// DIN  -> PA7       SPI1 Master Out Slave In (mandatory hardware pin)
// CS   -> PA4       Chip Select (driven as plain GPIO, need not be SPI1 NSS)
// DC   -> PA3       Data / Command select (any GPIO)
// RST  -> PA2       Reset (any GPIO; required for hibernate/wake)
// BUSY -> PA1       Busy, active HIGH on SSD1681 (any GPIO input)
// GND  -> GND
// VCC  -> 3.3V      Never 5V: PA0-PA7 are not 5V tolerant on the F103.
//
// MISO is unused -- these panels are write-only.

#define EPD_CS   PA4
#define EPD_DC   PA3
#define EPD_RST  PA2
#define EPD_BUSY PA1

// The Bluepill has 20 KiB of SRAM. A full 200x200 1bpp framebuffer is 5000
// bytes, which does fit, but GxEPD2's paged drawing keeps that down to
// 200 * 50 / 8 = 1250 bytes per page (4 passes over the draw code) and leaves
// the rest of RAM for the application. See README.md, "RAM usage".
#define PAGE_HEIGHT (PANEL_CLASS::HEIGHT / 4)

GxEPD2_BW<PANEL_CLASS, PAGE_HEIGHT> display(
    PANEL_CLASS(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY));

// ---------------------------------------------------------------------------
// Demo pacing
// ---------------------------------------------------------------------------
// E-paper wears with every refresh, so the demo deliberately waits between
// screens rather than looping as fast as the panel allows.
#define SCREEN_HOLD_MS   8000
#define PARTIAL_STEPS    10
#define PARTIAL_PERIOD_MS 1500

uint8_t screenIndex = 0;

// ---------------------------------------------------------------------------
// Drawing helpers
// ---------------------------------------------------------------------------

// Horizontally centres text on the current font/size at baseline y.
// getTextBounds() reports the bounding box relative to the cursor, so x1 has
// to be subtracted back out to land the glyphs (not the cursor) in the middle.
static void drawCentered(const char *text, int16_t y) {
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(text, 0, y, &x1, &y1, &w, &h);
  display.setCursor((display.width() - int16_t(w)) / 2 - x1, y);
  display.print(text);
}

// Screen 1: what this is and what it's talking to.
static void drawSplash() {
  display.fillScreen(GxEPD_WHITE);
  display.setTextColor(GxEPD_BLACK);

  display.drawRect(0, 0, display.width(), display.height(), GxEPD_BLACK);
  display.drawRect(3, 3, display.width() - 6, display.height() - 6, GxEPD_BLACK);

  display.setFont(&FreeSansBold12pt7b);
  drawCentered("MH-ET Live", 45);
  display.setFont(&FreeSans9pt7b);
  drawCentered("1.54\" e-paper", 70);

  display.drawFastHLine(25, 85, display.width() - 50, GxEPD_BLACK);

  display.setFont(&FreeSans9pt7b);
  drawCentered("200 x 200 px", 110);
  drawCentered(PANEL_NAME, 132);

  display.drawFastHLine(25, 147, display.width() - 50, GxEPD_BLACK);

  display.setFont(NULL);  // built-in 5x7 font
  display.setTextSize(1);
  drawCentered("STM32F103C8T6 + GxEPD2", 165);
  drawCentered("SPI1 @ PA5/PA7", 180);
}

// Screen 2: GFX primitives, outlined on the left, filled on the right.
static void drawShapes() {
  display.fillScreen(GxEPD_WHITE);
  display.setTextColor(GxEPD_BLACK);
  display.setFont(&FreeSansBold9pt7b);
  drawCentered("GFX primitives", 18);
  display.drawFastHLine(0, 24, display.width(), GxEPD_BLACK);

  display.drawRect(10, 32, 80, 38, GxEPD_BLACK);
  display.fillRect(110, 32, 80, 38, GxEPD_BLACK);

  display.drawRoundRect(10, 76, 80, 38, 8, GxEPD_BLACK);
  display.fillRoundRect(110, 76, 80, 38, 8, GxEPD_BLACK);

  display.drawCircle(35, 140, 22, GxEPD_BLACK);
  display.fillCircle(100, 140, 22, GxEPD_BLACK);
  display.drawTriangle(140, 162, 165, 118, 190, 162, GxEPD_BLACK);

  // A fan of lines, to show how cleanly the panel renders 1px diagonals.
  for (int16_t x = 0; x < display.width(); x += 12) {
    display.drawLine(100, display.height() - 2, x, 170, GxEPD_BLACK);
  }
}

// Screen 3: the built-in font at several sizes, plus the FreeFonts.
static void drawText() {
  display.fillScreen(GxEPD_WHITE);
  display.setTextColor(GxEPD_BLACK);

  display.setFont(&FreeSansBold9pt7b);
  drawCentered("Text & fonts", 18);
  display.drawFastHLine(0, 24, display.width(), GxEPD_BLACK);

  display.setFont(NULL);
  display.setTextSize(1);
  display.setCursor(6, 36);
  display.print("Built-in 5x7, size 1");
  display.setTextSize(2);
  display.setCursor(6, 50);
  display.print("Size 2");
  display.setTextSize(3);
  display.setCursor(6, 72);
  display.print("Size 3");
  display.setTextSize(1);

  display.drawFastHLine(6, 104, display.width() - 12, GxEPD_BLACK);

  display.setFont(&FreeSans9pt7b);
  display.setCursor(6, 125);
  display.print("FreeSans 9pt");
  display.setFont(&FreeSansBold9pt7b);
  display.setCursor(6, 148);
  display.print("FreeSansBold 9pt");
  display.setFont(&FreeSansBold12pt7b);
  display.setCursor(6, 176);
  display.print("Bold 12pt");

  // Inverted text: white glyphs knocked out of a black bar.
  display.fillRect(0, 185, display.width(), 15, GxEPD_BLACK);
  display.setFont(NULL);
  display.setTextColor(GxEPD_WHITE);
  display.setCursor(6, 189);
  display.print("Inverted text on black");
  display.setTextColor(GxEPD_BLACK);
}

// Static part of screen 4; the counter box itself is drawn separately so it
// can be refreshed on its own.
static void drawPartialFrame() {
  display.fillScreen(GxEPD_WHITE);
  display.setTextColor(GxEPD_BLACK);

  display.setFont(&FreeSansBold9pt7b);
  drawCentered("Partial update", 18);
  display.drawFastHLine(0, 24, display.width(), GxEPD_BLACK);

  display.setFont(NULL);
  display.setTextSize(1);
  drawCentered("Only the box below is", 45);
  drawCentered("rewritten -- no flash.", 58);

  display.drawRect(30, 75, 140, 60, GxEPD_BLACK);

  drawCentered("A full refresh follows,", 165);
  drawCentered("to clear any ghosting.", 178);
}

// The counter, drawn inside the box outlined by drawPartialFrame().
static void drawCounter(uint8_t value) {
  display.fillScreen(GxEPD_WHITE);
  display.setTextColor(GxEPD_BLACK);
  display.setFont(&FreeSansBold12pt7b);

  char buf[8];
  snprintf(buf, sizeof(buf), "%u", value);
  drawCentered(buf, 118);
}

// ---------------------------------------------------------------------------
// Refresh helpers
// ---------------------------------------------------------------------------

// Full-screen refresh: flashes the panel and leaves it free of ghosting.
// The draw callback runs once per page, so it must be repeatable -- it may
// not consume a stream, advance a counter, or read a sensor.
static void showFullScreen(void (*draw)()) {
  display.setFullWindow();
  display.firstPage();
  do {
    draw();
  } while (display.nextPage());
}

// Same, but only the given rectangle is rewritten. Cheap and flicker-free,
// at the cost of accumulating ghosting -- always follow a run of these with
// a full refresh.
static void showPartial(void (*draw)(uint8_t), uint8_t value,
                        int16_t x, int16_t y, int16_t w, int16_t h) {
  display.setPartialWindow(x, y, w, h);
  display.firstPage();
  do {
    draw(value);
  } while (display.nextPage());
}

// Screen 4 runs a small sequence of its own rather than a single paint.
static void runPartialDemo() {
  showFullScreen(drawPartialFrame);

  for (uint8_t i = 1; i <= PARTIAL_STEPS; i++) {
    showPartial(drawCounter, i, 31, 76, 138, 58);
    delay(PARTIAL_PERIOD_MS);
  }

  // Ghosting builds up over a run of partial updates, so clear it before
  // moving on. This is the habit to keep in real applications too.
  showFullScreen(drawPartialFrame);
}

// ---------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);

  // init(115200) forwards GxEPD2's own diagnostics to Serial, which is where
  // a BUSY line that never goes inactive will show up (the library gives up
  // on its internal timeout rather than hanging forever).
  display.init(115200);
  display.setRotation(0);

  Serial.println();
  Serial.print(F("e-paper demo: "));
  Serial.print(display.width());
  Serial.print('x');
  Serial.print(display.height());
  Serial.print(F(", controller "));
  Serial.println(F(PANEL_NAME));
  Serial.print(F("fast partial update: "));
  Serial.println(display.epd2.hasFastPartialUpdate ? F("yes") : F("no"));

  // A panel coming out of storage can hold an arbitrary image; one full
  // refresh puts it into a known state before the demo starts.
  display.clearScreen();
}

void loop() {
  switch (screenIndex) {
    case 0: showFullScreen(drawSplash); break;
    case 1: showFullScreen(drawShapes); break;
    case 2: showFullScreen(drawText);   break;
    case 3: runPartialDemo();           break;
  }
  screenIndex = (screenIndex + 1) % 4;

  // Power the panel down between screens. The image stays on the glass with
  // no power at all; hibernate() also drops the controller into deep sleep,
  // and GxEPD2 re-initialises it automatically on the next draw (which is
  // why RST has to be wired).
  display.hibernate();

  delay(SCREEN_HOLD_MS);
}
