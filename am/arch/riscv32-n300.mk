CROSS_COMPILE := riscv64-unknown-elf-

COMMON_FLAGS  := -fno-pic -march=rv32imafdc_zifencei -mabi=ilp32 -mcmodel=medany -nostdlib -nostartfiles
CFLAGS        += $(COMMON_FLAGS) -static
ASFLAGS       += $(COMMON_FLAGS) -O0
LDFLAGS       += -melf32lriscv

AM_SRCS := nemu/common/mainargs.S \
           nemu/isa/riscv/trap.S \
           nemu/isa/riscv/mtime.S \
           noop/common/input.c \
           noop/isa/riscv/instr.c \
           linknan/asm/start.S \
           n300/asm/mtrap.S \
           n300/src/mtrap.c \
           n300/src/trm.c \
           n300/src/ioe.c \
           n300/src/clint.c \
           linknan/src/uartlite.c


CFLAGS  += -I$(AM_HOME)/am/src/nemu/include -I$(AM_HOME)/am/src/linknan/include -DISA_H=\"riscv.h\"

ASFLAGS += -DMAINARGS=\"$(mainargs)\"
.PHONY: $(AM_HOME)/am/src/nemu/common/mainargs.S

LDFLAGS += -L $(AM_HOME)/am/src/nemu/ldscript
LDFLAGS += -T $(AM_HOME)/am/src/nemu/isa/riscv/boot/loader64.ld

image:
	@echo + LD "->" $(BINARY_REL).elf
	@$(LD) $(LDFLAGS) --gc-sections -o $(BINARY).elf --start-group $(LINK_FILES) --end-group
	@$(OBJDUMP) --disassemble-all -S $(BINARY).elf > $(BINARY).txt
	@echo + OBJCOPY "->" $(BINARY_REL).bin
	@$(OBJCOPY) -S --set-section-flags .bss=alloc,contents -O binary $(BINARY).elf $(BINARY).bin
