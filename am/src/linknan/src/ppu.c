#include "ppu.h"
#include <klib.h>
#include <klib-macros.h>
#include <printf.h>

int switch_on_core(int cpu) {
  PwsrUnion ps = {.u32_val = READ_U32(PWSR(cpu))};
  PwprUnion pp = {.u32_val = READ_U32(PWPR(cpu))};

  if(ps.state.dev != PWR_ON) {
    pp.policy.pwr_plcy = PWR_ON;
    WRITE_U32(PWPR(cpu), pp.u32_val);
  }
  do {
    ps.u32_val = READ_U32(PWSR(cpu));
  } while(ps.state.dev != PWR_ON);
  atomic_printf("Core %d is powered on!\n", cpu);
  return 0;
}

int switch_ret_core(int cpu) {
  PwsrUnion ps = {.u32_val = READ_U32(PWSR(cpu))};
  PwprUnion pp = {.u32_val = READ_U32(PWPR(cpu))};
  ImrUnion imr = {.u32_val = READ_U32(IMR(cpu))};
  ImrUnion ipr;

  if(ps.state.dev == PWR_ON) {
    imr.intr.stc_evnt = 0;
    WRITE_U32(IMR(cpu), imr.u32_val);
    pp.policy.pwr_plcy = PWR_RET;
    WRITE_U32(PWPR(cpu), pp.u32_val);
  }

  do {
    ipr.u32_val = READ_U32(IPR(cpu));
  } while(ipr.intr.stc_evnt == 0);

  ipr.intr.stc_evnt = 0;
  WRITE_U32(IPR(cpu), ipr.u32_val);

  ps.u32_val = READ_U32(PWSR(cpu));
  if(ps.state.dev == PWR_RET){
    atomic_printf("Core %d is retention!\n", cpu);
    return 0;
  } else {
    atomic_printf("Core %d retention deny!\n", cpu);
    return 1;
  }
}

int switch_off_core(int cpu) {
  PwsrUnion ps = {.u32_val = READ_U32(PWSR(cpu))};
  PwprUnion pp = {.u32_val = READ_U32(PWPR(cpu))};
  ImrUnion imr = {.u32_val = READ_U32(IMR(cpu))};
  ImrUnion ipr;

  if(ps.state.dev == PWR_ON) {
    imr.intr.stc_evnt = 0;
    WRITE_U32(IMR(cpu), imr.u32_val);
    pp.policy.pwr_plcy = PWR_OFF;
    WRITE_U32(PWPR(cpu), pp.u32_val);
  }

  do {
    ipr.u32_val = READ_U32(IPR(cpu));
  } while(ipr.intr.stc_evnt == 0);

  ipr.intr.stc_evnt = 0;
  WRITE_U32(IPR(cpu), ipr.u32_val);

  ps.u32_val = READ_U32(PWSR(cpu));
  if(ps.state.dev == PWR_OFF){
    atomic_printf("Core %d is powered off!\n", cpu);
    return 0;
  } else {
    atomic_printf("Core %d powered off deny!\n", cpu);
    return 1;
  }
}

void switch_power_mode(int cpu, int mode, int policy) {
  PwprUnion pp = {.u32_val = READ_U32(PWPR(cpu))};
  pp.policy.pwr_plcy = policy;
  pp.policy.dyn_en = mode;
  WRITE_U32(PWPR(cpu), pp.u32_val);
  return;
}