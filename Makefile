.RECIPEPREFIX := >

CROSS = riscv64-unknown-elf-
CC = $(CROSS)gcc
LD = $(CROSS)ld

CFLAGS = \
	-mcmodel=medany \
	-ffreestanding \
	-fno-pie \
	-fno-stack-protector \
	-fno-builtin \
	-msmall-data-limit=0

BUILD = build
BENCHMARK_MODE ?= 0

ifeq ($(BENCHMARK_MODE),1)
CFLAGS += -DBENCHMARK_MODE
endif

USER_OBJS = \
	$(BUILD)/user_entry.o \
	$(BUILD)/user_syscall.o

ifeq ($(BENCHMARK_MODE),1)
USER_OBJS += $(BUILD)/user_benchmark.o
BENCHMARK_OBJS = $(BUILD)/research.o
else
USER_OBJS += $(BUILD)/user_shell.o
BENCHMARK_OBJS =
endif

OBJS = \
	$(BUILD)/entry.o \
	$(BUILD)/trap_entry.o \
	$(USER_OBJS) \
	$(BENCHMARK_OBJS) \
	$(BUILD)/main.o \
	$(BUILD)/uart.o \
	$(BUILD)/memory.o \
	$(BUILD)/vm.o \
	$(BUILD)/trap.o \
	$(BUILD)/syscall.o \
	$(BUILD)/sbi.o \
	$(BUILD)/scheduler.o \
	$(BUILD)/process.o \
	$(BUILD)/fs.o

KERNEL = $(BUILD)/kernel.elf

all: $(KERNEL)

$(BUILD):
>mkdir -p $(BUILD)

$(BUILD)/entry.o: kernel/entry.S | $(BUILD)
>$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/trap_entry.o: kernel/trap_entry.S | $(BUILD)
>$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/user_entry.o: kernel/user_entry.S | $(BUILD)
>$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/user_shell.o: kernel/user_shell.c | $(BUILD)
>$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/user_benchmark.o: kernel/user_benchmark.c | $(BUILD)
>$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/research.o: kernel/research.c | $(BUILD)
>$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/user_syscall.o: kernel/user_syscall.S | $(BUILD)
>$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/main.o: kernel/main.c | $(BUILD)
>$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/uart.o: kernel/uart.c | $(BUILD)
>$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/memory.o: kernel/memory.c | $(BUILD)
>$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/vm.o: kernel/vm.c | $(BUILD)
>$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/trap.o: kernel/trap.c | $(BUILD)
>$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/syscall.o: kernel/syscall.c | $(BUILD)
>$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/sbi.o: kernel/sbi.c | $(BUILD)
>$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/scheduler.o: kernel/scheduler.c | $(BUILD)
>$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/process.o: kernel/process.c | $(BUILD)
>$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/fs.o: kernel/fs.c | $(BUILD)
>$(CC) $(CFLAGS) -c $< -o $@

$(KERNEL): $(OBJS) linker.ld
>$(LD) -T linker.ld $(OBJS) -o $@

run: $(KERNEL)
>qemu-system-riscv64 \
>	-machine virt \
>	-m 128M \
>	-nographic \
>	-bios default \
>	-kernel $(KERNEL)

test: $(KERNEL)
>./tests/smoke.sh

benchmark:
>$(MAKE) BUILD=build-benchmark BENCHMARK_MODE=1 all

run-benchmark:
>$(MAKE) BUILD=build-benchmark BENCHMARK_MODE=1 run

clean:
>rm -rf build build-benchmark

.PHONY: all run test benchmark run-benchmark clean
