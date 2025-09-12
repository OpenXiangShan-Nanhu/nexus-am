#ifndef __LINKNAN_DMAC_H__
#define __LINKNAN_DMAC_H__

#include <stdint.h>

#define PREFETCH_DATA_BEFORE_DMA

#define DMAC_MAX_CHANNELS	2
#define DMAC_MAX_MASTERS	1
#define DMAC_MAX_SLAVES		1
#define DMAC_MAX_BLK_SIZE	0x200000	// 512KB

#define NUM_DMA_CONTROLLERS		0x6
#define COMMON_REG_LEN			0x100
#define CHAN_REG_LEN			0x100
#define CHAN1_BASE              0x100

#define TOTAL_LLI_POOL_SIZE     32	// set to 256 if lli is support
#define TOTAL_DESC_POOL_SIZE    32
#define TOTAL_QUEUE_NODES       32 
#define LLI_POOL_BITMAP_SIZE    ((TOTAL_LLI_POOL_SIZE + 31) / 32)
#define DESC_POOL_BITMAP_SIZE   ((TOTAL_DESC_POOL_SIZE + 31) / 32)

/* Common registers offset */
#define DMAC_ID			0x000 /* R DMAC ID */
#define DMAC_COMPVER		0x008 /* R DMAC Component Version */
#define DMAC_CFG		0x010 /* R/W DMAC Configuration */
#define DMAC_CHEN		0x018 /* R/W DMAC Channel Enable */
#define DMAC_CHEN_L		0x018 /* R/W DMAC Channel Enable 00-31 */
#define DMAC_CHEN_H		0x01C /* R/W DMAC Channel Enable 32-63 */
#define DMAC_CHSUSPREG		0x020 /* R/W DMAC Channel Suspend */
#define DMAC_CHABORTREG		0x028 /* R/W DMAC Channel Abort */
#define DMAC_INTSTATUS		0x030 /* R DMAC Interrupt Status */
#define DMAC_COMMON_INTCLEAR	0x038 /* W DMAC Interrupt Clear */
#define DMAC_COMMON_INTSTATUS_ENA 0x040 /* R DMAC Interrupt Status Enable */
#define DMAC_COMMON_INTSIGNAL_ENA 0x048 /* R/W DMAC Interrupt Signal Enable */
#define DMAC_COMMON_INTSTATUS	0x050 /* R DMAC Interrupt Status */
#define DMAC_RESET		0x058 /* R DMAC Reset Register1 */

/* DMA channel registers offset */
#define CH_SAR			0x000 /* R/W Chan Source Address */
#define CH_DAR			0x008 /* R/W Chan Destination Address */
#define CH_BLOCK_TS		0x010 /* R/W Chan Block Transfer Size */
#define CH_CTL			0x018 /* R/W Chan Control */
#define CH_CTL_L		0x018 /* R/W Chan Control 00-31 */
#define CH_CTL_H		0x01C /* R/W Chan Control 32-63 */
#define CH_CFG			0x020 /* R/W Chan Configuration */
#define CH_CFG_L		0x020 /* R/W Chan Configuration 00-31 */
#define CH_CFG_H		0x024 /* R/W Chan Configuration 32-63 */
#define CH_LLP			0x028 /* R/W Chan Linked List Pointer */
#define CH_STATUS		0x030 /* R Chan Status */
#define CH_SWHSSRC		0x038 /* R/W Chan SW Handshake Source */
#define CH_SWHSDST		0x040 /* R/W Chan SW Handshake Destination */
#define CH_BLK_TFR_RESUMEREQ	0x048 /* W Chan Block Transfer Resume Req */
#define CH_AXI_ID		0x050 /* R/W Chan AXI ID */
#define CH_AXI_QOS		0x058 /* R/W Chan AXI QOS */
#define CH_SSTAT		0x060 /* R Chan Source Status */
#define CH_DSTAT		0x068 /* R Chan Destination Status */
#define CH_SSTATAR		0x070 /* R/W Chan Source Status Fetch Addr */
#define CH_DSTATAR		0x078 /* R/W Chan Destination Status Fetch Addr */
#define CH_INTSTATUS_ENA	0x080 /* R/W Chan Interrupt Status Enable */
#define CH_INTSTATUS		0x088 /* R/W Chan Interrupt Status */
#define CH_INTSIGNAL_ENA	0x090 /* R/W Chan Interrupt Signal Enable */
#define CH_INTCLEAR		0x098 /* W Chan Interrupt Clear */

/* DMAC_CFG */
#define DMAC_EN_POS			0
#define DMAC_EN_MASK			BIT(DMAC_EN_POS)

#define INT_EN_POS			1
#define INT_EN_MASK			BIT(INT_EN_POS)

/* DMAC_CHEN */
#define DMAC_CHAN_EN_SHIFT		0
#define DMAC_CHAN_EN_WE_SHIFT		8

#define DMAC_CHAN_SUSP_SHIFT		16
#define DMAC_CHAN_SUSP_WE_SHIFT		24

/* DMAC_CHEN2 */
#define DMAC_CHAN_EN2_WE_SHIFT		16

/* DMAC CHAN BLOCKS */
#define DMAC_CHAN_BLOCK_SHIFT		32
#define DMAC_CHAN_16			16

/* DMAC_CHSUSP */
#define DMAC_CHAN_SUSP2_SHIFT		0
#define DMAC_CHAN_SUSP2_WE_SHIFT	16

/* CH_CTL_H */
#define CH_CTL_H_ARLEN_EN		BIT(6)
#define CH_CTL_H_ARLEN_POS		7
#define CH_CTL_H_AWLEN_EN		BIT(15)
#define CH_CTL_H_AWLEN_POS		16

enum {
	DWAXIDMAC_ARWLEN_1		= 0,
	DWAXIDMAC_ARWLEN_2		= 1,
	DWAXIDMAC_ARWLEN_4		= 3,
	DWAXIDMAC_ARWLEN_8		= 7,
	DWAXIDMAC_ARWLEN_16		= 15,
	DWAXIDMAC_ARWLEN_32		= 31,
	DWAXIDMAC_ARWLEN_64		= 63,
	DWAXIDMAC_ARWLEN_128	= 127,
	DWAXIDMAC_ARWLEN_256	= 255,
	DWAXIDMAC_ARWLEN_MIN	= DWAXIDMAC_ARWLEN_1,
	DWAXIDMAC_ARWLEN_MAX	= DWAXIDMAC_ARWLEN_256
};

#define CH_CTL_H_LLI_LAST		BIT(30)
#define CH_CTL_H_LLI_VALID		BIT(31)

/* CH_CTL_L */
#define CH_CTL_L_LAST_WRITE_EN		BIT(30)

#define CH_CTL_L_AR_CACHE_POS		22
#define CH_CTL_L_AW_CACHE_POS		26		

enum {
	DWAXIDMAC_AX_CACHE_DEVICE = 0,
	DWAXIDMAC_AX_CACHE_NONCACHE = 2,
	DWAXIDMAC_AX_CACHE_CACHEABLE = 15
};


#define CH_CTL_L_DST_MSIZE_POS		18
#define CH_CTL_L_SRC_MSIZE_POS		14

enum {
	DWAXIDMAC_BURST_TRANS_LEN_1	= 0,
	DWAXIDMAC_BURST_TRANS_LEN_4,
	DWAXIDMAC_BURST_TRANS_LEN_8,
	DWAXIDMAC_BURST_TRANS_LEN_16,
	DWAXIDMAC_BURST_TRANS_LEN_32,
	DWAXIDMAC_BURST_TRANS_LEN_64,
	DWAXIDMAC_BURST_TRANS_LEN_128,
	DWAXIDMAC_BURST_TRANS_LEN_256,
	DWAXIDMAC_BURST_TRANS_LEN_512,
	DWAXIDMAC_BURST_TRANS_LEN_1024
};

#define CH_CTL_L_DST_WIDTH_POS		11
#define CH_CTL_L_SRC_WIDTH_POS		8

#define CH_CTL_L_DST_INC_POS		6
#define CH_CTL_L_SRC_INC_POS		4
enum {
	DWAXIDMAC_CH_CTL_L_INC		= 0,
	DWAXIDMAC_CH_CTL_L_NOINC
};

#define CH_CTL_L_DST_MAST		BIT(2)
#define CH_CTL_L_SRC_MAST		BIT(0)

/* CH_CFG_H */
#define CH_CFG_H_DST_OSR_LMT_POS	27
#define CH_CFG_H_SRC_OSR_LMT_POS	23
#define CH_CFG_H_PRIORITY_POS		17
#define CH_CFG_H_DST_PER_POS		12
#define CH_CFG_H_SRC_PER_POS		7
#define CH_CFG_H_HS_SEL_DST_POS		4
#define CH_CFG_H_HS_SEL_SRC_POS		3
enum {
	DWAXIDMAC_HS_SEL_HW		= 0,
	DWAXIDMAC_HS_SEL_SW
};

#define CH_CFG_H_TT_FC_POS		0
enum {
	DWAXIDMAC_TT_FC_MEM_TO_MEM_DMAC	= 0,
	DWAXIDMAC_TT_FC_MEM_TO_PER_DMAC,
	DWAXIDMAC_TT_FC_PER_TO_MEM_DMAC,
	DWAXIDMAC_TT_FC_PER_TO_PER_DMAC,
	DWAXIDMAC_TT_FC_PER_TO_MEM_SRC,
	DWAXIDMAC_TT_FC_PER_TO_PER_SRC,
	DWAXIDMAC_TT_FC_MEM_TO_PER_DST,
	DWAXIDMAC_TT_FC_PER_TO_PER_DST
};

/* CH_CFG_L */
#define CH_CFG_L_DST_MULTBLK_TYPE_POS	2
#define CH_CFG_L_SRC_MULTBLK_TYPE_POS	0
enum {
	DWAXIDMAC_MBLK_TYPE_CONTIGUOUS	= 0,
	DWAXIDMAC_MBLK_TYPE_RELOAD,
	DWAXIDMAC_MBLK_TYPE_SHADOW_REG,
	DWAXIDMAC_MBLK_TYPE_LL
};

/* XFER DIRECTION*/
enum xfer_direction{
	DMA_MEM_TO_MEM,
	DMA_MEM_TO_DEV,
	DMA_DEV_TO_MEM,
	DMA_DEV_TO_DEV,
	DMA_TRANS_NONE,
};

/* CH_STATUS */
enum {
	DWAXIDMAC_CH_IDLE,
	DWAXIDMAC_CH_SUSPENDED,
	DWAXIDMAC_CH_ABORT,
	DWAXIDMAC_CH_ACTIVE
};

// /* CH_CFG2 */
// #define CH_CFG2_L_SRC_PER_POS		4
// #define CH_CFG2_L_DST_PER_POS		11

// #define CH_CFG2_H_TT_FC_POS		0
// #define CH_CFG2_H_HS_SEL_SRC_POS	3
// #define CH_CFG2_H_HS_SEL_DST_POS	4
// #define CH_CFG2_H_PRIORITY_POS		20

/**
 * DW AXI DMA channel interrupts
 *
 * @DWAXIDMAC_IRQ_NONE: Bitmask of no one interrupt
 * @DWAXIDMAC_IRQ_BLOCK_TRF: Block transfer complete
 * @DWAXIDMAC_IRQ_DMA_TRF: Dma transfer complete
 * @DWAXIDMAC_IRQ_SRC_TRAN: Source transaction complete
 * @DWAXIDMAC_IRQ_DST_TRAN: Destination transaction complete
 * @DWAXIDMAC_IRQ_SRC_DEC_ERR: Source decode error
 * @DWAXIDMAC_IRQ_DST_DEC_ERR: Destination decode error
 * @DWAXIDMAC_IRQ_SRC_SLV_ERR: Source slave error
 * @DWAXIDMAC_IRQ_DST_SLV_ERR: Destination slave error
 * @DWAXIDMAC_IRQ_LLI_RD_DEC_ERR: LLI read decode error
 * @DWAXIDMAC_IRQ_LLI_WR_DEC_ERR: LLI write decode error
 * @DWAXIDMAC_IRQ_LLI_RD_SLV_ERR: LLI read slave error
 * @DWAXIDMAC_IRQ_LLI_WR_SLV_ERR: LLI write slave error
 * @DWAXIDMAC_IRQ_INVALID_ERR: LLI invalid error or Shadow register error
 * @DWAXIDMAC_IRQ_MULTIBLKTYPE_ERR: Slave Interface Multiblock type error
 * @DWAXIDMAC_IRQ_DEC_ERR: Slave Interface decode error
 * @DWAXIDMAC_IRQ_WR2RO_ERR: Slave Interface write to read only error
 * @DWAXIDMAC_IRQ_RD2RWO_ERR: Slave Interface read to write only error
 * @DWAXIDMAC_IRQ_WRONCHEN_ERR: Slave Interface write to channel error
 * @DWAXIDMAC_IRQ_SHADOWREG_ERR: Slave Interface shadow reg error
 * @DWAXIDMAC_IRQ_WRONHOLD_ERR: Slave Interface hold error
 * @DWAXIDMAC_IRQ_LOCK_CLEARED: Lock Cleared Status
 * @DWAXIDMAC_IRQ_SRC_SUSPENDED: Source Suspended Status
 * @DWAXIDMAC_IRQ_SUSPENDED: Channel Suspended Status
 * @DWAXIDMAC_IRQ_DISABLED: Channel Disabled Status
 * @DWAXIDMAC_IRQ_ABORTED: Channel Aborted Status
 * @DWAXIDMAC_IRQ_ALL_ERR: Bitmask of all error interrupts
 * @DWAXIDMAC_IRQ_ALL: Bitmask of all interrupts
 */
enum {
	DWAXIDMAC_IRQ_NONE		= 0,
	DWAXIDMAC_IRQ_BLOCK_TRF		= BIT(0),
	DWAXIDMAC_IRQ_DMA_TRF		= BIT(1),
	DWAXIDMAC_IRQ_SRC_TRAN		= BIT(3),
	DWAXIDMAC_IRQ_DST_TRAN		= BIT(4),
	DWAXIDMAC_IRQ_SRC_DEC_ERR	= BIT(5),
	DWAXIDMAC_IRQ_DST_DEC_ERR	= BIT(6),
	DWAXIDMAC_IRQ_SRC_SLV_ERR	= BIT(7),
	DWAXIDMAC_IRQ_DST_SLV_ERR	= BIT(8),
	DWAXIDMAC_IRQ_LLI_RD_DEC_ERR	= BIT(9),
	DWAXIDMAC_IRQ_LLI_WR_DEC_ERR	= BIT(10),
	DWAXIDMAC_IRQ_LLI_RD_SLV_ERR	= BIT(11),
	DWAXIDMAC_IRQ_LLI_WR_SLV_ERR	= BIT(12),
	DWAXIDMAC_IRQ_INVALID_ERR	= BIT(13),
	DWAXIDMAC_IRQ_MULTIBLKTYPE_ERR	= BIT(14),
	DWAXIDMAC_IRQ_DEC_ERR		= BIT(16),
	DWAXIDMAC_IRQ_WR2RO_ERR		= BIT(17),
	DWAXIDMAC_IRQ_RD2RWO_ERR	= BIT(18),
	DWAXIDMAC_IRQ_WRONCHEN_ERR	= BIT(19),
	DWAXIDMAC_IRQ_SHADOWREG_ERR	= BIT(20),
	DWAXIDMAC_IRQ_WRONHOLD_ERR	= BIT(21),
	DWAXIDMAC_IRQ_LOCK_CLEARED	= BIT(27),
	DWAXIDMAC_IRQ_SRC_SUSPENDED	= BIT(28),
	DWAXIDMAC_IRQ_SUSPENDED		= BIT(29),
	DWAXIDMAC_IRQ_DISABLED		= BIT(30),
	DWAXIDMAC_IRQ_ABORTED		= BIT(31),
	DWAXIDMAC_IRQ_ALL_ERR		= (0x003F0000UL | 0x00007FE0UL),	// MASK(21,16) | MASK(14,5)
	DWAXIDMAC_IRQ_ALL		= 0xFFFFFFFFUL
};

enum {
	DWAXIDMAC_TRANS_WIDTH_8		= 0,
	DWAXIDMAC_TRANS_WIDTH_16,
	DWAXIDMAC_TRANS_WIDTH_32,
	DWAXIDMAC_TRANS_WIDTH_64,
	DWAXIDMAC_TRANS_WIDTH_128,
	DWAXIDMAC_TRANS_WIDTH_256,
	DWAXIDMAC_TRANS_WIDTH_512,
	DWAXIDMAC_TRANS_WIDTH_MAX	= DWAXIDMAC_TRANS_WIDTH_512
};

// Forward Declarations 
struct dma_desc;
struct dma_chan;
struct dma_controller;

typedef void (*dma_callback)(int error_code, uint64_t user_data);

struct dma_hcfg {
	uint32_t	nr_channels;    // Number of channels conifgured for this DMAC hardware: 8
	uint32_t	nr_masters;     // Number of AXI master interfaces: 1
	uint32_t	m_data_width;   // Data width of the master interface bus in bits: TBD
	uint32_t	block_size[DMAC_MAX_CHANNELS];  // maximum block transfer size supported by the hardware in each channels: TBD
	uint32_t	priority[DMAC_MAX_CHANNELS];    // hardware priority for each channel: TBD
	/* maximum supported axi burst length */
	uint32_t	axi_rw_burst_len;   // Maximum supported AXI read/write burst length: TBD
};

struct dma_chx_cfg {
	uint8_t src_multblk_type;
    uint8_t dst_multblk_type;
    uint8_t tt_fc;
    uint8_t src_per;
    uint8_t dst_per;
    uint8_t src_burst_len;
    uint8_t dst_burst_len;
	uint8_t hs_sel_src;
	uint8_t hs_sel_dst;
	uint8_t src_osr_lmt;
	uint8_t dst_osr_lmt;
	uint8_t prior;
};

struct dma_chan {
	uintptr_t 			base_addr;				// channel base addr
	uint8_t				id;						// channel id 0-7
	enum xfer_direction		direction;
	uint32_t				chan_status;
	struct dma_desc			*active_desc;		
    struct dma_controller      *parent_dmac;   	// parent_dmac
};

struct dma_controller {
	uintptr_t	base_addr;
	uint8_t		id;
	uint8_t		num_channels;
	uint32_t	hw_cfg_flags;
	struct dma_hcfg 	*hdata;
	struct dma_chan	channels[DMAC_MAX_CHANNELS];
};

/* LLI == Linked List Item */
struct dma_lli {
	uint64_t		sar;
	uint64_t		dar;
	uint32_t		block_ts_lo;
	uint32_t		block_ts_hi;
	uint64_t		llp;
	uint32_t		ctl_lo;
	uint32_t		ctl_hi;
	uint32_t		sstat;
	uint32_t		dstat;
	uint32_t		status_lo;
	uint32_t		status_hi;
	uint32_t		reserved_lo;
	uint32_t		reserved_hi;
};

struct dma_desc {
	volatile bool 		is_active;
	struct dma_lli 		*first_hw_desc;
	struct dma_lli 		*current_hw_desc;
	
	struct dma_chan		*chan;
	dma_callback 	    blk_xfer_callback;
	uint64_t			user_data;

	enum xfer_direction		direction;
	uint32_t			chan_status;
    uint32_t            length;
    uint32_t            num_blocks;	
	uint32_t 			completed_blocks;
};

struct dma_queue_node {
	struct dma_desc *desc;
	struct dma_queue_node *next;
};

struct dma_task_queue {
	struct dma_queue_node *head;
	struct dma_queue_node *tail;
	volatile uint64_t lock;
	uint32_t count;
};

/* USER API */
void dma_init();
int dma_transfer(enum xfer_direction direction, uint64_t src_addr, uint64_t dst_addr, uint32_t length, uint32_t ar_cache, uint32_t aw_cache, dma_callback call_back, uint64_t user_data);
void lock_release(volatile uint64_t *addr);
void lock_acquire(volatile uint64_t *addr);


/* Fine grained dmac api, the alternative and simple implementation use for debugging */
// @param i: dmac id
void tiny_dma_init(int i);
void tiny_dma_transfer_single_block(int i, uint64_t src, uint64_t dst, uint32_t length, uint32_t ar_cache, uint32_t aw_cache);
void default_dma_handler(int i);

#endif /* _AXI_DMA_PLATFORM_H */
