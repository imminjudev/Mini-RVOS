# Mini-RVOS 1차 연구 최종보고서

## RISC-V Sv39 환경에서 ASID 기반 주소 공간 전환이 선점형 스케줄링 오버헤드에 미치는 영향

**프로젝트:** Mini-RVOS
**연구 단계:** 1차 연구 최종보고서
**플랫폼:** RISC-V RV64 / Sv39 / QEMU virt
**최종 정리:** 2026-08-26

---

## 초록

본 연구는 직접 구현한 RISC-V 운영체제 Mini-RVOS에서
ASID(Address Space Identifier)를 이용한 Sv39 주소 공간 전환이
선점형 프로세스 스케줄링 비용에 어떤 영향을 주는지 측정했다.

Mini-RVOS의 기존 주소 공간 전환 방식인 `FULL_FLUSH`는
모든 프로세스가 ASID 0을 사용하고, 측정 대상 주소 공간 활성화
과정에서 global `SFENCE.VMA`를 실행한다.

비교 대상으로 각 프로세스에 고유한 ASID를 할당하고
실험 중 page table을 변경하지 않으며 ASID를 재사용하지 않는 조건에서
프로세스 전환마다 global `SFENCE.VMA`를 수행하지 않는
`ASID` 방식을 구현했다.

실험은 두 개의 user process가 memory workload를 실행하는 환경에서
주소 공간 전환 정책, timer quantum, memory working-set size를
독립변수로 설정했다.

실험 조건은 다음과 같다.

~~~text
2 switching policies
x
3 timer quanta
x
4 working-set sizes
x
20 repetitions
=
480 runs
~~~

최종 실험에서 ASID 방식은 100회의 측정된 context switch 동안
FULL_FLUSH에서 발생한 200회의 global `SFENCE.VMA`를
측정된 switch path에서 제거했다.

또한 12개의 동일 quantum/working-set 비교 조건 모두에서
평균 address-space switch latency가 감소했다.

감소폭은:

~~~text
14.24% ~ 46.09%
~~~

였으며, 12개 조건의 비가중 평균 감소율은 약:

~~~text
29.68%
~~~

이었다.

그러나 end-to-end user throughput 변화는 일관되지 않았다.
12개 조건에서 계산한 ASID throughput change의
bootstrap 95% confidence interval이 모두 0을 포함했다.

따라서 본 연구에서 확인된 핵심 결과는 다음과 같다.

~~~text
ASID는 Mini-RVOS의 측정된 주소 공간 전환 경로 비용을
일관되게 감소시켰다.

그러나 해당 감소가 실험한 QEMU 환경에서
측정 가능한 end-to-end throughput 향상으로 이어졌다고
확인할 수는 없었다.
~~~

---

# 1. 연구 배경

운영체제가 여러 프로세스를 실행할 때 각 프로세스는
서로 다른 virtual address space를 사용할 수 있다.

RISC-V Sv39 환경에서는 page table root와 ASID 등의 정보가
`satp` CSR에 저장된다.

프로세스가 변경되면서 address space가 바뀌는 경우,
운영체제는 다음과 같은 문제를 처리해야 한다.

~~~text
현재 address space
        |
        v
scheduler context switch
        |
        v
next page table root
        |
        v
satp update
        |
        v
address translation state handling
~~~

ASID를 사용하지 않거나 모든 address space가 동일한 ASID를 사용하는 경우
이전 주소 변환 정보가 새로운 address space와 혼동되지 않도록
적절한 `SFENCE.VMA`가 필요하다.

ASID는 address translation entry를 특정 address space와 구분할 수 있도록
identifier를 제공한다.

Mini-RVOS는 이 구조를 직접 구현하고 실험할 수 있는 작은 운영체제이므로,
ASID가 실제 scheduler와 address-space switching 경로에서
어떤 비용 차이를 만드는지 통제된 조건에서 측정할 수 있다.

---

# 2. 연구 목적

본 연구의 목적은 Mini-RVOS에 ASID 기반 address-space switching을 구현하고,
기존 FULL_FLUSH 방식과 성능 특성을 비교하는 것이다.

연구 질문은 다음과 같다.

~~~text
How does ASID-based Sv39 address-space switching affect
preemptive scheduling overhead in Mini-RVOS across different
timer quanta and memory working-set sizes?
~~~

한국어로 표현하면 다음과 같다.

> ASID 기반 Sv39 주소 공간 전환은 서로 다른 timer quantum과
> memory working-set size에서 Mini-RVOS의 선점형 스케줄링
> 오버헤드에 어떤 영향을 주는가?

측정 대상은 다음과 같다.

~~~text
global SFENCE.VMA count
address-space switch latency
benchmark elapsed time
completed user work
user-work throughput
per-process work distribution
~~~

본 연구는 hardware TLB miss count를 직접 측정하지 않는다.

---

# 3. 연구 가설

실험 전 다음 가설을 설정했다.

## 3.1 H1 — ASID와 global fence

ASID 방식은 FULL_FLUSH 방식보다
context switch 과정에서 수행되는 global `SFENCE.VMA` 횟수를
줄일 것으로 예상했다.

## 3.2 H2 — Timer quantum

timer quantum이 짧을수록 프로세스 전환 간 user execution 시간이 짧아지므로
주소 공간 전환 비용이 전체 실행시간에서 차지하는 비중이 증가할 것으로 예상했다.

초기 연구 계획에서는 짧은 quantum이 switch count 자체를 증가시키는 구조도
고려했으나, 최종 benchmark protocol에서는 모든 run을
정확히 100회의 measured context switch에서 종료하도록 고정했다.

따라서 최종 실험에서 비교하는 값은 switch count가 아니라
switch frequency와 전체 실행시간 중 switching cost의 비중이다.

## 3.3 H3 — Working-set size

memory working set이 커지면 address translation과 관련된 상태의 영향이
더 크게 나타날 가능성이 있다고 예상했다.

## 3.4 H4 — Workload type

CPU-heavy workload에서는 memory-heavy workload보다
FULL_FLUSH와 ASID의 성능 차이가 작을 가능성이 있다고 예상했다.

최종 replicated experiment는 memory workload만 포함하므로
H4는 최종 실험으로 검증하지 않았다.

---

# 4. Mini-RVOS 연구 환경

Mini-RVOS는 RV64 기반의 작은 운영체제로,
본 연구 시점에는 다음 기능을 포함한다.

~~~text
OpenSBI boot
physical memory allocator
Sv39 virtual memory
trap handling
U-mode execution
system calls
per-process page tables
round-robin scheduling
timer preemption
in-memory filesystem
interactive shell
~~~

연구 benchmark는 interactive shell과 분리된
`BENCHMARK_MODE`에서 실행된다.

최종 실험 환경은 다음과 같다.

| 항목 | 값 |
| --- | --- |
| Architecture | RISC-V RV64 |
| Virtual memory | Sv39 |
| Machine | QEMU `virt` |
| Guest RAM | 128 MiB |
| Timer frequency | 10 MHz |
| Processes | 2 |
| Context switches per run | 100 |
| Compiler | riscv64-unknown-elf-gcc 14.2.0+19 |
| QEMU | 10.2.1 |

최종 dataset을 생성한 Mini-RVOS commit은:

~~~text
63d1fbb42a371c612d8726bf2e3440b78a1363cb
~~~

이다.

실험 당시 metadata에는:

~~~text
working_tree_dirty=no
~~~

가 기록되어 있다.

---

# 5. 비교한 주소 공간 전환 방식

## 5.1 FULL_FLUSH

FULL_FLUSH는 baseline implementation이다.

모든 process는:

~~~text
ASID = 0
~~~

을 사용한다.

측정된 address-space activation path는 `satp`를 변경하는 과정에서
두 번의 global `SFENCE.VMA`를 실행한다.

따라서 100회의 measured context switch가 발생하면:

~~~text
sfence_count = 200
~~~

이 기록된다.

이 값은 RISC-V 전체에 대한 일반적인 요구 횟수를 의미하지 않는다.
Mini-RVOS에서 정의한 FULL_FLUSH baseline implementation의 특성이다.

## 5.2 ASID

ASID experimental mode에서는:

~~~text
Process 1
ASID = 1

Process 2
ASID = 2
~~~

를 사용한다.

`satp`에는 다음 정보가 포함된다.

~~~text
MODE = Sv39
ASID
root page-table PPN
~~~

측정 구간 동안 다음 조건을 유지한다.

- 각 process의 ASID는 서로 다르다.
- ASID를 재사용하지 않는다.
- benchmark 시작 후 process page table을 변경하지 않는다.
- 필요한 mapping은 측정 전에 생성한다.

이 조건에서 process switch마다 global `SFENCE.VMA`를 수행하지 않는
address-space activation path를 구현했다.

실험 전에 구현된 ASID bit 수를 확인하는 과정도 수행하며,
해당 초기화 작업은 측정된 switching sequence에 포함하지 않는다.

---

# 6. Benchmark 구현

## 6.1 두 개의 독립된 address space

두 user process는 서로 다른 root page table을 가진다.

각 process는 private physical memory를 사용하고,
동일한 benchmark virtual address 범위를 자신의 private page에 매핑한다.

실험은 두 process의 address space와 private memory가
실제로 분리되어 있는지 검증한다.

## 6.2 Memory workload

최종 실험에서는 memory workload를 사용했다.

benchmark process는 working set의 모든 page를 순회하면서
각 page의 64-bit 위치에 대해 read-modify-write를 수행한다.

한 번의 전체 working-set 순회를:

~~~text
1 work unit
~~~

으로 정의한다.

Working set은 다음 네 종류이다.

| Pages | Size |
| ---: | ---: |
| 1 | 4 KiB |
| 8 | 32 KiB |
| 32 | 128 KiB |
| 128 | 512 KiB |

128-page workload의 한 work unit은
1-page workload의 한 work unit보다 훨씬 많은 memory operation을 포함한다.

따라서 서로 다른 working-set size 사이의
절대 work-unit throughput은 직접 비교하지 않는다.

정책 효과는 항상 동일한:

~~~text
timer quantum
working-set size
~~~

조건에서 FULL_FLUSH와 ASID를 비교한다.

## 6.3 Work counter

각 process의 완료된 work unit 수는 saved register `s11`에 저장한다.

Mini-RVOS trap frame이 `s11`을 보존하므로
timer preemption 이후에도 progress counter가 유지된다.

이를 통해 benchmark progress 측정을 위해
추가 syscall이나 shared progress page를 사용하지 않았다.

---

# 7. 실험 변수

## 7.1 Switching policy

~~~text
FULL_FLUSH
ASID
~~~

## 7.2 Timer quantum

| Ticks | Approximate interval |
| ---: | ---: |
| 10,000 | 1 ms |
| 100,000 | 10 ms |
| 1,000,000 | 100 ms |

## 7.3 Memory working set

~~~text
1 page
8 pages
32 pages
128 pages
~~~

---

# 8. 실험 행렬

전체 실험 조건은:

~~~text
2 policies
x
3 quanta
x
4 working sets
=
24 conditions
~~~

이다.

각 condition을 20회 반복했다.

~~~text
24 conditions
x
20 repetitions
=
480 final runs
~~~

각 run은:

~~~text
100 measured context switches
~~~

에서 종료한다.

실행 순서는 fixed random seed:

~~~text
20260825
~~~

를 이용해 섞었다.

이를 통해 특정 policy나 working-set configuration이
항상 실험 초반이나 후반에 실행되는 현상을 피했다.

---

# 9. 측정 지표

각 run에서 다음 값을 기록한다.

~~~text
elapsed_ticks
context_switches
sfence_count
address_space_switch_ticks_total
address_space_switch_ticks_max
p1_work_units
p2_work_units
total_work_units
~~~

## 9.1 Throughput

User-work throughput은 다음과 같이 계산한다.

~~~text
throughput =
    total_work_units
    x timer_frequency
    / elapsed_ticks
~~~

## 9.2 Switch cost

평균 address-space switching cost는:

~~~text
address_space_switch_ticks_total
/
context_switches
~~~

로 계산한다.

## 9.3 Switching time share

전체 benchmark 시간에서 측정된 address-space switching이 차지하는 비율은:

~~~text
address_space_switch_ticks_total
/
elapsed_ticks
x 100
~~~

으로 계산한다.

---

# 10. 실험 자동화 및 재현성

실험은 Python 기반 runner로 자동화했다.

~~~text
scripts/run_experiments.py
~~~

runner는 각 experimental condition을 build하고,
QEMU를 실행하고,
benchmark CSV를 추출하고,
결과의 기본 invariant를 검증한다.

통계 분석은:

~~~text
scripts/analyze_experiment.py
~~~

에서 수행한다.

그래프 생성은:

~~~text
scripts/plot_experiment.py
~~~

에서 수행한다.

실험 metadata에는 다음 항목을 저장한다.

~~~text
Git commit
Git branch
working-tree state
QEMU version
compiler version
timer frequency
timer quanta
working-set sizes
run count
random seed
~~~

raw experiment result는 Git repository에서 생성 결과로 취급하며
`research-results/` 아래에 저장한다.

---

# 11. 실험 중 발견된 timer race

최초 480-run 실험은 중간에 실패했다.

실패 시 다음 trap이 관찰됐다.

~~~text
scause = 0x000000000000000c
sepc   = 0x00000000000b2ee4
stval  = 0x00000000000b2ee4
~~~

`scause = 12`는 instruction page fault를 의미한다.

문제를 추적한 결과 benchmark setup 중
S-mode global interrupt가 활성화된 상태에서 supervisor timer가 발생할 수 있었다.

scheduler는 timer interrupt가 user process에서 발생했다고 가정하고 있었기 때문에,
S-mode에서 생성된 trap frame을 현재 process frame으로 저장할 가능성이 있었다.

이 경우 이후 scheduler가 해당 frame을 user context로 복원하면서
잘못된 `sepc`를 사용하게 된다.

수정 내용은 다음과 같다.

- benchmark S-mode setup 중 global SIE를 비활성 상태로 유지
- supervisor timer interrupt enable bit는 유지
- 최초 user address space를 활성화한 뒤 첫 timer를 arm
- timer trap frame의 `SSTATUS.SPP`를 검사
- S-mode에서 발생한 timer frame을 user process frame으로 저장하지 않음

RISC-V privilege 동작상 SIE가 0이더라도
낮은 privilege level인 U-mode 실행 중에는
enabled supervisor interrupt가 전달될 수 있으므로
user process preemption은 정상적으로 유지된다.

수정 후 다음 stress test를 수행했다.

~~~text
quantum = 10,000 ticks
working set = 8 pages
switches = 100

FULL_FLUSH = 50 runs
ASID       = 50 runs

total = 100 runs
~~~

100개 run 모두 정상 완료됐다.

오류 수정 전에 생성된 partial dataset은:

~~~text
INVALID
~~~

로 처리하고 최종 분석에서 제외했다.

이 과정을 통해 benchmark correctness 자체도
연구 결과의 일부로 검증했다.

---

# 12. 최종 dataset 검증

수정된 benchmark로 전체 matrix를 다시 실행했다.

최종 dataset은:

~~~text
480 rows
24 conditions
20 runs per condition
~~~

을 포함한다.

구성은:

~~~text
FULL_FLUSH = 240 runs
ASID       = 240 runs
~~~

이다.

모든 row에서:

~~~text
context_switches = 100
~~~

이 확인됐다.

Fence invariant는:

~~~text
FULL_FLUSH
sfence_count = 200

ASID
sfence_count = 0
~~~

으로 유지됐다.

또한:

~~~text
p1_work_units > 0
p2_work_units > 0

total_work_units =
p1_work_units + p2_work_units
~~~

가 모든 run에서 성립했다.

최종 experiment log에서 `[FAIL]` marker는 발견되지 않았다.

---

# 13. 통계 분석

각 condition별로 다음 통계를 계산했다.

- mean
- median
- sample standard deviation
- minimum
- maximum
- coefficient of variation

주요 정책 효과는 동일한 quantum과 working set에서:

~~~text
FULL_FLUSH
vs
ASID
~~~

를 비교했다.

ASID throughput change는:

~~~text
(
    ASID mean throughput
    /
    FULL_FLUSH mean throughput
    - 1
)
x 100
~~~

으로 정의했다.

Throughput 변화의 uncertainty를 확인하기 위해
10,000회 independent bootstrap resampling을 수행했다.

Bootstrap seed는:

~~~text
20260825
~~~

이다.

최종 결과 해석에서는 bootstrap 95% interval을
효과의 불확실성을 표현하는 값으로 사용한다.

---

# 14. 결과 1 — Global SFENCE.VMA

FULL_FLUSH에서는 모든 final run에서:

~~~text
100 measured context switches
200 measured global SFENCE.VMA
~~~

가 기록됐다.

ASID에서는:

~~~text
100 measured context switches
0 measured per-switch global SFENCE.VMA
~~~

가 기록됐다.

따라서 H1은 실험한 Mini-RVOS implementation에서 지지됐다.

ASID 방식은 측정된 process-switch path에서
FULL_FLUSH baseline이 수행하던 global fence를 제거했다.

이 결과는 hardware TLB miss 감소를 의미하지 않는다.

본 실험은 TLB miss counter를 측정하지 않았다.

---

# 15. 결과 2 — Address-Space Switch Cost

12개의 동일 quantum/working-set comparison 모두에서
ASID의 평균 measured switch cost가 FULL_FLUSH보다 낮았다.

전체 감소폭은:

~~~text
14.24% ~ 46.09%
~~~

였다.

12개 experimental cell의 비가중 평균 감소폭은 약:

~~~text
29.68%
~~~

였다.

![Measured address-space switch cost](assets/asid-research/02-switch-cost.svg)

Shortest quantum인 10,000 ticks에서 결과는 다음과 같다.

| Working set | ASID switch-cost reduction |
| ---: | ---: |
| 1 page | 34.01% |
| 8 pages | 37.65% |
| 32 pages | 42.38% |
| 128 pages | 46.09% |

이 quantum에서는 working set 증가와 함께
switch-cost reduction도 증가했다.

그러나 이 패턴은 100,000 ticks와 1,000,000 ticks에서는 반복되지 않았다.

따라서 working-set size가 커질수록
ASID의 switch-cost benefit이 항상 커진다고 결론내릴 수는 없다.

---

# 16. 결과 3 — End-to-End Throughput

ASID의 throughput change는 조건에 따라
양수와 음수가 모두 관찰됐다.

| Quantum | WS | ASID throughput change | Bootstrap 95% CI |
| ---: | ---: | ---: | ---: |
| 10,000 | 1 | -0.985% | -2.376% ~ +0.405% |
| 10,000 | 8 | -0.887% | -2.428% ~ +0.795% |
| 10,000 | 32 | +1.379% | -0.829% ~ +4.014% |
| 10,000 | 128 | +0.193% | -3.483% ~ +3.738% |
| 100,000 | 1 | -0.767% | -1.862% ~ +0.251% |
| 100,000 | 8 | +0.572% | -0.728% ~ +1.935% |
| 100,000 | 32 | -3.014% | -6.326% ~ +0.599% |
| 100,000 | 128 | -0.043% | -1.052% ~ +0.999% |
| 1,000,000 | 1 | +0.238% | -0.291% ~ +0.795% |
| 1,000,000 | 8 | +0.372% | -0.536% ~ +1.348% |
| 1,000,000 | 32 | -0.450% | -2.562% ~ +1.064% |
| 1,000,000 | 128 | +1.548% | -0.006% ~ +3.557% |

![ASID throughput change](assets/asid-research/01-throughput-change.svg)

12개 bootstrap 95% interval 모두 0을 포함한다.

따라서 현재 dataset은:

~~~text
ASID가 end-to-end user throughput을 향상시켰다
~~~

는 결론을 확립하지 못한다.

Point estimate의 범위는:

~~~text
-3.014% ~ +1.548%
~~~

이며 조건에 따라 방향도 달라졌다.

---

# 17. 결과 4 — Switching Time Share

주소 공간 전환 비용이 전체 benchmark 시간에서 차지하는 비율은
짧은 timer quantum에서 더 컸다.

10,000-tick quantum:

~~~text
FULL_FLUSH
약 0.74% ~ 0.85%

ASID
약 0.46% ~ 0.48%
~~~

100,000-tick quantum:

~~~text
FULL_FLUSH
약 0.15% ~ 0.16%

ASID
약 0.11% ~ 0.13%
~~~

1,000,000-tick quantum:

~~~text
FULL_FLUSH
약 0.0168% ~ 0.0198%

ASID
약 0.0127% ~ 0.0146%
~~~

![Address-space switching share](assets/asid-research/03-switch-time-share.svg)

이 결과는 direct switch cost 감소와
end-to-end throughput 결과 사이의 차이를 이해하는 데 중요하다.

10,000-tick 조건에서도 측정된 address-space switch path는
전체 benchmark 시간의 1% 미만이다.

1,000,000-tick 조건에서는 약 0.02% 이하이다.

따라서 switch path 내부 비용이 큰 비율로 줄어도
전체 workload throughput에서는 작은 차이로 나타날 수 있다.

---

# 18. 가설 평가

## H1 — ASID는 global fence 수를 줄인다

**지지됨.**

~~~text
FULL_FLUSH = 200 measured fences / 100 switches
ASID       = 0 measured per-switch fences / 100 switches
~~~

## H2 — 짧은 quantum에서 switching overhead의 중요도가 커진다

**부분적으로 지지됨.**

최종 protocol에서는 switch count가 100으로 고정되어 있다.

그러나 짧은 quantum에서는 같은 100회 switch가
더 짧은 전체 실행시간 안에서 발생하므로
switching time share가 크게 증가했다.

Throughput improvement는 확인되지 않았다.

## H3 — Working set이 커질수록 ASID benefit이 커진다

**일반적인 결과로는 지지되지 않음.**

10,000-tick quantum에서는 switch-cost reduction이
working set 증가와 함께 커졌다.

다른 quantum에서는 동일한 패턴이 반복되지 않았으며,
throughput에서도 일관된 관계가 나타나지 않았다.

## H4 — CPU-heavy workload에서는 효과가 작다

**최종 실험에서 검증하지 않음.**

최종 480-run matrix는 memory workload만 포함한다.

따라서 workload type 간 성능 차이에 대한 결론은 내리지 않는다.

---

# 19. 결과 해석

본 연구에서 가장 중요한 점은
direct mechanism measurement와 end-to-end measurement가
서로 다른 결과를 보였다는 것이다.

ASID implementation은:

~~~text
global fence count
address-space switching latency
~~~

에서 명확한 차이를 만들었다.

그러나 전체 benchmark throughput에서는
그 차이가 명확하게 분리되지 않았다.

이는 모순된 결과가 아니다.

측정된 address-space switching path가
전체 실행시간에서 차지하는 비율이 작기 때문이다.

예를 들어 특정 component가 전체 실행시간의 1% 미만이라면,
해당 component 내부 비용을 30% 줄여도
전체 실행시간에서 기대되는 차이는 훨씬 작다.

실제 experiment에서도 shorter quantum에서
switching-time share가 커졌지만,
throughput variation보다 충분히 큰 효과가 나타나지는 않았다.

---

# 20. 연구의 한계

## 20.1 QEMU 환경

실험은 QEMU `virt`에서 수행했다.

QEMU의 instruction execution, address translation,
timer delivery 및 `SFENCE.VMA` timing은
실제 RISC-V processor와 동일하지 않을 수 있다.

따라서 결과의 범위는:

~~~text
tested Mini-RVOS / QEMU environment
~~~

로 제한한다.

## 20.2 Hardware TLB counter 부재

실험은 hardware TLB miss count를 직접 측정하지 않는다.

따라서 ASID 사용으로 TLB miss가 얼마나 감소했는지에 대한
정량적 주장은 하지 않는다.

## 20.3 Host scheduling noise

QEMU는 host operating system 위에서 실행된다.

Host scheduling과 background workload가
guest execution timing에 영향을 줄 수 있다.

실험 순서 randomization과 condition별 20회 반복으로
이를 완화했지만 완전히 제거할 수는 없다.

## 20.4 두 개의 process

현재 benchmark scheduler는 두 process를 사용한다.

더 많은 address space가 활성화되는 환경의 특성은
본 연구 범위에 포함되지 않는다.

## 20.5 ASID reuse 없음

두 process는 고정 ASID 1과 2를 사용한다.

ASID exhaustion과 reuse는 발생하지 않는다.

따라서 ASID recycle 시 필요한 invalidation cost는 측정하지 않았다.

## 20.6 Immutable page tables

측정 중 process page table을 수정하지 않는다.

Dynamic mapping 변경이 존재하는 실제 운영체제에서는
적절한 address-translation fence 처리가 추가로 필요하다.

## 20.7 Baseline implementation

FULL_FLUSH는 Mini-RVOS에서 정의한 특정 baseline이며
주소 공간 전환마다 두 global fence를 측정한다.

다른 operating system이나 다른 fence placement를 사용하는 구현에서는
정량적 차이가 달라질 수 있다.

## 20.8 반복 횟수

각 experimental cell의 반복 횟수는 20회이다.

Throughput bootstrap interval이 모든 조건에서 0을 포함했으므로,
더 많은 반복 실험을 수행하면 uncertainty가 줄어들 가능성이 있다.

현재 연구는 관찰된 불확실성을 그대로 결과로 보고한다.

---

# 21. 연구에서 구현한 시스템

1차 연구를 통해 Mini-RVOS에는 다음 infrastructure가 추가됐다.

## 21.1 Benchmark boot mode

interactive shell과 분리된 연구용 boot path를 구현했다.

## 21.2 Research instrumentation

다음을 측정하는 instrumentation을 추가했다.

~~~text
context switches
sfence count
address-space switch total ticks
address-space switch maximum ticks
completed process work
elapsed ticks
~~~

## 21.3 ASID switching

Sv39 `satp`에 process별 ASID를 적용하는 path를 구현했다.

## 21.4 Configurable benchmark

다음 변수를 build configuration으로 변경할 수 있다.

~~~text
switch policy
timer quantum
working-set size
switch count
~~~

## 21.5 Automated experiment runner

QEMU build/run/data extraction을 자동화했다.

## 21.6 Dataset validation

실험 결과가 configuration 및 counter invariant와 일치하는지
자동 검증한다.

## 21.7 Statistical analysis

Condition summary와 policy effect를 자동 생성한다.

## 21.8 Visualization

연구 결과를 재생성 가능한 SVG/PNG figure로 출력한다.

---

# 22. 1차 연구의 성과

본 연구의 성과는 특정 성능 수치 하나에 한정되지 않는다.

첫 번째 성과는 Mini-RVOS에
ASID-based Sv39 address-space switching을 직접 구현한 것이다.

두 번째 성과는 운영체제 내부 mechanism을 측정할 수 있는
benchmark instrumentation과 experiment pipeline을 구축한 것이다.

세 번째 성과는 실험 과정에서 supervisor timer race를 발견하고,
privilege-level trap provenance를 검사하도록 scheduler를 수정하면서
benchmark correctness를 개선한 것이다.

네 번째 성과는 480-run controlled experiment를 수행하고
raw measurement, 통계 분석, uncertainty, figure를
하나의 재현 가능한 pipeline으로 연결한 것이다.

다섯 번째 성과는 예상한 방향과 다른 결과도 유지한 것이다.

ASID의 direct switching cost reduction은 명확했지만
end-to-end throughput improvement는 확인되지 않았다.

이 결과를 수정하거나 유리한 조건만 선택하지 않고
측정 결과와 한계를 그대로 최종 결론에 반영했다.

---

# 23. 최종 결론

Mini-RVOS의 첫 번째 연구 질문은 다음과 같았다.

> ASID 기반 Sv39 주소 공간 전환은 서로 다른 timer quantum과
> memory working-set size에서 선점형 scheduling overhead에
> 어떤 영향을 주는가?

480-run memory benchmark 결과,
ASID 방식은 FULL_FLUSH baseline과 비교했을 때
측정된 global `SFENCE.VMA`를 제거했고,
12개 experimental cell 모두에서
measured address-space switch latency를 감소시켰다.

감소폭은:

~~~text
14.24% ~ 46.09%
~~~

였고 비가중 평균은 약:

~~~text
29.68%
~~~

이었다.

반면 end-to-end user throughput 결과에서는
모든 bootstrap 95% confidence interval이 0을 포함했다.

따라서 1차 연구의 최종 결론은 다음과 같다.

~~~text
ASID-based switching은 Mini-RVOS의 측정된
address-space switching path 비용을 줄였다.

그러나 실험한 two-process memory workload와
QEMU 환경에서는 이 감소가 명확한
end-to-end throughput improvement로 이어졌다고
확인할 수 없었다.
~~~

짧은 timer quantum에서는 address-space switching이
전체 실행시간에서 차지하는 비중이 증가했지만,
tested range에서도 1% 미만이었다.

Working-set size에 따른 ASID 효과는
일관된 관계를 보이지 않았다.

본 연구는 ASID를 적용하면 시스템 전체 성능이 항상 개선된다는 결론보다,
운영체제 optimization의 직접 비용과 전체 workload 성능을
분리해서 측정해야 한다는 결과를 보여준다.

---

# 24. 후속 연구 가능성

1차 연구에서 제외된 항목들은 다음 연구 문제로 확장할 수 있다.

~~~text
larger process counts
ASID allocation
finite ASID pools
ASID reuse
ASID-specific SFENCE.VMA
dynamic page-table modification
CPU-heavy workloads
syscall-heavy workloads
physical RISC-V hardware
hardware translation-event counters
~~~

이 항목들은 1차 연구 결과에 포함하지 않는다.

1차 연구는 현재의 two-process, fixed-ASID, immutable-page-table
experiment를 하나의 완결된 연구 단위로 종료한다.

---

# 25. 재현 명령

기존 Mini-RVOS regression:

~~~bash
make test
~~~

전체 research experiment:

~~~bash
make research-experiment
~~~

Dataset 분석:

~~~bash
make research-analyze DATASET=research-results/<dataset>
~~~

Figure 생성:

~~~bash
make research-plot DATASET=research-results/<dataset>
~~~

영문 technical report:

~~~text
docs/08-asid-research-report.md
~~~

1차 연구 한국어 최종보고서:

~~~text
docs/09-asid-research-final-report-ko.md
~~~

---

# 26. 1차 연구 종료 상태

~~~text
Mini-RVOS v1.0 implementation
        complete

ASID implementation
        complete

benchmark infrastructure
        complete

final replicated experiment
        480 / 480 runs complete

dataset validation
        complete

statistical analysis
        complete

figures
        complete

English technical report
        complete

Korean final report
        complete

research branch
        merged into main

1st research cycle
        COMPLETE
~~~
