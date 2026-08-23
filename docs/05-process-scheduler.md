# Mini-RVOS Process and Scheduler

## 1. Overview

Mini-RVOS의 process structure는 PID, page table, kernel stack, user stack, trap frame, syscall test state를 저장한다.

각 process는 최소한 다음 실행 상태를 가진다.

~~~text
Process
 |
 +--> PID
 |
 +--> Sv39 page table
 |
 +--> private user memory
 |
 +--> user stack
 |
 +--> kernel stack
 |
 +--> saved CPU context
       (trap frame)
~~~

이 정보를 함께 관리해야 process를 중단했다가
나중에 다시 실행할 수 있다.

Mini-RVOS에는 timer interrupt 기반의
2-process round-robin scheduler도 구현되어 있다.

다만 현재 v1.0 interactive shell checkpoint에서는
scheduler timer를 비활성화하고 shell process 하나만 실행한다.

즉 다음 두 구조를 구분해야 한다.

~~~text
implemented scheduler
    two-process timer preemption

current v1.0 shell mode
    single interactive process
    timer scheduling disabled
~~~

---

# 2. What Is a Process?

program과 process는 같은 개념이 아니다.

program은 실행할 code와 data 자체다.

process는 그 program이 실제로 실행되는 상태까지 포함한다.

예:

~~~text
Program

code
data
~~~

반면:

~~~text
Process

code
data
PID
register state
stack
page table
execution position
kernel state
~~~

를 가진다.

Mini-RVOS에서도 process를
실행 상태를 보관하는 kernel object로 표현한다.

---

# 3. Mini-RVOS Process Structure

현재 구조는 다음과 같다.

~~~c
struct process {
    unsigned long pid;

    pagetable_t pagetable;

    void *kernel_stack;
    void *user_stack;

    struct trap_frame *frame;

    int syscall_complete;
};
~~~

각 field는 서로 다른 역할을 가진다.

---

# 4. PID

~~~c
unsigned long pid;
~~~

PID는 process를 식별하기 위한 번호다.

예:

~~~text
Process A
    PID = 1

Process B
    PID = 2
~~~

process structure에는 PID와 함께 page table, stack, trap frame이 저장된다.

PID는 process를 구분하는 identifier다.

실제 실행을 위해서는 page table,
stack, CPU context 등이 필요하다.

---

# 5. Page Table

~~~c
pagetable_t pagetable;
~~~

각 process는 자신의 Sv39 root page table을 가진다.

예:

~~~text
Process 1
    root page table A

Process 2
    root page table B
~~~

따라서 같은 virtual address를 사용하더라도
서로 다른 physical page를 가리킬 수 있다.

---

# 6. Same VA, Different PA

예를 들어 두 process가 모두:

~~~text
VA = 0x40000000
~~~

를 stack으로 사용한다고 하자.

실제로는:

~~~text
Process 1

VA 0x40000000
      |
      v
PA 0x81000000


Process 2

VA 0x40000000
      |
      v
PA 0x82000000
~~~

처럼 서로 다른 RAM을 사용할 수 있다.

이것이 per-process address space다.

---

# 7. User Stack

현재 Mini-RVOS user stack virtual address:

~~~text
USER_STACK_BASE = 0x40000000
~~~

stack 크기:

~~~text
PAGE_SIZE = 4096 bytes
~~~

따라서:

~~~text
USER_STACK_TOP
    = 0x40001000
~~~

이다.

RISC-V stack은 아래쪽 주소 방향으로 성장하므로
초기 user `sp`는 stack top으로 설정한다.

~~~c
frame->sp = USER_STACK_TOP;
~~~

---

# 8. Private User Stack

`process_create()`는:

~~~text
page_alloc()
~~~

으로 새로운 physical page를 얻는다.

그 page를:

~~~text
VA 0x40000000
~~~

에 mapping한다.

permission은:

~~~text
R
W
U
A
D
~~~

이다.

따라서 process마다 동일한 stack VA를 가지지만
backing physical page는 독립적이다.

---

# 9. Kernel Stack

각 process는 user stack과 별도로
kernel stack도 가진다.

~~~c
void *kernel_stack;
~~~

kernel stack은 trap이나 syscall이 발생한 뒤
S-mode에서 사용한다.

구조:

~~~text
U-mode
    user stack

       |
       | trap
       v

S-mode
    process kernel stack
~~~

---

# 10. Why Each Process Needs a Kernel Stack

process마다 별도의 kernel stack을 가지는 이유는
kernel execution state 역시 process마다 달라질 수 있기 때문이다.

예:

~~~text
Process A
    trap -> kernel stack A

Process B
    trap -> kernel stack B
~~~

scheduler가 A 실행을 중단하고 B를 실행할 때
A의 kernel-side saved context를 그대로 유지할 수 있다.

---

# 11. Trap Frame Inside Kernel Stack

Mini-RVOS는 kernel stack의 위쪽에
초기 trap frame을 배치한다.

개념:

~~~text
kernel stack page

low address

+----------------------+
|                      |
| free kernel stack    |
|                      |
+----------------------+
| trap frame           |
+----------------------+

high address
~~~

현재 계산:

~~~c
unsigned long kernel_stack_top =
    (unsigned long)kernel_stack +
    PAGE_SIZE;

struct trap_frame *frame =
    (struct trap_frame *)
    (kernel_stack_top -
     TRAP_FRAME_SIZE);
~~~

즉 kernel stack top 바로 아래에
trap frame 공간을 만든다.

---

# 12. Why Initial Trap Frame Exists

새 process는 아직 실제로 CPU에서 실행된 적이 없다.

따라서 저장된 register context도 존재하지 않는다.

하지만 scheduler/trap restore mechanism은:

~~~text
trap frame
~~~

을 받아 register를 복원하는 방식으로 process를 실행한다.

그래서 새 process를 만들 때
"처음 실행될 CPU 상태"를 trap frame 형태로 미리 만든다.

---

# 13. Initial Process Context

새 process의 초기 frame은 0으로 초기화된다.

~~~c
clear_frame(frame);
~~~

그리고 핵심 값만 설정한다.

~~~c
frame->sp = USER_STACK_TOP;

frame->sepc =
    (unsigned long)user_entry;

frame->sstatus = 0;
~~~

즉 처음 실행할 때:

~~~text
PC
    user_entry

SP
    top of user stack

return privilege
    U-mode
~~~

가 되도록 준비한다.

---

# 14. Why sepc Points to user_entry

`sepc`는 `sret` 이후 실행할 PC를 결정한다.

따라서 새 process의:

~~~c
frame->sepc =
    (unsigned long)user_entry;
~~~

설정은:

~~~text
sret
 |
 v
PC = user_entry
~~~

를 의미한다.

---

# 15. Process Entry Point

현재 process의 초기 execution address는:

~~~text
user_entry
~~~

이다.

`process_create()`는:

~~~c
frame->sepc =
    (unsigned long)user_entry;
~~~

로 초기 PC를 설정한다.

`user_entry`는 linker section의 시작 주소를 계산해서 사용하는 값이 아니라
assembly에서 정의된 명시적인 symbol이다.

---

# 16. User Text Is Private

현재 user program code는
kernel ELF 안에 template 형태로 존재한다.

하지만 process page table에서
그 kernel image page 자체를 그대로 U-mode에 공개하지 않는다.

`process_create()`는 각 user text page마다:

~~~text
page_alloc()
      |
      v
copy original user text
      |
      v
private physical page
      |
      v
map into process
~~~

를 수행한다.

---

# 17. User Read-Only Data Is Also Private

user rodata 역시 같은 방식으로 복사된다.

~~~text
linked user rodata
      |
      v
new physical page
      |
      v
copy
      |
      v
process mapping
~~~

permission:

~~~text
R
U
A
~~~

를 사용한다.

write permission은 없다.

---

# 18. copy_user_region()

현재 user region 복사 과정:

~~~text
for each 4 KiB page
        |
        v
page_alloc()
        |
        v
copy source page
        |
        v
vm_map_page()
~~~

이다.

이 방식으로 각 process의 user memory를
독립 physical page에 둘 수 있다.

---

# 19. Kernel Mappings

process page table에는 user memory만 있는 것이 아니다.

kernel이 trap 이후 계속 실행되려면
kernel address도 mapping되어 있어야 한다.

현재 각 process root에는 다음 영역이 mapping된다.

~~~text
kernel text
    R-X

kernel rodata
    R--

kernel data
    RW-

remaining RAM
    RW-

UART
    RW-
~~~

user permission인 `PTE_U`는 주지 않는다.

---

# 20. Why Kernel Is in Every Process Page Table

U-mode에서 trap이 발생해도
CPU가 자동으로 다른 page table로 바꾸지는 않는다.

따라서 현재 process page table에서
kernel code를 실행할 수 있어야 한다.

구조:

~~~text
process page table
 |
 +--> user mappings
 |      PTE_U = 1
 |
 +--> kernel mappings
        PTE_U = 0
~~~

이렇게 하면 하나의 page table 안에서
user와 kernel address를 함께 유지할 수 있다.

---

# 21. process_create() Flow

전체 process 생성 과정:

~~~text
process_create()
      |
      +--> initialize metadata
      |
      +--> vm_create()
      |
      +--> map kernel
      |
      +--> copy private user text
      |
      +--> copy private user rodata
      |
      +--> allocate user stack
      |
      +--> allocate kernel stack
      |
      +--> map user stack
      |
      +--> build initial trap frame
      |
      v
ready process
~~~

---

# 22. Current Process

Mini-RVOS는:

~~~c
static struct process *current_process;
~~~

를 사용한다.

이 pointer는 현재 CPU에서 실행 중인 process를 나타낸다.

현재 single-hart OS이므로
global pointer 하나로 충분하다.

---

# 23. process_activate()

process를 현재 실행 context로 활성화하는 함수:

~~~c
void process_activate(
    struct process *process)
~~~

는 두 가지 핵심 작업을 한다.

~~~text
current_process = process
        |
        v
vm_enable(process->pagetable)
~~~

즉:

1. kernel이 현재 process를 알게 한다.
2. CPU의 current page table을 바꾼다.

---

# 24. Why Address-Space Switching Matters

process pointer만 바꾸고
`satp`를 바꾸지 않으면 CPU는 여전히
이전 process의 page table을 사용한다.

따라서 실제 context switch에는:

~~~text
process metadata switch
        +
address-space switch
~~~

가 모두 필요하다.

Mini-RVOS에서는 `process_activate()`가
`vm_enable()`을 호출해 이를 수행한다.

---

# 25. process_start()

새 process를 실제로 처음 실행하는 함수:

~~~c
void process_start(
    struct process *process)
~~~

흐름:

~~~text
process_activate(process)
        |
        v
trap_resume(process->frame)
        |
        v
restore prepared context
        |
        v
sret
        |
        v
U-mode
~~~

즉 특별한 별도 "user mode entry" code path를 만드는 대신
기존 trap return mechanism을 재사용한다.

---

# 26. Why Reuse trap_resume()

process 시작과 trap return은 본질적으로 비슷한 작업이 필요하다.

둘 다:

~~~text
register state 준비
      |
      v
restore registers
      |
      v
restore sepc/sstatus
      |
      v
sret
~~~

가 필요하다.

따라서 초기 process 실행에도
trap restore path를 재사용할 수 있다.

---

# 27. Context

CPU context란 process 실행을 다시 이어가기 위해
필요한 CPU 상태를 말한다.

Mini-RVOS에서는 trap frame이 이를 보관한다.

대표적으로:

~~~text
program counter
stack pointer
general registers
argument registers
saved registers
temporary registers
sstatus
~~~

등이다.

---

# 28. Context Switch

context switch는:

~~~text
현재 process CPU state 저장
        |
        v
다음 process 선택
        |
        v
address space 변경
        |
        v
다음 process CPU state 복원
~~~

과정이다.

Mini-RVOS에서는 timer trap과 trap frame을 이용한다.

---

# 29. Preemptive Scheduling

cooperative scheduling에서는 process가
스스로 CPU를 양보해야 한다.

~~~text
Process A
 |
 | yield
 v
Scheduler
~~~

preemptive scheduling에서는
process가 양보하지 않아도 timer interrupt가 실행을 끊는다.

~~~text
Process A
 |
 | running
 |
 | TIMER INTERRUPT
 v
Kernel
 |
 v
Scheduler
~~~

Mini-RVOS scheduler는 timer 기반 preemption을 구현했다.

---

# 30. Timer Interrupt to Scheduler

timer interrupt가 발생하면 trap handler에서:

~~~text
scheduler_on_timer(frame)
~~~

를 호출한다.

여기서 `frame`은
현재 process의 방금 저장된 CPU context다.

즉 timer interrupt가 context switch의
안전한 저장 지점을 제공한다.

---

# 31. Saving Previous Process

scheduler는 먼저:

~~~c
struct process *previous =
    processes[current_index];

previous->frame = frame;
~~~

을 수행한다.

즉 현재 timer trap에서 생성된 trap frame을
현재 process object에 저장한다.

개념:

~~~text
CPU running Process A
        |
        v
timer trap
        |
        v
frame A created
        |
        v
A->frame = frame A
~~~

이다.

---

# 32. Selecting the Next Process

현재 scheduler는 process 두 개를 사용한다.

~~~text
PROCESS_COUNT = 2
~~~

다음 index:

~~~c
(current_index + 1) %
PROCESS_COUNT
~~~

를 사용한다.

따라서 순서:

~~~text
Process 1
   |
   v
Process 2
   |
   v
Process 1
   |
   v
Process 2
   |
   ...
~~~

가 된다.

현재 scheduler는 두 process를 번갈아 선택하는 round-robin 방식을 사용한다.

---

# 33. Round Robin

round-robin scheduler는 runnable process에
차례대로 CPU time을 제공한다.

예:

~~~text
time slice 1
    Process A

time slice 2
    Process B

time slice 3
    Process C

time slice 4
    Process A
~~~

Mini-RVOS test scheduler에서는 process가 두 개이므로:

~~~text
A -> B -> A -> B -> ...
~~~

형태다.

---

# 34. Address-Space Switch During Scheduling

다음 process가 결정되면:

~~~c
process_activate(next);
~~~

를 호출한다.

이 함수가:

~~~text
current_process 변경
      +
satp 변경
~~~

을 수행한다.

이 과정에서 register context와
virtual memory context가 함께 바뀐다.

---

# 35. Returning the Next Trap Frame

`scheduler_on_timer()`의 return type은:

~~~c
struct trap_frame *
~~~

이다.

scheduler는 마지막에:

~~~c
return next->frame;
~~~

을 한다.

trap assembly는 그 frame을 복원한다.

따라서:

~~~text
trap came from Process A
       |
       v
scheduler returns B->frame
       |
       v
trap_restore
       |
       v
Process B resumes
~~~

가 가능하다.

---

# 36. Scheduler Does Not Directly Jump to User Code

scheduler가 다음 process function을 직접 호출하는 것이 아니다.

잘못된 mental model:

~~~text
scheduler
    |
    v
call process_B()
~~~

실제 구조:

~~~text
scheduler
    |
    v
choose saved frame
    |
    v
trap restore
    |
    v
restore CPU state
    |
    v
sret
~~~

이다.

process는 일반 C function call처럼 전환되지 않는다.

---

# 37. Context Switch and Virtual Memory Together

완전한 switch:

~~~text
Process A
 |
 | timer
 v
trap_entry
 |
 | save A registers
 v
frame A
 |
 v
scheduler
 |
 +--> A->frame = frame A
 |
 +--> choose B
 |
 +--> current_process = B
 |
 +--> satp = B page table
 |
 v
return B->frame
 |
 v
trap_restore
 |
 | restore B registers
 v
sret
 |
 v
Process B
~~~

이것이 Mini-RVOS의 preemptive process switching 핵심이다.

---

# 38. Isolation Between Processes

Mini-RVOS에는 address-space가 실제로 다른지 검사하는
test helper도 존재한다.

확인 대상:

~~~text
root page table

user text physical page

user stack physical page
~~~

두 process에서 이 값들이 서로 달라야
private address space라고 볼 수 있다.

---

# 39. process_has_private_user_memory()

이 함수는 user text VA를 translation한다.

그 physical address가
원래 linked user text address와 다른지 검사한다.

즉:

~~~text
linked template page
    !=
process private page
~~~

인지 확인한다.

---

# 40. process_address_spaces_distinct()

두 process에 대해 다음을 비교한다.

~~~text
root page table A != B

user text PA A != B

user stack PA A != B
~~~

이 검사는 process isolation implementation을
확인하기 위한 테스트 helper다.

---

# 41. Syscall and Current Process

syscall에서는 현재 process의 정보를 알아야 한다.

예:

~~~text
getpid()
file descriptor ownership
user page table validation
~~~

그래서 다음 함수가 존재한다.

~~~text
process_current_pid()

process_current_pagetable()
~~~

이 값은 `current_process`를 기준으로 반환된다.

---

# 42. Process and Filesystem

filesystem syscall 역시 current PID를 사용한다.

예:

~~~text
fs_open(pid, ...)
fs_read(pid, ...)
fs_write(pid, ...)
fs_close(pid, ...)
~~~

즉 file descriptor state도
process identity와 연결된다.

---

# 43. syscall_complete Field

현재 process structure에는:

~~~c
int syscall_complete;
~~~

가 있다.

이 field는 이전 scheduler/process test에서
각 process가 실제 syscall까지 실행했는지 확인하기 위해 사용된다.

즉 일반적인 production process state라기보다
현재 Mini-RVOS development/test history의 흔적에 가깝다.

---

# 44. Current v1.0 Interactive Mode

중요한 점은 현재 `kernel_main()`에서는
2-process scheduler를 실행하지 않는다는 것이다.

현재 흐름:

~~~text
create shell process PID 1
      |
      v
trap_init()
      |
      v
disable timer interrupt
      |
      v
process_start(shell)
~~~

즉 현재 interactive shell은:

~~~text
one process
no timer preemption
~~~

모드다.

---

# 45. Why Scheduler Is Disabled in Current Shell

interactive shell은 UART에서 blocking input을 읽는다.

현재 checkpoint에서는 shell 기능을 안정적으로 검증하기 위해
timer scheduling을 사용하지 않는 구조로 두었다.

따라서:

~~~text
scheduler implementation exists
~~~

와

~~~text
current shell actively uses scheduler
~~~

를 혼동하면 안 된다.

현재 후자는 아니다.

---

# 46. Implemented vs Active

Mini-RVOS v1.0을 설명할 때 정확한 표현:

~~~text
Implemented:

- timer interrupt handling
- preemptive context switch
- two-process round robin
- per-process page-table switching

Current interactive shell configuration:

- one shell process
- timer scheduling disabled
~~~

이다.

"현재 shell에서 여러 process가 동시에 실행된다"고 설명하면
현재 코드 기준으로는 틀린 설명이다.

---

# 46.1 Scheduler Test Stop Condition

현재 scheduler test configuration에는:

~~~c
#define TEST_SWITCHES 6
~~~

이 정의되어 있다.

여섯 번째 context switch에서 두 process의 `syscall_complete` 값을 검사한다.

두 process가 모두 syscall을 실행했으면 다음 message를 출력한다.

~~~text
[OK] both processes executed
[OK] address space switching
[OK] process round robin
~~~

그 후:

~~~c
riscv_disable_timer_interrupt();
~~~

를 실행한다.

이 동작은 현재 scheduler test path의 종료 조건이다.

---

# 47. Current Scheduler Limitations

현재 scheduler는 두 개의 process pointer를 고정 배열에 저장하고 timer interrupt마다 다음 process를 선택한다.

## Fixed Process Count

현재:

~~~text
PROCESS_COUNT = 2
~~~

로 고정되어 있다.

---

## No Run Queue

일반적인 runnable task queue가 없다.

~~~text
ready queue
blocked queue
sleep queue
~~~

같은 구조도 없다.

---

## No Process State

예를 들어:

~~~text
RUNNING
RUNNABLE
BLOCKED
ZOMBIE
EXITED
~~~

같은 명시적 process state machine이 없다.

---

## No Dynamic Process Creation

runtime에서 새로운 process를 생성해서
scheduler queue에 넣는 구조가 없다.

---

## No Full Process Exit

현재 `SYS_EXIT`는 resource를 회수하거나
scheduler에서 process를 제거하지 않는다.

---

## No Blocking Scheduler Integration

UART나 filesystem operation 때문에 process가 기다릴 때
다른 process를 실행시키는 일반적인 blocking/wakeup system이 없다.

---

## No Priority

모든 process에 priority를 부여하는 기능이 없다.

---

## No Fairness Measurement

round-robin 구조는 존재하지만
실제 runtime fairness를 정량 측정하는 기능은 아직 없다.

이 부분은 이후 research benchmark 후보가 될 수 있다.

---

# 48. Current Process Limitations

현재 process subsystem은 kernel image에 포함된 user code를 private physical page로 복사해 process address space를 구성한다.

## No ELF Loader

user application은 별도 executable file에서 load되지 않는다.

kernel ELF 안의 user code template을 복사한다.

---

## No fork()

parent address space를 복제하는 process creation이 없다.

---

## No exec()

기존 process image를 다른 program으로 교체하는 기능이 없다.

---

## No Resource Destruction

process가 종료될 때:

~~~text
user pages
page tables
kernel stack
file descriptors
~~~

를 모두 정리하는 lifecycle implementation이 없다.

---

# 49. Subsystem Relationships

현재 process와 scheduler는
PMM, Sv39 VM, trap frame, timer interrupt를 함께 사용한다.

~~~text
process object
      |
      +--> page table
      +--> stacks
      +--> CPU context
      |
      v
timer trap
      |
      v
scheduler
      |
      v
address-space switch
      |
      v
context restore
~~~

즉 production scheduler의 복잡성을 제거하면서
process와 context switch의 본질을 확인할 수 있다.

---

# 50. Important Design Decision

Mini-RVOS에서 process를:

~~~text
"실행 중인 C function"
~~~

으로 생각하면 안 된다.

더 정확한 mental model은:

~~~text
"독립 address space와
저장 가능한 CPU execution state를 가진 kernel object"
~~~

다.

scheduler도:

~~~text
"다음 함수를 호출하는 코드"
~~~

가 아니라:

~~~text
"다음에 복원할 CPU context를 선택하는 코드"
~~~

로 이해해야 한다.

---

# 51. Process Mental Model

하나의 Mini-RVOS process:

~~~text
+--------------------------------+
| Process                        |
|                                |
| PID                            |
|                                |
| Sv39 root page table           |
|     |                          |
|     +--> kernel mappings       |
|     +--> private user text     |
|     +--> private user rodata   |
|     +--> private user stack    |
|                                |
| kernel stack                   |
|     |                          |
|     +--> saved trap frame      |
|                                |
+--------------------------------+
~~~

이다.

---

# 52. Scheduler Mental Model

현재 scheduler의 context-switch path는 다음과 같다:

~~~text
"현재 process의 context를 저장하고,
다음 process의 context와 address space를 선택해서
trap return mechanism으로 복원하는 코드"
~~~

다.

전체 구조:

~~~text
Timer
  |
  v
Trap
  |
  v
Save Process A
  |
  v
Scheduler
  |
  +--> choose Process B
  |
  +--> switch satp
  |
  v
Restore Process B
  |
  v
sret
~~~

---

# 53. Connection to Previous Subsystems

process와 scheduler는 앞에서 만든 subsystem 위에 올라간다.

~~~text
Physical Memory Manager
        |
        v
physical pages
        |
        v
Sv39 Virtual Memory
        |
        v
per-process address spaces
        |
        v
Trap mechanism
        |
        v
saved CPU contexts
        |
        v
Process
        |
        v
Scheduler
~~~

즉 scheduler 하나만 독립적으로 존재하는 것이 아니다.

PMM, VM, trap이 모두 준비되어야
실제 process switching이 가능하다.

---

# 54. Final Summary

Mini-RVOS process의 핵심:

~~~text
PID
+
private address space
+
user stack
+
kernel stack
+
saved trap frame
~~~

Mini-RVOS context switch의 핵심:

~~~text
save current trap frame
+
choose next process
+
switch page table
+
restore next trap frame
~~~

Mini-RVOS scheduler의 핵심:

~~~text
timer-driven
two-process
round robin
~~~

단, 현재 v1.0 interactive shell configuration은:

~~~text
PID 1 shell only
timer scheduler disabled
~~~

이다.

따라서 현재 구현을 설명할 때
"구현된 scheduler mechanism"과
"현재 활성화된 shell runtime configuration"을
구분해서 설명해야 한다.
