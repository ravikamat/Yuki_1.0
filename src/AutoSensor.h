// AutoSensor.h — Yuki_1.0
// Centralised auto-start for every hardware sensor.
// Called ONCE at the end of BabyMode's constructor.
// Retries failed sensors up to 3 times with 200ms gap.
#pragma once
#include <string>

class BabyMode;   // forward

void autoStartAllSensors(BabyMode& baby);
