#include <am.h>
#include <platform.h>
#include <klib.h>
#include <klib-macros.h>

#define NUM_CORES 4
#define ITERATIONS 50
#define MEMCPY_SIZE (1024 * 4)
#define MMIO_TEST_COUNT 1000

#define DDR_SRC_BASE 0x80010000
#define DDR_DST_BASE 0x80011000
#define PER_CORE_MEM_REGION_SIZE (2 * MEMCPY_SIZE)

#define PERIPHERAL_REG_BASE_0 0x30000000000
#define PERIPHERAL_REG_BASE_1 0x30000010000

static volatile int core_done_flags[NUM_CORES] = {0};
static volatile int test_failed_flags[NUM_CORES] = {0};
static volatile int all_cores_ready = 0;

void run_memcpy_task(int hartid) {
    uintptr_t src_addr = DDR_SRC_BASE + hartid * PER_CORE_MEM_REGION_SIZE;
    uintptr_t dst_addr = src_addr + MEMCPY_SIZE;

    uint8_t *src = (uint8_t *)src_addr;
    for (int i = 0; i < MEMCPY_SIZE; i++) {
        src[i] = (uint8_t)(i + hartid);
    }
    riscv_fence();

    for (int i = 0; i < ITERATIONS; i++) {
        memcpy((void*)dst_addr, (void*)src_addr, MEMCPY_SIZE);
        for (int j = 0; j < MEMCPY_SIZE; j++) {
            if (((uint8_t*)dst_addr)[j] != (uint8_t)(j + hartid)) {
                test_failed_flags[hartid] = 1;
                return;
            }
        }
    }
}

void run_mmio_task(int hartid, uintptr_t reg_addr) {
    volatile uint32_t *reg = (volatile uint32_t*)reg_addr;
    uint32_t write_val = 0xCAFE0000 | hartid;
    
    for (int i = 0; i < ITERATIONS; i++) {
        for (int j = 0; j < MMIO_TEST_COUNT; j++) {
            *reg = write_val;
            if (*reg != write_val) {
                test_failed_flags[hartid] = 1;
                return;
            }
            write_val = ~write_val;
        }
    }
}


int main() {
    int hartid = riscv_mhartid();

    if (hartid >= NUM_CORES) {
        while(1) riscv_wfi();
    }

    if(hartid == 0) {
        atomic_printf("--- ST-STR-004: 4-Core Concurrent memcpy & MMIO Test Started ---\n");
        all_cores_ready = 1;
        riscv_fence();
    } else {
        while(all_cores_ready == 0);
    }

    switch(hartid) {
        case 0:
        case 1:
            run_memcpy_task(hartid);
            break;
        case 2:
            run_mmio_task(hartid, PERIPHERAL_REG_BASE_0);
            break;
        case 3:
            run_mmio_task(hartid, PERIPHERAL_REG_BASE_1);
            break;
    }

    core_done_flags[hartid] = 1;
    riscv_fence();

    if (hartid == 0) {
        for (int i = 1; i < NUM_CORES; i++) {
            while (core_done_flags[i] == 0);
        }

        int final_result = 0;
        for (int i = 0; i < NUM_CORES; i++) {
            if (test_failed_flags[i] != 0) {
                atomic_printf("Core %d reported a failure!\n", i);
                final_result = 1;
            }
        }

        if (final_result == 0) {
            atomic_printf("--- SUCCESS! All cores completed tasks without errors. ---\n");
        } else {
            atomic_printf("--- FAILED! ---\n");
        }
    }
    
    return 0;
}