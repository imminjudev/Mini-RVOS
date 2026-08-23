# Mini-RVOS Trap and System Call

## 1. Overview

Mini-RVOS에서 trap은 CPU 실행 흐름이 정상적인 instruction sequence에서
kernel의 trap handler로 강제로 전환되는 사건이다.

대표적인 원인은 다음과 같다.

~~~text
exception
    ecall
    page fault
    illegal instruction

interrupt
    timer interrupt
    external interrupt
~~~

Mini-RVOS에서는 trap mechanism을 이용해 다음 기능을 구현한다.

- U-mode syscall
- timer interrupt
- page fault handling
- scheduler entry
- privilege transition

전체 흐름은 대략 다음과 같다.

~~~text
U-mode program
      |
      | ecall / exception / interrupt
      v
CPU trap mechanism
      |
      v
stvec
      |
      v
trap_entry
      |
      v
save registers
      |
      v
trap_handler
      |
      +--> syscall
      +--> timer
      +--> page fault
      |
      v
restore registers
      |
      v
sret
      |
      v
previous execution context
~~~

---

# 2. Why Traps Are Needed

U-mode program은 kernel 함수를 직접 호출할 수 없다.

예를 들어 user shell이 다음 작업을 하고 싶다고 하자.

~~~text
write to UART
read keyboard
open file
read file
get PID
~~~

hardware 접근과 kernel data structure는
S-mode에서 관리되어야 한다.

따라서 U-mode program은 직접 kernel 함수를 호출하는 대신
CPU가 제공하는 controlled entry mechanism을 사용한다.

Mini-RVOS에서는:

~~~text
ecall
~~~

instruction이 그 역할을 한다.

---

# 3. Privilege Transition

정상적인 user code 실행:

~~~text
U-mode
~~~

에서 `ecall`이 발생하면 CPU는 trap을 발생시킨다.

그 결과:

~~~text
U-mode
   |
   | ecall
   v
S-mode
~~~

로 privilege level이 바뀐다.

kernel은 syscall을 처리한 뒤:

~~~text
sret
~~~

을 사용해서 다시 U-mode로 돌아간다.

---

# 4. Important Trap CSRs

Mini-RVOS trap handling에서 중요한 CSR은 다음과 같다.

~~~text
stvec
sscratch
sepc
scause
stval
sstatus
~~~

각각의 역할은 서로 다르다.

---

# 5. stvec

`stvec`는 S-mode trap이 발생했을 때
CPU가 실행을 시작할 address를 저장한다.

Mini-RVOS 초기화에서는:

~~~c
riscv_write_stvec(
    (unsigned long)trap_entry
);
~~~

를 실행한다.

즉:

~~~text
trap occurs
    |
    v
PC = trap_entry
~~~

가 된다.

`trap_entry`는 C 함수가 아니라 assembly code다.

trap 직후에는 CPU register state를 직접 다뤄야 하기 때문이다.

---

# 6. scause

`scause`는 trap이 왜 발생했는지를 나타낸다.

Mini-RVOS는:

~~~c
unsigned long cause =
    riscv_read_scause();
~~~

로 읽는다.

최상위 bit는 interrupt 여부를 나타내고,
나머지 bits는 cause code다.

개념적으로:

~~~text
scause

highest bit
    0 = exception
    1 = interrupt

remaining bits
    cause code
~~~

Mini-RVOS에서 사용하는 주요 cause:

~~~text
5
    Supervisor Timer Interrupt

8
    Environment Call from U-mode

15
    Store Page Fault
~~~

---

# 7. Exception vs Interrupt

Exception은 현재 실행 중인 instruction 때문에 발생한다.

예:

~~~text
ecall
page fault
illegal instruction
~~~

Interrupt는 현재 instruction 자체와 직접 관련 없이
외부 또는 timer event 때문에 발생한다.

예:

~~~text
timer interrupt
~~~

Mini-RVOS는 `scause`의 interrupt bit를 보고
두 종류를 구분한다.

---

# 8. sepc

`sepc`는 trap이 발생했을 때
원래 실행 중이던 instruction address를 저장한다.

예:

~~~text
U-mode

0x40001000
0x40001004
0x40001008  <- ecall
0x4000100c
~~~

`ecall`에서 trap이 발생하면:

~~~text
sepc = 0x40001008
~~~

같은 형태가 된다.

kernel이 syscall 처리를 끝낸 뒤
`sepc`를 기반으로 user execution 위치를 복원한다.

---

# 9. Why sepc += 4 for Syscalls

RISC-V의 일반적인 `ecall` instruction 길이는 4 bytes다.

trap이 발생하면 `sepc`는 `ecall` instruction 자체를 가리킨다.

그 상태 그대로 `sret`하면:

~~~text
ecall
 |
 trap
 |
 return
 |
 ecall
 |
 trap
 |
 return
 |
 ...
~~~

무한 반복이 발생한다.

그래서 syscall handler에서:

~~~c
frame->sepc += 4;
~~~

를 실행한다.

즉 return address를:

~~~text
ecall instruction
       |
       + 4 bytes
       |
       v
next instruction
~~~

으로 변경한다.

---

# 10. stval

`stval`은 exception과 관련된 추가 값을 제공한다.

특히 page fault에서는 fault를 발생시킨 address를 확인할 수 있다.

예:

~~~text
store page fault

stval = faulting virtual address
~~~

Mini-RVOS는 unexpected exception diagnostic에서:

~~~text
scause
sepc
stval
~~~

을 출력한다.

이 세 값을 함께 보면 low-level fault를 분석하기 쉽다.

---

# 11. sstatus

`sstatus`는 S-mode CPU 상태를 나타내는 CSR이다.

Mini-RVOS에서 중요한 bit:

~~~text
SIE
SPIE
SPP
SUM
~~~

특히 trap return과 user memory access에서 중요하다.

---

# 12. SPP

`SPP`는 trap 이전 privilege level과 관련된다.

Mini-RVOS에서 핵심적으로:

~~~text
SPP = 0
    return target is U-mode

SPP = 1
    return target is S-mode
~~~

로 사용한다.

trap handler에서는 user syscall인지 확인하기 위해:

~~~c
if (frame->sstatus & SSTATUS_SPP)
~~~

를 검사한다.

U-mode에서 발생한 `ecall`이어야 하므로
SPP가 S-mode를 나타내면 오류로 처리한다.

---

# 13. SUM

`SUM`은 S-mode가 `PTE_U`가 설정된 user page에
접근할 수 있는지 제어하는 bit다.

Mini-RVOS syscall에서는 user pointer validation이 끝난 후:

~~~text
enable SUM
      |
      v
access user memory
      |
      v
disable SUM
~~~

순서를 사용한다.

즉 SUM은 항상 켜두지 않는다.

---

# 14. Why Assembly Trap Entry Is Needed

trap이 발생한 순간 CPU register에는
trap 이전 program의 상태가 들어 있다.

C compiler는 함수 실행 과정에서 register를 자유롭게 사용할 수 있다.

따라서 바로 C handler를 호출하면
원래 context가 손상될 수 있다.

그래서 먼저 assembly에서 register를 저장한다.

~~~text
trap
 |
 v
assembly entry
 |
 v
save CPU state
 |
 v
C trap handler
~~~

---

# 15. Trap Frame

Mini-RVOS는 저장된 CPU context를:

~~~c
struct trap_frame
~~~

으로 표현한다.

주요 내용:

~~~text
ra
sp
gp
tp

t0-t6

s0-s11

a0-a7

sepc
sstatus
~~~

즉 일반 register와 trap return에 필요한 CSR 값을
하나의 structure에 저장한다.

---

# 16. Why Trap Frame Is Needed

trap 처리 중 scheduler가 다른 process를 선택할 수도 있다.

그렇다면 단순히 현재 register만 복원하는 것이 아니라
다른 process의 저장된 context를 복원할 수 있어야 한다.

구조:

~~~text
Process A running
      |
      v
timer trap
      |
      v
save A trap frame
      |
      v
scheduler
      |
      v
select B trap frame
      |
      v
restore B
      |
      v
Process B running
~~~

즉 trap frame은 syscall뿐 아니라
context switching의 기반이 된다.

---

# 17. Trap Frame Size

현재 Mini-RVOS에서는:

~~~c
#define TRAP_FRAME_SIZE 272UL
~~~

을 사용한다.

assembly의 register offset과
C structure layout이 정확히 일치해야 한다.

예:

~~~text
assembly
    sd a0, 72(sp)

C struct
    corresponding a0 field
~~~

이 layout이 어긋나면
C handler가 잘못된 register 값을 읽게 된다.

---

# 18. sscratch

Mini-RVOS trap entry에서 가장 중요한 CSR 중 하나가:

~~~text
sscratch
~~~

다.

현재 설계는 다음 규칙을 사용한다.

~~~text
S-mode execution
    sscratch = 0

U-mode execution
    sscratch = kernel stack top
~~~

이 값을 이용해 trap이 user context에서 왔는지 구분하고
user stack에서 kernel stack으로 전환한다.

---

# 19. Why User Stack and Kernel Stack Are Separate

U-mode program이 자신의 stack을 직접 관리한다.

만약 kernel trap handler도 같은 user stack을 사용한다면
user가 stack 내용을 조작해서 kernel execution에 영향을 줄 수 있다.

또 user stack이 invalid하거나 overflow된 상태에서
trap이 발생할 수도 있다.

따라서 kernel은 신뢰 가능한 별도 kernel stack을 사용한다.

~~~text
U-mode
    user stack

trap
    |
    v

S-mode
    kernel stack
~~~

---

# 20. U-mode Trap Stack Switch

U-mode에서 trap이 발생하기 전:

~~~text
sp
    user stack pointer

sscratch
    kernel stack top
~~~

trap entry 첫 부분:

~~~asm
csrrw sp, sscratch, sp
~~~

은 두 값을 교환한다.

결과:

~~~text
sp
    kernel stack top

sscratch
    original user sp
~~~

이 된다.

이제 kernel은 신뢰 가능한 kernel stack에서 실행할 수 있다.

---

# 21. Saving User SP

stack switch 이후 원래 user stack pointer는
`sscratch`에 들어 있다.

Mini-RVOS는 이를 trap frame의 `sp` field에 저장한다.

개념:

~~~text
sscratch
    original user sp
        |
        v
trap_frame.sp
~~~

이 값은 나중에 U-mode로 돌아갈 때 복원된다.

---

# 22. S-mode Trap Handling

trap은 U-mode에서만 발생하는 것이 아니다.

kernel S-mode 실행 중에도 exception이나 interrupt가 발생할 수 있다.

Mini-RVOS 규칙:

~~~text
S-mode
    sscratch = 0
~~~

이므로 trap entry에서 U-mode trap과 구분할 수 있다.

S-mode trap에서는 현재 supervisor stack을 계속 사용한다.

---

# 23. Saving Registers

Mini-RVOS의 `trap_entry`는 trap frame 공간을 확보한 뒤
register들을 저장한다.

개념:

~~~text
kernel stack

+--------------------+
| ra                 |
| sp                 |
| gp                 |
| tp                 |
| t0                 |
| ...                |
| a0                 |
| ...                |
| a7                 |
| ...                |
| sepc               |
| sstatus            |
+--------------------+
~~~

이 상태가 C의 `struct trap_frame`과 대응한다.

---

# 24. Calling trap_handler

register save가 끝나면:

~~~asm
mv a0, sp
call trap_handler
~~~

를 실행한다.

RISC-V C calling convention에서 `a0`는 첫 번째 argument다.

따라서 C에서는:

~~~c
struct trap_frame *trap_handler(
    struct trap_frame *frame)
~~~

형태로 현재 trap frame을 받는다.

---

# 25. trap_handler Dispatch

현재 trap handler의 주요 분기는:

~~~text
trap_handler
    |
    +--> interrupt?
    |       |
    |       +--> timer
    |
    +--> user ecall?
    |       |
    |       +--> syscall_handle
    |
    +--> store page fault?
    |       |
    |       +--> page fault test handling
    |
    +--> unexpected exception
~~~

이다.

즉 assembly는 CPU state 관리에 집중하고,
C handler가 trap policy를 결정한다.

---

# 26. Timer Interrupt

timer interrupt가 발생하면:

~~~text
scause interrupt bit = 1
cause code = 5
~~~

이다.

Mini-RVOS는:

~~~text
timer_ticks++
      |
      v
program next timer
      |
      v
scheduler_on_timer(frame)
~~~

순서로 처리한다.

scheduler가 다른 trap frame을 반환하면
trap restore code가 그 frame을 복원한다.

---

# 27. Store Page Fault

현재 Mini-RVOS에는 VM 검증 과정에서 사용했던
store page fault handler가 남아 있다.

cause code:

~~~text
15
~~~

를 처리한다.

현재 test path에서는 fault를 확인한 뒤:

~~~c
frame->sepc += 4;
~~~

로 fault instruction을 건너뛴다.

일반적인 production OS의 page fault handler와는 다르다.

현재 구현은 VM protection이 실제로 동작하는지 검증하기 위한
단순한 처리 방식이다.

---

# 28. System Call ABI

Mini-RVOS syscall ABI는 다음 register를 사용한다.

~~~text
a7
    syscall number

a0
    argument 0

a1
    argument 1

a2
    argument 2
~~~

return value는:

~~~text
a0
~~~

에 저장한다.

---

# 29. user_syscall3

C user code는 직접 register placement를 처리하지 않고
assembly wrapper를 호출한다.

C ABI 진입 시:

~~~text
a0 = syscall number
a1 = arg0
a2 = arg1
a3 = arg2
~~~

`user_syscall3`는 이를 Mini-RVOS syscall ABI로 변환한다.

~~~asm
mv a7, a0
mv a0, a1
mv a1, a2
mv a2, a3

ecall
ret
~~~

결과:

~~~text
before ecall

a7 = syscall number
a0 = arg0
a1 = arg1
a2 = arg2
~~~

가 된다.

---

# 30. Full Syscall Entry Flow

예를 들어 user code가:

~~~text
write(1, buffer, length)
~~~

를 호출한다고 하자.

전체 흐름:

~~~text
user shell
    |
    v
user syscall wrapper
    |
    +--> a7 = SYS_WRITE
    +--> a0 = fd
    +--> a1 = buffer
    +--> a2 = length
    |
    v
ecall
    |
    v
CPU switches to S-mode
    |
    v
PC = stvec
    |
    v
trap_entry
    |
    +--> kernel stack switch
    +--> save registers
    |
    v
trap_handler
    |
    v
syscall_handle
    |
    v
sys_write
~~~

---

# 31. syscall_handle

`syscall_handle()`은 가장 먼저:

~~~c
frame->sepc += 4;
~~~

를 수행한다.

그 다음:

~~~c
switch (frame->a7)
~~~

로 syscall number를 확인한다.

즉 user register에 저장되었던 `a7` 값이
trap frame을 통해 syscall dispatcher까지 전달된다.

---

# 32. Current Syscalls

Mini-RVOS의 현재 syscall 번호:

~~~text
1
    write

4
    getpid

5
    open

6
    read

7
    create

8
    close

9
    exit
~~~

syscall number는 kernel과 user code 사이의 ABI다.

따라서 양쪽이 같은 번호 정의를 공유해야 한다.

---

# 33. Syscall Arguments Through Trap Frame

trap 이전:

~~~text
a0 = arg0
a1 = arg1
a2 = arg2
a7 = syscall number
~~~

trap entry가 register를 저장하면:

~~~text
frame->a0
frame->a1
frame->a2
frame->a7
~~~

로 접근할 수 있다.

kernel syscall code는 CPU register를 직접 다루지 않고
trap frame field를 사용한다.

---

# 34. Syscall Return Value

syscall implementation이 결과를 반환하면
kernel은:

~~~c
frame->a0 = result;
~~~

형태로 저장한다.

trap restore에서 `a0` register가 복원되므로
U-mode로 돌아간 후 user wrapper에서는:

~~~text
a0 = syscall return value
~~~

가 된다.

RISC-V C ABI에서도 function return value가 `a0`이므로
wrapper는 그대로 `ret`할 수 있다.

---

# 35. User Pointer Validation

syscall argument 중 일부는 pointer다.

예:

~~~text
write
    buffer pointer

read
    destination buffer pointer

open
    path pointer
~~~

kernel은 user가 전달한 pointer를 신뢰하면 안 된다.

Mini-RVOS는 먼저:

~~~text
vm_user_range_valid()
~~~

를 사용해 mapping과 permission을 검사한다.

---

# 36. Readable vs Writable User Memory

`write()` syscall의 경우:

~~~text
user buffer
    |
    | kernel reads
    v
kernel
~~~

이므로 user buffer는:

~~~text
PTE_R
~~~

가 필요하다.

반대로 `read()`는:

~~~text
kernel
    |
    | writes
    v
user buffer
~~~

이므로:

~~~text
PTE_W
~~~

가 필요하다.

---

# 37. Kernel Access to User Memory

pointer validation을 통과한 뒤
S-mode가 실제 user page를 접근해야 한다.

Mini-RVOS는 필요한 구간에서:

~~~c
riscv_enable_user_memory_access();
~~~

를 호출한다.

이는 `SSTATUS_SUM`을 설정한다.

access가 끝나면:

~~~c
riscv_disable_user_memory_access();
~~~

로 다시 끈다.

---

# 38. Why Validation and SUM Are Different

두 기능은 목적이 다르다.

~~~text
vm_user_range_valid()

    "이 pointer가 허용된 user mapping인가?"
~~~

반면:

~~~text
SUM

    "S-mode가 PTE_U page를 실제로 접근할 수 있게 할 것인가?"
~~~

이다.

validation만 하고 SUM을 켜지 않으면
S-mode access가 제한될 수 있다.

SUM만 켜고 validation하지 않으면
잘못된 user pointer를 kernel이 사용할 수 있다.

따라서 둘 다 필요하다.

---

# 39. open() Path Copy

path는 C string이므로 길이를 미리 알 수 없다.

Mini-RVOS는 한 byte씩:

~~~text
validate address
      |
      v
read one byte
      |
      v
copy to kernel buffer
      |
      v
NUL?
~~~

과정을 반복한다.

최대:

~~~text
FS_NAME_MAX
~~~

까지만 허용한다.

이 방식은 user memory를 kernel internal string으로
안전하게 복사하기 위한 간단한 `copy_from_user` 형태다.

---

# 40. stdin read()

stdin에서 user buffer로 데이터를 쓸 때:

~~~text
UART input
    |
    v
kernel
    |
    v
user buffer
~~~

이므로 buffer가 writable인지 먼저 검사한다.

중요한 점은 validation이 UART blocking read보다 먼저 일어난다는 것이다.

잘못된 pointer라면:

~~~text
return -1
~~~

하고 즉시 끝난다.

---

# 41. exit()

현재 `SYS_EXIT`는 일반적인 process termination 구현이 아니다.

현재 동작:

~~~text
print exit message
      |
      v
disable timer interrupt
      |
      v
wfi loop forever
~~~

이다.

즉 process resource 회수나 scheduler removal은 하지 않는다.

현재 interactive shell checkpoint를 종료하기 위한
단순한 방식이다.

---

# 42. Trap Restore

C handler가 끝나면 복원할 trap frame pointer를 반환한다.

assembly `trap_restore`는:

~~~text
restore sepc
restore sstatus
restore registers
restore sp
sret
~~~

순서로 실행한다.

---

# 43. Restoring sepc and sstatus

먼저 trap frame에서:

~~~text
sepc
sstatus
~~~

를 CSR로 복원한다.

이 두 값은 `sret`의 동작을 결정하는 데 중요하다.

특히:

~~~text
sepc
    return PC

SPP
    return privilege mode
~~~

를 결정한다.

---

# 44. Preparing sscratch Before U-mode Return

U-mode로 돌아갈 경우
다음 trap에서 다시 kernel stack을 찾을 수 있어야 한다.

그래서 `trap_restore`는 kernel stack top을 계산해:

~~~text
sscratch = kernel stack top
~~~

으로 설정한다.

그리고 trap frame에 저장된 user `sp`를 복원한다.

결과적으로 `sret` 직전:

~~~text
sp
    user stack pointer

sscratch
    kernel stack top
~~~

상태가 다시 만들어진다.

---

# 45. sret

마지막 instruction:

~~~asm
sret
~~~

은 supervisor trap return이다.

CPU는 저장된 CSR 상태를 이용해:

~~~text
PC <- sepc
privilege <- previous mode
~~~

형태로 실행을 복구한다.

syscall이었다면 최종적으로:

~~~text
S-mode kernel
    |
    v
sret
    |
    v
U-mode
    |
    v
instruction after ecall
~~~

이 된다.

---

# 46. Complete Syscall Round Trip

전체 syscall 왕복을 한 번에 정리하면:

~~~text
U-mode user code
      |
      v
prepare a0-a2, a7
      |
      v
ecall
      |
      v
hardware trap
      |
      +--> save cause
      +--> save PC in sepc
      +--> enter S-mode
      +--> jump to stvec
      |
      v
trap_entry
      |
      +--> switch to kernel stack
      +--> save registers
      |
      v
trap_handler
      |
      v
syscall_handle
      |
      +--> sepc += 4
      +--> dispatch with a7
      +--> validate arguments
      +--> execute kernel operation
      +--> result -> a0
      |
      v
trap_restore
      |
      +--> restore CSR
      +--> restore registers
      +--> restore user stack
      |
      v
sret
      |
      v
U-mode
      |
      v
instruction after ecall
~~~

---

# 47. Why Syscalls Are Safer Than Direct Kernel Calls

U-mode에서 kernel function pointer를 직접 호출할 수 있다면
privilege separation이 의미가 없어질 수 있다.

syscall은 하나의 제한된 진입점을 제공한다.

~~~text
user
 |
 | controlled arguments
 v
syscall interface
 |
 | validation
 v
kernel
~~~

kernel은 어떤 operation을 허용할지 직접 결정할 수 있다.

---

# 48. Trap Mechanism and Scheduling

trap handling은 syscall만을 위한 것이 아니다.

timer interrupt 역시 같은 trap path를 사용한다.

~~~text
running process
      |
      v
timer interrupt
      |
      v
trap_entry
      |
      v
save context
      |
      v
scheduler
      |
      v
choose next context
      |
      v
trap_restore
~~~

따라서 trap frame은 syscall mechanism과
preemptive scheduling을 연결하는 핵심 구조다.

---

# 49. Important Design Decisions

## Assembly Handles Mechanism

assembly가 담당하는 것:

~~~text
stack switch
register save
register restore
sret
~~~

## C Handles Policy

C가 담당하는 것:

~~~text
trap cause dispatch
syscall dispatch
timer handling
fault policy
~~~

이 분리는 architecture-dependent code와
OS policy를 구분하기 위한 것이다.

---

# 50. Current Limitations

현재 trap/syscall subsystem에는 여러 제한이 있다.

## No General Page Fault Recovery

현재 page fault handling은 실제 demand paging을 구현하지 않는다.

## No Signal Mechanism

user process fault를 signal로 변환하는 기능이 없다.

## No Process Kill on Fault

잘못된 user process 하나만 종료시키는 대신
unexpected exception은 kernel을 멈추게 할 수 있다.

## No Full Process Exit

`SYS_EXIT`가 process resources를 회수하지 않는다.

## Small Syscall ABI

현재 syscall argument는 최대 3개를 중심으로 구성되어 있다.

## No Copyin/Copyout Abstraction

현재 user memory 접근 logic이 syscall code에 직접 들어가 있다.

더 큰 OS에서는 일반적으로:

~~~text
copy_from_user()
copy_to_user()
copy_string_from_user()
~~~

같은 abstraction을 둔다.

---

# 51. Final Mental Model

Mini-RVOS trap mechanism을 가장 간단히 표현하면:

~~~text
"CPU execution을 안전하게 kernel로 넘기고,
현재 context를 저장한 뒤,
원인을 처리하고,
원래 또는 다른 context로 복귀하는 mechanism"
~~~

이다.

syscall은 그 trap mechanism의 한 사용 사례다.

~~~text
ecall
    |
    v
trap
    |
    v
kernel syscall handler
    |
    v
sret
~~~

핵심 register와 CSR 관계:

~~~text
a7
    syscall number

a0-a2
    syscall arguments

a0
    return value

stvec
    trap entry address

scause
    trap reason

sepc
    return PC

stval
    fault-related value

sstatus.SPP
    previous privilege mode

sstatus.SUM
    S-mode user-memory access

sscratch
    kernel/user stack transition
~~~

Mini-RVOS에서 trap subsystem은
다음 세 가지를 연결하는 중심점이다.

~~~text
U-mode
   |
   v
syscalls

timer
   |
   v
scheduler

fault
   |
   v
exception handling
~~~

따라서 trap을 이해하면 syscall, privilege transition,
context switch의 구조가 함께 연결된다.
