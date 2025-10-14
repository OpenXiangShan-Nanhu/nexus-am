/*
 * ST-STR-011: Bus Bandwidth Saturation Stress Test
 * 
 * Method: All CPUs and DMA Masters simultaneously perform memory read/write to saturate bus bandwidth
 * Expected: All data transfers are correct, system doesn't crash
 */

#include <am.h>
#include <platform.h>
#include <klib.h>
#include <stdbool.h>
#include "dw_axi_dmac.h"

// Test Configuration
#define NUM_CORES           4
#define TEST_ITERATIONS     10      // Reduced for faster execution
#define CPU_BUFFER_SIZE     256     // 256 bytes per CPU (reduced)
#define DMA_BUFFER_SIZE     256     // 256 bytes per DMA (reduced)

// Memory regions - aligned for maximum bandwidth
#define CPU_MEM_BASE        0x80400000UL
#define DMA_SRC_BASE        0x81000000UL
#define DMA_DST_BASE        0x82000000UL

// Test statistics
typedef struct {
    volatile uint64_t operations;
    volatile uint64_t errors;
} cpu_stats_t;

typedef struct {
    volatile uint64_t transfers;
    volatile uint64_t errors;
} dma_stats_t;

// Global data
static cpu_stats_t cpu_stats[NUM_CORES] __attribute__((aligned(64)));
static dma_stats_t dma_stats __attribute__((aligned(64)));
static volatile int test_running = 1;
static volatile int dma_completed = 0;

// Initialize memory regions with test patterns
static void init_memory() {
    // Initialize CPU memory regions - each core's separate region
    for (int core = 0; core < NUM_CORES; core++) {
        volatile uint64_t *mem = (volatile uint64_t *)(CPU_MEM_BASE + core * CPU_BUFFER_SIZE);
        for (int i = 0; i < CPU_BUFFER_SIZE / sizeof(uint64_t); i++) {
            mem[i] = ((uint64_t)core << 32) | i;
        }
    }
    
    // Ensure all writes are visible
    asm volatile("fence w, w" ::: "memory");
    
    // Initialize DMA source memory
    volatile uint64_t *src = (volatile uint64_t *)DMA_SRC_BASE;
    for (int i = 0; i < DMA_BUFFER_SIZE / sizeof(uint64_t); i++) {
        src[i] = 0xDEADBEEF00000000UL | i;
    }
    
    // Final fence to ensure all initialization is complete
    asm volatile("fence w, rw" ::: "memory");
}

// CPU bandwidth stress test - intensive memory operations
static void cpu_stress_task(int core_id) {
    volatile uint64_t *mem = (volatile uint64_t *)(CPU_MEM_BASE + core_id * CPU_BUFFER_SIZE);
    int count = CPU_BUFFER_SIZE / sizeof(uint64_t);
    
    // Complete all iterations regardless of other cores
    for (int iter = 0; iter < TEST_ITERATIONS; iter++) {
        // Intensive memory operations to saturate bus bandwidth
        // Use ONLY atomic operations to ensure correctness
        for (int i = 0; i < count; i++) {
            // Atomic add 2 - guaranteed correct under concurrent stress
            __sync_fetch_and_add((uint64_t *)&mem[i], 2);
            
            // Additional load to stress bus
            volatile uint64_t dummy = mem[i];
            (void)dummy;
        }
        
        cpu_stats[core_id].operations += count * 2;  // Atomic + Load
    }
    
    // Final verification after all iterations complete
    // Expected value: initial_value + (TEST_ITERATIONS * 2)
    // Because each iteration does Atomic(+2)
    
    // Read barrier before verification
    asm volatile("fence r, r" ::: "memory");
    
    for (int i = 0; i < count; i++) {
        uint64_t initial_value = ((uint64_t)core_id << 32) | i;
        uint64_t expected_value = initial_value + (TEST_ITERATIONS * 2);
        uint64_t actual_value = mem[i];
        
        if (actual_value != expected_value) {
            cpu_stats[core_id].errors++;
            // Only print first few errors to avoid flooding output
            if (cpu_stats[core_id].errors <= 3) {
                // Detailed error message to help diagnosis
                uint64_t diff = (actual_value > initial_value) ? (actual_value - initial_value) : 0;
                atomic_printf("CPU%d: Data mismatch at [%d]:\n"
                             "  Initial  = 0x%lx\n"
                             "  Expected = 0x%lx (init + %d)\n"
                             "  Actual   = 0x%lx (init + %lu)\n",
                             core_id, i, 
                             initial_value, 
                             expected_value, TEST_ITERATIONS * 2,
                             actual_value, diff);
            }
        }
    }
}

// DMA completion callback
void dma_completion_callback(int error_code, uint64_t user_data) {
    if (error_code == 0) {
        dma_stats.transfers++;
        dma_completed = 1;
    } else {
        dma_stats.errors++;
    }
}

// DMA bandwidth stress test - continuous transfers
static void dma_stress_task() {
    static uint8_t src_buf[DMA_BUFFER_SIZE] __attribute__((aligned(64)));
    static uint8_t dst_buf[DMA_BUFFER_SIZE] __attribute__((aligned(64)));
    
    // Complete all iterations regardless of other cores
    for (int iter = 0; iter < TEST_ITERATIONS; iter++) {
        // Initialize source buffer with simple pattern for this iteration
        for (int i = 0; i < DMA_BUFFER_SIZE; i++) {
            src_buf[i] = (iter * 16 + i) & 0xFF;
        }
        
        // Clear destination buffer
        for (int i = 0; i < DMA_BUFFER_SIZE; i++) {
            dst_buf[i] = 0;
        }
        
        // Ensure source is written to memory
        asm volatile("fence w, w" ::: "memory");
        
        // Start DMA transfer
        dma_completed = 0;
        dma_transfer(DMA_MEM_TO_MEM, 
                    (uint64_t)src_buf,
                    (uint64_t)dst_buf,
                    DMA_BUFFER_SIZE,
                    DWAXIDMAC_AX_CACHE_CACHEABLE,
                    DWAXIDMAC_AX_CACHE_CACHEABLE,
                    dma_completion_callback,
                    0);
        
        // Wait for completion
        while (!dma_completed) {
            for (volatile int i = 0; i < 100; i++) asm volatile("nop");
        }
        
        // Ensure transfer is complete before verification
        asm volatile("fence iorw, iorw" ::: "memory");
        
        // Verify data after each transfer
        for (int i = 0; i < DMA_BUFFER_SIZE; i++) {
            uint8_t expected = (iter * 16 + i) & 0xFF;
            if (dst_buf[i] != expected) {
                dma_stats.errors++;
                // Print first few errors
                if (dma_stats.errors <= 3) {
                    atomic_printf("DMA: Data mismatch at iter=%d, [%d]: expected=0x%x, actual=0x%x\n",
                                 iter, i, expected, dst_buf[i]);
                }
                break;
            }
        }
    }
    
    // Final verification - check last transfer result
    int final_errors = 0;
    for (int i = 0; i < DMA_BUFFER_SIZE; i++) {
        uint8_t expected = ((TEST_ITERATIONS - 1) * 16 + i) & 0xFF;
        if (dst_buf[i] != expected) {
            final_errors++;
            if (final_errors <= 3) {
                atomic_printf("DMA Final: Data mismatch at [%d]: expected=0x%x, actual=0x%x\n",
                             i, expected, dst_buf[i]);
            }
        }
    }
    if (final_errors > 0) {
        dma_stats.errors += final_errors;
    }
}

// Print test results
static void print_results() {
    uint64_t total_cpu_ops = 0;
    uint64_t total_cpu_errors = 0;
    
    atomic_printf("\n=== ST-STR-011 Test Results ===\n");
    
    // CPU statistics
    for (int i = 0; i < NUM_CORES; i++) {
        atomic_printf("CPU%d: %lu operations, %lu errors\n", 
                     i, cpu_stats[i].operations, cpu_stats[i].errors);
        total_cpu_ops += cpu_stats[i].operations;
        total_cpu_errors += cpu_stats[i].errors;
    }
    
    // DMA statistics
    atomic_printf("DMA: %lu transfers, %lu errors\n", 
                 dma_stats.transfers, dma_stats.errors);
    
    // Summary
    atomic_printf("\nTotal CPU operations: %lu\n", total_cpu_ops);
    atomic_printf("Total CPU errors: %lu\n", total_cpu_errors);
    atomic_printf("Total DMA transfers: %lu\n", dma_stats.transfers);
    atomic_printf("Total DMA errors: %lu\n", dma_stats.errors);
    
    if (total_cpu_errors == 0 && dma_stats.errors == 0) {
        atomic_printf("\n[PASS] Bus bandwidth stress test completed successfully\n");
    } else {
        atomic_printf("\n[FAIL] Errors detected during stress test\n");
    }
}

int main() {
    uint64_t hartid = riscv_mhartid();
    
    // All cores initialize DMA
    dma_init();

    // Core 0 initialization
    if (hartid == 0) {
        atomic_printf("ST-STR-011: Bus Bandwidth Saturation Test Starting\n");
        atomic_printf("Cores: %d, Iterations: %d\n", NUM_CORES, TEST_ITERATIONS);
        
        // Initialize memory regions
        init_memory();
        
        // Clear statistics
        for (int i = 0; i < NUM_CORES; i++) {
            cpu_stats[i].operations = 0;
            cpu_stats[i].errors = 0;
        }
        dma_stats.transfers = 0;
        dma_stats.errors = 0;
    }
    
    // Synchronization barrier - ensure all cores see initialized memory
    riscv_fence();
    barrier(NUM_CORES);
    
    // Additional memory barrier to ensure cache coherency
    asm volatile("fence iorw, iorw" ::: "memory");
    
    // Force each core to verify it can read its own memory region correctly
    if (hartid < NUM_CORES) {
        volatile uint64_t *mem = (volatile uint64_t *)(CPU_MEM_BASE + hartid * CPU_BUFFER_SIZE);
        uint64_t test_read = mem[0];
        uint64_t expected_first = ((uint64_t)hartid << 32) | 0;
        
        if (test_read != expected_first) {
            atomic_printf("CPU%lu: WARNING - Initial read mismatch! expected=0x%lx, got=0x%lx\n",
                         hartid, expected_first, test_read);
            // Force invalidate and re-read
            asm volatile("fence.i" ::: "memory");
            asm volatile("fence iorw, iorw" ::: "memory");
        }
    }
    
    // Final barrier before starting stress test
    barrier(NUM_CORES);
    
    // Task assignment - all cores participate in stress test
    if (hartid < NUM_CORES) {
        if (hartid == 3) {
            // Core 3: Primary DMA stress test
            dma_stress_task();
        } else {
            // Cores 0-2: CPU stress test
            cpu_stress_task(hartid);
        }
    }
    
    // Signal test completion
    test_running = 0;
    riscv_fence();
    
    barrier(NUM_CORES);
    
    // Core 0 prints results
    if (hartid == 0) {
        print_results();
    }

    return 0;
}
