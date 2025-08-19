#include <am.h>
#include <platform.h>
#include <klib.h>
#include <klib-macros.h>
#include <plic.h>
#include <xsextra.h>
#include "dw_axi_dmac.h"

#define NUM_CORES 4

#define SHARED_MEM_BASE     0x80010000
#define SHARED_MEM_SIZE     (1024 * 4)
#define LITERATION          10

#define MAX_TRANSFER_SIZE   1024
#define TRANSFER_ITEM_SIZE  32      // Byte

#define DMA_BUFFER_BASE     0x80020000

uint8_t reference[SHARED_MEM_SIZE];
volatile uint32_t xfer_cnt = 0;
volatile uint64_t xfer_cnt_lock = 0;

typedef struct {
    uintptr_t start_addr;
    size_t size;
    uint32_t xferid;
    bool is_active;
} inflight_xfer;
volatile inflight_xfer inflight_table[NUM_DMA_CONTROLLERS * DMAC_MAX_CHANNELS];

void dma_test_callback(int error_code, uint64_t user_data){
    for (int i = 0; i < NUM_DMA_CONTROLLERS * DMAC_MAX_CHANNELS; i++){
        if(inflight_table[i].xferid == user_data) {
            inflight_table[i].is_active = false;
        }
    }

    lock_acquire(&xfer_cnt_lock);
    xfer_cnt++;
    lock_release(&xfer_cnt_lock);

    atomic_printf("interrupt!!! transfer id: %d\n", user_data);
}

void update_reference(uintptr_t src, uintptr_t dest, size_t size) {
    memcpy((void*)&reference[dest - SHARED_MEM_BASE], (void*)src, size);
}

int check_overlap(uintptr_t new_start, size_t new_size) {
    for (int i = 0; i < NUM_DMA_CONTROLLERS * DMAC_MAX_CHANNELS; i++){
        if(inflight_table[i].is_active) {
            uintptr_t active_start = inflight_table[i].start_addr;
            size_t    active_size  = inflight_table[i].size;
            if (new_start < active_start + active_size && active_start < new_start + new_size) {
                return 1;
            }
        }
    }
    return 0;
}

int main() {
    int hartid = riscv_mhartid();
    
    dma_init();
    srand(0x1234);

    if(hartid == 0){
        memset((void*)SHARED_MEM_BASE, 0, SHARED_MEM_SIZE);
        memset(reference, 0, SHARED_MEM_SIZE);

        riscv_fence();

        for(int i = 0; i < LITERATION; i++){
            int is_write = rand() % 2;
            uintptr_t shared_addr;
            size_t transfer_size;

            do {
                transfer_size = rand_in_range(TRANSFER_ITEM_SIZE, MAX_TRANSFER_SIZE);
                transfer_size -= transfer_size % TRANSFER_ITEM_SIZE;
                size_t offset = (rand() % (SHARED_MEM_SIZE - transfer_size)) & ~0x3;
                shared_addr = SHARED_MEM_BASE + offset;
            } while(check_overlap(shared_addr, transfer_size));

            uintptr_t buffer_addr = DMA_BUFFER_BASE + i * MAX_TRANSFER_SIZE;
            uintptr_t src_addr, dest_addr;

            if(is_write) { 
                src_addr = buffer_addr;
                dest_addr = shared_addr;
                for(int j = 0; j < transfer_size / 4; j++)
                    ((uint32_t*)buffer_addr)[j] = rand();

                update_reference(src_addr, dest_addr, transfer_size);
            } else {
                src_addr = shared_addr;
                dest_addr = buffer_addr;
            }

            riscv_fence();

            int ret = dma_transfer(
                DMA_MEM_TO_MEM,
                (uintptr_t)src_addr,
                (uintptr_t)dest_addr,
                transfer_size,
                DWAXIDMAC_AX_CACHE_CACHEABLE,
                DWAXIDMAC_AX_CACHE_CACHEABLE,
                dma_test_callback,
                i
            );

            if(ret){
                uint32_t delay_cnt = 1000;
                while(delay_cnt--);
            }
        }
    }

    while(1){
        if(xfer_cnt == LITERATION)
            break;
        riscv_wfi();
    }    

    // possiable bug
    bool flag = false;
    for(int i = 0; i < SHARED_MEM_SIZE; i++){
        volatile uint8_t *hw_mem_ptr = (volatile uint8_t *)SHARED_MEM_BASE;
        if(reference[i] != hw_mem_ptr[i]){
            flag = true;
            atomic_printf("FAILED!!! Index:%d, rtl data:%x, reference data:%x\n", i, hw_mem_ptr[i], reference[i]);
            break;
        }
    }
    if(!flag)
        atomic_printf("SUCCESS!!!\n");

    return 0;
}