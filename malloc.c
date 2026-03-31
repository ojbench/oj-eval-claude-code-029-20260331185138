#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

/* Memory block header structure */
typedef struct block {
    size_t size;           /* Size of the data area (not including header) */
    struct block *next;    /* Next block in list */
    int free;              /* 1 if block is free, 0 if allocated */
} block_t;

#define BLOCK_SIZE sizeof(block_t)
#define ALIGN8(x) (((((x)-1)>>3)<<3)+8)

static block_t *heap_list = NULL;

/* Get block pointer from data pointer */
static inline block_t *get_block_ptr(void *ptr) {
    return (block_t*)ptr - 1;
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
    block->free = 0;

    if (last) {
        last->next = block;
    }

    return block;
}

/* Find a free block that fits the requested size */
static block_t *find_free_block(block_t **last, size_t size) {
    block_t *current = heap_list;
    while (current) {
        if (current->free && current->size >= size) {
            return current;
        }
        *last = current;
        current = current->next;
    }
    return NULL;
}

/* malloc implementation */
void *malloc(size_t size) {
    block_t *block;

    if (size == 0) {
        return NULL;
    }

    /* Align size to 8 bytes */
    size = ALIGN8(size);

    if (!heap_list) {
        /* First call, request space */
        block = request_space(NULL, size);
        if (!block) {
            return NULL;
        }
        heap_list = block;
    } else {
        block_t *last = heap_list;
        block = find_free_block(&last, size);
        if (block) {
            /* Found a free block */
            block->free = 0;
        } else {
            /* No free block found, request new space */
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

    /* Simple coalescing with next block */
    if (block->next && block->next->free) {
        block->size += BLOCK_SIZE + block->next->size;
        block->next = block->next->next;
    }
}

/* calloc implementation */
void *calloc(size_t nmemb, size_t size) {
    if (nmemb == 0 || size == 0) {
        return NULL;
    }

    /* Check for overflow */
    size_t total = nmemb * size;
    if (nmemb != 0 && total / nmemb != size) {
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

    block_t *block = get_block_ptr(ptr);

    /* Align the requested size */
    size = ALIGN8(size);

    if (block->size >= size) {
        return ptr; /* Current block is large enough */
    }

    /* Allocate new block and copy data */
    void *new_ptr = malloc(size);
    if (new_ptr) {
        size_t copy_size = block->size < size ? block->size : size;
        memcpy(new_ptr, ptr, copy_size);
        free(ptr);
    }
    return new_ptr;
}
