#ifndef PAGE_ALLOC_H
#define PAGE_ALLOC_H

#include <stdint.h>

#define PAGE_SIZE 4096UL

void page_alloc_init(void);
void *page_alloc(void);
int page_free(void *page);
uint64_t page_total_pages(void);
uint64_t page_free_pages(void);
uint64_t page_used_pages(void);

#endif
