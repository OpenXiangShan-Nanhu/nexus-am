// plic: classic PLIC (TLPLIC @ 0x38000000) external interrupt test.
// Source s (1..NUM_SOURCES) is enabled only in core (s-1)'s M context, so
// each source is deterministically delivered to one core.  Core 0 raises all
// sources through the sim interrupt generator (pure writes: the device does
// not respond to reads), every core claims its interrupt via PLIC MMIO and
// verifies the source id.

#include <am.h>
#include <klib.h>
#include <klib-macros.h>
#include <stdint.h>
#include "ppu.h"
#include "csr.h"
#include "platform.h"

#define NUM_CORES    4
#define NUM_SOURCES  4

/* rocketchip TLPLIC register map */
#define PLIC_BASE            0x38000000UL
#define PLIC_PRIO(id)        (PLIC_BASE + 4UL * (id))
#define PLIC_PENDING         (PLIC_BASE + 0x1000UL)
#define PLIC_ENABLE(ctx)     (PLIC_BASE + 0x2000UL + 0x80UL * (ctx))
#define PLIC_THRESHOLD(ctx)  (PLIC_BASE + 0x200000UL + 0x1000UL * (ctx))
#define PLIC_CLAIM(ctx)      (PLIC_BASE + 0x200004UL + 0x1000UL * (ctx))

/* contexts are interleaved M/S per hart: 2*hart = M, 2*hart+1 = S */
#define PLIC_CTX_M(hart)     (2UL * (hart))

#define MSTATUS_MIE_BIT 3
#define MEIP_BIT        11

#define INTR_GEN_BASE 0x40070000UL

/* Raise interrupts via the sim interrupt generator.  The device register is
 * replace-on-write and does not respond to reads, so all sources sharing a
 * 64-bit word are raised in a single write. */
static void raise_ext_intrs(uint32_t mask) {
  WRITE_U64(INTR_GEN_BASE, mask);
}

static volatile uint32_t plic_fail    = 0;
static volatile uint32_t plic_claims[NUM_CORES];

int main() {
  uint64_t id = riscv_mhartid();
  if(id == 0) {
    for(int i = 1; i < NUM_CORES; i++) switch_on_core(i);
    /* source s -> core s-1's M context only */
    for(int s = 1; s <= NUM_SOURCES; s++) {
      uint64_t ctx = PLIC_CTX_M(s - 1);
      WRITE_U32(PLIC_PRIO(s), 1);
      WRITE_U32(PLIC_ENABLE(ctx) + 4UL * (s / 32), 1UL << (s % 32));
      WRITE_U32(PLIC_THRESHOLD(ctx), 0);
    }
    riscv_fence();
  }
  /* poll the pending bit instead of enabling MEIE: mip reports MEIP
   * regardless of mie, and this keeps the AM default trap handler out of
   * the loop (a taken interrupt would need a claim/complete handler). */
  if(barrier(NUM_CORES)) return 1;

  if(id == 0) {
    printf("PLIC test started!\n");
    /* raise sources 1..NUM_SOURCES in a single write (the generator
     * register is replace-on-write); intrGen bit N drives PLIC source N+1,
     * so sources 1..NUM_SOURCES = bits 0..NUM_SOURCES-1 */
    raise_ext_intrs((1UL << NUM_SOURCES) - 1);
  }

  /* busy-poll for the external interrupt (wfi would not wake with MEIE
   * disabled), then claim + complete */
  while(!(csr_read(mip) & (1UL << MEIP_BIT))) ;
  uint32_t claimed = READ_U32(PLIC_CLAIM(PLIC_CTX_M(id)));
  plic_claims[id] = claimed;
  if(claimed != id + 1) plic_fail = 1;
  WRITE_U32(PLIC_CLAIM(PLIC_CTX_M(id)), claimed);
  riscv_fence();
  if(barrier(NUM_CORES)) return 1;

  if(id == 0) {
    for(int h = 0; h < NUM_CORES; h++) printf("Core %d: claimed intr %u\n", h, plic_claims[h]);
    printf("PLIC test %s\n", plic_fail ? "FAIL" : "passed!");
  }
  /* second barrier: core 0's summary prints must hit the UART before any
   * core halts (the sim finishes on the first halt) */
  if(barrier(NUM_CORES)) return 1;
  return plic_fail ? 1 : 0;
}
