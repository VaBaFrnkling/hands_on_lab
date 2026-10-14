#ifndef APP_H
#define APP_H

// Different modes of operation for the application
enum Modo {
    NORMAL,
    BLUE,
    OFF
};

// Functions to initialize the peripherals
int adc_init(void);
int led_init(void);
int button_init(void);

#endif