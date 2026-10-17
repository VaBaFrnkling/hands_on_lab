#ifndef BUTTON_ISR_H
#define BUTTON_ISR_H

#include <zephyr/kernel.h>

enum button_event
{
    BUTTON_NONE,
    BUTTON_SHORT,
    BUTTON_LONG
};

// Allows waiting for a button event with a specified timeout. If the timeout expires before an event occurs, it returns BUTTON_NONE.
enum button_event button_wait_event(k_timeout_t timeout);

#endif
