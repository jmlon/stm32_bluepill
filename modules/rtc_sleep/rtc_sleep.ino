/*
  STOP and wake up from an RTC alarm every 5 seconds, toggling the LED

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

  rtc.begin();  // LSI clock by default
  rtc.setTime(0, 0, 0);
  rtc.setDate(1, 1, 1, 26);
  rtc.attachInterrupt(alarmMatch);
}

// the loop function runs over and over again forever
void loop() {
  // Arm an alarm 5 s ahead, then STOP until it fires.
  rtc.setAlarmEpoch(rtc.getEpoch() + 5);

  __HAL_PWR_CLEAR_FLAG(PWR_FLAG_WU);
  HAL_SuspendTick();
  HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);

  // Woken by the RTC alarm. STOP switches the system clock to HSI;
  // restore HSE + PLL.
  SystemClock_Config();
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
