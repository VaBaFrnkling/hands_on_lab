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
