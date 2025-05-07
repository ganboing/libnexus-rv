#ifndef LIBNEXUS_RV_RETURN_STACK_H
#define LIBNEXUS_RV_RETURN_STACK_H

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct nexusrv_return_stack {
    uint64_t *entries;
    unsigned size;
    unsigned used;
    unsigned end;
} nexusrv_return_stack;

static inline int nexusrv_retstack_init(nexusrv_return_stack *stack,
                                        unsigned size) {
    stack->entries = (uint64_t*)malloc(sizeof(stack->entries[0]) * size);
    if (!stack->entries)
        return -nexus_no_mem;
    stack->size = size;
    stack->used = stack->end = 0;
    return 0;
}

static inline void nexusrv_retstack_fini(nexusrv_return_stack *stack) {
    free(stack->entries);
    stack->entries = NULL;
}

static inline unsigned nexusrv_retstack_used(nexusrv_return_stack *stack) {
    return stack->used;
}

static inline void nexusrv_retstack_clear(nexusrv_return_stack *stack) {
    stack->used = 0;
}

static inline void nexusrv_retstack_push(nexusrv_return_stack *stack,
                                         uint64_t addr) {
    stack->entries[stack->end++] = addr;
    stack->end %= stack->size;
    if (stack->used < stack->size)
        ++stack->used;
}

static inline uint64_t nexusrv_retstack_pop(nexusrv_return_stack *stack) {
    assert(stack->used);
    if (!stack->end)
        stack->end = stack->size;
    uint64_t ret = stack->entries[--stack->end];
    --stack->used;
    return ret;
}

#endif
