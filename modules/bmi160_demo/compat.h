#pragma once

// The STMicroelectronics STM32 core implements ltoa/itoa (see
// cores/arduino/itoa.c) but only declares them for its own String.cpp,
// not globally via Arduino.h. TFT_eSPI expects them to be available the
// way avr-libc exposes them on AVR boards, so redeclare them here.

#ifdef __cplusplus
extern "C" {
#endif

extern char *itoa(int value, char *string, int radix);
extern char *ltoa(long value, char *string, int radix);

#ifdef __cplusplus
}
#endif
