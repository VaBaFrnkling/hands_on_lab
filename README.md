# Hands-on Lab: ADC-Based RGB LED Control with Zephyr RTOS

## Authors
- Frank Julio Vásquez Báez
- Dunia Amroune Faidi

## Overview

This project is a concurrent embedded application built with **Zephyr RTOS** for the **NUCLEO-WL55JC** development board. It reads an analog light-sensor signal through the ADC and controls an RGB LED. A push button switches between three operating modes: `NORMAL`, `BLUE`, and `OFF`.

The application demonstrates RTOS threads, GPIO interrupts, work queues, delayed work, message queues, semaphores, mutexes, atomic variables, and inter-thread communication.

## Hardware

- NUCLEO-WL55JC development board
- Analog light sensor / photoresistor module connected to the configured ADC input (`A0/PB1`, using the `analog_in_pin` devicetree alias)
- RGB LED outputs configured through the devicetree nodes `rgb_red`, `rgb_green`, and `rgb_blue` (reported by the application as D2/PB12, D3/PB3, and D4/PB5)
- Board push button selected through the `sw1` devicetree alias (Button B2 in this setup)
- Jumper wires and suitable current-limiting resistors for any externally connected LED channels

**Electrical note:** Keep all sensor output voltages within the board's ADC input limits and use the correct supply voltage for the sensor module. Check the board pinout and circuit wiring before powering the hardware.

## Implementation

The source code is organized into separate modules:

- `src/main.c` initializes the button interface and reports initialization errors.
- `src/adc_thread.c` initializes the ADC and periodically samples the analog input while the system is in `NORMAL` mode.
- `src/led_thread.c` initializes the RGB outputs, handles queued button events, and updates the LED based on the active mode and ADC readings.
- `src/button_isr.c` configures the button GPIO interrupt, processes button state changes using the system work queue, detects short and long presses, and places button events in a message queue. No additional software debounce delay is used because the board provides hardware debouncing.
- `src/shared.c` defines the shared mode state and synchronization objects, and implements `system_set_mode()`.
- `include/` contains the corresponding module interfaces and shared declarations.
- `confs/prj_nucleo_wl55jc.conf` contains the project configuration for the target board.

### Operating modes

| Mode | Behavior | Button action |
|---|---|---|
| `NORMAL` | ADC samples are processed and the RGB LED indicates the sample range: red below 33%, yellow from 33% through 66%, and green above 66% of the 12-bit ADC range. | Short press switches to `BLUE`; long press switches to `OFF`. |
| `BLUE` | The RGB LED is set to blue and ADC acquisition is suspended. | Short press switches to `NORMAL`; long press switches to `OFF`. |
| `OFF` | The RGB LED is turned off and ADC acquisition is suspended. | Short press switches to `NORMAL`; long press leaves the mode unchanged. |

The percentage shown in the serial output is calculated from the raw 12-bit ADC value. It is a percentage of the ADC range, **not a calibrated measurement of illuminance**.

## Thread architecture

Two application threads are defined with `K_THREAD_DEFINE`:

### ADC thread

- Priority: `5`
- Stack size: `1024` bytes
- Reads the ADC immediately when entering `NORMAL`, then waits up to two seconds between acquisitions.
- Uses a semaphore wait that can be interrupted by a mode-change notification.
- Publishes valid raw samples to `sample_q` without blocking.
- Checks the mode and a generation counter before publishing, so a sample acquired across a mode transition is discarded.

### LED thread

- Priority: `4`
- Stack size: `1024` bytes
- Checks for queued button events without blocking.
- Changes modes and controls the RGB LED accordingly.
- Consumes ADC samples only in `NORMAL` mode and sleeps for 50 ms between loop iterations.

In Zephyr, a numerically lower thread-priority value represents a higher priority, so the LED thread (priority 4) has higher priority than the ADC thread (priority 5).

### Button interrupt and work queue

The GPIO interrupt is triggered on both edges of the button signal. Since the NUCLEO-WL55JC board already provides hardware debouncing, no additional software debounce delay is implemented.

Instead of performing lengthy processing inside the ISR, the interrupt submits a `k_work` item to the system work queue using `k_work_submit()`. The `button_handler()` function reads the GPIO state and determines whether the button has been pressed or released.

A separate `k_work_delayable` item is used to detect long presses. When the button is pressed, the long-press work is scheduled to execute after one second. If the button remains pressed when the threshold is reached, a `BUTTON_LONG` event is generated without waiting for the button to be released.

If the button is released before the long-press threshold, the pending work is canceled and a `BUTTON_SHORT` event is generated. The `long_press_detected` flag prevents a long press from generating an additional short-press event upon release.

Accepted button events are transmitted through `button_event_q` to the LED thread for processing.

**Implementation limitation:** `k_work_submit()` does not queue a separate execution for every GPIO edge. Very brief presses may be missed if the button is pressed and released before `button_handler()` reads the GPIO state. An ISR-side edge queue would be needed to preserve every transition reliably.

## Inter-thread communication and synchronization

| Mechanism | Purpose |
|---|---|
| `sample_q` (`k_msgq`) | Transfers raw `int16_t` ADC samples from the ADC thread to the LED thread. The queue holds four samples. |
| `button_event_q` (`k_msgq`) | Transfers button events from the button work handlers to the LED thread. The queue holds eight events. |
| `signal_adc` (`k_sem`) | Wakes the ADC thread when the operating mode changes and interrupts its timed wait. |
| `system_state_lock` (`k_mutex`) | Protects mode transitions, sample publication, and the mode-generation counter. |
| `current_mode` (atomic variable) | Stores the current operating mode for safe atomic reads and updates. |
| `mode_generation` | Identifies mode transitions so an ADC acquisition spanning a transition cannot be published as a current sample. |
| `button_work` (`k_work`) | Processes button press and release state changes outside the GPIO ISR without an additional software debounce delay. |
| `long_press_work` (`k_work_delayable`) | Schedules long-press detection after one second, allowing the event to be generated without waiting for button release. |

When the mode changes, `system_set_mode()` locks the shared state, purges stale ADC samples, increments the generation counter, updates the atomic mode, and signals the ADC thread. This prevents queued samples from an earlier mode from being treated as current data.

## Build and flash

Install the Zephyr development environment and its required toolchain first. See the [official Zephyr Getting Started Guide](https://docs.zephyrproject.org/latest/develop/getting_started/index.html).

Run the following commands from the project root, where `CMakeLists.txt` is located.

### Build for NUCLEO-WL55JC

```bash
west build -b nucleo_wl55jc/stm32wl55xx -d build -- -DEXTRA_CONF_FILE=confs/prj_nucleo_wl55jc.conf
```

If your installed Zephyr version uses a different board-target spelling, check the board's entry in the Zephyr documentation and substitute the appropriate target.

### Flash the board

Connect the board to your computer and run:

```bash
west flash -d build
```

### View serial output

Open a serial terminal on the board's UART/console interface using the baud rate configured by your board and environment. The application prints initialization messages, mode changes, button events, ADC readings, and error messages through `printk()`.

### Clean rebuild

If you need to rebuild from a clean build directory, run:

```bash
west build -t pristine -d build
west build -b nucleo_wl55jc/stm32wl55xx -d build -- -DEXTRA_CONF_FILE=confs/prj_nucleo_wl55jc.conf
```

## Configuration

The file `confs/prj_nucleo_wl55jc.conf` enables console and UART output, `printk()`, GPIO, ADC, multithreading, and logging. The application uses devicetree aliases/nodes for the ADC input, button, and RGB outputs; make sure these names are defined for the selected board or its devicetree overlay.

## Testing checklist

1. Build and flash the application to the NUCLEO-WL55JC.
2. Open the serial console and confirm that button initialization succeeds.
3. In `NORMAL`, vary the light sensor input and check the ADC readings and red/yellow/green LED indication.
4. Press the button briefly and confirm that the mode changes to `BLUE`.
5. Press it briefly again and confirm that the system returns to `NORMAL`.
6. Hold the button for about one second or longer and confirm that the mode changes to `OFF`.
7. In `OFF`, use a short press to return to `NORMAL`.
8. Check the console for ADC, GPIO, or queue-related errors.
9. Verify that short and long presses are correctly detected without software debouncing and that releasing the button after a long press does not generate an additional short-press event.

## References

- [Zephyr Project documentation](https://docs.zephyrproject.org/latest/)
- [Zephyr Getting Started Guide](https://docs.zephyrproject.org/latest/develop/getting_started/index.html)
