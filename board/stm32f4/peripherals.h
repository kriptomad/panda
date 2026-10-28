#pragma once

#ifdef BOOTSTUB
void gpio_usb_init(void) {
#else
static void gpio_usb_init(void) {
#endif
  // A11,A12: USB
  set_gpio_alternate(GPIOA, 11, GPIO_AF10_OTG_FS);
  set_gpio_alternate(GPIOA, 12, GPIO_AF10_OTG_FS);
  GPIOA->OSPEEDR = GPIO_OSPEEDER_OSPEEDR11 | GPIO_OSPEEDER_OSPEEDR12;
}

// No SPI slave interface on the STM32F4 boards
void gpio_spi_init(void) {
}

// Common GPIO initialization
void common_init_gpio(void) {
  // enable OTG out tied to ground
  GPIOA->ODR = 0;
  GPIOB->ODR = 0;
  GPIOA->PUPDR = 0;
  GPIOB->AFR[0] = 0;
  GPIOB->AFR[1] = 0;

  // C2: Voltage sense line
  set_gpio_mode(GPIOC, 2, MODE_ANALOG);

  gpio_usb_init();

  // B8,B9: CAN 1
  set_gpio_alternate(GPIOB, 8, GPIO_AF8_CAN1);
  set_gpio_alternate(GPIOB, 9, GPIO_AF8_CAN1);
}

#ifdef BOOTSTUB
void flasher_peripherals_init(void) {
  RCC->AHB2ENR |= RCC_AHB2ENR_OTGFSEN;
}
#endif

// FFI_SM_0, FFI_SM_1: hold everything without a clock enable in reset, RST and EN bits share positions
// all RST bits per stm32f413xx.h
static void hold_unused_in_reset(void) {
  static const struct { volatile uint32_t *enr; volatile uint32_t *rstr; uint32_t rst_bits; } regs[] = {
    {&(RCC->AHB1ENR), &(RCC->AHB1RSTR), 0x006010FFU},
    {&(RCC->AHB2ENR), &(RCC->AHB2RSTR), 0x000000C0U},
    {&(RCC->AHB3ENR), &(RCC->AHB3RSTR), 0x00000003U},
    {&(RCC->APB1ENR), &(RCC->APB1RSTR), 0xFFFECBFFU},
    {&(RCC->APB2ENR), &(RCC->APB2RSTR), 0x035779F3U},
  };
  for (uint8_t i = 0U; i < (sizeof(regs) / sizeof(regs[0])); i++) {
    uint32_t en = *regs[i].enr;
    register_set(regs[i].rstr, regs[i].rst_bits & ~en, 0xFFFFFFFFU);
    register_set(regs[i].enr, en, 0xFFFFFFFFU);
  }
}

// Peripheral initialization
void peripherals_init(void) {
  // enable GPIO(A,B,C,D)
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIODEN;

  // Supplemental
  RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN;  // self test DMA
  RCC->APB1ENR |= RCC_APB1ENR_PWREN;   // for RTC config
  RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;

  // Connectivity
  RCC->AHB2ENR |= RCC_AHB2ENR_OTGFSEN;
  RCC->APB1ENR |= RCC_APB1ENR_CAN1EN;
  RCC->APB1ENR |= RCC_APB1ENR_CAN2EN;
  RCC->APB1ENR |= RCC_APB1ENR_CAN3EN;

  // Analog
  RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;

  // Timers
  RCC->APB2ENR |= RCC_APB2ENR_TIM1EN;  // clock source timer
  RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;  // main counter
  RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;  // fan PWM
  RCC->APB1ENR |= RCC_APB1ENR_TIM5EN;  // clock check
  RCC->APB1ENR |= RCC_APB1ENR_TIM6EN;  // interrupt timer
  RCC->APB2ENR |= RCC_APB2ENR_TIM8EN;  // clock source slave timer
  RCC->APB2ENR |= RCC_APB2ENR_TIM9EN;  // slow loop

  hold_unused_in_reset();
}

void enable_interrupt_timer(void) {
  register_set_bits(&(RCC->APB1ENR), RCC_APB1ENR_TIM6EN);  // Enable interrupt timer peripheral
}

// FLASH_SM_3: nothing unlocks the option bytes. The F4 has no BOOT_ADD option bytes, the boot
// address is fixed to 0x08000000 when BOOT0 is low.
void check_option_bytes(void) {
  bool locked = (FLASH->OPTCR & FLASH_OPTCR_OPTLOCK) != 0U;
  if (!locked) {
    fault_occurred(FAULT_REGISTER_DIVERGENT);
  }
}