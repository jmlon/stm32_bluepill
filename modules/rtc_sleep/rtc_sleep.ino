/*
  SLEEP and wake up from an RTC alarm every 5 seconds, toggling the LED

*/

// Required library:
// arduino-cli lib install "STM32duino RTC"

#include <Arduino.h>
#include <STM32RTC.h>

/* Get the rtc object */
STM32RTC& rtc = STM32RTC::getInstance();

volatile bool woke = false;

void alarmMatch(void *data);

// the setup function runs once when you press reset or power the board
void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);  // LED off (active-low)

  // HSE/128 (8 MHz crystal -> 62.5 kHz): crystal-accurate, but the HSE keeps
  // running only in Sleep mode, not STOP. Must be set before begin().
  rtc.setClockSource(STM32RTC::HSE_CLOCK);
  rtc.begin();
  rtc.setTime(0, 0, 0);
  rtc.setDate(1, 1, 1, 26);
  rtc.attachInterrupt(alarmMatch);
}

// the loop function runs over and over again forever
void loop() {
  // Arm an alarm 5 s ahead, then STOP until it fires.
  rtc.setAlarmEpoch(rtc.getEpoch() + 5);

  // Sleep keeps all clocks running; only the CPU halts. Stop SysTick so its
  // 1 ms tick doesn't wake the core early.
  HAL_SuspendTick();
  HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);
  HAL_ResumeTick();

  if (woke) {
    woke = false;
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
  }
}

void alarmMatch(void *data)
{
  UNUSED(data);
  woke = true;
}
