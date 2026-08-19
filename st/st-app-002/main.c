#include <am.h>
#include <platform.h>
#include <klib.h>
#include <klib-macros.h>
#include <plic.h>
#include <mtrap.h>
#include <xsextra.h>
#include <clint.h>
#include <csr.h>
#include "dw_axi_dmac.h"
#include <cmo.h>

#define NUM_CORES 4
#define NUM_FRAMES 5
#define PACKET_SIZE 1024
#define BLOCK_SIZE 4096
#define MATRIX_DIM 16
#define LFSR_KEY 0xACE1u
#define SEED 1928490

// 内存缓冲区 - 按照 app-001 风格使用静态数组
static uint8_t offline_raw_buffer[BLOCK_SIZE] __attribute__((aligned(64)));
static uint8_t offline_proc_buffer[BLOCK_SIZE] __attribute__((aligned(64)));
static uint8_t online_ring_buffer[PACKET_SIZE] __attribute__((aligned(64)));
static uint8_t online_proc_buffer[PACKET_SIZE] __attribute__((aligned(64)));
static uint8_t final_mixed_buffer[BLOCK_SIZE + PACKET_SIZE] __attribute__((aligned(64)));
static uint8_t final_output_buffer[BLOCK_SIZE + PACKET_SIZE] __attribute__((aligned(64)));
static uint8_t final_check_buffer[BLOCK_SIZE + PACKET_SIZE] __attribute__((aligned(64)));

// 变换矩阵 (16x16)
static uint32_t transform_matrix[MATRIX_DIM][MATRIX_DIM];

// 同步标志
static volatile uint8_t flags[NUM_CORES] = {0};
static volatile uint8_t core_done[NUM_CORES] = {0};
static volatile uint64_t current_frame = 0;

// DMA回调函数
void dma_callback_master_a(int error_code, uint64_t user_data) {
    uint64_t hartid = riscv_mhartid();
    if (error_code != 0) {
        atomic_printf("Core %d: Master A DMA failed with error: %d\n", hartid, error_code);
    }
    atomic_printf("Core %d: Master A transfer %lu complete\n", hartid, user_data);
    
    // Master A完成后，通知CPU2和CPU3执行LFSR异或(T2)
    raise_ipi(2);
    raise_ipi(3);
}

void dma_callback_master_c(int error_code, uint64_t user_data) {
    uint64_t hartid = riscv_mhartid();
    if (error_code != 0) {
        atomic_printf("Core %d: Master C DMA failed with error: %d\n", hartid, error_code);
    }
    atomic_printf("Core %d: Master C transfer %lu complete\n", hartid, user_data);
    
    // Master C完成后，CPU0执行数据包重组，然后通知CPU1执行矩阵乘法(T1)
    raise_ipi(0);  // 通知CPU0重组
}

void dma_callback_master_b(int error_code, uint64_t user_data) {
    uint64_t hartid = riscv_mhartid();
    if (error_code != 0) {
        atomic_printf("Core %d: Master B (GPU) DMA failed with error: %d\n", hartid, error_code);
    }
    atomic_printf("Core %d: Master B (GPU) transfer %lu complete\n", hartid, user_data);
    
    // Master B完成后，通知CPU0启动Master D
    raise_ipi(0);
}

void dma_callback_master_d(int error_code, uint64_t user_data) {
    uint64_t hartid = riscv_mhartid();
    if (error_code != 0) {
        atomic_printf("Core %d: Master D (USB) DMA failed with error: %d\n", hartid, error_code);
    }
    atomic_printf("Core %d: Master D (USB) transfer %lu complete\n", hartid, user_data);
    
    // Master D完成后，通知CPU0进行下一帧
    raise_ipi(0);
}

// 等待标志位 - 参考 app-001 的 riscv_wff
void riscv_wff() {
    uint64_t hartid = riscv_mhartid();
    flags[hartid] = 0;
    while (flags[hartid] == 0) {
        riscv_wfi();
    }
    flags[hartid] = 0;
}

// IPI中断处理 - 参考 app-001
void ipi_handler() {
    if (imsic_ipi_claim() != IPI_EIID) {
      default_trap_handler();
      return;
    }
    uint64_t hartid = riscv_mhartid();
    flags[hartid] = 1;
    riscv_fence();
}

// 使能软件中断 - 参考 app-001
void enable_softwareinterrupt() {
    imsic_ipi_enable();
    uint64_t mie = csr_read(mie);
    csr_write(mie, mie | MEIE);
    uint64_t mstatus = csr_read(mstatus);
    csr_write(mstatus, mstatus | (0x1UL << 3));
}

// T2: LFSR异或变换
void lfsr_xor_transform(uint8_t *input, uint8_t *output, uint32_t size, uint16_t key, uint16_t offset) {
    uint16_t lfsr = key ^ offset;
    for (uint32_t i = 0; i < size; i++) {
        uint16_t bit = ((lfsr >> 0) ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 5)) & 1;
        lfsr = (lfsr >> 1) | (bit << 15);
        output[i] = input[i] ^ (uint8_t)(lfsr & 0xFF);
    }
}

// T1: 矩阵乘法变换
void matrix_transform(uint8_t *input, uint8_t *output, uint32_t size) {
    uint32_t num_vectors = size / (MATRIX_DIM * sizeof(uint32_t));
    uint32_t *in_ptr = (uint32_t *)input;
    uint32_t *out_ptr = (uint32_t *)output;
    
    for (uint32_t vec_idx = 0; vec_idx < num_vectors; vec_idx++) {
        uint32_t vec[MATRIX_DIM];
        uint32_t result[MATRIX_DIM] = {0};
        
        for (int i = 0; i < MATRIX_DIM; i++) {
            vec[i] = in_ptr[vec_idx * MATRIX_DIM + i];
        }
        
        for (int i = 0; i < MATRIX_DIM; i++) {
            for (int j = 0; j < MATRIX_DIM; j++) {
                result[i] += transform_matrix[i][j] * vec[j];
            }
        }
        
        for (int i = 0; i < MATRIX_DIM; i++) {
            out_ptr[vec_idx * MATRIX_DIM + i] = result[i];
        }
    }
}

// T3: 块交织混合
void interleave_mix(uint8_t *offline_data, uint8_t *online_data, uint8_t *output, 
                    uint32_t offline_size, uint32_t online_size) {
    uint32_t output_idx = 0;
    uint32_t max_size = offline_size > online_size ? offline_size : online_size;
    
    for (uint32_t i = 0; i < max_size; i++) {
        if (i < offline_size) {
            output[output_idx++] = offline_data[i];
        }
        if (i < online_size) {
            output[output_idx++] = online_data[i];
        }
    }
}

int main() {
    uint64_t hartid = riscv_mhartid();
    
    dma_init();
    enable_softwareinterrupt();
    
    if (hartid == 0)
        m_trap_handler_register(MEIP, ipi_handler);
    barrier(NUM_CORES);
    
    srand(SEED);
    
    if (hartid == 0) {
        core_done[1] = 0;
        core_done[2] = 0;
        core_done[3] = 0;
        riscv_fence();

        // 步骤1: CPU0初始化
        atomic_printf("Core 0: Initializing buffers and parameters\n");
        
        // 填充离线原始数据
        for (int i = 0; i < BLOCK_SIZE; i++) {
            offline_raw_buffer[i] = (uint8_t)(rand_in_range(1, 256));
        }
        
        // 初始化变换矩阵 (单位矩阵)
        for (int i = 0; i < MATRIX_DIM; i++) {
            for (int j = 0; j < MATRIX_DIM; j++) {
                transform_matrix[i][j] = (i == j) ? 1 : 0;
            }
        }
        
        // 初始化环形缓冲区
        for (int i = 0; i < PACKET_SIZE; i++) {
            online_ring_buffer[i] = (uint8_t)(rand_in_range(1, 256));
        }
        
        atomic_printf("Core 0: Initialization complete\n");
        
        // 主循环：处理多帧
        for (current_frame = 0; current_frame < NUM_FRAMES; current_frame++) {
            atomic_printf("\n=== Frame %lu ===\n", current_frame);

            // 重置每帧的完成标志，确保 CPU0 会等待各核的新一轮处理
            core_done[1] = 0;
            core_done[2] = 0;
            core_done[3] = 0;
            riscv_fence();
            
            // 步骤2: 启动双流 - Master A (离线) 和 Master C (在线)
            atomic_printf("Core 0: Starting Master A (offline) and Master C (online)\n");
            
            dma_transfer(DMA_MEM_TO_MEM, (uintptr_t)offline_raw_buffer, (uintptr_t)offline_proc_buffer, 
                        BLOCK_SIZE, DWAXIDMAC_AX_CACHE_CACHEABLE, DWAXIDMAC_AX_CACHE_CACHEABLE, 
                        dma_callback_master_a, current_frame);
            
            dma_transfer(DMA_MEM_TO_MEM, (uintptr_t)online_ring_buffer, (uintptr_t)online_proc_buffer, 
                        PACKET_SIZE, DWAXIDMAC_AX_CACHE_CACHEABLE, DWAXIDMAC_AX_CACHE_CACHEABLE, 
                        dma_callback_master_c, current_frame);
            
            // 步骤3: 等待Master C中断，执行数据包重组，通知CPU1
            riscv_wff();
            atomic_printf("Core 0: Master C complete, performing packet reassembly\n");
            // 数据包重组（简化为直接使用）
            
            raise_ipi(1);  // 通知CPU1执行T1
            
            // 步骤5: 等待CPU1, CPU2, CPU3完成（帧级同步）
            // CPU2和CPU3已经在执行T2
            while ((core_done[1] == 0) || (core_done[2] == 0) || (core_done[3] == 0)) {
                riscv_wff();
            }
            
            atomic_printf("Core 0: All processing complete, performing T3 mix\n");
            
            // 步骤6: CPU0执行块交织混合(T3)
            interleave_mix(offline_proc_buffer, online_proc_buffer, final_mixed_buffer, 
                          BLOCK_SIZE, PACKET_SIZE);

            // 刷新混合结果到内存供后续 DMA 读取
            mem_flush((const volatile uint8_t *)final_mixed_buffer, BLOCK_SIZE + PACKET_SIZE);
            riscv_fence();
            
            atomic_printf("Core 0: T3 mix complete\n");
            
            // 步骤7: 配置Master B (GPU DMA) - 一致性传输
            atomic_printf("Core 0: Starting Master B (GPU DMA)\n");
            dma_transfer(DMA_MEM_TO_MEM, (uintptr_t)final_mixed_buffer, (uintptr_t)final_output_buffer, 
                        BLOCK_SIZE + PACKET_SIZE, DWAXIDMAC_AX_CACHE_CACHEABLE, DWAXIDMAC_AX_CACHE_CACHEABLE, 
                        dma_callback_master_b, current_frame);
            
            riscv_wff();  // 等待Master B完成

            mem_flush((const volatile uint8_t *)final_output_buffer, BLOCK_SIZE + PACKET_SIZE);
            riscv_fence();

            // 步骤8: 配置Master D (USB DMA) - 非一致性回环
            atomic_printf("Core 0: Starting Master D (USB DMA loopback)\n");
            dma_transfer(DMA_MEM_TO_MEM, (uintptr_t)final_output_buffer, (uintptr_t)final_check_buffer, 
                        BLOCK_SIZE + PACKET_SIZE, DWAXIDMAC_AX_CACHE_NONCACHE, DWAXIDMAC_AX_CACHE_NONCACHE, 
                        dma_callback_master_d, current_frame);
            // dma_transfer(DMA_MEM_TO_MEM, (uintptr_t)final_output_buffer, (uintptr_t)final_check_buffer, 
            //             BLOCK_SIZE + PACKET_SIZE, DWAXIDMAC_AX_CACHE_CACHEABLE, DWAXIDMAC_AX_CACHE_CACHEABLE, 
            //             dma_callback_master_d, current_frame);
            
            riscv_wff();  // 等待Master D完成

            // 失效 DMA 回环结果，确保后续验证读取到最新数据
            mem_invalid((const volatile uint8_t *)final_check_buffer, BLOCK_SIZE + PACKET_SIZE);
            riscv_fence();
            
            // 最终验证：计算期望结果并与final_check_buffer比较
            atomic_printf("Core 0: Starting final verification for frame %lu\n", current_frame);
            
            // 步骤1: 计算期望的offline处理结果 (经过T2 LFSR)
            uint8_t expected_offline[BLOCK_SIZE];
            lfsr_xor_transform(offline_raw_buffer, expected_offline, BLOCK_SIZE / 2, LFSR_KEY, 0);
            lfsr_xor_transform(offline_raw_buffer + BLOCK_SIZE / 2, expected_offline + BLOCK_SIZE / 2, 
                             BLOCK_SIZE / 2, LFSR_KEY, 0x1234);
            
            // 步骤2: 计算期望的online处理结果 (经过T1矩阵变换，单位矩阵所以不变)
            uint8_t expected_online[PACKET_SIZE];
            for (uint32_t i = 0; i < PACKET_SIZE; i++) {
                expected_online[i] = online_ring_buffer[i];
            }
            
            // 步骤3: 计算期望的T3交织结果
            uint8_t expected_mixed[BLOCK_SIZE + PACKET_SIZE];
            interleave_mix(expected_offline, expected_online, expected_mixed, BLOCK_SIZE, PACKET_SIZE);
            
            // 步骤4: 验证final_check_buffer (经过Master B和Master D的DMA链)
            uint32_t errors = 0;
            for (uint32_t i = 0; i < BLOCK_SIZE + PACKET_SIZE; i++) {
                if (final_check_buffer[i] != expected_mixed[i]) {
                    if (errors < 10) {
                        atomic_printf("Frame %lu Check FAILED at %u: expected 0x%02x, got 0x%02x\n", 
                                     current_frame, i, expected_mixed[i], final_check_buffer[i]);
                    }
                    errors++;
                }
            }
            
            if (errors == 0) {
                atomic_printf("=== Frame %lu: VERIFICATION PASSED ===\n", current_frame);
            } else {
                atomic_printf("=== Frame %lu: VERIFICATION FAILED (%u errors) ===\n", current_frame, errors);
            }
            
            atomic_printf("Core 0: Frame %lu complete\n", current_frame);
        }
        
        atomic_printf("\nCore 0: All frames processed\n");
        
    } else if (hartid == 1) {
        // CPU1: 执行矩阵乘法(T1)
        for (uint64_t frame = 0; frame < NUM_FRAMES; frame++) {
            riscv_wff();  // 等待CPU0的IPI
            atomic_printf("Core 1: Performing matrix transform (T1) for frame %lu\n", frame);
            matrix_transform(online_proc_buffer, online_proc_buffer, PACKET_SIZE);
            atomic_printf("Core 1: T1 complete for frame %lu\n", frame);
            core_done[1] = 1;
            riscv_fence();
            raise_ipi(0);  // 通知CPU0完成
        }
        
    } else if (hartid == 2) {
        // CPU2: 执行LFSR异或(T2) - 并行处理
        for (uint64_t frame = 0; frame < NUM_FRAMES; frame++) {
            riscv_wff();  // 等待Master A的中断IPI
            atomic_printf("Core 2: Performing LFSR XOR (T2) for frame %lu\n", frame);
            lfsr_xor_transform(offline_proc_buffer, offline_proc_buffer, BLOCK_SIZE / 2, LFSR_KEY, 0);
            atomic_printf("Core 2: T2 complete for frame %lu\n", frame);
            core_done[2] = 1;
            riscv_fence();
            raise_ipi(0);  // 通知CPU0完成
        }
        
    } else if (hartid == 3) {
        // CPU3: 执行LFSR异或(T2) - 并行处理
        for (uint64_t frame = 0; frame < NUM_FRAMES; frame++) {
            riscv_wff();  // 等待Master A的中断IPI
            atomic_printf("Core 3: Performing LFSR XOR (T2) for frame %lu\n", frame);
            lfsr_xor_transform(offline_proc_buffer + BLOCK_SIZE / 2, 
                             offline_proc_buffer + BLOCK_SIZE / 2, 
                             BLOCK_SIZE / 2, LFSR_KEY, 0x1234);
            atomic_printf("Core 3: T2 complete for frame %lu\n", frame);
            core_done[3] = 1;
            riscv_fence();
            raise_ipi(0);  // 通知CPU0完成
        }
    }
    
    return 0;
}
