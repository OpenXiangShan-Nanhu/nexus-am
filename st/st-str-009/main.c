// ST-STR-009: 全路压力负载并发压力测试（含定时器中断）
#include <am.h>
#include <platform.h>
#include <klib.h>
#include <klib-macros.h>
#include <plic.h>
#include <riscv.h>
#include <csr.h>
#include "dw_axi_dmac.h"
#include "vmm.h"
#include "mtrap.h"
#include "dw_apb_timer.h"

#define NUM_CORES 4
#define DDR_BASE 0x80400000
#define DDR_SIZE 0x100000
#define MMIO_BASE 0x60020000
#define MMIO_SIZE 0x10000
#define MMIO_CPU1_BASE MMIO_BASE
#define MMIO_CPU1_SIZE 0x8000
#define MMIO_DMA_BASE (MMIO_BASE + 0x8000)
#define MMIO_DMA_SIZE 0x8000
#define DMA_SRC_BASE 0x80500000
#define DMA_DEST_BASE 0x80600000
#define DMA_TRANSFER_SIZE 1024
#define MMIO_CONFIG_SIZE 1024
#define VALUE_BASE 0x12345678
#define TIMER_INTR_SOURCE_BASE 244

volatile uint32_t *ddr_ptr = (volatile uint32_t*)DDR_BASE;
volatile uint32_t *mmio_ptr = (volatile uint32_t*)MMIO_BASE;
volatile uint32_t *dma_src_ptr = (volatile uint32_t*)DMA_SRC_BASE;
volatile uint32_t *dma_dest_ptr = (volatile uint32_t*)DMA_DEST_BASE;
volatile uint64_t dma_completion_count = 0;
volatile uint64_t mmio_config_count = 0;
volatile uint64_t timer_intr_count = 0;
volatile uint64_t dma_verify_errors = 0;
volatile uint64_t mmio_verify_errors = 0;
volatile uint64_t s_mode_hartid[NUM_CORES] = {0};

extern uint64_t atomic_add();

void timer_intr_handler() {
    uint32_t intr = READ_U32(CTX_COMP_REG(0));
    timer_irq_handler(intr - TIMER_INTR_SOURCE_BASE);
    WRITE_U32(CTX_COMP_REG(0), intr);
    timer_intr_count++;
}

void enable_external_intr() {
    uint64_t mie = csr_read(mie);
    csr_write(mie, mie | MEIE);
    uint64_t mstatus = csr_read(mstatus);
    csr_write(mstatus, mstatus | (0x1UL << 3));
}

void dma_master_a_callback(int error_code, uint64_t user_data) {
    if (error_code == 0) {
        atomic_add(&dma_completion_count, 1);
        
        // 验证DMA传输的数据正确性
        uint32_t expected_value = VALUE_BASE + user_data;
        volatile uint32_t *actual_dest = (volatile uint32_t*)((uintptr_t)dma_dest_ptr + (user_data * DMA_TRANSFER_SIZE));
        
        for(int i = 0; i < DMA_TRANSFER_SIZE / sizeof(uint32_t); i++) {
            if(*(actual_dest + i) != expected_value + i) {
                atomic_add(&dma_verify_errors, 1);
                s_atomic_printf("Master A DMA verification failed at transfer %lu, index %d: expected 0x%x, got 0x%x\n", 
                              user_data, i, expected_value + i, *(actual_dest + i));
                break;
            }
        }
    } else {
        s_atomic_printf("Master A DMA transfer %lu failed with error %d\n", user_data, error_code);
    }
}

void dma_master_b_callback(int error_code, uint64_t user_data) {
    if (error_code == 0) {
        atomic_add(&mmio_config_count, 1);
        
        // 验证MMIO传输的数据正确性
        uintptr_t mmio_dest_addr = MMIO_DMA_BASE + (user_data * MMIO_CONFIG_SIZE);
        if(mmio_dest_addr + MMIO_CONFIG_SIZE > MMIO_DMA_BASE + MMIO_DMA_SIZE) {
            uint32_t max_slots = MMIO_DMA_SIZE / MMIO_CONFIG_SIZE;
            mmio_dest_addr = MMIO_DMA_BASE + ((user_data % max_slots) * MMIO_CONFIG_SIZE);
        }
        
        volatile uint32_t *mmio_dest = (volatile uint32_t*)mmio_dest_addr;
        uint32_t expected_value = (VALUE_BASE + user_data) | 0xC0DE0000;
        
        for(int i = 0; i < MMIO_CONFIG_SIZE / sizeof(uint32_t); i++) {
            if(*(mmio_dest + i) != expected_value + i) {
                atomic_add(&mmio_verify_errors, 1);
                s_atomic_printf("Master B MMIO verification failed at transfer %lu, index %d: expected 0x%x, got 0x%x\n", 
                              user_data, i, expected_value + i, *(mmio_dest + i));
                break;
            }
        }
    } else {
        s_atomic_printf("Master B MMIO transfer %lu failed with error %d\n", user_data, error_code);
    }
}

void task0(int hartid) {
    // 写入数据
    for(int i = 0; i < 1; i++) {
        for(int j = 0; j < 128; j++) {
            *(ddr_ptr + j) = VALUE_BASE + i + j;
        }
        riscv_fence();
    }
    // 检查数据
    int errors = 0;
    for(int j = 0; j < 128; j++) {
        uint32_t expected = VALUE_BASE + j;
        uint32_t actual = *(ddr_ptr + j);
        if(actual != expected) {
            errors++;
            s_atomic_printf("DDR verify failed at %d: expected 0x%x, got 0x%x\n", j, expected, actual);
        }
    }
    if(errors == 0) {
        s_atomic_printf("DDR region verification passed!\n");
    } else {
        s_atomic_printf("DDR region verification failed, errors: %d\n", errors);
    }
}

void task1(int hartid) {
    volatile uint32_t *mmio_test_base = (volatile uint32_t*)MMIO_CPU1_BASE;
    // 写入数据
    for(int i = 0; i < 1; i++) {
        for(int j = 0; j < MMIO_CPU1_SIZE / sizeof(uint32_t); j += 256) {
            *(mmio_test_base + j) = VALUE_BASE + i + j;
        }
        riscv_fence();
    }
    // 检查数据
    int errors = 0;
    for(int j = 0; j < MMIO_CPU1_SIZE / sizeof(uint32_t); j += 256) {
        uint32_t expected = VALUE_BASE + j;
        uint32_t actual = *(mmio_test_base + j);
        if(actual != expected) {
            errors++;
            s_atomic_printf("MMIO_CPU1 verify failed at %d: expected 0x%x, got 0x%x\n", j, expected, actual);
        }
    }
    if(errors == 0) {
        s_atomic_printf("MMIO_CPU1 region verification passed!\n");
    } else {
        s_atomic_printf("MMIO_CPU1 region verification failed, errors: %d\n", errors);
    }
}

void task2(int hartid) {
    // 等待所有DMA传输完成
    while(dma_completion_count < 4) {
        // DMA中断会更新计数，无需延迟等待
    }
    s_atomic_printf("CPU%d: All DMA transfers completed, count: %lu, timer_intrs: %lu\n", 
                   hartid, dma_completion_count, timer_intr_count);
}

void task3(int hartid) {
    // 等待所有MMIO配置完成
    while(mmio_config_count < 4) {
        // DMA中断会更新计数，无需延迟等待
    }
    s_atomic_printf("CPU%d: All MMIO transfers completed, count: %lu, timer_intrs: %lu\n", 
                   hartid, mmio_config_count, timer_intr_count);
}

void s_main(){
    int hartid;
    asm volatile("mv %0, a0" : "=r"(hartid));
    s_mode_hartid[hartid] = hartid;
    switch(hartid) {
        case 0: task0(hartid); break;
        case 1: task1(hartid); break;
        case 2: task2(hartid); break;
        case 3: task3(hartid); break;
    }
    s_barrier(NUM_CORES, hartid);
    if(hartid == 0) {
        s_atomic_printf("=== Final Statistics ===\n");
        s_atomic_printf("DMA Master A completions: %lu\n", dma_completion_count);
        s_atomic_printf("DMA Master B completions: %lu\n", mmio_config_count);
        s_atomic_printf("Timer interrupts: %lu\n", timer_intr_count);
        s_atomic_printf("DMA verification errors: %lu\n", dma_verify_errors);
        s_atomic_printf("MMIO verification errors: %lu\n", mmio_verify_errors);
        
        if(dma_verify_errors == 0 && mmio_verify_errors == 0) {
            s_atomic_printf("All data verified successfully!\n");
        } else {
            s_atomic_printf("Data verification FAILED!\n");
        }
    }
    _halt(0);
}

int main() {
    int hartid = riscv_mhartid();
    dma_init();
    if(hartid == 0){
        enable_external_intr();
        timer_init(0);
        timer_set_count(0, 0x1000);
        if(setup_intr(TIMER_INTR_SOURCE_BASE, 7)) {
            atomic_printf("Setup timer interrupt failed\n");
        }
        if(enable_intr(0, TIMER_INTR_SOURCE_BASE)) {
            atomic_printf("Enable timer interrupt failed\n");
        }
        vm_init(0x84000000);
        vm_map((void *)ddr_ptr, (void *)ddr_ptr, PTE_R | PTE_W | PTE_X | PTE_A | PTE_D);
        
        // 映射 MMIO_CPU1 区域 (32KB = 8 pages)
        uint32_t mmio_cpu1_pages = MMIO_CPU1_SIZE / 0x1000;
        for(uint32_t i = 0; i < mmio_cpu1_pages; i++) {
            void *mmio_cpu1_page = (void*)((uintptr_t)MMIO_CPU1_BASE + (i * 0x1000));
            vm_map(mmio_cpu1_page, mmio_cpu1_page, PTE_R | PTE_W | PTE_X | PTE_A | PTE_D);
        }
        
        // 映射 MMIO_DMA 区域 (32KB = 8 pages)
        uint32_t mmio_dma_pages = MMIO_DMA_SIZE / 0x1000;
        for(uint32_t i = 0; i < mmio_dma_pages; i++) {
            void *mmio_dma_page = (void*)((uintptr_t)MMIO_DMA_BASE + (i * 0x1000));
            vm_map(mmio_dma_page, mmio_dma_page, PTE_R | PTE_W | PTE_X | PTE_A | PTE_D);
        }
        
        vm_map((void *)dma_src_ptr, (void *)dma_src_ptr, PTE_R | PTE_W | PTE_X | PTE_A | PTE_D);
        vm_map((void *)(DMA_SRC_BASE + 0x10000), (void *)(DMA_SRC_BASE + 0x10000), PTE_R | PTE_W | PTE_X | PTE_A | PTE_D);
        
        // 映射DMA目标区域，支持4次传输
        uint32_t total_dma_size = 4 * DMA_TRANSFER_SIZE;
        uint32_t num_pages = ((total_dma_size + 0xFFF) >> 12) + 1;
        for(uint32_t i = 0; i < num_pages; i++) {
            void *dest_page = (void*)((uintptr_t)dma_dest_ptr + (i * 0x1000));
            vm_map(dest_page, dest_page, PTE_R | PTE_W | PTE_X | PTE_A | PTE_D);
        }
        
        for(int i = 0; i < 4 * DMA_TRANSFER_SIZE / sizeof(uint32_t); i++) {
            *(dma_dest_ptr + i) = 0;
        }
    }
    barrier(NUM_CORES);
    if(hartid == 2) {
        for(int iter = 0; iter < 4; iter++) {
            for(int i = 0; i < DMA_TRANSFER_SIZE / sizeof(uint32_t); i++) {
                *(dma_src_ptr + i) = VALUE_BASE + iter + i;
            }
            riscv_fence();
            uintptr_t dest_addr = (uintptr_t)dma_dest_ptr + (iter * DMA_TRANSFER_SIZE);
            dma_transfer(DMA_MEM_TO_MEM, (uintptr_t)dma_src_ptr, dest_addr, DMA_TRANSFER_SIZE, DWAXIDMAC_AX_CACHE_CACHEABLE, DWAXIDMAC_AX_CACHE_CACHEABLE, dma_master_a_callback, iter);
        }
    }
    if(hartid == 3) {
        for(int iter = 0; iter < 4; iter++) {
            volatile uint32_t *config_src = (volatile uint32_t*)(DMA_SRC_BASE + 0x10000);
            uintptr_t mmio_dest_addr = MMIO_DMA_BASE + (iter * MMIO_CONFIG_SIZE);
            if(mmio_dest_addr + MMIO_CONFIG_SIZE > MMIO_DMA_BASE + MMIO_DMA_SIZE) {
                uint32_t max_slots = MMIO_DMA_SIZE / MMIO_CONFIG_SIZE;
                mmio_dest_addr = MMIO_DMA_BASE + ((iter % max_slots) * MMIO_CONFIG_SIZE);
            }
            for(int i = 0; i < MMIO_CONFIG_SIZE / sizeof(uint32_t); i++) {
                *(config_src + i) = (VALUE_BASE + iter + i) | 0xC0DE0000;
            }
            riscv_fence();
            dma_transfer(DMA_MEM_TO_DEV, (uintptr_t)config_src, mmio_dest_addr, MMIO_CONFIG_SIZE, DWAXIDMAC_AX_CACHE_CACHEABLE, DWAXIDMAC_AX_CACHE_DEVICE, dma_master_b_callback, iter);
        }
    }
    barrier(NUM_CORES);
    vm_enable(0x84000000);
    m_switch_mode(hartid, MODE_S, (uint64_t)&s_main);
    return 0;
}
