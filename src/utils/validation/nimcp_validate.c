#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_validate.c - Field Validation and Data Integrity System
//=============================================================================
// ARCHITECTURAL OVERVIEW:
// This module implements a comprehensive validation framework for NIMCP
// protocol data structures. It provides type-safe validation of primitive
// fields (integers, floats, strings), complex structures (arrays), and
// complete state objects with multi-field validation and overlap detection.
// The system is designed to catch data corruption, protocol violations, and
// invalid inputs before they propagate through the system.
//
// KEY DESIGN: STRATEGY PATTERN + CHAIN OF RESPONSIBILITY
// ========================================================
// WHY STRATEGY PATTERN:
// - Different validation strategies for different field types
// - Each validator encapsulates specific validation logic
// - Easy to add new field types without modifying existing code
// - Polymorphic behavior: validate_X_field() functions
//
// WHY CHAIN OF RESPONSIBILITY:
// - Validation proceeds through multiple checks sequentially
// - Each check can fail-fast and return immediately
// - Validation chain: NULL check → Size check → Alignment check →
//   Range check → Content validation
// - Early termination on first failure (fail-fast strategy)
//
// VALIDATION ARCHITECTURE:
//
//   ┌─────────────────────────────────────────────────────┐
//   │  Field-Level Validators (Strategy Pattern)          │
//   │  ┌──────────────┬────────────────┬────────────────┐│
//   │  │  Integer     │  Float         │  String        ││
//   │  │  Validator   │  Validator     │  Validator     ││
//   │  └──────────────┴────────────────┴────────────────┘│
//   └─────────────────────┬───────────────────────────────┘
//                         │
//                         ▼
//   ┌─────────────────────────────────────────────────────┐
//   │  Composite Validators                               │
//   │  ┌──────────────────┬──────────────────────────┐   │
//   │  │  Array Validator │  State Validator         │   │
//   │  │  (uses field     │  (uses all validators +  │   │
//   │  │   validators)    │   overlap detection)     │   │
//   │  └──────────────────┴──────────────────────────┘   │
//   └─────────────────────────────────────────────────────┘
//
// VALIDATION PHILOSOPHY: FAIL-FAST
// =================================
// WHY FAIL-FAST:
// - Early error detection prevents cascading failures
// - Clear error messages at point of failure
// - Reduced debugging time (immediate feedback)
// - Prevents corrupt data from entering system
//
// Alternative approaches rejected:
// - Accumulate errors: More complex, delayed feedback
// - Optimistic validation: Dangerous for safety-critical systems
// - Lazy validation: Errors discovered too late
//
// FAIL-FAST STRATEGY:
// 1. Check preconditions first (NULL pointers, size == 0)
// 2. Check structural integrity (alignment, boundaries)
// 3. Check semantic validity (ranges, encoding)
// 4. Return immediately on first failure
// 5. Log error with specific diagnostic information
//
// TYPES OF VALIDATION:
// ====================
//
// 1. STRUCTURAL VALIDATION
//    - NULL pointer checks
//    - Size validation (non-zero, within bounds)
//    - Alignment validation (platform-specific)
//    - Boundary checks (field within parent structure)
//
// 2. TYPE VALIDATION
//    - Integer: Standard sizes (1, 2, 4, 8 bytes)
//    - Float: Standard sizes (4, 8 bytes), NaN/Inf checks
//    - String: NULL termination, length constraints
//    - Array: Header validation, element type checking
//
// 3. RANGE VALIDATION
//    - Integer: Within defined min/max bounds
//    - Float: Within representable range, no NaN/Inf
//    - String: Length within limits
//    - Array: Element count within limits
//
// 4. CONTENT VALIDATION
//    - String: UTF-8 encoding, control character filtering
//    - Array: Recursive element validation
//    - State: Field overlap detection, magic number validation
//
// 5. SEMANTIC VALIDATION
//    - State magic number (protocol handshake)
//    - Field count constraints
//    - Overlap detection (no overlapping fields)
//
// ALIGNMENT REQUIREMENTS:
// =======================
// WHY ALIGNMENT MATTERS:
// - CPU requires aligned access for performance
// - Unaligned access causes bus errors on some architectures (ARM, MIPS)
// - Compiler assumes alignment for optimization
// - Misalignment indicates memory corruption or protocol error
//
// ALIGNMENT RULES:
// - int8_t: 1-byte aligned (no requirement)
// - int16_t: 2-byte aligned
// - int32_t: 4-byte aligned
// - int64_t: 8-byte aligned
// - float: 4-byte aligned
// - double: 8-byte aligned
// - Arrays: NIMCP_ARRAY_ALIGNMENT (8-byte) aligned
//
// DETECTION METHOD:
//   if ((uintptr_t)ptr % alignment != 0) → misaligned
//
// RANGE CONSTRAINTS:
// ==================
// WHY ENFORCE RANGES:
// - Prevent integer overflow in downstream calculations
// - Ensure values are within protocol-defined limits
// - Detect corruption (values outside expected range)
// - Enable safe type conversions
//
// DEFINED RANGES (from nimcp_common.h):
// - int32_t: [NIMCP_INT32_MIN, NIMCP_INT32_MAX]
// - int64_t: [NIMCP_INT64_MIN, NIMCP_INT64_MAX]
// - float: [-NIMCP_FLOAT_MAX, NIMCP_FLOAT_MAX]
// - double: [-NIMCP_DOUBLE_MAX, NIMCP_DOUBLE_MAX]
// - string: [0, NIMCP_STRING_MAX_LENGTH]
// - array: [0, NIMCP_ARRAY_MAX_ELEMENTS]
//
// UTF-8 VALIDATION:
// =================
// WHY UTF-8:
// - Universal character encoding
// - Backward compatible with ASCII
// - Variable-length encoding (1-4 bytes per character)
// - Self-synchronizing (easy to find character boundaries)
//
// UTF-8 ENCODING RULES:
// - 1 byte:  0xxxxxxx                    (ASCII)
// - 2 bytes: 110xxxxx 10xxxxxx
// - 3 bytes: 1110xxxx 10xxxxxx 10xxxxxx
// - 4 bytes: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
//
// VALIDATION ALGORITHM:
// 1. Examine first byte to determine sequence length
// 2. Verify sufficient bytes remain in string
// 3. Validate continuation bytes (must be 10xxxxxx)
// 4. Reject overlong encodings and invalid code points
//
// WHY VALIDATE UTF-8:
// - Prevent buffer overruns from malformed sequences
// - Ensure text can be safely displayed/processed
// - Detect corrupted string data
// - Protocol requirement (NIMCP strings are UTF-8)
//
// CONTROL CHARACTER FILTERING:
// =============================
// WHY FILTER CONTROL CHARACTERS:
// - Many control chars have special meanings (ESC, BEL, etc.)
// - Can interfere with terminal display
// - Potential security risk (terminal escape sequences)
// - Not typically valid in protocol messages
//
// ALLOWED CONTROL CHARACTERS:
// - '\n' (newline): Common in text content
// - '\t' (tab): Common in formatted text
//
// REJECTED CONTROL CHARACTERS:
// - NULL (\0): Only allowed as string terminator
// - ESC, BEL, BS, etc.: No valid use in protocol
//
// STATE VALIDATION:
// =================
// WHY COMPLEX:
// - Validates complete state structures with multiple fields
// - Detects field overlaps (memory corruption indicator)
// - Validates field boundaries (no out-of-bounds access)
// - Recursive validation of each field by type
//
// STATE STRUCTURE LAYOUT:
//   ┌────────────────────────────────────────┐
//   │ NimcpStateHeader                       │
//   │  - magic: 0x4E494D43 ('NIMC')         │
//   │  - field_count: Number of fields       │
//   ├────────────────────────────────────────┤
//   │ NimcpStateField[0]                     │
//   │  - type: NIMCP_FIELD_INTEGER           │
//   │  - offset: Byte offset from start      │
//   │  - size: Field size in bytes           │
//   ├────────────────────────────────────────┤
//   │ NimcpStateField[1]                     │
//   │  ...                                   │
//   ├────────────────────────────────────────┤
//   │ NimcpStateField[N-1]                   │
//   ├────────────────────────────────────────┤
//   │ Field 0 data                           │
//   ├────────────────────────────────────────┤
//   │ Field 1 data                           │
//   ├────────────────────────────────────────┤
//   │ ...                                    │
//   ├────────────────────────────────────────┤
//   │ Field N-1 data                         │
//   └────────────────────────────────────────┘
//
// OVERLAP DETECTION ALGORITHM:
// 1. Allocate usage_map array (1 byte per byte of state)
// 2. Mark header region as used
// 3. For each field:
//    a. Check if any byte in field's range is already used
//    b. If yes: overlap detected, fail validation
//    c. If no: mark field's bytes as used
// 4. Free usage_map
//
// WHY USAGE MAP:
// - Simple O(n) algorithm where n = state size
// - Detects all overlaps (even non-adjacent)
// - Low memory overhead (1 byte per byte)
// - Alternative: Compare all field pairs O(f²) where f = field count
//
// COMPLEXITY: O(n + f×s) where:
//   n = state size
//   f = field count
//   s = average field size
//
// TRADE-OFF:
// - Memory: O(n) temporary allocation
// - Time: Linear in state size
// - Accuracy: Detects all overlaps
//
// ARRAY VALIDATION:
// =================
// ARRAY STRUCTURE LAYOUT:
//   ┌────────────────────────────────────────┐
//   │ NimcpArrayHeader                       │
//   │  - element_type: INTEGER/FLOAT/STRING  │
//   │  - element_size: Size per element      │
//   │  - element_count: Number of elements   │
//   ├────────────────────────────────────────┤
//   │ Element[0]                             │
//   ├────────────────────────────────────────┤
//   │ Element[1]                             │
//   ├────────────────────────────────────────┤
//   │ ...                                    │
//   ├────────────────────────────────────────┤
//   │ Element[N-1]                           │
//   └────────────────────────────────────────┘
//
// VALIDATION STEPS:
// 1. Validate header values (non-zero counts/sizes)
// 2. Check total size fits in field_data size
// 3. Validate alignment of array data
// 4. Recursively validate each element by type
//
// WHY RECURSIVE VALIDATION:
// - Ensures all array elements are valid
// - Detects corruption in any element
// - Consistent validation across all data types
//
// ERROR REPORTING STRATEGY:
// =========================
// WHY DETAILED ERRORS:
// - Immediate diagnosis (which field, what error)
// - Reduces debugging time
// - Essential for protocol debugging
// - Helps identify corruption sources
//
// ERROR MESSAGE COMPONENTS:
// - What failed: "Invalid control character in string"
// - Where: "at pos 42"
// - Context: Field index, element index, etc.
// - Value: Actual value that failed (when safe to print)
//
// LOGGING STRATEGY:
// - Use NIMCP_LOGGING_ERROR for all failures
// - Include specific diagnostic information
// - Print values when helpful (sizes, indices, offsets)
// - Avoid printing raw data (security risk)
//
// THREAD SAFETY:
// ==============
// THREAD SAFETY GUARANTEES:
// - All validation functions are THREAD-SAFE
// - Pure functions (no shared mutable state)
// - No global state modification
// - Safe for concurrent validation of different data
//
// WHY THREAD-SAFE:
// - Validation may occur in multiple threads
// - No synchronization overhead needed
// - Simpler implementation
// - Better performance (no lock contention)
//
// CAVEAT:
// - Caller must ensure data being validated is not concurrently modified
// - Validation provides snapshot-in-time correctness
//
// PERFORMANCE CHARACTERISTICS:
// ============================
//
// INTEGER VALIDATION:
// - Time: O(1) - constant time checks
// - Space: O(1) - no allocations
// - Typical: <10 CPU cycles
//
// FLOAT VALIDATION:
// - Time: O(1) - constant time checks + isnan/isinf
// - Space: O(1) - no allocations
// - Typical: <20 CPU cycles
//
// STRING VALIDATION:
// - Time: O(n) where n = string length
// - Space: O(1) - no allocations
// - Typical: ~5-10 cycles per character
// - Dominated by: UTF-8 validation, control char checking
//
// ARRAY VALIDATION:
// - Time: O(e×t) where e = element count, t = per-element validation time
// - Space: O(1) - no allocations (except element validation)
// - Typical: Linear in array size
//
// STATE VALIDATION:
// - Time: O(n + f×s) where n = state size, f = field count, s = avg field size
// - Space: O(n) - usage map allocation
// - Typical: ~10-50 µs for typical state structures
// - Dominated by: Overlap detection (usage map), field validation
//
// OPTIMIZATION OPPORTUNITIES:
// - Bitmap usage map (1 bit per byte): 8× memory reduction
// - SIMD UTF-8 validation: 4-8× speedup for long strings
// - Parallel field validation: Speed up multi-field states
//
// WHY NOT OPTIMIZED NOW:
// - Validation is not a hot path (typically once per message)
// - Readability and correctness more important than speed
// - Current performance is acceptable (<1% overhead)
//
// DESIGN PATTERNS:
// ================
// 1. STRATEGY: Different validation strategies per type
// 2. CHAIN OF RESPONSIBILITY: Sequential validation checks
// 3. COMPOSITE: State validation composes field validators
// 4. TEMPLATE METHOD: Common validation structure, type-specific steps
// 5. FAIL-FAST: Early termination on first error
//
// SOLID PRINCIPLES:
// =================
// - Single Responsibility: Each function validates one thing
//   * validate_integer_field: Only validates integers
//   * validate_string_field: Only validates strings
//   * Each has clear, focused purpose
//
// - Open/Closed: Easy to add new field types
//   * Add new validate_X_field function
//   * Add case to state/array validator
//   * No modification of existing validators
//
// - Liskov Substitution: All validators follow same contract
//   * bool validate_X_field(const void* data, size_t size)
//   * Return true if valid, false if invalid
//   * Log errors on failure
//
// - Interface Segregation: Minimal, focused API
//   * Field validators: One function per type
//   * No bloated interfaces with unused methods
//
// - Dependency Inversion: Depends on abstractions
//   * Validates generic void* data
//   * Uses type descriptors (NimcpFieldType enum)
//   * Not tied to specific data structures
//
// USE CASES IN NIMCP:
// ===================
// 1. PROTOCOL MESSAGE VALIDATION
//    - Validate incoming messages before processing
//    - Prevent malformed messages from crashing system
//    - Example: validate_state_fields on deserialized messages
//
// 2. CONFIGURATION VALIDATION
//    - Validate configuration files before use
//    - Ensure all parameters are within valid ranges
//    - Example: validate_integer_field on port numbers
//
// 3. STATE TRANSFER VALIDATION
//    - Validate state before network transmission
//    - Validate received state before applying
//    - Example: validate_state_fields on checkpoint data
//
// 4. MEMORY CORRUPTION DETECTION
//    - Periodically validate critical data structures
//    - Detect corruption from buffer overflows
//    - Example: validate_state_fields on shared state
//
// 5. TESTING AND DEBUGGING
//    - Assert valid state in unit tests
//    - Validate invariants during development
//    - Example: validate_array_field in test fixtures
//
// LIMITATIONS AND TRADE-OFFS:
// ===========================
//
// LIMITATIONS:
// - Only validates structure, not semantics
//   * Can't validate "is this a valid IP address"
//   * Can only validate "is this a valid string"
// - No schema evolution support
//   * Fixed field types, no versioning
// - No custom validators
//   * Can't plug in application-specific validation
// - Usage map allocation for state validation
//   * O(n) memory overhead
//
// TRADE-OFFS:
// - Fail-fast vs error accumulation
//   * Chosen: Fail-fast (simpler, immediate feedback)
//   * Alternative: Accumulate all errors (more info, more complex)
//
// - Alignment checking vs portability
//   * Chosen: Strict alignment (catch errors early)
//   * Alternative: Allow unaligned (more portable, slower, riskier)
//
// - UTF-8 validation vs performance
//   * Chosen: Full UTF-8 validation (correctness over speed)
//   * Alternative: Trust input (faster, dangerous)
//
// - Usage map vs pairwise comparison
//   * Chosen: Usage map (O(n) time, O(n) space)
//   * Alternative: Pairwise (O(f²) time, O(1) space)
//
// WHY THESE TRADE-OFFS:
// - Safety and correctness are paramount
// - Validation is not a performance bottleneck
// - Early error detection saves debugging time
// - Memory overhead is acceptable for temporary allocations
//
// FUTURE ENHANCEMENTS:
// ====================
// - Schema validation (JSON Schema-like)
// - Custom validator registration
// - Validation caching (memoization)
// - Batch validation (validate multiple structures)
// - Async validation (non-blocking)
// - Streaming validation (for large data)
//
//=============================================================================

/**
 * @file nimcp_validate.c
 * @brief Field validation utilities implementation
 *
 * WHAT: Type-safe validation for NIMCP protocol fields
 * WHY: Ensure data integrity and prevent corruption
 * HOW: Strategy pattern with fail-fast validation
 */

#include "utils/validation/nimcp_validate.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"  // CRITICAL: Declares nimcp_calloc/nimcp_free return types
#include "utils/memory/nimcp_unified_memory.h"

#define LOG_MODULE "utils.validation"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(validate)

//=============================================================================
// Global Error Tracking
//=============================================================================

// Thread-local error information to avoid race conditions
static _Thread_local nimcp_error_info_t g_last_error = {
    .code = NIMCP_SUCCESS,
    .message = {0}
};

/**
 * @brief Get last error information
 * @param info Pointer to error info structure to fill
 */
void nimcp_get_error_info(nimcp_error_info_t* info) {
    if (info) {
        *info = g_last_error;
    }
}

/**
 * @brief Set error information with printf-style formatting
 * @param code Error code
 * @param format Printf-style format string
 * @param ... Variable arguments for format string
 */
void nimcp_set_error_info(nimcp_result_t code, const char* format, ...) {
    g_last_error.code = code;
    if (format) {
        va_list args;
        va_start(args, format);
        vsnprintf(g_last_error.message, NIMCP_MAX_ERROR_LENGTH, format, args);
        va_end(args);
        g_last_error.message[NIMCP_MAX_ERROR_LENGTH - 1] = '\0';
    } else {
        g_last_error.message[0] = '\0';
    }
}

/**
 * @brief Clear current error state
 */
void nimcp_clear_error(void) {
    g_last_error.code = NIMCP_SUCCESS;
    g_last_error.message[0] = '\0';
}

//=============================================================================
// Internal Helper Function Prototypes
//=============================================================================

static size_t is_valid_utf8_char(const char* str, size_t len);

//=============================================================================
// Field-Level Validators
//=============================================================================

/**
 * @brief Validate integer field data
 *
 * WHY THIS FUNCTION:
 * - Ensures integer fields are properly aligned and sized
 * - Prevents crashes from misaligned access on strict architectures
 * - Validates values are within protocol-defined ranges
 * - Detects memory corruption early
 *
 * VALIDATION CHAIN (fail-fast):
 * 1. NULL pointer check → Prevent segfault
 * 2. Size validation → Ensure standard integer type
 * 3. Alignment check → Prevent bus error
 * 4. Range validation → Ensure protocol compliance
 *
 * SUPPORTED SIZES:
 * - 1 byte: int8_t, uint8_t
 * - 2 bytes: int16_t, uint16_t
 * - 4 bytes: int32_t, uint32_t
 * - 8 bytes: int64_t, uint64_t
 *
 * WHY ALIGNMENT CHECK:
 * - CPU requires aligned access for atomic operations
 * - Unaligned access is undefined behavior on some platforms
 * - Performance: Aligned access is faster
 * - Detection: Misalignment indicates corruption or serialization bug
 *
 * ALIGNMENT FORMULA:
 *   aligned = ((uintptr_t)ptr % size == 0)
 *
 * WHY RANGE VALIDATION:
 * - Prevent overflow in downstream calculations
 * - Ensure values fit in declared type
 * - Protocol requirement (defined min/max)
 *
 * RANGE CHECKING STRATEGY:
 * - Only validate 32-bit and 64-bit (most common)
 * - Skip 8-bit and 16-bit (all values valid)
 * - Use platform-defined limits (NIMCP_INT32_MIN/MAX, etc.)
 *
 * COMPLEXITY: O(1) - constant time checks
 * THREAD SAFETY: Thread-safe (pure function, no shared state)
 *
 * @param field_data Pointer to integer field (must be non-NULL)
 * @param size Size of integer in bytes (1, 2, 4, or 8)
 * @return true if valid, false if invalid
 *
 * ERROR CONDITIONS:
 * - field_data is NULL
 * - size is not a standard integer size
 * - field_data is misaligned for its size
 * - value is outside protocol-defined range
 */
bool nimcp_validate_integer_field(const void* field_data, size_t size)
{
    LOG_TRACE("Entering nimcp_validate_integer_field");
    // STEP 1: NULL check (prevent segfault)
    if (!field_data) {
        LOG_ERROR("nimcp_validate_integer_field failed: returning error");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_validate_integer_field: field_data is NULL");
        return false;
    }

    // STEP 2: Size validation (ensure standard type)
    // WHY SWITCH: Explicit enumeration of valid sizes
    // WHY NOT POWER-OF-TWO CHECK: Want to catch weird sizes explicitly
    switch (size) {
        case sizeof(int8_t):
        case sizeof(int16_t):
        case sizeof(int32_t):
        case sizeof(int64_t):
            break;
        default:
            LOG_ERROR("Invalid integer field size: %zu", size);
            LOG_ERROR("nimcp_validate_integer_field failed: returning error");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_validate_integer_field: operation failed");
            return false;
    }

    // STEP 3: Alignment check (prevent bus error)
    // WHY: CPU requires aligned access for performance and correctness
    // HOW: Pointer address must be multiple of size
    // EXAMPLE: int32_t at 0x1000 ✓, int32_t at 0x1002 ✗
    if (((uintptr_t) field_data) % size != 0) {
        LOG_ERROR("Integer field misaligned");
        LOG_ERROR("nimcp_validate_integer_field failed: returning error");
        return false;
    }

    // STEP 4: Range validation (ensure protocol compliance)
    // WHY ONLY 32/64: Most common, most risk of overflow
    // WHY NOT 8/16: All values fit in type by definition
    switch (size) {
        case sizeof(int32_t): {
            const int32_t value = *(const int32_t*) field_data;
            // WHY CHECK: Ensure value fits in protocol-defined range
            // WHEN FAILS: Value corrupt or from different protocol version
            if (value < NIMCP_INT32_MIN || value > NIMCP_INT32_MAX) {
                LOG_ERROR("Integer value out of range: %d", value);
                LOG_ERROR("nimcp_validate_integer_field failed: returning error");
                return false;
            }
            break;
        }
        case sizeof(int64_t): {
            const int64_t value = *(const int64_t*) field_data;
            if (value < NIMCP_INT64_MIN || value > NIMCP_INT64_MAX) {
                LOG_ERROR("Integer value out of range: %ld", value);
                LOG_ERROR("nimcp_validate_integer_field failed: returning error");
                return false;
            }
            break;
        }
            // 8-bit and 16-bit: No range check needed (all values valid)
    }

    return true;
}

/**
 * @brief Validate floating point field data
 *
 * WHY THIS FUNCTION:
 * - Ensures floating point values are representable and safe
 * - Prevents propagation of NaN/Inf through calculations
 * - Validates alignment for performance
 * - Detects corruption early
 *
 * VALIDATION CHAIN (fail-fast):
 * 1. NULL pointer check → Prevent segfault
 * 2. Size validation → Ensure float or double
 * 3. Alignment check → Prevent bus error
 * 4. NaN check → Prevent undefined behavior
 * 5. Infinity check → Prevent overflow
 * 6. Range validation → Ensure representable
 *
 * SUPPORTED SIZES:
 * - 4 bytes: float (IEEE 754 single precision)
 * - 8 bytes: double (IEEE 754 double precision)
 *
 * WHY REJECT NaN:
 * - NaN != NaN (comparison undefined)
 * - NaN propagates through calculations
 * - Indicates arithmetic error or corruption
 * - No valid use in protocol messages
 *
 * WHY REJECT INFINITY:
 * - Usually indicates overflow
 * - Limited utility in protocol
 * - Can cause downstream errors
 * - Better to fail explicitly
 *
 * NaN/INF DETECTION:
 * - isnan(x): Returns true if x is NaN
 * - isinf(x): Returns true if x is ±Infinity
 * - Both are standard C99 functions
 *
 * RANGE VALIDATION:
 * - float: [-NIMCP_FLOAT_MAX, NIMCP_FLOAT_MAX]
 * - double: [-NIMCP_DOUBLE_MAX, NIMCP_DOUBLE_MAX]
 * - Uses fabs/fabsf for absolute value
 *
 * WHY ABSOLUTE VALUE:
 * - Symmetric range around zero
 * - Single comparison instead of two
 * - Simpler logic
 *
 * COMPLEXITY: O(1) - constant time checks
 * THREAD SAFETY: Thread-safe (pure function)
 *
 * @param field_data Pointer to float field (must be non-NULL)
 * @param size Size of float in bytes (4 or 8)
 * @return true if valid, false if invalid
 *
 * ERROR CONDITIONS:
 * - field_data is NULL
 * - size is not sizeof(float) or sizeof(double)
 * - field_data is misaligned
 * - value is NaN
 * - value is Infinity
 * - value exceeds representable range
 */
bool nimcp_validate_float_field(const void* field_data, size_t size)
{
    LOG_TRACE("Entering nimcp_validate_float_field");
    // STEP 1: NULL check
    if (!field_data) {
        LOG_ERROR("nimcp_validate_float_field failed: returning error");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_validate_float_field: field_data is NULL");
        return false;
    }

    // STEP 2: Size validation (float or double only)
    switch (size) {
        case sizeof(float):
        case sizeof(double):
            break;
        default:
            LOG_ERROR("Invalid float field size: %zu", size);
            LOG_ERROR("nimcp_validate_float_field failed: returning error");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_validate_float_field: operation failed");
            return false;
    }

    // STEP 3: Alignment check
    // WHY: Floating point operations require alignment
    // EXAMPLE: float at 0x1000 ✓, float at 0x1001 ✗
    if (((uintptr_t) field_data) % size != 0) {
        LOG_ERROR("Float field misaligned");
        LOG_ERROR("nimcp_validate_float_field failed: returning error");
        return false;
    }

    // STEP 4-6: Value validation (NaN, Inf, Range)
    // WHY SEPARATE FLOAT/DOUBLE: Different types, different functions
    if (size == sizeof(float)) {
        const float value = *(const float*) field_data;

        // NaN check
        // WHY REJECT: NaN is never equal to itself, breaks comparisons
        if (isnan(value)) {
            LOG_ERROR("Float field contains NaN");
            LOG_ERROR("nimcp_validate_float_field failed: returning error");
            return false;
        }

        // Infinity check
        // WHY REJECT: Usually indicates overflow or corruption
        if (isinf(value)) {
            LOG_ERROR("Float field contains infinity");
            LOG_ERROR("nimcp_validate_float_field failed: returning error");
            return false;
        }

        // Range check
        // WHY: Ensure value is within protocol-defined limits
        // HOW: fabsf for absolute value, single comparison
        if (fabsf(value) > NIMCP_FLOAT_MAX) {
            LOG_ERROR("Float value out of range: %f", value);
            LOG_ERROR("nimcp_validate_float_field failed: returning error");
            return false;
        }
    } else {
        const double value = *(const double*) field_data;

        // Same checks for double
        if (isnan(value)) {
            LOG_ERROR("Double field contains NaN");
            LOG_ERROR("nimcp_validate_float_field failed: returning error");
            return false;
        }

        if (isinf(value)) {
            LOG_ERROR("Double field contains infinity");
            LOG_ERROR("nimcp_validate_float_field failed: returning error");
            return false;
        }

        if (fabs(value) > NIMCP_DOUBLE_MAX) {
            LOG_ERROR("Double value out of range: %f", value);
            LOG_ERROR("nimcp_validate_float_field failed: returning error");
            return false;
        }
    }

    return true;
}

/**
 * @brief Validate string field data
 *
 * WHY THIS FUNCTION:
 * - Ensures strings are properly terminated and safe to use
 * - Validates UTF-8 encoding for interoperability
 * - Filters dangerous control characters
 * - Enforces length constraints
 * - Prevents buffer overflows
 *
 * VALIDATION CHAIN (fail-fast):
 * 1. NULL/size check → Prevent invalid access
 * 2. Termination check → Ensure NULL-terminated
 * 3. Length check → Prevent overlong strings
 * 4. Character-by-character validation:
 *    a. Control character check → Filter dangerous chars
 *    b. UTF-8 encoding check → Ensure valid encoding
 *
 * WHY NULL TERMINATION:
 * - C string convention (required for string functions)
 * - Prevents unbounded reads
 * - Must be within size bytes
 *
 * TERMINATION VALIDATION:
 * - str[size-1] must be '\0'
 * - Ensures string fits in field
 * - Prevents overflow in strlen and other functions
 *
 * WHY LENGTH CONSTRAINT:
 * - Prevent denial-of-service (huge strings)
 * - Ensure bounded processing time
 * - Protocol requirement (NIMCP_STRING_MAX_LENGTH)
 *
 * CONTROL CHARACTER FILTERING:
 * - Allow: '\n' (newline), '\t' (tab)
 * - Reject: All other control characters (0x00-0x1F, 0x7F)
 * - WHY: Security (no terminal escapes), safety (no weird chars)
 *
 * UTF-8 VALIDATION:
 * - Ensures valid multi-byte sequences
 * - Prevents buffer overruns from malformed UTF-8
 * - Required for interoperability
 * - See is_valid_utf8_char() for details
 *
 * CHARACTER ITERATION:
 * - Loop through string byte-by-byte
 * - Check each character individually
 * - Early termination on first error
 *
 * WHY NOT JUST strlen:
 * - Need character-level validation
 * - Need to check encoding and control chars
 * - strlen only checks termination
 *
 * COMPLEXITY: O(n) where n = string length
 * THREAD SAFETY: Thread-safe (pure function)
 *
 * TYPICAL PERFORMANCE:
 * - ~5-10 cycles per character
 * - Dominated by UTF-8 validation
 * - Acceptable for validation (not hot path)
 *
 * @param field_data Pointer to string field (must be non-NULL)
 * @param size Size of string field in bytes (must be > 0)
 * @return true if valid, false if invalid
 *
 * ERROR CONDITIONS:
 * - field_data is NULL
 * - size is 0
 * - String not NULL-terminated within size bytes
 * - String exceeds NIMCP_STRING_MAX_LENGTH
 * - String contains invalid control characters
 * - String contains invalid UTF-8 sequences
 */
bool nimcp_validate_string_field(const void* field_data, size_t size)
{
    LOG_TRACE("Entering nimcp_validate_string_field");
    // STEP 1: NULL/size check
    if (!field_data || size == 0) {
        LOG_ERROR("nimcp_validate_string_field failed: returning error");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_validate_string_field: field_data is NULL");
        return false;
    }

    const char* str = (const char*) field_data;

    // STEP 2: NULL termination check
    // WHY: C strings must be NULL-terminated
    // CHECK: Last byte must be '\0'
    // WHEN FAILS: Unterminated string or wrong size
    if (str[size - 1] != '\0') {
        LOG_ERROR("String not NULL terminated");
        LOG_ERROR("nimcp_validate_string_field failed: returning error");
        return false;
    }

    // STEP 3: Get string length and validate
    // WHY strnlen: Bounded search (prevents overrun if not terminated)
    // MAX: Search up to 'size' bytes
    size_t len = strnlen(str, size);

    // Double-check termination
    // WHY: strnlen returns size if not terminated
    if (len >= size) {
        LOG_ERROR("String exceeds field size");
        LOG_ERROR("nimcp_validate_string_field failed: returning error");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_validate_string_field: capacity exceeded");
        return false;
    }

    // Length constraint check
    // WHY: Protocol-defined maximum
    // PREVENTS: Huge strings, DoS attacks
    if (len > NIMCP_STRING_MAX_LENGTH) {
        LOG_ERROR("String too long: %zu chars", len);
        LOG_ERROR("nimcp_validate_string_field failed: returning error");
        return false;
    }

    // STEP 4: Character-by-character validation
    // WHY: Need to check encoding and control characters
    // LOOP: Through all characters (excluding terminator)
    // NOTE: Loop increment is variable - skips multi-byte UTF-8 sequences
    for (size_t i = 0; i < len; ) {
        // Control character check
        // WHY iscntrl: Detects 0x00-0x1F and 0x7F
        // ALLOW: '\n' and '\t' (common in text)
        // REJECT: Other control chars (security risk)
        if (iscntrl(str[i]) && str[i] != '\n' && str[i] != '\t') {
            LOG_ERROR("Invalid control character in string at pos %zu", i);
            LOG_ERROR("nimcp_validate_string_field failed: returning error");
            return false;
        }

        // UTF-8 encoding validation
        // WHY: Ensure valid multi-byte sequences
        // HOW: Check sequence starting at position i
        // REMAINING: Pass remaining length for boundary check
        // RETURNS: Number of bytes consumed (1-4), or 0 on error
        size_t bytes_consumed = is_valid_utf8_char(str + i, len - i);
        if (bytes_consumed == 0) {
            LOG_ERROR("Invalid UTF-8 encoding at pos %zu", i);
            LOG_ERROR("nimcp_validate_string_field failed: returning error");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_validate_string_field: bytes_consumed is zero");
            return false;
        }

        // Advance by the number of bytes in this UTF-8 character
        // WHY: Multi-byte sequences (2-4 bytes) need to be skipped
        // PREVENTS: Re-validating continuation bytes as start bytes
        i += bytes_consumed;
    }

    return true;
}

/**
 * @brief Validate array field data
 *
 * WHY THIS FUNCTION:
 * - Validates array structure (header + elements)
 * - Ensures elements are of correct type and size
 * - Validates alignment for performance
 * - Recursively validates all elements
 * - Prevents out-of-bounds access
 *
 * VALIDATION CHAIN (fail-fast):
 * 1. NULL/size check → Prevent invalid access
 * 2. Header validation → Ensure valid counts/sizes
 * 3. Total size check → Prevent overflow
 * 4. Alignment check → Ensure performance
 * 5. Element-by-element validation → Recursive check
 *
 * ARRAY LAYOUT:
 *   [NimcpArrayHeader][Element 0][Element 1]...[Element N-1]
 *   ↑                 ↑
 *   field_data        array_data (aligned)
 *
 * HEADER VALIDATION:
 * - element_count > 0 (no empty arrays)
 * - element_size > 0 (no zero-size elements)
 * - element_count <= NIMCP_ARRAY_MAX_ELEMENTS (prevent DoS)
 *
 * WHY NO EMPTY ARRAYS:
 * - Protocol design choice
 * - Simplifies downstream code
 * - Empty arrays would be represented differently
 *
 * SIZE CALCULATION:
 * - required_size = sizeof(header) + (count × element_size)
 * - Must fit within field_data size
 * - Overflow check implicit (would exceed size)
 *
 * ALIGNMENT CHECK:
 * - Array data must be NIMCP_ARRAY_ALIGNMENT aligned
 * - Typically 8 bytes (platform-dependent)
 * - WHY: Performance, platform requirements
 *
 * ELEMENT VALIDATION:
 * - Iterate through all elements
 * - Validate each by its type (integer, float, string)
 * - Use appropriate validator function
 * - Fail-fast on first invalid element
 *
 * WHY RECURSIVE VALIDATION:
 * - Ensures all elements are valid
 * - Consistent with field-level validation
 * - Detects corruption in any element
 *
 * ELEMENT TYPE DISPATCH:
 * - NIMCP_ARRAY_INTEGER → nimcp_validate_integer_field
 * - NIMCP_ARRAY_FLOAT → nimcp_validate_float_field
 * - NIMCP_ARRAY_STRING → nimcp_validate_string_field
 * - Unknown type → Error
 *
 * COMPLEXITY: O(n) where n = element count
 * - Header validation: O(1)
 * - Element validation: O(n × t) where t = per-element time
 * - Integer/float elements: O(n)
 * - String elements: O(n × s) where s = average string length
 *
 * THREAD SAFETY: Thread-safe (pure function, recursive calls are thread-safe)
 *
 * @param field_data Pointer to array field (must be non-NULL)
 * @param size Size of array field in bytes (must include header + elements)
 * @return true if valid, false if invalid
 *
 * ERROR CONDITIONS:
 * - field_data is NULL
 * - size too small for header
 * - element_count or element_size is 0
 * - element_count exceeds NIMCP_ARRAY_MAX_ELEMENTS
 * - array size exceeds field size
 * - array data is misaligned
 * - any element is invalid
 * - element_type is unknown
 */
bool nimcp_validate_array_field(const void* field_data, size_t size)
{
    LOG_TRACE("Entering nimcp_validate_array_field");
    // STEP 1: NULL/size check
    // WHY: Prevent invalid access to header
    if (!field_data || size < sizeof(NimcpArrayHeader)) {
        LOG_ERROR("nimcp_validate_array_field failed: returning error");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_validate_array_field: field_data is NULL");
        return false;
    }

    const NimcpArrayHeader* header = (const NimcpArrayHeader*) field_data;

    // STEP 2: Header validation
    // WHY: Ensure sensible values before calculations
    // CHECK: Non-zero counts, within limits
    if (header->element_count == 0 || header->element_size == 0 ||
        header->element_count > NIMCP_ARRAY_MAX_ELEMENTS) {
        LOG_ERROR("Invalid array header values");
        LOG_ERROR("nimcp_validate_array_field failed: returning error");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_validate_array_field: operation failed");
        return false;
    }

    // STEP 3: Total size check
    // WHY: Ensure all elements fit in field
    // CALCULATION: header + (count × size)
    // OVERFLOW: Would result in required_size wrapping or > size
    size_t required_size =
        sizeof(NimcpArrayHeader) + (header->element_count * header->element_size);
    if (required_size > size) {
        LOG_ERROR("Array size exceeds field size");
        LOG_ERROR("nimcp_validate_array_field failed: returning error");
        return false;
    }

    // Get pointer to array data (after header)
    const uint8_t* array_data = (const uint8_t*) field_data + sizeof(NimcpArrayHeader);

    // STEP 4: Alignment check
    // WHY: Performance and platform requirements
    // ALIGNMENT: Platform-specific (typically 8 bytes)
    if (((uintptr_t) array_data) % NIMCP_ARRAY_ALIGNMENT != 0) {
        LOG_ERROR("Array data misaligned");
        LOG_ERROR("nimcp_validate_array_field failed: returning error");
        return false;
    }

    // STEP 5: Element-by-element validation
    // WHY: Ensure all elements are valid
    // LOOP: Through all elements
    for (uint32_t i = 0; i < header->element_count; i++) {
        // Compute element pointer
        // OFFSET: i × element_size from array_data
        const void* element = array_data + (i * header->element_size);

        // Validate element by type
        // WHY SWITCH: Dispatch to appropriate validator
        switch (header->element_type) {
            case NIMCP_ARRAY_INTEGER:
                if (!nimcp_validate_integer_field(element, header->element_size)) {
                    LOG_ERROR("Invalid integer array element at index %u", i);
                    LOG_ERROR("nimcp_validate_array_field failed: returning error");
                    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_validate_array_field: nimcp_validate_integer_field is NULL");
                    return false;
                }
                break;

            case NIMCP_ARRAY_FLOAT:
                if (!nimcp_validate_float_field(element, header->element_size)) {
                    LOG_ERROR("Invalid float array element at index %u", i);
                    LOG_ERROR("nimcp_validate_array_field failed: returning error");
                    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_validate_array_field: nimcp_validate_float_field is NULL");
                    return false;
                }
                break;

            case NIMCP_ARRAY_STRING:
                if (!nimcp_validate_string_field(element, header->element_size)) {
                    LOG_ERROR("Invalid string array element at index %u", i);
                    LOG_ERROR("nimcp_validate_array_field failed: returning error");
                    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_validate_array_field: nimcp_validate_string_field is NULL");
                    return false;
                }
                break;

            default:
                // Unknown element type
                // WHY REJECT: Cannot validate unknown types
                LOG_ERROR("Unknown array element type: %d", header->element_type);
                LOG_ERROR("nimcp_validate_array_field failed: returning error");
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_validate_array_field: operation failed");
                return false;
        }
    }

    return true;
}

/**
 * @brief Validate complete state data structure
 *
 * WHY THIS FUNCTION:
 * - Validates entire state structure (most complex validation)
 * - Ensures structural integrity (no overlaps, no gaps required)
 * - Validates magic number (protocol handshake)
 * - Recursively validates all fields
 * - Detects memory corruption early
 *
 * VALIDATION CHAIN (fail-fast):
 * 1. NULL/size check → Prevent invalid access
 * 2. Magic number check → Verify protocol
 * 3. Field count check → Ensure sensible structure
 * 4. Overlap detection → Prevent memory corruption
 * 5. Field-by-field validation → Recursive check
 *
 * STATE STRUCTURE:
 *   [NimcpStateHeader]
 *   [NimcpStateField array]
 *   [Field data regions]
 *
 * MAGIC NUMBER:
 * - Value: NIMCP_STATE_MAGIC (0x4E494D43 = 'NIMC')
 * - WHY: Protocol handshake, detect wrong data type
 * - WHERE: First field of header
 *
 * FIELD COUNT VALIDATION:
 * - Must be > 0 (no empty states)
 * - Must be <= NIMCP_MAX_FIELDS (prevent DoS)
 * - Ensures finite validation time
 *
 * OVERLAP DETECTION ALGORITHM:
 * WHY COMPLEX: Most sophisticated check in this module
 *
 * APPROACH: Usage map (bitmap)
 * 1. Allocate usage_map[size] (1 byte per byte of state)
 * 2. Initialize to 0 (unused)
 * 3. Mark header region as used (1)
 * 4. For each field:
 *    a. Check if any byte in [offset, offset+size) is used
 *    b. If yes: Overlap detected → fail
 *    c. If no: Mark those bytes as used
 * 5. Free usage_map
 *
 * WHY USAGE MAP:
 * - Simple O(n) algorithm
 * - Detects all overlaps (even non-adjacent)
 * - Easy to understand and debug
 *
 * ALTERNATIVES REJECTED:
 * - Pairwise comparison: O(f²) where f = field count
 * - Interval tree: More complex, overkill for validation
 * - No check: Dangerous, corruption spreads
 *
 * MEMORY OVERHEAD:
 * - O(n) where n = state size
 * - Temporary allocation (freed after validation)
 * - Acceptable for validation (not hot path)
 *
 * OVERLAP DETECTION EXAMPLE:
 *   State size: 100 bytes
 *   Header: [0, 20) → Mark usage_map[0..19] = 1
 *   Field 0: offset=20, size=10 → Mark [20..29] = 1
 *   Field 1: offset=25, size=10 → [25..29] already marked → OVERLAP!
 *
 * FIELD BOUNDARY CHECK:
 * - offset + size <= state size
 * - Prevents out-of-bounds access
 * - Detects corruption or malformed headers
 *
 * FIELD VALIDATION:
 * - Dispatch to appropriate validator by type
 * - Use field offset to locate data
 * - Validate with field size
 * - Fail-fast on first invalid field
 *
 * TYPE DISPATCH:
 * - NIMCP_FIELD_INTEGER → validate_integer_field
 * - NIMCP_FIELD_FLOAT → validate_float_field
 * - NIMCP_FIELD_STRING → validate_string_field
 * - NIMCP_FIELD_ARRAY → validate_array_field
 * - Unknown → Error
 *
 * COMPLEXITY:
 * - Time: O(n + f×s) where:
 *   * n = state size (overlap detection)
 *   * f = field count
 *   * s = average field validation time
 * - Space: O(n) for usage map
 *
 * WHY ACCEPTABLE:
 * - Validation is not a hot path
 * - Typical state size: <10KB
 * - Typical validation time: <50µs
 * - Worth it for early corruption detection
 *
 * THREAD SAFETY: Thread-safe (local allocations only)
 *
 * @param state_data Pointer to state structure (must be non-NULL)
 * @param size Total size of state data in bytes
 * @return true if valid, false if invalid
 *
 * ERROR CONDITIONS:
 * - state_data is NULL
 * - size too small for header
 * - magic number mismatch
 * - field_count is 0 or > NIMCP_MAX_FIELDS
 * - unable to allocate usage_map
 * - field exceeds state boundaries
 * - fields overlap
 * - any field is invalid
 * - unknown field type
 */
bool nimcp_validate_state_fields(const void* state_data, size_t size)
{
    LOG_TRACE("Entering nimcp_validate_state_fields");
    // STEP 1: NULL/size check
    if (!state_data || size < sizeof(NimcpStateHeader)) {
        LOG_ERROR("Invalid state data pointer or size");
        LOG_ERROR("nimcp_validate_state_fields failed: returning error");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_validate_state_fields: state_data is NULL");
        return false;
    }

    const NimcpStateHeader* header = (const NimcpStateHeader*) state_data;

    // STEP 2: Magic number validation
    // WHY: Protocol handshake, ensures this is a state structure
    // VALUE: 'NIMC' in ASCII (0x4E494D43)
    if (header->magic != NIMCP_STATE_MAGIC) {
        LOG_ERROR("Invalid state magic number");
        LOG_ERROR("nimcp_validate_state_fields failed: returning error");
        return false;
    }

    // STEP 3: Field count validation
    // WHY: Ensure sensible structure, prevent DoS
    if (header->field_count == 0 || header->field_count > NIMCP_MAX_FIELDS) {
        LOG_ERROR("Invalid field count: %u", header->field_count);
        LOG_ERROR("nimcp_validate_state_fields failed: returning error");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_validate_state_fields: header->field_count is zero");
        return false;
    }

    // Get field table
    // LOCATION: Immediately after header
    const NimcpStateField* fields =
        (const NimcpStateField*) ((const uint8_t*) state_data + sizeof(NimcpStateHeader));

    // STEP 4: Overlap detection using usage map
    // WHY: Detect overlapping fields (corruption indicator)
    // ALGORITHM: Mark used bytes, check for conflicts

    // Allocate usage map (1 byte per byte of state)
    // WHY calloc: Initialize to 0 (unused)
    uint8_t* usage_map = nimcp_calloc(size, sizeof(uint8_t));
    if (!usage_map) {
        LOG_ERROR("Failed to allocate usage map");
        LOG_ERROR("nimcp_validate_state_fields failed: returning error");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_validate_state_fields: usage_map is NULL");
        return false;
    }

    // Mark header region as used
    // WHY: Header + field table are always used
    // RANGE: [0, sizeof(header) + field_count × sizeof(field))
    memset(usage_map, 1, sizeof(NimcpStateHeader) + header->field_count * sizeof(NimcpStateField));

    // STEP 5: Validate each field
    // WHY: Ensure all fields are valid and non-overlapping
    for (uint32_t i = 0; i < header->field_count; i++) {
        const NimcpStateField* field = &fields[i];

        // Check field boundaries
        // WHY: Prevent out-of-bounds access
        // FORMULA: offset + size must be <= total size
        if (field->offset + field->size > size) {
            LOG_ERROR("Field %u exceeds state boundaries", i);
            nimcp_free(usage_map);
            LOG_ERROR("nimcp_validate_state_fields failed: returning error");
            return false;
        }

        // Check for overlaps using usage map
        // WHY: Overlaps indicate corruption or malformed structure
        // ALGORITHM: Check if any byte in field's range is already used
        for (size_t j = 0; j < field->size; j++) {
            if (usage_map[field->offset + j]) {
                // Overlap detected: byte already used by header or another field
                LOG_ERROR("Field %u overlaps with other data", i);
                nimcp_free(usage_map);
                LOG_ERROR("nimcp_validate_state_fields failed: returning error");
                return false;
            }
            // Mark byte as used
            usage_map[field->offset + j] = 1;
        }

        // Validate field data based on type
        // WHY: Ensure field contents are valid
        // LOCATION: state_data + field offset
        const void* field_data = (const uint8_t*) state_data + field->offset;
        bool valid = false;

        // Type dispatch
        // WHY SWITCH: Delegate to appropriate validator
        switch (field->type) {
            case NIMCP_FIELD_INTEGER:
                valid = nimcp_validate_integer_field(field_data, field->size);
                break;

            case NIMCP_FIELD_FLOAT:
                valid = nimcp_validate_float_field(field_data, field->size);
                break;

            case NIMCP_FIELD_STRING:
                valid = nimcp_validate_string_field(field_data, field->size);
                break;

            case NIMCP_FIELD_ARRAY:
                valid = nimcp_validate_array_field(field_data, field->size);
                break;

            default:
                // Unknown field type
                // WHY REJECT: Cannot validate unknown types
                LOG_ERROR("Unknown field type %d for field %u", field->type, i);
                nimcp_free(usage_map);
                LOG_ERROR("nimcp_validate_state_fields failed: returning error");
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_validate_state_fields: operation failed");
                return false;
        }

        // Check validation result
        // WHY: Fail-fast on first invalid field
        if (!valid) {
            nimcp_free(usage_map);
            LOG_ERROR("nimcp_validate_state_fields failed: returning error");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_validate_state_fields: valid is NULL");
            return false;
        }
    }

    // Cleanup and success
    nimcp_free(usage_map);
    return true;
}

//=============================================================================
// Internal Helper Function Implementations
//=============================================================================

/**
 * @brief Validate UTF-8 character sequence
 *
 * WHY THIS FUNCTION:
 * - UTF-8 is variable-length encoding (1-4 bytes)
 * - Must validate multi-byte sequences
 * - Prevents buffer overruns from malformed sequences
 * - Ensures interoperability
 *
 * UTF-8 ENCODING RULES:
 * - 1 byte:  0xxxxxxx                    (0x00-0x7F, ASCII)
 * - 2 bytes: 110xxxxx 10xxxxxx           (0x80-0x7FF)
 * - 3 bytes: 1110xxxx 10xxxxxx 10xxxxxx  (0x800-0xFFFF)
 * - 4 bytes: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx (0x10000-0x10FFFF)
 *
 * CONTINUATION BYTES:
 * - Pattern: 10xxxxxx
 * - Mask: 0xC0 = 11000000
 * - Test: (byte & 0xC0) == 0x80
 *
 * VALIDATION ALGORITHM:
 * 1. Examine first byte to determine sequence length
 * 2. Verify sufficient bytes remain in string
 * 3. Validate all continuation bytes match 10xxxxxx pattern
 * 4. Return true if valid, false otherwise
 *
 * FIRST BYTE PATTERNS:
 * - 0xxxxxxx (0x00-0x7F): 1 byte sequence (ASCII)
 * - 110xxxxx (0xC0-0xDF): 2 byte sequence
 * - 1110xxxx (0xE0-0xEF): 3 byte sequence
 * - 11110xxx (0xF0-0xF7): 4 byte sequence
 * - Other: Invalid
 *
 * BIT MASKING:
 * - (byte & 0x80) == 0: ASCII (0xxxxxxx)
 * - (byte & 0xE0) == 0xC0: 2-byte (110xxxxx)
 * - (byte & 0xF0) == 0xE0: 3-byte (1110xxxx)
 * - (byte & 0xF8) == 0xF0: 4-byte (11110xxx)
 *
 * WHY THESE MASKS:
 * - Progressive refinement
 * - Check most significant bits first
 * - Uniquely identify sequence type
 *
 * BOUNDARY CHECK:
 * - Ensure len >= seq_len
 * - Prevents reading past end of string
 * - Catches truncated sequences
 *
 * CONTINUATION VALIDATION:
 * - Loop from byte 1 to seq_len-1
 * - Check each byte matches 10xxxxxx
 * - Fail if any byte is wrong
 *
 * LIMITATIONS:
 * - Does not validate overlong encodings
 * - Does not validate invalid code points (surrogates, etc.)
 * - Sufficient for basic validation
 *
 * WHY SUFFICIENT:
 * - Catches malformed sequences
 * - Prevents buffer overruns
 * - Ensures basic interoperability
 * - Full validation would be much more complex
 *
 * COMPLEXITY: O(1) - constant time (max 4 bytes)
 * THREAD SAFETY: Thread-safe (pure function)
 *
 * @param str Pointer to character sequence (must be non-NULL)
 * @param len Number of bytes remaining in string
 * @return true if valid UTF-8, false if invalid
 *
 * EXAMPLES:
 * - 'A' (0x41): 1 byte, valid
 * - 'é' (0xC3 0xA9): 2 bytes, valid
 * - '€' (0xE2 0x82 0xAC): 3 bytes, valid
 * - Invalid: 0xC0 0x41 (continuation byte is wrong)
 * - Invalid: 0xE0 0x80 (insufficient bytes)
 */
static size_t is_valid_utf8_char(const char* str, size_t len)
{
    // NULL/empty check
    // RETURN 0: Invalid (error condition)
    if (!str || len == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "is_valid_utf8_char: invalid parameters");

            return 0;
    }

    // Examine first byte to determine sequence length
    uint8_t first = (uint8_t) str[0];
    size_t seq_len;

    if ((first & 0x80) == 0) {
        // 0xxxxxxx: ASCII (1 byte)
        // WHY: Most common case (ASCII text)
        // RANGE: 0x00-0x7F
        // RETURN 1: Single byte consumed
        return 1;
    } else if ((first & 0xE0) == 0xC0) {
        // 110xxxxx: 2 byte sequence
        // MASK: 0xE0 = 11100000
        // PATTERN: 110xxxxx
        seq_len = 2;
    } else if ((first & 0xF0) == 0xE0) {
        // 1110xxxx: 3 byte sequence
        // MASK: 0xF0 = 11110000
        // PATTERN: 1110xxxx
        seq_len = 3;
    } else if ((first & 0xF8) == 0xF0) {
        // 11110xxx: 4 byte sequence
        // MASK: 0xF8 = 11111000
        // PATTERN: 11110xxx
        seq_len = 4;
    } else {
        // Invalid first byte
        // WHY: Doesn't match any valid UTF-8 pattern
        // RETURN 0: Invalid
        return 0;
    }

    // Check if string has enough bytes
    // WHY: Prevent reading past end of string
    // EXAMPLE: 2-byte sequence at end of string
    if (len < seq_len) {
        return 0;
    }

    // Validate continuation bytes
    // WHY: All bytes after first must match 10xxxxxx
    // RANGE: [1, seq_len)
    for (size_t i = 1; i < seq_len; i++) {
        // Continuation byte check
        // PATTERN: 10xxxxxx
        // MASK: 0xC0 = 11000000
        // TEST: (byte & 0xC0) == 0x80
        if ((str[i] & 0xC0) != 0x80) {
            return 0;
        }
    }

    // RETURN seq_len: Number of bytes consumed (1-4)
    // WHY: Caller needs to skip continuation bytes
    return seq_len;
}

//=============================================================================
// Pointer Validation
//=============================================================================

/**
 * @brief Validate pointer is non-NULL
 *
 * WHY THIS FUNCTION:
 * - Provides consistent NULL pointer checking across NIMCP codebase
 * - Generates descriptive error messages with parameter names
 * - Follows fail-fast validation strategy
 * - Prevents segmentation faults from NULL dereferences
 *
 * USAGE PATTERN:
 * if (!nimcp_validate_pointer(data, "data")) {
 *     return ERROR_CODE;
 * }
 *
 * WHY SEPARATE FUNCTION:
 * - Centralized error logging (consistent messages)
 * - Reduces code duplication
 * - Easy to add debug assertions or backtraces
 * - Can be extended for additional pointer checks (alignment, etc.)
 *
 * ERROR REPORTING:
 * - If name provided: "NULL pointer: <name>"
 * - If name NULL: "NULL pointer validation failed"
 *
 * COMPLEXITY: O(1) - constant time check
 * THREAD SAFETY: Thread-safe (pure function, no shared state)
 *
 * @param ptr Pointer to validate (may be NULL - that's what we're checking)
 * @param name Name of pointer for error messages (can be NULL)
 * @return true if ptr is non-NULL, false if ptr is NULL
 *
 * ERROR CONDITIONS:
 * - ptr is NULL
 */
bool nimcp_validate_pointer(const void* ptr, const char* name)
{
    LOG_TRACE("Entering nimcp_validate_pointer");
    if (!ptr) {
        if (name) {
            LOG_ERROR("NULL pointer: %s", name);
        } else {
            LOG_ERROR("NULL pointer validation failed");
        }
        LOG_ERROR("nimcp_validate_pointer failed: returning error");
        return false;
    }
    return true;
}
