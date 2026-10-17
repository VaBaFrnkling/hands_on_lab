#include <zephyr/sys/printk.h>

#include "button_isr.h"

int main(void)
{
    int ret = button_init();

    if (ret < 0)
    {
        printk("Error al inicializar el boton: %d\n", ret);
        return ret;
    }

    printk("Button B2 initialized. RGB outputs: D2/PB12, D3/PB3, D4/PB5\n");
    return 0;
}
