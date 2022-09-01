#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void
error_msg(const char * file, int line, const char * message) {
    fprintf(stderr, "Critical Error (%s:%i): %s\n", file, line, message);
    exit(2);
}

void
assume_fail(const char * file, int line, const char * expr) {
    fprintf(stderr, "Assumption failure (%s:%i): %s\n", file, line, expr);
    exit(2);
}

#ifndef __GNUC__
  #define __builtin_expect(_Expr, _Value) _Expr
  #define __builtin_unreachable ((void)0)
#endif

#define assert_msg(_Expr, _Msg) if(__builtin_expect(!(_Expr), 0)) error_msg(__FILE__, __LINE__, _Msg " (" #_Expr ")")

#ifdef DEBUG
  #define _assume(_Expr) if(__builtin_expect(!(_Expr), 0)) assume_fail(__FILE__, __LINE__, #_Expr)
#else
  #define _assume(_Expr) if(!(_Expr)) __builtin_unreachable()
#endif

#define DYN_ALLOCATOR_BANKS_PER_POOL 32
#define DYN_ALLOCATOR_SMALLEST_BANK_SHIFT 7

typedef struct {
    uint32_t capacity;
    uint32_t used;
    uint16_t pool_index;
    uint8_t  pool_group_index;
    uint8_t  slot_index;
    uint8_t  _reserved [4];
} DynAllocationHeader;

_Static_assert(sizeof(DynAllocationHeader) == 16, "DynAllocationHeader must be 16 bytes.");

typedef struct {
    size_t    count_vacant;
    uint8_t   vacant [DYN_ALLOCATOR_BANKS_PER_POOL];
    uint8_t * data;
} DynAllocatorPool;

typedef struct {
    uint32_t next_unallocated;
    uint32_t count_available;
    uint16_t available [32];
    DynAllocatorPool pools [4096];
} DynAllocatorPoolGroup;

typedef struct {
    DynAllocatorPoolGroup pool_groups [13];
} DynAllocator;

DynAllocator *
new_dyn_allocator() {
    DynAllocator * alloc = calloc(1, sizeof(*alloc));

    return alloc;
}

void
free_dyn_allocator(DynAllocator * alloc) {
    for (int group_i=0; group_i < LENGTH(alloc->pool_groups); group_i++) {
        DynAllocatorPoolGroup * group = &alloc->pool_groups[group_i];
        for (int pool_i=0; pool_i < group->next_unallocated; pool_i++) {
            free(group->pools[pool_i].data);
        }
    }
    free(alloc);
}

void
dyn_allocator_free(DynAllocator * alloc, void * ptr) {
    if (!ptr) return;
    DynAllocationHeader * header = (DynAllocationHeader *)ptr - 1;
    assert_msg(header->used > 0, "Double free.");
    DynAllocatorPoolGroup * group = &alloc->pool_groups[header->pool_group_index];
    DynAllocatorPool * pool = &group->pools[header->pool_index];
    pool->vacant[pool->count_vacant++] = header->slot_index;
    _assume(pool->count_vacant <= DYN_ALLOCATOR_BANKS_PER_POOL);
    if (pool->count_vacant == 1 && group->count_available < LENGTH(group->available)) {
        group->available[group->count_available++] = header->pool_index;
    }
    header->used = 0;
}

void *
dyn_allocator_realloc(DynAllocator * alloc, void * ptr, size_t amount) {
    assert_msg(amount > 0, "Cannot allocate 0 bytes.");

    DynAllocationHeader * prev_header = NULL;
    if (ptr) {
        prev_header = (DynAllocationHeader *)ptr - 1;
        if (prev_header->capacity >= amount) {
            prev_header->used = amount;
            return ptr;
        }
    }

    size_t size_bit_index = INTRO_BSR64(amount + sizeof(DynAllocationHeader)) + 2;
    size_t pool_group_index = (size_bit_index >= DYN_ALLOCATOR_SMALLEST_BANK_SHIFT)
                               ? size_bit_index - DYN_ALLOCATOR_SMALLEST_BANK_SHIFT
                               : 0;
    pool_group_index >>= 1;
    size_t bank_size = 1 << ((pool_group_index << 1) + DYN_ALLOCATOR_SMALLEST_BANK_SHIFT);

    assert_msg(pool_group_index < LENGTH(alloc->pool_groups), "Allocation size is too large.");
    DynAllocatorPoolGroup * group = &alloc->pool_groups[pool_group_index];

    DynAllocatorPool * pool;
    uint16_t pool_i;
    uint8_t slot_i = 0;

    if (group->count_available > 0) {
        pool_i = group->available[group->count_available - 1];
        pool = &group->pools[pool_i];

        _assume(pool->count_vacant > 0);
        slot_i = pool->vacant[--pool->count_vacant];
        if (pool->count_vacant == 0) {
            group->count_available -= 1;
        }
    } else {
        pool_i = group->next_unallocated++;
        _assume(pool_i < LENGTH(group->pools));
        pool = &group->pools[pool_i];

        pool->data = malloc(DYN_ALLOCATOR_BANKS_PER_POOL * bank_size);
        assert_msg(pool->data, "Failed to malloc buffer.");

        for (uint8_t new_i=0; new_i < DYN_ALLOCATOR_BANKS_PER_POOL; new_i++) {
            DynAllocationHeader * new_header = (DynAllocationHeader *)(pool->data + bank_size * new_i);
            new_header->capacity = bank_size - sizeof(DynAllocationHeader);
            new_header->used = 0;
            new_header->pool_index = pool_i;
            new_header->pool_group_index = pool_group_index;
            new_header->slot_index = new_i;
        }
        for (int i=0; i < DYN_ALLOCATOR_BANKS_PER_POOL; i++) {
            pool->vacant[i] = DYN_ALLOCATOR_BANKS_PER_POOL - i - 1;
        }
        slot_i = 0;
        pool->count_vacant = DYN_ALLOCATOR_BANKS_PER_POOL - 1;

        group->available[group->count_available++] = pool_i;
    }

    DynAllocationHeader * slot_header = (DynAllocationHeader *)(pool->data + bank_size * slot_i);
    slot_header->used = amount;

    void * dst = (void *)(slot_header + 1);
    
    if (prev_header) {
        void * src = (void *)(prev_header + 1);
        memcpy(dst, src, prev_header->used);
        dyn_allocator_free(alloc, src);
    }

    return dst;
}
