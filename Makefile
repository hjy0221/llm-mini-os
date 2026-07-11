BREW_PREFIX := $(shell brew --prefix 2>/dev/null)
LLVM_BIN := $(BREW_PREFIX)/opt/llvm/bin
LLD_BIN := $(BREW_PREFIX)/opt/lld/bin
CC := $(LLVM_BIN)/clang
LD := $(LLD_BIN)/ld.lld

CFLAGS := --target=aarch64-none-elf -ffreestanding -fno-stack-protector \
	-fno-builtin -nostdlib -Wall -Wextra -O2 -mcpu=cortex-a72
LDFLAGS := -T linker.ld -nostdlib

BUILD := build
OBJS := $(BUILD)/boot.o $(BUILD)/kernel.o $(BUILD)/uart.o \
	$(BUILD)/console.o $(BUILD)/shell.o $(BUILD)/platform.o

.PHONY: all run clean check-tools

all: check-tools $(BUILD)/kernel.elf

check-tools:
	@test -x "$(CC)" || (echo "LLVM이 없습니다: brew install llvm"; exit 1)
	@test -x "$(LD)" || (echo "LLD 링커가 없습니다: brew install lld"; exit 1)
	@command -v qemu-system-aarch64 >/dev/null || \
		(echo "QEMU가 없습니다: brew install qemu"; exit 1)

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/boot.o: src/boot.S | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/kernel.o: src/kernel.c src/shell.h src/uart.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/uart.o: src/uart.c src/uart.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/console.o: src/console.c src/console.h src/uart.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/shell.o: src/shell.c src/shell.h src/console.h src/platform.h \
		src/uart.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/platform.o: src/platform.c src/platform.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/kernel.elf: $(OBJS) linker.ld
	$(LD) $(LDFLAGS) $(OBJS) -o $@

run: all
	qemu-system-aarch64 -M virt -cpu cortex-a72 -m 128M \
		-nographic -serial mon:stdio -kernel $(BUILD)/kernel.elf

clean:
	rm -rf $(BUILD)
