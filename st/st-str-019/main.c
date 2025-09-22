/*
 * ST-STR-019: High-frequency CMO stress test
 * 多核并发对不同的内存区域执行CBO.FLUSH，验证CMO操作正确性和系统稳定性
 * 
 * Test description:
 * - 4个核并发对不同内存区域执行高频CBO.FLUSH操作
 * - 每个核负责独立的内存区域，避免冲突
 * - 使用数据验证确保CMO操作正确完成
 * - 统计CMO操作频率和正确性
 * - 验证系统长期稳定性
 */

#include <am.h>
#include <platform.h>
#include <klib.h>
#include <klib-macros.h>
#include <cmo.h>
#include <xsextra.h>

#define NUM_CORES 4
#define CMO_ITERATIONS 10
#define MEMORY_REGIONS 4
#define REGION_SIZE (64 * 1024)    // 64KB per region
#define CACHELINE_SIZE 64
#define LINES_PER_REGION (REGION_SIZE / CACHELINE_SIZE)

// Memory regions for different cores
#define CORE0_MEM_BASE 0x80200000
#define CORE1_MEM_BASE 0x80220000  
#define CORE2_MEM_BASE 0x80240000
#define CORE3_MEM_BASE 0x80260000

// Test statistics structure
typedef struct {
    volatile uint64_t cmo_operations;      // Total CMO operations
    volatile uint64_t cmo_flush_count;     // CBO.FLUSH count
    volatile uint64_t data_write_count;    // Data writes
    volatile uint64_t data_read_count;     // Data reads
    volatile uint64_t verification_pass;   // Verification successes
    volatile uint64_t verification_fail;   // Verification failures
    volatile uint64_t start_time;          // Start timestamp
    volatile uint64_t end_time;            // End timestamp
} core_stats_t;

// Global variables
static volatile core_stats_t core_stats[NUM_CORES] __attribute__((aligned(64)));
static volatile bool test_complete[NUM_CORES] = {false, false, false, false};
static volatile bool test_failed = false;
static volatile uint64_t global_error_count = 0;
static volatile uint64_t sync_barrier = 0;

// Core memory regions
static volatile uint32_t* core_memory[NUM_CORES] = {
    (volatile uint32_t*)CORE0_MEM_BASE,
    (volatile uint32_t*)CORE1_MEM_BASE,
    (volatile uint32_t*)CORE2_MEM_BASE,
    (volatile uint32_t*)CORE3_MEM_BASE
};

/*
 * High-frequency CBO.FLUSH operation on a memory region
 */
static inline void cmo_flush_region(volatile uint8_t* addr, size_t size) {
    volatile uint8_t* current = addr;
    volatile uint8_t* end = addr + size;
    
    while (current < end) {
        riscv_cbo_flush((uint64_t)current);
        current += CACHELINE_SIZE;
    }
    
    // Memory barrier to ensure ordering
    riscv_fence();
}

/*
 * Initialize memory region with pattern
 */
static void init_memory_region(volatile uint32_t* base, uint32_t pattern) {
    for (int i = 0; i < REGION_SIZE / sizeof(uint32_t); i++) {
        base[i] = pattern + i;
    }
}

/*
 * Verify memory region integrity
 */
static bool verify_memory_region(volatile uint32_t* base, uint32_t expected_pattern) {
    for (int i = 0; i < REGION_SIZE / sizeof(uint32_t); i++) {
        if (base[i] != (expected_pattern + i)) {
            return false;
        }
    }
    return true;
}

/*
 * Barrier synchronization for all cores
 */
static void barrier_sync(void) {
    static volatile uint64_t barrier_lock = 0;
    uint64_t hartid = riscv_mhartid();
    
    // Simple atomic increment using test-and-set
    while (__sync_lock_test_and_set((volatile int*)&barrier_lock, 1)) {
        // Spin wait
    }
    sync_barrier++;
    __sync_lock_release((volatile int*)&barrier_lock);
    
    // Wait for all cores
    while (sync_barrier < NUM_CORES) {
        riscv_fence();
    }
    
    // Reset barrier for next use (only core 0)
    if (hartid == 0) {
        sync_barrier = 0;
        riscv_fence();
    } else {
        while (sync_barrier != 0) {
            riscv_fence();
        }
    }
}

/*
 * Core-specific CMO stress test
 */
static void cmo_stress_test(uint64_t hartid) {
    volatile uint32_t* my_memory = core_memory[hartid];
    core_stats_t* my_stats = (core_stats_t*)&core_stats[hartid];
    uint32_t base_pattern = 0x12345000 + (hartid << 16);
    
    // Initialize statistics
    my_stats->cmo_operations = 0;
    my_stats->cmo_flush_count = 0;
    my_stats->data_write_count = 0;
    my_stats->data_read_count = 0;
    my_stats->verification_pass = 0;
    my_stats->verification_fail = 0;
    my_stats->start_time = uptime();
    
    atomic_printf("Core %d: Starting CMO stress test, memory base: 0x%lx\n", 
                  hartid, (uint64_t)my_memory);
    
    // Main test loop
    for (uint64_t iter = 0; iter < CMO_ITERATIONS && !test_failed; iter++) {
        uint32_t pattern = base_pattern + (iter & 0xFFFF);
        
        // Write data pattern to memory
        init_memory_region(my_memory, pattern);
        my_stats->data_write_count++;
        
        // Force data to cache/memory with fence
        riscv_fence();
        
        // High-frequency CBO.FLUSH operations
        for (int flush_round = 0; flush_round < 10; flush_round++) {
            cmo_flush_region((volatile uint8_t*)my_memory, REGION_SIZE);
            my_stats->cmo_flush_count++;
        }
        
        my_stats->cmo_operations += 10;  // 10 flush operations per iteration
        
        // Read and verify data
        my_stats->data_read_count++;
        if (verify_memory_region(my_memory, pattern)) {
            my_stats->verification_pass++;
        } else {
            my_stats->verification_fail++;
            atomic_printf("Core %d: Data verification failed at iteration %d!\n", 
                          hartid, iter);
            
            static volatile uint64_t error_lock = 0;
            while (__sync_lock_test_and_set((volatile int*)&error_lock, 1)) {
                // Spin wait
            }
            global_error_count++;
            __sync_lock_release((volatile int*)&error_lock);
            
            if (global_error_count > 10) {
                test_failed = true;
                break;
            }
        }
        
        // Periodic progress report
        if ((iter % 1000) == 0) {
            atomic_printf("Core %d: Completed %d iterations, %d CMO ops, %d verifications passed\n",
                          hartid, iter, my_stats->cmo_operations, my_stats->verification_pass);
        }
        
        // Small delay to allow other cores to run
        if ((iter % 100) == 0) {
            for (volatile int i = 0; i < 10; i++) {
                asm volatile("nop");
            }
        }
    }
    
    my_stats->end_time = uptime();
    test_complete[hartid] = true;
    
    atomic_printf("Core %d: CMO stress test completed\n", hartid);
}

/*
 * Print comprehensive test results
 */
static void print_test_results(void) {
    uint64_t total_cmo_ops = 0;
    uint64_t total_verifications = 0;
    uint64_t total_failures = 0;
    uint64_t min_time = UINT64_MAX;
    uint64_t max_time = 0;
    
    atomic_printf("\n=== ST-STR-019 CMO Stress Test Results ===\n");
    atomic_printf("Global error count: %d\n", global_error_count);
    atomic_printf("Test status: %s\n", test_failed ? "FAILED" : "PASSED");
    atomic_printf("\nPer-core statistics:\n");
    
    for (int i = 0; i < NUM_CORES; i++) {
        core_stats_t* stats = (core_stats_t*)&core_stats[i];
        uint64_t duration = stats->end_time - stats->start_time;
        uint64_t cmo_rate = (duration > 0) ? (stats->cmo_operations * 1000000) / duration : 0;
        
        atomic_printf("Core %d:\n", i);
        atomic_printf("  Memory region: 0x%lx - 0x%lx\n", 
                      (uint64_t)core_memory[i], 
                      (uint64_t)core_memory[i] + REGION_SIZE);
        atomic_printf("  CMO operations: %d\n", stats->cmo_operations);
        atomic_printf("  CBO.FLUSH count: %d\n", stats->cmo_flush_count);
        atomic_printf("  Data writes: %d\n", stats->data_write_count);
        atomic_printf("  Data reads: %d\n", stats->data_read_count);
        atomic_printf("  Verifications passed: %d\n", stats->verification_pass);
        atomic_printf("  Verifications failed: %d\n", stats->verification_fail);
        atomic_printf("  Duration: %d us\n", duration);
        atomic_printf("  CMO rate: %d ops/sec\n", cmo_rate);
        atomic_printf("  Success rate: %.2f%%\n", 
                      stats->verification_pass * 100.0 / (stats->verification_pass + stats->verification_fail));
        
        total_cmo_ops += stats->cmo_operations;
        total_verifications += stats->verification_pass;
        total_failures += stats->verification_fail;
        
        if (duration < min_time) min_time = duration;
        if (duration > max_time) max_time = duration;
    }
    
    atomic_printf("\nOverall summary:\n");
    atomic_printf("  Total CMO operations: %d\n", total_cmo_ops);
    atomic_printf("  Total verifications: %d\n", total_verifications);
    atomic_printf("  Total failures: %d\n", total_failures);
    atomic_printf("  Min core duration: %d us\n", min_time);
    atomic_printf("  Max core duration: %d us\n", max_time);
    atomic_printf("  Overall success rate: %.2f%%\n", 
                  total_verifications * 100.0 / (total_verifications + total_failures));
    
    // Final validation
    bool all_cores_completed = true;
    for (int i = 0; i < NUM_CORES; i++) {
        if (!test_complete[i]) {
            all_cores_completed = false;
            break;
        }
    }
    
    atomic_printf("\nFinal validation:\n");
    atomic_printf("  All cores completed: %s\n", all_cores_completed ? "YES" : "NO");
    atomic_printf("  System stable: %s\n", (!test_failed && total_failures == 0) ? "YES" : "NO");
    atomic_printf("  High-frequency CMO stress test: %s\n", 
                  (!test_failed && total_failures == 0 && all_cores_completed) ? "PASSED" : "FAILED");
}

int main(void) {
    uint64_t hartid = riscv_mhartid();
    
    if (hartid == 0) {
        atomic_printf("=== ST-STR-019: High-frequency CMO Stress Test ===\n");
        atomic_printf("Test configuration:\n");
        atomic_printf("  Cores: %d\n", NUM_CORES);
        atomic_printf("  Iterations per core: %d\n", CMO_ITERATIONS);
        atomic_printf("  Memory region size: %d KB\n", REGION_SIZE / 1024);
        atomic_printf("  Cache line size: %d bytes\n", CACHELINE_SIZE);
        atomic_printf("  Lines per region: %d\n", LINES_PER_REGION);
        atomic_printf("Starting test...\n\n");
    }
    
    // Wait for all cores to start
    barrier_sync();
    
    if (hartid < NUM_CORES) {
        // Initialize memory region for this core
        atomic_printf("Core %d: Initializing memory region at 0x%lx\n", 
                      hartid, (uint64_t)core_memory[hartid]);
        
        // Clear memory region
        memset((void*)core_memory[hartid], 0, REGION_SIZE);
        
        // Synchronize before starting test
        barrier_sync();
        
        // Run CMO stress test
        cmo_stress_test(hartid);
        
        // Wait for all cores to complete
        while (!test_complete[0] || !test_complete[1] || 
               !test_complete[2] || !test_complete[3]) {
            riscv_fence();
        }
        
        // Print results (only core 0)
        if (hartid == 0) {
            print_test_results();
        }
    } else {
        atomic_printf("Core %d: Not participating in test (only %d cores used)\n", 
                      hartid, NUM_CORES);
    }
    
    return test_failed ? 1 : 0;
}