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

  if(ps.state.dev == PWR_ON) {
    pp.policy.pwr_plcy = PWR_RET;
    WRITE_U32(PWPR(cpu), pp.u32_val);
  }
  do {
    ps.u32_val = READ_U32(PWSR(cpu));
  } while(ps.state.dev != pp.policy.pwr_plcy);

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

  if(ps.state.dev == PWR_ON) {
    pp.policy.pwr_plcy = PWR_OFF;
    WRITE_U32(PWPR(cpu), pp.u32_val);
  }
  do {
    ps.u32_val = READ_U32(PWSR(cpu));
  } while(ps.state.dev != pp.policy.pwr_plcy);

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