/**
 * @file nimcp_math_constants.h
 * @brief Centralized mathematical constants for NIMCP
 * @version 1.0.0
 * @date 2026-02-16
 *
 * WHAT: Defines mathematical constants used throughout the codebase
 * WHY:  Eliminates duplicate #define M_PI / TWO_PI / SQRT2 and inline magic numbers
 * HOW:  Single header replacing ~49 local #define M_PI and ~80 inline 3.14159f literals
 *
 * Usage: #include "constants/nimcp_math_constants.h"
 */

#ifndef NIMCP_MATH_CONSTANTS_H
#define NIMCP_MATH_CONSTANTS_H

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Pi and Multiples
 * ============================================================================ */

/** Pi (float) */
#ifndef NIMCP_PI_F
#define NIMCP_PI_F          3.14159265358979323846f
#endif

/** Pi (double) */
#ifndef NIMCP_PI
#define NIMCP_PI            3.14159265358979323846
#endif

/** 2*Pi (float) - full circle in radians */
#ifndef NIMCP_TWO_PI_F
#define NIMCP_TWO_PI_F      6.28318530717958647692f
#endif

/** 2*Pi (double) */
#ifndef NIMCP_TWO_PI
#define NIMCP_TWO_PI        6.28318530717958647692
#endif

/** Pi/2 (float) - quarter turn */
#ifndef NIMCP_HALF_PI_F
#define NIMCP_HALF_PI_F     1.57079632679489661923f
#endif

/* ============================================================================
 * Square Roots
 * ============================================================================ */

/** sqrt(2) (float) - used for exploration constants, normalization */
#ifndef NIMCP_SQRT2_F
#define NIMCP_SQRT2_F       1.41421356237309504880f
#endif

/** sqrt(2) (double) */
#ifndef NIMCP_SQRT2
#define NIMCP_SQRT2         1.41421356237309504880
#endif

/** 1/sqrt(2) (float) */
#ifndef NIMCP_INV_SQRT2_F
#define NIMCP_INV_SQRT2_F   0.70710678118654752440f
#endif

/* ============================================================================
 * Euler's Number
 * ============================================================================ */

/** e (float) - base of natural logarithm */
#ifndef NIMCP_EULER_F
#define NIMCP_EULER_F       2.71828182845904523536f
#endif

/** e (double) */
#ifndef NIMCP_EULER
#define NIMCP_EULER         2.71828182845904523536
#endif

/* ============================================================================
 * Compatibility Aliases
 *
 * Provide M_PI for files that previously defined it locally.
 * Only M_PI/M_PI_F are safe as compatibility aliases since they are never
 * used as local variable names. TWO_PI/SQRT2/SQRT_2 are NOT aliased here
 * because some files use them as local variable names.
 * ============================================================================ */

#ifndef M_PI
#define M_PI    NIMCP_PI
#endif

#ifndef M_PI_F
#define M_PI_F  NIMCP_PI_F
#endif

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MATH_CONSTANTS_H */
