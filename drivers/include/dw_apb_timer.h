#ifndef __DW_APB_TIMER_H__
#define __DW_APB_TIMER_H__

#include <am.h>
#include <platform.h>

#define TIMER_BASE(n)           0x50000000L + (n) * 0x10000
#define APBTMR_N_LOAD_COUNT     0x00
#define APBTMR_N_CURRENT_VALUE  0x04
#define APBTMR_N_CONTROL        0x08
#define APBTMR_N_EOI            0x0c
#define APBTMR_N_INT_STATUS     0x10

#define APBTMR_CONTROL_ENABLE           BIT(0)
/* 1: periodic, 0:free running. */
#define APBTMR_CONTROL_MODE_PERIODIC    BIT(1)
#define APBTMR_CONTROL_INT              BIT(2)

void timer_eoi(int timer);
void timer_enable_int(int timer);
void timer_disable_int(int timer);
void timer_set_periodic(int timer, uint32_t load_count);
void timer_irq_handler(int timer);

#endif /* __DW_APB_TIMER_H__ */
