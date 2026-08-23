# Mini-RVOS Physical Memory Management

## 1. Overview

Mini-RVOS는 physical memory를 4 KiB 단위의 page로 관리한다.

현재 Physical Memory Manager(PMM)는 사용 가능한 physical page를 free-list로 관리한다.

~~~text
usable physical memory
        |
        v
4 KiB pages
        |
        v
free-list
        |
        +--> page_alloc()
        |
        +--> page_free()
~~~

Mini-RVOS의 PMM은 variable-size allocator가 아니다.

즉 다음과 같은 요청을 직접 처리하지 않는다.

~~~text
malloc(10 bytes)
malloc(200 bytes)
malloc(3000 bytes)
~~~

PMM이 다루는 기본 단위는 항상 하나의 physical page다.

~~~text
PAGE_SIZE = 4096 bytes
~~~

---

# 2. Physical Memory vs Virtual Memory

Physical memory와 virtual memory는 구분해야 한다.

Physical memory는 실제 RAM address를 의미한다.

예:

~~~text
0x80200000
0x80300000
0x81000000
~~~

Virtual memory는 CPU가 program을 실행하면서 사용하는 virtual address space다.

Sv39 page table은 다음 관계를 만든다.

~~~text
virtual address
      |
      v
page table
      |
      v
physical address
~~~

Mini-RVOS의 PMM은 이 중 physical page를 관리한다.

즉:

~~~text
PMM
    "사용 가능한 실제 RAM page를 하나 달라"

VM
    "이 virtual address를 저 physical page에 연결하라"
~~~

이다.

따라서 PMM과 VM은 서로 다른 역할을 가진다.

---

# 3. Page Size

`include/memory.h`에는 다음 상수가 정의되어 있다.

~~~c
#define PAGE_SIZE 4096UL
~~~

즉 하나의 page는:

~~~text
4096 bytes
= 4 KiB
~~~

다.

RISC-V Sv39의 기본 page size 역시 4 KiB이므로,
physical allocator와 page table mapping 단위를 맞추기 좋다.

---

# 4. Why Page-Based Allocation

OS kernel에서는 page table을 만들거나
process의 memory를 구성할 때 physical page가 필요하다.

예:

~~~text
page table page
user text page
user rodata page
user stack page
kernel data page
~~~

이런 구조를 모두 같은 4 KiB 단위로 관리하면
physical memory allocation 단위를 4 KiB page로 통일할 수 있다.

Mini-RVOS에서는:

~~~text
page_alloc()
~~~

을 호출하면 4 KiB physical page 하나를 얻는다.

사용이 끝나면:

~~~text
page_free()
~~~

로 반환한다.

---

# 5. PMM Data Structure

현재 allocator의 핵심 자료구조는 다음과 같다.

~~~c
struct free_page {
    struct free_page *next;
};
~~~

그리고 free page들의 시작점은:

~~~c
static struct free_page *free_list;
~~~

이다.

구조는 다음과 같다.

~~~text
free_list
    |
    v
+----------+
| next ----+----+
+----------+    |
               v
          +----------+
          | next ----+----+
          +----------+    |
                         v
                    +----------+
                    | next = 0 |
                    +----------+
~~~

즉 singly linked list다.

---

# 6. Intrusive Free List

Mini-RVOS의 allocator에는 별도의 metadata array가 없다.

대신 사용되지 않는 physical page의 첫 부분 자체를
`struct free_page`로 사용한다.

예를 들어 free page 하나가 있다고 하자.

~~~text
physical page
+-------------------------+
| next pointer             |
|                         |
| unused space             |
|                         |
|                         |
+-------------------------+
4096 bytes
~~~

page가 free 상태이므로
그 안의 내용을 보존할 필요가 없다.

따라서 page의 첫 8 bytes를
다음 free page를 가리키는 pointer로 사용할 수 있다.

이런 형태를 intrusive free list라고 볼 수 있다.

장점:

- 별도 metadata memory가 거의 필요 없다.
- free-list 삽입과 제거는 pointer 갱신으로 수행된다.
- allocation/free가 빠르다.

---

# 7. Kernel Memory Must Not Be Allocated

physical RAM 전체를 free page로 만들면 안 된다.

이미 Mini-RVOS kernel image가 RAM 일부를 사용하고 있기 때문이다.

RAM의 개념적 모습은 다음과 같다.

~~~text
low address

+-----------------------+
| OpenSBI / firmware    |
+-----------------------+
| Mini-RVOS kernel      |
| text                  |
| user template         |
| rodata                |
| data                  |
| bss                   |
| initial stack         |
+-----------------------+
|                       |
| free physical memory  |
|                       |
+-----------------------+

high address
~~~

kernel이 사용하고 있는 page를 allocator에 넣으면
나중에 `page_alloc()`이 그 page를 반환할 수 있다.

그러면 page table이나 process memory가
kernel code/data를 덮어쓰게 된다.

따라서 PMM은 kernel image 이후부터만 관리한다.

---

# 8. __kernel_end

linker script는 kernel image의 끝을 나타내는 symbol을 만든다.

~~~text
__kernel_end
~~~

`memory.c`에서는 다음처럼 사용한다.

~~~c
extern char __kernel_end[];
~~~

PMM initialization에서 시작 주소는:

~~~c
unsigned long start =
    align_up((unsigned long)__kernel_end);
~~~

으로 계산한다.

즉:

~~~text
kernel image end
       |
       v
align to next 4 KiB boundary
       |
       v
first free physical page
~~~

가 된다.

---

# 9. Why Alignment Is Necessary

physical page allocator는 반드시 page-aligned address를 반환해야 한다.

정상적인 4 KiB page boundary 예:

~~~text
0x80300000
0x80301000
0x80302000
0x80303000
~~~

반면:

~~~text
0x80300123
~~~

같은 주소는 page boundary가 아니다.

그래서 Mini-RVOS는 `align_up()`과 `align_down()`을 사용한다.

---

# 10. align_up

현재 구현:

~~~c
static unsigned long align_up(
    unsigned long value)
{
    return (value + PAGE_SIZE - 1)
        & ~(PAGE_SIZE - 1);
}
~~~

목적은 값을 다음 page boundary까지 올리는 것이다.

예:

~~~text
value
0x80208123

        |
        v

align_up

        |
        v

0x80209000
~~~

kernel이 page 중간에서 끝났다면
그 page의 남은 부분만 allocator에 넣을 수는 없다.

그 page 일부는 이미 kernel이 사용하고 있기 때문이다.

그래서 완전히 비어 있는 다음 page부터 시작한다.

---

# 11. align_down

현재 구현:

~~~c
static unsigned long align_down(
    unsigned long value)
{
    return value & ~(PAGE_SIZE - 1);
}
~~~

목적은 memory 끝 주소를 이전 page boundary로 내리는 것이다.

예:

~~~text
memory_end
0x88000123

        |
        v

align_down

        |
        v

0x88000000
~~~

이렇게 하면 allocator가 RAM 범위를 넘어가는
불완전한 마지막 page를 관리하지 않는다.

---

# 12. pmm_init

현재 initialization의 핵심은 다음과 같다.

~~~c
void pmm_init(unsigned long memory_end)
{
    unsigned long start =
        align_up((unsigned long)__kernel_end);

    unsigned long end =
        align_down(memory_end);

    free_list = 0;
    free_page_count = 0;
    total_page_count = 0;

    for (unsigned long addr = start;
         addr + PAGE_SIZE <= end;
         addr += PAGE_SIZE) {

        page_free((void *)addr);
    }

    total_page_count =
        free_page_count;
}
~~~

전체 흐름은:

~~~text
__kernel_end
     |
     v
align_up
     |
     v
start

memory_end
     |
     v
align_down
     |
     v
end

start
  |
  +--> page
  |
  +--> page
  |
  +--> page
  |
  ...
  |
 end
~~~

각 page에 대해 `page_free()`를 호출해서
free-list에 삽입한다.

---

# 13. Why pmm_init Uses page_free

초기화 코드가 free-list를 직접 구성할 수도 있다.

하지만 현재 구현은 기존 `page_free()`를 재사용한다.

~~~c
page_free((void *)addr);
~~~

그러면 page를 free-list에 넣는 logic이
한 함수에만 존재하게 된다.

즉:

~~~text
initialization
       |
       v
page_free()

runtime free
       |
       v
page_free()
~~~

둘 다 같은 동작을 사용한다.

구현 중복을 줄일 수 있다는 장점이 있다.

---

# 14. page_free

현재 구현:

~~~c
void page_free(void *ptr)
{
    struct free_page *page =
        (struct free_page *)ptr;

    page->next = free_list;
    free_list = page;

    free_page_count++;
}
~~~

기존 list가 다음과 같다고 하자.

~~~text
free_list
    |
    v
[A] -> [B] -> [C] -> NULL
~~~

새로운 page `[X]`를 free하면:

~~~text
X.next = free_list
free_list = X
~~~

결과:

~~~text
free_list
    |
    v
[X] -> [A] -> [B] -> [C] -> NULL
~~~

가 된다.

즉 list 앞쪽에 삽입한다.

시간 복잡도는:

~~~text
O(1)
~~~

이다.

---

# 15. page_alloc

현재 구현:

~~~c
void *page_alloc(void)
{
    if (free_list == 0) {
        return 0;
    }

    struct free_page *page =
        free_list;

    free_list = page->next;
    free_page_count--;

    return page;
}
~~~

기존 상태:

~~~text
free_list
    |
    v
[A] -> [B] -> [C] -> NULL
~~~

`page_alloc()`을 실행하면:

~~~text
return A

free_list
    |
    v
[B] -> [C] -> NULL
~~~

가 된다.

역시 list의 head만 제거하므로:

~~~text
O(1)
~~~

이다.

---

# 16. Out of Memory

free-list가 비어 있으면:

~~~c
if (free_list == 0) {
    return 0;
}
~~~

을 실행한다.

즉 allocation 실패는:

~~~text
NULL
~~~

로 표현된다.

caller는 반드시 이 값을 확인해야 한다.

예:

~~~c
void *page = page_alloc();

if (page == 0) {
    /* allocation failure */
}
~~~

---

# 17. Page Counters

Mini-RVOS는 두 개의 counter를 유지한다.

~~~c
static unsigned long free_page_count;
static unsigned long total_page_count;
~~~

각각:

~~~text
free_page_count
    현재 사용 가능한 page 수

total_page_count
    PMM이 처음 관리하기 시작한 전체 page 수
~~~

를 의미한다.

조회 함수:

~~~c
pmm_free_pages()
pmm_total_pages()
~~~

가 존재한다.

---

# 18. Counter Behavior

초기화 직후:

~~~text
free_page_count = total_page_count
~~~

이다.

page 하나를 allocation하면:

~~~text
free_page_count -= 1
~~~

page 하나를 반환하면:

~~~text
free_page_count += 1
~~~

하지만:

~~~text
total_page_count
~~~

는 초기화 이후 변하지 않는다.

예:

~~~text
initial state

total = 100
free  = 100

page_alloc()

total = 100
free  = 99

page_alloc()

total = 100
free  = 98

page_free()

total = 100
free  = 99
~~~

---

# 19. Current RAM Boundary

현재 `kernel_main`에서는 RAM 끝을 고정값으로 사용한다.

~~~text
RAM_END = 0x88000000
~~~

그리고:

~~~c
pmm_init(RAM_END);
~~~

형태로 PMM을 초기화한다.

즉 Mini-RVOS는 현재 Device Tree를 읽어서
실제 RAM size를 자동으로 알아내지 않는다.

QEMU 실행 설정:

~~~text
-m 128M
~~~

에 맞춰 고정된 RAM layout을 사용한다.

---

# 20. Why PMM Is Needed by Virtual Memory

Sv39 page table 자체도 memory 안에 존재해야 한다.

새 page table을 만들려면 physical page가 필요하다.

예:

~~~text
create page table
       |
       v
page_alloc()
       |
       v
physical page
       |
       v
use page as page-table page
~~~

process 생성 역시 동일하다.

예:

~~~text
create process
      |
      +--> allocate page table
      |
      +--> allocate user text page
      |
      +--> allocate user rodata page
      |
      +--> allocate user stack page
~~~

즉 PMM은 virtual memory와 process 구현의 기반이다.

---

# 21. Why PMM Does Not Know About Processes

현재 `page_alloc()`은 자신이 반환하는 page가
어디에 사용될지 알지 못한다.

PMM은 모든 allocation을 4 KiB physical page 단위로 처리한다.

예:

~~~text
page_alloc()
    |
    +--> page table일 수도 있음
    |
    +--> user stack일 수도 있음
    |
    +--> user code일 수도 있음
~~~

어떤 용도로 사용할지는 caller가 결정한다.

이렇게 하면 PMM과 상위 subsystem의 역할이 분리된다.

---

# 22. Why PMM Does Not Return Virtual Addresses

현재 Mini-RVOS는 초기 kernel environment에서
physical RAM 영역을 직접 접근 가능한 형태로 사용한다.

그러나 개념적으로 PMM의 책임은:

~~~text
physical page ownership
~~~

을 관리하는 것이다.

virtual address mapping 자체는 VM subsystem의 책임이다.

즉 다음 두 단계는 구분된다.

~~~text
1. page_alloc()

   physical page 확보

2. vm_map(...)

   virtual address에 physical page 연결
~~~

---

# 23. Strengths of the Current Design

현재 allocator의 장점:

## Simple

자료구조가 singly linked list 하나뿐이다.

## Fast

allocation:

~~~text
O(1)
~~~

free:

~~~text
O(1)
~~~

## Low metadata overhead

free page 자신의 첫 부분을 metadata로 사용한다.

## Suitable for page tables

Sv39 page table이 요구하는 4 KiB 단위와 잘 맞는다.

## Easy to verify

allocator 상태 변화를 이해하기 쉽다.

---

# 24. Current Limitations

현재 PMM은 4 KiB fixed-size page와 singly linked free-list를 사용한다.

다음 기능은 없다.

## 24.1 No Double-Free Detection

같은 page를 두 번 `page_free()`하면
free-list가 손상될 수 있다.

예:

~~~text
page_free(A)
page_free(A)
~~~

를 막는 검사가 없다.

---

## 24.2 No Address Validation

`page_free()`에 잘못된 pointer를 전달해도
현재 함수 자체에서는 검사하지 않는다.

예:

~~~text
unaligned address
kernel address
RAM outside address
~~~

를 자동으로 거부하지 않는다.

---

## 24.3 Allocated Pages Are Not Zeroed

`page_alloc()`은 page 내용을 0으로 만들지 않는다.

이전에 사용했던 data가 그대로 남아 있을 수 있다.

따라서 필요한 subsystem이 직접 초기화해야 한다.

특히 page table page는 사용 전에 반드시 깨끗하게 만들어야 한다.

---

## 24.4 No Contiguous Multi-Page Allocation

현재 API는 한 번에 page 하나만 반환한다.

~~~text
page_alloc()
    -> one 4 KiB page
~~~

연속된 physical page 여러 개를 요청하는 기능은 없다.

---

## 24.5 No Concurrency Protection

현재 allocator에는:

~~~text
spinlock
mutex
atomic operation
~~~

이 없다.

현재 Mini-RVOS는 single-hart 환경이므로 문제가 발생하지 않는다.

SMP 환경으로 확장하면 여러 CPU가 동시에 free-list를 수정할 수 있으므로
동기화가 필요하다.

---

## 24.6 No Buddy Allocator

Linux 같은 실제 OS에서는 더 복잡한 physical memory allocator를 사용한다.

예를 들어 buddy allocator는
여러 크기의 연속 page allocation을 효율적으로 관리할 수 있다.

Mini-RVOS의 현재 PMM은 buddy allocator 대신
singly linked free-list를 사용한다.

---

# 25. Important Design Decision

Mini-RVOS에서 PMM의 목표는:

~~~text
최고 성능의 general-purpose memory allocator
~~~

가 아니다.

목표는:

~~~text
Sv39 page tables와 process memory를 만들 수 있는
작고 이해하기 쉬운 physical page allocator
~~~

다.

따라서 다음 구조를 선택했다.

~~~text
4 KiB fixed-size pages
        +
singly linked free-list
        +
O(1) allocate/free
~~~

이 설계는 작은 교육/연구용 OS에 적합하다.

---

# 26. PMM Flow Summary

초기화:

~~~text
kernel end
    |
    v
align_up
    |
    v
first usable page
    |
    v
page_free()
    |
    v
free-list construction
    |
    v
RAM end
~~~

allocation:

~~~text
free_list
    |
    v
remove head
    |
    v
return physical page
~~~

free:

~~~text
returned page
    |
    v
insert at head
    |
    v
free_list
~~~

---

# 27. Final Mental Model

Mini-RVOS physical memory manager의 전체 구조는 다음과 같다:

~~~text
"kernel이 사용하지 않는 RAM을
4 KiB 조각으로 나눈 뒤
사용 가능한 조각들을 linked list로 관리한다."
~~~

이다.

PMM은 physical page를 제공한다.

~~~text
PMM
 |
 v
physical pages
 |
 +--> Sv39 page tables
 |
 +--> process address spaces
 |
 +--> user text
 |
 +--> user rodata
 |
 +--> user stack
~~~

이 위에 Virtual Memory Manager가 올라가면서
physical page와 virtual address의 관계를 만든다.
