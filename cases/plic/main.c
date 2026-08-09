#include <am.h>
#include <klib.h>
#include <klib-macros.h>
#include "plic.h"
#include "mtrap.h"
#include "ppu.h"
#include "csr.h"
#include "platform.h"
#include <stdint.h>

#define NUM_CORES 4
#define NUM_SOURCES 4
#define EXPECTED_DELIVERIES (NUM_SOURCES + 1)

static volatile uint32_t delivery_count = 0;
static volatile uint32_t test_done = 0;
static volatile uint32_t test_failed = 0;

static void switch_on_core_quiet(uint32_t cpu) {
  PwsrUnion state = {.u32_val = READ_U32(PWSR(cpu))};
  PwprUnion policy = {.u32_val = READ_U32(PWPR(cpu))};
  if(state.state.dev != PWR_ON) {
    policy.policy.pwr_plcy = PWR_ON;
    WRITE_U32(PWPR(cpu), policy.u32_val);
  }
  do {
    state.u32_val = READ_U32(PWSR(cpu));
  } while(state.state.dev != PWR_ON);
}

void intr_handler() {
  uint32_t hart = riscv_mhartid();
  uint32_t intr = imsic_claim_machine();
  uint32_t expected_intr = delivery_count % NUM_SOURCES + 1;

  if (intr != expected_intr || hart != expected_intr - 1) {
    test_failed = 1;
    test_done = 1;
    riscv_fence();
    if (hart != 0) aplic_set_pending(1);
    return;
  }

  delivery_count++;
  riscv_fence();
  if (delivery_count == EXPECTED_DELIVERIES) {
    test_done = 1;
  } else {
    aplic_set_pending(intr % NUM_SOURCES + 1);
  }
}

void enable_external_intr() {
  uint64_t mie = csr_read(mie);
  csr_write(mie, mie | MEIE);

  uint64_t mstatus = csr_read(mstatus);
  csr_write(mstatus, mstatus | (0x1UL << 3));
}

int setup_aplic() {
  aplic_init(NUM_SOURCES);
  for (uint32_t i = 1; i <= NUM_SOURCES; i++) {
    if (aplic_config_source(i, (i - 1) % NUM_CORES, i)) return 1;
    if (aplic_enable_source(i)) return 1;
  }
  return 0;
}

int main() {
  uint64_t id = riscv_mhartid();
  if(id == 0) {
    if(setup_aplic()) return 1;
    if(m_trap_handler_register_quiet(MEIP, intr_handler)) return 1;
    for(int i = 1; i < NUM_CORES; i++) switch_on_core_quiet(i);
  }
  imsic_enable_machine(id + 1);
  enable_external_intr();
  if(barrier(NUM_CORES)) return 1;
  if(id == 0) {
    aplic_set_pending(1);
    while(!test_done) riscv_wfi();
    riscv_fence();
    if(test_failed || delivery_count != EXPECTED_DELIVERIES) return 1;
    printf("APLIC/IMSIC four-core test PASS\n");
  } else {
    while(1) riscv_wfi();
  }
  return 0;
}
