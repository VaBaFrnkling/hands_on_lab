#ifndef ADC_THREAD_H
#define ADC_THREAD_H

int adc_init(void);
int adc_get_sample(void);
void adc_thread(void *a, void *b, void *c);

#endif
