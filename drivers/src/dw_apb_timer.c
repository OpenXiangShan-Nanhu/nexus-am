#include <dw_apb_timer.h>

inline void timer_eoi(int timer) {
    READ_U32(TIMER_BASE(timer) + APBTMR_N_EOI);
}

inline void timer_enable_int(int timer) {
    uint32_t ctrl = READ_U32(TIMER_BASE(timer) + APBTMR_N_CONTROL);
    /* clear pending intr */
    timer_eoi(timer);
    ctrl &= ~APBTMR_CONTROL_INT;
    WRITE_U32(TIMER_BASE(timer) + APBTMR_N_CONTROL, ctrl);
}

inline void timer_disable_int(int timer) {
    uint32_t ctrl = READ_U32(TIMER_BASE(timer) + APBTMR_N_CONTROL);
    ctrl |= APBTMR_CONTROL_INT;
    WRITE_U32(TIMER_BASE(timer) + APBTMR_N_CONTROL, ctrl);
}

inline void timer_set_periodic(int timer, uint32_t load_count) {
    uint32_t ctrl = READ_U32(TIMER_BASE(timer) + APBTMR_N_CONTROL);
    ctrl |= APBTMR_CONTROL_MODE_PERIODIC;
    WRITE_U32(TIMER_BASE(timer) + APBTMR_N_CONTROL, ctrl);

    ctrl &= ~APBTMR_CONTROL_ENABLE;
    WRITE_U32(TIMER_BASE(timer) + APBTMR_N_CONTROL, ctrl);

    WRITE_U32(TIMER_BASE(timer) + APBTMR_N_LOAD_COUNT, load_count);
    ctrl |= APBTMR_CONTROL_ENABLE;
    WRITE_U32(TIMER_BASE(timer) + APBTMR_N_CONTROL, ctrl);
}

inline void timer_irq_handler(int timer) {
    timer_eoi(timer);
}