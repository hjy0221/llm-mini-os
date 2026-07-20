#include "page_alloc.h"

#include <stddef.h>
#include <stdint.h>

#define QEMU_RAM_START 0x40000000UL
#define QEMU_RAM_END 0x48000000UL

struct free_page {
    struct free_page *next;
};

extern char __kernel_end[];

static uintptr_t managed_start;
static uintptr_t bump_next;
static struct free_page *free_list;
static uint64_t total_pages;
static uint64_t used_pages;
static int initialized;

static void zero_page(void *page) {
    uint64_t *words = page;

    for (size_t index = 0U; index < PAGE_SIZE / sizeof(*words); ++index) {
        words[index] = 0U;
    }
}

static uintptr_t align_up_to_page(uintptr_t address) {
    return (address + PAGE_SIZE - 1U) & ~(PAGE_SIZE - 1U);
}

void page_alloc_init(void) {
    uintptr_t start = align_up_to_page((uintptr_t)__kernel_end);

    if (initialized) {
        return;
    }

    free_list = NULL;
    used_pages = 0U;
    initialized = 1;

    if (start < QEMU_RAM_START || start > QEMU_RAM_END) {
        managed_start = QEMU_RAM_END;
        bump_next = QEMU_RAM_END;
        total_pages = 0U;
        return;
    }

    managed_start = start;
    bump_next = start;
    total_pages = (QEMU_RAM_END - start) / PAGE_SIZE;
}

void *page_alloc(void) {
    struct free_page *page;

    if (!initialized) {
        return NULL;
    }

    if (free_list != NULL) {
        page = free_list;
        free_list = page->next;
    } else {
        if (bump_next >= QEMU_RAM_END) {
            return NULL;
        }

        page = (struct free_page *)bump_next;
        bump_next += PAGE_SIZE;
    }

    ++used_pages;
    zero_page(page);
    return page;
}

int page_free(void *page) {
    uintptr_t address = (uintptr_t)page;
    struct free_page *current;
    struct free_page *released;

    if (!initialized || page == NULL ||
        (address & (PAGE_SIZE - 1U)) != 0U ||
        address < managed_start || address >= bump_next ||
        address >= QEMU_RAM_END || used_pages == 0U) {
        return 0;
    }

    for (current = free_list; current != NULL; current = current->next) {
        if (current == page) {
            return 0;
        }
    }

    released = (struct free_page *)page;
    released->next = free_list;
    free_list = released;
    --used_pages;
    return 1;
}

uint64_t page_total_pages(void) {
    return total_pages;
}

uint64_t page_free_pages(void) {
    return total_pages - used_pages;
}

uint64_t page_used_pages(void) {
    return used_pages;
}
