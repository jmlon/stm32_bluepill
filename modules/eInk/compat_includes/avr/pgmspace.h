#pragma once

// GxEPD2 includes <avr/pgmspace.h> unconditionally on every target that is
// not an ESP8266/ESP32 (see GxEPD2_EPD.cpp and src/bitmaps/*.h). The
// STMicroelectronics STM32 core does ship an AVR compatibility shim, but at
// cores/arduino/api/deprecated-avr-comp/avr/pgmspace.h, which is not on the
// include search path -- so the bare <avr/pgmspace.h> fails to resolve.
//
// Forward to the core's copy rather than patching the library, so GxEPD2 can
// be updated from the library manager without losing the fix. This directory
// is added to the include path by the Makefile (see compat_includes/).

#include <api/deprecated-avr-comp/avr/pgmspace.h>
