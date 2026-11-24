.syntax unified
.cpu cortex-m3
.thumb

.section .isr_vector, "a", %progbits
.global g_pfnVectors

g_pfnVectors:
    .word __StackTop
    .word Reset_Handler
    .word Default_Handler        /* NMI */
    .word HardFault_Handler      /* HardFault */
    .word MemManage_Handler      /* MemManage (MPU Fault) */
    .word Default_Handler        /* BusFault */
    .word Default_Handler        /* UsageFault */
    .word 0
    .word 0
    .word 0
    .word 0
    .word vPortSVCHandler
    .word Default_Handler
    .word 0
    .word xPortPendSVHandler
    .word xPortSysTickHandler

.section .text.Reset_Handler, "ax", %progbits
.global Reset_Handler
.thumb_func
Reset_Handler:
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

.section .text.Default_Handler, "ax", %progbits
.global Default_Handler
.thumb_func
Default_Handler:
    b Default_Handler
