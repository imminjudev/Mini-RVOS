# Mini-RVOS

Mini-RVOS is a small educational operating system for **64-bit RISC-V**.

It boots on QEMU through OpenSBI and implements physical memory management,
Sv39 virtual memory, traps, user mode, system calls, process address spaces,
an in-memory filesystem, and an interactive user-space shell.

## Architecture

~~~text
OpenSBI
  |
  v
Kernel Entry
  |
  +--> Physical Memory Manager
  |
  +--> Sv39 Virtual Memory
  |
  +--> Trap / Exception Handling
  |
  +--> Process Address Space
  |
  +--> U-mode
  |
  +--> System Calls
  |
  +--> In-Memory File System
  |
  v
User Shell
~~~

## Features

### Boot

- RISC-V 64-bit kernel
- QEMU `virt` machine
- OpenSBI firmware
- Supervisor mode kernel
- Kernel entry at `0x80200000`
- UART console

### Physical Memory Management

- 4 KiB page allocator
- Free-list based physical page management
- Physical page allocation and release

### Virtual Memory

- Sv39 three-level page tables
- Kernel mappings
- User mappings
- Per-process page tables
- User text pages
- User read-only data pages
- User stack
- Virtual-to-physical address translation
- User pointer validation using page-table permissions

### Trap Handling

- RISC-V trap entry and return
- Full register trap frame
- U-mode `ecall` handling
- Page fault handling
- Supervisor timer interrupt support
- Exception diagnostics using:
  - `scause`
  - `sepc`
  - `stval`

### Scheduler

- Round-robin scheduler
- Timer-based preemption support
- Address-space switching between processes

The current interactive shell runs as a single user process with timer
preemption disabled while waiting for UART input.

### User Mode

- S-mode to U-mode transition
- Dedicated user stack
- Dedicated kernel stack
- User text and read-only mappings
- Trap-based return to user mode

### System Calls

Current syscall interface:

| Number | Syscall | Description |
| --- | --- | --- |
| 1 | `write` | Write to stdout or an open file |
| 4 | `getpid` | Get current process ID |
| 5 | `open` | Open a file |
| 6 | `read` | Read from stdin or a file |
| 7 | `create` | Create an in-memory file |
| 8 | `close` | Close a file descriptor |
| 9 | `exit` | Exit the user shell |

System-call numbers are shared through:

~~~text
include/syscall.h
~~~

User-space code enters the kernel through:

~~~text
kernel/user_syscall.S
~~~

### User Memory Validation

System calls validate user pointers before dereferencing them.

The kernel checks:

- whether the virtual address is mapped
- whether the page has `PTE_U`
- required read/write permissions
- multi-page ranges
- integer overflow in address ranges
- attempts to pass kernel-only addresses

The shell includes a `memtest` command that deliberately passes invalid
addresses to the kernel.

Successful output:

~~~text
$ memtest
[OK] user pointer validation
~~~

### In-Memory File System

Mini-RVOS includes a small in-memory filesystem with:

- inode table
- open-file table
- file descriptors
- per-open-file offsets
- create
- open
- read
- write
- close

The default filesystem contains:

~~~text
/hello.txt
~~~

Contents:

~~~text
Hello from /hello.txt in Mini-RVOS!
~~~

The filesystem is currently volatile and resets whenever the kernel reboots.

## Interactive Shell

The shell executes in RISC-V U-mode.

Available commands:

~~~text
help
pid
echo <text>
cat <path>
memtest
exit
~~~

Example session:

~~~text
Mini-RVOS shell
Type 'help' for commands.

$ help
commands: help pid echo cat exit memtest

$ pid
1

$ echo hello
hello

$ cat /hello.txt
Hello from /hello.txt in Mini-RVOS!

$ memtest
[OK] user pointer validation

$ exit

Mini-RVOS shell exited.
~~~

## Project Structure

~~~text
Mini-RVOS/
├── include/
│   ├── fs.h
│   ├── memory.h
│   ├── process.h
│   ├── riscv.h
│   ├── sbi.h
│   ├── scheduler.h
│   ├── syscall.h
│   ├── trap.h
│   ├── uart.h
│   └── vm.h
│
├── kernel/
│   ├── entry.S
│   ├── fs.c
│   ├── main.c
│   ├── memory.c
│   ├── process.c
│   ├── sbi.c
│   ├── scheduler.c
│   ├── syscall.c
│   ├── trap.c
│   ├── trap_entry.S
│   ├── uart.c
│   ├── user_entry.S
│   ├── user_shell.c
│   ├── user_syscall.S
│   └── vm.c
│
├── tests/
│   └── smoke.sh
│
├── LICENSE
├── linker.ld
├── Makefile
└── README.md
~~~

## Development Environment

Mini-RVOS was developed using WSL2 Ubuntu.

Required tools:

- RISC-V bare-metal GCC toolchain
- GNU Make
- QEMU RISC-V system emulator
- OpenSBI
- GNU `timeout`

Commands expected to be available:

~~~text
riscv64-unknown-elf-gcc
riscv64-unknown-elf-ld
qemu-system-riscv64
make
timeout
~~~

Development versions include:

~~~text
RISC-V GCC: 14.2
QEMU:       10.2
OpenSBI:    1.8
~~~

## Build

From WSL:

~~~bash
cd /mnt/d/Mini-RVOS
make clean
make
~~~

The resulting kernel image is:

~~~text
build/kernel.elf
~~~

## Run

~~~bash
make run
~~~

QEMU configuration:

~~~text
Machine:  virt
Memory:   128 MiB
Firmware: OpenSBI
Console:  UART / nographic
~~~

To manually terminate QEMU:

~~~text
Ctrl+A
X
~~~

The Mini-RVOS `exit` command stops user execution and parks the kernel.
QEMU itself remains running until terminated externally.

## Automated Smoke Test

Run:

~~~bash
make test
~~~

The smoke test verifies:

- kernel boot
- filesystem initialization
- shell process creation
- transition to user shell
- `help`
- `getpid`
- `echo`
- filesystem read using `cat`
- user-pointer validation
- shell exit

Successful result:

~~~text
[OK] Mini-RVOS smoke test passed
~~~

## ASID Context-Switch Research

Mini-RVOS includes a reproducible experiment for evaluating
ASID-based Sv39 address-space switching.

Research question:

~~~text
How does ASID-based Sv39 address-space switching affect
preemptive scheduling overhead in Mini-RVOS across different
timer quanta and memory working-set sizes?
~~~

The experiment compares:

~~~text
FULL_FLUSH
    ASID 0
    global SFENCE.VMA operations during address-space activation

ASID
    fixed per-process ASIDs
    no measured per-switch global SFENCE.VMA
~~~

The final experiment used:

~~~text
2 switching policies
3 timer quanta
4 memory working-set sizes
20 runs per condition

24 conditions
480 total runs
100 measured context switches per run
~~~

Tested timer quanta:

~~~text
10,000 ticks
100,000 ticks
1,000,000 ticks
~~~

Tested private memory working sets:

~~~text
1 page    =   4 KiB
8 pages   =  32 KiB
32 pages  = 128 KiB
128 pages = 512 KiB
~~~

### Research Results

Across the final 480-run memory-workload experiment:

- `FULL_FLUSH` recorded 200 measured global `SFENCE.VMA` operations
  for every 100-switch run.
- `ASID` recorded zero measured per-switch global `SFENCE.VMA`
  operations.
- ASID reduced mean measured address-space switch latency in all
  12 matched quantum/working-set comparisons.
- The cellwise switch-cost reduction ranged from **14.24% to 46.09%**.
- The unweighted mean cellwise switch-cost reduction was approximately
  **29.68%**.
- All bootstrap 95% confidence intervals for end-to-end throughput
  change included zero.

The experiment therefore found a consistent reduction in the measured
address-space switching-path cost, while it did not establish a
corresponding end-to-end user-throughput improvement under the tested
QEMU environment.

The study does not directly measure TLB miss counts.

### Research Figures

#### ASID Throughput Change

![ASID throughput change](docs/assets/asid-research/01-throughput-change.svg)

#### Address-Space Switch Cost

![Measured address-space switch cost](docs/assets/asid-research/02-switch-cost.svg)

#### Switching Share of Benchmark Time

![Address-space switching share](docs/assets/asid-research/03-switch-time-share.svg)

### Technical Report

The complete methodology, benchmark validation, results, discussion,
limitations, and hypothesis evaluation are documented in:

~~~text
docs/08-asid-research-report.md
~~~

### Reproducing the Research Pipeline

Run the benchmark matrix:

~~~bash
make research-experiment
~~~

Analyze an accepted dataset:

~~~bash
make research-analyze DATASET=research-results/<dataset>
~~~

Generate figures:

~~~bash
make research-plot DATASET=research-results/<dataset>
~~~

The benchmark runner records Git revision, QEMU/compiler versions,
experiment parameters, random seed, and raw per-run measurements.

Generated raw research results under `research-results/` are excluded
from normal Git tracking. The report contains frozen SVG figures from
the accepted final dataset.

## Development Milestones

~~~text
v0.1  Boot
v0.2  Physical Memory Manager
v0.3  Sv39 Virtual Memory
v0.4  Trap Handling
v0.5  Scheduler
v0.6  User Mode + Syscalls
v0.7  Processes + Address Spaces
v0.8  In-Memory File System
v0.9  Interactive User Shell
v1.0  Hardening, Validation, Testing, Documentation
~~~

## Current Limitations

Mini-RVOS intentionally remains small.

Current limitations include:

- no persistent disk filesystem
- no VirtIO block driver
- no ELF executable loader
- user application is linked into the kernel image
- no dynamic user-space program loading
- interactive shell currently runs as one process
- stdin uses polling UART input
- shell `exit` parks the kernel instead of shutting down QEMU

Possible future extensions include a VirtIO block driver, persistent filesystem,
ELF loader, process lifecycle management, and multiple independent user
programs.

## License

Mini-RVOS is licensed under the MIT License.

See `LICENSE`.
