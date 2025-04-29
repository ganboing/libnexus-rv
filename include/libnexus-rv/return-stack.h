#ifndef LIBNEXUS_RV_RETURN_STACK_H
#define LIBNEXUS_RV_RETURN_STACK_H

#include <assert.h>
#include <stdint.h>

#define NEXUSRV_RETURN_STACK_MAX 32

typedef struct nexusrv_return_stack {
    unsigned used;
    unsigned end;
    uint64_t entries[NEXUSRV_RETURN_STACK_MAX];
} nexusrv_return_stack;

static inline unsigned nexusrv_retstack_used(nexusrv_return_stack *stack) {
    return stack->used;
}

static inline void nexusrv_retstack_clear(nexusrv_return_stack *stack) {
    stack->used = 0;
}

static inline void nexusrv_retstack_push(nexusrv_return_stack *stack,
                                         uint64_t addr) {
    stack->entries[stack->end++] = addr;
    stack->end %= NEXUSRV_RETURN_STACK_MAX;
    if (stack->used < NEXUSRV_RETURN_STACK_MAX)
        ++stack->used;
}

static inline uint64_t nexusrv_retstack_pop(nexusrv_return_stack *stack) {
    assert(stack->used);
    if (!stack->end)
        stack->end = NEXUSRV_RETURN_STACK_MAX;
    uint64_t ret = stack->entries[--stack->end];
    --stack->used;
    return ret;
}

#endif
