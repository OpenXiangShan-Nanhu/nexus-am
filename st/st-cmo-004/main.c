#include <am.h>
#include <platform.h>
#include <klib.h>
#include <klib-macros.h>
#include <plic.h>
#include <xsextra.h>
#include "dw_axi_dmac.h"

#define STRESS  // Stress mode, but it takes much longer

#define NUM_CORES 4

#ifdef STRESS
    #define NUM_TRANSFERS_PER_CORE 200
    #define MAX_TRANSFER_SIZE 8192  // 8KB
#else
    #define NUM_TRANSFERS_PER_CORE 5
    #define MAX_TRANSFER_SIZE 1024  // 1KB
#endif

static uint8_t src_buffers[NUM_CORES * NUM_TRANSFERS_PER_CORE][MAX_TRANSFER_SIZE] __attribute__((aligned(64)));
static uint8_t dst_buffers_cached[NUM_CORES * NUM_TRANSFERS_PER_CORE / 2][MAX_TRANSFER_SIZE] __attribute__((aligned(64)));
static uint8_t dst_buffers_uncached[NUM_CORES * NUM_TRANSFERS_PER_CORE / 2][MAX_TRANSFER_SIZE] __attribute__((aligned(64)));
static volatile uint64_t xfer_cnt = 0;
static volatile uintptr_t xfer_cnt_lock = 0;
static volatile bool failed = false;
static volatile uint64_t transfer_size[NUM_CORES][NUM_TRANSFERS_PER_CORE];

void dma_test_callback(int error_code, uint64_t user_data){
    uint64_t xferid = user_data;
    uint64_t dstid = xferid >> 1;
    uint64_t hartid = riscv_mhartid();
    uint8_t (*dst_buffers)[MAX_TRANSFER_SIZE];
    
    if(xferid % 2)
        dst_buffers = dst_buffers_uncached;
    else{
        dst_buffers = dst_buffers_cached;
    }

    int mismatch = 0;
    for(int i = 0; i < transfer_size[hartid][xferid % 200]; i++){
        if(src_buffers[xferid][i] != dst_buffers[dstid][i]) {
            if(xferid % 2 == 1 && xferid / NUM_TRANSFERS_PER_CORE == hartid){
                atomic_printf("Core %d: Transfer %d verified successfully.\n", hartid, xferid);
                mismatch = 1;
                break;
            }
            else{
                atomic_printf("Core %d: Transfer %d verified failed!!!\n", hartid, xferid);
                atomic_printf("Xferid:%d, Src Data:0x%04x, Dst Data:0x%04x, Index:%d, src_address:%p, dst_address:%p\n", xferid, src_buffers[xferid][i], dst_buffers[dstid][i], i, (void*)(&src_buffers[xferid][i]), (void*)(&dst_buffers[dstid][i]));
                mismatch = 1;
                failed = true;
                break;
            }
        }
    }

    if(!mismatch){
        if(xferid % 2 == 1 && xferid / NUM_TRANSFERS_PER_CORE == hartid){
            atomic_printf("Core %d: Transfer %d verified failed!!!\n", hartid, xferid);
            failed = true;
        }
        else{
            atomic_printf("Core %d: Transfer %d verified successfully.\n", hartid, xferid);
        }
    }
        
    lock_acquire(&xfer_cnt_lock);
    xfer_cnt++;
    lock_release(&xfer_cnt_lock);
}

int main(){
    uint64_t hartid = riscv_mhartid();
    uint64_t xferid, dstid;
    int ret, delay_cnt;

    dma_init();
    _barrier();

    srand(12345);

    for(int i = 0; i < NUM_TRANSFERS_PER_CORE; i++){
        xferid = hartid * NUM_TRANSFERS_PER_CORE + i;
        transfer_size[hartid][xferid % 200] = rand_in_range(32, MAX_TRANSFER_SIZE);
        transfer_size[hartid][xferid % 200] -= transfer_size[hartid][xferid % 200] % 32;
        dstid = xferid >> 1;
        memset(src_buffers[xferid], (uint8_t)(1 + i + xferid), transfer_size[hartid][xferid % 200]);
        // memset(dst_buffers[xferid], 0, transfer_size[hartid]);
        while(1) {
            if(xferid % 2)
                ret = dma_transfer(DMA_MEM_TO_MEM, (uintptr_t)src_buffers[xferid], (uintptr_t)(dst_buffers_uncached[dstid]), transfer_size[hartid][xferid % 200], DWAXIDMAC_AX_CACHE_NONCACHE, DWAXIDMAC_AX_CACHE_NONCACHE, dma_test_callback, xferid);
            else
                ret = dma_transfer(DMA_MEM_TO_MEM, (uintptr_t)src_buffers[xferid], (uintptr_t)(dst_buffers_cached[dstid]), transfer_size[hartid][xferid % 200], DWAXIDMAC_AX_CACHE_CACHEABLE, DWAXIDMAC_AX_CACHE_CACHEABLE, dma_test_callback, xferid);    
            
            // if submit failed, delay and resubmit
            if(ret){
                delay_cnt = 1000;
                while(delay_cnt--);
            }
            else{
                break;
            }
        }
    }

    while(1){
        if(xfer_cnt == NUM_TRANSFERS_PER_CORE * NUM_CORES)
            break;
        riscv_wfi();
    }

    if(failed)
        atomic_printf("TEST CASE FAILED!!!\n");

    return 0;
}
