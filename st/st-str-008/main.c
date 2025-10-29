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

// 内存地址定义
#define DDR_BASE            0x80400000
#define DDR_SIZE            0x100000      // 1MB
#define MMIO_BASE           0x60020000
#define MMIO_SIZE           0x10000       // 64KB (60020000-6002ffff)

// MMIO区域分割：避免task1和task3冲突
#define MMIO_CPU1_BASE      MMIO_BASE           // 前32KB给CPU1读写检测
#define MMIO_CPU1_SIZE      0x8000              // 32KB
#define MMIO_DMA_BASE       (MMIO_BASE + 0x8000) // 后32KB给DMA传输  
#define MMIO_DMA_SIZE       0x8000              // 32KB

// DMA传输地址
#define DMA_SRC_BASE        0x80500000
#define DMA_DEST_BASE       0x80600000
#define DMA_TRANSFER_SIZE   1024          // 4KB

// PCIE不同属性测试区域 (添加的新定义)
#define PCIE_BASE           0x30000000000
#define PCIE_NC_BASE        0x30000000000     // PCIE + PBMT_NC
#define PCIE_IO_BASE        0x30000100000     // PCIE + PBMT_IO  
#define PCIE_NORMAL_BASE    0x30000200000     // PCIE + 无PBMT
#define PCIE_TRANSFER_SIZE  1024              // PCIE传输大小

// 计算和轮询相关
#define COMPUTE_ITERATIONS  10
#define POLLING_ITERATIONS  5
#define MMIO_CONFIG_SIZE    1024  // 每次配置1KB
#define PCIE_TEST_ROUNDS    5    // PCIE测试轮数

#define VALUE_BASE          0x12345678

// 全局变量
volatile uint32_t *ddr_ptr = (volatile uint32_t*)DDR_BASE;
volatile uint32_t *mmio_ptr = (volatile uint32_t*)MMIO_BASE;
volatile uint32_t *dma_src_ptr = (volatile uint32_t*)DMA_SRC_BASE;
volatile uint32_t *dma_dest_ptr = (volatile uint32_t*)DMA_DEST_BASE;

// PCIE区域指针 (新增)
volatile uint32_t *pcie_nc_ptr = (volatile uint32_t*)PCIE_NC_BASE;
volatile uint32_t *pcie_io_ptr = (volatile uint32_t*)PCIE_IO_BASE;
volatile uint32_t *pcie_normal_ptr = (volatile uint32_t*)PCIE_NORMAL_BASE;

volatile uint64_t dma_completion_count = 0;
volatile uint64_t mmio_config_count = 0;
volatile uint64_t pcie_access_count = 0;  // 新增PCIE访问计数

// S 模式下的 hartid 存储（避免访问 mhartid CSR）
volatile uint64_t s_mode_hartid[NUM_CORES] = {0};

extern uint64_t atomic_add();

void dma_master_a_callback(int error_code, uint64_t user_data) {
    if (error_code == 0) {
        atomic_add(&dma_completion_count, 1);
        s_atomic_printf("Master A PCIE DMA transfer %lu completed successfully\n", user_data);
        
        // 验证 PCIE DMA 传输：源数据在 DMA_SRC_BASE + 0x30000，目标在 PCIE_NC_BASE
        uint32_t expected_value = VALUE_BASE + user_data;
        uint32_t verification_errors = 0;
        
        // 验证 PCIE NC 区域的数据
        for(int i = 0; i < PCIE_TRANSFER_SIZE / sizeof(uint32_t); i++) {
            uint32_t expected = expected_value + i;
            uint32_t actual = *(pcie_nc_ptr + i);
            if(actual != expected) {
                verification_errors++;
                if(verification_errors <= 5) {
                    s_atomic_printf("Master A PCIE DMA verification failed at index %d, expected 0x%x, got 0x%x\n", 
                                  i, expected, actual);
                }
            }
        }
        
        if(verification_errors == 0) {
            s_atomic_printf("Master A PCIE DMA: All %d words verified successfully\n", 
                          PCIE_TRANSFER_SIZE / sizeof(uint32_t));
        } else {
            s_atomic_printf("Master A PCIE DMA: %d verification errors found\n", verification_errors);
        }
    } else {
        s_atomic_printf("Master A PCIE DMA transfer %lu failed with error %d\n", user_data, error_code);
    }
}

void dma_master_b_callback(int error_code, uint64_t user_data) {
    if (error_code == 0) {
        atomic_add(&mmio_config_count, 1);
        s_atomic_printf("Master B MMIO config %lu completed successfully\n", user_data);
        
        // 验证MMIO传输的正确性 - 计算对应的MMIO目标地址
        uintptr_t mmio_dest_addr = MMIO_DMA_BASE + (user_data * MMIO_CONFIG_SIZE);
        
        // 确保地址计算与task3中的逻辑一致
        if(mmio_dest_addr + MMIO_CONFIG_SIZE > MMIO_DMA_BASE + MMIO_DMA_SIZE) {
            uint32_t max_slots = MMIO_DMA_SIZE / MMIO_CONFIG_SIZE;
            mmio_dest_addr = MMIO_DMA_BASE + ((user_data % max_slots) * MMIO_CONFIG_SIZE);
        }
        
        volatile uint32_t *mmio_target = (volatile uint32_t*)mmio_dest_addr;
        uint32_t verification_errors = 0;
        
        // 验证传输到MMIO的数据
        for(int i = 0; i < MMIO_CONFIG_SIZE / sizeof(uint32_t); i++) {
            uint32_t expected_value = (VALUE_BASE + user_data + i) | 0xC0DE0000;
            uint32_t actual_value = *(mmio_target + i);
            
            if(actual_value != expected_value) {
                verification_errors++;
                if(verification_errors <= 3) {  // 只打印前3个错误
                    s_atomic_printf("Master B MMIO verification failed at offset %d, expected 0x%x, got 0x%x\n", 
                                  i, expected_value, actual_value);
                }
            }
        }
        
        if(verification_errors == 0) {
            s_atomic_printf("Master B MMIO config %lu: All %d words verified successfully at 0x%lx\n", 
                          user_data, MMIO_CONFIG_SIZE / sizeof(uint32_t), mmio_dest_addr);
        } else {
            s_atomic_printf("Master B MMIO config %lu: %d verification errors found\n", 
                          user_data, verification_errors);
        }
        
    } else {
        s_atomic_printf("Master B MMIO config %lu failed with error %d\n", user_data, error_code);
    }
}

void task0(int hartid) {
    s_atomic_printf("CPU%d: Starting DDR computation task\n", hartid);
    
    uint32_t max_elements = DDR_SIZE / sizeof(uint32_t);
    uint32_t total_errors = 0;
    
    for(int iter = 0; iter < COMPUTE_ITERATIONS; iter++) {
        // 确保不超出DDR边界，每次使用固定大小的块
        uint32_t block_size = 128;  // 64个数据 + 2个结果 + 一些缓冲
        uint32_t base_addr = (iter * block_size) % (max_elements - block_size);
        
        // 先计算期望的 sum 值
        uint64_t expected_sum = 0;
        
        for(int i = 0; i < 64; i++) { 
            // 避免整数溢出：使用较小的乘数或改为位操作
            uint32_t computed_value = (VALUE_BASE + iter) ^ (i * 0x5A5A);
            *(ddr_ptr + base_addr + i) = computed_value;
            expected_sum += computed_value;
        }
        riscv_fence();
        
        // 读取并进行累加计算
        uint64_t sum = 0;
        for(int i = 0; i < 64; i++) {
            sum += *(ddr_ptr + base_addr + i);
        }
        
        // 将计算结果写回DDR
        *(ddr_ptr + base_addr + 64) = (uint32_t)(sum & 0xFFFFFFFF);
        *(ddr_ptr + base_addr + 65) = (uint32_t)(sum >> 32);
        
        riscv_fence();
        
        // 验证计算结果
        uint32_t actual_low = *(ddr_ptr + base_addr + 64);
        uint32_t actual_high = *(ddr_ptr + base_addr + 65);
        uint64_t actual_sum = ((uint64_t)actual_high << 32) | actual_low;
        
        if(actual_sum != expected_sum) {
            total_errors++;
            s_atomic_printf("CPU%d: Computation verification failed at iter %d: expected 0x%lx, got 0x%lx\n", 
                          hartid, iter, expected_sum, actual_sum);
        }
        
        if(iter % 10 == 0) {
            s_atomic_printf("CPU%d: Computation iteration %d completed, sum=0x%lx (verified)\n", hartid, iter, sum);
        }
    }
    
    if(total_errors == 0) {
        s_atomic_printf("CPU%d: DDR computation task completed - All %d iterations verified successfully\n", 
                      hartid, COMPUTE_ITERATIONS);
    } else {
        s_atomic_printf("CPU%d: DDR computation task completed - %d verification errors found\n", 
                      hartid, total_errors);
    }
}

void task1(int hartid) {
    // s_atomic_printf("CPU%d: Starting MMIO read/write detection task (0x%lx-0x%lx)\n", hartid, MMIO_CPU1_BASE, MMIO_CPU1_BASE + MMIO_CPU1_SIZE - 1);
    
    // 限制在MMIO区域前32KB：0x60020000-0x60027fff
    // 必须使用 volatile 防止编译器优化，并确保 32 位访问
    volatile uint32_t *mmio_test_base = (volatile uint32_t*)MMIO_CPU1_BASE;
    uint32_t mmio_test_size = MMIO_CPU1_SIZE / sizeof(uint32_t);  // 以32位字为单位
    
    for(int iter = 0; iter < POLLING_ITERATIONS; iter++) {
        // s_atomic_printf("CPU%d: MMIO detection iteration %d\n", hartid, iter);
        
        // 写入测试模式
        uint32_t test_pattern = VALUE_BASE + iter;
        uint32_t write_count = 0;
        uint32_t read_count = 0;
        uint32_t error_count = 0;
        
        // 遍历CPU1专用的MMIO区域进行读写检测
        for(uint32_t offset = 0; offset < mmio_test_size; offset += 256) {  // 每次跳跃256个字(1KB)
            // 写入测试数据
            uint32_t write_value = test_pattern + offset;
            *(mmio_test_base + offset) = write_value;
            write_count++;
            
            riscv_fence();
            
            // 读取并验证
            uint32_t read_value = *(mmio_test_base + offset);
            read_count++;
            
            // 检测读写是否一致
            if(read_value != write_value) {
                error_count++;
                if(error_count <= 5) {  // 只打印前5个错误
                    s_atomic_printf("CPU%d: MMIO mismatch at addr 0x%lx, wrote 0x%x, read 0x%x\n", 
                                  hartid, MMIO_CPU1_BASE + offset * 4, write_value, read_value);
                }
            }
            
            // 写入反向模式进行进一步检测
            uint32_t inv_value = ~write_value;
            *(mmio_test_base + offset) = inv_value;
            riscv_fence();
            
            uint32_t inv_read = *(mmio_test_base + offset);
            if(inv_read != inv_value) {
                error_count++;
                if(error_count <= 5) {
                    s_atomic_printf("CPU%d: MMIO inverse mismatch at addr 0x%lx, wrote 0x%x, read 0x%x\n", 
                                  hartid, MMIO_CPU1_BASE + offset * 4, inv_value, inv_read);
                }
            }
        }
        
        // s_atomic_printf("CPU%d: MMIO iteration %d - writes: %d, reads: %d, errors: %d\n", 
        //               hartid, iter, write_count * 2, read_count * 2, error_count);
        
        riscv_fence();
    }
    
    // s_atomic_printf("CPU%d: MMIO read/write detection task completed\n", hartid);
}

void task2(int hartid) {
    // CPU2 -> PCIE 并发访问测试（CPU 直接访问，DMA 已在 M 模式发起）
    // s_atomic_printf("CPU%d: Starting PCIE concurrent access test (NC/IO/Normal)\n", hartid);
    
    for(int round = 0; round < PCIE_TEST_ROUNDS; round++) {
        // s_atomic_printf("CPU%d: PCIE test round %d\n", hartid, round);
        
        // PCIE NC 区域测试 - 确保每次写入都是独立的 32 位操作
        uint32_t nc_test_value = VALUE_BASE + round + 0x1000;
        for(int i = 0; i < PCIE_TRANSFER_SIZE / sizeof(uint32_t); i++) {
            uint32_t write_val = nc_test_value + i;
            *(pcie_nc_ptr + i) = write_val;
            // 添加 fence 确保每次写入独立完成
            if((i & 0x3) == 3) riscv_fence();
        }
        riscv_fence();
        
        uint32_t nc_errors = 0;
        for(int i = 0; i < PCIE_TRANSFER_SIZE / sizeof(uint32_t); i++) {
            uint32_t expected = nc_test_value + i;
            uint32_t actual = *(pcie_nc_ptr + i);
            if(actual != expected) {
                nc_errors++;
                if(nc_errors <= 3) {
                    s_atomic_printf("CPU%d: PCIE NC verify failed at %d, expected 0x%x, got 0x%x\n", 
                                  hartid, i, expected, actual);
                }
            }
        }
        
        // PCIE IO 区域测试
        uint32_t io_test_value = VALUE_BASE + round + 0x2000;
        for(int i = 0; i < PCIE_TRANSFER_SIZE / sizeof(uint32_t); i++) {
            uint32_t write_val = io_test_value + i;
            *(pcie_io_ptr + i) = write_val;
            if((i & 0x3) == 3) riscv_fence();
        }
        riscv_fence();
        
        uint32_t io_errors = 0;
        for(int i = 0; i < PCIE_TRANSFER_SIZE / sizeof(uint32_t); i++) {
            uint32_t expected = io_test_value + i;
            uint32_t actual = *(pcie_io_ptr + i);
            if(actual != expected) {
                io_errors++;
                if(io_errors <= 3) {
                    s_atomic_printf("CPU%d: PCIE IO verify failed at %d, expected 0x%x, got 0x%x\n", 
                                  hartid, i, expected, actual);
                }
            }
        }
        
        // PCIE Normal 区域测试
        uint32_t normal_test_value = VALUE_BASE + round + 0x3000;
        for(int i = 0; i < PCIE_TRANSFER_SIZE / sizeof(uint32_t); i++) {
            uint32_t write_val = normal_test_value + i;
            *(pcie_normal_ptr + i) = write_val;
            if((i & 0x3) == 3) riscv_fence();
        }
        riscv_fence();
        
        uint32_t normal_errors = 0;
        for(int i = 0; i < PCIE_TRANSFER_SIZE / sizeof(uint32_t); i++) {
            uint32_t expected = normal_test_value + i;
            uint32_t actual = *(pcie_normal_ptr + i);
            if(actual != expected) {
                normal_errors++;
                if(normal_errors <= 3) {
                    s_atomic_printf("CPU%d: PCIE Normal verify failed at %d, expected 0x%x, got 0x%x\n", 
                                  hartid, i, expected, actual);
                }
            }
        }
        
        // 跨区域测试
        for(int j = 0; j < 10; j++) {
            uint32_t cross_value = VALUE_BASE + round + 0x4000 + j;
            *(pcie_nc_ptr + j) = cross_value;
            *(pcie_io_ptr + j) = cross_value + 0x100;
            *(pcie_normal_ptr + j) = cross_value + 0x200;
            riscv_fence();
            
            uint32_t nc_read = *(pcie_nc_ptr + j);
            uint32_t io_read = *(pcie_io_ptr + j);
            uint32_t normal_read = *(pcie_normal_ptr + j);
            
            if(nc_read != cross_value || io_read != (cross_value + 0x100) || normal_read != (cross_value + 0x200)) {
                s_atomic_printf("CPU%d: Cross-region test failed at %d: NC=0x%x, IO=0x%x, Normal=0x%x\n", 
                              hartid, j, nc_read, io_read, normal_read);
            }
        }
        
        atomic_add(&pcie_access_count, 1);
        
        // if(round % 5 == 0) {
        //     s_atomic_printf("CPU%d: PCIE round %d completed - NC errors: %d, IO errors: %d, Normal errors: %d\n", 
        //                   hartid, round, nc_errors, io_errors, normal_errors);
        // }
    }
    
}

void task3(int hartid) {
    // CPU3 -> 监控 Master B MMIO 配置传输完成情况
    s_atomic_printf("CPU%d: Monitoring Master B MMIO config completions\n", hartid);
    
    uint64_t last_count = 0;
    for(int iter = 0; iter < 20; iter++) {
        
        uint64_t current_count = mmio_config_count;
        if(current_count != last_count) {
            s_atomic_printf("CPU%d: Master B MMIO completions: %lu\n", hartid, current_count);
            last_count = current_count;
        }
        
        if(current_count >= 10) {
            break;  // 所有 10 次传输都完成了
        }
    }
    
    s_atomic_printf("CPU%d: Master B MMIO monitoring completed, total: %lu\n", hartid, mmio_config_count);
}

void s_main(){
    int hartid;
    asm volatile("mv %0, a0" : "=r"(hartid));
    
    // 保存 hartid 到全局变量供 DMA 驱动使用
    s_mode_hartid[hartid] = hartid;
    
    s_atomic_printf("CPU%d: Entering S-mode task\n", hartid);
    
    switch(hartid) {
        case 0:
            task0(hartid);
            break;
        case 1:
            task1(hartid);
            break;
        case 2:
            task2(hartid);
            break;
        case 3:
            task3(hartid);
            break;
        default:
            s_atomic_printf("CPU%d: No task assigned\n", hartid);
            break;
    }
    
    s_atomic_printf("CPU%d: Task completed, halting\n", hartid);

    s_barrier(NUM_CORES, hartid);
    
    // 打印最终统计信息
    if(hartid == 0) {
        s_atomic_printf("=== Final Statistics ===\n");
        s_atomic_printf("DMA Master A completions: %lu\n", dma_completion_count);
        s_atomic_printf("DMA Master B completions: %lu\n", mmio_config_count);
        s_atomic_printf("PCIE access rounds: %lu\n", pcie_access_count);
    }
    
    _halt(0);
}

void svpbmt_enable(){
    // 检查是否支持 Svpbmt 扩展
    // 0x30a = menvcfg，bit 62 = PBMTE (Page Based Memory Type Enable)
    // 注意：某些硬件可能不支持此 CSR，导致 illegal instruction
    uint64_t menvcfgVal;
    
    // 尝试读取 menvcfg，如果不支持会触发 illegal instruction
    asm volatile("csrr %0, 0x30a" : "=r"(menvcfgVal));
    menvcfgVal |= (1ULL << 62);
    asm volatile("csrw 0x30a, %0" : : "r"(menvcfgVal));
}

int main() {
    int hartid = riscv_mhartid();
    
    // 在 M-mode 下使能 PBMT（如果硬件支持）
    // 如果出现 illegal instruction，说明硬件不支持 Svpbmt
    svpbmt_enable();
    
    dma_init();

    if(hartid == 0){
        s_atomic_printf("CPU%d: System initialization started\n", hartid);
        
        vm_init(0x84000000);
        
        // 映射整个内核代码和数据区域（包括 DMA 驱动的全局变量）
        for(uintptr_t addr = 0x80000000; addr < 0x80100000; addr += 0x1000) {
            vm_map((void *)addr, (void *)addr, PTE_R | PTE_W | PTE_X | PTE_A | PTE_D);
        }
        
        // 映射DDR区域（需要映射整个DDR_SIZE，不只是单个页面）
        uint32_t ddr_pages = (DDR_SIZE + 0xFFF) >> 12;  // 1MB / 4KB = 256 页
        for(uint32_t i = 0; i < ddr_pages; i++) {
            void *ddr_page = (void*)((uintptr_t)ddr_ptr + (i * 0x1000));
            vm_map(ddr_page, ddr_page, PTE_R | PTE_W | PTE_X | PTE_A | PTE_D);
        }
        
        // 映射分离的MMIO区域 - 需要映射整个区域，而不是单个页面
        // MMIO_CPU1_BASE: 0x60020000 - 0x60027fff (32KB)
        uint32_t mmio_cpu1_pages = MMIO_CPU1_SIZE / 0x1000;  // 32KB / 4KB = 8 页
        for(uint32_t i = 0; i < mmio_cpu1_pages; i++) {
            void *mmio_cpu1_page = (void*)((uintptr_t)MMIO_CPU1_BASE + (i * 0x1000));
            vm_map(mmio_cpu1_page, mmio_cpu1_page, PTE_R | PTE_W | PTE_X | PTE_A | PTE_D);
        }
        
        // MMIO_DMA_BASE: 0x60028000 - 0x6002ffff (32KB)
        uint32_t mmio_dma_pages = MMIO_DMA_SIZE / 0x1000;  // 32KB / 4KB = 8 页
        for(uint32_t i = 0; i < mmio_dma_pages; i++) {
            void *mmio_dma_page = (void*)((uintptr_t)MMIO_DMA_BASE + (i * 0x1000));
            vm_map(mmio_dma_page, mmio_dma_page, PTE_R | PTE_W | PTE_X | PTE_A | PTE_D);
        }
        
        // 映射DMA源区域，包括：
        // - task2 使用的 DMA_SRC_BASE + 0x30000 (0x80530000)
        // - task3 使用的 DMA_SRC_BASE + 0x10000 (0x80510000)
        // 需要至少映射 0x80500000 - 0x80540000 (256KB)
        uint32_t dma_src_pages = 64;  // 映射 256KB / 4KB = 64 页
        for(uint32_t i = 0; i < dma_src_pages; i++) {
            void *dma_src_page = (void*)((uintptr_t)dma_src_ptr + (i * 0x1000));
            vm_map(dma_src_page, dma_src_page, PTE_R | PTE_W | PTE_X | PTE_A | PTE_D);
        }
        
        // 映射PCIE区域 (新增) - 使用不同的PBMT属性
        // 每个PCIE区域需要映射多个页面以容纳PCIE_TRANSFER_SIZE（1KB）
        uint32_t pcie_pages = (PCIE_TRANSFER_SIZE + 0xFFF) >> 12;  // 向上取整到页数
        if(pcie_pages == 0) pcie_pages = 1;  // 至少映射1页
        
        for(uint32_t i = 0; i < pcie_pages; i++) {
            void *pcie_nc_page = (void*)((uintptr_t)pcie_nc_ptr + (i * 0x1000));
            vm_map(pcie_nc_page, pcie_nc_page, PTE_PBMT_NC | PTE_R | PTE_W | PTE_X | PTE_A | PTE_D);
            
            void *pcie_io_page = (void*)((uintptr_t)pcie_io_ptr + (i * 0x1000));
            vm_map(pcie_io_page, pcie_io_page, PTE_PBMT_IO | PTE_R | PTE_W | PTE_X | PTE_A | PTE_D);
            
            void *pcie_normal_page = (void*)((uintptr_t)pcie_normal_ptr + (i * 0x1000));
            vm_map(pcie_normal_page, pcie_normal_page, PTE_R | PTE_W | PTE_X | PTE_A | PTE_D);
        }
        
        
        atomic_printf("CPU%d: DDR Base: 0x%lx, Size: 0x%x\n", hartid, (uint64_t)ddr_ptr, DDR_SIZE);
        atomic_printf("CPU%d: MMIO CPU1: 0x%lx-0x%lx, DMA: 0x%lx-0x%lx\n", hartid, 
                    MMIO_CPU1_BASE, MMIO_CPU1_BASE + MMIO_CPU1_SIZE - 1,
                    MMIO_DMA_BASE, MMIO_DMA_BASE + MMIO_DMA_SIZE - 1);
        atomic_printf("CPU%d: PCIE NC: 0x%lx, IO: 0x%lx, Normal: 0x%lx\n", hartid, 
                    (uint64_t)pcie_nc_ptr, (uint64_t)pcie_io_ptr, (uint64_t)pcie_normal_ptr);
        atomic_printf("CPU%d: DMA Src: 0x%lx, DMA Dest: 0x%lx\n", hartid, (uint64_t)dma_src_ptr, (uint64_t)dma_dest_ptr);
    }
    
    barrier(NUM_CORES);

    // 在 M 模式下发起所有 DMA 传输（避免在 S 模式下访问 mhartid CSR）
    if(hartid == 2) {
        // CPU2 负责发起 Master A 的 PCIE DMA 传输
        atomic_printf("CPU%d: Initiating Master A PCIE DMA transfer (M-mode)\n", hartid);
        
        volatile uint32_t *dma_src = (volatile uint32_t*)(DMA_SRC_BASE + 0x30000);
        for(int i = 0; i < PCIE_TRANSFER_SIZE / sizeof(uint32_t); i++) {
            *(dma_src + i) = VALUE_BASE + 0xA000 + i;
        }
        riscv_fence();
        
        int ret = dma_transfer(
            DMA_MEM_TO_DEV,
            (uintptr_t)dma_src,
            PCIE_NC_BASE,
            PCIE_TRANSFER_SIZE,
            DWAXIDMAC_AX_CACHE_CACHEABLE,
            DWAXIDMAC_AX_CACHE_DEVICE,
            dma_master_a_callback,
            0xA000
        );
        
        if(ret == 0) {
            atomic_printf("CPU%d: Master A PCIE DMA transfer initiated\n", hartid);
        } else {
            atomic_printf("CPU%d: Master A PCIE DMA transfer failed with error %d\n", hartid, ret);
        }
    }
    
    if(hartid == 3) {
        // CPU3 负责发起 Master B 的 MMIO 配置传输
        atomic_printf("CPU%d: Initiating Master B MMIO config transfers (M-mode)\n", hartid);
        for(int iter = 0; iter < 10; iter++) {
            // 为每轮 DMA 使用独立源缓冲，避免异步传输期间被覆盖
            volatile uint32_t *config_src = (volatile uint32_t*)((uintptr_t)DMA_SRC_BASE + 0x10000 + (uintptr_t)iter * MMIO_CONFIG_SIZE);

            uintptr_t mmio_dest_addr = MMIO_DMA_BASE + (iter * MMIO_CONFIG_SIZE);
            
            if(mmio_dest_addr + MMIO_CONFIG_SIZE > MMIO_DMA_BASE + MMIO_DMA_SIZE) {
                uint32_t max_slots = MMIO_DMA_SIZE / MMIO_CONFIG_SIZE;
                mmio_dest_addr = MMIO_DMA_BASE + ((iter % max_slots) * MMIO_CONFIG_SIZE);
            }
            
            // 填充配置数据
            for(int i = 0; i < MMIO_CONFIG_SIZE / sizeof(uint32_t); i++) {
                *(config_src + i) = (VALUE_BASE + iter + i) | 0xC0DE0000;
            }
            riscv_fence();
            
            int ret = dma_transfer(
                DMA_MEM_TO_DEV,
                (uintptr_t)config_src,
                mmio_dest_addr,
                MMIO_CONFIG_SIZE,
                DWAXIDMAC_AX_CACHE_CACHEABLE,
                DWAXIDMAC_AX_CACHE_DEVICE,
                dma_master_b_callback,
                iter
            );

            if(ret != 0) {
                atomic_printf("CPU%d: Master B MMIO config %d initiation failed with error %d\n", hartid, iter, ret);
            }
            
            riscv_fence();
        }
        atomic_printf("CPU%d: All Master B MMIO config transfers initiated\n", hartid);
    }
    
    barrier(NUM_CORES);

    vm_enable(0x84000000);

    m_switch_mode(hartid, MODE_S, (uint64_t)&s_main);

    return 0;
}
