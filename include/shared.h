#ifndef SHARED_H
#define SHARED_H

#include <zephyr/kernel.h>
#include <stdint.h>
#include <zephyr/sys/atomic.h>

// Queue declaration for adc and led threads
extern struct k_msgq sample_q;

// Wake the ADC on mode changes or interrupt its periodic sampling wait.
extern struct k_sem signal_adc;

// States operation mode
typedef enum {
    MODE_NORMAL,
    MODE_BLUE,
    MODE_OFF
} system_mode_t;

extern atomic_t current_mode;

// Protect mode transitions, sample publication and the generation counter.
extern struct k_mutex system_state_lock;

// Shared variable for the current mode generation
extern uint32_t mode_generation;

#endif
