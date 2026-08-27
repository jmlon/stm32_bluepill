#pragma once

// DFRobot_BMI160.h does `#include<arduino.h>` (lowercase), which only
// resolves on case-insensitive filesystems (Windows/macOS). This shim
// makes it resolve on Linux too, without editing the library itself --
// see build-flags.txt, which adds this directory's -I to the include path.
#include "Arduino.h"
