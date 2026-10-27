#pragma once

// ///////////////////////////////////////////////////////////// //
// Hardware abstraction layer for all different supported boards //
// ///////////////////////////////////////////////////////////// //
#include "board/boards/board_declarations.h"
#include "board/boards/unused_funcs.h"

// ///// Board definition and detection ///// //
#include "board/stm32f4/lladc.h"
#include "board/stm32f4/lldts.h"
#include "board/drivers/harness.h"
#include "board/drivers/fan.h"
#include "board/stm32f4/llfan.h"
#include "board/drivers/clock_source.h"
#include "board/boards/black.h"

// No sound hardware on the STM32F4 boards
#ifndef BOOTSTUB
uint16_t sound_output_level = 0U;
#endif
void sound_tick(void) {}

void detect_board_type(void) {
  // Of the STM32F4 pandas, only the black panda is still supported. The pins the deprecated
  // boards were detected on are checked so that they end up as HW_TYPE_UNKNOWN instead.
  set_gpio_output(GPIOC, 14, 1);
  set_gpio_output(GPIOC, 5, 1);
  if (!detect_with_pull(GPIOB, 1, PULL_UP) && !detect_with_pull(GPIOB, 7, PULL_UP)) {
    // dos is deprecated
    hw_type = HW_TYPE_UNKNOWN;
  } else if ((detect_with_pull(GPIOA, 4, PULL_DOWN)) || (detect_with_pull(GPIOA, 5, PULL_DOWN)) || (detect_with_pull(GPIOA, 6, PULL_DOWN)) || (detect_with_pull(GPIOA, 7, PULL_DOWN))) {
    // white is deprecated
    hw_type = HW_TYPE_UNKNOWN;
  } else if (detect_with_pull(GPIOA, 13, PULL_DOWN)) {
    // grey is deprecated. Rev AB deprecated, so no pullup means black. In REV C, A13 is pulled up to 5V with a 10K
    hw_type = HW_TYPE_UNKNOWN;
  } else if (!detect_with_pull(GPIOB, 15, PULL_UP)) {
    // uno is deprecated
    hw_type = HW_TYPE_UNKNOWN;
  } else {
    hw_type = HW_TYPE_BLACK_PANDA;
    current_board = &board_black;
  }

  // Return A13 to the alt mode to fix SWD
  set_gpio_alternate(GPIOA, 13, GPIO_AF0_SWJ);
}