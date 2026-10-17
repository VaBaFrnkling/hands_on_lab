#include <zephyr/sys/printk.h>

#include "shared.h"

// Shared operation mode for the LED and ADC threads
atomic_t current_mode = ATOMIC_INIT(MODE_NORMAL);

// Queue of raw ADC samples
K_MSGQ_DEFINE_TYPE(sample_q, int16_t, 4);

// Notify the ADC thread of mode changes
K_SEM_DEFINE(signal_adc, 0, 1);

// Mutex to protect access to the system state
K_MUTEX_DEFINE(system_state_lock);

// Shared variable for the current mode generation
uint32_t mode_generation;

/**
 * system_set_mode: Set the system operation mode, notifying the ADC thread of the change.
 * @param mode: The new system mode to set (MODE_NORMAL, MODE_BLUE, or MODE_OFF).
 */
void system_set_mode(system_mode_t mode)
{
    k_mutex_lock(&system_state_lock, K_FOREVER);
    atomic_val_t previous = atomic_get(&current_mode);

    if (previous != (atomic_val_t)mode)
    {
        // Drop queued data and invalidate any acquisition already in progress.
        k_msgq_purge(&sample_q);
        mode_generation++;
        atomic_set(&current_mode, (atomic_val_t)mode);
        k_sem_give(&signal_adc);
    }
    k_mutex_unlock(&system_state_lock);
    if (previous != (atomic_val_t)mode)
    {
        const char *name = mode == MODE_NORMAL ? "NORMAL" :
                           mode == MODE_BLUE ? "BLUE" : "OFF";
        printk("Mode: %s\n", name);
        if (mode == MODE_OFF)
        {
            printk("System OFF\n");
        }
    }
}
