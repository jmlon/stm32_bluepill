# eInk display modules

## MH-ET Live e-paper (E-ink) display

> **Status:** notes only. This module does not contain driver code yet.

These notes target the **MH-ET Live 1.54-inch** e-paper module (200 × 200 pixels).
The **WeAct 1.54-inch** module is a compatible unit: same resolution, same SPI
signal set, and it is driven by the same libraries. Most of what follows applies
to either board, but always confirm details against the exact panel you have.

### Key specifications & features

* **Resolution / size:** 1.54-inch, 200 × 200 pixels. The same product family
  also includes 2.13-inch (e.g. 122 × 250) and 2.9-inch variants.
* **Controller:** SSD1681 on current units. Some older 1.54-inch modules ship
  with an IL3829 / SSD1608 instead — this changes which driver class you select.
* **Color options:** black-and-white (BW), three-color (black/white plus
  red or yellow), and newer four-color configurations.
* **Interface:** SPI, using `CS` (chip select), `DC` (data/command), `RST`
  (reset) and `BUSY` in addition to `SCK` and `MOSI`.
* **Operating voltage:** 3.3 V for both supply and logic signals.

### Using it with a microcontroller

The usual software stack is **GxEPD2** (controller-specific e-paper drivers)
on top of **Adafruit GFX** (drawing and text), from the Arduino IDE.

* **Wiring:** `VCC` to 3.3 V, `GND` to ground, and map `MOSI`, `SCK`, `CS`,
  `DC`, `RST` and `BUSY` to your Bluepill. See [Wiring](#wiring) below.
* **Driver class:** for a 1.54-inch 200 × 200 black/white panel, GxEPD2 uses
  `GxEPD2_154_D67` for the SSD1681 controller, and `GxEPD2_154` for the older
  IL3829 / SSD1608 units. Selecting the wrong one gives a blank or garbled
  screen even though the wiring is correct.
* **SPI clock:** the SSD1681 accepts roughly 20 MHz; GxEPD2 defaults to a
  conservative 4 MHz, which is a good starting point.
* **Refreshing:** full refreshes flash the screen to clear ghosting; partial
  updates redraw a region without flashing, and are best supported on
  black-and-white panels.

## Care

E-paper works by moving charged pigment particles inside microcapsules. The
image is bistable — it stays on screen with no power — but the panel is
sensitive both to how it is driven in software and to its physical environment.

### Refresh considerations

* Avoid refreshing more often than necessary. Frequent refreshes increase power
  consumption, mechanical/electrical stress, and visible wear. Use the
  manufacturer's recommended refresh interval and waveform. Full refreshes are
  often preferable when image quality matters more than speed.
* Use partial updates cautiously. Partial refreshing updates only changed pixels
  without flashing the whole screen, but it causes charge buildup and ghosting
  over time, so schedule a periodic full refresh. Partial-refresh support and
  the recommended frequency are panel- and controller-specific: follow the panel
  datasheet and the driver-library example.
* Do not assume that color panels support partial updates. Many require
  full-screen refreshes, while some support limited black/white partial updates.
  Use only the update modes documented for the exact panel and library driver.
* After an update completes and `BUSY` is inactive, call the driver's
  `powerOff()` — or `hibernate()`, if the exact controller and library driver
  support it — when the display will remain idle. The image stays visible
  without continuous refresh. `hibernate()` requires `RST` to be wired, and
  reinitialization may be required after waking.

### Handling & environment

* **Keep away from direct sunlight and UV.** Long-term exposure degrades the
  fluids inside the panel: colors fade, white areas yellow, and updates become
  slow and unreliable.
* **Respect 3.3 V logic.** The module requires a 3.3 V supply and 3.3 V logic.
  A Bluepill is already a 3.3 V part, so no level shifter is needed — but never
  feed the module 5 V, and never let a 5 V signal reach it. Note that on the
  STM32F103C8T6, `PA0`–`PA7` are *not* 5 V tolerant, which includes the SPI1
  pins used below.
* **Protect the glass panel.** The matrix layer is rigid but extremely thin
  glass. Do not apply pressure, bend, or drop the display: minor flexing can
  crack the internal glass while the outer plastic layer still looks intact.
* **Stay inside the rated temperature range.** E-paper fluid viscosity changes
  with temperature (typically 0 °C to 50 °C for black-and-white, 0 °C to 40 °C
  for tri-color). Updating below the rated range can leave incomplete images
  and permanent ghosting.

## Bluepill considerations

### Wiring

Common wiring for a STM32F103C8T6 Bluepill using SPI1:

| Display | Bluepill SPI1 example |
|---|---|
| `SCK` / `CLK` | `PA5` |
| `MOSI` / `DIN` | `PA7` |
| `CS` | Any suitable GPIO, for example `PA4` |
| `DC` | Any GPIO |
| `RST` | Any GPIO |
| `BUSY` | Any GPIO input |
| `GND` | `GND` |
| `VCC` | Verified 3.3 V supply |

`MISO` is normally not required for these displays because they are usually
write-only. `CS` is driven as a plain GPIO by GxEPD2, so it does not have to be
the SPI1 hardware NSS pin (`PA4`), though that is a convenient choice.

### RAM usage

The Bluepill has 20 KiB of SRAM. A full framebuffer for a black/white
200 × 200 panel is 200 × 200 / 8 = 5,000 bytes, so it does fit — but paged
drawing (GxEPD2's default) is still the better choice, since it leaves the rest
of the SRAM for the application. Three-color panels need two buffers
(≈ 10 KB) and make paged drawing effectively mandatory.

### BUSY / reset behavior

- Commands must not be sent while the display is busy.
- On the SSD1681, `BUSY` is active-high: high means busy. Other controllers
  differ, so confirm the polarity for the panel you have.
- Reset timing is controller/module-specific.
- Use a timeout rather than waiting forever, in case `BUSY` is disconnected
  or stuck.
- The display should normally receive an initial full refresh after power-up
  or reinitialization.
- SPI settings and maximum clock rate should follow the module/library
  documentation.

### Library

GxEPD2 provides controller-specific e-paper drivers and uses Adafruit GFX for
drawing and text. It can be used from the STM32 Arduino core. Native STM32 HAL
projects will require a different driver integration, or a port of the relevant
controller commands.

The GxEPD2 project is GPL-3.0 licensed, which may matter for projects that
redistribute firmware or library modifications.

---

## References

[GxEPD2 - Display Library for SPI E-Paper Displays](https://github.com/ZinggJM/GxEPD2)

[How to Use WeAct 1.54 E-paper module: Examples, Pinouts, and Specs](https://docs.cirkitdesigner.com/component/175fa3e3-2b65-451c-aa10-715821aab4fa) — the compatible WeAct unit
