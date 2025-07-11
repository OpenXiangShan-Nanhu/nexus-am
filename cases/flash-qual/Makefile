NAME = flash
BUILD_DIR = $(shell pwd)/build
BUILD_BIN = $(BUILD_DIR)/$(NAME).bin
BUILD_ELF = $(BUILD_DIR)/$(NAME).elf
BUILD_TXT = $(BUILD_DIR)/$(NAME).txt
BUILD_OBJ = $(BUILD_DIR)/$(NAME).o
CC = riscv64-unknown-linux-gnu-

.PHONY: all clean

all: $(BUILD_BIN) $(BUILD_TXT)

$(BUILD_BIN): $(BUILD_ELF)
	$(CC)objcopy -O binary $< $@

$(BUILD_TXT): $(BUILD_ELF)
	$(CC)objdump -d $< > $@

$(BUILD_ELF): $(BUILD_OBJ)
	$(CC)ld -T link.ld -o $@ $<

$(BUILD_OBJ): $(BUILD_DIR)
	$(CC)as -march=rv64g -o $@ flash.S

$(BUILD_DIR):
	mkdir -p $@

clean:
	rm -rf $(BUILD_DIR)
