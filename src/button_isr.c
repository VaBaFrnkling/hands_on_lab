#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>

#include "button_isr.h"

// Button selected by the board's sw1 devicetree alias.
#define BUTTON_NODE DT_ALIAS(sw1)

// Configuration and callback of button
static const struct gpio_dt_spec button2 = GPIO_DT_SPEC_GET(BUTTON_NODE, gpios);
static struct gpio_callback button2_cb;

// Both handlers run sequentially on the system work queue.
static struct k_work_delayable long_press_work;

// work for the button press/release events
static struct k_work button_work;

// Keep accepted events in order without blocking the system work queue.
K_MSGQ_DEFINE(button_event_q, sizeof(enum button_event), 8, 1);

// Press state is accessed only by handlers on the system work queue.
static bool button_pressed = false;
static bool long_press_detected = false;
static int64_t press_started_ms;

/**
 * button_send_event: Queues a button event for processing by the LED thread.
 * @param event: The button event to queue.
 */
static void button_send_event(enum button_event event)
{
    if (k_msgq_put(&button_event_q, &event, K_NO_WAIT) != 0)
    {
        printk("Button event queue full: dropped event %d\n", (int)event);
    }
    else
    {
        printk("Button event: %s\n", event == BUTTON_LONG ? "LONG" : "SHORT");
    }
}

/**
 * long_press_handler: Handles the detection of a long button press.
 * It checks if the button is still pressed after 1 second and sends a BUTTON_LONG event if so.
 * @param work: Pointer to the work structure associated with this handler.
 */
static void long_press_handler(struct k_work *work)
{
    (void)work;

    if (button_pressed && !long_press_detected)
    {
        long_press_detected = true;
        button_send_event(BUTTON_LONG);
    }
}

/**
    * button_handler: Handles button press and release events. 
    * It reads the button state, determines if it is a press or release, 
    * and schedules the long press detection handler if necessary.
    * @param work: Pointer to the work structure associated with this handler.
 */
static void button_handler(struct k_work *work)
{
    (void)work;

    int value = gpio_pin_get_dt(&button2);
    if (value < 0)
    {
        printk("Error reading button GPIO: %d\n", value);
        return;
    }

    // Boton pressed
    if (value && !button_pressed)
    {
        printk("Button pressed\n");
        button_pressed = true;
        long_press_detected = false;
        press_started_ms = k_uptime_get();

        k_work_reschedule(&long_press_work, K_SECONDS(1));
    }
    // Boton released
    else if (!value && button_pressed)
    {
        printk("Button released\n");
        button_pressed = false;
        // No synchronous cancellation: both handlers use this same work queue.
        k_work_cancel_delayable(&long_press_work);

        // Emit a release event only if this press has not already emitted LONG.
        if (!long_press_detected)
        {
            // Classify by elapsed time even if the long-press work was delayed.
            enum button_event event =
                (k_uptime_get() - press_started_ms >= 1000)
                    ? BUTTON_LONG
                    : BUTTON_SHORT;
            button_send_event(event);
        }
        long_press_detected = false;
    }
}

/**
 * button_isr: Interrupt Service Routine for the button GPIO. 
 * It is triggered on both rising and falling edges of the button signal.
 * This function submits the button work to the system work queue for processing.
 * @param port: Pointer to the GPIO device structure.
 * @param cb: Pointer to the GPIO callback structure.
 * @param pins: The pin mask indicating which pins triggered the interrupt.
 */
static void button_isr(const struct device *port,
                       struct gpio_callback *cb,
                       gpio_port_pins_t pins)
{
    (void)port;
    (void)cb;
    (void)pins;

    // Submit the button work to the work queue
    k_work_submit(&button_work);
}

/**
 * button_init: Initializes the button GPIO, configures the interrupt, 
 * and sets up the work queue for handling button events.
 * Returns 0 on success or a negative error code on failure.
 */
int button_init(void)
{
    if (!gpio_is_ready_dt(&button2))
    {
        return -ENODEV;
    }

    int ret = gpio_pin_configure_dt(&button2, GPIO_INPUT);
    if (ret < 0)
        return ret;

    // Prepare all resources before enabling the GPIO interrupt.
    k_work_init_delayable(&long_press_work, long_press_handler);
    k_work_init(&button_work, button_handler);

    // Configure the button callback and add it to the GPIO driver.
    gpio_init_callback(&button2_cb, button_isr, BIT(button2.pin));
    ret = gpio_add_callback(button2.port, &button2_cb);
    if (ret < 0)
        return ret;

    // Enable interrupts only after the callback and its resources are ready.
    return gpio_pin_interrupt_configure_dt(&button2, GPIO_INT_EDGE_BOTH);
}

/**
 * button_wait_event: Waits for a button event to be queued.  
 * @param timeout: The maximum time to wait for an event.
 */
enum button_event button_wait_event(k_timeout_t timeout)
{
    enum button_event event;

    if (k_msgq_get(&button_event_q, &event, timeout) != 0)
    {
        return BUTTON_NONE;
    }

    return event;
}
