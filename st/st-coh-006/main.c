#include <am.h>
#include <platform.h>
#include <klib.h>
#include <klib-macros.h>
#include <plic.h>
#include <xsextra.h>
#include "dw_axi_dmac.h"

#define NUM_CORES 4
#define NUM_TRANSFERS_PER_CORE 4
#define TRANSFER_SIZE (1024)   // ≤ 200000

#define SHARED_MEM_ADDRESS  0x80100000

volatile uint32_t cpu0_write_complete = 0;
volatile uint32_t cpu1_read_complete = 0;
static volatile uint32_t test_status[2] = {0};

const uint32_t INITIAL_WORD_VALUE = 0xDEADBEEF;
const uint8_t  BYTE_TO_WRITE      = 0xAB;
// 0xDEADBEEF -> 0xDEADBEAB
const uint32_t EXPECTED_WORD_VALUE= 0xDEADBEAB;

int main() {
    uint64_t hartid = riscv_mhartid();

    if (hartid == 0) {
        volatile uint32_t* word_ptr = (volatile uint32_t*)SHARED_MEM_ADDRESS;
        volatile uint8_t*  byte_ptr = (volatile uint8_t*)SHARED_MEM_ADDRESS;

        *word_ptr = INITIAL_WORD_VALUE;

        cpu0_write_complete = 0;
        cpu1_read_complete = 0;
        test_status[0] = 0;
        test_status[1] = 0;

        riscv_fence();

        uint32_t initial_read = *word_ptr;
        // INITIAL FAILED
        if (initial_read != INITIAL_WORD_VALUE) {
            test_status[0] = 2;
        }

        if (test_status[0] == 0) {
            // CPU0 WRITE -> A
            *byte_ptr = BYTE_TO_WRITE;

            riscv_fence();

            cpu0_write_complete = 1;

            riscv_fence();  // FLUSH STORE BUFFER

            while(cpu1_read_complete == 0);
            
            if (test_status[1] == 1) {
                test_status[0] = 1;     // SUCCESS
            } else {
                test_status[0] = 2;     // FAILED
            }
        }
        
        if (test_status[0] == 1) {
            atomic_printf("TEST PASS!\n");
        } else {
            atomic_printf("TEST FAILED!!!\n");
        }

    } 
    else if (hartid == 1) {
        volatile uint32_t* word_ptr = (volatile uint32_t*)SHARED_MEM_ADDRESS;

        while(cpu0_write_complete == 0);

        uint32_t value_read_by_cpu1 = *word_ptr;

        if (value_read_by_cpu1 == EXPECTED_WORD_VALUE) {
            test_status[1] = 1;
        } else {
            test_status[1] = 2;
        }

        riscv_fence();
        
        cpu1_read_complete = 1;

        riscv_fence();
        
        riscv_wfi();
    }
    else {
        riscv_wfi();
    }
    
    return 0;
}