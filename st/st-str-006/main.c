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

#define DEST0_ADDRESS       0x80301000
#define DEST1_ADDRESS       0x80302000

#define MEM_BASE            0x80300000
#define NC_BASE             0xe0000000
#define MMIO_BASE           0x60020000
#define PCIE_BASE           0x30000000000

#define PCIE_NC_BASE        0x30000000000     // PCIE + PBMT_NC
#define PCIE_IO_BASE        0x30000100000     // PCIE + PBMT_IO  
#define PCIE_NORMAL_BASE    0x30000200000     // PCIE + No PBMT

#define TRANSFER_SIZE       2048    // Byte
#define MMIO_TRANSFER_SIZE  1024    // Byte
#define PCIE_TRANSFER_SIZE  1024    // Byte

#define VALUE_INITIAL       0xCAFEBABE

volatile uint32_t *mem_ptr = (volatile uint32_t*)MEM_BASE;
volatile uint32_t *nc_ptr = (volatile uint32_t*)NC_BASE;
volatile uint32_t *mmio_ptr = (volatile uint32_t*)MMIO_BASE;
volatile uint32_t *pcie_nc_ptr = (volatile uint32_t*)PCIE_NC_BASE;
volatile uint32_t *pcie_io_ptr = (volatile uint32_t*)PCIE_IO_BASE;
volatile uint32_t *pcie_normal_ptr = (volatile uint32_t*)PCIE_NORMAL_BASE;
volatile uint32_t *dest0_ptr = (volatile uint32_t*)DEST0_ADDRESS;
volatile uint32_t *dest1_ptr = (volatile uint32_t*)DEST1_ADDRESS;

void task0(int hartid) {
    s_atomic_printf("CPU%d: Starting memcpy task\n", hartid);
    
    for(int i = 0; i < TRANSFER_SIZE / sizeof(uint32_t); i++) {
        *(mem_ptr + i) = VALUE_INITIAL + i;
    }
    riscv_fence();
    
    for(int round = 0; round < 5; round++) {
        memcpy((void*)(dest0_ptr), (void*)(mem_ptr), TRANSFER_SIZE);
        riscv_fence();
        
        for(int i = 0; i < TRANSFER_SIZE / sizeof(uint32_t); i++) {
            if(*(dest0_ptr + i) != VALUE_INITIAL + i) {
                s_atomic_printf("CPU%d: memcpy verification failed at index %d\n", hartid, i);
                break;
            }
        }
        
        if(round % 5 == 0) {
            s_atomic_printf("CPU%d: memcpy round %d completed\n", hartid, round);
        }
    }
    s_atomic_printf("CPU%d: memcpy task completed\n", hartid);
}

void task1(int hartid) {
    s_atomic_printf("CPU%d: Starting PCIE PBMT_NC access test\n", hartid);
    
    for(int round = 0; round < 5; round++) {
        for(int i = 0; i < PCIE_TRANSFER_SIZE / sizeof(uint32_t); i++) {
            *(pcie_nc_ptr + i) = VALUE_INITIAL + i + round + 0x100;
        }
        riscv_fence();
        
        for(int i = 0; i < PCIE_TRANSFER_SIZE / sizeof(uint32_t); i++) {
            uint32_t read_val = *(pcie_nc_ptr + i);
            if(read_val != VALUE_INITIAL + i + round + 0x100) {
                s_atomic_printf("CPU%d: PCIE NC verification failed at index %d, expected 0x%x, got 0x%x\n", 
                              hartid, i, VALUE_INITIAL + i + round + 0x100, read_val);
                break;
            }
        }
        
        // if(round % 10 == 0) {
        //     s_atomic_printf("CPU%d: PCIE PBMT_NC round %d completed\n", hartid, round);
        // }
    }
    
    s_atomic_printf("CPU%d: Additional NC and Normal memory access\n", hartid);
    for(int round = 0; round < 5; round++) {
        // NC
        for(int i = 0; i < TRANSFER_SIZE / sizeof(uint32_t); i++) {
            *(nc_ptr + i) = VALUE_INITIAL + i + round + 0x200;
        }
        riscv_fence();
        
        // Normal
        for(int i = 0; i < TRANSFER_SIZE / sizeof(uint32_t); i++) {
            *(mem_ptr + i) = VALUE_INITIAL + i + round + 0x300;
        }
        riscv_fence();
        
        // if(round % 10 == 0) {
        //     s_atomic_printf("CPU%d: Additional access round %d completed\n", hartid, round);
        // }
    }
    
    s_atomic_printf("CPU%d: PCIE PBMT_NC task completed\n", hartid);
}

void task2(int hartid) {
    s_atomic_printf("CPU%d: Starting PCIE PBMT_IO and Normal access test\n", hartid);
    
    for(int round = 0; round < 5; round++) {
        for(int i = 0; i < PCIE_TRANSFER_SIZE / sizeof(uint32_t); i++) {
            *(pcie_io_ptr + i) = VALUE_INITIAL + i + round + 0x400;
        }
        riscv_fence();
        
        for(int i = 0; i < PCIE_TRANSFER_SIZE / sizeof(uint32_t); i++) {
            uint32_t read_val = *(pcie_io_ptr + i);
            if(read_val != VALUE_INITIAL + i + round + 0x400) {
                s_atomic_printf("CPU%d: PCIE IO verification failed at index %d, expected 0x%x, got 0x%x\n", 
                              hartid, i, VALUE_INITIAL + i + round + 0x400, read_val);
                break;
            }
        }
        
        for(int i = 0; i < PCIE_TRANSFER_SIZE / sizeof(uint32_t); i++) {
            *(pcie_normal_ptr + i) = VALUE_INITIAL + i + round + 0x500;
        }
        riscv_fence();
        
        for(int i = 0; i < PCIE_TRANSFER_SIZE / sizeof(uint32_t); i++) {
            uint32_t read_val = *(pcie_normal_ptr + i);
            if(read_val != VALUE_INITIAL + i + round + 0x500) {
                s_atomic_printf("CPU%d: PCIE Normal verification failed at index %d, expected 0x%x, got 0x%x\n", 
                              hartid, i, VALUE_INITIAL + i + round + 0x500, read_val);
                break;
            }
        }
        
        if(round % 10 == 0) {
            s_atomic_printf("CPU%d: PCIE IO/Normal round %d completed\n", hartid, round);
        }
    }
    
    s_atomic_printf("CPU%d: Performance comparison test\n", hartid);
    for(int round = 0; round < 5; round++) {
        uint32_t test_val = VALUE_INITIAL + round + 0x600;
        
        *(pcie_nc_ptr + round) = test_val;
        uint32_t nc_read = *(pcie_nc_ptr + round);
        
        *(pcie_io_ptr + round) = test_val;
        uint32_t io_read = *(pcie_io_ptr + round);
        
        *(pcie_normal_ptr + round) = test_val;
        uint32_t normal_read = *(pcie_normal_ptr + round);
        
        riscv_fence();
        
        if(nc_read != test_val || io_read != test_val || normal_read != test_val) {
            s_atomic_printf("CPU%d: Performance test failed at round %d\n", hartid, round);
        }
    }
    
    s_atomic_printf("CPU%d: PCIE IO/Normal task completed\n", hartid);
}

void task3(int hartid) {
    s_atomic_printf("CPU%d: Starting MMIO R/W task\n", hartid);
    
    for(int round = 0; round < 5; round++) {
        for(int i = 0; i < MMIO_TRANSFER_SIZE / sizeof(uint32_t); i++) {
            *(mmio_ptr + i) = VALUE_INITIAL + i + round + 0x700;
        }
        riscv_fence();
        
        for(int i = 0; i < MMIO_TRANSFER_SIZE / sizeof(uint32_t); i++) {
            uint32_t read_val = *(mmio_ptr + i);
            if(read_val != VALUE_INITIAL + i + round + 0x700) {
                s_atomic_printf("CPU%d: MMIO verification failed at index %d, expected 0x%x, got 0x%x\n", hartid, i, VALUE_INITIAL + i + round + 0x700, read_val);
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
    
    s_atomic_printf("CPU%d: Entering S-mode task\n", hartid);
    
    switch(hartid) {
        case 0:
            task0(hartid);  // CPU0: memcpy
            break;
        case 1:
            task1(hartid);  // CPU1: PCIE PBMT_NC
            break;
        case 2:
            task2(hartid);  // CPU2: PCIE PBMT_IO and Normal PCIE  
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
    
    // 
    svpbmt_enable();

    if(hartid == 0){
        s_atomic_printf("CPU%d: System initialization started\n", hartid);
        
        vm_init(0x84000000);
        
        vm_map((void *)nc_ptr, (void *)nc_ptr, PTE_PBMT_NC | PTE_R | PTE_W | PTE_X | PTE_A | PTE_D);
        vm_map((void *)mem_ptr, (void *)mem_ptr, PTE_R | PTE_W | PTE_X | PTE_A | PTE_D);
        vm_map((void *)mmio_ptr, (void *)mmio_ptr, PTE_PBMT_IO | PTE_R | PTE_W | PTE_X | PTE_A | PTE_D);

        s_atomic_printf("CPU%d: Mapping PCIE regions with different PBMT attributes\n", hartid);
        vm_map((void *)pcie_nc_ptr, (void *)pcie_nc_ptr, PTE_PBMT_NC | PTE_R | PTE_W | PTE_X | PTE_A | PTE_D);
        vm_map((void *)pcie_io_ptr, (void *)pcie_io_ptr, PTE_PBMT_IO | PTE_R | PTE_W | PTE_X | PTE_A | PTE_D);
        vm_map((void *)pcie_normal_ptr, (void *)pcie_normal_ptr, PTE_R | PTE_W | PTE_X | PTE_A | PTE_D);  
        
        vm_map((void *)dest0_ptr, (void *)dest0_ptr, PTE_R | PTE_W | PTE_X | PTE_A | PTE_D);
        vm_map((void *)dest1_ptr, (void *)dest1_ptr, PTE_R | PTE_W | PTE_X | PTE_A | PTE_D);
        
        for(int i = 0; i < TRANSFER_SIZE / sizeof(uint32_t); i++) {
            *(dest0_ptr + i) = 0;
            *(dest1_ptr + i) = 0;
        }
        
        s_atomic_printf("CPU%d: PCIE PBMT mapping completed - NC: 0x%lx, IO: 0x%lx, Normal: 0x%lx\n", 
                       hartid, (uint64_t)pcie_nc_ptr, (uint64_t)pcie_io_ptr, (uint64_t)pcie_normal_ptr);
        s_atomic_printf("CPU%d: System initialization completed\n", hartid);
    }

    barrier(NUM_CORES);

    vm_enable(0x84000000);

    m_switch_mode(hartid, MODE_S, (uint64_t)&s_main);

    return 0;
}
