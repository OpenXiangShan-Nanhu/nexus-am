#include "dw_wdt.h"
#include "platform.h"

inline void dw_wdt_enable(){
    uint32_t control = READ_U32(WDT_BASE + WDT_CR);

    control |= WDT_CR_WDT_EN;
    WRITE_U32(WDT_BASE + WDT_CR, control);
}

inline void dw_wdt_response_mode_set(const int mode) {
    uint32_t control = READ_U32(WDT_BASE + WDT_CR);

    if (mode) {
        control |= WDT_CR_RMOD;
    } else {
        control &= ~WDT_CR_RMOD;
    }

    WRITE_U32(WDT_BASE + WDT_CR, control);
}

inline void dw_wdt_reset_pulse_length_set(const uint32_t pclk_cycles) {
    uint32_t control = READ_U32(WDT_BASE + WDT_CR);

    control &= ~WDT_CR_RPL;
    control |= SET_FIELD(WDT_CR_RPL, pclk_cycles);
    WRITE_U32(WDT_BASE + WDT_CR, control);
}

inline void dw_wdt_timeout_period_set(const uint32_t timeout_period){
    uint32_t timeout = READ_U32(WDT_BASE + WDT_TORR);

    timeout &= ~WDT_TORR_TOP;
    timeout |= SET_FIELD(WDT_TORR_TOP, timeout_period);
    WRITE_U32(WDT_BASE + WDT_TORR, timeout);
}

inline uint32_t dw_wdt_timeout_period_get() {
    return GET_FIELD(WDT_TORR_TOP, READ_U32(WDT_BASE + WDT_TORR));
}

inline void dw_wdt_timeout_period_init_set(const uint32_t timeout_period) {
    uint32_t timeout = READ_U32(WDT_BASE + WDT_TORR);

    timeout &= ~WDT_TORR_TOP_INIT;
    timeout |= SET_FIELD(WDT_TORR_TOP_INIT, timeout_period);
    WRITE_U32(WDT_BASE + WDT_TORR, timeout);
}

inline void dw_wdt_counter_restart(){
    WRITE_U32(0x76, WDT_BASE + WDT_CRR);
}

inline uint32_t dw_wdt_current_counter_value_register_get() {
    return  READ_U32(WDT_BASE + WDT_CCVR);
}

inline uint32_t dw_wdt_interrupt_status_register_get() {
	return READ_U32(WDT_BASE + WDT_STAT) & 1;
}

inline void dw_wdt_clear_interrupt() {
	READ_U32(WDT_BASE + WDT_EOI);
}
