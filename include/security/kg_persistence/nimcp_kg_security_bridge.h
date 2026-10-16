/**
 * @file nimcp_kg_security_bridge.h
 * @brief KG Persistence Security Bridge for NIMCP
 *
 * WHAT: Security bridge integrating KG persistence with NIMCP security modules
 * WHY:  Protect knowledge graph data with post-quantum cryptography and validation
 * HOW:  Integrates Kyber/Dilithium (PQ), AES-256-GCM, Argon2id, and BBB validation
 *
 * ARCHITECTURE:
 * ```
 * +------------------------------------------------------------------+
 * |                    KG Security Bridge                            |
 * +------------------------------------------------------------------+
 * |  Post-Quantum  |  Key Derivation |  BBB Validation |  HSM (opt)  |
 * |  (Kyber/Dili)  |    (Argon2id)   |   (Input Gate)  |             |
 * +------------------------------------------------------------------+
 * |                    Security Integration Layer                     |
 * +------------------------------------------------------------------+
 * ```
 *
 * INTEGRATION POINTS:
 * - nimcp_post_quantum.h: Kyber KEM for key exchange, Dilithium for signatures
 * - nimcp_security.h: AES-256-GCM symmetric encryption
 * - nimcp_key_derivation.h: Argon2id for key stretching
 * - nimcp_blood_brain_barrier.h: Input validation and threat detection
 * - nimcp_security_integration.h: Module registration and trust management
 *
 * CRYPTOGRAPHIC FLOW:
 * 1. Encrypt: plaintext -> AES-256-GCM(key from Kyber+Argon2id) -> ciphertext
 * 2. Sign: data -> Dilithium signature (post-quantum secure)
 * 3. Validate: BBB input gate checks before any crypto operation
 *
 * BIOLOGICAL ANALOGY:
 * Like the myelin sheath protecting neural axons, this bridge provides
 * protective encryption around knowledge graph data during storage and
 * transmission, ensuring data integrity through signature verification
 * (analogous to neural signal fidelity checking).
 *
 * @author NIMCP Security Team
 * @date 2025-01-16
 * @version 1.0.0
 */

#ifndef NIMCP_KG_SECURITY_BRIDGE_H
#define NIMCP_KG_SECURITY_BRIDGE_H

#include "security/nimcp_security.h"
#include "security/nimcp_post_quantum.h"
#include "security/nimcp_key_derivation.h"
#include "security/nimcp_security_integration.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "utils/thread/nimcp_thread.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** @brief Magic number for bridge validation ("KGSB") */
#define KG_SECURITY_BRIDGE_MAGIC 0x4B475342

/** @brief Maximum signature size (Dilithium-5 worst case) */
#define KG_SECURITY_MAX_SIGNATURE_SIZE 4595

/** @brief Maximum ciphertext overhead (Kyber-1024 + AES-GCM tag + nonce) */
#define KG_SECURITY_MAX_OVERHEAD (NIMCP_KYBER_1024_CIPHERTEXT_BYTES + \
                                   NIMCP_SECURITY_TAG_SIZE + \
                                   NIMCP_SECURITY_NONCE_SIZE + 64)

/** @brief Default HSM connection timeout (ms) */
#define KG_SECURITY_HSM_TIMEOUT_MS 5000

//=============================================================================
// Forward Declarations
//=============================================================================

/** @brief Opaque HSM handle (implementation-defined) */
typedef struct kg_hsm_handle kg_hsm_handle_t;

//=============================================================================
// Configuration Structures
//=============================================================================

/**
 * @brief HSM configuration for hardware security module integration
 *
 * WHAT: Configuration for optional HSM connection
 * WHY:  Production deployments should use HSM for key protection
 * HOW:  PKCS#11 or vendor-specific HSM API
 */
typedef struct {
    const char* library_path;         /**< Path to HSM library (PKCS#11) */
    const char* slot_id;              /**< HSM slot identifier */
    const char* pin;                  /**< HSM PIN (should be from secure storage) */
    uint32_t timeout_ms;              /**< Connection timeout */
    bool enable_logging;              /**< Log HSM operations */
} kg_hsm_config_t;

/**
 * @brief KG security bridge configuration
 *
 * WHAT: Configuration for security bridge initialization
 * WHY:  Allow customization of security parameters per deployment
 * HOW:  Passed to kg_security_bridge_create()
 */
typedef struct {
    nimcp_kyber_variant_t kyber_variant;       /**< Kyber variant (default: 1024) */
    nimcp_dilithium_variant_t dilithium_variant; /**< Dilithium variant (default: 5) */
    bool enable_hybrid_mode;                   /**< Use hybrid classical+PQ */
    bool require_hsm;                          /**< Fail if HSM unavailable */
    bool enable_logging;                       /**< Log security operations */
    bool enable_statistics;                    /**< Track operation statistics */
    uint32_t key_rotation_interval_hours;      /**< Auto key rotation (0=disabled) */
} kg_security_bridge_config_t;

//=============================================================================
// Statistics Structure
//=============================================================================

/**
 * @brief KG security bridge statistics
 *
 * WHAT: Operational statistics for monitoring and auditing
 * WHY:  Enable security audit trails and performance monitoring
 * HOW:  Atomic counters updated on each operation
 */
typedef struct {
    uint64_t encryptions;                /**< Total encryption operations */
    uint64_t decryptions;                /**< Total decryption operations */
    uint64_t signatures;                 /**< Total signatures generated */
    uint64_t signature_verifications;    /**< Total signature verifications */
    uint64_t verification_failures;      /**< Failed signature verifications */
    uint64_t validation_failures;        /**< BBB validation failures */
    uint64_t threat_reports;             /**< Threats reported to BBB */
    uint64_t key_rotations;              /**< Key rotation events */
    uint64_t hsm_operations;             /**< HSM-backed operations */
    double avg_encrypt_time_ms;          /**< Average encryption time */
    double avg_decrypt_time_ms;          /**< Average decryption time */
    double avg_sign_time_ms;             /**< Average signing time */
    double avg_verify_time_ms;           /**< Average verification time */
    uint64_t total_bytes_encrypted;      /**< Total bytes encrypted */
    uint64_t total_bytes_decrypted;      /**< Total bytes decrypted */
} kg_security_stats_t;

//=============================================================================
// Main Bridge Context
//=============================================================================

/**
 * @brief KG security bridge context
 *
 * WHAT: Central context for all KG persistence security operations
 * WHY:  Encapsulate security state and provide unified interface
 * HOW:  Integrates PQ crypto, KDF, encryption, and BBB components
 *
 * THREAD SAFETY: All operations are mutex-protected
 *
 * LIFECYCLE:
 * 1. Create with kg_security_bridge_create()
 * 2. Optionally connect HSM with kg_security_bridge_connect_hsm()
 * 3. Initialize keys with kg_security_bridge_init_keys()
 * 4. Use encrypt/decrypt/sign/verify operations
 * 5. Destroy with kg_security_bridge_destroy()
 */
typedef struct {
    uint32_t magic;                      /**< Magic number for validation */

    /* Parent security contexts (not owned - caller manages lifecycle) */
    nimcp_pq_context_t pq_ctx;           /**< Post-quantum context (Kyber/Dilithium) */
    nimcp_kdf_context_t kdf_ctx;         /**< Key derivation context (Argon2id) */
    nimcp_sec_integration_t* sec_int;    /**< Security integration (trust/monitoring) */
    bbb_system_t bbb;                    /**< Blood-Brain Barrier (validation) */

    /* KG-specific state */
    uint32_t module_id;                  /**< Registered module ID in sec_int */
    nimcp_kyber_keypair_t* master_keypair;   /**< Master KEM keypair */
    nimcp_dilithium_keypair_t* signing_keypair; /**< Signing keypair */

    /* HSM state (if available) */
    kg_hsm_handle_t* hsm;                /**< HSM connection handle */
    bool using_hsm;                      /**< True if keys are HSM-protected */

    /* Configuration */
    nimcp_kyber_variant_t kyber_variant;
    nimcp_dilithium_variant_t dilithium_variant;
    bool enable_hybrid_mode;
    bool enable_logging;
    bool enable_statistics;

    /* Statistics */
    kg_security_stats_t stats;

    /* Thread synchronization */
    nimcp_mutex_t* mutex;
} kg_security_bridge_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Get default bridge configuration
 *
 * WHAT: Returns secure default configuration
 * WHY:  Simplify initialization with recommended settings
 * HOW:  Static configuration with current best practices
 *
 * DEFAULTS:
 * - Kyber-1024 (NIST Level 5)
 * - Dilithium-5 (NIST Level 5)
 * - Hybrid mode disabled (PQ-only)
 * - HSM not required
 * - Statistics enabled
 *
 * @return Default configuration structure
 */
kg_security_bridge_config_t kg_security_bridge_default_config(void);

/**
 * @brief Create KG security bridge
 *
 * WHAT: Initialize security bridge with provided contexts
 * WHY:  Establish secure channel for KG persistence operations
 * HOW:  Allocate bridge, register with security integration, init mutex
 *
 * PREREQUISITES:
 * - All context parameters must be valid and initialized
 * - Security integration must be running
 * - BBB system must be enabled
 *
 * @param pq_ctx Post-quantum context (required)
 * @param kdf_ctx Key derivation context (required)
 * @param sec_int Security integration context (required)
 * @param bbb Blood-Brain Barrier system (required)
 * @param config Bridge configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
kg_security_bridge_t* kg_security_bridge_create(
    nimcp_pq_context_t pq_ctx,
    nimcp_kdf_context_t kdf_ctx,
    nimcp_sec_integration_t* sec_int,
    bbb_system_t bbb,
    const kg_security_bridge_config_t* config
);

/**
 * @brief Destroy KG security bridge
 *
 * WHAT: Clean up bridge and securely wipe keys
 * WHY:  Prevent key material leakage after use
 * HOW:  Zero memory, unregister from security integration, free resources
 *
 * SECURITY:
 * - All keypairs are securely wiped
 * - HSM connection is properly closed
 * - Module is unregistered from security integration
 *
 * @param bridge Bridge to destroy (NULL-safe)
 */
void kg_security_bridge_destroy(kg_security_bridge_t* bridge);

/**
 * @brief Connect bridge to Hardware Security Module
 *
 * WHAT: Establish connection to HSM for key protection
 * WHY:  Production deployments need hardware key protection
 * HOW:  PKCS#11 or vendor-specific API
 *
 * BEHAVIOR:
 * - If HSM available: keys stored in HSM, operations done in HSM
 * - If HSM unavailable and not required: software fallback
 * - If HSM unavailable and required: returns error
 *
 * @param bridge Bridge handle
 * @param hsm_config HSM configuration
 * @return 0 on success, -1 on error
 */
int kg_security_bridge_connect_hsm(
    kg_security_bridge_t* bridge,
    const kg_hsm_config_t* hsm_config
);

//=============================================================================
// Key Management Functions
//=============================================================================

/**
 * @brief Initialize master keypairs
 *
 * WHAT: Generate or load master KEM and signing keypairs
 * WHY:  Required before any encrypt/sign operations
 * HOW:  Generate via PQ context or load from HSM
 *
 * BEHAVIOR:
 * - If using HSM: load existing keys or generate new ones in HSM
 * - If software: generate new keypairs
 * - Validates key material via BBB
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int kg_security_bridge_init_keys(kg_security_bridge_t* bridge);

/**
 * @brief Rotate master keypairs
 *
 * WHAT: Generate new keypairs and migrate existing data
 * WHY:  Cryptographic hygiene - limit key exposure window
 * HOW:  Generate new keys, re-encrypt existing data, securely erase old keys
 *
 * WARNING: This operation may be slow for large KG stores.
 * Consider scheduling during low-activity periods.
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int kg_security_bridge_rotate_keys(kg_security_bridge_t* bridge);

/**
 * @brief Export public keys for sharing
 *
 * WHAT: Export public keys for other instances to encrypt to us
 * WHY:  Enable secure KG replication across instances
 * HOW:  Copy public key material to provided buffers
 *
 * @param bridge Bridge handle (const - read-only operation)
 * @param kem_public Output buffer for KEM public key (NULL to query size)
 * @param kem_len In: buffer size, Out: actual/required size
 * @param sig_public Output buffer for signing public key (NULL to query size)
 * @param sig_len In: buffer size, Out: actual/required size
 * @return 0 on success, -1 on error
 */
int kg_security_bridge_export_public_keys(
    const kg_security_bridge_t* bridge,
    uint8_t* kem_public,
    size_t* kem_len,
    uint8_t* sig_public,
    size_t* sig_len
);

//=============================================================================
// Encryption/Decryption Functions
//=============================================================================

/**
 * @brief Encrypt data for secure storage
 *
 * WHAT: Encrypt plaintext using hybrid PQ+symmetric encryption
 * WHY:  Protect KG data at rest with post-quantum security
 * HOW:  Kyber KEM -> shared secret -> Argon2id -> AES-256-GCM
 *
 * CRYPTOGRAPHIC FLOW:
 * 1. Validate input via BBB
 * 2. Encapsulate with Kyber to get shared secret
 * 3. Derive AES key from shared secret using Argon2id
 * 4. Encrypt with AES-256-GCM (includes auth tag)
 * 5. Output: [ciphertext_header][kyber_ciphertext][aes_ciphertext][auth_tag]
 *
 * @param bridge Bridge handle
 * @param plaintext Data to encrypt
 * @param pt_len Plaintext length
 * @param ciphertext Output: allocated ciphertext buffer (caller must free)
 * @param ct_len Output: ciphertext length
 * @return 0 on success, -1 on error
 */
int kg_security_bridge_encrypt(
    kg_security_bridge_t* bridge,
    const void* plaintext,
    size_t pt_len,
    void** ciphertext,
    size_t* ct_len
);

/**
 * @brief Decrypt data from secure storage
 *
 * WHAT: Decrypt ciphertext produced by kg_security_bridge_encrypt()
 * WHY:  Retrieve KG data from encrypted storage
 * HOW:  Reverse of encryption: Kyber decap -> Argon2id -> AES-256-GCM
 *
 * SECURITY:
 * - Validates ciphertext structure before decryption
 * - Verifies AES-GCM authentication tag
 * - Reports tampering to BBB if verification fails
 *
 * @param bridge Bridge handle
 * @param ciphertext Encrypted data
 * @param ct_len Ciphertext length
 * @param plaintext Output: allocated plaintext buffer (caller must free)
 * @param pt_len Output: plaintext length
 * @return 0 on success, -1 on error (includes auth failure)
 */
int kg_security_bridge_decrypt(
    kg_security_bridge_t* bridge,
    const void* ciphertext,
    size_t ct_len,
    void** plaintext,
    size_t* pt_len
);

/**
 * @brief Free buffer allocated by encrypt/decrypt operations
 *
 * WHAT: Securely free buffer returned by encrypt/decrypt
 * WHY:  Ensure sensitive data is wiped before freeing
 * HOW:  Zero memory then free
 *
 * @param buffer Buffer to free (NULL-safe)
 */
void kg_security_bridge_free_buffer(void* buffer);

//=============================================================================
// Signing/Verification Functions
//=============================================================================

/**
 * @brief Sign data with Dilithium
 *
 * WHAT: Create post-quantum digital signature
 * WHY:  Authenticate KG data origin and integrity
 * HOW:  Dilithium signature using signing keypair
 *
 * @param bridge Bridge handle
 * @param data Data to sign
 * @param data_len Data length
 * @param signature Output buffer (must be KG_SECURITY_MAX_SIGNATURE_SIZE)
 * @param sig_len In: buffer size, Out: actual signature length
 * @return 0 on success, -1 on error
 */
int kg_security_bridge_sign(
    kg_security_bridge_t* bridge,
    const void* data,
    size_t data_len,
    uint8_t* signature,
    size_t* sig_len
);

/**
 * @brief Verify Dilithium signature
 *
 * WHAT: Verify post-quantum digital signature
 * WHY:  Authenticate KG data before processing
 * HOW:  Dilithium verification using signing public key
 *
 * SECURITY:
 * - Reports verification failures to BBB
 * - Updates signature_verifications/verification_failures stats
 *
 * @param bridge Bridge handle
 * @param data Data that was signed
 * @param data_len Data length
 * @param signature Signature to verify
 * @param sig_len Signature length
 * @return 0 if valid, -1 if invalid or error
 */
int kg_security_bridge_verify(
    kg_security_bridge_t* bridge,
    const void* data,
    size_t data_len,
    const uint8_t* signature,
    size_t sig_len
);

//=============================================================================
// Input Validation Functions
//=============================================================================

/**
 * @brief Validate input data before processing
 *
 * WHAT: Check input data against BBB security policies
 * WHY:  Prevent malicious data from entering KG
 * HOW:  Delegate to BBB input validation
 *
 * CHECKS:
 * - Buffer overflow patterns
 * - Injection attacks
 * - Suspicious byte sequences
 *
 * @param bridge Bridge handle
 * @param data Input data to validate
 * @param size Data size
 * @return true if valid, false if suspicious
 */
bool kg_security_bridge_validate_input(
    kg_security_bridge_t* bridge,
    const void* data,
    size_t size
);

/**
 * @brief Sanitize string input
 *
 * WHAT: Remove/escape dangerous content from string
 * WHY:  Safe handling of untrusted string data
 * HOW:  BBB string sanitization
 *
 * @param bridge Bridge handle
 * @param input Input string
 * @param output Output buffer for sanitized string
 * @param output_size Output buffer size
 * @return Length of sanitized string, or -1 on error
 */
ssize_t kg_security_bridge_sanitize_string(
    kg_security_bridge_t* bridge,
    const char* input,
    char* output,
    size_t output_size
);

//=============================================================================
// Threat Reporting Functions
//=============================================================================

/**
 * @brief Report security threat to immune system
 *
 * WHAT: Report detected security threat via BBB
 * WHY:  Coordinate threat response with brain immune system
 * HOW:  Delegate to BBB threat reporting
 *
 * @param bridge Bridge handle
 * @param type Threat type
 * @param severity Threat severity
 * @param description Human-readable description
 */
void kg_security_bridge_report_threat(
    kg_security_bridge_t* bridge,
    bbb_threat_type_t type,
    bbb_severity_t severity,
    const char* description
);

//=============================================================================
// Statistics Functions
//=============================================================================

/**
 * @brief Get bridge statistics
 *
 * WHAT: Retrieve operational statistics
 * WHY:  Monitoring, auditing, and debugging
 * HOW:  Copy current stats (thread-safe)
 *
 * @param bridge Bridge handle (const - read-only)
 * @param stats Output statistics structure
 * @return 0 on success, -1 on error
 */
int kg_security_bridge_get_stats(
    const kg_security_bridge_t* bridge,
    kg_security_stats_t* stats
);

/**
 * @brief Reset bridge statistics
 *
 * WHAT: Clear all statistics counters
 * WHY:  Start fresh measurement period
 * HOW:  Zero stats structure (thread-safe)
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int kg_security_bridge_reset_stats(kg_security_bridge_t* bridge);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Check if bridge has initialized keys
 *
 * @param bridge Bridge handle
 * @return true if keys are initialized
 */
bool kg_security_bridge_has_keys(const kg_security_bridge_t* bridge);

/**
 * @brief Check if bridge is using HSM
 *
 * @param bridge Bridge handle
 * @return true if HSM is connected and in use
 */
bool kg_security_bridge_using_hsm(const kg_security_bridge_t* bridge);

/**
 * @brief Get security level in bits
 *
 * @param bridge Bridge handle
 * @return Security level (128, 192, or 256)
 */
int kg_security_bridge_get_security_level(const kg_security_bridge_t* bridge);

/**
 * @brief Validate bridge context
 *
 * WHAT: Check if bridge context is valid
 * WHY:  Defensive programming - detect corruption
 * HOW:  Verify magic number and essential fields
 *
 * @param bridge Bridge handle
 * @return true if valid, false if corrupted/invalid
 */
bool kg_security_bridge_is_valid(const kg_security_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_KG_SECURITY_BRIDGE_H */
