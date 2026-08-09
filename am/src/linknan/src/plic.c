#include "platform.h"
#include "plic.h"

static inline void csr_write_num(unsigned int csr, uint64_t value) {
  asm volatile("csrw %0, %1" : : "i"(csr), "rK"(value) : "memory");
}

static inline uint64_t csr_read_num(unsigned int csr) {
  uint64_t value;
  asm volatile("csrr %0, %1" : "=r"(value) : "i"(csr));
  return value;
}

static inline void csr_set_num(unsigned int csr, uint64_t value) {
  asm volatile("csrs %0, %1" : : "i"(csr), "rK"(value) : "memory");
}

void aplic_init(uint32_t source_count) {
  if (source_count >= NR_INTR) source_count = NR_INTR - 1;
  WRITE_U32(APLIC_DOMAINCFG(APLIC_M_BASE_ADDR), 0);
  for (uint32_t id = 1; id <= source_count; id++) {
    WRITE_U32(APLIC_SOURCECFG(APLIC_M_BASE_ADDR, id), 0);
    WRITE_U32(APLIC_TARGET(APLIC_M_BASE_ADDR, id), 0);
    WRITE_U32(APLIC_CLRIENUM(APLIC_M_BASE_ADDR), id);
  }
}

int aplic_config_source(uint32_t id, uint32_t hart, uint32_t eiid) {
  if (id == 0 || id >= NR_INTR || hart >= APLIC_HART_SLOTS || eiid == 0 || eiid >= 2048) {
    return 1;
  }
  WRITE_U32(APLIC_SOURCECFG(APLIC_M_BASE_ADDR, id), APLIC_SOURCE_EDGE1);
  WRITE_U32(APLIC_TARGET(APLIC_M_BASE_ADDR, id), APLIC_TARGET_VALUE(hart, eiid));
  return 0;
}

int aplic_enable_source(uint32_t id) {
  if (id == 0 || id >= NR_INTR) {
    return 1;
  }
  WRITE_U32(APLIC_SETIENUM(APLIC_M_BASE_ADDR), id);
  WRITE_U32(APLIC_DOMAINCFG(APLIC_M_BASE_ADDR), APLIC_DOMAINCFG_MSI);
  return 0;
}

int aplic_disable_source(uint32_t id) {
  if (id == 0 || id >= NR_INTR) {
    return 1;
  }
  WRITE_U32(APLIC_CLRIENUM(APLIC_M_BASE_ADDR), id);
  return 0;
}

int aplic_set_pending(uint32_t id) {
  if (id == 0 || id >= NR_INTR) {
    return 1;
  }
  WRITE_U32(APLIC_SETIPNUM(APLIC_M_BASE_ADDR), id);
  return 0;
}

void aplic_init_guest(uint32_t source_count) {
  if (source_count >= NR_INTR) source_count = NR_INTR - 1;
  WRITE_U32(APLIC_DOMAINCFG(APLIC_S_BASE_ADDR), 0);
  for (uint32_t id = 1; id <= source_count; id++) {
    WRITE_U32(APLIC_SOURCECFG(APLIC_M_BASE_ADDR, id), APLIC_SOURCE_DELEGATE);
    WRITE_U32(APLIC_SOURCECFG(APLIC_S_BASE_ADDR, id), 0);
    WRITE_U32(APLIC_TARGET(APLIC_S_BASE_ADDR, id), 0);
    WRITE_U32(APLIC_CLRIENUM(APLIC_S_BASE_ADDR), id);
  }
}

int aplic_config_guest_source(uint32_t id, uint32_t hart, uint32_t guest, uint32_t eiid) {
  if (id == 0 || id >= NR_INTR || hart >= APLIC_HART_SLOTS ||
      guest == 0 || guest > APLIC_GEILEN || eiid == 0 || eiid >= 2048) {
    return 1;
  }
  WRITE_U32(APLIC_SOURCECFG(APLIC_S_BASE_ADDR, id), APLIC_SOURCE_EDGE1);
  WRITE_U32(APLIC_TARGET(APLIC_S_BASE_ADDR, id),
            APLIC_TARGET_VALUE_GUEST(hart, guest, eiid));
  return 0;
}

int aplic_enable_guest_source(uint32_t id) {
  if (id == 0 || id >= NR_INTR) return 1;
  WRITE_U32(APLIC_SETIENUM(APLIC_S_BASE_ADDR), id);
  WRITE_U32(APLIC_DOMAINCFG(APLIC_S_BASE_ADDR), APLIC_DOMAINCFG_MSI);
  return 0;
}

int aplic_disable_guest_source(uint32_t id) {
  if (id == 0 || id >= NR_INTR) return 1;
  WRITE_U32(APLIC_CLRIENUM(APLIC_S_BASE_ADDR), id);
  return 0;
}

int aplic_set_guest_pending(uint32_t id) {
  if (id == 0 || id >= NR_INTR) return 1;
  WRITE_U32(APLIC_SETIPNUM(APLIC_S_BASE_ADDR), id);
  return 0;
}

void imsic_enable_machine(uint32_t eiid) {
  const uint32_t bank = eiid / 64;
  const uint32_t bit = eiid % 64;
  csr_write_num(IMSIC_MISELECT_CSR, IMSIC_EIE_BASE + 2 * bank);
  csr_set_num(IMSIC_MIREG_CSR, 1UL << bit);
  csr_write_num(IMSIC_MISELECT_CSR, IMSIC_EIDELIVERY);
  csr_write_num(IMSIC_MIREG_CSR, 1);
  csr_write_num(IMSIC_MISELECT_CSR, IMSIC_EITHRESHOLD);
  csr_write_num(IMSIC_MIREG_CSR, 0);
}

uint32_t imsic_claim_machine(void) {
  const uint64_t top = csr_read_num(IMSIC_MTOPEI_CSR);
  csr_write_num(IMSIC_MTOPEI_CSR, top);
  return (uint32_t)((top >> 16) & 0x7ffU);
}

int imsic_select_guest(uint32_t guest) {
  if (guest == 0 || guest > APLIC_GEILEN) return 1;
  uint64_t hstatus = csr_read_num(IMSIC_HSTATUS_CSR);
  hstatus = (hstatus & ~IMSIC_HSTATUS_VGEIN_MASK) |
            ((uint64_t)guest << IMSIC_HSTATUS_VGEIN_SHIFT);
  csr_write_num(IMSIC_HSTATUS_CSR, hstatus);
  hstatus = csr_read_num(IMSIC_HSTATUS_CSR);
  return ((hstatus & IMSIC_HSTATUS_VGEIN_MASK) >> IMSIC_HSTATUS_VGEIN_SHIFT) != guest;
}

int imsic_enable_guest(uint32_t eiid) {
  if (eiid == 0 || eiid >= 2048) return 1;
  const uint32_t bank = eiid / 64;
  const uint32_t bit = eiid % 64;
  csr_write_num(IMSIC_VSISELECT_CSR, IMSIC_EIE_BASE + 2 * bank);
  csr_set_num(IMSIC_VSIREG_CSR, 1UL << bit);
  csr_write_num(IMSIC_VSISELECT_CSR, IMSIC_EIDELIVERY);
  csr_write_num(IMSIC_VSIREG_CSR, 1);
  csr_write_num(IMSIC_VSISELECT_CSR, IMSIC_EITHRESHOLD);
  csr_write_num(IMSIC_VSIREG_CSR, 0);
  return 0;
}

uint32_t imsic_claim_guest(void) {
  const uint64_t top = csr_read_num(IMSIC_VSTOPEI_CSR);
  csr_write_num(IMSIC_VSTOPEI_CSR, top);
  return (uint32_t)((top >> 16) & 0x7ffU);
}

uint64_t imsic_guest_pending(void) {
  return csr_read_num(IMSIC_HGEIP_CSR);
}

/* Compatibility wrappers for older PLIC-oriented cases. */
void plic_init(uint32_t core_num) {
  (void)core_num;
  aplic_init(NR_INTR - 1);
}

int setup_intr(uint32_t id, uint32_t priority) {
  (void)priority;
  return aplic_config_source(id, 0, id);
}

int setup_context(uint32_t ctx, uint32_t threshold) {
  (void)ctx;
  return threshold > MAX_PRIORITY;
}

int enable_intr(uint32_t ctx, uint32_t id) {
  return aplic_config_source(id, ctx / 2, id) || aplic_enable_source(id);
}

int disable_intr(uint32_t ctx, uint32_t id) {
  (void)ctx;
  return aplic_disable_source(id);
}
