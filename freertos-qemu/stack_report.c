#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"

/* Debug output for the QEMU experiment only */
static void debug_print(const char *message)
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

/* Print a number without printf or dynamic allocation */
static void print_number(uint32_t value)
{
    char digits[11];
    unsigned int index = 10U;

    digits[10] = '\0';

    do
    {
        digits[--index] = (char)('0' + (value % 10U));
        value /= 10U;
    }
    while (value != 0U);

    debug_print(&digits[index]);
}

/* Report minimum observed free stack, in 32-bit words */
void stack_report(
    TaskHandle_t task1,
    TaskHandle_t task2,
    TaskHandle_t uart_task)
{
    debug_print("STACK_FREE_WORDS Task1=");
    print_number(uxTaskGetStackHighWaterMark(task1));

    debug_print(" Task2=");
    print_number(uxTaskGetStackHighWaterMark(task2));

    debug_print(" UART=");
    print_number(uxTaskGetStackHighWaterMark(uart_task));

    debug_print("\n");
}
