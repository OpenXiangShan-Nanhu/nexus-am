#include <am.h>
#include <platform.h>
#include <klib.h>
#include <klib-macros.h>
#include <plic.h>
#include <xsextra.h>
#include "dw_axi_dmac.h"

#define NUM_CORES 4

#define DMA_SRC0_ADDRESS    0x80010000
#define DMA_DEST0_ADDRESS   0x80011000
#define DMA_DEST1_ADDRESS   0x80012000
#define TRANSFER_SIZE       1024    // Byte
#define TRANSFER_ITEM_SIZE  32      // Byte

#define VALUE_INITIAL       0xDEADBEEF

volatile uint32_t xfer_cnt = 0;
volatile uint32_t *dma_src0_ptr, *dma_dest0_ptr, *dma_dest1_ptr;

void dma_test_callback(int error_code, uint64_t user_data){
    int flag = 1;
    
    xfer_cnt++;
    riscv_fence();

    if(xfer_cnt == 1) {
        dma_transfer(DMA_MEM_TO_MEM, (uintptr_t)dma_dest0_ptr, (uintptr_t)dma_dest1_ptr, TRANSFER_SIZE, DWAXIDMAC_AX_CACHE_CACHEABLE, DWAXIDMAC_AX_CACHE_CACHEABLE, dma_test_callback, 1);
    } else if(xfer_cnt == 2) {
        for(int i = 0; i < TRANSFER_SIZE / TRANSFER_ITEM_SIZE; i++){
            uint32_t value_read = *(dma_dest1_ptr + i);
            if (value_read != VALUE_INITIAL) {
                atomic_printf("FAILED!\n");
                flag = 0;
                break;
            }
        }
        if(flag)
            atomic_printf("SUCCESS!\n");
    }
}

int main() {
    int hartid = riscv_mhartid();
    
    dma_init();
    
    if (hartid == 0){
        dma_src0_ptr  = (volatile uint32_t*)DMA_SRC0_ADDRESS;
        dma_dest0_ptr = (volatile uint32_t*)DMA_DEST0_ADDRESS;
        dma_dest1_ptr = (volatile uint32_t*)DMA_DEST1_ADDRESS;

        for(int i = 0; i < TRANSFER_SIZE / TRANSFER_ITEM_SIZE; i++)
            *(dma_src0_ptr + i) = VALUE_INITIAL;

        riscv_fence();

        dma_transfer(DMA_MEM_TO_MEM, (uintptr_t)dma_src0_ptr, (uintptr_t)dma_dest0_ptr, TRANSFER_SIZE, DWAXIDMAC_AX_CACHE_CACHEABLE, DWAXIDMAC_AX_CACHE_CACHEABLE, dma_test_callback, 0);
    }

    while(1){
        if(xfer_cnt == 2)
            break;
        riscv_wfi();
    }    

    return 0;
}