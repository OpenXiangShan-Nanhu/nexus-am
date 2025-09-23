/*
 * ST-STR-020: High-frequency sfence.vma pressure test
 * Purpose: Test multi-core concurrent page table modification with sfence.vma in S-mode
 * Expected: All address spaces correctly updated, system remains stable
 */

#include <am.h>
#include <platform.h>
#include <klib.h>
#include <klib-macros.h>
#include <printf.h>
#include <csr.h>
#include <riscv.h>
#include "clint.h"
#include "ppu.h"
#include "mtrap.h"
#include "strap.h"
#include "vmm.h"

// Test configuration
#define NUM_CORES           4
#define TEST_LITERATION     100     // Increased for better stress testing
#define SFENCE_FREQ         10      // Execute sfence.vma every SFENCE_FREQ operations
#define PROGRESS_INTERVAL   10      // Print progress every N iterations
#define TEST_PAGES          64      // Number of pages each core manages
#define PAGE_SIZE           4096    // 4KB
#define TEST_REGION_SIZE    (TEST_PAGES * PAGE_SIZE)
#define TEST_BASE_VADDR     0x90000000UL  // Virtual address base for testing
#define TEST_BASE_PADDR     0x90000000UL  // Physical address base

// Test data structures
static volatile int test_running = 1;
static volatile int cores_ready = 0;
static volatile int barrier_count = 0;

// Per-core test regions (virtual addresses)
static volatile uint64_t sfence_counts[NUM_CORES] = {0};
static volatile uint64_t page_mod_counts[NUM_CORES] = {0};
static volatile uint64_t error_counts[NUM_CORES] = {0};
static volatile int current_iterations[NUM_CORES] = {0};  // Track current iteration per core

// Page table operation counters
static volatile uint64_t total_operations = 0;
static volatile uint64_t total_sfence_ops = 0;

// Simple barrier synchronization for S-mode
static void s_barrier_sync(int expected_cores) {
    __sync_fetch_and_add(&barrier_count, 1);
    while (barrier_count < expected_cores) {
        asm volatile("nop");
    }
}

// Generate test pattern for verification
static uint32_t generate_pattern(int core_id, int page_idx, int iteration) {
    return (core_id << 24) | (page_idx << 16) | (iteration & 0xFFFF);
}

// Get virtual address for core's test page
static uintptr_t get_test_vaddr(int core_id, int page_idx) {
    return TEST_BASE_VADDR + (core_id * TEST_REGION_SIZE) + (page_idx * PAGE_SIZE);
}

// Get physical address for core's test page  
static uintptr_t get_test_paddr(int core_id, int page_idx) {
    return TEST_BASE_PADDR + (core_id * TEST_REGION_SIZE) + (page_idx * PAGE_SIZE);
}

// Verify page content integrity
static int verify_page_content(int core_id, uintptr_t vaddr, int page_idx, int expected_iteration) {
    volatile uint32_t *data = (volatile uint32_t *)vaddr;
    uint32_t expected = generate_pattern(core_id, page_idx, expected_iteration);
    
    for (int i = 0; i < PAGE_SIZE / sizeof(uint32_t); i++) {
        if (data[i] != expected) {
            return 0; // Verification failed
        }
    }
    return 1; // Verification passed
}

// Fill page with test pattern
static void fill_page_pattern(int core_id, uintptr_t vaddr, int page_idx, int iteration) {
    volatile uint32_t *data = (volatile uint32_t *)vaddr;
    uint32_t pattern = generate_pattern(core_id, page_idx, iteration);
    
    for (int i = 0; i < PAGE_SIZE / sizeof(uint32_t); i++) {
        data[i] = pattern;
    }
}

// Execute sfence.vma operations (must be in S-mode)
static void execute_sfence_vma_operations(uintptr_t vaddr) {
    // Execute various forms of sfence.vma
    // 1. Specific page flush
    asm volatile("sfence.vma %0, zero" : : "r"(vaddr) : "memory");
    
    // 2. Global TLB flush (all pages, all ASIDs) - less frequent
    static int counter = 0;
    if ((++counter % 10) == 0) {
        asm volatile("sfence.vma zero, zero" : : : "memory");
    }
    
    // Additional memory barriers to ensure ordering
    asm volatile("fence rw, rw" : : : "memory");
}

// Core test function for sfence.vma pressure testing (runs in S-mode)
static void sfence_pressure_test(int core_id) {
    int iteration = 0;
    uint64_t operation_count = 0;
    uint64_t local_sfence_count = 0;
    uint64_t local_page_mod_count = 0;
    uint64_t local_error_count = 0;
    
    s_atomic_printf("Core %d: Starting sfence.vma pressure test in S-mode\n", core_id);
    
    while (test_running) {
        for (int page_idx = 0; page_idx < TEST_PAGES && test_running; page_idx++) {
            uintptr_t vaddr = get_test_vaddr(core_id, page_idx);
            uintptr_t paddr = get_test_paddr(core_id, page_idx);
            
            // Phase 1: Remap page with different physical address periodically
            if ((iteration % 4) == 0) {
                // Change mapping to test TLB invalidation
                uintptr_t alt_paddr = paddr + 0x1000000; // Different physical page
                vm_map((void *)vaddr, (void *)alt_paddr, PTE_R | PTE_W | PTE_X | PTE_A | PTE_D);
                execute_sfence_vma_operations(vaddr);
                local_sfence_count++;
                
                // Map back to original
                vm_map((void *)vaddr, (void *)paddr, PTE_R | PTE_W | PTE_X | PTE_A | PTE_D);
                execute_sfence_vma_operations(vaddr);
                local_sfence_count++;
            }
            
            // Phase 2: Fill page with test pattern
            fill_page_pattern(core_id, vaddr, page_idx, iteration);
            local_page_mod_count++;
            
            // Phase 3: Execute sfence.vma operations
            execute_sfence_vma_operations(vaddr);
            
            // Phase 4: Verify page content after sfence.vma operations
            if (!verify_page_content(core_id, vaddr, page_idx, iteration)) {
                local_error_count++;
                s_atomic_printf("Core %d: Data verification failed at page %d, iteration %d\n", 
                               core_id, page_idx, iteration);
            }
            
            operation_count++;
            
            // Execute additional sfence.vma at specified frequency
            if (operation_count % SFENCE_FREQ == 0) {
                // Global sfence.vma to flush all TLB entries
                asm volatile("sfence.vma zero, zero" : : : "memory");
                local_sfence_count++;
            }
            
            // Periodic yield to allow other cores to run
            if (operation_count % 50 == 0) {
                asm volatile("nop; nop; nop; nop");
            }
        }
        
        iteration++;
        current_iterations[core_id] = iteration;  // Update current progress
        
        // Periodic progress report from each core
        if ((iteration % PROGRESS_INTERVAL) == 0) {
            s_atomic_printf("Core %d: Progress - Iteration %d/%d, Ops=%lu, sfence.vma=%lu, Errors=%lu\n",
                           core_id, iteration, TEST_LITERATION, operation_count, 
                           local_sfence_count, local_error_count);
            
            // Core 0 prints overall progress summary
            if (core_id == 0) {
                uint64_t total_current_ops = 0;
                uint64_t total_current_errors = 0;
                int min_iteration = iteration;
                int max_iteration = iteration;
                
                for (int i = 0; i < NUM_CORES; i++) {
                    total_current_ops += sfence_counts[i];
                    total_current_errors += error_counts[i];
                    if (current_iterations[i] < min_iteration) min_iteration = current_iterations[i];
                    if (current_iterations[i] > max_iteration) max_iteration = current_iterations[i];
                }
                
                s_atomic_printf("=== Global Progress: Min/Max Iteration: %d/%d, Total sfence.vma: %lu, Total Errors: %lu ===\n",
                               min_iteration, max_iteration, total_current_ops, total_current_errors);
            }
        }
        
        // Check if test duration exceeded (simplified time check)
        if (iteration > TEST_LITERATION) {  // Limit iterations instead of time-based
            break;
        }
    }
    
    // Update global counters
    sfence_counts[core_id] = local_sfence_count;
    page_mod_counts[core_id] = local_page_mod_count;
    error_counts[core_id] = local_error_count;
    
    __sync_fetch_and_add(&total_operations, operation_count);
    __sync_fetch_and_add(&total_sfence_ops, local_sfence_count);
    
    s_atomic_printf("Core %d: Completed %lu operations, %lu sfence.vma calls, %lu page mods, %lu errors\n",
           core_id, operation_count, local_sfence_count, local_page_mod_count, local_error_count);
}

// Print final test results (runs in S-mode)
static void print_test_results() {
    s_atomic_printf("\n=== ST-STR-020 Test Results ===\n");
    s_atomic_printf("Total Operations: %lu\n", total_operations);
    s_atomic_printf("Total sfence.vma Calls: %lu\n", total_sfence_ops);
    
    s_atomic_printf("\nPer-Core Statistics:\n");
    uint64_t total_errors = 0;
    uint64_t total_page_mods = 0;
    
    for (int i = 0; i < NUM_CORES; i++) {
        s_atomic_printf("Core %d: sfence.vma=%lu, page_mods=%lu, errors=%lu\n",
               i, sfence_counts[i], page_mod_counts[i], error_counts[i]);
        total_errors += error_counts[i];
        total_page_mods += page_mod_counts[i];
    }
    
    s_atomic_printf("\nSummary:\n");
    s_atomic_printf("Total Page Modifications: %lu\n", total_page_mods);
    s_atomic_printf("Total Errors: %lu\n", total_errors);
    
    if (total_errors == 0) {
        s_atomic_printf("✓ PASS: All address spaces correctly updated, system stable\n");
    } else {
        s_atomic_printf("✗ FAIL: %lu errors detected in address space updates\n", total_errors);
    }
}

// S-mode main function - this runs after switch to supervisor mode
void __attribute__((constructor)) s_main(int hartid) {
    asm volatile("mv a0, %0" :: "r"(hartid));
    
    if (hartid == 0) {
        s_atomic_printf("=== ST-STR-020: High-frequency sfence.vma Pressure Test (S-mode) ===\n");
        s_atomic_printf("Testing multi-core concurrent page table modification with sfence.vma\n");
        s_atomic_printf("Test Configuration:\n");
        s_atomic_printf("  Cores: %d\n", NUM_CORES);
        s_atomic_printf("  Pages per Core: %d\n", TEST_PAGES);
        s_atomic_printf("  Total Iterations: %d\n", TEST_LITERATION);
        s_atomic_printf("  Progress Report Interval: every %d iterations\n", PROGRESS_INTERVAL);
        s_atomic_printf("  sfence.vma Frequency: every %d operations\n", SFENCE_FREQ);
        s_atomic_printf("Starting test...\n");
        
        cores_ready = 1;
        riscv_fence();
    } else {
        // Wait for core 0 to initialize
        while (!cores_ready) {
            riscv_fence();
        }
    }
    
    // All cores run the sfence.vma pressure test in S-mode
    sfence_pressure_test(hartid);
    
    // Wait for all cores to complete using barrier
    s_barrier_sync(NUM_CORES);
    
    // Signal test completion
    test_running = 0;
    
    if (hartid == 0) {
        // Print results
        print_test_results();
        s_atomic_printf("ST-STR-020 test completed\n");
    }
    
    s_barrier(NUM_CORES, hartid);
    _halt(0);
}

// M-mode main function - initializes VM and switches to S-mode
int main() {
    uint64_t hartid = riscv_mhartid();
    
    // Register page fault handlers
    m_trap_handler_register(INS_PAGE_FAULT, default_page_fault_handler);
    m_trap_handler_register(LOAD_PAGE_FAULT, default_page_fault_handler);
    m_trap_handler_register(STORE_PAGE_FAULT, default_page_fault_handler);
    
    if (hartid >= NUM_CORES) {
        // Core not participating in test, go to sleep
        while (1) {
            riscv_wfi();
        }
    }
    
    if (hartid == 0) {
        atomic_printf("=== ST-STR-020: Initializing VM and switching to S-mode ===\n");
        
        // Start other cores
        for (int i = 1; i < NUM_CORES; i++) {
            switch_on_core(i);
        }
        
        // Initialize virtual memory management
        vm_init(0x84000000);
        
        // Map test regions for all cores
        atomic_printf("Mapping test regions for %d cores...\n", NUM_CORES);
        for (int core_id = 0; core_id < NUM_CORES; core_id++) {
            for (int page_idx = 0; page_idx < TEST_PAGES; page_idx++) {
                uintptr_t vaddr = get_test_vaddr(core_id, page_idx);
                uintptr_t paddr = get_test_paddr(core_id, page_idx);
                
                // Map virtual to physical with full permissions
                vm_map((void *)vaddr, (void *)paddr, PTE_R | PTE_W | PTE_X | PTE_A | PTE_D);
            }
        }
        atomic_printf("Virtual memory mapping completed\n");
    }
    
    // Enable virtual memory for this core
    vm_enable(0x84000000);
    
    // Wait for all cores to reach this point
    barrier(NUM_CORES);
    
    // Switch to supervisor mode and start the test
    atomic_printf("Core %lu: Switching to S-mode\n", hartid);
    switch_mode(hartid, MODE_S, (uint64_t)&s_main);
    
    return 0;
}