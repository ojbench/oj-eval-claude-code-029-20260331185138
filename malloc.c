#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <sys/mman.h>
#include <assert.h>

/* Memory block header structure */
typedef struct block {
    size_t size;           /* Size of the data area (not including header) */
    struct block *next;    /* Next block in list */
    struct block *prev;    /* Previous block in list */
    int free;              /* 1 if block is free, 0 if allocated */
} block_t;

#define BLOCK_SIZE sizeof(block_t)
#define ALIGN8(x) (((((x)-1)>>3)<<3)+8)
#define MIN_BLOCK_SIZE 32  /* Minimum size for splitting */

static block_t *heap_start = NULL;

/* Get block pointer from data pointer */
static inline block_t *get_block_ptr(void *ptr) {
    return (block_t*)ptr - 1;
}

/* Coalesce adjacent free blocks */
static void coalesce(block_t *block) {
    if (!block) return;

    /* Coalesce with next block */
    if (block->next && block->next->free) {
        block->size += BLOCK_SIZE + block->next->size;
        block->next = block->next->next;
        if (block->next) {
            block->next->prev = block;
        }
    }

    /* Coalesce with previous block */
    if (block->prev && block->prev->free) {
        block->prev->size += BLOCK_SIZE + block->size;
        block->prev->next = block->next;
        if (block->next) {
            block->next->prev = block->prev;
        }
    }
}

/* Split block if it's larger than needed */
static void split_block(block_t *block, size_t size) {
    if (block->size >= size + BLOCK_SIZE + MIN_BLOCK_SIZE) {
        block_t *new_block = (block_t*)((char*)(block + 1) + size);
        new_block->size = block->size - size - BLOCK_SIZE;
        new_block->next = block->next;
        new_block->prev = block;
        new_block->free = 1;

        if (block->next) {
            block->next->prev = new_block;
        }

        block->next = new_block;
        block->size = size;
    }
}

/* Find a free block that fits the requested size (best fit) */
static block_t *find_free_block(size_t size) {
    block_t *current = heap_start;
    block_t *best = NULL;
    size_t best_size = (size_t)-1;

    while (current) {
        if (current->free && current->size >= size) {
            if (current->size < best_size) {
                best = current;
                best_size = current->size;
                /* Perfect fit */
                if (best_size == size) {
                    break;
                }
            }
        }
        current = current->next;
    }

    return best;
}

/* Request more space from the OS */
static block_t *request_space(block_t *last, size_t size) {
    block_t *block;
    block = sbrk(0);
    void *request = sbrk(size + BLOCK_SIZE);

    if (request == (void*) -1) {
        return NULL; /* sbrk failed */
    }

    block->size = size;
    block->next = NULL;
    block->prev = last;
    block->free = 0;

    if (last) {
        last->next = block;
    }

    return block;
}

/* malloc implementation */
void *malloc(size_t size) {
    block_t *block;

    if (size == 0) {
        return NULL;
    }

    /* Align size to 8 bytes */
    size = ALIGN8(size);

    if (!heap_start) {
        /* First call, request space */
        block = request_space(NULL, size);
        if (!block) {
            return NULL;
        }
        heap_start = block;
    } else {
        /* Find a free block */
        block = find_free_block(size);
        if (block) {
            /* Found a free block */
            block->free = 0;
            split_block(block, size);
        } else {
            /* No free block found, request new space */
            block_t *last = heap_start;
            while (last->next) {
                last = last->next;
            }
            block = request_space(last, size);
            if (!block) {
                return NULL;
            }
        }
    }

    return (block + 1); /* Return pointer to data area */
}

/* free implementation */
void free(void *ptr) {
    if (!ptr) {
        return;
    }

    block_t *block = get_block_ptr(ptr);
    block->free = 1;

    /* Coalesce with adjacent free blocks */
    coalesce(block);
}

/* calloc implementation */
void *calloc(size_t nmemb, size_t size) {
    if (nmemb == 0 || size == 0) {
        return NULL;
    }

    /* Check for overflow */
    size_t total = nmemb * size;
    if (total / nmemb != size) {
        return NULL;
    }

    void *ptr = malloc(total);
    if (ptr) {
        memset(ptr, 0, total);
    }
    return ptr;
}

/* realloc implementation */
void *realloc(void *ptr, size_t size) {
    if (!ptr) {
        return malloc(size);
    }

    if (size == 0) {
        free(ptr);
        return NULL;
    }

    size = ALIGN8(size);
    block_t *block = get_block_ptr(ptr);

    if (block->size >= size) {
        /* Current block is large enough, try to split */
        split_block(block, size);
        return ptr;
    }

    /* Check if we can expand by coalescing with next block */
    if (block->next && block->next->free &&
        block->size + BLOCK_SIZE + block->next->size >= size) {
        /* Merge with next block */
        block->size += BLOCK_SIZE + block->next->size;
        block->next = block->next->next;
        if (block->next) {
            block->next->prev = block;
        }
        split_block(block, size);
        return ptr;
    }

    /* Allocate new block and copy data */
    void *new_ptr = malloc(size);
    if (new_ptr) {
        memcpy(new_ptr, ptr, block->size < size ? block->size : size);
        free(ptr);
    }
    return new_ptr;
}
