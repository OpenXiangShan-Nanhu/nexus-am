/*
 * ST-STR-016: DMA Master1/2 False Sharing Test
 * 
 * Method: Master1 updates regions 0,1; Master2 updates regions 2,3; CPU2 checks consistency
 * Expected: Data remains consistent, no system crash
 */

#include <am.h>
#include <platform.h>
#include <klib.h>
#include <klib-macros.h>
#include <stdbool.h>
#include "dw_axi_dmac.h"

// Test Configuration
#define TEST_ITERATIONS     10
#define CACHELINE_SIZE      64
#define REGION_SIZE         32  // Must be 32 bytes for DMA alignment requirement

#define NUM_CORES 4

// Test structure aligned to cacheline boundary
// Each cacheline (64 bytes) contains 2 regions (32 bytes each)
// This creates false sharing when different DMA masters update different regions
typedef struct {
    volatile uint32_t region0[8];  // 32 bytes - DMA Master1 updates
    volatile uint32_t region1[8];  // 32 bytes - DMA Master2 updates
} __attribute__((aligned(64))) test_cacheline_t;

// Global test data
static test_cacheline_t test_cacheline __attribute__((aligned(64)));
// Test control variables
static volatile int test_running = 1;
static volatile int errors = 0;
static volatile int dma_masters_ready = 0;  // DMA masters signal when ready

// DMA completion tracking: each transfer has unique ID = (iter * 2 + region)
// Master1 region0: iter*2+0
// Master2 region1: iter*2+1
#define MAX_TRANSFERS (TEST_ITERATIONS * 2)
static volatile int dma_completed[MAX_TRANSFERS];  // Completion flag for each transfer
static volatile int cpu2_check_done = 0;  // CPU2 signals when check is done for current iteration

// Generate test pattern
static uint32_t generate_pattern(int master, int region, int iteration) {
    return (master << 24) | (region << 16) | (iteration & 0xFFFF);
}

// DMA completion callback
void dma_completion_callback(int error_code, uint64_t user_data) {
    if (error_code == 0) {
        int transfer_id = (int)user_data;
        if (transfer_id >= 0 && transfer_id < MAX_TRANSFERS) {
            dma_completed[transfer_id] = 1;
        }
    } else {
        errors++;
    }
}

// DMA Master1 task - updates region 0
static void dma_master1_task() {
    static uint32_t src_buffer[8] __attribute__((aligned(32)));  // 32 bytes buffer
    
    atomic_printf("Master1 starting...\n");
    
    // Signal ready
    __sync_fetch_and_add(&dma_masters_ready, 1);
    
    // Wait for both DMA masters to be ready
    while (dma_masters_ready < 2) {
        for (volatile int d = 0; d < 10; d++) asm volatile("nop");
    }
    
    for (int iter = 0; iter < TEST_ITERATIONS; iter++) {
        // Update region 0 - Transfer ID: iter*2+0
        int transfer_id = iter * 2 + 0;
        
        // Fill buffer with pattern (8 uint32_t = 32 bytes)
        for (int i = 0; i < 8; i++) {
            src_buffer[i] = generate_pattern(1, 0, iter);
        }
        
        int ret = dma_transfer(DMA_MEM_TO_MEM, (uint64_t)src_buffer,
                    (uint64_t)&test_cacheline.region0[0], 32,  // 32 bytes
                    DWAXIDMAC_AX_CACHE_CACHEABLE, DWAXIDMAC_AX_CACHE_CACHEABLE,
                    dma_completion_callback, transfer_id);
        
        if (ret != 0) {
            atomic_printf("Master1: DMA transfer failed with ret=%d\n", ret);
            errors++;
            return;
        }
        
        // Wait for this transfer to complete
        uint32_t timeout = 0;
        while (!dma_completed[transfer_id] && timeout < 100000) {
            asm volatile("nop");
            timeout++;
        }
        if (!dma_completed[transfer_id]) {
            atomic_printf("Master1: timeout at iter %d (id=%d)\n", iter, transfer_id);
            errors++;
            return;
        }
        
        // Wait for CPU2 to finish checking this iteration before proceeding
        timeout = 0;
        while (cpu2_check_done < iter + 1 && timeout < 1000000) {
            asm volatile("nop");
            timeout++;
        }
        if (cpu2_check_done < iter + 1) {
            atomic_printf("Master1: timeout waiting for CPU2 check at iter %d\n", iter);
            errors++;
            return;
        }
    }
}

// DMA Master2 task - updates region 1
static void dma_master2_task() {
    static uint32_t src_buffer[8] __attribute__((aligned(32)));  // 32 bytes buffer
    
    atomic_printf("Master2 starting...\n");
    
    // Signal ready
    __sync_fetch_and_add(&dma_masters_ready, 1);
    
    // Wait for both DMA masters to be ready
    while (dma_masters_ready < 2) {
        for (volatile int d = 0; d < 10; d++) asm volatile("nop");
    }
    
    for (int iter = 0; iter < TEST_ITERATIONS; iter++) {
        // Update region 1 - Transfer ID: iter*2+1
        int transfer_id = iter * 2 + 1;
        
        // Fill buffer with pattern (8 uint32_t = 32 bytes)
        for (int i = 0; i < 8; i++) {
            src_buffer[i] = generate_pattern(2, 1, iter);
        }
        
        int ret = dma_transfer(DMA_MEM_TO_MEM, (uint64_t)src_buffer,
                    (uint64_t)&test_cacheline.region1[0], 32,  // 32 bytes
                    DWAXIDMAC_AX_CACHE_CACHEABLE, DWAXIDMAC_AX_CACHE_CACHEABLE,
                    dma_completion_callback, transfer_id);
        
        if (ret != 0) {
            atomic_printf("Master2: DMA transfer failed with ret=%d\n", ret);
            errors++;
            return;
        }
        
        // Wait for this transfer to complete
        uint32_t timeout = 0;
        while (!dma_completed[transfer_id] && timeout < 100000) {
            asm volatile("nop");
            timeout++;
        }
        if (!dma_completed[transfer_id]) {
            atomic_printf("Master2: timeout at iter %d (id=%d)\n", iter, transfer_id);
            errors++;
            return;
        }
        
        // Wait for CPU2 to finish checking this iteration before proceeding
        timeout = 0;
        while (cpu2_check_done < iter + 1 && timeout < 1000000) {
            asm volatile("nop");
            timeout++;
        }
        if (cpu2_check_done < iter + 1) {
            atomic_printf("Master2: timeout waiting for CPU2 check at iter %d\n", iter);
            errors++;
            return;
        }
    }
}

// CPU2 checker task - validates consistency
static void cpu2_checker_task() {
    atomic_printf("CPU2 checker waiting for DMA masters to be ready...\n");
    
    // Wait for both DMA masters to be ready
    while (dma_masters_ready < 2) {
        for (volatile int d = 0; d < 100; d++) asm volatile("nop");
    }
    
    atomic_printf("CPU2 checker starting checks...\n");
    
    // Check each iteration after both DMA transfers complete
    for (int iter = 0; iter < TEST_ITERATIONS; iter++) {
        // Calculate transfer IDs for this iteration
        int id_r0 = iter * 2 + 0;  // Master1 region0
        int id_r1 = iter * 2 + 1;  // Master2 region1
        
        // Wait for both transfers of this iteration to complete
        uint32_t timeout = 0;
        while ((!dma_completed[id_r0] || !dma_completed[id_r1]) && 
               timeout < 1000000) {
            asm volatile("nop");
            timeout++;
        }
        
        if (timeout >= 1000000) {
            atomic_printf("CPU2: Timeout waiting for iter %d transfers\n", iter);
            atomic_printf("  Completion status: r0=%d r1=%d\n",
                         dma_completed[id_r0], dma_completed[id_r1]);
            errors++;
            return;
        }
        
        atomic_printf("CPU2: Checking iteration %d (both transfers completed)...\n", iter);
        
        // Now check both regions - they should be consistent and match expected iteration
        for (int region = 0; region < 2; region++) {
            volatile uint32_t *region_ptr;
            int expected_master;
            
            if (region == 0) {
                region_ptr = test_cacheline.region0;
                expected_master = 1;
            } else {
                region_ptr = test_cacheline.region1;
                expected_master = 2;
            }
            
            // Read all 8 values in the region (32 bytes)
            uint32_t values[8];
            for (int i = 0; i < 8; i++) {
                values[i] = region_ptr[i];
            }
            
            // Check consistency within region
            bool consistent = true;
            uint32_t pattern = values[0];
            int master = (pattern >> 24) & 0xFF;
            int reg = (pattern >> 16) & 0xFF;
            int iteration = pattern & 0xFFFF;
            
            for (int i = 1; i < 8; i++) {
                if (values[i] != pattern) {
                    consistent = false;
                    break;
                }
            }
            
            if (!consistent) {
                errors++;
                atomic_printf("ERROR: Inconsistency in region %d at iter %d\n", region, iter);
                atomic_printf("  First 4 values: 0x%08x 0x%08x 0x%08x 0x%08x\n", 
                             values[0], values[1], values[2], values[3]);
                atomic_printf("  Last 4 values:  0x%08x 0x%08x 0x%08x 0x%08x\n", 
                             values[4], values[5], values[6], values[7]);
            } else if (master != expected_master || reg != region) {
                errors++;
                atomic_printf("ERROR: Wrong pattern in region %d at iter %d\n", region, iter);
                atomic_printf("  Got: master=%d reg=%d iter=%d, Expected: master=%d reg=%d iter=%d\n", 
                             master, reg, iteration, expected_master, region, iter);
            } else if (iteration != iter) {
                errors++;
                atomic_printf("ERROR: Wrong iteration in region %d: got %d, expected %d\n", 
                             region, iteration, iter);
            }
        }
        
        // Signal CPU2 has finished checking this iteration
        __sync_fetch_and_add(&cpu2_check_done, 1);
    }
    
    atomic_printf("CPU2: All %d iterations checked successfully\n", TEST_ITERATIONS);
}

int main() {
    uint64_t hartid = riscv_mhartid();
    
    // All cores initialize DMA
    dma_init();
    
    // Only Core 0 initializes memory and prints startup message
    if (hartid == 0) {
        atomic_printf("ST-STR-016: DMA False Sharing Test Starting\n");
        atomic_printf("  Configuration: 1 cacheline (64 bytes) with 2 regions (32 bytes each)\n");
        atomic_printf("  Master1 updates region0, Master2 updates region1\n");
        
        // Clear test data - now 8 uint32_t per region (32 bytes each)
        for (int i = 0; i < 8; i++) {
            test_cacheline.region0[i] = 0;
            test_cacheline.region1[i] = 0;
        }
        
        // Ensure initialization is visible to all cores
        asm volatile("fence w, rw" ::: "memory");
    }
    
    // Synchronization barrier - wait for Core 0 to finish initialization
    barrier(NUM_CORES);
    
    switch (hartid) {
        case 0:
            dma_master1_task();  // Updates region 0
            break;
        case 1:
            dma_master2_task();  // Updates region 1
            break;
        case 2:
            cpu2_checker_task(); // Validates consistency
            break;
        default:
            // Other cores wait
            riscv_wfi();
            break;
    }
    
    // Test complete
    test_running = 0;
    riscv_fence();
    
    if (hartid == 0) {
        atomic_printf("ST-STR-016 Test Complete\n");
        atomic_printf("Errors detected: %d\n", errors);
        if (errors == 0) {
            atomic_printf("PASS: No false sharing issues detected\n");
        } else {
            atomic_printf("FAIL: False sharing issues found\n");
        }
    }

    barrier(NUM_CORES);
    
    return 0;
}
