/**
 * @file nimcp_security_memory_bridge.c
 * @brief Security - Memory Systems Bidirectional Integration Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-09
 *
 * WHAT: Implementation of security-memory bridge for protecting sensitive data
 * WHY:  Enforce access control, encryption, and audit on memory operations
 * HOW:  Access tables, classification, AES-256-GCM stubs, secure erase
 */

#include "security/memory/nimcp_security_memory_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"

BRIDGE_BOILERPLATE_MESH_ONLY(security_memory_bridge, MESH_ADAPTER_CATEGORY_SECURITY)


/* ============================================================================
 * Internal Constants
 * ============================================================================ */

/** @brief Whitelist entry for legitimate transfers */
typedef struct {
    security_mem_system_type_t source;
    security_mem_system_type_t dest;
    uint32_t subject_id;
    uint64_t valid_until;
} transfer_whitelist_entry_t;

/** @brief Maximum whitelist entries */
#define MAX_WHITELIST_ENTRIES 64

/** @brief Internal bridge state */
typedef struct {
    /* Whitelist for legitimate transfers */
    transfer_whitelist_entry_t whitelist[MAX_WHITELIST_ENTRIES];
    uint32_t whitelist_count;

    /* Access pattern history for anomaly detection */
    struct {
        uint32_t subject_id;
        security_mem_system_type_t memory_type;
        security_mem_operation_t operation;
        uint64_t timestamp;
    } access_history[SEC_MEM_PATTERN_HISTORY];
    uint32_t history_head;
    uint32_t history_count;

    /* Failed attempt tracking */
    struct {
        uint32_t subject_id;
        uint32_t failed_count;
        uint64_t first_failure;
    } failed_attempts[SEC_MEM_MAX_SUBJECTS];
    uint32_t failed_subjects_count;

    /* Encryption keys (stub - in production use HSM) */
    uint8_t keys[SEC_MEM_MAX_KEYS][SEC_MEM_AES256_KEY_SIZE];
    uint64_t key_timestamps[SEC_MEM_MAX_KEYS];
} security_mem_internal_t;

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Get current timestamp in microseconds
 */
static uint64_t get_timestamp_us(void)
{
    return nimcp_time_monotonic_us();
}

/**
 * @brief Find subject in access table
 *
 * @param bridge Bridge handle
 * @param subject_id Subject to find
 * @return Index in access_table or -1 if not found
 */
static int find_subject_index(const security_mem_bridge_t* bridge, uint32_t subject_id)
{
    if (!bridge || !bridge->access_table) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_subject_index: required parameter is NULL (bridge, bridge->access_table)");
        return -1;
    }

    for (uint32_t i = 0; i < bridge->num_subjects; i++) {
        if (bridge->access_table[i].subject_id == subject_id) {
            return (int)i;
        }
    }
    return -1;
}

/**
 * @brief Check if subject has permission for operation
 *
 * @param rights Subject access rights
 * @param operation Requested operation
 * @return true if permitted, false otherwise
 */
static bool check_operation_permission(
    const security_mem_access_rights_t* rights,
    security_mem_operation_t operation
)
{
    if (!rights) {
        return false;
    }

    switch (operation) {
        case SEC_MEM_OP_READ:
            return rights->can_read;
        case SEC_MEM_OP_WRITE:
            return rights->can_write;
        case SEC_MEM_OP_DELETE:
            return rights->can_delete;
        case SEC_MEM_OP_SHARE:
            return rights->can_share;
        case SEC_MEM_OP_CONSOLIDATE:
            return rights->can_consolidate;
        case SEC_MEM_OP_RETRIEVE:
            return rights->can_retrieve;
        case SEC_MEM_OP_ENCODE:
            return rights->can_encode;
        default:
            return false;
    }
}

/**
 * @brief Check if subject can access memory system
 *
 * @param rights Subject access rights
 * @param memory_type Memory system type
 * @return true if accessible, false otherwise
 */
static bool check_memory_system_access(
    const security_mem_access_rights_t* rights,
    security_mem_system_type_t memory_type
)
{
    if (!rights) {
        return false;
    }

    uint32_t mask = 1U << (uint32_t)memory_type;
    return (rights->memory_systems_mask & mask) != 0;
}

/**
 * @brief Record access in history for pattern analysis
 *
 * @param bridge Bridge handle (must be locked)
 * @param internal Internal state
 * @param subject_id Subject
 * @param memory_type Memory system
 * @param operation Operation
 */
static void record_access_pattern(
    security_mem_bridge_t* bridge,
    security_mem_internal_t* internal,
    uint32_t subject_id,
    security_mem_system_type_t memory_type,
    security_mem_operation_t operation
)
{
    if (!internal) {
        return;
    }

    uint32_t idx = internal->history_head;
    internal->access_history[idx].subject_id = subject_id;
    internal->access_history[idx].memory_type = memory_type;
    internal->access_history[idx].operation = operation;
    internal->access_history[idx].timestamp = get_timestamp_us();

    internal->history_head = (internal->history_head + 1) % SEC_MEM_PATTERN_HISTORY;
    if (internal->history_count < SEC_MEM_PATTERN_HISTORY) {
        internal->history_count++;
    }

    (void)bridge;
}

/**
 * @brief Record failed access attempt
 *
 * @param internal Internal state
 * @param subject_id Subject that failed
 */
static void record_failed_attempt(security_mem_internal_t* internal, uint32_t subject_id)
{
    if (!internal) {
        return;
    }

    /* Find existing entry for subject */
    for (uint32_t i = 0; i < internal->failed_subjects_count; i++) {
        if (internal->failed_attempts[i].subject_id == subject_id) {
            internal->failed_attempts[i].failed_count++;
            return;
        }
    }

    /* Add new entry */
    if (internal->failed_subjects_count < SEC_MEM_MAX_SUBJECTS) {
        uint32_t idx = internal->failed_subjects_count++;
        internal->failed_attempts[idx].subject_id = subject_id;
        internal->failed_attempts[idx].failed_count = 1;
        internal->failed_attempts[idx].first_failure = get_timestamp_us();
    }
}

/**
 * @brief Check if subject is locked out
 *
 * @param bridge Bridge handle
 * @param internal Internal state
 * @param subject_id Subject to check
 * @return true if locked out, false otherwise
 */
static bool is_subject_locked_out(
    const security_mem_bridge_t* bridge,
    const security_mem_internal_t* internal,
    uint32_t subject_id
)
{
    if (!bridge || !internal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "is_subject_locked_out: required parameter is NULL (bridge, internal)");
        return false;
    }

    for (uint32_t i = 0; i < internal->failed_subjects_count; i++) {
        if (internal->failed_attempts[i].subject_id == subject_id) {
            if (internal->failed_attempts[i].failed_count >= bridge->config.max_failed_attempts) {
                uint64_t lockout_end = internal->failed_attempts[i].first_failure +
                                       (uint64_t)bridge->config.lockout_duration_s * 1000000ULL;
                if (get_timestamp_us() < lockout_end) {
                    return true;
                }
            }
            break;
        }
    }
    return false;
}

/**
 * @brief Simple XOR-based "encryption" stub for demonstration
 *
 * In production, this would use a proper AES-256-GCM implementation
 * via OpenSSL, libsodium, or hardware crypto module.
 */
static void stub_encrypt(
    const uint8_t* plaintext,
    size_t size,
    const uint8_t* key,
    uint8_t* ciphertext,
    uint8_t* iv,
    uint8_t* tag
)
{
    /* Generate pseudo-random IV */
    uint64_t timestamp = get_timestamp_us();
    for (size_t i = 0; i < SEC_MEM_GCM_IV_SIZE; i++) {
        iv[i] = (uint8_t)((timestamp >> (i * 4)) ^ (timestamp >> ((i + 4) * 4)));
    }

    /* Stub XOR "encryption" - NOT SECURE, placeholder only */
    for (size_t i = 0; i < size; i++) {
        ciphertext[i] = plaintext[i] ^ key[i % SEC_MEM_AES256_KEY_SIZE] ^ iv[i % SEC_MEM_GCM_IV_SIZE];
    }

    /* Generate pseudo-tag */
    memset(tag, 0, SEC_MEM_GCM_TAG_SIZE);
    for (size_t i = 0; i < size && i < SEC_MEM_GCM_TAG_SIZE; i++) {
        tag[i] = ciphertext[i] ^ key[i];
    }
}

/**
 * @brief Simple XOR-based "decryption" stub for demonstration
 */
static bool stub_decrypt(
    const uint8_t* ciphertext,
    size_t size,
    const uint8_t* key,
    const uint8_t* iv,
    const uint8_t* tag,
    uint8_t* plaintext
)
{
    /* Verify tag (stub verification) */
    uint8_t computed_tag[SEC_MEM_GCM_TAG_SIZE];
    memset(computed_tag, 0, SEC_MEM_GCM_TAG_SIZE);
    for (size_t i = 0; i < size && i < SEC_MEM_GCM_TAG_SIZE; i++) {
        computed_tag[i] = ciphertext[i] ^ key[i];
    }

    if (memcmp(computed_tag, tag, SEC_MEM_GCM_TAG_SIZE) != 0) {
        return false;  /* Authentication failed */
    }

    /* Stub XOR "decryption" */
    for (size_t i = 0; i < size; i++) {
        plaintext[i] = ciphertext[i] ^ key[i % SEC_MEM_AES256_KEY_SIZE] ^ iv[i % SEC_MEM_GCM_IV_SIZE];
    }

    return true;
}

/**
 * @brief Multi-pass secure erase
 *
 * @param data Data to erase
 * @param size Size of data
 * @param passes Number of overwrite passes
 */
static void secure_erase_memory(void* data, size_t size, uint32_t passes)
{
    if (!data || size == 0) {
        return;
    }

    volatile uint8_t* vdata = (volatile uint8_t*)data;

    for (uint32_t pass = 0; pass < passes; pass++) {
        /* Pass 1: zeros, Pass 2: ones, Pass 3+: pseudo-random */
        uint8_t pattern;
        if (pass == 0) {
            pattern = 0x00;
        } else if (pass == 1) {
            pattern = 0xFF;
        } else {
            pattern = (uint8_t)(pass * 0x37 + 0x5A);  /* Simple pseudo-random pattern */
        }

        for (size_t i = 0; i < size; i++) {
            vdata[i] = pattern;
        }

        /* Memory barrier to prevent optimization */
        __asm__ __volatile__("" ::: "memory");
    }

    /* Final zero pass */
    for (size_t i = 0; i < size; i++) {
        vdata[i] = 0x00;
    }
}

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

int security_memory_default_config(security_mem_config_t* config)
{
    NIMCP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");

    memset(config, 0, sizeof(security_mem_config_t));

    /* Feature enable flags */
    config->enable_access_control = true;
    config->enable_encryption = true;
    config->enable_secure_erase = true;
    config->enable_classification = true;
    config->enable_audit = true;
    config->enable_leakage_detection = true;
    config->enable_anomaly_detection = true;

    /* Encryption settings */
    config->encrypt_at_rest = true;
    config->encrypt_in_transit = true;
    config->min_encryption_class = SEC_MEM_CLASS_CONFIDENTIAL;
    config->key_rotation_interval_s = 86400;  /* 24 hours */

    /* Access control settings */
    config->default_access_mask = 0x0F;  /* All memory systems by default */
    config->require_mfa_for_secret = false;
    config->max_failed_attempts = 5;
    config->lockout_duration_s = 300;  /* 5 minutes */

    /* Secure erase settings */
    config->erase_passes = 3;
    config->verify_erase = true;

    /* Audit settings */
    config->audit_all_access = false;  /* Only audit denials by default */
    config->audit_retention_days = 30;

    /* Leakage detection settings */
    config->leakage_threshold = 0.7f;
    config->pattern_window_size = 50;

    /* Sensitivity parameters */
    config->security_sensitivity = 1.0f;
    config->memory_sensitivity = 1.0f;

    /* Bio-async integration */
    config->enable_bio_async = true;

    return 0;
}

security_mem_bridge_t* security_memory_bridge_create(const security_mem_config_t* config)
{
    /* Allocate bridge */
    security_mem_bridge_t* bridge = nimcp_malloc(sizeof(security_mem_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate security-memory bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;
    }
    memset(bridge, 0, sizeof(security_mem_bridge_t));

    /* Initialize base */
    if (bridge_base_init(&bridge->base, BIO_MODULE_SECURITY_MEMORY, "security_memory_bridge") != 0) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "security_memory_bridge_create: validation failed");
        return NULL;
    }

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        security_memory_default_config(&bridge->config);
    }

    /* Allocate access table */
    bridge->access_table = nimcp_malloc(SEC_MEM_MAX_SUBJECTS * sizeof(security_mem_access_rights_t));
    if (!bridge->access_table) {
        NIMCP_LOGGING_ERROR("Failed to allocate access table");
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "security_memory_bridge_create: bridge->access_table is NULL");
        return NULL;
    }
    memset(bridge->access_table, 0, SEC_MEM_MAX_SUBJECTS * sizeof(security_mem_access_rights_t));

    /* Allocate audit log */
    if (bridge->config.enable_audit) {
        bridge->audit_log = nimcp_malloc(SEC_MEM_MAX_AUDIT_ENTRIES * sizeof(security_mem_audit_entry_t));
        if (!bridge->audit_log) {
            NIMCP_LOGGING_ERROR("Failed to allocate audit log");
            nimcp_free(bridge->access_table);
            bridge_base_cleanup(&bridge->base);
            nimcp_free(bridge);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "security_memory_bridge_create: bridge->audit_log is NULL");
            return NULL;
        }
        memset(bridge->audit_log, 0, SEC_MEM_MAX_AUDIT_ENTRIES * sizeof(security_mem_audit_entry_t));
    }

    /* Allocate internal state */
    security_mem_internal_t* internal = nimcp_malloc(sizeof(security_mem_internal_t));
    if (!internal) {
        NIMCP_LOGGING_ERROR("Failed to allocate internal state");
        if (bridge->audit_log) {
            nimcp_free(bridge->audit_log);
        }
        nimcp_free(bridge->access_table);
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_memory_bridge_create: validation failed");
        return NULL;
    }
    memset(internal, 0, sizeof(security_mem_internal_t));

    /* Initialize encryption keys (stub - in production derive from HSM) */
    for (uint32_t i = 0; i < SEC_MEM_MAX_KEYS; i++) {
        for (uint32_t j = 0; j < SEC_MEM_AES256_KEY_SIZE; j++) {
            internal->keys[i][j] = (uint8_t)(i * 17 + j * 31 + 0xAB);
        }
        internal->key_timestamps[i] = get_timestamp_us();
    }

    /* Store internal state in base system_b (unused for connections) */
    bridge->base.system_b = internal;

    /* Initialize state */
    bridge->state.state = SEC_MEM_STATE_IDLE;

    NIMCP_LOGGING_INFO("Created security-memory bridge");
    return bridge;
}

void security_memory_bridge_destroy(security_mem_bridge_t* bridge)
{
    if (!bridge) {
        return;
    }

    /* Disconnect bio-async */
    if (bridge->base.bio_async_enabled) {
        security_memory_disconnect_bio_async(bridge);
    }

    /* Secure erase encryption keys */
    security_mem_internal_t* internal = (security_mem_internal_t*)bridge->base.system_b;
    if (internal) {
        secure_erase_memory(internal->keys, sizeof(internal->keys), 3);
        nimcp_free(internal);
    }

    /* Free audit log */
    if (bridge->audit_log) {
        nimcp_free(bridge->audit_log);
    }

    /* Free access table */
    if (bridge->access_table) {
        nimcp_free(bridge->access_table);
    }

    /* Cleanup base */
    bridge_base_cleanup(&bridge->base);

    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Destroyed security-memory bridge");
}

int security_memory_bridge_reset(security_mem_bridge_t* bridge)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    BRIDGE_LOCK(bridge);

    /* Reset statistics */
    memset(&bridge->stats, 0, sizeof(security_mem_stats_t));

    /* Reset effects */
    memset(&bridge->security_effects, 0, sizeof(security_to_memory_effects_t));
    memset(&bridge->memory_effects, 0, sizeof(memory_to_security_effects_t));

    /* Reset state */
    bridge->state.state = SEC_MEM_STATE_IDLE;
    bridge->state.last_access_check = 0;
    bridge->state.last_encryption = 0;
    bridge->state.last_audit = 0;
    bridge->state.last_leak_check = 0;
    bridge->state.active_sessions = 0;
    bridge->state.pending_operations = 0;

    /* Clear audit log */
    bridge->audit_log_head = 0;
    bridge->audit_log_count = 0;

    /* Reset internal state */
    security_mem_internal_t* internal = (security_mem_internal_t*)bridge->base.system_b;
    if (internal) {
        internal->whitelist_count = 0;
        internal->history_head = 0;
        internal->history_count = 0;
        internal->failed_subjects_count = 0;
    }

    BRIDGE_UNLOCK(bridge);

    NIMCP_LOGGING_INFO("Reset security-memory bridge");
    return 0;
}

/* ============================================================================
 * Connection Functions
 * ============================================================================ */

int security_memory_connect_working(
    security_mem_bridge_t* bridge,
    working_memory_t* working_memory
)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(working_memory, NIMCP_ERROR_NULL_POINTER, "working_memory is NULL");

    BRIDGE_LOCK(bridge);
    bridge->working_memory = working_memory;
    bridge->working_connected = true;
    bridge->state.all_systems_connected = security_memory_is_fully_connected(bridge);
    BRIDGE_UNLOCK(bridge);

    NIMCP_LOGGING_INFO("Connected working memory to security-memory bridge");
    return 0;
}

int security_memory_connect_episodic(
    security_mem_bridge_t* bridge,
    episodic_memory_t* episodic_memory
)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(episodic_memory, NIMCP_ERROR_NULL_POINTER, "episodic_memory is NULL");

    BRIDGE_LOCK(bridge);
    bridge->episodic_memory = episodic_memory;
    bridge->episodic_connected = true;
    bridge->state.all_systems_connected = security_memory_is_fully_connected(bridge);
    BRIDGE_UNLOCK(bridge);

    NIMCP_LOGGING_INFO("Connected episodic memory to security-memory bridge");
    return 0;
}

int security_memory_connect_semantic(
    security_mem_bridge_t* bridge,
    semantic_memory_system_t* semantic_memory
)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(semantic_memory, NIMCP_ERROR_NULL_POINTER, "semantic_memory is NULL");

    BRIDGE_LOCK(bridge);
    bridge->semantic_memory = semantic_memory;
    bridge->semantic_connected = true;
    bridge->state.all_systems_connected = security_memory_is_fully_connected(bridge);
    BRIDGE_UNLOCK(bridge);

    NIMCP_LOGGING_INFO("Connected semantic memory to security-memory bridge");
    return 0;
}

int security_memory_connect_procedural(
    security_mem_bridge_t* bridge,
    procedural_memory_t procedural_memory
)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(procedural_memory, NIMCP_ERROR_NULL_POINTER, "procedural_memory is NULL");

    BRIDGE_LOCK(bridge);
    bridge->procedural_memory = procedural_memory;
    bridge->procedural_connected = true;
    bridge->state.all_systems_connected = security_memory_is_fully_connected(bridge);
    BRIDGE_UNLOCK(bridge);

    NIMCP_LOGGING_INFO("Connected procedural memory to security-memory bridge");
    return 0;
}

int security_memory_connect_bbb(
    security_mem_bridge_t* bridge,
    bbb_system_t bbb
)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(bbb, NIMCP_ERROR_NULL_POINTER, "bbb is NULL");

    BRIDGE_LOCK(bridge);
    bridge->bbb = bbb;
    bridge->bbb_connected = true;
    BRIDGE_UNLOCK(bridge);

    NIMCP_LOGGING_INFO("Connected BBB to security-memory bridge");
    return 0;
}

int security_memory_disconnect_all(security_mem_bridge_t* bridge)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    BRIDGE_LOCK(bridge);

    bridge->working_memory = NULL;
    bridge->working_connected = false;
    bridge->episodic_memory = NULL;
    bridge->episodic_connected = false;
    bridge->semantic_memory = NULL;
    bridge->semantic_connected = false;
    bridge->procedural_memory = NULL;
    bridge->procedural_connected = false;
    bridge->bbb = NULL;
    bridge->bbb_connected = false;
    bridge->state.all_systems_connected = false;

    BRIDGE_UNLOCK(bridge);

    NIMCP_LOGGING_INFO("Disconnected all systems from security-memory bridge");
    return 0;
}

bool security_memory_is_fully_connected(const security_mem_bridge_t* bridge)
{
    if (!bridge) {
        return false;
    }
    return bridge->working_connected &&
           bridge->episodic_connected &&
           bridge->semantic_connected &&
           bridge->procedural_connected;
}

/* ============================================================================
 * Access Control Functions
 * ============================================================================ */

bool security_memory_check_access(
    security_mem_bridge_t* bridge,
    uint32_t subject_id,
    security_mem_system_type_t memory_type,
    security_mem_operation_t operation,
    security_mem_classification_t data_classification
)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_memory_check_access: bridge is NULL");
        return false;
    }

    uint64_t start_time = get_timestamp_us();
    bool access_granted = false;

    BRIDGE_LOCK(bridge);

    bridge->state.state = SEC_MEM_STATE_CHECKING;
    bridge->stats.total_access_checks++;

    security_mem_internal_t* internal = (security_mem_internal_t*)bridge->base.system_b;

    /* Check lockout status */
    if (is_subject_locked_out(bridge, internal, subject_id)) {
        bridge->security_effects.lockout_active = true;
        access_granted = false;
        goto access_check_done;
    }

    /* Find subject rights */
    int idx = find_subject_index(bridge, subject_id);
    if (idx < 0) {
        /* Subject not registered - deny */
        access_granted = false;
        goto access_check_done;
    }

    const security_mem_access_rights_t* rights = &bridge->access_table[idx];

    /* Check expiration */
    if (rights->valid_until != 0 && get_timestamp_us() > rights->valid_until) {
        access_granted = false;
        goto access_check_done;
    }

    /* Check memory system access */
    if (!check_memory_system_access(rights, memory_type)) {
        access_granted = false;
        goto access_check_done;
    }

    /* Check operation permission */
    if (!check_operation_permission(rights, operation)) {
        access_granted = false;
        goto access_check_done;
    }

    /* Check classification clearance */
    if ((uint32_t)data_classification > rights->max_classification) {
        access_granted = false;
        goto access_check_done;
    }

    /* MFA requirement for SECRET+ (stub - would integrate with auth system) */
    if (bridge->config.require_mfa_for_secret &&
        data_classification >= SEC_MEM_CLASS_SECRET) {
        /* In production, verify MFA token here */
    }

    access_granted = true;

access_check_done:
    /* Record access pattern */
    if (internal) {
        record_access_pattern(bridge, internal, subject_id, memory_type, operation);
    }

    /* Update statistics */
    if (access_granted) {
        bridge->stats.access_granted++;

        /* Update per-memory system stats */
        switch (memory_type) {
            case SEC_MEM_TYPE_WORKING:
                bridge->stats.working_memory_ops++;
                break;
            case SEC_MEM_TYPE_EPISODIC:
                bridge->stats.episodic_memory_ops++;
                break;
            case SEC_MEM_TYPE_SEMANTIC:
                bridge->stats.semantic_memory_ops++;
                break;
            case SEC_MEM_TYPE_PROCEDURAL:
                bridge->stats.procedural_memory_ops++;
                break;
        }
    } else {
        bridge->stats.access_denied++;
        bridge->security_effects.blocked_operations++;

        if (internal) {
            record_failed_attempt(internal, subject_id);
        }
    }

    /* Audit if configured - capture decision under lock, call audit after unlock
     * to avoid holding two locks (audit function acquires its own BRIDGE_LOCK) */
    bool needs_audit = bridge->config.enable_audit &&
                       (bridge->config.audit_all_access || !access_granted);

    if (needs_audit) {
        BRIDGE_UNLOCK(bridge);
        security_memory_audit_access(bridge, subject_id, memory_type, operation,
                                     data_classification, access_granted, NULL);
        BRIDGE_LOCK(bridge);
    }

    /* Update latency stats */
    uint64_t elapsed = get_timestamp_us() - start_time;
    float latency_us = (float)elapsed;
    bridge->stats.mean_check_latency_us =
        (bridge->stats.mean_check_latency_us * (bridge->stats.total_access_checks - 1) +
         latency_us) / bridge->stats.total_access_checks;

    bridge->state.state = SEC_MEM_STATE_IDLE;
    bridge->state.last_access_check = get_timestamp_us();

    BRIDGE_UNLOCK(bridge);

    return access_granted;
}

int security_memory_register_subject(
    security_mem_bridge_t* bridge,
    const security_mem_access_rights_t* rights
)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(rights, NIMCP_ERROR_NULL_POINTER, "rights is NULL");

    BRIDGE_LOCK(bridge);

    /* Check if already registered */
    int idx = find_subject_index(bridge, rights->subject_id);
    if (idx >= 0) {
        /* Update existing */
        bridge->access_table[idx] = *rights;
        BRIDGE_UNLOCK(bridge);
        return 0;
    }

    /* Check capacity */
    if (bridge->num_subjects >= SEC_MEM_MAX_SUBJECTS) {
        NIMCP_LOGGING_ERROR("Access table full");
        BRIDGE_UNLOCK(bridge);
        return NIMCP_ERROR_OUT_OF_RANGE;
    }

    /* Add new subject */
    bridge->access_table[bridge->num_subjects++] = *rights;

    BRIDGE_UNLOCK(bridge);

    NIMCP_LOGGING_INFO("Registered subject %u with access rights", rights->subject_id);
    return 0;
}

int security_memory_update_rights(
    security_mem_bridge_t* bridge,
    const security_mem_access_rights_t* rights
)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(rights, NIMCP_ERROR_NULL_POINTER, "rights is NULL");

    BRIDGE_LOCK(bridge);

    int idx = find_subject_index(bridge, rights->subject_id);
    if (idx < 0) {
        BRIDGE_UNLOCK(bridge);
        NIMCP_LOGGING_ERROR("Subject %u not found", rights->subject_id);
        return NIMCP_ERROR_NOT_FOUND;
    }

    bridge->access_table[idx] = *rights;

    BRIDGE_UNLOCK(bridge);

    NIMCP_LOGGING_INFO("Updated access rights for subject %u", rights->subject_id);
    return 0;
}

int security_memory_revoke_subject(
    security_mem_bridge_t* bridge,
    uint32_t subject_id
)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    BRIDGE_LOCK(bridge);

    int idx = find_subject_index(bridge, subject_id);
    if (idx < 0) {
        BRIDGE_UNLOCK(bridge);
        return NIMCP_ERROR_NOT_FOUND;
    }

    /* Shift remaining entries */
    for (uint32_t i = (uint32_t)idx; i < bridge->num_subjects - 1; i++) {
        bridge->access_table[i] = bridge->access_table[i + 1];
    }
    bridge->num_subjects--;

    BRIDGE_UNLOCK(bridge);

    NIMCP_LOGGING_INFO("Revoked access for subject %u", subject_id);
    return 0;
}

int security_memory_get_rights(
    const security_mem_bridge_t* bridge,
    uint32_t subject_id,
    security_mem_access_rights_t* rights_out
)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(rights_out, NIMCP_ERROR_NULL_POINTER, "rights_out is NULL");

    int idx = find_subject_index(bridge, subject_id);
    NIMCP_CHECK_THROW(idx >= 0, NIMCP_ERROR_NOT_FOUND, "subject not found");

    *rights_out = bridge->access_table[idx];
    return 0;
}

/* ============================================================================
 * Classification Functions
 * ============================================================================ */

int security_memory_classify_data(
    security_mem_bridge_t* bridge,
    const void* data_ptr,
    size_t data_size,
    const char* context_hint,
    security_mem_classification_t* classification_out
)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(classification_out, NIMCP_ERROR_NULL_POINTER, "classification_out is NULL");

    /* Default to PUBLIC */
    security_mem_classification_t classification = SEC_MEM_CLASS_PUBLIC;

    if (!data_ptr || data_size == 0) {
        *classification_out = classification;
        return 0;
    }

    const char* data = (const char*)data_ptr;

    /* Content-based classification (simplified pattern matching) */
    /* In production, use ML-based content analysis */

    /* Check for sensitive keywords */
    static const char* confidential_keywords[] = {
        "password", "secret", "private", "credential", "token", "key", NULL
    };
    static const char* secret_keywords[] = {
        "classified", "restricted", "confidential", "sensitive", NULL
    };
    static const char* top_secret_keywords[] = {
        "top_secret", "topsecret", "ultra", "cosmic", NULL
    };

    /* Simple substring search (case-insensitive would be better in production) */
    for (int i = 0; top_secret_keywords[i] != NULL; i++) {
        if (data_size >= strlen(top_secret_keywords[i]) &&
            strstr(data, top_secret_keywords[i]) != NULL) {
            classification = SEC_MEM_CLASS_TOP_SECRET;
            break;
        }
    }

    if (classification < SEC_MEM_CLASS_SECRET) {
        for (int i = 0; secret_keywords[i] != NULL; i++) {
            if (data_size >= strlen(secret_keywords[i]) &&
                strstr(data, secret_keywords[i]) != NULL) {
                classification = SEC_MEM_CLASS_SECRET;
                break;
            }
        }
    }

    if (classification < SEC_MEM_CLASS_CONFIDENTIAL) {
        for (int i = 0; confidential_keywords[i] != NULL; i++) {
            if (data_size >= strlen(confidential_keywords[i]) &&
                strstr(data, confidential_keywords[i]) != NULL) {
                classification = SEC_MEM_CLASS_CONFIDENTIAL;
                break;
            }
        }
    }

    /* Context hint can upgrade classification */
    if (context_hint) {
        if (strstr(context_hint, "top_secret") || strstr(context_hint, "maximum")) {
            classification = SEC_MEM_CLASS_TOP_SECRET;
        } else if (strstr(context_hint, "secret") || strstr(context_hint, "high")) {
            if (classification < SEC_MEM_CLASS_SECRET) {
                classification = SEC_MEM_CLASS_SECRET;
            }
        } else if (strstr(context_hint, "confidential") || strstr(context_hint, "medium")) {
            if (classification < SEC_MEM_CLASS_CONFIDENTIAL) {
                classification = SEC_MEM_CLASS_CONFIDENTIAL;
            }
        } else if (strstr(context_hint, "internal")) {
            if (classification < SEC_MEM_CLASS_INTERNAL) {
                classification = SEC_MEM_CLASS_INTERNAL;
            }
        }
    }

    /* Apply sensitivity multiplier */
    if (bridge->config.security_sensitivity > 1.0f && classification < SEC_MEM_CLASS_TOP_SECRET) {
        /* Higher sensitivity upgrades classification */
        if (bridge->config.security_sensitivity >= 1.5f) {
            classification = (security_mem_classification_t)(classification + 1);
        }
    }

    *classification_out = classification;

    BRIDGE_LOCK(bridge);
    bridge->stats.classifications_assigned++;
    BRIDGE_UNLOCK(bridge);

    return 0;
}

int security_memory_set_classification(
    security_mem_bridge_t* bridge,
    security_mem_system_type_t memory_type,
    uint64_t region_id,
    security_mem_classification_t classification
)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    /* In a full implementation, this would update a classification registry */
    /* For now, just update stats */
    (void)memory_type;
    (void)region_id;

    BRIDGE_LOCK(bridge);
    bridge->security_effects.reclassified_items++;
    BRIDGE_UNLOCK(bridge);

    return 0;
}

int security_memory_get_classification(
    const security_mem_bridge_t* bridge,
    security_mem_system_type_t memory_type,
    uint64_t region_id,
    security_mem_classification_t* classification_out
)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(classification_out, NIMCP_ERROR_NULL_POINTER, "classification_out is NULL");

    /* In a full implementation, look up from classification registry */
    (void)memory_type;
    (void)region_id;

    /* Default to PUBLIC if not tracked */
    *classification_out = SEC_MEM_CLASS_PUBLIC;
    return 0;
}

/* ============================================================================
 * Encryption Functions
 * ============================================================================ */

int security_memory_encrypt_sensitive(
    security_mem_bridge_t* bridge,
    const void* plaintext,
    size_t plaintext_size,
    security_mem_classification_t classification,
    void* ciphertext_out,
    size_t ciphertext_size,
    size_t* bytes_written_out
)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(plaintext, NIMCP_ERROR_NULL_POINTER, "plaintext is NULL");
    NIMCP_CHECK_THROW(ciphertext_out, NIMCP_ERROR_NULL_POINTER, "ciphertext_out is NULL");
    NIMCP_CHECK_THROW(bytes_written_out, NIMCP_ERROR_NULL_POINTER, "bytes_written_out is NULL");

    /* Check if encryption required for this classification */
    if ((uint32_t)classification < bridge->config.min_encryption_class) {
        /* No encryption needed - copy plaintext */
        NIMCP_CHECK_THROW(ciphertext_size >= plaintext_size, NIMCP_ERROR_BUFFER_TOO_SMALL,
                         "ciphertext buffer too small for unencrypted copy");
        memcpy(ciphertext_out, plaintext, plaintext_size);
        *bytes_written_out = plaintext_size;
        return 0;
    }

    /* Calculate output size: IV + ciphertext + tag */
    size_t output_size = SEC_MEM_GCM_IV_SIZE + plaintext_size + SEC_MEM_GCM_TAG_SIZE;
    NIMCP_CHECK_THROW(ciphertext_size >= output_size, NIMCP_ERROR_BUFFER_TOO_SMALL,
                     "output buffer too small: need %zu, have %zu", output_size, ciphertext_size);

    BRIDGE_LOCK(bridge);

    bridge->state.state = SEC_MEM_STATE_ENCRYPTING;

    security_mem_internal_t* internal = (security_mem_internal_t*)bridge->base.system_b;
    if (!internal) {
        BRIDGE_UNLOCK(bridge);
        NIMCP_CHECK_THROW(internal, NIMCP_ERROR_INVALID_STATE, "internal state is NULL");
    }

    /* Select key based on classification */
    uint32_t key_idx = (uint32_t)classification % SEC_MEM_MAX_KEYS;
    const uint8_t* key = internal->keys[key_idx];

    /* Output layout: IV | ciphertext | tag */
    uint8_t* output = (uint8_t*)ciphertext_out;
    uint8_t* iv = output;
    uint8_t* cipher = output + SEC_MEM_GCM_IV_SIZE;
    uint8_t* tag = output + SEC_MEM_GCM_IV_SIZE + plaintext_size;

    /* Encrypt */
    stub_encrypt((const uint8_t*)plaintext, plaintext_size, key, cipher, iv, tag);

    *bytes_written_out = output_size;

    /* Update stats */
    bridge->stats.total_encryptions++;
    bridge->stats.bytes_encrypted += plaintext_size;
    bridge->security_effects.encrypted_regions++;

    uint64_t elapsed = get_timestamp_us() - bridge->state.last_encryption;
    float latency_us = (float)elapsed;
    bridge->stats.mean_encrypt_latency_us =
        (bridge->stats.mean_encrypt_latency_us * (bridge->stats.total_encryptions - 1) +
         latency_us) / bridge->stats.total_encryptions;

    bridge->state.state = SEC_MEM_STATE_IDLE;
    bridge->state.last_encryption = get_timestamp_us();

    BRIDGE_UNLOCK(bridge);

    return 0;
}

int security_memory_decrypt_sensitive(
    security_mem_bridge_t* bridge,
    const void* ciphertext,
    size_t ciphertext_size,
    security_mem_classification_t classification,
    uint32_t subject_id,
    void* plaintext_out,
    size_t plaintext_size,
    size_t* bytes_written_out
)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(ciphertext, NIMCP_ERROR_NULL_POINTER, "ciphertext is NULL");
    NIMCP_CHECK_THROW(plaintext_out, NIMCP_ERROR_NULL_POINTER, "plaintext_out is NULL");
    NIMCP_CHECK_THROW(bytes_written_out, NIMCP_ERROR_NULL_POINTER, "bytes_written_out is NULL");

    /* Check access rights first */
    bool access_ok = security_memory_check_access(bridge, subject_id, SEC_MEM_TYPE_WORKING,
                                                  SEC_MEM_OP_READ, classification);
    NIMCP_CHECK_THROW(access_ok, NIMCP_ERROR_PERMISSION_DENIED,
                     "decryption denied for subject %u", subject_id);

    /* Check if encryption was applied */
    if ((uint32_t)classification < bridge->config.min_encryption_class) {
        /* Data was not encrypted - just copy */
        NIMCP_CHECK_THROW(plaintext_size >= ciphertext_size, NIMCP_ERROR_BUFFER_TOO_SMALL,
                         "plaintext buffer too small for unencrypted copy");
        memcpy(plaintext_out, ciphertext, ciphertext_size);
        *bytes_written_out = ciphertext_size;
        return 0;
    }

    /* Validate input size */
    NIMCP_CHECK_THROW(ciphertext_size >= SEC_MEM_GCM_IV_SIZE + SEC_MEM_GCM_TAG_SIZE,
                     NIMCP_ERROR_INVALID_PARAMETER, "ciphertext too small");

    size_t data_size = ciphertext_size - SEC_MEM_GCM_IV_SIZE - SEC_MEM_GCM_TAG_SIZE;
    NIMCP_CHECK_THROW(plaintext_size >= data_size, NIMCP_ERROR_BUFFER_TOO_SMALL,
                     "plaintext buffer too small for decrypted data");

    BRIDGE_LOCK(bridge);

    bridge->state.state = SEC_MEM_STATE_DECRYPTING;

    security_mem_internal_t* internal = (security_mem_internal_t*)bridge->base.system_b;
    if (!internal) {
        BRIDGE_UNLOCK(bridge);
        NIMCP_CHECK_THROW(internal, NIMCP_ERROR_INVALID_STATE, "internal state is NULL");
    }

    /* Select key */
    uint32_t key_idx = (uint32_t)classification % SEC_MEM_MAX_KEYS;
    const uint8_t* key = internal->keys[key_idx];

    /* Parse input: IV | ciphertext | tag */
    const uint8_t* input = (const uint8_t*)ciphertext;
    const uint8_t* iv = input;
    const uint8_t* cipher = input + SEC_MEM_GCM_IV_SIZE;
    const uint8_t* tag = input + SEC_MEM_GCM_IV_SIZE + data_size;

    /* Decrypt */
    bool success = stub_decrypt(cipher, data_size, key, iv, tag, (uint8_t*)plaintext_out);

    if (success) {
        *bytes_written_out = data_size;
        bridge->stats.total_decryptions++;
    } else {
        NIMCP_LOGGING_ERROR("Decryption authentication failed");
    }

    bridge->state.state = SEC_MEM_STATE_IDLE;

    BRIDGE_UNLOCK(bridge);

    return success ? 0 : NIMCP_ERROR_OPERATION_FAILED;
}

int security_memory_rotate_keys(
    security_mem_bridge_t* bridge,
    security_mem_classification_t classification
)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    BRIDGE_LOCK(bridge);

    security_mem_internal_t* internal = (security_mem_internal_t*)bridge->base.system_b;
    if (!internal) {
        BRIDGE_UNLOCK(bridge);
        NIMCP_CHECK_THROW(internal, NIMCP_ERROR_INVALID_STATE, "internal state is NULL");
    }

    uint32_t key_idx = (uint32_t)classification % SEC_MEM_MAX_KEYS;

    /* Generate new key (stub - in production use CSPRNG) */
    uint64_t timestamp = get_timestamp_us();
    for (uint32_t j = 0; j < SEC_MEM_AES256_KEY_SIZE; j++) {
        internal->keys[key_idx][j] = (uint8_t)(timestamp ^ (timestamp >> (j + 1) * 8));
        timestamp = timestamp * 6364136223846793005ULL + 1442695040888963407ULL;  /* LCG */
    }
    internal->key_timestamps[key_idx] = get_timestamp_us();

    BRIDGE_UNLOCK(bridge);

    NIMCP_LOGGING_INFO("Rotated key for classification %s",
                      security_memory_classification_name(classification));
    return 0;
}

/* ============================================================================
 * Secure Erase Functions
 * ============================================================================ */

int security_memory_secure_erase(
    security_mem_bridge_t* bridge,
    security_mem_system_type_t memory_type,
    uint64_t region_id,
    bool verify
)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    BRIDGE_LOCK(bridge);

    bridge->state.state = SEC_MEM_STATE_ERASING;

    /* In a full implementation, this would:
     * 1. Look up region pointer from region_id
     * 2. Perform secure erase on that memory
     * 3. Remove from any indexes/registries
     */
    (void)memory_type;
    (void)region_id;
    (void)verify;

    bridge->stats.secure_erases++;
    bridge->state.state = SEC_MEM_STATE_IDLE;

    BRIDGE_UNLOCK(bridge);

    return 0;
}

int security_memory_secure_erase_ptr(
    security_mem_bridge_t* bridge,
    void* data_ptr,
    size_t data_size
)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(data_ptr, NIMCP_ERROR_NULL_POINTER, "data_ptr is NULL");
    NIMCP_CHECK_THROW(data_size > 0, NIMCP_ERROR_INVALID_PARAMETER, "data_size is zero");

    BRIDGE_LOCK(bridge);

    bridge->state.state = SEC_MEM_STATE_ERASING;

    secure_erase_memory(data_ptr, data_size, bridge->config.erase_passes);

    /* Verify if configured */
    if (bridge->config.verify_erase) {
        const uint8_t* bytes = (const uint8_t*)data_ptr;
        for (size_t i = 0; i < data_size; i++) {
            if (bytes[i] != 0) {
                NIMCP_LOGGING_ERROR("Secure erase verification failed at offset %zu", i);
                bridge->state.state = SEC_MEM_STATE_ERROR;
                BRIDGE_UNLOCK(bridge);
                return NIMCP_ERROR_OPERATION_FAILED;
            }
        }
    }

    bridge->stats.secure_erases++;
    bridge->stats.bytes_erased += data_size;
    bridge->state.state = SEC_MEM_STATE_IDLE;

    BRIDGE_UNLOCK(bridge);

    return 0;
}

int security_memory_secure_erase_classification(
    security_mem_bridge_t* bridge,
    security_mem_classification_t classification
)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    /* In a full implementation, iterate through all memory regions
     * with the specified classification and erase them */
    (void)classification;

    BRIDGE_LOCK(bridge);
    int erased_count = 0;  /* Would count actual erasures */
    BRIDGE_UNLOCK(bridge);

    NIMCP_LOGGING_INFO("Erased %d regions of classification %s",
                      erased_count, security_memory_classification_name(classification));
    return erased_count;
}

/* ============================================================================
 * Leakage Detection Functions
 * ============================================================================ */

bool security_memory_detect_leakage(
    security_mem_bridge_t* bridge,
    security_mem_leakage_t* leakage_out,
    float* confidence_out,
    char* details_out,
    size_t details_size
)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "security_memory_detect_leakage: bridge is NULL");
        return false;
    }

    BRIDGE_LOCK(bridge);

    bridge->state.state = SEC_MEM_STATE_DETECTING;
    bridge->stats.leak_checks++;

    security_mem_internal_t* internal = (security_mem_internal_t*)bridge->base.system_b;

    security_mem_leakage_t leakage = SEC_MEM_LEAK_NONE;
    float confidence = 0.0f;

    if (internal && internal->history_count >= bridge->config.pattern_window_size) {
        /* Analyze access patterns for anomalies */

        /* Count unique subjects in recent window */
        uint32_t unique_subjects[SEC_MEM_PATTERN_HISTORY];
        uint32_t unique_count = 0;

        for (uint32_t i = 0; i < internal->history_count; i++) {
            uint32_t subj = internal->access_history[i].subject_id;
            bool found = false;
            for (uint32_t j = 0; j < unique_count; j++) {
                if (unique_subjects[j] == subj) {
                    found = true;
                    break;
                }
            }
            if (!found && unique_count < SEC_MEM_PATTERN_HISTORY) {
                unique_subjects[unique_count++] = subj;
            }
        }

        /* Anomaly: too many unique accessors in short time */
        if (unique_count > bridge->config.pattern_window_size / 2) {
            leakage = SEC_MEM_LEAK_UNAUTHORIZED;
            confidence = (float)unique_count / bridge->config.pattern_window_size;
            if (confidence > 1.0f) confidence = 1.0f;
        }

        /* Check for rapid access patterns (timing side channel) */
        uint64_t total_interval = 0;
        uint32_t interval_count = 0;
        for (uint32_t i = 1; i < internal->history_count; i++) {
            uint64_t t1 = internal->access_history[i - 1].timestamp;
            uint64_t t2 = internal->access_history[i].timestamp;
            if (t2 > t1) {
                total_interval += t2 - t1;
                interval_count++;
            }
        }

        if (interval_count > 0) {
            float avg_interval_us = (float)total_interval / interval_count;
            if (avg_interval_us < 100.0f) {  /* Less than 100us between accesses */
                leakage = SEC_MEM_LEAK_TIMING;
                confidence = (100.0f - avg_interval_us) / 100.0f;
            }
        }
    }

    /* Update effects */
    bridge->memory_effects.leakage_type = leakage;
    bridge->memory_effects.leakage_confidence = confidence;
    if (leakage != SEC_MEM_LEAK_NONE) {
        bridge->memory_effects.suspected_leak_time = get_timestamp_us();
        bridge->stats.leaks_detected++;
    }

    bridge->state.state = SEC_MEM_STATE_IDLE;
    bridge->state.last_leak_check = get_timestamp_us();

    BRIDGE_UNLOCK(bridge);

    /* Output results */
    if (leakage_out) {
        *leakage_out = leakage;
    }
    if (confidence_out) {
        *confidence_out = confidence;
    }
    if (details_out && details_size > 0) {
        if (leakage != SEC_MEM_LEAK_NONE) {
            snprintf(details_out, details_size, "Detected %s with %.1f%% confidence",
                    security_memory_leakage_name(leakage), confidence * 100.0f);
        } else {
            details_out[0] = '\0';
        }
    }

    return leakage != SEC_MEM_LEAK_NONE && confidence >= bridge->config.leakage_threshold;
}

int security_memory_whitelist_transfer(
    security_mem_bridge_t* bridge,
    security_mem_system_type_t source_type,
    security_mem_system_type_t dest_type,
    uint32_t subject_id
)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    BRIDGE_LOCK(bridge);

    security_mem_internal_t* internal = (security_mem_internal_t*)bridge->base.system_b;
    if (!internal) {
        BRIDGE_UNLOCK(bridge);
        NIMCP_CHECK_THROW(internal, NIMCP_ERROR_INVALID_STATE, "internal state is NULL");
    }
    if (internal->whitelist_count >= MAX_WHITELIST_ENTRIES) {
        BRIDGE_UNLOCK(bridge);
        NIMCP_CHECK_THROW(internal->whitelist_count < MAX_WHITELIST_ENTRIES,
                         NIMCP_ERROR_OUT_OF_RANGE, "whitelist is full");
    }

    transfer_whitelist_entry_t* entry = &internal->whitelist[internal->whitelist_count++];
    entry->source = source_type;
    entry->dest = dest_type;
    entry->subject_id = subject_id;
    entry->valid_until = get_timestamp_us() + 3600000000ULL;  /* 1 hour */

    BRIDGE_UNLOCK(bridge);

    return 0;
}

/* ============================================================================
 * Audit Functions
 * ============================================================================ */

int security_memory_audit_access(
    security_mem_bridge_t* bridge,
    uint32_t subject_id,
    security_mem_system_type_t memory_type,
    security_mem_operation_t operation,
    security_mem_classification_t data_class,
    bool success,
    const char* details
)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(bridge->config.enable_audit && bridge->audit_log,
                     NIMCP_ERROR_INVALID_STATE, "audit not enabled or audit_log is NULL");

    BRIDGE_LOCK(bridge);

    bridge->state.state = SEC_MEM_STATE_AUDITING;

    /* Get next entry slot (circular buffer) */
    security_mem_audit_entry_t* entry = &bridge->audit_log[bridge->audit_log_head];

    entry->timestamp = get_timestamp_us();
    entry->type = success ? SEC_MEM_AUDIT_ACCESS : SEC_MEM_AUDIT_DENIED;
    entry->subject_id = subject_id;
    entry->memory_type = memory_type;
    entry->operation = operation;
    entry->data_class = data_class;
    entry->success = success;

    if (details) {
        strncpy(entry->details, details, sizeof(entry->details) - 1);
        entry->details[sizeof(entry->details) - 1] = '\0';
    } else {
        entry->details[0] = '\0';
    }

    /* Advance circular buffer */
    bridge->audit_log_head = (bridge->audit_log_head + 1) % SEC_MEM_MAX_AUDIT_ENTRIES;
    if (bridge->audit_log_count < SEC_MEM_MAX_AUDIT_ENTRIES) {
        bridge->audit_log_count++;
    }

    bridge->stats.audit_entries++;
    if (!success) {
        bridge->stats.audit_alerts++;
    }

    bridge->state.state = SEC_MEM_STATE_IDLE;
    bridge->state.last_audit = get_timestamp_us();

    BRIDGE_UNLOCK(bridge);

    return 0;
}

int security_memory_get_audit_log(
    const security_mem_bridge_t* bridge,
    security_mem_audit_entry_t* entries_out,
    size_t max_entries,
    size_t* count_out
)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(entries_out, NIMCP_ERROR_NULL_POINTER, "entries_out is NULL");
    NIMCP_CHECK_THROW(count_out, NIMCP_ERROR_NULL_POINTER, "count_out is NULL");
    NIMCP_CHECK_THROW(bridge->audit_log, NIMCP_ERROR_INVALID_STATE, "audit_log is NULL");

    size_t to_copy = (bridge->audit_log_count < max_entries) ?
                     bridge->audit_log_count : max_entries;

    /* Copy from circular buffer, newest first */
    for (size_t i = 0; i < to_copy; i++) {
        uint32_t idx = (bridge->audit_log_head + SEC_MEM_MAX_AUDIT_ENTRIES - 1 - i) %
                       SEC_MEM_MAX_AUDIT_ENTRIES;
        entries_out[i] = bridge->audit_log[idx];
    }

    *count_out = to_copy;
    return 0;
}

int security_memory_clear_audit_log(security_mem_bridge_t* bridge)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    BRIDGE_LOCK(bridge);

    /* Log the clear operation first */
    if (bridge->audit_log && bridge->config.enable_audit) {
        security_mem_audit_entry_t* entry = &bridge->audit_log[bridge->audit_log_head];
        entry->timestamp = get_timestamp_us();
        entry->type = SEC_MEM_AUDIT_ERASE;
        entry->subject_id = 0;  /* System action */
        entry->memory_type = SEC_MEM_TYPE_WORKING;
        entry->operation = SEC_MEM_OP_DELETE;
        entry->data_class = SEC_MEM_CLASS_PUBLIC;
        entry->success = true;
        strncpy(entry->details, "Audit log cleared", sizeof(entry->details) - 1);
    }

    bridge->audit_log_head = 0;
    bridge->audit_log_count = 0;

    BRIDGE_UNLOCK(bridge);

    NIMCP_LOGGING_INFO("Cleared audit log");
    return 0;
}

int security_memory_export_audit_log(
    const security_mem_bridge_t* bridge,
    const char* filepath
)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(filepath, NIMCP_ERROR_NULL_POINTER, "filepath is NULL");
    NIMCP_CHECK_THROW(bridge->audit_log, NIMCP_ERROR_INVALID_STATE, "audit_log is NULL");

    FILE* fp = fopen(filepath, "w");
    NIMCP_CHECK_THROW(fp, NIMCP_ERROR_FILE_OPEN, "failed to open %s for writing", filepath);

    fprintf(fp, "timestamp,type,subject_id,memory_type,operation,classification,success,details\n");

    for (uint32_t i = 0; i < bridge->audit_log_count; i++) {
        uint32_t idx = (bridge->audit_log_head + SEC_MEM_MAX_AUDIT_ENTRIES - bridge->audit_log_count + i) %
                       SEC_MEM_MAX_AUDIT_ENTRIES;
        const security_mem_audit_entry_t* entry = &bridge->audit_log[idx];

        fprintf(fp, "%lu,%s,%u,%s,%s,%s,%s,\"%s\"\n",
               (unsigned long)entry->timestamp,
               security_memory_audit_type_name(entry->type),
               entry->subject_id,
               security_memory_system_name(entry->memory_type),
               security_memory_operation_name(entry->operation),
               security_memory_classification_name(entry->data_class),
               entry->success ? "true" : "false",
               entry->details);
    }

    fclose(fp);
    NIMCP_LOGGING_INFO("Exported %u audit entries to %s", bridge->audit_log_count, filepath);
    return 0;
}

/* ============================================================================
 * Bidirectional Update Functions
 * ============================================================================ */

int security_memory_bridge_update(
    security_mem_bridge_t* bridge,
    uint64_t delta_ms
)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    BRIDGE_LOCK(bridge);

    /* Record update */
    bridge_base_record_update(&bridge->base);

    /* Gather memory effects */
    security_memory_gather_memory_effects(bridge);

    /* Apply security effects */
    security_memory_apply_security_effects(bridge);

    /* Periodic leakage detection */
    if (bridge->config.enable_leakage_detection) {
        uint64_t now = get_timestamp_us();
        if (now - bridge->state.last_leak_check > (uint64_t)SEC_MEM_LEAKAGE_WINDOW_MS * 1000) {
            security_mem_leakage_t leakage;
            float confidence;
            BRIDGE_UNLOCK(bridge);
            security_memory_detect_leakage(bridge, &leakage, &confidence, NULL, 0);
            BRIDGE_LOCK(bridge);
        }
    }

    /* Update security effects based on time */
    bridge->security_effects.security_latency_ms =
        (float)delta_ms * bridge->config.security_sensitivity;

    BRIDGE_UNLOCK(bridge);

    return 0;
}

int security_memory_apply_security_effects(security_mem_bridge_t* bridge)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    /* Update effective access mask based on lockouts and configuration */
    bridge->security_effects.current_access_mask = bridge->config.default_access_mask;

    /* Update encryption requirements */
    bridge->security_effects.encryption_required =
        bridge->config.enable_encryption && bridge->config.encrypt_at_rest;

    bridge->security_effects.effective_min_class =
        (security_mem_classification_t)bridge->config.min_encryption_class;

    /* Calculate throughput reduction from security overhead */
    float overhead = 0.0f;
    if (bridge->config.enable_access_control) overhead += 0.02f;
    if (bridge->config.enable_encryption) overhead += 0.05f;
    if (bridge->config.enable_audit) overhead += 0.01f;
    if (bridge->config.enable_leakage_detection) overhead += 0.02f;

    bridge->security_effects.throughput_reduction = overhead * bridge->config.security_sensitivity;
    if (bridge->security_effects.throughput_reduction > 0.5f) {
        bridge->security_effects.throughput_reduction = 0.5f;
    }

    return 0;
}

int security_memory_gather_memory_effects(security_mem_bridge_t* bridge)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    security_mem_internal_t* internal = (security_mem_internal_t*)bridge->base.system_b;
    NIMCP_CHECK_THROW(internal, NIMCP_ERROR_INVALID_STATE, "internal state is NULL");

    /* Calculate access frequency from history */
    if (internal->history_count > 1) {
        uint64_t first_ts = internal->access_history[0].timestamp;
        uint64_t last_ts = internal->access_history[(internal->history_head + SEC_MEM_PATTERN_HISTORY - 1) %
                                                    SEC_MEM_PATTERN_HISTORY].timestamp;
        if (last_ts > first_ts) {
            float duration_sec = (float)(last_ts - first_ts) / 1000000.0f;
            if (duration_sec > 0.0f) {
                bridge->memory_effects.access_frequency = (float)internal->history_count / duration_sec;
            }
        }
    }

    /* Calculate read/write ratio */
    uint32_t reads = 0, writes = 0;
    for (uint32_t i = 0; i < internal->history_count; i++) {
        if (internal->access_history[i].operation == SEC_MEM_OP_READ ||
            internal->access_history[i].operation == SEC_MEM_OP_RETRIEVE) {
            reads++;
        } else if (internal->access_history[i].operation == SEC_MEM_OP_WRITE ||
                   internal->access_history[i].operation == SEC_MEM_OP_ENCODE) {
            writes++;
        }
    }
    if (writes > 0) {
        bridge->memory_effects.read_write_ratio = (float)reads / (float)writes;
    } else {
        bridge->memory_effects.read_write_ratio = (float)reads;
    }

    /* Count unique accessors */
    uint32_t unique_subjects[SEC_MEM_PATTERN_HISTORY];
    uint32_t unique_count = 0;
    for (uint32_t i = 0; i < internal->history_count; i++) {
        uint32_t subj = internal->access_history[i].subject_id;
        bool found = false;
        for (uint32_t j = 0; j < unique_count; j++) {
            if (unique_subjects[j] == subj) {
                found = true;
                break;
            }
        }
        if (!found && unique_count < SEC_MEM_PATTERN_HISTORY) {
            unique_subjects[unique_count++] = subj;
        }
    }
    bridge->memory_effects.unique_accessors = unique_count;

    /* Pattern regularity (simplified) */
    bridge->memory_effects.pattern_regularity = 1.0f - (float)unique_count / SEC_MEM_PATTERN_HISTORY;
    if (bridge->memory_effects.pattern_regularity < 0.0f) {
        bridge->memory_effects.pattern_regularity = 0.0f;
    }

    return 0;
}

/* ============================================================================
 * Query Functions
 * ============================================================================ */

int security_memory_get_security_effects(
    const security_mem_bridge_t* bridge,
    security_to_memory_effects_t* effects_out
)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(effects_out, NIMCP_ERROR_NULL_POINTER, "effects_out is NULL");

    *effects_out = bridge->security_effects;
    return 0;
}

int security_memory_get_memory_effects(
    const security_mem_bridge_t* bridge,
    memory_to_security_effects_t* effects_out
)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(effects_out, NIMCP_ERROR_NULL_POINTER, "effects_out is NULL");

    *effects_out = bridge->memory_effects;
    return 0;
}

int security_memory_get_state(
    const security_mem_bridge_t* bridge,
    security_mem_state_info_t* state_out
)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(state_out, NIMCP_ERROR_NULL_POINTER, "state_out is NULL");

    *state_out = bridge->state;
    return 0;
}

int security_memory_get_stats(
    const security_mem_bridge_t* bridge,
    security_mem_stats_t* stats_out
)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(stats_out, NIMCP_ERROR_NULL_POINTER, "stats_out is NULL");

    *stats_out = bridge->stats;
    return 0;
}

int security_memory_reset_stats(security_mem_bridge_t* bridge)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    BRIDGE_LOCK(bridge);
    memset(&bridge->stats, 0, sizeof(security_mem_stats_t));
    BRIDGE_UNLOCK(bridge);

    return 0;
}

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

int security_memory_connect_bio_async(security_mem_bridge_t* bridge)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    return bridge_base_connect_bio_async(&bridge->base);
}

int security_memory_disconnect_bio_async(security_mem_bridge_t* bridge)
{
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    return bridge_base_disconnect_bio_async(&bridge->base);
}

bool security_memory_is_bio_async_connected(const security_mem_bridge_t* bridge)
{
    if (!bridge) {
        return false;
    }

    return bridge_base_is_bio_async_connected(&bridge->base);
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

const char* security_memory_classification_name(security_mem_classification_t classification)
{
    switch (classification) {
        case SEC_MEM_CLASS_PUBLIC:       return "PUBLIC";
        case SEC_MEM_CLASS_INTERNAL:     return "INTERNAL";
        case SEC_MEM_CLASS_CONFIDENTIAL: return "CONFIDENTIAL";
        case SEC_MEM_CLASS_SECRET:       return "SECRET";
        case SEC_MEM_CLASS_TOP_SECRET:   return "TOP_SECRET";
        default:                         return "UNKNOWN";
    }
}

const char* security_memory_operation_name(security_mem_operation_t operation)
{
    switch (operation) {
        case SEC_MEM_OP_READ:        return "READ";
        case SEC_MEM_OP_WRITE:       return "WRITE";
        case SEC_MEM_OP_DELETE:      return "DELETE";
        case SEC_MEM_OP_SHARE:       return "SHARE";
        case SEC_MEM_OP_CONSOLIDATE: return "CONSOLIDATE";
        case SEC_MEM_OP_RETRIEVE:    return "RETRIEVE";
        case SEC_MEM_OP_ENCODE:      return "ENCODE";
        default:                     return "UNKNOWN";
    }
}

const char* security_memory_system_name(security_mem_system_type_t mem_type)
{
    switch (mem_type) {
        case SEC_MEM_TYPE_WORKING:    return "WORKING";
        case SEC_MEM_TYPE_EPISODIC:   return "EPISODIC";
        case SEC_MEM_TYPE_SEMANTIC:   return "SEMANTIC";
        case SEC_MEM_TYPE_PROCEDURAL: return "PROCEDURAL";
        default:                      return "UNKNOWN";
    }
}

const char* security_memory_state_name(security_mem_state_t state)
{
    switch (state) {
        case SEC_MEM_STATE_IDLE:       return "IDLE";
        case SEC_MEM_STATE_CHECKING:   return "CHECKING";
        case SEC_MEM_STATE_ENCRYPTING: return "ENCRYPTING";
        case SEC_MEM_STATE_DECRYPTING: return "DECRYPTING";
        case SEC_MEM_STATE_ERASING:    return "ERASING";
        case SEC_MEM_STATE_AUDITING:   return "AUDITING";
        case SEC_MEM_STATE_DETECTING:  return "DETECTING";
        case SEC_MEM_STATE_ERROR:      return "ERROR";
        default:                       return "UNKNOWN";
    }
}

const char* security_memory_leakage_name(security_mem_leakage_t leakage)
{
    switch (leakage) {
        case SEC_MEM_LEAK_NONE:           return "NONE";
        case SEC_MEM_LEAK_CLASSIFICATION: return "CLASSIFICATION_DOWNGRADE";
        case SEC_MEM_LEAK_UNAUTHORIZED:   return "UNAUTHORIZED_ACCESS";
        case SEC_MEM_LEAK_TIMING:         return "TIMING_CHANNEL";
        case SEC_MEM_LEAK_COVERT:         return "COVERT_CHANNEL";
        case SEC_MEM_LEAK_CROSS_MEMORY:   return "CROSS_MEMORY_TRANSFER";
        default:                          return "UNKNOWN";
    }
}

const char* security_memory_audit_type_name(security_mem_audit_type_t audit_type)
{
    switch (audit_type) {
        case SEC_MEM_AUDIT_ACCESS:      return "ACCESS";
        case SEC_MEM_AUDIT_DENIED:      return "DENIED";
        case SEC_MEM_AUDIT_ENCRYPT:     return "ENCRYPT";
        case SEC_MEM_AUDIT_DECRYPT:     return "DECRYPT";
        case SEC_MEM_AUDIT_ERASE:       return "ERASE";
        case SEC_MEM_AUDIT_CLASSIFY:    return "CLASSIFY";
        case SEC_MEM_AUDIT_LEAK_DETECT: return "LEAK_DETECT";
        case SEC_MEM_AUDIT_ANOMALY:     return "ANOMALY";
        default:                        return "UNKNOWN";
    }
}

void security_memory_print_summary(const security_mem_bridge_t* bridge)
{
    if (!bridge) {
        printf("Security-Memory Bridge: NULL\n");
        return;
    }

    printf("\n=== Security-Memory Bridge Summary ===\n");
    printf("State: %s\n", security_memory_state_name(bridge->state.state));
    printf("Connections:\n");
    printf("  Working Memory:   %s\n", bridge->working_connected ? "Connected" : "Disconnected");
    printf("  Episodic Memory:  %s\n", bridge->episodic_connected ? "Connected" : "Disconnected");
    printf("  Semantic Memory:  %s\n", bridge->semantic_connected ? "Connected" : "Disconnected");
    printf("  Procedural Memory: %s\n", bridge->procedural_connected ? "Connected" : "Disconnected");
    printf("  BBB:              %s\n", bridge->bbb_connected ? "Connected" : "Disconnected");
    printf("  Bio-Async:        %s\n", bridge->base.bio_async_enabled ? "Connected" : "Disconnected");
    printf("Registered Subjects: %u\n", bridge->num_subjects);
    printf("Audit Log Entries:   %u\n", bridge->audit_log_count);
    printf("\n");
}

void security_memory_print_stats(const security_mem_stats_t* stats)
{
    if (!stats) {
        printf("Statistics: NULL\n");
        return;
    }

    printf("\n=== Security-Memory Bridge Statistics ===\n");
    printf("Access Control:\n");
    printf("  Total Checks:    %lu\n", (unsigned long)stats->total_access_checks);
    printf("  Granted:         %lu\n", (unsigned long)stats->access_granted);
    printf("  Denied:          %lu\n", (unsigned long)stats->access_denied);
    printf("  Mean Latency:    %.2f us\n", stats->mean_check_latency_us);
    printf("\nEncryption:\n");
    printf("  Encryptions:     %lu\n", (unsigned long)stats->total_encryptions);
    printf("  Decryptions:     %lu\n", (unsigned long)stats->total_decryptions);
    printf("  Bytes Encrypted: %lu\n", (unsigned long)stats->bytes_encrypted);
    printf("\nSecure Erase:\n");
    printf("  Erase Operations: %lu\n", (unsigned long)stats->secure_erases);
    printf("  Bytes Erased:     %lu\n", (unsigned long)stats->bytes_erased);
    printf("\nLeakage Detection:\n");
    printf("  Checks:           %lu\n", (unsigned long)stats->leak_checks);
    printf("  Leaks Detected:   %lu\n", (unsigned long)stats->leaks_detected);
    printf("\nPer-System Operations:\n");
    printf("  Working Memory:   %lu\n", (unsigned long)stats->working_memory_ops);
    printf("  Episodic Memory:  %lu\n", (unsigned long)stats->episodic_memory_ops);
    printf("  Semantic Memory:  %lu\n", (unsigned long)stats->semantic_memory_ops);
    printf("  Procedural Memory: %lu\n", (unsigned long)stats->procedural_memory_ops);
    printf("\n");
}
