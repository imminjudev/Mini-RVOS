#ifndef MEMORY_H
#define MEMORY_H

#define PAGE_SIZE 4096UL

void pmm_init(unsigned long memory_end);
void *page_alloc(void);
void page_free(void *ptr);
unsigned long pmm_free_pages(void);

#endif
