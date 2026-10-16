//=============================================================================
// nimcp_ternary_logic.h - Ternary Logic Operations
//=============================================================================
/**
 * @file nimcp_ternary_logic.h
 * @brief Kleene and Lukasiewicz three-valued logic operations
 *
 * WHAT: Three-valued logic operations for uncertainty reasoning
 * WHY:  Classical boolean logic cannot represent unknown/uncertain states;
 *       ternary logic enables sound reasoning with partial information
 * HOW:  Implements Kleene K3 and Lukasiewicz L3 truth tables
 *
 * BIOLOGICAL BASIS:
 * - Prefrontal cortex maintains uncertainty representations
 * - Working memory holds partial conclusions with confidence bounds
 * - "I don't know" is a valid cognitive state requiring explicit representation
 *
 * MATHEMATICAL FOUNDATIONS:
 * - Kleene strong three-valued logic (K3): UNKNOWN propagates conservatively
 * - Lukasiewicz logic (L3): Different implication semantics for fuzzy reasoning
 *
 * TRUTH TABLES (Kleene K3):
 *
 * NOT:
 *   NOT(-1) = +1
 *   NOT( 0) =  0
 *   NOT(+1) = -1
 *
 * AND (min semantics):
 *        -1   0  +1
 *   -1   -1  -1  -1
 *    0   -1   0   0
 *   +1   -1   0  +1
 *
 * OR (max semantics):
 *        -1   0  +1
 *   -1   -1   0  +1
 *    0    0   0  +1
 *   +1   +1  +1  +1
 *
 * @author NIMCP Development Team
 * @date 2025-12-21
 */

#ifndef NIMCP_TERNARY_LOGIC_H
#define NIMCP_TERNARY_LOGIC_H

#include "nimcp_ternary_types.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Kleene Logic Operations (K3)
//=============================================================================

/**
 * @brief Kleene NOT (negation)
 *
 * WHAT: Three-valued negation where UNKNOWN stays UNKNOWN
 * WHY:  Negating uncertainty yields uncertainty
 * HOW:  Flip sign, keep zero unchanged: NOT(x) = -x
 *
 * TRUTH TABLE:
 *   NOT(-1) = +1  (not false is true)
 *   NOT( 0) =  0  (not unknown is unknown)
 *   NOT(+1) = -1  (not true is false)
 *
 * @param a Operand
 * @return Negated trit
 */
static inline trit_t trit_not(trit_t a) {
    return -a;
}

/**
 * @brief Kleene AND (conjunction)
 *
 * WHAT: Three-valued AND where FALSE dominates, then UNKNOWN
 * WHY:  Model uncertainty in logical conjunction
 * HOW:  min(a, b) semantics with ordering -1 < 0 < +1
 *
 * TRUTH TABLE:
 *   AND(-1, x) = -1 for all x (FALSE dominates)
 *   AND(+1, x) = x for all x  (TRUE is identity)
 *   AND(0, 0) = 0             (UNKNOWN + UNKNOWN = UNKNOWN)
 *
 * BIOLOGICAL BASIS:
 * - Amygdala threat detection: any threat signal dominates
 * - Prefrontal integration: uncertainty propagates through reasoning
 *
 * @param a First operand
 * @param b Second operand
 * @return Conjunction result
 */
static inline trit_t trit_and(trit_t a, trit_t b) {
    if (a == TRIT_NEGATIVE || b == TRIT_NEGATIVE) return TRIT_NEGATIVE;
    if (a == TRIT_UNKNOWN || b == TRIT_UNKNOWN) return TRIT_UNKNOWN;
    return TRIT_POSITIVE;
}

/**
 * @brief Kleene OR (disjunction)
 *
 * WHAT: Three-valued OR where TRUE dominates, then UNKNOWN
 * WHY:  Model uncertainty in logical disjunction
 * HOW:  max(a, b) semantics with ordering -1 < 0 < +1
 *
 * TRUTH TABLE:
 *   OR(+1, x) = +1 for all x  (TRUE dominates)
 *   OR(-1, x) = x for all x   (FALSE is identity)
 *   OR(0, 0) = 0              (UNKNOWN + UNKNOWN = UNKNOWN)
 *
 * @param a First operand
 * @param b Second operand
 * @return Disjunction result
 */
static inline trit_t trit_or(trit_t a, trit_t b) {
    if (a == TRIT_POSITIVE || b == TRIT_POSITIVE) return TRIT_POSITIVE;
    if (a == TRIT_UNKNOWN || b == TRIT_UNKNOWN) return TRIT_UNKNOWN;
    return TRIT_NEGATIVE;
}

/**
 * @brief Kleene XOR (exclusive or)
 *
 * WHAT: Three-valued XOR
 * WHY:  Detect exactly one true value
 * HOW:  XOR(a,b) = AND(OR(a,b), NOT(AND(a,b)))
 *
 * TRUTH TABLE:
 *        -1   0  +1
 *   -1   -1   0  +1
 *    0    0   0   0
 *   +1   +1   0  -1
 *
 * @param a First operand
 * @param b Second operand
 * @return XOR result
 */
static inline trit_t trit_xor(trit_t a, trit_t b) {
    /* XOR is true iff exactly one is true */
    if (a == TRIT_UNKNOWN || b == TRIT_UNKNOWN) return TRIT_UNKNOWN;
    if (a == b) return TRIT_NEGATIVE;
    return TRIT_POSITIVE;
}

/**
 * @brief Kleene NAND (negated conjunction)
 *
 * WHAT: NOT(AND(a, b))
 * WHY:  Universal gate, useful for logic synthesis
 *
 * @param a First operand
 * @param b Second operand
 * @return NAND result
 */
static inline trit_t trit_nand(trit_t a, trit_t b) {
    return trit_not(trit_and(a, b));
}

/**
 * @brief Kleene NOR (negated disjunction)
 *
 * WHAT: NOT(OR(a, b))
 * WHY:  Universal gate, useful for logic synthesis
 *
 * @param a First operand
 * @param b Second operand
 * @return NOR result
 */
static inline trit_t trit_nor(trit_t a, trit_t b) {
    return trit_not(trit_or(a, b));
}

/**
 * @brief Kleene implication (material conditional)
 *
 * WHAT: a → b = NOT(a) OR b
 * WHY:  Material implication in three-valued logic
 * HOW:  Standard definition via OR and NOT
 *
 * TRUTH TABLE (Kleene):
 *        -1   0  +1
 *   -1   +1  +1  +1  (false implies anything)
 *    0    0   0  +1  (unknown implies known-true)
 *   +1   -1   0  +1  (true implies true)
 *
 * KEY PROPERTY: FALSE → x = TRUE (principle of explosion avoided)
 *
 * @param a Antecedent
 * @param b Consequent
 * @return Implication result
 */
static inline trit_t trit_implies(trit_t a, trit_t b) {
    return trit_or(trit_not(a), b);
}

/**
 * @brief Kleene biconditional (equivalence)
 *
 * WHAT: a ↔ b = (a → b) AND (b → a)
 * WHY:  Test if two values are logically equivalent
 *
 * TRUTH TABLE:
 *        -1   0  +1
 *   -1   +1   0  -1
 *    0    0   0   0
 *   +1   -1   0  +1
 *
 * @param a First operand
 * @param b Second operand
 * @return Biconditional result
 */
static inline trit_t trit_iff(trit_t a, trit_t b) {
    return trit_and(trit_implies(a, b), trit_implies(b, a));
}

//=============================================================================
// Lukasiewicz Logic Operations (L3)
//=============================================================================

/**
 * @brief Lukasiewicz implication
 *
 * WHAT: min(1, 1 - a + b) in Lukasiewicz semantics
 * WHY:  Fuzzy reasoning with graded implication
 * HOW:  Different from Kleene: 0 → 0 = +1 (unknown implies unknown is true)
 *
 * KEY DIFFERENCE FROM KLEENE:
 * - Lukasiewicz: UNKNOWN → UNKNOWN = TRUE
 * - Kleene: UNKNOWN → UNKNOWN = UNKNOWN
 *
 * TRUTH TABLE (Lukasiewicz):
 *        -1   0  +1
 *   -1   +1  +1  +1
 *    0    0  +1  +1
 *   +1   -1   0  +1
 *
 * FUZZY INTERPRETATION:
 * Maps to fuzzy implication: I(a,b) = min(1, 1-a+b)
 * When restricted to {0, 0.5, 1} and mapped to {-1, 0, +1}
 *
 * @param a Antecedent
 * @param b Consequent
 * @return Implication result
 */
static inline trit_t trit_luk_implies(trit_t a, trit_t b) {
    /* Lukasiewicz: min(1, 1-a+b) in fuzzy [0,1] domain, mapped to trits
     * Using fuzzy mapping: fuzzy(t) = (t+1)/2, trit(f) = 2f-1
     * Derivation: result = 2 * min(1, 1 - (a+1)/2 + (b+1)/2) - 1
     *           = 2 * min(1, (2-a+b)/2) - 1
     *           = min(2, 2-a+b) - 1
     * Then clamp to trit range [-1, +1]
     */
    int val = 2 - a + b;  /* Range: [0, 4] when a,b in {-1,0,+1} */
    if (val >= 2) return TRIT_POSITIVE;
    if (val <= 0) return TRIT_NEGATIVE;
    return TRIT_UNKNOWN;
}

/**
 * @brief Lukasiewicz conjunction (t-norm)
 *
 * WHAT: max(0, a + b - 1) in Lukasiewicz semantics
 * WHY:  Strong conjunction for fuzzy reasoning
 * HOW:  More restrictive than Kleene AND
 *
 * TRUTH TABLE:
 *        -1   0  +1
 *   -1   -1  -1  -1
 *    0   -1  -1   0
 *   +1   -1   0  +1
 *
 * @param a First operand
 * @param b Second operand
 * @return Conjunction result
 */
static inline trit_t trit_luk_and(trit_t a, trit_t b) {
    /* Lukasiewicz t-norm: max(-1, a+b-1) */
    int val = a + b - 1;
    if (val >= 1) return TRIT_POSITIVE;
    if (val <= -1) return TRIT_NEGATIVE;
    return TRIT_UNKNOWN;
}

/**
 * @brief Lukasiewicz disjunction (t-conorm)
 *
 * WHAT: min(1, a + b) in Lukasiewicz semantics
 * WHY:  Bounded sum for fuzzy OR
 *
 * TRUTH TABLE:
 *        -1   0  +1
 *   -1   -1   0  +1
 *    0    0  +1  +1
 *   +1   +1  +1  +1
 *
 * @param a First operand
 * @param b Second operand
 * @return Disjunction result
 */
static inline trit_t trit_luk_or(trit_t a, trit_t b) {
    /* Lukasiewicz t-conorm: min(1, a+b+1) - adjusted for {-1,0,1} */
    int val = a + b;
    if (val >= 1) return TRIT_POSITIVE;
    if (val <= -1) return TRIT_NEGATIVE;
    return TRIT_UNKNOWN;
}

/**
 * @brief Lukasiewicz negation (same as Kleene)
 *
 * @param a Operand
 * @return Negated trit
 */
static inline trit_t trit_luk_not(trit_t a) {
    return -a;
}

/**
 * @brief Lukasiewicz equivalence
 *
 * WHAT: 1 - |a - b| in fuzzy semantics
 * WHY:  Measure equality with graded result
 *
 * TRUTH TABLE:
 *        -1   0  +1
 *   -1   +1   0  -1
 *    0    0  +1   0
 *   +1   -1   0  +1
 *
 * @param a First operand
 * @param b Second operand
 * @return Equivalence result
 */
static inline trit_t trit_luk_equiv(trit_t a, trit_t b) {
    int diff = (a > b) ? (a - b) : (b - a);
    return (trit_t)(1 - diff);
}

//=============================================================================
// Extended Operations with Confidence
//=============================================================================

/**
 * @brief AND operation with confidence propagation
 *
 * WHAT: Kleene AND with uncertainty tracking
 * WHY:  Propagate confidence through inference chains
 * HOW:  Result confidence is minimum of operand confidences
 *
 * @param a First extended operand
 * @param b Second extended operand
 * @return Extended result with propagated confidence
 */
static inline trit_extended_t trit_ext_and(trit_extended_t a, trit_extended_t b) {
    trit_extended_t result;
    result.value = trit_and(a.value, b.value);
    result.confidence = (a.confidence < b.confidence) ? a.confidence : b.confidence;
    result.uncertainty = 1.0f - result.confidence;
    result.inference_depth = (a.inference_depth > b.inference_depth ?
                              a.inference_depth : b.inference_depth) + 1;
    return result;
}

/**
 * @brief OR operation with confidence propagation
 *
 * @param a First extended operand
 * @param b Second extended operand
 * @return Extended result with propagated confidence
 */
static inline trit_extended_t trit_ext_or(trit_extended_t a, trit_extended_t b) {
    trit_extended_t result;
    result.value = trit_or(a.value, b.value);
    result.confidence = (a.confidence < b.confidence) ? a.confidence : b.confidence;
    result.uncertainty = 1.0f - result.confidence;
    result.inference_depth = (a.inference_depth > b.inference_depth ?
                              a.inference_depth : b.inference_depth) + 1;
    return result;
}

/**
 * @brief NOT operation with confidence preservation
 *
 * @param a Extended operand
 * @return Extended negation (confidence unchanged)
 */
static inline trit_extended_t trit_ext_not(trit_extended_t a) {
    trit_extended_t result;
    result.value = trit_not(a.value);
    result.confidence = a.confidence;
    result.uncertainty = a.uncertainty;
    result.inference_depth = a.inference_depth + 1;
    return result;
}

/**
 * @brief Implication with confidence propagation
 *
 * @param a Antecedent
 * @param b Consequent
 * @return Extended implication result
 */
static inline trit_extended_t trit_ext_implies(trit_extended_t a, trit_extended_t b) {
    return trit_ext_or(trit_ext_not(a), b);
}

//=============================================================================
// Aggregation Operations
//=============================================================================

/**
 * @brief Majority vote (consensus)
 *
 * WHAT: Return majority trit value from array
 * WHY:  Aggregate multiple ternary opinions
 * HOW:  Count each value, return majority; UNKNOWN if tie
 *
 * @param trits Array of trit values
 * @param count Number of values
 * @return Majority value, or UNKNOWN if no majority
 */
static inline trit_t trit_majority(const trit_t* trits, size_t count) {
    if (count == 0 || !trits) return TRIT_UNKNOWN;

    int neg_count = 0, pos_count = 0;
    for (size_t i = 0; i < count; i++) {
        if (trits[i] == TRIT_NEGATIVE) neg_count++;
        else if (trits[i] == TRIT_POSITIVE) pos_count++;
    }

    size_t half = count / 2;
    if ((size_t)pos_count > half) return TRIT_POSITIVE;
    if ((size_t)neg_count > half) return TRIT_NEGATIVE;
    return TRIT_UNKNOWN;
}

/**
 * @brief Unanimous agreement check
 *
 * WHAT: Check if all trits have same value
 * WHY:  Detect consensus
 *
 * @param trits Array of trit values
 * @param count Number of values
 * @return The unanimous value, or UNKNOWN if not unanimous
 */
static inline trit_t trit_unanimous(const trit_t* trits, size_t count) {
    if (count == 0 || !trits) return TRIT_UNKNOWN;

    trit_t first = trits[0];
    for (size_t i = 1; i < count; i++) {
        if (trits[i] != first) return TRIT_UNKNOWN;
    }
    return first;
}

/**
 * @brief All-positive check (universal quantifier)
 *
 * WHAT: Check if all trits are POSITIVE
 * WHY:  Implement "for all x: P(x)" with ternary result
 *
 * @param trits Array of trit values
 * @param count Number of values
 * @return POSITIVE if all positive, NEGATIVE if any negative, else UNKNOWN
 */
static inline trit_t trit_all(const trit_t* trits, size_t count) {
    if (count == 0 || !trits) return TRIT_POSITIVE;  /* Vacuously true */

    trit_t result = TRIT_POSITIVE;
    for (size_t i = 0; i < count; i++) {
        result = trit_and(result, trits[i]);
        if (result == TRIT_NEGATIVE) return TRIT_NEGATIVE;  /* Short-circuit */
    }
    return result;
}

/**
 * @brief Any-positive check (existential quantifier)
 *
 * WHAT: Check if any trit is POSITIVE
 * WHY:  Implement "exists x: P(x)" with ternary result
 *
 * @param trits Array of trit values
 * @param count Number of values
 * @return POSITIVE if any positive, NEGATIVE if all negative, else UNKNOWN
 */
static inline trit_t trit_any(const trit_t* trits, size_t count) {
    if (count == 0 || !trits) return TRIT_NEGATIVE;  /* Vacuously false */

    trit_t result = TRIT_NEGATIVE;
    for (size_t i = 0; i < count; i++) {
        result = trit_or(result, trits[i]);
        if (result == TRIT_POSITIVE) return TRIT_POSITIVE;  /* Short-circuit */
    }
    return result;
}

//=============================================================================
// Comparison Operations
//=============================================================================

/**
 * @brief Ternary equality comparison
 *
 * WHAT: Compare two trits for equality
 * WHY:  Basic comparison with ternary result
 *
 * @param a First operand
 * @param b Second operand
 * @return POSITIVE if equal, NEGATIVE if different, UNKNOWN if either is UNKNOWN
 */
static inline trit_t trit_eq(trit_t a, trit_t b) {
    if (a == TRIT_UNKNOWN || b == TRIT_UNKNOWN) return TRIT_UNKNOWN;
    return (a == b) ? TRIT_POSITIVE : TRIT_NEGATIVE;
}

/**
 * @brief Ternary less-than comparison
 *
 * WHAT: Compare two trits (a < b)
 * WHY:  Ordering comparison with ternary result
 *
 * @param a First operand
 * @param b Second operand
 * @return POSITIVE if a < b, NEGATIVE if a >= b, UNKNOWN if uncertain
 */
static inline trit_t trit_lt(trit_t a, trit_t b) {
    if (a == TRIT_UNKNOWN || b == TRIT_UNKNOWN) return TRIT_UNKNOWN;
    return (a < b) ? TRIT_POSITIVE : TRIT_NEGATIVE;
}

/**
 * @brief Ternary greater-than comparison
 *
 * @param a First operand
 * @param b Second operand
 * @return POSITIVE if a > b, NEGATIVE if a <= b, UNKNOWN if uncertain
 */
static inline trit_t trit_gt(trit_t a, trit_t b) {
    if (a == TRIT_UNKNOWN || b == TRIT_UNKNOWN) return TRIT_UNKNOWN;
    return (a > b) ? TRIT_POSITIVE : TRIT_NEGATIVE;
}

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_TERNARY_LOGIC_H */
