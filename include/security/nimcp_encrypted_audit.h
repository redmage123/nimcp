/**
 * @file nimcp_encrypted_audit.h
 * @brief Encrypted Circular Buffer for Security Audit Logs
 *
 * WHAT: Tamper-proof encrypted audit log with AES-256-GCM
 * WHY:  Protect audit logs from unauthorized access and tampering
 * HOW:  Per-entry encryption with unique nonces, authenticated encryption
 *
 * FEATURES:
 * - AES-256-GCM authenticated encryption (libsodium)
 * - Per-entry unique nonce (never reused)
 * - Key rotation support
 * - Tampering detection via authentication tags
 * - Efficient circular buffer with encryption
 * - Secure memory handling (key protection)
 * - Batch decryption for reading
 * - Export encrypted logs to file
 * - Optional compression before encryption
 *
 * SECURITY PROPERTIES:
 * - Confidentiality: AES-256 encryption
 * - Integrity: GCM authentication tags
 * - Freshness: Unique nonces per entry
 * - Non-repudiation: Timestamped entries
 *
 * ARCHITECTURE:
 * ┌─────────────────────────────────────────────────────────────┐
 * │                  Encrypted Audit Buffer                     │
 * ├─────────────────────────────────────────────────────────────┤
 * │  ┌──────────┐    ┌──────────┐    ┌─────────────────────┐   │
 * │  │  Entry   │───▶│ Compress │───▶│  Encrypt (AES-GCM)  │   │
 * │  │ (Plain)  │    │(Optional)│    │  + Unique Nonce     │   │
 * │  └──────────┘    └──────────┘    └──────────┬──────────┘   │
 * │                                              │              │
 * │                                   ┌──────────▼──────────┐   │
 * │                                   │  Circular Buffer    │   │
 * │                                   │  (Encrypted Data)   │   │
 * │                                   └──────────┬──────────┘   │
 * │                                              │              │
 * │  ┌──────────┐    ┌──────────┐    ┌──────────▼──────────┐   │
 * │  │  Entry   │◀───│Decompress│◀───│  Decrypt + Verify   │   │
 * │  │ (Plain)  │    │(Optional)│    │  Auth Tag           │   │
 * │  └──────────┘    └──────────┘    └─────────────────────┘   │
 * └─────────────────────────────────────────────────────────────┘
 *
 * USAGE:
 * ```c
 * // Initialize with master key
 * uint8_t master_key[32];
 * nimcp_encryption_generate_key(master_key);
 *
 * nimcp_encrypted_audit_config_t config = nimcp_encrypted_audit_default_config();
 * nimcp_encrypted_audit_t audit = nimcp_encrypted_audit_create(&config, master_key, 32);
 *
 * // Log encrypted entry
 * nimcp_encrypted_audit_log(audit,
 *     NIMCP_AUDIT_CRITICAL,
 *     NIMCP_AUDIT_AUTHENTICATION,
 *     "Failed login attempt from 192.168.1.100",
 *     &login_data, sizeof(login_data));
 *
 * // Read entries (decrypts on-the-fly)
 * nimcp_audit_entry_t entries[100];
 * size_t num_entries;
 * nimcp_encrypted_audit_read(audit, entries, 100, &num_entries);
 *
 * // Rotate key periodically
 * uint8_t new_key[32];
 * nimcp_encryption_generate_key(new_key);
 * nimcp_encrypted_audit_rotate_key(audit, new_key, 32);
 *
 * // Export encrypted logs
 * nimcp_encrypted_audit_export(audit, "/var/log/nimcp/audit_encrypted.bin");
 * ```
 *
 * THREAT MODEL:
 * - Attacker cannot read logs without key
 * - Attacker cannot modify logs without detection (auth tags fail)
 * - Attacker cannot replay old entries (unique nonces)
 * - Keys protected in memory (mlock, secure zero on free)
 *
 * @author NIMCP Security Team
 * @date 2025-12-07
 * @version 1.0.0
 */

#ifndef NIMCP_ENCRYPTED_AUDIT_H
#define NIMCP_ENCRYPTED_AUDIT_H

#include "utils/validation/nimcp_common.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

#define NIMCP_ENCRYPTED_AUDIT_MAGIC 0x45415544       /**< 'EAUD' */
#define NIMCP_ENCRYPTED_AUDIT_VERSION 1              /**< Format version */
#define NIMCP_ENCRYPTED_AUDIT_FILE_MAGIC 0x4E414C4F  /**< 'NALO' - File export format magic */

// Encryption constants (AES-256-GCM with libsodium)
#define NIMCP_AUDIT_KEY_SIZE 32                      /**< 256-bit key */
#define NIMCP_AUDIT_NONCE_SIZE 12                    /**< 96-bit nonce (GCM standard) */
#define NIMCP_AUDIT_TAG_SIZE 16                      /**< 128-bit auth tag */

#define NIMCP_AUDIT_DEFAULT_BUFFER_SIZE 10000        /**< Default entries */
#define NIMCP_AUDIT_DEFAULT_ENTRY_SIZE 4096          /**< Default max entry size */
#define NIMCP_AUDIT_DEFAULT_KEY_ROTATION 100000      /**< Rotate every 100k entries */
#define NIMCP_AUDIT_MAX_MESSAGE_SIZE 8192            /**< Maximum message length */
#define NIMCP_AUDIT_MAX_DATA_SIZE 16384              /**< Maximum auxiliary data size */

//=============================================================================
// Type Definitions
//=============================================================================

/**
 * @brief Opaque encrypted audit buffer handle
 */
typedef struct nimcp_encrypted_audit_impl* nimcp_encrypted_audit_t;

/**
 * @brief Audit severity levels
 */
typedef enum {
    NIMCP_AUDIT_DEBUG = 0,       /**< Debug information */
    NIMCP_AUDIT_INFO,            /**< Informational */
    NIMCP_AUDIT_WARNING,         /**< Warning condition */
    NIMCP_AUDIT_ERROR,           /**< Error condition */
    NIMCP_AUDIT_CRITICAL,        /**< Critical security event */
    NIMCP_AUDIT_EMERGENCY,       /**< System emergency */
    NIMCP_AUDIT_SEVERITY_COUNT
} nimcp_audit_severity_t;

/**
 * @brief Audit event categories
 */
typedef enum {
    NIMCP_AUDIT_AUTHENTICATION = 0,  /**< Login/logout events */
    NIMCP_AUDIT_AUTHORIZATION,       /**< Access control events */
    NIMCP_AUDIT_DATA_ACCESS,         /**< Data read/write events */
    NIMCP_AUDIT_CONFIGURATION,       /**< Configuration changes */
    NIMCP_AUDIT_NETWORK,             /**< Network events */
    NIMCP_AUDIT_SYSTEM,              /**< System events */
    NIMCP_AUDIT_APPLICATION,         /**< Application events */
    NIMCP_AUDIT_THREAT,              /**< Security threats detected */
    NIMCP_AUDIT_ENCRYPTION,          /**< Encryption/key events */
    NIMCP_AUDIT_PATTERN,             /**< Pattern database events */
    NIMCP_AUDIT_CUSTOM,              /**< Custom events */
    NIMCP_AUDIT_CATEGORY_COUNT
} nimcp_audit_category_t;

/**
 * @brief Audit entry (plaintext representation)
 *
 * WHAT: Single audit log entry before encryption
 * WHY:  Standard format for audit events
 * HOW:  Encrypted as a whole with AES-GCM
 */
typedef struct {
    uint64_t timestamp_ns;               /**< Nanosecond timestamp */
    nimcp_audit_severity_t severity;     /**< Severity level */
    nimcp_audit_category_t category;     /**< Event category */
    uint32_t entry_id;                   /**< Sequential entry ID */
    uint32_t key_version;                /**< Key version used */
    char message[NIMCP_AUDIT_MAX_MESSAGE_SIZE];  /**< Event message */
    uint8_t data[NIMCP_AUDIT_MAX_DATA_SIZE];     /**< Auxiliary data */
    size_t data_len;                     /**< Auxiliary data length */
} nimcp_audit_entry_t;

/**
 * @brief Encrypted audit entry (on-disk/in-buffer format)
 *
 * WHAT: Encrypted representation of audit entry
 * WHY:  Protect confidentiality and integrity
 * HOW:  AES-256-GCM with unique nonce and auth tag
 */
typedef struct {
    uint8_t nonce[NIMCP_AUDIT_NONCE_SIZE];       /**< Unique nonce */
    uint8_t tag[NIMCP_AUDIT_TAG_SIZE];           /**< Authentication tag */
    uint32_t ciphertext_len;                     /**< Ciphertext length */
    uint8_t ciphertext[];                        /**< Encrypted entry data */
} nimcp_encrypted_audit_entry_t;

/**
 * @brief Encrypted audit configuration
 */
typedef struct {
    size_t buffer_size;                  /**< Number of entries in circular buffer */
    size_t max_entry_size;               /**< Maximum size per entry (bytes) */
    uint32_t key_rotation_interval;      /**< Entries before automatic key rotation (0 = never) */
    bool enable_compression;             /**< Compress before encryption (LZ4) */
    bool enable_bio_async;               /**< Enable bio-async integration */
    bool lock_memory;                    /**< Use mlock() to prevent swapping */
    bool secure_erase;                   /**< Securely erase entries on overwrite */
    bio_module_id_t module_id;           /**< Module ID for bio-async */
    float export_batch_size;             /**< Entries per export batch */
} nimcp_encrypted_audit_config_t;

/**
 * @brief Encrypted audit statistics
 */
typedef struct {
    uint64_t total_entries;              /**< Total entries logged */
    uint64_t entries_encrypted;          /**< Entries successfully encrypted */
    uint64_t entries_decrypted;          /**< Entries successfully decrypted */
    uint64_t encryption_failures;        /**< Failed encryptions */
    uint64_t decryption_failures;        /**< Failed decryptions */
    uint64_t tampering_detected;         /**< Auth tag verification failures */
    uint64_t key_rotations;              /**< Number of key rotations */
    uint64_t buffer_wraps;               /**< Times buffer wrapped around */
    uint32_t current_key_version;        /**< Current key version */
    uint32_t oldest_entry_id;            /**< Oldest entry in buffer */
    uint32_t newest_entry_id;            /**< Newest entry in buffer */
    size_t memory_usage_bytes;           /**< Total memory usage */
    float avg_encryption_time_us;        /**< Average encryption time */
    float avg_decryption_time_us;        /**< Average decryption time */
    float compression_ratio;             /**< Compression ratio (if enabled) */
} nimcp_encrypted_audit_stats_t;

/**
 * @brief Key rotation policy
 */
typedef enum {
    NIMCP_KEY_ROTATION_MANUAL = 0,       /**< Manual rotation only */
    NIMCP_KEY_ROTATION_COUNT,            /**< Rotate after N entries */
    NIMCP_KEY_ROTATION_TIME,             /**< Rotate after time interval */
    NIMCP_KEY_ROTATION_SIZE              /**< Rotate after data size */
} nimcp_key_rotation_policy_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Get default encrypted audit configuration
 *
 * WHAT: Returns sensible default configuration
 * WHY:  Simplifies initialization
 * HOW:  Static defaults tuned for security and performance
 *
 * @return Default configuration structure
 */
nimcp_encrypted_audit_config_t nimcp_encrypted_audit_default_config(void);

/**
 * @brief Create encrypted audit buffer
 *
 * WHAT: Initialize encrypted audit buffer with master key
 * WHY:  Required before logging any audit events
 * HOW:  Allocates buffer, derives encryption keys, sets up crypto
 *
 * SECURITY NOTE: master_key is copied and original should be securely erased
 *
 * @param config Configuration (NULL for defaults)
 * @param master_key Encryption master key (must be NIMCP_AUDIT_KEY_SIZE bytes)
 * @param key_len Key length (must be NIMCP_AUDIT_KEY_SIZE)
 * @return Audit buffer handle or NULL on failure
 */
nimcp_encrypted_audit_t nimcp_encrypted_audit_create(
    const nimcp_encrypted_audit_config_t* config,
    const uint8_t* master_key,
    size_t key_len
);

/**
 * @brief Destroy encrypted audit buffer
 *
 * WHAT: Free all resources and securely erase keys
 * WHY:  Prevent memory leaks and key exposure
 * HOW:  Secure zero of keys, munlock memory, free buffers
 *
 * @param audit Audit buffer handle
 */
void nimcp_encrypted_audit_destroy(nimcp_encrypted_audit_t audit);

//=============================================================================
// Logging Functions
//=============================================================================

/**
 * @brief Log audit entry
 *
 * WHAT: Add encrypted audit entry to circular buffer
 * WHY:  Core audit logging function
 * HOW:  Compress (optional) → Encrypt (AES-GCM) → Store in buffer
 *
 * ENCRYPTION PROCESS:
 * 1. Generate unique nonce (counter + random)
 * 2. Optionally compress plaintext (LZ4)
 * 3. Encrypt with AES-256-GCM
 * 4. Verify authentication tag
 * 5. Store encrypted entry in circular buffer
 *
 * THREAD SAFETY: Thread-safe (internal locking)
 *
 * @param audit Audit buffer handle
 * @param severity Severity level
 * @param category Event category
 * @param message Human-readable message
 * @param data Auxiliary binary data (can be NULL)
 * @param data_len Length of auxiliary data
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_encrypted_audit_log(
    nimcp_encrypted_audit_t audit,
    nimcp_audit_severity_t severity,
    nimcp_audit_category_t category,
    const char* message,
    const void* data,
    size_t data_len
);

/**
 * @brief Log audit entry with timestamp
 *
 * WHAT: Log entry with explicit timestamp
 * WHY:  Support backdated or replayed events
 * HOW:  Uses provided timestamp instead of current time
 *
 * @param audit Audit buffer handle
 * @param timestamp_ns Nanosecond timestamp
 * @param severity Severity level
 * @param category Event category
 * @param message Message
 * @param data Auxiliary data
 * @param data_len Data length
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_encrypted_audit_log_timestamped(
    nimcp_encrypted_audit_t audit,
    uint64_t timestamp_ns,
    nimcp_audit_severity_t severity,
    nimcp_audit_category_t category,
    const char* message,
    const void* data,
    size_t data_len
);

/**
 * @brief Log formatted audit entry
 *
 * WHAT: Log entry with printf-style formatting
 * WHY:  Convenient formatted logging
 * HOW:  vsnprintf formatting before encryption
 *
 * @param audit Audit buffer handle
 * @param severity Severity level
 * @param category Event category
 * @param format Printf-style format string
 * @param ... Format arguments
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_encrypted_audit_logf(
    nimcp_encrypted_audit_t audit,
    nimcp_audit_severity_t severity,
    nimcp_audit_category_t category,
    const char* format,
    ...
) __attribute__((format(printf, 4, 5)));

//=============================================================================
// Reading Functions
//=============================================================================

/**
 * @brief Read and decrypt audit entries
 *
 * WHAT: Read entries from buffer with decryption
 * WHY:  Access audit logs for analysis
 * HOW:  Batch decryption with authentication verification
 *
 * DECRYPTION PROCESS:
 * 1. Read encrypted entries from buffer
 * 2. Verify authentication tags
 * 3. Decrypt with AES-256-GCM
 * 4. Optionally decompress
 * 5. Return plaintext entries
 *
 * SECURITY: Fails entire batch if any auth tag invalid
 *
 * @param audit Audit buffer handle
 * @param entries Output buffer for decrypted entries
 * @param max_entries Maximum entries to read
 * @param num_entries Output: actual number of entries read
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_encrypted_audit_read(
    nimcp_encrypted_audit_t audit,
    nimcp_audit_entry_t* entries,
    size_t max_entries,
    size_t* num_entries
);

/**
 * @brief Read entries matching filter
 *
 * WHAT: Read entries filtered by severity and/or category
 * WHY:  Efficient filtered queries
 * HOW:  Decrypts and filters in single pass
 *
 * @param audit Audit buffer handle
 * @param min_severity Minimum severity (inclusive)
 * @param category Category filter (NIMCP_AUDIT_CATEGORY_COUNT = all)
 * @param entries Output buffer
 * @param max_entries Maximum entries
 * @param num_entries Output: actual entries read
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_encrypted_audit_read_filtered(
    nimcp_encrypted_audit_t audit,
    nimcp_audit_severity_t min_severity,
    nimcp_audit_category_t category,
    nimcp_audit_entry_t* entries,
    size_t max_entries,
    size_t* num_entries
);

/**
 * @brief Read entries in time range
 *
 * WHAT: Read entries between start and end timestamps
 * WHY:  Time-based audit queries
 * HOW:  Decrypts and filters by timestamp
 *
 * @param audit Audit buffer handle
 * @param start_time_ns Start timestamp (inclusive)
 * @param end_time_ns End timestamp (inclusive)
 * @param entries Output buffer
 * @param max_entries Maximum entries
 * @param num_entries Output: actual entries read
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_encrypted_audit_read_range(
    nimcp_encrypted_audit_t audit,
    uint64_t start_time_ns,
    uint64_t end_time_ns,
    nimcp_audit_entry_t* entries,
    size_t max_entries,
    size_t* num_entries
);

//=============================================================================
// Key Management
//=============================================================================

/**
 * @brief Rotate encryption key
 *
 * WHAT: Replace current encryption key with new key
 * WHY:  Limit exposure window for compromised keys
 * HOW:  Re-encrypts recent entries with new key, increments version
 *
 * KEY ROTATION PROCESS:
 * 1. Verify new key is valid
 * 2. Increment key version counter
 * 3. Switch to new key for new entries
 * 4. Old entries remain encrypted with old keys
 * 5. Key version stored per-entry for decryption
 *
 * SECURITY NOTE: Old keys must be retained for decryption
 *
 * @param audit Audit buffer handle
 * @param new_key New encryption key (NIMCP_AUDIT_KEY_SIZE bytes)
 * @param key_len Key length (must be NIMCP_AUDIT_KEY_SIZE)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_encrypted_audit_rotate_key(
    nimcp_encrypted_audit_t audit,
    const uint8_t* new_key,
    size_t key_len
);

/**
 * @brief Get current key version
 *
 * WHAT: Return current key version number
 * WHY:  Track key rotations
 * HOW:  Returns internal version counter
 *
 * @param audit Audit buffer handle
 * @return Current key version (0 = initial key)
 */
uint32_t nimcp_encrypted_audit_get_key_version(nimcp_encrypted_audit_t audit);

/**
 * @brief Set key rotation policy
 *
 * WHAT: Configure automatic key rotation
 * WHY:  Automate key rotation for security
 * HOW:  Triggers rotation based on policy
 *
 * @param audit Audit buffer handle
 * @param policy Rotation policy
 * @param threshold Policy-specific threshold (entry count, seconds, bytes)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_encrypted_audit_set_rotation_policy(
    nimcp_encrypted_audit_t audit,
    nimcp_key_rotation_policy_t policy,
    uint64_t threshold
);

//=============================================================================
// Import/Export Functions
//=============================================================================

/**
 * @brief Export encrypted audit buffer to file
 *
 * WHAT: Write encrypted entries to binary file
 * WHY:  Enable log archival and transfer
 * HOW:  Streams encrypted entries to file
 *
 * FILE FORMAT:
 * - Magic number (4 bytes)
 * - Version (4 bytes)
 * - Entry count (8 bytes)
 * - Encrypted entries (variable)
 * - Checksum (32 bytes)
 *
 * SECURITY: Entries remain encrypted in file
 *
 * @param audit Audit buffer handle
 * @param filepath Output file path
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_encrypted_audit_export(
    nimcp_encrypted_audit_t audit,
    const char* filepath
);

/**
 * @brief Import encrypted audit entries from file
 *
 * WHAT: Load encrypted entries from binary file
 * WHY:  Restore archived logs
 * HOW:  Reads and validates encrypted entries
 *
 * SECURITY: Verifies checksums and magic numbers
 *
 * @param audit Audit buffer handle
 * @param filepath Input file path
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_encrypted_audit_import(
    nimcp_encrypted_audit_t audit,
    const char* filepath
);

/**
 * @brief Export to JSON (decrypted)
 *
 * WHAT: Export audit logs as decrypted JSON
 * WHY:  Enable human-readable analysis
 * HOW:  Decrypts and formats as JSON
 *
 * SECURITY WARNING: Output is plaintext!
 *
 * @param audit Audit buffer handle
 * @param filepath Output JSON file path
 * @param master_key Master key for decryption
 * @param key_len Key length
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_encrypted_audit_export_json(
    nimcp_encrypted_audit_t audit,
    const char* filepath,
    const uint8_t* master_key,
    size_t key_len
);

//=============================================================================
// Statistics and Monitoring
//=============================================================================

/**
 * @brief Get audit buffer statistics
 *
 * WHAT: Retrieve comprehensive statistics
 * WHY:  Monitor audit system health
 * HOW:  Aggregates counters and metrics
 *
 * @param audit Audit buffer handle
 * @param stats Output: statistics structure
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_encrypted_audit_get_stats(
    nimcp_encrypted_audit_t audit,
    nimcp_encrypted_audit_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * WHAT: Clear all statistics counters
 * WHY:  Fresh measurement period
 * HOW:  Atomic reset of counters
 *
 * @param audit Audit buffer handle
 */
void nimcp_encrypted_audit_reset_stats(nimcp_encrypted_audit_t audit);

/**
 * @brief Get severity name as string
 *
 * WHAT: Convert severity enum to string
 * WHY:  Human-readable logging
 * HOW:  Static string lookup
 *
 * @param severity Severity level
 * @return Severity name string
 */
const char* nimcp_audit_severity_name(nimcp_audit_severity_t severity);

/**
 * @brief Get category name as string
 *
 * WHAT: Convert category enum to string
 * WHY:  Human-readable logging
 * HOW:  Static string lookup
 *
 * @param category Audit category
 * @return Category name string
 */
const char* nimcp_audit_category_name(nimcp_audit_category_t category);

//=============================================================================
// Bio-Async Integration
//=============================================================================

/**
 * @brief Process bio-async inbox messages
 *
 * WHAT: Handle incoming bio-async messages for audit system
 * WHY:  Enable remote audit queries and management
 * HOW:  Processes messages from module inbox
 *
 * MESSAGE TYPES:
 * - AUDIT_QUERY: Query audit logs
 * - AUDIT_EXPORT: Export to file
 * - AUDIT_KEY_ROTATE: Rotate encryption key
 * - AUDIT_STATS: Get statistics
 *
 * @param audit Audit buffer handle
 * @param max_messages Maximum messages to process (0 = all)
 * @return Number of messages processed
 */
uint32_t nimcp_encrypted_audit_process_inbox(
    nimcp_encrypted_audit_t audit,
    uint32_t max_messages
);

/**
 * @brief Register audit system with bio-async router
 *
 * WHAT: Register module with central bio-async router
 * WHY:  Enable inter-module communication
 * HOW:  Registers handlers for audit messages
 *
 * @param audit Audit buffer handle
 * @param module_id Module ID for registration
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_encrypted_audit_register_bio_async(
    nimcp_encrypted_audit_t audit,
    bio_module_id_t module_id
);

/**
 * @brief Send audit alert via bio-async
 *
 * WHAT: Broadcast critical audit event to all modules
 * WHY:  Enable real-time threat response
 * HOW:  Sends bio-async broadcast message
 *
 * @param audit Audit buffer handle
 * @param entry Audit entry to broadcast
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_encrypted_audit_broadcast_alert(
    nimcp_encrypted_audit_t audit,
    const nimcp_audit_entry_t* entry
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_ENCRYPTED_AUDIT_H */
