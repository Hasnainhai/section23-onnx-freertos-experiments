ENTRY(Reset_Handler)

MEMORY
{
    FLASH           (rx)  : ORIGIN = 0x00000000, LENGTH = 256K
    KERNEL_RAM      (rwx) : ORIGIN = 0x20000000, LENGTH = 32K
    INFERENCE_ARENA (rw)  : ORIGIN = 0x20008000, LENGTH = 4K
    AUX_TASK_RAM    (rw)  : ORIGIN = 0x20009000, LENGTH = 4K
    SHARED_IPC_RAM  (rw)  : ORIGIN = 0x2000A000, LENGTH = 1K
}

__StackTop = ORIGIN(KERNEL_RAM) + LENGTH(KERNEL_RAM);
__StackLimit = __StackTop - 4K;

SECTIONS
{
    .isr_vector : { KEEP(*(.isr_vector)) } > FLASH

    .text :
    {
        *(.text*)
        *(.rodata*)
        KEEP(*(.init))
        KEEP(*(.fini))
        . = ALIGN(4);
    } > FLASH

    .ARM.exidx : { *(.ARM.exidx*) } > FLASH

    .data :
    {
        . = ALIGN(4);
        _sdata = .;
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } > KERNEL_RAM AT > FLASH

    _sidata = LOADADDR(.data);

    .bss (NOLOAD) :
    {
        . = ALIGN(4);
        _sbss = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
    } > KERNEL_RAM

    .inference_arena (NOLOAD) :
    {
        . = ALIGN(4096);
        *(.inference_arena*)
        . = ALIGN(4096);
    } > INFERENCE_ARENA

    .aux_task_ram (NOLOAD) :
    {
        . = ALIGN(4096);
        *(.aux_task_ram*)
        . = ALIGN(4096);
    } > AUX_TASK_RAM

    .shared_ipc_ram (NOLOAD) :
    {
        . = ALIGN(1024);
        *(.shared_ipc_ram*)
        . = ALIGN(1024);
    } > SHARED_IPC_RAM

    . = ALIGN(8);
    _end = .;

    ASSERT(_ebss <= __StackLimit, "KERNEL_RAM overlaps reserved main stack")
}
