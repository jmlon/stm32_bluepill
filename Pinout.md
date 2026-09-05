# Bluepill (STM32F103C8T6) pinout & internal devices

To master the STM32 Blue Pill (STM32F103C8T6) development board, you need to understand how to handle its 3.3V architecture, identify restricted pins, and safely utilize its GPIO structure. [1] 

## 1. How to Use the I/O Pins

Every general-purpose I/O (GPIO) pin on the Blue Pill can be configured via firmware (using tools like the Arduino IDE or [STM32CubeIDE](https://microcontrollerslab.com/push-button-stm32-blue-pill-stm32cube-ide-tutorial/)) into several modes: [2] 

* Digital Input: Can be configured as Floating (high impedance), Internal Pull-Up, or Internal Pull-Down.
* Digital Output: Can be set to Push-Pull (standard high/low driving) or Open-Drain (requires an external pull-up resistor to pull high).
* Alternate Function (AF): Configures the pin to connect directly to internal hardware peripherals like UART, SPI, I2C, or PWM timers.
* Analog Input: Disables digital circuitry so the pin can route external signals to the Analog-to-Digital Converter (ADC). [3, 4, 5] 

## 2. Reserved & Special-Function Pins

Certain pins on the Blue Pill have hardware restrictions or default configurations that you must plan around: [6] 

| Pins | Default / Reserved Function | Usage Notes |
|---|---|---|
| PA11 & PA12 | USB D- & D+ | Hardwired to the onboard micro-USB port. Avoid using them as general I/O if your project uses USB. |
| PA13 & PA14 | SWDIO & SWCLK | Used for debugging and programming via the ST-Link. Avoid using these or you will lock yourself out of active debugging. |
| PA15, PB3, PB4 | JTAG Pins | Default to JTAG mode upon reset. To use them as general GPIO, you must explicitly disable JTAG in software and enable SWD-only mode. |
| PC13 | Onboard LED | Hardwired to the green user LED. Note that it is active-LOW (writing LOW turns it ON). |
| PC14 & PC15 | Low-Speed Crystal (32.768 kHz) | Connected to the external RTC crystal. They have low current-drive capability and should avoid heavy electrical loads. |
| PD0 & PD1 | High-Speed Crystal (8 MHz) | Tied to the main oscillator crystal providing the MCU clock. Do not use as GPIO. |

**JTAG** (Joint Test Action Group) is the legacy, industry-standard interface used for testing and debugging integrated circuits. It was originally designed for verifying printed circuit boards after manufacture (boundary scan testing) but evolved into a debugging tool.

**SWD** (Serial Wire Debug) is a modern, 2-pin alternative to JTAG designed specifically by ARM for their Cortex-M processors (which include the STM32). Uses two pins: SWDIO (Serial Wire Data Input/Output) – Handles bi-directional data, SWCLK (Serial Wire Clock) – Controls clock synchronization. These are the pins used by the **ST-Link programmer**.

## 3. Things to Be Careful About (Gotchas)

### ⚡ 5V Tolerance Danger

The STM32F103 chip is a 3.3V device. Operating an I/O pin outside its electrical limits will permanently destroy the microcontroller: [1] 

* Not all pins are 5V tolerant. Always check the STM32F103C8T6 datasheet pinout diagrams. Pins marked FT (Five-Volt Tolerant) can safely accept a 5V logic input.
* Analog pins (PA0–PA7) are NOT 5V tolerant. Connecting 5V to an ADC-capable pin will ruin it. They must stay under 3.3V.
* Outputs cannot source 5V. Even on 5V-tolerant pins, a high digital output will only output 3.3V. [1, 5] 

### 🔌 Current Limitations

* Per-pin maximum: Individual pins can safely source or sink up to 25 mA.
* Total chip maximum: The aggregate current driven across all GPIO pins concurrently must never exceed 150 mA. Always use transistors, MOSFETs, or optocouplers when driving high-load devices like motors, relays, or long LED strips. [7] 

### 🪱 The Infamous "PA12 USB Resistor" Blue Pill Hardware Flaw

Many third-party Blue Pill clones ship with a 10kΩ pull-up resistor on the PA12 (USB D+) line instead of the USB-standard 1.5kΩ resistor.

* The Symptom: Your computer throws an "Unknown USB Device" error when you plug the board in.
* The Fix: If you intend to use native USB functionality, you may need to manually desolder the "R10" resistor on the board and replace it with a 1.5kΩ resistor.

### 📍 The "C13 / C14" Bootleg Chip Warning

Many modern Blue Pills use counterfeit or rebranded microcontrollers (such as CH32F103 or CKS32F103) stamped with fake STMicroelectronics markings. While they generally work, they can have slight register deviations, timing issues, or quirks when attempting to flash them using authentic ST-Link programmers. [8, 9] 
Would you like assistance in:

* Setting up a basic blink or button configuration code in Arduino IDE or STM32CubeIDE?
* Verifying if a specific sensor/peripheral you intend to connect requires a 5V-tolerant pin?
* Learning how to remap standard functions (like shifting a UART to different pins)?


---

## Peripherals

### 1. Analog-to-Digital Converters (ADC)

The Blue Pill features two independent 12-bit ADC peripherals (ADC1 and ADC2).

* Resolution: 12-bit resolution means it divides a 0 to 3.3V analog signal into 4,096 discrete steps (providing vastly superior precision compared to an Arduino Uno's 10-bit/1024-step ADC).
* Channels: Up to 10 external channels broken out to pins (PA0–PA7 and PB0–PB1).
* Special Modes: The ADCs can operate in "Dual Mode" (interleaved sampling for higher speed) and feature an internal temperature sensor and internal reference voltage channel.
* ⚠️ Reminder: None of the ADC pins are 5V tolerant.

### 2. Pulse-Width Modulation (PWM) & Timers

The chip has 4 powerful 16-bit hardware timers (TIM1, TIM2, TIM3, and TIM4).

* PWM Capability: Across these timers, the Blue Pill can generate up to 15 independent PWM outputs.
* Advanced Timer (TIM1): This is a specialized motor-control timer capable of generating complementary PWM outputs with programmable "dead-time" insertions (essential for safely driving H-bridges and brushless DC motors).
* Input Capture & Encoder Mode: Timers can also be configured to measure external pulse widths, frequencies, or directly decode quadrature rotary encoders without burning CPU cycles.

### 3. Communication Interfaces

The Blue Pill has excellent hardware connectivity options, allowing it to talk to multiple sensors, displays, and computers simultaneously:

* USART (Serial) x 3: Three independent hardware serial ports. USART1 is capable of high-speed communication up to 4.5 Mbit/s.
* SPI (Serial Peripheral Interface) x 2: Two high-speed SPI ports (up to 18 Mbit/s) for fast displays (like TFT screens) and SD card modules.
* I2C (Inter-Integrated Circuit) x 2: Two I2C interfaces supporting standard (100 kHz) and fast (400 kHz) modes for sensors, RTCs, and small OLEDs.
* USB 2.0 Full Speed x 1: A dedicated USB peripheral (12 Mbit/s). When properly configured, the Blue Pill can act directly as a native USB device (Keyboard, Mouse, MIDI controller, or Virtual COM Port).
* CAN Bus 2.0B x 1: An automotive-grade Controller Area Network interface. It requires an external transceiver chip (like the MCP2551) to connect to a physical CAN network but handles all protocol framing internally.

### 4. Direct Memory Access (DMA)

The 7-channel DMA controller is one of the Blue Pill's greatest performance advantages. It allows peripherals (like the ADC, SPI, or UART) to send and receive data directly to and from the chip's internal RAM without involving the CPU. For example, the ADC can stream thousands of sensor readings into a data array in the background while the CPU performs heavy math calculations.

### 5. Other Notable Internal Functions

* RTC (Real-Time Clock): An internal clock module with an independent BCD timer/counter. If you connect a small 3V coin-cell battery to the VBAT pin, the RTC will keep accurate time even when the main Blue Pill power is disconnected.
* CRC Calculation Unit: A hardware block that calculates Cyclic Redundancy Checks to quickly verify data integrity for communication packets.
* Watchdog Timers x 2: An Independent Watchdog (IWDG) and a Window Watchdog (WWDG). These act as safety nets; if your code freezes or enters an infinite loop, the watchdog will automatically reset the microcontroller.
* Nested Vectored Interrupt Controller (NVIC): Manages low-latency real-time interrupts. Every single GPIO pin on the Blue Pill can be configured as an external interrupt source.


---

## The 3 Low-Power Modes

By default, running at full speed (72 MHz), the Blue Pill draws around 30–50 mA. Using these sleep modes can drop that consumption significantly.

The table below breaks down the modes from the highest power consumption (easiest to wake up) to the lowest power consumption (deepest sleep).

| Low-Power Mode | What is Shut Down? | What Stays Active? | Typical MCU Current | How to Wake It Up | Effect on Wake Up |
|---|---|---|---|---|---|
| 1. Sleep Mode | CPU core clock stops. | All peripherals (ADC, Timers, UART, etc.) and the main system clock. | ~5.5 mA to 15 mA (Depends on active peripherals) | Any interrupt or event (e.g., button press, timer tick, UART data received). | Resumes code instantly right after the sleep instruction. |
| 2. Stop Mode | CPU clock, all peripheral clocks, and High-Speed Oscillators (HSE/HSI). | Internal SRAM memory content and registers are preserved. | ~15 µA to 25 µA | External Interrupts (EXTI line on any GPIO), RTC Alarm, or USB wakeup. | Resumes code right after the sleep instruction, but you must manually re-initialize the system clock (it wakes up on the slow internal HSI clock). |
| 3. Standby Mode | The entire digital core. Internal voltage regulator is turned off. | Only the RTC (Real-Time Clock), Backup Registers, and Standby circuitry. | ~2 µA to 3.4 µA | WKUP pin (PA0) rising edge, RTC Alarm, or a Hardware Reset. | The chip resets completely. Code starts executing from the very beginning (setup() or main()). |

### ⚠️ Critical "Gotchas" for Blue Pill Power Saving

If you put the Blue Pill into Stop or Standby mode, you might find that it still draws several milliamperes instead of microamps. This is caused by the hardware design of the Blue Pill board itself, not the chip:

   1. The Onboard Power LED: The board has a red power LED hardwired directly to the 3.3V line. This single LED continuously draws about 3 to 5 mA. If you want true microamp consumption, you must desolder or smash the power LED (usually labeled PWR).
   2. The Voltage Regulator: The onboard 3.3V regulator (usually an AMS1117 or similar clone) has a high "quiescent current." Even if the STM32 chip draws nothing, the regulator itself wastes around 5 mA just staying on. For ultra-low power projects, you should bypass this regulator and feed clean 3.3V directly into the 3.3V pin from a highly efficient regulator.
   3. Floating Pins: Any unused GPIO pins left "floating" (not connected to anything and not configured in software) will bounce around due to electrical noise, consuming power. Always configure unused pins as Analog Input or turn on their internal pull-up/pull-down resistors before entering sleep.


---

## References

[1] [https://sourceforge.net](https://sourceforge.net/p/mecrisp/discussion/general/thread/47921c0c90/)
[2] [https://microcontrollerslab.com](https://microcontrollerslab.com/push-button-stm32-blue-pill-stm32cube-ide-tutorial/)
[3] [https://community.st.com](https://community.st.com/stm32-mcus-products-25/basic-questions-about-alternate-functions-100663)
[4] [https://community.st.com](https://community.st.com/stm32-mcus-products-25/recommended-course-of-action-for-unused-pin-of-stm32-mcu-119796)
[5] [https://robocraze.com](https://robocraze.com/blogs/post/introduction-to-the-stm32-bluepill)
[6] [https://community.st.com](https://community.st.com/stm32-mcus-products-25/stm32446re-is-there-a-simple-chart-that-states-which-pins-are-usable-for-just-i-o-120611)
[7] [https://www.makeriot2020.com](https://www.makeriot2020.com/index.php/category/stm32f103c8t6-blue-pill/)
[8] [https://visualgdb.com](https://visualgdb.com/tutorials/arduino/stm32/bluepill/)
[9] [https://www.youtube.com](https://www.youtube.com/watch?v=L670v-Oghs4)
[STM32 vs ESP32 for Production IoT Projects](https://www.hackster.io/sudoyasir/stm32-vs-esp32-for-production-iot-projects-917a63)
