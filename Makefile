CROSS = riscv64-unknown-elf-
CC = $(CROSS)gcc
LD = $(CROSS)ld

CFLAGS = -mcmodel=medany -ffreestanding -fno-pie

BUILD = build

OBJS = \
	$(BUILD)/entry.o \
	$(BUILD)/main.o \
	$(BUILD)/uart.o \
	$(BUILD)/memory.o \
	$(BUILD)/vm.o

KERNEL = $(BUILD)/kernel.elf

all: $(KERNEL)

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/entry.o: kernel/entry.S | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/main.o: kernel/main.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/uart.o: kernel/uart.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/memory.o: kernel/memory.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/vm.o: kernel/vm.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(KERNEL): $(OBJS) linker.ld
	$(LD) -T linker.ld $(OBJS) -o $@

run: $(KERNEL)
	qemu-system-riscv64 \
		-machine virt \
		-m 128M \
		-nographic \
		-bios default \
		-kernel $(KERNEL)

clean:
	rm -rf $(BUILD)

.PHONY: all run clean
