/**
 * @file nimcp_reasoning_kb_persistence.h
 * @brief Knowledge Base Persistence — serialization/deserialization of KB state
 *
 * SINGLE RESPONSIBILITY: Export and import knowledge base facts/rules to/from
 *                        binary or text buffers and files.
 *
 * WHAT: Serialize/deserialize the symbolic-logic and quantum-reasoning KBs
 * WHY:  Derived facts and rules are lost between sessions; persistence enables
 *       checkpointing, transfer, and offline inspection of knowledge bases
 * HOW:  Binary format (header + entries) and human-readable text format;
 *       CRC32-style checksum for integrity; conflict resolution on import
 *
 * SRP ADHERENCE:
 * - ONLY handles KB serialization/deserialization
 * - Does NOT perform inference (see forward_chaining.h, backward_chaining.h)
 * - Does NOT manage KB content (see knowledge_base_interface.h)
 * - Does NOT create engines (see reasoning_factory.h)
 *
 * INTEGRATION POINTS:
 * - Quantum reasoning KB (qreason_kb_t): via qreason_get_fact/qreason_set_fact
 * - Symbolic logic KB: via symbolic_logic_t accessors
 * - File I/O: standard C fopen/fwrite/fread/fclose
 *
 * @author NIMCP Development Team
 * @date 2026-02-26
 * @version 1.0.0
 */

#ifndef NIMCP_REASONING_KB_PERSISTENCE_H
#define NIMCP_REASONING_KB_PERSISTENCE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

/** Magic number identifying a NIMCP KB persistence buffer ("KBP1") */
#define KB_PERSISTENCE_MAGIC   0x4B425031u

/** Current persistence format version */
#define KB_PERSISTENCE_VERSION 1u

/** Maximum length of a fact description string (including NUL) */
#define KB_MAX_FACT_LEN  256

/** Maximum length of a rule description string (including NUL) */
#define KB_MAX_RULE_LEN  512

/*=============================================================================
 * ENUMERATIONS
 *===========================================================================*/

/**
 * @brief Serialization format
 */
typedef enum {
    KB_FORMAT_BINARY = 0,   /**< Compact binary format */
    KB_FORMAT_TEXT   = 1    /**< Human-readable text format */
} kb_format_t;

/*=============================================================================
 * DATA STRUCTURES
 *===========================================================================*/

/**
 * @brief Persistence configuration
 *
 * WHAT: Tuneable parameters for KB export
 * WHY:  Allow callers to control what is included in the export
 */
typedef struct {
    kb_format_t format;           /**< Binary or text (default: binary) */
    bool include_derived_facts;   /**< Include derived facts (default: true) */
    bool include_confidences;     /**< Include confidence values (default: true) */
    bool compress;                /**< Reserved for future LZ4 (default: false) */
} kb_persistence_config_t;

/**
 * @brief Binary file/buffer header
 *
 * WHAT: Fixed-size header at the start of every binary KB buffer
 * WHY:  Enable validation, versioning, and integrity checks
 */
typedef struct {
    uint32_t magic;       /**< Must be KB_PERSISTENCE_MAGIC */
    uint32_t version;     /**< Must be KB_PERSISTENCE_VERSION */
    uint32_t num_facts;   /**< Number of fact entries that follow */
    uint32_t num_rules;   /**< Number of rule entries that follow */
    uint64_t timestamp;   /**< Unix epoch seconds when exported */
    uint32_t checksum;    /**< FNV-1a over fact+rule data (not header) */
} kb_header_t;

/**
 * @brief Serialized fact entry
 */
typedef struct {
    uint32_t variable_id;                /**< Variable/fact index */
    uint8_t  truth_value;                /**< 0=FALSE, 1=TRUE, 2=UNKNOWN */
    float    confidence;                 /**< Confidence [0,1] */
    char     description[KB_MAX_FACT_LEN]; /**< Human-readable label */
    bool     is_derived;                 /**< True if derived by inference */
} kb_fact_entry_t;

/**
 * @brief Serialized rule entry
 */
typedef struct {
    uint32_t antecedents[8];  /**< Antecedent variable IDs */
    uint32_t num_antecedents; /**< Number of valid antecedents */
    uint32_t consequent;      /**< Consequent variable ID */
    float    confidence;      /**< Rule confidence [0,1] */
    char     description[KB_MAX_RULE_LEN]; /**< Human-readable label */
} kb_rule_entry_t;

/**
 * @brief Export result metadata
 */
typedef struct {
    uint32_t num_facts_exported;  /**< Facts written */
    uint32_t num_rules_exported;  /**< Rules written */
    size_t   bytes_written;       /**< Total bytes produced */
    uint32_t checksum;            /**< Checksum of data section */
} kb_export_result_t;

/**
 * @brief Import result metadata
 */
typedef struct {
    uint32_t num_facts_imported;  /**< Facts read and applied */
    uint32_t num_rules_imported;  /**< Rules read and applied */
    uint32_t num_conflicts;       /**< Conflicts resolved (higher-confidence wins) */
    size_t   bytes_read;          /**< Total bytes consumed */
} kb_import_result_t;

/*=============================================================================
 * API FUNCTIONS
 *===========================================================================*/

/**
 * @brief Get default persistence configuration
 *
 * WHAT: Return configuration with sensible defaults
 * WHY:  Simplify export calls with proven parameters
 * HOW:  Static initialization
 *
 * @return Default configuration (binary format, all facts, confidences, no compress)
 *
 * COMPLEXITY: O(1)
 */
kb_persistence_config_t reasoning_kb_default_config(void);

/**
 * @brief Export knowledge base to a heap-allocated buffer
 *
 * WHAT: Serialize KB facts and rules into a buffer
 * WHY:  Enable in-memory transfer and validation before file write
 * HOW:  Allocate buffer with nimcp_malloc, write header + entries
 *
 * The kb_source is a pointer to a qreason_t (quantum reasoning context).
 * Caller must free *buffer_out with nimcp_free().
 *
 * @param kb_source   Opaque KB source (qreason_t)
 * @param config      Persistence configuration (NULL = defaults)
 * @param buffer_out  Output: heap-allocated buffer (caller frees)
 * @param size_out    Output: buffer size in bytes
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(F + R) where F=facts, R=rules
 * THREAD-SAFE: No
 * MALLOC: Yes (output buffer)
 */
int reasoning_kb_export_to_buffer(
    const void* kb_source,
    const kb_persistence_config_t* config,
    uint8_t** buffer_out,
    size_t* size_out
);

/**
 * @brief Import knowledge base from a buffer
 *
 * WHAT: Deserialize KB facts and rules from a buffer
 * WHY:  Restore previously-exported knowledge base state
 * HOW:  Validate header, read entries, apply to KB with conflict resolution
 *
 * The kb_target is a pointer to a qreason_t (quantum reasoning context).
 * Conflict resolution: if a fact already exists with a different truth value,
 * the version with higher confidence wins.
 *
 * @param kb_target   Opaque KB target (qreason_t)
 * @param buffer      Input buffer (from export or file read)
 * @param size        Buffer size in bytes
 * @param result      Output: import statistics (can be NULL)
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(F + R)
 * THREAD-SAFE: No
 */
int reasoning_kb_import_from_buffer(
    void* kb_target,
    const uint8_t* buffer,
    size_t size,
    kb_import_result_t* result
);

/**
 * @brief Save knowledge base to a file
 *
 * WHAT: Export KB and write to disk
 * WHY:  Persist knowledge base between sessions
 * HOW:  Export to buffer, write buffer to file via fwrite
 *
 * @param kb_source   Opaque KB source (qreason_t)
 * @param filepath    Output file path
 * @param config      Persistence configuration (NULL = defaults)
 * @param result      Output: export statistics (can be NULL)
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(F + R)
 * THREAD-SAFE: No
 */
int reasoning_kb_save_to_file(
    const void* kb_source,
    const char* filepath,
    const kb_persistence_config_t* config,
    kb_export_result_t* result
);

/**
 * @brief Load knowledge base from a file
 *
 * WHAT: Read file and import into KB
 * WHY:  Restore knowledge base from persistent storage
 * HOW:  Read file into buffer, validate, import via reasoning_kb_import_from_buffer
 *
 * @param kb_target   Opaque KB target (qreason_t)
 * @param filepath    Input file path
 * @param result      Output: import statistics (can be NULL)
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(F + R)
 * THREAD-SAFE: No
 */
int reasoning_kb_load_from_file(
    void* kb_target,
    const char* filepath,
    kb_import_result_t* result
);

/**
 * @brief Validate a KB persistence buffer
 *
 * WHAT: Check magic, version, and checksum of a buffer
 * WHY:  Detect corruption or incompatible versions before import
 * HOW:  Verify header fields, recalculate and compare checksum
 *
 * @param buffer   Buffer to validate
 * @param size     Buffer size in bytes
 * @return 0 if valid, -1 if invalid
 *
 * COMPLEXITY: O(N) where N = buffer size
 * THREAD-SAFE: Yes (read-only)
 */
int reasoning_kb_validate_buffer(
    const uint8_t* buffer,
    size_t size
);

/**
 * @brief Calculate checksum over buffer data section
 *
 * WHAT: Compute FNV-1a hash of the data portion (after header)
 * WHY:  Enable integrity verification
 * HOW:  FNV-1a 32-bit hash over fact + rule entries
 *
 * @param buffer   Full buffer (including header)
 * @param size     Buffer size in bytes
 * @return FNV-1a checksum, or 0 if buffer is too small
 *
 * COMPLEXITY: O(N)
 * THREAD-SAFE: Yes (read-only)
 */
uint32_t reasoning_kb_get_checksum(
    const uint8_t* buffer,
    size_t size
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_REASONING_KB_PERSISTENCE_H */
