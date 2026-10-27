#pragma once

#include "lladc_declarations.h"

static uint32_t adc_avdd_mV = 0U;

void adc_init(ADC_TypeDef *adc) {
  // temperature sensor and VREFINT on, VBAT off (shares channel 18 with the temperature sensor)
  register_set(&(ADC->CCR), ADC_CCR_TSVREFE, 0xC30000U);
  register_set(&(adc->CR2), ADC_CR2_ADON, 0xFF7F0F03U);
}

static uint16_t adc_get_raw_single(const adc_signal_t *signal) {
  // sample time
  if (signal->channel < 10U) {
    signal->adc->SMPR2 = ((uint32_t) signal->sample_time << (signal->channel * 3U));
  } else {
    signal->adc->SMPR1 = ((uint32_t) signal->sample_time << ((signal->channel - 10U) * 3U));
  }

  // select channel, one injected conversion
  signal->adc->JSQR = ((uint32_t) signal->channel << 15U);

  // start conversion
  signal->adc->SR &= ~(ADC_SR_JEOC);
  signal->adc->CR2 |= ADC_CR2_JSWSTART;
  // a stuck ADC reads 0
  uint32_t timeout = 1000000U;
  while (((signal->adc->SR & ADC_SR_JEOC) == 0U) && (timeout > 0U)) {
    timeout--;
  }
  uint16_t res = (timeout > 0U) ? (uint16_t)signal->adc->JDR1 : 0U;
  signal->adc->SR &= ~(ADC_SR_JEOC);

  return res;
}

// 12 bit, averaged over 2^oversampling conversions
uint16_t adc_get_raw(const adc_signal_t *signal) {
  uint32_t samples = 1UL << (uint32_t) signal->oversampling;
  uint32_t sum = 0U;
  for (uint32_t i = 0U; i < samples; i++) {
    sum += adc_get_raw_single(signal);
  }
  return (uint16_t)(sum / samples);
}

static uint32_t adc_measure_avdd_mV(void) {
  uint16_t raw_vrefint = adc_get_raw(&(adc_signal_t){.adc = ADC1, .channel = ADC_CHANNEL_VREFINT, .sample_time = SAMPLETIME_480_CYCLES, .oversampling = OVERSAMPLING_256});
  return (raw_vrefint != 0U) ? ((uint32_t) *VREFINT_CAL_ADDR * 3300U / raw_vrefint) : 0U;
}

static void adc_calibrate_vdda(void) {
  adc_avdd_mV = adc_measure_avdd_mV();
  print("  AVDD: 0x"); puth(adc_avdd_mV); print(" mV\n");
}

// ADC_SM_2, ADC_SM_3: AVDD from VREFINT has to match the board
bool adc_check(uint32_t expected_avdd_mV) {
  uint32_t avdd_mV = adc_measure_avdd_mV();
  return (avdd_mV > ((expected_avdd_mV * 9U) / 10U)) && (avdd_mV < ((expected_avdd_mV * 11U) / 10U));
}

uint16_t adc_get_mV(const adc_signal_t *signal) {
  if (adc_avdd_mV == 0U) {
    adc_calibrate_vdda();
  }
  return (adc_get_raw(signal) * adc_avdd_mV) / 4095U;
}