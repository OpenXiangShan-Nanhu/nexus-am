#include <am.h>
#include <platform.h>
#include <klib.h>
#include <klib-macros.h>
#include <plic.h>
#include <xsextra.h>
#include <riscv.h>
#include <csr.h>
#include "dw_axi_dmac.h"
#include "vmm.h"
#include "mtrap.h"

#define NUM_CORES 4

#define LITERATION          0x1

#define DEST0_ADDRESS   0x80210000
#define DEST1_ADDRESS   0x80220000

#define MEM_BASE        0x80200000
#define NC_BASE         0xe0000000
#define MMIO_BASE       0x60020000
#define PCIE_BASE       0x30000000000

#define MMIO_SIZE           0xffff
#define TRANSFER_SIZE       1024    // Byte
#define MMIO_TRANSFER_SIZE  1024      // Byte
#define TRANSFER_ITEM_SIZE  32      // Byte

#define VALUE_INITIAL       0xDEADBEEF

volatile uint32_t *mem_ptr = (volatile uint32_t*)MEM_BASE;
volatile uint32_t *nc_ptr = (volatile uint32_t*)NC_BASE;
volatile uint32_t *mmio_ptr = (volatile uint32_t*)MMIO_BASE;
volatile uint32_t *pcie_ptr =  (volatile uint32_t*)PCIE_BASE;
volatile uint32_t *dest0_ptr = (volatile uint32_t*)DEST0_ADDRESS;
volatile uint32_t *dest1_ptr = (volatile uint32_t*)DEST1_ADDRESS;

// volatile uint32_t *dma_llp_base = (volatile uint32_t*)0x50070000 + COMMON_REG_LEN + CH_LLP + 0x4;

void task0(int hartid) {
    s_atomic_printf("CPU%d: Starting memcpy task\n", hartid);
    
    for(int i = 0; i < TRANSFER_SIZE / sizeof(uint32_t); i++) {
        *(mem_ptr + i) = VALUE_INITIAL + i;
    }
    // 强制刷新所有写缓冲区和cache
    asm volatile("fence w, w" ::: "memory");
    asm volatile("fence.i" ::: "memory");
    riscv_fence();
    
    for(int round = 0; round < 10; round++) {
        // 确保源数据已经写入内存
        asm volatile("fence w, rw" ::: "memory");
        
        memcpy((void*)(dest0_ptr), (void*)(mem_ptr), TRANSFER_SIZE);
        
        // 确保拷贝完成且对所有核心可见
        asm volatile("fence rw, rw" ::: "memory");
        asm volatile("fence.i" ::: "memory");
        riscv_fence();
        
        // 确保读取最新数据
        asm volatile("fence r, r" ::: "memory");
        
        for(int i = 0; i < TRANSFER_SIZE / sizeof(uint32_t); i++) {
            // 使用 volatile 强制从内存读取
            volatile uint32_t actual = *(dest0_ptr + i);
            uint32_t expected = VALUE_INITIAL + i;
            if(actual != expected) {
                s_atomic_printf("CPU%d: memcpy verification failed at index %d, addr 0x%lx, expected 0x%x, got 0x%x\n", 
                               hartid, i, (uint64_t)(dest0_ptr + i), expected, actual);
                break;
            }
        }
        s_atomic_printf("CPU%d: memcpy round %d completed\n", hartid, round);
    }
    s_atomic_printf("CPU%d: memcpy task completed\n", hartid);
}

void task1(int hartid) {
    s_atomic_printf("CPU%d: Starting NC memory R/W task\n", hartid);
    
    for(int round = 0; round < 10; round++) {
        for(int i = 0; i < TRANSFER_SIZE / sizeof(uint32_t); i++) {
            *(nc_ptr + i) = VALUE_INITIAL + i + round;
        }
        riscv_fence();
        
        for(int i = 0; i < TRANSFER_SIZE / sizeof(uint32_t); i++) {
            uint32_t read_val = *(nc_ptr + i);
            if(read_val != VALUE_INITIAL + i + round) {
                s_atomic_printf("CPU%d: NC memory verification failed at index %d, expected 0x%x, got 0x%x\n", hartid, i, VALUE_INITIAL + i + round, read_val);
                break;
            }
        }
        if(round % 10 == 0) {
            s_atomic_printf("CPU%d: NC memory R/W round %d completed\n", hartid, round);
        }
    }
    s_atomic_printf("CPU%d: NC memory R/W task completed\n", hartid);
}

void task2(int hartid) {
    s_atomic_printf("CPU%d: Starting memcpy task\n", hartid);

    for(int i = 0; i < TRANSFER_SIZE / sizeof(uint32_t); i++) {
        *(mem_ptr + i) = VALUE_INITIAL + i + 0x1000;
    }
    // 强制刷新所有写缓冲区和cache
    asm volatile("fence w, w" ::: "memory");
    asm volatile("fence.i" ::: "memory");
    riscv_fence();
    
    for(int round = 0; round < 10; round++) {
        // 确保源数据已经写入内存
        asm volatile("fence w, rw" ::: "memory");
        
        memcpy((void*)(dest1_ptr), (void*)(mem_ptr), TRANSFER_SIZE);
        
        // 确保拷贝完成且对所有核心可见
        asm volatile("fence rw, rw" ::: "memory");
        asm volatile("fence.i" ::: "memory");
        riscv_fence();
        
        // 确保读取最新数据
        asm volatile("fence r, r" ::: "memory");

        for(int i = 0; i < TRANSFER_SIZE / sizeof(uint32_t); i++) {
            // 使用 volatile 强制从内存读取
            volatile uint32_t actual = *(dest1_ptr + i);
            uint32_t expected = VALUE_INITIAL + i + 0x1000;
            if(actual != expected) {
                s_atomic_printf("CPU%d: memcpy verification failed at index %d\n, expected 0x%x, got 0x%x\n", 
                               hartid, i, expected, actual);
                break;
            }
        }
        s_atomic_printf("CPU%d: memcpy round %d completed\n", hartid, round);
    }
    s_atomic_printf("CPU%d: memcpy task completed\n", hartid);
}

void task3(int hartid) {
    s_atomic_printf("CPU%d: Starting MMIO R/W task\n", hartid);
    
    for(int round = 0; round < 10; round++) {
        for(int i = 0; i < MMIO_TRANSFER_SIZE / sizeof(uint32_t); i++) {
            *(mmio_ptr + i) = VALUE_INITIAL + i + round;
        }
        riscv_fence();
        
        for(int i = 0; i < MMIO_TRANSFER_SIZE / sizeof(uint32_t); i++) {
            uint32_t read_val = *(mmio_ptr + i);
            if(read_val != VALUE_INITIAL + i + round) {
                s_atomic_printf("CPU%d: MMIO verification failed at index %d, expected 0x%x, got 0x%x\n", hartid, i, VALUE_INITIAL + i + round, read_val);
                break;
            }
        }
        
        s_atomic_printf("CPU%d: MMIO R/W round %d completed\n", hartid, round);
        
    }
    s_atomic_printf("CPU%d: MMIO R/W task completed\n", hartid);
}

void s_main(){
    int hartid;
    asm volatile("mv %0, a0" : "=r"(hartid));
    
    s_barrier(NUM_CORES, hartid);

    switch(hartid) {
        case 0:
            task0(hartid);  // CPU0: memcpy
            break;
        case 1:
            task1(hartid);  // CPU1: NC memory
            break;
        case 2:
            task2(hartid);  // CPU2: memcpy
            break;
        case 3:
            task3(hartid);  // CPU3: MMIO
            break;
        default:
            s_atomic_printf("CPU%d: No task assigned\n", hartid);
            break;
    }
    
    s_atomic_printf("CPU%d: Task completed, halting\n", hartid);

    s_barrier(NUM_CORES, hartid);
    _halt(0);
}

void svpbmt_enable(){
    uint64_t menvcfgVal;
    asm volatile("csrr %0, 0x30a" : "=r"(menvcfgVal));
    menvcfgVal |= (1ULL << 62);
    asm volatile("csrw 0x30a, %0" : : "r"(menvcfgVal));
}

int main() {
    int hartid = riscv_mhartid();
    
    // dma_init();

    svpbmt_enable();

    if(hartid == 0){
        vm_init(0x84000000);
        vm_map((void *)nc_ptr, (void *)nc_ptr, PTE_PBMT_NC | PTE_R | PTE_W | PTE_X | PTE_A | PTE_D);
        vm_map((void *)mem_ptr, (void *)mem_ptr, PTE_R | PTE_W | PTE_X | PTE_A | PTE_D);
        vm_map((void *)mmio_ptr, (void *)mmio_ptr, PTE_R | PTE_W | PTE_X | PTE_A | PTE_D);
        vm_map((void *)dest0_ptr, (void *)dest0_ptr, PTE_R | PTE_W | PTE_X | PTE_A | PTE_D);
        vm_map((void *)dest1_ptr, (void *)dest1_ptr, PTE_R | PTE_W | PTE_X | PTE_A | PTE_D);
        // vm_map((void *)mmio_ptr, (void *)mmio_ptr, PTE_PBMT_IO | PTE_R | PTE_W | PTE_X | PTE_A | PTE_D);
        
        // 初始化所有目标内存区域
        for(int i = 0; i < TRANSFER_SIZE / sizeof(uint32_t); i++) {
            *(dest0_ptr + i) = 0;
            *(dest1_ptr + i) = 0;
        }
    }
    
    barrier(NUM_CORES);

    vm_enable(0x84000000);

    m_switch_mode(hartid, MODE_S, (uint64_t)&s_main);

    return 0;
}
