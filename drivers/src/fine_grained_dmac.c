#include <am.h>
#include <platform.h>
#include <klib.h>
#include <klib-macros.h>
#include <mtrap.h>
#include <cmo.h>
#include <csr.h>
#include <plic.h>
#include <stdint.h>
#include "dw_axi_dmac.h" 

#define DMAC_INTR_SOURCE_BASE 250
#define DMA_BASE 0x50070000UL

#define TRANSFER_WITH 32   // data bus width

#define dma_read(i, offset) \
    READ_U64(DMA_BASE + (i) * 0x10000 + (offset))

#define dma_write(i, offset, val) \
    WRITE_U64(DMA_BASE + (i) * 0x10000 + (offset), val)

#define dma_set(i, offset, mask) \
    do { \
        uint64_t addr = DMA_BASE + (i) * 0x10000 + (offset); \
        uint64_t val = READ_U64(addr); \
        val |= mask; \
        WRITE_U64(addr, val); \
    } while(0)

#define dma_clr(i, offset, mask) \
    do { \
        uint64_t addr = DMA_BASE + (i) * 0x10000 + (offset); \
        uint64_t val = READ_U64(addr); \
        val &= ~mask; \
        WRITE_U64(addr, val); \
    } while(0)

#define dma_irq_disable(i)          dma_clr(i, DMAC_CFG, INT_EN_MASK)
#define dma_irq_enable(i)           dma_set(i, DMAC_CFG, INT_EN_MASK)

#define dma_disable(i)              dma_clr(i, DMAC_CFG, DMAC_EN_MASK)
#define dma_enable(i)               dma_set(i, DMAC_CFG, DMAC_EN_MASK)

#define dma_chan_disable(i) \
    dma_set(i, DMAC_CHEN, BIT(DMAC_CHAN_EN_WE_SHIFT)); \
    dma_clr(i, DMAC_CHEN, BIT(DMAC_CHAN_EN_SHIFT))

#define dma_chan_enable(i) \
    dma_set(i, DMAC_CHEN, (BIT(DMAC_CHAN_EN_WE_SHIFT) | BIT(DMAC_CHAN_EN_SHIFT)))
    

#define chan_irq_enable(i, mask)    dma_set(i, CHAN1_BASE + CH_INTSTATUS_ENA, mask)
#define chan_irq_disable(i, mask)   dma_clr(i, CHAN1_BASE + CH_INTSTATUS_ENA, mask)

#define chan_irq_sig_set(i, val)   dma_write(i, CHAN1_BASE + CH_INTSIGNAL_ENA, val)

#define chan_irq_clear(i, val)     dma_write(i, CHAN1_BASE + CH_INTCLEAR, val)

#define chan_cfg_set(i, mask)       dma_set(i, CHAN1_BASE + CH_CFG, mask)
#define chan_cfg_clr(i, mask)       dma_clr(i, CHAN1_BASE + CH_CFG, mask)

#define chan_sar_set(i, val)        dma_write(i, CHAN1_BASE + CH_SAR, val)
#define chan_dar_set(i, val)        dma_write(i, CHAN1_BASE + CH_DAR, val)
#define chan_bts_set(i, val)        dma_write(i, CHAN1_BASE + CH_BLOCK_TS, val)
#define chan_ctl_set(i, val)        dma_write(i, CHAN1_BASE + CH_CTL, val)

void tiny_dma_init(int i){
    dma_irq_disable(i);
    dma_disable(i);

    chan_irq_disable(i, ~0);
    chan_irq_clear(i, ~0);
    dma_chan_disable(i);

    dma_irq_enable(i);
    dma_enable(i);
}

// ONLY FOR SINGLE BLOCK TRANSFER
// ONLY FOR SINGLE MEM_TO_MEM
void tiny_dma_transfer_single_block(
    int i,
    uint64_t src,
    uint64_t dst,
    uint32_t length,
    uint32_t ar_cache, 
    uint32_t aw_cache
){

    assert(length <= DMAC_MAX_BLK_SIZE);

    chan_irq_sig_set(i, DWAXIDMAC_IRQ_DMA_TRF);
    chan_irq_enable(i, (DWAXIDMAC_IRQ_DMA_TRF | DWAXIDMAC_IRQ_SUSPENDED));

    chan_cfg_clr(i, ~0);
    chan_cfg_set(i, 
        ((uint64_t)DWAXIDMAC_MBLK_TYPE_CONTIGUOUS << CH_CFG_L_SRC_MULTBLK_TYPE_POS) |
        ((uint64_t)DWAXIDMAC_MBLK_TYPE_CONTIGUOUS << CH_CFG_L_DST_MULTBLK_TYPE_POS) |
        ((uint64_t)DWAXIDMAC_TT_FC_MEM_TO_MEM_DMAC << (CH_CFG_H_TT_FC_POS+32)) |
        ((uint64_t)DWAXIDMAC_HS_SEL_HW << (CH_CFG_H_HS_SEL_SRC_POS+32)) |
        ((uint64_t)DWAXIDMAC_HS_SEL_HW << (CH_CFG_H_HS_SEL_DST_POS+32)) |
        ((uint64_t)0xf << (CH_CFG_H_SRC_OSR_LMT_POS+32)) |
        ((uint64_t)0xf << (CH_CFG_H_DST_OSR_LMT_POS+32))
    );

    chan_sar_set(i, src);
    chan_dar_set(i, dst);
    chan_bts_set(i, length / TRANSFER_WITH - 1);

    chan_ctl_set(i, 0);
    chan_ctl_set(i, 
        DWAXIDMAC_TRANS_WIDTH_256 << CH_CTL_L_SRC_WIDTH_POS |
        DWAXIDMAC_TRANS_WIDTH_256 << CH_CTL_L_DST_WIDTH_POS |
        (ar_cache & 0xf) << CH_CTL_L_AR_CACHE_POS |
        (aw_cache & 0xf) << CH_CTL_L_AW_CACHE_POS |
        BIT(62) | BIT(63)
    );

    dma_chan_enable(i);
}

void default_dma_handler(int dma_id){
    uint64_t chan_status = dma_read(dma_id, CHAN1_BASE + CH_INTSTATUS);

    if(chan_status & DWAXIDMAC_IRQ_DMA_TRF){
        chan_irq_clear(dma_id, DWAXIDMAC_IRQ_DMA_TRF);
    }
}