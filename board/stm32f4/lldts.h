#pragma once

// Internal temperature sensor on ADC1 channel 18 (the STM32F4 has no DTS peripheral)

void dts_init(void) {
  // TSVREFE is enabled in adc_init(). VBATE must stay off since it shares channel 18.
  register_clear_bits(&(ADC->CCR), ADC_CCR_VBATE);
}

float dts_get_temperature(void) {
  // factory calibration at 30C and 110C (12 bit, VDDA = 3.3V); the raw value is rescaled to 3.3V using the measured AVDD
  float ret = 0.0f;
  uint16_t raw = adc_get_raw(&(adc_signal_t){.adc = ADC1, .channel = ADC_CHANNEL_TEMPSENSOR, .sample_time = SAMPLETIME_480_CYCLES, .oversampling = OVERSAMPLING_64});
  float cal1 = (float) *TEMPSENSOR_CAL1_ADDR;
  float cal2 = (float) *TEMPSENSOR_CAL2_ADDR;
  if ((adc_avdd_mV != 0U) && (cal2 > cal1)) {
    float raw_3v3 = ((float) raw * (float) adc_avdd_mV) / 3300.0f;
    ret = (float) TEMPSENSOR_CAL1_TEMP + ((raw_3v3 - cal1) * (float) (TEMPSENSOR_CAL2_TEMP - TEMPSENSOR_CAL1_TEMP) / (cal2 - cal1));
  }
  return ret;
}