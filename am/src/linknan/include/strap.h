#ifndef __LINKNAN_STRAP_H__
#define __LINKNAN_STRAP_H__
#include <stdint.h>
#include "mtrap.h"

int s_trap_handler_register(uint64_t cause, void handler(void));

#endif