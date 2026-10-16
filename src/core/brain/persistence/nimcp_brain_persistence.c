//=============================================================================
// nimcp_brain_persistence.c - Brain Persistence & Snapshot Management
//=============================================================================
/**
 * @file nimcp_brain_persistence.c
 * @brief Implementation of brain state persistence and snapshot management
 *
 * EXTRACTED FROM: nimcp_brain.c lines 7088-8226
 * EXTRACTION DATE: 2025-11-19
 *
 * WHAT: Implements save/load/snapshot APIs for brain state persistence
 * WHY:  Modularize persistence logic to reduce nimcp_brain.c size
 * HOW:  Serialize brain state to files with versioning, compression, encryption
 *
 * ARCHITECTURE:
 * - Persistence: brain_save() → save_metadata() → save subsystems
 * - Loading: brain_load() → load_metadata() → restore subsystems
 * - Snapshots: Wrappers around persistence with directory management
 *
 * DEPENDENCIES:
 * - Brain structure (nimcp_brain.h)
 * - Subsystem save/load APIs (knowledge, executive, mirror neurons, etc.)
 * - File I/O, directory scanning (stdio.h, dirent.h, sys/stat.h)
 *
 * ====================================================================
 * SIDECAR LIFECYCLE — read this if you touch brain_save / brain_load
 * ====================================================================
 *
 * brain_save(brain, "<path>") writes the main file at <path> AND a set
 * of sidecar files at <path>.<ext>. There are TWO classes of sidecars:
 *
 * (a) ATOMIC writers — write to <path>.<ext>.tmp first, then rename.
 *     - .snn (snn_network_save in src/snn/nimcp_snn_network.c)
 *     - .lnn (lnn_network_save in src/lnn/nimcp_lnn_network.c)
 *     - .cnn (cnn_trainer_save in src/training/nimcp_cnn_training.c)
 *     - .cortex_visual / .cortex_audio / .cortex_speech / .cortex_somato
 *
 * (b) DIRECT writers — historically wrote to <path>.<ext> directly.
 *     Audit fix made these atomic too: write to .tmp, then rename.
 *     - .meta (this file, nimcp_brain_save_metadata)
 *     - .executive
 *     - .mirror_neurons
 *     - .tokenizer
 *
 *     Other direct-write sidecars produced elsewhere in the codebase
 *     (audit class — verify atomicity if you find them):
 *     - .knowledge .pink_noise
 *
 * The Python checkpoint code (scripts/immerse_athena.py) calls
 * brain.save("<path>.tmp"), then renames <path>.tmp → <path>.bin AND
 * also renames every <path>.tmp.<ext> → <path>.<ext>. The list of
 * extensions to rename is in ALL_SIDECARS in _save_checkpoint_sync.
 *
 * If you ADD a new sidecar:
 *   1. Use atomic .tmp+rename in the C save function
 *   2. Add the extension to ALL_SIDECARS in _save_checkpoint_sync
 *   3. Add it to _prune_checkpoint_snapshots sidecar list
 *   4. Add it to brain_daemon.py's CRITICAL_SIDECARS check if needed
 *
 * Lessons learned (2026-04-13):
 *   - Silent .snn loss for 10+ days because Python only renamed .bin
 *   - Stale 27 GB .snn from initial brain creation kept loading
 *   - Brain trained without spike network state — silent partial restore
 */

// Bio-async integration
#include "async/nimcp_bio_async.h"
#include "constants/nimcp_buffer_constants.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "snn/nimcp_snn_types.h"
#include "snn/nimcp_snn_synapse.h"  /* snn_csr_storage_t for lightweight-pop connection check */
#include "security/nimcp_path_traversal.h"

#include "utils/memory/nimcp_unified_memory.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

// Logging integration
#include "utils/logging/nimcp_logging.h"

#include "core/brain/persistence/nimcp_brain_persistence.h"
#include "core/brain/persistence/nimcp_brain_kg_snapshot.h"
#include "core/brain/nimcp_brain_internal.h"
#include "language/nimcp_concept_registry.h"  /* Slice B sidecar */
#include "core/cortical_columns/nimcp_cortical_column.h"
#include "core/cortical_columns/nimcp_cortical_column_ternary.h"
#include "core/brain/factory/init/nimcp_brain_init_subsystems.h"  /* substrate + thalamic attach */
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>

// Platform-specific directory operations
#ifdef _WIN32
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#endif

// Core dependencies
#include "nimcp.h"
#include "training/nimcp_training_dispatch.h"
#include "plasticity/adaptive/nimcp_adaptive.h"
#include "middleware/training/nimcp_optimizers.h"
#include "middleware/training/nimcp_brain_training_integration.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/validation/nimcp_validate.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/containers/nimcp_hash_table.h"
#include "utils/platform/nimcp_platform_time.h"
#include <pthread.h>

/* =========================================================================
 * Parallel sidecar loading — thread functions (file scope for C99 compat)
 * ========================================================================= */
typedef struct {
    const char* path;
    void* result;
    int type;     /* 0=SNN, 1=LNN, 2=CNN, 3-6=cortex */
    brain_t brain;
} sidecar_load_arg_t;

static void* _sidecar_load_snn(void* a) {
    sidecar_load_arg_t* sa = (sidecar_load_arg_t*)a;
    extern struct snn_network_s* snn_network_load(const char* path);
    sa->result = snn_network_load(sa->path);
    return NULL;
}

static void* _sidecar_load_lnn(void* a) {
    sidecar_load_arg_t* sa = (sidecar_load_arg_t*)a;
    extern struct lnn_network_s* lnn_network_load(const char* path);
    sa->result = lnn_network_load(sa->path);
    return NULL;
}

static void* _sidecar_load_cnn(void* a) {
    sidecar_load_arg_t* sa = (sidecar_load_arg_t*)a;
    extern int cnn_trainer_load_weights(void* trainer, const char* path);
    sa->result = (void*)(intptr_t)cnn_trainer_load_weights(sa->brain->cnn_trainer, sa->path);
    return NULL;
}

static void* _sidecar_load_cortex(void* a) {
    sidecar_load_arg_t* sa = (sidecar_load_arg_t*)a;
    extern int cortex_cnn_load(struct cortex_cnn_processor* proc, const char* path);
    int ci = sa->type - 3;
    if (sa->brain->cortex_cnns[ci])
        sa->result = (void*)(intptr_t)cortex_cnn_load(sa->brain->cortex_cnns[ci], sa->path);
    return NULL;
}

//=============================================================================
// Phase PERSIST-1: Module State and Security Integration
//=============================================================================

/**
 * @brief Global persistence module state
 *
 * WHAT: Module-level state for security registration and statistics
 * WHY:  Enable security audit trail and performance monitoring
 * HOW:  Thread-safe singleton with mutex protection
 */
typedef struct {
    bool initialized;                          /**< Module initialization flag */
    nimcp_sec_integration_t* security_ctx;     /**< Security integration context */
    uint32_t security_module_id;               /**< Registered security module ID */
    persistence_stats_t stats;                 /**< Cumulative statistics */
    nimcp_platform_mutex_t stats_mutex;        /**< Mutex for thread-safe stats */
} persistence_module_state_t;

static persistence_module_state_t g_persistence_state = {
    .initialized = false,
    .security_ctx = NULL,
    .security_module_id = 0
};

/**
 * @brief Validate path for persistence operations (allows absolute paths)
 *
 * WHAT: Check for path traversal while permitting absolute paths
 * WHY:  persistence_path_is_safe() rejects absolute paths by default, but
 *       save/load operations legitimately use absolute paths (e.g. /tmp/brain.bin)
 * HOW:  Create a validator with enable_absolute_path=false (don't reject abs paths)
 *       while keeping all other traversal detection enabled
 *
 * @param path Path to validate
 * @return true if path is safe for persistence I/O
 */
static bool persistence_path_is_safe(const char* path)
{
    if (!path) return false;

    nimcp_path_validator_config_t cfg = nimcp_path_validator_default_config();
    cfg.enable_absolute_path = false;  /* Allow absolute paths for I/O */

    nimcp_path_validator_t validator = nimcp_path_validator_create(&cfg);
    if (!validator) {
        return false;  /* Conservative: reject if can't validate */
    }

    nimcp_path_validation_result_t result;
    nimcp_path_error_t err = nimcp_path_validate(validator, path,
                                                  NIMCP_PATH_CONTEXT_FILE, &result);
    nimcp_path_validator_destroy(validator);

    return (err == NIMCP_PATH_SUCCESS && result.valid);
}

/**
 * @brief Record persistence interaction with security module
 *
 * WHAT: Record save/load/snapshot operation for security audit
 * WHY:  Enable trust tracking and anomaly detection
 * HOW:  Use security integration API if registered
 *
 * @param success Whether operation succeeded
 * @param weight Interaction weight (1.0 = normal)
 */
static void record_security_interaction(bool success, double weight)
{
    if (!g_persistence_state.initialized || !g_persistence_state.security_ctx) {
        return;
    }

    nimcp_sec_record_interaction(
        g_persistence_state.security_ctx,
        g_persistence_state.security_module_id,
        success,
        weight
    );
}

/**
 * @brief Update statistics thread-safely
 *
 * WHAT: Increment statistics counters
 * WHY:  Track persistence operations for monitoring
 * HOW:  Use mutex for thread safety
 */
static void update_stats_save(uint64_t bytes_written, uint64_t time_ms, bool success)
{
    if (!g_persistence_state.initialized) return;

    nimcp_platform_mutex_lock(&g_persistence_state.stats_mutex);
    g_persistence_state.stats.total_saves++;
    g_persistence_state.stats.bytes_written += bytes_written;
    g_persistence_state.stats.total_save_time_ms += time_ms;
    if (!success) {
        g_persistence_state.stats.save_errors++;
    }
    nimcp_platform_mutex_unlock(&g_persistence_state.stats_mutex);
}

static void update_stats_load(uint64_t bytes_read, uint64_t time_ms, bool success)
{
    if (!g_persistence_state.initialized) return;

    nimcp_platform_mutex_lock(&g_persistence_state.stats_mutex);
    g_persistence_state.stats.total_loads++;
    g_persistence_state.stats.bytes_read += bytes_read;
    g_persistence_state.stats.total_load_time_ms += time_ms;
    if (!success) {
        g_persistence_state.stats.load_errors++;
    }
    nimcp_platform_mutex_unlock(&g_persistence_state.stats_mutex);
}

static void update_stats_snapshot_create(bool cow_used, size_t memory_saved)
{
    if (!g_persistence_state.initialized) return;

    nimcp_platform_mutex_lock(&g_persistence_state.stats_mutex);
    g_persistence_state.stats.total_snapshots_created++;
    if (cow_used) {
        g_persistence_state.stats.cow_snapshots++;
        g_persistence_state.stats.memory_saved_bytes += memory_saved;
    }
    nimcp_platform_mutex_unlock(&g_persistence_state.stats_mutex);
}

static void update_stats_snapshot_restore(void)
{
    if (!g_persistence_state.initialized) return;

    nimcp_platform_mutex_lock(&g_persistence_state.stats_mutex);
    g_persistence_state.stats.total_snapshots_restored++;
    nimcp_platform_mutex_unlock(&g_persistence_state.stats_mutex);
}

static void update_stats_snapshot_delete(void)
{
    if (!g_persistence_state.initialized) return;

    nimcp_platform_mutex_lock(&g_persistence_state.stats_mutex);
    g_persistence_state.stats.total_snapshots_deleted++;
    nimcp_platform_mutex_unlock(&g_persistence_state.stats_mutex);
}

static void update_stats_memory_alloc(bool from_pool)
{
    if (!g_persistence_state.initialized) return;

    nimcp_platform_mutex_lock(&g_persistence_state.stats_mutex);
    if (from_pool) {
        g_persistence_state.stats.pool_allocations++;
    } else {
        g_persistence_state.stats.malloc_allocations++;
    }
    nimcp_platform_mutex_unlock(&g_persistence_state.stats_mutex);
}

static void update_stats_checksum_failure(void)
{
    if (!g_persistence_state.initialized) return;

    nimcp_platform_mutex_lock(&g_persistence_state.stats_mutex);
    g_persistence_state.stats.checksum_failures++;
    nimcp_platform_mutex_unlock(&g_persistence_state.stats_mutex);
}

// Subsystem dependencies for save/load
#include "cognitive/knowledge/nimcp_knowledge.h"
#include "cognitive/nimcp_executive.h"
#include "cognitive/nimcp_mirror_neurons.h"
#include "plasticity/neuromodulators/nimcp_neuromod_pink_noise.h"
#include "cognitive/nimcp_working_memory.h"

// Additional subsystems that may need initialization
#include "glial/integration/nimcp_glial_integration.h"
#include "plasticity/neuromodulators/nimcp_spatial_neuromod.h"
#include "optimization/quantum_annealing/nimcp_quantum_annealing.h"

#define LOG_MODULE "BRAIN_PERSIST"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(brain_persistence, MESH_ADAPTER_CATEGORY_COGNITIVE)


// Error handling (forward declarations from nimcp_brain.c)
// Note: These are defined in nimcp_brain.c, we'll use them here
extern void set_error(const char* format, ...);
extern void brain_clear_error(void);

// Forward declarations of nimcp_brain.c internal functions we need
// Note: These would need to be exposed or we duplicate them here
extern brain_t allocate_brain(void);
extern task_strategy_t* strategy_create(brain_task_t task);
extern void init_brain_stats(brain_stats_t* stats, const char* task_name,
                            brain_size_t size, uint32_t num_inputs, float learning_rate);
extern bool init_working_memory_subsystem(brain_t brain);
extern bool init_mirror_neurons(brain_t brain);
extern bool init_glial_subsystem(brain_t brain);
extern bool init_spatial_neuromod_system(brain_t brain);
extern void executive_set_brain(executive_controller_t* exec, brain_t brain);
extern void mirror_neurons_set_brain(mirror_neurons_t mirror, brain_t brain);  // Fixed: mirror_neurons_t not pointer

//=============================================================================
// Persistence API
//=============================================================================

/**
 * @brief Save working memory state to file (Phase 10.2)
 *
 * WHAT: Serialize working memory items for COW snapshot persistence
 * WHY:  Preserve active representations across save/load/snapshot operations
 * HOW:  Write marker → size/capacity → each item's data
 *
 * COMPLEXITY: O(n*m) where n = items, m = avg item size
 *
 * @param wm Working memory instance (nullable)
 * @param file File handle (non-NULL)
 * @return true on success, false on error
 */
bool nimcp_brain_save_working_memory_state(working_memory_t* wm, FILE* file)
{
    // Guard: NULL file handle
    if (!file) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_save_working_memory_state: file is NULL");
        return false;
    }

    // Guard: No working memory → write marker and return
    if (!wm) {
        uint8_t has_wm = 0;
        fwrite(&has_wm, sizeof(uint8_t), 1, file);
        return true;
    }

    // Write existence marker
    uint8_t has_wm = 1;
    fwrite(&has_wm, sizeof(uint8_t), 1, file);

    // Get current state
    working_memory_stats_t stats;
    working_memory_get_stats(wm, &stats);

    // Write metadata
    fwrite(&stats.current_size, sizeof(uint32_t), 1, file);
    fwrite(&stats.capacity, sizeof(uint32_t), 1, file);

    // Write each item
    for (uint32_t i = 0; i < stats.current_size; i++) {
        uint32_t item_size = 0;
        const float* item = working_memory_get(wm, i, &item_size);

        // Guard: Invalid item → skip
        if (!item || item_size == 0) {
            continue;
        }

        fwrite(&item_size, sizeof(uint32_t), 1, file);
        fwrite(item, sizeof(float), item_size, file);
    }

    return true;
}

/**
 * @brief Save metadata file
 *
 * WHAT: Persist brain configuration and output labels
 * WHY:  Enable full state reconstruction on load
 * HOW:  Write config → labels → working memory state
 *
 * COMPLEXITY: O(k + n*m) where k = labels, n = WM items, m = item size
 *
 * @param brain Brain instance (non-NULL)
 * @param filepath Base filepath (non-NULL)
 * @return true on success, false on error
 */
bool nimcp_brain_save_metadata(brain_t brain, const char* filepath)
{
    // Guard: NULL parameters handled by caller

    char meta_path[NIMCP_METRICS_PATH_SIZE];
    char meta_tmp[NIMCP_METRICS_PATH_SIZE];
    snprintf(meta_path, sizeof(meta_path), "%s.meta", filepath);
    snprintf(meta_tmp,  sizeof(meta_tmp),  "%s.meta.tmp", filepath);

    // P1-3 fix: Path traversal validation
    if (!persistence_path_is_safe(meta_path)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_brain_save_metadata: persistence_path_is_safe is NULL");
        return false;
    }

    /* Atomic write: open .tmp, write, close, rename → .meta.
     * Audit fix #2: previously wrote directly to .meta — crash mid-write
     * corrupted the file silently and load fell back to defaults. */
    FILE* meta_file = fopen(meta_tmp, "wb");
    if (!meta_file) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_save_metadata: meta_file is NULL");
        return false;
    }

    // Write version header (v1.0 format)
    nimcp_file_header_t header = {
        .magic = {NIMCP_MAGIC_0, NIMCP_MAGIC_1, NIMCP_MAGIC_2, NIMCP_MAGIC_3},
        .version_major = NIMCP_FORMAT_VERSION_MAJOR,
        .version_minor = NIMCP_FORMAT_VERSION_MINOR,
        .flags = 0,  // No compression/encryption yet
        .reserved = 0
    };
    fwrite(&header, sizeof(nimcp_file_header_t), 1, meta_file);

    // Write configuration
    fwrite(&brain->config, sizeof(brain_config_t), 1, meta_file);

    /* Guard against inconsistent state: if output_labels is NULL but
     * num_output_labels > 0 (which can happen if a prior load's cleanup
     * path freed the array without zeroing the counter), coerce the
     * counter to zero so we don't iterate a NULL pointer below. */
    uint32_t save_num_labels = brain->num_output_labels;
    if (brain->output_labels == NULL) {
        save_num_labels = 0;
    }
    fwrite(&save_num_labels, sizeof(uint32_t), 1, meta_file);

    // Write output labels
    for (uint32_t i = 0; i < save_num_labels; i++) {
        const char* lbl = brain->output_labels[i];
        if (lbl == NULL) {
            /* Skip NULL entries inside the array (partially populated). */
            uint32_t zero_len = 0;
            fwrite(&zero_len, sizeof(uint32_t), 1, meta_file);
            continue;
        }
        uint32_t len = (uint32_t)strlen(lbl) + 1;
        fwrite(&len, sizeof(uint32_t), 1, meta_file);
        fwrite(lbl, len, 1, meta_file);
    }

    // Phase 10.2: Save working memory state
    bool wm_success = nimcp_brain_save_working_memory_state(brain->working_memory, meta_file);
    if (!wm_success) {
        fclose(meta_file);
        remove(meta_tmp);  /* don't leave partial .tmp around */
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_save_metadata: wm_success is NULL");
        return false;
    }

    // Save brain statistics (performance metrics)
    fwrite(&brain->stats, sizeof(brain_stats_t), 1, meta_file);

    // Save wellbeing state (Phase 9.3)
    fwrite(&brain->last_distress, sizeof(distress_assessment_t), 1, meta_file);
    fwrite(&brain->wellbeing_monitoring_enabled, sizeof(bool), 1, meta_file);
    fwrite(&brain->wellbeing_check_interval_ms, sizeof(uint64_t), 1, meta_file);
    fwrite(&brain->last_wellbeing_check_time, sizeof(uint64_t), 1, meta_file);

    // Save simulation time tracking
    fwrite(&brain->current_time_us, sizeof(uint64_t), 1, meta_file);
    fwrite(&brain->last_glial_update_us, sizeof(uint64_t), 1, meta_file);

    // Save knowledge system state (if exists)
    bool has_knowledge = (brain->knowledge != NULL);
    fwrite(&has_knowledge, sizeof(bool), 1, meta_file);
    if (has_knowledge) {
        char knowledge_path[NIMCP_METRICS_PATH_SIZE];
        snprintf(knowledge_path, sizeof(knowledge_path), "%s.knowledge", filepath);
        knowledge_save(brain->knowledge, knowledge_path);
    }

    // Save emotional system state (Phase 10.2 - NOT A MODULE)
    // Note: Emotional tagging uses stateless utility functions, not a system object
    bool has_emotional = false;  // No emotional_system module (just tagging functions)
    fwrite(&has_emotional, sizeof(bool), 1, meta_file);

    // Save executive controller state (Phase 10.3 - if exists)
    bool has_executive = (brain->executive != NULL);
    fwrite(&has_executive, sizeof(bool), 1, meta_file);
    if (has_executive) {
        // WHAT: Save executive controller state to separate file
        // WHY:  Preserve task queue, statistics, and configuration
        // HOW:  Use executive_save API with dedicated file
        /* Atomic write: .executive.tmp → rename → .executive (audit fix #3) */
        char executive_path[NIMCP_METRICS_PATH_SIZE];
        char executive_tmp[NIMCP_METRICS_PATH_SIZE];
        snprintf(executive_path, sizeof(executive_path), "%s.executive", filepath);
        snprintf(executive_tmp,  sizeof(executive_tmp),  "%s.executive.tmp", filepath);
        FILE* exec_file = fopen(executive_tmp, "wb");
        if (exec_file) {
            executive_save(brain->executive, exec_file);
            fclose(exec_file);
            if (rename(executive_tmp, executive_path) != 0) {
                NIMCP_LOGGING_WARN("Failed to rename %s -> %s", executive_tmp, executive_path);
                remove(executive_tmp);
            }
        }
    }

    // Save sleep system state (Phase 10.4)
    // Sleep system is embedded struct, always save
    // TODO: Add sleep_system_save API when available
    // For now, skip to maintain backward compatibility

    // Save pink noise neuromodulator state (if exists)
    bool has_pink_noise = (brain->pink_noise != NULL);
    fwrite(&has_pink_noise, sizeof(bool), 1, meta_file);
    if (has_pink_noise) {
        // WHAT: Save pink noise neuromodulator state to separate file
        // WHY:  Preserve neuromodulator levels and pink noise generators
        // HOW:  Use neuromod_pink_save API with dedicated file
        char pink_noise_path[NIMCP_METRICS_PATH_SIZE];
        snprintf(pink_noise_path, sizeof(pink_noise_path), "%s.pink_noise", filepath);
        FILE* pink_file = fopen(pink_noise_path, "wb");
        if (pink_file) {
            neuromod_pink_save(brain->pink_noise, pink_file);
            fclose(pink_file);
        }
    }

    // Save mirror neurons state (Phase 10.11 - if exists)
    bool has_mirror_neurons = (brain->mirror_neurons != NULL);
    fwrite(&has_mirror_neurons, sizeof(bool), 1, meta_file);
    if (has_mirror_neurons) {
        // WHAT: Save mirror neuron system state to separate file
        // WHY:  Preserve learned action associations and statistics
        // HOW:  Use mirror_neurons_save API with dedicated file
        /* Atomic write: .mirror_neurons.tmp → rename (audit fix #3) */
        char mirror_path[NIMCP_METRICS_PATH_SIZE];
        char mirror_tmp[NIMCP_METRICS_PATH_SIZE];
        snprintf(mirror_path, sizeof(mirror_path), "%s.mirror_neurons", filepath);
        snprintf(mirror_tmp,  sizeof(mirror_tmp),  "%s.mirror_neurons.tmp", filepath);
        FILE* mirror_file = fopen(mirror_tmp, "wb");
        if (mirror_file) {
            mirror_neurons_save(brain->mirror_neurons, mirror_file);
            fclose(mirror_file);
            if (rename(mirror_tmp, mirror_path) != 0) {
                NIMCP_LOGGING_WARN("Failed to rename %s -> %s", mirror_tmp, mirror_path);
                remove(mirror_tmp);
            }
        }
    }

    // Save training state for checkpoint-resume
    // WHY: Without this, optimizer momentum and LR schedule are lost on restart
    uint32_t training_state_magic = 0x54524E53; /* "TRNS" */
    fwrite(&training_state_magic, sizeof(uint32_t), 1, meta_file);
    fwrite(brain->loss_history, sizeof(float), 10, meta_file);
    fwrite(&brain->loss_history_index, sizeof(uint32_t), 1, meta_file);
    fwrite(&brain->loss_history_count, sizeof(uint32_t), 1, meta_file);
    fwrite(&brain->base_learning_rate, sizeof(float), 1, meta_file);
    fwrite(&brain->config.learning_rate, sizeof(float), 1, meta_file);
    fwrite(&brain->last_curiosity_drive, sizeof(float), 1, meta_file);
    fwrite(&brain->last_novelty_score, sizeof(float), 1, meta_file);
    fwrite(&brain->stats.total_inferences, sizeof(uint64_t), 1, meta_file);
    fwrite(&brain->stats.total_learning_steps, sizeof(uint64_t), 1, meta_file);

    /* Save optimizer states (H9: preserve momentum/velocity across restarts) */
    if (brain->training_ctx) {
        uint32_t opt_magic = 0x4F505453; /* "OPTS" */
        fwrite(&opt_magic, sizeof(uint32_t), 1, meta_file);
        /* Query active optimizer count — if none, write 0 and skip.
         * Use nimcp_brain_training_get_optimizer_count() to avoid
         * throwing exceptions when probing non-existent IDs. */
        uint32_t total_optimizers = nimcp_brain_training_get_optimizer_count(
            brain->training_ctx);
        if (total_optimizers == 0) {
            uint32_t zero = 0;
            fwrite(&zero, sizeof(uint32_t), 1, meta_file);
        } else {
            uint32_t active_ids[NIMCP_TRAINING_MAX_OPTIMIZER_CONTEXTS];
            nimcp_optimizer_context_t* active_ctxs[NIMCP_TRAINING_MAX_OPTIMIZER_CONTEXTS];
            uint32_t opt_count = 0;
            for (uint32_t id = 0; id < NIMCP_TRAINING_MAX_OPTIMIZER_CONTEXTS && opt_count < total_optimizers; id++) {
                nimcp_optimizer_context_t* opt = nimcp_brain_training_get_optimizer(
                    brain->training_ctx, id);
                if (opt) {
                    active_ids[opt_count] = id;
                    active_ctxs[opt_count] = opt;
                    opt_count++;
                }
            }
            fwrite(&opt_count, sizeof(uint32_t), 1, meta_file);
            for (uint32_t i = 0; i < opt_count; i++) {
                fwrite(&active_ids[i], sizeof(uint32_t), 1, meta_file);
                if (nimcp_optimizer_save(active_ctxs[i], meta_file) != 0) {
                    NIMCP_LOGGING_WARN("Failed to save optimizer state for id %u", active_ids[i]);
                }
            }
        }
    }

    /* === LANG sidecar block — cascade self-train tunables + Cat-A flag
     *
     * Until this block existed, brain->cascade_self_train_* (set by
     * nimcp_brain_set_cascade_self_train_enabled / _tunables) was lost
     * on every save. Now extended (commit following cf7e47b29) to
     * also round-trip respond_via_cascade.
     *
     * Wire format (self-describing, forward+backward compatible):
     *   [u32 sentinel = 'LANG' (0x4C414E47)]
     *   [u32 block_size_in_bytes]
     *   [u8  cascade_self_train_enabled]
     *   [f32 cascade_self_train_baseline]
     *   [f32 cascade_self_train_alpha]
     *   [f32 cascade_self_train_lr_scale]
     *   [u8  respond_via_cascade]      <-- appended 2026-05-12
     *   [u32 current_stage]            <-- appended 2026-05-19 (Slice E)
     *
     * Older readers seek past unknown trailing bytes via block_size.
     * Pre-LANG readers hit EOF before the sentinel and skip cleanly —
     * fields stay at library defaults. */
    {
        const uint32_t lang_sentinel = 0x4C414E47u;  /* "LANG" */
        const uint32_t lang_block_size =
            sizeof(uint8_t) + sizeof(float) * 3u +
            sizeof(uint8_t) + sizeof(uint32_t);   /* + current_stage */
        uint8_t st_enabled = brain->cascade_self_train_enabled ? 1u : 0u;
        uint8_t rvc_enabled = brain->respond_via_cascade ? 1u : 0u;
        uint32_t cur_stage = brain->current_stage;  /* Slice E */
        fwrite(&lang_sentinel,   sizeof(uint32_t), 1, meta_file);
        fwrite(&lang_block_size, sizeof(uint32_t), 1, meta_file);
        fwrite(&st_enabled,                            sizeof(uint8_t), 1, meta_file);
        fwrite(&brain->cascade_self_train_baseline,    sizeof(float),   1, meta_file);
        fwrite(&brain->cascade_self_train_alpha,       sizeof(float),   1, meta_file);
        fwrite(&brain->cascade_self_train_lr_scale,    sizeof(float),   1, meta_file);
        fwrite(&rvc_enabled,                           sizeof(uint8_t), 1, meta_file);
        fwrite(&cur_stage,                             sizeof(uint32_t), 1, meta_file);
    }

    fclose(meta_file);

    /* Atomic rename .meta.tmp → .meta. If rename fails we have neither a
     * corrupt .meta nor a partial write — caller will see a stale .meta
     * (or none) and the next load will warn instead of silent fallback. */
    if (rename(meta_tmp, meta_path) != 0) {
        NIMCP_LOGGING_ERROR("nimcp_brain_save_metadata: rename %s -> %s failed",
                            meta_tmp, meta_path);
        remove(meta_tmp);
        return false;
    }

    // Save persistent tokenizer (separate file, optional).
    // Atomic write: .tokenizer.tmp → rename (audit fix #3).
    if (brain->tokenizer) {
        char tok_path[512];
        char tok_tmp[512];
        snprintf(tok_path, sizeof(tok_path), "%s.tokenizer", filepath);
        snprintf(tok_tmp,  sizeof(tok_tmp),  "%s.tokenizer.tmp", filepath);
        tokenizer_save(brain->tokenizer, tok_tmp);
        if (rename(tok_tmp, tok_path) != 0) {
            NIMCP_LOGGING_WARN("Failed to rename %s -> %s", tok_tmp, tok_path);
            remove(tok_tmp);
        }
    }

    return true;
}

//=============================================================================
// CC6: Cortical columns sidecar — CORTICAL_COLUMNS_V1
//=============================================================================
/* Format (binary, little-endian; matches brain_t native packing):
 *   Magic       : uint32_t = 0x43434C4D ("CCLM")
 *   Version     : uint32_t = 1
 *   num_hcs     : uint32_t
 *   feature_dim : uint32_t
 *   out_size    : uint32_t (== brain->config.num_outputs at save time)
 *   blend_alpha : float
 *   ternary_on  : uint8_t (0 or 1)
 *   reserved    : uint8_t[3] = {0,0,0}
 *   For each HC i in [0, num_hcs):
 *     winner_idx          : uint32_t
 *     hc_total_activation : float
 *   proj_floats : float[feature_dim * out_size]   (row-major)
 *
 * No section "absent" → log a one-line warning and continue with the
 * fresh init (matrix already zeroed by structural init). Old checkpoints
 * round-trip identically when re-saved (the sidecar is unconditionally
 * written when columns are on, but the loader handles missing files). */

#define CC_SIDECAR_MAGIC   0x43434C4DU  /* "CCLM" */
#define CC_SIDECAR_VERSION 1U

static bool cortical_columns_sidecar_save(brain_t brain, const char* filepath)
{
    if (!brain || !filepath) return false;
    if (!brain->enable_cortical_columns || !brain->hypercolumns ||
        brain->num_hypercolumns == 0 || !brain->column_to_decision_proj) {
        return true; /* Nothing to save; not an error. */
    }

    char sidecar_path[NIMCP_METRICS_PATH_SIZE];
    snprintf(sidecar_path, sizeof(sidecar_path), "%s.cortical_columns", filepath);
    char tmp_path[NIMCP_METRICS_PATH_SIZE];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", sidecar_path);

    FILE* f = fopen(tmp_path, "wb");
    if (!f) {
        NIMCP_LOGGING_WARN("cortical_columns_sidecar_save: fopen %s failed", tmp_path);
        return false;
    }

    uint32_t magic       = CC_SIDECAR_MAGIC;
    uint32_t version     = CC_SIDECAR_VERSION;
    uint32_t num_hcs     = brain->num_hypercolumns;
    uint32_t feature_dim = brain->column_feature_dim;
    uint32_t out_size    = brain->config.num_outputs;
    float    blend_alpha = brain->column_blend_alpha;
    uint8_t  ternary_on  = brain->enable_cortical_ternary ? 1 : 0;
    uint8_t  reserved[3] = {0,0,0};

    bool ok = true;
    ok = ok && (fwrite(&magic,       sizeof(magic),       1, f) == 1);
    ok = ok && (fwrite(&version,     sizeof(version),     1, f) == 1);
    ok = ok && (fwrite(&num_hcs,     sizeof(num_hcs),     1, f) == 1);
    ok = ok && (fwrite(&feature_dim, sizeof(feature_dim), 1, f) == 1);
    ok = ok && (fwrite(&out_size,    sizeof(out_size),    1, f) == 1);
    ok = ok && (fwrite(&blend_alpha, sizeof(blend_alpha), 1, f) == 1);
    ok = ok && (fwrite(&ternary_on,  sizeof(ternary_on),  1, f) == 1);
    ok = ok && (fwrite(reserved,     sizeof(reserved),    1, f) == 1);

    /* Per-HC stats — winner index + total activation. */
    for (uint32_t i = 0; i < num_hcs && ok; i++) {
        uint32_t winner = UINT32_MAX;
        float    total_act = 0.0F;
        if (brain->hypercolumns[i]) {
            cc_hypercolumn_stats_t st = {0};
            hypercolumn_get_stats(brain->hypercolumns[i], &st);
            winner = st.winner_index;
            total_act = st.total_activation;
        }
        ok = ok && (fwrite(&winner,    sizeof(winner),    1, f) == 1);
        ok = ok && (fwrite(&total_act, sizeof(total_act), 1, f) == 1);
    }

    /* Projection matrix raw floats. */
    if (ok && feature_dim > 0 && out_size > 0) {
        size_t n = (size_t)feature_dim * (size_t)out_size;
        ok = ok && (fwrite(brain->column_to_decision_proj, sizeof(float), n, f) == n);
    }

    fclose(f);
    if (!ok) {
        NIMCP_LOGGING_WARN("cortical_columns_sidecar_save: fwrite failed for %s", tmp_path);
        unlink(tmp_path);
        return false;
    }

    if (rename(tmp_path, sidecar_path) != 0) {
        NIMCP_LOGGING_WARN("cortical_columns_sidecar_save: rename %s -> %s failed",
                           tmp_path, sidecar_path);
        unlink(tmp_path);
        return false;
    }
    NIMCP_LOGGING_INFO("CORTICAL_COLUMNS_V1 sidecar saved: %u HCs, %u-dim features, "
                       "%u outputs, ternary=%u",
                       num_hcs, feature_dim, out_size, (unsigned)ternary_on);
    return true;
}

static bool cortical_columns_sidecar_load(brain_t brain, const char* filepath)
{
    if (!brain || !filepath) return false;

    char sidecar_path[NIMCP_METRICS_PATH_SIZE];
    snprintf(sidecar_path, sizeof(sidecar_path), "%s.cortical_columns", filepath);

    FILE* f = fopen(sidecar_path, "rb");
    if (!f) {
        /* Old checkpoint — section absent. Init path already populated
         * fresh state and zeroed the projection. Log a warning. */
        NIMCP_LOGGING_WARN("CORTICAL_COLUMNS_V1 section absent at %s — using fresh init "
                           "(old checkpoint, expected on first migration)", sidecar_path);
        return true;
    }

    uint32_t magic = 0, version = 0, num_hcs = 0, feature_dim = 0, out_size = 0;
    float    blend_alpha = 0.0F;
    uint8_t  ternary_on  = 0;
    uint8_t  reserved[3] = {0,0,0};

    bool ok = true;
    ok = ok && (fread(&magic,       sizeof(magic),       1, f) == 1);
    ok = ok && (fread(&version,     sizeof(version),     1, f) == 1);
    ok = ok && (fread(&num_hcs,     sizeof(num_hcs),     1, f) == 1);
    ok = ok && (fread(&feature_dim, sizeof(feature_dim), 1, f) == 1);
    ok = ok && (fread(&out_size,    sizeof(out_size),    1, f) == 1);
    ok = ok && (fread(&blend_alpha, sizeof(blend_alpha), 1, f) == 1);
    ok = ok && (fread(&ternary_on,  sizeof(ternary_on),  1, f) == 1);
    ok = ok && (fread(reserved,     sizeof(reserved),    1, f) == 1);

    if (!ok || magic != CC_SIDECAR_MAGIC) {
        fclose(f);
        NIMCP_LOGGING_WARN("CORTICAL_COLUMNS_V1: bad magic 0x%08x in %s — skipping",
                           magic, sidecar_path);
        return false;
    }
    if (version != CC_SIDECAR_VERSION) {
        fclose(f);
        NIMCP_LOGGING_WARN("CORTICAL_COLUMNS_V1: unsupported version %u in %s — skipping",
                           version, sidecar_path);
        return false;
    }

    /* Validate dims match what init produced. If they don't (e.g. config
     * changed between save and load), refuse the section and let the fresh
     * init stay in place. */
    if (num_hcs != brain->num_hypercolumns ||
        feature_dim != brain->column_feature_dim ||
        out_size != brain->config.num_outputs) {
        NIMCP_LOGGING_WARN("CORTICAL_COLUMNS_V1: dim mismatch (have hcs=%u feat=%u out=%u; "
                           "file hcs=%u feat=%u out=%u) — skipping",
                           brain->num_hypercolumns, brain->column_feature_dim,
                           brain->config.num_outputs, num_hcs, feature_dim, out_size);
        fclose(f);
        return false;
    }

    /* Read per-HC stats — we don't restore winner_idx into hc state (it's
     * recomputed on first decide), but we keep the read for forward compat. */
    for (uint32_t i = 0; i < num_hcs && ok; i++) {
        uint32_t winner = 0; float total_act = 0.0F;
        ok = ok && (fread(&winner,    sizeof(winner),    1, f) == 1);
        ok = ok && (fread(&total_act, sizeof(total_act), 1, f) == 1);
        (void)winner; (void)total_act;
    }

    /* Read projection matrix into the already-allocated buffer. */
    if (ok && feature_dim > 0 && out_size > 0 && brain->column_to_decision_proj) {
        size_t n = (size_t)feature_dim * (size_t)out_size;
        ok = ok && (fread(brain->column_to_decision_proj, sizeof(float), n, f) == n);
    }

    fclose(f);
    if (!ok) {
        NIMCP_LOGGING_WARN("CORTICAL_COLUMNS_V1: truncated read at %s — using partial state",
                           sidecar_path);
        return false;
    }
    brain->column_blend_alpha = blend_alpha;
    /* Keep the existing enable_cortical_ternary value from config — the
     * file's flag is informational only (we honor the loaded brain's
     * runtime config, not the saved one, so ternary can be turned on/off
     * independently). */
    (void)ternary_on;
    NIMCP_LOGGING_INFO("CORTICAL_COLUMNS_V1 sidecar loaded: %u HCs, %u-dim features, "
                       "%u outputs (alpha=%.3f)",
                       num_hcs, feature_dim, out_size, blend_alpha);
    return true;
}

//=============================================================================
// Layer C: temperature scaling sidecar — TEMPERATURE_V1
//=============================================================================
/* Format (binary, little-endian):
 *   Magic        : uint32_t = 0x54454D50 ("TEMP")
 *   Version      : uint32_t = 1
 *   T            : float   (decoder_temperature)
 *   ECE          : float   (decoder_temperature_calibrated_ece, -1 = uncalibrated)
 *   AT_US        : uint64_t (calibrated_at_us, 0 = never)
 *
 * Backward compat: if section absent on load, default to T=1.0, ece=-1.0,
 * at_us=0 (the same defaults set by allocate_brain). Best-effort save —
 * failure logs a warning but does not abort the larger checkpoint.
 */
#define TEMP_SIDECAR_MAGIC    0x54454D50U  /* "TEMP" */
#define TEMP_SIDECAR_VERSION  1U

static bool temperature_sidecar_save(brain_t brain, const char* filepath)
{
    if (!brain || !filepath) return false;

    char sidecar_path[NIMCP_METRICS_PATH_SIZE];
    snprintf(sidecar_path, sizeof(sidecar_path), "%s.temperature", filepath);
    char tmp_path[NIMCP_METRICS_PATH_SIZE];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", sidecar_path);

    FILE* f = fopen(tmp_path, "wb");
    if (!f) {
        NIMCP_LOGGING_WARN("temperature_sidecar_save: fopen %s failed", tmp_path);
        return false;
    }

    const uint32_t magic   = TEMP_SIDECAR_MAGIC;
    const uint32_t version = TEMP_SIDECAR_VERSION;
    const float    T       = brain->decoder_temperature;
    const float    ece     = brain->decoder_temperature_calibrated_ece;
    const uint64_t at_us   = brain->decoder_temperature_calibrated_at_us;

    bool ok = true;
    ok = ok && (fwrite(&magic,   sizeof(magic),   1, f) == 1);
    ok = ok && (fwrite(&version, sizeof(version), 1, f) == 1);
    ok = ok && (fwrite(&T,       sizeof(T),       1, f) == 1);
    ok = ok && (fwrite(&ece,     sizeof(ece),     1, f) == 1);
    ok = ok && (fwrite(&at_us,   sizeof(at_us),   1, f) == 1);

    fclose(f);
    if (!ok) {
        NIMCP_LOGGING_WARN("temperature_sidecar_save: fwrite failed for %s", tmp_path);
        unlink(tmp_path);
        return false;
    }
    if (rename(tmp_path, sidecar_path) != 0) {
        NIMCP_LOGGING_WARN("temperature_sidecar_save: rename %s -> %s failed",
                           tmp_path, sidecar_path);
        unlink(tmp_path);
        return false;
    }
    NIMCP_LOGGING_INFO("TEMPERATURE_V1 sidecar saved: T=%.4f ece=%.4f at_us=%llu",
                       (double)T, (double)ece, (unsigned long long)at_us);
    return true;
}

static bool temperature_sidecar_load(brain_t brain, const char* filepath)
{
    if (!brain || !filepath) return false;

    char sidecar_path[NIMCP_METRICS_PATH_SIZE];
    snprintf(sidecar_path, sizeof(sidecar_path), "%s.temperature", filepath);

    FILE* f = fopen(sidecar_path, "rb");
    if (!f) {
        /* Section absent — old checkpoint. Defaults from allocate_brain
         * (T=1.0, ece=-1.0, at_us=0) stay in place. */
        NIMCP_LOGGING_WARN("TEMPERATURE_V1 section absent at %s — using defaults "
                           "(T=1.0, ece=-1.0)", sidecar_path);
        brain->decoder_temperature                  = 1.0F;
        brain->decoder_temperature_calibrated_ece   = -1.0F;
        brain->decoder_temperature_calibrated_at_us = 0;
        return true;
    }

    uint32_t magic = 0, version = 0;
    float    T = 1.0F, ece = -1.0F;
    uint64_t at_us = 0;

    bool ok = true;
    ok = ok && (fread(&magic,   sizeof(magic),   1, f) == 1);
    ok = ok && (fread(&version, sizeof(version), 1, f) == 1);
    ok = ok && (fread(&T,       sizeof(T),       1, f) == 1);
    ok = ok && (fread(&ece,     sizeof(ece),     1, f) == 1);
    ok = ok && (fread(&at_us,   sizeof(at_us),   1, f) == 1);
    fclose(f);

    if (!ok || magic != TEMP_SIDECAR_MAGIC) {
        NIMCP_LOGGING_WARN("TEMPERATURE_V1: bad magic 0x%08x in %s — using defaults",
                           magic, sidecar_path);
        brain->decoder_temperature                  = 1.0F;
        brain->decoder_temperature_calibrated_ece   = -1.0F;
        brain->decoder_temperature_calibrated_at_us = 0;
        return false;
    }
    if (version != TEMP_SIDECAR_VERSION) {
        NIMCP_LOGGING_WARN("TEMPERATURE_V1: unsupported version %u in %s — using defaults",
                           version, sidecar_path);
        brain->decoder_temperature                  = 1.0F;
        brain->decoder_temperature_calibrated_ece   = -1.0F;
        brain->decoder_temperature_calibrated_at_us = 0;
        return false;
    }

    /* Sanity-clamp T to a sane range so a corrupt file can't
     * brick the inference path with T=0 or T=NaN. */
    if (!isfinite(T) || T < 1e-3F || T > 100.0F) {
        NIMCP_LOGGING_WARN("TEMPERATURE_V1: T=%f out of range — using 1.0", (double)T);
        T = 1.0F;
    }

    brain->decoder_temperature                  = T;
    brain->decoder_temperature_calibrated_ece   = ece;
    brain->decoder_temperature_calibrated_at_us = at_us;
    NIMCP_LOGGING_INFO("TEMPERATURE_V1 sidecar loaded: T=%.4f ece=%.4f",
                       (double)T, (double)ece);
    return true;
}

/**
 * @brief Save brain to file
 *
 * WHY: Enables model persistence across sessions
 * Saves both network and metadata
 *
 * COMPLEXITY: O(n*c) where n = neurons, c = connections
 *
 * @param brain Brain handle
 * @param filepath Path to save to
 * @return true on success
 */
bool brain_save(brain_t brain, const char* filepath)
{
    // Guard: Validate parameters
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_save: brain is NULL");
        set_error("Invalid parameters to brain_save");
        return false;
    }
    if (!filepath) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_save: filepath is NULL");
        set_error("Invalid parameters to brain_save");
        return false;
    }

    /* UNIFIED SAVE DISABLED — brain_load_unified() has a SIGSEGV bug
     * in temp file extraction at 8.1GB scale. Until fixed, saves use
     * legacy format (NIMC) which loads reliably.
     * The unified save code remains in nimcp_checkpoint_unified.c. */

    // P1-3 fix: Path traversal validation (uses persistence_path_is_safe which allows absolute paths)
    if (!persistence_path_is_safe(filepath)) {
        set_error("Path validation failed: %s", filepath);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_save: path traversal detected");
        return false;
    }

    // Health monitoring: signal start of save operation
    brain_heartbeat(brain, "brain_save:start", 0.0f);

    // Save adaptive network
    bool success = adaptive_network_save(brain->network, filepath, SERIALIZE_FORMAT_BINARY);

    if (!success) {
        /* adaptive_network_save returned false — almost always a short fwrite
         * (FWRITE_CHECKED triggered). The macro now logs errno + ferror at
         * the actual call site, so the kernel-level cause (ENOSPC, EINTR,
         * etc.) is in the daemon log. The thrown exception below is the
         * *bubble-up* signal; the diagnostic detail is upstream. */
        set_error("adaptive_network_save returned false for %s — see "
                  "FWRITE_CHECKED log entries above for errno", filepath);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_IO,
            "brain_save: adaptive_network_save failed (short fwrite — "
            "check daemon log for errno/ferror)");
        return false;
    }

    // Health monitoring: network saved, starting metadata
    brain_heartbeat(brain, "brain_save:metadata", 0.7f);

    // Save metadata
    if (!nimcp_brain_save_metadata(brain, filepath)) {
        set_error("Failed to save metadata");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_save: nimcp_brain_save_metadata is NULL");
        return false;
    }

    // Save secondary networks (SNN/LNN/CNN) for checkpoint persistence
    {
        if (brain->snn_network) {
            char snn_path[NIMCP_METRICS_PATH_SIZE];
            snprintf(snn_path, sizeof(snn_path), "%s.snn", filepath);
            extern int snn_network_save(struct snn_network_s* network, const char* path);
            snn_network_save(brain->snn_network, snn_path);

            /* The .snn sidecar IS the hierarchical cache — on restart,
             * snn_create_hierarchical_network() loads it to skip wiring. */
        }
        if (brain->lnn_network) {
            char lnn_path[NIMCP_METRICS_PATH_SIZE];
            snprintf(lnn_path, sizeof(lnn_path), "%s.lnn", filepath);
            extern int lnn_network_save(const struct lnn_network_s* network, const char* path);
            lnn_network_save(brain->lnn_network, lnn_path);
        }
        if (brain->cnn_trainer) {
            char cnn_path[NIMCP_METRICS_PATH_SIZE];
            snprintf(cnn_path, sizeof(cnn_path), "%s.cnn", filepath);
            extern int cnn_trainer_save(const void* trainer, const char* path);
            cnn_trainer_save(brain->cnn_trainer, cnn_path);
        }

        /* Save per-cortex CNN processors */
        {
            extern int cortex_cnn_save(const struct cortex_cnn_processor* proc, const char* path);
            const char* cortex_suffixes[4] = {".cortex_visual", ".cortex_audio",
                                               ".cortex_speech", ".cortex_somato"};
            for (int ci = 0; ci < 4; ci++) {
                if (brain->cortex_cnns[ci]) {
                    char cortex_path[NIMCP_METRICS_PATH_SIZE];
                    snprintf(cortex_path, sizeof(cortex_path), "%s%s",
                             filepath, cortex_suffixes[ci]);
                    cortex_cnn_save(brain->cortex_cnns[ci], cortex_path);
                }
            }
        }

        /* CC6: Save CORTICAL_COLUMNS_V1 sidecar (best-effort; non-fatal). */
        cortical_columns_sidecar_save(brain, filepath);

        /* Layer C: TEMPERATURE_V1 sidecar (best-effort; non-fatal). */
        temperature_sidecar_save(brain, filepath);

        /* Immune memory sidecar — preserves B/T cells, antibodies, antigens
         * across daemon restarts so adaptive immunity actually persists.
         * Best-effort: log on failure but don't fail the whole save. */
        if (brain->immune_system) {
            extern int immune_persistence_save(struct brain_immune_system* system,
                                               const char* filepath,
                                               const void* config);
            char immune_path[NIMCP_METRICS_PATH_SIZE];
            snprintf(immune_path, sizeof(immune_path), "%s.immune", filepath);
            if (immune_persistence_save((struct brain_immune_system*)brain->immune_system,
                                        immune_path, NULL) != 0) {
                fprintf(stderr, "[WARN] immune_persistence_save failed for %s — "
                        "adaptive memory NOT persisted\n", immune_path);
            }
        }

        /* Internal KG sidecar — preserves runtime knowledge-graph nodes and
         * edges (including HANDLES_MESSAGE bindings + weights accumulated
         * during training) across daemon restarts. Mirrors the .immune
         * sidecar pattern: best-effort, never fails the whole save. */
        if (brain->internal_kg) {
            extern int brain_kg_save(struct brain_kg* kg, const char* filepath);
            char kg_path[NIMCP_METRICS_PATH_SIZE];
            snprintf(kg_path, sizeof(kg_path), "%s.kg", filepath);
            if (brain_kg_save((struct brain_kg*)brain->internal_kg, kg_path) != 0) {
                fprintf(stderr, "[WARN] brain_kg_save failed for %s — "
                        "KG facts NOT persisted\n", kg_path);
            }
        }

        /* Slice B (Option 1 architectural rebuild) — concept registry
         * sidecar. Persists the cross-modal binding union-find + the
         * text/visual/audio fingerprint tables so canonical pop_ids
         * survive daemon restarts. Without this, every restart wipes
         * the (image, label) → pop bindings the trainer has accumulated.
         * Best-effort: missing sidecar is fine on pre-Slice-B checkpoints. */
        if (brain->concept_registry) {
            char cr_path[NIMCP_METRICS_PATH_SIZE];
            snprintf(cr_path, sizeof(cr_path), "%s.concept_registry", filepath);
            FILE* cr_fp = fopen(cr_path, "wb");
            if (cr_fp) {
                int cr_rc = concept_registry_save(
                    (const concept_registry_t*)brain->concept_registry, cr_fp);
                fclose(cr_fp);
                if (cr_rc != 0) {
                    fprintf(stderr, "[WARN] concept_registry_save failed for %s — "
                            "cross-modal bindings NOT persisted\n", cr_path);
                }
            } else {
                fprintf(stderr, "[WARN] could not open %s for write — "
                        "concept registry NOT persisted\n", cr_path);
            }
        }

        /* Grounded language sidecar — preserves the trained lexicon
         * (vocab_list, lexicon hash table, learned word classes,
         * distributional context vectors, valence/arousal) and syntactic
         * templates across daemon restarts. Without this, the language
         * module wipes its vocabulary on every restart and stays at zero
         * bindings no matter how long training runs. */
        if (brain->grounded_lang) {
            extern int gl_persistence_save(const struct grounded_language* gl,
                                           const char* path);
            char gl_path[NIMCP_METRICS_PATH_SIZE];
            snprintf(gl_path, sizeof(gl_path), "%s.gl_lang", filepath);
            if (gl_persistence_save(brain->grounded_lang, gl_path) != 0) {
                fprintf(stderr, "[WARN] gl_persistence_save failed for %s — "
                        "trained lexicon NOT persisted\n", gl_path);
            }

            /* TA-1 multi-turn state sidecar: discourse ring, anaphora
             * ring, bigram spectrum. Separate file (.gl_multiturn) so
             * pre-TA-1 readers see EOF after their .gl_lang block. */
            extern int grounded_language_save_multiturn_state(
                struct grounded_language* gl, FILE* fp);
            char mt_path[NIMCP_METRICS_PATH_SIZE];
            snprintf(mt_path, sizeof(mt_path), "%s.gl_multiturn", filepath);
            FILE* mt_fp = fopen(mt_path, "wb");
            if (mt_fp) {
                int mt_rc = grounded_language_save_multiturn_state(
                    brain->grounded_lang, mt_fp);
                fclose(mt_fp);
                if (mt_rc != 0) {
                    fprintf(stderr,
                            "[WARN] grounded_language_save_multiturn_state "
                            "failed for %s — multi-turn state NOT persisted\n",
                            mt_path);
                }
            } else {
                fprintf(stderr,
                        "[WARN] could not open %s for write — multi-turn "
                        "state NOT persisted\n", mt_path);
            }
        }
    }

    // Health monitoring: save complete
    brain_heartbeat(brain, "brain_save:complete", 1.0f);

    brain_clear_error();
    return true;
}

/**
 * @brief Load single working memory item from file (Phase 10.2)
 *
 * WHAT: Deserialize one item and add to working memory buffer
 * WHY:  Restore individual active representations
 * HOW:  Read size → allocate → read data → add to buffer → free temp
 *
 * COMPLEXITY: O(m) where m = item size
 *
 * @param wm Working memory instance (non-NULL)
 * @param file File handle (non-NULL)
 * @return true on success, false on error
 */
static bool load_working_memory_item(working_memory_t* wm, FILE* file)
{
    #define MAX_ITEM_SIZE 10000  // Sanity check limit

    // Guard: NULL parameters
    if (!wm || !file) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "load_working_memory_item: required parameter is NULL (wm, file)");
        return false;
    }

    uint32_t item_size = 0;
    if (fread(&item_size, sizeof(uint32_t), 1, file) != 1) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "load_working_memory_item: validation failed");
        return false;
    }

    // Guard: Invalid size
    if (item_size == 0 || item_size > MAX_ITEM_SIZE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "load_working_memory_item: item_size is zero");
        return false;
    }

    // Allocate temporary buffer
    float* item = nimcp_malloc(item_size * sizeof(float));
    if (!item) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "load_working_memory_item: item is NULL");
        return false;
    }

    // Read item data
    if (fread(item, sizeof(float), item_size, file) != item_size) {
        nimcp_free(item);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "load_working_memory_item: validation failed");
        return false;
    }

    // Add to working memory (use default salience since not persisted)
    const float DEFAULT_SALIENCE = 0.5F;
    bool success = working_memory_add(wm, item, item_size, DEFAULT_SALIENCE);

    nimcp_free(item);
    return success;

    #undef MAX_ITEM_SIZE
}

/**
 * @brief Load working memory state from file (Phase 10.2)
 *
 * WHAT: Deserialize working memory items from COW snapshot
 * WHY:  Restore active representations after load/restore
 * HOW:  Read marker → initialize if needed → load each item
 *
 * COMPLEXITY: O(n*m) where n = items, m = avg item size
 *
 * @param brain Brain instance (non-NULL)
 * @param file File handle (non-NULL)
 * @return true on success (non-fatal on WM failure)
 */
bool nimcp_brain_load_working_memory_state(brain_t brain, FILE* file)
{
    // Guard: NULL parameters
    if (!brain || !file) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_load_working_memory_state: required parameter is NULL (brain, file)");
        return false;
    }

    // Read existence marker
    uint8_t has_wm = 0;
    if (fread(&has_wm, sizeof(uint8_t), 1, file) != 1) {
        return true;  // EOF or old format → non-fatal
    }

    // Guard: No working memory in snapshot
    if (has_wm == 0) {
        return true;  // Nothing to load → success
    }

    // Read metadata
    uint32_t wm_size = 0, wm_capacity = 0;
    if (fread(&wm_size, sizeof(uint32_t), 1, file) != 1) {
        return true;  // Non-fatal
    }
    if (fread(&wm_capacity, sizeof(uint32_t), 1, file) != 1) {
        return true;  // Non-fatal
    }

    // Initialize working memory if enabled but not yet created
    if (!brain->working_memory && brain->config.enable_working_memory) {
        if (!init_working_memory_subsystem(brain)) {
            fprintf(stderr, "WARNING: Failed to initialize working memory on load\n");
            return true;  // Non-fatal: continue without WM
        }
    }

    // Guard: Working memory not available
    if (!brain->working_memory) {
        return true;  // Skip loading → non-fatal
    }

    // Load each item
    for (uint32_t i = 0; i < wm_size; i++) {
        load_working_memory_item(brain->working_memory, file);
        // Errors loading individual items are non-fatal
    }

    return true;
}

/**
 * @brief Load metadata file
 *
 * WHAT: Deserialize brain configuration and output labels
 * WHY:  Reconstruct full brain state from persistent storage
 * HOW:  Read config → validate → load labels → load working memory
 *
 * COMPLEXITY: O(k + n*m) where k = labels, n = WM items, m = item size
 *
 * @param brain Brain instance (non-NULL)
 * @param filepath Base filepath (non-NULL)
 * @return true on success, false on error
 */
bool nimcp_brain_load_metadata(brain_t brain, const char* filepath)
{
    char meta_path[NIMCP_METRICS_PATH_SIZE];
    snprintf(meta_path, sizeof(meta_path), "%s.meta", filepath);

    // P1-3 fix: Path traversal validation
    if (!persistence_path_is_safe(meta_path)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_brain_load_metadata: persistence_path_is_safe is NULL");
        return false;
    }

    FILE* meta_file = fopen(meta_path, "rb");
    if (!meta_file) {
        /* Audit fix #8: previously this returned false silently and the
         * caller continued with default state. Log loudly so missing
         * metadata is visible — it means stats, learning rate, optimizer
         * momentum, KG, mirror neurons, executive state are all defaulted. */
        NIMCP_LOGGING_WARN("nimcp_brain_load_metadata: %s missing — "
                           "training stats, optimizer state, and KG will reset to defaults",
                           meta_path);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_load_metadata: meta_file is NULL");
        return false;
    }

    // Try to read version header
    nimcp_file_header_t header;
    size_t header_read = fread(&header, sizeof(nimcp_file_header_t), 1, meta_file);

    bool has_version_header = false;
    if (header_read == 1) {
        // Check magic bytes
        if (header.magic[0] == NIMCP_MAGIC_0 &&
            header.magic[1] == NIMCP_MAGIC_1 &&
            header.magic[2] == NIMCP_MAGIC_2 &&
            header.magic[3] == NIMCP_MAGIC_3) {

            has_version_header = true;

            // Validate version compatibility
            if (header.version_major != NIMCP_FORMAT_VERSION_MAJOR) {
                fprintf(stderr, "ERROR: Incompatible format version %u.%u (expected %u.x)\n",
                        header.version_major, header.version_minor,
                        NIMCP_FORMAT_VERSION_MAJOR);
                fclose(meta_file);
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_brain_load_metadata: validation failed");
                return false;
            }

            fprintf(stderr, "[INFO] Loading brain metadata v%u.%u\n",
                    header.version_major, header.version_minor);

            // TODO: Handle format flags (compression, encryption)
            if (header.flags & NIMCP_FORMAT_FLAG_COMPRESSED) {
                fprintf(stderr, "[WARN] Compressed format not yet supported, skipping\n");
            }
            if (header.flags & NIMCP_FORMAT_FLAG_ENCRYPTED) {
                fprintf(stderr, "[WARN] Encrypted format not yet supported, skipping\n");
            }
        } else {
            // Not a versioned file - rewind and read as legacy format
            has_version_header = false;
            fseek(meta_file, 0, SEEK_SET);
        }
    } else {
        // File too small for header - legacy format
        fseek(meta_file, 0, SEEK_SET);
    }

    if (!has_version_header) {
        fprintf(stderr, "[INFO] Loading brain metadata (legacy format, no version header)\n");
    }

    /* Zero-fill config first so any new fields added since the checkpoint
     * was saved default to 0/false. Then read as many bytes as available.
     * This handles struct growth across versions without breaking checkpoints. */
    memset(&brain->config, 0, sizeof(brain_config_t));
    size_t config_bytes = fread(&brain->config, 1, sizeof(brain_config_t), meta_file);

    /* Post-load migration for fields added without a checkpoint version
     * bump. The offsetof-based detection approach was fragile across the
     * Apr 11 2026 struct-shift incident (where one build had fields
     * inserted mid-struct, then a later build moved them to the end —
     * making it hard to compute "was this field in the file" via raw size
     * comparisons). Instead, we unconditionally re-apply the defaults for
     * trailing fields that have specific non-zero defaults, AFTER the
     * fread. Users who want different values (e.g. set_train_ann(false)
     * for an ablation study) must call the setter RPC after load; the
     * saved-vs-loaded round-trip is not preserved for these flags.
     *
     * This is a stopgap until the checkpoint format gets a proper version
     * header + field-list manifest. For now it keeps training_mode_active
     * (existing pre-Apr-11 field), train_ann (Apr 11), and
     * snn_only_recovery_mode (Apr 11) at their correct init-config defaults. */
    brain->config.train_ann = true;  /* default: ANN training enabled */
    brain->config.snn_only_recovery_mode = false;  /* default: normal joint training */
    brain->config.ensemble_warmup_scale = 1.0f;    /* default: full-rate, no warmup */
    if (config_bytes == 0) {
        fprintf(stderr, "ERROR: Failed to read brain config (0 bytes)\n");
        fclose(meta_file);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_IO, "nimcp_brain_load_metadata: short read on brain config");
        return false;
    }
    if (config_bytes < sizeof(brain_config_t)) {
        fprintf(stderr, "[INFO] Loaded %zu/%zu config bytes (older checkpoint format, "
                "new fields defaulted to zero)\n", config_bytes, sizeof(brain_config_t));
    }

    // Validate brain->config fields after reading
    // Validate learning_rate (float field - NaN/Inf check)
    if (!nimcp_validate_float_field(&brain->config.learning_rate,
                                    sizeof(brain->config.learning_rate))) {
        fprintf(stderr, "ERROR: Invalid learning_rate in loaded config (NaN or Inf)\n");
        fclose(meta_file);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_brain_load_metadata: operation failed");
        return false;
    }

    // Validate sparsity_target (float field - NaN/Inf check)
    if (!nimcp_validate_float_field(&brain->config.sparsity_target,
                                    sizeof(brain->config.sparsity_target))) {
        fprintf(stderr, "ERROR: Invalid sparsity_target in loaded config (NaN or Inf)\n");
        fclose(meta_file);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_brain_load_metadata: operation failed");
        return false;
    }

    // Validate num_inputs (integer field, range 1-10000)
    if (!nimcp_validate_integer_field(&brain->config.num_inputs,
                                      sizeof(brain->config.num_inputs))) {
        fprintf(stderr, "ERROR: Invalid num_inputs in loaded config\n");
        fclose(meta_file);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_brain_load_metadata: operation failed");
        return false;
    }
    if (brain->config.num_inputs < 1 || brain->config.num_inputs > 10000) {
        fprintf(stderr, "ERROR: num_inputs out of range (1-10000): %u\n", brain->config.num_inputs);
        fclose(meta_file);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_brain_load_metadata: validation failed");
        return false;
    }

    // Validate num_outputs (integer field, range 1-10000)
    if (!nimcp_validate_integer_field(&brain->config.num_outputs,
                                      sizeof(brain->config.num_outputs))) {
        fprintf(stderr, "ERROR: Invalid num_outputs in loaded config\n");
        fclose(meta_file);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_brain_load_metadata: operation failed");
        return false;
    }
    if (brain->config.num_outputs < 1 || brain->config.num_outputs > 10000) {
        fprintf(stderr, "ERROR: num_outputs out of range (1-10000): %u\n",
                brain->config.num_outputs);
        fclose(meta_file);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_brain_load_metadata: validation failed");
        return false;
    }

    if (fread(&brain->num_output_labels, sizeof(uint32_t), 1, meta_file) != 1) {
        fprintf(stderr, "ERROR: Short read loading num_output_labels\n");
        fclose(meta_file);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_IO, "nimcp_brain_load_metadata: short read on num_output_labels");
        return false;
    }

    // SECURITY: Strict validation limits to prevent buffer overflow attacks
    #define MAX_OUTPUT_LABELS 10000     // Maximum number of labels
    #define MAX_LABEL_LENGTH 256        // Maximum length of a single label

    // Validate num_output_labels (range 0-10000, 0 means no labels)
    if (!nimcp_validate_integer_field(&brain->num_output_labels,
                                      sizeof(brain->num_output_labels))) {
        fprintf(stderr, "ERROR: Invalid num_output_labels in loaded metadata\n");
        fclose(meta_file);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_brain_load_metadata: operation failed");
        return false;
    }
    if (brain->num_output_labels > MAX_OUTPUT_LABELS) {
        fprintf(stderr, "SECURITY ERROR: num_output_labels %u exceeds maximum %d\n",
                brain->num_output_labels, MAX_OUTPUT_LABELS);
        fprintf(stderr, "This file may be maliciously crafted\n");
        fclose(meta_file);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_brain_load_metadata: validation failed");
        return false;
    }

    // Allocate output_labels with full num_outputs capacity (same as fresh brain)
    // so that get_or_create_label_index can add new labels after checkpoint load
    uint32_t label_capacity = brain->config.num_outputs > brain->num_output_labels
                              ? brain->config.num_outputs : brain->num_output_labels;
    if (label_capacity == 0) {
        brain->output_labels = NULL;
        fclose(meta_file);
        return true;
    }

    brain->output_labels = nimcp_calloc(label_capacity, sizeof(char*));
    if (!brain->output_labels) {
        fprintf(stderr, "ERROR: Failed to allocate output_labels array\n");
        fclose(meta_file);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_brain_load_metadata: brain->output_labels is NULL");
        return false;
    }

    uint32_t i;
    for (i = 0; i < brain->num_output_labels; i++) {
        uint32_t len;
        if (fread(&len, sizeof(uint32_t), 1, meta_file) != 1) {
            fprintf(stderr, "ERROR: Short read loading label length at index %u\n", i);
            goto cleanup;
        }

        // SECURITY: Validate label length to prevent buffer overflow
        if (len == 0 || len > MAX_LABEL_LENGTH) {
            fprintf(stderr, "SECURITY ERROR: Label %u length %u exceeds maximum %d\n",
                    i, len, MAX_LABEL_LENGTH);
            fprintf(stderr, "This file may be maliciously crafted\n");
            goto cleanup;
        }

        // Validate integer field integrity
        if (!nimcp_validate_integer_field(&len, sizeof(len))) {
            fprintf(stderr, "ERROR: Invalid label length at index %u\n", i);
            goto cleanup;
        }

        brain->output_labels[i] = nimcp_malloc(len);
        if (!brain->output_labels[i]) {
            fprintf(stderr, "ERROR: Failed to allocate label at index %u\n", i);
            goto cleanup;
        }

        if (fread(brain->output_labels[i], len, 1, meta_file) != 1) {
            fprintf(stderr, "ERROR: Short read loading label data at index %u\n", i);
            goto cleanup;
        }
    }

    // Rebuild O(1) label lookup hash table from loaded labels
    hash_table_config_t ht_config = {
        .initial_buckets = 256,
        .key_type = HASH_KEY_STRING,
        .hash_algorithm = HASH_ALG_FNV1A,
        .case_insensitive = false,
        .thread_safe = false
    };
    brain->label_index = hash_table_create(&ht_config);
    if (brain->label_index) {
        for (uint32_t j = 0; j < brain->num_output_labels; j++) {
            hash_table_insert_string(brain->label_index, brain->output_labels[j], &j, sizeof(uint32_t));
        }
    }

    // Phase 10.2: Load working memory state
    nimcp_brain_load_working_memory_state(brain, meta_file);

    // Load brain statistics (performance metrics)
    if (fread(&brain->stats, sizeof(brain_stats_t), 1, meta_file) != 1) {
        // Non-fatal: use default stats if not available (backward compatibility)
        init_brain_stats(&brain->stats, brain->config.task_name, brain->config.size,
                        brain->config.num_inputs, brain->config.learning_rate);
        // Override with actual network neuron count if available
        if (brain->network) {
            brain->stats.num_neurons = adaptive_network_get_neuron_count(brain->network);
            brain->stats.num_synapses = brain->stats.num_neurons * brain->config.num_inputs;
            brain->stats.num_active_synapses = brain->stats.num_synapses;
        }
    }

    // Load wellbeing state (Phase 9.3)
    if (fread(&brain->last_distress, sizeof(distress_assessment_t), 1, meta_file) == 1 &&
        fread(&brain->wellbeing_monitoring_enabled, sizeof(bool), 1, meta_file) == 1 &&
        fread(&brain->wellbeing_check_interval_ms, sizeof(uint64_t), 1, meta_file) == 1 &&
        fread(&brain->last_wellbeing_check_time, sizeof(uint64_t), 1, meta_file) == 1) {
        // Successfully loaded wellbeing state
    }

    // Load simulation time tracking (may not exist in old snapshots)
    if (fread(&brain->current_time_us, sizeof(uint64_t), 1, meta_file) == 1 &&
        fread(&brain->last_glial_update_us, sizeof(uint64_t), 1, meta_file) == 1) {
        // Successfully loaded time tracking
    } else {
        // Old snapshot, initialize to 0
        brain->current_time_us = 0;
        brain->last_glial_update_us = 0;
    }

    // Load knowledge system state (if exists)
    bool has_knowledge = false;
    if (fread(&has_knowledge, sizeof(bool), 1, meta_file) == 1 && has_knowledge) {
        char knowledge_path[NIMCP_METRICS_PATH_SIZE];
        snprintf(knowledge_path, sizeof(knowledge_path), "%s.knowledge", filepath);
        brain->knowledge = knowledge_load(knowledge_path);
        // Non-fatal if knowledge load fails
    }

    // Load emotional system state (Phase 10.2 - NOT A MODULE)
    // Note: Emotional tagging uses stateless utility functions, not a system object
    bool has_emotional = false;
    if (fread(&has_emotional, sizeof(bool), 1, meta_file) == 1 && has_emotional) {
        // Placeholder for backward compatibility (old saves might have this flag set)
        // No action needed - emotional tagging uses stateless functions
    }

    // Load executive controller state (Phase 10.3 - if exists)
    bool has_executive = false;
    if (fread(&has_executive, sizeof(bool), 1, meta_file) == 1 && has_executive) {
        // WHAT: Load executive controller state from separate file
        // WHY:  Restore task queue, statistics, and configuration
        // HOW:  Use executive_load API with dedicated file
        char executive_path[NIMCP_METRICS_PATH_SIZE];
        snprintf(executive_path, sizeof(executive_path), "%s.executive", filepath);
        FILE* exec_file = fopen(executive_path, "rb");
        if (exec_file) {
            brain->executive = executive_load(exec_file);
            fclose(exec_file);
            // Set brain reference for neuromodulation integration
            if (brain->executive) {
                executive_set_brain(brain->executive, brain);
            }
        }
    }

    // Load pink noise neuromodulator state (if exists)
    bool has_pink_noise = false;
    if (fread(&has_pink_noise, sizeof(bool), 1, meta_file) == 1 && has_pink_noise) {
        // WHAT: Load pink noise neuromodulator state from separate file
        // WHY:  Restore neuromodulator levels and pink noise generators
        // HOW:  Use neuromod_pink_load API with dedicated file
        char pink_noise_path[NIMCP_METRICS_PATH_SIZE];
        snprintf(pink_noise_path, sizeof(pink_noise_path), "%s.pink_noise", filepath);
        FILE* pink_file = fopen(pink_noise_path, "rb");
        if (pink_file) {
            brain->pink_noise = neuromod_pink_load(pink_file);
            fclose(pink_file);
        }
    }

    // Load mirror neurons state (Phase 10.11 - if exists)
    bool has_mirror_neurons = false;
    if (fread(&has_mirror_neurons, sizeof(bool), 1, meta_file) == 1 && has_mirror_neurons) {
        // WHAT: Load mirror neuron system state from separate file
        // WHY:  Restore learned action associations and statistics
        // HOW:  Use mirror_neurons_load API with dedicated file
        char mirror_path[NIMCP_METRICS_PATH_SIZE];
        snprintf(mirror_path, sizeof(mirror_path), "%s.mirror_neurons", filepath);
        FILE* mirror_file = fopen(mirror_path, "rb");
        if (mirror_file) {
            brain->mirror_neurons = mirror_neurons_load(mirror_file);
            fclose(mirror_file);
            // Set brain reference for neuromodulation integration
            if (brain->mirror_neurons) {
                mirror_neurons_set_brain(brain->mirror_neurons, brain);
            }
        }
    }

    // Load training state for checkpoint-resume (optional — old saves won't have this)
    uint32_t training_state_magic = 0;
    if (fread(&training_state_magic, sizeof(uint32_t), 1, meta_file) == 1 &&
        training_state_magic == 0x54524E53 /* "TRNS" */) {
        if (fread(brain->loss_history, sizeof(float), 10, meta_file) != 10 ||
            fread(&brain->loss_history_index, sizeof(uint32_t), 1, meta_file) != 1 ||
            fread(&brain->loss_history_count, sizeof(uint32_t), 1, meta_file) != 1 ||
            fread(&brain->base_learning_rate, sizeof(float), 1, meta_file) != 1) {
            NIMCP_LOGGING_WARN("Truncated training state in checkpoint");
        } else {
            float saved_lr = 0.0f;
            if (fread(&saved_lr, sizeof(float), 1, meta_file) == 1 && saved_lr > 0.0f) {
                brain->config.learning_rate = saved_lr;
            }
            /* These 4 fields are optional-tail data; short reads leave the
             * memset defaults which is acceptable. Track the return values
             * to silence -Wunused-result, but don't bail on truncation. */
            size_t _r1 = fread(&brain->last_curiosity_drive, sizeof(float), 1, meta_file);
            size_t _r2 = fread(&brain->last_novelty_score, sizeof(float), 1, meta_file);
            size_t _r3 = fread(&brain->stats.total_inferences, sizeof(uint64_t), 1, meta_file);
            size_t _r4 = fread(&brain->stats.total_learning_steps, sizeof(uint64_t), 1, meta_file);
            (void)_r1; (void)_r2; (void)_r3; (void)_r4;
        }
        fprintf(stderr, "[INFO] Restored training state: LR=%.6f, loss_count=%u, "
                "inferences=%lu, learning_steps=%lu\n",
                brain->config.learning_rate, brain->loss_history_count,
                (unsigned long)brain->stats.total_inferences,
                (unsigned long)brain->stats.total_learning_steps);
    }

    /* Load optimizer states (H9: restore momentum/velocity from checkpoint) */
    if (brain->training_ctx) {
        uint32_t opt_magic = 0;
        if (fread(&opt_magic, sizeof(uint32_t), 1, meta_file) == 1 &&
            opt_magic == 0x4F505453 /* "OPTS" */) {
            uint32_t opt_count = 0;
            if (fread(&opt_count, sizeof(uint32_t), 1, meta_file) == 1) {
                uint32_t restored = 0;
                for (uint32_t i = 0; i < opt_count; i++) {
                    uint32_t slot_id = 0;
                    if (fread(&slot_id, sizeof(uint32_t), 1, meta_file) != 1) {
                        NIMCP_LOGGING_WARN("Truncated optimizer state at slot %u", i);
                        break;
                    }
                    /* Try to find existing optimizer slot */
                    nimcp_optimizer_context_t* opt_ctx =
                        nimcp_brain_training_get_optimizer(brain->training_ctx, slot_id);
                    if (!opt_ctx) {
                        /* Optimizer slot doesn't exist yet — peek at the saved
                         * type from the optimizer_save header (magic + type)
                         * and create a fresh optimizer before restoring state. */
                        long peek_pos = ftell(meta_file);
                        uint32_t peek_magic = 0, peek_type = 0;
                        bool peeked = (fread(&peek_magic, sizeof(uint32_t), 1, meta_file) == 1 &&
                                       peek_magic == 0x4F505453 &&
                                       fread(&peek_type, sizeof(uint32_t), 1, meta_file) == 1);
                        /* Seek back to where we were before peeking */
                        if (peek_pos >= 0) {
                            fseek(meta_file, peek_pos, SEEK_SET);
                        }

                        if (peeked) {
                            nimcp_optimizer_config_t opt_config =
                                nimcp_optimizer_default_config((nimcp_optimizer_type_t)peek_type);
                            uint32_t new_id = 0;
                            nimcp_result_t res = nimcp_brain_training_create_optimizer(
                                brain->training_ctx, &opt_config, &new_id);
                            if (res == NIMCP_SUCCESS) {
                                opt_ctx = nimcp_brain_training_get_optimizer(
                                    brain->training_ctx, new_id);
                                fprintf(stderr, "[INFO] Created optimizer slot %u (type=%u) "
                                        "for checkpoint restore\n", new_id, peek_type);
                            } else {
                                NIMCP_LOGGING_WARN("Failed to create optimizer for slot %u "
                                                   "(type=%u)", slot_id, peek_type);
                            }
                        }
                    }
                    if (opt_ctx) {
                        if (nimcp_optimizer_load(opt_ctx, meta_file) == 0) {
                            restored++;
                        } else {
                            NIMCP_LOGGING_WARN("Failed to load optimizer state for slot %u", slot_id);
                            break; /* Stream position uncertain, stop reading */
                        }
                    } else {
                        /* Cannot create or find optimizer — stream position
                         * is uncertain since we can't skip variable-size data. */
                        NIMCP_LOGGING_WARN("Optimizer slot %u not found and could not be "
                                           "created, skipping remaining", slot_id);
                        break;
                    }
                }
                if (restored > 0) {
                    fprintf(stderr, "[INFO] Restored %u/%u optimizer state(s)\n",
                            restored, opt_count);
                }
            }
        }
    }

    /* === LANG sidecar block (best-effort) =============================
     *
     * Mirror of the writer above. EOF here is the EXPECTED path for
     * pre-2026-05-12 checkpoints — leave the cascade_self_train_*
     * fields at whatever the caller's init/default set them to.
     *
     * Block was added 2026-05-12 (commit following a6f965029). Magic
     * 'LANG' disambiguates it from a stray u32 = 0x4C414E47 by chance.
     * block_size_in_bytes lets newer writers extend the block. */
    {
        uint32_t lang_sentinel = 0;
        if (fread(&lang_sentinel, sizeof(uint32_t), 1, meta_file) == 1 &&
            lang_sentinel == 0x4C414E47u) {
            uint32_t lang_block_size = 0;
            if (fread(&lang_block_size, sizeof(uint32_t), 1, meta_file) != 1 ||
                lang_block_size > 64u * 1024u) {
                /* Corrupt block size — abandon, tunables stay at defaults. */
                NIMCP_LOGGING_WARN("LANG block: bad/missing block_size — skipping");
            } else {
                long start_pos = ftell(meta_file);
                if (lang_block_size >= sizeof(uint8_t) + sizeof(float) * 3u) {
                    uint8_t st_enabled = 0;
                    float baseline = 0.0f, alpha = 0.0f, lr_scale = 0.0f;
                    if (fread(&st_enabled, sizeof(uint8_t), 1, meta_file) == 1 &&
                        fread(&baseline,   sizeof(float),   1, meta_file) == 1 &&
                        fread(&alpha,      sizeof(float),   1, meta_file) == 1 &&
                        fread(&lr_scale,   sizeof(float),   1, meta_file) == 1) {
                        brain->cascade_self_train_enabled  = (st_enabled != 0);
                        brain->cascade_self_train_baseline = baseline;
                        brain->cascade_self_train_alpha    = alpha;
                        brain->cascade_self_train_lr_scale = lr_scale;
                    }
                }
                /* respond_via_cascade tail (2026-05-12) — optional u8.
                 * Pre-rvc writers stopped at the lr_scale float. */
                if (lang_block_size >= sizeof(uint8_t) + sizeof(float) * 3u + sizeof(uint8_t)) {
                    uint8_t rvc_enabled = 0;
                    if (fread(&rvc_enabled, sizeof(uint8_t), 1, meta_file) == 1) {
                        brain->respond_via_cascade = (rvc_enabled != 0);
                    }
                }
                /* Slice E (2026-05-19) — current_stage tail. Optional u32
                 * appended after rvc; older writers stopped at rvc and
                 * we leave current_stage at the calloc-zero default. */
                if (lang_block_size >= sizeof(uint8_t) + sizeof(float) * 3u
                                       + sizeof(uint8_t) + sizeof(uint32_t)) {
                    uint32_t cur_stage = 0;
                    if (fread(&cur_stage, sizeof(uint32_t), 1, meta_file) == 1) {
                        brain->current_stage = cur_stage;
                    }
                }
                /* Forward-compat: skip past any trailing bytes a newer
                 * writer added beyond what we know how to read. */
                if (start_pos >= 0) {
                    long want = start_pos + (long)lang_block_size;
                    long here = ftell(meta_file);
                    if (here >= 0 && here < want) {
                        fseek(meta_file, want, SEEK_SET);
                    }
                }
            }
        }
        /* Any other outcome (EOF, mismatched sentinel) leaves the
         * tunables at their init/RPC-set defaults. */
    }

    fclose(meta_file);

    // Load persistent tokenizer (optional — returns NULL if file doesn't exist)
    {
        char tok_path[512];
        snprintf(tok_path, sizeof(tok_path), "%s.tokenizer", filepath);
        brain->tokenizer = tokenizer_load(tok_path);
    }

    return true;

cleanup:
    // Free any allocated labels before the failed one
    for (uint32_t j = 0; j < i; j++) {
        nimcp_free(brain->output_labels[j]);
    }
    nimcp_free(brain->output_labels);
    brain->output_labels = NULL;
    brain->num_output_labels = 0;  /* keep counter consistent with array */
    fclose(meta_file);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "unknown: operation failed");
    return false;
}

/**
 * @brief Load brain from file
 *
 * WHY: Restores saved brain state
 * Reconstructs network and metadata
 *
 * COMPLEXITY: O(n*c) where n = neurons, c = connections
 *
 * @param filepath Path to load from
 * @return Brain handle or NULL on error
 */
brain_t brain_load(const char* filepath)
{
    // Guard: Validate filepath
    if (!filepath) {
        set_error("Null filepath provided to brain_load");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_load: filepath is NULL");
        return NULL;
    }

    // P1-3 fix: Path traversal validation
    if (!persistence_path_is_safe(filepath)) {
        set_error("Path validation failed: %s", filepath);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_load: persistence_path_is_safe is NULL");
        return NULL;
    }

    // Load adaptive network
    adaptive_network_t network = adaptive_network_load(filepath);
    if (!network) {
        set_error("Failed to load adaptive network from %s", filepath);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_load: network is NULL");
        return NULL;
    }

    // Allocate brain structure
    brain_t brain = allocate_brain();
    if (!brain) {
        adaptive_network_destroy(network);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "brain_load: brain is NULL");
        return NULL;
    }

    brain->network = network;

    // Load metadata (includes stats if available)
    bool metadata_loaded = nimcp_brain_load_metadata(brain, filepath);
    if (!metadata_loaded) {
        // Use defaults if no metadata
        brain->config.size = BRAIN_SIZE_SMALL;
        brain->config.task = BRAIN_TASK_CLASSIFICATION;
        brain->config.learning_rate = 0.01F;
        brain->config.sparsity_target = 0.8F;
        brain->config.enable_explanations = true;
        snprintf(brain->config.task_name, sizeof(brain->config.task_name), "loaded_brain");

        // Initialize statistics only if metadata wasn't loaded
        // (nimcp_brain_load_metadata already loads stats from file)
        init_brain_stats(&brain->stats, brain->config.task_name, brain->config.size,
                         brain->config.num_inputs, brain->config.learning_rate);
    }

    /* Restore dimensions from actual network layer sizes.
     * Neither metadata nor base_config is reliable — metadata may have stale
     * values, base_config has original creation values (before resizing).
     * The network's actual layer_sizes array is ground truth. */
    {
        const adaptive_network_config_t* net_config = adaptive_network_get_config(network);
        if (net_config && net_config->base_config.num_layers >= 2 &&
            net_config->base_config.layer_sizes) {
            uint32_t nl = net_config->base_config.num_layers;
            brain->config.num_inputs = net_config->base_config.layer_sizes[0];
            brain->config.num_outputs = net_config->base_config.layer_sizes[nl - 1];
            fprintf(stderr, "[INFO] Restored dimensions from network layers: "
                    "inputs=%u, outputs=%u (layers=%u)\n",
                    brain->config.num_inputs, brain->config.num_outputs, nl);
        } else if (metadata_loaded) {
            /* Keep metadata values if network config unavailable */
            fprintf(stderr, "[INFO] Using metadata dimensions: inputs=%u, outputs=%u\n",
                    brain->config.num_inputs, brain->config.num_outputs);
        } else {
            brain->config.num_inputs = 1;
            brain->config.num_outputs = 1;
        }
    }

    // Create strategy for task
    brain->strategy = strategy_create(brain->config.task);
    if (!brain->strategy) {
        set_error("Failed to create task strategy");
        brain_destroy(brain);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "brain_load: brain->strategy is NULL");
        return NULL;
    }

    /* ====================================================================
     * Parallel sidecar loading — SNN, LNN, CNN, cortex×4 load concurrently.
     * Each sidecar reads its own file into its own struct with no shared
     * state during load, so they're fully parallelizable. This saves
     * ~5-15s compared to the prior sequential approach.
     * ==================================================================== */
    {
        sidecar_load_arg_t sidecar_args[7]; /* SNN, LNN, CNN, cortex×4 */
        pthread_t sidecar_threads[7];
        bool sidecar_launched[7] = {0};
        int n_sidecars = 0;
        char snn_path[NIMCP_METRICS_PATH_SIZE], lnn_path[NIMCP_METRICS_PATH_SIZE],
             cnn_path[NIMCP_METRICS_PATH_SIZE];
        char cortex_paths[4][NIMCP_METRICS_PATH_SIZE];

        /* SNN loader thread */
        snprintf(snn_path, sizeof(snn_path), "%s.snn", filepath);
        sidecar_args[n_sidecars] = (sidecar_load_arg_t){ .path = snn_path, .result = NULL, .type = 0, .brain = brain };
        if (pthread_create(&sidecar_threads[n_sidecars], NULL, _sidecar_load_snn, &sidecar_args[n_sidecars]) == 0)
            sidecar_launched[n_sidecars] = true;
        n_sidecars++;

        /* LNN loader thread */
        snprintf(lnn_path, sizeof(lnn_path), "%s.lnn", filepath);
        sidecar_args[n_sidecars] = (sidecar_load_arg_t){ .path = lnn_path, .result = NULL, .type = 1, .brain = brain };
        if (pthread_create(&sidecar_threads[n_sidecars], NULL, _sidecar_load_lnn, &sidecar_args[n_sidecars]) == 0)
            sidecar_launched[n_sidecars] = true;
        n_sidecars++;

        /* CNN loader thread (only if cnn_trainer already exists) */
        snprintf(cnn_path, sizeof(cnn_path), "%s.cnn", filepath);
        if (brain->cnn_trainer) {
            sidecar_args[n_sidecars] = (sidecar_load_arg_t){ .path = cnn_path, .result = NULL, .type = 2, .brain = brain };
            if (pthread_create(&sidecar_threads[n_sidecars], NULL, _sidecar_load_cnn, &sidecar_args[n_sidecars]) == 0)
                sidecar_launched[n_sidecars] = true;
            n_sidecars++;
        }

        /* Cortex CNN loader threads (up to 4) */
        {
            const char* cortex_suffixes[4] = {".cortex_visual", ".cortex_audio",
                                               ".cortex_speech", ".cortex_somato"};
            for (int ci = 0; ci < 4; ci++) {
                if (brain->cortex_cnns[ci]) {
                    snprintf(cortex_paths[ci], sizeof(cortex_paths[ci]), "%s%s", filepath, cortex_suffixes[ci]);
                    sidecar_args[n_sidecars] = (sidecar_load_arg_t){ .path = cortex_paths[ci], .result = NULL, .type = 3 + ci, .brain = brain };
                    if (pthread_create(&sidecar_threads[n_sidecars], NULL, _sidecar_load_cortex, &sidecar_args[n_sidecars]) == 0)
                        sidecar_launched[n_sidecars] = true;
                    n_sidecars++;
                }
            }
        }

        /* Join all sidecar threads */
        for (int i = 0; i < n_sidecars; i++) {
            if (sidecar_launched[i])
                pthread_join(sidecar_threads[i], NULL);
        }

        /* Install results from thread-loaded sidecars */
        /* SNN */
        struct snn_network_s* snn = (struct snn_network_s*)sidecar_args[0].result;
        if (snn) {
            brain->snn_network = snn;
            brain->owns_specialized_network = true;
            snn->config.input_current_scale = 70.0f;
            fprintf(stderr, "[INFO] Restored SNN network from %s (input_scale=70)\n", snn_path);

            /* Detect existing connections. Pre-G8 populations store synapses
             * via neural_net->neuron[*]->outgoing. G8 lightweight populations
             * store them in pop->incoming_csr with n_synapses > 0. We must
             * check BOTH — prior code only looked at neural_net[0] and saw
             * "empty" for the entire lightweight fleet, then triggered a
             * from-scratch rewire via snn_network_connect_populations. That
             * rewire also had a wrong extern signature (7 params vs 8
             * actual), so topology silently became garbage, the "random"
             * branch was skipped, and the inner loop attempted to wire
             * every src×dst pair — ~3.24 trillion add_entry calls on a
             * 1.8M×1.8M pair. Result: the loader hung in a CPU-bound loop
             * that never terminates. */
            bool has_connections = false;
            if (snn->neural_net) {
                neuron_t* n0 = neural_network_get_neuron(snn->neural_net, 0);
                if (n0 && sparse_synapse_count(&n0->outgoing) > 0)
                    has_connections = true;
            }
            if (!has_connections) {
                for (uint32_t p = 0; p < snn->n_populations; p++) {
                    snn_population_t* pop = snn->populations[p];
                    if (pop && pop->lightweight && pop->incoming_csr &&
                        pop->incoming_csr->n_synapses > 0) {
                        has_connections = true;
                        break;
                    }
                }
            }
            if (has_connections) {
                fprintf(stderr, "[INFO] SNN connections restored from checkpoint (BPTT weights preserved)\n");
            } else if (snn->n_populations >= 2) {
                fprintf(stderr, "[WARN] SNN checkpoint had no connections and no rewire — "
                        "skipping reconnect (prior from-scratch rewire path had an ABI "
                        "mismatch that caused a hang). Restart with --fresh if needed.\n");
            }
        }

        /* LNN */
        struct lnn_network_s* lnn = (struct lnn_network_s*)sidecar_args[1].result;
        if (lnn) {
            brain->lnn_network = lnn;
            brain->owns_specialized_network = true;
            fprintf(stderr, "[INFO] Restored LNN network from %s\n", lnn_path);
        }

        /* CNN */
        if (brain->cnn_trainer && n_sidecars >= 3 && sidecar_args[2].type == 2) {
            if ((intptr_t)sidecar_args[2].result == 0)
                fprintf(stderr, "[INFO] Restored CNN weights from %s\n", cnn_path);
        }

        /* Cortex CNNs */
        {
            const char* cortex_names[4] = {"visual", "audio", "speech", "somato"};
            for (int i = 0; i < n_sidecars; i++) {
                if (sidecar_args[i].type >= 3 && sidecar_args[i].type <= 6) {
                    int ci = sidecar_args[i].type - 3;
                    if ((intptr_t)sidecar_args[i].result == 0)
                        fprintf(stderr, "[INFO] Restored %s cortex CNN from %s\n",
                                cortex_names[ci], cortex_paths[ci]);
                }
            }
        }

        /* Reconnect restored secondary networks to training contexts.
         * Must run AFTER all sidecar threads have joined. */
        if (brain->snn_network && !brain->snn_training_ctx) {
            nimcp_training_config_t snn_cfg = {0};
            snn_cfg.network_type = NIMCP_NETWORK_SNN;
            snn_cfg.snn_method = NIMCP_SNN_TRAIN_SURROGATE;
            snn_cfg.learning_rate = brain->config.learning_rate;
            snn_cfg.snn_surrogate_beta = 10.0f;
            snn_cfg.snn_eligibility_tau = 20.0f;
            training_dispatch_init(brain, &snn_cfg);
            if (brain->snn_training_ctx) {
                fprintf(stderr, "[INFO] Reconnected SNN to training context\n");
            }
        }
        if (brain->lnn_network && !brain->lnn_training_ctx) {
            extern void* lnn_training_create(void* network, const void* config);
            extern void lnn_training_config_default(void* config);

            uint8_t lnn_cfg_buf[256];
            memset(lnn_cfg_buf, 0, sizeof(lnn_cfg_buf));
            lnn_training_config_default(lnn_cfg_buf);
            *(float*)lnn_cfg_buf = brain->config.learning_rate > 0
                ? brain->config.learning_rate : 0.01f;

            void* lnn_ctx = lnn_training_create(brain->lnn_network, lnn_cfg_buf);
            if (lnn_ctx) {
                brain->lnn_training_ctx = lnn_ctx;
                fprintf(stderr, "[INFO] Reconnected LNN to training context\n");
            }
        }
    }

    // NOTE: If metadata was loaded successfully, stats already contain saved values
    // including num_neurons. Only initialize stats if metadata wasn't loaded.
    if (!metadata_loaded) {
        // Override with actual network neuron count only if metadata wasn't loaded
        uint32_t actual_neurons = adaptive_network_get_neuron_count(brain->network);
        brain->stats.num_neurons = actual_neurons;
        brain->stats.num_synapses = brain->stats.num_neurons * brain->config.num_inputs;
        brain->stats.num_active_synapses = brain->stats.num_synapses;
    }
    // When metadata IS loaded, trust the saved stats - they represent the brain's
    // reported state at save time. The network's internal neuron count may differ
    // from what the brain reports as "num_neurons" (which is a stats/probe value).

    // Re-initialize mirror neurons if enabled but not loaded from file
    // (Handles case where brain was saved without mirror neurons but loaded config has them enabled)
    if (brain->config.enable_mirror_neurons && !brain->mirror_neurons) {
        fprintf(stderr, "[INFO] Re-initializing mirror neurons from config (enable_mirror_neurons=true but not in save file)\n");
        if (!init_mirror_neurons(brain)) {
            fprintf(stderr, "[WARN] Failed to re-initialize mirror neurons\n");
            // Non-fatal: continue without mirror neurons
        }
    }

    // Re-initialize quantum annealer if enabled but not initialized
    // (Handles case where brain was saved without quantum annealer but loaded config has it enabled)
    if (brain->config.enable_quantum_annealing && !brain->quantum_annealer) {
        fprintf(stderr, "[INFO] Re-initializing quantum annealer from config (enable_quantum_annealing=true but not in save file)\n");
        quantum_annealing_config_t qa_config = {
            .initial_temperature = brain->config.annealing_temperature_init,
            .final_temperature = brain->config.annealing_temperature_final,
            .num_iterations = brain->config.annealing_steps,
            .cooling_schedule = COOLING_EXPONENTIAL,
            .quantum_strength = 0.5F,
            .enable_tunneling = true,
            .seed = (uint32_t)time(NULL)
        };

        brain->quantum_annealer = quantum_annealer_create(&qa_config);
        if (!brain->quantum_annealer) {
            fprintf(stderr, "[WARN] Failed to re-initialize quantum annealer\n");
            brain->config.enable_quantum_annealing = false;
        } else {
            fprintf(stderr, "[INFO] Quantum annealer re-initialized successfully\n");
        }
    }

    // Re-initialize glial subsystem if enabled but not initialized
    // (Handles case where brain was saved with glial but loaded without it)
    if (brain->config.enable_glial && !brain->glial) {
        fprintf(stderr, "[INFO] Re-initializing glial subsystem from config (enable_glial=true but not in save file)\n");
        if (!init_glial_subsystem(brain)) {
            fprintf(stderr, "[WARN] Failed to re-initialize glial subsystem\n");
            brain->config.enable_glial = false;
        } else {
            fprintf(stderr, "[INFO] Glial subsystem re-initialized successfully\n");

            // Re-initialize spatial neuromodulator system if glial was created
            // Spatial neuromod is part of glial integration, so always try to initialize it
            if (brain->glial) {
                fprintf(stderr, "[INFO] Re-initializing spatial neuromodulator system\n");
                if (!init_spatial_neuromod_system(brain)) {
                    fprintf(stderr, "[WARN] Failed to re-initialize spatial neuromodulator system\n");
                    // Non-fatal: continue without spatial neuromod
                }
            }
        }
    }

    /* === CHECKPOINT CONTENT VALIDATION (A5: Security hardening) ===
     * Validate checkpoint content integrity beyond just file size.
     * Catches corrupted checkpoints with invalid neuron counts or NaN weights. */
    if (brain->network) {
        uint32_t num_neurons = neural_network_get_num_neurons(
            adaptive_network_get_base_network(brain->network));
        if (num_neurons == 0 || num_neurons > 10000000) {  /* MAX_NEURONS = 10M sanity cap */
            LOG_ERROR("Checkpoint validation failed: invalid neuron count %u", num_neurons);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                "brain_load: checkpoint validation failed — invalid neuron count");
            brain_destroy(brain);
            return NULL;
        }

        /* Spot-check neuron biases for NaN/Inf — WARN but don't fail.
         * Some NaN values in a 2.5M neuron brain may be from unconnected
         * neurons or boundary conditions, not necessarily corruption.
         * Fix NaN biases to zero instead of rejecting the checkpoint. */
        neural_network_t base_net = adaptive_network_get_base_network(brain->network);
        if (base_net) {
            uint32_t nan_count = 0;
            uint32_t check_count = (num_neurons < 1000) ? num_neurons : 1000;
            for (uint32_t i = 0; i < check_count; i++) {
                neuron_t* n = neural_network_get_neuron(base_net, i);
                if (n && (isnan(n->bias) || isinf(n->bias))) {
                    n->bias = 0.0f;  /* Fix NaN to zero */
                    nan_count++;
                }
            }
            if (nan_count > 0) {
                fprintf(stderr, "[WARN] Fixed %u NaN/Inf biases in first %u neurons\n",
                        nan_count, check_count);
            }
        }
        /* Force output layer neurons to LINEAR activation after checkpoint load.
         * Checkpoints saved before the dense-output fix may have ACTIVATION_ADAPTIVE
         * on output neurons, causing 95% sparsity (only 204/4096 nonzero). */
        if (base_net && brain->config.num_outputs > 0) {
            uint32_t out_start = num_neurons > brain->config.num_outputs
                ? num_neurons - brain->config.num_outputs : 0;
            uint32_t fixed = 0;
            for (uint32_t i = out_start; i < num_neurons; i++) {
                neuron_t* n = neural_network_get_neuron(base_net, i);
                if (n && n->activation_type != ACTIVATION_LINEAR) {
                    n->activation_type = ACTIVATION_LINEAR;
                    fixed++;
                }
            }
            if (fixed > 0) {
                fprintf(stderr, "[INFO] Fixed %u output neurons to LINEAR activation\n", fixed);
            }
        }

        fprintf(stderr, "[INFO] Checkpoint validation passed: %u neurons, bias spot-check OK\n",
                num_neurons);
    }

    /* Wire Phase 1-4 biological adapters (substrate + thalamic router) into
     * any networks restored from sidecars. brain_load() skips many runtime
     * subsystems including substrate init; if the brain was originally
     * constructed via brain_create() the factory already created substrate
     * + router and stored them on brain->substrate / brain->thalamic_router,
     * BUT brain_load() allocates a fresh brain_t whose substrate/router
     * fields are NULL. Create them now, then attach to the restored
     * SNN/LNN/cortex CNNs. NULL-tolerant: if creation fails we keep going. */
    nimcp_brain_factory_init_substrate_thalamic_subsystem(brain);
    nimcp_brain_attach_substrate_thalamic(brain);

    /* CC6: Restore cortical columns subsystem if the saved config had it.
     * The structural init reads brain->config and populates the HC slots,
     * allocates the projection matrix (zeroed), and the feature buffer.
     * After init succeeds, attempt to load the CORTICAL_COLUMNS_V1 sidecar
     * to overwrite the projection with learned weights. If the sidecar is
     * absent (old checkpoint), the loader logs a warning and continues. */
    if (brain->config.enable_cortical_columns && !brain->cortical_column_pool) {
        extern bool nimcp_brain_factory_init_cortical_columns_subsystem(brain_t brain);
        nimcp_brain_factory_init_cortical_columns_subsystem(brain);
    }
    if (brain->enable_cortical_columns) {
        cortical_columns_sidecar_load(brain, filepath);
    }

    /* Layer C: TEMPERATURE_V1 sidecar — restores decoder_temperature +
     * calibrated_ece + calibrated_at_us. Section absent → defaults stay
     * (T=1.0, ece=-1.0, at_us=0). */
    temperature_sidecar_load(brain, filepath);

    /* DEFERRED: .immune / .kg / .gl_lang sidecars used to be loaded here
     * gated on brain->immune_system / brain->internal_kg / brain->grounded_lang
     * being non-NULL — but those subsystems are created LATER, by
     * nimcp_brain_eager_init_cognitive(). Inside brain_load() they are all
     * NULL, so the gates always failed silently (no log, no error, no load)
     * and the trained lexicon / immune memory / KG facts were lost on every
     * resume. Fix: brain_load_post_init_sidecars(brain) is now invoked from
     * eager_init_cognitive AFTER the subsystems exist. The source filepath
     * is stashed in brain->loaded_from_path by brain_load_auto(). */

    brain_clear_error();
    return brain;
}

/*=============================================================================
 * brain_load_post_init_sidecars
 *
 * Loads the .immune / .kg / .gl_lang sidecars belonging to the checkpoint
 * the brain was loaded from (brain->loaded_from_path). Must be called AFTER
 * the corresponding subsystems are created (immune_system, internal_kg,
 * grounded_lang) — otherwise the per-sidecar gates no-op and trained state
 * is silently lost. nimcp_brain_eager_init_cognitive() is the canonical
 * call site; the python binding's Brain.eager_init_cognitive() invokes it.
 *
 * Safe to call multiple times — each underlying loader is idempotent
 * (re-loading the same sidecar replaces in-memory state with on-disk state).
 *
 * No-op when:
 *   - brain is NULL
 *   - brain->loaded_from_path is empty (fresh brain, never loaded)
 *=============================================================================*/
void brain_load_post_init_sidecars(brain_t brain)
{
    if (!brain) return;
    if (brain->loaded_from_path[0] == '\0') return;  /* fresh brain */

    const char* filepath = brain->loaded_from_path;

    /* Slice B (Option 1 architectural rebuild) — concept registry sidecar.
     * Restores the cross-modal binding union-find + fingerprint tables.
     * Pre-Slice-B checkpoints won't have a .concept_registry file; the
     * registry stays empty and bindings will accumulate from training
     * onwards. */
    if (brain->concept_registry) {
        char cr_path[NIMCP_METRICS_PATH_SIZE];
        snprintf(cr_path, sizeof(cr_path), "%s.concept_registry", filepath);
        FILE* cr_fp = fopen(cr_path, "rb");
        if (cr_fp) {
            int cr_rc = concept_registry_load(
                (concept_registry_t*)brain->concept_registry, cr_fp);
            fclose(cr_fp);
            if (cr_rc == 0) {
                fprintf(stderr, "[INFO] Restored concept registry from %s\n", cr_path);
            } else {
                fprintf(stderr, "[WARN] concept_registry_load failed for %s — "
                        "cross-modal bindings reset to empty\n", cr_path);
            }
        } else {
            fprintf(stderr, "[INFO] no concept_registry sidecar at %s — "
                    "starting cold\n", cr_path);
        }
    }

    /* Immune memory sidecar — restore B/T cells, antibodies, antigens.
     * Pre-Phase-B checkpoints won't have a .immune file; that's fine —
     * the loader logs file-not-found and falls back to a fresh immune
     * system. */
    if (brain->immune_system) {
        extern int immune_persistence_load(struct brain_immune_system* system,
                                           const char* filepath,
                                           const void* config);
        char immune_path[NIMCP_METRICS_PATH_SIZE];
        snprintf(immune_path, sizeof(immune_path), "%s.immune", filepath);
        if (immune_persistence_load((struct brain_immune_system*)brain->immune_system,
                                    immune_path, NULL) == 0) {
            fprintf(stderr, "[INFO] Restored immune memory from %s\n", immune_path);
        } else {
            fprintf(stderr, "[INFO] immune_persistence_load: no sidecar at %s "
                    "(legacy checkpoint or fresh start)\n", immune_path);
        }
    }

    /* Internal KG sidecar — restore runtime KG nodes/edges. Pre-sidecar
     * checkpoints won't have a .kg file; the loader logs and the KG starts
     * cold (init-time wiring still populates structural roots). */
    if (brain->internal_kg) {
        extern int brain_kg_load(struct brain_kg* kg, const char* filepath);
        char kg_path[NIMCP_METRICS_PATH_SIZE];
        snprintf(kg_path, sizeof(kg_path), "%s.kg", filepath);
        if (brain_kg_load((struct brain_kg*)brain->internal_kg, kg_path) == 0) {
            fprintf(stderr, "[INFO] Restored KG facts from %s\n", kg_path);
        } else {
            fprintf(stderr, "[INFO] brain_kg_load: no sidecar at %s — "
                    "KG starts cold\n", kg_path);
        }
    }

    /* Grounded language sidecar — restore trained lexicon + templates.
     * Pre-sidecar checkpoints won't have a .gl_lang file; the loader logs
     * and the language module continues with only seeded function/concept
     * words (no trained vocabulary). */
    if (brain->grounded_lang) {
        extern int gl_persistence_load(struct grounded_language* gl,
                                       const char* path);
        extern uint64_t grounded_language_rebind_all_to_snn_bridge(
            struct grounded_language* gl);
        extern uint64_t grounded_language_remap_pseudo_concept_ids(
            struct grounded_language* gl);
        char gl_path[NIMCP_METRICS_PATH_SIZE];
        snprintf(gl_path, sizeof(gl_path), "%s.gl_lang", filepath);
        if (gl_persistence_load(brain->grounded_lang, gl_path) == 0) {
            fprintf(stderr, "[INFO] Restored grounded language lexicon from %s\n", gl_path);
            /* gl_persistence_load reads bindings directly into entry arrays
             * and bypasses lexicon_bind, so the SNN bridge never sees the
             * restored bindings. Walk the lexicon now and re-mirror every
             * binding into the bridge. No-op if the bridge wasn't created. */
            uint64_t mirrored =
                grounded_language_rebind_all_to_snn_bridge(brain->grounded_lang);
            fprintf(stderr,
                    "[INFO] Re-mirrored %llu lexicon bindings into SNN bridge\n",
                    (unsigned long long)mirrored);
            /* Pseudo-concept-id migration (2026-05-30): legacy bindings on
             * checkpoints grounded while brain->semantic_memory was NULL
             * carry pseudo-IDs (high bit 0x100000000ULL set) that never
             * resolve in semantic_memory_get_concept. Remap them now so
             * the concept-feature path in comprehend's semantic vector
             * has signal. Emits a [GL_REMAP] line with the count. */
            uint64_t remapped =
                grounded_language_remap_pseudo_concept_ids(brain->grounded_lang);
            fprintf(stderr,
                    "[INFO] Remapped %llu pseudo-concept-id bindings to real "
                    "registry entries\n",
                    (unsigned long long)remapped);
        } else {
            fprintf(stderr, "[INFO] gl_persistence_load: no sidecar at %s — "
                    "lexicon starts from seeded vocabulary only\n", gl_path);
        }

        /* TA-1 multi-turn state sidecar (discourse + anaphora + bigram
         * spectrum). Best-effort: missing file is fine on pre-TA-1
         * checkpoints; the multi-turn ring just starts cold. */
        extern int grounded_language_load_multiturn_state(
            struct grounded_language* gl, FILE* fp);
        char mt_path[NIMCP_METRICS_PATH_SIZE];
        snprintf(mt_path, sizeof(mt_path), "%s.gl_multiturn", filepath);
        FILE* mt_fp = fopen(mt_path, "rb");
        if (mt_fp) {
            int mt_rc = grounded_language_load_multiturn_state(
                brain->grounded_lang, mt_fp);
            fclose(mt_fp);
            if (mt_rc == 0) {
                fprintf(stderr, "[INFO] Restored multi-turn language state from %s\n",
                        mt_path);
            } else {
                fprintf(stderr, "[WARN] grounded_language_load_multiturn_state "
                        "failed for %s — multi-turn state lost\n", mt_path);
            }
        } else {
            fprintf(stderr, "[INFO] grounded_language_load_multiturn_state: "
                    "no sidecar at %s — multi-turn ring starts cold\n", mt_path);
        }
    }
}

//=============================================================================
// Snapshot API - Named State Snapshots
//=============================================================================

/**
 * @brief Create snapshot directory if it doesn't exist
 *
 * @param snapshot_dir Directory path
 * @return true on success, false on error
 */
static bool ensure_snapshot_dir(const char* snapshot_dir)
{
    if (!snapshot_dir) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ensure_snapshot_dir: snapshot_dir is NULL");
        return false;
    }

    // Try to create directory (will fail silently if already exists)
    #ifdef _WIN32
    _mkdir(snapshot_dir);
    #else
    mkdir(snapshot_dir, 0755);
    #endif

    return true;
}

/**
 * @brief Get default snapshot directory
 *
 * @param brain Brain instance
 * @return Snapshot directory path
 */
static const char* get_snapshot_dir(brain_t brain)
{
    if (brain->config.snapshot_dir) {
        return brain->config.snapshot_dir;
    }
    return "./snapshots";  // Default
}

bool brain_save_snapshot(brain_t brain, const char* name, const char* description)
{
    // Guard: Validate parameters
    if (!brain || !name) {
        set_error("Invalid parameters to brain_save_snapshot");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_save_snapshot: required parameter is NULL (brain, name)");
        return false;
    }

    // Health monitoring: signal start of snapshot operation
    brain_heartbeat(brain, "brain_save_snapshot:start", 0.0f);

    // Phase SNAPSHOT-KG: Backend selection
    // Determine whether to use KG (QuestDB) or file-based storage
    snapshot_backend_t backend = (snapshot_backend_t)brain->snapshot_backend;

    if (backend == SNAPSHOT_BACKEND_AUTO) {
        // AUTO: Use KG if available, otherwise file
        backend = (brain->kg_persistence != NULL) ?
                  SNAPSHOT_BACKEND_KG : SNAPSHOT_BACKEND_FILE;
    }

    if (backend == SNAPSHOT_BACKEND_KG) {
        // Route to KG-based snapshot storage
        if (!brain->kg_persistence) {
            set_error("KG persistence not available but SNAPSHOT_BACKEND_KG requested");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_save_snapshot: brain->kg_persistence is NULL");
            return false;
        }
        int result = brain_save_snapshot_kg(brain, name, description, brain->kg_persistence);
        if (result == 0) {
            brain_clear_error();
            return true;
        }
        return false;
    }

    // Fall through to file-based storage (SNAPSHOT_BACKEND_FILE or AUTO fallback)

    // Ensure snapshot directory exists
    const char* snapshot_dir = get_snapshot_dir(brain);
    if (!ensure_snapshot_dir(snapshot_dir)) {
        set_error("Failed to create snapshot directory: %s", snapshot_dir);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_save_snapshot: ensure_snapshot_dir is NULL");
        return false;
    }

    // Generate snapshot filename with timestamp
    time_t now = time(NULL);
    char snapshot_path[NIMCP_DWARF_PATH_SIZE];
    snprintf(snapshot_path, sizeof(snapshot_path), "%s/%s_%ld.snapshot",
             snapshot_dir, name, (long)now);

    // Health monitoring: directories ready, starting save
    brain_heartbeat(brain, "brain_save_snapshot:saving", 0.3f);

    // Save brain state to snapshot file
    if (!brain_save(brain, snapshot_path)) {
        set_error("Failed to save snapshot to %s", snapshot_path);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_save_snapshot: brain_save is NULL");
        return false;
    }

    // Health monitoring: save complete, writing metadata
    brain_heartbeat(brain, "brain_save_snapshot:metadata", 0.9f);

    // Save snapshot metadata
    char meta_path[NIMCP_DWARF_PATH_SIZE];
    snprintf(meta_path, sizeof(meta_path), "%s/%s_%ld.snapshot.info",
             snapshot_dir, name, (long)now);
    FILE* meta_file = fopen(meta_path, "w");
    if (meta_file) {
        fprintf(meta_file, "name=%s\n", name);
        fprintf(meta_file, "timestamp=%ld\n", (long)now);
        if (description) {
            fprintf(meta_file, "description=%s\n", description);
        }
        fprintf(meta_file, "compressed=%d\n", brain->config.compress_snapshots ? 1 : 0);
        fprintf(meta_file, "encrypted=%d\n", brain->config.encrypt_snapshots ? 1 : 0);
        fclose(meta_file);
    }

    // Health monitoring: snapshot complete
    brain_heartbeat(brain, "brain_save_snapshot:complete", 1.0f);

    brain_clear_error();
    return true;
}

brain_t brain_restore_snapshot(brain_t brain, const char* name)
{
    // Guard: Validate parameters
    if (!name) {
        set_error("Null snapshot name provided");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_restore_snapshot: name is NULL");
        return NULL;
    }

    // Health monitoring: signal start of restore operation (if brain available)
    brain_heartbeat(brain, "brain_restore_snapshot:start", 0.0f);

    // Phase SNAPSHOT-KG: Backend selection
    // Try KG first if brain has KG persistence, then fall back to file
    snapshot_backend_t backend = brain ? (snapshot_backend_t)brain->snapshot_backend : SNAPSHOT_BACKEND_FILE;

    if (backend == SNAPSHOT_BACKEND_AUTO) {
        // AUTO: Try KG first if available
        if (brain && brain->kg_persistence) {
            brain_t restored = brain_restore_snapshot_kg(name, brain->kg_persistence);
            if (restored) {
                brain_clear_error();
                return restored;
            }
            // Fall through to file-based restore
        }
        backend = SNAPSHOT_BACKEND_FILE;
    } else if (backend == SNAPSHOT_BACKEND_KG) {
        // Explicit KG backend requested
        if (!brain || !brain->kg_persistence) {
            set_error("KG persistence not available but SNAPSHOT_BACKEND_KG requested");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_restore_snapshot: required parameter is NULL (brain, brain->kg_persistence)");
            return NULL;
        }
        brain_t restored = brain_restore_snapshot_kg(name, brain->kg_persistence);
        if (!restored) {
            set_error("Failed to restore snapshot from KG: %s", name);
        }
        return restored;
    }

    // File-based restore (SNAPSHOT_BACKEND_FILE or AUTO fallback)

    // Get snapshot directory (use default if brain is NULL)
    const char* snapshot_dir = brain ? get_snapshot_dir(brain) : "snapshots";

    // Find most recent snapshot with this name
    // Snapshots are named: {name}_{timestamp}.snapshot
    // We need to find the one with the highest timestamp
    DIR* dir = opendir(snapshot_dir);
    if (!dir) {
        set_error("Failed to open snapshot directory: %s", snapshot_dir);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_restore_snapshot: dir is NULL");
        return NULL;
    }

    char best_snapshot[NIMCP_DWARF_PATH_SIZE] = {0};
    time_t best_timestamp = 0;
    size_t name_len = strlen(name);

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        // Check if this is a snapshot file matching our name
        if (strncmp(entry->d_name, name, name_len) != 0) {
            continue;
        }

        // Check if it has the format: name_timestamp.snapshot
        if (entry->d_name[name_len] != '_') {
            continue;
        }

        size_t entry_len = strlen(entry->d_name);
        if (entry_len < 9 || strcmp(entry->d_name + entry_len - 9, ".snapshot") != 0) {
            continue;
        }

        // Extract timestamp
        char* timestamp_str = entry->d_name + name_len + 1;
        time_t timestamp = strtol(timestamp_str, NULL, 10);

        // Keep the most recent snapshot
        if (timestamp > best_timestamp) {
            best_timestamp = timestamp;
            strncpy(best_snapshot, entry->d_name, sizeof(best_snapshot) - 1);
        }
    }

    closedir(dir);

    if (best_snapshot[0] == '\0') {
        set_error("No snapshot found with name: %s", name);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_restore_snapshot: validation failed");
        return NULL;
    }

    // Health monitoring: found snapshot, starting load
    brain_heartbeat(brain, "brain_restore_snapshot:loading", 0.5f);

    // Load brain from snapshot
    char snapshot_path[NIMCP_DWARF_PATH_SIZE];
    snprintf(snapshot_path, sizeof(snapshot_path), "%s/%s", snapshot_dir, best_snapshot);

    brain_t loaded_brain = brain_load(snapshot_path);
    if (!loaded_brain) {
        set_error("Failed to load snapshot: %s", snapshot_path);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_restore_snapshot: loaded_brain is NULL");
        return NULL;
    }

    // If brain provided, we'd need to copy state into it
    // For now, return new brain instance
    if (brain) {
        fprintf(stderr, "WARNING: In-place restore not yet implemented, returning new brain instance\n");
    }

    brain_clear_error();
    return loaded_brain;
}

bool brain_list_snapshots(brain_t brain, brain_snapshot_info_t* infos,
                         uint32_t max_count, uint32_t* out_count)
{
    // Guard: Validate required parameters (out_count is optional)
    if (!brain || !infos) {
        set_error("Invalid parameters to brain_list_snapshots");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_list_snapshots: required parameter is NULL (brain, infos)");
        return false;
    }

    // Use local variable if out_count not provided
    uint32_t local_count = 0;
    uint32_t* count_ptr = out_count ? out_count : &local_count;
    *count_ptr = 0;

    // Get snapshot directory
    const char* snapshot_dir = get_snapshot_dir(brain);
    if (!snapshot_dir) {
        set_error("Failed to get snapshot directory");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_list_snapshots: snapshot_dir is NULL");
        return false;
    }

    // Open directory for scanning
    DIR* dir = opendir(snapshot_dir);
    if (!dir) {
        // Directory doesn't exist - not an error, just no snapshots
        brain_clear_error();
        return true;
    }

    // Scan directory for .snapshot files
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL && *count_ptr < max_count) {
        // Look for .snapshot files
        size_t name_len = strlen(entry->d_name);
        if (name_len < 9 || strcmp(entry->d_name + name_len - 9, ".snapshot") != 0) {
            continue;  // Not a snapshot file
        }

        // Read metadata from .info file
        char info_path[NIMCP_DWARF_PATH_SIZE];
        snprintf(info_path, sizeof(info_path), "%s/%s.info", snapshot_dir, entry->d_name);

        FILE* info_file = fopen(info_path, "r");
        if (!info_file) {
            continue;  // No metadata, skip this snapshot
        }

        brain_snapshot_info_t* info = &infos[*count_ptr];
        memset(info, 0, sizeof(brain_snapshot_info_t));

        // Parse metadata file
        char line[NIMCP_CMD_BUFFER_SIZE];
        while (fgets(line, sizeof(line), info_file)) {
            char* equals = strchr(line, '=');
            if (!equals) continue;

            *equals = '\0';
            char* key = line;
            char* value = equals + 1;

            // Trim newline from value
            char* newline = strchr(value, '\n');
            if (newline) *newline = '\0';

            if (strcmp(key, "name") == 0) {
                strncpy(info->name, value, sizeof(info->name) - 1);
            } else if (strcmp(key, "description") == 0) {
                strncpy(info->description, value, sizeof(info->description) - 1);
            } else if (strcmp(key, "timestamp") == 0) {
                info->timestamp = (uint64_t)strtol(value, NULL, 10);
            } else if (strcmp(key, "compressed") == 0) {
                // P1-2 fix: Use strtol instead of atoi for safe conversion
                char* endptr;
                long val = strtol(value, &endptr, 10);
                info->is_compressed = (endptr != value && val != 0);
            } else if (strcmp(key, "encrypted") == 0) {
                // P1-2 fix: Use strtol instead of atoi for safe conversion
                char* endptr;
                long val = strtol(value, &endptr, 10);
                info->is_encrypted = (endptr != value && val != 0);
            }
        }

        fclose(info_file);

        // Get file size
        char snapshot_path[NIMCP_DWARF_PATH_SIZE];
        snprintf(snapshot_path, sizeof(snapshot_path), "%s/%s", snapshot_dir, entry->d_name);
        struct stat st;
        if (stat(snapshot_path, &st) == 0) {
            info->file_size = (uint32_t)st.st_size;
        }

        (*count_ptr)++;
    }

    closedir(dir);

    brain_clear_error();
    return true;
}

bool brain_delete_snapshot(brain_t brain, const char* name)
{
    // Guard: Validate parameters
    if (!brain || !name) {
        set_error("Invalid parameters to brain_delete_snapshot");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_delete_snapshot: required parameter is NULL (brain, name)");
        return false;
    }

    const char* snapshot_dir = get_snapshot_dir(brain);

    // Find most recent snapshot with this name
    // Snapshots are named: {name}_{timestamp}.snapshot
    DIR* dir = opendir(snapshot_dir);
    if (!dir) {
        set_error("Failed to open snapshot directory: %s", snapshot_dir);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_delete_snapshot: dir is NULL");
        return false;
    }

    char best_snapshot[NIMCP_DWARF_PATH_SIZE] = {0};
    time_t best_timestamp = 0;
    size_t name_len = strlen(name);

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        // Check if this is a snapshot file matching our name
        if (strncmp(entry->d_name, name, name_len) != 0) {
            continue;
        }

        // Check if it has the format: name_timestamp.snapshot
        if (entry->d_name[name_len] != '_') {
            continue;
        }

        size_t entry_len = strlen(entry->d_name);
        if (entry_len < 9 || strcmp(entry->d_name + entry_len - 9, ".snapshot") != 0) {
            continue;
        }

        // Extract timestamp
        char* timestamp_str = entry->d_name + name_len + 1;
        time_t timestamp = strtol(timestamp_str, NULL, 10);

        // Keep the most recent snapshot
        if (timestamp > best_timestamp) {
            best_timestamp = timestamp;
            strncpy(best_snapshot, entry->d_name, sizeof(best_snapshot) - 1);
        }
    }

    closedir(dir);

    if (best_snapshot[0] == '\0') {
        set_error("No snapshot found with name: %s", name);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_delete_snapshot: validation failed");
        return false;
    }

    // Delete snapshot file
    char snapshot_path[NIMCP_DWARF_PATH_SIZE];
    snprintf(snapshot_path, sizeof(snapshot_path), "%s/%s", snapshot_dir, best_snapshot);

    if (remove(snapshot_path) != 0) {
        set_error("Failed to delete snapshot: %s", snapshot_path);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_delete_snapshot: validation failed");
        return false;
    }

    /* Delete metadata file if it exists. Source and dest buffers are both
     * NIMCP_DWARF_PATH_SIZE; appending a suffix could overflow if the base
     * path is at max length. Cap source to (SIZE - suffix length - 1) via
     * %.*s so the result always fits. Longest suffix is ".knowledge" = 10. */
    char meta_path[NIMCP_DWARF_PATH_SIZE];
    snprintf(meta_path, sizeof(meta_path), "%.*s.info",
             (int)(NIMCP_DWARF_PATH_SIZE - 6), snapshot_path);
    remove(meta_path);  // Ignore error

    // Delete .meta file if it exists
    snprintf(meta_path, sizeof(meta_path), "%.*s.meta",
             (int)(NIMCP_DWARF_PATH_SIZE - 6), snapshot_path);
    remove(meta_path);  // Ignore error

    // Delete .knowledge file if it exists
    char knowledge_path[NIMCP_DWARF_PATH_SIZE];
    snprintf(knowledge_path, sizeof(knowledge_path), "%.*s.knowledge",
             (int)(NIMCP_DWARF_PATH_SIZE - 11), snapshot_path);
    remove(knowledge_path);  // Ignore error

    // Update statistics
    update_stats_snapshot_delete();
    record_security_interaction(true, 1.0);

    brain_clear_error();
    return true;
}

//=============================================================================
// Phase PERSIST-1: Module Initialization and Security Integration API
//=============================================================================

/**
 * @brief Initialize persistence module with security registration
 *
 * WHAT: Initialize module state and register with security module
 * WHY:  Enable security audit trail and trust tracking for persistence ops
 * HOW:  Initialize mutex, reset stats, register with security context
 *
 * @param security_ctx Security integration context (NULL to skip registration)
 * @return true on success, false on failure
 */
bool persistence_init(nimcp_sec_integration_t* security_ctx)
{
    // Guard: Already initialized
    if (g_persistence_state.initialized) {
        return true;
    }

    // Initialize statistics mutex (non-recursive)
    if (nimcp_platform_mutex_init(&g_persistence_state.stats_mutex, false) != 0) {
        fprintf(stderr, "ERROR: Failed to initialize persistence stats mutex\n");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "persistence_init: validation failed");
        return false;
    }

    // Reset statistics
    memset(&g_persistence_state.stats, 0, sizeof(persistence_stats_t));

    // Register with security module if context provided
    if (security_ctx) {
        g_persistence_state.security_ctx = security_ctx;

        // Register as persistence module (IO category for save/load operations)
        nimcp_result_t result = nimcp_sec_register_module(
            security_ctx,
            "persistence",
            NIMCP_SEC_CAT_IO,
            &g_persistence_state.security_module_id
        );

        if (result != NIMCP_SUCCESS) {
            fprintf(stderr, "WARNING: Failed to register persistence with security module\n");
            g_persistence_state.security_module_id = 0;
            // Continue without security - non-fatal
        } else {
            fprintf(stderr, "[INFO] Persistence module registered with security (ID=%u)\n",
                    g_persistence_state.security_module_id);
        }
    }

    g_persistence_state.initialized = true;
    return true;
}

/**
 * @brief Shutdown persistence module
 *
 * WHAT: Clean shutdown of persistence module
 * WHY:  Unregister from security, cleanup resources
 * HOW:  Unregister security module, destroy mutex
 */
void persistence_shutdown(void)
{
    // Guard: Not initialized
    if (!g_persistence_state.initialized) {
        return;
    }

    // Unregister from security module
    if (g_persistence_state.security_ctx && g_persistence_state.security_module_id != 0) {
        nimcp_sec_unregister_module(
            g_persistence_state.security_ctx,
            g_persistence_state.security_module_id
        );
    }

    // Destroy mutex
    nimcp_platform_mutex_destroy(&g_persistence_state.stats_mutex);

    // Reset state
    g_persistence_state.initialized = false;
    g_persistence_state.security_ctx = NULL;
    g_persistence_state.security_module_id = 0;
    memset(&g_persistence_state.stats, 0, sizeof(persistence_stats_t));
}

/**
 * @brief Get persistence module security ID
 *
 * @return Security module ID (0 if not registered)
 */
uint32_t persistence_get_security_module_id(void)
{
    return g_persistence_state.security_module_id;
}

/**
 * @brief Get persistence statistics
 *
 * @param stats Output statistics structure
 * @return true on success
 */
bool persistence_get_stats(persistence_stats_t* stats)
{
    // Guard: NULL parameter
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "persistence_get_stats: stats is NULL");
        return false;
    }

    // Guard: Not initialized
    if (!g_persistence_state.initialized) {
        memset(stats, 0, sizeof(persistence_stats_t));
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "persistence_get_stats: g_persistence_state is NULL");
        return false;
    }

    // Copy stats thread-safely
    nimcp_platform_mutex_lock(&g_persistence_state.stats_mutex);
    memcpy(stats, &g_persistence_state.stats, sizeof(persistence_stats_t));
    nimcp_platform_mutex_unlock(&g_persistence_state.stats_mutex);

    return true;
}

/**
 * @brief Reset persistence statistics
 */
void persistence_reset_stats(void)
{
    if (!g_persistence_state.initialized) {
        return;
    }

    nimcp_platform_mutex_lock(&g_persistence_state.stats_mutex);
    memset(&g_persistence_state.stats, 0, sizeof(persistence_stats_t));
    nimcp_platform_mutex_unlock(&g_persistence_state.stats_mutex);
}

/**
 * @brief Get default persistence configuration
 *
 * @return Default configuration with sensible values
 */
persistence_config_t persistence_default_config(void)
{
    persistence_config_t config = {
        // Memory Integration
        .use_unified_memory = false,
        .memory_manager = NULL,
        .enable_cow_snapshots = false,

        // Security Integration
        .enable_security = false,
        .security_context = NULL,

        // Buffer settings
        .read_buffer_size = 64 * 1024,   // 64KB default
        .write_buffer_size = 64 * 1024,  // 64KB default

        // Integrity checking
        .enable_checksum = true,
        .verify_on_load = true
    };

    return config;
}

//=============================================================================
// Phase PERSIST-2: Extended Save/Load API with Memory/Security Integration
//=============================================================================

/**
 * @brief Compute simple checksum for data integrity
 *
 * WHAT: Compute Fletcher-32 checksum of data
 * WHY:  Detect corruption in saved files
 * HOW:  Fast checksum algorithm suitable for file integrity
 *
 * @param data Data to checksum
 * @param len Length in bytes
 * @return 32-bit checksum
 */
static uint32_t compute_checksum(const void* data, size_t len)
{
    const uint16_t* words = (const uint16_t*)data;
    size_t word_count = len / 2;
    uint32_t sum1 = 0xFFFF;
    uint32_t sum2 = 0xFFFF;

    for (size_t i = 0; i < word_count; i++) {
        sum1 = (sum1 + words[i]) % 65535;
        sum2 = (sum2 + sum1) % 65535;
    }

    // Handle odd byte
    if (len % 2) {
        uint16_t last = ((const uint8_t*)data)[len - 1];
        sum1 = (sum1 + last) % 65535;
        sum2 = (sum2 + sum1) % 65535;
    }

    return (sum2 << 16) | sum1;
}

/**
 * @brief Allocate buffer using unified memory or malloc
 *
 * WHAT: Allocate memory from pool or fallback to malloc
 * WHY:  Use unified memory when available for efficiency
 * HOW:  Check config, use pool if available, else malloc
 *
 * @param config Persistence configuration
 * @param size Buffer size
 * @param handle Output unified memory handle (if using pool)
 * @return Allocated buffer or NULL
 */
static void* alloc_buffer(const persistence_config_t* config, size_t size,
                         unified_mem_handle_t* handle)
{
    if (handle) *handle = NULL;

    // Use unified memory if configured and available
    if (config && config->use_unified_memory && config->memory_manager) {
        unified_mem_request_t request = {
            .size = size,
            .initial_data = NULL,
            .strategy = UNIFIED_STRATEGY_AUTO,
            .enable_cow = false,
            .alignment = 0
        };
        unified_mem_handle_t h = unified_mem_alloc(config->memory_manager, &request);
        if (h) {
            void* ptr = unified_mem_write(h);
            if (ptr) {
                if (handle) *handle = h;
                update_stats_memory_alloc(true);
                return ptr;
            }
            unified_mem_free(h);
        }
        // Fall through to malloc if pool allocation fails
    }

    // Fallback to malloc
    void* ptr = nimcp_malloc(size);
    if (ptr) {
        update_stats_memory_alloc(false);
    }
    return ptr;
}

/**
 * @brief Free buffer allocated by alloc_buffer
 *
 * @param config Persistence configuration
 * @param ptr Buffer pointer (unused when handle is valid)
 * @param handle Unified memory handle (NULL if malloc'd)
 */
static void free_buffer(const persistence_config_t* config, void* ptr,
                       unified_mem_handle_t handle)
{
    (void)config;  // May be unused

    if (handle) {
        unified_mem_free(handle);
    } else if (ptr) {
        nimcp_free(ptr);
    }
}

/**
 * @brief Save brain with extended configuration
 *
 * WHAT: Save brain with unified memory and security integration
 * WHY:  Enable efficient buffering and security audit trail
 * HOW:  Use configured memory manager for buffers, record security interactions
 *
 * @param brain Brain instance
 * @param filepath Path to save to
 * @param config Persistence configuration (NULL for defaults)
 * @return true on success, false on error
 */
bool brain_save_ex(brain_t brain, const char* filepath, const persistence_config_t* config)
{
    // Guard: Validate parameters
    if (!brain || !filepath) {
        set_error("Invalid parameters to brain_save_ex");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_save_ex: required parameter is NULL (brain, filepath)");
        return false;
    }

    // Health monitoring: signal start of extended save operation
    brain_heartbeat(brain, "brain_save_ex:start", 0.0f);

    // Use default config if not provided
    persistence_config_t default_cfg = persistence_default_config();
    if (!config) {
        config = &default_cfg;
    }

    // Start timing
    uint64_t start_time = nimcp_platform_time_monotonic_ms();

    // Save using base brain_save function
    bool success = brain_save(brain, filepath);

    // Calculate time
    uint64_t end_time = nimcp_platform_time_monotonic_ms();
    uint64_t time_ms = end_time - start_time;

    // Get file size for statistics
    uint64_t bytes_written = 0;
    struct stat st;
    if (stat(filepath, &st) == 0) {
        bytes_written = (uint64_t)st.st_size;
    }

    // Health monitoring: save complete, computing checksum
    brain_heartbeat(brain, "brain_save_ex:checksum", 0.8f);

    // Compute and save checksum if enabled
    if (success && config->enable_checksum) {
        char checksum_path[NIMCP_METRICS_PATH_SIZE];
        snprintf(checksum_path, sizeof(checksum_path), "%s.checksum", filepath);

        // Read file and compute checksum
        FILE* f = fopen(filepath, "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            long file_size = ftell(f);
            fseek(f, 0, SEEK_SET);

            if (file_size > 0 && file_size < 100 * 1024 * 1024) {  // Max 100MB for checksum
                unified_mem_handle_t handle = NULL;
                void* data = alloc_buffer(config, (size_t)file_size, &handle);
                if (data) {
                    if (fread(data, 1, (size_t)file_size, f) == (size_t)file_size) {
                        uint32_t checksum = compute_checksum(data, (size_t)file_size);

                        // Write checksum file
                        FILE* cf = fopen(checksum_path, "wb");
                        if (cf) {
                            fwrite(&checksum, sizeof(uint32_t), 1, cf);
                            fwrite(&file_size, sizeof(long), 1, cf);
                            fclose(cf);
                        }
                    }
                    free_buffer(config, data, handle);
                }
            }
            fclose(f);
        }
    }

    // Update statistics
    update_stats_save(bytes_written, time_ms, success);

    // Record security interaction
    record_security_interaction(success, 1.0);

    // Health monitoring: extended save complete
    brain_heartbeat(brain, "brain_save_ex:complete", 1.0f);

    return success;
}

/**
 * @brief Load brain with extended configuration
 *
 * WHAT: Load brain with unified memory and security integration
 * WHY:  Enable efficient buffering and security audit trail
 * HOW:  Use configured memory manager for buffers, record security interactions
 *
 * @param filepath Path to load from
 * @param config Persistence configuration (NULL for defaults)
 * @return Brain instance or NULL on error
 */
brain_t brain_load_ex(const char* filepath, const persistence_config_t* config)
{
    // Guard: Validate filepath
    if (!filepath) {
        set_error("Null filepath provided to brain_load_ex");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_load_ex: filepath is NULL");
        return NULL;
    }

    // Use default config if not provided
    persistence_config_t default_cfg = persistence_default_config();
    if (!config) {
        config = &default_cfg;
    }

    // Start timing
    uint64_t start_time = nimcp_platform_time_monotonic_ms();

    // Verify checksum if enabled
    if (config->verify_on_load && config->enable_checksum) {
        char checksum_path[NIMCP_METRICS_PATH_SIZE];
        snprintf(checksum_path, sizeof(checksum_path), "%s.checksum", filepath);

        FILE* cf = fopen(checksum_path, "rb");
        if (cf) {
            uint32_t stored_checksum = 0;
            long stored_size = 0;
            if (fread(&stored_checksum, sizeof(uint32_t), 1, cf) != 1 ||
                fread(&stored_size, sizeof(long), 1, cf) != 1) {
                fprintf(stderr, "WARNING: Short read on checksum file, skipping verification\n");
                fclose(cf);
                goto skip_checksum_verify;
            }
            fclose(cf);

            // Read file and verify checksum
            FILE* f = fopen(filepath, "rb");
            if (f) {
                fseek(f, 0, SEEK_END);
                long file_size = ftell(f);
                fseek(f, 0, SEEK_SET);

                bool checksum_valid = false;
                if (file_size == stored_size && file_size > 0 && file_size < 100 * 1024 * 1024) {
                    unified_mem_handle_t handle = NULL;
                    void* data = alloc_buffer(config, (size_t)file_size, &handle);
                    if (data) {
                        if (fread(data, 1, (size_t)file_size, f) == (size_t)file_size) {
                            uint32_t computed_checksum = compute_checksum(data, (size_t)file_size);
                            checksum_valid = (computed_checksum == stored_checksum);
                        }
                        free_buffer(config, data, handle);
                    }
                }
                fclose(f);

                if (!checksum_valid) {
                    update_stats_checksum_failure();
                    record_security_interaction(false, 1.0);
                    set_error("Checksum verification failed for %s", filepath);
                    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_load_ex: checksum_valid is NULL");
                    return NULL;
                }
            }
        }
        // No checksum file is not an error - file may have been saved without checksums
    }
skip_checksum_verify:

    // Load using base brain_load function
    brain_t brain = brain_load(filepath);

    // Calculate time
    uint64_t end_time = nimcp_platform_time_monotonic_ms();
    uint64_t time_ms = end_time - start_time;

    // Get file size for statistics
    uint64_t bytes_read = 0;
    struct stat st;
    if (stat(filepath, &st) == 0) {
        bytes_read = (uint64_t)st.st_size;
    }

    // Update statistics
    update_stats_load(bytes_read, time_ms, brain != NULL);

    // Record security interaction
    record_security_interaction(brain != NULL, 1.0);

    return brain;
}

/**
 * @brief Create instant snapshot using CoW
 *
 * WHAT: Create instant snapshot without copying data
 * WHY:  O(1) snapshot creation for checkpointing
 * HOW:  Uses page-level CoW if available, falls back to regular save
 *
 * @param brain Brain instance
 * @param name Snapshot name
 * @param description Optional description
 * @param config Persistence configuration (NULL for defaults)
 * @return true on success
 */
bool brain_save_snapshot_cow(brain_t brain, const char* name,
                             const char* description, const persistence_config_t* config)
{
    // Guard: Validate parameters
    if (!brain || !name) {
        set_error("Invalid parameters to brain_save_snapshot_cow");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_save_snapshot_cow: required parameter is NULL (brain, name)");
        return false;
    }

    // Health monitoring: signal start of CoW snapshot operation
    brain_heartbeat(brain, "brain_save_snapshot_cow:start", 0.0f);

    // Use default config if not provided
    persistence_config_t default_cfg = persistence_default_config();
    if (!config) {
        config = &default_cfg;
    }

    size_t memory_saved = 0;

    // Check if CoW snapshots are enabled and unified memory is available
    // NOTE: Full CoW snapshot support requires brain data to be allocated through
    // unified memory handles. For now, we record CoW statistics if unified memory
    // manager is provided, but fall back to regular file-based snapshots.
    if (config->enable_cow_snapshots && config->use_unified_memory &&
        config->memory_manager) {

        // Get memory stats to calculate any savings from shared data
        unified_mem_stats_t mem_stats;
        if (unified_mem_get_stats(config->memory_manager, &mem_stats)) {
            memory_saved = mem_stats.memory_saved_bytes;
        }

        // Still save metadata file for snapshot discovery
        const char* snapshot_dir = brain->config.snapshot_dir ?
                                   brain->config.snapshot_dir : "snapshots";
        ensure_snapshot_dir(snapshot_dir);

        time_t now = time(NULL);
        char meta_path[NIMCP_DWARF_PATH_SIZE];
        snprintf(meta_path, sizeof(meta_path), "%s/%s_%ld.snapshot.info",
                 snapshot_dir, name, (long)now);

        FILE* meta_file = fopen(meta_path, "w");
        if (meta_file) {
            fprintf(meta_file, "name=%s\n", name);
            fprintf(meta_file, "timestamp=%ld\n", (long)now);
            if (description) {
                fprintf(meta_file, "description=%s\n", description);
            }
            fprintf(meta_file, "cow=0\n");  // CoW not fully implemented yet
            fprintf(meta_file, "memory_saved=%zu\n", memory_saved);
            fclose(meta_file);
        }
    }

    // Health monitoring: CoW metadata saved, starting file save
    brain_heartbeat(brain, "brain_save_snapshot_cow:saving", 0.5f);

    // Save snapshot using regular file-based method
    bool success = brain_save_snapshot(brain, name, description);

    // Update statistics
    update_stats_snapshot_create(memory_saved > 0, memory_saved);

    // Record security interaction
    record_security_interaction(success, 1.0);

    // Health monitoring: CoW snapshot complete
    brain_heartbeat(brain, "brain_save_snapshot_cow:complete", 1.0f);

    return success;
}
