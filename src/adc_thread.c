#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/drivers/adc.h>

#include "adc_thread.h"

// ADC node definition
#define ADC_NODE DT_ALIAS(analog_in_pin)

// Variables obtained for the ADC samples from the LDR
static int16_t sample_raw;
struct adc_sequence sequence;

// Variables for the ADC channel configuration
static const struct adc_dt_spec adc_ch = ADC_DT_SPEC_GET(ADC_NODE);

/**
 * adc_init: Initializes the ADC channel and sequence for reading samples.
 * Returns 0 on success, or a negative error code if the ADC is not ready or
 * if the channel setup fails.
 */
static int adc_init(void)
{
    if (!adc_is_ready_dt(&adc_ch))
        return -1;

    int ret = adc_channel_setup_dt(&adc_ch);
    if (ret < 0)
        return ret;

    sequence.buffer = &sample_raw;
    sequence.buffer_size = sizeof(sample_raw);

    return adc_sequence_init_dt(&adc_ch, &sequence);
}

/**
 * adc_get_sample: Reads a sample_raw from the ADC channel and returns the value.
 * Returns the sample_raw value on success, or a negative error code if the read fails. 
 */
static int adc_get_sample(void)
{
    int ret = adc_read_dt(&adc_ch, &sequence);
    if (ret < 0)
        return ret;

    return sample_raw;
}
