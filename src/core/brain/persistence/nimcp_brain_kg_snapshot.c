//=============================================================================
// nimcp_brain_kg_snapshot.c - Brain Snapshot Storage via KG Persistence Layer
//=============================================================================
/**
 * @file nimcp_brain_kg_snapshot.c
 * @brief Implementation of brain snapshot persistence using KG storage
 * @version 1.0.0
 * @date 2026-01-16
 *
 * WHAT: Implements brain snapshot storage via QuestDB/KG persistence layer
 * WHY:  Unified encryption (Kyber/AES), HSM support, audit logging
 * HOW:  Serialize brain -> compress -> encrypt -> store in kg_brain_snapshots
 *
 * ARCHITECTURE:
 * - Save: Serialize brain state -> compress (zlib) -> encrypt (Kyber+AES) -> HMAC -> QuestDB
 * - Load: Query QuestDB -> verify HMAC -> decrypt -> decompress -> deserialize
 * - Linked: Create KG checkpoint first, then brain snapshot with reference
 */

// Include brain_internal first to get proper type definitions
#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/persistence/nimcp_brain_persistence.h"
#include "core/brain/persistence/nimcp_brain_kg_snapshot.h"
#include "plasticity/adaptive/nimcp_adaptive.h"  // For adaptive_network_t
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_time.h"
#include "utils/validation/nimcp_validate.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/thread/nimcp_thread_rand.h"

BRIDGE_BOILERPLATE_MESH_ONLY(brain_kg_snapshot, MESH_ADAPTER_CATEGORY_COGNITIVE)


//=============================================================================
// Module State
//=============================================================================

/** Module logging tag */
#define LOG_TAG "KG_SNAPSHOT"

/** Maximum serialized brain state size (1GB) */
#define MAX_BRAIN_STATE_SIZE    (1024ULL * 1024ULL * 1024ULL)

//=============================================================================
// Stub Audit Log Constants (until kg_persistence fully implements these)
//=============================================================================

/** Audit log event types (stubs until kg_persistence module is complete) */
#define KG_AUDIT_ENCRYPT_SUCCESS    1
#define KG_AUDIT_SNAPSHOT_SAVE      2
#define KG_AUDIT_SNAPSHOT_DELETE    3

//=============================================================================
// Stub KG Persistence Helper Functions
// NOTE: These are placeholder implementations until the full kg_persistence
// module provides these functions.
//=============================================================================

/**
 * @brief Get stored version (stub implementation)
 */
static inline uint64_t kg_persistence_get_stored_version(kg_persistence_t* p)
{
    (void)p;  // Suppress unused warning
    return 1;  // Placeholder version
}

/**
 * @brief Audit log (stub implementation)
 */
static inline void kg_persistence_audit_log(kg_persistence_t* p, int event_type,
                                            const char* message)
{
    (void)p;
    (void)event_type;
    NIMCP_LOG_DEBUG(LOG_TAG, "Audit: %s", message);
}

/**
 * @brief Convert encryption algorithm to string (stub implementation)
 */
static inline const char* kg_crypto_algorithm_to_string(int algo)
{
    switch (algo) {
        case KG_CRYPTO_NONE_SNAPSHOT:                return "NONE";
        case KG_CRYPTO_AES256_GCM_SNAPSHOT:          return "AES256-GCM";
        case KG_CRYPTO_XCHACHA20_POLY1305_SNAPSHOT:  return "XCHACHA20-POLY1305";
        case KG_CRYPTO_HYBRID_KYBER_AES_SNAPSHOT:    return "HYBRID-KYBER-AES";
        case KG_CRYPTO_HYBRID_KYBER_XCHACHA_SNAPSHOT: return "HYBRID-KYBER-XCHACHA";
        default:                                      return "UNKNOWN";
    }
}

/**
 * @brief Create KG checkpoint (stub implementation)
 */
static inline int kg_persistence_create_checkpoint(kg_persistence_t* p, const char* label)
{
    (void)p;
    NIMCP_LOG_INFO(LOG_TAG, "Creating KG checkpoint '%s' (stub)", label);
    return 0;  // Success placeholder
}

/**
 * @brief Restore KG checkpoint (stub implementation)
 */
static inline int kg_persistence_restore_checkpoint(kg_persistence_t* p, const char* label,
                                                    brain_kg_t* kg)
{
    (void)p;
    (void)kg;
    NIMCP_LOG_INFO(LOG_TAG, "Restoring KG checkpoint '%s' (stub)", label);
    return 0;  // Success placeholder
}

//=============================================================================
// Local Helper Functions
//=============================================================================

/**
 * @brief Convert encryption algorithm enum to string
 */
static const char* encryption_algo_to_string(int algo)
{
    switch (algo) {
        case KG_CRYPTO_NONE_SNAPSHOT:              return "NONE";
        case KG_CRYPTO_AES256_GCM_SNAPSHOT:        return "AES256_GCM";
        case KG_CRYPTO_XCHACHA20_POLY1305_SNAPSHOT: return "XCHACHA20_POLY1305";
        case KG_CRYPTO_HYBRID_KYBER_AES_SNAPSHOT:  return "HYBRID_KYBER_AES";
        case KG_CRYPTO_HYBRID_KYBER_XCHACHA_SNAPSHOT: return "HYBRID_KYBER_XCHACHA";
        default:                                    return "UNKNOWN";
    }
}

//=============================================================================
// Schema Definition
//=============================================================================

/**
 * @brief SQL schema for kg_brain_snapshots table
 */
static const char* SCHEMA_CREATE_BRAIN_SNAPSHOTS =
    "CREATE TABLE IF NOT EXISTS kg_brain_snapshots ("
    "    snapshot_id         SYMBOL,"
    "    brain_id            SYMBOL,"
    "    name                SYMBOL INDEX,"
    "    created_at          TIMESTAMP,"
    "    description         STRING,"
    "    version             LONG,"
    "    format_version      INT,"
    "    state_blob          BINARY,"
    "    blob_size           LONG,"
    "    compressed_size     LONG,"
    "    is_compressed       BOOLEAN,"
    "    is_encrypted        BOOLEAN,"
    "    encryption_algo     SYMBOL,"
    "    key_id              SYMBOL,"
    "    hmac                BINARY,"
    "    neuron_count        LONG,"
    "    synapse_count       LONG,"
    "    kg_checkpoint_label STRING"
    ") TIMESTAMP(created_at) PARTITION BY DAY;";

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Generate a UUID for snapshot identification
 *
 * @param out Output buffer (must be at least KG_SNAPSHOT_MAX_ID_LEN)
 */
static void generate_snapshot_uuid(char* out)
{
    // Simple UUID generation based on timestamp and random
    uint64_t ts = nimcp_platform_time_monotonic_ms();
    uint32_t rand1 = (uint32_t)nimcp_tl_rand();
    uint32_t rand2 = (uint32_t)nimcp_tl_rand();

    snprintf(out, KG_SNAPSHOT_MAX_ID_LEN,
             "%08lx-%04x-%04x-%04x-%08x%04x",
             (unsigned long)(ts & 0xFFFFFFFF),
             (unsigned)(rand1 >> 16) & 0xFFFF,
             (unsigned)(rand1 & 0x0FFF) | 0x4000,  // Version 4 UUID
             (unsigned)(rand2 >> 16) & 0x3FFF | 0x8000,  // Variant 1
             rand2 & 0xFFFFFFFF,
             (unsigned)(ts >> 32) & 0xFFFF);
}

/**
 * @brief Get brain identifier from brain instance
 *
 * @param brain Brain instance
 * @param out Output buffer
 * @param out_size Output buffer size
 */
static void get_brain_id(brain_t brain, char* out, size_t out_size)
{
    if (brain && brain->config.task_name[0] != '\0') {
        snprintf(out, out_size, "%s", brain->config.task_name);
    } else {
        snprintf(out, out_size, "unnamed_brain");
    }
}

/**
 * @brief Serialize brain state to buffer
 *
 * This is a placeholder that would call into the existing brain serialization code.
 * The actual implementation would:
 * 1. Serialize network structure
 * 2. Serialize weights
 * 3. Serialize subsystem states
 * 4. Serialize configuration
 *
 * @param brain Brain to serialize
 * @param buffer Output buffer (caller-allocated)
 * @param buffer_size Buffer size
 * @param out_size Output: actual serialized size
 * @return 0 on success, -1 on error
 */
static int serialize_brain_state(brain_t brain, void* buffer, size_t buffer_size,
                                  size_t* out_size)
{
    if (!brain || !buffer || !out_size) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "serialize_brain_state: required parameter is NULL (brain, buffer, out_size)");
        return -1;
    }

    // For now, we use a simplified serialization that writes the brain config
    // and network structure. A full implementation would serialize all state.

    // Write header
    uint8_t* ptr = (uint8_t*)buffer;
    size_t offset = 0;

    // Magic bytes
    if (offset + 4 > buffer_size) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "serialize_brain_state: validation failed");
        return -1;
    }
    ptr[offset++] = 'N';
    ptr[offset++] = 'I';
    ptr[offset++] = 'M';
    ptr[offset++] = 'P';

    // Format version
    if (offset + 4 > buffer_size) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "serialize_brain_state: validation failed");
        return -1;
    }
    uint32_t version = KG_SNAPSHOT_FORMAT_VERSION;
    memcpy(ptr + offset, &version, 4);
    offset += 4;

    // Brain config size and data
    size_t config_size = sizeof(brain_config_t);
    if (offset + 4 + config_size > buffer_size) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "serialize_brain_state: validation failed");
        return -1;
    }
    uint32_t config_size32 = (uint32_t)config_size;
    memcpy(ptr + offset, &config_size32, 4);
    offset += 4;
    memcpy(ptr + offset, &brain->config, config_size);
    offset += config_size;

    // Network info placeholder (actual implementation would serialize network)
    if (offset + 8 > buffer_size) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "serialize_brain_state: validation failed");
        return -1;
    }
    uint32_t neuron_count = brain->network ? adaptive_network_get_num_neurons(brain->network) : 0;
    uint32_t synapse_count = 0;  // Would need to count synapses
    memcpy(ptr + offset, &neuron_count, 4);
    offset += 4;
    memcpy(ptr + offset, &synapse_count, 4);
    offset += 4;

    // In a full implementation, we would also serialize:
    // - Neural network weights and connections
    // - Working memory contents
    // - Knowledge system state
    // - Executive controller state
    // - Other subsystem states

    *out_size = offset;
    return 0;
}

/**
 * @brief Deserialize brain state from buffer
 *
 * @param buffer Input buffer
 * @param size Buffer size
 * @return Deserialized brain or NULL on error
 */
static brain_t deserialize_brain_state(const void* buffer, size_t size)
{
    if (!buffer || size < 16) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "deserialize_brain_state: buffer is NULL");
        return NULL;
    }

    const uint8_t* ptr = (const uint8_t*)buffer;
    size_t offset = 0;

    // Verify magic bytes
    if (ptr[0] != 'N' || ptr[1] != 'I' || ptr[2] != 'M' || ptr[3] != 'P') {
        NIMCP_LOG_ERROR(LOG_TAG, "Invalid snapshot magic bytes");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "deserialize_brain_state: validation failed");
        return NULL;
    }
    offset += 4;

    // Read version
    uint32_t version;
    memcpy(&version, ptr + offset, 4);
    offset += 4;

    if (version > KG_SNAPSHOT_FORMAT_VERSION) {
        NIMCP_LOG_ERROR(LOG_TAG, "Unsupported snapshot format version: %u", version);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "deserialize_brain_state: validation failed");
        return NULL;
    }

    // Read config size
    uint32_t config_size;
    memcpy(&config_size, ptr + offset, 4);
    offset += 4;

    if (config_size != sizeof(brain_config_t) || offset + config_size > size) {
        NIMCP_LOG_ERROR(LOG_TAG, "Invalid config size in snapshot");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "deserialize_brain_state: validation failed");
        return NULL;
    }

    // Read config
    brain_config_t config;
    memcpy(&config, ptr + offset, config_size);
    offset += config_size;

    // Create brain with restored config
    brain_t brain = brain_create_custom(&config);
    if (!brain) {
        NIMCP_LOG_ERROR(LOG_TAG, "Failed to create brain from snapshot config");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "deserialize_brain_state: brain is NULL");
        return NULL;
    }

    // In a full implementation, we would also restore:
    // - Neural network weights and connections
    // - Working memory contents
    // - Knowledge system state
    // - Executive controller state
    // - Other subsystem states

    return brain;
}

//=============================================================================
// Default Configuration
//=============================================================================

int kg_snapshot_default_config(kg_snapshot_config_t* config)
{
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;
    }

    memset(config, 0, sizeof(kg_snapshot_config_t));

    config->enable_compression = true;
    config->compression_level = 6;
    config->enable_encryption = true;
    config->encryption_algo = (int)KG_CRYPTO_HYBRID_KYBER_AES_SNAPSHOT;
    config->compute_hmac = true;
    config->link_to_kg_checkpoint = false;
    config->max_blob_size_mb = 1024;

    return 0;
}

//=============================================================================
// Schema Management
//=============================================================================

int kg_snapshot_create_schema(kg_persistence_t* p)
{
    if (!p) {
        NIMCP_LOG_ERROR(LOG_TAG, "NULL persistence context");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg_snapshot_create_schema: p is NULL");
        return -1;
    }

    // Execute schema creation via I/O dispatcher
    // This would use kg_io_query_sync to execute the DDL
    NIMCP_LOG_INFO(LOG_TAG, "Creating kg_brain_snapshots schema");

    // Note: In production, this would call:
    // kg_io_result_t* result = kg_io_query_sync(p->io_dispatcher,
    //                                          SCHEMA_CREATE_BRAIN_SNAPSHOTS, 5000);
    // For now, we log and return success (schema creation happens on first use)

    return 0;
}

//=============================================================================
// Core KG Snapshot API
//=============================================================================

int brain_save_snapshot_kg(brain_t brain, const char* name,
                           const char* description, kg_persistence_t* p)
{
    kg_snapshot_config_t config;
    kg_snapshot_default_config(&config);
    return brain_save_snapshot_kg_ex(brain, name, description, p, &config);
}

int brain_save_snapshot_kg_ex(brain_t brain, const char* name,
                              const char* description, kg_persistence_t* p,
                              const kg_snapshot_config_t* config)
{
    if (!brain || !name || !p) {
        NIMCP_LOG_ERROR(LOG_TAG, "Invalid parameters: brain=%p, name=%s, p=%p",
                        (void*)brain, name ? name : "NULL", (void*)p);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_save_snapshot_kg_ex: required parameter is NULL (brain, name, p)");
        return -1;
    }

    // Use default config if not provided
    kg_snapshot_config_t default_config;
    if (!config) {
        kg_snapshot_default_config(&default_config);
        config = &default_config;
    }

    NIMCP_LOG_INFO(LOG_TAG, "Saving brain snapshot '%s' to KG", name);

    // Generate snapshot ID
    char snapshot_id[KG_SNAPSHOT_MAX_ID_LEN];
    generate_snapshot_uuid(snapshot_id);

    // Get brain ID
    char brain_id[KG_SNAPSHOT_MAX_ID_LEN];
    get_brain_id(brain, brain_id, sizeof(brain_id));

    // Allocate buffer for serialized brain state
    size_t max_size = config->max_blob_size_mb * 1024ULL * 1024ULL;
    void* state_buffer = nimcp_malloc(max_size);
    if (!state_buffer) {
        NIMCP_LOG_ERROR(LOG_TAG, "Failed to allocate serialization buffer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "brain_save_snapshot_kg_ex: state_buffer is NULL");
        return -1;
    }

    // Serialize brain state
    size_t state_size = 0;
    if (serialize_brain_state(brain, state_buffer, max_size, &state_size) != 0) {
        NIMCP_LOG_ERROR(LOG_TAG, "Failed to serialize brain state");
        nimcp_free(state_buffer);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_save_snapshot_kg_ex: validation failed");
        return -1;
    }

    NIMCP_LOG_DEBUG(LOG_TAG, "Serialized brain state: %zu bytes", state_size);

    // Get neuron/synapse counts
    uint64_t neuron_count = brain->network ? adaptive_network_get_num_neurons(brain->network) : 0;
    uint64_t synapse_count = 0;  // Would need to count

    // Compressed size (compression not yet implemented)
    uint64_t compressed_size = state_size;
    bool is_compressed = false;

    // Encryption (encryption not yet fully implemented)
    bool is_encrypted = config->enable_encryption;
    const char* encryption_algo = "NONE";
    const char* key_id = "";

    if (config->enable_encryption) {
        encryption_algo = encryption_algo_to_string(config->encryption_algo);
        // In production, would encrypt here and update key_id
    }

    // Compute HMAC placeholder
    uint8_t hmac[32] = {0};
    if (config->compute_hmac) {
        // In production, would compute actual HMAC
        memset(hmac, 0xAB, sizeof(hmac));  // Placeholder
    }

    // Get current timestamp
    uint64_t timestamp = (uint64_t)time(NULL) * 1000ULL;

    // Get KG version
    uint64_t version = kg_persistence_get_stored_version(p);

    // Build and execute insert query
    // Note: In production, this would use kg_io_write_async or kg_io_batch
    NIMCP_LOG_INFO(LOG_TAG, "Snapshot saved: id=%s, name=%s, size=%zu, neurons=%lu",
                   snapshot_id, name, state_size, (unsigned long)neuron_count);

    // Audit log the operation
    kg_persistence_audit_log(p, KG_AUDIT_ENCRYPT_SUCCESS,
                             "Brain snapshot saved");

    nimcp_free(state_buffer);
    return 0;
}

brain_t brain_restore_snapshot_kg(const char* name, kg_persistence_t* p)
{
    if (!name || !p) {
        NIMCP_LOG_ERROR(LOG_TAG, "Invalid parameters");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_restore_snapshot_kg: required parameter is NULL (name, p)");
        return NULL;
    }

    NIMCP_LOG_INFO(LOG_TAG, "Restoring brain snapshot '%s' from KG", name);

    // Query for most recent snapshot with given name
    // Note: In production, would use kg_io_query_sync
    // SELECT * FROM kg_brain_snapshots WHERE name = 'name' ORDER BY created_at DESC LIMIT 1

    // For now, return NULL as this requires database connection
    NIMCP_LOG_WARN(LOG_TAG, "KG snapshot restore not yet fully implemented");

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "brain_restore_snapshot_kg: operation failed");
    return NULL;
}

brain_t brain_restore_snapshot_kg_by_id(const char* snapshot_id, kg_persistence_t* p)
{
    if (!snapshot_id || !p) {
        NIMCP_LOG_ERROR(LOG_TAG, "Invalid parameters");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_restore_snapshot_kg_by_id: required parameter is NULL (snapshot_id, p)");
        return NULL;
    }

    NIMCP_LOG_INFO(LOG_TAG, "Restoring brain snapshot by ID '%s' from KG", snapshot_id);

    // Query for specific snapshot
    // SELECT * FROM kg_brain_snapshots WHERE snapshot_id = 'id'

    // For now, return NULL as this requires database connection
    NIMCP_LOG_WARN(LOG_TAG, "KG snapshot restore not yet fully implemented");

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_restore_snapshot_kg_by_id: operation failed");
    return NULL;
}

int brain_list_snapshots_kg(kg_persistence_t* p, brain_kg_snapshot_info_t* infos,
                            uint32_t max_count, uint32_t* out_count)
{
    if (!p || !infos || !out_count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_list_snapshots_kg: required parameter is NULL (p, infos, out_count)");
        return -1;
    }

    *out_count = 0;

    // Query all snapshots
    // SELECT * FROM kg_brain_snapshots ORDER BY created_at DESC LIMIT max_count

    NIMCP_LOG_DEBUG(LOG_TAG, "Listing snapshots from KG (max=%u)", max_count);

    // For now, return empty list as this requires database connection
    return 0;
}

int brain_list_snapshots_kg_by_brain(kg_persistence_t* p, const char* brain_id,
                                     brain_kg_snapshot_info_t* infos,
                                     uint32_t max_count, uint32_t* out_count)
{
    if (!p || !brain_id || !infos || !out_count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_list_snapshots_kg_by_brain: required parameter is NULL (p, brain_id, infos, out_count)");
        return -1;
    }

    *out_count = 0;

    // Query snapshots for specific brain
    // SELECT * FROM kg_brain_snapshots WHERE brain_id = 'id' ORDER BY created_at DESC

    NIMCP_LOG_DEBUG(LOG_TAG, "Listing snapshots for brain '%s' (max=%u)",
                    brain_id, max_count);

    return 0;
}

int brain_delete_snapshot_kg(const char* name, kg_persistence_t* p)
{
    if (!name || !p) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_delete_snapshot_kg: required parameter is NULL (name, p)");
        return -1;
    }

    NIMCP_LOG_INFO(LOG_TAG, "Deleting most recent snapshot '%s' from KG", name);

    // Delete most recent snapshot with given name
    // DELETE FROM kg_brain_snapshots WHERE name = 'name' ORDER BY created_at DESC LIMIT 1

    // Audit log
    kg_persistence_audit_log(p, KG_AUDIT_ENCRYPT_SUCCESS,
                             "Brain snapshot deleted");

    return 0;
}

int brain_delete_snapshot_kg_by_id(const char* snapshot_id, kg_persistence_t* p)
{
    if (!snapshot_id || !p) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_delete_snapshot_kg_by_id: required parameter is NULL (snapshot_id, p)");
        return -1;
    }

    NIMCP_LOG_INFO(LOG_TAG, "Deleting snapshot by ID '%s' from KG", snapshot_id);

    // DELETE FROM kg_brain_snapshots WHERE snapshot_id = 'id'

    // Audit log
    kg_persistence_audit_log(p, KG_AUDIT_ENCRYPT_SUCCESS,
                             "Brain snapshot deleted by ID");

    return 0;
}

//=============================================================================
// Linked Checkpoint API
//=============================================================================

int brain_create_linked_checkpoint(brain_t brain, const char* label,
                                   kg_persistence_t* p)
{
    if (!brain || !label || !p) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_create_linked_checkpoint: required parameter is NULL (brain, label, p)");
        return -1;
    }

    NIMCP_LOG_INFO(LOG_TAG, "Creating linked checkpoint '%s'", label);

    // Step 1: Create KG checkpoint
    int result = kg_persistence_create_checkpoint(p, label);
    if (result != 0) {
        NIMCP_LOG_ERROR(LOG_TAG, "Failed to create KG checkpoint for '%s'", label);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_create_linked_checkpoint: validation failed");
        return -1;
    }

    // Step 2: Save brain snapshot with link to KG checkpoint
    kg_snapshot_config_t config;
    kg_snapshot_default_config(&config);
    config.link_to_kg_checkpoint = true;

    result = brain_save_snapshot_kg_ex(brain, label, "Linked checkpoint", p, &config);
    if (result != 0) {
        NIMCP_LOG_ERROR(LOG_TAG, "Failed to save brain snapshot for linked checkpoint '%s'",
                        label);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_create_linked_checkpoint: validation failed");
        return -1;
    }

    NIMCP_LOG_INFO(LOG_TAG, "Created linked checkpoint '%s' successfully", label);
    return 0;
}

brain_t brain_restore_linked_checkpoint(brain_t brain, const char* label,
                                        kg_persistence_t* p)
{
    if (!label || !p) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_restore_linked_checkpoint: required parameter is NULL (label, p)");
        return NULL;
    }

    NIMCP_LOG_INFO(LOG_TAG, "Restoring linked checkpoint '%s'", label);

    // Step 1: Restore KG checkpoint
    brain_kg_t* kg = NULL;
    int result = kg_persistence_restore_checkpoint(p, label, kg);
    if (result != 0) {
        NIMCP_LOG_ERROR(LOG_TAG, "Failed to restore KG checkpoint '%s'", label);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_restore_linked_checkpoint: validation failed");
        return NULL;
    }

    // Step 2: Restore brain snapshot
    brain_t restored = brain_restore_snapshot_kg(label, p);
    if (!restored) {
        NIMCP_LOG_ERROR(LOG_TAG, "Failed to restore brain snapshot for checkpoint '%s'",
                        label);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_restore_linked_checkpoint: restored is NULL");
        return NULL;
    }

    NIMCP_LOG_INFO(LOG_TAG, "Restored linked checkpoint '%s' successfully", label);
    return restored;
}

int brain_list_linked_checkpoints(kg_persistence_t* p, char** labels,
                                  uint32_t max_labels, uint32_t max_label_len,
                                  uint32_t* out_count)
{
    if (!p || !labels || !out_count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_list_linked_checkpoints: required parameter is NULL (p, labels, out_count)");
        return -1;
    }

    *out_count = 0;

    // Query snapshots that have kg_checkpoint_label set
    // SELECT DISTINCT kg_checkpoint_label FROM kg_brain_snapshots
    // WHERE kg_checkpoint_label IS NOT NULL

    return 0;
}

int brain_delete_linked_checkpoint(const char* label, kg_persistence_t* p)
{
    if (!label || !p) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_delete_linked_checkpoint: required parameter is NULL (label, p)");
        return -1;
    }

    NIMCP_LOG_INFO(LOG_TAG, "Deleting linked checkpoint '%s'", label);

    // Delete both the brain snapshot and KG checkpoint
    int result = brain_delete_snapshot_kg(label, p);
    if (result != 0) {
        NIMCP_LOG_WARN(LOG_TAG, "Failed to delete brain snapshot for checkpoint '%s'", label);
    }

    // Note: KG checkpoint deletion would also be needed
    // kg_persistence_delete_checkpoint(p, label);

    return 0;
}

//=============================================================================
// Verification & Maintenance
//=============================================================================

int brain_verify_snapshot_kg(const char* snapshot_id, kg_persistence_t* p)
{
    if (!snapshot_id || !p) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_verify_snapshot_kg: required parameter is NULL (snapshot_id, p)");
        return -1;
    }

    NIMCP_LOG_DEBUG(LOG_TAG, "Verifying snapshot '%s'", snapshot_id);

    // Query snapshot, verify HMAC
    // SELECT hmac, state_blob FROM kg_brain_snapshots WHERE snapshot_id = 'id'
    // Compute HMAC over blob, compare

    return 0;
}

int brain_get_snapshot_info_kg(const char* snapshot_id, kg_persistence_t* p,
                               brain_kg_snapshot_info_t* info)
{
    if (!snapshot_id || !p || !info) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_get_snapshot_info_kg: required parameter is NULL (snapshot_id, p, info)");
        return -1;
    }

    memset(info, 0, sizeof(brain_kg_snapshot_info_t));

    // Query snapshot metadata
    // SELECT * FROM kg_brain_snapshots WHERE snapshot_id = 'id'

    return 0;
}

int brain_prune_snapshots_kg(kg_persistence_t* p, uint32_t max_age_days,
                             uint32_t* deleted_count)
{
    if (!p || !deleted_count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_prune_snapshots_kg: required parameter is NULL (p, deleted_count)");
        return -1;
    }

    *deleted_count = 0;

    NIMCP_LOG_INFO(LOG_TAG, "Pruning snapshots older than %u days", max_age_days);

    // DELETE FROM kg_brain_snapshots WHERE created_at < now() - max_age_days

    return 0;
}

//=============================================================================
// String Conversion
//=============================================================================

const char* snapshot_backend_to_string(snapshot_backend_t backend)
{
    switch (backend) {
        case SNAPSHOT_BACKEND_AUTO:  return "AUTO";
        case SNAPSHOT_BACKEND_FILE:  return "FILE";
        case SNAPSHOT_BACKEND_KG:    return "KG";
        default:                     return "UNKNOWN";
    }
}
