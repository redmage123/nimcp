/**
 * @file nimcp_swarm_pheromone.c
 * @brief Implementation of stigmergic pheromone trails for swarm coordination
 *
 * This module provides a spatial pheromone system inspired by ant colony
 * communication. Pheromones are stored in a 3D voxel grid and decay over time,
 * enabling indirect coordination between swarm agents.
 *
 * @author NIMCP Team
 * @date 2025-12-08
 */

#include "swarm/nimcp_swarm_pheromone.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/validation/nimcp_common.h"
#include "api/nimcp_api_exception.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "security/nimcp_bbb_helpers.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <math.h>
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(swarm_pheromone)

/* ============================================================================
 * Constants and Macros
 * ============================================================================ */

#define PHEROMONE_MODULE "PheromoneSystem"
#define MAX_GRID_DIMENSION 1000
#define MIN_VOXEL_SIZE 0.1F
#define MAX_VOXEL_SIZE 100.0F
#include "constants/nimcp_constants.h"
#define EPSILON NIMCP_EPSILON_NUMERICAL

/* Bio-async message types */
#define BIOMSG_PHEROMONE_DEPOSIT 0x5000
#define BIOMSG_PHEROMONE_UPDATE  0x5001

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief 3D grid index
 */
typedef struct {
    int32_t x;
    int32_t y;
    int32_t z;
} grid_index_t;

/**
 * @brief Voxel hash table entry
 */
typedef struct voxel_entry_t {
    grid_index_t index;
    nimcp_pheromone_voxel_t voxel;
    struct voxel_entry_t* next;
} voxel_entry_t;

/**
 * @brief Pheromone system state
 */
struct nimcp_pheromone_system_t {
    nimcp_pheromone_config_t config;

    /* Spatial grid (hash table for sparse storage) */
    voxel_entry_t** grid_hash;
    size_t hash_size;
    size_t active_voxels;

    /* Grid dimensions */
    grid_index_t grid_dims;

    /* Bio-async integration */
    bio_module_context_t bio_ctx;
    bool bio_async_enabled;
    uint64_t last_broadcast_time;

    /* Security */
    bbb_system_t bbb;

    /* Statistics */
    nimcp_pheromone_stats_t stats;

    /* Synchronization */
    nimcp_platform_mutex_t mutex;
};

/**
 * @brief Pheromone bio-async message (header + payload)
 */
typedef struct {
    bio_message_header_t header;      /* Must be first - bio-async requirement */
    nimcp_position3d_t position;
    nimcp_pheromone_type_t type;
    float concentration;
} bio_msg_pheromone_t;

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * @brief Convert world position to grid index
 */
static grid_index_t position_to_grid(
    const nimcp_pheromone_system_t* system,
    const nimcp_position3d_t* pos
) {
    grid_index_t idx;
    idx.x = (int32_t)floorf((pos->x - system->config.world_min.x) /
                            system->config.voxel_size);
    idx.y = (int32_t)floorf((pos->y - system->config.world_min.y) /
                            system->config.voxel_size);
    idx.z = (int32_t)floorf((pos->z - system->config.world_min.z) /
                            system->config.voxel_size);
    return idx;
}

/**
 * @brief Convert grid index to world position (center of voxel)
 */
static nimcp_position3d_t grid_to_position(
    const nimcp_pheromone_system_t* system,
    const grid_index_t* idx
) {
    nimcp_position3d_t pos;
    pos.x = system->config.world_min.x + (idx->x + 0.5F) * system->config.voxel_size;
    pos.y = system->config.world_min.y + (idx->y + 0.5F) * system->config.voxel_size;
    pos.z = system->config.world_min.z + (idx->z + 0.5F) * system->config.voxel_size;
    return pos;
}

/**
 * @brief Calculate hash for grid index
 */
static size_t grid_hash_function(const grid_index_t* idx, size_t hash_size) {
    /* Simple hash combining x, y, z coordinates */
    size_t hash = (size_t)idx->x;
    hash = hash * 73856093 ^ (size_t)idx->y;
    hash = hash * 19349663 ^ (size_t)idx->z;
    return hash % hash_size;
}

/**
 * @brief Check if grid index is within bounds
 */
static bool is_index_valid(
    const nimcp_pheromone_system_t* system,
    const grid_index_t* idx
) {
    return idx->x >= 0 && idx->x < system->grid_dims.x &&
           idx->y >= 0 && idx->y < system->grid_dims.y &&
           idx->z >= 0 && idx->z < system->grid_dims.z;
}

/**
 * @brief Get or create voxel at grid index
 */
static voxel_entry_t* get_or_create_voxel(
    nimcp_pheromone_system_t* system,
    const grid_index_t* idx
) {
    if (!is_index_valid(system, idx)) {
        return NULL;
    }

    size_t hash = grid_hash_function(idx, system->hash_size);
    voxel_entry_t* entry = system->grid_hash[hash];

    /* Search for existing entry */
    while (entry) {
        if (entry->index.x == idx->x &&
            entry->index.y == idx->y &&
            entry->index.z == idx->z) {
            return entry;
        }
        entry = entry->next;
    }

    /* Create new entry */
    entry = (voxel_entry_t*)nimcp_calloc(1, sizeof(voxel_entry_t));
    if (!entry) {
        LOG_ERROR("Failed to allocate voxel entry");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "get_or_create_voxel: entry is NULL");
        return NULL;
    }

    entry->index = *idx;
    entry->voxel.environmental_modifier = 1.0F;
    entry->voxel.last_update = nimcp_time_get_us() / 1000;

    /* Insert at head of chain */
    entry->next = system->grid_hash[hash];
    system->grid_hash[hash] = entry;
    system->active_voxels++;

    return entry;
}

/**
 * @brief Get existing voxel at grid index (read-only)
 */
static const voxel_entry_t* get_voxel(
    const nimcp_pheromone_system_t* system,
    const grid_index_t* idx
) {
    if (!is_index_valid(system, idx)) {
        return NULL;  /* Out-of-range index - not an error */
    }

    size_t hash = grid_hash_function(idx, system->hash_size);
    voxel_entry_t* entry = system->grid_hash[hash];

    while (entry) {
        if (entry->index.x == idx->x &&
            entry->index.y == idx->y &&
            entry->index.z == idx->z) {
            return entry;
        }
        entry = entry->next;
    }

    return NULL;  /* Normal search miss - voxel not found */
}

/**
 * @brief Calculate distance between two positions
 */
static float position_distance(
    const nimcp_position3d_t* a,
    const nimcp_position3d_t* b
) {
    float dx = a->x - b->x;
    float dy = a->y - b->y;
    float dz = a->z - b->z;
    return sqrtf(dx*dx + dy*dy + dz*dz);
}

/**
 * @brief Normalize a 3D vector
 */
static void normalize_vector(nimcp_position3d_t* vec) {
    float len = sqrtf(vec->x * vec->x + vec->y * vec->y + vec->z * vec->z);
    if (len > EPSILON) {
        vec->x /= len;
        vec->y /= len;
        vec->z /= len;
    }
}

/**
 * @brief Apply exponential decay to concentration
 */
static float apply_decay(
    float concentration,
    float decay_rate,
    float evaporation_rate,
    float env_modifier,
    float delta_time_s
) {
    float total_decay = (decay_rate + evaporation_rate) * env_modifier;
    return concentration * expf(-total_decay * delta_time_s);
}

/**
 * @brief Validate position is within world bounds
 */
static bool validate_position(
    const nimcp_pheromone_system_t* system,
    const nimcp_position3d_t* pos
) {
    return pos->x >= system->config.world_min.x &&
           pos->x <= system->config.world_max.x &&
           pos->y >= system->config.world_min.y &&
           pos->y <= system->config.world_max.y &&
           pos->z >= system->config.world_min.z &&
           pos->z <= system->config.world_max.z;
}

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

void nimcp_pheromone_default_config(nimcp_pheromone_config_t* out_config) {
    if (!out_config) return;

    memset(out_config, 0, sizeof(nimcp_pheromone_config_t));

    /* Default world: 100x100x100 units */
    out_config->world_min = (nimcp_position3d_t){-50.0F, -50.0F, -50.0F};
    out_config->world_max = (nimcp_position3d_t){50.0F, 50.0F, 50.0F};
    out_config->voxel_size = 1.0F;

    /* Decay rates (per second) - different types decay at different rates */
    out_config->decay_rates[PHEROMONE_DANGER] = 0.05F;    /* Long-lasting */
    out_config->decay_rates[PHEROMONE_RESOURCE] = 0.1F;   /* Medium */
    out_config->decay_rates[PHEROMONE_PATH] = 0.2F;       /* Quick decay */
    out_config->decay_rates[PHEROMONE_AVOID] = 0.15F;     /* Medium-slow */
    out_config->decay_rates[PHEROMONE_RALLY] = 0.08F;     /* Slow */
    out_config->decay_rates[PHEROMONE_TARGET] = 0.05F;    /* Long-lasting */

    out_config->evaporation_rate = 0.01F;
    out_config->max_concentration = 100.0F;
    out_config->deposit_amount = 1.0F;
    out_config->reinforcement_factor = 1.5F;
    out_config->detection_threshold = 0.1F;
    out_config->gradient_sample_distance = 2.0F;

    out_config->enable_bio_async = true;
    out_config->broadcast_interval_ms = 1000;
}

nimcp_result_t nimcp_pheromone_validate_config(
    const nimcp_pheromone_config_t* config
) {
    NIMCP_CHECK_THROW(config, NIMCP_INVALID_PARAM, "configuration pointer is NULL");

    /* Validate world bounds */
    if (config->world_min.x >= config->world_max.x ||
        config->world_min.y >= config->world_max.y ||
        config->world_min.z >= config->world_max.z) {
        LOG_ERROR("Invalid world bounds");
        return NIMCP_INVALID_PARAM;
    }

    /* Validate voxel size */
    if (config->voxel_size < MIN_VOXEL_SIZE ||
        config->voxel_size > MAX_VOXEL_SIZE) {
        LOG_ERROR("Invalid voxel size: %f (must be between %f and %f)",
                  config->voxel_size, MIN_VOXEL_SIZE, MAX_VOXEL_SIZE);
        return NIMCP_INVALID_PARAM;
    }

    /* Check grid dimensions are reasonable */
    int32_t dim_x = (int32_t)ceilf((config->world_max.x - config->world_min.x) /
                                    config->voxel_size);
    int32_t dim_y = (int32_t)ceilf((config->world_max.y - config->world_min.y) /
                                    config->voxel_size);
    int32_t dim_z = (int32_t)ceilf((config->world_max.z - config->world_min.z) /
                                    config->voxel_size);

    if (dim_x > MAX_GRID_DIMENSION || dim_y > MAX_GRID_DIMENSION ||
        dim_z > MAX_GRID_DIMENSION) {
        LOG_ERROR("Grid dimensions too large: %dx%dx%d (max: %d)",
                  dim_x, dim_y, dim_z, MAX_GRID_DIMENSION);
        return NIMCP_INVALID_PARAM;
    }

    /* Validate decay rates */
    for (int i = 0; i < PHEROMONE_TYPE_COUNT; i++) {
        if (config->decay_rates[i] < 0.0F || config->decay_rates[i] > 10.0F) {
            LOG_ERROR("Invalid decay rate for type %d: %f", i, config->decay_rates[i]);
            return NIMCP_INVALID_PARAM;
        }
    }

    /* Validate other parameters */
    if (config->max_concentration <= 0.0F || config->deposit_amount <= 0.0F ||
        config->reinforcement_factor <= 0.0F || config->detection_threshold < 0.0F ||
        config->gradient_sample_distance <= 0.0F) {
        LOG_ERROR("Invalid parameter values");
        return NIMCP_INVALID_PARAM;
    }

    return NIMCP_SUCCESS;
}

nimcp_pheromone_system_t* nimcp_pheromone_create(
    const nimcp_pheromone_config_t* config,
    bbb_system_t bbb
) {
    if (!config) {
        LOG_ERROR("NULL configuration");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL config for pheromone create");
        return NULL;
    }

    /* Validate configuration */
    if (nimcp_pheromone_validate_config(config) != NIMCP_SUCCESS) {
        LOG_ERROR("Pheromone config validation failed");
        return NULL;  /* Validation already logged details */
    }

    /* Security audit */
    if (bbb) {
        bbb_audit_log(BBB_AUDIT_INFO, "pheromone", "pheromone_create",
                      "Creating pheromone system");
    }

    nimcp_pheromone_system_t* system =
        (nimcp_pheromone_system_t*)nimcp_calloc(1, sizeof(nimcp_pheromone_system_t));
    if (!system) {
        LOG_ERROR("Failed to allocate pheromone system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "failed to allocate pheromone system");
        return NULL;
    }

    /* Copy configuration */
    system->config = *config;
    system->bbb = bbb;

    /* Calculate grid dimensions */
    system->grid_dims.x = (int32_t)ceilf(
        (config->world_max.x - config->world_min.x) / config->voxel_size);
    system->grid_dims.y = (int32_t)ceilf(
        (config->world_max.y - config->world_min.y) / config->voxel_size);
    system->grid_dims.z = (int32_t)ceilf(
        (config->world_max.z - config->world_min.z) / config->voxel_size);

    /* Create hash table for sparse voxel storage */
    system->hash_size = (size_t)(system->grid_dims.x * system->grid_dims.y *
                                 system->grid_dims.z / 10); /* ~10% density */
    if (system->hash_size < 1024) system->hash_size = 1024;

    system->grid_hash = (voxel_entry_t**)nimcp_calloc(
        system->hash_size, sizeof(voxel_entry_t*));
    if (!system->grid_hash) {
        LOG_ERROR("Failed to allocate grid hash table");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "failed to allocate pheromone grid hash table");
        nimcp_free(system);
        return NULL;
    }

    /* Initialize mutex */
    if (nimcp_platform_mutex_init(&system->mutex, false) != 0) {
        LOG_ERROR("Failed to initialize mutex");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "failed to init pheromone mutex");
        nimcp_free(system->grid_hash);
        nimcp_free(system);
        return NULL;
    }

    LOG_INFO("Created pheromone system: grid=%dx%dx%d, voxel_size=%f, hash_size=%zu",
             system->grid_dims.x, system->grid_dims.y, system->grid_dims.z,
             config->voxel_size, system->hash_size);

    return system;
}

void nimcp_pheromone_destroy(nimcp_pheromone_system_t* system) {
    if (!system) return;

    LOG_DEBUG("Destroying pheromone system");

    /* Free all voxel entries */
    if (system->grid_hash) {
        for (size_t i = 0; i < system->hash_size; i++) {
            voxel_entry_t* entry = system->grid_hash[i];
            while (entry) {
                voxel_entry_t* next = entry->next;
                nimcp_free(entry);
                entry = next;
            }
        }
        nimcp_free(system->grid_hash);
    }

    nimcp_platform_mutex_destroy(&system->mutex);
    nimcp_free(system);
}

nimcp_result_t nimcp_pheromone_reset(nimcp_pheromone_system_t* system) {
    NIMCP_CHECK_THROW(system, NIMCP_INVALID_PARAM, "pheromone system is NULL");

    nimcp_platform_mutex_lock(&system->mutex);

    /* Free all voxel entries */
    for (size_t i = 0; i < system->hash_size; i++) {
        voxel_entry_t* entry = system->grid_hash[i];
        while (entry) {
            voxel_entry_t* next = entry->next;
            nimcp_free(entry);
            entry = next;
        }
        system->grid_hash[i] = NULL;
    }

    /* Reset statistics */
    memset(&system->stats, 0, sizeof(nimcp_pheromone_stats_t));
    system->active_voxels = 0;

    nimcp_platform_mutex_unlock(&system->mutex);

    LOG_INFO("Reset pheromone system");
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Pheromone Deposit Functions
 * ============================================================================ */

nimcp_result_t nimcp_pheromone_deposit(
    nimcp_pheromone_system_t* system,
    const nimcp_position3d_t* position,
    nimcp_pheromone_type_t type,
    float amount
) {
    return nimcp_pheromone_deposit_modified(system, position, type, amount, 1.0F);
}

nimcp_result_t nimcp_pheromone_deposit_modified(
    nimcp_pheromone_system_t* system,
    const nimcp_position3d_t* position,
    nimcp_pheromone_type_t type,
    float amount,
    float env_modifier
) {
    NIMCP_CHECK_THROW(system, NIMCP_INVALID_PARAM, "pheromone system is NULL");
    NIMCP_CHECK_THROW(position, NIMCP_INVALID_PARAM, "position pointer is NULL");

    if (type >= PHEROMONE_TYPE_COUNT) {
        LOG_ERROR("Invalid pheromone type: %d", type);
        return NIMCP_INVALID_PARAM;
    }

    if (amount <= 0.0F) {
        LOG_ERROR("Invalid deposit amount: %f", amount);
        return NIMCP_INVALID_PARAM;
    }

    if (env_modifier < 0.0F || env_modifier > 1.0F) {
        LOG_ERROR("Invalid environmental modifier: %f", env_modifier);
        return NIMCP_INVALID_PARAM;
    }

    /* Validate position */
    if (!validate_position(system, position)) {
        LOG_ERROR("Position out of bounds: (%f, %f, %f)",
                  position->x, position->y, position->z);
        return NIMCP_INVALID_PARAM;
    }

    /* Security validation */
    if (system->bbb) {
        bbb_validation_result_t validation_result = {0};
        if (!bbb_validate_input(system->bbb, position,
                               sizeof(nimcp_position3d_t), &validation_result)) {
            LOG_ERROR("BBB rejected position input");
            return NIMCP_PERMISSION_DENIED;
        }
    }

    nimcp_platform_mutex_lock(&system->mutex);

    /* Get grid index */
    grid_index_t idx = position_to_grid(system, position);

    /* Get or create voxel */
    voxel_entry_t* entry = get_or_create_voxel(system, &idx);
    if (!entry) {
        nimcp_platform_mutex_unlock(&system->mutex);
        LOG_ERROR("Failed to get/create voxel");
        return NIMCP_NO_MEMORY;
    }

    /* Update voxel */
    float old_concentration = entry->voxel.concentration[type];
    entry->voxel.concentration[type] = fminf(
        entry->voxel.concentration[type] + amount,
        system->config.max_concentration
    );
    entry->voxel.environmental_modifier = env_modifier;
    entry->voxel.last_update = nimcp_time_get_us() / 1000;

    /* Update statistics */
    system->stats.total_deposits++;

    nimcp_platform_mutex_unlock(&system->mutex);

    LOG_DEBUG("Deposited pheromone type=%d amount=%f at (%f,%f,%f) [%d,%d,%d]",
              type, amount, position->x, position->y, position->z,
              idx.x, idx.y, idx.z);

    /* Broadcast update if enabled */
    if (system->config.enable_bio_async && system->bio_async_enabled) {
        nimcp_pheromone_broadcast_update(system, position, type,
                                        entry->voxel.concentration[type]);
    }

    /* Security audit */
    if (system->bbb) {
        bbb_audit_log(BBB_AUDIT_INFO, "pheromone", "pheromone_deposit",
                      "Deposited pheromone type=%d", type);
    }

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_pheromone_reinforce_path(
    nimcp_pheromone_system_t* system,
    const nimcp_position3d_t* path,
    size_t path_length,
    nimcp_pheromone_type_t type,
    float success_factor
) {
    NIMCP_CHECK_THROW(system, NIMCP_INVALID_PARAM, "pheromone system is NULL");
    NIMCP_CHECK_THROW(path, NIMCP_INVALID_PARAM, "path array is NULL");
    NIMCP_CHECK_THROW(path_length > 0, NIMCP_INVALID_PARAM, "path_length must be greater than 0");

    if (type >= PHEROMONE_TYPE_COUNT) {
        return NIMCP_INVALID_PARAM;
    }

    if (success_factor <= 0.0F) {
        return NIMCP_INVALID_PARAM;
    }

    LOG_DEBUG("Reinforcing path of length %zu with factor %f",
              path_length, success_factor);

    float reinforced_amount = system->config.deposit_amount *
                             system->config.reinforcement_factor *
                             success_factor;

    /* Deposit pheromone along entire path */
    for (size_t i = 0; i < path_length; i++) {
        nimcp_result_t result = nimcp_pheromone_deposit(
            system, &path[i], type, reinforced_amount);
        if (result != NIMCP_SUCCESS) {
            LOG_WARN("Failed to reinforce path at position %zu", i);
            /* Continue with remaining path */
        }
    }

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Pheromone Query Functions
 * ============================================================================ */

nimcp_result_t nimcp_pheromone_get_concentration(
    nimcp_pheromone_system_t* system,
    const nimcp_position3d_t* position,
    nimcp_pheromone_type_t type,
    float* out_concentration
) {
    NIMCP_CHECK_THROW(system, NIMCP_INVALID_PARAM, "pheromone system is NULL");
    NIMCP_CHECK_THROW(position, NIMCP_INVALID_PARAM, "position pointer is NULL");
    NIMCP_CHECK_THROW(out_concentration, NIMCP_INVALID_PARAM, "out_concentration pointer is NULL");

    if (type >= PHEROMONE_TYPE_COUNT) {
        return NIMCP_INVALID_PARAM;
    }

    if (!validate_position(system, position)) {
        *out_concentration = 0.0F;
        return NIMCP_SUCCESS; /* Out of bounds = no pheromone */
    }

    nimcp_platform_mutex_lock(&system->mutex);

    grid_index_t idx = position_to_grid(system, position);
    const voxel_entry_t* entry = get_voxel(system, &idx);

    if (entry) {
        *out_concentration = entry->voxel.concentration[type];
    } else {
        *out_concentration = 0.0F;
    }

    system->stats.total_queries++;

    nimcp_platform_mutex_unlock(&system->mutex);

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_pheromone_get_gradient(
    nimcp_pheromone_system_t* system,
    const nimcp_position3d_t* position,
    nimcp_pheromone_type_t type,
    nimcp_pheromone_gradient_t* out_gradient
) {
    NIMCP_CHECK_THROW(system, NIMCP_INVALID_PARAM, "pheromone system is NULL");
    NIMCP_CHECK_THROW(position, NIMCP_INVALID_PARAM, "position pointer is NULL");
    NIMCP_CHECK_THROW(out_gradient, NIMCP_INVALID_PARAM, "out_gradient pointer is NULL");

    if (type >= PHEROMONE_TYPE_COUNT) {
        return NIMCP_INVALID_PARAM;
    }

    if (!validate_position(system, position)) {
        return NIMCP_INVALID_PARAM;
    }

    memset(out_gradient, 0, sizeof(nimcp_pheromone_gradient_t));
    out_gradient->type = type;

    /* Get concentration at current position */
    float center_conc;
    nimcp_pheromone_get_concentration(system, position, type, &center_conc);
    out_gradient->concentration = center_conc;

    /* Sample 6 neighbors (±x, ±y, ±z) */
    float sample_dist = system->config.gradient_sample_distance;
    nimcp_position3d_t samples[6] = {
        {position->x + sample_dist, position->y, position->z},
        {position->x - sample_dist, position->y, position->z},
        {position->x, position->y + sample_dist, position->z},
        {position->x, position->y - sample_dist, position->z},
        {position->x, position->y, position->z + sample_dist},
        {position->x, position->y, position->z - sample_dist}
    };

    float concentrations[6];
    for (int i = 0; i < 6; i++) {
        nimcp_pheromone_get_concentration(system, &samples[i], type,
                                         &concentrations[i]);
    }

    /* Calculate gradient direction */
    nimcp_position3d_t gradient = {0, 0, 0};
    gradient.x = concentrations[0] - concentrations[1];
    gradient.y = concentrations[2] - concentrations[3];
    gradient.z = concentrations[4] - concentrations[5];

    out_gradient->magnitude = sqrtf(gradient.x * gradient.x +
                                   gradient.y * gradient.y +
                                   gradient.z * gradient.z);

    if (out_gradient->magnitude > EPSILON) {
        out_gradient->direction = gradient;
        normalize_vector(&out_gradient->direction);
    }

    LOG_DEBUG("Gradient at (%f,%f,%f): mag=%f dir=(%f,%f,%f)",
              position->x, position->y, position->z,
              out_gradient->magnitude,
              out_gradient->direction.x, out_gradient->direction.y,
              out_gradient->direction.z);

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_pheromone_query_radius(
    nimcp_pheromone_system_t* system,
    const nimcp_position3d_t* center,
    float radius,
    nimcp_pheromone_type_t type,
    nimcp_pheromone_trail_t* out_trails,
    size_t max_trails,
    size_t* out_count
) {
    NIMCP_CHECK_THROW(system, NIMCP_INVALID_PARAM, "pheromone system is NULL");
    NIMCP_CHECK_THROW(center, NIMCP_INVALID_PARAM, "center position is NULL");
    NIMCP_CHECK_THROW(out_trails, NIMCP_INVALID_PARAM, "out_trails array is NULL");
    NIMCP_CHECK_THROW(out_count, NIMCP_INVALID_PARAM, "out_count pointer is NULL");
    NIMCP_CHECK_THROW(max_trails > 0, NIMCP_INVALID_PARAM, "max_trails must be greater than 0");

    if (radius <= 0.0F) {
        return NIMCP_INVALID_PARAM;
    }

    *out_count = 0;

    nimcp_platform_mutex_lock(&system->mutex);

    /* Calculate grid bounds for radius */
    grid_index_t min_idx = position_to_grid(system, &(nimcp_position3d_t){
        center->x - radius, center->y - radius, center->z - radius
    });
    grid_index_t max_idx = position_to_grid(system, &(nimcp_position3d_t){
        center->x + radius, center->y + radius, center->z + radius
    });

    /* Clamp to grid bounds */
    if (min_idx.x < 0) min_idx.x = 0;
    if (min_idx.y < 0) min_idx.y = 0;
    if (min_idx.z < 0) min_idx.z = 0;
    if (max_idx.x >= system->grid_dims.x) max_idx.x = system->grid_dims.x - 1;
    if (max_idx.y >= system->grid_dims.y) max_idx.y = system->grid_dims.y - 1;
    if (max_idx.z >= system->grid_dims.z) max_idx.z = system->grid_dims.z - 1;

    /* Search voxels in range */
    for (int32_t x = min_idx.x; x <= max_idx.x; x++) {
        for (int32_t y = min_idx.y; y <= max_idx.y; y++) {
            for (int32_t z = min_idx.z; z <= max_idx.z; z++) {
                grid_index_t idx = {x, y, z};
                const voxel_entry_t* entry = get_voxel(system, &idx);

                if (!entry) continue;

                /* Check each pheromone type */
                for (int t = 0; t < PHEROMONE_TYPE_COUNT; t++) {
                    /* Skip if type filter specified and doesn't match */
                    if (type != PHEROMONE_TYPE_COUNT && t != type) {
                        continue;
                    }

                    float conc = entry->voxel.concentration[t];
                    if (conc < system->config.detection_threshold) {
                        continue;
                    }

                    /* Check if within radius */
                    nimcp_position3d_t pos = grid_to_position(system, &idx);
                    float dist = position_distance(center, &pos);
                    if (dist > radius) {
                        continue;
                    }

                    /* Add to results */
                    if (*out_count < max_trails) {
                        out_trails[*out_count].position = pos;
                        out_trails[*out_count].type = (nimcp_pheromone_type_t)t;
                        out_trails[*out_count].concentration = conc;
                        out_trails[*out_count].timestamp = entry->voxel.last_update;
                        (*out_count)++;
                    } else {
                        /* Buffer full */
                        nimcp_platform_mutex_unlock(&system->mutex);
                        return NIMCP_SUCCESS;
                    }
                }
            }
        }
    }

    nimcp_platform_mutex_unlock(&system->mutex);

    LOG_DEBUG("Found %zu pheromone trails in radius %f around (%f,%f,%f)",
              *out_count, radius, center->x, center->y, center->z);

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_pheromone_plan_path(
    nimcp_pheromone_system_t* system,
    const nimcp_position3d_t* start,
    nimcp_pheromone_type_t type,
    size_t max_steps,
    nimcp_position3d_t* out_path,
    size_t* out_path_length
) {
    NIMCP_CHECK_THROW(system, NIMCP_INVALID_PARAM, "pheromone system is NULL");
    NIMCP_CHECK_THROW(start, NIMCP_INVALID_PARAM, "start position is NULL");
    NIMCP_CHECK_THROW(out_path, NIMCP_INVALID_PARAM, "out_path array is NULL");
    NIMCP_CHECK_THROW(out_path_length, NIMCP_INVALID_PARAM, "out_path_length pointer is NULL");
    NIMCP_CHECK_THROW(max_steps > 0, NIMCP_INVALID_PARAM, "max_steps must be greater than 0");

    if (type >= PHEROMONE_TYPE_COUNT) {
        return NIMCP_INVALID_PARAM;
    }

    *out_path_length = 0;

    nimcp_position3d_t current = *start;
    float step_size = system->config.gradient_sample_distance;

    for (size_t i = 0; i < max_steps; i++) {
        /* Add current position to path */
        out_path[*out_path_length] = current;
        (*out_path_length)++;

        /* Get gradient at current position */
        nimcp_pheromone_gradient_t gradient;
        nimcp_result_t result = nimcp_pheromone_get_gradient(
            system, &current, type, &gradient);
        if (result != NIMCP_SUCCESS) {
            break;
        }

        /* Stop if no gradient */
        if (gradient.magnitude < EPSILON) {
            LOG_DEBUG("Path planning stopped: no gradient");
            break;
        }

        /* Move in gradient direction */
        current.x += gradient.direction.x * step_size;
        current.y += gradient.direction.y * step_size;
        current.z += gradient.direction.z * step_size;

        /* Check if out of bounds */
        if (!validate_position(system, &current)) {
            LOG_DEBUG("Path planning stopped: out of bounds");
            break;
        }
    }

    LOG_DEBUG("Planned path with %zu steps", *out_path_length);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Update and Maintenance Functions
 * ============================================================================ */

nimcp_result_t nimcp_pheromone_update(
    nimcp_pheromone_system_t* system,
    uint64_t delta_time_ms
) {
    NIMCP_CHECK_THROW(system, NIMCP_INVALID_PARAM, "pheromone system is NULL");

    if (delta_time_ms == 0) {
        return NIMCP_SUCCESS;
    }

    float delta_time_s = delta_time_ms / 1000.0F;

    nimcp_platform_mutex_lock(&system->mutex);

    uint64_t voxels_processed = 0;
    uint64_t voxels_cleared = 0;

    /* Update all voxels */
    for (size_t i = 0; i < system->hash_size; i++) {
        voxel_entry_t** entry_ptr = &system->grid_hash[i];

        while (*entry_ptr) {
            voxel_entry_t* entry = *entry_ptr;
            bool has_pheromone = false;

            /* Decay all pheromone types */
            for (int t = 0; t < PHEROMONE_TYPE_COUNT; t++) {
                if (entry->voxel.concentration[t] > EPSILON) {
                    entry->voxel.concentration[t] = apply_decay(
                        entry->voxel.concentration[t],
                        system->config.decay_rates[t],
                        system->config.evaporation_rate,
                        entry->voxel.environmental_modifier,
                        delta_time_s
                    );

                    if (entry->voxel.concentration[t] >
                        system->config.detection_threshold) {
                        has_pheromone = true;
                    }
                }
            }

            voxels_processed++;

            /* Remove voxel if all pheromones below threshold */
            if (!has_pheromone) {
                *entry_ptr = entry->next;
                nimcp_free(entry);
                system->active_voxels--;
                voxels_cleared++;
            } else {
                entry_ptr = &entry->next;
            }
        }
    }

    system->stats.last_decay_time = nimcp_time_get_us() / 1000;

    nimcp_platform_mutex_unlock(&system->mutex);

    LOG_DEBUG("Updated pheromones: processed=%lu cleared=%lu active=%zu dt=%f",
              voxels_processed, voxels_cleared, system->active_voxels, delta_time_s);

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_pheromone_set_environment(
    nimcp_pheromone_system_t* system,
    const nimcp_position3d_t* center,
    float radius,
    float modifier
) {
    NIMCP_CHECK_THROW(system, NIMCP_INVALID_PARAM, "pheromone system is NULL");
    NIMCP_CHECK_THROW(center, NIMCP_INVALID_PARAM, "center position is NULL");

    if (radius <= 0.0F || modifier < 0.0F || modifier > 1.0F) {
        return NIMCP_INVALID_PARAM;
    }

    nimcp_platform_mutex_lock(&system->mutex);

    /* Calculate grid bounds */
    grid_index_t min_idx = position_to_grid(system, &(nimcp_position3d_t){
        center->x - radius, center->y - radius, center->z - radius
    });
    grid_index_t max_idx = position_to_grid(system, &(nimcp_position3d_t){
        center->x + radius, center->y + radius, center->z + radius
    });

    /* Clamp to grid bounds */
    if (min_idx.x < 0) min_idx.x = 0;
    if (min_idx.y < 0) min_idx.y = 0;
    if (min_idx.z < 0) min_idx.z = 0;
    if (max_idx.x >= system->grid_dims.x) max_idx.x = system->grid_dims.x - 1;
    if (max_idx.y >= system->grid_dims.y) max_idx.y = system->grid_dims.y - 1;
    if (max_idx.z >= system->grid_dims.z) max_idx.z = system->grid_dims.z - 1;

    /* Set environmental modifier for voxels in range */
    size_t count = 0;
    for (int32_t x = min_idx.x; x <= max_idx.x; x++) {
        for (int32_t y = min_idx.y; y <= max_idx.y; y++) {
            for (int32_t z = min_idx.z; z <= max_idx.z; z++) {
                grid_index_t idx = {x, y, z};
                nimcp_position3d_t pos = grid_to_position(system, &idx);

                float dist = position_distance(center, &pos);
                if (dist <= radius) {
                    voxel_entry_t* entry = get_or_create_voxel(system, &idx);
                    if (entry) {
                        entry->voxel.environmental_modifier = modifier;
                        count++;
                    }
                }
            }
        }
    }

    nimcp_platform_mutex_unlock(&system->mutex);

    LOG_INFO("Set environmental modifier %f for %zu voxels in radius %f",
             modifier, count, radius);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Bio-Async Integration Functions
 * ============================================================================ */

nimcp_result_t nimcp_pheromone_register_bioasync(
    nimcp_pheromone_system_t* system,
    bio_router_t* router
) {
    NIMCP_CHECK_THROW(system, NIMCP_INVALID_PARAM, "pheromone system is NULL");

    (void)router;  /* Not used - we register directly with bio_router_register_module */

    /* Create module info */
    bio_module_info_t bio_info = {
        .module_id = 0,  /* Auto-assign */
        .module_name = "pheromone",
        .inbox_capacity = NIMCP_INBOX_CAPACITY_MEDIUM,
        .user_data = system
    };

    /* Register with bio-router */
    system->bio_ctx = bio_router_register_module(&bio_info);
    if (system->bio_ctx) {
        system->bio_async_enabled = true;
        system->last_broadcast_time = nimcp_time_get_ms();
        LOG_INFO("Registered pheromone system with bio-async router");
        return NIMCP_SUCCESS;
    }

    LOG_WARN("Failed to register pheromone system with bio-async router");
    return NIMCP_ERROR;
}

nimcp_result_t nimcp_pheromone_broadcast_update(
    nimcp_pheromone_system_t* system,
    const nimcp_position3d_t* position,
    nimcp_pheromone_type_t type,
    float concentration
) {
    NIMCP_CHECK_THROW(system, NIMCP_INVALID_PARAM, "pheromone system is NULL");
    NIMCP_CHECK_THROW(position, NIMCP_INVALID_PARAM, "position pointer is NULL");

    if (!system->bio_async_enabled || !system->bio_ctx) {
        return NIMCP_SUCCESS; /* Not registered, skip broadcast */
    }

    /* Check broadcast interval */
    uint64_t current_time = nimcp_time_get_ms();
    if (current_time - system->last_broadcast_time <
        system->config.broadcast_interval_ms) {
        return NIMCP_SUCCESS; /* Too soon */
    }

    /* Create bio-async message with embedded header */
    bio_msg_pheromone_t msg = {0};
    bio_msg_init_header(&msg.header, BIOMSG_PHEROMONE_UPDATE,
                        bio_module_context_get_id(system->bio_ctx), 0, sizeof(msg));
    msg.header.flags |= BIO_MSG_FLAG_BROADCAST;
    msg.header.timestamp_us = current_time * 1000;  /* Convert ms to us */
    msg.position = *position;
    msg.type = type;
    msg.concentration = concentration;

    /* Broadcast via bio-router */
    bio_router_broadcast(system->bio_ctx, &msg, sizeof(msg));

    system->last_broadcast_time = current_time;
    system->stats.broadcasts_sent++;

    LOG_DEBUG("Broadcast pheromone update: type=%d pos=(%f,%f,%f) conc=%f",
              type, position->x, position->y, position->z, concentration);

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_pheromone_handle_message(
    nimcp_pheromone_system_t* system,
    const void* message,
    size_t msg_size
) {
    NIMCP_CHECK_THROW(system, NIMCP_INVALID_PARAM, "pheromone system is NULL");
    NIMCP_CHECK_THROW(message, NIMCP_INVALID_PARAM, "message pointer is NULL");

    /* Cast to our message type (header is first field) */
    const bio_msg_pheromone_t* msg = (const bio_msg_pheromone_t*)message;

    if (msg->header.type != BIOMSG_PHEROMONE_UPDATE &&
        msg->header.type != BIOMSG_PHEROMONE_DEPOSIT) {
        return NIMCP_SUCCESS; /* Not for us */
    }

    if (msg_size < sizeof(bio_msg_pheromone_t)) {
        LOG_ERROR("Invalid pheromone message size: %zu", msg_size);
        return NIMCP_INVALID_PARAM;
    }

    /* Update local pheromone map */
    nimcp_result_t result = nimcp_pheromone_deposit(
        system, &msg->position, msg->type, msg->concentration);

    if (result == NIMCP_SUCCESS) {
        LOG_DEBUG("Handled pheromone message from swarm");
    }

    return result;
}

/* ============================================================================
 * Utility and Statistics Functions
 * ============================================================================ */

nimcp_result_t nimcp_pheromone_get_stats(
    nimcp_pheromone_system_t* system,
    nimcp_pheromone_stats_t* out_stats
) {
    NIMCP_CHECK_THROW(system, NIMCP_INVALID_PARAM, "pheromone system is NULL");
    NIMCP_CHECK_THROW(out_stats, NIMCP_INVALID_PARAM, "out_stats pointer is NULL");

    nimcp_platform_mutex_lock(&system->mutex);

    /* Copy statistics */
    *out_stats = system->stats;
    out_stats->active_voxels = system->active_voxels;

    /* Calculate average concentrations */
    memset(out_stats->avg_concentration, 0, sizeof(out_stats->avg_concentration));

    if (system->active_voxels > 0) {
        for (size_t i = 0; i < system->hash_size; i++) {
            voxel_entry_t* entry = system->grid_hash[i];
            while (entry) {
                for (int t = 0; t < PHEROMONE_TYPE_COUNT; t++) {
                    out_stats->avg_concentration[t] +=
                        entry->voxel.concentration[t];
                }
                entry = entry->next;
            }
        }

        for (int t = 0; t < PHEROMONE_TYPE_COUNT; t++) {
            out_stats->avg_concentration[t] /= system->active_voxels;
        }
    }

    nimcp_platform_mutex_unlock(&system->mutex);

    return NIMCP_SUCCESS;
}

const char* nimcp_pheromone_type_name(nimcp_pheromone_type_t type) {
    switch (type) {
        case PHEROMONE_DANGER: return "DANGER";
        case PHEROMONE_RESOURCE: return "RESOURCE";
        case PHEROMONE_PATH: return "PATH";
        case PHEROMONE_AVOID: return "AVOID";
        case PHEROMONE_RALLY: return "RALLY";
        case PHEROMONE_TARGET: return "TARGET";
        default: return "UNKNOWN";
    }
}

/* ============================================================================
 * KG Self-Awareness Integration
 * ============================================================================ */

/**
 * @brief Query knowledge graph for module self-knowledge
 *
 * WHAT: Introspect module identity from knowledge graph
 * WHY:  Enable self-awareness and runtime reflection
 * HOW:  Query KG for Swarm_Pheromone entity and its relations
 *
 * @param kg Knowledge graph reader
 * @return 1 if self-knowledge found, 0 otherwise
 */
int swarm_pheromone_query_self_knowledge(kg_reader_t* kg)
{
    if (!kg) return 0;

    const kg_entity_t* self = kg_reader_get_entity(kg, "Swarm_Pheromone");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            LOG_DEBUG("Pheromone self-knowledge: %s", self->observations[i]);
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Swarm_Pheromone");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Swarm_Pheromone");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}
