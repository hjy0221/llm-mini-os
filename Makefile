HOST_OS := $(shell uname -s)

ifeq ($(HOST_OS),Darwin)
LLVM_PREFIX := $(shell command -v brew >/dev/null 2>&1 && \
	brew --prefix llvm 2>/dev/null)
LLD_PREFIX := $(shell command -v brew >/dev/null 2>&1 && \
	brew --prefix lld 2>/dev/null)
DEFAULT_CLANG := $(if $(LLVM_PREFIX),$(LLVM_PREFIX)/bin/clang,clang)
DEFAULT_LLD := $(if $(LLD_PREFIX),$(LLD_PREFIX)/bin/ld.lld,ld.lld)
BUILD_INSTALL_HINT := brew install llvm lld
QEMU_INSTALL_HINT := brew install qemu
PYTHON_INSTALL_HINT := brew install python
else
DEFAULT_CLANG := clang
DEFAULT_LLD := ld.lld
BUILD_INSTALL_HINT := sudo apt-get install clang lld
QEMU_INSTALL_HINT := sudo apt-get install qemu-system-arm
PYTHON_INSTALL_HINT := sudo apt-get install python3
endif

# 명령행 또는 환경변수로 도구 경로를 덮어쓸 수 있다.
CLANG ?= $(DEFAULT_CLANG)
LLD ?= $(DEFAULT_LLD)
QEMU ?= qemu-system-aarch64
PYTHON ?= python3
TEST_SCRIPT ?= tests/qemu_smoke.py

CFLAGS := --target=aarch64-none-elf -ffreestanding -fno-stack-protector \
	-fno-builtin -nostdlib -Wall -Wextra -O2 -mcpu=cortex-a72 \
	-mgeneral-regs-only
LDFLAGS := -T linker.ld -nostdlib

BUILD := build
OBJS := $(BUILD)/boot.o $(BUILD)/kernel.o $(BUILD)/uart.o \
	$(BUILD)/console.o $(BUILD)/shell.o $(BUILD)/platform.o \
	$(BUILD)/exception_vectors.o $(BUILD)/exception.o $(BUILD)/gic.o \
	$(BUILD)/timer.o $(BUILD)/irq.o $(BUILD)/mmu.o \
	$(BUILD)/page_alloc.o $(BUILD)/task_switch.o $(BUILD)/task.o

.PHONY: all run test clean print-tools check-build-tools check-run-tools \
	check-test-tools

all: check-build-tools $(BUILD)/kernel.elf

check-build-tools:
	@command -v "$(CLANG)" >/dev/null 2>&1 || { \
		echo "Clang을 찾을 수 없습니다: $(CLANG)"; \
		echo "설치: $(BUILD_INSTALL_HINT)"; \
		exit 1; \
	}
	@command -v "$(LLD)" >/dev/null 2>&1 || { \
		echo "LLD를 찾을 수 없습니다: $(LLD)"; \
		echo "설치: $(BUILD_INSTALL_HINT)"; \
		exit 1; \
	}

check-run-tools:
	@command -v "$(QEMU)" >/dev/null 2>&1 || { \
		echo "QEMU를 찾을 수 없습니다: $(QEMU)"; \
		echo "설치: $(QEMU_INSTALL_HINT)"; \
		exit 1; \
	}

check-test-tools: check-run-tools
	@command -v "$(PYTHON)" >/dev/null 2>&1 || { \
		echo "Python을 찾을 수 없습니다: $(PYTHON)"; \
		echo "설치: $(PYTHON_INSTALL_HINT)"; \
		exit 1; \
	}

print-tools:
	@echo "Host:   $(HOST_OS)"
	@echo "Clang:  $(CLANG)"
	@echo "LLD:    $(LLD)"
	@echo "QEMU:   $(QEMU)"
	@echo "Python: $(PYTHON)"

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/boot.o: src/boot.S | $(BUILD)
	$(CLANG) $(CFLAGS) -c $< -o $@

$(BUILD)/kernel.o: src/kernel.c src/exception.h src/gic.h src/irq.h \
		src/mmu.h src/page_alloc.h src/shell.h src/task.h src/timer.h \
		src/uart.h | $(BUILD)
	$(CLANG) $(CFLAGS) -c $< -o $@

$(BUILD)/uart.o: src/uart.c src/uart.h src/gic.h | $(BUILD)
	$(CLANG) $(CFLAGS) -c $< -o $@

$(BUILD)/console.o: src/console.c src/console.h src/uart.h | $(BUILD)
	$(CLANG) $(CFLAGS) -c $< -o $@

$(BUILD)/shell.o: src/shell.c src/shell.h src/console.h src/exception.h \
		src/mmu.h src/page_alloc.h src/platform.h src/task.h src/timer.h \
		src/uart.h | $(BUILD)
	$(CLANG) $(CFLAGS) -c $< -o $@

$(BUILD)/platform.o: src/platform.c src/platform.h | $(BUILD)
	$(CLANG) $(CFLAGS) -c $< -o $@

$(BUILD)/exception_vectors.o: src/exception_vectors.S | $(BUILD)
	$(CLANG) $(CFLAGS) -c $< -o $@

$(BUILD)/exception.o: src/exception.c src/exception.h src/uart.h | $(BUILD)
	$(CLANG) $(CFLAGS) -c $< -o $@

$(BUILD)/gic.o: src/gic.c src/gic.h | $(BUILD)
	$(CLANG) $(CFLAGS) -c $< -o $@

$(BUILD)/timer.o: src/timer.c src/timer.h src/gic.h | $(BUILD)
	$(CLANG) $(CFLAGS) -c $< -o $@

$(BUILD)/irq.o: src/irq.c src/irq.h src/gic.h src/timer.h src/uart.h | $(BUILD)
	$(CLANG) $(CFLAGS) -c $< -o $@

$(BUILD)/mmu.o: src/mmu.c src/mmu.h | $(BUILD)
	$(CLANG) $(CFLAGS) -c $< -o $@

$(BUILD)/page_alloc.o: src/page_alloc.c src/page_alloc.h | $(BUILD)
	$(CLANG) $(CFLAGS) -c $< -o $@

$(BUILD)/task_switch.o: src/task_switch.S src/task.h | $(BUILD)
	$(CLANG) $(CFLAGS) -c $< -o $@

$(BUILD)/task.o: src/task.c src/task.h src/page_alloc.h | $(BUILD)
	$(CLANG) $(CFLAGS) -c $< -o $@

$(BUILD)/kernel.elf: $(OBJS) linker.ld
	$(LLD) $(LDFLAGS) $(OBJS) -o $@

run: all check-run-tools
	$(QEMU) -M virt,gic-version=2,secure=off -cpu cortex-a72 \
		-smp 1 -m 128M \
		-nographic -serial mon:stdio -kernel $(BUILD)/kernel.elf

test: all check-test-tools
	$(PYTHON) "$(TEST_SCRIPT)" \
		--qemu "$(QEMU)" --kernel "$(BUILD)/kernel.elf"

clean:
	rm -rf $(BUILD)
