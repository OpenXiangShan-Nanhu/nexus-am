#ifndef __LINKNAN_MTRAP_H__
#define __LINKNAN_MTRAP_H__
#include <stdint.h>

#define INTR (1UL << 63)
#define EXCP (0UL << 63)

#define SSIP (INTR | 1)
#define MSIP (INTR | 3)
#define STIP (INTR | 5)
#define MTIP (INTR | 7)
#define SEIP (INTR | 9)
#define MEIP (INTR | 11)

#define SSIE (0x1UL << 1)
#define MSIE (0x1UL << 3)
#define STIE (0x1UL << 5)
#define MTIE (0x1UL << 7)
#define SEIE (0x1UL << 9)
#define MEIE (0x1UL << 11)

#define INS_MISALIGN     (EXCP | 0)
#define INS_ACC_FAULT    (EXCP | 1)
#define INS_ILLEGAL      (EXCP | 2)
#define BREAKPOINT       (EXCP | 3)
#define LOAD_MISALIGN    (EXCP | 4)
#define LOAD_ACC_FAULT   (EXCP | 5)
#define STORE_MISALIGN   (EXCP | 6)
#define STORE_ACC_FAULT  (EXCP | 7)
#define ENV_UCALL        (EXCP | 8)
#define ENV_SCALL        (EXCP | 9)
#define ENV_MCALL        (EXCP | 11)
#define INS_PAGE_FAULT   (EXCP | 12)
#define LOAD_PAGE_FAULT  (EXCP | 13)
#define STORE_PAGE_FAULT (EXCP | 15)

int m_trap_handler_register(uint64_t cause, void handler(void));

void switch_mode(uint64_t hartid, uint64_t next_mode, uint64_t next_pc);

void m_switch_mode(uint64_t hartid, uint64_t next_mode, uint64_t next_pc);
#endif