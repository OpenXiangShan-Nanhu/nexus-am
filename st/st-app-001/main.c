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

#define NUM_CORES 4
#define NUM_BUFFER 10
#define FACTOR 7
#define TRANSFER_SIZE (1024)   // ≤ 200000n

#define SEED 1928490

static uint8_t src_buffers[TRANSFER_SIZE] __attribute__((aligned(64)));
static uint8_t dst_buffers[NUM_BUFFER][TRANSFER_SIZE] __attribute__((aligned(64)));

static volatile uint8_t flags[NUM_CORES] = {0};
static volatile uint64_t xfer_cnt = 0;
static bool failed = false;

void dma_test_callback(int error_code, uint64_t user_data){
    uint64_t hartid = riscv_mhartid();

    if (error_code != 0) {
        atomic_printf("Core %d: DMA task failed with hardware error code: %d\n", hartid, error_code);
    }

    atomic_printf("Core %d Transfer %d interrupt\n", hartid, user_data);

    xfer_cnt++;
    
    if(xfer_cnt == 2){
        raise_ipi(1);
        raise_ipi(2);
    }

    // bufferC transfer complete
    if(xfer_cnt == 4){
        raise_ipi(0);
    }
    
    if(xfer_cnt == 6){
        raise_ipi(3);
    }
}

// wait for flags
void riscv_wff(){
    uint64_t hartid = riscv_mhartid();

    flags[hartid] = 0;

    while(flags[hartid] == 0){
        riscv_wfi();
    }

    flags[hartid] = 0;
}

void ipi_handler() {
    if (imsic_ipi_claim() != IPI_EIID) {
      default_trap_handler();
      return;
    }
    uint64_t hartid = riscv_mhartid();

    flags[hartid] = 1;
    riscv_fence();
}

void enable_softwareinterrupt(){
  imsic_ipi_enable();
  uint64_t mie = csr_read(mie);
  csr_write(mie, mie | MEIE);

  uint64_t mstatus = csr_read(mstatus);
  csr_write(mstatus, mstatus | (0x1UL << 3));
}

int main(){
    uint64_t hartid = riscv_mhartid();

    dma_init();
    enable_softwareinterrupt();

    if(hartid == 0)
        m_trap_handler_register(MEIP, ipi_handler);
    barrier(NUM_CORES);

    srand(SEED);

    if(hartid == 0){
        for(int i = 0; i < TRANSFER_SIZE; i++)
            src_buffers[i] = (uint8_t)(rand_in_range(1, 65536)) >> 3;
        
        dma_transfer(DMA_MEM_TO_MEM, (uintptr_t)src_buffers, (uintptr_t)dst_buffers[0], TRANSFER_SIZE, DWAXIDMAC_AX_CACHE_CACHEABLE, DWAXIDMAC_AX_CACHE_CACHEABLE, dma_test_callback, 0);
        dma_transfer(DMA_MEM_TO_MEM, (uintptr_t)src_buffers, (uintptr_t)dst_buffers[1], TRANSFER_SIZE, DWAXIDMAC_AX_CACHE_CACHEABLE, DWAXIDMAC_AX_CACHE_CACHEABLE, dma_test_callback, 1);
        
        riscv_wff();
        
        for(int i = 0; i < TRANSFER_SIZE; i++){
            dst_buffers[6][i] = dst_buffers[4][i] / FACTOR;
            dst_buffers[7][i] = dst_buffers[5][i] / FACTOR;
        }
            
        dma_transfer(DMA_MEM_TO_MEM, (uintptr_t)dst_buffers[6], (uintptr_t)dst_buffers[8], TRANSFER_SIZE, DWAXIDMAC_AX_CACHE_CACHEABLE, DWAXIDMAC_AX_CACHE_CACHEABLE, dma_test_callback, 5);
        dma_transfer(DMA_MEM_TO_MEM, (uintptr_t)dst_buffers[7], (uintptr_t)dst_buffers[9], TRANSFER_SIZE, DWAXIDMAC_AX_CACHE_CACHEABLE, DWAXIDMAC_AX_CACHE_CACHEABLE, dma_test_callback, 6);
    } else if(hartid == 1){
        riscv_wff();

        for(int i = 0; i < TRANSFER_SIZE; i++)
            dst_buffers[2][i] = dst_buffers[0][i] * FACTOR;
        dma_transfer(DMA_MEM_TO_MEM, (uintptr_t)dst_buffers[2], (uintptr_t)dst_buffers[4], TRANSFER_SIZE, DWAXIDMAC_AX_CACHE_CACHEABLE, DWAXIDMAC_AX_CACHE_CACHEABLE, dma_test_callback, 3);

        riscv_wff();
    } else if(hartid == 2){
        riscv_wff();

        for(int i = 0; i < TRANSFER_SIZE; i++)
            dst_buffers[3][i] = dst_buffers[1][i] * FACTOR;
        dma_transfer(DMA_MEM_TO_MEM, (uintptr_t)dst_buffers[3], (uintptr_t)dst_buffers[5], TRANSFER_SIZE, DWAXIDMAC_AX_CACHE_CACHEABLE, DWAXIDMAC_AX_CACHE_CACHEABLE, dma_test_callback, 4);

        riscv_wff();
    } else {
        riscv_wff();

        // data check
        for(int i = 0; i < TRANSFER_SIZE; i++){
            if(src_buffers[i] != dst_buffers[8][i] || src_buffers[i] != dst_buffers[9][i]){ 
                failed = true;
                atomic_printf("Index: %d, src_data: %d, dst_buffers[8]: %d, dst_buffer[9]: %d\n", i, src_buffers[i], dst_buffers[8][i], dst_buffers[9][i]);
                // break;
            }
        }
        if(failed == 1)
            atomic_printf("FAILED!\n");
        else
            atomic_printf("SUCCESS\n");
    }

    if(hartid != 3)
        riscv_wff();
    // for(int i = 0; i < NUM_TRANSFERS_PER_CORE; i++){
    //     xferid = hartid * NUM_TRANSFERS_PER_CORE + i;
    //     memset(src_buffers[xferid], (uint8_t)(i + xferid), TRANSFER_SIZE);
    //     memset(dst_buffers[xferid], 0, TRANSFER_SIZE);
    //     do {
    //         delay_cnt = 1000;
    //         ret = dma_transfer(DMA_MEM_TO_MEM, (uintptr_t)src_buffers, (uintptr_t)dst_buffers[], TRANSFER_SIZE, DWAXIDMAC_AX_CACHE_NONCACHE, DWAXIDMAC_AX_CACHE_NONCACHE, dma_test_callback, xferid);
    //         while(delay_cnt --);
    //     }while(ret);
    // }

    // while(1){
    //     if(xfer_cnt == NUM_TRANSFERS_PER_CORE * NUM_CORES)
    //         break;
    //     riscv_wff();
    // }

    return 0;
}
