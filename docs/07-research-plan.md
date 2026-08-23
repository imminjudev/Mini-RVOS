# Mini-RVOS Research Plan

## 1. Research Topic

Mini-RVOS의 첫 연구 주제는 RISC-V Sv39 address-space switching과
ASID(Address Space Identifier)의 성능 영향을 측정하는 것이다.

현재 Mini-RVOS는 process를 전환할 때:

~~~text
scheduler
    |
    v
process_activate()
    |
    v
vm_enable()
    |
    +--> satp 변경
    |
    +--> sfence.vma
~~~

경로를 사용한다.

현재 `satp`에는 ASID를 사용하지 않는다.

각 address-space switch에서 전체 `sfence.vma`를 수행한다.

연구에서는 현재 구현을 baseline으로 유지하고,
process별 ASID를 사용하는 experimental implementation을 추가한다.

---

# 2. Research Question

연구 질문:

~~~text
How does ASID-based Sv39 address-space switching affect
preemptive scheduling overhead in Mini-RVOS across different
timer quanta and memory working-set sizes?
~~~

측정 대상은 다음 세 요소의 관계다.

~~~text
ASID policy
    |
    +--> full TLB fence
    |
    +--> ASID-based switching

timer quantum

memory working-set size
~~~

그리고 이 조건들이 다음 값에 어떤 영향을 주는지 측정한다.

~~~text
execution time
throughput
context-switch count
address-space switch cost
sfence.vma count
per-process completed work
~~~

---

# 3. Current Baseline

현재 Mini-RVOS의 address-space activation은:

~~~text
process_activate(process)
        |
        v
current_process = process
        |
        v
vm_enable(process->pagetable)
~~~

이다.

현재 `vm_enable()`은 Sv39 root page table을 `satp`에 설정하고
`sfence.vma`를 실행한다.

현재 `satp` 값에는:

~~~text
MODE = Sv39
ASID = 0
PPN  = root page table
~~~

구조가 사용된다.

따라서 모든 process가 ASID 0을 사용한다.

이 구현을 연구의 baseline으로 사용한다.

이 실험 조건의 이름은:

~~~text
FULL_FLUSH
~~~

로 한다.

---

# 4. Experimental ASID Mode

두 번째 구현에서는 각 process에 서로 다른 ASID를 할당한다.

예:

~~~text
Process 1
    ASID = 1

Process 2
    ASID = 2
~~~

`satp`는 다음 정보를 포함한다.

~~~text
MODE
ASID
PPN
~~~

process switch에서는:

~~~text
Process 1
ASID 1
   |
   v
Process 2
ASID 2
~~~

형태로 address space를 전환한다.

page table mapping이 변경되지 않았고
ASID가 재사용되지 않는 일반 context switch에서는
global `sfence.vma`를 실행하지 않는 experimental path를 만든다.

이 실험 조건의 이름은:

~~~text
ASID
~~~

로 한다.

ASID 재사용이나 page-table modification이 필요한 경우에는
RISC-V memory-management fence 규칙에 맞춰 별도 fence가 필요하다.

이번 실험에서는 두 process에 고정된 서로 다른 ASID를 사용한다.

---

# 5. Hypotheses

## H1

ASID mode는 FULL_FLUSH mode보다
context switch 중 실행되는 global `sfence.vma` 횟수가 적을 것이다.

## H2

timer quantum이 짧아질수록
context switch 횟수가 증가할 것이다.

따라서 FULL_FLUSH와 ASID 사이의 성능 차이도
짧은 quantum에서 커질 것으로 예상한다.

## H3

memory working set이 커질수록
ASID를 통해 유지되는 address-translation state의 효과가
더 크게 나타날 가능성이 있다.

## H4

memory access가 적은 CPU-heavy workload에서는
FULL_FLUSH와 ASID 사이의 차이가
memory-heavy workload보다 작을 것으로 예상한다.

이 항목들은 실험 전에 세우는 hypothesis이며
결과가 hypothesis와 일치해야 하는 것은 아니다.

---

# 6. Independent Variables

연구자가 변경하는 변수는 세 가지다.

## 6.1 Address-Space Switching Mode

~~~text
FULL_FLUSH
ASID
~~~

## 6.2 Timer Quantum

QEMU `virt`의 현재 timer timebase를 기준으로
다음 quantum을 우선 사용한다.

~~~text
10,000 ticks
100,000 ticks
1,000,000 ticks
~~~

현재 10 MHz timebase 기준으로 해석하면:

~~~text
10,000 ticks
    approximately 1 ms

100,000 ticks
    approximately 10 ms

1,000,000 ticks
    approximately 100 ms
~~~

실험 데이터에는 변환된 milliseconds보다
원래 timer tick 값을 함께 저장한다.

## 6.3 Memory Working Set

process가 반복적으로 접근하는 private user memory의 크기를 변경한다.

초기 후보:

~~~text
1 page
8 pages
32 pages
128 pages
~~~

4 KiB page 기준:

~~~text
1 page
    4 KiB

8 pages
    32 KiB

32 pages
    128 KiB

128 pages
    512 KiB
~~~

---

# 7. Dependent Variables

실험에서 측정할 값:

~~~text
elapsed_ticks

context_switch_count

sfence_count

address_space_switch_ticks

process_1_iterations

process_2_iterations

total_iterations
~~~

추가로 계산할 값:

~~~text
throughput

average address-space switch cost

work distribution between processes
~~~

---

# 8. Workloads

세 종류의 workload를 사용한다.

## 8.1 CPU Workload

register arithmetic과 integer operation을 반복한다.

memory access를 최소화한다.

목적:

~~~text
scheduler/context-switch overhead 중심 측정
~~~

## 8.2 Memory Workload

private user memory 영역의 여러 page를 반복적으로 접근한다.

working-set 크기는:

~~~text
1
8
32
128 pages
~~~

로 변경한다.

각 page를 반복적으로 읽고 써서
address translation이 지속적으로 발생하도록 한다.

목적:

~~~text
ASID와 address-translation state의 영향 측정
~~~

## 8.3 Syscall Workload

U-mode에서 반복적으로 syscall을 발생시킨다.

UART output은 I/O 비용이 결과를 지배할 수 있으므로
benchmark에서는 출력 syscall을 반복 workload로 사용하지 않는다.

우선 다음 syscall을 사용한다.

~~~text
SYS_GETPID
~~~

목적:

~~~text
U-mode
    |
    v
ecall
    |
    v
trap
    |
    v
syscall
    |
    v
sret
~~~

경로가 많은 workload에서 scheduler configuration의 영향을 확인한다.

---

# 9. Experimental Matrix

기본 matrix:

~~~text
2 switching modes
x
3 timer quanta
x
4 memory working-set sizes
~~~

즉 memory workload 기준:

~~~text
2 x 3 x 4
=
24 experimental conditions
~~~

각 condition은 여러 번 반복 실행한다.

초기 목표:

~~~text
20 runs per condition
~~~

이면:

~~~text
24 x 20
=
480 runs
~~~

이 된다.

CPU workload와 syscall workload는 working-set dimension이 없으므로
별도 matrix로 실행한다.

---

# 10. Experimental Controls

각 run에서 다음 조건을 고정한다.

~~~text
QEMU machine
    virt

RAM
    128 MiB

CPU architecture
    RISC-V 64-bit

virtual memory
    Sv39

process count
    2

compiler
    riscv64-unknown-elf-gcc

compiler flags
    fixed

kernel source
    same experiment commit

work performed
    fixed benchmark implementation
~~~

각 run의 환경 정보도 결과와 함께 기록한다.

---

# 11. Benchmark Mode

현재 interactive shell mode는 유지한다.

연구용 실행은 별도의 benchmark mode로 구성한다.

개념:

~~~text
Mini-RVOS

normal build
    |
    v
interactive shell


benchmark build
    |
    v
two benchmark processes
    |
    v
timer scheduler
    |
    v
automatic measurement
    |
    v
CSV-compatible output
~~~

interactive shell 기능을 benchmark logic과 섞지 않는다.

---

# 12. Instrumentation

kernel에 research instrumentation counter를 추가한다.

예:

~~~text
context_switch_count

sfence_count

address_space_switch_ticks_total

address_space_switch_ticks_max
~~~

시간 측정은 현재 사용할 수 있는:

~~~text
rdtime
~~~

을 사용한다.

address-space switch 구간은 개념적으로:

~~~text
start = rdtime()

process_activate(next)

end = rdtime()

cost = end - start
~~~

형태로 측정한다.

이 값은 전체 context-switch latency와 구분한다.

측정 이름은:

~~~text
address_space_switch_ticks
~~~

로 기록한다.

---

# 13. What Will Not Be Claimed

현재 instrumentation으로 hardware TLB miss를 직접 측정하지 않는다.

따라서 결과에서:

~~~text
TLB miss count decreased
~~~

라고 직접 주장하지 않는다.

측정 가능한 값:

~~~text
sfence count
address-space switch time
overall execution time
throughput
~~~

을 기준으로 분석한다.

ASID와 address translation cache의 관계는
RISC-V architecture specification을 기반으로 설명한다.

---

# 14. Output Format

benchmark 결과는 machine-readable 형식으로 출력한다.

목표 형식:

~~~text
CSV
~~~

예상 column:

~~~text
run
mode
workload
quantum_ticks
working_set_pages
elapsed_ticks
context_switches
sfence_count
address_space_switch_ticks_total
address_space_switch_ticks_max
process1_iterations
process2_iterations
total_iterations
~~~

예:

~~~text
1,FULL_FLUSH,memory,100000,32,...
2,FULL_FLUSH,memory,100000,32,...
3,ASID,memory,100000,32,...
~~~

---

# 15. Reproducibility

각 experiment에는 다음 정보를 저장한다.

~~~text
Git commit hash

QEMU version

compiler version

kernel configuration

benchmark mode

timer quantum

working-set size

run number
~~~

raw result는 수정하지 않고 보관한다.

analysis script는 raw data를 읽어
통계 결과와 graph를 생성한다.

---

# 16. Analysis

각 condition에서 우선 다음 값을 계산한다.

~~~text
mean
median
standard deviation
minimum
maximum
~~~

주요 비교:

~~~text
FULL_FLUSH vs ASID

short quantum vs long quantum

small working set vs large working set

CPU vs memory vs syscall workload
~~~

분석할 관계:

~~~text
quantum decreases
    |
    v
context switches increase
    |
    v
FULL_FLUSH cost changes


working set increases
    |
    v
address translation activity changes
    |
    v
FULL_FLUSH / ASID difference changes
~~~

결과가 hypothesis와 다르면
그 차이 자체를 분석 대상으로 기록한다.

---

# 17. Threats to Validity

## QEMU

QEMU의 address-translation implementation과
실제 RISC-V hardware의 microarchitecture는 다를 수 있다.

따라서 결과는 우선:

~~~text
Mini-RVOS running on the tested QEMU environment
~~~

에 대한 결과로 해석한다.

## Host Scheduling

짧은 timer quantum에서는
host operating system과 QEMU scheduling이 측정값에 영향을 줄 수 있다.

반복 측정과 분산 분석을 통해 영향을 확인한다.

## Timer Resolution

`rdtime` resolution과 QEMU timer behavior가
짧은 operation 측정에 영향을 줄 수 있다.

따라서 개별 switch measurement와 함께
전체 benchmark throughput도 측정한다.

## Small Process Count

현재 scheduler는 두 process만 지원한다.

결과의 범위도:

~~~text
two-process Mini-RVOS workloads
~~~

로 명시한다.

---

# 18. Expected Contribution

이 연구에서 제공할 결과는 다음과 같다.

~~~text
1. Mini-RVOS에 ASID-based address-space switching 구현

2. FULL_FLUSH와 ASID의 controlled comparison

3. scheduler quantum 변화에 따른 switch overhead 측정

4. memory working-set 변화에 따른 성능 차이 측정

5. reproducible benchmark infrastructure

6. raw CSV data와 analysis scripts

7. experimental results에 기반한 technical report
~~~

---

# 19. Implementation Plan

구현은 다음 순서로 진행한다.

~~~text
Step 1
research branch 생성

Step 2
benchmark mode 추가

Step 3
measurement counters 추가

Step 4
FULL_FLUSH baseline 측정 가능 상태 만들기

Step 5
per-process ASID 추가

Step 6
ASID switching mode 추가

Step 7
user benchmark memory region 추가

Step 8
CPU / memory / syscall workloads 추가

Step 9
CSV output 추가

Step 10
automated QEMU benchmark runner 작성

Step 11
repeated experiments

Step 12
raw data 저장

Step 13
statistical analysis

Step 14
graphs 생성

Step 15
technical report 작성
~~~

---

# 20. Freeze Rule

research branch에서는 연구 질문과 직접 관련된 변경만 추가한다.

현재 연구에서 필요한 기능:

~~~text
ASID
benchmark processes
working-set memory
measurement
automation
data collection
~~~

연구 질문과 관련 없는 kernel 기능은 추가하지 않는다.

---

# 21. Final Research Structure

전체 연구 흐름:

~~~text
Mini-RVOS v1.0
       |
       v
FULL_FLUSH baseline
       |
       +---------------------+
       |                     |
       v                     v
timer quantum            working set
variation                variation
       |                     |
       +----------+----------+
                  |
                  v
          baseline results
                  |
                  v
          ASID implementation
                  |
                  v
            same experiments
                  |
                  v
        FULL_FLUSH vs ASID
                  |
                  v
          statistical analysis
                  |
                  v
           technical report
~~~

이 구조를 Mini-RVOS의 첫 research experiment 기준으로 사용한다.
