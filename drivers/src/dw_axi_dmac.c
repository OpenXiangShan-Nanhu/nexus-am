#include <am.h>
#include <platform.h>
#include <klib.h>
#include <klib-macros.h>
#include <mtrap.h>
#include <cmo.h>
#include <csr.h>
#include <plic.h>
#include "dw_axi_dmac.h" 

#define NUM_CORES 4     // TBD
#define DMAC_INTR_SOURCE_BASE 249

static struct dma_lli g_lli_pool[TOTAL_LLI_POOL_SIZE] __attribute__((aligned(64)));
static struct dma_desc g_desc_pool[TOTAL_DESC_POOL_SIZE];
static struct dma_queue_node g_node_pool[TOTAL_QUEUE_NODES];
static struct dma_task_queue g_dma_task_queue;

static uint32_t g_lli_pool_bitmap[LLI_POOL_BITMAP_SIZE];
static uint32_t g_desc_pool_bitmap[DESC_POOL_BITMAP_SIZE];
static struct dma_queue_node *g_free_node_list_head;

static volatile uint64_t g_lli_pool_lock = 0;
static volatile uint64_t g_desc_pool_lock = 0;
static volatile uint64_t g_node_pool_lock = 0;
static volatile uint64_t _mstatus[NUM_CORES];

struct dma_controller g_dma_controllers[NUM_DMA_CONTROLLERS];

static const uintptr_t DMAC_BASE_ADDRS[NUM_DMA_CONTROLLERS] = {
    0x50070000,
    0x50080000,
    0x50090000,
    0x500A0000,
    0x500B0000,
    0x500C0000
};

void lock_acquire(volatile uint64_t *addr){
    uint64_t hartid = riscv_mhartid();
    uint64_t mstatus;
    __asm__ volatile ("csrr %0, mstatus" : "=r" (mstatus));
    _mstatus[hartid] = mstatus;
    __asm__ volatile("csrci mstatus, 0x8");

    while(compare_and_swap(addr, 0, 1));
}

void lock_release(volatile uint64_t *addr){
    *addr = 0;
    __asm__ volatile("fence");

    uint64_t hartid = riscv_mhartid();
    uint64_t mstatus = _mstatus[hartid];
    __asm__ volatile ("csrw mstatus, %0" : : "r" (mstatus));
}

static inline void chan_iowrite64(struct dma_chan *chan, uint32_t offset, uint64_t val){
    /*
	 * We split one 64 bit write for two 32 bit write as some HW doesn't
	 * support 64 bit access.
	 */
    WRITE_U32(chan->base_addr + offset, (uint64_t)(val & 0xFFFF));
    WRITE_U32(chan->base_addr + offset + 4, (uint64_t)(val & 0xFFFF));
}

static void cfx_cfg_write(struct dma_chan *chan, struct dma_chx_cfg *config) {
    uint32_t cfg_lo = 0, cfg_hi = 0;

    cfg_lo = (config->dst_multblk_type << CH_CFG_L_DST_MULTBLK_TYPE_POS) |
             (config->src_multblk_type << CH_CFG_L_SRC_MULTBLK_TYPE_POS);

    cfg_hi = (config->tt_fc       << CH_CFG_H_TT_FC_POS) |
             (config->hs_sel_src  << CH_CFG_H_HS_SEL_SRC_POS) |
             (config->hs_sel_dst  << CH_CFG_H_HS_SEL_DST_POS) |
             (config->src_per     << CH_CFG_H_SRC_PER_POS) |
             (config->dst_per     << CH_CFG_H_DST_PER_POS) |
             (config->prior       << CH_CFG_H_PRIORITY_POS) |
             (config->src_osr_lmt << CH_CFG_H_SRC_OSR_LMT_POS) |
             (config->dst_osr_lmt << CH_CFG_H_DST_OSR_LMT_POS);
    
    WRITE_U32(chan->base_addr + CH_CFG, cfg_lo);
    WRITE_U32(chan->base_addr + CH_CFG + 4, cfg_hi);
}

// disable the dmac
static void dma_disable(struct dma_controller *dmac){
    uint32_t val;

    val = READ_U32(dmac->base_addr + DMAC_CFG);
    val &= ~DMAC_EN_MASK;
    WRITE_U32(dmac->base_addr + DMAC_CFG, val);
}

// enable the dmac
static void dma_enable(struct dma_controller *dmac){
    uint32_t val;

    val = READ_U32(dmac->base_addr + DMAC_CFG);
    val |= DMAC_EN_MASK;
    WRITE_U32(dmac->base_addr + DMAC_CFG, val);
}

static void dma_irq_disable(struct dma_controller *dmac){
	uint32_t val;

	val = READ_U32(dmac->base_addr + DMAC_CFG);
	val &= ~INT_EN_MASK;
	WRITE_U32(dmac->base_addr + DMAC_CFG, val);
}

static void dma_irq_enable(struct dma_controller *dmac){
	uint32_t val;

	val = READ_U32(dmac->base_addr + DMAC_CFG);
	val |= INT_EN_MASK;
	WRITE_U32(dmac->base_addr + DMAC_CFG, val);
}

static void chan_irq_disable(struct dma_chan *chan, uint32_t irq_mask){
    uint32_t val;

    if(irq_mask == DWAXIDMAC_IRQ_ALL){
        WRITE_U32(chan->base_addr + CH_INTSTATUS_ENA, DWAXIDMAC_IRQ_NONE);
    } else {
        val = READ_U32(chan->base_addr + CH_INTSTATUS_ENA);
        val &= ~irq_mask;
        WRITE_U32(chan->base_addr + CH_INTSTATUS_ENA, val);
    }
}

static void chan_irq_set(struct dma_chan *chan, uint32_t irq_mask){
	WRITE_U32(chan->base_addr + CH_INTSTATUS_ENA, irq_mask);
}

static void chan_irq_sig_set(struct dma_chan *chan, uint32_t irq_mask){
	WRITE_U32(chan->base_addr + CH_INTSIGNAL_ENA, irq_mask);
}

static void chan_irq_clear(struct dma_chan *chan, uint32_t irq_mask){
	WRITE_U32(chan->base_addr + CH_INTCLEAR, irq_mask);
}

// static uint32_t chan_irq_read(struct dma_chan *chan){
// 	return READ_U32(chan->base_addr + CH_INTSTATUS);
// }

static void dma_chan_disable(struct dma_chan *chan){
    uint32_t val;

    struct dma_controller *dmac = (struct dma_controller *)chan->parent_dmac;
    val = READ_U32(dmac->base_addr + DMAC_CHEN);
    val &= ~(BIT(chan->id) << DMAC_CHAN_EN_SHIFT);
    val |= BIT(chan->id) << DMAC_CHAN_EN_WE_SHIFT;
    WRITE_U32(dmac->base_addr + DMAC_CHEN, (uint32_t)val);
}

static void dma_chan_enable(struct dma_chan *chan){
    uint32_t val;

    struct dma_controller *dmac = (struct dma_controller *)chan->parent_dmac;
    val = READ_U32(dmac->base_addr + DMAC_CHEN);
    val |= BIT(chan->id) << DMAC_CHAN_EN_SHIFT | BIT(chan->id) << DMAC_CHAN_EN_WE_SHIFT;
    WRITE_U32(dmac->base_addr + DMAC_CHEN, (uint32_t)val);
}

// static bool dma_chan_is_enabled(struct dma_chan* chan){
//     uint64_t val;

//     struct dma_controller *dmac = (struct dma_controller *)chan->parent_dmac;
//     val = READ_U32(dmac->base_addr + DMAC_CHEN);
    
//     return !!(val & (BIT(chan->id) << DMAC_CHAN_EN_SHIFT));
// }

static void dma_chan_init(struct dma_controller *dmac){
    for (uint32_t i = 0; i < dmac->num_channels; i++) {
        struct dma_chan *chan = &dmac->channels[i];
        chan_irq_disable(chan, DWAXIDMAC_IRQ_ALL);
        chan_irq_clear(chan, DWAXIDMAC_IRQ_ALL);
        dma_chan_disable(chan);
    }
}

static int dma_xfer_width(struct dma_lli *lli, uint32_t src_data_width, uint32_t dst_data_width){
    uint32_t ctl_l = 0;
    const uint32_t clear_mask = (0x7 << CH_CTL_L_SRC_WIDTH_POS) | (0x7 << CH_CTL_L_DST_WIDTH_POS);
    lli->ctl_lo &= ~clear_mask;
    
    switch (src_data_width) {
        case 1:
            ctl_l |= DWAXIDMAC_TRANS_WIDTH_8 << CH_CTL_L_SRC_WIDTH_POS;
            break;
        case 2:
            ctl_l |= DWAXIDMAC_TRANS_WIDTH_16 << CH_CTL_L_SRC_WIDTH_POS;
            break;
        case 4:
            ctl_l |= DWAXIDMAC_TRANS_WIDTH_32 << CH_CTL_L_SRC_WIDTH_POS;
            break;
        case 8:
            ctl_l |= DWAXIDMAC_TRANS_WIDTH_64 << CH_CTL_L_SRC_WIDTH_POS;
            break;
        case 16:
            ctl_l |= DWAXIDMAC_TRANS_WIDTH_128 << CH_CTL_L_SRC_WIDTH_POS;
            break;
        case 32:
            ctl_l |= DWAXIDMAC_TRANS_WIDTH_256 << CH_CTL_L_SRC_WIDTH_POS;
            break;    
        case 64:
            ctl_l |= DWAXIDMAC_TRANS_WIDTH_512 << CH_CTL_L_SRC_WIDTH_POS;
            break;
        default:
            // atomic_printf("Source transfer width %u not supported", src_data_width);
        return -1;
    }

    switch (dst_data_width) {
        case 1:
            ctl_l |= DWAXIDMAC_TRANS_WIDTH_8 << CH_CTL_L_DST_WIDTH_POS;
            break;
        case 2:
            ctl_l |= DWAXIDMAC_TRANS_WIDTH_16 << CH_CTL_L_DST_WIDTH_POS;
            break;
        case 4:
            ctl_l |= DWAXIDMAC_TRANS_WIDTH_32 << CH_CTL_L_DST_WIDTH_POS;
            break;
        case 8:
            ctl_l |= DWAXIDMAC_TRANS_WIDTH_64 << CH_CTL_L_DST_WIDTH_POS;
            break;
        case 16:
            ctl_l |= DWAXIDMAC_TRANS_WIDTH_128 << CH_CTL_L_DST_WIDTH_POS;
            break;
        case 32:
            ctl_l |= DWAXIDMAC_TRANS_WIDTH_256 << CH_CTL_L_DST_WIDTH_POS;
            break;    
        case 64:
            ctl_l |= DWAXIDMAC_TRANS_WIDTH_512 << CH_CTL_L_DST_WIDTH_POS;
            break;
        default:
            // atomic_printf("Destination transfer width %u not supported", src_data_width);
        return -1;
    }

    lli->ctl_lo |= ctl_l;

    return 0;
}

// static void write_chan_llp(struct dma_chan *chan, uint64_t addr){
//     WRITE_U64(chan->base_addr + CH_LLP, addr);
// }

// static int chan_status_read(struct dma_chan *chan){
//     struct dma_controller *dmac = chan->parent_dmac;
//     uint32_t chan_id = chan->id;
//     uint32_t chen = READ_U32(dmac->base_addr + DMAC_CHEN);

//     if(chen & (BIT(chan_id) << DMAC_CHAN_EN_SHIFT)){
//         return DWAXIDMAC_CH_ACTIVE;
//     }

//     if (chen & (BIT(chan_id) << DMAC_CHAN_SUSP_SHIFT)) {
//         return DWAXIDMAC_CH_SUSPENDED;
//     }

//     return DWAXIDMAC_CH_IDLE;
// }

static void dma_controller_init(){
    
    for(int i = 0; i < NUM_DMA_CONTROLLERS; i++){
        struct dma_controller *dmac = &g_dma_controllers[i];
        dmac->base_addr = DMAC_BASE_ADDRS[i];
        dmac->id = i;
        dmac->num_channels = DMAC_MAX_CHANNELS;
        dma_irq_disable(dmac);
        dma_disable(dmac);
        for(int j = 0; j < DMAC_MAX_CHANNELS; j++){
            struct dma_chan *chan = &dmac->channels[j];

            chan->id = j;
            chan->parent_dmac = dmac;
            chan->chan_status = DWAXIDMAC_CH_IDLE;
            chan->base_addr = dmac->base_addr + COMMON_REG_LEN + j * CHAN_REG_LEN;
        }
        dma_chan_init(dmac);
        dma_irq_enable(dmac);
        dma_enable(dmac);
    }
}

static void dma_pools_init(void){
    memset(g_lli_pool, 0, sizeof(g_lli_pool));
    memset(g_lli_pool_bitmap, 0, sizeof(g_lli_pool_bitmap));
    
    memset(g_desc_pool, 0, sizeof(g_desc_pool));
    memset(g_desc_pool_bitmap, 0, sizeof(g_desc_pool_bitmap));
}

static struct dma_desc *dma_alloc_desc(void){
    struct dma_desc *desc = NULL;
    int allocated_index = -1;
    bool found = false;

    lock_acquire(&g_desc_pool_lock);

    for (int i = 0; i < DESC_POOL_BITMAP_SIZE && !found; i++) {
        if (g_desc_pool_bitmap[i] != 0xFFFFFFFF) {
            for (int j = 0; j < 32; j++) {
                if (!(g_desc_pool_bitmap[i] & (1U << j))) {
                    int index = i * 32 + j;
                    if (index < TOTAL_DESC_POOL_SIZE) {
                        g_desc_pool_bitmap[i] |= (1U << j);
                        allocated_index = index;
                        found = true;
                        break;
                    }
                }
            }
        }
    }

    lock_release(&g_desc_pool_lock);

    if(allocated_index != -1){
        desc = &g_desc_pool[allocated_index];
        memset(desc, 0, sizeof(struct dma_desc));
    } else {
        // atomic_printf("DMA Descriptor Pool is full!");
    }

    return desc;
}

static void dma_free_desc(struct dma_desc *desc){
    if (!desc) {
        return;
    }

    int index = desc - g_desc_pool;

    if (index >= 0 && index < TOTAL_DESC_POOL_SIZE) {
        int i = index / 32;
        int j = index % 32;

        lock_acquire(&g_desc_pool_lock);

        g_desc_pool_bitmap[i] &= ~(1U << j);

        lock_release(&g_desc_pool_lock);
    } else {
        // atomic_printf("Attempt to free a descriptor not belonging to the pool.");
    }
}

struct dma_lli *dma_alloc_lli_chain(uint32_t num_llc) {
    uint32_t search_start = 0;
    bool block_found = false;
    struct dma_lli *start_ptr = NULL;

    if (num_llc == 0 || num_llc > TOTAL_LLI_POOL_SIZE) {
        return NULL;
    }
    
    lock_acquire(&g_lli_pool_lock);

    for (uint32_t i = 0; i <= TOTAL_LLI_POOL_SIZE - num_llc; i++) {
        bool is_block_free = true;
        for (uint32_t j = 0; j < num_llc; j++) {
            uint32_t current_index = i + j;
            if (g_lli_pool_bitmap[current_index / 32] & (1U << (current_index % 32))) {
                is_block_free = false;
                i = current_index; 
                break;
            }
        }
        
        if (is_block_free) {
            search_start = i;
            block_found = true;
            break;
        }
    }

    if (block_found) {
        for (uint32_t i = 0; i < num_llc; i++) {
            uint32_t current_index = search_start + i;
            g_lli_pool_bitmap[current_index / 32] |= (1U << (current_index % 32));
        }

        // platform_lock_release(&g_pool_lock);
        
        start_ptr = &g_lli_pool[search_start];
    }

    lock_release(&g_lli_pool_lock);
    
    if(start_ptr)
        memset(start_ptr, 0, sizeof(struct dma_lli) * num_llc);
    // else
        // atomic_printf("No contiguous block of %u LLIs available in the pool.\n");

    return start_ptr; 
}

static void dma_free_lli_chain(struct dma_lli *start_lli, uint32_t num_llc){
    if (!start_lli || num_llc == 0) {
        return;
    }

    int start_index = start_lli - g_lli_pool;

    if (start_index < 0 || (start_index + num_llc) > TOTAL_LLI_POOL_SIZE) {
        // atomic_printf("Attempt to free an invalid LLI chain.");
        return;
    }

    lock_acquire(&g_lli_pool_lock);

    for (uint32_t i = 0; i < num_llc; i++) {
        uint32_t current_index = start_index + i;
        g_lli_pool_bitmap[current_index / 32] &= ~(1U << (current_index % 32));
    }

    lock_release(&g_lli_pool_lock);
}

static int dma_prepare_llp(struct dma_desc *desc, uint64_t src_addr, uint64_t dst_addr, uint32_t total_length, uint32_t ar_cache, uint32_t aw_cache){
    uint32_t num_blocks = 0;
    uint32_t remaining_length = total_length;
    struct dma_lli *current_lli, *prev_lli = NULL;

    num_blocks = (total_length + DMAC_MAX_BLK_SIZE - 1) / DMAC_MAX_BLK_SIZE;
    desc->num_blocks = num_blocks;

    current_lli = dma_alloc_lli_chain(num_blocks);
    if(!current_lli) {
        // atomic_printf("Failed to allocate new lli\n");
        return -1;
    }

    desc->first_hw_desc = current_lli;
    
    for(uint32_t i = 0; i < num_blocks; i++) {
        uint32_t block_size = (remaining_length > DMAC_MAX_BLK_SIZE) ? DMAC_MAX_BLK_SIZE : remaining_length;
        current_lli->sar = src_addr;
        current_lli->dar = dst_addr;
        current_lli->block_ts_lo = block_size / 32 - 1;
        current_lli->ctl_lo = 0;
        current_lli->ctl_hi = 0;

        // Default Configuration
        // TBD:
        dma_xfer_width(current_lli, 32, 32);
        current_lli->ctl_lo |= (DWAXIDMAC_CH_CTL_L_INC << CH_CTL_L_SRC_INC_POS);
        current_lli->ctl_lo |= (DWAXIDMAC_CH_CTL_L_INC << CH_CTL_L_DST_INC_POS);
        current_lli->ctl_lo |= (ar_cache << CH_CTL_L_AR_CACHE_POS);
        current_lli->ctl_lo |= (aw_cache << CH_CTL_L_AW_CACHE_POS);
        current_lli->ctl_hi |= CH_CTL_H_LLI_VALID;
        // current_lli->ctl_hi |= CH_CTL_H_ARLEN_EN;
        // current_lli->ctl_hi |= (DWAXIDMAC_ARWLEN_2 << CH_CTL_H_ARLEN_POS);
        // current_lli->ctl_hi |= CH_CTL_H_AWLEN_EN;
        // current_lli->ctl_hi |= (DWAXIDMAC_ARWLEN_2 << CH_CTL_H_AWLEN_POS);

        if(prev_lli != NULL) {
            prev_lli->llp = (uint64_t)current_lli;
            mem_flush((const volatile uint8_t *)prev_lli, sizeof(struct dma_lli));
        }

        remaining_length -= block_size;
        src_addr += block_size;
        dst_addr += block_size;
        prev_lli = current_lli;
        current_lli++;
    }

    if (prev_lli != NULL) {
        prev_lli->llp = 0x0;
        prev_lli->ctl_hi |= CH_CTL_H_LLI_LAST;
        mem_flush((const volatile uint8_t *)prev_lli, sizeof(struct dma_lli));
    }

    return 0;
}

static void dma_node_pool_init(void){
    for (int i = 0; i < TOTAL_QUEUE_NODES - 1; i++) {
        g_node_pool[i].next = &g_node_pool[i+1];
    }

    g_node_pool[TOTAL_QUEUE_NODES - 1].next = NULL;

    g_free_node_list_head = &g_node_pool[0];
}

static struct dma_queue_node *alloc_queue_node(void) {
    struct dma_queue_node *node = NULL;

    lock_acquire(&g_node_pool_lock);

    if(g_free_node_list_head) {
        node = g_free_node_list_head;
        g_free_node_list_head = node->next;
    }

    lock_release(&g_node_pool_lock);

    if(node){
        node->next = NULL;
    }else{
        // atomic_printf("Queue node pool is empty\n");
    }

    return node;
}

static void free_queue_node(struct dma_queue_node *node){
    if (!node) {
        return;
    }

    lock_acquire(&g_node_pool_lock);

    node->next = g_free_node_list_head;
    g_free_node_list_head = node;

    lock_release(&g_node_pool_lock);
}

static struct dma_chan *dma_find_free_channel(void){
    for (int i = 0; i < NUM_DMA_CONTROLLERS; i++) {
        struct dma_controller *dmac = &g_dma_controllers[i];
        for (int j = 0; j < dmac->num_channels; j++) {
            struct dma_chan *chan = &dmac->channels[j];
            if (chan->chan_status == DWAXIDMAC_CH_IDLE) {
                chan->chan_status = DWAXIDMAC_CH_ACTIVE;
                return chan;
            }
        }
    }

    return NULL;
}

#ifdef PREFETCH_DATA_BEFORE_DMA
static void prefetch2cache(struct dma_chan *chan, struct dma_lli *lli){
    uint32_t length = chan->active_desc->length;
    uintptr_t dst_addr = lli->dar;

    const volatile uint8_t *ptr = (const volatile uint8_t *)dst_addr;
    uint64_t dummy_read __attribute__((unused));

    for(int i = 0; i < length; i++)
        dummy_read = ptr[i]; 
}
#endif

static void chan_single_block_init(struct dma_chan *chan, struct dma_lli *lli){
    /* CH_SAR */
    WRITE_U64(chan->base_addr + CH_SAR, lli->sar);
    
    /* CH_DAR */
    WRITE_U64(chan->base_addr + CH_DAR, lli->dar);

    /* CH_BLOCK_TS */
    WRITE_U32(chan->base_addr + CH_BLOCK_TS, lli->block_ts_lo);
    WRITE_U32(chan->base_addr + CH_BLOCK_TS + 4, lli->block_ts_hi);

    /* CH_CTL */
    WRITE_U32(chan->base_addr + CH_CTL_L, lli->ctl_lo);
    WRITE_U32(chan->base_addr + CH_CTL_H, lli->ctl_hi);
}

static void chan_xfer_start(struct dma_chan *chan, struct dma_lli *first){
    struct dma_chx_cfg config = {};
    uint32_t irq_mask;
    // uint8_t lms = 0;    /* Select AXI0 master for LLI fetching */

    // if(dma_chan_is_enabled(chan))
        // atomic_printf("dma chan %d has already been enabled\n", chan->id);

    // config.dst_multblk_type = DWAXIDMAC_MBLK_TYPE_LL;
	// config.src_multblk_type = DWAXIDMAC_MBLK_TYPE_LL;
    config.src_multblk_type = DWAXIDMAC_MBLK_TYPE_CONTIGUOUS;
    config.dst_multblk_type = DWAXIDMAC_MBLK_TYPE_CONTIGUOUS;
	config.tt_fc = DWAXIDMAC_TT_FC_MEM_TO_MEM_DMAC;
	config.prior = chan->parent_dmac->hdata->priority[chan->id];
    // default hs_sel
	config.hs_sel_dst = DWAXIDMAC_HS_SEL_HW;
	config.hs_sel_src = DWAXIDMAC_HS_SEL_HW;
    config.src_osr_lmt = 0xf;
    config.dst_osr_lmt = 0xf;
    switch(chan->direction){
    /* no peripherals connected to the DMAC for now */
    case DMA_MEM_TO_DEV:
		break;
	case DMA_DEV_TO_MEM:
		break;
	default:
		break;
    }

    /* Generate 'Complete' and 'Error' interrupt*/
    irq_mask = DWAXIDMAC_IRQ_DMA_TRF | DWAXIDMAC_IRQ_ALL_ERR;
	chan_irq_sig_set(chan, irq_mask);

    /* Generate 'suspend' status but don't generate interrupt */
	irq_mask |= DWAXIDMAC_IRQ_SUSPENDED;
	chan_irq_set(chan, irq_mask);

    cfx_cfg_write(chan, &config);

    /* LLI MODE */
    //write_chan_llp(chan, (uintptr_t)first | lms);
    
    /* SINGLE MODE */
    chan_single_block_init(chan, first);

    // atomic_printf("transfer %d submit to dmac %d channel %d successfully\n", chan->active_desc->user_data, chan->parent_dmac->id ,chan->id);

    // // atomic_printf("LLP:%x\n", READ_U64(chan->base_addr + CH_LLP));
    // // atomic_printf("SAR:%x\n", READ_U64(chan->base_addr + CH_SAR));
    // // atomic_printf("DAR:%x\n", READ_U64(chan->base_addr + CH_DAR));
    // // atomic_printf("BLOCK_TS:%x\n", READ_U64(chan->base_addr + CH_BLOCK_TS));
    // // atomic_printf("CFG:%x\n", READ_U64(chan->base_addr + CH_CFG));
    // // atomic_printf("CTL:%x\n", READ_U64(chan->base_addr + CH_CTL));
    
    #ifdef PREFETCH_DATA_BEFORE_DMA
        prefetch2cache(chan, first);
    #endif

    dma_chan_enable(chan);
}

// // TBD
// static int dma_chan_pause(struct dma_chan *chan){
//     unsigned long flags;
// 	unsigned int timeout = 20; /* timeout iterations */
//     uint32_t val;
//     struct dma_controller *dmac = (struct dma_controller *)chan->parent_dmac;



//     val = READ_U32(dmac->base_addr + DMAC_CHEN);
//     val |= BIT(chan->id) << DMAC_CHAN_SUSP_SHIFT | BIT(chan->id) << DMAC_CHAN_SUSP_WE_SHIFT;
//     WRITE_U32(dmac->base_addr + DMAC_CHEN, val);

//     do  {
// 		if (chan_irq_read(chan) & DWAXIDMAC_IRQ_SUSPENDED)
// 			break;
//         riscv_delay_us(2);
// 	} while (--timeout);

//     chan_irq_clear(chan, DWAXIDMAC_IRQ_SUSPENDED);

//     chan->is_paused = true;

//     // TBD spinlock

//     return timeout ? 0 : -1;
// }

// // TBD
// static void dma_chan_resume(dma_chan *chan){
//     uint32_t val;
//     dma_controller *dmac = (dma_controller *)chan->parent_dmac;

//     val = READ_U32(dmac->base_addr + DMAC_CHEN);
// 	val &= ~(BIT(chan->id) << DMAC_CHAN_SUSP_SHIFT);
// 	val |=  (BIT(chan->id) << DMAC_CHAN_SUSP_WE_SHIFT);
// 	WRITE_U32(chan->chip, DMAC_CHEN, val);

//     chan->is_paused = false;
// }

static void chan_xfer_complete(struct dma_chan *chan, uint64_t chan_status)
{
    struct dma_desc *completed_desc = chan->active_desc;
    int error_code = 0;

    if (!completed_desc) {
        // atomic_printf("Channel %d completed, but no active descriptor found!\n", chan->id);
        chan->chan_status = DWAXIDMAC_CH_IDLE;
        return;
    }

    if (chan_status & DWAXIDMAC_IRQ_ALL_ERR) {
        error_code = -(int)(chan_status & DWAXIDMAC_IRQ_ALL_ERR);
        atomic_printf("DMA task on channel %d completed with error. Status: 0x%llx\n", chan->id, chan_status);
    }

    completed_desc->is_active = false;
    completed_desc->chan_status = chan_status;

    if (completed_desc->blk_xfer_callback) {
        completed_desc->blk_xfer_callback(error_code, completed_desc->user_data);
    }

    // Free Mem Space
    dma_free_lli_chain(completed_desc->first_hw_desc, completed_desc->num_blocks);    
    dma_free_desc(completed_desc);

    chan->active_desc = NULL;
    chan->chan_status = DWAXIDMAC_CH_IDLE;

    // atomic_printf("DMA task on channel %d completed and resources freed.\n", chan->id);
}

static void dma_schedule_next(void){
    struct dma_queue_node *node_to_run = NULL;
    struct dma_desc *desc_to_run = NULL;
    struct dma_chan *chan = NULL;

    lock_acquire(&g_dma_task_queue.lock);

    if (g_dma_task_queue.head != NULL) {
        chan = dma_find_free_channel();
        if (chan) {
            node_to_run = g_dma_task_queue.head;
            desc_to_run = node_to_run->desc;
            g_dma_task_queue.head = node_to_run->next;
            if (g_dma_task_queue.head == NULL) {
                g_dma_task_queue.tail = NULL;
            }
            g_dma_task_queue.count--;
        }
    }

    lock_release(&g_dma_task_queue.lock);

    if(desc_to_run && chan){
        desc_to_run->chan = chan;
        chan->active_desc = desc_to_run;
        // chan->direction = desc_to_run->direction;
        
        desc_to_run->is_active = true;
        
        chan_xfer_start(chan, desc_to_run->first_hw_desc);
        free_queue_node(node_to_run);
    }
}

static int dma_submit_task(struct dma_desc *desc){
    if(!desc){
        return -1;
    }

    struct dma_queue_node *node = alloc_queue_node();
    if (!node) {
        // atomic_printf("FAILED to allocate new queue node\n");
        return -1;
    }   
    node->desc = desc;
    node->next = NULL;

    lock_acquire(&g_dma_task_queue.lock);

    if (g_dma_task_queue.tail == NULL) {
        g_dma_task_queue.head = node;
        g_dma_task_queue.tail = node;
    } else {
        g_dma_task_queue.tail->next = node;
        g_dma_task_queue.tail = node;
    }
    g_dma_task_queue.count++;

    lock_release(&g_dma_task_queue.lock);
    
    dma_schedule_next();

    return 0;
}

static void dmac_intr_handler(){
    // int error_code = 0;
    uint64_t hartid = riscv_mhartid();
    uint32_t ctx = hartid * 2;
    int chan_id = -1;

    uint32_t intr = READ_U32(CTX_COMP_REG(ctx));
    uint32_t dmac_id = intr - DMAC_INTR_SOURCE_BASE - 1;
    // atomic_printf("Core %d get dmac %d interrupt\n", hartid, dmac_id);

    struct dma_controller *dmac = &g_dma_controllers[dmac_id];

    uint32_t dmac_status = READ_U32(dmac->base_addr + DMAC_INTSTATUS);

    /* get chan id */
    for(uint32_t i = 0; i < dmac->num_channels; i++) 
        if(dmac_status & BIT(i))
            chan_id = i;

    if(chan_id == -1)
        atomic_printf("Illegal chan_id !!!\n");
    // active_desc = chan->active_desc;

    struct dma_chan *chan = &dmac->channels[chan_id];

    uint64_t chan_status = READ_U64(chan->base_addr + CH_INTSTATUS);

    /* handle dma transfer errors if any */
    if (chan_status & DWAXIDMAC_IRQ_ALL_ERR) {
        chan_xfer_complete(chan, chan_status);
        chan_irq_clear(chan, DWAXIDMAC_IRQ_ALL_ERR);
	}

    /* handle block transfer completion */
    /* block interrupt not support now */
	// if (chan_status & DWAXIDMAC_IRQ_BLOCK_TRF) {
	// 	WRITE_U64(chan->base_addr + CH_INTCLEAR, DWAXIDMAC_IRQ_ALL_ERR | DWAXIDMAC_IRQ_BLOCK_TRF);
    //     // TBD
	// }
    
    /* handle dma transfer completion */
	if (chan_status & DWAXIDMAC_IRQ_DMA_TRF) {
        chan_xfer_complete(chan, chan_status);
        chan_irq_clear(chan, DWAXIDMAC_IRQ_DMA_TRF);
	}

    dma_schedule_next();

    WRITE_U32(CTX_COMP_REG(ctx), intr);
}

/* Copy From AM_HOME/case/plic/main.c */
static int setup_plic() {
  plic_init(NUM_CORES);
  for(int i = DMAC_INTR_SOURCE_BASE; i <= NR_INTR; i++) if(setup_intr(i, 7)) return 1;
  for(int i = 0; i < NUM_CORES; i++) {
    uint32_t ctx = i * 2;
    if(setup_context(ctx, 3)) return 1;
    for(int j = DMAC_INTR_SOURCE_BASE; j <= NR_INTR; j++)
        if(j % NUM_CORES == i)
            if(enable_intr(ctx, j)) return 1;
  }
  return 0;
}

static void enable_external_intr(){
    uint64_t mie = csr_read(mie);
    csr_write(mie, mie | MEIE);
  
    uint64_t mstatus = csr_read(mstatus);
    csr_write(mstatus, mstatus | (0x1UL << 3));
}

static int dmac_intr_init() {
  if(setup_plic())
    return 1;

  if(m_trap_handler_register(MEIP, dmac_intr_handler))
    return 1;

  return 0;
}

void dma_init(){
    if(riscv_mhartid() == 0){
        dma_pools_init();
        dma_node_pool_init();
        dma_controller_init();
        dmac_intr_init();
    }
    enable_external_intr();
}

int dma_transfer(enum xfer_direction direction, uint64_t src_addr, uint64_t dst_addr, uint32_t length, uint32_t ar_cache, uint32_t aw_cache, dma_callback call_back, uint64_t user_data){
    int ret = 0;
    struct dma_desc *desc;

    desc = dma_alloc_desc();
    if(desc == NULL){
        // atomic_printf("Failed to allocate new desc, transfer id : %d\n", user_data);
        return -1;
    }

    desc->direction = direction;
    desc->length = length;
    desc->blk_xfer_callback = call_back;
    desc->user_data = user_data;
    desc->is_active = false;

    ret = dma_prepare_llp(desc, src_addr, dst_addr, length, ar_cache, aw_cache);
    if(ret){
        dma_free_desc(desc);
        // atomic_printf("Failed to allocate new llp, transfer id : %d\n", user_data);
        return -1;
    }

    // Won't Failed because the number of desc is equal to the queue size
    ret = dma_submit_task(desc);
    if(ret){
        // atomic_printf("Failed to submit DMA task to queue");
        return -1;
    }

    return 0;
}