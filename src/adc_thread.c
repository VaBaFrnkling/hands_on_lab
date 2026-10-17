#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/drivers/adc.h>

#include "adc_thread.h"
#include "shared.h"

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
int adc_init(void)
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
int adc_get_sample(void)
{
    int ret = adc_read_dt(&adc_ch, &sequence);
    if (ret < 0)
        return ret;

    return sample_raw;
}

/**
 * adc_thread: Thread function for the ADC sampling. It waits for the system to be in MODE_NORMAL,
 * acquires samples from the ADC, and sends them to the sample queue for processing by the LED thread. 
 * It handles mode changes and ensures that samples are only acquired in the correct mode.
 */
void adc_thread(void *a, void *b, void *c)
{
    (void)a;
    (void)b;
    (void)c;

    int ret = adc_init();

    if (ret < 0)
    {
        printk("Error to initialize ADC: %d\n", ret);
        return;
    }

    bool sample_now = true;

    while (1)
    {
        if (atomic_get(&current_mode) != MODE_NORMAL)
        {
            printk("ADC thread waiting for MODE_NORMAL\n");
            k_sem_take(&signal_adc, K_FOREVER);
            printk("ADC thread resuming in MODE_NORMAL\n");
            sample_now = true;
            continue;
        }

        if (!sample_now)
        {
            printk("ADC thread waiting for next sample acquisition\n");
            ret = k_sem_take(&signal_adc, K_SECONDS(2));
            if (ret == 0)
            {
                printk("ADC thread resuming for immediate sample acquisition\n");
                sample_now = true;
                continue;
            }
            if (ret != -EAGAIN)
            {
                printk("Error waiting for semaphore: %d\n", ret);
                continue;
            }
        }
        sample_now = false;

        k_mutex_lock(&system_state_lock, K_FOREVER);
        if (atomic_get(&current_mode) != MODE_NORMAL)
        {
            printk("ADC thread detected mode change before acquisition\n");
            k_mutex_unlock(&system_state_lock);
            continue;
        }
        // Capture the current mode generation to detect changes during acquisition
        uint32_t acquisition_generation = mode_generation;
        printk("ADC thread acquiring sample in generation %u\n", acquisition_generation);
        k_sem_take(&signal_adc, K_NO_WAIT);
        k_mutex_unlock(&system_state_lock);

        int value = adc_get_sample();
        printk("ADC thread acquired sample: %d\n", value);

        if (value < 0)
        {
            printk("Error reading ADC: %d\n", value);
            continue;
        }

        k_mutex_lock(&system_state_lock, K_FOREVER);
        if (atomic_get(&current_mode) != MODE_NORMAL ||
            acquisition_generation != mode_generation)
        {
            k_mutex_unlock(&system_state_lock);
            sample_now = true;
            continue;
        }
        ret = k_msgq_put(&sample_q, &sample_raw, K_NO_WAIT);
        k_mutex_unlock(&system_state_lock);

        if (ret < 0)
        {
            printk("Error to send sample_raw to led_thread\n");
            continue;
        }
        printk("ADC thread sent sample_raw to led_thread: %d\n", sample_raw);
    }
    printk("ADC thread exiting\n");
}

// Thread definition for the ADC thread
K_THREAD_DEFINE(adc_thread_id, 1024, adc_thread, NULL, NULL, NULL, 5, 0, 0);
