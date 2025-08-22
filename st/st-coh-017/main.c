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

#define SHARED_MEM_BASE     0x80110000
#define DEST_MEM_BASE       0x80120000
#define SHARED_MEM_SIZE     MAX_TRANSFER_SIZE * NUM_CHANNELS

static volatile uint32_t *src_addr = (volatile uint32_t *)SHARED_MEM_BASE;
static volatile uint32_t *region_a_addr = (volatile uint32_t *)DEST_MEM_BASE + 0 * MAX_TRANSFER_SIZE;
static volatile uint32_t *region_b_addr = (volatile uint32_t *)DEST_MEM_BASE + 1 * MAX_TRANSFER_SIZE;
static volatile uint32_t *region_c_addr = (volatile uint32_t *)DEST_MEM_BASE + 2 * MAX_TRANSFER_SIZE;
static volatile uint32_t *region_d_addr = (volatile uint32_t *)DEST_MEM_BASE + 3 * MAX_TRANSFER_SIZE;

static volatile uint32_t xfer_cnt = 0;
static volatile uint64_t xfer_cnt_lock = 0;
static volatile uint32_t complete_flag[NUM_CORES] = {0};

static size_t transfer_size;

void dma_test_callback(int error_code, uint64_t user_data){
    lock_acquire(&xfer_cnt_lock);
    xfer_cnt++;
    lock_release(&xfer_cnt_lock);

    complete_flag[xfer_cnt % 4] = 1;
    riscv_fence();

    atomic_printf("interrupt!!! transfer id: %d\n", user_data);
}

bool reader_check(int hartid){
    uint32_t *dest_addr = (uint32_t *)DEST_MEM_BASE + hartid * MAX_TRANSFER_SIZE;

    for(int k = 0; k < transfer_size; k++){
        if(src_addr[k] != dest_addr[k]){
            if(hartid < 2){
                atomic_printf("FAILED!!! Index:%d, src data:%x, dest data:%x\n", k, src_addr[k], dest_addr[k]);
                return true;
            }else{
                return false;
            }
        }
    }

    if(hartid > 1)
        return true;
    else
        return false;
}

int main() {
    int hartid = riscv_mhartid();
    bool flag[NUM_CORES] = {false};

    dma_init();
    srand(0x1234);

    for(int i = 0; i < LITERATION; i++){
        if(hartid == 0){
            transfer_size = rand_in_range(TRANSFER_ITEM_SIZE, MAX_TRANSFER_SIZE);
            transfer_size -= transfer_size % TRANSFER_ITEM_SIZE;
            
            memset((void*)SHARED_MEM_BASE, 0, MAX_TRANSFER_SIZE);

            for(int j = 0; j < transfer_size / 4; j++)
                ((uint32_t*)src_addr)[j] = rand();
            riscv_fence();

            barrier(NUM_CORES);

            memset((void*)region_a_addr, 0, SHARED_MEM_SIZE);
            
            dma_transfer(
                DMA_MEM_TO_MEM,
                (uintptr_t)src_addr,
                (uintptr_t)region_a_addr,
                transfer_size,
                DWAXIDMAC_AX_CACHE_CACHEABLE,
                DWAXIDMAC_AX_CACHE_CACHEABLE,
                dma_test_callback,
                0 + i * NUM_CORES
            );
    
            while(1){
                if(complete_flag[hartid] == 1)
                    break;
            }

            flag[hartid] = reader_check(hartid);
            barrier(NUM_CORES);

            if(flag[hartid] == 1)
                break;

            complete_flag[hartid] = 0;
            riscv_fence();
        } else if(hartid == 1){
            barrier(NUM_CORES);
            memset((void*)region_b_addr, 0, SHARED_MEM_SIZE);

            dma_transfer(
                DMA_MEM_TO_MEM,
                (uintptr_t)src_addr,
                (uintptr_t)region_b_addr,
                transfer_size,
                DWAXIDMAC_AX_CACHE_CACHEABLE,
                DWAXIDMAC_AX_CACHE_CACHEABLE,
                dma_test_callback,
                1 + i * NUM_CORES
            );

            while(1){
                if(complete_flag[hartid] == 1)
                    break;
            }

            flag[hartid] = reader_check(hartid);
            barrier(NUM_CORES);

            if(flag[hartid] == 1)
                break;

            complete_flag[hartid] = 0;
            riscv_fence();
        } else if(hartid == 2){
            barrier(NUM_CORES);
            memset((void*)region_c_addr, 0, SHARED_MEM_SIZE);

            dma_transfer(
                DMA_MEM_TO_MEM,
                (uintptr_t)src_addr,
                (uintptr_t)region_c_addr,
                transfer_size,
                DWAXIDMAC_AX_CACHE_CACHEABLE,
                DWAXIDMAC_AX_CACHE_NONCACHE,
                dma_test_callback,
                2 + i * NUM_CORES
            );

            while(1){
                if(complete_flag[hartid] == 1)
                    break;
            }

            flag[hartid] = reader_check(hartid);
            barrier(NUM_CORES);

            if(flag[hartid] == 1)
                break;

            complete_flag[hartid] = 0;
            riscv_fence();
        } else if(hartid == 3){
            barrier(NUM_CORES);
            memset((void*)region_d_addr, 0, SHARED_MEM_SIZE);

            dma_transfer(
                DMA_MEM_TO_MEM,
                (uintptr_t)src_addr,
                (uintptr_t)region_d_addr,
                transfer_size,
                DWAXIDMAC_AX_CACHE_CACHEABLE,
                DWAXIDMAC_AX_CACHE_NONCACHE,
                dma_test_callback,
                3 + i * NUM_CORES
            );

            while(1){
                if(complete_flag[hartid] == 1)
                    break;
            }

            flag[hartid] = reader_check(hartid);
            barrier(NUM_CORES);

            if(flag[hartid] == 1)
                break;

            complete_flag[hartid] = 0;
            riscv_fence();
        }
    }

    if(flag[0] == false && flag[1] == false && flag[2] == false && flag[3] == false)
        atomic_printf("SUCCESS!\n");
    else
        atomic_printf("FAILED!\n");

    return 0;
}