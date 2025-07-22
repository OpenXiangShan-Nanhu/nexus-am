#ifndef __DW_WDT_H__
#define __DW_WDT_H__

#include <am.h>
#include <platform.h>

#define WDT_BASE            0x50060000L
#define WDT_CR              0x00        // Control Register
#define WDT_TORR            0x04        // Timeout Range Register
#define WDT_CCVR            0x08        // Current Counter Value Register
#define WDT_CRR             0x0c        // Counter Restart Register
#define WDT_STAT            0x10        // Interrupt Status Register
#define WDT_EOI             0x0c        // Interrupt Clear Register

#define WDT_CR_WDT_EN       BIT(0)
#define WDT_CR_RMOD         BIT(1)
#define WDT_CR_RPL          GENMASK(4, 2)

#define WDT_TORR_TOP        GENMASK(3, 0)
#define WDT_TORR_TOP_INIT   GENMASK(7, 4)


void dw_wdt_enable(void);
void dw_wdt_response_mode_set(const int mode);
void dw_wdt_reset_pulse_length_set(const uint32_t pclk_cycles);
void dw_wdt_timeout_period_set(const uint32_t timeout_period);
uint32_t dw_wdt_timeout_period_get(void);
void dw_wdt_timeout_period_init_set(const uint32_t timeout_period);
void dw_wdt_counter_restart(void);
uint32_t dw_wdt_current_counter_value_register_get(void);
uint32_t dw_wdt_interrupt_status_register_get(void);
void dw_wdt_clear_interrupt(void);

#endif /* __DW_WDT_H__ */
