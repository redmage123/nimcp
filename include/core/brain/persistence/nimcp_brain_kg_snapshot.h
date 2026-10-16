//=============================================================================
// nimcp_brain_kg_snapshot.h - Brain Snapshot Storage via KG Persistence Layer
//=============================================================================
/**
 * @file nimcp_brain_kg_snapshot.h
 * @brief Brain snapshot persistence using KG storage (QuestDB)
 * @version 1.0.0
 * @date 2026-01-16
 *
 * WHAT: Persistent storage of brain snapshots via the KG persistence layer
 * WHY:  Unified encryption (Kyber/AES), HSM support, audit logging, and
 *       transactional consistency with KG state
 * HOW:  Store brain snapshots in QuestDB kg_brain_snapshots table with
 *       same security guarantees as KG data
 *
 * ARCHITECTURE:
 * ```
 * CURRENT (FILE-BASED):
 * brain_save_snapshot() --> files: {snapshot_dir}/{name}_{timestamp}.snapshot
 *                      --> .snapshot.info, .meta, .knowledge, etc.
 *
 * TARGET (KG-BASED):
 * brain_save_snapshot() --> kg_persistence_t --> QuestDB (kg_brain_snapshots table)
 *                                           --> Same encryption/HSM/audit as KG
 * ```
 *
 * SCHEMA: kg_brain_snapshots
 * - snapshot_id:       SYMBOL (UUID)
 * - brain_id:          SYMBOL (Brain identifier)
 * - name:              SYMBOL INDEX (User-provided name)
 * - created_at:        TIMESTAMP (Designated timestamp)
 * - description:       STRING
 * - version:           LONG (KG version at snapshot)
 * - format_version:    INT (Format compatibility)
 * - state_blob:        BINARY (Encrypted brain state)
 * - blob_size:         LONG
 * - compressed_size:   LONG
 * - is_compressed:     BOOLEAN
 * - is_encrypted:      BOOLEAN
 * - encryption_algo:   SYMBOL
 * - key_id:            SYMBOL
 * - hmac:              BINARY (Integrity verification)
 * - neuron_count:      LONG
 * - synapse_count:     LONG
 * - kg_checkpoint_label: STRING (Link to KG checkpoint)
 *
 * THREAD SAFETY: All operations are thread-safe via internal synchronization
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_BRAIN_KG_SNAPSHOT_H
#define NIMCP_BRAIN_KG_SNAPSHOT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Forward declarations to avoid include conflicts
typedef struct brain_struct* brain_t;
typedef struct kg_persistence kg_persistence_t;
typedef struct brain_kg brain_kg_t;

// Include only for kg_crypto_algorithm_t enum definition
// Note: This is needed for the snapshot info struct
typedef enum {
    KG_CRYPTO_NONE_SNAPSHOT = 0,             // No encryption (testing only)
    KG_CRYPTO_AES256_GCM_SNAPSHOT,           // Classical
    KG_CRYPTO_XCHACHA20_POLY1305_SNAPSHOT,   // Classical
    KG_CRYPTO_HYBRID_KYBER_AES_SNAPSHOT,     // RECOMMENDED
    KG_CRYPTO_HYBRID_KYBER_XCHACHA_SNAPSHOT  // Alternative
} kg_crypto_algorithm_snapshot_t;

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Maximum snapshot ID length */
#define KG_SNAPSHOT_MAX_ID_LEN              64

/** Maximum checkpoint label length */
#define KG_SNAPSHOT_MAX_LABEL_LEN           64

/** Current snapshot format version */
#define KG_SNAPSHOT_FORMAT_VERSION          1

/** Table name for brain snapshots */
#define KG_BRAIN_SNAPSHOTS_TABLE            "kg_brain_snapshots"

//=============================================================================
// Snapshot Backend Selection
//=============================================================================

/**
 * @brief Snapshot storage backend selection
 *
 * WHAT: Controls where snapshots are stored
 * WHY:  Allow gradual migration from file-based to KG-based storage
 * HOW:  AUTO tries KG first, falls back to file
 */
typedef enum {
    SNAPSHOT_BACKEND_AUTO = 0,    /**< KG if available, else file (default) */
    SNAPSHOT_BACKEND_FILE,        /**< Force file-based storage */
    SNAPSHOT_BACKEND_KG           /**< Force KG storage (fail if unavailable) */
} snapshot_backend_t;

//=============================================================================
// KG Snapshot Info Structure
//=============================================================================

/**
 * @brief Extended snapshot metadata for KG-stored snapshots
 *
 * WHAT: Metadata for brain snapshots stored in KG
 * WHY:  Includes KG-specific fields like version, checkpoint labels, encryption info
 * HOW:  Extends base brain_snapshot_info_t with additional KG fields
 */
typedef struct {
    /* Base snapshot info (same as brain_snapshot_info_t) */
    char name[128];                          /**< Snapshot name */
    char description[512];                   /**< Human-readable description */
    uint64_t timestamp;                      /**< Creation timestamp (Unix epoch) */
    uint32_t file_size;                      /**< Original uncompressed size */
    bool is_compressed;                      /**< Compression enabled */
    bool is_encrypted;                       /**< Encryption enabled */

    /* KG-specific fields */
    char snapshot_id[KG_SNAPSHOT_MAX_ID_LEN];       /**< UUID of the snapshot */
    char brain_id[KG_SNAPSHOT_MAX_ID_LEN];          /**< Brain identifier */
    char kg_checkpoint_label[KG_SNAPSHOT_MAX_LABEL_LEN]; /**< Linked KG checkpoint */
    int encryption_algo;                            /**< Encryption algorithm (kg_crypto_algorithm_snapshot_t) */
    uint64_t version;                               /**< KG version at snapshot time */
    int format_version;                             /**< Snapshot format version */
    uint64_t compressed_size;                       /**< Compressed blob size */
    uint64_t neuron_count;                          /**< Number of neurons */
    uint64_t synapse_count;                         /**< Number of synapses */
    char key_id[KG_SNAPSHOT_MAX_ID_LEN];           /**< Encryption key identifier */
    bool hmac_verified;                             /**< HMAC verification status */
} brain_kg_snapshot_info_t;

//=============================================================================
// KG Snapshot Configuration
//=============================================================================

/**
 * @brief Configuration for KG snapshot operations
 *
 * WHAT: Settings for KG-based snapshot storage
 * WHY:  Allow fine-grained control over compression, encryption, and metadata
 */
typedef struct {
    bool enable_compression;                 /**< Compress before encryption (default: true) */
    int compression_level;                   /**< Compression level 1-9 (default: 6) */
    bool enable_encryption;                  /**< Encrypt snapshot data (default: true) */
    int encryption_algo;                     /**< Encryption algorithm (kg_crypto_algorithm_snapshot_t) */
    bool compute_hmac;                       /**< Compute HMAC for integrity (default: true) */
    bool link_to_kg_checkpoint;              /**< Create linked KG checkpoint (default: false) */
    uint32_t max_blob_size_mb;               /**< Maximum blob size in MB (default: 1024) */
} kg_snapshot_config_t;

//=============================================================================
// Default Configuration
//=============================================================================

/**
 * @brief Get default KG snapshot configuration
 *
 * @param config Output configuration structure (caller-allocated)
 * @return 0 on success, -1 on error
 */
int kg_snapshot_default_config(kg_snapshot_config_t* config);

//=============================================================================
// Core KG Snapshot API
//=============================================================================

/**
 * @brief Save brain snapshot to KG persistence layer
 *
 * WHAT: Persist brain state to QuestDB via KG persistence
 * WHY:  Unified storage with KG data, consistent encryption/audit
 * HOW:  Serialize -> compress -> encrypt -> HMAC -> write to kg_brain_snapshots
 *
 * @param brain Brain instance to snapshot (non-NULL)
 * @param name Snapshot name (non-NULL, e.g., "before_training_v1")
 * @param description Optional description (can be NULL)
 * @param p KG persistence context (non-NULL)
 * @return 0 on success, -1 on error
 */
int brain_save_snapshot_kg(brain_t brain, const char* name,
                           const char* description, kg_persistence_t* p);

/**
 * @brief Save brain snapshot with configuration
 *
 * WHAT: Persist brain state with custom configuration
 * WHY:  Allow control over compression, encryption settings
 *
 * @param brain Brain instance to snapshot
 * @param name Snapshot name
 * @param description Optional description
 * @param p KG persistence context
 * @param config Snapshot configuration (NULL for defaults)
 * @return 0 on success, -1 on error
 */
int brain_save_snapshot_kg_ex(brain_t brain, const char* name,
                              const char* description, kg_persistence_t* p,
                              const kg_snapshot_config_t* config);

/**
 * @brief Restore brain from KG snapshot
 *
 * WHAT: Load brain state from most recent snapshot with given name
 * WHY:  Restore previous state, rollback changes, A/B testing
 * HOW:  Query -> verify HMAC -> decrypt -> decompress -> deserialize
 *
 * @param name Snapshot name to restore (non-NULL)
 * @param p KG persistence context (non-NULL)
 * @return Brain instance with restored state, or NULL on error
 */
brain_t brain_restore_snapshot_kg(const char* name, kg_persistence_t* p);

/**
 * @brief Restore brain from specific snapshot ID
 *
 * WHAT: Load brain state from specific snapshot by UUID
 * WHY:  Restore specific version when multiple snapshots exist with same name
 *
 * @param snapshot_id UUID of snapshot to restore
 * @param p KG persistence context
 * @return Brain instance with restored state, or NULL on error
 */
brain_t brain_restore_snapshot_kg_by_id(const char* snapshot_id, kg_persistence_t* p);

/**
 * @brief List available snapshots in KG
 *
 * WHAT: Enumerate all snapshots stored in KG
 * WHY:  Allow users to see available restore points
 * HOW:  Query kg_brain_snapshots table, parse metadata
 *
 * @param p KG persistence context
 * @param infos Output array of snapshot info (caller-allocated)
 * @param max_count Maximum number of snapshots to return
 * @param out_count Output: actual number of snapshots found
 * @return 0 on success, -1 on error
 */
int brain_list_snapshots_kg(kg_persistence_t* p, brain_kg_snapshot_info_t* infos,
                            uint32_t max_count, uint32_t* out_count);

/**
 * @brief List snapshots for specific brain
 *
 * WHAT: Enumerate snapshots for a specific brain ID
 * WHY:  Filter snapshots when multiple brains share storage
 *
 * @param p KG persistence context
 * @param brain_id Brain identifier to filter by
 * @param infos Output array of snapshot info
 * @param max_count Maximum number of snapshots to return
 * @param out_count Output: actual number of snapshots found
 * @return 0 on success, -1 on error
 */
int brain_list_snapshots_kg_by_brain(kg_persistence_t* p, const char* brain_id,
                                     brain_kg_snapshot_info_t* infos,
                                     uint32_t max_count, uint32_t* out_count);

/**
 * @brief Delete snapshot from KG
 *
 * WHAT: Remove snapshot and associated data from KG
 * WHY:  Free storage space, remove obsolete restore points
 * HOW:  Delete from kg_brain_snapshots, audit log the operation
 *
 * @param name Snapshot name to delete (deletes most recent with this name)
 * @param p KG persistence context
 * @return 0 on success, -1 on error
 */
int brain_delete_snapshot_kg(const char* name, kg_persistence_t* p);

/**
 * @brief Delete snapshot by ID
 *
 * WHAT: Remove specific snapshot by UUID
 * WHY:  Delete specific version when multiple exist with same name
 *
 * @param snapshot_id UUID of snapshot to delete
 * @param p KG persistence context
 * @return 0 on success, -1 on error
 */
int brain_delete_snapshot_kg_by_id(const char* snapshot_id, kg_persistence_t* p);

//=============================================================================
// Linked Checkpoint API (Brain + KG Atomic)
//=============================================================================

/**
 * @brief Create linked checkpoint (brain + KG atomic)
 *
 * WHAT: Create atomic checkpoint of both brain state and KG state
 * WHY:  Ensure brain and knowledge graph can be restored together
 * HOW:  Create KG checkpoint, then brain snapshot linked to it
 *
 * @param brain Brain instance to checkpoint
 * @param label Checkpoint label (e.g., "pre-training-v1")
 * @param p KG persistence context
 * @return 0 on success, -1 on error
 */
int brain_create_linked_checkpoint(brain_t brain, const char* label,
                                   kg_persistence_t* p);

/**
 * @brief Restore from linked checkpoint
 *
 * WHAT: Restore both brain state and KG from linked checkpoint
 * WHY:  Atomic rollback of brain + knowledge graph
 * HOW:  Restore KG first, then restore brain snapshot
 *
 * @param brain Brain instance to restore into (can be NULL to create new)
 * @param label Checkpoint label to restore
 * @param p KG persistence context
 * @return Restored brain instance (new if brain was NULL), or NULL on error
 */
brain_t brain_restore_linked_checkpoint(brain_t brain, const char* label,
                                        kg_persistence_t* p);

/**
 * @brief List linked checkpoints
 *
 * WHAT: List all linked checkpoints (brain + KG pairs)
 * WHY:  Show available atomic restore points
 *
 * @param p KG persistence context
 * @param labels Output array of checkpoint labels (caller-allocated)
 * @param max_labels Maximum labels to return
 * @param max_label_len Maximum length per label
 * @param out_count Output: actual number of labels found
 * @return 0 on success, -1 on error
 */
int brain_list_linked_checkpoints(kg_persistence_t* p, char** labels,
                                  uint32_t max_labels, uint32_t max_label_len,
                                  uint32_t* out_count);

/**
 * @brief Delete linked checkpoint
 *
 * WHAT: Delete both brain snapshot and KG checkpoint
 * WHY:  Remove obsolete linked restore points
 *
 * @param label Checkpoint label to delete
 * @param p KG persistence context
 * @return 0 on success, -1 on error
 */
int brain_delete_linked_checkpoint(const char* label, kg_persistence_t* p);

//=============================================================================
// Snapshot Verification & Maintenance
//=============================================================================

/**
 * @brief Verify snapshot integrity
 *
 * WHAT: Check HMAC and decrypt test of snapshot
 * WHY:  Verify snapshot is not corrupted before restore
 * HOW:  Compute HMAC over stored blob, compare with stored HMAC
 *
 * @param snapshot_id UUID of snapshot to verify
 * @param p KG persistence context
 * @return 0 if valid, -1 if invalid or error
 */
int brain_verify_snapshot_kg(const char* snapshot_id, kg_persistence_t* p);

/**
 * @brief Get snapshot metadata
 *
 * WHAT: Retrieve metadata for specific snapshot
 * WHY:  Inspect snapshot without loading full brain state
 *
 * @param snapshot_id UUID of snapshot
 * @param p KG persistence context
 * @param info Output snapshot info
 * @return 0 on success, -1 on error
 */
int brain_get_snapshot_info_kg(const char* snapshot_id, kg_persistence_t* p,
                               brain_kg_snapshot_info_t* info);

/**
 * @brief Prune old snapshots
 *
 * WHAT: Delete snapshots older than specified age
 * WHY:  Automatic storage management
 *
 * @param p KG persistence context
 * @param max_age_days Delete snapshots older than this
 * @param deleted_count Output: number of snapshots deleted
 * @return 0 on success, -1 on error
 */
int brain_prune_snapshots_kg(kg_persistence_t* p, uint32_t max_age_days,
                             uint32_t* deleted_count);

//=============================================================================
// Schema Management
//=============================================================================

/**
 * @brief Create kg_brain_snapshots table if not exists
 *
 * WHAT: Initialize the brain snapshots table schema
 * WHY:  Called during kg_persistence_create() to ensure table exists
 * HOW:  Execute CREATE TABLE IF NOT EXISTS statement
 *
 * @param p KG persistence context
 * @return 0 on success, -1 on error
 */
int kg_snapshot_create_schema(kg_persistence_t* p);

//=============================================================================
// String Conversion Utilities
//=============================================================================

/**
 * @brief Convert snapshot backend to string
 *
 * @param backend Backend type
 * @return Static string representation
 */
const char* snapshot_backend_to_string(snapshot_backend_t backend);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_KG_SNAPSHOT_H */
