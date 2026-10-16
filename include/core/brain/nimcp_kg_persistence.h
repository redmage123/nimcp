/**
 * @file nimcp_kg_persistence.h
 * @brief Persistent Storage Layer for Knowledge Graph with QuestDB + Quantum-Resistant Encryption
 * @version 1.0.0
 * @date 2025-01-16
 *
 * WHAT: Persistent storage layer for brain knowledge graph with enterprise-grade security
 * WHY:  Brain needs durable storage of KG state with quantum-resistant protection
 * HOW:  QuestDB time-series database with hybrid Kyber/AES encryption via NIMCP security module
 *
 * ARCHITECTURE:
 * ```
 * +===========================================================================+
 * |                    KG PERSISTENCE ARCHITECTURE                            |
 * +===========================================================================+
 * |                                                                           |
 * |   Application Layer (KG Operations)                                       |
 * |   +-------------------------------------------------------------------+   |
 * |   |  kg_persist_*()  kg_query_*()  kg_stream_*()  kg_batch_*()       |   |
 * |   +-------------------------------------------------------------------+   |
 * |                                |                                          |
 * |                                v                                          |
 * |   Encryption Layer (NIMCP Security Module)                               |
 * |   +-------------------------------------------------------------------+   |
 * |   |  Kyber1024 + AES-256-GCM  |  Dilithium5 Signatures  |  Argon2id  |   |
 * |   +-------------------------------------------------------------------+   |
 * |                                |                                          |
 * |                                v                                          |
 * |   I/O Dispatcher Layer                                                   |
 * |   +-------------------------------------------------------------------+   |
 * |   |  Write Queue  |  Read Queue  |  Batch Queue  |  Priority Queue   |   |
 * |   |  (lock-free)  |  (lock-free) |  (coalesce)   |  (urgent)         |   |
 * |   +-------------------------------------------------------------------+   |
 * |                                |                                          |
 * |                                v                                          |
 * |   Connection Pool + Thread Pool                                          |
 * |   +-------------------------------------------------------------------+   |
 * |   |  Writer Threads (4)  |  Reader Threads (8)  |  Pool (4-32 conn)  |   |
 * |   +-------------------------------------------------------------------+   |
 * |                                |                                          |
 * |                                v                                          |
 * |   QuestDB (Time-Series Database)                                         |
 * |   +-------------------------------------------------------------------+   |
 * |   |  ILP (9009)  |  PG Wire (8812)  |  HTTP REST (9000)              |   |
 * |   +-------------------------------------------------------------------+   |
 * |                                                                           |
 * +===========================================================================+
 * ```
 *
 * DATABASE SCHEMA (QuestDB):
 * - kg_nodes:              Node storage (partitioned by day)
 * - kg_edges:              Edge storage (partitioned by day)
 * - kg_weight_snapshots:   Neural weight snapshots (partitioned by hour)
 * - kg_neuromod_state:     Neuromodulator state (partitioned by hour)
 * - kg_change_events:      Audit trail (partitioned by day)
 * - kg_hippocampal_episodes: Episodic memory (partitioned by day)
 * - kg_autobiographical:   Self-history (partitioned by month)
 *
 * SECURITY:
 * - Encryption at rest:    Hybrid Kyber1024 + AES-256-GCM (quantum-resistant)
 * - Encryption in transit: mTLS with TLS 1.3
 * - Authentication:        Composite (mTLS + JWT supported)
 * - Authorization:         RBAC with row/column-level security
 * - Audit:                 Tamper-evident hash-chain logging
 * - HSM:                   Optional hardware key protection
 *
 * THREAD SAFETY: All operations are thread-safe via internal synchronization
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_KG_PERSISTENCE_H
#define NIMCP_KG_PERSISTENCE_H

#include "core/brain/nimcp_brain_kg.h"
#include "utils/validation/nimcp_common.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

/** Persistence context (opaque) */
typedef struct kg_persistence kg_persistence_t;

/** Connection pool (opaque) */
typedef struct kg_connection_pool kg_connection_pool_t;

/** HSM handle (opaque) */
typedef struct kg_hsm_handle kg_hsm_handle_t;

/* Forward declarations for NIMCP security module types (if not included) */
#ifndef NIMCP_PQ_CONTEXT_DEFINED
typedef struct nimcp_pq_context nimcp_pq_context_t;
#endif

#ifndef NIMCP_KDF_CONTEXT_DEFINED
typedef struct nimcp_kdf_context nimcp_kdf_context_t;
#endif

#ifndef NIMCP_ENCRYPTION_CONTEXT_DEFINED
typedef struct nimcp_encryption_context nimcp_encryption_context_t;
#endif

#ifndef NIMCP_SEC_INTEGRATION_DEFINED
typedef struct nimcp_sec_integration nimcp_sec_integration_t;
#endif

#ifndef BBB_SYSTEM_DEFINED
typedef struct bbb_system_struct* bbb_system_t;
#endif

#ifndef NIMCP_PQ_CONFIG_DEFINED
typedef struct nimcp_pq_config nimcp_pq_config_t;
#endif

#ifndef NIMCP_KDF_CONFIG_DEFINED
typedef struct nimcp_kdf_config nimcp_kdf_config_t;
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Maximum path length for storage/config paths */
#define KG_PERSIST_MAX_PATH_LEN         256

/** Maximum connection pool size */
#define KG_PERSIST_MAX_CONNECTIONS      128

/** Maximum writer/reader threads */
#define KG_PERSIST_MAX_THREADS          64

/** Default QuestDB ILP port */
#define KG_QUESTDB_DEFAULT_ILP_PORT     9009

/** Default QuestDB PostgreSQL wire port */
#define KG_QUESTDB_DEFAULT_PG_PORT      8812

/** Default QuestDB HTTP port */
#define KG_QUESTDB_DEFAULT_HTTP_PORT    9000

/* ============================================================================
 * Encryption Configuration Types
 * ============================================================================ */

/**
 * @brief Cryptographic algorithm selection
 *
 * WHAT: Encryption algorithm for data at rest
 * WHY:  Balance between security level and performance
 * HOW:  Wraps NIMCP security module algorithms
 */
typedef enum {
    KG_CRYPTO_NONE = 0,                  /**< No encryption (testing only) */
    KG_CRYPTO_AES256_GCM,                /**< nimcp_encryption_*() - Classical */
    KG_CRYPTO_XCHACHA20_POLY1305,        /**< nimcp_encrypt_with_password() - Classical */
    KG_CRYPTO_HYBRID_KYBER_AES,          /**< nimcp_hybrid_* + AES-256-GCM (RECOMMENDED) */
    KG_CRYPTO_HYBRID_KYBER_XCHACHA       /**< nimcp_hybrid_* + XChaCha20-Poly1305 */
} kg_crypto_algorithm_t;

/**
 * @brief Hardware Security Module type
 *
 * WHAT: HSM provider for hardware key protection
 * WHY:  REQUIRED for production deployments
 * HOW:  Master keys stored in HSM, never leave hardware
 */
typedef enum {
    KG_HSM_NONE = 0,                     /**< Software-only (dev/test) */
    KG_HSM_TPM2,                         /**< TPM 2.0 (common on modern PCs) */
    KG_HSM_PKCS11,                       /**< PKCS#11 compatible HSM */
    KG_HSM_AWS_CLOUDHSM,                 /**< AWS CloudHSM */
    KG_HSM_AZURE_KEYVAULT,               /**< Azure Key Vault HSM */
    KG_HSM_GCP_CLOUD_HSM                 /**< Google Cloud HSM */
} kg_hsm_type_t;

/**
 * @brief Audit logging detail level
 */
typedef enum {
    KG_AUDIT_LEVEL_NONE = 0,             /**< No audit logging */
    KG_AUDIT_LEVEL_BASIC,                /**< Log encrypt/decrypt operations */
    KG_AUDIT_LEVEL_DETAILED,             /**< Include key IDs, timestamps */
    KG_AUDIT_LEVEL_FULL                  /**< Include data sizes, checksums */
} kg_audit_level_t;

/**
 * @brief Audit event types for security logging
 */
typedef enum {
    KG_AUDIT_KEY_GENERATED = 0,          /**< New key generated */
    KG_AUDIT_KEY_ROTATED,                /**< Key rotation performed */
    KG_AUDIT_KEY_IMPORTED,               /**< Key imported from external source */
    KG_AUDIT_ENCRYPT_START,              /**< Encryption operation started */
    KG_AUDIT_ENCRYPT_SUCCESS,            /**< Encryption completed successfully */
    KG_AUDIT_ENCRYPT_FAILURE,            /**< Encryption failed */
    KG_AUDIT_DECRYPT_START,              /**< Decryption operation started */
    KG_AUDIT_DECRYPT_SUCCESS,            /**< Decryption completed successfully */
    KG_AUDIT_DECRYPT_FAILURE,            /**< Decryption failed */
    KG_AUDIT_INTEGRITY_CHECK,            /**< Integrity verification performed */
    KG_AUDIT_TAMPER_DETECTED,            /**< Data tampering detected */
    KG_AUDIT_HSM_CONNECTED,              /**< HSM connection established */
    KG_AUDIT_HSM_DISCONNECTED            /**< HSM connection lost */
} kg_audit_event_type_t;

/**
 * @brief HSM configuration
 */
typedef struct {
    kg_hsm_type_t type;                  /**< HSM type */
    char provider_path[KG_PERSIST_MAX_PATH_LEN]; /**< Path to PKCS#11 library or config */
    char slot_id[64];                    /**< HSM slot identifier */
    char key_label[64];                  /**< Label for the master key in HSM */
    bool require_hsm_in_production;      /**< FAIL if HSM unavailable in production */
} kg_hsm_config_t;

/**
 * @brief HSM key information
 */
typedef struct {
    char key_label[64];                  /**< Key label in HSM */
    char key_id[64];                     /**< Key identifier */
    uint64_t creation_time;              /**< Key creation timestamp */
    uint64_t last_used_time;             /**< Last key usage timestamp */
    uint32_t usage_count;                /**< Number of times key was used */
    bool is_extractable;                 /**< Whether key can be exported */
    bool is_rotated;                     /**< Whether key has been rotated */
} kg_hsm_key_info_t;

/**
 * @brief Audit logging configuration
 */
typedef struct {
    kg_audit_level_t level;              /**< Audit detail level */
    char log_path[KG_PERSIST_MAX_PATH_LEN]; /**< Path to audit log file */
    bool log_to_syslog;                  /**< Also log to syslog */
    bool enable_tamper_evident;          /**< Hash-chain for tamper evidence */
    uint64_t max_log_size_mb;            /**< Max log size before rotation */
    uint32_t log_retention_days;         /**< Days to retain logs */
    bool enable_remote_logging;          /**< Forward to remote SIEM */
    char remote_endpoint[KG_PERSIST_MAX_PATH_LEN]; /**< SIEM endpoint URL */
} kg_audit_config_t;

/**
 * @brief Encryption configuration (integrates with NIMCP Security Module)
 */
typedef struct {
    kg_crypto_algorithm_t algorithm;     /**< Default: KG_CRYPTO_HYBRID_KYBER_AES */

    /* Security module handles (set during initialization) */
    nimcp_pq_context_t* pq_ctx;          /**< Post-quantum context (Kyber/Dilithium) */
    nimcp_kdf_context_t* kdf_ctx;        /**< Key derivation context (Argon2id) */
    nimcp_encryption_context_t* enc_ctx; /**< Symmetric encryption context (AES-256-GCM) */
    nimcp_sec_integration_t* sec_int;    /**< Security integration (trust, monitoring) */
    bbb_system_t* bbb;                   /**< Blood-Brain Barrier (input validation) */

    /* KDF configuration (passed to nimcp_kdf_create()) */
    nimcp_kdf_config_t* kdf_config;      /**< Uses existing NIMCP KDF config */

    /* Post-quantum configuration (passed to nimcp_pq_context_create()) */
    nimcp_pq_config_t* pq_config;        /**< Uses existing NIMCP PQ config */

    /* Key management */
    char master_key_path[KG_PERSIST_MAX_PATH_LEN]; /**< Path to master key file (if no HSM) */
    bool enable_key_rotation;            /**< Rotate keys periodically */
    uint32_t key_rotation_days;          /**< Days between rotations */

    /* HSM configuration (REQUIRED for production) */
    kg_hsm_config_t hsm;                 /**< Hardware security module settings */

    /* Audit logging (REQUIRED for compliance) */
    kg_audit_config_t audit;             /**< Audit log settings */

    /* Integrity verification */
    bool enable_hmac;                    /**< HMAC on encrypted data */
    bool enable_merkle_tree;             /**< Merkle tree for chunk integrity */

    /* Security module registration */
    uint32_t security_module_id;         /**< Module ID from nimcp_sec_register_module() */
} kg_encryption_config_t;

/* ============================================================================
 * QuestDB Configuration Types
 * ============================================================================ */

/**
 * @brief QuestDB connection mode
 */
typedef enum {
    KG_QUESTDB_EMBEDDED = 0,             /**< In-process via JNI (single-user) */
    KG_QUESTDB_SERVER,                   /**< Connect to standalone server */
    KG_QUESTDB_CLUSTER                   /**< Connect to QuestDB cluster */
} kg_questdb_mode_t;

/**
 * @brief TLS/SSL mode for QuestDB connections
 */
typedef enum {
    KG_TLS_MODE_DISABLED = 0,            /**< No TLS (NEVER use in production) */
    KG_TLS_MODE_REQUIRE,                 /**< Require TLS, no client cert */
    KG_TLS_MODE_VERIFY_CA,               /**< Verify server cert against CA */
    KG_TLS_MODE_VERIFY_FULL,             /**< Verify server cert + hostname */
    KG_TLS_MODE_MTLS                     /**< Mutual TLS (client + server certs) - REQUIRED FOR PRODUCTION */
} kg_tls_mode_t;

/**
 * @brief Data-at-rest encryption algorithm
 */
typedef enum {
    KG_DAR_ENCRYPTION_NONE = 0,          /**< No encryption (NEVER use in production) */
    KG_DAR_ENCRYPTION_AES256_GCM,        /**< AES-256-GCM (FIPS 140-2 compliant) */
    KG_DAR_ENCRYPTION_CHACHA20_POLY1305, /**< ChaCha20-Poly1305 (for non-AES-NI systems) */
    KG_DAR_ENCRYPTION_HYBRID_PQ          /**< AES-256-GCM + Kyber1024 key wrap - RECOMMENDED */
} kg_dar_encryption_t;

/**
 * @brief Authentication method
 */
typedef enum {
    KG_AUTH_NONE = 0,                    /**< No authentication (NEVER use in production) */
    KG_AUTH_PASSWORD,                    /**< Username + password (legacy) */
    KG_AUTH_MTLS,                        /**< Mutual TLS certificate auth */
    KG_AUTH_JWT,                         /**< JWT token authentication */
    KG_AUTH_OIDC,                        /**< OpenID Connect (SSO integration) */
    KG_AUTH_KERBEROS,                    /**< Kerberos/GSSAPI */
    KG_AUTH_COMPOSITE                    /**< Multiple methods (e.g., mTLS + JWT) - RECOMMENDED */
} kg_auth_method_t;

/**
 * @brief Database role levels
 */
typedef enum {
    KG_ROLE_NONE = 0,                    /**< No access */
    KG_ROLE_READER,                      /**< Read-only access to allowed tables */
    KG_ROLE_WRITER,                      /**< Read + write to allowed tables */
    KG_ROLE_ADMIN,                       /**< Full access to allowed tables + schema changes */
    KG_ROLE_SUPERUSER                    /**< Full system access (use sparingly) */
} kg_db_role_t;

/**
 * @brief Database permission flags
 */
typedef enum {
    KG_PERM_SELECT = 1 << 0,             /**< Read rows */
    KG_PERM_INSERT = 1 << 1,             /**< Insert rows */
    KG_PERM_UPDATE = 1 << 2,             /**< Update rows */
    KG_PERM_DELETE = 1 << 3,             /**< Delete rows */
    KG_PERM_CREATE = 1 << 4,             /**< Create tables */
    KG_PERM_DROP = 1 << 5,               /**< Drop tables */
    KG_PERM_ALTER = 1 << 6,              /**< Alter schema */
    KG_PERM_GRANT = 1 << 7,              /**< Grant permissions to others */
    KG_PERM_EXECUTE = 1 << 8             /**< Execute stored procedures */
} kg_db_permission_t;

/**
 * @brief TLS configuration for QuestDB connections
 */
typedef struct {
    kg_tls_mode_t mode;                  /**< TLS mode (default: KG_TLS_MODE_MTLS) */

    /* Server certificate verification */
    char ca_cert_path[KG_PERSIST_MAX_PATH_LEN];       /**< CA certificate for server verification */
    char ca_cert_bundle_path[KG_PERSIST_MAX_PATH_LEN]; /**< CA bundle for chain verification */

    /* Client certificate (for mTLS) */
    char client_cert_path[KG_PERSIST_MAX_PATH_LEN];  /**< Client certificate (PEM) */
    char client_key_path[KG_PERSIST_MAX_PATH_LEN];   /**< Client private key (PEM, encrypted) */
    char client_key_password[128];       /**< Password for encrypted key (secured in memory) */

    /* TLS protocol settings */
    char min_tls_version[8];             /**< Minimum TLS version: "1.2" or "1.3" (default: "1.3") */
    char cipher_suites[512];             /**< Allowed cipher suites (TLS 1.3 only by default) */
    bool enable_session_tickets;         /**< Session resumption (default: false for security) */
    bool enable_ocsp_stapling;           /**< OCSP stapling for cert revocation (default: true) */

    /* Certificate rotation */
    bool enable_cert_hot_reload;         /**< Reload certs without restart (default: true) */
    uint32_t cert_check_interval_sec;    /**< Check for new certs every N seconds (default: 300) */
} kg_questdb_tls_config_t;

/**
 * @brief Data-at-rest encryption configuration
 */
typedef struct {
    kg_dar_encryption_t algorithm;       /**< Encryption algorithm (default: HYBRID_PQ) */

    /* Key management */
    char master_key_path[KG_PERSIST_MAX_PATH_LEN]; /**< Path to master key (if no HSM) */
    bool use_hsm_for_master_key;         /**< Store master key in HSM (REQUIRED for production) */
    char hsm_key_label[64];              /**< Label for master key in HSM */

    /* Key encryption key (KEK) hierarchy */
    bool enable_kek_rotation;            /**< Rotate KEK periodically (default: true) */
    uint32_t kek_rotation_days;          /**< Days between KEK rotations (default: 90) */

    /* Data encryption key (DEK) management */
    bool enable_per_table_dek;           /**< Separate DEK per table (default: true) */
    bool enable_per_partition_dek;       /**< Separate DEK per partition (default: false) */
    uint32_t dek_cache_size;             /**< Number of DEKs to cache in memory (default: 100) */

    /* Secure deletion */
    bool enable_crypto_shredding;        /**< Delete data by destroying DEK (default: true) */
    uint32_t shred_passes;               /**< Overwrite passes for physical deletion (default: 3) */
} kg_questdb_dar_config_t;

/**
 * @brief Authentication configuration
 */
typedef struct {
    kg_auth_method_t primary_method;     /**< Primary auth method (default: KG_AUTH_COMPOSITE) */
    kg_auth_method_t fallback_method;    /**< Fallback if primary fails (default: KG_AUTH_NONE = no fallback) */

    /* Password authentication */
    char username[64];
    char password[128];                  /**< Hashed with Argon2id, never stored plaintext */
    bool require_password_complexity;    /**< Enforce password policy (default: true) */
    uint32_t password_min_length;        /**< Minimum password length (default: 16) */
    uint32_t password_expiry_days;       /**< Force password change (default: 90, 0 = never) */
    uint32_t max_failed_attempts;        /**< Lockout after N failures (default: 5) */
    uint32_t lockout_duration_sec;       /**< Lockout duration (default: 300) */

    /* JWT authentication */
    char jwt_issuer[KG_PERSIST_MAX_PATH_LEN];     /**< Expected JWT issuer */
    char jwt_audience[KG_PERSIST_MAX_PATH_LEN];   /**< Expected JWT audience */
    char jwt_public_key_path[KG_PERSIST_MAX_PATH_LEN]; /**< Path to JWT verification public key */
    char jwt_jwks_url[KG_PERSIST_MAX_PATH_LEN];   /**< JWKS URL for key rotation */
    uint32_t jwt_clock_skew_sec;         /**< Allowed clock skew (default: 30) */
    bool jwt_require_exp;                /**< Require expiration claim (default: true) */
    uint32_t jwt_max_age_sec;            /**< Maximum token age (default: 3600) */

    /* OpenID Connect */
    char oidc_discovery_url[KG_PERSIST_MAX_PATH_LEN]; /**< OIDC discovery URL */
    char oidc_client_id[128];            /**< OIDC client ID */
    char oidc_client_secret[KG_PERSIST_MAX_PATH_LEN]; /**< OIDC client secret (encrypted) */
    char oidc_scopes[KG_PERSIST_MAX_PATH_LEN];    /**< Required scopes (default: "openid profile") */

    /* Kerberos */
    char kerberos_keytab_path[KG_PERSIST_MAX_PATH_LEN]; /**< Path to Kerberos keytab */
    char kerberos_principal[KG_PERSIST_MAX_PATH_LEN];   /**< Service principal name */
    char kerberos_realm[128];            /**< Kerberos realm */

    /* mTLS authentication (uses TLS config) */
    bool mtls_require_client_cert;       /**< Require client certificate (default: true with mTLS) */
    char mtls_allowed_cn_pattern[KG_PERSIST_MAX_PATH_LEN]; /**< Regex for allowed Common Names */
    char mtls_allowed_san_pattern[KG_PERSIST_MAX_PATH_LEN]; /**< Regex for allowed Subject Alt Names */

    /* Session management */
    uint32_t session_timeout_sec;        /**< Idle session timeout (default: 1800) */
    uint32_t session_absolute_timeout_sec; /**< Absolute session timeout (default: 28800) */
    bool enable_session_binding;         /**< Bind session to client IP/cert (default: true) */
    uint32_t max_concurrent_sessions;    /**< Max sessions per user (default: 5) */
} kg_questdb_auth_config_t;

/**
 * @brief Per-table permission configuration
 */
typedef struct {
    char table_pattern[128];             /**< Table name or pattern (e.g., "kg_*", "kg_nodes") */
    uint32_t permissions;                /**< Bitmask of kg_db_permission_t */
    char column_whitelist[512];          /**< Comma-separated allowed columns (empty = all) */
    char column_blacklist[512];          /**< Comma-separated denied columns (e.g., "weights_blob") */
    char row_filter[KG_PERSIST_MAX_PATH_LEN]; /**< SQL WHERE clause for row-level security */
} kg_table_permission_t;

/**
 * @brief Role configuration
 */
typedef struct {
    char role_name[64];                  /**< Role identifier */
    kg_db_role_t base_role;              /**< Base role level */
    kg_table_permission_t* table_permissions; /**< Per-table permissions */
    uint32_t table_permission_count;
    bool can_create_temp_tables;         /**< Allow temporary tables (default: false) */
    bool can_use_copy;                   /**< Allow COPY command (default: false for security) */
    uint64_t max_query_time_ms;          /**< Max query execution time (default: 60000) */
    uint64_t max_memory_bytes;           /**< Max memory per query (default: 1GB) */
    uint32_t max_result_rows;            /**< Max rows returned (default: 100000) */
} kg_db_role_config_t;

/**
 * @brief User-to-role mapping entry
 */
typedef struct {
    char username_pattern[128];          /**< Username or pattern (e.g., "*@admin.example.com") */
    char cert_cn_pattern[128];           /**< Certificate CN pattern */
    char jwt_claim_pattern[KG_PERSIST_MAX_PATH_LEN]; /**< JWT claim pattern */
    char role_name[64];                  /**< Assigned role */
} kg_user_role_mapping_t;

/**
 * @brief Sensitive column protection entry
 */
typedef struct {
    char table_name[64];
    char column_name[64];
    bool mask_in_logs;                   /**< Mask value in query logs */
    bool mask_in_errors;                 /**< Mask value in error messages */
    char mask_pattern[64];               /**< Masking pattern (e.g., "***REDACTED***") */
} kg_sensitive_column_t;

/**
 * @brief Authorization (RBAC) configuration
 */
typedef struct {
    bool enable_rbac;                    /**< Enable role-based access control (default: true) */
    bool enable_row_level_security;      /**< Enable row-level security (default: true) */
    bool enable_column_level_security;   /**< Enable column-level security (default: true) */

    /* Default permissions */
    kg_db_role_t default_role;           /**< Role for unauthenticated (default: KG_ROLE_NONE) */
    kg_db_role_t authenticated_default;  /**< Role for authenticated without explicit role (default: KG_ROLE_READER) */

    /* Role definitions */
    kg_db_role_config_t* roles;
    uint32_t role_count;

    /* User-to-role mappings */
    kg_user_role_mapping_t* user_role_mappings;
    uint32_t user_role_mapping_count;

    /* Sensitive data protection */
    kg_sensitive_column_t* sensitive_columns;
    uint32_t sensitive_column_count;

    /* Audit requirements */
    bool require_audit_for_admin;        /**< Audit all admin operations (default: true) */
    bool require_audit_for_sensitive;    /**< Audit access to sensitive columns (default: true) */
} kg_questdb_authz_config_t;

/**
 * @brief Security audit configuration for QuestDB
 */
typedef struct {
    bool enable_query_logging;           /**< Log all queries (default: true) */
    bool enable_connection_logging;      /**< Log connections/disconnections (default: true) */
    bool enable_auth_logging;            /**< Log authentication attempts (default: true) */
    bool enable_authz_logging;           /**< Log authorization decisions (default: true) */
    bool enable_schema_change_logging;   /**< Log DDL operations (default: true) */
    bool enable_data_access_logging;     /**< Log DML operations (default: for sensitive only) */

    char audit_log_path[KG_PERSIST_MAX_PATH_LEN]; /**< Path to audit log */
    bool audit_log_encrypt;              /**< Encrypt audit logs (default: true) */
    bool audit_log_sign;                 /**< Sign audit log entries (default: true) */
    bool audit_log_tamper_evident;       /**< Hash chain for tamper evidence (default: true) */
    uint64_t audit_log_max_size_mb;      /**< Max size before rotation (default: 1024) */
    uint32_t audit_log_retention_days;   /**< Days to retain logs (default: 365) */

    /* Remote audit (SIEM integration) */
    bool enable_siem_forwarding;         /**< Forward to SIEM (default: true in production) */
    char siem_endpoint[KG_PERSIST_MAX_PATH_LEN]; /**< SIEM endpoint URL */
    char siem_format[32];                /**< Format: "CEF", "LEEF", "JSON" (default: "CEF") */
    bool siem_use_tls;                   /**< Use TLS for SIEM connection (default: true) */
} kg_questdb_audit_config_t;

/**
 * @brief Complete QuestDB security configuration
 */
typedef struct {
    kg_questdb_tls_config_t tls;         /**< TLS/SSL configuration */
    kg_questdb_dar_config_t encryption;  /**< Data-at-rest encryption */
    kg_questdb_auth_config_t auth;       /**< Authentication configuration */
    kg_questdb_authz_config_t authz;     /**< Authorization configuration */
    kg_questdb_audit_config_t audit;     /**< Security audit configuration */

    /* Security policy enforcement */
    bool require_tls;                    /**< Reject non-TLS connections (default: true) */
    bool require_authentication;         /**< Reject unauthenticated (default: true) */
    bool require_encryption_at_rest;     /**< Fail if DAR encryption unavailable (default: true) */
    bool fail_secure;                    /**< On security error, deny access (default: true) */

    /* Network security */
    char allowed_ip_ranges[512];         /**< CIDR ranges allowed to connect */
    char denied_ip_ranges[512];          /**< CIDR ranges explicitly denied */
    uint32_t max_connections_per_ip;     /**< Rate limit per IP (default: 100) */
    uint32_t connection_rate_limit;      /**< New connections per second per IP (default: 10) */
} kg_questdb_security_config_t;

/* ============================================================================
 * I/O Configuration Types
 * ============================================================================ */

/**
 * @brief I/O thread pool configuration
 */
typedef struct {
    uint32_t writer_threads;             /**< Threads for write operations (default: 4) */
    uint32_t reader_threads;             /**< Threads for read operations (default: 8) */
    uint32_t io_threads;                 /**< Low-level I/O threads (default: CPU cores) */
    bool pin_threads_to_cores;           /**< CPU affinity for I/O threads */
    uint32_t thread_stack_size_kb;       /**< Stack size per thread (default: 256) */
} kg_io_thread_config_t;

/**
 * @brief Connection pool configuration
 */
typedef struct {
    uint32_t min_connections;            /**< Minimum pool size (default: 4) */
    uint32_t max_connections;            /**< Maximum pool size (default: 32) */
    uint32_t connection_timeout_ms;      /**< Timeout waiting for connection (default: 5000) */
    uint32_t idle_timeout_ms;            /**< Close idle connections after (default: 60000) */
    uint32_t max_lifetime_ms;            /**< Max connection lifetime (default: 1800000) */
    bool enable_connection_validation;   /**< Validate before use (default: true) */
    uint32_t validation_interval_ms;     /**< Validate every N ms (default: 30000) */
} kg_connection_pool_config_t;

/**
 * @brief Async I/O and buffering configuration
 */
typedef struct {
    /* Write buffering */
    uint32_t write_buffer_size_kb;       /**< Per-table write buffer (default: 1024) */
    uint32_t write_queue_depth;          /**< Async write queue depth (default: 1024) */
    bool enable_batch_writes;            /**< Batch small writes (default: true) */
    uint32_t batch_size;                 /**< Rows per batch (default: 10000) */
    uint32_t batch_timeout_ms;           /**< Max wait for batch fill (default: 100) */

    /* Read buffering */
    uint32_t read_buffer_size_kb;        /**< Read-ahead buffer (default: 512) */
    uint32_t prefetch_rows;              /**< Prefetch for sequential reads (default: 10000) */
    bool enable_result_caching;          /**< Cache frequent queries (default: true) */
    uint32_t cache_size_mb;              /**< Result cache size (default: 256) */
    uint32_t cache_ttl_ms;               /**< Cache entry TTL (default: 5000) */

    /* I/O multiplexing */
    bool enable_io_uring;                /**< Use io_uring on Linux (default: true) */
    bool enable_direct_io;               /**< Bypass page cache (default: false) */
    uint32_t io_uring_queue_depth;       /**< io_uring SQ depth (default: 256) */
} kg_async_io_config_t;

/**
 * @brief QuestDB configuration (complete)
 */
typedef struct {
    kg_questdb_mode_t mode;              /**< Connection mode */
    char host[KG_PERSIST_MAX_PATH_LEN];  /**< Server host (for SERVER/CLUSTER mode) */
    uint16_t ilp_port;                   /**< ILP ingestion port (default: 9009) */
    uint16_t pg_port;                    /**< PostgreSQL wire protocol (default: 8812) */
    uint16_t http_port;                  /**< HTTP REST API (default: 9000) */

    /* Embedded mode settings */
    char data_dir[KG_PERSIST_MAX_PATH_LEN]; /**< Data directory for embedded mode */
    uint32_t writer_memory_mb;           /**< Memory for write buffers (default: 512) */

    /* I/O optimization (CRITICAL for performance) */
    kg_io_thread_config_t threads;       /**< Thread pool settings */
    kg_connection_pool_config_t pool;    /**< Connection pool settings */
    kg_async_io_config_t async_io;       /**< Async I/O and buffering */

    /* Performance tuning */
    uint32_t commit_lag_ms;              /**< Max delay before commit (default: 200ms) */
    uint32_t max_uncommitted_rows;       /**< Rows before forced commit (default: 500000) */
    bool enable_wal;                     /**< Write-ahead log for durability */
    bool enable_parallel_ingestion;      /**< Parallel table ingestion (default: true) */

    /* Partitioning */
    char partition_by[16];               /**< "HOUR", "DAY", "MONTH", "YEAR" */

    /* SECURITY (REQUIRED FOR PRODUCTION) */
    kg_questdb_security_config_t security; /**< Complete security configuration */
} kg_questdb_config_t;

/* ============================================================================
 * Diff Record Types
 * ============================================================================ */

/**
 * @brief Change type for differential updates
 */
typedef enum {
    KG_DIFF_NODE_ADDED = 0,              /**< New node added */
    KG_DIFF_NODE_REMOVED,                /**< Node removed */
    KG_DIFF_NODE_MODIFIED,               /**< Node properties changed */
    KG_DIFF_EDGE_ADDED,                  /**< New edge added */
    KG_DIFF_EDGE_REMOVED,                /**< Edge removed */
    KG_DIFF_EDGE_MODIFIED,               /**< Edge properties changed */
    KG_DIFF_STATE_CHANGED                /**< State transition occurred */
} kg_diff_change_type_t;

/**
 * @brief Differential change record
 *
 * WHAT: Record of a single change in the KG
 * WHY:  Enable incremental persistence instead of full saves
 * HOW:  Track what changed, when, and context
 */
typedef struct {
    kg_diff_change_type_t change_type;   /**< Type of change */
    brain_kg_node_id_t node_id;          /**< Affected node */
    brain_kg_node_id_t edge_from;        /**< For edge changes: source node */
    brain_kg_node_id_t edge_to;          /**< For edge changes: target node */
    uint64_t timestamp;                  /**< When change occurred (ms since epoch) */
    char description[128];               /**< Human-readable description */
} kg_diff_record_t;

/**
 * @brief Differential result from comparing stored vs current state
 *
 * WHAT: Result of diff computation between persisted and in-memory KG
 * WHY:  Determine what needs to be saved incrementally
 * HOW:  List of changes plus version tracking
 */
typedef struct {
    kg_diff_record_t* changes;           /**< Array of change records */
    uint32_t change_count;               /**< Number of changes */
    uint64_t stored_version;             /**< Version of persisted state */
    uint64_t current_version;            /**< Version of current state */
    bool requires_full_rebuild;          /**< True if diff too large for incremental */
} kg_diff_result_t;

/* ============================================================================
 * Persistence Configuration
 * ============================================================================ */

/**
 * @brief KG persistence configuration
 *
 * WHAT: Complete configuration for persistence layer
 * WHY:  Central configuration point for all persistence settings
 * HOW:  Combines storage, database, and security settings
 */
typedef struct {
    char storage_path[KG_PERSIST_MAX_PATH_LEN]; /**< e.g., ".aim/kg/questdb/" */
    bool enable_auto_save;               /**< Auto-persist on changes */
    uint32_t auto_save_interval_ms;      /**< How often to auto-save (streaming: 0) */
    bool enable_compression;             /**< Compress before encryption */
    bool enable_checksums;               /**< Verify data integrity */

    /* QuestDB configuration */
    kg_questdb_config_t questdb;         /**< Time-series database settings */

    /* Security (REQUIRED for production) */
    kg_encryption_config_t encryption;   /**< Encryption settings */
    bool require_encryption;             /**< Fail if encryption unavailable */
} kg_persistence_config_t;

/* ============================================================================
 * Persistence Lifecycle API
 * ============================================================================ */

/**
 * @brief Create persistence context with configuration
 *
 * WHAT: Initialize persistence layer with full configuration
 * WHY:  Set up database connections, encryption, thread pools
 * HOW:  Parse config, establish connections, initialize security
 *
 * @param config Persistence configuration (NULL for defaults)
 * @return Persistence handle or NULL on error
 */
kg_persistence_t* kg_persistence_create(const kg_persistence_config_t* config);

/**
 * @brief Destroy persistence context and cleanup
 *
 * WHAT: Shutdown persistence layer cleanly
 * WHY:  Release resources, close connections, flush pending writes
 * HOW:  Shutdown threads, close pool, zero sensitive memory
 *
 * @param p Persistence handle (NULL safe)
 */
void kg_persistence_destroy(kg_persistence_t* p);

/* ============================================================================
 * Save/Load API
 * ============================================================================ */

/**
 * @brief Save complete KG to storage
 *
 * WHAT: Persist entire knowledge graph to database
 * WHY:  Full snapshot for recovery or migration
 * HOW:  Serialize nodes/edges, encrypt, write to QuestDB
 *
 * @param p Persistence handle
 * @param kg Brain knowledge graph to save
 * @return 0 on success, -1 on error
 */
int kg_persistence_save(kg_persistence_t* p, const brain_kg_t* kg);

/**
 * @brief Load KG from storage
 *
 * WHAT: Restore knowledge graph from database
 * WHY:  Recovery after restart or migration
 * HOW:  Query QuestDB, decrypt, deserialize into KG structure
 *
 * @param p Persistence handle
 * @return Loaded KG or NULL if not found/error
 */
brain_kg_t* kg_persistence_load(kg_persistence_t* p);

/* ============================================================================
 * Differential Update API
 * ============================================================================ */

/**
 * @brief Compute differential between stored and current state
 *
 * WHAT: Calculate what changed since last persistence
 * WHY:  Enable incremental saves for efficiency
 * HOW:  Compare versions, generate change list
 *
 * @param p Persistence handle
 * @param current Current in-memory KG
 * @param diff Output diff result (caller must free changes array)
 * @return 0 on success, -1 on error
 */
int kg_persistence_compute_diff(kg_persistence_t* p,
                                 const brain_kg_t* current,
                                 kg_diff_result_t* diff);

/**
 * @brief Apply differential changes to KG
 *
 * WHAT: Apply stored changes to in-memory KG
 * WHY:  Incremental update without full reload
 * HOW:  Process change records, update KG structure
 *
 * @param p Persistence handle
 * @param kg KG to update
 * @param diff Changes to apply
 * @return 0 on success, -1 on error
 */
int kg_persistence_apply_diff(kg_persistence_t* p,
                               brain_kg_t* kg,
                               const kg_diff_result_t* diff);

/**
 * @brief Save incremental changes (not full KG)
 *
 * WHAT: Persist only the changes since last save
 * WHY:  More efficient than full saves for small changes
 * HOW:  Write change records to audit trail table
 *
 * @param p Persistence handle
 * @param diff Changes to save
 * @return 0 on success, -1 on error
 */
int kg_persistence_save_incremental(kg_persistence_t* p,
                                     const kg_diff_result_t* diff);

/* ============================================================================
 * Version Management API
 * ============================================================================ */

/**
 * @brief Get stored KG version
 *
 * @param p Persistence handle
 * @return Version number of stored state
 */
uint64_t kg_persistence_get_stored_version(kg_persistence_t* p);

/**
 * @brief Create named checkpoint
 *
 * WHAT: Create a named snapshot for later restore
 * WHY:  Enable rollback to known good state
 * HOW:  Full snapshot with label in metadata
 *
 * @param p Persistence handle
 * @param label Checkpoint name (e.g., "pre-training-v1")
 * @return 0 on success, -1 on error
 */
int kg_persistence_create_checkpoint(kg_persistence_t* p, const char* label);

/**
 * @brief Restore from named checkpoint
 *
 * WHAT: Load KG from previously created checkpoint
 * WHY:  Rollback after failed operation
 * HOW:  Load checkpoint snapshot into provided KG
 *
 * @param p Persistence handle
 * @param label Checkpoint name to restore
 * @param kg Output KG to populate
 * @return 0 on success, -1 on error
 */
int kg_persistence_restore_checkpoint(kg_persistence_t* p, const char* label,
                                       brain_kg_t* kg);

/* ============================================================================
 * Encryption Key Management API
 * ============================================================================ */

/**
 * @brief Initialize encryption subsystem
 *
 * WHAT: Set up encryption with provided configuration
 * WHY:  Required before any encrypted operations
 * HOW:  Initialize security module contexts, load/generate keys
 *
 * @param p Persistence handle
 * @param config Encryption configuration
 * @return 0 on success, -1 on error
 */
int kg_persistence_init_encryption(kg_persistence_t* p,
                                    const kg_encryption_config_t* config);

/**
 * @brief Generate new master key
 *
 * WHAT: Create new cryptographic master key
 * WHY:  Initial setup or key rotation
 * HOW:  Generate using CSPRNG, optionally store in HSM
 *
 * @param output_path Where to store key (NULL if HSM-only)
 * @param algorithm Encryption algorithm to generate key for
 * @return 0 on success, -1 on error
 */
int kg_persistence_generate_master_key(const char* output_path,
                                        kg_crypto_algorithm_t algorithm);

/**
 * @brief Rotate encryption keys
 *
 * WHAT: Generate new keys and re-encrypt data
 * WHY:  Security best practice, compliance requirement
 * HOW:  Generate new keys, re-encrypt with new keys, retire old keys
 *
 * @param p Persistence handle
 * @return 0 on success, -1 on error
 */
int kg_persistence_rotate_keys(kg_persistence_t* p);

/**
 * @brief Verify data integrity
 *
 * WHAT: Check integrity of stored data
 * WHY:  Detect tampering or corruption
 * HOW:  Verify HMACs, checksums, Merkle tree
 *
 * @param p Persistence handle
 * @return 0 if integrity verified, -1 on error or tampering detected
 */
int kg_persistence_verify_integrity(kg_persistence_t* p);

/* ============================================================================
 * HSM Operations API
 * ============================================================================ */

/**
 * @brief Initialize HSM connection
 *
 * WHAT: Connect to hardware security module
 * WHY:  REQUIRED for production security
 * HOW:  Connect via PKCS#11 or cloud provider SDK
 *
 * @param p Persistence handle
 * @param config HSM configuration
 * @return 0 on success, -1 on error
 */
int kg_persistence_init_hsm(kg_persistence_t* p, const kg_hsm_config_t* config);

/**
 * @brief Generate key in HSM
 *
 * WHAT: Create new key inside HSM (never extractable)
 * WHY:  Maximum key security, HSM-resident keys
 * HOW:  PKCS#11 C_GenerateKey or cloud HSM API
 *
 * @param p Persistence handle
 * @param key_label Label for the key in HSM
 * @return 0 on success, -1 on error
 */
int kg_persistence_hsm_generate_key(kg_persistence_t* p, const char* key_label);

/**
 * @brief Import key to HSM
 *
 * WHAT: Import existing key material into HSM
 * WHY:  Migration from software keys to HSM
 * HOW:  Wrap key for transport, import via PKCS#11
 *
 * @param p Persistence handle
 * @param key_material Key bytes to import
 * @param key_size Size of key material
 * @param key_label Label for key in HSM
 * @return 0 on success, -1 on error
 */
int kg_persistence_hsm_import_key(kg_persistence_t* p, const void* key_material,
                                   size_t key_size, const char* key_label);

/**
 * @brief Check if HSM is available
 *
 * @param p Persistence handle
 * @return true if HSM connected and functional
 */
bool kg_persistence_hsm_is_available(const kg_persistence_t* p);

/**
 * @brief Get HSM key information
 *
 * @param p Persistence handle
 * @param info Output key information
 * @return 0 on success, -1 on error
 */
int kg_persistence_hsm_get_key_info(const kg_persistence_t* p,
                                     kg_hsm_key_info_t* info);

/* ============================================================================
 * Audit Logging API
 * ============================================================================ */

/**
 * @brief Initialize audit logging
 *
 * WHAT: Set up security audit trail
 * WHY:  Compliance, forensics, tamper detection
 * HOW:  Configure log destination, encryption, hash chain
 *
 * @param p Persistence handle
 * @param config Audit configuration
 * @return 0 on success, -1 on error
 */
int kg_persistence_audit_init(kg_persistence_t* p, const kg_audit_config_t* config);

/**
 * @brief Log audit event
 *
 * WHAT: Record security-relevant event
 * WHY:  Compliance, incident investigation
 * HOW:  Append to tamper-evident log with timestamp and hash
 *
 * @param p Persistence handle
 * @param event Event type
 * @param details Human-readable event details
 * @return 0 on success, -1 on error
 */
int kg_persistence_audit_log(kg_persistence_t* p, kg_audit_event_type_t event,
                              const char* details);

/**
 * @brief Verify audit log integrity
 *
 * WHAT: Check tamper-evident hash chain
 * WHY:  Detect log manipulation
 * HOW:  Verify each entry's hash against previous
 *
 * @param p Persistence handle
 * @return 0 if chain valid, -1 on error or tampering
 */
int kg_persistence_audit_verify_chain(kg_persistence_t* p);

/**
 * @brief Export audit logs for time range
 *
 * WHAT: Extract audit records for external analysis
 * WHY:  SIEM integration, compliance reporting
 * HOW:  Query audit table, format for export
 *
 * @param p Persistence handle
 * @param output_path Where to write exported logs
 * @param start_time Start of time range (ms since epoch)
 * @param end_time End of time range (ms since epoch)
 * @return 0 on success, -1 on error
 */
int kg_persistence_audit_export(kg_persistence_t* p, const char* output_path,
                                 uint64_t start_time, uint64_t end_time);

/* ============================================================================
 * Secure Memory Utilities
 * ============================================================================ */

/**
 * @brief Securely zero sensitive memory
 *
 * WHAT: Wipe memory containing sensitive data
 * WHY:  Prevent key/password recovery from memory
 * HOW:  Use volatile writes to prevent optimization, verify zeroing
 *
 * @param ptr Memory to zero
 * @param size Number of bytes to zero
 */
void kg_persistence_secure_zero(void* ptr, size_t size);

/* ============================================================================
 * Default Configuration Helpers
 * ============================================================================ */

/**
 * @brief Get default persistence configuration
 *
 * @param config Output configuration (caller-allocated)
 * @return 0 on success
 */
int kg_persistence_default_config(kg_persistence_config_t* config);

/**
 * @brief Get default QuestDB configuration
 *
 * @param config Output configuration (caller-allocated)
 * @return 0 on success
 */
int kg_persistence_default_questdb_config(kg_questdb_config_t* config);

/**
 * @brief Get default encryption configuration
 *
 * @param config Output configuration (caller-allocated)
 * @return 0 on success
 */
int kg_persistence_default_encryption_config(kg_encryption_config_t* config);

/* ============================================================================
 * String Conversion Utilities
 * ============================================================================ */

/**
 * @brief Convert crypto algorithm to string
 */
const char* kg_crypto_algorithm_to_string(kg_crypto_algorithm_t algorithm);

/**
 * @brief Convert HSM type to string
 */
const char* kg_hsm_type_to_string(kg_hsm_type_t type);

/**
 * @brief Convert audit level to string
 */
const char* kg_audit_level_to_string(kg_audit_level_t level);

/**
 * @brief Convert audit event type to string
 */
const char* kg_audit_event_type_to_string(kg_audit_event_type_t event);

/**
 * @brief Convert QuestDB mode to string
 */
const char* kg_questdb_mode_to_string(kg_questdb_mode_t mode);

/**
 * @brief Convert TLS mode to string
 */
const char* kg_tls_mode_to_string(kg_tls_mode_t mode);

/**
 * @brief Convert diff change type to string
 */
const char* kg_diff_change_type_to_string(kg_diff_change_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_KG_PERSISTENCE_H */
