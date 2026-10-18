#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>

#include "button_isr.h"
#include "led_thread.h"
#include "shared.h"

// Definition of the RGB LED pins
static const struct gpio_dt_spec led_r = GPIO_DT_SPEC_GET(DT_NODELABEL(rgb_red), gpios);
static const struct gpio_dt_spec led_g = GPIO_DT_SPEC_GET(DT_NODELABEL(rgb_green), gpios);
static const struct gpio_dt_spec led_b = GPIO_DT_SPEC_GET(DT_NODELABEL(rgb_blue), gpios);

/**
 * rgb_init: Initializes the RGB LED pins as outputs and sets them to inactive (off).
 * Returns 0 on success, or -1 if a pin is not ready or configuration fails.
 */
int rgb_init(void)
{
    if (!gpio_is_ready_dt(&led_r) || !gpio_is_ready_dt(&led_g) || !gpio_is_ready_dt(&led_b))
    {
        return -1;
    }
    if (gpio_pin_configure_dt(&led_r, GPIO_OUTPUT_INACTIVE) < 0)
        return -1;
    if (gpio_pin_configure_dt(&led_g, GPIO_OUTPUT_INACTIVE) < 0)
        return -1;
    if (gpio_pin_configure_dt(&led_b, GPIO_OUTPUT_INACTIVE) < 0)
        return -1;
    return 0;
}

/**
 * rgb_set: Sets the RGB LED pins to the specified states (on/off).
 * @param r: true to turn on the red LED, false to turn it off.
 * @param g: true to turn on the green LED, false to turn it off.
 * @param b: true to turn on the blue LED, false to turn it off.
 */
void rgb_set(bool r, bool g, bool b)
{
    gpio_pin_set_dt(&led_r, r);
    gpio_pin_set_dt(&led_g, g);
    gpio_pin_set_dt(&led_b, b);
}

/*
 * process_adc_raw_sample: Processes the raw ADC sample and updates the RGB LED color.
 * @param raw_val: The raw ADC value (0-4095).
 */
void process_adc_raw_sample(int16_t raw_val)
{
    if (raw_val < 0)
        raw_val = 0;
    if (raw_val > 4095)
        raw_val = 4095;

    // Inverison of the ADC value to represent brightness (0 = dark, 4095 = bright)
    int32_t brightness_raw = 4095 - raw_val;

    // Percentage of the 12-bit ADC range; this is not calibrated illuminance.
    uint8_t brightness_pct = (uint8_t)((brightness_raw * 100) / 4095);

    const char *color;
    if (brightness_raw * 100 < 33 * 4095)
    {
        rgb_set(true, false, false);
        color = "RED";
    }
    else if (brightness_raw * 100 <= 66 * 4095)
    {
        rgb_set(true, true, false);
        color = "YELLOW";
    }
    else
    {
        rgb_set(false, true, false); // Green   (> 66%)
        color = "GREEN";
    }
    printk("NORMAL: ADC=%d/4095, brightness=%u%%, RGB=%s\n",
           (int)raw_val, (unsigned int)brightness_pct, color);
}

/**
 * led_thread: Thread function that manages the RGB LED based on button events and ADC samples.
 * Polls button events without blocking and processes ADC samples only in NORMAL.
 * Sleeps 50 ms between iterations; stops if RGB initialization fails.
 */
void led_thread(void *p1, void *p2, void *p3)
{
    (void)p1;
    (void)p2;
    (void)p3;

    int ret = rgb_init();
    if (ret < 0)
    {
        printk("Error to initialize RGB: %d\n", ret);
        return;
    }

    int16_t sample;
    enum button_event evt;
    printk("Mode: NORMAL; waiting for ADC on A0/PB1\n");

    while (1)
    {
        // Poll one queued button event; BUTTON_NONE means no event is pending.
        evt = button_wait_event(K_NO_WAIT);

        // B. Update the current mode based on the button event
        switch ((system_mode_t)atomic_get(&current_mode))
        {

        case MODE_NORMAL:
            if (evt == BUTTON_SHORT)
            {
                system_set_mode(MODE_BLUE);
                rgb_set(false, false, true); // Blue
            }
            else if (evt == BUTTON_LONG)
            {
                system_set_mode(MODE_OFF);
                rgb_set(false, false, false); // Turn off the LED
            }
            break;

        case MODE_BLUE:
            if (evt == BUTTON_SHORT)
            {
                system_set_mode(MODE_NORMAL);
                rgb_set(false, false, false); // Clear BLUE while awaiting a fresh sample.
            }
            else if (evt == BUTTON_LONG)
            {
                system_set_mode(MODE_OFF);
                rgb_set(false, false, false); // Turn off the LED
            }
            break;

        case MODE_OFF:
            if (evt == BUTTON_SHORT)
            {
                system_set_mode(MODE_NORMAL);
            }
            // A long press in OFF leaves the mode unchanged.
            break;
        }
        /* Process raw ADC samples only in NORMAL. */
        if (atomic_get(&current_mode) == MODE_NORMAL)
        {
            // Consume one raw sample if the ADC has queued one.
            if (k_msgq_get(&sample_q, &sample, K_NO_WAIT) == 0)
            {
                process_adc_raw_sample(sample);
            }
        }

        k_msleep(50); // Yield between polls.
    }
}

// Thread definition for the LED thread
K_THREAD_DEFINE(led_thread_id, 1024, led_thread, NULL, NULL, NULL, 4, 0, 0);
