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
#include "platform.h"

#define NUM_CORES 4

#define TEST_SIZE 512
#define TEST_GOLD ((TEST_SIZE * (TEST_SIZE - 1)) / 2)

volatile uint64_t step_lock = 0;
volatile int step = 0;
volatile void *vmem  = (void *)0x90000000;
volatile void *pmema = (void *)0xa0000000;
volatile void *pmemb = (void *)0xb0000000;


/* dirty code begin
   vmm.c only support one address space
   this case need more address space
*/

static char *sv48_alloc_base2;
static uintptr_t sv48_alloced_size2 = 0;
static _AddressSpace as2;

static const _Area ln_area[] = {     // Linknan default memory mappings
  (_Area) { .start = (void *)(0x80000000), .end = (void *)(0x80016000) },
  (_Area) { .start = (void *)(0x40600000), .end = (void *)(0x40601000) }
};

static inline uintptr_t VPNi(uintptr_t va, int i) {
  uintptr_t vpn_mask = (1 << 9) - 1;
  return (va >> 12 >> (9 * i)) & vpn_mask;
}

static inline void *new_page2() {
  void *p = (void *)(sv48_alloc_base2 + sv48_alloced_size2);
  sv48_alloced_size2 += 0x1000; 
  memset(p, 0, 0x1000);
  return p;
}

void vm_map2(void *va, void *pa, uintptr_t prot) {
  // printf("map va %lx to pa %lx with prot %lx\n", (uintptr_t)va, (uintptr_t)pa, prot);
  assert((uintptr_t)va % 0x1000 == 0);
  assert((uintptr_t)pa % 0x1000 == 0);
  uint64_t *pg_base = as2.ptr;
  uint64_t *pte;
  int level;

  for (level = 4 - 1; ; level--) {
    pte = &pg_base[VPNi((uintptr_t)va, level)];
    pg_base = (uint64_t *)PTE_ADDR(*pte);
    if (level == 0) break;
    if (!(*pte & PTE_V)) {
      pg_base = new_page2();
      uint64_t val = PTE_V | ((uintptr_t)pg_base >> 12 << 10);
      *pte = val;
    }
  }

  if (!(*pte & PTE_V) || ((uintptr_t)pa >> 12) != (*pte >> 10)) {
    *pte = PTE_V | prot | ((uintptr_t)pa >> 12 << 10);
  }
  riscv_fence();
}

void vm_init2(uint64_t addr) {

  sv48_alloc_base2 = (char *)addr;
  as2.ptr = new_page2();

  // root page table
  int page_num = 0x10;
  atomic_printf("va %llx --> pa %llx (size %llx)\n", (uintptr_t)addr, (uintptr_t)addr, page_num * 0x1000);
  for (int i = 0; i < page_num; i++) {
    vm_map2((void *)addr + i * 0x1000, (void *)addr + i * 0x1000, PTE_R | PTE_W | PTE_X | PTE_A | PTE_D);
  }

  // initial page table
  int i;
  for (i = 0; i < LENGTH(ln_area); i ++) {
    void *va = ln_area[i].start;
    atomic_printf("va %llx --> pa %llx (size %llx)\n", (uintptr_t)ln_area[i].start, (uintptr_t)ln_area[i].start, (uintptr_t)ln_area[i].end - (uintptr_t)ln_area[i].start);
    for (; va < ln_area[i].end; va += 0x1000) {
      vm_map2(va, va, PTE_R | PTE_W | PTE_X | PTE_A | PTE_D);
    }
  }
}

void asid1_page_fault_handler() {
  uint64_t mhartid = riscv_mhartid();
  uint64_t mtval = csr_read(mtval);
  uint64_t vaddr = mtval & ~0xfff;
  atomic_printf("Core %d Page fault at address 0x%lx\n", mhartid, mtval);
  atomic_printf("va %llx --> pa %llx (size %llx)\n", vaddr, vaddr, 0x1000);
  if(mhartid == 0){
    vm_map((void *)vaddr, (void *)vaddr, PTE_R | PTE_W | PTE_X | PTE_A | PTE_D);
  }
  else{
    vm_map2((void *)vaddr, (void *)vaddr, PTE_R | PTE_W | PTE_X | PTE_A | PTE_D);
  }
  asm volatile("sfence.vma");
  asm volatile("fence.i");
  for (int i = 0; i < 4; i++) {
    if (i != mhartid) {
      raise_ipi(i);
    }
  }
}

// dirty code end

void ipi_handler() {
  uint64_t id = riscv_mhartid();
  s_atomic_printf("Core %d get ipi and sfence.vma\n", id);
  asm volatile("sfence.vma");
  asm volatile("fence.i");
  clear_ipi(id);
}

int ipi_init() {
  uint64_t mie = csr_read(mie);
  csr_write(mie, mie | MSIE);

  uint64_t mstatus = csr_read(mstatus);
  csr_write(mstatus, mstatus | (0x1UL << 3));
  return 0;
}


void task0(uint64_t hartid) {
  s_barrier(NUM_CORES, hartid);
  int i = 0;
  do{
    vm_map((void *)vmem, (void *)pmema + i * 0x1000, PTE_R | PTE_W | PTE_X | PTE_A | PTE_D);
    raise_ipi(1);
    for(volatile int j = 0; j < 64; j++);
    i++;
  }while(step != 1);
}

void task1(uint64_t hartid) {
  s_barrier(NUM_CORES, hartid);
  int sum = 0;
  vm_map2((void *)vmem, (void *)pmemb, PTE_R | PTE_W | PTE_X | PTE_A | PTE_D);
  for(int i = 0; i < TEST_SIZE; i++){
    sum += READ_U64((uint64_t *)vmem + i);
  }
  step++;
  riscv_fence();
  if(sum != TEST_GOLD)
    s_atomic_printf("Core %d get wrong result: %d\n", hartid, sum);
  else
    s_atomic_printf("Core %d get correct result: %d\n", hartid, sum);
  
}

void task2(uint64_t hartid) {
  s_barrier(NUM_CORES, hartid);
}

void empty(){}
void (*cpu[NUM_CORES])() = {task0, task1, task2, task2};

void __attribute__((constructor)) s_main(int hartid){
  asm volatile("mv a0, %0" :: "r"(hartid));
  // s_barrier(NUM_CORES, hartid);
  cpu[hartid]();
  s_barrier(NUM_CORES, hartid);
  _halt(0);
}

int main() {

  uint64_t hartid = riscv_mhartid();

  if (hartid == 0) {
    for(volatile int i = 1; i < NUM_CORES; i++) switch_on_core(i);
    csr_write(satp, ((9ULL << 60) | 0x84000000 >> 12));
    vm_init(0x84000000);
    vm_map((void *)0x1000000, (void *)0x1000000, PTE_R | PTE_W | PTE_X | PTE_A | PTE_D);
    asm volatile("sfence.vma");
  }
  // barrier(NUM_CORES);
  if(hartid == 1){
    m_trap_handler_register(MSIP, ipi_handler);
    ipi_init();
  }

  if (hartid == 1) {
    for (volatile int i = 0; i < TEST_SIZE; i++) {
      WRITE_U64((uint64_t *)pmemb + i, i);
    }
    csr_write(satp, ((9ULL << 60) | 1ULL << 44 | 0x88000000 >> 12));
    vm_init2(0x88000000);
    asm volatile("sfence.vma");
  }

  m_trap_handler_register(INS_PAGE_FAULT, asid1_page_fault_handler);
  m_trap_handler_register(LOAD_PAGE_FAULT, asid1_page_fault_handler);
  m_trap_handler_register(STORE_PAGE_FAULT, asid1_page_fault_handler);


  barrier(NUM_CORES);
  switch_mode(hartid, MODE_S, (uint64_t)&s_main);
  atomic_printf("Core %d should not reach here\n", hartid);
}