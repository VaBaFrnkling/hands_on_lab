#ifndef LED_THREAD_H
#define LED_THREAD_H

int rgb_init(void);
void rgb_set(bool r, bool g, bool b);
void process_adc_raw_sample(int16_t raw_val);
void led_thread(void *p1, void *p2, void *p3);

#endif

