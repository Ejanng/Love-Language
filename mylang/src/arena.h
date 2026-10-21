#pragma once
// Memory for everything the front end builds (tokens, AST nodes, compiled functions).
// Nothing is freed one piece at a time: the whole arena is released at exit by arena_free_all().
// That also means a syntax error can abandon a half-built tree without leaking anything.
#include <stddef.h>

// Returns zeroed memory, aligned for any type.
void* arena_alloc(size_t size);

// Copies `length` bytes of `text` and adds a terminating '\0'.
char* arena_strndup(const char* text, size_t length);

// Growable arrays: makes sure `array` (which currently holds `count` elements and has room for *cap)
// has space for one more element, and returns the (possibly moved) array.
void* arena_array_reserve(void* array, int count, int* cap, size_t elemSize);

void arena_free_all(void);

// A growable list of pointers, used while the parser collects statements, arguments and parameters.
typedef struct {
    void** items;
    int count;
    int cap;
} PtrVec;

void ptrvec_push(PtrVec* vec, void* item);
