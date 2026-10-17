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

// work delayable for the debounce
static struct k_work_delayable debounce_work;

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
 * long_press_handler: Handles long press detection. It checks if the button is still pressed after a specified duration (1 second) and sends a BUTTON_LONG event if the condition is met.
 * This function is scheduled to run after the long press duration has elapsed.     
 * @param work: Pointer to the work structure associated with this handler.   
 */
static void long_press_handler(struct k_work *work)
{
    (void)work;

    if (button_pressed && !long_press_detected)
    {
        int64_t elapsed = k_uptime_get() - press_started_ms;

        // A previously queued invocation must not classify a newer press early.
        if (elapsed < 1000)
        {
            k_work_reschedule(&long_press_work, K_MSEC(1000 - elapsed));
            return;
        }

        long_press_detected = true;
        button_send_event(BUTTON_LONG);
    }
}

/**
 * debounce_handler: Handles button debounce by checking the button state after a short delay (15ms).
 * It determines whether the button was pressed or released and sets the appropriate event (short or long press) accordingly.
 * Events are queued for the LED thread.
 * @param work: Pointer to the work structure associated with this handler.
 */
static void debounce_handler(struct k_work *work)
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
 * button_isr: Interrupt Service Routine (ISR) for the button. It is triggered on both rising and falling edges of the button signal.
 * The ISR schedules a debounce handler to run after a short delay (15ms) to filter out mechanical bounce and determine the actual button state (pressed or released).
 * The debounce handler queues short or long press events for the LED thread.
 * @param port: Pointer to the GPIO device structure for the button's port.
 * @param cb: Pointer to the GPIO callback structure associated with this ISR.  
 * @param pins: Bitmask of the pins that triggered the interrupt (not used in this implementation).
 */
static void button_isr(const struct device *port,
                       struct gpio_callback *cb,
                       gpio_port_pins_t pins)
{
    (void)port;
    (void)cb;
    (void)pins;

    // Schedule the debounce handler to run after 15ms to handle button state changes and determine the type of press (short or long).
    k_work_reschedule(&debounce_work, K_MSEC(15));
}

/**
 * button_init: Initializes the button, callback and delayed work.
 * Returns 0 on success, or a negative error code on failure.
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
    k_work_init_delayable(&debounce_work, debounce_handler);

    // Configure the button callback and add it to the GPIO driver.
    gpio_init_callback(&button2_cb, button_isr, BIT(button2.pin));
    ret = gpio_add_callback(button2.port, &button2_cb);
    if (ret < 0)
        return ret;

    // Enable interrupts only after the callback and its resources are ready.
    return gpio_pin_interrupt_configure_dt(&button2, GPIO_INT_EDGE_BOTH);
}

/**
 * button_wait_event: Waits for a button event (short or long press) and returns the corresponding event type.
 * Returns BUTTON_NONE if no event is received within the supplied timeout.
 * @param timeout: The maximum time to wait for a button event. If the timeout expires before an event occurs, it returns BUTTON_NONE.
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
