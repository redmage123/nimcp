# Phase 10: Coding Standards & Best Practices

## Overview

All Phase 10 code MUST follow NIMCP's established coding standards. This document consolidates standards from Phases 1-9 and adds Phase 10-specific guidelines.

---

## 1. Documentation Standards

### WHAT-WHY-HOW Comments (MANDATORY)

Every function MUST have WHAT-WHY-HOW documentation:

```c
/**
 * @brief Add item to working memory
 *
 * WHAT: Insert new item into working memory buffer
 * WHY:  Maintain active representations for reasoning
 * HOW:  Check capacity, evict if full, insert with salience
 *
 * COMPLEXITY: O(n) where n = capacity (due to eviction)
 * MEMORY: O(1) - reuses existing slots
 *
 * @param wm Working memory instance
 * @param item Item data to add
 * @param item_size Size of item in floats
 * @param salience Importance [0,1]
 * @return true on success, false if invalid params
 */
bool working_memory_add(
    working_memory_t* wm,
    const float* item,
    uint32_t item_size,
    float salience
);
```

**Required Sections**:
- `@brief`: One-line description
- `WHAT`: What does this function do?
- `WHY`: Why does this function exist?
- `HOW`: How does it work? (algorithm summary)
- `COMPLEXITY`: Time complexity (Big-O)
- `MEMORY`: Space complexity or memory impact
- `@param`: Each parameter documented
- `@return`: Return value meaning

### File Headers (MANDATORY)

Every `.h` and `.c` file MUST have:

```c
/**
 * @file nimcp_working_memory.h
 * @brief Working memory system with capacity limits
 *
 * WHAT: Implements Miller's 7±2 working memory buffer
 * WHY:  Enable reasoning over active representations
 * HOW:  Fixed-size buffer with salience-based eviction
 *
 * BIOLOGICAL BASIS:
 * - Prefrontal cortex maintains ~7 items via persistent firing
 * - Attention refresh prevents decay
 * - Salience determines retention priority
 *
 * INTEGRATION:
 * - Used by: Executive functions, Theory of mind
 * - Depends on: None (standalone)
 *
 * @author NIMCP Development Team
 * @date 2025-01-09
 * @version 2.7.0 Phase 10.1
 */
```

### Inline Comments (MANDATORY for Complex Logic)

```c
// GOOD: Explains WHY
// Use exponential decay: e^(-t/tau)
// tau=1000ms gives half-life of ~700ms
float decay_factor = expf(-(float)time_delta / 1000.0f);

// BAD: States WHAT (obvious from code)
// Multiply by decay factor
item_strength *= decay_factor;
```

---

## 2. Function Design Standards

### Single Responsibility Principle (SRP)

Each function does ONE thing:

```c
// GOOD: Single responsibility
float working_memory_calculate_decay(uint64_t time_delta, float tau) {
    return expf(-(float)time_delta / tau);
}

void working_memory_apply_decay(working_memory_t* wm, uint64_t current_time) {
    for (uint32_t i = 0; i < wm->current_size; i++) {
        uint64_t age = current_time - wm->timestamps[i];
        float decay = working_memory_calculate_decay(age, wm->decay_tau);
        wm->salience[i] *= decay;
    }
}

// BAD: Multiple responsibilities (calculate + apply + evict)
void working_memory_update(working_memory_t* wm, uint64_t current_time) {
    // Calculates decay
    // Applies decay
    // Evicts items
    // Reorders buffer
    // ... 80 lines of mixed logic
}
```

### Function Length: < 50 Lines

If function exceeds 50 lines, decompose:

```c
// GOOD: Decomposed into helpers
bool working_memory_add(working_memory_t* wm, const float* item,
                       uint32_t item_size, float salience) {
    if (!validate_add_params(wm, item, item_size, salience)) {
        return false;
    }

    if (is_capacity_full(wm)) {
        evict_lowest_salience_item(wm);
    }

    insert_item_at_next_slot(wm, item, item_size, salience);
    return true;
}

// Each helper is < 20 lines
```

### Guard Clauses (Early Returns)

Always validate inputs first, return early on failure:

```c
// GOOD: Guard clauses at top
bool working_memory_add(...) {
    // Guard: NULL checks
    if (!wm || !item) {
        set_error("NULL parameter to working_memory_add");
        return false;
    }

    // Guard: Capacity check
    if (item_size == 0 || item_size > MAX_ITEM_SIZE) {
        set_error("Invalid item_size: %u", item_size);
        return false;
    }

    // Guard: Salience range
    if (salience < 0.0f || salience > 1.0f) {
        set_error("Salience out of range: %.2f", salience);
        return false;
    }

    // Main logic (no deep nesting)
    if (wm->current_size >= wm->capacity) {
        evict_lowest_salience_item(wm);
    }

    insert_item_at_next_slot(wm, item, item_size, salience);
    return true;
}

// BAD: Nested ifs
bool working_memory_add(...) {
    if (wm) {
        if (item) {
            if (item_size > 0) {
                if (salience >= 0.0f && salience <= 1.0f) {
                    // Logic nested 4 levels deep
                }
            }
        }
    }
    return false;
}
```

---

## 3. Naming Conventions

### Functions

```c
// Pattern: module_noun_verb() or module_verb_noun()
working_memory_add()           // ✅ Clear action
working_memory_get()           // ✅ Clear action
working_memory_calculate_decay() // ✅ Clear action

add_to_memory()                // ❌ Too vague
wm_add()                       // ❌ Unclear abbreviation
```

### Variables

```c
// GOOD: Descriptive names
uint32_t current_size;
float salience_threshold;
uint64_t timestamp_ms;

// BAD: Unclear abbreviations
uint32_t sz;
float st;
uint64_t ts;
```

### Constants

```c
// GOOD: ALL_CAPS with semantic meaning
#define WORKING_MEMORY_DEFAULT_CAPACITY 7
#define WORKING_MEMORY_DECAY_TAU_MS 1000.0f
#define WORKING_MEMORY_MIN_SALIENCE 0.01f

// BAD: Magic numbers
if (wm->current_size > 7) { ... }        // ❌
if (decay_factor < 0.01f) { ... }        // ❌

// GOOD: Named constants
if (wm->current_size > WORKING_MEMORY_DEFAULT_CAPACITY) { ... }  // ✅
if (decay_factor < WORKING_MEMORY_MIN_SALIENCE) { ... }          // ✅
```

---

## 4. Error Handling

### NULL Pointer Checks (MANDATORY)

```c
bool working_memory_add(...) {
    // MANDATORY: Check ALL pointer parameters
    if (!wm) {
        set_error("NULL working_memory_t in working_memory_add");
        return false;
    }

    if (!item) {
        set_error("NULL item in working_memory_add");
        return false;
    }

    // Main logic...
}
```

### Error Messages

```c
// GOOD: Specific, actionable error messages
set_error("Working memory capacity exceeded: %u/%u items",
          wm->current_size, wm->capacity);

set_error("Invalid salience %.2f, expected [0.0, 1.0]", salience);

// BAD: Vague errors
set_error("Error");
set_error("Failed");
```

### Buffer Overflow Protection

```c
// GOOD: Check before writing
int written = snprintf(output->explanation, sizeof(output->explanation),
                      "Item stored at index %u with salience %.2f",
                      index, salience);

if (written < 0 || (size_t)written >= sizeof(output->explanation)) {
    set_error("Explanation buffer overflow");
    return false;
}

// BAD: Unchecked sprintf
sprintf(output->explanation, "Item stored..."); // ❌ No bounds check
```

---

## 5. Memory Management

### Use NIMCP Memory Functions

```c
// GOOD: NIMCP memory functions
float* item_copy = nimcp_malloc(item_size * sizeof(float));
if (!item_copy) {
    set_error("Failed to allocate %zu bytes", item_size * sizeof(float));
    return false;
}

// Copy data
memcpy(item_copy, item, item_size * sizeof(float));

// Free when done
nimcp_free(item_copy);

// BAD: Raw malloc/free
float* item = malloc(size);  // ❌ Use nimcp_malloc
free(item);                  // ❌ Use nimcp_free
```

### Always Check Allocations

```c
// GOOD: Check allocation success
working_memory_t* wm = nimcp_malloc(sizeof(working_memory_t));
if (!wm) {
    set_error("Failed to allocate working memory");
    return NULL;
}

// BAD: Assume allocation succeeds
working_memory_t* wm = nimcp_malloc(sizeof(working_memory_t));
wm->capacity = 7;  // ❌ Could be NULL!
```

### Cleanup on Error Paths

```c
// GOOD: Cleanup allocated resources on error
working_memory_t* working_memory_create(uint32_t capacity) {
    working_memory_t* wm = nimcp_malloc(sizeof(working_memory_t));
    if (!wm) {
        return NULL;
    }

    wm->items = nimcp_malloc(capacity * sizeof(float*));
    if (!wm->items) {
        nimcp_free(wm);  // ✅ Cleanup before return
        return NULL;
    }

    wm->salience = nimcp_malloc(capacity * sizeof(float));
    if (!wm->salience) {
        nimcp_free(wm->items);  // ✅ Cleanup both
        nimcp_free(wm);
        return NULL;
    }

    return wm;
}
```

---

## 6. Data Structure Design

### Opaque Pointers (Public API)

```c
// HEADER (.h): Opaque pointer
typedef struct working_memory working_memory_t;

// IMPLEMENTATION (.c): Full definition
struct working_memory {
    float** items;
    uint32_t capacity;
    uint32_t current_size;
    float* salience;
    uint64_t* timestamps;
    bool* attention_refreshed;
    float decay_tau;
};
```

### Configuration Structs

```c
/**
 * @brief Working memory configuration
 *
 * All parameters have sensible defaults
 */
typedef struct {
    uint32_t capacity;              /**< Max items (default: 7) */
    float decay_tau_ms;             /**< Decay time constant (default: 1000ms) */
    float min_salience;             /**< Eviction threshold (default: 0.01) */
    bool enable_attention_refresh;  /**< Enable rehearsal (default: true) */
} working_memory_config_t;

// Default config function
working_memory_config_t working_memory_default_config(void) {
    return (working_memory_config_t){
        .capacity = WORKING_MEMORY_DEFAULT_CAPACITY,
        .decay_tau_ms = WORKING_MEMORY_DECAY_TAU_MS,
        .min_salience = WORKING_MEMORY_MIN_SALIENCE,
        .enable_attention_refresh = true
    };
}
```

---

## 7. Testing Standards

### Unit Test Structure

```c
/**
 * @test Working Memory Capacity Enforcement
 *
 * WHAT: Verify capacity limit enforced
 * WHY:  Ensure Miller's 7±2 principle
 * HOW:  Add 10 items to capacity-7 buffer, verify only 7 stored
 */
TEST(WorkingMemoryTest, CapacityEnforcement) {
    // ARRANGE: Setup
    working_memory_config_t config = working_memory_default_config();
    config.capacity = 7;
    working_memory_t* wm = working_memory_create_custom(&config);
    ASSERT_NE(wm, nullptr);

    // ACT: Add 10 items
    for (int i = 0; i < 10; i++) {
        float item[5] = {i, i+1, i+2, i+3, i+4};
        working_memory_add(wm, item, 5, 0.5f);
    }

    // ASSERT: Only 7 items stored
    EXPECT_EQ(working_memory_get_size(wm), 7);

    // CLEANUP
    working_memory_destroy(wm);
}
```

### Test Coverage

- **Target**: 95% line coverage minimum
- **Required**: All public API functions tested
- **Edge Cases**: NULL params, boundary values, error conditions

---

## 8. Phase 10-Specific Standards

### Integration Points

Every Phase 10 module MUST document integration:

```c
/**
 * @file nimcp_working_memory.h
 *
 * INTEGRATION POINTS:
 *
 * 1. Brain Structure (src/core/brain/nimcp_brain.c)
 *    - Add: working_memory_t* working_memory;
 *    - Init: brain_create_custom()
 *    - Cleanup: brain_destroy()
 *
 * 2. Configuration (src/core/brain/nimcp_brain.h)
 *    - Add: bool enable_working_memory;
 *    - Add: uint32_t working_memory_capacity;
 *
 * 3. Processing Pipeline (apply_cognitive_processing())
 *    - Stage: After executive control
 *    - Purpose: Maintain active representations
 *
 * DEPENDENCIES:
 * - None (standalone module)
 *
 * DEPENDENT MODULES:
 * - Theory of Mind (uses working_memory for belief tracking)
 * - Executive Functions (uses working_memory for planning)
 */
```

### Backward Compatibility

All features MUST be opt-in:

```c
// MANDATORY: Check enable flag before using
if (brain->config.enable_working_memory && brain->working_memory) {
    // Use working memory
    working_memory_add(brain->working_memory, ...);
} else {
    // Graceful degradation - don't fail
    // Just skip this enhancement
}
```

### Performance Annotations

```c
/**
 * @brief Apply temporal decay to all items
 *
 * PERFORMANCE:
 * - O(n) where n = current_size
 * - Typical: 7 items × 1 float op = 7 FLOPs
 * - Expected: < 1 microsecond on modern CPU
 *
 * OPTIMIZATION NOTES:
 * - Consider SIMD if capacity increased to >100
 * - Current implementation optimized for small n (7)
 */
void working_memory_decay(working_memory_t* wm, uint64_t current_time);
```

---

## 9. Code Review Checklist

Before submitting PR, verify:

### Documentation
- [ ] File header with WHAT-WHY-HOW present
- [ ] All functions have WHAT-WHY-HOW comments
- [ ] All parameters documented
- [ ] Integration points documented
- [ ] Complex logic has inline comments

### Code Quality
- [ ] No functions > 50 lines
- [ ] Single Responsibility Principle followed
- [ ] Guard clauses used (early returns)
- [ ] No magic numbers (all constants named)
- [ ] No deep nesting (< 3 levels)

### Safety
- [ ] All pointer parameters NULL-checked
- [ ] All allocations checked for failure
- [ ] Cleanup on error paths
- [ ] Buffer overflow protection (snprintf bounds)
- [ ] No memory leaks (valgrind clean)

### Testing
- [ ] Unit tests written (AAA pattern)
- [ ] All public APIs tested
- [ ] Edge cases tested
- [ ] Coverage > 95%
- [ ] Tests pass (100%)

### Integration
- [ ] Backward compatible (opt-in)
- [ ] Enable flag added to config
- [ ] Pointer added to brain_struct
- [ ] Initialization in brain_create_custom()
- [ ] Cleanup in brain_destroy()
- [ ] No impact if disabled

### Performance
- [ ] Complexity documented
- [ ] Profiled (< 2x baseline latency)
- [ ] Memory overhead acceptable (< 50%)
- [ ] No unnecessary allocations in hot path

---

## 10. Example: Perfect Function

```c
/**
 * @brief Add item to working memory with salience-based eviction
 *
 * WHAT: Insert new item into working memory buffer
 * WHY:  Maintain active representations for reasoning and planning
 * HOW:  Validate params → Check capacity → Evict if full → Insert new item
 *
 * ALGORITHM:
 * 1. Validate all parameters (NULL checks, range checks)
 * 2. If at capacity, find and evict lowest-salience item
 * 3. Copy item data to buffer
 * 4. Record salience and timestamp
 * 5. Mark as not yet attention-refreshed
 *
 * COMPLEXITY: O(n) where n = capacity (due to eviction search)
 * MEMORY: O(item_size) for item copy
 *
 * BIOLOGICAL BASIS:
 * - Mimics prefrontal cortex working memory
 * - Salience-based retention matches attention systems
 * - Capacity limit matches Miller's 7±2 observations
 *
 * @param wm Working memory instance (non-NULL)
 * @param item Item data to add (non-NULL)
 * @param item_size Size of item in floats (must be > 0)
 * @param salience Importance score [0.0, 1.0] for eviction priority
 * @return true on success, false on invalid parameters
 *
 * @note Higher salience items are retained longer during eviction
 * @note Caller retains ownership of item data (deep copy made)
 */
bool working_memory_add(
    working_memory_t* wm,
    const float* item,
    uint32_t item_size,
    float salience
)
{
    // =========================================================================
    // GUARD: Validate all parameters
    // =========================================================================

    // Guard: NULL pointer checks
    if (!wm) {
        set_error("NULL working_memory_t in working_memory_add");
        return false;
    }

    if (!item) {
        set_error("NULL item in working_memory_add");
        return false;
    }

    // Guard: Item size validation
    if (item_size == 0) {
        set_error("Item size cannot be 0");
        return false;
    }

    if (item_size > WORKING_MEMORY_MAX_ITEM_SIZE) {
        set_error("Item size %u exceeds maximum %u",
                  item_size, WORKING_MEMORY_MAX_ITEM_SIZE);
        return false;
    }

    // Guard: Salience range check
    if (salience < 0.0f || salience > 1.0f) {
        set_error("Salience %.2f out of range [0.0, 1.0]", salience);
        return false;
    }

    // =========================================================================
    // EVICTION: Remove lowest-salience item if at capacity
    // =========================================================================

    if (wm->current_size >= wm->capacity) {
        // WHAT: Find lowest-salience item for eviction
        // WHY:  Make room for new item while preserving important items
        // HOW:  Linear search (acceptable for capacity=7)
        uint32_t evict_index = find_lowest_salience_index(wm);

        // WHAT: Remove evicted item
        // WHY:  Free memory and slot for new item
        // HOW:  Free item data, shift metadata
        evict_item_at_index(wm, evict_index);
    }

    // =========================================================================
    // INSERTION: Add new item to next available slot
    // =========================================================================

    uint32_t insert_index = wm->current_size;

    // WHAT: Allocate and copy item data
    // WHY:  Working memory owns item (deep copy)
    // HOW:  nimcp_malloc + memcpy
    float* item_copy = nimcp_malloc(item_size * sizeof(float));
    if (!item_copy) {
        set_error("Failed to allocate %zu bytes for item",
                  item_size * sizeof(float));
        return false;
    }

    memcpy(item_copy, item, item_size * sizeof(float));

    // WHAT: Store item and metadata
    // WHY:  Track item, salience, timestamp for decay/eviction
    // HOW:  Direct assignment to arrays
    wm->items[insert_index] = item_copy;
    wm->item_sizes[insert_index] = item_size;
    wm->salience[insert_index] = salience;
    wm->timestamps[insert_index] = get_current_time_ms();
    wm->attention_refreshed[insert_index] = false;

    // Increment size
    wm->current_size++;

    return true;
}
```

---

## Enforcement

- **Automated**: CI/CD runs `clang-format`, `cppcheck`, `cpplint`
- **Code Review**: Architect reviews all PRs for standards compliance
- **No Merge**: PRs failing standards checks cannot merge

**All Phase 10 code MUST meet these standards. No exceptions.**
