#include <am.h>
#include <platform.h>
#include <klib.h>
#include <klib-macros.h>
#include <plic.h>
#include <xsextra.h>
#include "dw_axi_dmac.h"

#define NUM_CORES 4

#define LITERATION          0x1

#define DMA_SRC0_ADDRESS    0x80010000
#define DMA_DEST0_ADDRESS   0x80011000
#define DMA_DEST1_ADDRESS   0x80012000

#define DMA_MMIO_BASE       0x30000000000

#define TRANSFER_SIZE       8192    // Byte
#define MMIO_TRANSFER_SIZE  32      // Byte
#define TRANSFER_ITEM_SIZE  32      // Byte

#define VALUE_INITIAL       0xDEADBEEF

volatile uint32_t xfer_cnt = 0;
static volatile uint64_t xfer_cnt_lock = 0;

volatile uint32_t *dma_mmio_ptr = (volatile uint32_t*)DMA_MMIO_BASE;
volatile uint32_t *dma_llp_base = (volatile uint32_t*)0x50070000 + COMMON_REG_LEN + CH_LLP + 0x4;

volatile uint32_t *dma_src0_ptr = (volatile uint32_t*)DMA_SRC0_ADDRESS;
volatile uint32_t *dma_dest0_ptr = (volatile uint32_t*)DMA_DEST0_ADDRESS;
volatile uint32_t *dma_dest1_ptr = (volatile uint32_t*)DMA_DEST1_ADDRESS;

void dma_test_callback(int error_code, uint64_t user_data){
    uint32_t xferid = user_data;
    
    xfer_cnt++;
    riscv_fence();


    if(xferid != 0) {
        if(xferid % 2){
            for(int i = 0; i < MMIO_TRANSFER_SIZE / TRANSFER_ITEM_SIZE; i++){
                if(*(dma_mmio_ptr + i) != VALUE_INITIAL)
                    atomic_printf("MEM->MMIO Check Failed!, index %d, val %d\n", i, *(dma_mmio_ptr + i));
            }
            // DEV->MEM
            dma_transfer(DMA_DEV_TO_MEM, (uintptr_t)dma_mmio_ptr, (uintptr_t)dma_dest1_ptr, MMIO_TRANSFER_SIZE, DWAXIDMAC_AX_CACHE_DEVICE, DWAXIDMAC_AX_CACHE_CACHEABLE, dma_test_callback, xferid++);
        }
        else{
            for(int i = 0; i < MMIO_TRANSFER_SIZE / TRANSFER_ITEM_SIZE; i++){
                if(*(dma_dest1_ptr + i) != VALUE_INITIAL)
                    atomic_printf("MMIO->MEM Check Failed!, index %d, val %d\n", i, *(dma_dest1_ptr + i));
            }
            // MEM->DEV
            dma_transfer(DMA_MEM_TO_DEV, (uintptr_t)dma_src0_ptr, (uintptr_t)dma_mmio_ptr, MMIO_TRANSFER_SIZE, DWAXIDMAC_AX_CACHE_CACHEABLE, DWAXIDMAC_AX_CACHE_DEVICE, dma_test_callback, xferid++);
        }
    } else {
        lock_acquire(&xfer_cnt_lock);    
        xfer_cnt++;
        lock_release(&xfer_cnt_lock);
    }
}

int main() {
    int hartid = riscv_mhartid();
    
    dma_init();

    if (hartid == 0){
        for(int i = 0; i < TRANSFER_SIZE / TRANSFER_ITEM_SIZE; i++)
            *(dma_src0_ptr + i) = VALUE_INITIAL;
        
        riscv_fence();
        
        // MEM->MEM
        dma_transfer(DMA_MEM_TO_MEM, (uintptr_t)dma_src0_ptr, (uintptr_t)dma_dest1_ptr, TRANSFER_SIZE, DWAXIDMAC_AX_CACHE_CACHEABLE, DWAXIDMAC_AX_CACHE_CACHEABLE, dma_test_callback, 0);
        // MEM->DEV
        dma_transfer(DMA_MEM_TO_DEV, (uintptr_t)dma_src0_ptr, (uintptr_t)dma_mmio_ptr, MMIO_TRANSFER_SIZE, DWAXIDMAC_AX_CACHE_CACHEABLE, DWAXIDMAC_AX_CACHE_DEVICE, dma_test_callback, 1);
    
        while(1){
            if(xfer_cnt == LITERATION)
                break;
            riscv_wfi();
        }
    }else{
        while(1){
            if(xfer_cnt == LITERATION)
                break;
            riscv_wfi();
        }
    }

    return 0;
}