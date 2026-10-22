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

static void inference_task(void *parameter)
{
    (void)parameter;

    print_message(
        "FreeRTOS ARM inference task started\n"
    );

    depthwise_channel(
        test_input,
        test_weights,
        inference_output
    );

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
