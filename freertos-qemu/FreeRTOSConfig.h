#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* Processor and scheduler */
#define configCPU_CLOCK_HZ                 ( 50000000UL )
#define configTICK_RATE_HZ                 ( 1000U )
#define configMAX_PRIORITIES               ( 5U )
#define configMINIMAL_STACK_SIZE           ( 128U )
#define configMAX_TASK_NAME_LEN            ( 16U )

/* Task scheduling */
#define configUSE_PREEMPTION               1
#define configUSE_TIME_SLICING             1
#define configUSE_16_BIT_TICKS             0
#define configUSE_IDLE_HOOK                0
#define configUSE_TICK_HOOK                0

/* Memory allocation */
#define configSUPPORT_STATIC_ALLOCATION    1
#define configSUPPORT_DYNAMIC_ALLOCATION   0
#define configTOTAL_HEAP_SIZE              ( 0 )

/* Features */
#define configUSE_MUTEXES                  0
#define configUSE_RECURSIVE_MUTEXES        0
#define configUSE_COUNTING_SEMAPHORES      0
#define configUSE_TIMERS                   0

/* Cortex-M3 interrupt priorities */
#define configPRIO_BITS                    3
#define configKERNEL_INTERRUPT_PRIORITY    255
#define configMAX_SYSCALL_INTERRUPT_PRIORITY  160

/* Check interrupt-handler installation */
#define configCHECK_HANDLER_INSTALLATION   1

/* Enable APIs used by our tasks */
#define INCLUDE_vTaskDelay                 1
#define INCLUDE_vTaskDelayUntil            1
#define INCLUDE_vTaskDelete                1

/* Assert handler */
void vApplicationAssertFailed(const char *file, int line);
#define configASSERT(x) \
    if ((x) == 0) vApplicationAssertFailed(__FILE__, __LINE__)

#endif
