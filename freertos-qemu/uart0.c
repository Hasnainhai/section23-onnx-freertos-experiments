#include <stdint.h>
#include "uart0.h"

/* LM3S6965 UART0 register addresses */
#define UART0_BASE  0x4000C000UL

#define UART0_DR    (*(volatile uint32_t *)(UART0_BASE + 0x000UL))
#define UART0_FR    (*(volatile uint32_t *)(UART0_BASE + 0x018UL))
#define UART0_CTL   (*(volatile uint32_t *)(UART0_BASE + 0x030UL))

#define UART_FR_RXFE  (1UL << 4)
#define UART_CTL_UARTEN (1UL << 0)
#define UART_CTL_TXE    (1UL << 8)
#define UART_CTL_RXE    (1UL << 9)

void uart0_init(void)
{
    UART0_CTL |= UART_CTL_UARTEN | UART_CTL_TXE | UART_CTL_RXE;
}

int uart0_try_read(void)
{
    if ((UART0_FR & UART_FR_RXFE) != 0UL)
    {
        return -1;
    }

    return (int)(UART0_DR & 0xFFUL);
}
