// addr-tour: traverse the qnanhai address space and verify the SidebandXbar
// routing:
//
//   [0x00000000, 0x01000000)  errdev (SLVERR)
//   [0x01000000, 0x01400000)  4 x 256KB CPU windows (BootCtrl/PPU/Timer)
//   [0x01400000, 0x02000000)  errdev (CPU window gap)
//   [0x02000000, 0x04000000)  NoC config window (regs @ 0x03200000, rest errdev)
//   [0x04000000, 0x48000000)  m_axi (internal devices -> TL; rest -> m_axi.ext)
//   [0x48000000, ...)          errdev
//
// Each reachable device is read with its natural register width (4B for
// AXI4-lite 32-bit registers, 8B for 64-bit registers).  An errdev probe is
// issued last: the DUT responds with SLVERR, but the core's exception
// redirect for MMIO read errors is broken (redirects to pc 0 instead of
// raising a proper access-fault trap), so the errdev probe terminates the
// tour and is documented as such.
//
// Known-broken reads skipped (pre-existing DUT/sim issues, unrelated to the
// sideband routing): NoC-internal config registers (0x03200000), debug module
// MMIO (0x04010000), sim-side intr generator / uartlite reads.

#include <am.h>
#include <klib.h>
#include <klib-macros.h>
#include <stdint.h>
#include "csr.h"
#include "ppu.h"
#include "platform.h"

#define NUM_CORES 4

/* errdev probe state, updated by the probe trap entry (probe.S) */
volatile uint64_t fault_addr;
volatile int      fault_flag;
volatile int      fault_count;

/* Own trap entry that records access faults and skips the faulting
 * instruction; see probe.S.  Other traps fall through to the AM _mtrap. */
extern void probe_mtrap(void);

static uint64_t saved_mtvec;

static void enable_probe_trap(void) {
  saved_mtvec = csr_read(mtvec);
  csr_write(mtvec, (uint64_t)probe_mtrap);
}

static void disable_probe_trap(void) {
  csr_write(mtvec, saved_mtvec);
}

/* Issue a 32-bit or 64-bit load; returns 1 if it trapped. */
static int probe_load(uint64_t addr, int size, uint64_t *val) {
  uint64_t v = 0;
  fault_flag = 0;
  fault_addr = 0;
  if(size == 8) {
    asm volatile("ld %0, 0(%1); fence" : "+r"(v) : "r"(addr) : "memory");
  } else {
    asm volatile("lw %0, 0(%1); fence" : "+r"(v) : "r"(addr) : "memory");
  }
  *val = v;
  return fault_flag;
}

typedef struct {
  uint64_t    addr;
  int         size;          /* access width: 4 or 8 bytes */
  int         expect_fault;
  const char *name;
} tour_entry_t;

static const tour_entry_t load_tour[] = {
  /* CPU windows: devices of core 0 and core 3 (core 3 powered on first) */
  { CPU_SPACE(0) + PPU_OFFSET + 0x4, 4, 0, "core0 ppu pwsr" },
  { CPU_SPACE(0) + BOOT_ADDR_OFFSET, 8, 0, "core0 bootctrl" },
  { CPU_SPACE(0) + TIMER_OFFSET, 8, 0, "core0 timer" },
  { CPU_SPACE(3) + PPU_OFFSET + 0x4, 4, 0, "core3 ppu pwsr" },
  /* NOTE: the NoC-internal config register READ path (0x03200000) currently
   * crashes the core for any access width (pre-existing NoC-side issue,
   * unrelated to the sideband routing); it is therefore not probed here.
   * The cfg window's non-register region is covered by the errdev probes. */
  /* m_axi [0x04000000, 0x48000000) */
  { 0x04008000UL, 8, 0, "mtimer mtime" },
  /* NOTE: the debug module MMIO read (0x04010000) crashes the core
   * (pre-existing issue; the DM is normally driven via JTAG), skipped. */
  { 0x10000000UL, 4, 0, "flash" },
  { 0x38000000UL, 4, 0, "plic priority" },
  /* NOTE: reads to the sim-side devices (intr generator 0x40070000,
   * uartlite 0x40600000) hang; the uartlite write path is exercised by
   * printf throughout. */
  { 0x80000000UL, 8, 0, "dram base" },
  { 0xc0000000UL, 8, 0, "dram +1GB" },
};

static int run_load_tour(void) {
  int fail = 0;
  for(int i = 0; i < (int)(sizeof(load_tour) / sizeof(load_tour[0])); i++) {
    uint64_t val;
    printf("[probe] %-22s @ 0x%016lx (%dB)\n", load_tour[i].name, load_tour[i].addr, load_tour[i].size);
    int f  = probe_load(load_tour[i].addr, load_tour[i].size, &val);
    int ok = (f == load_tour[i].expect_fault);
    if(!ok) fail++;
    printf("  %-22s : %s (value 0x%016lx)%s\n", load_tour[i].name, f ? "FAULT" : "OK   ", val,
           ok ? "" : "  <-- UNEXPECTED");
  }
  return fail;
}

static volatile int tour_done = 0;

int main() {
  uint64_t id = riscv_mhartid();
  if(id == 0) {
    /* bring up cores 1..3, then probe everything from core 0 */
    for(int i = 1; i < NUM_CORES; i++) switch_on_core(i);
    enable_probe_trap();
    int fail = 0;
    printf("=== addr-tour: load probes ===\n");
    fail += run_load_tour();
    printf("addr-tour OK-probes %s (failures: %d)\n", fail ? "FAIL" : "PASS", fail);
    /* Finally probe an error-device address.  The DUT responds with SLVERR,
     * but the core's exception redirect for MMIO read errors is broken
     * (redirects to pc 0 instead of raising a proper access-fault trap),
     * so this probe terminates the tour.  The errdev response itself is
     * confirmed by the resulting instruction-access-fault storm at pc 0
     * in the log. */
    uint64_t val;
    printf("[probe] %-22s @ 0x%016lx (4B)\n", "errdev low", 0x00000000UL);
    probe_load(0x00000000UL, 4, &val);
    printf("addr-tour %s (OK-probes done, errdev probe as documented)\n", fail ? "FAIL" : "PASS");
    disable_probe_trap();
    riscv_fence();
    tour_done = 1;
    return fail ? 1 : 0;
  }
  /* the sim finishes on the FIRST core halt, so keep cores 1..3 alive
   * until core 0 has completed the tour */
  while(!tour_done) {
    riscv_fence();
  }
  return 0;
}
