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

OBJS = \
	$(BUILD)/entry.o \
	$(BUILD)/trap_entry.o \
	$(BUILD)/user_entry.o \
	$(BUILD)/user_shell.o \
	$(BUILD)/user_syscall.o \
	$(BUILD)/main.o \
	$(BUILD)/uart.o \
	$(BUILD)/memory.o \
	$(BUILD)/vm.o \
	$(BUILD)/trap.o \
	$(BUILD)/sbi.o \
	$(BUILD)/scheduler.o \
	$(BUILD)/process.o \
	$(BUILD)/fs.o

KERNEL = $(BUILD)/kernel.elf

all: $(KERNEL)

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/entry.o: kernel/entry.S | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/trap_entry.o: kernel/trap_entry.S | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/user_entry.o: kernel/user_entry.S | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/user_shell.o: kernel/user_shell.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/user_syscall.o: kernel/user_syscall.S | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/main.o: kernel/main.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/uart.o: kernel/uart.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/memory.o: kernel/memory.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/vm.o: kernel/vm.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/trap.o: kernel/trap.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/sbi.o: kernel/sbi.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/scheduler.o: kernel/scheduler.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/process.o: kernel/process.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/fs.o: kernel/fs.c | $(BUILD)
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
