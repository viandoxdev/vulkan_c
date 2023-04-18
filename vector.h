#ifndef TEST_H
#define TEST_H
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include "assert.h"
#include "macro_utils.h"

#ifndef VECTOR_IMPL_LIST
#define VECTOR_IMPL_LIST
#endif

#define _VECTOR_MAP_ADD(m, a, fst, ...) \
    m(a, fst) \
    __VA_OPT__(DEFER1(_VECTOR__MAP_ADD)()(m, a, __VA_ARGS__))
#define _VECTOR__MAP_ADD() _VECTOR_MAP_ADD

#define _VECTOR__IMPL(T, V, qualifier) \
    typedef struct { \
        T* data; \
        size_t len; \
        size_t cap; \
    } V; \
    __attribute__((unused)) \
    static inline V _vec_##qualifier##_init() { \
        return (V) vec_init(); \
    } \
    __attribute__((unused)) \
    static inline void _vec_##qualifier##_drop(V vec) { \
        if(vec.data != NULL) { \
            free(vec.data); \
        } \
    } \
    __attribute__((unused)) \
    static inline void _vec_##qualifier##_grow(V *vec, size_t cap) { \
        if (cap <= vec->cap) \
            return; \
        if (vec->data == NULL || vec->cap == 0) { \
            vec->data = malloc(cap * sizeof(T)); \
            assert_alloc(vec->data); \
            vec->cap = cap; \
            return; \
        } \
        if (cap < 2 * vec->cap) { \
            cap = 2 * vec->cap; \
        } \
        T *newp = realloc(vec->data, cap * sizeof(T)); \
        assert_alloc(newp); \
        vec->data = newp; \
        vec->cap = cap; \
    } \
    __attribute__((unused)) \
    static inline V _vec_##qualifier##_init_with_cap(size_t cap) { \
        V vec = vec_init(); \
        _vec_##qualifier##_grow(&vec, cap); \
        return vec; \
    } \
    __attribute__((unused)) \
    static inline void _vec_##qualifier##_shrink(V *vec) { \
        if (vec->len > 0) { \
            T *newp = realloc(vec->data, vec->len); \
            assert_alloc(newp); \
            vec->data = newp; \
            vec->cap = vec->len; \
        } else { \
            free(vec->data); \
            vec->data = NULL; \
            vec->cap = 0; \
        } \
    } \
    __attribute__((unused)) \
    static inline void _vec_##qualifier##_push(V *vec, T val) { \
        _vec_##qualifier##_grow(vec, vec->len + 1); \
        vec->data[vec->len++] = val; \
    } \
    __attribute__((unused)) \
    static inline void _vec_##qualifier##_push_array(V *vec, T* vals, size_t count) { \
        _vec_##qualifier##_grow(vec, vec->len + count); \
        for(size_t i = 0 ; i < count; i++) { \
            vec->data[vec->len++] = vals[i]; \
        } \
    } \
    __attribute__((unused)) \
    static inline bool _vec_##qualifier##_pop_opt(V *vec, T *val) { \
        if (vec->len == 0) \
            return false; \
        vec->len--; \
        if (val != NULL) \
            *val = vec->data[vec->len]; \
        return true; \
    } \
    __attribute__((unused)) \
    static inline T _vec_##qualifier##_pop(V *vec) { \
        debug_assert(vec->len > 0, "Popping zero length %s", #V); \
        return vec->data[--vec->len]; \
    } \
    __attribute__((unused)) \
    static inline void _vec_##qualifier##_clear(V *vec) { vec->len = 0; } \
    __attribute__((unused)) \
    static inline T _vec_##qualifier##_get(V *vec, size_t index) { \
        debug_assert(index < vec->len, "Out of bound index, on %s (index is %lu, but length is %lu)", #V, index, vec->len); \
        return vec->data[index]; \
    } \
    static inline bool _vec_##qualifier##_get_opt(V * vec, size_t index, T *val) { \
        if(index >= vec->len) { \
            return false; \
        } else if(val != NULL) { \
            *val = vec->data[index]; \
        } \
        return true; \
    } \

#define _VECTOR_IMPL(x) _VECTOR__IMPL x

#define _VECTOR_GEN(a, x) DEFER1(_VECTOR__GEN)(a, _VECTOR__GEN_CLOSE x
#define _VECTOR__GEN_CLOSE(a, b, c) a, b, c)
#define _VECTOR__GEN(x, _, V, qualifier) V: _vec_##qualifier##x,

#define _VECTOR_GENERIC(expr, x) _Generic(expr, EVAL(CALL(_VECTOR_MAP_ADD, _VECTOR_GEN, _##x, VECTOR_IMPL_LIST)) struct _VectorNever: (void)0)

#define vec_init() { .data = NULL, .len = 0, .cap = 0 }
#define vec_drop(vec) _VECTOR_GENERIC(vec, drop)(vec)
#define vec_grow(vec, len) _VECTOR_GENERIC(*(vec), grow)(vec, len)
#define vec_shrink(vec) _VECTOR_GENERIC(*(vec), shrink)(vec)
#define vec_push(vec, val) _VECTOR_GENERIC(*(vec), push)(vec, val)
#define vec_push_array(vec, vals, count) _VECTOR_GENERIC(*(vec), push_array)(vec, vals, count)
#define vec_pop_opt(vec, val) _VECTOR_GENERIC(*(vec), pop_opt)(vec, val)
#define vec_pop(vec) _VECTOR_GENERIC(*(vec), pop)(vec)
#define vec_clear(vec) _VECTOR_GENERIC(*(vec), clear)(vec)
#define vec_get(vec, index) _VECTOR_GENERIC(*(vec), get)(vec, index)
#define vec_get_opt(vec, index, val) _VECTOR_GENERIC(*(vec), get_opt)(vec, index, val)

struct _VectorNever { void * _; };

EVAL(CALL(MAP, _VECTOR_IMPL, VECTOR_IMPL_LIST))

#endif
