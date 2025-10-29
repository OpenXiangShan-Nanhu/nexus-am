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
#define DMA_TRANSFER_SIZE   1024          // 1KB (修正注释)

// 计算和轮询相关
#define COMPUTE_ITERATIONS  5
#define POLLING_ITERATIONS  2
#define MMIO_CONFIG_SIZE    1024  // 每次配置1KB (32KB/1KB = 32个槽位)

#define VALUE_BASE          0x12345678

// 全局变量
volatile uint32_t *ddr_ptr = (volatile uint32_t*)DDR_BASE;
volatile uint32_t *mmio_ptr = (volatile uint32_t*)MMIO_BASE;
volatile uint32_t *dma_src_ptr = (volatile uint32_t*)DMA_SRC_BASE;
volatile uint32_t *dma_dest_ptr = (volatile uint32_t*)DMA_DEST_BASE;

volatile uint64_t dma_completion_count = 0;
volatile uint64_t mmio_config_count = 0;

// S 模式下的 hartid 存储（避免访问 mhartid CSR）
volatile uint64_t s_mode_hartid[NUM_CORES] = {0};

extern uint64_t atomic_add();

void dma_master_a_callback(int error_code, uint64_t user_data) {
    if (error_code == 0) {
        atomic_add(&dma_completion_count, 1);
        s_atomic_printf("Master A DMA transfer %lu completed successfully\n", user_data);
        
        // 验证数据正确性 - 根据iter计算正确的目标地址
        uint32_t expected_value = VALUE_BASE + user_data;
        volatile uint32_t *actual_dest = (volatile uint32_t*)((uintptr_t)dma_dest_ptr + (user_data * DMA_TRANSFER_SIZE));
        
        for(int i = 0; i < DMA_TRANSFER_SIZE / sizeof(uint32_t); i++) {
            if(*(actual_dest + i) != expected_value + i) {
                s_atomic_printf("Master A DMA verification failed at index %d, expected 0x%x, got 0x%x\n", 
                              i, expected_value + i, *(actual_dest + i));
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
        s_atomic_printf("Master B MMIO config %lu completed successfully\n", user_data);
        
        // 验证MMIO传输的正确性 - 计算对应的MMIO目标地址
        uintptr_t mmio_dest_addr = MMIO_DMA_BASE + (user_data * MMIO_CONFIG_SIZE);
        
        // 修正槽位计算：32KB/1KB = 32个槽位
        if(mmio_dest_addr + MMIO_CONFIG_SIZE > MMIO_DMA_BASE + MMIO_DMA_SIZE) {
            uint32_t max_slots = MMIO_DMA_SIZE / MMIO_CONFIG_SIZE;  // 32KB/1KB = 32个槽位
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
    // CPU0 -> DDR
    s_atomic_printf("CPU%d: Starting DDR computation task\n", hartid);
    
    uint32_t max_elements = DDR_SIZE / sizeof(uint32_t);
    
    for(int iter = 0; iter < COMPUTE_ITERATIONS; iter++) {
        // 确保不超出DDR边界，每次使用固定大小的块
        uint32_t block_size = 128;  // 64个数据 + 2个结果 + 一些缓冲
        uint32_t base_addr = (iter * block_size) % (max_elements - block_size);
        
        for(int i = 0; i < 64; i++) { 
            // 避免整数溢出：使用较小的乘数或改为位操作
            uint32_t computed_value = (VALUE_BASE + iter) ^ (i * 0x5A5A);
            *(ddr_ptr + base_addr + i) = computed_value;
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
        
        if(iter % 10 == 0) {
            s_atomic_printf("CPU%d: Computation iteration %d completed, sum=0x%lx\n", hartid, iter, sum);
        }
    }
    
    s_atomic_printf("CPU%d: DDR computation task completed\n", hartid);
}

void task1(int hartid) {
    // CPU1 -> MMIO读写检测 (使用前32KB区域)
    s_atomic_printf("CPU%d: Starting MMIO read/write detection task (0x%lx-0x%lx)\n", 
                  hartid, MMIO_CPU1_BASE, MMIO_CPU1_BASE + MMIO_CPU1_SIZE - 1);
    
    // 限制在MMIO区域前32KB：0x60020000-0x60027fff
    // 必须使用 volatile 防止编译器优化，并确保 32 位访问
    volatile uint32_t *mmio_test_base = (volatile uint32_t*)MMIO_CPU1_BASE;
    uint32_t mmio_test_size = MMIO_CPU1_SIZE / sizeof(uint32_t);  // 以32位字为单位
    
    for(int iter = 0; iter < POLLING_ITERATIONS; iter++) {
        s_atomic_printf("CPU%d: MMIO detection iteration %d\n", hartid, iter);
        
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
        
        s_atomic_printf("CPU%d: MMIO iteration %d - writes: %d, reads: %d, errors: %d\n", 
                      hartid, iter, write_count * 2, read_count * 2, error_count);
        
        riscv_fence();
    }
    
    s_atomic_printf("CPU%d: MMIO read/write detection task completed\n", hartid);
}

void task2(int hartid) {
    // CPU2 -> 监控 Master A DMA 传输完成情况
    s_atomic_printf("CPU%d: Monitoring Master A DMA completions\n", hartid);
    
    uint64_t last_count = 0;
    for(int iter = 0; iter < 20; iter++) {
        // 等待一段时间
        for(volatile int i = 0; i < 100000; i++);
        
        uint64_t current_count = dma_completion_count;
        if(current_count != last_count) {
            s_atomic_printf("CPU%d: Master A DMA completions: %lu\n", hartid, current_count);
            last_count = current_count;
        }
        
        if(current_count >= 10) {
            break;  // 所有 10 次传输都完成了
        }
    }
    
    s_atomic_printf("CPU%d: Master A DMA monitoring completed, total: %lu\n", hartid, dma_completion_count);
}

void task3(int hartid) {
    // CPU3 -> 监控 Master B MMIO 配置传输完成情况
    s_atomic_printf("CPU%d: Monitoring Master B MMIO config completions\n", hartid);
    
    uint64_t last_count = 0;
    for(int iter = 0; iter < 20; iter++) {
        // 等待一段时间
        for(volatile int i = 0; i < 100000; i++);
        
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
            task0(hartid);  // CPU0: DDR计算
            break;
        case 1:
            task1(hartid);  // CPU1: MMIO读写检测
            break;
        case 2:
            task2(hartid);  // Master A: DMA to DDR
            break;
        case 3:
            task3(hartid);  // Master B: DMA to MMIO
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
        
        // 映射 DMA 驱动需要的内核数据区域
        // DMA 驱动的全局变量（g_lli_pool, g_desc_pool等）需要被映射
        // 假设这些数据在 0x80800000 附近，映射整个内核区域
        for(uintptr_t addr = 0x80800000; addr < 0x81000000; addr += 0x1000) {
            vm_map((void *)addr, (void *)addr, PTE_R | PTE_W | PTE_X | PTE_A | PTE_D);
        }
        
        // 映射各种内存区域
        vm_map((void *)ddr_ptr, (void *)ddr_ptr, PTE_R | PTE_W | PTE_X | PTE_A | PTE_D);
        
        // 映射 MMIO_CPU1 区域（32KB，8页）
        for(uintptr_t addr = MMIO_CPU1_BASE; addr < MMIO_CPU1_BASE + MMIO_CPU1_SIZE; addr += 0x1000) {
            vm_map((void *)addr, (void *)addr, PTE_R | PTE_W | PTE_X | PTE_A | PTE_D);
        }
        
        // 映射 MMIO_DMA 区域（32KB，8页）
        for(uintptr_t addr = MMIO_DMA_BASE; addr < MMIO_DMA_BASE + MMIO_DMA_SIZE; addr += 0x1000) {
            vm_map((void *)addr, (void *)addr, PTE_R | PTE_W | PTE_X | PTE_A | PTE_D);
        }
        
        // 映射 DMA 源区域，确保足够大以支持多个 DMA master
        // 为 Master A 和 Master B 各分配足够空间
        vm_map((void *)dma_src_ptr, (void *)dma_src_ptr, PTE_R | PTE_W | PTE_X | PTE_A | PTE_D);
        vm_map((void *)(DMA_SRC_BASE + 0x10000), (void *)(DMA_SRC_BASE + 0x10000), PTE_R | PTE_W | PTE_X | PTE_A | PTE_D);
        
        // 为DMA目标区域映射足够的空间，支持10次不重叠传输
        // 计算需要的页数：10 * 1KB = 10KB，至少需要 3 个 4KB 页面，为了安全映射 4 页
        uint32_t total_dma_size = 10 * DMA_TRANSFER_SIZE;  // 10KB
        uint32_t num_pages = ((total_dma_size + 0xFFF) >> 12) + 1;  // 向上取整并额外增加1页：4 页
        for(uint32_t i = 0; i < num_pages; i++) {
            void *dest_page = (void*)((uintptr_t)dma_dest_ptr + (i * 0x1000));  // 按 4KB 对齐
            vm_map(dest_page, dest_page, PTE_R | PTE_W | PTE_X | PTE_A | PTE_D);
        }
        
        atomic_printf("CPU%d: DDR Base: 0x%lx, Size: 0x%x\n", hartid, (uint64_t)ddr_ptr, DDR_SIZE);
        atomic_printf("CPU%d: MMIO CPU1: 0x%lx-0x%lx, DMA: 0x%lx-0x%lx\n", hartid, 
                    MMIO_CPU1_BASE, MMIO_CPU1_BASE + MMIO_CPU1_SIZE - 1,
                    MMIO_DMA_BASE, MMIO_DMA_BASE + MMIO_DMA_SIZE - 1);
        atomic_printf("CPU%d: DMA Src: 0x%lx, DMA Dest: 0x%lx\n", hartid, (uint64_t)dma_src_ptr, (uint64_t)dma_dest_ptr);
    }
    
    barrier(NUM_CORES);

    // 在 M 模式下发起所有 DMA 传输（避免在 S 模式下访问 mhartid CSR）
    if(hartid == 2) {
        // CPU2 负责发起 Master A 的 DMA 传输
        atomic_printf("CPU%d: Initiating Master A DMA transfers (M-mode)\n", hartid);
        for(int iter = 0; iter < 10; iter++) {
            // 准备源数据
            uint32_t base_value = VALUE_BASE + iter;
            for(int i = 0; i < DMA_TRANSFER_SIZE / sizeof(uint32_t); i++) {
                *(dma_src_ptr + i) = base_value + i;
            }
            riscv_fence();
            
            uintptr_t dest_addr = (uintptr_t)dma_dest_ptr + (iter * DMA_TRANSFER_SIZE);
            
            int ret = dma_transfer(
                DMA_MEM_TO_MEM,
                (uintptr_t)dma_src_ptr,
                dest_addr,
                DMA_TRANSFER_SIZE,
                DWAXIDMAC_AX_CACHE_CACHEABLE,
                DWAXIDMAC_AX_CACHE_CACHEABLE,
                dma_master_a_callback,
                iter
            );
            
            if(ret != 0) {
                atomic_printf("CPU%d: Master A DMA transfer %d initiation failed with error %d\n", hartid, iter, ret);
            }
        }
        atomic_printf("CPU%d: All Master A DMA transfers initiated\n", hartid);
    }
    
    if(hartid == 3) {
        // CPU3 负责发起 Master B 的 MMIO 配置传输
        atomic_printf("CPU%d: Initiating Master B MMIO config transfers (M-mode)\n", hartid);
        for(int iter = 0; iter < 10; iter++) {
            volatile uint32_t *config_src = (volatile uint32_t*)(DMA_SRC_BASE + 0x10000);

            uintptr_t mmio_dest_addr = MMIO_DMA_BASE + (iter * MMIO_CONFIG_SIZE);
            
            // 修正槽位计算：32KB/1KB = 32个槽位
            if(mmio_dest_addr + MMIO_CONFIG_SIZE > MMIO_DMA_BASE + MMIO_DMA_SIZE) {
                uint32_t max_slots = MMIO_DMA_SIZE / MMIO_CONFIG_SIZE;
                mmio_dest_addr = MMIO_DMA_BASE + ((iter % max_slots) * MMIO_CONFIG_SIZE);
            }
            
            // 填充配置数据
            for(int i = 0; i < MMIO_CONFIG_SIZE / sizeof(uint32_t); i++) {
                *(config_src + i) = (VALUE_BASE + iter + i) | 0xC0DE0000;
            }
            riscv_fence();
            
            // 发起DMA传输：Master B -> MMIO配置
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
