#include <am.h>
#include <klib.h>
#include "csr.h"
#include "plic.h"
#include "platform.h"
#include <stdint.h>

#define TEST_HART 0U
#define VS1_SOURCE 1U
#define VS7_SOURCE 2U
#define INVALID_SOURCE 3U
#define VS1_EIID 33U
#define VS7_EIID 47U
#define INVALID_EIID 63U
#define POLL_LIMIT 100000U

static int wait_guest_pending(uint32_t guest, int expected) {
  const uint64_t mask = 1UL << guest;
  for (uint32_t i = 0; i < POLL_LIMIT; i++) {
    const int pending = (imsic_guest_pending() & mask) != 0;
    if (pending == expected) return 0;
  }
  return 1;
}

static int run_guest_delivery(uint32_t source, uint32_t guest, uint32_t eiid) {
  if (imsic_select_guest(guest)) return 1;
  if (imsic_enable_guest(eiid)) return 1;
  if (aplic_config_guest_source(source, TEST_HART, guest, eiid)) return 1;

  const uint32_t target = READ_U32(APLIC_TARGET(APLIC_S_BASE_ADDR, source));
  if (APLIC_TARGET_GUEST(target) != guest) return 1;

  if (aplic_enable_guest_source(source)) return 1;
  riscv_fence();
  if (aplic_set_guest_pending(source)) return 1;
  if (wait_guest_pending(guest, 1)) return 1;
  if (imsic_claim_guest() != eiid) return 1;
  if (wait_guest_pending(guest, 0)) return 1;
  if (aplic_disable_guest_source(source)) return 1;

  printf("APLIC VS GuestIndex %u PASS\n", guest);
  return 0;
}

static int check_invalid_guest(void) {
  WRITE_U32(APLIC_SOURCECFG(APLIC_S_BASE_ADDR, INVALID_SOURCE), APLIC_SOURCE_EDGE1);
  WRITE_U32(APLIC_TARGET(APLIC_S_BASE_ADDR, INVALID_SOURCE),
            APLIC_TARGET_VALUE_GUEST(TEST_HART, APLIC_GEILEN + 1, INVALID_EIID));
  const uint32_t target = READ_U32(APLIC_TARGET(APLIC_S_BASE_ADDR, INVALID_SOURCE));
  if (APLIC_TARGET_GUEST(target) != 0) return 1;

  if (imsic_select_guest(APLIC_GEILEN)) return 1;
  const uint64_t old_hstatus = csr_read(IMSIC_HSTATUS_CSR);
  const uint64_t invalid_hstatus =
    (old_hstatus & ~IMSIC_HSTATUS_VGEIN_MASK) |
    ((uint64_t)(APLIC_GEILEN + 1) << IMSIC_HSTATUS_VGEIN_SHIFT);
  csr_write(IMSIC_HSTATUS_CSR, invalid_hstatus);
  const uint64_t new_hstatus = csr_read(IMSIC_HSTATUS_CSR);
  if ((new_hstatus & IMSIC_HSTATUS_VGEIN_MASK) !=
      (old_hstatus & IMSIC_HSTATUS_VGEIN_MASK)) return 1;

  printf("APLIC GEILEN boundary PASS\n");
  return 0;
}

int main(void) {
  if (riscv_mhartid() != TEST_HART) {
    while (1) riscv_wfi();
  }

  aplic_init_guest(INVALID_SOURCE);
  if (run_guest_delivery(VS1_SOURCE, 1, VS1_EIID)) return 1;
  if (run_guest_delivery(VS7_SOURCE, APLIC_GEILEN, VS7_EIID)) return 1;
  if (check_invalid_guest()) return 1;

  printf("APLIC/IMSIC GEILEN=7 test PASS\n");
  return 0;
}
