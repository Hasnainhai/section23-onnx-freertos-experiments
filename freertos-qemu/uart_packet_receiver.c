#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "uart0.h"
#include "packet_decoder.h"

/* Debug output through QEMU semihosting */
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

/* Print an unsigned integer without printf or malloc */
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

/* Same task entry point as the original PING receiver */
void uart_receiver_task(void *parameter)
{
    uint8_t packet[PACKET_SIZE];
    unsigned int count = 0U;
    PacketResult result;

    (void)parameter;

    debug_print("FreeRTOS binary packet receiver ready\n");

    for (;;)
    {
        int received = uart0_try_read();

        if (received < 0)
        {
            vTaskDelay(pdMS_TO_TICKS(1));
            continue;
        }

        uint8_t byte = (uint8_t)received;

        /* Find the first start-marker byte */
        if (count == 0U)
        {
            if (byte == PACKET_MAGIC0)
            {
                packet[count++] = byte;
            }
            continue;
        }

        /* Find the second start-marker byte */
        if (count == 1U)
        {
            if (byte == PACKET_MAGIC1)
            {
                packet[count++] = byte;
            }
            else if (byte != PACKET_MAGIC0)
            {
                count = 0U;
            }
            continue;
        }

        packet[count++] = byte;

        if (count == PACKET_SIZE)
        {
            if (packet_decode(packet, &result) == 1)
            {
                debug_print("FreeRTOS packet seq=");
                print_number(result.sequence);

                debug_print(" class=");
                print_number(result.class_index);

                debug_print(" timestamp_ms=");
                print_number(result.timestamp_ms);

                debug_print(" valid=");
                print_number(result.valid);

                debug_print("\n");
            }
            else
            {
                debug_print("FreeRTOS packet rejected\n");
            }

            count = 0U;
        }
    }
}
