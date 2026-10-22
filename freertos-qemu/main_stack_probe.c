#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "uart0.h"

void uart_receiver_task(void *parameter);
void stack_report(TaskHandle_t task1, TaskHandle_t task2, TaskHandle_t uart_task);

/* Fixed-size task stacks: 256 words = 1024 bytes each */
#define TASK_STACK_WORDS 256

static StaticTask_t task1_tcb;
static StaticTask_t task2_tcb;
static StaticTask_t uart_tcb;

static StackType_t task1_stack[TASK_STACK_WORDS];
static StackType_t task2_stack[TASK_STACK_WORDS];
static StackType_t uart_stack[TASK_STACK_WORDS];

/* FreeRTOS also requires memory for its idle task */
static StaticTask_t idle_tcb;
static StackType_t idle_stack[configMINIMAL_STACK_SIZE];

static TaskHandle_t measured_task1;
static TaskHandle_t measured_task2;
static TaskHandle_t measured_uart;

/* Print a message through QEMU semihosting */
static void print_message(const char *message)
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

/* First periodic task */
static void task1(void *parameter)
{
    (void)parameter;
    unsigned int iterations = 0U;

    for (;;)
    {
        print_message("FreeRTOS Task 1 running\n");

        iterations++;
        if ((iterations % 10U) == 0U)
        {
            stack_report(
                measured_task1,
                measured_task2,
                measured_uart
            );
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/* Second periodic task */
static void task2(void *parameter)
{
    (void)parameter;

    for (;;)
    {
        print_message("FreeRTOS Task 2 running\n");
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}

/* Provide static memory for the FreeRTOS idle task */
void vApplicationGetIdleTaskMemory(
    StaticTask_t **ppxIdleTaskTCBBuffer,
    StackType_t **ppxIdleTaskStackBuffer,
    configSTACK_DEPTH_TYPE *pulIdleTaskStackSize)
{
    *ppxIdleTaskTCBBuffer = &idle_tcb;
    *ppxIdleTaskStackBuffer = idle_stack;
    *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

/* Called if a FreeRTOS assertion fails */
void vApplicationAssertFailed(const char *file, int line)
{
    (void)file;
    (void)line;

    print_message("ERROR: FreeRTOS assertion failed\n");

    for (;;)
    {
    }
}

int main(void)
{
    print_message("Starting FreeRTOS on QEMU Cortex-M3\n");

    uart0_init();

    TaskHandle_t handle1 = xTaskCreateStatic(
        task1,
        "Task1",
        TASK_STACK_WORDS,
        NULL,
        2,
        task1_stack,
        &task1_tcb
    );

    TaskHandle_t handle2 = xTaskCreateStatic(
        task2,
        "Task2",
        TASK_STACK_WORDS,
        NULL,
        2,
        task2_stack,
        &task2_tcb
    );

    TaskHandle_t uart_handle = xTaskCreateStatic(
        uart_receiver_task,
        "UART_RX",
        TASK_STACK_WORDS,
        NULL,
        2,
        uart_stack,
        &uart_tcb
    );

    configASSERT(uart_handle != NULL);
    configASSERT(handle1 != NULL);
    configASSERT(handle2 != NULL);

    measured_task1 = handle1;
    measured_task2 = handle2;
    measured_uart = uart_handle;

    print_message("Starting FreeRTOS scheduler\n");

    vTaskStartScheduler();

    print_message("ERROR: Scheduler returned\n");

    for (;;)
    {
    }
}
