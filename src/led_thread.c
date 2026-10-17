#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>

#include "led_thread.h"

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

    // Percentage of the 12-bit ADC range; this is not calibrated illuminance.
    uint8_t brightness_pct = (uint8_t)((raw_val * 100) / 4095);

    const char *color;
    if (raw_val * 100 < 33 * 4095)
    {
        rgb_set(true, false, false);
        color = "RED";
    }
    else if (raw_val * 100 <= 66 * 4095)
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