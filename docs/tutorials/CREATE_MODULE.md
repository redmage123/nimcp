# Create Your First NIMCP Module

This tutorial walks you through creating a new NIMCP module from scratch.

## Overview

NIMCP modules follow a consistent pattern:

1. **Header file** (`include/`) - Public API declarations
2. **Implementation file** (`src/`) - Function implementations
3. **Unit tests** (`test/unit/`) - Test coverage
4. **CMake integration** - Build system updates

## Example: Creating a "Memory Bank" Module

We'll create a simple module that stores and retrieves patterns.

### Step 1: Create the Header File

Create `/home/bbrelin/nimcp/include/cognitive/nimcp_memory_bank.h`:

```c
/**
 * @file nimcp_memory_bank.h
 * @brief Simple pattern memory bank for NIMCP
 *
 * This module provides a memory bank that can store patterns
 * and retrieve them based on similarity.
 */

#ifndef NIMCP_MEMORY_BANK_H
#define NIMCP_MEMORY_BANK_H

#include "nimcp_types.h"
#include "nimcp_error.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------
 * Types
 * --------------------------------------------------------------------- */

/**
 * @brief Memory bank handle
 */
typedef struct nimcp_memory_bank_s nimcp_memory_bank_t;

/**
 * @brief Stored pattern structure
 */
typedef struct {
    float* pattern;      /**< Pattern data */
    size_t pattern_size; /**< Size of pattern */
    float confidence;    /**< Storage confidence */
    uint64_t timestamp;  /**< When pattern was stored */
} nimcp_stored_pattern_t;

/**
 * @brief Memory bank configuration
 */
typedef struct {
    size_t max_patterns;     /**< Maximum number of patterns to store */
    size_t pattern_size;     /**< Size of each pattern */
    float similarity_threshold; /**< Threshold for similarity matching */
} nimcp_memory_bank_config_t;

/* ---------------------------------------------------------------------
 * Lifecycle Functions
 * --------------------------------------------------------------------- */

/**
 * @brief Create a new memory bank
 *
 * @param config Configuration for the memory bank
 * @return New memory bank handle, or NULL on failure
 */
nimcp_memory_bank_t* nimcp_memory_bank_create(
    const nimcp_memory_bank_config_t* config);

/**
 * @brief Destroy a memory bank and free resources
 *
 * @param bank Memory bank to destroy (can be NULL)
 */
void nimcp_memory_bank_destroy(nimcp_memory_bank_t* bank);

/* ---------------------------------------------------------------------
 * Core Functions
 * --------------------------------------------------------------------- */

/**
 * @brief Store a pattern in the memory bank
 *
 * @param bank Memory bank handle
 * @param pattern Pattern data to store
 * @param size Size of the pattern
 * @param confidence Confidence level (0.0 to 1.0)
 * @return NIMCP_OK on success, error code on failure
 */
nimcp_error_t nimcp_memory_bank_store(
    nimcp_memory_bank_t* bank,
    const float* pattern,
    size_t size,
    float confidence);

/**
 * @brief Retrieve the most similar pattern
 *
 * @param bank Memory bank handle
 * @param query Query pattern
 * @param query_size Size of query pattern
 * @param result Output: most similar stored pattern (caller must free)
 * @param similarity Output: similarity score (0.0 to 1.0)
 * @return NIMCP_OK on success, NIMCP_ERROR_NOT_FOUND if no match
 */
nimcp_error_t nimcp_memory_bank_retrieve(
    nimcp_memory_bank_t* bank,
    const float* query,
    size_t query_size,
    nimcp_stored_pattern_t** result,
    float* similarity);

/**
 * @brief Get the number of stored patterns
 *
 * @param bank Memory bank handle
 * @return Number of patterns, or 0 if bank is NULL
 */
size_t nimcp_memory_bank_count(const nimcp_memory_bank_t* bank);

/**
 * @brief Clear all patterns from the memory bank
 *
 * @param bank Memory bank handle
 */
void nimcp_memory_bank_clear(nimcp_memory_bank_t* bank);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MEMORY_BANK_H */
```

### Step 2: Create the Implementation File

Create `/home/bbrelin/nimcp/src/cognitive/nimcp_memory_bank.c`:

```c
/**
 * @file nimcp_memory_bank.c
 * @brief Implementation of pattern memory bank
 */

#include "cognitive/nimcp_memory_bank.h"
#include "utils/nimcp_memory.h"
#include "utils/nimcp_logging.h"

#include <string.h>
#include <math.h>

/* Internal structure */
struct nimcp_memory_bank_s {
    nimcp_memory_bank_config_t config;
    nimcp_stored_pattern_t* patterns;
    size_t pattern_count;
    nimcp_mutex_t* mutex;
};

/* ---------------------------------------------------------------------
 * Helper Functions
 * --------------------------------------------------------------------- */

static float compute_similarity(const float* a, const float* b, size_t size) {
    /* Cosine similarity */
    float dot = 0.0f, norm_a = 0.0f, norm_b = 0.0f;

    for (size_t i = 0; i < size; i++) {
        dot += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }

    if (norm_a < 1e-10f || norm_b < 1e-10f) {
        return 0.0f;
    }

    return dot / (sqrtf(norm_a) * sqrtf(norm_b));
}

/* ---------------------------------------------------------------------
 * Lifecycle Functions
 * --------------------------------------------------------------------- */

nimcp_memory_bank_t* nimcp_memory_bank_create(
    const nimcp_memory_bank_config_t* config)
{
    if (!config || config->max_patterns == 0 || config->pattern_size == 0) {
        LOG_ERROR("Invalid memory bank configuration");
        return NULL;
    }

    nimcp_memory_bank_t* bank = nimcp_calloc(1, sizeof(nimcp_memory_bank_t));
    if (!bank) {
        LOG_ERROR("Failed to allocate memory bank");
        return NULL;
    }

    bank->config = *config;
    bank->patterns = nimcp_calloc(config->max_patterns,
                                   sizeof(nimcp_stored_pattern_t));
    if (!bank->patterns) {
        LOG_ERROR("Failed to allocate pattern storage");
        nimcp_free(bank);
        return NULL;
    }

    bank->mutex = nimcp_mutex_create(NULL);
    if (!bank->mutex) {
        LOG_ERROR("Failed to create mutex");
        nimcp_free(bank->patterns);
        nimcp_free(bank);
        return NULL;
    }

    bank->pattern_count = 0;

    LOG_INFO("Memory bank created (max=%zu, size=%zu)",
             config->max_patterns, config->pattern_size);

    return bank;
}

void nimcp_memory_bank_destroy(nimcp_memory_bank_t* bank) {
    if (!bank) return;

    nimcp_memory_bank_clear(bank);

    if (bank->mutex) {
        nimcp_mutex_destroy(bank->mutex);
    }

    nimcp_free(bank->patterns);
    nimcp_free(bank);

    LOG_DEBUG("Memory bank destroyed");
}

/* ---------------------------------------------------------------------
 * Core Functions
 * --------------------------------------------------------------------- */

nimcp_error_t nimcp_memory_bank_store(
    nimcp_memory_bank_t* bank,
    const float* pattern,
    size_t size,
    float confidence)
{
    if (!bank || !pattern) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (size != bank->config.pattern_size) {
        LOG_ERROR("Pattern size mismatch: expected %zu, got %zu",
                  bank->config.pattern_size, size);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(bank->mutex);

    /* Check capacity */
    if (bank->pattern_count >= bank->config.max_patterns) {
        nimcp_mutex_unlock(bank->mutex);
        LOG_WARN("Memory bank full");
        return NIMCP_ERROR_CAPACITY;
    }

    /* Allocate and copy pattern */
    nimcp_stored_pattern_t* stored = &bank->patterns[bank->pattern_count];
    stored->pattern = nimcp_malloc(size * sizeof(float));
    if (!stored->pattern) {
        nimcp_mutex_unlock(bank->mutex);
        return NIMCP_ERROR_MEMORY;
    }

    memcpy(stored->pattern, pattern, size * sizeof(float));
    stored->pattern_size = size;
    stored->confidence = confidence;
    stored->timestamp = nimcp_get_time_ms();

    bank->pattern_count++;

    nimcp_mutex_unlock(bank->mutex);

    LOG_DEBUG("Pattern stored (count=%zu, confidence=%.2f)",
              bank->pattern_count, confidence);

    return NIMCP_OK;
}

nimcp_error_t nimcp_memory_bank_retrieve(
    nimcp_memory_bank_t* bank,
    const float* query,
    size_t query_size,
    nimcp_stored_pattern_t** result,
    float* similarity)
{
    if (!bank || !query || !result || !similarity) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (query_size != bank->config.pattern_size) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(bank->mutex);

    if (bank->pattern_count == 0) {
        nimcp_mutex_unlock(bank->mutex);
        return NIMCP_ERROR_NOT_FOUND;
    }

    /* Find most similar pattern */
    float best_similarity = -1.0f;
    size_t best_index = 0;

    for (size_t i = 0; i < bank->pattern_count; i++) {
        float sim = compute_similarity(query, bank->patterns[i].pattern,
                                        query_size);
        if (sim > best_similarity) {
            best_similarity = sim;
            best_index = i;
        }
    }

    /* Check threshold */
    if (best_similarity < bank->config.similarity_threshold) {
        nimcp_mutex_unlock(bank->mutex);
        return NIMCP_ERROR_NOT_FOUND;
    }

    /* Copy result */
    *result = nimcp_malloc(sizeof(nimcp_stored_pattern_t));
    if (!*result) {
        nimcp_mutex_unlock(bank->mutex);
        return NIMCP_ERROR_MEMORY;
    }

    nimcp_stored_pattern_t* src = &bank->patterns[best_index];
    (*result)->pattern = nimcp_malloc(src->pattern_size * sizeof(float));
    memcpy((*result)->pattern, src->pattern, src->pattern_size * sizeof(float));
    (*result)->pattern_size = src->pattern_size;
    (*result)->confidence = src->confidence;
    (*result)->timestamp = src->timestamp;

    *similarity = best_similarity;

    nimcp_mutex_unlock(bank->mutex);

    return NIMCP_OK;
}

size_t nimcp_memory_bank_count(const nimcp_memory_bank_t* bank) {
    if (!bank) return 0;
    return bank->pattern_count;
}

void nimcp_memory_bank_clear(nimcp_memory_bank_t* bank) {
    if (!bank) return;

    nimcp_mutex_lock(bank->mutex);

    for (size_t i = 0; i < bank->pattern_count; i++) {
        nimcp_free(bank->patterns[i].pattern);
        bank->patterns[i].pattern = NULL;
    }
    bank->pattern_count = 0;

    nimcp_mutex_unlock(bank->mutex);

    LOG_DEBUG("Memory bank cleared");
}
```

### Step 3: Create Unit Tests

Create `/home/bbrelin/nimcp/test/unit/cognitive/test_memory_bank.cpp`:

```cpp
/**
 * @file test_memory_bank.cpp
 * @brief Unit tests for memory bank module
 */

#include <gtest/gtest.h>

extern "C" {
#include "cognitive/nimcp_memory_bank.h"
}

class MemoryBankTest : public ::testing::Test {
protected:
    nimcp_memory_bank_t* bank = nullptr;

    void SetUp() override {
        nimcp_memory_bank_config_t config = {
            .max_patterns = 100,
            .pattern_size = 10,
            .similarity_threshold = 0.8f
        };
        bank = nimcp_memory_bank_create(&config);
        ASSERT_NE(bank, nullptr);
    }

    void TearDown() override {
        nimcp_memory_bank_destroy(bank);
    }
};

TEST_F(MemoryBankTest, CreateAndDestroy) {
    // Bank created in SetUp, just verify it's valid
    EXPECT_EQ(nimcp_memory_bank_count(bank), 0);
}

TEST_F(MemoryBankTest, StoreAndRetrieve) {
    float pattern[10] = {1.0f, 0.0f, 1.0f, 0.0f, 1.0f,
                          0.0f, 1.0f, 0.0f, 1.0f, 0.0f};

    nimcp_error_t err = nimcp_memory_bank_store(bank, pattern, 10, 0.9f);
    EXPECT_EQ(err, NIMCP_OK);
    EXPECT_EQ(nimcp_memory_bank_count(bank), 1);

    // Retrieve with similar pattern
    float query[10] = {1.0f, 0.1f, 1.0f, 0.1f, 1.0f,
                        0.1f, 1.0f, 0.1f, 1.0f, 0.1f};
    nimcp_stored_pattern_t* result = nullptr;
    float similarity = 0.0f;

    err = nimcp_memory_bank_retrieve(bank, query, 10, &result, &similarity);
    EXPECT_EQ(err, NIMCP_OK);
    EXPECT_GT(similarity, 0.8f);
    EXPECT_NE(result, nullptr);

    // Cleanup
    if (result) {
        free(result->pattern);
        free(result);
    }
}

TEST_F(MemoryBankTest, ClearPatterns) {
    float pattern[10] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
                          1.0f, 1.0f, 1.0f, 1.0f, 1.0f};

    nimcp_memory_bank_store(bank, pattern, 10, 0.9f);
    EXPECT_EQ(nimcp_memory_bank_count(bank), 1);

    nimcp_memory_bank_clear(bank);
    EXPECT_EQ(nimcp_memory_bank_count(bank), 0);
}

TEST_F(MemoryBankTest, InvalidParams) {
    nimcp_error_t err;

    // Null bank
    err = nimcp_memory_bank_store(nullptr, nullptr, 0, 0.0f);
    EXPECT_EQ(err, NIMCP_ERROR_INVALID_PARAM);

    // Wrong pattern size
    float pattern[5] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
    err = nimcp_memory_bank_store(bank, pattern, 5, 0.9f);
    EXPECT_EQ(err, NIMCP_ERROR_INVALID_PARAM);
}
```

### Step 4: Update CMakeLists.txt

Add to `/home/bbrelin/nimcp/src/lib/CMakeLists.txt`:

```cmake
# Add memory bank source
list(APPEND NIMCP_SOURCES
    ${CMAKE_SOURCE_DIR}/src/cognitive/nimcp_memory_bank.c
)
```

Add to `/home/bbrelin/nimcp/test/unit/CMakeLists.txt`:

```cmake
# Memory bank tests
add_executable(test_memory_bank
    cognitive/test_memory_bank.cpp
)
target_link_libraries(test_memory_bank nimcp GTest::gtest_main)
add_test(NAME test_memory_bank COMMAND test_memory_bank)
```

### Step 5: Build and Test

```bash
cd /home/bbrelin/nimcp/build
cmake ..
make -j4
./test/unit/test_memory_bank
```

Expected output:

```
[==========] Running 4 tests from 1 test suite.
[----------] 4 tests from MemoryBankTest
[ RUN      ] MemoryBankTest.CreateAndDestroy
[       OK ] MemoryBankTest.CreateAndDestroy (0 ms)
[ RUN      ] MemoryBankTest.StoreAndRetrieve
[       OK ] MemoryBankTest.StoreAndRetrieve (1 ms)
[ RUN      ] MemoryBankTest.ClearPatterns
[       OK ] MemoryBankTest.ClearPatterns (0 ms)
[ RUN      ] MemoryBankTest.InvalidParams
[       OK ] MemoryBankTest.InvalidParams (0 ms)
[----------] 4 tests from MemoryBankTest (1 ms total)
[==========] 4 tests from 1 test suite ran. (1 ms total)
[  PASSED  ] 4 tests.
```

## Best Practices

1. **Follow naming conventions**: Use `nimcp_modulename_functionname` pattern
2. **Include proper error handling**: Return error codes, validate inputs
3. **Thread safety**: Use mutexes for shared state
4. **Memory management**: Always pair allocations with frees
5. **Documentation**: Document all public APIs with Doxygen comments
6. **Testing**: Write tests before or alongside implementation

## Next Steps

- Read [API Patterns](../claude/03-api-patterns.md) for more design guidelines
- See [Coding Standards](../claude/02-coding-standards.md) for style requirements
- Explore existing modules in `src/cognitive/` and `src/core/` for examples
