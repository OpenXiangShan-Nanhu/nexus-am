#include <klib.h>
#include <klib-macros.h>
#include <stdint.h>
#include "clint.h"
#include "printf.h"
#include "riscv.h"
#include "csr.h"

static _AddressSpace as;

// default root page table address
static char *sv48_alloc_base;
static uintptr_t sv48_alloced_size = 0;

#define RANGE_LEN(start, len) RANGE((start), (start + len))
#define SATP_MODE (9ull << 60)
#define PTW_LEVEL 4
#define VPN_WIDTH 9
#define PGSIZE 0x1000 // default 4K page
#define PGSHFT 12 // log2(PGSIZE)

static const _Area ln_area[] = {     // Linknan default memory mappings
  RANGE_LEN(0x80000000, 0x10000), // PMEM
  RANGE_LEN(0x40600000, 0x1000), // uart
};


static inline uintptr_t VPNi(uintptr_t va, int i) {
  uintptr_t vpn_mask = (1 << VPN_WIDTH) - 1;
  return (va >> PGSHFT >> (VPN_WIDTH * i)) & vpn_mask;
}

static inline void *new_page() {
  void *p = (void *)(sv48_alloc_base + sv48_alloced_size);
  sv48_alloced_size += PGSIZE; 
  memset(p, 0, PGSIZE);
  return p;
}

/*
 * map va to pa with prot permission with page table root as
 * pagetable_level indicates page table level to be used
 * 0: basic 4KiB page
 * 1: 2MiB megapage
 * 2: 1GiB gigapage
 * Note that RISC-V allow hardware to fault when A and D bit is not set
 */
void vm_huge_map(void *va, void *pa, uintptr_t prot, int pagetable_level) {
  int hugepage_size;
  switch (pagetable_level) {
    case 0: hugepage_size = PGSIZE; break; // 4KiB
    case 1: hugepage_size = PGSIZE * 512; break;  // 2MiB
    case 2: hugepage_size = PGSIZE * 512 * 512; break;  // 1GiB
    default: assert(0);
  }
  assert((uintptr_t)va % hugepage_size == 0);
  assert((uintptr_t)pa % hugepage_size == 0);
  uint64_t *pg_base = as.ptr;
  uint64_t *pte;
  int level;
  for (level = PTW_LEVEL - 1; ; level --) {
    pte = &pg_base[VPNi((uintptr_t)va, level)];
    pg_base = (uint64_t *)PTE_ADDR(*pte);
    if (level == pagetable_level) break;
    if (!(*pte & PTE_V)) {
      pg_base = new_page();
      uint64_t val = PTE_V | ((uintptr_t)pg_base >> PGSHFT << 10);
      *pte = val;
    }
  }

  int hugepage_pn_shift = pagetable_level * 9;
  if (!(*pte & PTE_V) || ((uintptr_t)pa >> PGSHFT >> hugepage_pn_shift << hugepage_pn_shift) != (*pte >> 10)) {
    *pte = PTE_V | prot | ((uintptr_t)pa >> PGSHFT >> hugepage_pn_shift << hugepage_pn_shift << 10);
  }
  riscv_fence();
}

void vm_map(void *va, void *pa, uintptr_t prot) {
  // printf("map va %lx to pa %lx with prot %lx\n", (uintptr_t)va, (uintptr_t)pa, prot);
  assert((uintptr_t)va % PGSIZE == 0);
  assert((uintptr_t)pa % PGSIZE == 0);
  uint64_t *pg_base = as.ptr;
  uint64_t *pte;
  int level;

  for (level = PTW_LEVEL - 1; ; level--) {
    pte = &pg_base[VPNi((uintptr_t)va, level)];
    pg_base = (uint64_t *)PTE_ADDR(*pte);
    if (level == 0) break;
    if (!(*pte & PTE_V)) {
      pg_base = new_page();
      uint64_t val = PTE_V | ((uintptr_t)pg_base >> PGSHFT << 10);
      *pte = val;
    }
  }

  if (!(*pte & PTE_V) || ((uintptr_t)pa >> PGSHFT) != (*pte >> 10)) {
    *pte = PTE_V | prot | ((uintptr_t)pa >> PGSHFT << 10);
  }
  riscv_fence();
}

void vm_init(uint64_t addr) {

  sv48_alloc_base = (char *)addr;
  as.ptr = new_page();

  // root page table
  int page_num = 0x10;
  atomic_printf("va %llx --> pa %llx (size %llx)\n", (uintptr_t)addr, (uintptr_t)addr, page_num * PGSIZE);
  for (int i = 0; i < page_num; i++) {
    vm_map((void *)addr + i * PGSIZE, (void *)addr + i * PGSIZE, PTE_R | PTE_W | PTE_X | PTE_A | PTE_D);
  }

  // initial page table
  int i;
  for (i = 0; i < LENGTH(ln_area); i ++) {
    void *va = ln_area[i].start;
    atomic_printf("va %llx --> pa %llx (size %llx)\n", (uintptr_t)ln_area[i].start, (uintptr_t)ln_area[i].start, (uintptr_t)ln_area[i].end - (uintptr_t)ln_area[i].start);
    for (; va < ln_area[i].end; va += PGSIZE) {
      vm_map(va, va, PTE_R | PTE_W | PTE_X | PTE_A | PTE_D);
    }
  }
}

void vm_enable(uint64_t addr) {
  csr_set(menvcfg, (1ULL << 62));
  csr_write(satp, (SATP_MODE | addr >> PGSHFT));
  asm volatile("sfence.vma");
}


void default_page_fault_handler() {
  uint64_t mhartid = riscv_mhartid();
  uint64_t mtval = csr_read(mtval);
  uint64_t vaddr = mtval & ~0xfff;
  atomic_printf("Core %d Page fault at address 0x%lx\n", mhartid, mtval);
  atomic_printf("va %llx --> pa %llx (size %llx)\n", vaddr, vaddr, PGSIZE);
  vm_map((void *)vaddr, (void *)vaddr, PTE_R | PTE_W | PTE_X | PTE_A | PTE_D);
  asm volatile("sfence.vma");
  asm volatile("fence.i");
  for (int i = 0; i < 4; i++) {
    if (i != mhartid) {
      raise_ipi(i);
    }
  }
}