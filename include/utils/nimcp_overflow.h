#ifndef NIMCP_OVERFLOW_H
#define NIMCP_OVERFLOW_H
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <limits.h>
#ifdef __cplusplus
extern "C" {
#endif
static inline bool nimcp_mul_safe(size_t a, size_t b) { if (a == 0 || b == 0) return true; return a <= SIZE_MAX / b; }
static inline bool nimcp_add_safe(size_t a, size_t b) { return a <= SIZE_MAX - b; }
static inline bool nimcp_mul_safe_result(size_t a, size_t b, size_t* result) { if (!nimcp_mul_safe(a, b)) return false; if (result) *result = a * b; return true; }
static inline bool nimcp_add_safe_result(size_t a, size_t b, size_t* result) { if (!nimcp_add_safe(a, b)) return false; if (result) *result = a + b; return true; }
#define NIMCP_MUL_SAFE(a, b) nimcp_mul_safe((size_t)(a), (size_t)(b))
#define NIMCP_ADD_SAFE(a, b) nimcp_add_safe((size_t)(a), (size_t)(b))
#define NIMCP_MUL_SAFE_RESULT(a, b, r) nimcp_mul_safe_result((size_t)(a), (size_t)(b), (r))
#define NIMCP_ADD_SAFE_RESULT(a, b, r) nimcp_add_safe_result((size_t)(a), (size_t)(b), (r))
#ifdef __cplusplus
}
#endif
#endif
