/**
 * @file nimcp_immune_persistence.h
 * @brief Immunological Memory Persistence for Brain Immune System
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Persistence layer for brain immune system memory (B cells, T cells, antibodies, antigens)
 * WHY:  Enable cross-session threat pattern learning, disaster recovery, and immune memory transfer
 * HOW:  Binary serialization with versioning, compression, encryption, and incremental updates
 *
 * BIOLOGICAL BASIS:
 * Immune memory is the hallmark of adaptive immunity. B and T memory cells persist for years,
 * enabling rapid secondary responses to previously encountered threats. This module enables
 * NIMCP's immune system to "remember" threats across sessions, mimicking biological immune memory.
 *
 * FEATURES:
 * - Full immune system save/load (all cells, antigens, antibodies, inflammation sites)
 * - Incremental updates (save only changed memory cells)
 * - Version compatibility checking
 * - Optional compression (zlib if available)
 * - Optional encryption (AES-256 if available)
 * - Atomic operations (partial writes don't corrupt state)
 * - Cross-session threat learning persistence
 *
 * FILE FORMAT:
 * ```
 * [HEADER: 64 bytes]
 *   - Magic: "NIMCPIMM" (8 bytes)
 *   - Version: uint32_t (4 bytes)
 *   - Flags: uint32_t (4 bytes) - compression, encryption bits
 *   - Timestamp: uint64_t (8 bytes)
 *   - Checksum: uint32_t (4 bytes)
 *   - Reserved: 36 bytes
 * [COUNTS: 32 bytes]
 *   - antigen_count: uint32_t
 *   - b_cell_count: uint32_t
 *   - t_cell_count: uint32_t
 *   - antibody_count: uint32_t
 *   - cytokine_count: uint32_t
 *   - inflammation_count: uint32_t
 *   - reserved: 8 bytes
 * [DATA SECTIONS]
 *   - Antigens: array of brain_antigen_t
 *   - B Cells: array of brain_b_cell_t
 *   - T Cells: array of brain_t_cell_t
 *   - Antibodies: array of brain_antibody_t
 *   - Cytokines: array of brain_cytokine_t
 *   - Inflammation Sites: array of brain_inflammation_site_t
 *   - Statistics: brain_immune_stats_t
 * ```
 *
 * USAGE EXAMPLE:
 * ```c
 * // Save immune memory to disk
 * immune_persistence_config_t config;
 * immune_persistence_default_config(&config);
 * config.enable_compression = true;
 * immune_persistence_save(immune_system, "immune_memory.dat", &config);
 *
 * // Load immune memory on startup
 * immune_persistence_load(immune_system, "immune_memory.dat", &config);
 *
 * // Incremental save (only memory cells changed since last save)
 * immune_persistence_save_incremental(immune_system, "immune_memory.dat", &config);
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_IMMUNE_PERSISTENCE_H
#define NIMCP_IMMUNE_PERSISTENCE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "cognitive/immune/nimcp_brain_immune.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define IMMUNE_PERSISTENCE_MAGIC         "NIMCPIMM"  /**< File magic header */
#define IMMUNE_PERSISTENCE_MAGIC_LEN     8           /**< Magic header length */
#define IMMUNE_PERSISTENCE_VERSION       1           /**< Current format version */
#define IMMUNE_PERSISTENCE_HEADER_SIZE   64          /**< Header size in bytes */

/* File format flags */
#define IMMUNE_FORMAT_FLAG_COMPRESSED    0x00000001  /**< Data is compressed */
#define IMMUNE_FORMAT_FLAG_ENCRYPTED     0x00000002  /**< Data is encrypted */
#define IMMUNE_FORMAT_FLAG_INCREMENTAL   0x00000004  /**< Incremental update */
#define IMMUNE_FORMAT_FLAG_MEMORY_ONLY   0x00000008  /**< Memory cells only */

/* Maximum string lengths */
#define IMMUNE_PERSIST_MAX_PATH          512         /**< Max filepath length */
#define IMMUNE_PERSIST_MAX_DESC          256         /**< Max description length */

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Immune persistence configuration
 *
 * WHAT: Configuration for immune memory persistence operations
 * WHY:  Customize save/load behavior (compression, encryption, selective saves)
 * HOW:  Pass to save/load functions to control behavior
 */
typedef struct {
    /* Compression/Encryption */
    bool enable_compression;         /**< Use zlib compression if available */
    bool enable_encryption;          /**< Use AES-256 encryption if available */
    uint8_t encryption_key[32];      /**< Encryption key (256-bit) */
    bool encryption_key_set;         /**< Whether encryption key is valid */

    /* Selective save options */
    bool save_antigens;              /**< Save antigen pool */
    bool save_b_cells;               /**< Save B cells */
    bool save_t_cells;               /**< Save T cells */
    bool save_antibodies;            /**< Save antibodies */
    bool save_cytokines;             /**< Save cytokines */
    bool save_inflammation;          /**< Save inflammation sites */
    bool save_statistics;            /**< Save statistics */

    /* Memory-only mode */
    bool memory_cells_only;          /**< Save only memory B/T cells */

    /* Validation */
    bool verify_on_load;             /**< Verify checksum on load */
    bool strict_version_check;       /**< Require exact version match */

    /* Backup */
    bool create_backup;              /**< Create .bak before overwriting */
    char backup_suffix[16];          /**< Backup file suffix (default ".bak") */
} immune_persistence_config_t;

/**
 * @brief Persistence file header
 *
 * WHAT: Metadata header for immune persistence files
 * WHY:  Version checking, validation, format detection
 * HOW:  Written at start of every persistence file
 */
typedef struct {
    char magic[IMMUNE_PERSISTENCE_MAGIC_LEN];  /**< Magic identifier */
    uint32_t version;                          /**< Format version */
    uint32_t flags;                            /**< Format flags */
    uint64_t timestamp;                        /**< Save timestamp (ms) */
    uint32_t checksum;                         /**< CRC32 checksum of data */
    uint32_t reserved1;                        /**< Reserved for future use */
    uint64_t file_size;                        /**< Total file size */
    uint64_t reserved2[3];                     /**< Reserved for future use */
} immune_persistence_header_t;

/**
 * @brief Persistence section counts
 *
 * WHAT: Counts of each immune component for array allocation
 * WHY:  Pre-allocation before loading data arrays
 * HOW:  Read after header, before data sections
 */
typedef struct {
    uint32_t antigen_count;          /**< Number of antigens */
    uint32_t b_cell_count;           /**< Number of B cells */
    uint32_t t_cell_count;           /**< Number of T cells */
    uint32_t antibody_count;         /**< Number of antibodies */
    uint32_t cytokine_count;         /**< Number of cytokines */
    uint32_t inflammation_count;     /**< Number of inflammation sites */
    uint32_t reserved[2];            /**< Reserved for future use */
} immune_persistence_counts_t;

/**
 * @brief Persistence operation result
 *
 * WHAT: Detailed result of save/load operation
 * WHY:  Provide diagnostics and statistics to caller
 * HOW:  Filled by save/load functions
 */
typedef struct {
    bool success;                    /**< Operation succeeded */
    uint32_t version_loaded;         /**< Version of loaded file */
    uint64_t bytes_written;          /**< Bytes written to disk */
    uint64_t bytes_read;             /**< Bytes read from disk */
    uint64_t save_time_ms;           /**< Time taken for save (ms) */
    uint64_t load_time_ms;           /**< Time taken for load (ms) */
    uint32_t items_saved;            /**< Total items saved */
    uint32_t items_loaded;           /**< Total items loaded */
    char error_message[256];         /**< Error description if failed */
} immune_persistence_result_t;

/* ============================================================================
 * Configuration API
 * ============================================================================ */

/**
 * @brief Get default persistence configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with good defaults
 * HOW:  Return struct with all flags enabled, no encryption
 *
 * DEFAULTS:
 * - Compression: disabled (for speed)
 * - Encryption: disabled (no key)
 * - Save all components: enabled
 * - Verification: enabled
 * - Strict version check: disabled (allow minor version differences)
 * - Backup: enabled
 *
 * @param config Output configuration (non-NULL)
 * @return 0 on success, -1 on error
 */
int immune_persistence_default_config(immune_persistence_config_t* config);

/**
 * @brief Set encryption key for persistence
 *
 * WHAT: Configure encryption key for save/load operations
 * WHY:  Protect sensitive immune memory data
 * HOW:  Copy key to config, set encryption flag
 *
 * @param config Configuration to update (non-NULL)
 * @param key Encryption key (32 bytes for AES-256)
 * @param key_len Key length (must be 32)
 * @return 0 on success, -1 on error
 */
int immune_persistence_set_encryption_key(
    immune_persistence_config_t* config,
    const uint8_t* key,
    size_t key_len
);

/* ============================================================================
 * Save/Load API
 * ============================================================================ */

/**
 * @brief Save complete immune system state to file
 *
 * WHAT: Serialize entire immune system to disk
 * WHY:  Enable cross-session memory persistence, disaster recovery
 * HOW:  Write header → counts → data sections with optional compression/encryption
 *
 * THREAD SAFETY: Acquires system mutex during save
 * ATOMICITY: Writes to temp file, then renames (atomic on POSIX)
 *
 * SAVED DATA:
 * - All antigens (active and neutralized)
 * - All B cells (naive, activated, plasma, memory)
 * - All T cells (helper, killer, regulatory, memory)
 * - All antibodies (active and inactive)
 * - All cytokines (delivered and pending)
 * - All inflammation sites
 * - System statistics
 *
 * @param system Immune system to save (non-NULL)
 * @param filepath Path to save to (non-NULL)
 * @param config Configuration (NULL for defaults)
 * @return 0 on success, -1 on error
 */
int immune_persistence_save(
    brain_immune_system_t* system,
    const char* filepath,
    const immune_persistence_config_t* config
);

/**
 * @brief Load immune system state from file
 *
 * WHAT: Restore immune system from disk
 * WHY:  Restore learned immunity from previous sessions
 * HOW:  Read header → validate → read counts → load data sections
 *
 * THREAD SAFETY: Acquires system mutex during load
 * VALIDATION: All loaded data validated before use:
 * - Header magic and version checked
 * - Counts checked against capacity limits
 * - Checksum verified (if enabled)
 * - Enum values validated
 *
 * BEHAVIOR:
 * - Clears existing immune state before loading
 * - Preserves system configuration
 * - Restores IDs and timestamps exactly as saved
 *
 * @param system Immune system to load into (non-NULL)
 * @param filepath Path to load from (non-NULL)
 * @param config Configuration (NULL for defaults)
 * @return 0 on success, -1 on error
 */
int immune_persistence_load(
    brain_immune_system_t* system,
    const char* filepath,
    const immune_persistence_config_t* config
);

/**
 * @brief Save only changed memory cells (incremental update)
 *
 * WHAT: Save only memory B/T cells changed since last save
 * WHY:  Faster saves for frequent checkpointing
 * HOW:  Track modification times, save only modified cells
 *
 * OPTIMIZATION: Only saves cells with:
 * - state == B_CELL_MEMORY or T_CELL_MEMORY
 * - activation_time > last_save_time
 *
 * INCREMENTAL FORMAT:
 * - Sets IMMUNE_FORMAT_FLAG_INCREMENTAL flag
 * - Includes timestamp of base save
 * - Can be merged with base save on load
 *
 * @param system Immune system (non-NULL)
 * @param filepath Path to incremental file (non-NULL)
 * @param config Configuration (NULL for defaults)
 * @return 0 on success, -1 on error
 */
int immune_persistence_save_incremental(
    brain_immune_system_t* system,
    const char* filepath,
    const immune_persistence_config_t* config
);

/**
 * @brief Save with detailed result information
 *
 * WHAT: Save immune system with detailed diagnostics
 * WHY:  Get statistics and error details
 * HOW:  Same as immune_persistence_save but fills result struct
 *
 * @param system Immune system (non-NULL)
 * @param filepath Path to save to (non-NULL)
 * @param config Configuration (NULL for defaults)
 * @param result Output result structure (non-NULL)
 * @return 0 on success, -1 on error
 */
int immune_persistence_save_ex(
    brain_immune_system_t* system,
    const char* filepath,
    const immune_persistence_config_t* config,
    immune_persistence_result_t* result
);

/**
 * @brief Load with detailed result information
 *
 * WHAT: Load immune system with detailed diagnostics
 * WHY:  Get statistics and error details
 * HOW:  Same as immune_persistence_load but fills result struct
 *
 * @param system Immune system (non-NULL)
 * @param filepath Path to load from (non-NULL)
 * @param config Configuration (NULL for defaults)
 * @param result Output result structure (non-NULL)
 * @return 0 on success, -1 on error
 */
int immune_persistence_load_ex(
    brain_immune_system_t* system,
    const char* filepath,
    const immune_persistence_config_t* config,
    immune_persistence_result_t* result
);

/* ============================================================================
 * Validation API
 * ============================================================================ */

/**
 * @brief Get persistence format version
 *
 * WHAT: Return current persistence format version
 * WHY:  Check compatibility before save/load
 * HOW:  Return compile-time constant
 *
 * @return Current persistence format version
 */
uint32_t immune_persistence_get_version(void);

/**
 * @brief Validate persistence file
 *
 * WHAT: Check if file is valid immune persistence file
 * WHY:  Verify file before loading, detect corruption
 * HOW:  Read header, validate magic/version/checksum
 *
 * CHECKS:
 * - File exists and is readable
 * - Magic header matches "NIMCPIMM"
 * - Version is compatible
 * - Checksum is valid (if verify_checksum enabled)
 * - File size matches header
 *
 * @param filepath Path to validate (non-NULL)
 * @param verify_checksum Whether to verify checksum
 * @return 0 if valid, -1 if invalid
 */
int immune_persistence_validate_file(
    const char* filepath,
    bool verify_checksum
);

/**
 * @brief Check version compatibility
 *
 * WHAT: Check if file version is compatible with current version
 * WHY:  Determine if file can be loaded
 * HOW:  Compare major version numbers
 *
 * COMPATIBILITY RULES:
 * - Same major version: compatible
 * - Different major version: incompatible
 * - Minor version differences: compatible (with warnings)
 *
 * @param file_version Version from file header
 * @return true if compatible, false otherwise
 */
bool immune_persistence_is_version_compatible(uint32_t file_version);

/**
 * @brief Get file information without loading
 *
 * WHAT: Read file header and counts without loading data
 * WHY:  Preview file contents, check size before loading
 * HOW:  Read header and counts section only
 *
 * @param filepath Path to file (non-NULL)
 * @param header Output header (can be NULL)
 * @param counts Output counts (can be NULL)
 * @return 0 on success, -1 on error
 */
int immune_persistence_get_file_info(
    const char* filepath,
    immune_persistence_header_t* header,
    immune_persistence_counts_t* counts
);

/* ============================================================================
 * Utility API
 * ============================================================================ */

/**
 * @brief Create backup of persistence file
 *
 * WHAT: Copy persistence file to backup location
 * WHY:  Prevent data loss on save failure
 * HOW:  Copy file to {filepath}.bak before save
 *
 * @param filepath Original file path (non-NULL)
 * @param backup_suffix Backup suffix (NULL for ".bak")
 * @return 0 on success, -1 on error
 */
int immune_persistence_create_backup(
    const char* filepath,
    const char* backup_suffix
);

/**
 * @brief Merge incremental save into base save
 *
 * WHAT: Merge incremental updates into base file
 * WHY:  Consolidate incremental saves, reduce file count
 * HOW:  Load base → load incremental → save merged
 *
 * @param base_filepath Base save file (non-NULL)
 * @param incremental_filepath Incremental save file (non-NULL)
 * @param output_filepath Output merged file (non-NULL)
 * @param config Configuration (NULL for defaults)
 * @return 0 on success, -1 on error
 */
int immune_persistence_merge_incremental(
    const char* base_filepath,
    const char* incremental_filepath,
    const char* output_filepath,
    const immune_persistence_config_t* config
);

/**
 * @brief Clear immune system state (for testing)
 *
 * WHAT: Reset immune system to initial state
 * WHY:  Testing, reset after load failure
 * HOW:  Clear all arrays, reset counters
 *
 * @param system Immune system (non-NULL)
 * @return 0 on success, -1 on error
 */
int immune_persistence_clear_state(brain_immune_system_t* system);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_IMMUNE_PERSISTENCE_H */
