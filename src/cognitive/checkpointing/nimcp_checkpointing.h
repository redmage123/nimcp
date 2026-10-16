/**
 * @file nimcp_checkpointing.h
 * @brief Automatic checkpointing and fault tolerance
 * @version 2.7.0
 * @date 2025-11-04
 *
 * Enhancement 5: Automatic Checkpointing
 *
 * WHAT: Periodic brain state snapshots for fault tolerance
 * WHY: Resume training after crash, experiment tracking, reproducibility
 * HOW: Auto-save every N steps, atomic writes, checkpoint rotation
 *
 * Benefits:
 * - Fault tolerance
 * - Experiment tracking
 * - Reproducibility
 */

#ifndef NIMCP_CHECKPOINTING_H
#define NIMCP_CHECKPOINTING_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include "common/nimcp_export.h"
#include "include/nimcp.h"  // For nimcp_brain_t and nimcp_status_t

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Checkpoint Configuration
//=============================================================================

/**
 * @brief Checkpoint compression algorithm
 */
typedef enum {
    NIMCP_COMPRESS_NONE = 0,    ///< No compression
    NIMCP_COMPRESS_GZIP = 1,    ///< Gzip compression (best compatibility)
    NIMCP_COMPRESS_ZSTD = 2,    ///< Zstandard (best ratio/speed trade-off)
    NIMCP_COMPRESS_LZ4 = 3      ///< LZ4 (fastest)
} nimcp_checkpoint_compression_t;

/**
 * @brief Checkpoint configuration
 */
typedef struct {
    const char* checkpoint_dir;           ///< Directory for checkpoints
    uint32_t interval_steps;              ///< Save every N learning steps (0 = manual only)
    uint32_t interval_seconds;            ///< Or every N seconds (0 = disabled)
    uint32_t max_checkpoints;             ///< Keep last N checkpoints (0 = unlimited)

    nimcp_checkpoint_compression_t compression;  ///< Compression algorithm
    bool atomic_write;                    ///< Use atomic rename?
    bool include_optimizer_state;         ///< Save optimizer state?
    bool save_metadata_json;              ///< Save metadata as JSON?

    // Callbacks
    void (*on_checkpoint_start)(void* user_data);
    void (*on_checkpoint_complete)(void* user_data, const char* filepath);
    void (*on_checkpoint_error)(void* user_data, const char* error);
    void* user_data;                      ///< User context for callbacks
} nimcp_checkpoint_config_t;

/**
 * @brief Get default checkpoint configuration
 *
 * @return Default configuration
 */
NIMCP_EXPORT nimcp_checkpoint_config_t nimcp_checkpoint_default_config(void);

//=============================================================================
// Checkpoint Metadata
//=============================================================================

/**
 * @brief Checkpoint metadata
 */
typedef struct {
    char filepath[256];                   ///< Full path to checkpoint file
    char brain_name[64];                  ///< Brain identifier
    uint64_t timestamp;                   ///< Unix timestamp (seconds)
    uint64_t learning_step;               ///< Learning step number
    uint64_t total_inferences;            ///< Total inferences so far
    size_t size_bytes;                    ///< Checkpoint size on disk
    size_t size_uncompressed;             ///< Uncompressed size

    // Training state
    float current_loss;                   ///< Training loss at checkpoint
    float accuracy;                       ///< Validation accuracy
    float learning_rate;                  ///< Current learning rate

    // Network architecture
    uint32_t num_neurons;                 ///< Total neurons
    uint32_t num_synapses;                ///< Total synapses
    uint32_t num_active_synapses;         ///< Active synapses

    // Version info
    uint32_t nimcp_version_major;         ///< NIMCP version
    uint32_t nimcp_version_minor;
    uint32_t nimcp_version_patch;

    nimcp_checkpoint_compression_t compression;  ///< Compression used
    bool includes_optimizer_state;        ///< Has optimizer state?
} nimcp_checkpoint_info_t;

//=============================================================================
// Checkpointing API
//=============================================================================

/**
 * @brief Enable automatic checkpointing
 *
 * Brain will auto-save periodically based on config.
 *
 * @param brain Brain handle
 * @param config Checkpoint configuration
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_status_t nimcp_brain_enable_checkpointing(
    nimcp_brain_t brain,
    const nimcp_checkpoint_config_t* config
);

/**
 * @brief Disable automatic checkpointing
 *
 * @param brain Brain handle
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_status_t nimcp_brain_disable_checkpointing(nimcp_brain_t brain);

/**
 * @brief Check if checkpointing is enabled
 *
 * @param brain Brain handle
 * @return true if enabled
 */
NIMCP_EXPORT bool nimcp_brain_checkpointing_enabled(nimcp_brain_t brain);

/**
 * @brief Manually trigger checkpoint
 *
 * Saves checkpoint immediately, regardless of interval.
 *
 * @param brain Brain handle
 * @param checkpoint_name Optional name (NULL = auto-generate)
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_status_t nimcp_brain_checkpoint(
    nimcp_brain_t brain,
    const char* checkpoint_name
);

//=============================================================================
// Checkpoint Loading
//=============================================================================

/**
 * @brief Load brain from checkpoint
 *
 * @param filepath Path to checkpoint file
 * @return Brain handle or NULL on error
 */
NIMCP_EXPORT nimcp_brain_t nimcp_brain_load_checkpoint(const char* filepath);

/**
 * @brief Load latest checkpoint from directory
 *
 * Searches for most recent checkpoint file.
 *
 * @param checkpoint_dir Directory containing checkpoints
 * @return Brain handle or NULL if no checkpoint found
 */
NIMCP_EXPORT nimcp_brain_t nimcp_brain_load_latest_checkpoint(const char* checkpoint_dir);

/**
 * @brief Load checkpoint at specific step
 *
 * @param checkpoint_dir Directory containing checkpoints
 * @param learning_step Step number to load
 * @return Brain handle or NULL if not found
 */
NIMCP_EXPORT nimcp_brain_t nimcp_brain_load_checkpoint_at_step(
    const char* checkpoint_dir,
    uint64_t learning_step
);

//=============================================================================
// Checkpoint Discovery & Inspection
//=============================================================================

/**
 * @brief List available checkpoints in directory
 *
 * @param checkpoint_dir Directory to search
 * @param checkpoints Output array of checkpoint info
 * @param max_checkpoints Maximum checkpoints to return
 * @return Number of checkpoints found
 */
NIMCP_EXPORT uint32_t nimcp_checkpoints_list(
    const char* checkpoint_dir,
    nimcp_checkpoint_info_t* checkpoints,
    uint32_t max_checkpoints
);

/**
 * @brief Get checkpoint metadata without loading
 *
 * @param filepath Path to checkpoint file
 * @param info Output checkpoint metadata
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_checkpoint_get_info(
    const char* filepath,
    nimcp_checkpoint_info_t* info
);

/**
 * @brief Get latest checkpoint info
 *
 * @param checkpoint_dir Directory to search
 * @param info Output checkpoint metadata
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_checkpoint_get_latest_info(
    const char* checkpoint_dir,
    nimcp_checkpoint_info_t* info
);

//=============================================================================
// Checkpoint Management
//=============================================================================

/**
 * @brief Delete old checkpoints (keep last N)
 *
 * @param checkpoint_dir Directory containing checkpoints
 * @param keep_count Number of checkpoints to keep
 * @return Number of checkpoints deleted
 */
NIMCP_EXPORT uint32_t nimcp_checkpoints_cleanup(
    const char* checkpoint_dir,
    uint32_t keep_count
);

/**
 * @brief Delete specific checkpoint
 *
 * @param filepath Path to checkpoint file
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_checkpoint_delete(const char* filepath);

/**
 * @brief Verify checkpoint integrity
 *
 * Checks file exists, is readable, and has valid format.
 *
 * @param filepath Path to checkpoint file
 * @return true if valid
 */
NIMCP_EXPORT bool nimcp_checkpoint_verify(const char* filepath);

//=============================================================================
// Checkpoint Statistics
//=============================================================================

/**
 * @brief Checkpoint statistics
 */
typedef struct {
    uint32_t total_checkpoints;           ///< Total checkpoints saved
    uint32_t successful_saves;            ///< Successful saves
    uint32_t failed_saves;                ///< Failed saves
    uint64_t total_bytes_written;         ///< Total bytes written
    uint64_t total_bytes_saved;           ///< Bytes saved via compression
    float avg_save_time_ms;               ///< Average save time
    float compression_ratio;              ///< Average compression ratio
} nimcp_checkpoint_stats_t;

/**
 * @brief Get checkpoint statistics
 *
 * @param brain Brain handle
 * @param stats Output statistics
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_brain_get_checkpoint_stats(
    nimcp_brain_t brain,
    nimcp_checkpoint_stats_t* stats
);

//=============================================================================
// Advanced Features
//=============================================================================

/**
 * @brief Export checkpoint to different format
 *
 * @param checkpoint_filepath Source checkpoint
 * @param output_filepath Destination file
 * @param format Export format (e.g., "onnx", "tensorflow", "pytorch")
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_status_t nimcp_checkpoint_export(
    const char* checkpoint_filepath,
    const char* output_filepath,
    const char* format
);

/**
 * @brief Compare two checkpoints
 *
 * @param checkpoint1 First checkpoint
 * @param checkpoint2 Second checkpoint
 * @param diff_output Output difference description
 * @param max_length Maximum length of diff output
 * @return Difference score (0 = identical, 1 = completely different)
 */
NIMCP_EXPORT float nimcp_checkpoint_compare(
    const char* checkpoint1,
    const char* checkpoint2,
    char* diff_output,
    uint32_t max_length
);

//=============================================================================
// Internal Checkpoint Functions
//=============================================================================

/**
 * @brief Check if checkpoint should be triggered (internal)
 *
 * Called after each learning step.
 *
 * @param brain Brain handle
 * @return true if checkpoint should be saved
 */
bool nimcp_checkpoint_should_trigger(nimcp_brain_t brain);

/**
 * @brief Perform checkpoint save (internal)
 *
 * @param brain Brain handle
 * @param config Checkpoint configuration
 * @param checkpoint_name Optional checkpoint name
 * @return true on success
 */
bool nimcp_checkpoint_save_internal(
    nimcp_brain_t brain,
    const nimcp_checkpoint_config_t* config,
    const char* checkpoint_name
);

/**
 * @brief Generate checkpoint filename
 *
 * Format: {brain_name}_step_{learning_step}_{timestamp}.ckpt
 *
 * @param brain_name Brain identifier
 * @param learning_step Current learning step
 * @param filename Output filename buffer
 * @param max_length Maximum buffer length
 */
void nimcp_checkpoint_generate_filename(
    const char* brain_name,
    uint64_t learning_step,
    char* filename,
    uint32_t max_length
);

#ifdef __cplusplus
}
#endif

#endif  // NIMCP_CHECKPOINTING_H
