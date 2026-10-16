# NIMCP Security Validation Quick Start Guide

This guide shows how to add security validation to your NIMCP modules.

---

## When to Add Security Validation

Add BBB (Blood-Brain Barrier) validation when your module:

- ✅ Receives data from external sources (files, network, IPC)
- ✅ Deserializes data from buffers
- ✅ Parses user input or configuration
- ✅ Processes event data from other processes
- ✅ Loads training data or models

---

## Step 1: Add Security Header

At the top of your `.c` file, after other includes:

```c
#include "security/nimcp_blood_brain_barrier.h"
```

---

## Step 2: Add Global BBB System

After your includes and before functions:

```c
//=============================================================================
// Global Security System
//=============================================================================

// Global BBB security system
static bbb_system_t g_bbb_system = NULL;
```

---

## Step 3: Add Initialization Function

Add init and cleanup functions for your module:

```c
//=============================================================================
// Security Initialization
//=============================================================================

/**
 * @brief Initialize security subsystem for mymodule
 *
 * WHAT: Create and configure BBB system for input validation
 * WHY: Protect against malicious external input
 * HOW: Initialize with conservative security settings
 */
static void mymodule_security_init(void) {
    if (g_bbb_system) {
        return;  // Already initialized
    }

    bbb_config_t config = bbb_default_config();
    config.strict_mode = false;  // Don't block, just log
    config.default_action = BBB_ACTION_LOG;
    config.input.validate_strings = true;
    config.input.validate_integers = true;
    config.input.max_string_length = 4096;  // Adjust as needed

    g_bbb_system = bbb_system_create(&config);
    if (!g_bbb_system) {
        NIMCP_LOG_ERROR("mymodule: Failed to initialize security");
    } else {
        NIMCP_LOG_INFO("mymodule: Security subsystem initialized");
    }
}

/**
 * @brief Cleanup security subsystem
 */
static void mymodule_security_cleanup(void) {
    if (g_bbb_system) {
        bbb_system_destroy(g_bbb_system);
        g_bbb_system = NULL;
    }
}
```

---

## Step 4: Call Init in Module Init

In your module's initialization function:

```c
mymodule_t mymodule_create(config_t* config) {
    // ... existing code ...

    // Initialize security
    mymodule_security_init();

    // ... rest of initialization ...
}
```

---

## Step 5: Call Cleanup in Module Cleanup

In your module's cleanup/destroy function:

```c
void mymodule_destroy(mymodule_t module) {
    // ... existing cleanup ...

    // Cleanup security
    mymodule_security_cleanup();

    // ... rest of cleanup ...
}
```

---

## Step 6: Add Validation at Entry Points

### For Buffer/Data Input

```c
nimcp_result_t mymodule_process_data(const uint8_t* data, size_t size) {
    // Guard clauses
    if (!data || size == 0) {
        return NIMCP_ERROR_INVALID_INPUT;
    }

    // BBB: Validate external input
    bbb_validation_result_t val_result = {0};
    if (!bbb_validate_input(g_bbb_system, data, size, &val_result)) {
        NIMCP_LOG_WARN("Input validation warning: %s", val_result.reason);
        // In log-only mode, continue processing
        // In strict mode, would return error here
    }

    // ... process data ...
}
```

### For String Input

```c
nimcp_result_t mymodule_process_string(const char* str) {
    // Guard clause
    if (!str) {
        return NIMCP_ERROR_INVALID_INPUT;
    }

    // BBB: Validate string input
    bbb_validation_result_t val_result = {0};
    if (!bbb_validate_string(g_bbb_system, str, &val_result)) {
        NIMCP_LOG_ERROR("String validation failed: %s", val_result.reason);
        return NIMCP_ERROR_INVALID_INPUT;
    }

    // ... process string ...
}
```

### For Integer Input

```c
nimcp_result_t mymodule_set_parameter(int64_t value) {
    // BBB: Validate integer input
    bbb_validation_result_t val_result = {0};
    if (!bbb_validate_integer(g_bbb_system, value, &val_result)) {
        NIMCP_LOG_ERROR("Integer validation failed: %s", val_result.reason);
        return NIMCP_ERROR_INVALID_INPUT;
    }

    // ... use value ...
}
```

### For Pointer Validation

```c
nimcp_result_t mymodule_process_buffer(void* buffer, size_t expected_size) {
    // BBB: Validate pointer
    bbb_validation_result_t val_result = {0};
    if (!bbb_validate_pointer(g_bbb_system, buffer, expected_size, &val_result)) {
        NIMCP_LOG_ERROR("Pointer validation failed: %s", val_result.reason);
        return NIMCP_ERROR_INVALID_INPUT;
    }

    // ... use buffer ...
}
```

---

## Step 7: Add Sanitization (Optional)

For untrusted strings that need to be used:

```c
nimcp_result_t mymodule_process_untrusted_string(const char* untrusted) {
    char sanitized[256];

    // Sanitize the input
    ssize_t len = bbb_sanitize_string(g_bbb_system, untrusted,
                                       sanitized, sizeof(sanitized));
    if (len < 0) {
        NIMCP_LOG_ERROR("String sanitization failed");
        return NIMCP_ERROR_INVALID_INPUT;
    }

    // Use sanitized string
    // ... process sanitized ...
}
```

---

## Complete Example

Here's a complete example for a network message handler:

```c
#include "mymodule.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "utils/logging/nimcp_logging.h"

#define LOG_MODULE "MYMODULE"

//=============================================================================
// Global Security System
//=============================================================================

static bbb_system_t g_bbb_system = NULL;

//=============================================================================
// Security Initialization
//=============================================================================

static void mymodule_security_init(void) {
    if (g_bbb_system) return;

    bbb_config_t config = bbb_default_config();
    config.strict_mode = false;
    config.default_action = BBB_ACTION_LOG;
    config.input.validate_strings = true;
    config.input.max_string_length = 4096;

    g_bbb_system = bbb_system_create(&config);
    if (!g_bbb_system) {
        NIMCP_LOG_ERROR("Failed to initialize security");
    }
}

static void mymodule_security_cleanup(void) {
    if (g_bbb_system) {
        bbb_system_destroy(g_bbb_system);
        g_bbb_system = NULL;
    }
}

//=============================================================================
// Module Functions
//=============================================================================

mymodule_t mymodule_create(void) {
    mymodule_security_init();
    // ... create module ...
}

void mymodule_destroy(mymodule_t module) {
    // ... cleanup module ...
    mymodule_security_cleanup();
}

nimcp_result_t mymodule_process_message(const uint8_t* msg, size_t size) {
    // Guard clauses
    if (!msg || size == 0) {
        return NIMCP_ERROR_INVALID_INPUT;
    }

    // BBB: Validate input
    bbb_validation_result_t val_result = {0};
    if (!bbb_validate_input(g_bbb_system, msg, size, &val_result)) {
        NIMCP_LOG_WARN("Message validation warning: %s", val_result.reason);
    }

    // Process message
    // ...

    return NIMCP_SUCCESS;
}
```

---

## Configuration Options

### Strict vs. Permissive Mode

```c
// Permissive (default): Log threats but continue
config.strict_mode = false;
config.default_action = BBB_ACTION_LOG;

// Strict: Block threats immediately
config.strict_mode = true;
config.default_action = BBB_ACTION_BLOCK;
```

### Input Validation Settings

```c
config.input.validate_strings = true;       // Enable string validation
config.input.validate_integers = true;      // Enable integer validation
config.input.validate_pointers = true;      // Enable pointer validation
config.input.max_string_length = 4096;      // Max string length
config.input.max_array_size = 1000000;      // Max array elements
config.input.min_integer = -2147483648;     // Min int value
config.input.max_integer = 2147483647;      // Max int value
```

### Sanitization Settings

```c
config.input.sanitize_html = true;          // Remove HTML tags
config.input.sanitize_sql = true;           // Escape SQL characters
```

---

## Threat Levels

BBB reports threats at different severity levels:

```c
typedef enum {
    BBB_SEVERITY_NONE = 0,      // No threat
    BBB_SEVERITY_LOW = 1,       // Log only
    BBB_SEVERITY_MEDIUM = 2,    // Block and alert
    BBB_SEVERITY_HIGH = 3,      // Quarantine and alert
    BBB_SEVERITY_CRITICAL = 4   // System lockdown
} bbb_severity_t;
```

---

## Common Threats Detected

1. **Buffer Overflow** - Input exceeds expected size
2. **Format String** - Contains format specifiers (%s, %d, etc.)
3. **Integer Overflow** - Value exceeds safe range
4. **SQL Injection** - Contains SQL commands or escape sequences
5. **Code Injection** - Contains shell commands or escape sequences
6. **Path Traversal** - Contains `../` or absolute paths
7. **Shell Injection** - Contains shell metacharacters

---

## Best Practices

### DO:
- ✅ Initialize security in module init
- ✅ Cleanup security in module destroy
- ✅ Validate all external input
- ✅ Log validation failures
- ✅ Use guard clauses before validation
- ✅ Check validation result
- ✅ Use appropriate validation for data type

### DON'T:
- ❌ Skip security initialization
- ❌ Assume input is safe
- ❌ Ignore validation failures
- ❌ Use unsanitized strings in format functions
- ❌ Trust network data without validation
- ❌ Hardcode security policies

---

## Testing Your Security Validation

### Unit Test Template

```c
void test_mymodule_security_validation(void) {
    // Test with valid input
    uint8_t valid_data[] = {0x01, 0x02, 0x03};
    assert(mymodule_process_data(valid_data, sizeof(valid_data)) == NIMCP_SUCCESS);

    // Test with NULL input
    assert(mymodule_process_data(NULL, 100) == NIMCP_ERROR_INVALID_INPUT);

    // Test with empty input
    assert(mymodule_process_data(valid_data, 0) == NIMCP_ERROR_INVALID_INPUT);

    // Test with oversized input (if applicable)
    uint8_t large_data[100000];
    // Should log warning but may continue (permissive mode)
    mymodule_process_data(large_data, sizeof(large_data));
}
```

---

## Troubleshooting

### Problem: Security init fails
**Solution:** Check that `bbb_default_config()` returns valid config

### Problem: Too many false positives
**Solution:** Adjust validation thresholds or use permissive mode

### Problem: Performance impact
**Solution:** Profile validation calls, consider lazy initialization

### Problem: Validation not triggered
**Solution:** Ensure security_init() is called before use

---

## Getting Help

- **Documentation:** See `docs/SECURITY_EXPANSION_REPORT.md`
- **Examples:** Check `src/io/serialization/` for reference implementations
- **Analysis:** Run `scripts/analyze_security_coverage.py` to check coverage

---

**Last Updated:** 2025-12-08
**Version:** 1.0
