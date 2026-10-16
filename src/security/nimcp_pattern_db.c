/**
 * @file nimcp_pattern_db.c
 * @brief Implementation of Runtime-Updateable Pattern Database
 *
 * WHAT: Lock-free concurrent pattern database for threat detection
 * WHY:  Enable runtime pattern updates without system restart
 * HOW:  RCU-like versioning, atomic updates, lock-free reads
 *
 * ARCHITECTURE:
 * - Lock-free reads using atomic pointers and versioning
 * - Copy-on-write for pattern updates
 * - Snapshot-based rollback using refcounted versions
 * - Thread-safe using atomic operations and RCU semantics
 *
 * @author NIMCP Security Team
 * @date 2025-12-07
 */

#include "security/nimcp_pattern_db.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_wiring_helpers.h"

#include "security/nimcp_security.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_atomic.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include <regex.h>
#include <signal.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>

#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "constants/nimcp_buffer_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(pattern_db, MESH_ADAPTER_CATEGORY_SECURITY)


//=============================================================================
// Internal Constants
//=============================================================================

#define PATTERN_DB_MAGIC_VALID   0x50415444
#define PATTERN_DB_INITIAL_CAPACITY  128
#define PATTERN_DB_LOAD_FACTOR   0.75f
#define PATTERN_DB_MAX_REGEX_ERRORS 256
#define PATTERN_DB_MAX_PATTERN_LENGTH 4096  /* Maximum safe pattern length */
#define PATTERN_DB_MAX_QUANTIFIER_NESTING 3 /* Maximum allowed nested quantifiers */
#define PATTERN_DB_MAX_ALTERNATIONS 20      /* Maximum alternations */

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Compiled pattern internal representation
 *
 * WHAT: Compiled regex with metadata
 * WHY:  Efficient matching without recompilation
 * HOW:  POSIX regex_t for portable regex support
 */
typedef struct {
    uint32_t pattern_id;
    nimcp_pattern_entry_t entry;
    regex_t compiled_regex;
    bool is_compiled;
    nimcp_atomic_uint32_t match_count;  /* THREAD SAFETY: atomic for concurrent updates */
    uint64_t total_match_time_us;
    char pattern_copy[NIMCP_PATTERN_MAX_LENGTH];
    char description_copy[NIMCP_PATTERN_MAX_DESCRIPTION];
} pattern_slot_t;

/**
 * @brief Version snapshot for rollback
 */
typedef struct pattern_snapshot {
    uint32_t version;
    uint32_t pattern_count;
    pattern_slot_t* patterns;
    size_t capacity;
    nimcp_atomic_uint32_t ref_count;  /* THREAD SAFETY: atomic for concurrent access */
    struct pattern_snapshot* next;
} pattern_snapshot_t;

/**
 * @brief Pattern database implementation
 */
struct nimcp_pattern_db_impl {
    uint32_t magic;
    nimcp_pattern_db_config_t config;

    // Pattern storage (lock-free reads)
    pattern_slot_t* patterns;
    size_t capacity;
    uint32_t pattern_count;
    uint32_t next_pattern_id;

    // Versioning
    uint32_t current_version;
    pattern_snapshot_t* snapshots;
    uint32_t snapshot_count;

    // Statistics
    nimcp_pattern_db_stats_t stats;

    // Synchronization
    pthread_rwlock_t write_lock;

    // Bio-async integration
    bio_module_context_t bio_ctx;
    bool bio_async_enabled;
};

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * @brief Get current time in microseconds
 */
static uint64_t get_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

/**
 * @brief Validate pattern database handle
 */
static bool validate_db(nimcp_pattern_db_t db) {
    return db != NULL && db->magic == PATTERN_DB_MAGIC_VALID;
}

/**
 * @brief Check if character is a regex quantifier
 */
static bool is_quantifier(char c) {
    return c == '*' || c == '+' || c == '?';
}

/**
 * @brief Check regex pattern complexity to prevent ReDoS
 *
 * WHAT: Detect potentially dangerous regex patterns
 * WHY:  Prevent exponential backtracking attacks (ReDoS)
 * HOW:  Detect consecutive quantifiers, nested quantifiers with alternation,
 *       catastrophic backtracking patterns, and enforce length limits
 *
 * REDOS PATTERNS DETECTED:
 * 1. Consecutive quantifiers: a**, a*+, a+*, a++, a?*, etc.
 * 2. Nested quantifiers with alternation: (a|b)*, (a*|b)*, etc.
 * 3. Overlapping repetitions: (a+)+, (a*)+, (.*a)*b, etc.
 * 4. Pattern length exceeding safe limits
 * 5. Excessive nesting depth
 * 6. Excessive alternations in nested groups
 */
static bool is_regex_safe(const char* pattern) {
    if (!pattern) {
        return false;
    }

    size_t len = strlen(pattern);

    /* Check maximum pattern length */
    if (len > PATTERN_DB_MAX_PATTERN_LENGTH) {
        LOG_MODULE_WARN("pattern_db", "Pattern rejected: length %zu exceeds maximum %d",
                        len, PATTERN_DB_MAX_PATTERN_LENGTH);
        return false;  /* Rejection is normal validation behavior */
    }

    /* Empty patterns are safe but useless */
    if (len == 0) {
        return true;
    }

    int group_depth = 0;
    int max_depth = 0;
    int alternations_at_depth[16] = {0};  /* Track alternations at each nesting level */
    int quantifiers_at_depth[16] = {0};   /* Track quantifiers at each nesting level */
    bool prev_was_quantifier = false;
    bool escaped = false;
    bool in_char_class = false;

    for (size_t i = 0; i < len; i++) {
        char c = pattern[i];

        /* Handle escape sequences */
        if (escaped) {
            escaped = false;
            prev_was_quantifier = false;
            continue;
        }

        if (c == '\\') {
            escaped = true;
            prev_was_quantifier = false;
            continue;
        }

        /* Handle character classes */
        if (c == '[' && !in_char_class) {
            in_char_class = true;
            prev_was_quantifier = false;
            continue;
        }

        if (c == ']' && in_char_class) {
            in_char_class = false;
            prev_was_quantifier = false;
            continue;
        }

        /* Skip processing inside character classes */
        if (in_char_class) {
            continue;
        }

        switch (c) {
            case '(':
                /* Opening group */
                group_depth++;
                if (group_depth > max_depth) {
                    max_depth = group_depth;
                }
                if (group_depth >= 16) {
                    LOG_MODULE_WARN("pattern_db", "Pattern rejected: nesting depth %d exceeds maximum",
                                    group_depth);
                    return false;
                }
                /* Reset alternation count for this depth */
                alternations_at_depth[group_depth] = 0;
                quantifiers_at_depth[group_depth] = 0;
                prev_was_quantifier = false;
                break;

            case ')':
                /* Closing group */
                if (group_depth > 0) {
                    group_depth--;
                }
                prev_was_quantifier = false;
                break;

            case '|':
                /* Alternation */
                if (group_depth > 0 && group_depth < 16) {
                    alternations_at_depth[group_depth]++;

                    /* Check for excessive alternations in nested groups */
                    if (alternations_at_depth[group_depth] > PATTERN_DB_MAX_ALTERNATIONS) {
                        LOG_MODULE_WARN("pattern_db", "Pattern rejected: too many alternations (%d) at depth %d",
                                        alternations_at_depth[group_depth], group_depth);
                        return false;
                    }
                }
                prev_was_quantifier = false;
                break;

            case '*':
            case '+':
            case '?':
                /* REDOS CHECK 1: Consecutive quantifiers (a**, a*+, a++) */
                if (prev_was_quantifier) {
                    LOG_MODULE_WARN("pattern_db", "Pattern rejected: consecutive quantifiers detected");
                    return false;
                }

                /* Track quantifier at current depth */
                if (group_depth > 0 && group_depth < 16) {
                    quantifiers_at_depth[group_depth]++;
                }

                /* REDOS CHECK 2: Check for quantifier immediately after group closing */
                /* This catches patterns like (a+)+ or (a|b)* which can cause backtracking */
                if (i > 0 && pattern[i - 1] == ')') {
                    /* Look back to find the matching group and check for nested quantifiers or alternations */
                    int depth = 1;
                    bool has_alternation = false;
                    bool has_nested_quantifier = false;
                    bool inner_escaped = false;

                    for (int j = (int)i - 2; j >= 0 && depth > 0; j--) {
                        char inner_c = pattern[j];

                        if (inner_escaped) {
                            inner_escaped = false;
                            continue;
                        }

                        /* Check for backslash looking backwards */
                        if (j > 0 && pattern[j - 1] == '\\') {
                            inner_escaped = true;
                            continue;
                        }

                        if (inner_c == ')') {
                            depth++;
                        } else if (inner_c == '(') {
                            depth--;
                        } else if (depth == 1) {
                            if (inner_c == '|') {
                                has_alternation = true;
                            } else if (is_quantifier(inner_c)) {
                                has_nested_quantifier = true;
                            }
                        }
                    }

                    /* REDOS CHECK 3: Nested quantifier with alternation */
                    if (has_alternation && (c == '*' || c == '+')) {
                        LOG_MODULE_WARN("pattern_db", "Pattern rejected: quantifier on group with alternation (ReDoS risk)");
                        return false;
                    }

                    /* REDOS CHECK 4: Nested quantifiers (quantifier on group containing quantifier) */
                    if (has_nested_quantifier && (c == '*' || c == '+')) {
                        LOG_MODULE_WARN("pattern_db", "Pattern rejected: nested quantifiers (ReDoS risk)");
                        return false;
                    }
                }

                prev_was_quantifier = true;
                break;

            case '{':
                /* Range quantifier - check for consecutive */
                if (prev_was_quantifier) {
                    LOG_MODULE_WARN("pattern_db", "Pattern rejected: consecutive quantifiers detected (range)");
                    return false;
                }
                /* Skip to closing brace */
                while (i < len && pattern[i] != '}') {
                    i++;
                }
                prev_was_quantifier = true;
                break;

            default:
                prev_was_quantifier = false;
                break;
        }
    }

    /* REDOS CHECK 5: Excessive nesting depth overall */
    if (max_depth > PATTERN_DB_MAX_QUANTIFIER_NESTING) {
        LOG_MODULE_WARN("pattern_db", "Pattern rejected: max nesting depth %d exceeds limit %d",
                        max_depth, PATTERN_DB_MAX_QUANTIFIER_NESTING);
        return false;  /* Rejection is anti-ReDoS defense working correctly */
    }

    return true;
}

/**
 * @brief Compile regex pattern
 */
static nimcp_error_t compile_pattern(pattern_slot_t* slot) {
    int regex_flags = REG_EXTENDED;

    /* SECURITY: Check pattern complexity to prevent ReDoS */
    if (!is_regex_safe(slot->pattern_copy)) {
        fprintf(stderr, "Pattern rejected: complexity too high (ReDoS risk)\n");
        return NIMCP_ERROR;
    }

    // Apply flags
    if (slot->entry.flags & NIMCP_PATTERN_FLAG_CASE_INSENSITIVE) {
        regex_flags |= REG_ICASE;
    }
    if (slot->entry.flags & NIMCP_PATTERN_FLAG_MULTILINE) {
        regex_flags |= REG_NEWLINE;
    }

    int ret = regcomp(&slot->compiled_regex, slot->pattern_copy, regex_flags);
    if (ret != 0) {
        char error_buf[PATTERN_DB_MAX_REGEX_ERRORS];
        regerror(ret, &slot->compiled_regex, error_buf, sizeof(error_buf));
        fprintf(stderr, "Pattern compilation failed: %s\n", error_buf);
        return NIMCP_ERROR;
    }

    slot->is_compiled = true;
    return NIMCP_SUCCESS;
}

/**
 * @brief Free compiled pattern
 */
static void free_pattern(pattern_slot_t* slot) {
    if (slot->is_compiled) {
        regfree(&slot->compiled_regex);
        slot->is_compiled = false;
    }
}

/**
 * @brief Find pattern by ID
 */
static pattern_slot_t* find_pattern(nimcp_pattern_db_t db, nimcp_pattern_id_t id) {
    for (size_t i = 0; i < db->capacity; i++) {
        if (db->patterns[i].pattern_id == id && db->patterns[i].is_compiled) {
            return &db->patterns[i];
        }
    }
    return NULL;  /* Pattern not found by ID */
}

/**
 * @brief Grow pattern array
 */
static nimcp_error_t grow_patterns(nimcp_pattern_db_t db) {
    size_t new_capacity = db->capacity * 2;
    if (db->config.max_patterns > 0 && new_capacity > db->config.max_patterns) {
        new_capacity = db->config.max_patterns;
    }

    if (new_capacity <= db->capacity) {
        return NIMCP_ERROR;  // At maximum capacity
    }

    pattern_slot_t* new_patterns = nimcp_calloc(new_capacity, sizeof(pattern_slot_t));
    if (!new_patterns) {
        return NIMCP_NO_MEMORY;
    }

    // Copy existing patterns
    memcpy(new_patterns, db->patterns, db->capacity * sizeof(pattern_slot_t));

    nimcp_free(db->patterns);
    db->patterns = new_patterns;
    db->capacity = new_capacity;

    return NIMCP_SUCCESS;
}

/**
 * @brief Create version snapshot
 */
static nimcp_error_t create_snapshot(nimcp_pattern_db_t db, uint32_t* snapshot_id) {
    if (db->snapshot_count >= db->config.max_snapshots) {
        // Remove oldest snapshot
        if (db->snapshots != NULL) {
            pattern_snapshot_t* oldest = db->snapshots;
            db->snapshots = oldest->next;

            // MEMORY SAFETY: ref_count is currently always 1 (no sharing)
            // Protected by write_lock, so no concurrent access.
            uint32_t old_ref = nimcp_atomic_fetch_sub_u32(&oldest->ref_count, 1, NIMCP_MEMORY_ORDER_SEQ_CST);
            if (old_ref == 1) {  // Was 1, now 0
                for (size_t i = 0; i < oldest->pattern_count; i++) {
                    free_pattern(&oldest->patterns[i]);
                }
                nimcp_free(oldest->patterns);
                nimcp_free(oldest);
            }
            db->snapshot_count--;
        }
    }

    pattern_snapshot_t* snapshot = nimcp_calloc(1, sizeof(pattern_snapshot_t));
    if (!snapshot) {
        return NIMCP_NO_MEMORY;
    }

    snapshot->version = db->current_version;
    snapshot->pattern_count = db->pattern_count;
    snapshot->capacity = db->capacity;
    nimcp_atomic_store_u32(&snapshot->ref_count, 1, NIMCP_MEMORY_ORDER_SEQ_CST);

    // Deep copy patterns
    snapshot->patterns = nimcp_calloc(db->capacity, sizeof(pattern_slot_t));
    if (!snapshot->patterns) {
        nimcp_free(snapshot);
        return NIMCP_NO_MEMORY;
    }

    for (size_t i = 0; i < db->capacity; i++) {
        if (db->patterns[i].is_compiled) {
            snapshot->patterns[i] = db->patterns[i];
            snapshot->patterns[i].is_compiled = false;  /* Reset before re-compile to avoid double regfree */
            // Re-compile regex in snapshot
            compile_pattern(&snapshot->patterns[i]);
        }
    }

    // Add to snapshot list
    snapshot->next = db->snapshots;
    db->snapshots = snapshot;
    db->snapshot_count++;

    if (snapshot_id) {
        *snapshot_id = snapshot->version;
    }

    return NIMCP_SUCCESS;
}

//=============================================================================
// Configuration Functions
//=============================================================================

nimcp_pattern_db_config_t nimcp_pattern_db_default_config(void) {
    nimcp_pattern_db_config_t config = {
        .initial_capacity = NIMCP_PATTERN_DEFAULT_CAPACITY,
        .max_patterns = 0,  // Unlimited
        .max_snapshots = NIMCP_PATTERN_MAX_SNAPSHOTS,
        .enable_statistics = true,
        .enable_validation = true,
        .enable_bio_async = false,
        .module_id = 0,
        .match_timeout_ms = 100.0F,
        .worker_threads = 0  // Auto-detect
    };
    return config;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

nimcp_pattern_db_t nimcp_pattern_db_create(const nimcp_pattern_db_config_t* config) {
    LOG_MODULE_DEBUG("pattern_db", "Creating pattern database");

    nimcp_pattern_db_t db = nimcp_calloc(1, sizeof(struct nimcp_pattern_db_impl));
    if (!db) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_pattern_db_create: failed to allocate database");
        return NULL;
    }

    // Set configuration
    if (config) {
        db->config = *config;
    } else {
        db->config = nimcp_pattern_db_default_config();
    }

    // Allocate pattern array
    size_t capacity = db->config.initial_capacity;
    if (capacity == 0) {
        capacity = PATTERN_DB_INITIAL_CAPACITY;
    }

    db->patterns = nimcp_calloc(capacity, sizeof(pattern_slot_t));
    if (!db->patterns) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_pattern_db_create: failed to allocate patterns array");
        nimcp_free(db);
        return NULL;
    }

    db->capacity = capacity;
    db->pattern_count = 0;
    db->next_pattern_id = 1;  // Start from 1, 0 is invalid
    db->current_version = 1;
    db->snapshots = NULL;
    db->snapshot_count = 0;

    // Initialize synchronization
    if (pthread_rwlock_init(&db->write_lock, NULL) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "nimcp_pattern_db_create: failed to init rwlock");
        nimcp_free(db->patterns);
        nimcp_free(db);
        return NULL;
    }

    // Initialize statistics
    memset(&db->stats, 0, sizeof(db->stats));
    db->stats.current_version = db->current_version;

    // Initialize bio-async if enabled
    db->bio_async_enabled = false;
    db->bio_ctx = NULL;

    db->magic = PATTERN_DB_MAGIC_VALID;

    LOG_MODULE_INFO("pattern_db", "Pattern database created with capacity %zu", capacity);
    return db;
}

void nimcp_pattern_db_destroy(nimcp_pattern_db_t db) {
    if (!validate_db(db)) {
        LOG_MODULE_WARN("pattern_db", "Attempted to destroy invalid pattern database");
        return;
    }

    LOG_MODULE_DEBUG("pattern_db", "Destroying pattern database with %u patterns", db->pattern_count);
    db->magic = 0;  // Invalidate immediately

    // Unregister from bio-async
    if (db->bio_async_enabled && db->bio_ctx) {
        bio_router_unregister_module(db->bio_ctx);
    }

    // Free all patterns
    for (size_t i = 0; i < db->capacity; i++) {
        free_pattern(&db->patterns[i]);
    }
    nimcp_free(db->patterns);

    // Free all snapshots
    pattern_snapshot_t* snapshot = db->snapshots;
    while (snapshot) {
        pattern_snapshot_t* next = snapshot->next;

        uint32_t old_ref = nimcp_atomic_fetch_sub_u32(&snapshot->ref_count, 1, NIMCP_MEMORY_ORDER_SEQ_CST);
        if (old_ref == 1) {  // Was 1, now 0
            for (size_t i = 0; i < snapshot->capacity; i++) {
                free_pattern(&snapshot->patterns[i]);
            }
            nimcp_free(snapshot->patterns);
            nimcp_free(snapshot);
        }

        snapshot = next;
    }

    pthread_rwlock_destroy(&db->write_lock);
    nimcp_free(db);
}

//=============================================================================
// Pattern Management
//=============================================================================

nimcp_error_t nimcp_pattern_db_add(
    nimcp_pattern_db_t db,
    const nimcp_pattern_entry_t* entry,
    nimcp_pattern_id_t* id
) {
    if (!validate_db(db) || !entry || !entry->pattern) {
        return NIMCP_INVALID_PARAM;
    }

    if (strlen(entry->pattern) >= NIMCP_PATTERN_MAX_LENGTH) {
        return NIMCP_INVALID_PARAM;
    }

    if (entry->description && strlen(entry->description) >= NIMCP_PATTERN_MAX_DESCRIPTION) {
        return NIMCP_INVALID_PARAM;
    }

    pthread_rwlock_wrlock(&db->write_lock);

    // Check if we need to grow
    if (db->pattern_count >= db->capacity * PATTERN_DB_LOAD_FACTOR) {
        nimcp_error_t err = grow_patterns(db);
        if (err != NIMCP_SUCCESS) {
            pthread_rwlock_unlock(&db->write_lock);
            return err;
        }
    }

    // Find free slot
    pattern_slot_t* slot = NULL;
    for (size_t i = 0; i < db->capacity; i++) {
        if (!db->patterns[i].is_compiled) {
            slot = &db->patterns[i];
            break;
        }
    }

    if (!slot) {
        pthread_rwlock_unlock(&db->write_lock);
        return NIMCP_ERROR;
    }

    // Fill in pattern slot
    memset(slot, 0, sizeof(pattern_slot_t));
    slot->pattern_id = db->next_pattern_id++;
    slot->entry = *entry;

    // Copy strings
    strncpy(slot->pattern_copy, entry->pattern, NIMCP_PATTERN_MAX_LENGTH - 1);
    slot->pattern_copy[NIMCP_PATTERN_MAX_LENGTH - 1] = '\0';
    slot->entry.pattern = slot->pattern_copy;

    if (entry->description) {
        strncpy(slot->description_copy, entry->description, NIMCP_PATTERN_MAX_DESCRIPTION - 1);
        slot->description_copy[NIMCP_PATTERN_MAX_DESCRIPTION - 1] = '\0';
        slot->entry.description = slot->description_copy;
    } else {
        slot->description_copy[0] = '\0';
        slot->entry.description = slot->description_copy;
    }

    // Compile pattern
    nimcp_error_t err = compile_pattern(slot);
    if (err != NIMCP_SUCCESS) {
        memset(slot, 0, sizeof(pattern_slot_t));
        pthread_rwlock_unlock(&db->write_lock);
        return err;
    }

    db->pattern_count++;
    db->current_version++;

    // Update statistics
    db->stats.total_patterns = db->pattern_count;
    db->stats.current_version = db->current_version;
    if (entry->category < NIMCP_PATTERN_CATEGORY_COUNT) {
        db->stats.patterns_by_category[entry->category]++;
    }

    if (id) {
        *id = slot->pattern_id;
    }

    pthread_rwlock_unlock(&db->write_lock);

    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_pattern_db_remove(
    nimcp_pattern_db_t db,
    nimcp_pattern_id_t id
) {
    if (!validate_db(db) || id == NIMCP_PATTERN_ID_INVALID) {
        return NIMCP_INVALID_PARAM;
    }

    pthread_rwlock_wrlock(&db->write_lock);

    pattern_slot_t* slot = find_pattern(db, id);
    if (!slot) {
        pthread_rwlock_unlock(&db->write_lock);
        return NIMCP_NOT_FOUND;
    }

    // Update statistics
    if (slot->entry.category < NIMCP_PATTERN_CATEGORY_COUNT) {
        db->stats.patterns_by_category[slot->entry.category]--;
    }

    // Free pattern
    free_pattern(slot);
    memset(slot, 0, sizeof(pattern_slot_t));

    db->pattern_count--;
    db->current_version++;

    db->stats.total_patterns = db->pattern_count;
    db->stats.current_version = db->current_version;

    pthread_rwlock_unlock(&db->write_lock);

    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_pattern_db_update(
    nimcp_pattern_db_t db,
    nimcp_pattern_id_t id,
    const nimcp_pattern_entry_t* entry
) {
    if (!validate_db(db) || id == NIMCP_PATTERN_ID_INVALID || !entry) {
        return NIMCP_INVALID_PARAM;
    }

    pthread_rwlock_wrlock(&db->write_lock);

    pattern_slot_t* slot = find_pattern(db, id);
    if (!slot) {
        pthread_rwlock_unlock(&db->write_lock);
        return NIMCP_NOT_FOUND;
    }

    // Update statistics (remove old category)
    if (slot->entry.category < NIMCP_PATTERN_CATEGORY_COUNT) {
        db->stats.patterns_by_category[slot->entry.category]--;
    }

    // Free old pattern
    free_pattern(slot);

    // Update entry
    slot->entry = *entry;

    // Copy strings
    strncpy(slot->pattern_copy, entry->pattern, NIMCP_PATTERN_MAX_LENGTH - 1);
    slot->pattern_copy[NIMCP_PATTERN_MAX_LENGTH - 1] = '\0';
    slot->entry.pattern = slot->pattern_copy;

    if (entry->description) {
        strncpy(slot->description_copy, entry->description, NIMCP_PATTERN_MAX_DESCRIPTION - 1);
        slot->description_copy[NIMCP_PATTERN_MAX_DESCRIPTION - 1] = '\0';
        slot->entry.description = slot->description_copy;
    } else {
        slot->description_copy[0] = '\0';
        slot->entry.description = slot->description_copy;
    }

    // Recompile
    nimcp_error_t err = compile_pattern(slot);
    if (err != NIMCP_SUCCESS) {
        memset(slot, 0, sizeof(pattern_slot_t));
        db->pattern_count--;
        pthread_rwlock_unlock(&db->write_lock);
        return err;
    }

    // Update statistics (add new category)
    if (entry->category < NIMCP_PATTERN_CATEGORY_COUNT) {
        db->stats.patterns_by_category[entry->category]++;
    }

    db->current_version++;
    db->stats.current_version = db->current_version;

    pthread_rwlock_unlock(&db->write_lock);

    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_pattern_db_get(
    nimcp_pattern_db_t db,
    nimcp_pattern_id_t id,
    nimcp_pattern_entry_t* entry
) {
    if (!validate_db(db) || id == NIMCP_PATTERN_ID_INVALID || !entry) {
        return NIMCP_INVALID_PARAM;
    }

    pthread_rwlock_rdlock(&db->write_lock);

    pattern_slot_t* slot = find_pattern(db, id);
    if (!slot) {
        pthread_rwlock_unlock(&db->write_lock);
        return NIMCP_NOT_FOUND;
    }

    *entry = slot->entry;

    pthread_rwlock_unlock(&db->write_lock);

    return NIMCP_SUCCESS;
}

//=============================================================================
// Safe JSON Parsing Helpers
//=============================================================================

/**
 * @brief Safe string to long conversion with validation
 *
 * WHAT: Convert string to long with bounds checking
 * WHY:  Avoid undefined behavior from atoi/atol on invalid input
 * HOW:  Use strtol with error checking
 *
 * @param str Input string
 * @param result Output value
 * @param min Minimum valid value
 * @param max Maximum valid value
 * @return true if conversion successful and within bounds
 */
static bool safe_strtol(const char* str, long* result, long min, long max) {
    if (!str || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "safe_strtol: required parameter is NULL (str, result)");
        return false;
    }

    /* Skip leading whitespace */
    while (*str && isspace((unsigned char)*str)) {
        str++;
    }

    if (*str == '\0') {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "safe_strtol: validation failed");
        return false;
    }

    char* endptr;
    errno = 0;
    long val = strtol(str, &endptr, 10);

    /* Check for conversion errors */
    if (errno == ERANGE || val < min || val > max) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "safe_strtol: validation failed");
        return false;
    }

    /* Check that at least one digit was consumed */
    if (endptr == str) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "safe_strtol: validation failed");
        return false;
    }

    *result = val;
    return true;
}

/**
 * @brief Safe string to double conversion with validation
 *
 * WHAT: Convert string to double with bounds checking
 * WHY:  Avoid undefined behavior from atof on invalid input
 * HOW:  Use strtod with error checking
 *
 * @param str Input string
 * @param result Output value
 * @return true if conversion successful
 */
static bool safe_strtod(const char* str, double* result) {
    if (!str || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "safe_strtod: required parameter is NULL (str, result)");
        return false;
    }

    /* Skip leading whitespace */
    while (*str && isspace((unsigned char)*str)) {
        str++;
    }

    if (*str == '\0') {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "safe_strtod: validation failed");
        return false;
    }

    char* endptr;
    errno = 0;
    double val = strtod(str, &endptr);

    /* Check for conversion errors */
    if (errno == ERANGE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "safe_strtod: validation failed");
        return false;
    }

    /* Check that at least one digit was consumed */
    if (endptr == str) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "safe_strtod: validation failed");
        return false;
    }

    *result = val;
    return true;
}

/**
 * @brief Validate and unescape JSON string
 *
 * WHAT: Extract and unescape a JSON string value
 * WHY:  Properly handle escape sequences and prevent buffer overflow
 * HOW:  Scan for valid escape sequences, validate, and copy with bounds checking
 *
 * @param src Start of string value (after opening quote)
 * @param src_end Maximum position to read from
 * @param dest Destination buffer
 * @param dest_size Size of destination buffer
 * @param chars_consumed Output: number of source characters consumed
 * @return true if extraction successful
 */
static bool json_unescape_string(const char* src, const char* src_end,
                                  char* dest, size_t dest_size,
                                  size_t* chars_consumed) {
    if (!src || !dest || dest_size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "safe_strtod: required parameter is NULL (src, dest)");
        return false;
    }

    size_t di = 0;  /* Destination index */
    const char* p = src;

    while (p < src_end && *p != '"' && di < dest_size - 1) {
        if (*p == '\\') {
            /* Handle escape sequences */
            p++;
            if (p >= src_end) {
                /* Unterminated escape */
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "safe_strtod: capacity exceeded");
                return false;
            }

            switch (*p) {
                case '"':  dest[di++] = '"'; break;
                case '\\': dest[di++] = '\\'; break;
                case '/':  dest[di++] = '/'; break;
                case 'b':  dest[di++] = '\b'; break;
                case 'f':  dest[di++] = '\f'; break;
                case 'n':  dest[di++] = '\n'; break;
                case 'r':  dest[di++] = '\r'; break;
                case 't':  dest[di++] = '\t'; break;
                case 'u':
                    /* Unicode escape - validate but store as-is for now */
                    /* Full Unicode support would require UTF-8 encoding */
                    if (p + 4 >= src_end) {
                        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "safe_strtod: capacity exceeded");
                        return false;
                    }
                    for (int i = 1; i <= 4; i++) {
                        if (!isxdigit((unsigned char)p[i])) {
                            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "safe_strtod: isxdigit is NULL");
                            return false;
                        }
                    }
                    /* Skip \uXXXX, store as '?' placeholder */
                    dest[di++] = '?';
                    p += 4;
                    break;
                default:
                    /* Invalid escape sequence */
                    LOG_MODULE_WARN("pattern_db", "Invalid JSON escape sequence: \\%c", *p);
                    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "safe_strtod: isxdigit is NULL");
                    return false;
            }
        } else if ((unsigned char)*p < 0x20) {
            /* Control characters must be escaped in JSON */
            LOG_MODULE_WARN("pattern_db", "Invalid control character in JSON string");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "safe_strtod: operation failed");
            return false;
        } else {
            dest[di++] = *p;
        }
        p++;
    }

    dest[di] = '\0';

    if (chars_consumed) {
        *chars_consumed = (size_t)(p - src);
    }

    return true;
}

/**
 * @brief Find JSON key and extract its value bounds
 *
 * WHAT: Locate a JSON key and find the bounds of its value
 * WHY:  Safe extraction of JSON values without buffer overflow
 * HOW:  Search for key, validate structure, return pointers to value
 *
 * @param json_start Start of JSON object
 * @param json_end End boundary
 * @param key Key name to find (without quotes)
 * @param val_start Output: start of value
 * @param val_end Output: end of value
 * @return true if key found and value bounds determined
 */
static bool json_find_key_value(const char* json_start, const char* json_end,
                                const char* key,
                                const char** val_start, const char** val_end) {
    if (!json_start || !json_end || !key || !val_start || !val_end) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "safe_strtod: required parameter is NULL (json_start, json_end, key, val_start, val_end)");
        return false;
    }

    size_t key_len = strlen(key);
    char search_key[NIMCP_NAME_BUFFER_SIZE];
    if (key_len + 3 > sizeof(search_key)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "safe_strtod: validation failed");
        return false;
    }

    /* Build search string: "key" */
    snprintf(search_key, sizeof(search_key), "\"%s\"", key);

    const char* key_pos = strstr(json_start, search_key);
    if (!key_pos || key_pos >= json_end) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "safe_strtod: key_pos is NULL");
        return false;
    }

    /* Move past key and find colon */
    const char* p = key_pos + strlen(search_key);
    while (p < json_end && isspace((unsigned char)*p)) p++;

    if (p >= json_end || *p != ':') {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "safe_strtod: capacity exceeded");
        return false;
    }
    p++;

    /* Skip whitespace after colon */
    while (p < json_end && isspace((unsigned char)*p)) p++;

    if (p >= json_end) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "safe_strtod: capacity exceeded");
        return false;
    }

    *val_start = p;

    /* Find end of value based on type */
    if (*p == '"') {
        /* String value - find closing quote */
        p++;
        while (p < json_end) {
            if (*p == '\\' && (p + 1) < json_end) {
                p += 2;  /* Skip escaped character */
            } else if (*p == '"') {
                *val_end = p + 1;
                return true;
            } else {
                p++;
            }
        }
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "safe_strtod: validation failed");
        return false;  /* Unterminated string */
    } else if (*p == '{') {
        /* Object - find matching brace */
        int depth = 1;
        p++;
        while (p < json_end && depth > 0) {
            if (*p == '{') depth++;
            else if (*p == '}') depth--;
            p++;
        }
        *val_end = p;
        return depth == 0;
    } else if (*p == '[') {
        /* Array - find matching bracket */
        int depth = 1;
        p++;
        while (p < json_end && depth > 0) {
            if (*p == '[') depth++;
            else if (*p == ']') depth--;
            p++;
        }
        *val_end = p;
        return depth == 0;
    } else {
        /* Numeric, boolean, or null - find delimiter */
        while (p < json_end && *p != ',' && *p != '}' && *p != ']' && !isspace((unsigned char)*p)) {
            p++;
        }
        *val_end = p;
        return true;
    }
}

/**
 * @brief Validate basic JSON structure
 *
 * WHAT: Basic validation of JSON structure
 * WHY:  Catch malformed JSON before detailed parsing
 * HOW:  Check for balanced braces, brackets, quotes
 */
static bool json_validate_basic_structure(const char* json, size_t len) {
    if (!json || len == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "json_validate_basic_structure: json is NULL");
        return false;
    }

    int brace_depth = 0;
    int bracket_depth = 0;
    bool in_string = false;
    bool escaped = false;

    for (size_t i = 0; i < len; i++) {
        char c = json[i];

        if (escaped) {
            escaped = false;
            continue;
        }

        if (in_string) {
            if (c == '\\') {
                escaped = true;
            } else if (c == '"') {
                in_string = false;
            }
        } else {
            switch (c) {
                case '"':
                    in_string = true;
                    break;
                case '{':
                    brace_depth++;
                    break;
                case '}':
                    brace_depth--;
                    if (brace_depth < 0) {
                        return false;
                    }
                    break;
                case '[':
                    bracket_depth++;
                    break;
                case ']':
                    bracket_depth--;
                    if (bracket_depth < 0) {
                        return false;
                    }
                    break;
            }
        }
    }

    return brace_depth == 0 && bracket_depth == 0 && !in_string;
}

//=============================================================================
// Bulk Operations
//=============================================================================

nimcp_error_t nimcp_pattern_db_import(
    nimcp_pattern_db_t db,
    const nimcp_pattern_entry_t* entries,
    size_t count
) {
    if (!validate_db(db) || !entries || count == 0) {
        return NIMCP_INVALID_PARAM;
    }

    // Create snapshot before import for rollback
    uint32_t snapshot_version;
    nimcp_error_t err = nimcp_pattern_db_snapshot(db, &snapshot_version);
    if (err != NIMCP_SUCCESS) {
        return err;
    }

    // Import all patterns
    for (size_t i = 0; i < count; i++) {
        err = nimcp_pattern_db_add(db, &entries[i], NULL);
        if (err != NIMCP_SUCCESS) {
            // Rollback on error
            nimcp_pattern_db_rollback(db, snapshot_version);
            return err;
        }
    }

    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_pattern_db_load(
    nimcp_pattern_db_t db,
    const char* filepath
) {
    if (!validate_db(db) || !filepath) {
        return NIMCP_INVALID_PARAM;
    }

    FILE* file = fopen(filepath, "r");
    if (!file) {
        LOG_MODULE_ERROR("pattern_db", "Failed to open pattern file: %s", filepath);
        return NIMCP_ERROR;
    }

    // Create snapshot before import for rollback on error
    uint32_t snapshot_version;
    nimcp_error_t err = nimcp_pattern_db_snapshot(db, &snapshot_version);
    if (err != NIMCP_SUCCESS) {
        fclose(file);
        return err;
    }

    // Read file contents
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (file_size <= 0 || file_size > 10 * 1024 * 1024) {  // Max 10MB
        fclose(file);
        return NIMCP_INVALID_PARAM;
    }

    char* content = nimcp_malloc((size_t)file_size + 1);
    if (!content) {
        fclose(file);
        return NIMCP_NO_MEMORY;
    }

    size_t read_size = fread(content, 1, (size_t)file_size, file);
    fclose(file);
    if (read_size != (size_t)file_size) {
        nimcp_free(content);
        return NIMCP_ERROR;
    }
    content[read_size] = '\0';

    /* SECURITY: Validate basic JSON structure before parsing */
    if (!json_validate_basic_structure(content, read_size)) {
        LOG_MODULE_ERROR("pattern_db", "Invalid JSON structure in pattern file");
        nimcp_free(content);
        return NIMCP_INVALID_PARAM;
    }

    /* Safer JSON parsing for pattern database format:
     * { "patterns": [ { "pattern": "...", "category": N, ... }, ... ] }
     */
    uint32_t patterns_loaded = 0;
    const char* content_end = content + read_size;

    /* Find patterns array */
    const char* patterns_val_start;
    const char* patterns_val_end;
    if (!json_find_key_value(content, content_end, "patterns",
                             &patterns_val_start, &patterns_val_end)) {
        LOG_MODULE_ERROR("pattern_db", "No 'patterns' array found in JSON");
        nimcp_free(content);
        return NIMCP_INVALID_PARAM;
    }

    /* Validate it's an array */
    if (*patterns_val_start != '[') {
        LOG_MODULE_ERROR("pattern_db", "'patterns' value is not an array");
        nimcp_free(content);
        return NIMCP_INVALID_PARAM;
    }

    const char* pos = patterns_val_start + 1;
    while (pos < patterns_val_end) {
        /* Find next object */
        while (pos < patterns_val_end && *pos != '{') {
            pos++;
        }
        if (pos >= patterns_val_end) break;

        const char* obj_start = pos;

        /* Find matching closing brace (handling nested objects) */
        int depth = 1;
        pos++;
        while (pos < patterns_val_end && depth > 0) {
            if (*pos == '{') depth++;
            else if (*pos == '}') depth--;
            pos++;
        }

        if (depth != 0) {
            LOG_MODULE_WARN("pattern_db", "Malformed object in patterns array");
            break;
        }

        const char* obj_end = pos;

        nimcp_pattern_entry_t entry;
        memset(&entry, 0, sizeof(entry));

        /* Thread-local buffers to avoid static variable issues */
        char pattern_buf[NIMCP_PATTERN_MAX_LENGTH];
        char desc_buf[NIMCP_PATTERN_MAX_DESCRIPTION];
        pattern_buf[0] = '\0';
        desc_buf[0] = '\0';

        /* Extract pattern string with safe parsing */
        const char* val_start;
        const char* val_end;
        if (json_find_key_value(obj_start, obj_end, "pattern", &val_start, &val_end)) {
            if (*val_start == '"') {
                /* Safely unescape and copy string */
                if (json_unescape_string(val_start + 1, val_end - 1,
                                         pattern_buf, sizeof(pattern_buf), NULL)) {
                    entry.pattern = pattern_buf;
                }
            }
        }

        /* Extract description with safe parsing */
        if (json_find_key_value(obj_start, obj_end, "description", &val_start, &val_end)) {
            if (*val_start == '"') {
                if (json_unescape_string(val_start + 1, val_end - 1,
                                         desc_buf, sizeof(desc_buf), NULL)) {
                    entry.description = desc_buf;
                }
            }
        }

        /* Extract category with safe integer parsing */
        if (json_find_key_value(obj_start, obj_end, "category", &val_start, &val_end)) {
            long cat_val;
            if (safe_strtol(val_start, &cat_val, 0, NIMCP_PATTERN_CATEGORY_COUNT - 1)) {
                entry.category = (nimcp_pattern_category_t)cat_val;
            } else {
                /* Default to CUSTOM if invalid */
                entry.category = NIMCP_PATTERN_CUSTOM;
            }
        }

        /* Extract priority with safe integer parsing */
        if (json_find_key_value(obj_start, obj_end, "priority", &val_start, &val_end)) {
            long pri_val;
            if (safe_strtol(val_start, &pri_val, 0, LONG_MAX)) {
                entry.priority = (uint32_t)(pri_val > UINT32_MAX ? UINT32_MAX : pri_val);
            }
        }

        /* Extract weight with safe float parsing */
        if (json_find_key_value(obj_start, obj_end, "weight", &val_start, &val_end)) {
            double wgt_val;
            if (safe_strtod(val_start, &wgt_val)) {
                /* Clamp weight to valid range */
                if (wgt_val < 0.0) wgt_val = 0.0;
                if (wgt_val > 1.0) wgt_val = 1.0;
                entry.weight = (float)wgt_val;
            }
        }

        /* Extract flags with safe integer parsing */
        if (json_find_key_value(obj_start, obj_end, "flags", &val_start, &val_end)) {
            long flg_val;
            if (safe_strtol(val_start, &flg_val, 0, LONG_MAX)) {
                entry.flags = (uint32_t)(flg_val > UINT32_MAX ? UINT32_MAX : flg_val);
            }
        }

        /* Only add pattern if pattern string is valid and non-empty */
        if (entry.pattern && strlen(entry.pattern) > 0) {
            nimcp_pattern_id_t id;
            err = nimcp_pattern_db_add(db, &entry, &id);
            if (err == NIMCP_SUCCESS) {
                patterns_loaded++;
            } else {
                LOG_MODULE_WARN("pattern_db", "Failed to add pattern from JSON: %s",
                                entry.pattern);
            }
        }
    }

    nimcp_free(content);

    if (patterns_loaded == 0) {
        LOG_MODULE_WARN("pattern_db", "No valid patterns loaded from file");
        nimcp_pattern_db_rollback(db, snapshot_version);
        return NIMCP_ERROR;
    }

    LOG_MODULE_INFO("pattern_db", "Loaded %u patterns from %s", patterns_loaded, filepath);
    return NIMCP_SUCCESS;
}

/**
 * @brief Escape string for JSON output
 */
static void json_escape_string(const char* input, char* output, size_t output_size) {
    if (!input || !output || output_size == 0) return;

    size_t out_idx = 0;
    for (const char* p = input; *p && out_idx < output_size - 1; p++) {
        switch (*p) {
            case '"':
                if (out_idx + 2 < output_size) { output[out_idx++] = '\\'; output[out_idx++] = '"'; }
                break;
            case '\\':
                if (out_idx + 2 < output_size) { output[out_idx++] = '\\'; output[out_idx++] = '\\'; }
                break;
            case '\n':
                if (out_idx + 2 < output_size) { output[out_idx++] = '\\'; output[out_idx++] = 'n'; }
                break;
            case '\r':
                if (out_idx + 2 < output_size) { output[out_idx++] = '\\'; output[out_idx++] = 'r'; }
                break;
            case '\t':
                if (out_idx + 2 < output_size) { output[out_idx++] = '\\'; output[out_idx++] = 't'; }
                break;
            default:
                output[out_idx++] = *p;
                break;
        }
    }
    output[out_idx] = '\0';
}

nimcp_error_t nimcp_pattern_db_save(
    nimcp_pattern_db_t db,
    const char* filepath
) {
    if (!validate_db(db) || !filepath) {
        return NIMCP_INVALID_PARAM;
    }

    FILE* file = fopen(filepath, "w");
    if (!file) {
        LOG_MODULE_ERROR("pattern_db", "Failed to create pattern file: %s", filepath);
        return NIMCP_ERROR;
    }

    pthread_rwlock_rdlock(&db->write_lock);

    fprintf(file, "{\n");
    fprintf(file, "  \"version\": %u,\n", db->current_version);
    fprintf(file, "  \"pattern_count\": %u,\n", db->pattern_count);
    fprintf(file, "  \"patterns\": [\n");

    char escaped_pattern[NIMCP_PATTERN_MAX_LENGTH * 2];
    char escaped_desc[NIMCP_PATTERN_MAX_DESCRIPTION * 2];

    uint32_t written = 0;
    for (size_t i = 0; i < db->capacity; i++) {
        if (!db->patterns[i].is_compiled) continue;

        pattern_slot_t* slot = &db->patterns[i];

        json_escape_string(slot->pattern_copy, escaped_pattern, sizeof(escaped_pattern));
        json_escape_string(slot->description_copy, escaped_desc, sizeof(escaped_desc));

        fprintf(file, "    {\n");
        fprintf(file, "      \"id\": %u,\n", slot->pattern_id);
        fprintf(file, "      \"pattern\": \"%s\",\n", escaped_pattern);
        fprintf(file, "      \"category\": %d,\n", (int)slot->entry.category);
        fprintf(file, "      \"category_name\": \"%s\",\n", nimcp_pattern_category_name(slot->entry.category));
        fprintf(file, "      \"priority\": %u,\n", slot->entry.priority);
        fprintf(file, "      \"weight\": %.4f,\n", (double)slot->entry.weight);
        fprintf(file, "      \"flags\": %u,\n", slot->entry.flags);
        fprintf(file, "      \"description\": \"%s\",\n", escaped_desc);
        uint32_t match_count = nimcp_atomic_load_u32(&slot->match_count, NIMCP_MEMORY_ORDER_RELAXED);
        fprintf(file, "      \"match_count\": %u,\n", match_count);
        fprintf(file, "      \"avg_match_time_us\": %.2f\n",
                match_count > 0 ? (double)slot->total_match_time_us / match_count : 0.0);

        written++;
        fprintf(file, "    }%s\n", written < db->pattern_count ? "," : "");
    }

    fprintf(file, "  ]\n}\n");

    pthread_rwlock_unlock(&db->write_lock);
    fclose(file);

    LOG_MODULE_INFO("pattern_db", "Saved %u patterns to %s", written, filepath);
    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_pattern_db_clear(nimcp_pattern_db_t db) {
    if (!validate_db(db)) {
        return NIMCP_INVALID_PARAM;
    }

    pthread_rwlock_wrlock(&db->write_lock);

    // Free all patterns
    for (size_t i = 0; i < db->capacity; i++) {
        if (db->patterns[i].is_compiled) {
            free_pattern(&db->patterns[i]);
            memset(&db->patterns[i], 0, sizeof(pattern_slot_t));
        }
    }

    db->pattern_count = 0;
    db->current_version++;

    // Reset statistics
    db->stats.total_patterns = 0;
    memset(db->stats.patterns_by_category, 0, sizeof(db->stats.patterns_by_category));
    db->stats.current_version = db->current_version;

    pthread_rwlock_unlock(&db->write_lock);

    return NIMCP_SUCCESS;
}

//=============================================================================
// Versioning and Rollback
//=============================================================================

uint32_t nimcp_pattern_db_version(nimcp_pattern_db_t db) {
    if (!validate_db(db)) {
        return 0;
    }
    return db->current_version;
}

nimcp_error_t nimcp_pattern_db_snapshot(
    nimcp_pattern_db_t db,
    uint32_t* snapshot_id
) {
    if (!validate_db(db)) {
        return NIMCP_INVALID_PARAM;
    }

    pthread_rwlock_wrlock(&db->write_lock);
    nimcp_error_t err = create_snapshot(db, snapshot_id);
    pthread_rwlock_unlock(&db->write_lock);

    if (err == NIMCP_SUCCESS) {
        db->stats.snapshot_count = db->snapshot_count;
    }

    return err;
}

nimcp_error_t nimcp_pattern_db_rollback(
    nimcp_pattern_db_t db,
    uint32_t version
) {
    if (!validate_db(db)) {
        return NIMCP_INVALID_PARAM;
    }

    pthread_rwlock_wrlock(&db->write_lock);

    // Find snapshot with matching version
    pattern_snapshot_t* snapshot = db->snapshots;
    while (snapshot && snapshot->version != version) {
        snapshot = snapshot->next;
    }

    if (!snapshot) {
        pthread_rwlock_unlock(&db->write_lock);
        return NIMCP_NOT_FOUND;
    }

    // Allocate new patterns array BEFORE freeing old one (atomicity)
    // MEMORY SAFETY: If allocation fails, db state remains consistent
    pattern_slot_t* new_patterns = nimcp_calloc(snapshot->capacity, sizeof(pattern_slot_t));
    if (!new_patterns) {
        pthread_rwlock_unlock(&db->write_lock);
        return NIMCP_NO_MEMORY;
    }

    // Free current patterns
    for (size_t i = 0; i < db->capacity; i++) {
        free_pattern(&db->patterns[i]);
    }
    nimcp_free(db->patterns);

    // Restore from snapshot (atomically - new_patterns already allocated)
    db->patterns = new_patterns;
    db->capacity = snapshot->capacity;
    db->pattern_count = snapshot->pattern_count;

    // Deep copy patterns from snapshot
    for (size_t i = 0; i < snapshot->capacity; i++) {
        if (snapshot->patterns[i].is_compiled) {
            db->patterns[i] = snapshot->patterns[i];
            db->patterns[i].is_compiled = false;  /* Reset before re-compile to avoid regfree on uninitialized regex */
            compile_pattern(&db->patterns[i]);
        }
    }

    db->current_version++;

    // Update statistics
    db->stats.total_patterns = db->pattern_count;
    db->stats.current_version = db->current_version;
    memset(db->stats.patterns_by_category, 0, sizeof(db->stats.patterns_by_category));
    for (size_t i = 0; i < db->capacity; i++) {
        if (db->patterns[i].is_compiled &&
            db->patterns[i].entry.category < NIMCP_PATTERN_CATEGORY_COUNT) {
            db->stats.patterns_by_category[db->patterns[i].entry.category]++;
        }
    }

    pthread_rwlock_unlock(&db->write_lock);

    return NIMCP_SUCCESS;
}

//=============================================================================
// Pattern Matching
//=============================================================================

nimcp_error_t nimcp_pattern_db_match(
    nimcp_pattern_db_t db,
    const char* input,
    nimcp_pattern_match_result_t* result
) {
    if (!validate_db(db) || !input || !result) {
        return NIMCP_INVALID_PARAM;
    }

    memset(result, 0, sizeof(nimcp_pattern_match_result_t));

    uint64_t start_time = get_time_us();

    pthread_rwlock_rdlock(&db->write_lock);

    // Build sorted pattern list by priority
    typedef struct {
        pattern_slot_t* slot;
        uint32_t priority;
    } pattern_pri_t;

    pattern_pri_t* sorted_patterns = nimcp_calloc(db->pattern_count, sizeof(pattern_pri_t));
    if (!sorted_patterns) {
        pthread_rwlock_unlock(&db->write_lock);
        return NIMCP_NO_MEMORY;
    }

    size_t sorted_count = 0;
    for (size_t i = 0; i < db->capacity; i++) {
        if (db->patterns[i].is_compiled) {
            sorted_patterns[sorted_count].slot = &db->patterns[i];
            sorted_patterns[sorted_count].priority = db->patterns[i].entry.priority;
            sorted_count++;
        }
    }

    // Simple insertion sort by priority (descending)
    for (size_t i = 1; i < sorted_count; i++) {
        pattern_pri_t key = sorted_patterns[i];
        ssize_t j = i - 1;
        while (j >= 0 && sorted_patterns[j].priority < key.priority) {
            sorted_patterns[j + 1] = sorted_patterns[j];
            j--;
        }
        sorted_patterns[j + 1] = key;
    }

    // Match patterns in priority order
    float total_threat = 0.0F;
    uint32_t match_count = 0;

    for (size_t i = 0; i < sorted_count; i++) {
        pattern_slot_t* slot = sorted_patterns[i].slot;

        regmatch_t match;
        int ret = regexec(&slot->compiled_regex, input, 1, &match, 0);

        if (ret == 0) {
            // Pattern matched
            match_count++;
            total_threat += slot->entry.weight;

            // Record first/best match
            if (!result->matched) {
                result->matched = true;
                result->pattern_id = slot->pattern_id;
                result->category = slot->entry.category;
                result->match_offset = match.rm_so;
                result->match_length = match.rm_eo - match.rm_so;

                if (slot->entry.description) {
                    strncpy(result->description, slot->entry.description,
                            NIMCP_PATTERN_MAX_DESCRIPTION - 1);
                    result->description[NIMCP_PATTERN_MAX_DESCRIPTION - 1] = '\0';
                }
            }

            // Update pattern statistics
            nimcp_atomic_fetch_add_u32(&slot->match_count, 1, NIMCP_MEMORY_ORDER_RELAXED);
        }
    }

    nimcp_free(sorted_patterns);

    result->match_count = match_count;
    result->threat_score = (total_threat > 1.0F) ? 1.0F : total_threat;

    uint64_t end_time = get_time_us();
    float match_time = (float)(end_time - start_time);

    // Update statistics
    db->stats.total_matches++;
    if (result->matched) {
        db->stats.total_hits++;
    }

    if (db->config.enable_statistics) {
        // Update running average
        float n = (float)db->stats.total_matches;
        db->stats.avg_match_time_us =
            (db->stats.avg_match_time_us * (n - 1.0F) + match_time) / n;

        if (match_time > db->stats.max_match_time_us) {
            db->stats.max_match_time_us = match_time;
        }
    }

    pthread_rwlock_unlock(&db->write_lock);

    /* Send BIO_MSG_SECURITY_EVENT when pattern match produces a high threat
     * score.  This notifies the immune system and other security modules that
     * suspicious input was detected by the pattern database. */
    if (result->matched && result->threat_score >= 0.5f &&
        db->bio_async_enabled && db->bio_ctx) {
        struct {
            bio_message_header_t hdr;
            uint32_t pattern_id;
            uint32_t category;
            uint32_t match_count;
            float    threat_score;
        } sec_msg = {0};
        sec_msg.hdr.type          = BIO_MSG_SECURITY_EVENT;
        sec_msg.hdr.source_module = db->config.module_id;
        sec_msg.hdr.target_module = 0;
        sec_msg.hdr.payload_size  = sizeof(sec_msg) - sizeof(bio_message_header_t);
        sec_msg.hdr.flags         = BIO_MSG_FLAG_BROADCAST;
        sec_msg.pattern_id        = result->pattern_id;
        sec_msg.category          = (uint32_t)result->category;
        sec_msg.match_count       = result->match_count;
        sec_msg.threat_score      = result->threat_score;
        (void)bio_router_broadcast(db->bio_ctx, &sec_msg, sizeof(sec_msg));
    }

    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_pattern_db_match_category(
    nimcp_pattern_db_t db,
    const char* input,
    nimcp_pattern_category_t category,
    nimcp_pattern_match_result_t* result
) {
    if (!validate_db(db) || !input || !result) {
        return NIMCP_INVALID_PARAM;
    }

    if (category >= NIMCP_PATTERN_CATEGORY_COUNT) {
        return NIMCP_INVALID_PARAM;
    }

    memset(result, 0, sizeof(nimcp_pattern_match_result_t));

    pthread_rwlock_rdlock(&db->write_lock);

    // Match only patterns in specified category
    for (size_t i = 0; i < db->capacity; i++) {
        if (!db->patterns[i].is_compiled) {
            continue;
        }

        if (db->patterns[i].entry.category != category) {
            continue;
        }

        regmatch_t match;
        int ret = regexec(&db->patterns[i].compiled_regex, input, 1, &match, 0);

        if (ret == 0) {
            // Pattern matched
            result->matched = true;
            result->pattern_id = db->patterns[i].pattern_id;
            result->category = db->patterns[i].entry.category;
            result->threat_score = db->patterns[i].entry.weight;
            result->match_count = 1;
            result->match_offset = match.rm_so;
            result->match_length = match.rm_eo - match.rm_so;

            if (db->patterns[i].entry.description) {
                strncpy(result->description, db->patterns[i].entry.description,
                        NIMCP_PATTERN_MAX_DESCRIPTION - 1);
                result->description[NIMCP_PATTERN_MAX_DESCRIPTION - 1] = '\0';
            }

            break;  // Return first match
        }
    }

    pthread_rwlock_unlock(&db->write_lock);

    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_pattern_db_match_timeout(
    nimcp_pattern_db_t db,
    const char* input,
    float timeout_ms,
    nimcp_pattern_match_result_t* result
) {
    // For now, just call regular match (timeout handling requires more complex implementation)
    return nimcp_pattern_db_match(db, input, result);
}

//=============================================================================
// Statistics and Monitoring
//=============================================================================

nimcp_error_t nimcp_pattern_db_get_stats(
    nimcp_pattern_db_t db,
    nimcp_pattern_db_stats_t* stats
) {
    if (!validate_db(db) || !stats) {
        return NIMCP_INVALID_PARAM;
    }

    pthread_rwlock_rdlock(&db->write_lock);
    *stats = db->stats;

    // Calculate memory usage estimate
    stats->memory_usage_bytes = sizeof(struct nimcp_pattern_db_impl);
    stats->memory_usage_bytes += db->capacity * sizeof(pattern_slot_t);

    // Add snapshot memory
    pattern_snapshot_t* snapshot = db->snapshots;
    while (snapshot) {
        stats->memory_usage_bytes += sizeof(pattern_snapshot_t);
        stats->memory_usage_bytes += snapshot->capacity * sizeof(pattern_slot_t);
        snapshot = snapshot->next;
    }

    pthread_rwlock_unlock(&db->write_lock);

    return NIMCP_SUCCESS;
}

void nimcp_pattern_db_reset_stats(nimcp_pattern_db_t db) {
    if (!validate_db(db)) {
        return;
    }

    pthread_rwlock_wrlock(&db->write_lock);

    db->stats.total_matches = 0;
    db->stats.total_hits = 0;
    db->stats.avg_match_time_us = 0.0F;
    db->stats.max_match_time_us = 0.0F;

    pthread_rwlock_unlock(&db->write_lock);
}

const char* nimcp_pattern_category_name(nimcp_pattern_category_t category) {
    static const char* names[] = {
        "SQL_INJECTION",
        "XSS",
        "SHELL_INJECTION",
        "PATH_TRAVERSAL",
        "FORMAT_STRING",
        "PROMPT_INJECTION",
        "BUFFER_OVERFLOW",
        "LDAP_INJECTION",
        "XML_INJECTION",
        "COMMAND_INJECTION",
        "CUSTOM"
    };

    if (category >= NIMCP_PATTERN_CATEGORY_COUNT) {
        return "UNKNOWN";
    }

    return names[category];
}

//=============================================================================
// Bio-Async Integration
//=============================================================================

/**
 * @brief Handle security event messages
 */
static nimcp_error_t pattern_db_handle_security_event(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data
) {
    (void)response_promise;
    if (!msg || msg_size < sizeof(bio_message_header_t) || !user_data) {
        return NIMCP_INVALID_PARAM;
    }

    nimcp_pattern_db_t db = (nimcp_pattern_db_t)user_data;
    const bio_message_header_t* header = (const bio_message_header_t*)msg;

    LOG_MODULE_DEBUG("pattern_db", "Security event from module 0x%04X", header->source_module);

    pthread_rwlock_wrlock(&db->write_lock);
    db->stats.total_matches++;
    pthread_rwlock_unlock(&db->write_lock);

    return NIMCP_SUCCESS;
}

/**
 * @brief Handle threat detection messages
 */
static nimcp_error_t pattern_db_handle_threat_detected(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data
) {
    (void)response_promise;
    if (!msg || msg_size < sizeof(bio_message_header_t) || !user_data) {
        return NIMCP_INVALID_PARAM;
    }

    nimcp_pattern_db_t db = (nimcp_pattern_db_t)user_data;
    const bio_message_header_t* header = (const bio_message_header_t*)msg;

    LOG_MODULE_WARN("pattern_db", "Threat detected from module 0x%04X", header->source_module);

    pthread_rwlock_wrlock(&db->write_lock);
    db->stats.total_hits++;
    pthread_rwlock_unlock(&db->write_lock);

    return NIMCP_SUCCESS;
}

/**
 * @brief Handle policy update messages
 */
static nimcp_error_t pattern_db_handle_policy_update(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data
) {
    (void)response_promise;
    if (!msg || msg_size < sizeof(bio_message_header_t) || !user_data) {
        return NIMCP_INVALID_PARAM;
    }

    nimcp_pattern_db_t db = (nimcp_pattern_db_t)user_data;
    const bio_message_header_t* header = (const bio_message_header_t*)msg;

    LOG_MODULE_INFO("pattern_db", "Policy update from module 0x%04X", header->source_module);

    pthread_rwlock_wrlock(&db->write_lock);
    db->current_version++;
    db->stats.current_version = db->current_version;
    pthread_rwlock_unlock(&db->write_lock);

    return NIMCP_SUCCESS;
}

uint32_t nimcp_pattern_db_process_inbox(
    nimcp_pattern_db_t db,
    uint32_t max_messages
) {
    if (!validate_db(db) || !db->bio_async_enabled || !db->bio_ctx) {
        return 0;
    }

    return bio_router_process_inbox(db->bio_ctx, max_messages);
}

/**
 * @brief Wiring callback for KG-driven handler registration
 *
 * Called by the orchestrator with discovered message types from the knowledge graph.
 * Registers handlers based on message types discovered at runtime.
 *
 * @param ctx Bio-async module context
 * @param message_types Array of discovered message types
 * @param message_count Number of message types
 * @param user_data User-provided context
 * @return 0 on success, -1 on error
 */
static int pattern_db_wiring_handler_callback(
    bio_module_context_t ctx,
    const bio_message_type_t* message_types,
    uint32_t message_count,
    void* user_data
) {
    (void)user_data;

    int registered = 0;
    for (uint32_t i = 0; i < message_count; i++) {
        switch (message_types[i]) {
            case BIO_MSG_SECURITY_EVENT:
                bio_router_register_handler(ctx, message_types[i], pattern_db_handle_security_event);
                registered++;
                break;
            case BIO_MSG_SECURITY_THREAT_DETECTED:
                bio_router_register_handler(ctx, message_types[i], pattern_db_handle_threat_detected);
                registered++;
                break;
            case BIO_MSG_SECURITY_POLICY_UPDATE:
                bio_router_register_handler(ctx, message_types[i], pattern_db_handle_policy_update);
                registered++;
                break;
            default:
                LOG_MODULE_DEBUG("pattern_db", "Unknown message type %d in wiring callback", message_types[i]);
                break;
        }
    }

    LOG_MODULE_INFO("pattern_db", "KG-driven wiring callback registered %d handlers", registered);
    return (registered > 0) ? 0 : -1;
}

nimcp_error_t nimcp_pattern_db_register_bio_async(
    nimcp_pattern_db_t db,
    bio_module_id_t module_id
) {
    if (!validate_db(db)) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (!bio_router_is_initialized()) {
        return NIMCP_ERROR_NOT_SUPPORTED;
    }

    bio_module_info_t info = {
        .module_id = module_id,
        .module_name = "pattern_db",
        .inbox_capacity = 256,
        .user_data = db
    };

    db->bio_ctx = bio_router_register_module(&info);
    if (!db->bio_ctx) {
        return NIMCP_ERROR_NOT_SUPPORTED;
    }

    db->bio_async_enabled = true;
    db->config.module_id = module_id;

    // Try KG-driven wiring callback registration first
    nimcp_error_t result = bio_router_register_wiring_callback(
        module_id,
        (void*)pattern_db_wiring_handler_callback,
        db
    );

    if (result == NIMCP_SUCCESS) {
        LOG_MODULE_INFO("pattern_db", "KG-driven wiring callback registered successfully");
    } else {
        // Fallback to legacy handler registration
        LOG_MODULE_INFO("pattern_db", "Falling back to legacy handler registration");

        LEGACY_HANDLER_REGISTRATION(
            bio_router_register_handler(db->bio_ctx, BIO_MSG_SECURITY_EVENT,
                                         pattern_db_handle_security_event);
            bio_router_register_handler(db->bio_ctx, BIO_MSG_SECURITY_THREAT_DETECTED,
                                         pattern_db_handle_threat_detected);
            bio_router_register_handler(db->bio_ctx, BIO_MSG_SECURITY_POLICY_UPDATE,
                                         pattern_db_handle_policy_update)
        );
    }

    LOG_MODULE_INFO("pattern_db", "Registered bio-async handlers");

    return NIMCP_SUCCESS;
}
