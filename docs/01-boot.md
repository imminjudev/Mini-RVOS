# Mini-RVOS Boot and Privilege Flow

## 1. Overview

Mini-RVOS는 QEMU의 RISC-V `virt` machine에서 실행된다.

전체 부팅 흐름은 다음과 같다.

~~~text
QEMU
 |
 v
OpenSBI
(M-mode)
 |
 v
Mini-RVOS _start
(S-mode)
 |
 v
kernel_main
 |
 v
kernel initialization
 |
 v
user process
(U-mode)
~~~

Mini-RVOS가 CPU reset 직후부터 모든 hardware initialization을 직접 수행하는 구조는 아니다.

QEMU에서 OpenSBI가 먼저 실행되고,
OpenSBI가 machine-level initialization을 수행한 뒤 Mini-RVOS에 제어를 넘긴다.

---

# 2. RISC-V Privilege Levels

Mini-RVOS에서 직접 관련 있는 privilege level은 다음 세 가지다.

~~~text
M-mode
  Machine mode

S-mode
  Supervisor mode

U-mode
  User mode
~~~

권한 관계는 대략 다음과 같다.

~~~text
M-mode
  highest privilege
      |
      v
S-mode
  operating system kernel
      |
      v
U-mode
  user program
~~~

Mini-RVOS에서는:

- OpenSBI가 M-mode에서 동작한다.
- Mini-RVOS kernel은 S-mode에서 동작한다.
- Mini-RVOS user shell은 U-mode에서 동작한다.

---

# 3. Why OpenSBI Exists

S-mode kernel은 모든 machine-level hardware operation을 직접 수행할 수 없다.

OpenSBI는 M-mode firmware로 동작하면서
S-mode operating system에 SBI interface를 제공한다.

구조는 다음과 같다.

~~~text
Mini-RVOS S-mode kernel
        |
        | SBI call
        v
OpenSBI M-mode firmware
        |
        v
hardware
~~~

Mini-RVOS에서는 timer programming 등에 SBI를 사용한다.

OpenSBI를 사용함으로써 Mini-RVOS가 M-mode firmware까지 직접 구현할 필요가 없고,
virtual memory, trap, syscall, process, scheduling 같은 OS 핵심 구현에 집중할 수 있다.

---

# 4. Kernel Load Address

Mini-RVOS의 `linker.ld`는 kernel image의 시작 위치를 다음 주소로 설정한다.

~~~text
0x80200000
~~~

linker script의 시작 부분은 다음과 같다.

~~~ld
ENTRY(_start)

SECTIONS
{
    . = 0x80200000;
~~~

즉 Mini-RVOS kernel의 entry point는 `0x80200000` 부근에 배치된다.

OpenSBI firmware는 그보다 아래쪽 RAM 영역에서 실행되므로,
Mini-RVOS kernel image와 OpenSBI가 겹치지 않는다.

---

# 5. ENTRY(_start)

linker script에는 다음 설정이 있다.

~~~ld
ENTRY(_start)
~~~

이 설정은 ELF executable의 entry point를 `_start` symbol로 지정한다.

따라서 OpenSBI가 Mini-RVOS로 제어를 넘긴 뒤
kernel에서 처음 실행되는 코드는 다음 위치다.

~~~text
kernel/entry.S::_start
~~~

---

# 6. Why Assembly Entry Is Needed

CPU가 `_start`에 도착했다고 해서
즉시 일반적인 C 함수를 안전하게 호출할 수 있는 것은 아니다.

C compiler가 생성하는 코드는 정상적인 runtime environment가 존재한다고 가정한다.

특히 stack이 필요하다.

따라서 `kernel_main()`을 호출하기 전에 최소한 다음 작업이 필요하다.

1. kernel stack 설정
2. BSS 초기화

Mini-RVOS에서는 이 초기 작업을 `kernel/entry.S`가 담당한다.

---

# 7. Kernel Stack Initialization

`_start`의 첫 부분에서는 stack pointer를 설정한다.

~~~asm
la sp, stack_top
~~~

`sp`는 RISC-V의 stack pointer register다.

Mini-RVOS는 초기 kernel stack으로 4 KiB를 예약한다.

~~~asm
.section .stack, "aw", @nobits
.align 12

stack_bottom:
    .skip 4096

stack_top:
~~~

초기 상태에서는 다음과 같다.

~~~text
sp = stack_top
~~~

stack은 낮은 주소 방향으로 성장하므로
예약된 stack 영역의 끝 주소인 `stack_top`을 초기 `sp`로 사용한다.

---

# 8. Why the Stack Must Exist Before kernel_main

C 함수가 실행되면 compiler는 필요에 따라 다음 정보를 stack에 저장한다.

- return address
- saved registers
- local variables
- temporary values

따라서 유효한 stack 없이 C 함수를 호출하면
CPU가 임의의 memory 영역을 stack처럼 사용할 수 있다.

그래서 Mini-RVOS는 반드시 다음 순서를 따른다.

~~~text
set sp
  |
  v
initialize BSS
  |
  v
call kernel_main
~~~

---

# 9. BSS Initialization

Mini-RVOS의 linker script는 BSS 영역의 시작과 끝을 나타내는 symbol을 만든다.

~~~text
__bss_start
__bss_end
~~~

`entry.S`에서는 이 범위를 0으로 초기화한다.

현재 코드는 다음 형태다.

~~~asm
la t0, __bss_start
la t1, __bss_end

1:
    bgeu t0, t1, 2f
    sb zero, 0(t0)
    addi t0, t0, 1
    j 1b
~~~

이를 C 형태로 생각하면 대략 다음과 같다.

~~~c
for (p = __bss_start;
     p < __bss_end;
     p++) {

    *p = 0;
}
~~~

---

# 10. Why BSS Must Be Zero

C에서는 초기값을 지정하지 않은 global/static variable이
프로그램 시작 시 0으로 초기화되어 있다고 가정한다.

예:

~~~c
static unsigned long counter;
~~~

이 변수는 C 코드에서는 다음 상태로 시작해야 한다.

~~~text
counter == 0
~~~

그러나 `.bss` 영역은 executable file 안에
실제 zero byte를 전부 저장하는 방식으로 구현되지 않는 경우가 일반적이다.

대신 memory 영역만 예약하고
프로그램 시작 시 runtime이 해당 영역을 0으로 만든다.

일반적인 application에서는 runtime이 이 작업을 처리하지만,
Mini-RVOS는 자체 kernel이므로 직접 BSS를 초기화한다.

---

# 11. Entering kernel_main

stack과 BSS가 준비되면 `_start`는 다음 명령을 실행한다.

~~~asm
call kernel_main
~~~

현재 Mini-RVOS의 entry function은 다음 형태다.

~~~c
void kernel_main(
    unsigned long hart_id,
    void *dtb)
~~~

두 argument는 boot environment에서 전달되는 값이다.

- `hart_id`: boot hardware thread ID
- `dtb`: Device Tree Blob address

현재 Mini-RVOS에서는 둘 다 실제로 사용하지 않는다.

~~~c
(void)hart_id;
(void)dtb;
~~~

현재 구현이 single-hart QEMU `virt` configuration을 기준으로 하고,
Device Tree parsing도 구현하지 않았기 때문이다.

---

# 12. kernel_main Initialization Flow

현재 `kernel_main`의 주요 흐름은 다음과 같다.

~~~text
kernel_main
    |
    +--> UART boot message
    |
    +--> physical memory initialization
    |
    +--> filesystem initialization
    |
    +--> shell process creation
    |
    +--> trap initialization
    |
    +--> timer interrupt disable
    |
    +--> process_start
             |
             v
           U-mode
~~~

즉 `_start`는 C 코드가 실행될 수 있는 최소 환경만 만든다.

실제 operating system subsystem 초기화는
대부분 `kernel_main`에서 수행한다.

이렇게 하면 assembly code의 양을 줄이고
대부분의 kernel logic을 C로 구현할 수 있다.

---

# 13. Linker Script as a Memory Layout Contract

Mini-RVOS에서 linker script는 단순히 object file을 합치는 역할만 하지 않는다.

kernel과 user 영역의 실제 memory layout을 결정한다.

현재 주요 영역은 다음과 같다.

~~~text
kernel text
user text
user read-only data
kernel read-only data
kernel writable data
BSS
kernel stack
~~~

각 영역은 page boundary에 맞춰 정렬된다.

이 구조는 이후 Sv39 page table에서
각 영역에 서로 다른 permission을 줄 수 있게 한다.

예:

~~~text
kernel text
    R-X

user text
    R-X + U

user rodata
    R-- + U

kernel rodata
    R--

kernel data
    RW-
~~~

따라서 linker script와 virtual memory 설계는 서로 독립적인 것이 아니다.

linker가 memory layout을 결정하고,
page table은 그 layout을 기준으로 permission을 설정한다.

---

# 14. User Code Inside the Kernel Image

현재 Mini-RVOS의 user shell은 별도의 ELF executable로 disk에서 load되는 구조가 아니다.

user code와 user rodata 역시 kernel ELF image 안에 포함되어 있다.

linker script에서는 별도의 영역으로 분리한다.

~~~text
__user_text_start
__user_text_end

__user_rodata_start
__user_rodata_end
~~~

process를 만들 때 이 내용을 별도의 physical page로 복사하고
user permission을 가진 page table mapping을 생성한다.

즉:

~~~text
kernel ELF image
      |
      +--> user text template
      |
      +--> user rodata template
                |
                v
        process creation
                |
                v
     private physical pages
                |
                v
       U-mode address space
~~~

이 방식은 구현이 단순하지만,
일반적인 OS처럼 arbitrary ELF application을 실행할 수는 없다.

---

# 15. Transition to U-mode

Mini-RVOS kernel은 S-mode에서 초기화된다.

process의 user context가 준비되면
kernel은 user-mode 실행에 필요한 register state를 만든다.

핵심적으로:

- user program counter
- user stack pointer
- process page table
- `sstatus`
- trap frame

등을 준비한다.

그 후 `sret` instruction을 이용해 U-mode로 내려간다.

개념적으로:

~~~text
S-mode kernel
     |
     | prepare user context
     |
     | set sepc
     | set sstatus
     |
     v
    sret
     |
     v
U-mode user program
~~~

이후 U-mode program은 kernel 함수를 직접 호출하지 않는다.

kernel 기능이 필요하면 `ecall`을 사용해 trap을 발생시키고,
syscall interface를 통해 S-mode kernel로 진입한다.

---

# 16. Important Design Decisions

## 16.1 OpenSBI Dependency

Mini-RVOS가 M-mode firmware를 직접 구현하지 않고 OpenSBI를 사용한다.

장점:

- 구현 범위가 작아진다.
- OS 핵심 기능에 집중할 수 있다.
- SBI 표준 interface를 사용할 수 있다.

단점:

- OpenSBI 없이 독립적으로 boot할 수 없다.

---

## 16.2 Small Assembly Entry

assembly는 반드시 필요한 low-level 작업만 담당한다.

현재 boot assembly의 핵심 역할:

~~~text
stack setup
BSS initialization
C entry
~~~

복잡한 OS 초기화는 C에서 처리한다.

장점:

- 코드 가독성이 좋아진다.
- architecture-dependent 부분과 kernel logic을 분리할 수 있다.

---

## 16.3 Fixed Hardware Configuration

현재 Mini-RVOS는 QEMU `virt` machine을 대상으로 한다.

대표적인 고정값:

~~~text
kernel base = 0x80200000
RAM end     = 0x88000000
UART        = 0x10000000
~~~

Device Tree에서 hardware configuration을 읽지 않고
kernel이 해당 환경을 미리 알고 있다고 가정한다.

장점:

- 구현이 단순하다.

단점:

- 다른 RISC-V machine으로 이식하기 어렵다.

---

# 17. Current Limitations

현재 boot architecture의 주요 한계:

- single hart만 사용
- Device Tree parsing 없음
- dynamic hardware discovery 없음
- fixed RAM layout
- fixed UART address
- OpenSBI dependency
- SMP boot 없음
- 별도의 bootloader 없음
- arbitrary user ELF loading 없음

이 기능들은 범용 operating system에서는 중요하지만,
Mini-RVOS의 현재 목적을 달성하기 위해 모두 필요한 것은 아니다.

---

# 18. Final Boot Flow

Mini-RVOS의 전체 boot path를 정리하면 다음과 같다.

~~~text
QEMU starts
      |
      v
OpenSBI executes in M-mode
      |
      v
OpenSBI prepares S-mode environment
      |
      v
Mini-RVOS kernel
0x80200000
      |
      v
_start
      |
      +--> sp = stack_top
      |
      +--> zero BSS
      |
      v
kernel_main
      |
      +--> PMM initialization
      |
      +--> filesystem initialization
      |
      +--> process creation
      |
      +--> trap initialization
      |
      v
process_start
      |
      v
sret
      |
      v
U-mode shell
~~~

이 boot path가 Mini-RVOS의 모든 subsystem이 시작되는 기반이다.
