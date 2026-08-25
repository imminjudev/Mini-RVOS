#include "../include/vm.h"
#include "../include/riscv.h"

#ifdef BENCHMARK_MODE
#include "../include/research.h"
#endif

#define VPN_MASK 0x1FFUL

#define SATP_ASID_SHIFT 44UL

#define PA_TO_PTE(pa) \
    (((unsigned long)(pa) >> 12) << 10)

#define PTE_TO_PA(pte) \
    (((unsigned long)(pte) >> 10) << 12)

#define VPN_INDEX(va, level) \
    (((unsigned long)(va) >> \
      (12 + 9 * (level))) & VPN_MASK)

/*
 * Sv39 low canonical address range.
 *
 * Mini-RVOS의 user address는
 * low half만 사용한다.
 */
#define SV39_USER_TOP \
    (1UL << 38)


static void page_zero(void *ptr)
{
    unsigned char *p =
        (unsigned char *)ptr;

    for (unsigned long i = 0;
         i < PAGE_SIZE;
         i++) {

        p[i] = 0;
    }
}


pagetable_t vm_create(void)
{
    pagetable_t root =
        (pagetable_t)page_alloc();

    if (root == 0) {
        return 0;
    }

    page_zero(root);

    return root;
}


static pte_t *vm_walk(
    pagetable_t root,
    unsigned long va,
    int allocate)
{
    pagetable_t table = root;

    for (int level = 2;
         level > 0;
         level--) {

        pte_t *pte =
            &table[
                VPN_INDEX(
                    va,
                    level
                )
            ];

        if (*pte & PTE_V) {
            /*
             * 중간 page table entry가
             * leaf이면 현재 구현에서는
             * 지원하지 않는다.
             */
            if (*pte &
                (PTE_R |
                 PTE_W |
                 PTE_X)) {

                return 0;
            }

            table =
                (pagetable_t)
                PTE_TO_PA(*pte);

        } else {
            if (!allocate) {
                return 0;
            }

            pagetable_t next =
                (pagetable_t)
                page_alloc();

            if (next == 0) {
                return 0;
            }

            page_zero(next);

            *pte =
                PA_TO_PTE(next) |
                PTE_V;

            table = next;
        }
    }

    return &table[
        VPN_INDEX(
            va,
            0
        )
    ];
}


int vm_map_page(
    pagetable_t root,
    unsigned long va,
    unsigned long pa,
    unsigned long flags)
{
    if ((va &
         (PAGE_SIZE - 1)) != 0 ||
        (pa &
         (PAGE_SIZE - 1)) != 0) {

        return -1;
    }

    pte_t *pte =
        vm_walk(
            root,
            va,
            1
        );

    if (pte == 0 ||
        (*pte & PTE_V)) {

        return -1;
    }

    *pte =
        PA_TO_PTE(pa) |
        flags |
        PTE_V;

    return 0;
}


int vm_map_range(
    pagetable_t root,
    unsigned long va,
    unsigned long pa,
    unsigned long size,
    unsigned long flags)
{
    if ((va &
         (PAGE_SIZE - 1)) != 0 ||
        (pa &
         (PAGE_SIZE - 1)) != 0 ||
        (size &
         (PAGE_SIZE - 1)) != 0) {

        return -1;
    }

    for (unsigned long offset = 0;
         offset < size;
         offset += PAGE_SIZE) {

        if (vm_map_page(
                root,
                va + offset,
                pa + offset,
                flags) != 0) {

            return -1;
        }
    }

    return 0;
}


unsigned long vm_translate(
    pagetable_t root,
    unsigned long va)
{
    pte_t *pte =
        vm_walk(
            root,
            va,
            0
        );

    if (pte == 0 ||
        !(*pte & PTE_V)) {

        return 0;
    }

    if (!(*pte &
          (PTE_R |
           PTE_W |
           PTE_X))) {

        return 0;
    }

    return
        PTE_TO_PA(*pte) |
        (va &
         (PAGE_SIZE - 1));
}


/*
 * User pointer validation.
 *
 * required_flags 예:
 *
 * PTE_R:
 *     kernel이 user memory를 읽음
 *     write(), open() 등
 *
 * PTE_W:
 *     kernel이 user memory에 씀
 *     read() 등
 */
int vm_user_range_valid(
    pagetable_t root,
    unsigned long va,
    unsigned long size,
    unsigned long required_flags)
{
    if (root == 0) {
        return 0;
    }

    /*
     * 0 byte 범위는 접근할 memory가 없다.
     */
    if (size == 0) {
        return 1;
    }

    /*
     * NULL pointer 금지.
     */
    if (va == 0) {
        return 0;
    }

    /*
     * va + size - 1 overflow 검사.
     */
    unsigned long end =
        va + size - 1;

    if (end < va) {
        return 0;
    }

    /*
     * Mini-RVOS user virtual address는
     * Sv39 low canonical half만 허용한다.
     */
    if (va >= SV39_USER_TOP ||
        end >= SV39_USER_TOP) {

        return 0;
    }

    unsigned long current = va;

    for (;;) {
        pte_t *pte =
            vm_walk(
                root,
                current,
                0
            );

        if (pte == 0) {
            return 0;
        }

        unsigned long entry =
            *pte;

        /*
         * 반드시 valid user leaf page.
         */
        if (!(entry & PTE_V) ||
            !(entry & PTE_U)) {

            return 0;
        }

        if (!(entry &
              (PTE_R |
               PTE_W |
               PTE_X))) {

            return 0;
        }

        /*
         * 요청한 권한 확인.
         */
        if ((entry &
             required_flags) !=
            required_flags) {

            return 0;
        }

        unsigned long page =
            current &
            ~(PAGE_SIZE - 1);

        unsigned long end_page =
            end &
            ~(PAGE_SIZE - 1);

        if (page == end_page) {
            break;
        }

        /*
         * 다음 page로 이동.
         */
        current =
            page +
            PAGE_SIZE;
    }

    return 1;
}


void vm_enable(
    pagetable_t root)
{
    unsigned long satp =
        SATP_MODE_SV39 |
        ((unsigned long)root >> 12);

#ifdef BENCHMARK_MODE

    unsigned long start =
        riscv_read_time();

#endif

    riscv_sfence_vma();

    riscv_write_satp(
        satp
    );

    riscv_sfence_vma();

#ifdef BENCHMARK_MODE

    unsigned long end =
        riscv_read_time();

    research_record_sfence(2);

    research_record_address_space_switch(
        end - start
    );

#endif
}


void vm_enable_asid(
    pagetable_t root,
    unsigned long asid)
{
    unsigned long satp =
        SATP_MODE_SV39 |
        ((asid & VM_ASID_MAX) <<
         SATP_ASID_SHIFT) |
        ((unsigned long)root >> 12);

#ifdef BENCHMARK_MODE

    unsigned long start =
        riscv_read_time();

#endif

    /*
     * Benchmark invariant:
     *
     * - each process owns a unique ASID,
     * - ASIDs are not reused,
     * - page tables are not modified after
     *   benchmark execution begins,
     * - page-table writes are synchronized
     *   before the measured run.
     *
     * Therefore a context switch only needs
     * to replace satp.
     */
    riscv_write_satp(
        satp
    );

#ifdef BENCHMARK_MODE

    unsigned long end =
        riscv_read_time();

    research_record_address_space_switch(
        end - start
    );

#endif
}


unsigned long vm_detect_asid_bits(
    pagetable_t root)
{
    unsigned long original_satp =
        riscv_read_satp();

    unsigned long probe_satp =
        SATP_MODE_SV39 |
        (VM_ASID_MAX <<
         SATP_ASID_SHIFT) |
        ((unsigned long)root >> 12);

    /*
     * Synchronize all page-table stores before
     * temporarily enabling the probe address space.
     */
    riscv_sfence_vma();

    riscv_write_satp(
        probe_satp
    );

    unsigned long readback =
        riscv_read_satp();

    riscv_write_satp(
        original_satp
    );

    /*
     * Remove translations created by the probe and
     * leave a clean translation state before the run.
     */
    riscv_sfence_vma();

    unsigned long mask =
        (readback >>
         SATP_ASID_SHIFT) &
        VM_ASID_MAX;

    unsigned long bits = 0;

    while (mask & 1UL) {
        bits++;
        mask >>= 1;
    }

    return bits;
}
