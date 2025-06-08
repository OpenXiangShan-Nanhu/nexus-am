MARCH ?= rv64gc_zifencei_zicsr_zicntr_zba_zbb_zbc_zbs_zbkb_zbkc_zbkx_zknd_zkne_zknh_zkr_zksed_zksh_zkt_zicboz_zicbom_zicbop

include $(AM_HOME)/am/arch/riscv64.mk

AM_SRCS := nemu/common/mainargs.S \
           nemu/isa/riscv/cte.c \
           nemu/isa/riscv/trap.S \
           nemu/isa/riscv/cte64.c \
           nemu/isa/riscv/mtime.S \
           nemu/isa/riscv/vme.c \
           noop/common/input.c \
           noop/isa/riscv/instr.c \
           linknan/src/cmo.c \
           linknan/src/intr_gen.c \
           linknan/src/plic.c \
           linknan/src/clint.c \
           linknan/asm/start.S \
           linknan/asm/mtrap.S \
           linknan/src/mtrap.c \
           linknan/src/trm.c \
           linknan/src/ioe.c \
           linknan/src/ppu.c \
           linknan/src/uartlite.c

CFLAGS  += -I$(AM_HOME)/am/src/nemu/include -I$(AM_HOME)/am/src/linknan/include -DISA_H=\"riscv.h\"

ASFLAGS += -DMAINARGS=\"$(mainargs)\"
.PHONY: $(AM_HOME)/am/src/nemu/common/mainargs.S

LDFLAGS += -L $(AM_HOME)/am/src/nemu/ldscript
LDFLAGS += -T $(AM_HOME)/am/src/nemu/isa/riscv/boot/loader64.ld

image:
	@echo + LD "->" $(BINARY_REL).elf
	@$(LD) $(LDFLAGS) --gc-sections -o $(BINARY).elf --start-group $(LINK_FILES) --end-group
	@$(OBJDUMP) -d $(BINARY).elf > $(BINARY).txt
	@echo + OBJCOPY "->" $(BINARY_REL).bin
	@$(OBJCOPY) -S --set-section-flags .bss=alloc,contents -O binary $(BINARY).elf $(BINARY).bin
