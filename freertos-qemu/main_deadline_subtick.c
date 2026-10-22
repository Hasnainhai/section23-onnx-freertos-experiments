#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "uart0.h"

void uart_receiver_task(void *parameter);
void stack_report(TaskHandle_t task1, TaskHandle_t task2, TaskHandle_t uart_task);

/* Fixed-size task stacks: 256 words = 1024 bytes each */
#define TASK_STACK_WORDS 256
#define TASK2_STACK_WORDS 192

static StaticTask_t task1_tcb;
static StaticTask_t task2_tcb;
static StaticTask_t uart_tcb;

static StackType_t task1_stack[TASK_STACK_WORDS];
static StackType_t task2_stack[TASK2_STACK_WORDS];
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


/* Diagnostic samples are stored statically, not allocated dynamically. */
#define TASK1_SAMPLE_COUNT 12U
#define TASK2_SAMPLE_COUNT 6U

typedef struct
{
    TickType_t expected;
    TickType_t started;
    TickType_t finished;
    uint32_t start_counter;
    uint32_t finish_counter;
} TimingSample;

static TimingSample task1_samples[TASK1_SAMPLE_COUNT];
static TimingSample task2_samples[TASK2_SAMPLE_COUNT];

/* Decimal conversion without printf or dynamic allocation. */
static unsigned int append_number(
    char *buffer,
    unsigned int position,
    uint32_t value)
{
    char reversed[10];
    unsigned int count = 0U;

    do
    {
        reversed[count++] = (char)('0' + (value % 10U));
        value /= 10U;
    }
    while (value != 0U);

    while (count != 0U)
    {
        buffer[position++] = reversed[--count];
    }

    return position;
}

/* Emit one complete line to reduce interleaved diagnostic output. */
static void print_timing_sample(
    const char *task,
    const TimingSample *sample)
{
    char line[160];
    unsigned int position = 0U;
    const char *part;

    part = task;
    while (*part != '\0')
    {
        line[position++] = *part++;
    }

    part = " expected=";
    while (*part != '\0')
    {
        line[position++] = *part++;
    }

    position = append_number(
        line, position, (uint32_t)sample->expected);

    part = " start=";
    while (*part != '\0')
    {
        line[position++] = *part++;
    }

    position = append_number(
        line, position, (uint32_t)sample->started);

    part = " finish=";
    while (*part != '\0')
    {
        line[position++] = *part++;
    }

    position = append_number(
        line, position, (uint32_t)sample->finished);

    part = " systick_start=";
    while (*part != '\0')
    {
        line[position++] = *part++;
    }
    position = append_number(
        line, position, sample->start_counter);

    part = " systick_finish=";
    while (*part != '\0')
    {
        line[position++] = *part++;
    }
    position = append_number(
        line, position, sample->finish_counter);

    line[position++] = '\n';
    line[position] = '\0';

    print_message(line);
}

/* First periodic task: record 12 execution observations. */
static void task1(void *parameter)
{
    (void)parameter;

    unsigned int iterations = 0U;
    TickType_t last_wake = xTaskGetTickCount();

    for (;;)
    {
        TickType_t expected = last_wake;
        TickType_t started = xTaskGetTickCount();
        uint32_t start_counter =
            *(volatile uint32_t *)0xE000E018UL;

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

        uint32_t finish_counter =
            *(volatile uint32_t *)0xE000E018UL;
        TickType_t finished = xTaskGetTickCount();

        if (iterations <= TASK1_SAMPLE_COUNT)
        {
            unsigned int index = iterations - 1U;

            task1_samples[index].expected = expected;
            task1_samples[index].started = started;
            task1_samples[index].finished = finished;
            task1_samples[index].start_counter = start_counter;
            task1_samples[index].finish_counter = finish_counter;
        }

        /*
         * Dump after both tasks have finished collecting samples.
         * Printing starts after the observation window.
         */
        if (iterations == 14U)
        {
            unsigned int i;

            print_message("=== DEADLINE_DIAGNOSTIC_BEGIN ===\n");

            for (i = 0U; i < TASK1_SAMPLE_COUNT; i++)
            {
                print_timing_sample("T1", &task1_samples[i]);
            }

            for (i = 0U; i < TASK2_SAMPLE_COUNT; i++)
            {
                print_timing_sample("T2", &task2_samples[i]);
            }

            print_message("=== DEADLINE_DIAGNOSTIC_END ===\n");
        }

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(100));
    }
}

/* Second periodic task: record 6 execution observations. */
static void task2(void *parameter)
{
    (void)parameter;

    unsigned int iterations = 0U;
    TickType_t last_wake = xTaskGetTickCount();

    for (;;)
    {
        TickType_t expected = last_wake;
        TickType_t started = xTaskGetTickCount();
        uint32_t start_counter =
            *(volatile uint32_t *)0xE000E018UL;

        print_message("FreeRTOS Task 2 running\n");

        uint32_t finish_counter =
            *(volatile uint32_t *)0xE000E018UL;
        TickType_t finished = xTaskGetTickCount();

        if (iterations < TASK2_SAMPLE_COUNT)
        {
            task2_samples[iterations].expected = expected;
            task2_samples[iterations].started = started;
            task2_samples[iterations].finished = finished;
            task2_samples[iterations].start_counter = start_counter;
            task2_samples[iterations].finish_counter = finish_counter;
        }

        iterations++;

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(250));
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
        TASK2_STACK_WORDS,
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
