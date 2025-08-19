#include <am.h>
#include <platform.h>
#include <klib.h>
#include <klib-macros.h>
#include <plic.h>
#include <xsextra.h>
#include "dw_axi_dmac.h"

#define NUM_CORES 4

#define NUM_CHANNELS    NUM_DMA_CONTROLLERS * DMAC_MAX_CHANNELS

#define MAX_TRANSFER_SIZE   1024
#define LITERATION          2
#define TRANSFER_ITEM_SIZE  32      // Byte

#define SHARED_MEM_BASE     0x80010000
#define DEST_MEM_BASE       0x80020000
#define SHARED_MEM_SIZE     MAX_TRANSFER_SIZE * NUM_CHANNELS

volatile uint32_t *src_addr = (volatile uint32_t *)SHARED_MEM_BASE;
volatile uint32_t *dest_addr = 0;
volatile uint32_t xfer_cnt = 0;
volatile uint64_t xfer_cnt_lock = 0;

void dma_test_callback(int error_code, uint64_t user_data){
    lock_acquire(&xfer_cnt_lock);
    xfer_cnt++;
    lock_release(&xfer_cnt_lock);

    atomic_printf("interrupt!!! transfer id: %d\n", user_data);
}

int main() {
    int hartid = riscv_mhartid();
    bool flag = false;

    dma_init();
    srand(0x1234);

    if(hartid == 0){
        for(int i = 0; i < LITERATION; i++){
            size_t transfer_size;
            transfer_size = rand_in_range(TRANSFER_ITEM_SIZE, MAX_TRANSFER_SIZE);
            transfer_size -= transfer_size % TRANSFER_ITEM_SIZE;
            
            memset((void*)SHARED_MEM_BASE, 0, MAX_TRANSFER_SIZE);
            memset((void*)DEST_MEM_BASE, 0, SHARED_MEM_SIZE);

            for(int j = 0; j < transfer_size / 4; j++)
                ((uint32_t*)src_addr)[j] = rand();
            
            riscv_fence();
            
            for(int j = 0; j < NUM_CHANNELS; j++){
                dest_addr = (volatile uint32_t *)DEST_MEM_BASE + j * MAX_TRANSFER_SIZE;
                int ret = dma_transfer(
                    DMA_MEM_TO_MEM,
                    (uintptr_t)src_addr,
                    (uintptr_t)dest_addr,
                    transfer_size,
                    DWAXIDMAC_AX_CACHE_CACHEABLE,
                    DWAXIDMAC_AX_CACHE_CACHEABLE,
                    dma_test_callback,
                    i * NUM_CHANNELS + j
                );
                
                if(ret){
                    uint32_t delay_cnt = 1000;
                    while(delay_cnt--);
                }
            }

            while(1){
                if(xfer_cnt == NUM_CHANNELS)
                    break;
            }
            xfer_cnt = 0;
            riscv_fence();

            for(int j = 0; j < NUM_CHANNELS; j++){
                dest_addr = (volatile uint32_t *)DEST_MEM_BASE + j * MAX_TRANSFER_SIZE;
                for(int k = 0; k < transfer_size; k++){
                    if(src_addr[k] != dest_addr[k]){
                        flag = true;
                        atomic_printf("FAILED!!! Index:%d, src data:%x, dest data:%x\n", k, src_addr[k], dest_addr[k]);
                        break;
                    }
                }
            }

            if(flag)
                break;
        }
    }

    if(hartid){
        while(1){
            riscv_wfi();
        }
    }

    if(!flag)
        atomic_printf("SUCCESS!!!\n");

    return 0;
}