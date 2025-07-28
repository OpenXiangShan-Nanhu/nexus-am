#include <am.h>
#include <platform.h>
#include <klib.h>
#include <klib-macros.h>
#include <plic.h>
#include "dw_axi_dmac.h"

#define NUM_CORES 4
#define NUM_TRANSFERS_PER_CORE 4
#define TRANSFER_SIZE (16 * 1024)   // 16KB

static uint8_t src_buffers[NUM_CORES * NUM_TRANSFERS_PER_CORE][TRANSFER_SIZE] __attribute__((aligned(64)));
static uint8_t dst_buffers[NUM_CORES * NUM_TRANSFERS_PER_CORE][TRANSFER_SIZE] __attribute__((aligned(64)));

void dma_test_callback(int error_code, void *user_data){
    uint64_t xferid = (uint64_t)user_data;
    uint64_t hartid = xferid / 4;

    if (error_code != 0) {
        atomic_printf("Core %d: DMA task failed with hardware error code: %d\n", hartid, error_code);
    } else {
        if (memcmp(src_buffers[xferid], dst_buffers[xferid], TRANSFER_SIZE) != 0) {
            atomic_printf("Core %d: !!! DATA MISMATCH !!! Transfer failed verification.\n", hartid);
        } else {
            atomic_printf("Core %d: Transfer verified successfully.\n", hartid);
        }
    }
}


int main(){
    uint64_t hartid = riscv_mhartid();
    uint64_t xferid;
    int ret;

    if(hartid == 0)
        dma_init();

    for(int i = 0; i < NUM_TRANSFERS_PER_CORE; i++){
        xferid = hartid * NUM_TRANSFERS_PER_CORE + i;
        memset(src_buffers[xferid], (uint8_t)(i + xferid), TRANSFER_SIZE);
        memset(dst_buffers[xferid], 0, TRANSFER_SIZE);
        do {
            atomic_printf("Transfer id : %d", xferid); 
            ret = dma_transfer(DMA_MEM_TO_MEM, (uint64_t)src_buffers[xferid], (uint64_t)dst_buffers[xferid], TRANSFER_SIZE, dma_test_callback, (void *)xferid);
        }while(ret);
    }

    return 0;
}
