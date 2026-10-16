/**
 * @file nimcp_checkpointing.c
 * @brief Automatic checkpointing and fault tolerance implementation
 * @version 2.7.0
 * @date 2025-11-04
 */

#include "include/nimcp.h"
#include "cognitive/checkpointing/nimcp_checkpointing.h"
#include "core/brain/nimcp_brain.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>

//=============================================================================
// Handle Conversion (from nimcp.c)
//=============================================================================

// Access internal brain handle structure (defined in nimcp.c)
struct nimcp_brain_handle {
    brain_t internal_brain;
};

//=============================================================================
// Checkpoint Configuration Defaults
//=============================================================================

nimcp_checkpoint_config_t nimcp_checkpoint_default_config(void) {
    nimcp_checkpoint_config_t config;
    memset(&config, 0, sizeof(config));

    config.checkpoint_dir = "./checkpoints";
    config.interval_steps = 1000;  // Save every 1000 steps
    config.interval_seconds = 0;   // Disabled by default
    config.max_checkpoints = 5;    // Keep last 5 checkpoints

    config.compression = NIMCP_COMPRESS_NONE;
    config.atomic_write = true;
    config.include_optimizer_state = true;
    config.save_metadata_json = true;

    config.on_checkpoint_start = NULL;
    config.on_checkpoint_complete = NULL;
    config.on_checkpoint_error = NULL;
    config.user_data = NULL;

    return config;
}

//=============================================================================
// Internal Checkpoint State
//=============================================================================

typedef struct {
    nimcp_checkpoint_config_t config;
    bool enabled;
    uint64_t last_checkpoint_step;
    uint64_t last_checkpoint_time_ms;
    nimcp_checkpoint_stats_t stats;
} checkpoint_state_t;

// Global checkpoint state per brain (simplified - could use hash map for multi-brain)
static checkpoint_state_t g_checkpoint_state = {0};

//=============================================================================
// Checkpointing Enable/Disable
//=============================================================================

nimcp_status_t nimcp_brain_enable_checkpointing(nimcp_brain_t brain,
                                                 const nimcp_checkpoint_config_t* config) {
    if (!brain || !config) {
        return NIMCP_ERROR_NULL_ARG;
    }

    if (!config->checkpoint_dir) {
        return NIMCP_ERROR_INVALID_ARG;
    }

    // Create checkpoint directory if it doesn't exist
    struct stat st = {0};
    if (stat(config->checkpoint_dir, &st) == -1) {
        if (mkdir(config->checkpoint_dir, 0755) != 0) {
            return NIMCP_ERROR_IO;
        }
    }

    // Initialize checkpoint state
    g_checkpoint_state.config = *config;
    g_checkpoint_state.enabled = true;
    g_checkpoint_state.last_checkpoint_step = 0;
    g_checkpoint_state.last_checkpoint_time_ms = nimcp_time_get_ms();

    memset(&g_checkpoint_state.stats, 0, sizeof(nimcp_checkpoint_stats_t));

    return NIMCP_OK;
}

nimcp_status_t nimcp_brain_disable_checkpointing(nimcp_brain_t brain) {
    if (!brain) {
        return NIMCP_ERROR_NULL_ARG;
    }

    g_checkpoint_state.enabled = false;

    return NIMCP_OK;
}

bool nimcp_brain_checkpointing_enabled(nimcp_brain_t brain) {
    if (!brain) {
        return false;
    }

    return g_checkpoint_state.enabled;
}

//=============================================================================
// Checkpoint Filename Generation
//=============================================================================

void nimcp_checkpoint_generate_filename(const char* brain_name,
                                         uint64_t learning_step,
                                         char* filename,
                                         uint32_t max_length) {
    if (!brain_name || !filename || max_length == 0) {
        return;
    }

    uint64_t timestamp = nimcp_time_get_ms() / 1000;  // Unix timestamp in seconds

    snprintf(filename, max_length, "%s_step_%lu_%lu.ckpt",
             brain_name, (unsigned long)learning_step, (unsigned long)timestamp);
}

//=============================================================================
// Checkpoint Trigger Logic
//=============================================================================

bool nimcp_checkpoint_should_trigger(nimcp_brain_t brain) {
    if (!brain || !g_checkpoint_state.enabled) {
        return false;
    }

    // Check step interval
    if (g_checkpoint_state.config.interval_steps > 0) {
        // Would need to get current learning step from brain
        // For now, simplified check
        uint64_t current_step = 0;  // TODO: Get from brain state

        if (current_step >= g_checkpoint_state.last_checkpoint_step + g_checkpoint_state.config.interval_steps) {
            return true;
        }
    }

    // Check time interval
    if (g_checkpoint_state.config.interval_seconds > 0) {
        uint64_t current_time_ms = nimcp_time_get_ms();
        uint64_t elapsed_ms = current_time_ms - g_checkpoint_state.last_checkpoint_time_ms;
        uint64_t interval_ms = g_checkpoint_state.config.interval_seconds * 1000;

        if (elapsed_ms >= interval_ms) {
            return true;
        }
    }

    return false;
}

//=============================================================================
// Checkpoint Save Implementation
//=============================================================================

bool nimcp_checkpoint_save_internal(nimcp_brain_t brain,
                                     const nimcp_checkpoint_config_t* config,
                                     const char* checkpoint_name) {
    if (!brain || !config) {
        return false;
    }

    uint64_t start_ms = nimcp_time_get_ms();

    // Invoke start callback
    if (config->on_checkpoint_start) {
        config->on_checkpoint_start(config->user_data);
    }

    // Generate filename
    char filename[256];
    if (checkpoint_name) {
        // Add .ckpt extension if not already present
        if (strstr(checkpoint_name, ".ckpt") == NULL) {
            snprintf(filename, sizeof(filename), "%s.ckpt", checkpoint_name);
        } else {
            snprintf(filename, sizeof(filename), "%s", checkpoint_name);
        }
    } else {
        nimcp_checkpoint_generate_filename("brain", 0, filename, sizeof(filename));
    }

    // Build full path
    char filepath[512];
    snprintf(filepath, sizeof(filepath), "%s/%s", config->checkpoint_dir, filename);

    // Save brain state
    // If atomic write enabled, save to temp file first
    char temp_filepath[520];  // filepath (512) + ".tmp" (4) + null (1) + padding
    const char* save_path = filepath;

    if (config->atomic_write) {
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wformat-truncation"
        snprintf(temp_filepath, sizeof(temp_filepath), "%s.tmp", filepath);
        #pragma GCC diagnostic pop
        save_path = temp_filepath;
    }

    // Call brain save function (unwrap handle to get internal brain)
    bool success = brain_save(brain->internal_brain, save_path);

    if (!success) {
        if (config->on_checkpoint_error) {
            config->on_checkpoint_error(config->user_data, "Failed to save brain state");
        }
        g_checkpoint_state.stats.failed_saves++;
        return false;
    }

    // Atomic rename
    if (config->atomic_write) {
        if (rename(temp_filepath, filepath) != 0) {
            if (config->on_checkpoint_error) {
                config->on_checkpoint_error(config->user_data, "Failed to rename checkpoint file");
            }
            unlink(temp_filepath);
            g_checkpoint_state.stats.failed_saves++;
            return false;
        }
    }

    // Update statistics
    uint64_t end_ms = nimcp_time_get_ms();
    float save_time_ms = (float)(end_ms - start_ms);

    g_checkpoint_state.stats.total_checkpoints++;
    g_checkpoint_state.stats.successful_saves++;

    // Update average save time
    if (g_checkpoint_state.stats.successful_saves == 1) {
        g_checkpoint_state.stats.avg_save_time_ms = save_time_ms;
    } else {
        float alpha = 0.1f;
        g_checkpoint_state.stats.avg_save_time_ms =
            alpha * save_time_ms + (1.0f - alpha) * g_checkpoint_state.stats.avg_save_time_ms;
    }

    // Get file size
    struct stat file_stat;
    if (stat(filepath, &file_stat) == 0) {
        g_checkpoint_state.stats.total_bytes_written += file_stat.st_size;
    }

    // Update last checkpoint time
    g_checkpoint_state.last_checkpoint_time_ms = end_ms;

    // Invoke complete callback
    if (config->on_checkpoint_complete) {
        config->on_checkpoint_complete(config->user_data, filepath);
    }

    // Cleanup old checkpoints
    if (config->max_checkpoints > 0) {
        nimcp_checkpoints_cleanup(config->checkpoint_dir, config->max_checkpoints);
    }

    return true;
}

nimcp_status_t nimcp_brain_checkpoint(nimcp_brain_t brain, const char* checkpoint_name) {
    if (!brain) {
        return NIMCP_ERROR_NULL_ARG;
    }

    if (!g_checkpoint_state.enabled) {
        return NIMCP_ERROR;  // Checkpointing not enabled
    }

    bool success = nimcp_checkpoint_save_internal(brain, &g_checkpoint_state.config, checkpoint_name);

    if (success) {
        return NIMCP_OK;
    } else {
        return NIMCP_ERROR;
    }
}

//=============================================================================
// Checkpoint Loading
//=============================================================================

nimcp_brain_t nimcp_brain_load_checkpoint(const char* filepath) {
    if (!filepath) {
        return NULL;
    }

    // Check file exists
    if (access(filepath, R_OK) != 0) {
        return NULL;
    }

    // Load internal brain
    brain_t internal_brain = brain_load(filepath);
    if (!internal_brain) {
        return NULL;
    }

    // Wrap in public handle
    nimcp_brain_t handle = (nimcp_brain_t)nimcp_malloc(sizeof(struct nimcp_brain_handle));
    if (!handle) {
        brain_destroy(internal_brain);
        return NULL;
    }

    handle->internal_brain = internal_brain;
    return handle;
}

nimcp_brain_t nimcp_brain_load_latest_checkpoint(const char* checkpoint_dir) {
    if (!checkpoint_dir) {
        return NULL;
    }

    nimcp_checkpoint_info_t latest_info;
    if (!nimcp_checkpoint_get_latest_info(checkpoint_dir, &latest_info)) {
        return NULL;
    }

    return nimcp_brain_load_checkpoint(latest_info.filepath);
}

nimcp_brain_t nimcp_brain_load_checkpoint_at_step(const char* checkpoint_dir, uint64_t learning_step) {
    if (!checkpoint_dir) {
        return NULL;
    }

    // List all checkpoints
    nimcp_checkpoint_info_t checkpoints[100];
    uint32_t count = nimcp_checkpoints_list(checkpoint_dir, checkpoints, 100);

    // Find checkpoint with matching step
    for (uint32_t i = 0; i < count; i++) {
        if (checkpoints[i].learning_step == learning_step) {
            return nimcp_brain_load_checkpoint(checkpoints[i].filepath);
        }
    }

    return NULL;  // Not found
}

//=============================================================================
// Checkpoint Discovery & Inspection
//=============================================================================

uint32_t nimcp_checkpoints_list(const char* checkpoint_dir,
                                 nimcp_checkpoint_info_t* checkpoints,
                                 uint32_t max_checkpoints) {
    if (!checkpoint_dir || !checkpoints || max_checkpoints == 0) {
        return 0;
    }

    DIR* dir = opendir(checkpoint_dir);
    if (!dir) {
        return 0;
    }

    uint32_t count = 0;
    struct dirent* entry;

    while ((entry = readdir(dir)) != NULL && count < max_checkpoints) {
        // Look for .ckpt files
        size_t len = strlen(entry->d_name);
        if (len > 5 && strcmp(entry->d_name + len - 5, ".ckpt") == 0) {
            // Build full path with bounds checking
            size_t dir_len = strlen(checkpoint_dir);
            if (dir_len + 1 + len < sizeof(checkpoints[count].filepath)) {
                // Suppress truncation warning - we've already checked bounds
                #pragma GCC diagnostic push
                #pragma GCC diagnostic ignored "-Wformat-truncation"
                snprintf(checkpoints[count].filepath, sizeof(checkpoints[count].filepath),
                         "%s/%s", checkpoint_dir, entry->d_name);
                #pragma GCC diagnostic pop

                // Get basic info
                if (nimcp_checkpoint_get_info(checkpoints[count].filepath, &checkpoints[count])) {
                    count++;
                }
            }
        }
    }

    closedir(dir);

    // Sort by timestamp (descending)
    // FIX: Guard against underflow when count is 0 or 1
    if (count > 1) {
        for (uint32_t i = 0; i < count - 1; i++) {
            for (uint32_t j = i + 1; j < count; j++) {
                if (checkpoints[j].timestamp > checkpoints[i].timestamp) {
                    nimcp_checkpoint_info_t temp = checkpoints[i];
                    checkpoints[i] = checkpoints[j];
                    checkpoints[j] = temp;
                }
            }
        }
    }

    return count;
}

bool nimcp_checkpoint_get_info(const char* filepath, nimcp_checkpoint_info_t* info) {
    if (!filepath || !info) {
        return false;
    }

    memset(info, 0, sizeof(nimcp_checkpoint_info_t));

    // Copy filepath
    strncpy(info->filepath, filepath, sizeof(info->filepath) - 1);

    // Get file stats
    struct stat file_stat;
    if (stat(filepath, &file_stat) != 0) {
        return false;
    }

    info->size_bytes = file_stat.st_size;
    info->timestamp = file_stat.st_mtime;

    // Parse filename for metadata (simplified)
    const char* filename = strrchr(filepath, '/');
    filename = filename ? filename + 1 : filepath;

    // Try to extract step and timestamp from filename
    // Format: {brain_name}_step_{learning_step}_{timestamp}.ckpt
    sscanf(filename, "%63[^_]_step_%lu_%lu.ckpt",
           info->brain_name, &info->learning_step, &info->timestamp);

    info->compression = NIMCP_COMPRESS_NONE;  // Default
    info->includes_optimizer_state = true;

    return true;
}

bool nimcp_checkpoint_get_latest_info(const char* checkpoint_dir,
                                       nimcp_checkpoint_info_t* info) {
    if (!checkpoint_dir || !info) {
        return false;
    }

    nimcp_checkpoint_info_t checkpoints[1];
    uint32_t count = nimcp_checkpoints_list(checkpoint_dir, checkpoints, 1);

    if (count == 0) {
        return false;
    }

    *info = checkpoints[0];
    return true;
}

//=============================================================================
// Checkpoint Management
//=============================================================================

uint32_t nimcp_checkpoints_cleanup(const char* checkpoint_dir, uint32_t keep_count) {
    if (!checkpoint_dir || keep_count == 0) {
        return 0;
    }

    // List all checkpoints
    nimcp_checkpoint_info_t checkpoints[100];
    uint32_t count = nimcp_checkpoints_list(checkpoint_dir, checkpoints, 100);

    if (count <= keep_count) {
        return 0;  // Nothing to delete
    }

    // Delete old checkpoints
    uint32_t deleted = 0;
    for (uint32_t i = keep_count; i < count; i++) {
        if (unlink(checkpoints[i].filepath) == 0) {
            deleted++;
        }
    }

    return deleted;
}

bool nimcp_checkpoint_delete(const char* filepath) {
    if (!filepath) {
        return false;
    }

    return (unlink(filepath) == 0);
}

bool nimcp_checkpoint_verify(const char* filepath) {
    if (!filepath) {
        return false;
    }

    // Check file exists and is readable
    if (access(filepath, R_OK) != 0) {
        return false;
    }

    // Check file size is reasonable (> 0 bytes)
    struct stat file_stat;
    if (stat(filepath, &file_stat) != 0) {
        return false;
    }

    if (file_stat.st_size == 0) {
        return false;
    }

    // Could add more validation (magic number, checksum, etc.)
    return true;
}

//=============================================================================
// Checkpoint Statistics
//=============================================================================

bool nimcp_brain_get_checkpoint_stats(nimcp_brain_t brain, nimcp_checkpoint_stats_t* stats) {
    if (!brain || !stats) {
        return false;
    }

    *stats = g_checkpoint_state.stats;

    // Calculate compression ratio
    if (stats->total_bytes_written > 0 && stats->total_bytes_saved > 0) {
        stats->compression_ratio = (float)stats->total_bytes_saved / stats->total_bytes_written;
    } else {
        stats->compression_ratio = 1.0f;
    }

    return true;
}

//=============================================================================
// Advanced Features
//=============================================================================

nimcp_status_t nimcp_checkpoint_export(const char* checkpoint_filepath,
                                        const char* output_filepath,
                                        const char* format) {
    if (!checkpoint_filepath || !output_filepath || !format) {
        return NIMCP_ERROR_NULL_ARG;
    }

    // Export feature not implemented yet
    return NIMCP_ERROR_NOT_IMPLEMENTED;
}

float nimcp_checkpoint_compare(const char* checkpoint1,
                                const char* checkpoint2,
                                char* diff_output,
                                uint32_t max_length) {
    if (!checkpoint1 || !checkpoint2) {
        return -1.0f;
    }

    // Compare feature not implemented yet
    if (diff_output && max_length > 0) {
        snprintf(diff_output, max_length, "Checkpoint comparison not implemented");
    }

    return -1.0f;
}
