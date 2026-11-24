#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "../linux-inference/depthwise_channel_fixture.h"

/* ARMv7-M System Control Block & MPU Hardware Registers */
#define SCB_SHCSR       (*(volatile uint32_t *)0xE000ED24UL)
#define SCB_CFSR        (*(volatile uint32_t *)0xE000ED28UL)
#define SCB_MMFAR       (*(volatile uint32_t *)0xE000ED34UL)

#define MPU_TYPE        (*(volatile uint32_t *)0xE000ED90UL)
#define MPU_CTRL        (*(volatile uint32_t *)0xE000ED94UL)
#define MPU_RNR         (*(volatile uint32_t *)0xE000ED98UL)
#define MPU_RBAR        (*(volatile uint32_t *)0xE000ED9CUL)
#define MPU_RASR        (*(volatile uint32_t *)0xE000EDA0UL)

/* MPU RASR bit encodings */
#define MPU_RASR_ENABLE         (1UL << 0)
#define MPU_RASR_SIZE_256K      (17UL << 1) /* 2^(17+1) = 256 KB */
#define MPU_RASR_SIZE_32K       (14UL << 1) /* 2^(14+1) = 32 KB  */
#define MPU_RASR_SIZE_4K        (11UL << 1) /* 2^(11+1) = 4 KB   */
#define MPU_RASR_SIZE_1K        (9UL  << 1) /* 2^(9+1)  = 1 KB   */

#define MPU_AP_PRIV_RW_USER_NO  (1UL << 24) /* AP = 001 */
#define MPU_AP_PRIV_RW_USER_RO  (2UL << 24) /* AP = 010 */
#define MPU_AP_FULL_ACCESS      (3UL << 24) /* AP = 011 */
#define MPU_AP_ALL_RO           (6UL << 24) /* AP = 110 */
#define MPU_XN                  (1UL << 28) /* Execute Never */

#define INFERENCE_STACK_WORDS   256U
#define AUX_STACK_WORDS         192U

/* Partition 1: INFERENCE_ARENA (0x20008000, 4 KB) */
typedef struct {
    StackType_t stack[INFERENCE_STACK_WORDS];
    int8_t      q_input[196];
    int8_t      q_weights[9];
    int32_t     q_output[49];
    float       dequant_output[49];
} inference_arena_t;

__attribute__((section(".inference_arena"), aligned(4096)))
static inference_arena_t g_inf_arena;

/* Partition 2: AUX_TASK_RAM (0x20009000, 4 KB) */
typedef struct {
    StackType_t stack[AUX_STACK_WORDS];
    uint32_t    scratch[64];
} aux_partition_t;

__attribute__((section(".aux_task_ram"), aligned(4096)))
static aux_partition_t g_aux_ram;

/* Partition 3: SHARED_IPC_RAM (0x2000A000, 1 KB) */
typedef struct {
    uint32_t sequence_id;
    uint32_t inference_done;
    uint32_t verified_outputs;
    uint32_t max_abs_error_micro;
} ipc_mailbox_t;

__attribute__((section(".shared_ipc_ram"), aligned(1024)))
static volatile ipc_mailbox_t g_ipc_mailbox;

/* Kernel-space TCBs (0x20000000, privileged only) */
static StaticTask_t inference_tcb;
static StaticTask_t aux_tcb;
static StaticTask_t idle_tcb;
static StackType_t  idle_stack[configMINIMAL_STACK_SIZE];

static TaskHandle_t h_inference_task = NULL;
static TaskHandle_t h_aux_task       = NULL;

static void semihost_print(const char *msg)
{
    __asm volatile (
        "movs r0, #4 \n"
        "mov  r1, %0 \n"
        "bkpt #0xAB \n"
        : : "r" (msg) : "r0", "r1", "memory"
    );
}

static void semihost_exit(uint32_t code)
{
    uint32_t block[2] = { 0x20026U, code }; /* ADP_Stopped_ApplicationExit */
    __asm volatile (
        "movs r0, #0x18 \n"
        "mov  r1, %0 \n"
        "bkpt #0xAB \n"
        : : "r" (block) : "r0", "r1", "memory"
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

static void print_hex32(uint32_t val)
{
    const char hex[] = "0123456789ABCDEF";
    char buf[11];
    buf[0] = '0';
    buf[1] = 'x';
    for (int i = 0; i < 8; ++i) {
        buf[2 + i] = hex[(val >> ((7 - i) * 4)) & 0xFU];
    }
    buf[10] = '\0';
    semihost_print(buf);
}

static void mpu_configure_base(void)
{
    /* Enable MemManage fault exception (SHCSR bit 16) */
    SCB_SHCSR |= (1UL << 16);

    /* Disable MPU before reconfiguring */
    MPU_CTRL = 0;

    /* Region 0: FLASH (0x00000000, 256 KB) - Read-Only, Executable */
    MPU_RNR  = 0;
    MPU_RBAR = 0x00000000UL;
    MPU_RASR = MPU_RASR_ENABLE | MPU_RASR_SIZE_256K | MPU_AP_ALL_RO;

    /* Region 1: KERNEL_RAM (0x20000000, 32 KB) - Privileged RW, User No-Access, XN */
    MPU_RNR  = 1;
    MPU_RBAR = 0x20000000UL;
    MPU_RASR = MPU_RASR_ENABLE | MPU_RASR_SIZE_32K | MPU_AP_PRIV_RW_USER_NO | MPU_XN;

    /* Region 2: SHARED_IPC_RAM (0x2000A000, 1 KB) - Default Priv RW / User RO */
    MPU_RNR  = 2;
    MPU_RBAR = 0x2000A000UL;
    MPU_RASR = MPU_RASR_ENABLE | MPU_RASR_SIZE_1K | MPU_AP_PRIV_RW_USER_RO | MPU_XN;

    /* Region 3: Active Task Private SRAM (configured dynamically on context switch) */
    MPU_RNR  = 3;
    MPU_RBAR = 0x20008000UL;
    MPU_RASR = MPU_RASR_ENABLE | MPU_RASR_SIZE_4K | MPU_AP_FULL_ACCESS | MPU_XN;

    /* Enable MPU with PRIVDEFENA=1 (privileged background map for peripheral/UART/NVIC) */
    MPU_CTRL = (1UL << 0) | (1UL << 2);
    __asm volatile ("dsb \n isb \n" ::: "memory");
}

void mpu_on_task_switched_in(void *current_tcb)
{
    if (current_tcb == (void *)h_inference_task) {
        /* Unlock INFERENCE_ARENA (0x20008000) + RW access to SHARED_IPC_RAM */
        MPU_RNR  = 2;
        MPU_RBAR = 0x2000A000UL;
        MPU_RASR = MPU_RASR_ENABLE | MPU_RASR_SIZE_1K | MPU_AP_FULL_ACCESS | MPU_XN;

        MPU_RNR  = 3;
        MPU_RBAR = 0x20008000UL;
        MPU_RASR = MPU_RASR_ENABLE | MPU_RASR_SIZE_4K | MPU_AP_FULL_ACCESS | MPU_XN;
    } else if (current_tcb == (void *)h_aux_task) {
        /* Lock INFERENCE_ARENA; expose ONLY AUX_TASK_RAM (0x20009000) + RO SHARED_IPC */
        MPU_RNR  = 2;
        MPU_RBAR = 0x2000A000UL;
        MPU_RASR = MPU_RASR_ENABLE | MPU_RASR_SIZE_1K | MPU_AP_PRIV_RW_USER_RO | MPU_XN;

        MPU_RNR  = 3;
        MPU_RBAR = 0x20009000UL;
        MPU_RASR = MPU_RASR_ENABLE | MPU_RASR_SIZE_4K | MPU_AP_FULL_ACCESS | MPU_XN;
    }
    __asm volatile ("dsb \n isb \n" ::: "memory");
}

/* Pure integer fixed-point depthwise convolution (zero soft-float loops) */
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

static void drop_to_unprivileged(void)
{
    __asm volatile (
        "mrs r0, control \n"
        "orr r0, r0, #1  \n"
        "msr control, r0 \n"
        "isb             \n"
        ::: "r0", "memory"
    );
}

static void inference_task(void *param)
{
    (void)param;

    /* Symmetric INT8 quantization into static INFERENCE_ARENA */
    float max_in = 0.0f;
    for (int i = 0; i < 196; ++i) {
        float v = test_input[i] < 0.0f ? -test_input[i] : test_input[i];
        if (v > max_in) max_in = v;
    }
    float max_w = 0.0f;
    for (int i = 0; i < 9; ++i) {
        float v = test_weights[i] < 0.0f ? -test_weights[i] : test_weights[i];
        if (v > max_w) max_w = v;
    }

    float scale_in = (max_in > 0.0f) ? (max_in / 127.0f) : 1.0f;
    float scale_w  = (max_w  > 0.0f) ? (max_w  / 127.0f) : 1.0f;

    for (int i = 0; i < 196; ++i) {
        float q = test_input[i] / scale_in;
        g_inf_arena.q_input[i] = (int8_t)(q >= 0.0f ? (q + 0.5f) : (q - 0.5f));
    }
    for (int i = 0; i < 9; ++i) {
        float q = test_weights[i] / scale_w;
        g_inf_arena.q_weights[i] = (int8_t)(q >= 0.0f ? (q + 0.5f) : (q - 0.5f));
    }

    /* Execute deterministic INT8 kernel inside isolated partition */
    depthwise_channel_int8(
        g_inf_arena.q_input,
        g_inf_arena.q_weights,
        g_inf_arena.q_output
    );

    float combined_scale = scale_in * scale_w;
    float max_err = 0.0f;
    uint32_t ok_count = 0;

    for (int i = 0; i < 49; ++i) {
        float out_f = (float)g_inf_arena.q_output[i] * combined_scale;
        g_inf_arena.dequant_output[i] = out_f;
        float err = out_f - expected_output[i];
        if (err < 0.0f) err = -err;
        if (err > max_err) max_err = err;
        if (err <= 0.05f) ok_count++;
    }

    g_ipc_mailbox.sequence_id = 1;
    g_ipc_mailbox.verified_outputs = ok_count;
    g_ipc_mailbox.max_abs_error_micro = (uint32_t)(max_err * 1000000.0f);
    g_ipc_mailbox.inference_done = 1;

    semihost_print("[PARTITION_1: INFERENCE_ARENA @ 0x20008000] INT8 Kernel Complete\n");
    semihost_print("  INT8 Tensor Buffer Bytes: 401 bytes (vs 1016 bytes FP32 -> 60.5% saving)\n");
    semihost_print("  Verified Outputs (tol 0.05): ");
    print_u32(ok_count);
    semihost_print("/49, Max Abs Error (u-units): ");
    print_u32(g_ipc_mailbox.max_abs_error_micro);
    semihost_print("\n");

    /* Yield to AUX_TASK to test spatial isolation boundary */
    vTaskDelay(pdMS_TO_TICKS(10));

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void aux_task(void *param)
{
    (void)param;

    while (g_ipc_mailbox.inference_done == 0U) {
        vTaskDelay(pdMS_TO_TICKS(5));
    }

    /* 1. Legal access to own partition AUX_TASK_RAM (0x20009000) */
    g_aux_ram.scratch[0] = 0xCAFEBABEU;

    /* 2. Legal read-only access to SHARED_IPC_RAM (0x2000A000) */
    uint32_t seen = g_ipc_mailbox.verified_outputs;
    semihost_print("[PARTITION_2: AUX_TASK_RAM @ 0x20009000] Read IPC verified_outputs=");
    print_u32(seen);
    semihost_print("\n");
    semihost_print("[FAULT_TEST] Unprivileged AUX_TASK attempting write to 0x20008000...\n");

    /* 3. Drop to unprivileged user mode (CONTROL.nPRIV = 1) and test MPU boundary */
    drop_to_unprivileged();
    volatile uint32_t *forbidden_addr = (volatile uint32_t *)0x20008000UL;
    *forbidden_addr = 0xDEADBEEFUL;

    /* Should never reach here */
    semihost_print("ERROR: Spatial isolation failed!\n");
    semihost_exit(1);
}

void MemManage_Handler(void)
{
    uint32_t cfsr  = SCB_CFSR;
    uint32_t mmfar = SCB_MMFAR;

    semihost_print("[MPU_MEMMANAGE_FAULT] Hardware Spatial Isolation Enforced!\n");
    semihost_print("  SCB_CFSR  = ");
    print_hex32(cfsr);
    semihost_print("\n  SCB_MMFAR = ");
    print_hex32(mmfar);
    semihost_print("\n  STATUS: PASS (Cross-partition write blocked by Cortex-M3 MPU)\n");

    semihost_exit(0);
}

void HardFault_Handler(void)
{
    semihost_print("[HARDFAULT] HFSR=");
    print_hex32(*(volatile uint32_t *)0xE000ED2CUL);
    semihost_print(" CFSR=");
    print_hex32(SCB_CFSR);
    semihost_print("\n");
    semihost_exit(2);
}

void vApplicationGetIdleTaskMemory(
    StaticTask_t **tcb,
    StackType_t **stack,
    configSTACK_DEPTH_TYPE *size)
{
    *tcb  = &idle_tcb;
    *stack = idle_stack;
    *size = configMINIMAL_STACK_SIZE;
}

void vApplicationAssertFailed(const char *file, int line)
{
    (void)file;
    (void)line;
    semihost_print("ERROR: FreeRTOS assert failed\n");
    semihost_exit(3);
}

int main(void)
{
    uint32_t mpu_type = MPU_TYPE;
    semihost_print("=== Stage 2: Cortex-M3 MPU Static Memory Partitioning + INT8 ===\n");
    semihost_print("MPU_TYPE Register = ");
    print_hex32(mpu_type);
    semihost_print("\n");

    mpu_configure_base();

    h_inference_task = xTaskCreateStatic(
        inference_task,
        "INF_PART1",
        INFERENCE_STACK_WORDS,
        NULL,
        3,
        g_inf_arena.stack,
        &inference_tcb
    );

    h_aux_task = xTaskCreateStatic(
        aux_task,
        "AUX_PART2",
        AUX_STACK_WORDS,
        NULL,
        2,
        g_aux_ram.stack,
        &aux_tcb
    );

    configASSERT(h_inference_task != NULL);
    configASSERT(h_aux_task != NULL);

    vTaskStartScheduler();
    for (;;) {}
}
