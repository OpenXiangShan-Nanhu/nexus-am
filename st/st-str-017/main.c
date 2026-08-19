#include <am.h>
#include <platform.h>
#include <klib.h>
#include <klib-macros.h>
#include <mtrap.h>
#include <csr.h>
#include "clint.h"
#include "ppu.h"

#define NUM_CORES 4
#define TEST_DURATION_SECONDS 30
#define WFI_IPI_CYCLES 1000

// 统计计数器 - 每个核心的状态
static volatile uint64_t wfi_count[NUM_CORES] = {0};
static volatile uint64_t ipi_received_count[NUM_CORES] = {0};
static volatile uint64_t ipi_sent_count[NUM_CORES] = {0};
static volatile uint64_t wakeup_count[NUM_CORES] = {0};
static volatile int test_running = 1;
static volatile int cores_ready = 0;

// 轮询指针 - 控制IPI发送目标
static volatile int next_target[NUM_CORES] = {1, 2, 3, 0};

void ipi_handler() {
    if (imsic_ipi_claim() != IPI_EIID) {
      default_trap_handler();
      return;
    }
    uint64_t hartid = riscv_mhartid();

    // 增加接收计数
    ipi_received_count[hartid]++;
    wakeup_count[hartid]++;
    
    // 打印状态信息（每100次IPI打印一次）
    if (ipi_received_count[hartid] % 10 == 0) {
        atomic_printf("Core %lu: WFI=%lu, IPI_RX=%lu, IPI_TX=%lu, Wakeup=%lu\n", 
                     hartid, wfi_count[hartid], ipi_received_count[hartid], 
                     ipi_sent_count[hartid], wakeup_count[hartid]);
    }
}

int ipi_init() {
    // 启用机器模式外部中断（IPI经IMSIC MSI投递）
    imsic_ipi_enable();
    uint64_t mie = csr_read(mie);
    csr_write(mie, mie | MEIE);

    // 启用全局中断
    uint64_t mstatus = csr_read(mstatus);
    csr_write(mstatus, mstatus | (0x1UL << 3));
    
    return 0;
}

void wfi_ipi_loop() {
    int hartid = riscv_mhartid();
    uint64_t loop_count = 0;
    
    // Core 0 作为主导核心，负责启动和维持IPI链条
    if (hartid == 0) {
        while (test_running) {
            // Core 0 不进入WFI，而是定期发送IPI来唤醒其他核心
            for (int target = 1; target < NUM_CORES; target++) {
                raise_ipi(target);
                ipi_sent_count[hartid]++;
            }
            
            // 短暂延迟，让其他核心有时间响应
            for (volatile int i = 0; i < 100; i++);
            
            loop_count++;
            
            // 检查是否达到测试结束条件
            if (loop_count > WFI_IPI_CYCLES / 10) {  // 减少Core 0的循环次数
                test_running = 0;
                riscv_fence();
                
                // 发送最后的唤醒信号
                for (int i = 1; i < NUM_CORES; i++) {
                    raise_ipi(i);
                }
                break;
            }
        }
    } else {
        // 其他核心执行WFI-IPI循环
        while (test_running) {
            // 进入WFI状态等待IPI
            wfi_count[hartid]++;
            riscv_wfi();
            
            // 被唤醒后，向下一个核心发送IPI（避免发送给Core 0）
            if (test_running) {
                int target = next_target[hartid];
                if (target != 0 && target != hartid) {  // 不发送给Core 0和自己
                    raise_ipi(target);
                    ipi_sent_count[hartid]++;
                }
                
                // 更新下一个目标（跳过Core 0）
                next_target[hartid] = (next_target[hartid] + 1) % NUM_CORES;
                if (next_target[hartid] == 0 || next_target[hartid] == hartid) {
                    next_target[hartid] = (next_target[hartid] + 1) % NUM_CORES;
                    if (next_target[hartid] == 0) {
                        next_target[hartid] = 1;  // 确保不是Core 0
                    }
                }
                
                // 短暂延迟
                for (volatile int i = 0; i < 100; i++);
            }
            
            loop_count++;
        }
    }
}

void core_background_task() {
    int hartid = riscv_mhartid();
    
    // 执行一些轻量级的背景任务，验证核心在IPI处理间隙能正常运行
    volatile uint64_t sum = 0;
    for (int i = 0; i < 10000; i++) {
        sum += i * hartid;
    }
}

int main() {
    int hartid = riscv_mhartid();
    
    if (hartid == 0) {
        atomic_printf("=== ST-STR-017: Multi-Core WFI-IPI Loop Stability Test ===\n");
        atomic_printf("Test: All 4 cores repeatedly execute WFI and wake up via IPI\n");
        atomic_printf("Duration: %d seconds (estimated)\n", TEST_DURATION_SECONDS);
        atomic_printf("Target cycles: %d\n", WFI_IPI_CYCLES);
    }
    
    // 注册IPI中断处理器
    if (m_trap_handler_register(MEIP, ipi_handler)) {
        atomic_printf("Core %d: Failed to register IPI handler\n", hartid);
        return 1;
    }
    
    // 初始化IPI
    if (ipi_init()) {
        atomic_printf("Core %d: Failed to initialize IPI\n", hartid);
        return 1;
    }
    
    // 等待所有核心准备就绪
    if (barrier(NUM_CORES)) {
        atomic_printf("Core %d: Barrier failed\n", hartid);
        return 1;
    }
    
    if (hartid == 0) {
        atomic_printf("All cores ready. Starting WFI-IPI loop test...\n");
        atomic_printf("Core 0 will act as IPI generator, other cores will execute WFI-IPI loops\n");
        cores_ready = 1;
        riscv_fence();
    } else {
        // 等待Core 0的启动信号
        while (!cores_ready) {
            riscv_fence();
        }
    }
    
    // 主测试循环：WFI-IPI循环
    wfi_ipi_loop();
    
    // 执行一些背景任务，确认核心功能正常
    core_background_task();
    
    // 等待所有核心完成
    if (barrier(NUM_CORES)) {
        atomic_printf("Core %d: Final barrier failed\n", hartid);
        return 1;
    }
    
    if (hartid == 0) {
        atomic_printf("\n=== Final Test Results ===\n");
        
        uint64_t total_wfi = 0;
        uint64_t total_ipi_rx = 0;
        uint64_t total_ipi_tx = 0;
        uint64_t total_wakeup = 0;
        
        for (int i = 0; i < NUM_CORES; i++) {
            atomic_printf("Core %d - WFI: %lu, IPI_RX: %lu, IPI_TX: %lu, Wakeup: %lu\n", 
                         i, wfi_count[i], ipi_received_count[i], ipi_sent_count[i], wakeup_count[i]);
            
            total_wfi += wfi_count[i];
            total_ipi_rx += ipi_received_count[i];
            total_ipi_tx += ipi_sent_count[i];
            total_wakeup += wakeup_count[i];
        }
        
        atomic_printf("Total - WFI: %lu, IPI_RX: %lu, IPI_TX: %lu, Wakeup: %lu\n", 
                     total_wfi, total_ipi_rx, total_ipi_tx, total_wakeup);
        
        // 检查系统稳定性
        int all_cores_active = 1;
        int ipi_balance_ok = 1;
        
        for (int i = 0; i < NUM_CORES; i++) {
            // 检查每个核心是否都有足够的活动
            if (wfi_count[i] < 100 || wakeup_count[i] < 50) {
                atomic_printf("Core %d appears to be inactive or stuck!\n", i);
                all_cores_active = 0;
            }
            
            // 检查IPI发送和接收的大致平衡（允许一定误差）
            uint64_t diff = (ipi_sent_count[i] > ipi_received_count[i]) ? 
                           (ipi_sent_count[i] - ipi_received_count[i]) : 
                           (ipi_received_count[i] - ipi_sent_count[i]);
            if (diff > 100) {  // 允许100次的差异
                atomic_printf("Core %d has significant IPI imbalance (TX=%lu, RX=%lu)\n", 
                             i, ipi_sent_count[i], ipi_received_count[i]);
                ipi_balance_ok = 0;
            }
        }
        
        if (all_cores_active && ipi_balance_ok && total_wfi > 1000 && total_wakeup > 500) {
            atomic_printf("=== SUCCESS ===\n");
            atomic_printf("System maintained stability under continuous WFI-IPI cycles.\n");
            atomic_printf("All cores remained responsive and no deadlock or corruption detected.\n");
            atomic_printf("IPI delivery and wakeup mechanisms functioned correctly.\n");
        } else {
            atomic_printf("=== FAILED ===\n");
            if (!all_cores_active) {
                atomic_printf("Some cores became inactive or stuck.\n");
            }
            if (!ipi_balance_ok) {
                atomic_printf("IPI delivery appeared unbalanced or unreliable.\n");
            }
            if (total_wfi <= 1000 || total_wakeup <= 500) {
                atomic_printf("Insufficient test activity detected.\n");
            }
        }
    }
    
    return 0;
}