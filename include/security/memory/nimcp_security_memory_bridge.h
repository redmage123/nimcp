/**
 * @file nimcp_security_memory_bridge.h
 * @brief Security - Memory Systems Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-09
 *
 * WHAT: Bidirectional bridge integrating security controls with memory systems
 * WHY:  Protect sensitive information in working, episodic, semantic, and
 *       procedural memory from unauthorized access, leakage, and tampering
 * HOW:  Access control, encryption, classification, secure erase, and audit
 *       mechanisms applied at memory operation boundaries
 *
 * BIOLOGICAL BASIS:
 * - Blood-Brain Barrier (BBB) protects neural tissue from harmful agents
 * - Memory consolidation includes protective mechanisms
 * - Hippocampal gating controls information flow to long-term storage
 * - Prefrontal cortex regulates access to working memory contents
 *
 * SECURITY MODEL:
 * ```
 * +-------------------------------------------------------------------------+
 * |                    SECURITY-MEMORY BRIDGE ARCHITECTURE                   |
 * +-------------------------------------------------------------------------+
 * |                                                                         |
 * |   SECURITY LAYER                    MEMORY SYSTEMS                      |
 * |   +-----------------+              +-----------------+                  |
 * |   | Access Control  |<----------->| Working Memory  |                  |
 * |   | Classification  |<----------->| Episodic Memory |                  |
 * |   | Encryption      |<----------->| Semantic Memory |                  |
 * |   | Secure Erase    |<----------->| Procedural Mem  |                  |
 * |   | Audit           |<----------->| BBB (Security)  |                  |
 * |   +-----------------+              +-----------------+                  |
 * |          |                                  |                           |
 * |          v                                  v                           |
 * |   +---------------------------------------------+                       |
 * |   |           BIDIRECTIONAL EFFECTS             |                       |
 * |   | Security->Memory: Access masks, encryption  |                       |
 * |   | Memory->Security: Patterns, anomalies       |                       |
 * |   +---------------------------------------------+                       |
 * +-------------------------------------------------------------------------+
 * ```
 *
 * CLASSIFICATION HIERARCHY:
 * - PUBLIC: No restrictions, freely accessible
 * - INTERNAL: Organization-only, basic protection
 * - CONFIDENTIAL: Need-to-know basis, encrypted at rest
 * - SECRET: Highly restricted, encrypted in transit and at rest
 * - TOP_SECRET: Maximum protection, hardware security modules
 *
 * @see nimcp_blood_brain_barrier.h
 * @see nimcp_working_memory.h
 * @see nimcp_semantic_memory.h
 * @see nimcp_procedural.h
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_SECURITY_MEMORY_BRIDGE_H
#define NIMCP_SECURITY_MEMORY_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "utils/bridge/nimcp_bridge_base.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** @brief Maximum subjects tracked for access control */
#define SEC_MEM_MAX_SUBJECTS             256

/** @brief Maximum memory regions tracked */
#define SEC_MEM_MAX_REGIONS              1024

/** @brief Maximum audit log entries retained */
#define SEC_MEM_MAX_AUDIT_ENTRIES        4096

/** @brief Maximum encryption keys managed */
#define SEC_MEM_MAX_KEYS                 64

/** @brief Default key derivation iterations */
#define SEC_MEM_KEY_ITERATIONS           100000

/** @brief AES-256 key size in bytes */
#define SEC_MEM_AES256_KEY_SIZE          32

/** @brief GCM IV size in bytes */
#define SEC_MEM_GCM_IV_SIZE              12

/** @brief GCM tag size in bytes */
#define SEC_MEM_GCM_TAG_SIZE             16

/** @brief Bio-async module ID for security-memory bridge */
#define BIO_MODULE_SECURITY_MEMORY       0x0E10

/** @brief Leakage detection window in milliseconds */
#define SEC_MEM_LEAKAGE_WINDOW_MS        60000

/** @brief Access pattern history length */
#define SEC_MEM_PATTERN_HISTORY          100

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Memory data classification levels
 *
 * WHAT: Sensitivity levels for data stored in memory systems
 * WHY:  Determine appropriate security controls for each data item
 * HOW:  Higher classifications require stronger protections
 */
typedef enum {
    SEC_MEM_CLASS_PUBLIC = 0,        /**< No restrictions, freely accessible */
    SEC_MEM_CLASS_INTERNAL,          /**< Organization internal only */
    SEC_MEM_CLASS_CONFIDENTIAL,      /**< Need-to-know, encrypted at rest */
    SEC_MEM_CLASS_SECRET,            /**< Highly restricted, full encryption */
    SEC_MEM_CLASS_TOP_SECRET         /**< Maximum protection, HSM required */
} security_mem_classification_t;

/**
 * @brief Memory operation types for access control
 *
 * WHAT: Types of memory operations that can be controlled
 * WHY:  Fine-grained access control per operation type
 */
typedef enum {
    SEC_MEM_OP_READ = 0,             /**< Read data from memory */
    SEC_MEM_OP_WRITE,                /**< Write data to memory */
    SEC_MEM_OP_DELETE,               /**< Delete data from memory */
    SEC_MEM_OP_SHARE,                /**< Share data with other systems */
    SEC_MEM_OP_CONSOLIDATE,          /**< Memory consolidation operations */
    SEC_MEM_OP_RETRIEVE,             /**< Retrieval/recall operations */
    SEC_MEM_OP_ENCODE                /**< Encoding new memories */
} security_mem_operation_t;

/**
 * @brief Memory system types
 *
 * WHAT: Types of memory systems this bridge can connect to
 * WHY:  Different memory systems may have different security needs
 */
typedef enum {
    SEC_MEM_TYPE_WORKING = 0,        /**< Working memory (short-term) */
    SEC_MEM_TYPE_EPISODIC,           /**< Episodic memory (autobiographical) */
    SEC_MEM_TYPE_SEMANTIC,           /**< Semantic memory (facts/knowledge) */
    SEC_MEM_TYPE_PROCEDURAL          /**< Procedural memory (skills/habits) */
} security_mem_system_type_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    SEC_MEM_STATE_IDLE = 0,          /**< Bridge idle, no active operations */
    SEC_MEM_STATE_CHECKING,          /**< Performing access check */
    SEC_MEM_STATE_ENCRYPTING,        /**< Encrypting data */
    SEC_MEM_STATE_DECRYPTING,        /**< Decrypting data */
    SEC_MEM_STATE_ERASING,           /**< Secure erase in progress */
    SEC_MEM_STATE_AUDITING,          /**< Writing audit log */
    SEC_MEM_STATE_DETECTING,         /**< Running leakage detection */
    SEC_MEM_STATE_ERROR              /**< Error state */
} security_mem_state_t;

/**
 * @brief Leakage detection types
 *
 * WHAT: Types of information leakage that can be detected
 * WHY:  Identify specific security violations
 */
typedef enum {
    SEC_MEM_LEAK_NONE = 0,           /**< No leakage detected */
    SEC_MEM_LEAK_CLASSIFICATION,     /**< Data moved to lower classification */
    SEC_MEM_LEAK_UNAUTHORIZED,       /**< Unauthorized access pattern */
    SEC_MEM_LEAK_TIMING,             /**< Timing side-channel detected */
    SEC_MEM_LEAK_COVERT,             /**< Covert channel detected */
    SEC_MEM_LEAK_CROSS_MEMORY        /**< Improper cross-memory transfer */
} security_mem_leakage_t;

/**
 * @brief Audit event types
 */
typedef enum {
    SEC_MEM_AUDIT_ACCESS = 0,        /**< Memory access event */
    SEC_MEM_AUDIT_DENIED,            /**< Access denied event */
    SEC_MEM_AUDIT_ENCRYPT,           /**< Encryption event */
    SEC_MEM_AUDIT_DECRYPT,           /**< Decryption event */
    SEC_MEM_AUDIT_ERASE,             /**< Secure erase event */
    SEC_MEM_AUDIT_CLASSIFY,          /**< Classification change */
    SEC_MEM_AUDIT_LEAK_DETECT,       /**< Leakage detection event */
    SEC_MEM_AUDIT_ANOMALY            /**< Anomaly detected */
} security_mem_audit_type_t;

//=============================================================================
// Configuration Structure
//=============================================================================

/**
 * @brief Security-Memory bridge configuration
 *
 * WHAT: Configuration parameters for the bridge
 * WHY:  Control security features and behavior
 * HOW:  Enable/disable features, set thresholds and policies
 */
typedef struct {
    /* Feature enable flags */
    bool enable_access_control;       /**< Enable access control checks */
    bool enable_encryption;           /**< Enable encryption for sensitive data */
    bool enable_secure_erase;         /**< Enable secure deletion */
    bool enable_classification;       /**< Enable data classification */
    bool enable_audit;                /**< Enable audit logging */
    bool enable_leakage_detection;    /**< Enable information leakage detection */
    bool enable_anomaly_detection;    /**< Enable access pattern anomaly detection */

    /* Encryption settings */
    bool encrypt_at_rest;             /**< Encrypt data at rest */
    bool encrypt_in_transit;          /**< Encrypt data in transit */
    uint32_t min_encryption_class;    /**< Minimum classification requiring encryption */
    uint32_t key_rotation_interval_s; /**< Key rotation interval in seconds */

    /* Access control settings */
    uint32_t default_access_mask;     /**< Default access rights mask */
    bool require_mfa_for_secret;      /**< Require MFA for SECRET+ access */
    uint32_t max_failed_attempts;     /**< Max failed attempts before lockout */
    uint32_t lockout_duration_s;      /**< Lockout duration in seconds */

    /* Secure erase settings */
    uint32_t erase_passes;            /**< Number of overwrite passes */
    bool verify_erase;                /**< Verify erasure completion */

    /* Audit settings */
    bool audit_all_access;            /**< Audit all access (not just denials) */
    uint32_t audit_retention_days;    /**< Audit log retention period */

    /* Leakage detection settings */
    float leakage_threshold;          /**< Threshold for leakage alerts [0-1] */
    uint32_t pattern_window_size;     /**< Window size for pattern analysis */

    /* Sensitivity parameters */
    float security_sensitivity;       /**< Overall security sensitivity [0.5-2.0] */
    float memory_sensitivity;         /**< Memory protection sensitivity [0.5-2.0] */

    /* Bio-async integration */
    bool enable_bio_async;            /**< Enable bio-async callbacks */
} security_mem_config_t;

//=============================================================================
// Access Rights Structure
//=============================================================================

/**
 * @brief Memory access rights for a subject
 *
 * WHAT: Permissions granted to a subject for memory operations
 * WHY:  Fine-grained access control
 * HOW:  Bit flags for each operation type
 */
typedef struct {
    uint32_t subject_id;              /**< Subject identifier */
    bool can_read;                    /**< Permission to read */
    bool can_write;                   /**< Permission to write */
    bool can_delete;                  /**< Permission to delete */
    bool can_share;                   /**< Permission to share */
    bool can_consolidate;             /**< Permission to consolidate */
    bool can_retrieve;                /**< Permission to retrieve */
    bool can_encode;                  /**< Permission to encode */
    uint32_t max_classification;      /**< Maximum classification accessible */
    uint64_t valid_until;             /**< Rights expiration timestamp */
    uint32_t memory_systems_mask;     /**< Bitmask of accessible memory systems */
} security_mem_access_rights_t;

//=============================================================================
// Effects Structures
//=============================================================================

/**
 * @brief Security effects on memory systems
 *
 * WHAT: How security controls affect memory operations
 * WHY:  Track security impositions on memory behavior
 */
typedef struct {
    /* Access control effects */
    uint32_t current_access_mask;     /**< Current effective access mask */
    uint32_t blocked_operations;      /**< Count of blocked operations */
    bool lockout_active;              /**< Whether lockout is active */
    uint64_t lockout_expires;         /**< Lockout expiration timestamp */

    /* Encryption effects */
    bool encryption_required;         /**< Whether encryption is currently required */
    uint32_t encrypted_regions;       /**< Number of encrypted regions */
    float encryption_overhead_ms;     /**< Encryption latency overhead */

    /* Classification effects */
    security_mem_classification_t effective_min_class;  /**< Minimum classification enforced */
    uint32_t reclassified_items;      /**< Items reclassified this session */

    /* Performance impact */
    float security_latency_ms;        /**< Security processing latency */
    float throughput_reduction;       /**< Throughput reduction factor [0-1] */
} security_to_memory_effects_t;

/**
 * @brief Memory effects on security system
 *
 * WHAT: How memory behavior affects security
 * WHY:  Memory patterns inform security decisions
 */
typedef struct {
    /* Access patterns */
    float access_frequency;           /**< Recent access frequency (ops/sec) */
    float read_write_ratio;           /**< Ratio of reads to writes */
    uint32_t unique_accessors;        /**< Number of unique accessing subjects */
    float pattern_regularity;         /**< How regular access patterns are [0-1] */

    /* Anomaly indicators */
    bool anomaly_detected;            /**< Whether anomaly was detected */
    float anomaly_score;              /**< Anomaly severity score [0-1] */
    uint32_t anomaly_type;            /**< Type of anomaly detected */
    char anomaly_description[128];    /**< Human-readable anomaly description */

    /* Leakage indicators */
    security_mem_leakage_t leakage_type;  /**< Type of leakage detected */
    float leakage_confidence;         /**< Confidence in leakage detection [0-1] */
    uint64_t suspected_leak_time;     /**< Timestamp of suspected leak */

    /* Memory system health */
    float working_memory_load;        /**< Working memory utilization [0-1] */
    float consolidation_rate;         /**< Memory consolidation rate */
    uint32_t pending_encryptions;     /**< Encryptions pending */
} memory_to_security_effects_t;

//=============================================================================
// State and Statistics Structures
//=============================================================================

/**
 * @brief Bridge operational state
 */
typedef struct {
    security_mem_state_t state;       /**< Current operational state */
    uint64_t last_access_check;       /**< Timestamp of last access check */
    uint64_t last_encryption;         /**< Timestamp of last encryption */
    uint64_t last_audit;              /**< Timestamp of last audit entry */
    uint64_t last_leak_check;         /**< Timestamp of last leak detection */
    uint32_t active_sessions;         /**< Number of active security sessions */
    uint32_t pending_operations;      /**< Operations pending security check */
    bool all_systems_connected;       /**< All memory systems connected */
} security_mem_state_info_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    /* Access control stats */
    uint64_t total_access_checks;     /**< Total access checks performed */
    uint64_t access_granted;          /**< Number of accesses granted */
    uint64_t access_denied;           /**< Number of accesses denied */
    float mean_check_latency_us;      /**< Mean access check latency */

    /* Encryption stats */
    uint64_t total_encryptions;       /**< Total encryption operations */
    uint64_t total_decryptions;       /**< Total decryption operations */
    uint64_t bytes_encrypted;         /**< Total bytes encrypted */
    float mean_encrypt_latency_us;    /**< Mean encryption latency */

    /* Classification stats */
    uint64_t classifications_assigned; /**< Classifications assigned */
    uint64_t classification_upgrades; /**< Classifications upgraded */
    uint64_t classification_downgrades; /**< Classifications downgraded */

    /* Secure erase stats */
    uint64_t secure_erases;           /**< Secure erase operations */
    uint64_t bytes_erased;            /**< Total bytes securely erased */

    /* Leakage detection stats */
    uint64_t leak_checks;             /**< Leakage detection runs */
    uint64_t leaks_detected;          /**< Leakages detected */
    uint64_t false_positives;         /**< Known false positive leaks */

    /* Audit stats */
    uint64_t audit_entries;           /**< Total audit log entries */
    uint64_t audit_alerts;            /**< Audit alerts generated */

    /* Anomaly stats */
    uint64_t anomalies_detected;      /**< Anomalies detected */
    float mean_anomaly_score;         /**< Mean anomaly score */

    /* Per-memory system stats */
    uint64_t working_memory_ops;      /**< Operations on working memory */
    uint64_t episodic_memory_ops;     /**< Operations on episodic memory */
    uint64_t semantic_memory_ops;     /**< Operations on semantic memory */
    uint64_t procedural_memory_ops;   /**< Operations on procedural memory */
} security_mem_stats_t;

//=============================================================================
// Audit Entry Structure
//=============================================================================

/**
 * @brief Audit log entry
 */
typedef struct {
    uint64_t timestamp;               /**< Event timestamp */
    security_mem_audit_type_t type;   /**< Event type */
    uint32_t subject_id;              /**< Subject that performed action */
    security_mem_system_type_t memory_type;  /**< Memory system involved */
    security_mem_operation_t operation;      /**< Operation attempted */
    security_mem_classification_t data_class; /**< Data classification */
    bool success;                     /**< Whether operation succeeded */
    char details[256];                /**< Additional details */
} security_mem_audit_entry_t;

//=============================================================================
// Forward Declarations
//=============================================================================

/* Memory system types - forward declarations for flexibility */
typedef struct working_memory working_memory_t;
typedef struct episodic_memory_system episodic_memory_t;
typedef struct semantic_memory_system semantic_memory_system_t;
typedef struct procedural_memory_internal* procedural_memory_t;

/* BBB forward declaration */
typedef struct bbb_system_struct* bbb_system_t;

//=============================================================================
// Bridge Structure
//=============================================================================

/**
 * @brief Security-Memory bridge
 *
 * WHAT: Main bridge structure connecting security to memory systems
 * WHY:  Centralized security control for all memory operations
 * HOW:  Contains connections, effects, state, and configuration
 */
typedef struct security_mem_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    /* Configuration */
    security_mem_config_t config;     /**< Bridge configuration */

    /* Memory system connections */
    working_memory_t* working_memory;          /**< Connected working memory */
    episodic_memory_t* episodic_memory;        /**< Connected episodic memory */
    semantic_memory_system_t* semantic_memory; /**< Connected semantic memory */
    procedural_memory_t procedural_memory;     /**< Connected procedural memory */

    /* Security system connection */
    bbb_system_t bbb;                 /**< Connected Blood-Brain Barrier */

    /* Connection flags */
    bool working_connected;           /**< Working memory connected */
    bool episodic_connected;          /**< Episodic memory connected */
    bool semantic_connected;          /**< Semantic memory connected */
    bool procedural_connected;        /**< Procedural memory connected */
    bool bbb_connected;               /**< BBB connected */

    /* Bidirectional effects */
    security_to_memory_effects_t security_effects;  /**< Security->Memory effects */
    memory_to_security_effects_t memory_effects;    /**< Memory->Security effects */

    /* State and statistics */
    security_mem_state_info_t state;  /**< Current operational state */
    security_mem_stats_t stats;       /**< Operational statistics */

    /* Access control state */
    security_mem_access_rights_t* access_table;  /**< Access rights table */
    uint32_t num_subjects;            /**< Number of registered subjects */

    /* Audit log */
    security_mem_audit_entry_t* audit_log;  /**< Audit log buffer */
    uint32_t audit_log_head;          /**< Circular buffer head */
    uint32_t audit_log_count;         /**< Number of entries in log */
} security_mem_bridge_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Get default configuration
 *
 * WHAT: Returns sensible default configuration
 * WHY:  Starting point for most deployments
 *
 * @param config Output configuration structure
 * @return 0 on success, -1 on error
 *
 * Defaults:
 * - enable_access_control: true
 * - enable_encryption: true (for CONFIDENTIAL+)
 * - enable_secure_erase: true
 * - enable_classification: true
 * - enable_audit: true
 * - erase_passes: 3
 * - security_sensitivity: 1.0
 */
int security_memory_default_config(security_mem_config_t* config);

/**
 * @brief Create security-memory bridge
 *
 * WHAT: Allocates and initializes bridge
 * WHY:  Entry point for security-memory integration
 * HOW:  Allocates structures, initializes state, applies config
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 *
 * Memory: ~64KB for default configuration
 * Thread safety: Returned handle is thread-safe
 */
security_mem_bridge_t* security_memory_bridge_create(
    const security_mem_config_t* config
);

/**
 * @brief Destroy security-memory bridge
 *
 * WHAT: Releases all resources
 * WHY:  Clean shutdown
 * HOW:  Disconnects systems, frees memory, clears sensitive data
 *
 * @param bridge Bridge to destroy (NULL safe)
 *
 * Note: Performs secure erase of any cached encryption keys
 */
void security_memory_bridge_destroy(security_mem_bridge_t* bridge);

/**
 * @brief Reset bridge to initial state
 *
 * WHAT: Clears state while preserving connections
 * WHY:  Fresh start without reconnection
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int security_memory_bridge_reset(security_mem_bridge_t* bridge);

//=============================================================================
// Connection Functions
//=============================================================================

/**
 * @brief Connect working memory system
 *
 * @param bridge Bridge handle
 * @param working_memory Working memory system
 * @return 0 on success, -1 on error
 */
int security_memory_connect_working(
    security_mem_bridge_t* bridge,
    working_memory_t* working_memory
);

/**
 * @brief Connect episodic memory system
 *
 * @param bridge Bridge handle
 * @param episodic_memory Episodic memory system
 * @return 0 on success, -1 on error
 */
int security_memory_connect_episodic(
    security_mem_bridge_t* bridge,
    episodic_memory_t* episodic_memory
);

/**
 * @brief Connect semantic memory system
 *
 * @param bridge Bridge handle
 * @param semantic_memory Semantic memory system
 * @return 0 on success, -1 on error
 */
int security_memory_connect_semantic(
    security_mem_bridge_t* bridge,
    semantic_memory_system_t* semantic_memory
);

/**
 * @brief Connect procedural memory system
 *
 * @param bridge Bridge handle
 * @param procedural_memory Procedural memory system
 * @return 0 on success, -1 on error
 */
int security_memory_connect_procedural(
    security_mem_bridge_t* bridge,
    procedural_memory_t procedural_memory
);

/**
 * @brief Connect Blood-Brain Barrier system
 *
 * @param bridge Bridge handle
 * @param bbb BBB system
 * @return 0 on success, -1 on error
 */
int security_memory_connect_bbb(
    security_mem_bridge_t* bridge,
    bbb_system_t bbb
);

/**
 * @brief Disconnect all systems
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int security_memory_disconnect_all(security_mem_bridge_t* bridge);

/**
 * @brief Check if all memory systems are connected
 *
 * @param bridge Bridge handle
 * @return true if all connected, false otherwise
 */
bool security_memory_is_fully_connected(const security_mem_bridge_t* bridge);

//=============================================================================
// Access Control Functions
//=============================================================================

/**
 * @brief Check access rights for memory operation
 *
 * WHAT: Verifies if subject can perform operation on data
 * WHY:  Enforce access control before memory operations
 * HOW:  Check rights, classification, lockout status
 *
 * @param bridge Bridge handle
 * @param subject_id Subject requesting access
 * @param memory_type Target memory system
 * @param operation Requested operation
 * @param data_classification Classification of target data
 * @return true if access granted, false if denied
 *
 * Side effects:
 * - Updates access statistics
 * - May trigger lockout on repeated failures
 * - Creates audit entry if auditing enabled
 */
bool security_memory_check_access(
    security_mem_bridge_t* bridge,
    uint32_t subject_id,
    security_mem_system_type_t memory_type,
    security_mem_operation_t operation,
    security_mem_classification_t data_classification
);

/**
 * @brief Register subject with access rights
 *
 * @param bridge Bridge handle
 * @param rights Access rights to register
 * @return 0 on success, -1 on error
 */
int security_memory_register_subject(
    security_mem_bridge_t* bridge,
    const security_mem_access_rights_t* rights
);

/**
 * @brief Update subject access rights
 *
 * @param bridge Bridge handle
 * @param rights Updated access rights
 * @return 0 on success, -1 on error
 */
int security_memory_update_rights(
    security_mem_bridge_t* bridge,
    const security_mem_access_rights_t* rights
);

/**
 * @brief Revoke subject access rights
 *
 * @param bridge Bridge handle
 * @param subject_id Subject to revoke
 * @return 0 on success, -1 on error
 */
int security_memory_revoke_subject(
    security_mem_bridge_t* bridge,
    uint32_t subject_id
);

/**
 * @brief Get access rights for subject
 *
 * @param bridge Bridge handle
 * @param subject_id Subject to query
 * @param rights_out Output rights structure
 * @return 0 on success, -1 on error
 */
int security_memory_get_rights(
    const security_mem_bridge_t* bridge,
    uint32_t subject_id,
    security_mem_access_rights_t* rights_out
);

//=============================================================================
// Classification Functions
//=============================================================================

/**
 * @brief Assign classification level to data
 *
 * WHAT: Labels data with security classification
 * WHY:  Determine appropriate protection level
 * HOW:  Analyze data content and context, assign label
 *
 * @param bridge Bridge handle
 * @param data_ptr Pointer to data (for content analysis)
 * @param data_size Size of data
 * @param context_hint Hint about data context (can be NULL)
 * @param classification_out Output classification level
 * @return 0 on success, -1 on error
 *
 * Classification factors:
 * - Content sensitivity (keywords, patterns)
 * - Source system sensitivity
 * - Context hints
 * - Organization policies
 */
int security_memory_classify_data(
    security_mem_bridge_t* bridge,
    const void* data_ptr,
    size_t data_size,
    const char* context_hint,
    security_mem_classification_t* classification_out
);

/**
 * @brief Manually set classification for memory region
 *
 * @param bridge Bridge handle
 * @param memory_type Memory system containing region
 * @param region_id Region identifier
 * @param classification Classification to assign
 * @return 0 on success, -1 on error
 */
int security_memory_set_classification(
    security_mem_bridge_t* bridge,
    security_mem_system_type_t memory_type,
    uint64_t region_id,
    security_mem_classification_t classification
);

/**
 * @brief Get classification for memory region
 *
 * @param bridge Bridge handle
 * @param memory_type Memory system
 * @param region_id Region identifier
 * @param classification_out Output classification
 * @return 0 on success, -1 on error
 */
int security_memory_get_classification(
    const security_mem_bridge_t* bridge,
    security_mem_system_type_t memory_type,
    uint64_t region_id,
    security_mem_classification_t* classification_out
);

//=============================================================================
// Encryption Functions
//=============================================================================

/**
 * @brief Encrypt sensitive data before storage
 *
 * WHAT: Encrypts data for secure storage in memory
 * WHY:  Protect confidential data at rest
 * HOW:  AES-256-GCM encryption with key derived from classification
 *
 * @param bridge Bridge handle
 * @param plaintext Input data to encrypt
 * @param plaintext_size Size of plaintext
 * @param classification Data classification (determines key)
 * @param ciphertext_out Output buffer for ciphertext
 * @param ciphertext_size Size of output buffer
 * @param bytes_written_out Actual bytes written
 * @return 0 on success, -1 on error
 *
 * Output format: IV (12) | ciphertext | tag (16)
 */
int security_memory_encrypt_sensitive(
    security_mem_bridge_t* bridge,
    const void* plaintext,
    size_t plaintext_size,
    security_mem_classification_t classification,
    void* ciphertext_out,
    size_t ciphertext_size,
    size_t* bytes_written_out
);

/**
 * @brief Decrypt sensitive data for authorized access
 *
 * WHAT: Decrypts data for authorized retrieval
 * WHY:  Enable access to encrypted data by authorized subjects
 * HOW:  AES-256-GCM decryption with authentication
 *
 * @param bridge Bridge handle
 * @param ciphertext Encrypted data
 * @param ciphertext_size Size of ciphertext
 * @param classification Data classification
 * @param subject_id Subject requesting decryption
 * @param plaintext_out Output buffer for plaintext
 * @param plaintext_size Size of output buffer
 * @param bytes_written_out Actual bytes written
 * @return 0 on success, -1 on error (including auth failure)
 */
int security_memory_decrypt_sensitive(
    security_mem_bridge_t* bridge,
    const void* ciphertext,
    size_t ciphertext_size,
    security_mem_classification_t classification,
    uint32_t subject_id,
    void* plaintext_out,
    size_t plaintext_size,
    size_t* bytes_written_out
);

/**
 * @brief Rotate encryption keys
 *
 * WHAT: Generate new encryption keys and re-encrypt data
 * WHY:  Periodic key rotation for security
 * HOW:  Generate new keys, re-encrypt affected regions
 *
 * @param bridge Bridge handle
 * @param classification Classification level to rotate
 * @return 0 on success, -1 on error
 */
int security_memory_rotate_keys(
    security_mem_bridge_t* bridge,
    security_mem_classification_t classification
);

//=============================================================================
// Secure Erase Functions
//=============================================================================

/**
 * @brief Securely erase sensitive data
 *
 * WHAT: Permanently and unrecoverably delete data
 * WHY:  Prevent recovery of deleted sensitive information
 * HOW:  Multi-pass overwrite with verification
 *
 * @param bridge Bridge handle
 * @param memory_type Memory system containing data
 * @param region_id Region identifier to erase
 * @param verify Whether to verify erasure
 * @return 0 on success, -1 on error
 *
 * Erase process:
 * 1. Overwrite with zeros
 * 2. Overwrite with ones
 * 3. Overwrite with random data
 * 4. Verify if requested
 */
int security_memory_secure_erase(
    security_mem_bridge_t* bridge,
    security_mem_system_type_t memory_type,
    uint64_t region_id,
    bool verify
);

/**
 * @brief Securely erase memory region by pointer
 *
 * @param bridge Bridge handle
 * @param data_ptr Pointer to data to erase
 * @param data_size Size of data
 * @return 0 on success, -1 on error
 */
int security_memory_secure_erase_ptr(
    security_mem_bridge_t* bridge,
    void* data_ptr,
    size_t data_size
);

/**
 * @brief Securely erase all data of a classification
 *
 * @param bridge Bridge handle
 * @param classification Classification to erase
 * @return Number of regions erased, -1 on error
 */
int security_memory_secure_erase_classification(
    security_mem_bridge_t* bridge,
    security_mem_classification_t classification
);

//=============================================================================
// Leakage Detection Functions
//=============================================================================

/**
 * @brief Detect unauthorized information flows
 *
 * WHAT: Analyze memory operations for potential leakage
 * WHY:  Identify data exfiltration or improper handling
 * HOW:  Pattern analysis, taint tracking, timing analysis
 *
 * @param bridge Bridge handle
 * @param leakage_out Output leakage type if detected
 * @param confidence_out Output confidence level
 * @param details_out Output buffer for details (can be NULL)
 * @param details_size Size of details buffer
 * @return true if leakage detected, false otherwise
 *
 * Detection methods:
 * - Classification downgrade tracking
 * - Unusual access patterns
 * - Cross-memory system transfers
 * - Timing anomalies
 */
bool security_memory_detect_leakage(
    security_mem_bridge_t* bridge,
    security_mem_leakage_t* leakage_out,
    float* confidence_out,
    char* details_out,
    size_t details_size
);

/**
 * @brief Mark data transfer as legitimate
 *
 * WHAT: Whitelist a specific data flow
 * WHY:  Prevent false positive leakage alerts
 *
 * @param bridge Bridge handle
 * @param source_type Source memory system
 * @param dest_type Destination memory system
 * @param subject_id Subject performing transfer
 * @return 0 on success, -1 on error
 */
int security_memory_whitelist_transfer(
    security_mem_bridge_t* bridge,
    security_mem_system_type_t source_type,
    security_mem_system_type_t dest_type,
    uint32_t subject_id
);

//=============================================================================
// Audit Functions
//=============================================================================

/**
 * @brief Log memory access for audit
 *
 * WHAT: Record memory operation in audit log
 * WHY:  Compliance, forensics, monitoring
 * HOW:  Create timestamped entry with operation details
 *
 * @param bridge Bridge handle
 * @param subject_id Subject performing operation
 * @param memory_type Memory system accessed
 * @param operation Operation performed
 * @param data_class Data classification
 * @param success Whether operation succeeded
 * @param details Additional details (can be NULL)
 * @return 0 on success, -1 on error
 */
int security_memory_audit_access(
    security_mem_bridge_t* bridge,
    uint32_t subject_id,
    security_mem_system_type_t memory_type,
    security_mem_operation_t operation,
    security_mem_classification_t data_class,
    bool success,
    const char* details
);

/**
 * @brief Get recent audit entries
 *
 * @param bridge Bridge handle
 * @param entries_out Output buffer for entries
 * @param max_entries Maximum entries to return
 * @param count_out Actual entries returned
 * @return 0 on success, -1 on error
 */
int security_memory_get_audit_log(
    const security_mem_bridge_t* bridge,
    security_mem_audit_entry_t* entries_out,
    size_t max_entries,
    size_t* count_out
);

/**
 * @brief Clear audit log
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 *
 * Note: May create final audit entry before clearing
 */
int security_memory_clear_audit_log(security_mem_bridge_t* bridge);

/**
 * @brief Export audit log to file
 *
 * @param bridge Bridge handle
 * @param filepath Output file path
 * @return 0 on success, -1 on error
 */
int security_memory_export_audit_log(
    const security_mem_bridge_t* bridge,
    const char* filepath
);

//=============================================================================
// Bidirectional Update Functions
//=============================================================================

/**
 * @brief Update bridge state (main update loop)
 *
 * WHAT: Process pending operations and update effects
 * WHY:  Maintain bridge synchronization
 * HOW:  Process queued checks, update effects, run detection
 *
 * @param bridge Bridge handle
 * @param delta_ms Time since last update in milliseconds
 * @return 0 on success, -1 on error
 *
 * Call frequency: Recommended 10-100ms intervals
 */
int security_memory_bridge_update(
    security_mem_bridge_t* bridge,
    uint64_t delta_ms
);

/**
 * @brief Apply security effects to memory systems
 *
 * WHAT: Propagate security state to memory systems
 * WHY:  Enforce security controls on memory behavior
 * HOW:  Update access masks, trigger encryptions, apply policies
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int security_memory_apply_security_effects(security_mem_bridge_t* bridge);

/**
 * @brief Gather memory effects for security analysis
 *
 * WHAT: Collect memory behavior data for security
 * WHY:  Inform security decisions based on memory patterns
 * HOW:  Query memory systems, analyze patterns, detect anomalies
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int security_memory_gather_memory_effects(security_mem_bridge_t* bridge);

//=============================================================================
// Query Functions
//=============================================================================

/**
 * @brief Get security effects on memory
 *
 * @param bridge Bridge handle
 * @param effects_out Output effects structure
 * @return 0 on success, -1 on error
 */
int security_memory_get_security_effects(
    const security_mem_bridge_t* bridge,
    security_to_memory_effects_t* effects_out
);

/**
 * @brief Get memory effects on security
 *
 * @param bridge Bridge handle
 * @param effects_out Output effects structure
 * @return 0 on success, -1 on error
 */
int security_memory_get_memory_effects(
    const security_mem_bridge_t* bridge,
    memory_to_security_effects_t* effects_out
);

/**
 * @brief Get bridge state information
 *
 * @param bridge Bridge handle
 * @param state_out Output state structure
 * @return 0 on success, -1 on error
 */
int security_memory_get_state(
    const security_mem_bridge_t* bridge,
    security_mem_state_info_t* state_out
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats_out Output statistics structure
 * @return 0 on success, -1 on error
 */
int security_memory_get_stats(
    const security_mem_bridge_t* bridge,
    security_mem_stats_t* stats_out
);

/**
 * @brief Reset statistics counters
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int security_memory_reset_stats(security_mem_bridge_t* bridge);

//=============================================================================
// Bio-Async Integration
//=============================================================================

/**
 * @brief Connect to bio-async router
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int security_memory_connect_bio_async(security_mem_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int security_memory_disconnect_bio_async(security_mem_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 *
 * @param bridge Bridge handle
 * @return true if connected, false otherwise
 */
bool security_memory_is_bio_async_connected(const security_mem_bridge_t* bridge);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get classification level name
 *
 * @param classification Classification level
 * @return Human-readable name
 */
const char* security_memory_classification_name(
    security_mem_classification_t classification
);

/**
 * @brief Get operation type name
 *
 * @param operation Operation type
 * @return Human-readable name
 */
const char* security_memory_operation_name(security_mem_operation_t operation);

/**
 * @brief Get memory system type name
 *
 * @param mem_type Memory system type
 * @return Human-readable name
 */
const char* security_memory_system_name(security_mem_system_type_t mem_type);

/**
 * @brief Get bridge state name
 *
 * @param state Bridge state
 * @return Human-readable name
 */
const char* security_memory_state_name(security_mem_state_t state);

/**
 * @brief Get leakage type name
 *
 * @param leakage Leakage type
 * @return Human-readable name
 */
const char* security_memory_leakage_name(security_mem_leakage_t leakage);

/**
 * @brief Get audit event type name
 *
 * @param audit_type Audit event type
 * @return Human-readable name
 */
const char* security_memory_audit_type_name(security_mem_audit_type_t audit_type);

/**
 * @brief Print bridge state summary (debug)
 *
 * @param bridge Bridge handle
 */
void security_memory_print_summary(const security_mem_bridge_t* bridge);

/**
 * @brief Print statistics (debug)
 *
 * @param stats Statistics to print
 */
void security_memory_print_stats(const security_mem_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SECURITY_MEMORY_BRIDGE_H */
