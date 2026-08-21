#include "../include/vm.h"
#include "../include/riscv.h"

#define VPN_MASK 0x1FFUL

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

    riscv_sfence_vma();

    riscv_write_satp(
        satp
    );

    riscv_sfence_vma();
}
