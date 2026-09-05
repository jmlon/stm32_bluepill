# eInk display modules

## MH-ET Live e-paper (E-ink) display

### Key Specifications & Features
* Resolutions & Sizes: Available in popular sizes such as 1.54-inch (200 × 200 pixels) [0.67] and 2.13-inch (122 × 250 pixels) or 2.9-inch variants [0.63, 0.66].
* Color Options: Sold in black-and-white (BW), three-color (black, white, and red/yellow), and newer four-color configurations [0.63, 0.67].
* Interface: Uses the Serial Peripheral Interface (SPI) protocol for communication, requiring pins like CS (Chip Select), DC (Data/Command), RST (Reset), and BUSY [0.64, 0.6].
* Operating Voltage: Typically runs on 3.3V for both logic power and input signals [0.61].

### How to Use with Microcontrollers: Most makers use the popular GxEPD2 and Adafruit GFX libraries in the Arduino IDE to drive these displays.
* Pin Wiring: Connect VCC to 3.3V, GND to ground, and map the SPI pins (MOSI, SCK, CS, DC, RST, and BUSY) to your ESP32 or Arduino board.
* Refreshing: Full refreshes cause screen flickering to clear ghosting, while partial updates can refresh specific areas without flashing (best supported on black-and-white panels).
* For a closer look at setup details and layout specs, read the Simple E Ink Display MH-ET LIVE 1.54" Guide.


## Care to Avoid Physical & Electrical Damage

To keep your MH-ET Live e-paper display working smoothly and prevent permanent physical damage, you need to manage how you refresh the screen and handle the hardware. E-paper operates through physical microparticles moving inside microcapsules, making it highly susceptible to wear and tear from incorrect software driving or environmental factors.

### Refresh Considerations

* Avoid continuous full refreshes: Full refreshes cause the characteristic "flashing" behavior (inverting white to black and back) to clear out "ghost images." Doing this too frequently (e.g., every few seconds) will drastically shorten the screen's lifespan. Limit full refreshes to once every few minutes, or only when data changes.
* Use partial updates cautiously: Partial refreshing updates only changed pixels without flashing the whole screen. However, this causes charge buildup and ghosting over time. Always trigger a full refresh after 5 to 10 consecutive partial updates to reset the crystals and clear the background.
* Never use partial updates on tri-color/multi-color screens: Red and yellow pigments move much slower than black and white. Attempting partial updates on a three-color or four-color MH-ET Live display can permanently stain or ghost the panel. Multi-color screens must always use a full refresh.
* Power down after refreshing: Leaving a continuous electrical voltage on the display cell can permanently burn in an image or damage the internal film. Always put the display driver to sleep using software commands (display.powerOff() or display.hibernate()) immediately after completing a screen update.

### Care to Avoid Physical & Electrical Damage

* Keep away from direct sunlight and UV rays: Long-term exposure to sunlight or ultraviolet light degrades the organic fluids inside the screen. This causes colors to fade, turns white areas yellow, and eventually makes the display completely unresponsive.
* Ensure strict 3.3V logic compliance: MH-ET Live modules typically require 3.3V power and logic levels. Connecting it directly to a 5V microcontroller (like an Arduino Uno) without a logic level shifter will quickly burn out the onboard display controller.
* Protect the flexible glass panel: The underlying matrix layer of the display is made of incredibly thin, fragile glass. Do not apply pressure, bend, or drop the display, as even minor flexing can crack the internal glass layer while leaving the outer plastic layer looking completely intact.
* Store inside operating temperature ranges: E-paper fluids change viscosity depending on temperature. Do not operate or store the module outside its rated limits (typically 0°C to 50°C for black-and-white, and 0°C to 40°C for tri-color panels). Operating in freezing conditions can cause the microcapsules to rupture.





---

## References

[How to Use WeAct 1.54 E-paper module: Examples, Pinouts, and Specs](https://docs.cirkitdesigner.com/component/175fa3e3-2b65-451c-aa10-715821aab4fa)

[GxEPD2 - Display Library for SPI E-Paper Displays](https://github.com/ZinggJM/GxEPD2)
