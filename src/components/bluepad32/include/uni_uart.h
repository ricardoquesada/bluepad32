// SPDX-License-Identifier: Apache-2.0
// Copyright 2019-2023 Ricardo Quesada
// http://retro.moe/unijoysticle2

#ifndef UNI_UART_H
#define UNI_UART_H

#include <stdbool.h>

// Interface
// Each arch needs to implement these functions

void uni_uart_init(void);
void uni_uart_enable_output(bool enabled);

#endif  // UNI_UART_H
