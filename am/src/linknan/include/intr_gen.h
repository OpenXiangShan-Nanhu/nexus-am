#ifndef __LINKNAN_INTRGEN_H__
#define __LINKNAN_INTRGEN_H__

#include <stdint.h>
#include "platform.h"

void raise_ext_intr(uint64_t id);

void clear_ext_intr(uint64_t id);

#endif