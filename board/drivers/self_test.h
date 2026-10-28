#pragma once

#include "board/crc.h"

// UM2331 CPU_SM_0, RAM_SM_0/7/8, FLASH_SM_0/7/9
// at boot: all of RAM in board/<mcu>/self_test.s before the C runtime, then CPU, FPU and the whole flash image
// at 8Hz: CPU, FPU, one RAM block, an ECC scrub of the DMA RAMs and one flash chunk, ECC flags of every RAM in use
// UM2331 DMA_SM_3/4 at 8Hz: one DMA copy across the D1 to D2 interconnect, transfer errors of the sound DMAs, SPI's in llspi_queue
// STM32F4: the SRAM and flash have no ECC, so RAM_SM_7 and the ECC flag checks don't apply, the DMA copy stays within the single AHB matrix

// keep in sync with board/stm32h7/self_test.s and board/stm32f4/self_test.s
#define SELF_TEST_PASS 0x5AFEC0DEU

// all IRQs share one priority, so each tick slice stays short
#define RAM_TEST_BLOCK_WORDS 64U
// consecutive blocks overlap by two words, for coupling across a block boundary
#define RAM_TEST_STEP ((RAM_TEST_BLOCK_WORDS - 2U) * 4U)
// kept clear of the frames of the running test
#define RAM_TEST_STACK_WINDOW 512U
// one alias per address bit above the block, AXI SRAM (H7) and the 320K SRAM (F4) need the most, 0x100 to 0x40000
#define RAM_TEST_ALIASES 11U
// SRAM1/2 and SRAM4 words read per tick for ECC scrubbing
#define RAM_SCRUB_WORDS 64U
#define FLASH_CRC_CHUNK 0x400U

#ifdef STM32H7
extern uint32_t _dtcmram_start;
extern uint32_t _dtcmram_end;
extern uint32_t _axisram_start;
extern uint32_t _axisram_end;
extern uint32_t _itcmram_start;
extern uint32_t _itcmram_end;
extern uint32_t _sram12_start;
extern uint32_t _sram12_end;
extern uint32_t _sram4_start;
extern uint32_t _sram4_end;
#else
extern uint32_t _sram_start;
extern uint32_t _sram_end;
#endif
extern uint32_t _stack_guard_end;
extern uint32_t self_test_cookie;

typedef struct {
  uint32_t start;
  uint32_t end;
} ram_region;

#ifdef STM32H7
// RM0468 Table 11, every RAM in use, not the ETM and backup RAMs
static RAMECC_MonitorTypeDef *const ecc_monitors[] = {
  RAMECC1_Monitor1, RAMECC1_Monitor2, RAMECC1_Monitor3, RAMECC1_Monitor4, RAMECC1_Monitor6,
  RAMECC2_Monitor1, RAMECC2_Monitor2, RAMECC2_Monitor3, RAMECC3_Monitor1,
};
#define ECC_MONITOR_CNT (sizeof(ecc_monitors) / sizeof(ecc_monitors[0]))
#endif

#define DMA_TEST_WORDS 8U
#ifdef STM32H7
// outside the tested regions, copies are read back instead
__attribute__((section(".sram12"))) static uint32_t dma_test_buf[DMA_TEST_WORDS];
#else
// inside the only RAM, ram_test_skip() keeps the running test clear of both
static volatile uint32_t ram_test_backup[RAM_TEST_BLOCK_WORDS];
static uint32_t dma_test_buf[DMA_TEST_WORDS];
#endif

#ifdef ALLOW_DEBUG
#define SELF_TEST_CPU 0U
#define SELF_TEST_RAM 1U
#define SELF_TEST_FLASH 2U
#define SELF_TEST_RAM_ECC 3U
#define SELF_TEST_FLASH_ECC 4U
#define SELF_TEST_DMA 5U
static uint8_t self_test_fail = 0U;
static uint32_t flash_crc_flip = 0U;

// the flash CRC injection goes through the CRC compare
void self_test_inject(uint8_t test) {
  if (test == SELF_TEST_FLASH) {
    flash_crc_flip = 1U;
  } else if (test <= SELF_TEST_DMA) {
    self_test_fail |= (uint8_t)(1U << test);
  } else {
  }
}

static bool self_test_injected(uint8_t test) {
  bool ret = (self_test_fail & (uint8_t)(1U << test)) != 0U;
  self_test_fail &= (uint8_t)~(1U << test);
  return ret;
}
#endif

// board/<mcu>/self_test.s
uint32_t cpu_test(void);
uint32_t fpu_test(void);

// march c-, word wide
static bool ram_test_march(volatile uint32_t *p, uint32_t n, uint32_t bg) {
  const uint32_t inv = ~bg;
  bool ok = true;
  for (uint32_t i = 0U; i < n; i++) {
    p[i] = bg;
  }
  for (uint32_t i = 0U; i < n; i++) {
    ok = ok && (p[i] == bg);
    p[i] = inv;
  }
  for (uint32_t i = 0U; i < n; i++) {
    ok = ok && (p[i] == inv);
    p[i] = bg;
  }
  for (uint32_t i = n; i > 0U; i--) {
    ok = ok && (p[i - 1U] == bg);
    p[i - 1U] = inv;
  }
  for (uint32_t i = n; i > 0U; i--) {
    ok = ok && (p[i - 1U] == inv);
    p[i - 1U] = bg;
  }
  for (uint32_t i = 0U; i < n; i++) {
    ok = ok && (p[i] == bg);
  }
  return ok;
}

// the stack guard faults on access and the running frames can't hold test patterns, both are covered at boot
static bool ram_test_skip(uint32_t start, uint32_t end) {
  uint32_t sp = __get_MSP();
  bool guard_hit = (start < (uint32_t)&_stack_guard_end) && (end > (uint32_t)&_stack_guard);
  bool stack_hit = (start < (sp + RAM_TEST_STACK_WINDOW)) && (end > (sp - RAM_TEST_STACK_WINDOW));
#ifdef STM32H7
  return guard_hit || stack_hit;
#else
  // the test's own backup and the DMA destination live in the tested RAM, both are covered at boot
  bool backup_hit = (start < ((uint32_t)&ram_test_backup[0] + sizeof(ram_test_backup))) && (end > (uint32_t)&ram_test_backup[0]);
  bool dma_hit = (start < ((uint32_t)&dma_test_buf[0] + sizeof(dma_test_buf))) && (end > (uint32_t)&dma_test_buf[0]);
  return guard_hit || stack_hit || backup_hit || dma_hit;
#endif
}

// a decoder fault on an address bit above the block makes the block share cells with an alias, which then reads the patterns
static bool ram_test_aliases_ok(volatile uint32_t *const *alias, const uint32_t *val, uint8_t n) {
  bool ok = true;
  for (uint8_t i = 0U; i < n; i++) {
    ok = ok && (*alias[i] == val[i]);
  }
  return ok;
}

// transparent, interrupts stay off while the block holds test patterns
// the march covers the address decoder within the block, the aliases the address bits above it (UM2331 RAM_SM_8),
// checked after both backgrounds since an alias can hold one of them already
static bool ram_test_block(uint32_t addr, uint32_t words, uint32_t start, uint32_t end) {
#ifdef STM32H7
  // outside the tested regions, copies are read back instead
  __attribute__((section(".sram12"))) static volatile uint32_t ram_test_backup[RAM_TEST_BLOCK_WORDS];
#endif
  volatile uint32_t *p = (uint32_t *)addr;
  volatile uint32_t *alias[RAM_TEST_ALIASES];
  uint32_t alias_val[RAM_TEST_ALIASES];
  uint8_t aliases = 0U;
  // unused slots point at the block itself and hold no pattern, only the first `aliases` slots are compared
  for (uint8_t i = 0U; i < RAM_TEST_ALIASES; i++) {
    alias[i] = p;
    alias_val[i] = 0U;
  }
  for (uint32_t bit = RAM_TEST_BLOCK_WORDS * 4U; (bit < (end - start)) && (aliases < RAM_TEST_ALIASES); bit <<= 1U) {
    uint32_t a = addr ^ bit;
    if ((a >= start) && ((a + 4U) <= end) && !ram_test_skip(a, a + 4U)) {
      alias[aliases] = (uint32_t *)a;
      aliases++;
    }
  }
  bool ok = true;
  ENTER_CRITICAL()
  // the block can hold the MPU protected safety state
  mpu_safety_open();
  for (uint32_t i = 0U; i < words; i++) {
    ram_test_backup[i] = p[i];
  }
  for (uint32_t i = 0U; i < words; i++) {
    ok = ok && (ram_test_backup[i] == p[i]);
  }
  for (uint8_t i = 0U; i < aliases; i++) {
    alias_val[i] = *alias[i];
  }
  ok = ram_test_march(p, words, 0x00000000U) && ok;
  ok = ram_test_aliases_ok(alias, alias_val, aliases) && ok;
  ok = ram_test_march(p, words, 0x55555555U) && ok;
  ok = ram_test_aliases_ok(alias, alias_val, aliases) && ok;
  for (uint32_t i = 0U; i < words; i++) {
    p[i] = ram_test_backup[i];
  }
  for (uint32_t i = 0U; i < words; i++) {
    ok = ok && (p[i] == ram_test_backup[i]);
  }
  mpu_safety_close();
  EXIT_CRITICAL()
  return ok;
}

// next block of the running pass, wraps around all regions
static bool ram_test_next(void) {
#ifdef STM32H7
  // SRAM1/2 and SRAM4 hold the DMA buffers, which DMA can write mid test, and the test's own backup, they're covered at boot, by ECC and the scrub
  static const ram_region ram_regions[] = {
    {(uint32_t)&_dtcmram_start, (uint32_t)&_dtcmram_end},
    {(uint32_t)&_axisram_start, (uint32_t)&_axisram_end},
    {(uint32_t)&_itcmram_start, (uint32_t)&_itcmram_end},
  };
#else
  static const ram_region ram_regions[] = {
    {(uint32_t)&_sram_start, (uint32_t)&_sram_end},
  };
#endif
  static uint8_t region = 0U;
  static uint32_t block = 0U;
  if (block == 0U) {
    block = ram_regions[region].start;
  }
  uint32_t words = MIN(RAM_TEST_BLOCK_WORDS, (ram_regions[region].end - block) / 4U);
  bool ok = ram_test_skip(block, block + (words * 4U)) || ram_test_block(block, words, ram_regions[region].start, ram_regions[region].end);
  block += RAM_TEST_STEP;
  if (block >= ram_regions[region].end) {
    // wraps after the last region, the F4 has only one so a modulo is avoided
    uint8_t next = (uint8_t)(region + 1U);
    region = (next < (sizeof(ram_regions) / sizeof(ram_regions[0]))) ? next : 0U;
    block = 0U;
  }
  return ok;
}

#ifdef STM32H7
// UM2331 RAM_SM_7, a read reveals an ECC error in RAM that isn't march tested at runtime
static void ram_scrub_next(void) {
  static const ram_region scrub_regions[] = {
    {(uint32_t)&_sram12_start, (uint32_t)&_sram12_end},
    {(uint32_t)&_sram4_start, (uint32_t)&_sram4_end},
  };
  static uint8_t region = 0U;
  static uint32_t addr = 0U;
  if (addr == 0U) {
    addr = scrub_regions[region].start;
  }
  const volatile uint32_t *p = (const uint32_t *)addr;
  uint32_t words = MIN(RAM_SCRUB_WORDS, (scrub_regions[region].end - addr) / 4U);
  for (uint32_t i = 0U; i < words; i++) {
    (void)p[i];
  }
  addr += words * 4U;
  if (addr >= scrub_regions[region].end) {
    region = (uint8_t)((region + 1U) % (sizeof(scrub_regions) / sizeof(scrub_regions[0])));
    addr = 0U;
  }
}
#endif

// board/crypto/sign.py: length, image, crc32 of everything before it, version tag and version, signature
static uint32_t flash_crc_len(void) {
  uint32_t len = (uint32_t)_app_start[0];
  return ((len >= 12U) && (len < sizeof(_app_start))) ? (len - 12U) : 0U;
}

static uint32_t flash_crc_stored(void) {
  const uint8_t *p = &((const uint8_t *)_app_start)[flash_crc_len()];
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8U) | ((uint32_t)p[2] << 16U) | ((uint32_t)p[3] << 24U);
}

static bool flash_test_next(void) {
  static uint32_t offset = 0U;
  static uint32_t crc = 0xFFFFFFFFU;
  bool ok = true;
  uint32_t len = flash_crc_len();
  uint32_t n = MIN(FLASH_CRC_CHUNK, len - offset);
  crc = crc32_update(crc, &((const uint8_t *)_app_start)[offset], n);
  offset += n;
  if (offset >= len) {
#ifdef ALLOW_DEBUG
    crc ^= flash_crc_flip;
    flash_crc_flip = 0U;
#endif
    ok = (len != 0U) && ((crc ^ 0xFFFFFFFFU) == flash_crc_stored());
    offset = 0U;
    crc = 0xFFFFFFFFU;
  }
  return ok;
}

static const uint32_t dma_test_pattern[DMA_TEST_WORDS] = {
  0x5AA5F00FU, 0xA55A0FF0U, 0x12345678U, 0x9ABCDEF0U, 0x0F1E2D3CU, 0x4B5A6978U, 0x87968574U, 0xC3D2E1F0U,
};

static void dma_test_start(void) {
  uint32_t len = DMA_TEST_WORDS;
#ifdef ALLOW_DEBUG
  // a short copy leaves the last word cleared
  if (self_test_injected(SELF_TEST_DMA)) {
    len -= 1U;
  }
#endif
  for (uint8_t i = 0U; i < DMA_TEST_WORDS; i++) {
    dma_test_buf[i] = 0U;
  }
  DMA2->HIFCR = DMA_HIFCR_CTCIF7 | DMA_HIFCR_CHTIF7 | DMA_HIFCR_CTEIF7 | DMA_HIFCR_CDMEIF7 | DMA_HIFCR_CFEIF7;
  DMA2_Stream7->NDTR = len;
  // the cleared buffer lands in SRAM1 before the DMA starts
  __DMB();
  DMA2_Stream7->CR |= DMA_SxCR_EN;
}

// flash to SRAM1, memory to memory needs the FIFO
static void dma_test_init(void) {
  register_set(&(DMA2_Stream7->PAR), (uint32_t)dma_test_pattern, 0xFFFFFFFFU);
  register_set(&(DMA2_Stream7->M0AR), (uint32_t)dma_test_buf, 0xFFFFFFFFU);
  register_set(&(DMA2_Stream7->FCR), DMA_SxFCR_DMDIS | DMA_SxFCR_FTH, DMA_SxFCR_DMDIS | DMA_SxFCR_FTH);
  register_set(&(DMA2_Stream7->CR), DMA_SxCR_DIR_1 | DMA_SxCR_PINC | DMA_SxCR_MINC | DMA_SxCR_PSIZE_1 | DMA_SxCR_MSIZE_1, 0x01FFFFFEU);
  dma_test_start();
}

static bool dma_test_next(void) {
  bool ok = (DMA2->HISR & (DMA_HISR_TCIF7 | DMA_HISR_TEIF7 | DMA_HISR_DMEIF7 | DMA_HISR_FEIF7)) == DMA_HISR_TCIF7;
  for (uint8_t i = 0U; i < DMA_TEST_WORDS; i++) {
    ok = ok && (dma_test_buf[i] == dma_test_pattern[i]);
  }
  // mic on DMA1 stream 0, sound and siren on DMA1 stream 1, SAI4 on BDMA channels 0 and 1, a sound stop clears them in between
#ifdef STM32H7
  ok = ok && ((DMA1->LISR & (DMA_LISR_TEIF0 | DMA_LISR_TEIF1)) == 0U);
  ok = ok && ((BDMA->ISR & (BDMA_ISR_TEIF0 | BDMA_ISR_TEIF1)) == 0U);
#endif
  dma_test_start();
  return ok;
}

static void self_test_check(bool cpu_ok, bool ram_ok, bool flash_ok) {
  bool cpu = cpu_ok;
  bool ram = ram_ok;
  bool flash = flash_ok;
#ifdef ALLOW_DEBUG
  cpu = cpu && !self_test_injected(SELF_TEST_CPU);
  ram = ram && !self_test_injected(SELF_TEST_RAM);
#endif
  if (!cpu) {
    fault_occurred(FAULT_SELF_TEST_CPU);
  }
  if (!ram) {
    fault_occurred(FAULT_SELF_TEST_RAM);
  }
  if (!flash) {
    fault_occurred(FAULT_SELF_TEST_FLASH);
  }
#ifdef STM32H7
  uint32_t flash_sr = FLASH->SR1;
  uint32_t ecc_sr = 0U;
  for (uint8_t i = 0U; i < ECC_MONITOR_CNT; i++) {
    ecc_sr |= ecc_monitors[i]->SR;
  }
#ifdef ALLOW_DEBUG
  if (self_test_injected(SELF_TEST_FLASH_ECC)) {
    flash_sr |= FLASH_SR_DBECCERR;
  }
  if (self_test_injected(SELF_TEST_RAM_ECC)) {
    ecc_sr |= RAMECC_SR_DEDF;
  }
#endif
  // a CPU read with a double ECC error bus faults, this catches the DMA reads from flash
  if ((flash_sr & FLASH_SR_DBECCERR) != 0U) {
    fault_occurred(FAULT_ECC);
  }
  if ((ecc_sr & (RAMECC_SR_DEDF | RAMECC_SR_DEBWDF)) != 0U) {
    fault_occurred(FAULT_ECC);
  }
#else
#ifdef ALLOW_DEBUG
  // no ECC on the F4, the injections still raise the fault so the test flow matches
  if (self_test_injected(SELF_TEST_FLASH_ECC) || self_test_injected(SELF_TEST_RAM_ECC)) {
    fault_occurred(FAULT_ECC);
  }
#endif
#endif
}

void self_test_init(void) {
  crc32_init();

#ifdef STM32H7
  // the boot RAM test wrote every word, so no RAM holds an unset ECC code anymore
  for (uint8_t i = 0U; i < ECC_MONITOR_CNT; i++) {
    ecc_monitors[i]->SR = 0U;
    register_set(&(ecc_monitors[i]->CR), RAMECC_CR_ECCELEN, 0x3CU);
  }
#endif

  bool ram_ok = (self_test_cookie == SELF_TEST_PASS);
  self_test_cookie = 0U;
  uint32_t len = flash_crc_len();
  bool flash_ok = (len != 0U) && (crc32((const uint8_t *)_app_start, len) == flash_crc_stored());
  self_test_check((cpu_test() == SELF_TEST_PASS) && (fpu_test() == SELF_TEST_PASS), ram_ok, flash_ok);
  dma_test_init();
}

// called at 8Hz
void self_test_tick(void) {
  bool cpu_ok = (cpu_test() == SELF_TEST_PASS) && (fpu_test() == SELF_TEST_PASS);
  bool ram_ok = ram_test_next();
#ifdef STM32H7
  ram_scrub_next();
#endif
  bool flash_ok = flash_test_next();
  self_test_check(cpu_ok, ram_ok, flash_ok);
  if (!dma_test_next()) {
    fault_occurred(FAULT_DMA);
  }
}
