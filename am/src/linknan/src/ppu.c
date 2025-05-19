#include "ppu.h"

void switch_on_core(int cpu) {
  PwsrUnion ps;
  PwprUnion pp;
  ps.u32_val = READ_U32(PWSR(cpu));
  pp.u32_val = READ_U32(PWPR(cpu));
  if(ps.state.dev != PWR_ON) {
    pp.policy.pwr_plcy = PWR_ON;
    WRITE_U32(PWPR(cpu), pp.u32_val);
  }
  do {
    ps.u32_val = READ_U32(PWSR(cpu));
  } while(ps.state.dev != PWR_ON);
}