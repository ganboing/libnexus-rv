#ifndef LIBNEXUS_RV_RETURN_STACK_H
#define LIBNEXUS_RV_RETURN_STACK_H

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

#define NEXUS_RV_RETSTACK_DEFAULT 32

typedef struct nexusrv_return_stack {
    uint64_t *entries;
    unsigned size;
    unsigned max;
    unsigned used;
    unsigned end;
} nexusrv_return_stack;

static inline int nexusrv_retstack_init(nexusrv_return_stack *stack,
                                        unsigned max) {
    stack->end = stack->used = 0;
    stack->max = stack->size = max;
    if (max > NEXUS_RV_RETSTACK_DEFAULT)
        stack->size = NEXUS_RV_RETSTACK_DEFAULT;
    stack->entries = NULL;
    if (!stack->size)
        return 0;
    stack->entries = (uint64_t*)malloc(
            sizeof(stack->entries[0]) * stack->size);
    if (!stack->entries)
        return -nexus_no_mem;
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
    stack->end = 0;
}

static inline int nexusrv_retstack_may_enlarge(nexusrv_return_stack *stack) {
    if (stack->size == stack->max)
        return 0;
    assert(stack->used == stack->end);
    if (stack->used != stack->size)
        return 0;
    unsigned newsize = stack->size * 2;
    if (newsize > stack->max)
        newsize = stack->max;
    void *newptr = realloc(stack->entries,
                           sizeof(stack->entries[0]) * newsize);
    if (!newptr)
        return -nexus_no_mem;
    stack->size = newsize;
    stack->entries = (uint64_t*)newptr;
    return 0;
}

static inline int nexusrv_retstack_push(nexusrv_return_stack *stack,
                                        uint64_t addr) {
    if (stack->used == stack->max)
        return 0;
    int rc = nexusrv_retstack_may_enlarge(stack);
    if (rc < 0)
        return rc;
    if (stack->end == stack->size)
        stack->end = 0;
    stack->entries[stack->end++] = addr;
    if (stack->used < stack->size)
        ++stack->used;
    return 0;
}

static inline int nexusrv_retstack_pop(nexusrv_return_stack *stack,
                                       uint64_t *addr) {
    if (!stack->used)
        return -nexus_trace_retstack_empty;
    assert(stack->end);
    *addr = stack->entries[--stack->end];
    --stack->used;
    if (!stack->end && stack->used)
        stack->end = stack->size;
    return 0;
}

#undef NEXUS_RV_RETSTACK_DEFAULT

#endif
