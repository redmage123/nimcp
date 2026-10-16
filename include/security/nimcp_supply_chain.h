/**
 * @file nimcp_supply_chain.h
 * @brief Supply Chain Security for NIMCP
 *
 * WHAT: Dependency integrity verification and Software Bill of Materials (SBOM)
 * WHY: Protect against supply chain attacks, malicious dependencies, and tampering
 * HOW: Hash verification, signature validation, SBOM generation, runtime monitoring
 *
 * Features:
 * - SPDX and CycloneDX SBOM formats
 * - Artifact hash and signature verification
 * - Runtime library integrity monitoring
 * - Trusted source management
 * - Certificate chain validation
 * - Integration with bio-async messaging
 */

#ifndef NIMCP_SUPPLY_CHAIN_H
#define NIMCP_SUPPLY_CHAIN_H

#include "utils/error/nimcp_error_codes.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/validation/nimcp_common.h"
#include "async/nimcp_bio_router.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Magic numbers */
#define NIMCP_SUPPLY_CHAIN_MAGIC    0x53434841  /* "SCHA" */
#define NIMCP_SBOM_MAGIC            0x53424F4D  /* "SBOM" */
#define NIMCP_DEPENDENCY_MAGIC      0x44455044  /* "DEPD" */
#define NIMCP_ARTIFACT_MAGIC        0x41525446  /* "ARTF" */

/* Size limits */
#define NIMCP_MAX_DEPENDENCY_NAME   256
#define NIMCP_MAX_VERSION_STRING    64
#define NIMCP_MAX_LICENSE_STRING    128
#define NIMCP_MAX_SUPPLIER_NAME     256
#define NIMCP_MAX_HASH_STRING       128
#define NIMCP_MAX_URL_LENGTH        1024
#define NIMCP_MAX_DEPENDENCIES      1024

/**
 * Supply chain context (opaque)
 */
typedef struct nimcp_supply_chain* nimcp_supply_chain_t;

/**
 * SBOM context (opaque)
 */
typedef struct nimcp_sbom* nimcp_sbom_t;

/**
 * SBOM format types
 */
typedef enum {
    NIMCP_SBOM_FORMAT_SPDX,       /* SPDX 2.3 format */
    NIMCP_SBOM_FORMAT_CYCLONEDX   /* CycloneDX 1.5 format */
} nimcp_sbom_format_t;

/**
 * Hash algorithm types
 */
typedef enum {
    NIMCP_HASH_SHA256,
    NIMCP_HASH_SHA512,
    NIMCP_HASH_SHA3_256,
    NIMCP_HASH_SHA3_512
} nimcp_hash_algorithm_t;

/**
 * Signature algorithm types
 */
typedef enum {
    NIMCP_SIG_ED25519,
    NIMCP_SIG_RSA_2048,
    NIMCP_SIG_RSA_4096,
    NIMCP_SIG_DILITHIUM_2,
    NIMCP_SIG_DILITHIUM_3,
    NIMCP_SIG_DILITHIUM_5
} nimcp_signature_algorithm_t;

/**
 * Verification status
 */
typedef enum {
    NIMCP_VERIFY_UNKNOWN,
    NIMCP_VERIFY_VALID,
    NIMCP_VERIFY_INVALID,
    NIMCP_VERIFY_EXPIRED,
    NIMCP_VERIFY_REVOKED,
    NIMCP_VERIFY_UNTRUSTED
} nimcp_verification_status_t;

/**
 * Dependency information
 */
typedef struct {
    uint32_t magic;
    char name[NIMCP_MAX_DEPENDENCY_NAME];
    char version[NIMCP_MAX_VERSION_STRING];
    char license[NIMCP_MAX_LICENSE_STRING];
    char hash_sha256[65];  /* Hex string + null */
    char hash_sha512[129]; /* Hex string + null */
    char supplier[NIMCP_MAX_SUPPLIER_NAME];
    char download_url[NIMCP_MAX_URL_LENGTH];
    time_t verified_at;
    nimcp_verification_status_t status;
    bool is_critical;      /* Critical dependency */
    bool is_direct;        /* Direct vs transitive dependency */
} nimcp_dependency_t;

/**
 * Artifact verification result
 */
typedef struct {
    char filepath[NIMCP_MAX_URL_LENGTH];
    nimcp_hash_algorithm_t hash_algo;
    char computed_hash[129];
    char expected_hash[129];
    bool hash_match;
    nimcp_signature_algorithm_t sig_algo;
    nimcp_verification_status_t sig_status;
    time_t verified_at;
    char signer[NIMCP_MAX_SUPPLIER_NAME];
} nimcp_artifact_verification_t;

/**
 * Trusted source configuration
 */
typedef struct {
    char url[NIMCP_MAX_URL_LENGTH];
    char public_key_path[NIMCP_MAX_URL_LENGTH];
    nimcp_signature_algorithm_t sig_algo;
    time_t added_at;
    bool is_active;
} nimcp_trusted_source_t;

/**
 * Supply chain statistics
 */
typedef struct {
    uint64_t total_dependencies;
    uint64_t verified_dependencies;
    uint64_t failed_verifications;
    uint64_t runtime_checks;
    uint64_t integrity_violations;
    uint64_t sbom_generations;
    uint64_t sbom_loads;
    time_t last_verification;
    time_t last_sbom_update;
} nimcp_supply_chain_stats_t;

/**
 * Runtime monitoring configuration
 */
typedef struct {
    bool enable_periodic_checks;
    uint32_t check_interval_seconds;
    bool verify_on_load;
    bool verify_symbol_tables;
    bool detect_modifications;
    bool alert_on_violation;
} nimcp_runtime_monitor_config_t;

/**
 * Supply chain configuration
 */
typedef struct {
    bool enable_logging;
    bool strict_mode;              /* Fail on any verification error */
    nimcp_hash_algorithm_t default_hash_algo;
    nimcp_signature_algorithm_t default_sig_algo;
    nimcp_runtime_monitor_config_t monitor_config;
    const char* sbom_cache_dir;    /* Directory for SBOM caching */
    const char* artifact_cache_dir; /* Directory for artifact caching */
    bio_module_context_t bio_ctx;       /* Optional bio-async integration */
} nimcp_supply_chain_config_t;

/* ========================================================================
 * Context Management
 * ======================================================================== */

/**
 * Create supply chain security context
 *
 * WHAT: Initializes supply chain verification system
 * WHY: Centralized management of dependency verification
 * HOW: Allocates context, loads configuration, sets up monitoring
 *
 * @param config Configuration (NULL for defaults)
 * @return Context handle or NULL on error
 */
nimcp_supply_chain_t nimcp_supply_chain_create(const nimcp_supply_chain_config_t* config);

/**
 * Destroy supply chain context
 *
 * WHAT: Safely releases all supply chain resources
 * WHY: Prevent memory leaks and clean shutdown
 * HOW: Stops monitoring, frees allocations, unregisters from bio-async
 *
 * @param sc Supply chain context to destroy
 */
void nimcp_supply_chain_destroy(nimcp_supply_chain_t sc);

/**
 * Get supply chain statistics
 *
 * @param sc Supply chain context
 * @param stats Output statistics structure
 * @return NIMCP_OK on success
 */
nimcp_error_t nimcp_supply_chain_get_stats(nimcp_supply_chain_t sc,
                                             nimcp_supply_chain_stats_t* stats);

/* ========================================================================
 * Software Bill of Materials (SBOM)
 * ======================================================================== */

/**
 * Load SBOM from file
 *
 * WHAT: Parses and loads SBOM document
 * WHY: Import dependency information for verification
 * HOW: Parses SPDX or CycloneDX format, validates structure
 *
 * @param sc Supply chain context
 * @param filepath Path to SBOM file
 * @param format SBOM format (SPDX or CycloneDX)
 * @return NIMCP_OK on success
 */
nimcp_error_t nimcp_sbom_load(nimcp_supply_chain_t sc,
                               const char* filepath,
                               nimcp_sbom_format_t format);

/**
 * Generate SBOM for current system
 *
 * WHAT: Creates SBOM document from detected dependencies
 * WHY: Document software composition for security audits
 * HOW: Scans loaded libraries, generates SPDX/CycloneDX output
 *
 * @param sc Supply chain context
 * @param format Output format (SPDX or CycloneDX)
 * @param output Output buffer (allocated by function, caller must free)
 * @return NIMCP_OK on success
 */
nimcp_error_t nimcp_sbom_generate(nimcp_supply_chain_t sc,
                                   nimcp_sbom_format_t format,
                                   char** output);

/**
 * Save SBOM to file
 *
 * WHAT: Writes SBOM to disk in specified format
 * WHY: Persist SBOM for sharing and archival
 * HOW: Generates SBOM and writes to file
 *
 * @param sc Supply chain context
 * @param filepath Output file path
 * @param format Output format
 * @return NIMCP_OK on success
 */
nimcp_error_t nimcp_sbom_save(nimcp_supply_chain_t sc,
                               const char* filepath,
                               nimcp_sbom_format_t format);

/**
 * Get dependencies from SBOM
 *
 * WHAT: Retrieves list of dependencies from loaded SBOM
 * WHY: Query dependency information for verification
 * HOW: Returns array of dependency structures
 *
 * @param sc Supply chain context
 * @param deps Output array of dependencies (caller must free)
 * @param count Output number of dependencies
 * @return NIMCP_OK on success
 */
nimcp_error_t nimcp_sbom_get_dependencies(nimcp_supply_chain_t sc,
                                           nimcp_dependency_t** deps,
                                           size_t* count);

/**
 * Add dependency to SBOM
 *
 * WHAT: Manually adds dependency information
 * WHY: Document custom or unlisted dependencies
 * HOW: Validates and adds to internal dependency list
 *
 * @param sc Supply chain context
 * @param dep Dependency information
 * @return NIMCP_OK on success
 */
nimcp_error_t nimcp_sbom_add_dependency(nimcp_supply_chain_t sc,
                                         const nimcp_dependency_t* dep);

/**
 * Remove dependency from SBOM
 *
 * @param sc Supply chain context
 * @param name Dependency name
 * @return NIMCP_OK on success
 */
nimcp_error_t nimcp_sbom_remove_dependency(nimcp_supply_chain_t sc,
                                            const char* name);

/**
 * Query dependency by name
 *
 * @param sc Supply chain context
 * @param name Dependency name
 * @param dep Output dependency information
 * @return NIMCP_OK if found
 */
nimcp_error_t nimcp_sbom_query_dependency(nimcp_supply_chain_t sc,
                                           const char* name,
                                           nimcp_dependency_t* dep);

/* ========================================================================
 * Artifact Verification
 * ======================================================================== */

/**
 * Verify artifact hash
 *
 * WHAT: Computes and compares file hash against expected value
 * WHY: Detect file tampering and corruption
 * HOW: Computes SHA-256/512, compares with expected hash
 *
 * @param sc Supply chain context
 * @param filepath Path to artifact file
 * @param expected_hash Expected hash (hex string)
 * @param algo Hash algorithm to use
 * @return NIMCP_OK if hash matches
 */
nimcp_error_t nimcp_artifact_verify_hash(
    nimcp_supply_chain_t sc,
    const char* filepath,
    const char* expected_hash,
    nimcp_hash_algorithm_t algo
);

/**
 * Verify artifact signature
 *
 * WHAT: Verifies digital signature on artifact
 * WHY: Authenticate artifact origin and integrity
 * HOW: Loads public key, verifies signature using specified algorithm
 *
 * @param sc Supply chain context
 * @param filepath Path to artifact file
 * @param signature_path Path to detached signature file
 * @param public_key_path Path to public key file
 * @param algo Signature algorithm
 * @return NIMCP_OK if signature valid
 */
nimcp_error_t nimcp_artifact_verify_signature(
    nimcp_supply_chain_t sc,
    const char* filepath,
    const char* signature_path,
    const char* public_key_path,
    nimcp_signature_algorithm_t algo
);

/**
 * Verify artifact (hash + signature)
 *
 * WHAT: Performs both hash and signature verification
 * WHY: Complete artifact integrity check
 * HOW: Verifies hash first, then signature if hash valid
 *
 * @param sc Supply chain context
 * @param filepath Path to artifact file
 * @param expected_hash Expected hash (hex string)
 * @param hash_algo Hash algorithm
 * @param signature_path Path to signature file
 * @param public_key_path Path to public key
 * @param sig_algo Signature algorithm
 * @param result Output verification result
 * @return NIMCP_OK if both checks pass
 */
nimcp_error_t nimcp_artifact_verify_full(
    nimcp_supply_chain_t sc,
    const char* filepath,
    const char* expected_hash,
    nimcp_hash_algorithm_t hash_algo,
    const char* signature_path,
    const char* public_key_path,
    nimcp_signature_algorithm_t sig_algo,
    nimcp_artifact_verification_t* result
);

/**
 * Compute artifact hash
 *
 * WHAT: Computes cryptographic hash of file
 * WHY: Generate hash for verification or SBOM
 * HOW: Reads file, computes hash, returns hex string
 *
 * @param sc Supply chain context
 * @param filepath Path to file
 * @param algo Hash algorithm
 * @param hash_output Output hash buffer (hex string, 129 bytes)
 * @return NIMCP_OK on success
 */
nimcp_error_t nimcp_artifact_compute_hash(
    nimcp_supply_chain_t sc,
    const char* filepath,
    nimcp_hash_algorithm_t algo,
    char* hash_output
);

/* ========================================================================
 * Runtime Verification
 * ======================================================================== */

/**
 * Verify loaded library
 *
 * WHAT: Checks integrity of loaded shared library
 * WHY: Detect runtime tampering or malicious library substitution
 * HOW: Computes library hash, compares with SBOM, checks symbols
 *
 * @param sc Supply chain context
 * @param library_path Path to loaded library
 * @return NIMCP_OK if library verified
 */
nimcp_error_t nimcp_runtime_verify_library(nimcp_supply_chain_t sc,
                                             const char* library_path);

/**
 * Verify all loaded libraries
 *
 * WHAT: Checks integrity of all currently loaded libraries
 * WHY: Comprehensive runtime integrity check
 * HOW: Iterates through loaded libraries, verifies each
 *
 * @param sc Supply chain context
 * @return NIMCP_OK if all libraries verified
 */
nimcp_error_t nimcp_runtime_verify_all(nimcp_supply_chain_t sc);

/**
 * Enable runtime monitoring
 *
 * WHAT: Starts background thread for periodic integrity checks
 * WHY: Detect runtime attacks and modifications
 * HOW: Creates thread that periodically verifies all libraries
 *
 * @param sc Supply chain context
 * @return NIMCP_OK on success
 */
nimcp_error_t nimcp_runtime_enable_monitoring(nimcp_supply_chain_t sc);

/**
 * Disable runtime monitoring
 *
 * @param sc Supply chain context
 * @return NIMCP_OK on success
 */
nimcp_error_t nimcp_runtime_disable_monitoring(nimcp_supply_chain_t sc);

/**
 * Verify binary integrity
 *
 * WHAT: Checks integrity of executable binary
 * WHY: Detect binary patching or modification
 * HOW: Computes binary hash, checks code sections
 *
 * @param sc Supply chain context
 * @param binary_path Path to executable
 * @return NIMCP_OK if binary unmodified
 */
nimcp_error_t nimcp_runtime_verify_binary(nimcp_supply_chain_t sc,
                                            const char* binary_path);

/* ========================================================================
 * Trusted Source Management
 * ======================================================================== */

/**
 * Add trusted source
 *
 * WHAT: Registers a trusted source for artifacts
 * WHY: Whitelist known-good artifact sources
 * HOW: Stores source URL and public key for verification
 *
 * @param sc Supply chain context
 * @param source_url Source URL or identifier
 * @param public_key_path Path to source's public key
 * @param sig_algo Signature algorithm used by source
 * @return NIMCP_OK on success
 */
nimcp_error_t nimcp_supply_chain_add_trusted_source(
    nimcp_supply_chain_t sc,
    const char* source_url,
    const char* public_key_path,
    nimcp_signature_algorithm_t sig_algo
);

/**
 * Revoke trusted source
 *
 * WHAT: Removes source from trusted list
 * WHY: Respond to compromised or untrusted sources
 * HOW: Marks source as inactive, rejects future artifacts
 *
 * @param sc Supply chain context
 * @param source_url Source URL to revoke
 * @return NIMCP_OK on success
 */
nimcp_error_t nimcp_supply_chain_revoke_source(nimcp_supply_chain_t sc,
                                                 const char* source_url);

/**
 * List trusted sources
 *
 * @param sc Supply chain context
 * @param sources Output array of trusted sources (caller must free)
 * @param count Output number of sources
 * @return NIMCP_OK on success
 */
nimcp_error_t nimcp_supply_chain_list_sources(nimcp_supply_chain_t sc,
                                                nimcp_trusted_source_t** sources,
                                                size_t* count);

/**
 * Check if source is trusted
 *
 * @param sc Supply chain context
 * @param source_url Source URL to check
 * @return true if trusted
 */
bool nimcp_supply_chain_is_source_trusted(nimcp_supply_chain_t sc,
                                            const char* source_url);

/* ========================================================================
 * Certificate and Timestamp Verification
 * ======================================================================== */

/**
 * Verify certificate chain
 *
 * WHAT: Validates X.509 certificate chain
 * WHY: Ensure artifact signer is trusted
 * HOW: Checks chain from leaf to root, validates expiry
 *
 * @param sc Supply chain context
 * @param cert_path Path to certificate file
 * @param ca_path Path to CA bundle
 * @return NIMCP_OK if chain valid
 */
nimcp_error_t nimcp_cert_verify_chain(
    nimcp_supply_chain_t sc,
    const char* cert_path,
    const char* ca_path
);

/**
 * Verify timestamp
 *
 * WHAT: Validates timestamp on signed artifact
 * WHY: Ensure signature was created before key revocation
 * HOW: Checks timestamp signature, validates against current time
 *
 * @param sc Supply chain context
 * @param timestamp_path Path to timestamp token
 * @param artifact_path Path to timestamped artifact
 * @return NIMCP_OK if timestamp valid
 */
nimcp_error_t nimcp_timestamp_verify(
    nimcp_supply_chain_t sc,
    const char* timestamp_path,
    const char* artifact_path
);

/**
 * Check revocation status
 *
 * WHAT: Queries certificate revocation status
 * WHY: Detect revoked certificates
 * HOW: Checks CRL or OCSP for certificate status
 *
 * @param sc Supply chain context
 * @param cert_path Path to certificate
 * @param crl_path Path to CRL file (optional)
 * @param ocsp_url OCSP responder URL (optional)
 * @return NIMCP_OK if not revoked
 */
nimcp_error_t nimcp_cert_check_revocation(
    nimcp_supply_chain_t sc,
    const char* cert_path,
    const char* crl_path,
    const char* ocsp_url
);

/* ========================================================================
 * Utility Functions
 * ======================================================================== */

/**
 * Scan directory for dependencies
 *
 * WHAT: Recursively scans directory for library files
 * WHY: Auto-discover dependencies for SBOM generation
 * HOW: Walks directory tree, identifies libraries, extracts metadata
 *
 * @param sc Supply chain context
 * @param directory Directory to scan
 * @return NIMCP_OK on success
 */
nimcp_error_t nimcp_supply_chain_scan_directory(
    nimcp_supply_chain_t sc,
    const char* directory
);

/**
 * Export verification report
 *
 * WHAT: Generates human-readable verification report
 * WHY: Document verification results for audit
 * HOW: Creates JSON/text report of all verifications
 *
 * @param sc Supply chain context
 * @param format Output format ("json", "text", "html")
 * @param output Output buffer (allocated by function)
 * @return NIMCP_OK on success
 */
nimcp_error_t nimcp_supply_chain_export_report(
    nimcp_supply_chain_t sc,
    const char* format,
    char** output
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SUPPLY_CHAIN_H */
