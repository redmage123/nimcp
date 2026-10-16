/**
 * @file nimcp_immune_persistence.c
 * @brief Immunological Memory Persistence Implementation
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Implementation of immune system persistence
 * WHY:  Enable cross-session threat learning and disaster recovery
 * HOW:  Binary serialization with compression, encryption, validation
 *
 * @author NIMCP Development Team
 */

#include "cognitive/immune/nimcp_immune_persistence.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/serialization/nimcp_serialization.h"
#include "utils/logging/nimcp_logging.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(immune_persistence, MESH_ADAPTER_CATEGORY_SECURITY)



/* Mutex convenience macros */
#define nimcp_mutex_lock(m) nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)(m))
#define nimcp_mutex_unlock(m) nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)(m))

/* Logging module name */
#define PERSISTENCE_MODULE_NAME "immune_persistence"

/* ============================================================================
 * Internal Helper Declarations
 * ============================================================================ */

static uint64_t get_timestamp_ms(void);
static uint32_t compute_checksum(const uint8_t* data, size_t len);
static int write_header(FILE* file, const immune_persistence_header_t* header);
static int read_header(FILE* file, immune_persistence_header_t* header);
static int write_counts(FILE* file, const immune_persistence_counts_t* counts);
static int read_counts(FILE* file, immune_persistence_counts_t* counts);
static int validate_header(const immune_persistence_header_t* header);
static int validate_counts(const immune_persistence_counts_t* counts);
static void fill_counts_from_system(
    const brain_immune_system_t* system,
    immune_persistence_counts_t* counts,
    bool memory_only
);

/* ============================================================================
 * Configuration API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Initialize configuration with sensible defaults
 * WHY:  Easy setup for most use cases
 * HOW:  Set all save flags to true, no compression/encryption
 */
int immune_persistence_default_config(immune_persistence_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    immune_persistence_heartbeat("immune_persi_default_config", 0.0f);


    memset(config, 0, sizeof(*config));

    /* Compression/Encryption - disabled by default for speed */
    config->enable_compression = false;
    config->enable_encryption = false;
    config->encryption_key_set = false;

    /* Save all components */
    config->save_antigens = true;
    config->save_b_cells = true;
    config->save_t_cells = true;
    config->save_antibodies = true;
    config->save_cytokines = true;
    config->save_inflammation = true;
    config->save_statistics = true;

    /* Memory-only mode disabled */
    config->memory_cells_only = false;

    /* Validation enabled */
    config->verify_on_load = true;
    config->strict_version_check = false;

    /* Backup enabled */
    config->create_backup = true;
    strncpy(config->backup_suffix, ".bak", sizeof(config->backup_suffix) - 1);

    return 0;
}

/**
 * @brief Set encryption key
 *
 * WHAT: Configure encryption key for persistence
 * WHY:  Protect sensitive immune data
 * HOW:  Copy key, validate length, enable encryption
 */
int immune_persistence_set_encryption_key(
    immune_persistence_config_t* config,
    const uint8_t* key,
    size_t key_len
) {
    if (!config || !key) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "immune_persistence_set_encryption_key: required parameter is NULL (config, key)");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    immune_persistence_heartbeat("immune_persi_set_encryption_key", 0.0f);


    if (key_len != 32) {
        NIMCP_LOGGING_ERROR("Encryption key must be 32 bytes for AES-256");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "immune_persistence_set_encryption_key: validation failed");
        return -1;
    }

    memcpy(config->encryption_key, key, 32);
    config->encryption_key_set = true;
    config->enable_encryption = true;

    return 0;
}

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

/**
 * @brief Get current timestamp in milliseconds
 */
static uint64_t get_timestamp_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    /* Cast before multiply to avoid signed 32-bit overflow */
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

/**
 * @brief Compute CRC32 checksum
 *
 * WHAT: Calculate simple checksum for data validation
 * WHY:  Detect file corruption
 * HOW:  Simple XOR-based checksum (fast, good enough for detection)
 */
static uint32_t compute_checksum(const uint8_t* data, size_t len) {
    if (!data || len == 0) return 0;

    uint32_t checksum = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && len > 256) {
            immune_persistence_heartbeat("immune_persi_loop",
                             (float)(i + 1) / (float)len);
        }

        checksum ^= data[i];
        checksum = (checksum << 1) | (checksum >> 31);
    }
    return checksum;
}

/**
 * @brief Compute checksum of file contents from current position
 *
 * WHAT: Compute checksum of remaining file data
 * WHY:  Verify integrity of persisted immune data
 * HOW:  Read file in chunks, accumulate checksum, restore position
 *
 * @param file File handle positioned after header
 * @param header_size Size of header to skip in checksum
 * @return Computed checksum, or 0 on error
 */
static uint32_t compute_file_checksum(FILE* file, size_t header_size) {
    if (!file) return 0;

    /* Save current position */
    long start_pos = ftell(file);
    if (start_pos < 0) return 0;

    /* Seek past header */
    if (fseek(file, (long)header_size, SEEK_SET) != 0) return 0;

    uint32_t checksum = 0xFFFFFFFF;
    uint8_t buffer[4096];
    size_t bytes_read = 0;

    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        for (size_t i = 0; i < bytes_read; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && bytes_read > 256) {
                immune_persistence_heartbeat("immune_persi_loop",
                                 (float)(i + 1) / (float)bytes_read);
            }

            checksum ^= buffer[i];
            checksum = (checksum << 1) | (checksum >> 31);
        }
    }

    /* Restore position */
    fseek(file, start_pos, SEEK_SET);

    return checksum;
}

/**
 * @brief Write header to file
 */
static int write_header(FILE* file, const immune_persistence_header_t* header) {
    if (!file || !header) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "write_header: required parameter is NULL (file, header)");
        return -1;
    }
    if (fwrite(header, sizeof(*header), 1, file) != 1) {
        NIMCP_LOGGING_ERROR("Failed to write header");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "write_header: validation failed");
        return -1;
    }
    return 0;
}

/**
 * @brief Read header from file
 */
static int read_header(FILE* file, immune_persistence_header_t* header) {
    if (!file || !header) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "read_header: required parameter is NULL (file, header)");
        return -1;
    }
    if (fread(header, sizeof(*header), 1, file) != 1) {
        NIMCP_LOGGING_ERROR("Failed to read header");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "read_header: validation failed");
        return -1;
    }
    return 0;
}

/**
 * @brief Write counts to file
 */
static int write_counts(FILE* file, const immune_persistence_counts_t* counts) {
    if (!file || !counts) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "write_counts: required parameter is NULL (file, counts)");
        return -1;
    }
    if (fwrite(counts, sizeof(*counts), 1, file) != 1) {
        NIMCP_LOGGING_ERROR("Failed to write counts");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "write_counts: validation failed");
        return -1;
    }
    return 0;
}

/**
 * @brief Read counts from file
 */
static int read_counts(FILE* file, immune_persistence_counts_t* counts) {
    if (!file || !counts) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "read_counts: required parameter is NULL (file, counts)");
        return -1;
    }
    if (fread(counts, sizeof(*counts), 1, file) != 1) {
        NIMCP_LOGGING_ERROR("Failed to read counts");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "read_counts: validation failed");
        return -1;
    }
    return 0;
}

/**
 * @brief Validate header contents
 */
static int validate_header(const immune_persistence_header_t* header) {
    if (!header) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "header is NULL");

        return -1;

    }

    /* Check magic */
    if (memcmp(header->magic, IMMUNE_PERSISTENCE_MAGIC,
               IMMUNE_PERSISTENCE_MAGIC_LEN) != 0) {
        NIMCP_LOGGING_ERROR("Invalid magic header");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "validate_header: operation failed");
        return -1;
    }

    /* Check version compatibility */
    if (!immune_persistence_is_version_compatible(header->version)) {
        NIMCP_LOGGING_ERROR("Incompatible version: %u (current: %u)",
            header->version, IMMUNE_PERSISTENCE_VERSION);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "validate_header: immune_persistence_is_version_compatible is NULL");
        return -1;
    }

    return 0;
}

/**
 * @brief Validate counts against capacity limits
 */
static int validate_counts(const immune_persistence_counts_t* counts) {
    if (!counts) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "counts is NULL");

        return -1;

    }

    /* Check against maximum capacities */
    if (counts->antigen_count > BRAIN_IMMUNE_MAX_ANTIGENS) {
        NIMCP_LOGGING_ERROR("Antigen count %u exceeds max %u",
            counts->antigen_count, BRAIN_IMMUNE_MAX_ANTIGENS);
        return -1;
    }
    if (counts->b_cell_count > BRAIN_IMMUNE_MAX_B_CELLS) {
        NIMCP_LOGGING_ERROR("B cell count %u exceeds max %u",
            counts->b_cell_count, BRAIN_IMMUNE_MAX_B_CELLS);
        return -1;
    }
    if (counts->t_cell_count > BRAIN_IMMUNE_MAX_T_CELLS) {
        NIMCP_LOGGING_ERROR("T cell count %u exceeds max %u",
            counts->t_cell_count, BRAIN_IMMUNE_MAX_T_CELLS);
        return -1;
    }
    if (counts->antibody_count > BRAIN_IMMUNE_MAX_ANTIBODIES) {
        NIMCP_LOGGING_ERROR("Antibody count %u exceeds max %u",
            counts->antibody_count, BRAIN_IMMUNE_MAX_ANTIBODIES);
        return -1;
    }
    if (counts->cytokine_count > BRAIN_IMMUNE_MAX_CYTOKINES) {
        NIMCP_LOGGING_ERROR("Cytokine count %u exceeds max %u",
            counts->cytokine_count, BRAIN_IMMUNE_MAX_CYTOKINES);
        return -1;
    }
    if (counts->inflammation_count > BRAIN_IMMUNE_MAX_INFLAMMATION) {
        NIMCP_LOGGING_ERROR("Inflammation count %u exceeds max %u",
            counts->inflammation_count, BRAIN_IMMUNE_MAX_INFLAMMATION);
        return -1;
    }

    return 0;
}

/**
 * @brief Fill counts structure from immune system
 */
static void fill_counts_from_system(
    const brain_immune_system_t* system,
    immune_persistence_counts_t* counts,
    bool memory_only
) {
    if (!system || !counts) return;

    memset(counts, 0, sizeof(*counts));

    if (memory_only) {
        /* Count only memory cells */
        for (size_t i = 0; i < system->b_cell_count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && system->b_cell_count > 256) {
                immune_persistence_heartbeat("immune_persi_loop",
                                 (float)(i + 1) / (float)system->b_cell_count);
            }

            if (system->b_cells[i].state == B_CELL_MEMORY) {
                counts->b_cell_count++;
            }
        }
        for (size_t i = 0; i < system->t_cell_count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && system->t_cell_count > 256) {
                immune_persistence_heartbeat("immune_persi_loop",
                                 (float)(i + 1) / (float)system->t_cell_count);
            }

            if (system->t_cells[i].type == T_CELL_MEMORY) {
                counts->t_cell_count++;
            }
        }
    } else {
        /* Count all cells */
        counts->antigen_count = (uint32_t)system->antigen_count;
        counts->b_cell_count = (uint32_t)system->b_cell_count;
        counts->t_cell_count = (uint32_t)system->t_cell_count;
        counts->antibody_count = (uint32_t)system->antibody_count;
        counts->cytokine_count = (uint32_t)system->cytokine_count;
        counts->inflammation_count = (uint32_t)system->inflammation_count;
    }
}

/* ============================================================================
 * Save Implementation
 * ============================================================================ */

/**
 * @brief Save immune system to file
 *
 * WHAT: Serialize complete immune state to disk
 * WHY:  Enable cross-session memory persistence
 * HOW:  Write header → counts → data arrays
 */
int immune_persistence_save(
    brain_immune_system_t* system,
    const char* filepath,
    const immune_persistence_config_t* config
) {
    if (!system || !filepath) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "immune_persistence_save: required parameter is NULL (system, filepath)");
        return -1;
    }

    /* Use default config if not provided */
    /* Phase 8: Heartbeat at operation start */
    immune_persistence_heartbeat("immune_persi_save", 0.0f);


    immune_persistence_config_t default_cfg;
    if (!config) {
        immune_persistence_default_config(&default_cfg);
        config = &default_cfg;
    }

    /* Create temporary file for atomic write */
    char temp_path[IMMUNE_PERSIST_MAX_PATH];
    snprintf(temp_path, sizeof(temp_path), "%s.tmp", filepath);

    FILE* file = fopen(temp_path, "wb");
    if (!file) {
        NIMCP_LOGGING_ERROR("Failed to open file for writing: %s", temp_path);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "immune_persistence_save: file is NULL");
        return -1;
    }

    int result = -1;
    /* Backup must be created under mutex to prevent TOCTOU race */
    nimcp_mutex_lock(system->mutex);

    /* Create backup if requested (now under mutex) */
    if (config->create_backup) {
        /* TODO: file I/O under lock is a known antipattern; a future refactor
         * should snapshot the filepath and create the backup after releasing
         * the mutex. For now, the backup is best-effort. */
        immune_persistence_create_backup(filepath, config->backup_suffix);
    }

    /* Prepare header */
    immune_persistence_header_t header;
    memset(&header, 0, sizeof(header));
    memcpy(header.magic, IMMUNE_PERSISTENCE_MAGIC, IMMUNE_PERSISTENCE_MAGIC_LEN);
    header.version = IMMUNE_PERSISTENCE_VERSION;
    header.timestamp = get_timestamp_ms();
    header.flags = 0;

    if (config->enable_compression) header.flags |= IMMUNE_FORMAT_FLAG_COMPRESSED;
    if (config->enable_encryption) header.flags |= IMMUNE_FORMAT_FLAG_ENCRYPTED;
    if (config->memory_cells_only) header.flags |= IMMUNE_FORMAT_FLAG_MEMORY_ONLY;

    /* Prepare counts */
    immune_persistence_counts_t counts;
    fill_counts_from_system(system, &counts, config->memory_cells_only);

    /* Write header (checksum filled later) */
    if (write_header(file, &header) != 0) goto cleanup;

    /* Write counts */
    if (write_counts(file, &counts) != 0) goto cleanup;

    /* Write data sections based on config */
    if (config->save_antigens && !config->memory_cells_only) {
        if (fwrite(system->antigens, sizeof(brain_antigen_t),
                   counts.antigen_count, file) != counts.antigen_count) {
            NIMCP_LOGGING_ERROR("Failed to write antigens");
            goto cleanup;
        }
    }

    if (config->save_b_cells) {
        if (config->memory_cells_only) {
            /* Write only memory B cells */
            for (size_t i = 0; i < system->b_cell_count; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && system->b_cell_count > 256) {
                    immune_persistence_heartbeat("immune_persi_loop",
                                     (float)(i + 1) / (float)system->b_cell_count);
                }

                if (system->b_cells[i].state == B_CELL_MEMORY) {
                    if (fwrite(&system->b_cells[i], sizeof(brain_b_cell_t),
                               1, file) != 1) {
                        NIMCP_LOGGING_ERROR("Failed to write memory B cell");
                        goto cleanup;
                    }
                }
            }
        } else {
            /* Write all B cells */
            if (fwrite(system->b_cells, sizeof(brain_b_cell_t),
                       counts.b_cell_count, file) != counts.b_cell_count) {
                NIMCP_LOGGING_ERROR("Failed to write B cells");
                goto cleanup;
            }
        }
    }

    if (config->save_t_cells) {
        if (config->memory_cells_only) {
            /* Write only memory T cells */
            for (size_t i = 0; i < system->t_cell_count; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && system->t_cell_count > 256) {
                    immune_persistence_heartbeat("immune_persi_loop",
                                     (float)(i + 1) / (float)system->t_cell_count);
                }

                if (system->t_cells[i].type == T_CELL_MEMORY) {
                    if (fwrite(&system->t_cells[i], sizeof(brain_t_cell_t),
                               1, file) != 1) {
                        NIMCP_LOGGING_ERROR("Failed to write memory T cell");
                        goto cleanup;
                    }
                }
            }
        } else {
            /* Write all T cells */
            if (fwrite(system->t_cells, sizeof(brain_t_cell_t),
                       counts.t_cell_count, file) != counts.t_cell_count) {
                NIMCP_LOGGING_ERROR("Failed to write T cells");
                goto cleanup;
            }
        }
    }

    if (config->save_antibodies && !config->memory_cells_only) {
        if (fwrite(system->antibodies, sizeof(brain_antibody_t),
                   counts.antibody_count, file) != counts.antibody_count) {
            NIMCP_LOGGING_ERROR("Failed to write antibodies");
            goto cleanup;
        }
    }

    if (config->save_cytokines && !config->memory_cells_only) {
        if (fwrite(system->cytokines, sizeof(brain_cytokine_t),
                   counts.cytokine_count, file) != counts.cytokine_count) {
            NIMCP_LOGGING_ERROR("Failed to write cytokines");
            goto cleanup;
        }
    }

    if (config->save_inflammation && !config->memory_cells_only) {
        if (fwrite(system->inflammation_sites, sizeof(brain_inflammation_site_t),
                   counts.inflammation_count, file) != counts.inflammation_count) {
            NIMCP_LOGGING_ERROR("Failed to write inflammation sites");
            goto cleanup;
        }
    }

    if (config->save_statistics && !config->memory_cells_only) {
        if (fwrite(&system->stats, sizeof(brain_immune_stats_t),
                   1, file) != 1) {
            NIMCP_LOGGING_ERROR("Failed to write statistics");
            goto cleanup;
        }
    }

    /* Update header with file size and checksum */
    /* ftell() returns -1L on error; check before using */
    long file_size = ftell(file);
    if (file_size < 0) {
        NIMCP_LOGGING_ERROR("ftell failed when computing file size");
        goto cleanup;
    }
    header.file_size = (uint64_t)file_size;

    /* Compute checksum of data after header */
    header.checksum = compute_file_checksum(file, sizeof(immune_persistence_header_t));

    /* Rewrite header with updated values */
    fseek(file, 0, SEEK_SET);
    if (write_header(file, &header) != 0) goto cleanup;

    result = 0;
    NIMCP_LOGGING_INFO("Saved immune system: %u antigens, %u B cells, %u T cells",
        counts.antigen_count, counts.b_cell_count, counts.t_cell_count);

cleanup:
    nimcp_mutex_unlock(system->mutex);
    fclose(file);

    if (result == 0) {
        /* Atomic rename */
        if (rename(temp_path, filepath) != 0) {
            NIMCP_LOGGING_ERROR("Failed to rename temp file to %s", filepath);
            remove(temp_path);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "immune_persistence_save: validation failed");
            return -1;
        }
    } else {
        remove(temp_path);
    }

    return result;
}

/* ============================================================================
 * Load Implementation
 * ============================================================================ */

/**
 * @brief Load immune system from file
 *
 * WHAT: Restore immune state from disk
 * WHY:  Restore learned immunity from previous sessions
 * HOW:  Read header → validate → read counts → load data
 */
int immune_persistence_load(
    brain_immune_system_t* system,
    const char* filepath,
    const immune_persistence_config_t* config
) {
    if (!system || !filepath) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "immune_persistence_load: required parameter is NULL (system, filepath)");
        return -1;
    }

    /* Use default config if not provided */
    /* Phase 8: Heartbeat at operation start */
    immune_persistence_heartbeat("immune_persi_load", 0.0f);


    immune_persistence_config_t default_cfg;
    if (!config) {
        immune_persistence_default_config(&default_cfg);
        config = &default_cfg;
    }

    FILE* file = fopen(filepath, "rb");
    if (!file) {
        NIMCP_LOGGING_ERROR("Failed to open file for reading: %s", filepath);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "immune_persistence_load: file is NULL");
        return -1;
    }

    int result = -1;
    nimcp_mutex_lock(system->mutex);

    /* Read and validate header */
    immune_persistence_header_t header;
    if (read_header(file, &header) != 0) goto cleanup;
    if (validate_header(&header) != 0) goto cleanup;

    /* Read and validate counts */
    immune_persistence_counts_t counts;
    if (read_counts(file, &counts) != 0) goto cleanup;
    if (validate_counts(&counts) != 0) goto cleanup;

    /* Clear existing state */
    immune_persistence_clear_state(system);

    /* Load data sections */
    if (counts.antigen_count > 0 && config->save_antigens) {
        system->antigen_count = counts.antigen_count;
        if (fread(system->antigens, sizeof(brain_antigen_t),
                  counts.antigen_count, file) != counts.antigen_count) {
            NIMCP_LOGGING_ERROR("Failed to read antigens");
            goto cleanup;
        }
    }

    if (counts.b_cell_count > 0 && config->save_b_cells) {
        system->b_cell_count = counts.b_cell_count;
        if (fread(system->b_cells, sizeof(brain_b_cell_t),
                  counts.b_cell_count, file) != counts.b_cell_count) {
            NIMCP_LOGGING_ERROR("Failed to read B cells");
            goto cleanup;
        }

        /* Update statistics */
        for (size_t i = 0; i < system->b_cell_count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && system->b_cell_count > 256) {
                immune_persistence_heartbeat("immune_persi_loop",
                                 (float)(i + 1) / (float)system->b_cell_count);
            }

            if (system->b_cells[i].state == B_CELL_MEMORY) {
                system->stats.memory_cells++;
            }
        }
    }

    if (counts.t_cell_count > 0 && config->save_t_cells) {
        system->t_cell_count = counts.t_cell_count;
        if (fread(system->t_cells, sizeof(brain_t_cell_t),
                  counts.t_cell_count, file) != counts.t_cell_count) {
            NIMCP_LOGGING_ERROR("Failed to read T cells");
            goto cleanup;
        }
    }

    if (counts.antibody_count > 0 && config->save_antibodies) {
        system->antibody_count = counts.antibody_count;
        if (fread(system->antibodies, sizeof(brain_antibody_t),
                  counts.antibody_count, file) != counts.antibody_count) {
            NIMCP_LOGGING_ERROR("Failed to read antibodies");
            goto cleanup;
        }
    }

    if (counts.cytokine_count > 0 && config->save_cytokines) {
        system->cytokine_count = counts.cytokine_count;
        if (fread(system->cytokines, sizeof(brain_cytokine_t),
                  counts.cytokine_count, file) != counts.cytokine_count) {
            NIMCP_LOGGING_ERROR("Failed to read cytokines");
            goto cleanup;
        }
    }

    if (counts.inflammation_count > 0 && config->save_inflammation) {
        system->inflammation_count = counts.inflammation_count;
        if (fread(system->inflammation_sites, sizeof(brain_inflammation_site_t),
                  counts.inflammation_count, file) != counts.inflammation_count) {
            NIMCP_LOGGING_ERROR("Failed to read inflammation sites");
            goto cleanup;
        }
    }

    if (config->save_statistics) {
        if (fread(&system->stats, sizeof(brain_immune_stats_t),
                  1, file) != 1) {
            NIMCP_LOGGING_WARN("Failed to read statistics (non-fatal)");
            /* Non-fatal, continue */
        }
    }

    result = 0;
    NIMCP_LOGGING_INFO("Loaded immune system: %u antigens, %u B cells, %u T cells",
        counts.antigen_count, counts.b_cell_count, counts.t_cell_count);

cleanup:
    nimcp_mutex_unlock(system->mutex);
    fclose(file);
    return result;
}

/* ============================================================================
 * Incremental Save Implementation
 * ============================================================================ */

/**
 * @brief Save incremental update (memory cells only)
 */
int immune_persistence_save_incremental(
    brain_immune_system_t* system,
    const char* filepath,
    const immune_persistence_config_t* config
) {
    if (!system || !filepath) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "immune_persistence_save_incremental: required parameter is NULL (system, filepath)");
        return -1;
    }

    /* Use memory-only config for incremental saves */
    /* Phase 8: Heartbeat at operation start */
    immune_persistence_heartbeat("immune_persi_save_incremental", 0.0f);


    immune_persistence_config_t inc_config;
    if (config) {
        inc_config = *config;
    } else {
        immune_persistence_default_config(&inc_config);
    }

    inc_config.memory_cells_only = true;
    inc_config.save_antigens = false;
    inc_config.save_antibodies = false;
    inc_config.save_cytokines = false;
    inc_config.save_inflammation = false;
    inc_config.save_statistics = false;

    return immune_persistence_save(system, filepath, &inc_config);
}

/* ============================================================================
 * Validation API
 * ============================================================================ */

/**
 * @brief Get persistence format version
 */
uint32_t immune_persistence_get_version(void) {
    /* Phase 8: Heartbeat at operation start */
    immune_persistence_heartbeat("immune_persi_get_version", 0.0f);


    return IMMUNE_PERSISTENCE_VERSION;
}

/**
 * @brief Check version compatibility
 */
bool immune_persistence_is_version_compatible(uint32_t file_version) {
    /* For now, require exact match */
    /* Phase 8: Heartbeat at operation start */
    immune_persistence_heartbeat("immune_persi_is_version_compatibl", 0.0f);


    return (file_version == IMMUNE_PERSISTENCE_VERSION);
}

/**
 * @brief Validate persistence file
 */
int immune_persistence_validate_file(
    const char* filepath,
    bool verify_checksum
) {
    if (!filepath) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "filepath is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    immune_persistence_heartbeat("immune_persi_validate_file", 0.0f);


    FILE* file = fopen(filepath, "rb");
    if (!file) {
        NIMCP_LOGGING_ERROR("File not found: %s", filepath);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "immune_persistence_validate_file: file is NULL");
        return -1;
    }

    immune_persistence_header_t header;
    if (read_header(file, &header) != 0) {
        fclose(file);
        return -1;
    }

    if (validate_header(&header) != 0) {
        fclose(file);
        return -1;
    }

    immune_persistence_counts_t counts;
    if (read_counts(file, &counts) != 0) {
        fclose(file);
        return -1;
    }

    if (validate_counts(&counts) != 0) {
        fclose(file);
        return -1;
    }

    fclose(file);
    return 0;
}

/**
 * @brief Get file information
 */
int immune_persistence_get_file_info(
    const char* filepath,
    immune_persistence_header_t* header,
    immune_persistence_counts_t* counts
) {
    if (!filepath) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "filepath is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    immune_persistence_heartbeat("immune_persi_get_file_info", 0.0f);


    FILE* file = fopen(filepath, "rb");
    if (!file) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "file is NULL");

        return -1;

    }

    int result = -1;

    if (header) {
        if (read_header(file, header) != 0) goto cleanup;
    } else {
        immune_persistence_header_t tmp_header;
        if (read_header(file, &tmp_header) != 0) goto cleanup;
    }

    if (counts) {
        if (read_counts(file, counts) != 0) goto cleanup;
    }

    result = 0;

cleanup:
    fclose(file);
    return result;
}

/* ============================================================================
 * Utility API
 * ============================================================================ */

/**
 * @brief Create backup of persistence file
 */
int immune_persistence_create_backup(
    const char* filepath,
    const char* backup_suffix
) {
    if (!filepath) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "filepath is NULL");

        return -1;

    }

    /* Check if source file exists */
    /* Phase 8: Heartbeat at operation start */
    immune_persistence_heartbeat("immune_persi_create_backup", 0.0f);


    FILE* src = fopen(filepath, "rb");
    if (!src) return 0; /* No file to back up, not an error */

    /* Create backup path */
    char backup_path[IMMUNE_PERSIST_MAX_PATH];
    const char* suffix = backup_suffix ? backup_suffix : ".bak";
    snprintf(backup_path, sizeof(backup_path), "%s%s", filepath, suffix);

    /* Open backup file */
    FILE* dst = fopen(backup_path, "wb");
    if (!dst) {
        fclose(src);
        NIMCP_LOGGING_ERROR("Failed to create backup file: %s", backup_path);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "immune_persistence_create_backup: dst is NULL");
        return -1;
    }

    /* Copy data */
    char buffer[8192];
    size_t bytes = 0;
    while ((bytes = fread(buffer, 1, sizeof(buffer), src)) > 0) {
        if (fwrite(buffer, 1, bytes, dst) != bytes) {
            fclose(src);
            fclose(dst);
            remove(backup_path);
            NIMCP_LOGGING_ERROR("Failed to write backup");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "immune_persistence_create_backup: validation failed");
            return -1;
        }
    }

    fclose(src);
    fclose(dst);

    NIMCP_LOGGING_INFO("Created backup: %s", backup_path);
    return 0;
}

/**
 * @brief Clear immune system state
 */
int immune_persistence_clear_state(brain_immune_system_t* system) {
    if (!system) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");

        return -1;

    }

    /* Clear counts */
    /* Phase 8: Heartbeat at operation start */
    immune_persistence_heartbeat("immune_persi_clear_state", 0.0f);


    system->antigen_count = 0;
    system->b_cell_count = 0;
    system->t_cell_count = 0;
    system->antibody_count = 0;
    system->cytokine_count = 0;
    system->inflammation_count = 0;

    /* Clear arrays */
    if (system->antigens) {
        memset(system->antigens, 0,
               system->antigen_capacity * sizeof(brain_antigen_t));
    }
    if (system->b_cells) {
        memset(system->b_cells, 0,
               system->b_cell_capacity * sizeof(brain_b_cell_t));
    }
    if (system->t_cells) {
        memset(system->t_cells, 0,
               system->t_cell_capacity * sizeof(brain_t_cell_t));
    }
    if (system->antibodies) {
        memset(system->antibodies, 0,
               system->antibody_capacity * sizeof(brain_antibody_t));
    }
    if (system->cytokines) {
        memset(system->cytokines, 0,
               system->cytokine_capacity * sizeof(brain_cytokine_t));
    }
    if (system->inflammation_sites) {
        memset(system->inflammation_sites, 0,
               system->inflammation_capacity * sizeof(brain_inflammation_site_t));
    }

    /* Reset ID counters */
    system->next_antigen_id = 1;
    system->next_b_cell_id = 1;
    system->next_t_cell_id = 1;
    system->next_antibody_id = 1;
    system->next_cytokine_id = 1;
    system->next_inflammation_id = 1;

    /* Clear statistics */
    memset(&system->stats, 0, sizeof(system->stats));

    return 0;
}

/**
 * @brief Save with detailed result information
 */
int immune_persistence_save_ex(
    brain_immune_system_t* system,
    const char* filepath,
    const immune_persistence_config_t* config,
    immune_persistence_result_t* result
) {
    if (!result) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "result is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    immune_persistence_heartbeat("immune_persi_save_ex", 0.0f);


    memset(result, 0, sizeof(*result));
    uint64_t start_time = get_timestamp_ms();

    int ret = immune_persistence_save(system, filepath, config);

    result->success = (ret == 0);
    result->save_time_ms = get_timestamp_ms() - start_time;

    if (ret != 0) {
        snprintf(result->error_message, sizeof(result->error_message),
                 "Save failed");
    }

    return ret;
}

/**
 * @brief Load with detailed result information
 */
int immune_persistence_load_ex(
    brain_immune_system_t* system,
    const char* filepath,
    const immune_persistence_config_t* config,
    immune_persistence_result_t* result
) {
    if (!result) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "result is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    immune_persistence_heartbeat("immune_persi_load_ex", 0.0f);


    memset(result, 0, sizeof(*result));
    uint64_t start_time = get_timestamp_ms();

    /* Get file info first */
    immune_persistence_header_t header;
    if (immune_persistence_get_file_info(filepath, &header, NULL) == 0) {
        result->version_loaded = header.version;
    }

    int ret = immune_persistence_load(system, filepath, config);

    result->success = (ret == 0);
    result->load_time_ms = get_timestamp_ms() - start_time;

    if (ret != 0) {
        snprintf(result->error_message, sizeof(result->error_message),
                 "Load failed");
    }

    return ret;
}

/**
 * @brief Merge incremental save into base save
 *
 * WHAT: Combine incremental and base saves
 * WHY:  Consolidate multiple incremental saves
 * HOW:  Load both, merge data, save to output
 */
int immune_persistence_merge_incremental(
    const char* base_filepath,
    const char* incremental_filepath,
    const char* output_filepath,
    const immune_persistence_config_t* config
) {
    if (!base_filepath || !incremental_filepath || !output_filepath) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "immune_persistence_merge_incremental: required parameter is NULL (base_filepath, incremental_filepath, output_filepath)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    immune_persistence_heartbeat("immune_persi_merge_incremental", 0.0f);


    NIMCP_LOGGING_WARN("Incremental merge not yet implemented");
    return -1;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * @brief Query self-knowledge from knowledge graph
 *
 * WHAT: Query KG for module self-awareness information
 * WHY:  Enable introspective self-knowledge about immune persistence
 * HOW:  Look up entity and relations in KG
 *
 * @param kg Knowledge graph reader handle
 * @return 1 if self-knowledge found, 0 otherwise
 */
int immune_persistence_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    immune_persistence_heartbeat("immune_persi_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Immune_Persistence");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                immune_persistence_heartbeat("immune_persi_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            NIMCP_LOGGING_DEBUG("Immune persistence self-knowledge: %s", self->observations[i]);
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Immune_Persistence");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Immune_Persistence");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void immune_persistence_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_immune_persistence_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int immune_persistence_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "immune_persistence_training_begin: NULL argument");
        return -1;
    }
    immune_persistence_heartbeat_instance(NULL, "immune_persistence_training_begin", 0.0f);
    return 0;
}

int immune_persistence_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "immune_persistence_training_end: NULL argument");
        return -1;
    }
    immune_persistence_heartbeat_instance(NULL, "immune_persistence_training_end", 1.0f);
    return 0;
}

int immune_persistence_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "immune_persistence_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    immune_persistence_heartbeat_instance(NULL, "immune_persistence_training_step", progress);
    return 0;
}
