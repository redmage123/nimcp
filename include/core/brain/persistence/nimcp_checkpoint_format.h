/**
 * @file nimcp_checkpoint_format.h
 * @brief Unified checkpoint file format — single file for brain + all sidecars
 *
 * WHAT: Section-based binary format that merges 13 separate files into one
 * WHY:  Multi-file saves caused 42 hours of training loss when sidecars were missed.
 *       Single-file atomic save eliminates partial checkpoint risk.
 * HOW:  Header + contiguous section data + section table at end.
 *       Backward compatible: auto-detects old "NIMC" format and loads legacy.
 */

#ifndef NIMCP_CHECKPOINT_FORMAT_H
#define NIMCP_CHECKPOINT_FORMAT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * Constants
 *=============================================================================*/

#define NIMCP_UNIFIED_MAGIC      0x4E494D56  /**< "NIMV" — unified format */
#define NIMCP_LEGACY_MAGIC       0x4E494D43  /**< "NIMC" — old format */
#define NIMCP_UNIFIED_VERSION    1
#define NIMCP_MAX_SECTIONS       16
#define NIMCP_SECTION_NAME_LEN   32
#define NIMCP_CHECKPOINT_HEADER_SIZE 64

/** Layout version — stored in reserved[0..3] of checkpoint header.
 *  Bump this when brain_config_t or neuron_t layout changes in a way
 *  that makes old checkpoints incompatible.
 *  0 = pre-SNN-primary (2M ANN, 768-neuron toy SNN)
 *  2 = SNN-primary (150K ANN, 1.8M hierarchical SNN, scaled CNN embeddings) */
#define NIMCP_LAYOUT_VERSION     2

/*=============================================================================
 * Structs
 *=============================================================================*/

/**
 * @brief Unified checkpoint file header (exactly 64 bytes)
 */
typedef struct {
    uint32_t magic;                     /**< NIMCP_UNIFIED_MAGIC */
    uint32_t format_version;            /**< NIMCP_UNIFIED_VERSION */
    uint32_t num_sections;              /**< Number of sections in file */
    uint32_t flags;                     /**< Reserved for future: compression, encryption */
    uint64_t section_table_offset;      /**< Byte offset of section table from file start */
    uint64_t total_size;                /**< Total file size in bytes */
    uint32_t checksum;                  /**< CRC32 of all section data (header excluded) */
    uint8_t  reserved[28];             /**< Padding to 64 bytes (4+4+4+4+8+8+4+28=64) */
} nimcp_checkpoint_header_t;

/**
 * @brief Section table entry (exactly 48 bytes)
 */
typedef struct {
    char     name[NIMCP_SECTION_NAME_LEN]; /**< Section name (null-terminated) */
    uint64_t offset;                       /**< Byte offset from file start */
    uint64_t size;                         /**< Section size in bytes */
} nimcp_section_entry_t;

/*=============================================================================
 * Section Name Constants
 *=============================================================================*/

/* Section name string constants — use NIMCP_ prefix to avoid collisions */
#define NIMCP_SEC_BRAIN_CORE    "brain_core"
#define NIMCP_SEC_META          "meta"
#define NIMCP_SEC_SNN           "snn"
#define NIMCP_SEC_LNN           "lnn"
#define NIMCP_SEC_CNN           "cnn"
#define NIMCP_SEC_CORTEX_VIS    "cortex_visual"
#define NIMCP_SEC_CORTEX_AUD    "cortex_audio"
#define NIMCP_SEC_CORTEX_SPE    "cortex_speech"
#define NIMCP_SEC_CORTEX_SOM    "cortex_somato"
#define NIMCP_SEC_MIRROR        "mirror_neurons"
#define NIMCP_SEC_EXECUTIVE     "executive"
#define NIMCP_SEC_TOKENIZER     "tokenizer"

/*=============================================================================
 * API
 *=============================================================================*/

/* Forward declaration */
typedef struct brain_struct* brain_t;

/**
 * @brief Save brain to unified checkpoint (single file, all sections)
 *
 * Syncs GPU weights to CPU, then writes header + all sections + section table.
 * Uses atomic temp-file + rename pattern for crash safety.
 *
 * @param brain  Internal brain handle
 * @param filepath  Output file path
 * @return true on success
 */
bool brain_save_unified(brain_t brain, const char* filepath);

/**
 * @brief Load brain from unified checkpoint
 *
 * Reads section table, dispatches each section to the appropriate loader.
 * Missing sections are tolerated (logged as warnings).
 *
 * @param filepath  Input file path
 * @return Loaded brain, or NULL on error
 */
brain_t brain_load_unified(const char* filepath);

/**
 * @brief Auto-detect format and load
 *
 * Reads first 4 bytes:
 *   "NIMV" → unified format (brain_load_unified)
 *   "NIMC" → legacy format (brain_load + sidecar search)
 *
 * @param filepath  Input file path
 * @return Loaded brain, or NULL on error
 */
brain_t brain_load_auto(const char* filepath);

/**
 * @brief CRC32 computation for checkpoint integrity
 */
uint32_t nimcp_crc32(const void* data, size_t length);
uint32_t nimcp_crc32_file(FILE* fp, uint64_t offset, uint64_t length);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CHECKPOINT_FORMAT_H */
