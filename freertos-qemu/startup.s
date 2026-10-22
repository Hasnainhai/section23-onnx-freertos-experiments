.syntax unified
.cpu cortex-m3
.thumb

/* ARM Cortex-M3 interrupt vector table */
.section .isr_vector, "a", %progbits
.global g_pfnVectors

g_pfnVectors:
    .word __StackTop
    .word Reset_Handler
    .word Default_Handler
    .word Default_Handler
    .word Default_Handler
    .word Default_Handler
    .word Default_Handler
    .word 0
    .word 0
    .word 0
    .word 0
    .word vPortSVCHandler
    .word Default_Handler
    .word 0
    .word xPortPendSVHandler
    .word xPortSysTickHandler

/* Initialize memory and start the application */
.section .text.Reset_Handler, "ax", %progbits
.global Reset_Handler
.thumb_func

Reset_Handler:
    /* Copy initialized data from Flash to RAM */
    ldr r0, =_sidata
    ldr r1, =_sdata
    ldr r2, =_edata

copy_data:
    cmp r1, r2
    bcs clear_bss
    ldr r3, [r0], #4
    str r3, [r1], #4
    b copy_data

clear_bss:
    /* Clear uninitialized RAM variables */
    ldr r1, =_sbss
    ldr r2, =_ebss
    movs r3, #0

zero_loop:
    cmp r1, r2
    bcs start_main
    str r3, [r1], #4
    b zero_loop

start_main:
    bl main

stop:
    b stop

/* Default handler for unexpected interrupts */
.section .text.Default_Handler, "ax", %progbits
.global Default_Handler
.thumb_func

Default_Handler:
    b Default_Handler
