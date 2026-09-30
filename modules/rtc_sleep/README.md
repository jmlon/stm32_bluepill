# Low power modes

| Mode | Power Savings | RAM Retained? | Wakeup Time | HAL Command |
|---|---|---|---|---|
| Sleep | Low | Yes | Fast | HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI); |
| Stop | High | Yes | Medium | HAL_PWR_EnterSTOPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI); |
| Standby | Maximum | No (Resets) | Slow | HAL_PWR_EnterSTANDBYMode(); |
