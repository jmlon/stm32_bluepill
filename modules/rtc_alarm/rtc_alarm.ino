/*
  Blink evey second.

  Turns an LED on for one second, then off for one second, repeatedly.

  RTC Guide:
  https://mischianti.org/stm32-internal-rtc-clock-and-battery-backup-vbat/

*/

// Required library:
// arduino-cli lib install "STM32duino RTC"

#include <Arduino.h>
#include <STM32RTC.h>

/* Get the rtc object */
STM32RTC& rtc = STM32RTC::getInstance();

/* Change these values to set the current initial time */
const byte seconds = 0;
const byte minutes = 15;
const byte hours = 21;

/* Change these values to set the current initial date */
/* Monday=1 */
const byte weekDay = 2;
const byte day = 29;
const byte month = 9;
const byte year = 26;

void alarmMatch(void *data);

// the setup function runs once when you press reset or power the board
void setup() {
  pinMode(LED_BUILTIN, OUTPUT);

  rtc.begin(); // By default the LSI (low speed internal 40Khz)
  rtc.setTime(hours, minutes, seconds);
  rtc.setDate(weekDay, day, month, year);

  rtc.attachInterrupt(alarmMatch);
  rtc.setAlarmDay(day);

  rtc.setAlarmTime(hours, minutes, seconds+5, 0);
  rtc.enableAlarm(rtc.MATCH_DHHMMSS);

  // The Blue Pill's onboard LED is active-low: HIGH is off, LOW is on.
  digitalWrite(LED_BUILTIN, HIGH);

}



// the loop function runs over and over again forever
void loop() {
  delay(10000);                      // wait for a 10 seconds
}

void alarmMatch(void *data)
{
  UNUSED(data);
  digitalWrite(LED_BUILTIN, LOW);   // change state of the LED by setting the pin to the LOW voltage level
}
