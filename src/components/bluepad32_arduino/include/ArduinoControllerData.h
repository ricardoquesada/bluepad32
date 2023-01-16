// Copyright 2021 - 2023, Ricardo Quesada, http://retro.moe
// SPDX-License-Identifier: Apache-2.0 or LGPL-2.1-or-later

#ifndef BP32_ARDUINO_CONTROLLER_DATA_H
#define BP32_ARDUINO_CONTROLLER_DATA_H

#include "sdkconfig.h"
#ifndef CONFIG_BLUEPAD32_PLATFORM_ARDUINO
#error "Must only be compiled when using Bluepad32 Arduino platform"
#endif  // !CONFIG_BLUEPAD32_PLATFORM_ARDUINO

#include <uni_platform_arduino.h>

using ControllerData = arduino_controller_data_t;

#endif  // BP32_ARDUINO_CONTROLLER_DATA_H
