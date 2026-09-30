# RTC and low power modes (STM32F103C8 Blue Pill)

The `rtc_sleep.ino` sketch arms an RTC alarm 5 s ahead, puts the CPU to sleep,
and toggles the LED each time the alarm wakes it. Uses the
[STM32duino RTC](https://github.com/stm32duino/STM32RTC) library
(`arduino-cli lib install "STM32duino RTC"`).

## Low power modes

| Mode | Power Savings | RAM Retained? | Wakeup Time | HAL Command |
|---|---|---|---|---|
| Sleep | Low | Yes | Fast | `HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);` |
| Stop | High | Yes | Medium | `HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);` |
| Standby | Maximum | No (Resets) | Slow | `HAL_PWR_EnterSTANDBYMode();` |

Approximate figures from the F103 datasheet (3.3 V, 25 °C, typical; check the
datasheet for exact conditions):

| Mode | Current | What stops | Wakes on | State after wakeup |
|---|---|---|---|---|
| Run (72 MHz) | ~20-50 mA | nothing | - | - |
| Sleep | ~7-30 mA | CPU clock only; peripherals and all clocks keep running | any enabled interrupt (incl. SysTick, RTC alarm) | resumes after the `WFI` |
| Stop | ~15-25 µA | all 1.2 V-domain clocks: HSE, HSI and PLL are off | EXTI line (any GPIO pin, RTC alarm, PVD, USB wakeup) | resumes after the `WFI`, running on the **HSI** |
| Standby | ~2-4 µA | the 1.2 V domain is powered off | `WKUP` pin (PA0) rising edge, RTC alarm, `NRST`, IWDG reset | full reset; SRAM lost. Only the backup registers and RTC survive (if VBAT/LSE) |

Sleep current depends heavily on which peripherals are clocked, so the range is
wide. The Blue Pill's power LED and 3.3 V regulator draw several mA on their
own, so the board as a whole will never measure in the µA range.

### Notes on each mode

- **Sleep** is the easy one: nothing needs restoring after wakeup. Any
  interrupt wakes it, including SysTick's 1 ms tick, so call
  `HAL_SuspendTick()` before sleeping and `HAL_ResumeTick()` after, or the core
  wakes every millisecond. This also means `delay()`/`millis()` are not
  advancing while the tick is suspended.
- **Stop** wakes on the HSI, so the system clock must be reconfigured
  (`SystemClock_Config()`) to get HSE + PLL back, otherwise the MCU runs at
  8 MHz. The wakeup source must be an EXTI line: the RTC alarm is line 17. Clear
  `PWR_FLAG_WU` before entering.
- **Standby** is effectively a power-off with a timer: execution restarts from
  `setup()`. To tell a wakeup from a cold boot, check `PWR_FLAG_SB` (clear it
  with `PWR_FLAG_SB`/`PWR_FLAG_WU` after reading) and keep any state you need in
  the backup registers (`RTC->BKP` / `BKP_DRx`).

## RTC clock source

The F103 RTC can be clocked from three sources, chosen before `rtc.begin()`
with `rtc.setClockSource(...)`:

| Source | Frequency | Accuracy | Works in Sleep | Works in Stop | Works in Standby | Survives power loss (VBAT) |
|---|---|---|---|---|---|---|
| `LSI_CLOCK` (default) | ~40 kHz (30-60) | poor: ±25-40 %, drifts with temperature/voltage | yes | yes | yes | no |
| `LSE_CLOCK` | 32.768 kHz crystal | ~20 ppm (< 2 s/day) | yes | yes | yes | yes |
| `HSE_CLOCK` | HSE / 128 = 62.5 kHz | crystal accurate | yes | **no** (HSE off) | **no** | no |

The HSI cannot clock the F103 RTC.

This sketch uses `HSE_CLOCK` because the Blue Pill has an 8 MHz crystal but no
32.768 kHz one fitted, and that is why it uses Sleep and not Stop: the HSE
stops in Stop mode, so the RTC would stop counting and never wake the MCU.

To get accurate timekeeping *and* deep sleep, solder a 32.768 kHz crystal to
the unused Y2 pads (PC14/PC15) and switch to `LSE_CLOCK`; then Stop and Standby
work with the RTC running.

## RTC alarm

```cpp
STM32RTC& rtc = STM32RTC::getInstance();

rtc.setClockSource(STM32RTC::HSE_CLOCK);
rtc.begin();
rtc.setTime(0, 0, 0);
rtc.setDate(1, 1, 1, 26);          // weekday, day, month, year (2-digit)
rtc.attachInterrupt(alarmMatch);   // void alarmMatch(void *data)

rtc.setAlarmEpoch(rtc.getEpoch() + 5);   // 5 s from now
```

- Prefer `setAlarmEpoch(getEpoch() + n)` for relative alarms.
  `setAlarmTime(h, m, s + n, 0)` breaks when `s + n` passes 59.
- The callback runs in interrupt context: set a flag and do the work in
  `loop()`. Don't call `Serial` or `delay()` there.
- The alarm is one-shot with `setAlarmEpoch`; re-arm it each cycle.
- `setAlarmTime` + `enableAlarm(MATCH_DHHMMSS)` also works for absolute
  times (see `../rtc_alarm`).
- The F103 RTC is a 32-bit seconds counter, so alarm resolution is 1 s.

## Gotchas

- Without a battery on VBAT, RTC time is lost on power-off and the sketch
  resets it at boot. Backup registers and the RTC keep their values across
  Standby only while VDD or VBAT stays up.
- The onboard LED is active-low: `HIGH` = off, `LOW` = on.
- Flashing/debugging over SWD can be unreliable while the MCU sleeps in Stop
  or Standby, since the debug clocks are gated. If the ST-Link cannot connect,
  hold `NRST` while starting the flash (connect-under-reset).
- Related notes: Stop/Standby demos are not included here; only Sleep is
  tested on hardware.

## References

- RM0008 (Reference manual), chapters *Power control (PWR)* and *Real-time clock (RTC)*
- STM32F103x8/xB datasheet, *Current consumption* tables
- [STM32duino RTC library](https://github.com/stm32duino/STM32RTC)
- [Mischianti: STM32 internal RTC and VBAT backup](https://mischianti.org/stm32-internal-rtc-clock-and-battery-backup-vbat/)
