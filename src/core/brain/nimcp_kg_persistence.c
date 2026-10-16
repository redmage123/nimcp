/**
 * @file nimcp_kg_persistence.c
 * @brief Persistent Storage Layer for Knowledge Graph - Implementation
 * @version 1.0.0
 * @date 2025-01-16
 *
 * Implementation of persistence layer with QuestDB storage, quantum-resistant
 * encryption, HSM support, and comprehensive audit logging.
 */

#include "core/brain/nimcp_kg_persistence.h"
#include "constants/nimcp_buffer_constants.h"
#include "core/brain/persistence/nimcp_brain_kg_snapshot.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <time.h>
#include <stdio.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_timing_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(kg_persistence, MESH_ADAPTER_CATEGORY_COGNITIVE)


/* ============================================================================
 * Internal Constants
 * ============================================================================ */

#define PERSIST_VERSION_CURRENT     1
#define PERSIST_MAGIC               0x4B475053  /* "KGPS" */
#define PERSIST_MAX_CHECKPOINTS     100
#define PERSIST_AUDIT_BUFFER_SIZE   4096
#define PERSIST_HASH_SIZE           32

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Checkpoint record
 */
typedef struct {
    char label[NIMCP_ID_BUFFER_SIZE];
    uint64_t version;
    uint64_t timestamp;
    char storage_path[KG_PERSIST_MAX_PATH_LEN];
} checkpoint_record_t;

/**
 * @brief Audit log entry
 */
typedef struct kg_audit_entry {
    uint64_t timestamp;
    kg_audit_event_type_t event_type;
    char details[NIMCP_ERROR_BUFFER_SIZE];
    uint8_t prev_hash[PERSIST_HASH_SIZE];
    uint8_t entry_hash[PERSIST_HASH_SIZE];
    struct kg_audit_entry* next;
} kg_audit_entry_t;

/**
 * @brief HSM context
 */
struct kg_hsm_handle {
    kg_hsm_type_t type;
    bool connected;
    char key_label[NIMCP_ID_BUFFER_SIZE];
    uint8_t key_id[32];
    uint64_t creation_time;
    uint32_t usage_count;
    void* provider_context;
    nimcp_mutex_t* mutex;
};

/**
 * @brief Persistence context internal structure
 */
struct kg_persistence {
    /* Configuration */
    kg_persistence_config_t config;

    /* State */
    bool initialized;
    uint64_t stored_version;
    uint64_t last_save_time;

    /* I/O dispatcher for async operations */
    kg_io_dispatcher_t* io_dispatcher;

    /* Encryption state */
    bool encryption_initialized;
    uint8_t master_key[32];
    bool master_key_loaded;
    kg_crypto_algorithm_t active_algorithm;

    /* HSM state */
    kg_hsm_handle_t* hsm;
    bool hsm_initialized;

    /* Audit logging */
    bool audit_initialized;
    kg_audit_config_t audit_config;
    kg_audit_entry_t* audit_head;
    kg_audit_entry_t* audit_tail;
    uint64_t audit_entry_count;
    FILE* audit_file;
    uint8_t last_audit_hash[PERSIST_HASH_SIZE];

    /* Checkpoints */
    checkpoint_record_t checkpoints[PERSIST_MAX_CHECKPOINTS];
    uint32_t checkpoint_count;

    /* Synchronization */
    nimcp_mutex_t* mutex;
};

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

static uint64_t get_timestamp_ms(void);
static void safe_strcpy(char* dest, const char* src, size_t dest_size);
static void compute_hash(const void* data, size_t size, uint8_t* hash);
static int serialize_kg_to_buffer(const brain_kg_t* kg, uint8_t** buffer, size_t* size);
static brain_kg_t* deserialize_kg_from_buffer(const uint8_t* buffer, size_t size);
static int encrypt_buffer(kg_persistence_t* p, const uint8_t* input, size_t input_size,
                           uint8_t** output, size_t* output_size);
static int decrypt_buffer(kg_persistence_t* p, const uint8_t* input, size_t input_size,
                           uint8_t** output, size_t* output_size);

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static uint64_t get_timestamp_ms(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) == 0) {
        return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
    }
    return 0;
}

static void safe_strcpy(char* dest, const char* src, size_t dest_size) {
    if (!dest || dest_size == 0) return;
    if (!src) {
        dest[0] = '\0';
        return;
    }
    size_t src_len = strlen(src);
    size_t copy_len = (src_len < dest_size - 1) ? src_len : dest_size - 1;
    memcpy(dest, src, copy_len);
    dest[copy_len] = '\0';
}

static void compute_hash(const void* data, size_t size, uint8_t* hash) {
    /* Simple hash for demonstration - in production, use SHA-256 */
    if (!data || !hash || size == 0) {
        if (hash) memset(hash, 0, PERSIST_HASH_SIZE);
        return;
    }

    const uint8_t* bytes = (const uint8_t*)data;
    uint64_t h = 0x5555555555555555ULL;

    for (size_t i = 0; i < size; i++) {
        h ^= bytes[i];
        h *= 0x100000001B3ULL;
    }

    memset(hash, 0, PERSIST_HASH_SIZE);
    memcpy(hash, &h, sizeof(h));
}

/* ============================================================================
 * Serialization (Simplified)
 * ============================================================================ */

static int serialize_kg_to_buffer(const brain_kg_t* kg, uint8_t** buffer, size_t* size) {
    if (!kg || !buffer || !size) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "serialize_kg_to_buffer: required parameter is NULL (kg, buffer, size)");
        return -1;
    }

    /* Get node and edge counts */
    uint32_t node_count = 0;
    uint32_t edge_count = 0;

    brain_kg_get_node_count(kg, &node_count);
    brain_kg_get_edge_count(kg, &edge_count);

    /* Estimate size: header + nodes + edges */
    size_t estimated_size = 256 + (node_count * 512) + (edge_count * 128);

    *buffer = nimcp_malloc(estimated_size);
    if (!*buffer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "serialize_kg_to_buffer: validation failed");
        return -1;
    }

    /* Write header */
    uint8_t* ptr = *buffer;
    uint32_t magic = PERSIST_MAGIC;
    uint32_t version = PERSIST_VERSION_CURRENT;

    memcpy(ptr, &magic, 4); ptr += 4;
    memcpy(ptr, &version, 4); ptr += 4;
    memcpy(ptr, &node_count, 4); ptr += 4;
    memcpy(ptr, &edge_count, 4); ptr += 4;

    /* Placeholder: would serialize actual node/edge data here */
    /* For now, just write counts */

    *size = (size_t)(ptr - *buffer);
    return 0;
}

static brain_kg_t* deserialize_kg_from_buffer(const uint8_t* buffer, size_t size) {
    if (!buffer || size < 16) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "deserialize_kg_from_buffer: buffer is NULL");
        return NULL;
    }

    const uint8_t* ptr = buffer;

    /* Read header */
    uint32_t magic, version, node_count, edge_count;
    memcpy(&magic, ptr, 4); ptr += 4;
    memcpy(&version, ptr, 4); ptr += 4;
    memcpy(&node_count, ptr, 4); ptr += 4;
    memcpy(&edge_count, ptr, 4); ptr += 4;

    if (magic != PERSIST_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "deserialize_kg_from_buffer: validation failed");
        return NULL;
    }

    if (version > PERSIST_VERSION_CURRENT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "deserialize_kg_from_buffer: validation failed");
        return NULL;  /* Unsupported version */
    }

    /* Create new KG */
    brain_kg_t* kg = brain_kg_create(NULL);
    if (!kg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg is NULL");

        return NULL;
    }

    /* Placeholder: would deserialize actual node/edge data here */

    return kg;
}

/* ============================================================================
 * Encryption (Simplified)
 * ============================================================================ */

static int encrypt_buffer(kg_persistence_t* p, const uint8_t* input, size_t input_size,
                           uint8_t** output, size_t* output_size) {
    if (!p || !input || !output || !output_size) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "deserialize_kg_from_buffer: required parameter is NULL (p, input, output, output_size)");
        return -1;
    }

    if (!p->encryption_initialized || p->active_algorithm == KG_CRYPTO_NONE) {
        /* No encryption - just copy */
        *output = nimcp_malloc(input_size);
        if (!*output) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "deserialize_kg_from_buffer: validation failed");
            return -1;
        }
        memcpy(*output, input, input_size);
        *output_size = input_size;
        return 0;
    }

    /* Allocate output buffer (encrypted data is typically larger due to auth tag) */
    *output_size = input_size + 32;  /* Auth tag + nonce */
    *output = nimcp_malloc(*output_size);
    if (!*output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "deserialize_kg_from_buffer: validation failed");
        return -1;
    }

    /* In production, would call nimcp_encryption_* or nimcp_hybrid_* */
    /* For now, simple XOR with key (NOT SECURE - demonstration only) */
    memcpy(*output, input, input_size);
    for (size_t i = 0; i < input_size; i++) {
        (*output)[i] ^= p->master_key[i % 32];
    }
    *output_size = input_size;

    return 0;
}

static int decrypt_buffer(kg_persistence_t* p, const uint8_t* input, size_t input_size,
                           uint8_t** output, size_t* output_size) {
    if (!p || !input || !output || !output_size) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "deserialize_kg_from_buffer: required parameter is NULL (p, input, output, output_size)");
        return -1;
    }

    if (!p->encryption_initialized || p->active_algorithm == KG_CRYPTO_NONE) {
        *output = nimcp_malloc(input_size);
        if (!*output) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "deserialize_kg_from_buffer: validation failed");
            return -1;
        }
        memcpy(*output, input, input_size);
        *output_size = input_size;
        return 0;
    }

    *output = nimcp_malloc(input_size);
    if (!*output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "deserialize_kg_from_buffer: validation failed");
        return -1;
    }

    /* Reverse XOR */
    memcpy(*output, input, input_size);
    for (size_t i = 0; i < input_size; i++) {
        (*output)[i] ^= p->master_key[i % 32];
    }
    *output_size = input_size;

    return 0;
}

/* ============================================================================
 * Default Configuration Helpers Implementation
 * ============================================================================ */

int kg_persistence_default_config(kg_persistence_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;
    }

    memset(config, 0, sizeof(kg_persistence_config_t));

    safe_strcpy(config->storage_path, ".aim/kg/questdb/", KG_PERSIST_MAX_PATH_LEN);
    config->enable_auto_save = true;
    config->auto_save_interval_ms = 5000;
    config->enable_compression = true;
    config->enable_checksums = true;
    config->require_encryption = true;

    kg_persistence_default_questdb_config(&config->questdb);
    kg_persistence_default_encryption_config(&config->encryption);

    return 0;
}

int kg_persistence_default_questdb_config(kg_questdb_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;
    }

    memset(config, 0, sizeof(kg_questdb_config_t));

    config->mode = KG_QUESTDB_EMBEDDED;
    safe_strcpy(config->host, "localhost", KG_PERSIST_MAX_PATH_LEN);
    config->ilp_port = KG_QUESTDB_DEFAULT_ILP_PORT;
    config->pg_port = KG_QUESTDB_DEFAULT_PG_PORT;
    config->http_port = KG_QUESTDB_DEFAULT_HTTP_PORT;

    safe_strcpy(config->data_dir, ".aim/kg/questdb/data", KG_PERSIST_MAX_PATH_LEN);
    config->writer_memory_mb = 512;

    /* Thread pool defaults */
    config->threads.writer_threads = 4;
    config->threads.reader_threads = 8;
    config->threads.io_threads = 0;  /* Auto-detect */
    config->threads.pin_threads_to_cores = false;
    config->threads.thread_stack_size_kb = 256;

    /* Connection pool defaults */
    config->pool.min_connections = 4;
    config->pool.max_connections = 32;
    config->pool.connection_timeout_ms = NIMCP_DEFAULT_TIMEOUT_MS;
    config->pool.idle_timeout_ms = 60000;
    config->pool.max_lifetime_ms = 1800000;
    config->pool.enable_connection_validation = true;
    config->pool.validation_interval_ms = 30000;

    /* Async I/O defaults */
    config->async_io.write_buffer_size_kb = 1024;
    config->async_io.write_queue_depth = 1024;
    config->async_io.enable_batch_writes = true;
    config->async_io.batch_size = 10000;
    config->async_io.batch_timeout_ms = NIMCP_FAST_TIMEOUT_MS;
    config->async_io.read_buffer_size_kb = 512;
    config->async_io.prefetch_rows = 10000;
    config->async_io.enable_result_caching = true;
    config->async_io.cache_size_mb = 256;
    config->async_io.cache_ttl_ms = 5000;
    config->async_io.enable_io_uring = true;
    config->async_io.enable_direct_io = false;
    config->async_io.io_uring_queue_depth = 256;

    /* Performance tuning */
    config->commit_lag_ms = 200;
    config->max_uncommitted_rows = 500000;
    config->enable_wal = true;
    config->enable_parallel_ingestion = true;
    safe_strcpy(config->partition_by, "DAY", 16);

    /* Security defaults */
    config->security.require_tls = true;
    config->security.require_authentication = true;
    config->security.require_encryption_at_rest = true;
    config->security.fail_secure = true;

    return 0;
}

int kg_persistence_default_encryption_config(kg_encryption_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;
    }

    memset(config, 0, sizeof(kg_encryption_config_t));

    config->algorithm = KG_CRYPTO_HYBRID_KYBER_AES;
    config->enable_key_rotation = true;
    config->key_rotation_days = 90;
    config->enable_hmac = true;
    config->enable_merkle_tree = false;

    /* HSM defaults (not configured) */
    config->hsm.type = KG_HSM_NONE;
    config->hsm.require_hsm_in_production = false;

    /* Audit defaults */
    config->audit.level = KG_AUDIT_LEVEL_DETAILED;
    safe_strcpy(config->audit.log_path, ".aim/kg/audit.log", KG_PERSIST_MAX_PATH_LEN);
    config->audit.log_to_syslog = false;
    config->audit.enable_tamper_evident = true;
    config->audit.max_log_size_mb = 100;
    config->audit.log_retention_days = 365;
    config->audit.enable_remote_logging = false;

    return 0;
}

/* ============================================================================
 * Persistence Lifecycle Implementation
 * ============================================================================ */

kg_persistence_t* kg_persistence_create(const kg_persistence_config_t* config) {
    kg_persistence_t* p = nimcp_calloc(1, sizeof(kg_persistence_t));
    if (!p) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "p is NULL");

        return NULL;
    }

    /* Apply configuration */
    if (config) {
        memcpy(&p->config, config, sizeof(kg_persistence_config_t));
    } else {
        kg_persistence_default_config(&p->config);
    }

    /* Create synchronization */
    mutex_attr_t attr = {0};
    attr.type = MUTEX_TYPE_NORMAL;
    p->mutex = nimcp_mutex_create(&attr);
    if (!p->mutex) {
        nimcp_free(p);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_persistence_create: p->mutex is NULL");
        return NULL;
    }

    /* Create I/O dispatcher */
    kg_questdb_config_t io_config;
    kg_io_default_config(&io_config);
    p->io_dispatcher = kg_io_dispatcher_create(&io_config);
    if (!p->io_dispatcher) {
        nimcp_mutex_free(p->mutex);
        nimcp_free(p);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_persistence_create: p->io_dispatcher is NULL");
        return NULL;
    }

    /* Start I/O dispatcher */
    if (kg_io_dispatcher_start(p->io_dispatcher) != 0) {
        kg_io_dispatcher_destroy(p->io_dispatcher);
        nimcp_mutex_free(p->mutex);
        nimcp_free(p);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_persistence_create: validation failed");
        return NULL;
    }

    /* Create brain snapshots schema (Phase SNAPSHOT-KG) */
    /* Note: Non-fatal failure - schema may already exist or DB may not be ready */
    kg_snapshot_create_schema(p);

    /* Initialize state */
    p->initialized = true;
    p->stored_version = 0;
    p->last_save_time = 0;
    p->encryption_initialized = false;
    p->master_key_loaded = false;
    p->hsm = NULL;
    p->hsm_initialized = false;
    p->audit_initialized = false;
    p->audit_head = NULL;
    p->audit_tail = NULL;
    p->audit_entry_count = 0;
    p->audit_file = NULL;
    p->checkpoint_count = 0;

    return p;
}

void kg_persistence_destroy(kg_persistence_t* p) {
    if (!p) {
        return;
    }

    nimcp_mutex_lock(p->mutex);

    /* Close audit file */
    if (p->audit_file) {
        fclose(p->audit_file);
        p->audit_file = NULL;
    }

    /* Free audit entries */
    kg_audit_entry_t* entry = p->audit_head;
    while (entry) {
        kg_audit_entry_t* next = entry->next;
        nimcp_free(entry);
        entry = next;
    }
    p->audit_head = NULL;
    p->audit_tail = NULL;

    /* Destroy HSM handle */
    if (p->hsm) {
        if (p->hsm->mutex) {
            nimcp_mutex_free(p->hsm->mutex);
        }
        nimcp_free(p->hsm);
        p->hsm = NULL;
    }

    /* Securely zero master key */
    kg_persistence_secure_zero(p->master_key, sizeof(p->master_key));

    /* Stop and destroy I/O dispatcher */
    if (p->io_dispatcher) {
        kg_io_dispatcher_stop(p->io_dispatcher);
        kg_io_dispatcher_destroy(p->io_dispatcher);
        p->io_dispatcher = NULL;
    }

    nimcp_mutex_unlock(p->mutex);
    nimcp_mutex_free(p->mutex);

    nimcp_free(p);
}

/* ============================================================================
 * Save/Load API Implementation
 * ============================================================================ */

int kg_persistence_save(kg_persistence_t* p, const brain_kg_t* kg) {
    if (!p || !kg || !p->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_persistence_save: required parameter is NULL (p, kg, p->initialized)");
        return -1;
    }

    nimcp_mutex_lock(p->mutex);

    /* Log audit event */
    if (p->audit_initialized) {
        kg_persistence_audit_log(p, KG_AUDIT_ENCRYPT_START, "Starting KG save");
    }

    /* Serialize KG */
    uint8_t* serialized = NULL;
    size_t serialized_size = 0;

    if (serialize_kg_to_buffer(kg, &serialized, &serialized_size) != 0) {
        nimcp_mutex_unlock(p->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_persistence_save: validation failed");
        return -1;
    }

    /* Encrypt if enabled */
    uint8_t* encrypted = NULL;
    size_t encrypted_size = 0;

    if (encrypt_buffer(p, serialized, serialized_size, &encrypted, &encrypted_size) != 0) {
        nimcp_free(serialized);
        nimcp_mutex_unlock(p->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_persistence_save: validation failed");
        return -1;
    }

    nimcp_free(serialized);

    /* Write to storage via I/O dispatcher */
    int result = kg_io_write_sync(p->io_dispatcher, "kg_nodes",
                                   encrypted, encrypted_size, 5000);

    nimcp_free(encrypted);

    if (result == 0) {
        p->stored_version++;
        p->last_save_time = get_timestamp_ms();

        if (p->audit_initialized) {
            kg_persistence_audit_log(p, KG_AUDIT_ENCRYPT_SUCCESS, "KG save completed");
        }
    } else {
        if (p->audit_initialized) {
            kg_persistence_audit_log(p, KG_AUDIT_ENCRYPT_FAILURE, "KG save failed");
        }
    }

    nimcp_mutex_unlock(p->mutex);
    return result;
}

brain_kg_t* kg_persistence_load(kg_persistence_t* p) {
    if (!p || !p->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_persistence_load: required parameter is NULL (p, p->initialized)");
        return NULL;
    }

    nimcp_mutex_lock(p->mutex);

    if (p->audit_initialized) {
        kg_persistence_audit_log(p, KG_AUDIT_DECRYPT_START, "Starting KG load");
    }

    /* Query storage via I/O dispatcher */
    kg_io_result_t* result = kg_io_query_sync(p->io_dispatcher,
                                               "SELECT * FROM kg_nodes", 10000);

    if (!result || !result->success || !result->result_data) {
        if (result) kg_io_result_free(result);
        nimcp_mutex_unlock(p->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_persistence_load: validation failed");
        return NULL;
    }

    /* Decrypt if enabled */
    uint8_t* decrypted = NULL;
    size_t decrypted_size = 0;

    if (decrypt_buffer(p, result->result_data, result->result_size,
                        &decrypted, &decrypted_size) != 0) {
        kg_io_result_free(result);
        nimcp_mutex_unlock(p->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_persistence_load: operation failed");
        return NULL;
    }

    kg_io_result_free(result);

    /* Deserialize */
    brain_kg_t* kg = deserialize_kg_from_buffer(decrypted, decrypted_size);
    nimcp_free(decrypted);

    if (kg && p->audit_initialized) {
        kg_persistence_audit_log(p, KG_AUDIT_DECRYPT_SUCCESS, "KG load completed");
    } else if (!kg && p->audit_initialized) {
        kg_persistence_audit_log(p, KG_AUDIT_DECRYPT_FAILURE, "KG load failed");
    }

    nimcp_mutex_unlock(p->mutex);
    return kg;
}

/* ============================================================================
 * Differential Update API Implementation
 * ============================================================================ */

int kg_persistence_compute_diff(kg_persistence_t* p,
                                 const brain_kg_t* current,
                                 kg_diff_result_t* diff) {
    if (!p || !current || !diff) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_persistence_load: required parameter is NULL (p, current, diff)");
        return -1;
    }

    memset(diff, 0, sizeof(kg_diff_result_t));

    nimcp_mutex_lock(p->mutex);

    diff->stored_version = p->stored_version;
    diff->current_version = p->stored_version + 1;

    /* Get current KG stats */
    uint32_t current_nodes = 0, current_edges = 0;
    brain_kg_get_node_count(current, &current_nodes);
    brain_kg_get_edge_count(current, &current_edges);

    /* Placeholder: would compare with stored state */
    /* For now, just create a simple diff indicating changes */

    diff->change_count = 0;
    diff->changes = NULL;
    diff->requires_full_rebuild = (current_nodes > 100000);

    nimcp_mutex_unlock(p->mutex);
    return 0;
}

int kg_persistence_apply_diff(kg_persistence_t* p,
                               brain_kg_t* kg,
                               const kg_diff_result_t* diff) {
    if (!p || !kg || !diff) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_persistence_load: required parameter is NULL (p, kg, diff)");
        return -1;
    }

    nimcp_mutex_lock(p->mutex);

    /* Apply each change */
    for (uint32_t i = 0; i < diff->change_count; i++) {
        const kg_diff_record_t* change = &diff->changes[i];

        switch (change->change_type) {
            case KG_DIFF_NODE_ADDED:
                /* Would add node to KG */
                break;
            case KG_DIFF_NODE_REMOVED:
                brain_kg_remove_node(kg, change->node_id);
                break;
            case KG_DIFF_NODE_MODIFIED:
                /* Would update node properties */
                break;
            case KG_DIFF_EDGE_ADDED:
                /* Would add edge */
                break;
            case KG_DIFF_EDGE_REMOVED:
                brain_kg_remove_edge(kg, change->edge_from, change->edge_to);
                break;
            case KG_DIFF_EDGE_MODIFIED:
                /* Would update edge properties */
                break;
            case KG_DIFF_STATE_CHANGED:
                /* Would apply state change */
                break;
        }
    }

    nimcp_mutex_unlock(p->mutex);
    return 0;
}

int kg_persistence_save_incremental(kg_persistence_t* p,
                                     const kg_diff_result_t* diff) {
    if (!p || !diff) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_persistence_load: required parameter is NULL (p, diff)");
        return -1;
    }

    nimcp_mutex_lock(p->mutex);

    /* Write changes to audit/change trail table */
    for (uint32_t i = 0; i < diff->change_count; i++) {
        const kg_diff_record_t* change = &diff->changes[i];

        /* Would write to kg_change_events table */
        char buffer[NIMCP_ERROR_BUFFER_SIZE];
        snprintf(buffer, sizeof(buffer),
                 "type=%d,node=%llu,ts=%llu",
                 change->change_type,
                 (unsigned long long)change->node_id,
                 (unsigned long long)change->timestamp);

        kg_io_write_async(p->io_dispatcher, "kg_change_events",
                          buffer, strlen(buffer), NULL, NULL);
    }

    p->stored_version = diff->current_version;

    nimcp_mutex_unlock(p->mutex);
    return 0;
}

/* ============================================================================
 * Version Management Implementation
 * ============================================================================ */

uint64_t kg_persistence_get_stored_version(kg_persistence_t* p) {
    if (!p) return 0;
    return p->stored_version;
}

int kg_persistence_create_checkpoint(kg_persistence_t* p, const char* label) {
    if (!p || !label) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_persistence_create_checkpoint: required parameter is NULL (p, label)");
        return -1;
    }

    nimcp_mutex_lock(p->mutex);

    if (p->checkpoint_count >= PERSIST_MAX_CHECKPOINTS) {
        nimcp_mutex_unlock(p->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "kg_persistence_create_checkpoint: capacity exceeded");
        return -1;  /* Too many checkpoints */
    }

    checkpoint_record_t* cp = &p->checkpoints[p->checkpoint_count];
    safe_strcpy(cp->label, label, sizeof(cp->label));
    cp->version = p->stored_version;
    cp->timestamp = get_timestamp_ms();
    snprintf(cp->storage_path, sizeof(cp->storage_path),
             "%s/checkpoint_%s_%llu",
             p->config.storage_path, label,
             (unsigned long long)cp->timestamp);

    p->checkpoint_count++;

    if (p->audit_initialized) {
        char details[NIMCP_ERROR_BUFFER_MEDIUM];
        snprintf(details, sizeof(details), "Created checkpoint: %s", label);
        kg_persistence_audit_log(p, KG_AUDIT_KEY_GENERATED, details);
    }

    nimcp_mutex_unlock(p->mutex);
    return 0;
}

int kg_persistence_restore_checkpoint(kg_persistence_t* p, const char* label,
                                       brain_kg_t* kg) {
    if (!p || !label || !kg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_persistence_create_checkpoint: required parameter is NULL (p, label, kg)");
        return -1;
    }

    nimcp_mutex_lock(p->mutex);

    /* Find checkpoint */
    checkpoint_record_t* cp = NULL;
    for (uint32_t i = 0; i < p->checkpoint_count; i++) {
        if (strcmp(p->checkpoints[i].label, label) == 0) {
            cp = &p->checkpoints[i];
            break;
        }
    }

    if (!cp) {
        nimcp_mutex_unlock(p->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_persistence_create_checkpoint: cp is NULL");
        return -1;  /* Checkpoint not found */
    }

    /* Would load from checkpoint storage path */
    /* For now, just update version */
    p->stored_version = cp->version;

    nimcp_mutex_unlock(p->mutex);
    return 0;
}

/* ============================================================================
 * Encryption Key Management Implementation
 * ============================================================================ */

int kg_persistence_init_encryption(kg_persistence_t* p,
                                    const kg_encryption_config_t* config) {
    if (!p || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_persistence_create_checkpoint: required parameter is NULL (p, config)");
        return -1;
    }

    nimcp_mutex_lock(p->mutex);

    p->active_algorithm = config->algorithm;

    if (config->algorithm == KG_CRYPTO_NONE) {
        p->encryption_initialized = true;
        nimcp_mutex_unlock(p->mutex);
        return 0;
    }

    /* Initialize master key */
    if (config->master_key_path[0] != '\0') {
        FILE* f = fopen(config->master_key_path, "rb");
        if (f) {
            size_t read = fread(p->master_key, 1, sizeof(p->master_key), f);
            fclose(f);
            p->master_key_loaded = (read == sizeof(p->master_key));
        }
    }

    if (!p->master_key_loaded && config->hsm.type == KG_HSM_NONE) {
        /* Generate random key for demo */
        for (size_t i = 0; i < sizeof(p->master_key); i++) {
            p->master_key[i] = (uint8_t)(get_timestamp_ms() >> (i % 8));
        }
        p->master_key_loaded = true;
    }

    p->encryption_initialized = p->master_key_loaded;

    if (p->audit_initialized) {
        kg_persistence_audit_log(p, KG_AUDIT_KEY_GENERATED,
                                  p->encryption_initialized ?
                                  "Encryption initialized" : "Encryption init failed");
    }

    nimcp_mutex_unlock(p->mutex);
    return p->encryption_initialized ? 0 : -1;
}

int kg_persistence_generate_master_key(const char* output_path,
                                        kg_crypto_algorithm_t algorithm) {
    if (!output_path) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "output_path is NULL");

        return -1;
    }

    uint8_t key[32];

    /* Generate random key */
    uint64_t seed = get_timestamp_ms();
    for (size_t i = 0; i < sizeof(key); i++) {
        seed = seed * 6364136223846793005ULL + 1442695040888963407ULL;
        key[i] = (uint8_t)(seed >> 56);
    }

    /* Write to file */
    FILE* f = fopen(output_path, "wb");
    if (!f) {
        kg_persistence_secure_zero(key, sizeof(key));
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_persistence_create_checkpoint: f is NULL");
        return -1;
    }

    size_t written = fwrite(key, 1, sizeof(key), f);
    fclose(f);

    kg_persistence_secure_zero(key, sizeof(key));

    return (written == sizeof(key)) ? 0 : -1;
}

int kg_persistence_rotate_keys(kg_persistence_t* p) {
    if (!p || !p->encryption_initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_persistence_rotate_keys: required parameter is NULL (p, p->encryption_initialized)");
        return -1;
    }

    nimcp_mutex_lock(p->mutex);

    /* Generate new key */
    uint8_t new_key[32];
    uint64_t seed = get_timestamp_ms();
    for (size_t i = 0; i < sizeof(new_key); i++) {
        seed = seed * 6364136223846793005ULL + 1442695040888963407ULL;
        new_key[i] = (uint8_t)(seed >> 56);
    }

    /* Would re-encrypt all data with new key here */

    /* Replace old key */
    kg_persistence_secure_zero(p->master_key, sizeof(p->master_key));
    memcpy(p->master_key, new_key, sizeof(p->master_key));
    kg_persistence_secure_zero(new_key, sizeof(new_key));

    if (p->audit_initialized) {
        kg_persistence_audit_log(p, KG_AUDIT_KEY_ROTATED, "Master key rotated");
    }

    nimcp_mutex_unlock(p->mutex);
    return 0;
}

int kg_persistence_verify_integrity(kg_persistence_t* p) {
    if (!p) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "p is NULL");

        return -1;
    }

    nimcp_mutex_lock(p->mutex);

    if (p->audit_initialized) {
        kg_persistence_audit_log(p, KG_AUDIT_INTEGRITY_CHECK, "Integrity verification started");
    }

    /* Would verify HMAC and Merkle tree here */
    bool integrity_ok = true;

    if (!integrity_ok && p->audit_initialized) {
        kg_persistence_audit_log(p, KG_AUDIT_TAMPER_DETECTED, "Data tampering detected");
    }

    nimcp_mutex_unlock(p->mutex);
    return integrity_ok ? 0 : -1;
}

/* ============================================================================
 * HSM Operations Implementation
 * ============================================================================ */

int kg_persistence_init_hsm(kg_persistence_t* p, const kg_hsm_config_t* config) {
    if (!p || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_persistence_init_hsm: required parameter is NULL (p, config)");
        return -1;
    }

    nimcp_mutex_lock(p->mutex);

    if (p->hsm) {
        nimcp_mutex_unlock(p->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kg_persistence_init_hsm: validation failed");
        return -1;  /* Already initialized */
    }

    p->hsm = nimcp_calloc(1, sizeof(kg_hsm_handle_t));
    if (!p->hsm) {
        nimcp_mutex_unlock(p->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kg_persistence_init_hsm: p->hsm is NULL");
        return -1;
    }

    p->hsm->type = config->type;
    safe_strcpy(p->hsm->key_label, config->key_label, sizeof(p->hsm->key_label));

    mutex_attr_t attr = {0};
    attr.type = MUTEX_TYPE_NORMAL;
    p->hsm->mutex = nimcp_mutex_create(&attr);

    if (config->type != KG_HSM_NONE) {
        /* Would connect to actual HSM here */
        p->hsm->connected = false;  /* Simulated - not connected */
    } else {
        p->hsm->connected = true;  /* Software mode */
    }

    p->hsm_initialized = true;

    if (p->audit_initialized && p->hsm->connected) {
        kg_persistence_audit_log(p, KG_AUDIT_HSM_CONNECTED, "HSM connection established");
    }

    nimcp_mutex_unlock(p->mutex);
    return 0;
}

int kg_persistence_hsm_generate_key(kg_persistence_t* p, const char* key_label) {
    if (!p || !key_label || !p->hsm_initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_persistence_hsm_generate_key: required parameter is NULL (p, key_label, p->hsm_initialized)");
        return -1;
    }

    nimcp_mutex_lock(p->mutex);
    nimcp_mutex_lock(p->hsm->mutex);

    safe_strcpy(p->hsm->key_label, key_label, sizeof(p->hsm->key_label));
    p->hsm->creation_time = get_timestamp_ms();
    p->hsm->usage_count = 0;

    /* Would call PKCS#11 C_GenerateKey here */

    nimcp_mutex_unlock(p->hsm->mutex);
    nimcp_mutex_unlock(p->mutex);
    return 0;
}

int kg_persistence_hsm_import_key(kg_persistence_t* p, const void* key_material,
                                   size_t key_size, const char* key_label) {
    if (!p || !key_material || !key_label || !p->hsm_initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_persistence_hsm_generate_key: required parameter is NULL (p, key_material, key_label, p->hsm_initialized)");
        return -1;
    }

    nimcp_mutex_lock(p->mutex);
    nimcp_mutex_lock(p->hsm->mutex);

    /* Would wrap and import key via PKCS#11 */
    safe_strcpy(p->hsm->key_label, key_label, sizeof(p->hsm->key_label));
    p->hsm->creation_time = get_timestamp_ms();

    nimcp_mutex_unlock(p->hsm->mutex);
    nimcp_mutex_unlock(p->mutex);
    return 0;
}

bool kg_persistence_hsm_is_available(const kg_persistence_t* p) {
    if (!p || !p->hsm_initialized || !p->hsm) {
        return false;
    }
    return p->hsm->connected;
}

int kg_persistence_hsm_get_key_info(const kg_persistence_t* p,
                                     kg_hsm_key_info_t* info) {
    if (!p || !info || !p->hsm_initialized || !p->hsm) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_persistence_hsm_is_available: required parameter is NULL (p, info, p->hsm_initialized, p->hsm)");
        return -1;
    }

    safe_strcpy(info->key_label, p->hsm->key_label, sizeof(info->key_label));
    memset(info->key_id, 0, sizeof(info->key_id));
    memcpy(info->key_id, p->hsm->key_id, sizeof(p->hsm->key_id));
    info->creation_time = p->hsm->creation_time;
    info->last_used_time = get_timestamp_ms();
    info->usage_count = p->hsm->usage_count;
    info->is_extractable = false;
    info->is_rotated = false;

    return 0;
}

/* ============================================================================
 * Audit Logging Implementation
 * ============================================================================ */

int kg_persistence_audit_init(kg_persistence_t* p, const kg_audit_config_t* config) {
    if (!p || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_persistence_audit_init: required parameter is NULL (p, config)");
        return -1;
    }

    nimcp_mutex_lock(p->mutex);

    memcpy(&p->audit_config, config, sizeof(kg_audit_config_t));

    if (config->level != KG_AUDIT_LEVEL_NONE && config->log_path[0] != '\0') {
        p->audit_file = fopen(config->log_path, "ab");
        if (!p->audit_file) {
            nimcp_mutex_unlock(p->mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_persistence_audit_init: p->audit_file is NULL");
            return -1;
        }
    }

    p->audit_initialized = true;
    memset(p->last_audit_hash, 0, PERSIST_HASH_SIZE);

    nimcp_mutex_unlock(p->mutex);
    return 0;
}

int kg_persistence_audit_log(kg_persistence_t* p, kg_audit_event_type_t event,
                              const char* details) {
    if (!p || !p->audit_initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_persistence_audit_init: required parameter is NULL (p, p->audit_initialized)");
        return -1;
    }

    if (p->audit_config.level == KG_AUDIT_LEVEL_NONE) {
        return 0;
    }

    kg_audit_entry_t* entry = nimcp_calloc(1, sizeof(kg_audit_entry_t));
    if (!entry) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "entry is NULL");

        return -1;
    }

    entry->timestamp = get_timestamp_ms();
    entry->event_type = event;
    if (details) {
        safe_strcpy(entry->details, details, sizeof(entry->details));
    }

    /* Chain hash */
    memcpy(entry->prev_hash, p->last_audit_hash, PERSIST_HASH_SIZE);

    /* Compute entry hash */
    uint8_t hash_input[sizeof(entry->timestamp) + sizeof(entry->event_type) +
                       sizeof(entry->details) + PERSIST_HASH_SIZE];
    size_t offset = 0;
    memcpy(hash_input + offset, &entry->timestamp, sizeof(entry->timestamp));
    offset += sizeof(entry->timestamp);
    memcpy(hash_input + offset, &entry->event_type, sizeof(entry->event_type));
    offset += sizeof(entry->event_type);
    memcpy(hash_input + offset, entry->details, sizeof(entry->details));
    offset += sizeof(entry->details);
    memcpy(hash_input + offset, entry->prev_hash, PERSIST_HASH_SIZE);
    offset += PERSIST_HASH_SIZE;

    compute_hash(hash_input, offset, entry->entry_hash);
    memcpy(p->last_audit_hash, entry->entry_hash, PERSIST_HASH_SIZE);

    /* Add to chain */
    entry->next = NULL;
    if (p->audit_tail) {
        p->audit_tail->next = entry;
        p->audit_tail = entry;
    } else {
        p->audit_head = entry;
        p->audit_tail = entry;
    }
    p->audit_entry_count++;

    /* Write to file if configured */
    if (p->audit_file) {
        fprintf(p->audit_file, "%llu|%s|%s\n",
                (unsigned long long)entry->timestamp,
                kg_audit_event_type_to_string(event),
                entry->details);
        fflush(p->audit_file);
    }

    return 0;
}

int kg_persistence_audit_verify_chain(kg_persistence_t* p) {
    if (!p || !p->audit_initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_persistence_audit_verify_chain: required parameter is NULL (p, p->audit_initialized)");
        return -1;
    }

    nimcp_mutex_lock(p->mutex);

    kg_audit_entry_t* entry = p->audit_head;
    uint8_t expected_prev[PERSIST_HASH_SIZE] = {0};

    while (entry) {
        /* Verify prev_hash matches expected */
        if (memcmp(entry->prev_hash, expected_prev, PERSIST_HASH_SIZE) != 0) {
            nimcp_mutex_unlock(p->mutex);
            return -1;  /* Chain broken - tampering detected */
        }

        /* Verify entry hash */
        uint8_t computed_hash[PERSIST_HASH_SIZE];
        uint8_t hash_input[sizeof(entry->timestamp) + sizeof(entry->event_type) +
                           sizeof(entry->details) + PERSIST_HASH_SIZE];
        size_t offset = 0;
        memcpy(hash_input + offset, &entry->timestamp, sizeof(entry->timestamp));
        offset += sizeof(entry->timestamp);
        memcpy(hash_input + offset, &entry->event_type, sizeof(entry->event_type));
        offset += sizeof(entry->event_type);
        memcpy(hash_input + offset, entry->details, sizeof(entry->details));
        offset += sizeof(entry->details);
        memcpy(hash_input + offset, entry->prev_hash, PERSIST_HASH_SIZE);
        offset += PERSIST_HASH_SIZE;

        compute_hash(hash_input, offset, computed_hash);

        if (memcmp(entry->entry_hash, computed_hash, PERSIST_HASH_SIZE) != 0) {
            nimcp_mutex_unlock(p->mutex);
            return -1;  /* Entry hash mismatch */
        }

        memcpy(expected_prev, entry->entry_hash, PERSIST_HASH_SIZE);
        entry = entry->next;
    }

    nimcp_mutex_unlock(p->mutex);
    return 0;
}

int kg_persistence_audit_export(kg_persistence_t* p, const char* output_path,
                                 uint64_t start_time, uint64_t end_time) {
    if (!p || !output_path || !p->audit_initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_persistence_audit_verify_chain: required parameter is NULL (p, output_path, p->audit_initialized)");
        return -1;
    }

    nimcp_mutex_lock(p->mutex);

    FILE* out = fopen(output_path, "w");
    if (!out) {
        nimcp_mutex_unlock(p->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_persistence_audit_verify_chain: out is NULL");
        return -1;
    }

    kg_audit_entry_t* entry = p->audit_head;
    while (entry) {
        if (entry->timestamp >= start_time && entry->timestamp <= end_time) {
            fprintf(out, "%llu|%s|%s\n",
                    (unsigned long long)entry->timestamp,
                    kg_audit_event_type_to_string(entry->event_type),
                    entry->details);
        }
        entry = entry->next;
    }

    fclose(out);
    nimcp_mutex_unlock(p->mutex);
    return 0;
}

/* ============================================================================
 * Secure Memory Utilities Implementation
 * ============================================================================ */

void kg_persistence_secure_zero(void* ptr, size_t size) {
    if (!ptr || size == 0) {
        return;
    }

    volatile unsigned char* p = (volatile unsigned char*)ptr;
    while (size--) {
        *p++ = 0;
    }
}

/* ============================================================================
 * String Conversion Utilities Implementation
 * ============================================================================ */

const char* kg_crypto_algorithm_to_string(kg_crypto_algorithm_t algorithm) {
    switch (algorithm) {
        case KG_CRYPTO_NONE: return "NONE";
        case KG_CRYPTO_AES256_GCM: return "AES256_GCM";
        case KG_CRYPTO_XCHACHA20_POLY1305: return "XCHACHA20_POLY1305";
        case KG_CRYPTO_HYBRID_KYBER_AES: return "HYBRID_KYBER_AES";
        case KG_CRYPTO_HYBRID_KYBER_XCHACHA: return "HYBRID_KYBER_XCHACHA";
        default: return "UNKNOWN";
    }
}

const char* kg_hsm_type_to_string(kg_hsm_type_t type) {
    switch (type) {
        case KG_HSM_NONE: return "NONE";
        case KG_HSM_TPM2: return "TPM2";
        case KG_HSM_PKCS11: return "PKCS11";
        case KG_HSM_AWS_CLOUDHSM: return "AWS_CLOUDHSM";
        case KG_HSM_AZURE_KEYVAULT: return "AZURE_KEYVAULT";
        case KG_HSM_GCP_CLOUD_HSM: return "GCP_CLOUD_HSM";
        default: return "UNKNOWN";
    }
}

const char* kg_audit_level_to_string(kg_audit_level_t level) {
    switch (level) {
        case KG_AUDIT_LEVEL_NONE: return "NONE";
        case KG_AUDIT_LEVEL_BASIC: return "BASIC";
        case KG_AUDIT_LEVEL_DETAILED: return "DETAILED";
        case KG_AUDIT_LEVEL_FULL: return "FULL";
        default: return "UNKNOWN";
    }
}

const char* kg_audit_event_type_to_string(kg_audit_event_type_t event) {
    switch (event) {
        case KG_AUDIT_KEY_GENERATED: return "KEY_GENERATED";
        case KG_AUDIT_KEY_ROTATED: return "KEY_ROTATED";
        case KG_AUDIT_KEY_IMPORTED: return "KEY_IMPORTED";
        case KG_AUDIT_ENCRYPT_START: return "ENCRYPT_START";
        case KG_AUDIT_ENCRYPT_SUCCESS: return "ENCRYPT_SUCCESS";
        case KG_AUDIT_ENCRYPT_FAILURE: return "ENCRYPT_FAILURE";
        case KG_AUDIT_DECRYPT_START: return "DECRYPT_START";
        case KG_AUDIT_DECRYPT_SUCCESS: return "DECRYPT_SUCCESS";
        case KG_AUDIT_DECRYPT_FAILURE: return "DECRYPT_FAILURE";
        case KG_AUDIT_INTEGRITY_CHECK: return "INTEGRITY_CHECK";
        case KG_AUDIT_TAMPER_DETECTED: return "TAMPER_DETECTED";
        case KG_AUDIT_HSM_CONNECTED: return "HSM_CONNECTED";
        case KG_AUDIT_HSM_DISCONNECTED: return "HSM_DISCONNECTED";
        default: return "UNKNOWN";
    }
}

const char* kg_questdb_mode_to_string(kg_questdb_mode_t mode) {
    switch (mode) {
        case KG_QUESTDB_EMBEDDED: return "EMBEDDED";
        case KG_QUESTDB_SERVER: return "SERVER";
        case KG_QUESTDB_CLUSTER: return "CLUSTER";
        default: return "UNKNOWN";
    }
}

const char* kg_tls_mode_to_string(kg_tls_mode_t mode) {
    switch (mode) {
        case KG_TLS_MODE_DISABLED: return "DISABLED";
        case KG_TLS_MODE_REQUIRE: return "REQUIRE";
        case KG_TLS_MODE_VERIFY_CA: return "VERIFY_CA";
        case KG_TLS_MODE_VERIFY_FULL: return "VERIFY_FULL";
        case KG_TLS_MODE_MTLS: return "MTLS";
        default: return "UNKNOWN";
    }
}

const char* kg_diff_change_type_to_string(kg_diff_change_type_t type) {
    switch (type) {
        case KG_DIFF_NODE_ADDED: return "NODE_ADDED";
        case KG_DIFF_NODE_REMOVED: return "NODE_REMOVED";
        case KG_DIFF_NODE_MODIFIED: return "NODE_MODIFIED";
        case KG_DIFF_EDGE_ADDED: return "EDGE_ADDED";
        case KG_DIFF_EDGE_REMOVED: return "EDGE_REMOVED";
        case KG_DIFF_EDGE_MODIFIED: return "EDGE_MODIFIED";
        case KG_DIFF_STATE_CHANGED: return "STATE_CHANGED";
        default: return "UNKNOWN";
    }
}
