#ifndef LED_THREAD_H
#define LED_THREAD_H

int rgb_init(void);
void rgb_set(bool r, bool g, bool b);
void process_adc_raw_sample(int16_t raw_val);


#endif

