#include <am.h>
#include <platform.h>
#include <klib.h>
#include <klib-macros.h>
#include <plic.h>
#include <xsextra.h>
#include "dw_axi_dmac.h"

#define NUM_CORES 4
#define NUM_TRANSFERS_PER_CORE 4
#define TRANSFER_SIZE (1024)   // 1KB

static uint8_t src_buffers[NUM_CORES * NUM_TRANSFERS_PER_CORE][TRANSFER_SIZE] __attribute__((aligned(64)));
static uint8_t dst_buffers[NUM_CORES * NUM_TRANSFERS_PER_CORE][TRANSFER_SIZE] __attribute__((aligned(64)));
static volatile uint64_t xfer_cnt = 0;
static volatile uintptr_t xfer_cnt_lock = 0;

void dma_test_callback(int error_code, uint64_t user_data){
    uint64_t xferid = user_data;
    uint64_t hartid = riscv_mhartid();

    if (error_code != 0) {
        atomic_printf("Core %d: DMA task failed with hardware error code: %d\n", hartid, error_code);
    } else {
        int mismatch = 0;
        for(int i = 0; i < TRANSFER_SIZE; i++){
            if(src_buffers[xferid][i] != dst_buffers[xferid][i]) {
                atomic_printf("Core %d: !!! DATA MISMATCH !!! Transfer %d failed verification.\n", hartid, xferid);
                atomic_printf("Xferid:%d, Src Data:0x%x, Dst Data:0x%x, Index:%d\n", xferid, src_buffers[xferid][i], dst_buffers[xferid][i], i);
                mismatch = 1;
                break;
            }
        }

        if(!mismatch)
            atomic_printf("Core %d: Transfer %d verified successfully.\n", hartid, xferid);
    }

    lock_acquire(&xfer_cnt_lock);
    xfer_cnt++;
    lock_release(&xfer_cnt_lock);
}

int main(){
    uint64_t hartid = riscv_mhartid();
    uint64_t xferid;
    int ret, delay_cnt;

    dma_init();
    _barrier();

    for(int i = 0; i < NUM_TRANSFERS_PER_CORE; i++){
        xferid = hartid * NUM_TRANSFERS_PER_CORE + i;
        memset(src_buffers[xferid], (uint8_t)(i + xferid), TRANSFER_SIZE);
        memset(dst_buffers[xferid], 0, TRANSFER_SIZE);
        do {
            delay_cnt = 1000;
            ret = dma_transfer(DMA_MEM_TO_MEM, (uintptr_t)src_buffers[xferid], (uintptr_t)dst_buffers[xferid], TRANSFER_SIZE, dma_test_callback, xferid);
            while(delay_cnt --);
        }while(ret);
    }

    while(1){
        if(xfer_cnt == NUM_TRANSFERS_PER_CORE * NUM_CORES)
            break;
        riscv_wfi();
    }

    return 0;
}
