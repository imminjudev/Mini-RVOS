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
BENCHMARK_QUANTUM_TICKS ?= 100000
BENCHMARK_SWITCHES ?= 100
BENCHMARK_USE_ASID ?= 0
BENCHMARK_WORKLOAD ?= cpu
BENCHMARK_WORKING_SET_PAGES ?= 1

ifeq ($(BENCHMARK_USE_ASID),1)
BENCHMARK_POLICY = asid
else
BENCHMARK_POLICY = full
endif

ifeq ($(BENCHMARK_WORKLOAD),cpu)
BENCHMARK_WORKLOAD_ID = 0
BENCHMARK_BUILD_WS = 0
else ifeq ($(BENCHMARK_WORKLOAD),memory)
BENCHMARK_WORKLOAD_ID = 1
BENCHMARK_BUILD_WS = $(BENCHMARK_WORKING_SET_PAGES)
else
$(error BENCHMARK_WORKLOAD must be cpu or memory)
endif

BENCHMARK_BUILD = build-benchmark-$(BENCHMARK_POLICY)-$(BENCHMARK_WORKLOAD)-ws$(BENCHMARK_BUILD_WS)-q$(BENCHMARK_QUANTUM_TICKS)-s$(BENCHMARK_SWITCHES)

ifeq ($(BENCHMARK_MODE),1)
CFLAGS += -DBENCHMARK_MODE
CFLAGS += -DBENCHMARK_QUANTUM_TICKS=$(BENCHMARK_QUANTUM_TICKS)UL
CFLAGS += -DBENCHMARK_SWITCHES=$(BENCHMARK_SWITCHES)UL
CFLAGS += -DBENCHMARK_USE_ASID=$(BENCHMARK_USE_ASID)
CFLAGS += -DBENCHMARK_WORKLOAD_MEMORY=$(BENCHMARK_WORKLOAD_ID)
CFLAGS += -DBENCHMARK_WORKING_SET_PAGES=$(BENCHMARK_WORKING_SET_PAGES)UL
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
>$(MAKE) BUILD=$(BENCHMARK_BUILD) BENCHMARK_MODE=1 BENCHMARK_QUANTUM_TICKS=$(BENCHMARK_QUANTUM_TICKS) BENCHMARK_SWITCHES=$(BENCHMARK_SWITCHES) BENCHMARK_USE_ASID=$(BENCHMARK_USE_ASID) BENCHMARK_WORKLOAD=$(BENCHMARK_WORKLOAD) BENCHMARK_WORKING_SET_PAGES=$(BENCHMARK_WORKING_SET_PAGES) all

run-benchmark:
>$(MAKE) BUILD=$(BENCHMARK_BUILD) BENCHMARK_MODE=1 BENCHMARK_QUANTUM_TICKS=$(BENCHMARK_QUANTUM_TICKS) BENCHMARK_SWITCHES=$(BENCHMARK_SWITCHES) BENCHMARK_USE_ASID=$(BENCHMARK_USE_ASID) BENCHMARK_WORKLOAD=$(BENCHMARK_WORKLOAD) BENCHMARK_WORKING_SET_PAGES=$(BENCHMARK_WORKING_SET_PAGES) run

clean:
>rm -rf build build-benchmark build-benchmark-*

.PHONY: all run test benchmark run-benchmark clean
