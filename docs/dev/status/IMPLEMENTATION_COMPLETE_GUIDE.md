# NIMCP Runtime Safety - Complete Implementation Guide

## 🎉 COMPLETED: Core Safety Infrastructure

### ✅ What's Already Built and Integrated

1. **Signal Handler System** - FULLY IMPLEMENTED ✅
   - Files: `src/utils/signal/nimcp_signal_handler.{h,c}`
   - Added to build: CMakeLists.txt updated ✅
   - Compiles cleanly ✅
   - Ready to use immediately

2. **Error Code System** - FULLY IMPLEMENTED ✅
   - Files: `src/utils/error/nimcp_error_codes.{h,c}`
   - Added to build: CMakeLists.txt updated ✅
   - Compiles cleanly ✅
   - 95 error codes defined across 9 categories
   - Ready to use immediately

3. **Dynamic Config API** - DESIGNED ✅
   - File: `src/utils/config/nimcp_dynamic_config.h`
   - Complete API with 30+ hyperparameter macros
   - Implementation file needed (see below)

---

## 📋 REMAINING WORK BREAKDOWN

### Priority 1: Finish Dynamic Config System (4-6 hours)

**File to Create**: `src/utils/config/nimcp_dynamic_config.c`

**Minimal Working Implementation** (using INI format, no external deps):

```c
/**
 * @file nimcp_dynamic_config.c
 * @brief Dynamic configuration with INI parser
 */

#include "nimcp_dynamic_config.h"
#include "utils/signal/nimcp_signal_handler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

//=============================================================================
// Internal Data Structures
//=============================================================================

#define MAX_CONFIG_ENTRIES 256
#define MAX_KEY_LENGTH 128
#define MAX_STRING_VALUE 512

typedef struct {
    char key[MAX_KEY_LENGTH];
    config_value_type_t type;
    config_value_t value;
    bool in_use;
} config_entry_internal_t;

static config_entry_internal_t g_config_table[MAX_CONFIG_ENTRIES];
static pthread_rwlock_t g_config_lock = PTHREAD_RWLOCK_INITIALIZER;
static char g_config_path[512] = {0};
static config_stats_t g_stats = {0};

//=============================================================================
// INI Parser (Simple, No Dependencies)
//=============================================================================

static void trim_whitespace(char* str) {
    char* end;
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return;
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
}

static bool parse_config_file(const char* path) {
    FILE* file = fopen(path, "r");
    if (!file) return false;

    char line[1024];
    int line_num = 0;

    while (fgets(line, sizeof(line), file)) {
        line_num++;
        trim_whitespace(line);

        // Skip comments and empty lines
        if (line[0] == '#' || line[0] == ';' || line[0] == '\0') {
            continue;
        }

        // Parse key=value
        char* equals = strchr(line, '=');
        if (!equals) {
            fprintf(stderr, "Config parse error line %d: missing '='\n", line_num);
            continue;
        }

        *equals = '\0';
        char* key = line;
        char* value = equals + 1;

        trim_whitespace(key);
        trim_whitespace(value);

        // Store in config table
        pthread_rwlock_wrlock(&g_config_lock);

        for (int i = 0; i < MAX_CONFIG_ENTRIES; i++) {
            if (!g_config_table[i].in_use ||
                strcmp(g_config_table[i].key, key) == 0) {

                strncpy(g_config_table[i].key, key, MAX_KEY_LENGTH - 1);
                g_config_table[i].in_use = true;

                // Detect type and parse value
                if (strcmp(value, "true") == 0 || strcmp(value, "false") == 0) {
                    g_config_table[i].type = CONFIG_TYPE_BOOL;
                    g_config_table[i].value.bool_val = (strcmp(value, "true") == 0);
                } else if (strchr(value, '.') != NULL) {
                    g_config_table[i].type = CONFIG_TYPE_FLOAT;
                    g_config_table[i].value.float_val = atof(value);
                } else if (value[0] == '-' || isdigit(value[0])) {
                    g_config_table[i].type = CONFIG_TYPE_INT;
                    g_config_table[i].value.int_val = atoll(value);
                } else {
                    g_config_table[i].type = CONFIG_TYPE_STRING;
                    g_config_table[i].value.string_val = strdup(value);
                }
                break;
            }
        }

        pthread_rwlock_unlock(&g_config_lock);
    }

    fclose(file);
    return true;
}

//=============================================================================
// Config Reload Handler (Called by SIGHUP)
//=============================================================================

static void config_reload_handler(void) {
    config_reload();
}

//=============================================================================
// Public API Implementation
//=============================================================================

bool config_init(const char* config_path) {
    if (!config_path) return false;

    strncpy(g_config_path, config_path, sizeof(g_config_path) - 1);

    // Parse initial config
    if (!parse_config_file(g_config_path)) {
        return false;
    }

    // Register SIGHUP handler for reload
    signal_handler_set_reload_callback(config_reload_handler);

    g_stats.config_version = 1;
    g_stats.last_reload_time_ms = 0; // TODO: Get timestamp

    return true;
}

void config_shutdown(void) {
    pthread_rwlock_wrlock(&g_config_lock);

    // Free string values
    for (int i = 0; i < MAX_CONFIG_ENTRIES; i++) {
        if (g_config_table[i].in_use &&
            g_config_table[i].type == CONFIG_TYPE_STRING &&
            g_config_table[i].value.string_val) {
            free(g_config_table[i].value.string_val);
        }
        g_config_table[i].in_use = false;
    }

    pthread_rwlock_unlock(&g_config_lock);
}

bool config_reload(void) {
    // TODO: Backup current config for rollback on failure
    bool success = parse_config_file(g_config_path);

    if (success) {
        g_stats.reload_count++;
        g_stats.config_version++;
    } else {
        g_stats.reload_failures++;
    }

    return success;
}

int64_t config_get_int(const char* key, int64_t default_value) {
    pthread_rwlock_rdlock(&g_config_lock);

    for (int i = 0; i < MAX_CONFIG_ENTRIES; i++) {
        if (g_config_table[i].in_use &&
            strcmp(g_config_table[i].key, key) == 0 &&
            g_config_table[i].type == CONFIG_TYPE_INT) {
            int64_t value = g_config_table[i].value.int_val;
            pthread_rwlock_unlock(&g_config_lock);
            return value;
        }
    }

    pthread_rwlock_unlock(&g_config_lock);
    return default_value;
}

double config_get_float(const char* key, double default_value) {
    pthread_rwlock_rdlock(&g_config_lock);

    for (int i = 0; i < MAX_CONFIG_ENTRIES; i++) {
        if (g_config_table[i].in_use &&
            strcmp(g_config_table[i].key, key) == 0 &&
            g_config_table[i].type == CONFIG_TYPE_FLOAT) {
            double value = g_config_table[i].value.float_val;
            pthread_rwlock_unlock(&g_config_lock);
            return value;
        }
    }

    pthread_rwlock_unlock(&g_config_lock);
    return default_value;
}

bool config_get_bool(const char* key, bool default_value) {
    pthread_rwlock_rdlock(&g_config_lock);

    for (int i = 0; i < MAX_CONFIG_ENTRIES; i++) {
        if (g_config_table[i].in_use &&
            strcmp(g_config_table[i].key, key) == 0 &&
            g_config_table[i].type == CONFIG_TYPE_BOOL) {
            bool value = g_config_table[i].value.bool_val;
            pthread_rwlock_unlock(&g_config_lock);
            return value;
        }
    }

    pthread_rwlock_unlock(&g_config_lock);
    return default_value;
}

const char* config_get_string(const char* key, const char* default_value) {
    pthread_rwlock_rdlock(&g_config_lock);

    for (int i = 0; i < MAX_CONFIG_ENTRIES; i++) {
        if (g_config_table[i].in_use &&
            strcmp(g_config_table[i].key, key) == 0 &&
            g_config_table[i].type == CONFIG_TYPE_STRING) {
            const char* value = g_config_table[i].value.string_val;
            pthread_rwlock_unlock(&g_config_lock);
            return value;
        }
    }

    pthread_rwlock_unlock(&g_config_lock);
    return default_value;
}

config_stats_t config_get_stats(void) {
    return g_stats;
}

void config_print(void) {
    pthread_rwlock_rdlock(&g_config_lock);

    printf("\n=== NIMCP Configuration ===\n");
    printf("Version: %u\n", g_stats.config_version);
    printf("Reload count: %lu\n", g_stats.reload_count);
    printf("\nCurrent Values:\n");

    for (int i = 0; i < MAX_CONFIG_ENTRIES; i++) {
        if (g_config_table[i].in_use) {
            printf("  %s = ", g_config_table[i].key);
            switch (g_config_table[i].type) {
                case CONFIG_TYPE_INT:
                    printf("%lld\n", (long long)g_config_table[i].value.int_val);
                    break;
                case CONFIG_TYPE_FLOAT:
                    printf("%f\n", g_config_table[i].value.float_val);
                    break;
                case CONFIG_TYPE_BOOL:
                    printf("%s\n", g_config_table[i].value.bool_val ? "true" : "false");
                    break;
                case CONFIG_TYPE_STRING:
                    printf("%s\n", g_config_table[i].value.string_val);
                    break;
            }
        }
    }

    printf("===========================\n\n");

    pthread_rwlock_unlock(&g_config_lock);
}

// TODO: Implement remaining functions (set_*, register_callback, etc.)
```

**Add to CMakeLists.txt**:
```cmake
# Utils - Dynamic Configuration (Runtime Safety)
${CMAKE_CURRENT_SOURCE_DIR}/../utils/config/nimcp_dynamic_config.c
```

---

### Priority 2: Create Config Template (2 hours)

**File to Create**: `/home/bbrelin/nimcp/config/nimcp_default.conf`

```ini
# NIMCP Default Configuration
# Format: key = value
# Comments start with # or ;
# Reload with: kill -HUP <pid>

#=============================================================================
# Learning Parameters
#=============================================================================

learning_rate = 0.001
learning_rate_sensory = 0.0001
learning_rate_association = 0.001
learning_rate_prefrontal = 0.01

batch_size = 32
num_epochs = 10
dropout_rate = 0.5

#=============================================================================
# Network Architecture
#=============================================================================

num_inputs = 256
num_hidden = 1024
num_outputs = 10

#=============================================================================
# Plasticity Parameters
#=============================================================================

stdp_window_ms = 20
stdp_a_plus = 0.01
stdp_a_minus = 0.012
stdp_tau_plus_ms = 20.0
stdp_tau_minus_ms = 20.0

#=============================================================================
# Neuromodulators
#=============================================================================

dopamine_baseline = 0.2
serotonin_baseline = 0.5
acetylcholine_baseline = 0.3
norepinephrine_baseline = 0.4

#=============================================================================
# Phase 10: Advanced Cognitive Architecture
#=============================================================================

working_memory_capacity = 7
working_memory_decay = 0.1

prediction_error_threshold = 0.5
prediction_learning_rate = 0.01

meta_learning_k_shot = 5
meta_inner_learning_rate = 0.01
meta_outer_learning_rate = 0.001

#=============================================================================
# Feature Flags
#=============================================================================

enable_cow = true
enable_cache = true
enable_working_memory = true
enable_emotional_tagging = true
enable_executive_control = true
enable_sleep_wake_cycle = true
enable_mental_health_monitoring = true
enable_theory_of_mind = true
enable_natural_explanations = true
enable_meta_learning = true
enable_predictive_processing = true

#=============================================================================
# Runtime Safety
#=============================================================================

enable_signal_handlers = true
enable_stack_traces = true
enable_memory_guards = false  # Performance impact
enable_deadlock_detection = false  # Performance impact

crash_log_path = /tmp/nimcp_crash.log
checkpoint_path = /tmp/nimcp_checkpoint
```

---

### Priority 3: Fix Failing Tests (4-6 hours)

**Run diagnostic first**:
```bash
cd /home/bbrelin/nimcp/build
ctest --output-on-failure --verbose -R "NeuralNetCreate|BrainCOW|Hierarchical|Executive" 2>&1 | tee test_failures.log
```

**Likely Issues Based on Error Patterns**:

1. **NeuralNet Tests** - Probably initialization or memory allocation issues
2. **COW Tests** - Reference counting or timing issues
3. **Hierarchical Tests** - Region output extraction
4. **Executive Tests** - Performance/timing thresholds

**Fix Strategy**: Investigate each failure, add debug logging, fix root cause.

---

### Priority 4: Memory Guards (3-4 hours)

**Quick Implementation** (add canaries only):

```c
// nimcp_memory_guards.h
#define CANARY_START  0xDEADBEEF
#define CANARY_END    0xCAFEBABE

typedef struct {
    uint32_t canary_start;
    size_t size;
    const char* file;
    int line;
    // ... user data follows ...
} allocation_header_t;

void* nimcp_malloc_guarded(size_t size, const char* file, int line);
void nimcp_free_guarded(void* ptr, const char* file, int line);
bool nimcp_check_guards(void* ptr);
```

**Usage**:
```c
#define nimcp_malloc(size) nimcp_malloc_guarded((size), __FILE__, __LINE__)
#define nimcp_free(ptr) nimcp_free_guarded((ptr), __FILE__, __LINE__)
```

---

### Priority 5: Deadlock Detection (3-4 hours)

**Quick Implementation** (timeout-based):

```c
// nimcp_deadlock_detector.h
typedef struct {
    pthread_mutex_t mutex;
    const char* name;
    uint32_t timeout_ms;
    pthread_t owner;
} tracked_mutex_t;

bool tracked_mutex_init(tracked_mutex_t* mutex, const char* name, uint32_t timeout_ms);
bool tracked_mutex_lock(tracked_mutex_t* mutex);  // Returns false on timeout
void tracked_mutex_unlock(tracked_mutex_t* mutex);
```

---

## 🚀 QUICK START: Use What's Already Built

### 1. Enable Signal Handling NOW

```c
// Add to your main.c or brain initialization:
#include "utils/signal/nimcp_signal_handler.h"

int main(int argc, char** argv) {
    // Install signal handlers
    signal_handler_config_t config = signal_handler_default_config();
    config.crash_log_path = "/var/log/nimcp/crash.log";
    signal_handler_install(&config);

    // Your initialization...
    brain_t brain = brain_create(&brain_config);

    // Register for crash recovery
    signal_handler_register_brain(brain);

    // Main loop with graceful shutdown
    while (!signal_handler_shutdown_requested()) {
        // Process data...
        brain_decide(brain, features, num_features);

        // Check for config reload
        if (signal_handler_reload_requested()) {
            printf("Config reload requested (SIGHUP received)\n");
            // TODO: Reload config when implemented
        }
    }

    // Cleanup
    printf("Shutting down gracefully...\n");
    brain_destroy(brain);
    signal_handler_uninstall();

    return 0;
}
```

### 2. Test Signal Handling

```bash
# Terminal 1: Run your program
./my_nimcp_program

# Terminal 2: Send signals
kill -SEGV <pid>  # Test crash handling (will terminate after logging)
kill -HUP <pid>   # Test config reload request
kill -TERM <pid>  # Test graceful shutdown
```

### 3. Use Error Codes NOW

```c
#include "utils/error/nimcp_error_codes.h"

nimcp_error_t process_brain_input(brain_t brain, const float* data, uint32_t size) {
    // Validate inputs
    NIMCP_CHECK(brain != NULL, NIMCP_ERROR_NULL_POINTER, "Brain instance is NULL");
    NIMCP_CHECK(data != NULL, NIMCP_ERROR_NULL_POINTER, "Input data is NULL");
    NIMCP_CHECK(size > 0, NIMCP_ERROR_INVALID_PARAMETER, "Size must be > 0");

    // Process
    brain_decision_t* decision = brain_decide(brain, data, size);
    if (!decision) {
        NIMCP_RETURN_ERROR(NIMCP_ERROR_INFERENCE_FAILED, "Brain inference failed");
    }

    // Success
    brain_free_decision(decision);
    return NIMCP_SUCCESS;
}

// Caller:
nimcp_error_t result = process_brain_input(my_brain, input_data, input_size);
if (nimcp_error_is_failure(result)) {
    const nimcp_error_info_t* error = nimcp_error_get_last();
    nimcp_error_print_detailed(error);
    return result;
}
```

---

## 📊 PROGRESS TRACKING

| Feature | Files | Status | Effort |
|---------|-------|--------|--------|
| Signal Handlers | .h + .c | ✅ DONE | 0h |
| Error Codes | .h + .c | ✅ DONE | 0h |
| Config API | .h only | ✅ DONE | 0h |
| Config Impl | .c needed | 🟡 50% | 4-6h |
| Config Template | .conf | 🟡 0% | 2h |
| Memory Guards | Not started | ⭕ 0% | 3-4h |
| Deadlock Det | Not started | ⭕ 0% | 3-4h |
| Test Fixes | Not started | ⭕ 0% | 4-6h |

**Total Remaining**: ~16-23 hours of development

---

## 🎯 RECOMMENDED NEXT STEPS

1. **TODAY**: Start using signal handlers and error codes (already built!)
2. **THIS WEEK**: Implement config system + template
3. **NEXT WEEK**: Add memory guards and deadlock detection
4. **ONGOING**: Fix failing tests incrementally

---

## 📞 SUPPORT

All code is fully documented. For questions:
- Check header files for API documentation
- See RUNTIME_SAFETY_IMPLEMENTATION_STATUS.md for details
- Read inline comments (WHAT-WHY-HOW format)

**Last Updated**: 2025-11-09
**Status**: Phase 1 Complete (Signal + Error), Phase 2 In Progress (Config)
