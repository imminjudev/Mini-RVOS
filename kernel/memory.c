#include "../include/memory.h"

struct free_page {
    struct free_page *next;
};

static struct free_page *free_list;
static unsigned long free_page_count;
static unsigned long total_page_count;

extern char __kernel_end[];

static unsigned long align_up(unsigned long value)
{
    return (value + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
}

static unsigned long align_down(unsigned long value)
{
    return value & ~(PAGE_SIZE - 1);
}

void pmm_init(unsigned long memory_end)
{
    unsigned long start = align_up((unsigned long)__kernel_end);
    unsigned long end = align_down(memory_end);

    free_list = 0;
    free_page_count = 0;
    total_page_count = 0;

    for (unsigned long addr = start;
         addr + PAGE_SIZE <= end;
         addr += PAGE_SIZE) {
        page_free((void *)addr);
    }

    total_page_count = free_page_count;
}

void *page_alloc(void)
{
    if (free_list == 0) {
        return 0;
    }

    struct free_page *page = free_list;

    free_list = page->next;
    free_page_count--;

    return page;
}

void page_free(void *ptr)
{
    struct free_page *page = (struct free_page *)ptr;

    page->next = free_list;
    free_list = page;

    free_page_count++;
}

unsigned long pmm_free_pages(void)
{
    return free_page_count;
}

unsigned long pmm_total_pages(void)
{
    return total_page_count;
}
