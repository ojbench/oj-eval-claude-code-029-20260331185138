#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <sys/mman.h>

/* Memory block header structure */
typedef struct block {
    size_t size;           /* Size of the data area (not including header) */
    struct block *next;    /* Next free block in free list */
    int free;              /* 1 if block is free, 0 if allocated */
} block_t;

#define BLOCK_SIZE sizeof(block_t)
#define ALIGN8(x) (((((x)-1)>>3)<<3)+8)

static block_t *free_list = NULL;

/* Find a free block that fits the requested size */
block_t *find_free_block(block_t **last, size_t size) {
    block_t *current = free_list;
    while (current && !(current->free && current->size >= size)) {
        *last = current;
        current = current->next;
    }
    return current;
}

/* Request more space from the OS */
block_t *request_space(block_t *last, size_t size) {
    block_t *block;
    block = sbrk(0);
    void *request = sbrk(size + BLOCK_SIZE);

    if (request == (void*) -1) {
        return NULL; /* sbrk failed */
    }

    if (last) {
        last->next = block;
    }

    block->size = size;
    block->next = NULL;
    block->free = 0;
    return block;
}

/* malloc implementation */
void *malloc(size_t size) {
    block_t *block;

    if (size <= 0) {
        return NULL;
    }

    /* Align size to 8 bytes */
    size = ALIGN8(size);

    if (!free_list) {
        /* First call, request space */
        block = request_space(NULL, size);
        if (!block) {
            return NULL;
        }
        free_list = block;
    } else {
        block_t *last = free_list;
        block = find_free_block(&last, size);
        if (!block) {
            /* No free block found, request new space */
            block = request_space(last, size);
            if (!block) {
                return NULL;
            }
        } else {
            /* Found a free block */
            block->free = 0;
        }
    }

    return (block + 1); /* Return pointer to data area */
}

/* Get block pointer from data pointer */
block_t *get_block_ptr(void *ptr) {
    return (block_t*)ptr - 1;
}

/* free implementation */
void free(void *ptr) {
    if (!ptr) {
        return;
    }

    block_t *block = get_block_ptr(ptr);
    block->free = 1;

    /* Simple coalescing with next block */
    if (block->next && block->next->free) {
        block->size += BLOCK_SIZE + block->next->size;
        block->next = block->next->next;
    }
}

/* calloc implementation */
void *calloc(size_t nmemb, size_t size) {
    size_t total = nmemb * size;
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

    block_t *block = get_block_ptr(ptr);
    if (block->size >= size) {
        return ptr; /* Current block is large enough */
    }

    /* Allocate new block and copy data */
    void *new_ptr = malloc(size);
    if (new_ptr) {
        memcpy(new_ptr, ptr, block->size);
        free(ptr);
    }
    return new_ptr;
}
