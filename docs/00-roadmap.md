# Mini-RVOS Completion Roadmap

## 1. Goal

Mini-RVOS의 목표는 기능 수를 계속 늘리는 것이 아니다.

최종 목표는 다음 두 단계다.

1. RISC-V 운영체제의 핵심 구조를 직접 구현하고 설명할 수 있는 상태
2. 완성된 Mini-RVOS를 실험 플랫폼으로 사용해 독립 연구를 수행하는 상태

즉 다음이 중요하다.

- 코드가 왜 동작하는지 설명할 수 있어야 한다.
- 각 설계가 어떤 하드웨어/OS 원리에 기반하는지 알아야 한다.
- 현재 구현의 한계를 설명할 수 있어야 한다.
- 대안 설계와 비교할 수 있어야 한다.
- 연구 단계에서는 기능 추가보다 측정과 실험을 우선한다.

---

# 2. Implementation Understanding Phase

## 01. Boot and Privilege

확인할 내용:

- QEMU `virt`
- OpenSBI
- M-mode와 S-mode의 관계
- kernel entry address `0x80200000`
- linker script
- `_start`
- kernel stack
- BSS initialization
- `kernel_main`

완료 기준:

- OpenSBI에서 Mini-RVOS까지 제어가 어떻게 넘어오는지 설명할 수 있다.
- C 코드 실행 전에 assembly entry가 필요한 이유를 설명할 수 있다.
- linker script가 왜 필요한지 설명할 수 있다.

---

## 02. Physical Memory Management

확인할 내용:

- physical memory
- 4 KiB page
- free-list allocator
- `page_alloc`
- `page_free`
- kernel image 이후의 free memory

완료 기준:

- physical page allocator가 왜 필요한지 설명할 수 있다.
- virtual memory allocator와 physical memory allocator의 차이를 설명할 수 있다.
- Mini-RVOS의 allocator가 어떤 자료구조를 사용하는지 설명할 수 있다.

---

## 03. Sv39 Virtual Memory

확인할 내용:

- `satp`
- Sv39
- VPN
- PPN
- 3-level page table
- PTE
- `PTE_R`
- `PTE_W`
- `PTE_X`
- `PTE_U`
- `PTE_A`
- `PTE_D`
- `sfence.vma`
- kernel/user mappings
- per-process page tables
- user pointer validation

완료 기준:

- virtual address가 physical address로 변환되는 과정을 설명할 수 있다.
- Sv39가 왜 3단계 page table을 사용하는지 설명할 수 있다.
- user page와 kernel page가 PTE 수준에서 어떻게 구분되는지 설명할 수 있다.
- 잘못된 user pointer를 syscall에서 검증해야 하는 이유를 설명할 수 있다.

---

## 04. Trap and System Call

확인할 내용:

- `stvec`
- `sscratch`
- `sepc`
- `scause`
- `stval`
- `sstatus`
- `SPP`
- `SUM`
- trap frame
- `trap_entry`
- `trap_handler`
- `sret`
- `ecall`
- syscall ABI

완료 기준:

- U-mode의 `ecall`부터 syscall 반환까지 전체 흐름을 설명할 수 있다.
- trap 발생 시 register를 저장해야 하는 이유를 설명할 수 있다.
- user stack과 kernel stack을 분리한 이유를 설명할 수 있다.
- `sepc += 4`가 필요한 이유를 설명할 수 있다.
- `SUM`을 항상 켜두지 않는 이유를 설명할 수 있다.

---

## 05. Process and Scheduling

확인할 내용:

- process structure
- PID
- trap frame
- kernel stack
- user stack
- process page table
- address-space switching
- timer interrupt
- context switch
- round-robin scheduling

완료 기준:

- process가 단순히 PID 하나가 아닌 이유를 설명할 수 있다.
- 두 process의 virtual address가 같아도 physical memory가 다를 수 있는 이유를 설명할 수 있다.
- timer interrupt가 preemptive scheduling으로 연결되는 과정을 설명할 수 있다.
- context switch에서 어떤 CPU 상태를 보존해야 하는지 설명할 수 있다.

---

## 06. Filesystem and User Shell

확인할 내용:

- inode
- open file
- file descriptor
- file offset
- create
- open
- read
- write
- close
- stdin/stdout
- U-mode shell

완료 기준:

- inode와 file descriptor의 차이를 설명할 수 있다.
- process가 같은 파일을 여러 번 열었을 때 offset을 어떻게 관리하는지 설명할 수 있다.
- shell command가 filesystem syscall로 연결되는 흐름을 설명할 수 있다.

---

# 3. Freeze Point

핵심 이해가 완료되면 Mini-RVOS에 기능을 계속 추가하지 않는다.

다음 조건을 만족하면 구현 중심 개발을 동결한다.

- 핵심 subsystem을 설명할 수 있다.
- 자동 regression test가 존재한다.
- 실험용 instrumentation을 추가할 수 있다.
- benchmark를 반복 실행할 수 있다.
- 측정 결과를 machine-readable format으로 저장할 수 있다.
- 현재 구현의 한계를 문서화했다.

VirtIO, ELF loader, network, fork, pipe 등을 단순히 기능 수를 늘리기 위해 추가하지 않는다.

연구 질문에 필요할 경우에만 구현한다.

---

# 4. Research Phase

연구 단계의 기본 구조:

~~~text
Research Question
        |
        v
Hypothesis
        |
        v
System Modification
        |
        v
Benchmark Design
        |
        v
Experiment
        |
        v
Measurement
        |
        v
Data Analysis
        |
        v
Technical Report
~~~

## Research Question

좋은 연구 질문은 측정 가능해야 한다.

예:

~~~text
How does scheduler policy X affect latency and fairness
under different workloads in a small RISC-V operating system?
~~~

다음 요소가 명확해야 한다.

- independent variable
- dependent variable
- workload
- baseline
- experimental condition

---

## Benchmark

측정 후보:

- execution time
- syscall latency
- context-switch count
- context-switch latency
- page fault count
- page allocation count
- memory overhead
- throughput
- scheduler fairness

workload 후보:

- CPU-bound
- memory-bound
- syscall-heavy
- mixed

---

## Experiment

각 실험은 재현 가능해야 한다.

필요한 요소:

- 동일한 QEMU configuration
- 동일한 kernel configuration
- warm-up 여부
- 반복 횟수
- random factor 통제
- raw measurement 저장

결과는 가능하면 CSV 형태로 저장한다.

---

## Analysis

결과 분석에서는 단순히 "빠르다/느리다"로 끝내지 않는다.

확인할 내용:

- 평균
- 분산
- workload별 차이
- trade-off
- unexpected result
- implementation overhead
- experiment limitation

좋지 않은 결과도 연구 결과다.

중요한 것은 왜 그런 결과가 발생했는지 설명하는 것이다.

---

# 5. Final Technical Report

최종 영어 technical report 구조:

~~~text
Title

Abstract

1. Introduction
2. Background
3. Research Question
4. System Design
5. Implementation
6. Experimental Setup
7. Results
8. Analysis
9. Limitations
10. Related Work
11. Conclusion
~~~

Mini-RVOS의 최종 목표는 단순히 작은 OS를 구현하는 것이 아니라,

> 직접 구현한 RISC-V operating system을 이해하고,
> 그 시스템을 이용해 하나의 검증 가능한 연구 질문에 답하는 것

이다.
