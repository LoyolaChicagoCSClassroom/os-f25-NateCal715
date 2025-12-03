// src/mini_libc.c
// Minimal libc-ish support for our freestanding kernel so KILO can link.

#include "rprintf.h"   // for size_t, NULL
#include <stdint.h>

// ----------------- simple heap allocator -----------------

#define HEAP_SIZE (64 * 1024)   // 64 KB heap for KILO etc.

typedef struct block_header {
    size_t size;
    int free;
    struct block_header *next;
} block_header_t;

static uint8_t heap[HEAP_SIZE];
static block_header_t *heap_head = NULL;

static void heap_init(void) {
    heap_head = (block_header_t *)heap;
    heap_head->size = HEAP_SIZE - sizeof(block_header_t);
    heap_head->free = 1;
    heap_head->next = NULL;
}

static void split_block(block_header_t *block, size_t size) {
    if (block->size >= size + sizeof(block_header_t) + 4) {
        block_header_t *new_block =
            (block_header_t *)((uint8_t *)block + sizeof(block_header_t) + size);
        new_block->size = block->size - size - sizeof(block_header_t);
        new_block->free = 1;
        new_block->next = block->next;

        block->size = size;
        block->next = new_block;
    }
}

static block_header_t *ptr_to_block(void *ptr) {
    if (!ptr) return NULL;
    return (block_header_t *)((uint8_t *)ptr - sizeof(block_header_t));
}

void *malloc(size_t size) {
    if (!size) return NULL;
    if (!heap_head) heap_init();

    block_header_t *curr = heap_head;
    while (curr) {
        if (curr->free && curr->size >= size) {
            split_block(curr, size);
            curr->free = 0;
            return (uint8_t *)curr + sizeof(block_header_t);
        }
        curr = curr->next;
    }
    return NULL;
}

void free(void *ptr) {
    block_header_t *block = ptr_to_block(ptr);
    if (!block) return;

    block->free = 1;

    // simple coalescing with next
    if (block->next && block->next->free) {
        block->size += sizeof(block_header_t) + block->next->size;
        block->next = block->next->next;
    }
}

void *realloc(void *ptr, size_t size) {
    if (!ptr) return malloc(size);
    if (!size) {
        free(ptr);
        return NULL;
    }

    block_header_t *block = ptr_to_block(ptr);
    if (block->size >= size) {
        // old block already big enough
        return ptr;
    }

    // allocate new block and copy old contents
    void *new_ptr = malloc(size);
    if (!new_ptr) return NULL;

    size_t copy_size = block->size < size ? block->size : size;
    extern void *memcpy(void *dest, const void *src, size_t n);
    memcpy(new_ptr, ptr, copy_size);
    free(ptr);
    return new_ptr;
}

// ----------------- basic memory / string helpers -----------------

void *memcpy(void *dest, const void *src, size_t n) {
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;
    for (size_t i = 0; i < n; i++) {
        d[i] = s[i];
    }
    return dest;
}

void *memmove(void *dest, const void *src, size_t n) {
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;

    if (d == s || n == 0) return dest;

    if (d < s) {
        for (size_t i = 0; i < n; i++) {
            d[i] = s[i];
        }
    } else {
        for (size_t i = n; i > 0; i--) {
            d[i - 1] = s[i - 1];
        }
    }
    return dest;
}

char *strncpy(char *dest, const char *src, size_t n) {
    size_t i = 0;
    for (; i < n && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    for (; i < n; i++) {
        dest[i] = '\0';
    }
    return dest;
}

// strlen is already implemented in rprintf.c, so just declare it
extern size_t strlen(const char *str);

char *strdup(const char *s) {
    size_t len = strlen(s);
    char *copy = (char *)malloc(len + 1);
    if (!copy) return NULL;
    memcpy(copy, s, len + 1);
    return copy;
}

// ----------------- fake time() -----------------

typedef long time_t;

time_t time(time_t *t) {
    static time_t fake = 0;
    fake++;             // just monotonic; not real wall-clock time
    if (t) *t = fake;
    return fake;
}
