#ifndef __VMM_H__
#define __VMM_H__

#include <stdint.h>
#include <am.h>

void vm_map(void *va, void *pa, uintptr_t prot);

void vm_init(uint64_t addr);
void vm_enable(uint64_t addr);

void default_page_fault_handler();

#endif