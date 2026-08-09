#ifndef __LINKNAN_PLIC_H__
#define __LINKNAN_PLIC_H__

#include "platform.h"
#include <stdint.h>

#define APLIC_M_BASE_ADDR          0x38050000UL
#define APLIC_S_BASE_ADDR          0x38054000UL
#define APLIC_DOMAINCFG(base)      ((base) + 0x0000UL)
#define APLIC_SOURCECFG(base, id)  ((base) + 0x0004UL + 0x4UL * ((id) - 1U))
#define APLIC_SETIPNUM(base)        ((base) + 0x1cdcUL)
#define APLIC_SETIENUM(base)       ((base) + 0x1edcUL)
#define APLIC_CLRIENUM(base)       ((base) + 0x1fdcUL)
#define APLIC_TARGET(base, id)     ((base) + 0x3004UL + 0x4UL * ((id) - 1U))
#define APLIC_DOMAINCFG_DM         (1UL << 2)
#define APLIC_DOMAINCFG_IE         (1UL << 8)
#define APLIC_DOMAINCFG_MSI        (APLIC_DOMAINCFG_DM | APLIC_DOMAINCFG_IE)
#define APLIC_SOURCE_EDGE1         4U
#define APLIC_SOURCE_DELEGATE      (1U << 10)
#define APLIC_GEILEN               7U
#define APLIC_HART_SLOTS           64U

/* APLIC target encoding: HartIndex[13:0], GuestIndex[5:0], and EIID[10:0]. */
#define APLIC_TARGET_VALUE_GUEST(hart, guest, eiid) \
  ((((uint32_t)(hart)) << 18) | (((uint32_t)(guest) & 0x3fU) << 12) | \
   ((uint32_t)(eiid) & 0x7ffU))
#define APLIC_TARGET_VALUE(hart, eiid) \
  APLIC_TARGET_VALUE_GUEST(hart, 0, eiid)
#define APLIC_TARGET_GUEST(value)  (((uint32_t)(value) >> 12) & 0x3fU)

/* Kept for source compatibility with older cases; this is the APLIC M domain. */
#ifndef PLIC_BASE_ADDR
#define PLIC_BASE_ADDR             APLIC_M_BASE_ADDR
#endif
#define MAX_PRIORITY               7

/* AIA machine-level indirect CSRs. */
#define IMSIC_MISELECT_CSR         0x350
#define IMSIC_MIREG_CSR            0x351
#define IMSIC_MTOPEI_CSR           0x35c
#define IMSIC_VSISELECT_CSR        0x250
#define IMSIC_VSIREG_CSR           0x251
#define IMSIC_VSTOPEI_CSR          0x25c
#define IMSIC_HSTATUS_CSR          0x600
#define IMSIC_HGEIP_CSR            0xe12
#define IMSIC_HSTATUS_VGEIN_SHIFT  12
#define IMSIC_HSTATUS_VGEIN_MASK   (0x3fUL << IMSIC_HSTATUS_VGEIN_SHIFT)
#define IMSIC_EIDELIVERY           0x70
#define IMSIC_EITHRESHOLD          0x72
#define IMSIC_EIE_BASE             0xc0

/* Legacy names are retained only so old source files continue to compile. */
#define INTR_PRIO_REG(id)          APLIC_SOURCECFG(APLIC_M_BASE_ADDR, id)
#define INTR_EN_REG(ctx, id)       APLIC_SETIENUM(APLIC_M_BASE_ADDR)
#define CTX_THD_REG(ctx)           APLIC_DOMAINCFG(APLIC_M_BASE_ADDR)
#define CTX_COMP_REG(ctx)          APLIC_TARGET(APLIC_M_BASE_ADDR, 0)

void plic_init(uint32_t core_num);

int setup_intr(uint32_t id, uint32_t priority);

int setup_context(uint32_t ctx, uint32_t threshold);

int enable_intr(uint32_t ctx, uint32_t id);

int disable_intr(uint32_t ctx, uint32_t id);

/* APLIC/IMSIC programming helpers used by current LinkNan cases. */
void aplic_init(uint32_t source_count);
int aplic_config_source(uint32_t id, uint32_t hart, uint32_t eiid);
int aplic_enable_source(uint32_t id);
int aplic_disable_source(uint32_t id);
int aplic_set_pending(uint32_t id);
void aplic_init_guest(uint32_t source_count);
int aplic_config_guest_source(uint32_t id, uint32_t hart, uint32_t guest, uint32_t eiid);
int aplic_enable_guest_source(uint32_t id);
int aplic_disable_guest_source(uint32_t id);
int aplic_set_guest_pending(uint32_t id);
void imsic_enable_machine(uint32_t eiid);
uint32_t imsic_claim_machine(void);
int imsic_select_guest(uint32_t guest);
int imsic_enable_guest(uint32_t eiid);
uint32_t imsic_claim_guest(void);
uint64_t imsic_guest_pending(void);

#endif
