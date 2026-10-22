#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "../linux-inference/depthwise_channel_fixture.h"

#define INFERENCE_STACK_WORDS 512

extern void depthwise_channel(
    const float input[196],
    const float weights[9],
    float output[49]
);

static StaticTask_t inference_tcb;
static StackType_t inference_stack[INFERENCE_STACK_WORDS];

static StaticTask_t idle_tcb;
static StackType_t idle_stack[configMINIMAL_STACK_SIZE];

static float inference_output[49];

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


#define INFERENCE_TIMING_SAMPLES 16U

static uint32_t timing_start_tick[INFERENCE_TIMING_SAMPLES];
static uint32_t timing_end_tick[INFERENCE_TIMING_SAMPLES];
static uint32_t timing_start_systick[INFERENCE_TIMING_SAMPLES];
static uint32_t timing_end_systick[INFERENCE_TIMING_SAMPLES];

static unsigned int append_text(
    char *buffer, unsigned int pos, const char *text)
{
    while (*text != '\0')
    {
        buffer[pos++] = *text++;
    }
    return pos;
}

static unsigned int append_uint(
    char *buffer, unsigned int pos, uint32_t value)
{
    char digits[10];
    unsigned int count = 0U;

    do
    {
        digits[count++] = (char)('0' + value % 10U);
        value /= 10U;
    }
    while (value != 0U);

    while (count != 0U)
    {
        buffer[pos++] = digits[--count];
    }

    return pos;
}

static void print_timing_sample(unsigned int i)
{
    char line[192];
    unsigned int pos = 0U;

    pos = append_text(line, pos, "ARM_TIMING sample=");
    pos = append_uint(line, pos, i + 1U);

    pos = append_text(line, pos, " start_tick=");
    pos = append_uint(line, pos, timing_start_tick[i]);

    pos = append_text(line, pos, " end_tick=");
    pos = append_uint(line, pos, timing_end_tick[i]);

    pos = append_text(line, pos, " start_systick=");
    pos = append_uint(line, pos, timing_start_systick[i]);

    pos = append_text(line, pos, " end_systick=");
    pos = append_uint(line, pos, timing_end_systick[i]);

    line[pos++] = '\n';
    line[pos] = '\0';

    print_message(line);
}

static void inference_task(void *parameter)
{
    (void)parameter;

    print_message(
        "FreeRTOS ARM inference task started\n"
    );


    for (unsigned int i = 0U;
         i < INFERENCE_TIMING_SAMPLES;
         ++i)
    {
        timing_start_tick[i] = xTaskGetTickCount();
        timing_start_systick[i] =
            *(volatile uint32_t *)0xE000E018UL;

        depthwise_channel(
            test_input,
            test_weights,
            inference_output
        );

        timing_end_systick[i] =
            *(volatile uint32_t *)0xE000E018UL;
        timing_end_tick[i] = xTaskGetTickCount();
    }

    print_message("ARM INFERENCE TIMING SAMPLES BEGIN\n");

    for (unsigned int i = 0U;
         i < INFERENCE_TIMING_SAMPLES;
         ++i)
    {
        print_timing_sample(i);
    }

    print_message("ARM INFERENCE TIMING SAMPLES END\n");

    /* Read the actual SysTick registers after all timing samples. */
    {
        uint32_t reload =
            *(volatile uint32_t *)0xE000E014UL;

        uint32_t control =
            *(volatile uint32_t *)0xE000E010UL;

        char line[128];
        unsigned int pos = 0U;

        pos = append_text(line, pos, "SYSTICK_REGISTER reload=");
        pos = append_uint(line, pos, reload);

        pos = append_text(line, pos, " control=");
        pos = append_uint(line, pos, control);

        line[pos++] = '\n';
        line[pos] = '\0';

        print_message(line);
    }



    unsigned int failures = 0U;

    for (unsigned int i = 0; i < 49U; ++i)
    {
        float difference =
            inference_output[i] - expected_output[i];

        if (difference < 0.0f)
        {
            difference = -difference;
        }

        if (!(difference <= 0.00001f))
        {
            failures++;
        }
    }

    if (failures == 0U)
    {
        print_message(
            "ARM FREERTOS INFERENCE VALIDATION: PASS\n"
        );
        print_message(
            "All 49 convolution outputs verified\n"
        );
    }
    else
    {
        print_message(
            "ARM FREERTOS INFERENCE VALIDATION: FAIL\n"
        );
    }

    for (;;)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void vApplicationGetIdleTaskMemory(
    StaticTask_t **tcb,
    StackType_t **stack,
    configSTACK_DEPTH_TYPE *size
)
{
    *tcb = &idle_tcb;
    *stack = idle_stack;
    *size = configMINIMAL_STACK_SIZE;
}

void vApplicationAssertFailed(
    const char *file,
    int line
)
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
    print_message(
        "Starting isolated ARM inference experiment\n"
    );

    TaskHandle_t task = xTaskCreateStatic(
        inference_task,
        "INFERENCE",
        INFERENCE_STACK_WORDS,
        NULL,
        2,
        inference_stack,
        &inference_tcb
    );

    configASSERT(task != NULL);

    print_message("Starting FreeRTOS scheduler\n");

    vTaskStartScheduler();

    print_message("ERROR: Scheduler returned\n");

    for (;;)
    {
    }
}
