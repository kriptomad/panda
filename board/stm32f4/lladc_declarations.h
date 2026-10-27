#pragma once

typedef enum {
  SAMPLETIME_3_CYCLES = 0,
  SAMPLETIME_15_CYCLES = 1,
  SAMPLETIME_28_CYCLES = 2,
  SAMPLETIME_56_CYCLES = 3,
  SAMPLETIME_84_CYCLES = 4,
  SAMPLETIME_112_CYCLES = 5,
  SAMPLETIME_144_CYCLES = 6,
  SAMPLETIME_480_CYCLES = 7
} adc_sample_time_t;

// no hardware oversampling on the F4, the samples are averaged in software
typedef enum {
  OVERSAMPLING_1 = 0,
  OVERSAMPLING_2 = 1,
  OVERSAMPLING_4 = 2,
  OVERSAMPLING_8 = 3,
  OVERSAMPLING_16 = 4,
  OVERSAMPLING_32 = 5,
  OVERSAMPLING_64 = 6,
  OVERSAMPLING_128 = 7,
  OVERSAMPLING_256 = 8,
  OVERSAMPLING_512 = 9,
  OVERSAMPLING_1024 = 10
} adc_oversampling_t;

typedef struct {
  ADC_TypeDef *adc;
  uint8_t channel;
  adc_sample_time_t sample_time;
  adc_oversampling_t oversampling;
} adc_signal_t;

#define ADC_CHANNEL_DEFAULT(a, c) {.adc = (a), .channel = (c), .sample_time = SAMPLETIME_480_CYCLES, .oversampling = OVERSAMPLING_16}

// RM0430 / DS11581: factory calibration at VDDA = 3.3V, 12 bit
#define VREFINT_CAL_ADDR ((uint16_t *)0x1FFF7A2AUL)
#define TEMPSENSOR_CAL1_ADDR ((uint16_t *)0x1FFF7A2CUL)
#define TEMPSENSOR_CAL2_ADDR ((uint16_t *)0x1FFF7A2EUL)
#define TEMPSENSOR_CAL1_TEMP 30
#define TEMPSENSOR_CAL2_TEMP 110
#define ADC_CHANNEL_VREFINT 17U
#define ADC_CHANNEL_TEMPSENSOR 18U