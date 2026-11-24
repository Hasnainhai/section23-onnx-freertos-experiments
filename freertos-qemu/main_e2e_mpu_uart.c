#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "uart0.h"
#include "packet_protocol.h"
#include "packet_decoder.h"
#include "../linux-inference/depthwise_channel_fixture.h"

#define SCB_SHCSR       (*(volatile uint32_t *)0xE000ED24UL)
#define MPU_CTRL        (*(volatile uint32_t *)0xE000ED94UL)
#define MPU_RNR         (*(volatile uint32_t *)0xE000ED98UL)
#define MPU_RBAR        (*(volatile uint32_t *)0xE000ED9CUL)
#define MPU_RASR        (*(volatile uint32_t *)0xE000EDA0UL)

#define MPU_RASR_ENABLE         (1UL << 0)
#define MPU_RASR_SIZE_256K      (17UL << 1)
#define MPU_RASR_SIZE_32K       (14UL << 1)
#define MPU_RASR_SIZE_4K        (11UL << 1)
#define MPU_RASR_SIZE_1K        (9UL  << 1)
#define MPU_AP_PRIV_RW_USER_NO  (1UL << 24)
#define MPU_AP_PRIV_RW_USER_RO  (2UL << 24)
#define MPU_AP_FULL_ACCESS      (3UL << 24)
#define MPU_AP_ALL_RO           (6UL << 24)
#define MPU_XN                  (1UL << 28)

#define INFERENCE_STACK_WORDS   256U
#define RX_STACK_WORDS          192U

typedef struct {
    StackType_t stack[INFERENCE_STACK_WORDS];
    int8_t      q_input[196];
    int8_t      q_weights[9];
    int32_t     q_output[49];
} inference_arena_t;

__attribute__((section(".inference_arena"), aligned(4096)))
static inference_arena_t g_inf_arena;

typedef struct {
    StackType_t stack[RX_STACK_WORDS];
    uint8_t     rx_buf[PACKET_SIZE];
} rx_partition_t;

__attribute__((section(".aux_task_ram"), aligned(4096)))
static rx_partition_t g_rx_ram;

typedef struct {
    volatile uint32_t pending_seq;
    volatile uint16_t class_index;
    volatile uint32_t timestamp_ms;
    volatile uint8_t  valid;
    volatile uint32_t processed_seq;
} ipc_mailbox_t;

__attribute__((section(".shared_ipc_ram"), aligned(1024)))
static ipc_mailbox_t g_ipc;

static StaticTask_t inf_tcb, rx_tcb, idle_tcb;
static StackType_t  idle_stack[configMINIMAL_STACK_SIZE];
static TaskHandle_t h_inf = NULL, h_rx = NULL;

static void semihost_print(const char *msg)
{
    __asm volatile (
        "movs r0, #4 \n"
        "mov  r1, %0 \n"
        "bkpt #0xAB \n"
        : : "r" (msg) : "r0", "r1", "memory"
    );
}

static void print_u32(uint32_t val)
{
    char buf[12];
    int i = 11;
    buf[11] = '\0';
    do {
        buf[--i] = (char)('0' + (val % 10U));
        val /= 10U;
    } while (val != 0U);
    semihost_print(&buf[i]);
}

static void mpu_configure_base(void)
{
    SCB_SHCSR |= (1UL << 16);
    MPU_CTRL = 0;

    MPU_RNR  = 0;
    MPU_RBAR = 0x00000000UL;
    MPU_RASR = MPU_RASR_ENABLE | MPU_RASR_SIZE_256K | MPU_AP_ALL_RO;

    MPU_RNR  = 1;
    MPU_RBAR = 0x20000000UL;
    MPU_RASR = MPU_RASR_ENABLE | MPU_RASR_SIZE_32K | MPU_AP_PRIV_RW_USER_NO | MPU_XN;

    MPU_RNR  = 2;
    MPU_RBAR = 0x2000A000UL;
    MPU_RASR = MPU_RASR_ENABLE | MPU_RASR_SIZE_1K | MPU_AP_FULL_ACCESS | MPU_XN;

    MPU_RNR  = 3;
    MPU_RBAR = 0x20008000UL;
    MPU_RASR = MPU_RASR_ENABLE | MPU_RASR_SIZE_4K | MPU_AP_FULL_ACCESS | MPU_XN;

    MPU_CTRL = (1UL << 0) | (1UL << 2);
    __asm volatile ("dsb \n isb \n" ::: "memory");
}

void mpu_on_task_switched_in(void *current_tcb)
{
    if (current_tcb == (void *)h_inf) {
        MPU_RNR  = 3;
        MPU_RBAR = 0x20008000UL;
        MPU_RASR = MPU_RASR_ENABLE | MPU_RASR_SIZE_4K | MPU_AP_FULL_ACCESS | MPU_XN;
    } else if (current_tcb == (void *)h_rx) {
        MPU_RNR  = 3;
        MPU_RBAR = 0x20009000UL;
        MPU_RASR = MPU_RASR_ENABLE | MPU_RASR_SIZE_4K | MPU_AP_FULL_ACCESS | MPU_XN;
    }
    __asm volatile ("dsb \n isb \n" ::: "memory");
}

__attribute__((noinline))
static void depthwise_channel_int8(
    const int8_t input[196],
    const int8_t weights[9],
    int32_t output[49])
{
    for (int oy = 0; oy < 7; ++oy) {
        for (int ox = 0; ox < 7; ++ox) {
            int32_t acc = 0;
            for (int ky = 0; ky < 3; ++ky) {
                for (int kx = 0; kx < 3; ++kx) {
                    int iy = oy * 2 + ky - 1;
                    int ix = ox * 2 + kx - 1;
                    if (iy >= 0 && iy < 14 && ix >= 0 && ix < 14) {
                        acc += (int32_t)input[iy * 14 + ix] *
                               (int32_t)weights[ky * 3 + kx];
                    }
                }
            }
            output[oy * 7 + ox] = acc;
        }
    }
}

static void uart_rx_task(void *param)
{
    (void)param;
    uint32_t idx = 0;
    PacketResult pkt;

    for (;;) {
        int ch;
        while ((ch = uart0_try_read()) >= 0) {
            uint8_t byte = (uint8_t)ch;
            if (idx == 0U && byte != PACKET_MAGIC0) {
                continue;
            }
            if (idx == 1U && byte != PACKET_MAGIC1) {
                idx = (byte == PACKET_MAGIC0) ? 1U : 0U;
                continue;
            }
            g_rx_ram.rx_buf[idx++] = byte;
            if (idx == PACKET_SIZE) {
                idx = 0U;
                if (packet_decode(g_rx_ram.rx_buf, &pkt) == 1) {
                    g_ipc.class_index  = pkt.class_index;
                    g_ipc.timestamp_ms = pkt.timestamp_ms;
                    g_ipc.valid        = pkt.valid;
                    g_ipc.pending_seq  = pkt.sequence;
                    xTaskNotifyGive(h_inf);
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

static void mpu_inference_task(void *param)
{
    (void)param;

    /* Pre-quantize static model weights/inputs into INFERENCE_ARENA (0x20008000) */
    for (int i = 0; i < 196; ++i) {
        g_inf_arena.q_input[i] = (int8_t)(test_input[i] * 64.0f);
    }
    for (int i = 0; i < 9; ++i) {
        g_inf_arena.q_weights[i] = (int8_t)(test_weights[i] * 64.0f);
    }

    semihost_print("E2E_MPU_RTOS_READY\n");

    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        uint32_t seq = g_ipc.pending_seq;
        if (seq != g_ipc.processed_seq) {
            TickType_t t0 = xTaskGetTickCount();
            depthwise_channel_int8(
                g_inf_arena.q_input,
                g_inf_arena.q_weights,
                g_inf_arena.q_output
            );
            TickType_t t1 = xTaskGetTickCount();
            g_ipc.processed_seq = seq;

            semihost_print("E2E_ACK seq=");
            print_u32(seq);
            semihost_print(" class=");
            print_u32(g_ipc.class_index);
            semihost_print(" rtos_ticks=");
            print_u32((uint32_t)(t1 - t0));
            semihost_print(" int8_out0=");
            print_u32((uint32_t)(g_inf_arena.q_output[0] & 0xFFFF));
            semihost_print("\n");
        }
    }
}

void MemManage_Handler(void) { for (;;) {} }
void HardFault_Handler(void) { for (;;) {} }

void vApplicationGetIdleTaskMemory(
    StaticTask_t **tcb, StackType_t **stack, configSTACK_DEPTH_TYPE *size)
{
    *tcb = &idle_tcb; *stack = idle_stack; *size = configMINIMAL_STACK_SIZE;
}

void vApplicationAssertFailed(const char *file, int line)
{
    (void)file; (void)line; for (;;) {}
}

int main(void)
{
    uart0_init();
    mpu_configure_base();

    h_inf = xTaskCreateStatic(
        mpu_inference_task, "MPU_INF", INFERENCE_STACK_WORDS,
        NULL, 3, g_inf_arena.stack, &inf_tcb);

    h_rx = xTaskCreateStatic(
        uart_rx_task, "MPU_RX", RX_STACK_WORDS,
        NULL, 2, g_rx_ram.stack, &rx_tcb);

    configASSERT(h_inf != NULL);
    configASSERT(h_rx != NULL);

    vTaskStartScheduler();
    for (;;) {}
}
