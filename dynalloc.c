#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#define DYN_ALLOCATOR_BANKS_PER_POOL 64
#define DYN_ALLOCATOR_SMALLEST_BANK_SHIFT 6

typedef struct {
    uint8_t reserved_front [4];
    uint32_t capacity;
    uint32_t used;
    uint16_t pool_index;
    uint8_t pool_group_index;
    uint8_t reserved_back [1];
} DynAllocationHeader;

_Static_assert(sizeof(DynAllocationHeader) == 16, "DynAllocationHeader must be 16 bytes.");

typedef struct {
    uint8_t * data;
    uint32_t count_vacancies;
} DynAllocatorPool;

typedef struct {
    DynAllocatorPool pool_groups [13][1024];
} DynAllocator;

DynAllocator *
new_dyn_allocator() {
    DynAllocator * alloc = calloc(1, sizeof(*alloc));

    return alloc;
}

void
free_dyn_allocator(DynAllocator * alloc) {
    for (int group_i=0; group_i < LENGTH(alloc->pool_groups); group_i++) {
        for (int pool_i=0; pool_i < LENGTH(alloc->pool_groups[0]); group_i++) {
            DynAllocatorPool * pool = &alloc->pool_groups[group_i][pool_i];
            if (pool->data) {
                free(pool->data);
            } else {
                break;
            }
        }
    }
    free(alloc);
}

void
dyn_allocator_free(DynAllocator * alloc, void * ptr) {
    if (!ptr) return;
    DynAllocationHeader * header = (DynAllocationHeader *)ptr - 1;
    DynAllocatorPool * pool = &alloc->pool_groups[header->pool_group_index][header->pool_index];
    pool->count_vacancies += 1;
    memset(header, 0, sizeof(*header));
}

void *
dyn_allocator_realloc(DynAllocator * alloc, void * ptr, size_t amount) {
    assert(amount > 0);

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

    assert(pool_group_index < LENGTH(alloc->pool_groups));

    for (int pool_i=0; pool_i < LENGTH(alloc->pool_groups[0]); pool_i++) {
        DynAllocatorPool * pool = &alloc->pool_groups[pool_group_index][pool_i];
        DynAllocationHeader * slot_header;
        if (pool->data) {
            if (pool->count_vacancies > 0) {
                for (int slot_i=0; slot_i < DYN_ALLOCATOR_BANKS_PER_POOL; slot_i++) {
                    slot_header = (DynAllocationHeader *)(pool->data + bank_size * slot_i);
                    if (slot_header->used == 0) {
                        break;
                    }
                }
            } else {
                continue;
            }
        } else {
            pool->data = malloc(DYN_ALLOCATOR_BANKS_PER_POOL * bank_size);
            if (!pool->data) {
                fprintf(stderr, "malloc failed.");
                exit(9);
            }
            for (int slot_i=0; slot_i < DYN_ALLOCATOR_BANKS_PER_POOL; slot_i++) {
                DynAllocationHeader * new_slot = (DynAllocationHeader *)(pool->data + bank_size * slot_i);
                memset(new_slot, 0, sizeof(*new_slot));
            }
            pool->count_vacancies = DYN_ALLOCATOR_BANKS_PER_POOL;
            slot_header = (DynAllocationHeader *)pool->data;
        }

        slot_header->capacity = bank_size - sizeof(DynAllocationHeader);
        slot_header->used = amount;
        slot_header->pool_index = pool_i;
        slot_header->pool_group_index = pool_group_index;

        pool->count_vacancies -= 1;

        void * dst = (void *)(slot_header + 1);
        
        if (prev_header) {
            void * src = (void *)(prev_header + 1);
            memcpy(dst, src, prev_header->used);
            dyn_allocator_free(alloc, src);
        }

        return dst;
    }

    fprintf(stderr, "Failed to find allocation slot.\n");
    exit(8);
}
