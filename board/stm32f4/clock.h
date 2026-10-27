#pragma once

void clock_init(void) {
  // enable external oscillator
  register_set_bits(&(RCC->CR), RCC_CR_HSEON);
  while ((RCC->CR & RCC_CR_HSERDY) == 0U);

  // divide things
  // AHB = 96MHz
  // APB1 = 48MHz
  // APB2 = 48MHz
  register_set(&(RCC->CFGR), RCC_CFGR_HPRE_DIV1 | RCC_CFGR_PPRE2_DIV2 | RCC_CFGR_PPRE1_DIV2, 0xFF7FFCF3U);

  // 16MHz crystal
  // PLLM: 8
  // PLLN: 96
  // PLLP: 2
  // PLLQ: 4
  // P output: 96MHz
  // Q output: 48MHz
  register_set(&(RCC->PLLCFGR), RCC_PLLCFGR_PLLQ_2 | RCC_PLLCFGR_PLLM_3 | RCC_PLLCFGR_PLLN_6 | RCC_PLLCFGR_PLLN_5 | RCC_PLLCFGR_PLLSRC_HSE, 0x7F437FFFU);

  // start PLL
  register_set_bits(&(RCC->CR), RCC_CR_PLLON);
  while ((RCC->CR & RCC_CR_PLLRDY) == 0U);

  // Configure Flash prefetch, Instruction cache, Data cache and wait state
  // *** without this, it breaks ***
  register_set(&(FLASH->ACR), FLASH_ACR_ICEN | FLASH_ACR_DCEN | FLASH_ACR_LATENCY_5WS, 0x1F0FU);

  // switch to PLL
  register_set_bits(&(RCC->CFGR), RCC_CFGR_SW_PLL);
  while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL);

  // *** running on PLL ***
}

// CLK_SM_3: TIM5 channel 4 captures every 8th edge of the 32kHz LSI on the HSE derived timer clock
// (TIM5_OR TI4_RMP = 01 routes the LSI to TI4, RM0430)
#define LSI_COUNTS_NOMINAL (APB1_TIMER_FREQ * 250U)

void clock_check_init(void) {
  register_set_bits(&(RCC->CSR), RCC_CSR_LSION);
  register_set(&(TIM5->OR), TIM_OR_TI4_RMP_0, TIM_OR_TI4_RMP);
  register_set(&(TIM5->CCMR2), TIM_CCMR2_CC4S_0 | TIM_CCMR2_IC4PSC_1 | TIM_CCMR2_IC4PSC_0, 0xFF00U);
  register_set(&(TIM5->CCER), TIM_CCER_CC4E, 0xFFFFU);
  register_set(&(TIM5->PSC), 0U, 0xFFFFU);
  register_set(&(TIM5->ARR), 0xFFFFFFFFU, 0xFFFFFFFFU);
  register_set(&(TIM5->CR1), TIM_CR1_CEN, 0x3FFU);
}

// waits up to 1ms for the next capture, an LSI/8 period is 250us
static bool lsi_capture(uint32_t *count) {
  uint32_t ts = microsecond_timer_get();
  bool captured = false;
  while (!captured && (get_ts_elapsed(microsecond_timer_get(), ts) < 1000U)) {
    captured = (TIM5->SR & TIM_SR_CC4IF) != 0U;
  }
  *count = TIM5->CCR4;
  return captured;
}

// called at 1Hz, LSI isn't accurate so the window only catches gross errors
bool clock_check(void) {
  // reading CCR4 clears CC4IF
  uint32_t start = TIM5->CCR4;
  uint32_t end = 0U;
  bool captured = lsi_capture(&end);
  uint32_t counts = end - start;
  return captured && (counts > ((LSI_COUNTS_NOMINAL * 3U) / 4U)) && (counts < ((LSI_COUNTS_NOMINAL * 5U) / 4U));
}