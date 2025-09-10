#include <am.h>
#include <stdint.h>
#include <xsextra.h>
#include <klib.h>
#include <klib-macros.h>
#include <printf.h>
#include "riscv.h"
#include "clint.h"
#include "ppu.h"
#include "mtrap.h"
#include "riscv.h"
#include "strap.h"
#include "csr.h"
#include "vmm.h"
#include "plic.h"
#include "intr_gen.h"
#include "platform.h"

#define NUM_CORES 4
#define TIMER_INTR_SOURCE_BASE 243

volatile uint8_t intr_cnt = 0;
volatile uint64_t step_lock = 0;
volatile int step = 0;
volatile int *reg = (int *)0x90000000;

volatile int ei_lock = 1;

// --- ipi
void m_ipi_inject() {
  uint64_t id = riscv_mhartid();
  clear_ipi(id);
  csr_set(mip, SSIE);
}


void s_ipi_handler() {
  s_atomic_printf("Get s mode software ipi\n");
  ei_lock = 0;
  riscv_fence();

  csr_clear(sip, SSIE);
  asm volatile("sfence.vma");
  asm volatile("fence.i");

  ei_lock = 1;
  riscv_fence();
  s_atomic_printf("Finish s mode software ipi\n");
}

int ipi_init() {
  csr_set(mie, MSIE);
  csr_set(mideleg, SSIE);
  csr_set(sie, SSIE);
  csr_set(sstatus, (0x1UL << 1) | (0x1UL << 5));
  return 0;
}

// --- timer
void m_timer_handler() {
  uint64_t id = riscv_mhartid();
  uint32_t ctx = id * 2;

  uint32_t intr = READ_U32(CTX_COMP_REG(ctx));
  clear_ext_intr(intr);
  WRITE_U32(CTX_COMP_REG(ctx), intr);

  atomic_printf("Get external interrupt %d!\n", intr);

  step++;
  riscv_fence();
}

void ei_enable() {
  csr_set(mie, MEIE);
  csr_set(mstatus, (0x1UL << 3));
}

int setup_plic() {
  plic_init(1);
  if(setup_context(2, 3)) return 1;
  if(setup_intr(1, 7)) return 1;
  if(enable_intr(2, 1)) return 1;
  return 0;
}


// --- cpu task
void task0(uint64_t hartid) {
  vm_map((void *)reg, (void *)reg, PTE_R | PTE_W | PTE_X | PTE_A | PTE_D);
  WRITE_U64(reg, 0x12345678);
  asm volatile("sfence.vma");
  s_atomic_printf("Core 0 raise ipi\n");
  raise_ipi(1);

  while(ei_lock);
  raise_ext_intr(1);
  s_atomic_printf("Core 0 raise ei\n");
}

void task1(uint64_t hartid) {
  while(step != 1);
  uint64_t val = READ_U32(reg);
  s_atomic_printf("hart %d: read %x\n", hartid, val);
}

void empty(){}
void (*cpu[NUM_CORES])() = {task0, task1, empty, empty};

// --- main
void __attribute__((constructor)) s_main(int hartid){
  asm volatile("mv a0, %0" :: "r"(hartid));
  cpu[hartid]();
  s_barrier(2, hartid);
  _halt(0);
}

int main() {

  uint64_t hartid = riscv_mhartid();

  // -- core 0 poweron core and setup ext intr and vmm
  if (hartid == 0) {
    for(volatile int i = 1; i < 2; i++) switch_on_core(i);
    setup_plic();
    vm_init(0x84000000);

    // avoid page fault on plic
    vm_map((void *)0x40070000, (void *)0x40070000, PTE_R | PTE_W | PTE_X | PTE_A | PTE_D);
  }
  vm_enable(0x84000000);

  // -- enable page fault handler
  m_trap_handler_register(INS_PAGE_FAULT, default_page_fault_handler);
  m_trap_handler_register(LOAD_PAGE_FAULT, default_page_fault_handler);
  m_trap_handler_register(STORE_PAGE_FAULT, default_page_fault_handler);

  // -- core 1 enable timer irq and ipi irq
  if(hartid == 1) {
    m_trap_handler_register(MEIP, m_timer_handler);
    m_trap_handler_register(MSIP, m_ipi_inject);
    s_trap_handler_register(SSIP, s_ipi_handler);
    ipi_init();
    ei_enable();
  }

  // enter S mode
  barrier(2);
  switch_mode(hartid, MODE_S, (uint64_t)&s_main);
}