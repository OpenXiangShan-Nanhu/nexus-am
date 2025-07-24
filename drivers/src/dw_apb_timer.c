#include <klib.h>
#include <dw_apb_timer.h>

inline void timer_set_enable(int timer, bool enable) {
    uint32_t ctrl = READ_U32(TIMER_BASE(timer) + APBTMR_N_CONTROL);
    if(enable) {
        ctrl |= APBTMR_CONTROL_ENABLE;
    } else {
        ctrl &= ~APBTMR_CONTROL_ENABLE;
    }
    WRITE_U32(TIMER_BASE(timer) + APBTMR_N_CONTROL, ctrl);
}

inline void timer_set_mode(int timer, bool mode) {
    uint32_t ctrl = READ_U32(TIMER_BASE(timer) + APBTMR_N_CONTROL);
    if(mode) {
        ctrl |= APBTMR_CONTROL_MODE_PERIODIC;
    } else {
        ctrl &= ~APBTMR_CONTROL_MODE_PERIODIC;
    }
    WRITE_U32(TIMER_BASE(timer) + APBTMR_N_CONTROL, ctrl);
}

inline void timer_set_mask(int timer, bool mask) {
    uint32_t ctrl = READ_U32(TIMER_BASE(timer) + APBTMR_N_CONTROL);
    if(mask) {
        ctrl |= APBTMR_CONTROL_INT;
    } else {
        ctrl &= ~APBTMR_CONTROL_INT;
    }
    WRITE_U32(TIMER_BASE(timer) + APBTMR_N_CONTROL, ctrl);
}

inline void timer_set_count(int timer, uint32_t count) {
    WRITE_U32(TIMER_BASE(timer) + APBTMR_N_LOAD_COUNT, count);
}

inline void timer_eoi(int timer) {
    READ_U32(TIMER_BASE(timer) + APBTMR_N_EOI);
}

void timer_irq_handler(int timer) {
    timer_eoi(timer);
    timer_set_count(timer, 0x10000);
}

void timer_init(int timer) {
    timer_set_enable(timer, false);

    timer_set_mode(timer, true);
    timer_set_mask(timer, false);
    timer_set_count(timer, 0x10000);

    timer_set_enable(timer, true);
    printf("timer %d is initialized!\n", timer);
}
