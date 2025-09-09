#include <am.h>
#include <platform.h>
#include <klib.h>
#include <klib-macros.h>

#define NUM_CORES 2
#define ITERATIONS 100
#define MEMCPY_SIZE (1024 * 4)
#define MMIO_TEST_COUNT 1000

#define DDR_SRC_ADDR 0x80100000
#define DDR_DST_ADDR 0x80200000

#define MMIO_BASE 0x30000000000 

// --- 用于多核同步的全局变量 ---
static volatile int cpu0_done = 0;
static volatile int cpu1_done = 0;
static volatile int test_failed = 0;

int main() {
    int hartid = riscv_mhartid();

    if (hartid >= NUM_CORES) {
        while(1) riscv_wfi();
    }

    if (hartid == 0) {
        atomic_printf("--- ST-STR-001: CPU0 (memcpy) vs CPU1 (MMIO) Test Started ---\n");

        uint8_t *src = (uint8_t *)DDR_SRC_ADDR;
        for (int i = 0; i < MEMCPY_SIZE; i++) {
            src[i] = (uint8_t)(i % 256);
        }
        riscv_fence();

        for (int i = 0; i < ITERATIONS; i++) {
            uint8_t *dst = (uint8_t *)DDR_DST_ADDR;

            memcpy(dst, src, MEMCPY_SIZE);

            for (int j = 0; j < MEMCPY_SIZE; j++) {
                if (dst[j] != (uint8_t)(j % 256)) {
                    atomic_printf("CPU0: Memcpy data check failed at iteration %d, index %d!\n", i, j);
                    test_failed = 1;
                    break;
                }
            }
            if(test_failed) break;
        }
        
        cpu0_done = 1;
        riscv_fence();

    } else if (hartid == 1) {
        volatile uint32_t *mmio_reg = (volatile uint32_t*)MMIO_BASE;
        uint32_t write_val = 0xAAAAAAAA;

        for (int i = 0; i < ITERATIONS; i++) {
            for (int j = 0; j < MMIO_TEST_COUNT; j++) {
                *mmio_reg = write_val;

                uint32_t read_val = *mmio_reg;
                if (read_val != write_val) {
                    atomic_printf("CPU1: Peripheral R/W check failed at iteration %d!\n", i);
                    atomic_printf("  Wrote: 0x%x, Read: 0x%x\n", write_val, read_val);
                    test_failed = 1;
                    break;
                }
                
                write_val = ~write_val;
            }
            if(test_failed) break;
        }

        cpu1_done = 1;
        riscv_fence();
    } else{
        riscv_wfi();
    }

    while(cpu0_done == 0 || cpu1_done == 0) {
        // spin & wait
    }

    if (hartid == 0) {
        if (test_failed == 0) {
            atomic_printf("--- SUCCESS! ---\n");
        } else {
            atomic_printf("--- FAILED! ---\n");
        }
    }

    return 0;
}