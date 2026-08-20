#ifndef VM_H
#define VM_H

#include "memory.h"

typedef unsigned long pte_t;
typedef pte_t *pagetable_t;

#define PTE_V (1UL << 0)
#define PTE_R (1UL << 1)
#define PTE_W (1UL << 2)
#define PTE_X (1UL << 3)
#define PTE_U (1UL << 4)
#define PTE_G (1UL << 5)
#define PTE_A (1UL << 6)
#define PTE_D (1UL << 7)

pagetable_t vm_create(void);

int vm_map_page(
    pagetable_t root,
    unsigned long va,
    unsigned long pa,
    unsigned long flags
);

int vm_map_range(
    pagetable_t root,
    unsigned long va,
    unsigned long pa,
    unsigned long size,
    unsigned long flags
);

unsigned long vm_translate(
    pagetable_t root,
    unsigned long va
);

void vm_enable(pagetable_t root);

#endif
