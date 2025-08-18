#include <am.h>
#include <platform.h>
#include <klib.h>
#include <klib-macros.h>
#include <plic.h>
#include <xsextra.h>
#include "dw_axi_dmac.h"

#define NUM_CORES 4

#define DMA_SRC_ADDRESS     0x80010000
#define DMA_DEST_ADDRESS    0x80011000
#define TRANSFER_SIZE       32 

// Master A Write Val
#define VALUE_V1            0xABCD1234

volatile uint32_t *dma_src_ptr, *dma_dest_ptr;

void dma_test_callback(int error_code, uint64_t user_data){
    uint32_t value_read_by_cpu = *dma_dest_ptr;

    if (value_read_by_cpu == VALUE_V1) {
        atomic_printf("SUCCESS!\n");
    } else {
        atomic_printf("FAILED!\n");
    }
}

int main() {
    int hartid = riscv_mhartid();
    
    // Core0 init dmac configuration
    // Core1-3 open external interrupt
    dma_init();
    
    if (hartid == 0){
        dma_src_ptr  = (volatile uint32_t*)DMA_SRC_ADDRESS;
        dma_dest_ptr = (volatile uint32_t*)DMA_DEST_ADDRESS;

        *dma_src_ptr = VALUE_V1;
        *dma_dest_ptr = 0;

        riscv_fence();

        dma_transfer(
            DMA_MEM_TO_MEM, 
            (uintptr_t)dma_src_ptr, 
            (uintptr_t)dma_dest_ptr, 
            TRANSFER_SIZE, 
            DWAXIDMAC_AX_CACHE_CACHEABLE, 
            DWAXIDMAC_AX_CACHE_CACHEABLE, 
            dma_test_callback, 
            0
        );
    }

    riscv_wfi();

    return 0;
}