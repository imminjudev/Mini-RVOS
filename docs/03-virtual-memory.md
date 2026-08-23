# Mini-RVOS Sv39 Virtual Memory

## 1. Overview

Mini-RVOS는 RISC-V의 Sv39 virtual memory 방식을 사용한다.

Virtual memory의 기본 목적은 CPU가 사용하는 virtual address와
실제 RAM의 physical address를 분리하는 것이다.

전체 구조는 다음과 같다.

~~~text
CPU
 |
 | virtual address
 v
Sv39 page table
 |
 | address translation
 v
physical address
 |
 v
RAM
~~~

Mini-RVOS에서는 각 process가 자신의 page table을 가진다.

따라서 서로 다른 process가 같은 virtual address를 사용하더라도
실제로는 서로 다른 physical page를 사용할 수 있다.

---

# 2. Why Virtual Memory Is Needed

virtual memory가 없다면 program은 physical RAM address를 직접 사용해야 한다.

예를 들어 process A와 process B가 둘 다 다음 주소를 사용하고 싶다고 하자.

~~~text
0x40000000
~~~

physical memory를 직접 사용한다면 두 process가 같은 memory를 덮어쓸 수 있다.

virtual memory를 사용하면 다음과 같이 만들 수 있다.

~~~text
Process A

VA 0x40000000
      |
      v
PA 0x81000000


Process B

VA 0x40000000
      |
      v
PA 0x82000000
~~~

즉 virtual address는 같지만 physical address는 다를 수 있다.

이것이 process address-space isolation의 기반이다.

---

# 3. Physical Address and Virtual Address

PMM은 physical page를 관리한다.

~~~text
page_alloc()
      |
      v
physical page
~~~

VM은 그 physical page를 virtual address에 연결한다.

~~~text
vm_map_page()
      |
      v
virtual address
      |
      v
physical page
~~~

따라서 두 subsystem의 역할은 다음과 같다.

~~~text
PMM
    physical page ownership

VM
    virtual -> physical mapping
~~~

---

# 4. Sv39

Mini-RVOS는 RISC-V의 Sv39 translation mode를 사용한다.

Sv39라는 이름은 virtual address translation에서
39 bits를 사용하는 구조라는 의미다.

일반적인 Sv39 virtual address는 다음 요소로 나뉜다.

~~~text
virtual address

+---------+---------+---------+------------+
| VPN[2]  | VPN[1]  | VPN[0]  | page offset|
+---------+---------+---------+------------+
   9 bit     9 bit     9 bit      12 bit
~~~

합계:

~~~text
9 + 9 + 9 + 12 = 39 bits
~~~

각 VPN은 page table의 index로 사용된다.

---

# 5. Why 12-Bit Page Offset

Mini-RVOS의 page size는:

~~~text
4096 bytes
~~~

다.

4096은:

~~~text
2^12
~~~

이므로 하나의 page 내부 위치를 표현하려면 12 bits가 필요하다.

따라서 virtual address의 하위 12 bits는 page offset이다.

예:

~~~text
VA = 0x40000123

page base
    0x40000000

offset
    0x123
~~~

page table은 page 단위 주소를 translation하고,
마지막 12-bit offset은 그대로 physical address에 붙는다.

---

# 6. Three-Level Page Table

Sv39는 3단계 page table 구조를 사용한다.

~~~text
VA
 |
 +--> VPN[2]
 |      |
 |      v
 |   Level 2 page table
 |      |
 +--> VPN[1]
 |      |
 |      v
 |   Level 1 page table
 |      |
 +--> VPN[0]
        |
        v
     Level 0 page table
        |
        v
     leaf PTE
        |
        v
 physical page
~~~

Mini-RVOS의 `vm_walk()`가 이 과정을 software에서 수행한다.

---

# 7. Page Table Entry

Mini-RVOS에서는 page table entry를 다음 type으로 정의한다.

~~~c
typedef unsigned long pte_t;
~~~

즉 하나의 PTE는 64-bit unsigned integer다.

page table 자체는:

~~~c
typedef pte_t *pagetable_t;
~~~

로 표현한다.

---

# 8. Page Table Size

하나의 PTE는 8 bytes다.

Sv39의 각 VPN index는 9 bits다.

9 bits로 표현할 수 있는 entry 수는:

~~~text
2^9 = 512
~~~

이다.

따라서 하나의 page table 크기는:

~~~text
512 entries * 8 bytes
= 4096 bytes
= 4 KiB
~~~

다.

즉 page table 하나가 정확히 physical page 하나에 들어간다.

이 때문에 Mini-RVOS는 PMM의 `page_alloc()`으로
page table 자체도 할당할 수 있다.

---

# 9. PTE Flags

Mini-RVOS에서는 다음 PTE flag를 정의한다.

~~~c
#define PTE_V (1UL << 0)
#define PTE_R (1UL << 1)
#define PTE_W (1UL << 2)
#define PTE_X (1UL << 3)
#define PTE_U (1UL << 4)
#define PTE_G (1UL << 5)
#define PTE_A (1UL << 6)
#define PTE_D (1UL << 7)
~~~

각 flag의 의미는 다음과 같다.

~~~text
PTE_V
    Valid

PTE_R
    Readable

PTE_W
    Writable

PTE_X
    Executable

PTE_U
    User-mode accessible

PTE_G
    Global

PTE_A
    Accessed

PTE_D
    Dirty
~~~

---

# 10. PTE_V

`PTE_V`는 해당 entry가 유효한지를 나타낸다.

~~~text
PTE_V = 0
    invalid entry

PTE_V = 1
    valid entry
~~~

Mini-RVOS에서는 page table walk 중:

~~~c
if (*pte & PTE_V)
~~~

형태로 확인한다.

---

# 11. Leaf PTE vs Intermediate PTE

Sv39 page table에서는 모든 valid PTE가
physical data page를 의미하는 것은 아니다.

중간 단계 PTE는 다음 page table을 가리킬 수 있다.

~~~text
Level 2 PTE
     |
     v
Level 1 page table
~~~

반면 leaf PTE는 실제 mapped page를 나타낸다.

Mini-RVOS에서는 다음 flag 중 하나라도 있으면 leaf로 판단한다.

~~~text
PTE_R
PTE_W
PTE_X
~~~

즉:

~~~text
V only
    intermediate page-table entry

V + R/W/X
    leaf mapping
~~~

이라는 구조를 사용한다.

---

# 12. Virtual Page Number Extraction

`vm.c`에는 다음 macro가 있다.

~~~c
#define VPN_MASK 0x1FFUL
~~~

`0x1FF`는 binary로 9 bits가 모두 1인 값이다.

~~~text
0x1FF = 511
~~~

VPN index를 추출하는 macro:

~~~c
#define VPN_INDEX(va, level) \
    (((unsigned long)(va) >> \
      (12 + 9 * (level))) & VPN_MASK)
~~~

level에 따라 shift 값이 달라진다.

~~~text
level 0
    shift 12

level 1
    shift 21

level 2
    shift 30
~~~

즉:

~~~text
VPN[0] = VA bits 20..12
VPN[1] = VA bits 29..21
VPN[2] = VA bits 38..30
~~~

를 추출한다.

---

# 13. Physical Address Inside PTE

Sv39 PTE에는 physical page number가 저장된다.

Mini-RVOS는 다음 macro를 사용한다.

~~~c
#define PA_TO_PTE(pa) \
    (((unsigned long)(pa) >> 12) << 10)
~~~

physical address에서 하위 12-bit page offset을 제거하고,
PTE의 PPN 위치에 넣는다.

반대로:

~~~c
#define PTE_TO_PA(pte) \
    (((unsigned long)(pte) >> 10) << 12)
~~~

를 사용하면 PTE에서 physical page base address를 다시 얻는다.

---

# 14. vm_create

새 page table root를 만드는 함수:

~~~c
pagetable_t vm_create(void)
~~~

구조:

~~~text
page_alloc()
     |
     v
4 KiB physical page
     |
     v
page_zero()
     |
     v
new root page table
~~~

현재 구현은:

~~~c
pagetable_t root =
    (pagetable_t)page_alloc();

if (root == 0) {
    return 0;
}

page_zero(root);

return root;
~~~

형태다.

---

# 15. Why Page Tables Must Be Zeroed

PMM의 `page_alloc()`은 이전 page 내용을 지우지 않는다.

새 page table을 zero-fill하지 않으면
이전 data가 PTE처럼 해석될 수 있다.

예:

~~~text
old page data

0x0000000081234567
~~~

이 값을 page table entry로 잘못 해석하면
CPU가 존재하지 않는 mapping을 valid mapping으로 인식할 수 있다.

그래서:

~~~text
page_alloc()
     |
     v
page_zero()
     |
     v
use as page table
~~~

순서가 필요하다.

---

# 16. vm_walk

Mini-RVOS virtual memory implementation의 핵심 함수는:

~~~c
static pte_t *vm_walk(
    pagetable_t root,
    unsigned long va,
    int allocate)
~~~

이다.

목적은 특정 virtual address에 해당하는
최종 Level-0 PTE의 주소를 찾는 것이다.

---

# 17. vm_walk Flow

전체 흐름은 다음과 같다.

~~~text
root page table
      |
      | VPN[2]
      v
Level 2 PTE
      |
      v
Level 1 table
      |
      | VPN[1]
      v
Level 1 PTE
      |
      v
Level 0 table
      |
      | VPN[0]
      v
Level 0 PTE
~~~

현재 loop는:

~~~c
for (int level = 2;
     level > 0;
     level--)
~~~

형태다.

즉 Level 2와 Level 1을 따라 내려간 뒤
마지막 Level 0 entry의 pointer를 반환한다.

---

# 18. Missing Intermediate Page Table

page walk 도중 PTE가 invalid인데
새 mapping을 만드는 상황이라면
새 intermediate page table이 필요하다.

현재 구현:

~~~text
invalid intermediate PTE
       |
       v
allocate == 1 ?
       |
       +-- no --> fail
       |
       +-- yes
              |
              v
          page_alloc()
              |
              v
          page_zero()
              |
              v
          PTE points to new table
~~~

즉 mapping을 생성할 때 필요한 page table도
동적으로 만들어진다.

---

# 19. allocate Parameter

`vm_walk()`의 세 번째 parameter:

~~~text
allocate
~~~

는 intermediate page table을 새로 만들 수 있는지를 결정한다.

~~~text
allocate = 1
    없는 page table을 생성할 수 있음

allocate = 0
    existing mapping만 탐색
~~~

예:

~~~text
vm_map_page()
    vm_walk(..., 1)

vm_translate()
    vm_walk(..., 0)
~~~

이다.

---

# 20. Intermediate Leaf Rejection

현재 Mini-RVOS는 intermediate level에서
leaf PTE를 발견하면 실패한다.

즉 Level 2나 Level 1에서:

~~~text
PTE_R
PTE_W
PTE_X
~~~

중 하나가 설정되어 있으면:

~~~c
return 0;
~~~

한다.

이는 현재 구현이 Sv39 superpage mapping을 지원하지 않기 때문이다.

Mini-RVOS는 기본적으로 4 KiB page mapping만 사용한다.

---

# 21. Superpages

Sv39는 원래 더 큰 page mapping도 지원할 수 있다.

예:

~~~text
4 KiB
2 MiB
1 GiB
~~~

하지만 Mini-RVOS의 현재 `vm_walk()`는
중간 level leaf를 지원하지 않는다.

따라서 현재 구현은:

~~~text
4 KiB pages only
~~~

라고 이해하면 된다.

---

# 22. vm_map_page

한 개의 virtual page를 physical page에 연결하는 함수:

~~~c
int vm_map_page(
    pagetable_t root,
    unsigned long va,
    unsigned long pa,
    unsigned long flags)
~~~

이다.

---

# 23. Mapping Alignment

mapping할 virtual address와 physical address는
모두 4 KiB aligned여야 한다.

현재 구현은:

~~~c
if ((va & (PAGE_SIZE - 1)) != 0 ||
    (pa & (PAGE_SIZE - 1)) != 0) {

    return -1;
}
~~~

형태로 검사한다.

정상:

~~~text
VA 0x40000000
PA 0x81000000
~~~

비정상:

~~~text
VA 0x40000123
PA 0x81000456
~~~

page table은 page 단위 mapping을 만들기 때문에
base address가 page boundary에 있어야 한다.

---

# 24. Creating a Mapping

`vm_map_page()`은:

~~~text
vm_walk(root, va, 1)
~~~

을 호출한다.

필요한 intermediate page table이 없으면
새 page를 allocation한다.

마지막 PTE를 찾은 뒤:

~~~c
*pte =
    PA_TO_PTE(pa) |
    flags |
    PTE_V;
~~~

형태로 mapping을 만든다.

---

# 25. Duplicate Mapping Protection

이미 마지막 PTE가 valid하면
새 mapping을 만들지 않는다.

~~~c
if (pte == 0 ||
    (*pte & PTE_V)) {

    return -1;
}
~~~

즉 같은 VA에 두 번째 mapping을
조용히 덮어쓰지 않는다.

이는 page table corruption이나
예상하지 못한 mapping 변경을 방지한다.

---

# 26. vm_map_range

여러 page를 연속 mapping하기 위한 함수:

~~~c
int vm_map_range(
    pagetable_t root,
    unsigned long va,
    unsigned long pa,
    unsigned long size,
    unsigned long flags)
~~~

이다.

개념적으로:

~~~text
VA + 0x0000 -> PA + 0x0000
VA + 0x1000 -> PA + 0x1000
VA + 0x2000 -> PA + 0x2000
VA + 0x3000 -> PA + 0x3000
~~~

형태로 page마다 `vm_map_page()`를 호출한다.

---

# 27. Range Alignment

`vm_map_range()`에서는:

- VA
- PA
- size

모두 page-aligned인지 검사한다.

즉 size 역시:

~~~text
4096
8192
12288
...
~~~

처럼 4 KiB 배수여야 한다.

---

# 28. vm_translate

software에서 virtual address가
어떤 physical address로 연결되어 있는지 확인하기 위한 함수:

~~~c
unsigned long vm_translate(
    pagetable_t root,
    unsigned long va)
~~~

이다.

먼저:

~~~text
vm_walk(root, va, 0)
~~~

으로 existing PTE를 찾는다.

---

# 29. Translation Failure

다음 경우 translation은 실패한다.

~~~text
page table path 없음
PTE_V 없음
leaf PTE가 아님
~~~

이때 현재 함수는:

~~~text
0
~~~

을 반환한다.

---

# 30. Preserving Page Offset

leaf PTE에서 얻는 것은 physical page base다.

하지만 원래 VA가 page 중간을 가리키고 있을 수 있다.

예:

~~~text
VA = 0x40000123
~~~

mapping이:

~~~text
VA page 0x40000000
    ->
PA page 0x81000000
~~~

라면 최종 physical address는:

~~~text
0x81000123
~~~

이어야 한다.

현재 구현:

~~~c
return
    PTE_TO_PA(*pte) |
    (va & (PAGE_SIZE - 1));
~~~

이 바로 그 동작을 한다.

---

# 31. Kernel and User Permissions

page table은 address translation만 하는 것이 아니다.

접근 권한도 제어한다.

예:

~~~text
Kernel text
    R-X

Kernel rodata
    R--

Kernel data
    RW-

User text
    R-X + U

User rodata
    R-- + U

User stack
    RW- + U
~~~

이 permission은 PTE flag로 표현된다.

---

# 32. PTE_U

user process가 접근할 수 있는 page에는:

~~~text
PTE_U
~~~

가 필요하다.

`PTE_U`가 없는 mapping은
U-mode에서 접근할 수 없다.

따라서 kernel과 user memory를
같은 page table 안에 mapping하더라도
PTE permission으로 접근을 분리할 수 있다.

---

# 33. User Pointer Problem

syscall은 U-mode에서 kernel로 argument를 전달한다.

예:

~~~text
write(fd, buffer, length)
~~~

여기서 `buffer`는 user가 제공하는 pointer다.

user process는 악의적이거나 잘못된 pointer를 전달할 수 있다.

예:

~~~text
NULL

unmapped address

kernel address

read-only page

Sv39 range outside address
~~~

kernel이 이런 pointer를 그대로 사용하면
kernel fault나 memory corruption이 발생할 수 있다.

---

# 34. vm_user_range_valid

Mini-RVOS는 syscall에서 user pointer를 사용하기 전에:

~~~c
vm_user_range_valid()
~~~

을 사용한다.

함수는 다음 조건을 검사한다.

~~~text
page table exists

address is not NULL

address arithmetic does not overflow

address is in allowed Sv39 user range

every covered page exists

PTE_V exists

PTE_U exists

entry is leaf

required permissions exist
~~~

---

# 35. Zero-Length Range

현재 구현에서:

~~~text
size == 0
~~~

이면 valid로 처리한다.

이유는 0 byte 범위는
실제로 접근해야 할 memory가 없기 때문이다.

~~~c
if (size == 0) {
    return 1;
}
~~~

---

# 36. NULL Pointer Validation

0 byte가 아닌 범위에서:

~~~text
VA = 0
~~~

은 invalid로 처리한다.

~~~c
if (va == 0) {
    return 0;
}
~~~

이렇게 NULL user pointer를 syscall에서 거부한다.

---

# 37. Integer Overflow Validation

user가 매우 큰 주소와 size를 전달하면:

~~~text
va + size - 1
~~~

계산이 unsigned integer 범위를 넘어
다시 작은 값으로 wrap될 수 있다.

현재 구현은:

~~~c
unsigned long end =
    va + size - 1;

if (end < va) {
    return 0;
}
~~~

으로 overflow를 탐지한다.

---

# 38. Sv39 User Range

Mini-RVOS는 user virtual address로
Sv39 low canonical half만 허용한다.

현재 정의:

~~~c
#define SV39_USER_TOP \
    (1UL << 38)
~~~

따라서:

~~~text
0 <= user VA < 2^38
~~~

범위만 user pointer로 인정한다.

현재 구현은 start와 end 모두 이 범위 안에 있는지 확인한다.

---

# 39. Validation Across Multiple Pages

user buffer는 하나의 page 안에만 존재한다는 보장이 없다.

예:

~~~text
buffer start

0x40000FF0
       |
       | 16 bytes
-------+---------------- page boundary
       |
       | more data
       v
0x40001020
~~~

이 경우 buffer는 두 page에 걸친다.

따라서 첫 page만 검사하면 안 된다.

`vm_user_range_valid()`는 범위에 포함된 모든 page를 순회하며
각 PTE를 검사한다.

---

# 40. Required Permissions

kernel이 user memory를 읽을 때는:

~~~text
PTE_R
~~~

가 필요하다.

예:

~~~text
write()
open()
~~~

kernel이 user memory에 쓸 때는:

~~~text
PTE_W
~~~

가 필요하다.

예:

~~~text
read()
~~~

현재 validation 함수는:

~~~c
if ((entry & required_flags) !=
    required_flags) {

    return 0;
}
~~~

형태로 필요한 permission을 검사한다.

---

# 41. Why PTE_U Is Also Required

`PTE_R` 또는 `PTE_W`만 있다고 해서
user pointer로 인정하면 안 된다.

kernel page 역시:

~~~text
PTE_R
PTE_W
~~~

를 가질 수 있기 때문이다.

그래서 반드시:

~~~text
PTE_U
~~~

도 확인한다.

즉:

~~~text
readable kernel page
    !=
readable user page
~~~

다.

---

# 42. SUM and User Memory

RISC-V S-mode는 일반적으로 `PTE_U`가 설정된 user page에
아무 때나 직접 접근할 수 있도록 두지 않는다.

Mini-RVOS syscall code는 user buffer 접근이 필요한 동안:

~~~text
SSTATUS_SUM
~~~

을 임시로 활성화한다.

개념적으로:

~~~text
validate user pointer
        |
        v
enable SUM
        |
        v
access user memory
        |
        v
disable SUM
~~~

이다.

pointer validation과 SUM은 서로 다른 역할이다.

~~~text
vm_user_range_valid()
    "이 주소가 정말 허용된 user page인가?"

SUM
    "S-mode가 user page에 접근할 수 있도록 할 것인가?"
~~~

둘 다 필요하다.

---

# 43. Why SUM Should Not Stay Enabled

SUM을 항상 활성화해 두면
kernel code가 실수로 user memory를 접근할 가능성이 커진다.

따라서 Mini-RVOS는 필요한 syscall code 구간에서만 켜고,
사용 후 다시 끈다.

이것은 kernel/user isolation을 더 명확하게 유지하기 위한 설계다.

---

# 44. satp

RISC-V에서 현재 address translation configuration은:

~~~text
satp
~~~

CSR로 제어한다.

Mini-RVOS에서는 Sv39 mode bit를 다음과 같이 정의한다.

~~~c
#define SATP_MODE_SV39 (8UL << 60)
~~~

그리고 page table root의 physical page number를
`satp`에 넣는다.

---

# 45. vm_enable

현재 함수:

~~~c
void vm_enable(
    pagetable_t root)
~~~

는 다음 값을 만든다.

~~~c
unsigned long satp =
    SATP_MODE_SV39 |
    ((unsigned long)root >> 12);
~~~

즉:

~~~text
satp
+----------------------+-------------------+
| Sv39 mode            | root table PPN    |
+----------------------+-------------------+
~~~

형태다.

---

# 46. Switching Address Spaces

process마다 root page table이 다르다면
`satp` 값을 바꾸는 것으로 현재 address space를 바꿀 수 있다.

예:

~~~text
Process A
root = PA_A
      |
      v
satp = A

context switch

Process B
root = PA_B
      |
      v
satp = B
~~~

CPU가 이후 수행하는 virtual address translation은
새 root page table을 기준으로 이루어진다.

---

# 47. sfence.vma

CPU는 page table translation 결과를
TLB 같은 cache에 저장할 수 있다.

page table이나 `satp`를 바꿨는데
이전 translation이 남아 있으면 잘못된 physical page를 사용할 수 있다.

그래서 Mini-RVOS는:

~~~text
sfence.vma
~~~

를 사용한다.

현재 `vm_enable()`은:

~~~text
sfence.vma
    |
    v
write satp
    |
    v
sfence.vma
~~~

순서로 동작한다.

---

# 48. TLB Mental Model

개념적으로 CPU가 매번 3-level page table을
RAM에서 처음부터 탐색한다고 생각하면 느리다.

그래서 translation 결과를 cache한다.

~~~text
VA
 |
 +--> TLB hit
 |       |
 |       v
 |      PA
 |
 +--> TLB miss
         |
         v
   page-table walk
         |
         v
        PA
~~~

`sfence.vma`는 이전 translation 정보를
더 이상 신뢰해서는 안 된다는 것을 CPU에 알리는 역할을 한다.

---

# 49. Per-Process Address Space

Mini-RVOS process는 각자 page table root를 가진다.

구조:

~~~text
Process A
 |
 +--> page table A
 |       |
 |       +--> private user text
 |       +--> private user rodata
 |       +--> private user stack
 |
Process B
 |
 +--> page table B
         |
         +--> private user text
         +--> private user rodata
         +--> private user stack
~~~

같은 user virtual address를 사용해도
각 root가 다른 physical page를 가리킬 수 있다.

---

# 50. Shared Kernel Mapping

각 process의 page table에는
kernel이 동작하는 데 필요한 kernel mapping도 존재한다.

따라서 U-mode에서 trap이 발생해 S-mode로 들어온 뒤에도
kernel code가 실행될 수 있다.

하지만 user process는 `PTE_U`가 없는 kernel page에
U-mode에서 접근할 수 없다.

즉:

~~~text
same page table
    |
    +--> kernel mappings
    |       PTE_U = 0
    |
    +--> user mappings
            PTE_U = 1
~~~

형태로 isolation을 구성할 수 있다.

---

# 51. Why User and Kernel Mapping Coexist

trap이 발생한다고 해서 CPU가 자동으로
완전히 다른 page table로 교체되는 것은 아니다.

현재 Mini-RVOS에서는 process page table 안에
kernel mapping도 포함시켜 둔다.

따라서:

~~~text
U-mode
   |
   | trap
   v
S-mode
~~~

로 privilege level이 바뀌어도
kernel entry code를 계속 fetch할 수 있다.

---

# 52. Current VM Strengths

현재 Mini-RVOS VM의 장점:

## Clear 4 KiB Mapping Model

superpage 없이 기본 page만 사용하므로 이해하기 쉽다.

## Dynamic Intermediate Tables

필요한 page table만 PMM에서 동적으로 만든다.

## Per-Process Roots

process마다 독립 address space를 만들 수 있다.

## Permission Separation

PTE flag로 kernel/user, read/write/execute를 분리한다.

## User Pointer Validation

syscall에서 user-controlled pointer를 검사할 수 있다.

## Explicit Address-Space Switching

`satp`와 `sfence.vma` 관계를 직접 확인할 수 있다.

---

# 53. Current Limitations

현재 VM implementation은 4 KiB page mapping과 per-process Sv39 page table을 지원한다.

## 53.1 No Unmap

현재 public API에는:

~~~text
vm_unmap()
~~~

이 없다.

한 번 mapping한 page를 제거하고
page-table hierarchy를 정리하는 기능이 없다.

---

## 53.2 No Page Table Destruction

process가 종료될 때 page table 전체를 순회하면서
intermediate table까지 PMM에 반환하는 기능이 없다.

---

## 53.3 No Superpages

2 MiB / 1 GiB mapping을 지원하지 않는다.

현재는 4 KiB page만 사용한다.

---

## 53.4 No Demand Paging

page fault가 발생했을 때
필요한 memory를 그 순간 allocation하는 기능이 없다.

현재 process memory는 미리 mapping한다.

---

## 53.5 No Copy-on-Write

여러 process가 page를 공유하다가
write 시점에 복사하는 Copy-on-Write가 없다.

---

## 53.6 No Swap

RAM이 부족할 때 page를 disk로 내보내는
swap system이 없다.

---

## 53.7 No ASID Usage

`satp`에는 ASID를 사용할 수 있지만
현재 Mini-RVOS에서는 별도의 ASID management를 하지 않는다.

따라서 address-space switching에서
address-space switching마다 `sfence.vma`를 실행한다.

---

# 54. Important Design Decision

Mini-RVOS의 VM 목표는:

~~~text
완전한 production virtual memory subsystem
~~~

이 아니다.

목표는:

~~~text
Sv39 page-table 구조를 직접 구현하고

kernel/user isolation

per-process address spaces

syscall user-pointer safety

를 이해할 수 있는 최소한의 VM
~~~

이다.

따라서 현재 핵심 구조는:

~~~text
4 KiB pages
      +
3-level Sv39
      +
per-process roots
      +
PTE permissions
      +
satp switching
      +
user range validation
~~~

이다.

---

# 55. Complete Address Translation Mental Model

CPU가 user program에서 다음 address를 접근한다고 하자.

~~~text
VA = 0x40000123
~~~

전체 과정은 개념적으로:

~~~text
0x40000123
     |
     +--> VPN[2]
     |
     v
Level 2 page table
     |
     +--> VPN[1]
     |
     v
Level 1 page table
     |
     +--> VPN[0]
     |
     v
Level 0 leaf PTE
     |
     +--> physical page base
     |
     v
0x81000000
     |
     +--> offset 0x123
     |
     v
PA = 0x81000123
~~~

이다.

이 과정에서 permission도 동시에 검사된다.

---

# 56. Final Mental Model

Mini-RVOS virtual memory의 전체 구조는 다음과 같다:

~~~text
"각 process에게 자신만의 virtual address space를 제공하고,
Sv39 page table을 이용해 그 주소를 실제 physical RAM page에 연결한다."
~~~

이다.

그리고 보안 관점에서는:

~~~text
PTE_U
    kernel memory와 user memory 구분

PTE_R/W/X
    page 접근 권한 구분

vm_user_range_valid()
    syscall pointer 검사

SUM
    S-mode의 user memory 접근을 제한

satp
    현재 process address space 선택
~~~

이 핵심이다.

전체 관계:

~~~text
PMM
 |
 | physical pages
 v
Sv39 page tables
 |
 | mappings + permissions
 v
Process address space
 |
 | U-mode execution
 v
Syscalls
 |
 | validate user pointers
 v
Kernel
~~~

이 구조가 Mini-RVOS의 process isolation,
trap handling, syscall safety를 가능하게 하는 기반이다.
