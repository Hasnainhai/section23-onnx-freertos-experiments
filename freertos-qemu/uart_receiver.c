#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "uart0.h"

/* Debug output through QEMU semihosting */
static void uart_debug_print(const char *message)
{
    __asm volatile (
        "movs r0, #4 \n"
        "mov r1, %0 \n"
        "bkpt #0xAB \n"
        :
        : "r" (message)
        : "r0", "r1", "memory"
    );
}

/* FreeRTOS task: receive a simple UART test message */
void uart_receiver_task(void *parameter)
{
    static const char expected[] = "PING\n";
    unsigned int matched = 0;

    (void)parameter;

    for (;;)
    {
        int received = uart0_try_read();

        if (received >= 0)
        {
            if ((char)received == expected[matched])
            {
                matched++;

                if (matched == sizeof(expected) - 1U)
                {
                    uart_debug_print(
                        "FreeRTOS UART received PING\n"
                    );
                    matched = 0;
                }
            }
            else
            {
                matched = ((char)received == 'P') ? 1U : 0U;
            }
        }
        else
        {
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }
}
