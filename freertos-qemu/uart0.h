#ifndef UART0_H
#define UART0_H

void uart0_init(void);

/* Returns 0-255 if a byte is available, or -1 if empty. */
int uart0_try_read(void);

#endif
