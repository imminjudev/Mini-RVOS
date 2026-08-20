#include "../include/vm.h"

#define PT_ENTRIES 512UL
#define VPN_MASK   0x1FFUL

#define PA_TO_PTE(pa) (((unsigned long)(pa) >> 12) << 10)
#define PTE_TO_PA(pte) (((unsigned long)(pte) >> 10) << 12)

#define VPN_INDEX(va, level) \
    (((unsigned long)(va) >> (12 + 9 * (level))) & VPN_MASK)

static void page_zero(void *ptr)
{
    unsigned char *p = (unsigned char *)ptr;

    for (unsigned long i = 0; i < PAGE_SIZE; i++) {
        p[i] = 0;
    }
}

pagetable_t vm_create(void)
{
    pagetable_t root = (pagetable_t)page_alloc();

    if (root == 0) {
        return 0;
    }

    page_zero(root);

    return root;
}

static pte_t *vm_walk(pagetable_t root,
                      unsigned long va,
                      int allocate)
{
    pagetable_t table = root;

    for (int level = 2; level > 0; level--) {
        pte_t *pte = &table[VPN_INDEX(va, level)];

        if (*pte & PTE_V) {
            if (*pte & (PTE_R | PTE_W | PTE_X)) {
                return 0;
            }

            table = (pagetable_t)PTE_TO_PA(*pte);
        } else {
            if (!allocate) {
                return 0;
            }

            pagetable_t next = (pagetable_t)page_alloc();

            if (next == 0) {
                return 0;
            }

            page_zero(next);

            *pte = PA_TO_PTE(next) | PTE_V;
            table = next;
        }
    }

    return &table[VPN_INDEX(va, 0)];
}

int vm_map_page(pagetable_t root,
                unsigned long va,
                unsigned long pa,
                unsigned long flags)
{
    if ((va & (PAGE_SIZE - 1)) != 0 ||
        (pa & (PAGE_SIZE - 1)) != 0) {
        return -1;
    }

    pte_t *pte = vm_walk(root, va, 1);

    if (pte == 0 || (*pte & PTE_V)) {
        return -1;
    }

    *pte = PA_TO_PTE(pa) | flags | PTE_V;

    return 0;
}

unsigned long vm_translate(pagetable_t root,
                           unsigned long va)
{
    pte_t *pte = vm_walk(root, va, 0);

    if (pte == 0 || !(*pte & PTE_V)) {
        return 0;
    }

    if (!(*pte & (PTE_R | PTE_W | PTE_X))) {
        return 0;
    }

    return PTE_TO_PA(*pte) | (va & (PAGE_SIZE - 1));
}
