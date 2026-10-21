#include "arena.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BLOCK_SIZE (64 * 1024)

typedef struct Block {
    struct Block* next;
    size_t used;
    size_t size;
    max_align_t align;  // forces the data that follows to be suitably aligned
} Block;

static Block* blocks = NULL;

static Block* new_block(size_t size) {
    Block* b = calloc(1, sizeof(Block) + size);  // calloc: arena memory starts out zeroed
    if (!b) {
        fputs("Out of memory.\n", stderr);
        exit(70);
    }
    b->size = size;
    return b;
}

void* arena_alloc(size_t size) {
    size = (size + 15) & ~(size_t)15;
    if (size > BLOCK_SIZE / 4) {  // big requests get a block of their own
        Block* b = new_block(size);
        if (blocks) {
            b->next = blocks->next;  // keep the current bump block at the head
            blocks->next = b;
        } else {
            blocks = b;
        }
        b->used = size;
        return (char*)(b + 1);
    }
    if (!blocks || blocks->used + size > blocks->size) {
        Block* b = new_block(BLOCK_SIZE);
        b->next = blocks;
        blocks = b;
    }
    void* p = (char*)(blocks + 1) + blocks->used;
    blocks->used += size;
    return p;
}

char* arena_strndup(const char* text, size_t length) {
    char* copy = arena_alloc(length + 1);
    memcpy(copy, text, length);
    return copy;
}

void* arena_array_reserve(void* array, int count, int* cap, size_t elemSize) {
    if (count < *cap) return array;
    int newCap = *cap < 8 ? 8 : *cap * 2;
    void* grown = arena_alloc((size_t)newCap * elemSize);
    if (count > 0) memcpy(grown, array, (size_t)count * elemSize);
    *cap = newCap;
    return grown;
}

void arena_free_all(void) {
    while (blocks) {
        Block* next = blocks->next;
        free(blocks);
        blocks = next;
    }
}

void ptrvec_push(PtrVec* vec, void* item) {
    vec->items = arena_array_reserve(vec->items, vec->count, &vec->cap, sizeof(void*));
    vec->items[vec->count++] = item;
}
