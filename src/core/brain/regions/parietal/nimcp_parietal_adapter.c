/**
 * @file nimcp_parietal_adapter.c
 * @brief Implementation of parietal cortex brain adapter
 *
 * WHAT: Unified adapter connecting parietal cortex sub-modules to the brain system
 * WHY:  Enable seamless integration with cognitive layers, training, and event system
 * HOW:  Orchestrates somatosensory, spatial attention, and sensorimotor processors
 *
 * NAMING CONVENTION:
 * All types and functions are prefixed with "parietal_cortex_" to distinguish
 * from the cognitive parietal lobe module (nimcp_parietal.h).
 *
 * @version Phase PC1: Parietal Cortex Brain Integration
 * @date 2025-12-30
 */

#include "core/brain/regions/parietal/nimcp_parietal_adapter.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/memory/nimcp_memory_pool.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(parietal_adapter, MESH_ADAPTER_CATEGORY_COGNITIVE)


/*=============================================================================
 * LOGGING MODULE IDENTIFIER
 *===========================================================================*/

#define PARIETAL_CORTEX_LOG_MODULE "PARIETAL_CORTEX"

/*=============================================================================
 * INTERNAL STRUCTURES
 *===========================================================================*/

/**
 * @brief Somatotopic map entry
 */
typedef struct {
    uint32_t region_id;              /**< Body region identifier */
    float activation;                /**< Current activation [0, 1] */
    float receptive_field_size;      /**< RF size in mm */
    parietal_cortex_position_t center; /**< RF center position */
    double last_update_ms;           /**< Last update timestamp */
} parietal_cortex_somatotopic_entry_t;

/**
 * @brief Somatosensory processor internal structure
 */
struct parietal_cortex_somatosensory_processor {
    parietal_cortex_somatotopic_entry_t* somatotopic_map; /**< Body region map */
    uint32_t map_capacity;               /**< Maximum regions */
    uint32_t map_count;                  /**< Active regions */

    /* Input buffer */
    parietal_cortex_somatosensory_input_t* input_buffer;
    uint32_t input_capacity;
    uint32_t input_count;

    /* Processing state */
    bool enable_two_point_disc;
    float min_discrimination_mm;         /**< Minimum 2-point distance */
};

/**
 * @brief Spatial attention processor internal structure
 */
struct parietal_cortex_spatial_attention_processor {
    parietal_cortex_spatial_target_t* targets;  /**< Tracked targets */
    uint32_t target_capacity;            /**< Maximum targets */
    uint32_t target_count;               /**< Current target count */

    /* Attention map (8x8 grid) */
    float attention_map[64];             /**< Spatial attention distribution */
    parietal_cortex_position_t focus_center; /**< Current focus center */
    float focus_sigma;                   /**< Attention spread */

    /* Hemispatial attention */
    float left_hemifield_gain;           /**< Left hemisphere attention */
    float right_hemifield_gain;          /**< Right hemisphere attention */

    /* Covert attention */
    bool covert_mode;                    /**< Covert attention active */
    double last_shift_ms;                /**< Last attention shift */
};

/**
 * @brief Sensorimotor integrator internal structure
 */
struct parietal_cortex_sensorimotor_integrator {
    parietal_cortex_motor_plan_t* plan_queue;   /**< Motor plan queue */
    uint32_t plan_capacity;              /**< Maximum plans */
    uint32_t plan_count;                 /**< Current plans */
    uint32_t plan_head;                  /**< Next plan to read */

    /* Coordinate transform matrices (6x6 for each pair) */
    float transform_weights[PARIETAL_CORTEX_SPATIAL_FRAME_COUNT][PARIETAL_CORTEX_SPATIAL_FRAME_COUNT][36];

    /* Reaching parameters */
    float reach_speed;                   /**< Default reach speed (m/s) */
    float grip_force;                    /**< Default grip force */

    /* Current hand positions */
    parietal_cortex_position_t left_hand_pos;
    parietal_cortex_position_t right_hand_pos;
};

/**
 * @brief Internal adapter structure
 */
struct parietal_adapter {
    /* Configuration */
    parietal_cortex_config_t config;

    /* Sub-modules */
    parietal_cortex_somatosensory_processor_t* somatosensory;
    parietal_cortex_spatial_attention_processor_t* spatial_attention;
    parietal_cortex_sensorimotor_integrator_t* sensorimotor;

    /* Callbacks */
    parietal_cortex_motor_callback_t motor_callback;
    void* motor_user_data;
    parietal_cortex_attention_callback_t attention_callback;
    void* attention_user_data;
    parietal_cortex_event_callback_t event_callback;
    void* event_user_data;

    /* State */
    parietal_cortex_status_t status;
    parietal_cortex_error_t last_error;
    double current_time_ms;

    /* Memory pool for hot-path allocations */
    memory_pool_t motor_plan_pool;

    /* Bio-async communication context */
    bio_module_context_t bio_ctx;
    nimcp_bio_channel_type_t default_channel;

    /* Statistics */
    parietal_cortex_stats_t stats;
};

/*=============================================================================
 * INTERNAL HELPERS
 *===========================================================================*/

/**
 * @brief Emit event to callback
 */
static void emit_event(parietal_adapter_t* adapter, uint32_t event_type, const void* data) {
    if (adapter->config.enable_events && adapter->event_callback) {
        adapter->event_callback(event_type, data, adapter->event_user_data);
    }
}

/**
 * @brief Set error state
 */
static void set_error(parietal_adapter_t* adapter, parietal_cortex_error_t error) {
    if (!adapter) return;
    adapter->last_error = error;
    if (error != PARIETAL_CORTEX_ERROR_NONE) {
        adapter->status = PARIETAL_CORTEX_STATUS_ERROR;
        LOG_ERROR("[%s] Error set: %d", PARIETAL_CORTEX_LOG_MODULE, error);
    }
}

/**
 * @brief Initialize transform matrices to identity
 */
static void init_transform_matrices(parietal_cortex_sensorimotor_integrator_t* integrator) {
    for (int i = 0; i < PARIETAL_CORTEX_SPATIAL_FRAME_COUNT; i++) {
        for (int j = 0; j < PARIETAL_CORTEX_SPATIAL_FRAME_COUNT; j++) {
            /* Initialize as identity-like (3x3 identity embedded in 6x6) */
            memset(integrator->transform_weights[i][j], 0, 36 * sizeof(float));
            integrator->transform_weights[i][j][0] = 1.0f;
            integrator->transform_weights[i][j][7] = 1.0f;
            integrator->transform_weights[i][j][14] = 1.0f;
        }
    }
}

/**
 * @brief Compute Gaussian attention weight
 */
static float gaussian_attention(const parietal_cortex_position_t* pos,
                                 const parietal_cortex_position_t* center,
                                 float sigma) {
    float dx = pos->x - center->x;
    float dy = pos->y - center->y;
    float dz = pos->z - center->z;
    float dist_sq = dx*dx + dy*dy + dz*dz;
    return expf(-dist_sq / (2.0f * sigma * sigma));
}

/**
 * @brief Update attention map from current focus
 */
static void update_attention_map(parietal_cortex_spatial_attention_processor_t* spatial) {
    float grid_step = 2.0f / 8.0f;  /* -1 to 1 range, 8 steps */

    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            parietal_cortex_position_t grid_pos = {
                .x = -1.0f + (x + 0.5f) * grid_step,
                .y = -1.0f + (y + 0.5f) * grid_step,
                .z = 0.0f
            };

            float weight = gaussian_attention(&grid_pos, &spatial->focus_center, spatial->focus_sigma);

            /* Apply hemispatial gain */
            if (grid_pos.x < 0) {
                weight *= spatial->left_hemifield_gain;
            } else {
                weight *= spatial->right_hemifield_gain;
            }

            spatial->attention_map[y * 8 + x] = weight;
        }
    }
}

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

parietal_cortex_config_t parietal_cortex_adapter_default_config(void) {
    parietal_cortex_config_t config;
    config.max_somatotopic_regions = PARIETAL_CORTEX_DEFAULT_MAX_SOMATOTOPIC_REGIONS;
    config.max_spatial_targets = PARIETAL_CORTEX_DEFAULT_MAX_SPATIAL_TARGETS;
    config.max_motor_plans = PARIETAL_CORTEX_DEFAULT_MAX_MOTOR_PLANS;
    config.receptive_field_size = PARIETAL_CORTEX_DEFAULT_RECEPTIVE_FIELD_SIZE;
    config.attention_resolution = PARIETAL_CORTEX_DEFAULT_ATTENTION_RESOLUTION;
    config.integration_window_ms = PARIETAL_CORTEX_DEFAULT_INTEGRATION_WINDOW_MS;

    config.enable_tactile_acuity = true;
    config.enable_proprioception = true;
    config.enable_two_point_discrimination = true;
    config.enable_covert_attention = true;
    config.enable_attention_shifting = true;
    config.enable_spatial_neglect_model = false;
    config.enable_reaching = true;
    config.enable_grasping = true;
    config.enable_tool_use = false;
    config.enable_coordinate_transforms = true;
    config.enable_events = true;
    config.enable_training = false;
    config.learning_rate = NIMCP_LEARNING_RATE_DEFAULT;

    config.enable_bio_async = true;
    config.default_channel = BIO_CHANNEL_ACETYLCHOLINE;

    return config;
}

parietal_adapter_t* parietal_cortex_adapter_create(const parietal_cortex_config_t* config) {
    LOG_INFO("[%s] Creating parietal cortex adapter", PARIETAL_CORTEX_LOG_MODULE);

    parietal_adapter_t* adapter = (parietal_adapter_t*)nimcp_calloc(1, sizeof(parietal_adapter_t));
    if (!adapter) {
        LOG_ERROR("[%s] Failed to allocate adapter memory", PARIETAL_CORTEX_LOG_MODULE);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "parietal_cortex_adapter_create: adapter is NULL");
        return NULL;
    }

    /* Set configuration */
    if (config) {
        adapter->config = *config;
        LOG_DEBUG("[%s] Using provided configuration", PARIETAL_CORTEX_LOG_MODULE);
    } else {
        adapter->config = parietal_cortex_adapter_default_config();
        LOG_DEBUG("[%s] Using default configuration", PARIETAL_CORTEX_LOG_MODULE);
    }

    /* Create somatosensory processor */
    LOG_DEBUG("[%s] Creating somatosensory processor", PARIETAL_CORTEX_LOG_MODULE);
    adapter->somatosensory = (parietal_cortex_somatosensory_processor_t*)nimcp_calloc(1, sizeof(parietal_cortex_somatosensory_processor_t));
    if (!adapter->somatosensory) {
        LOG_ERROR("[%s] Failed to create somatosensory processor", PARIETAL_CORTEX_LOG_MODULE);
        parietal_cortex_adapter_destroy(adapter);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "parietal_cortex_adapter_create: adapter->somatosensory is NULL");
        return NULL;
    }

    adapter->somatosensory->map_capacity = adapter->config.max_somatotopic_regions;
    adapter->somatosensory->somatotopic_map = (parietal_cortex_somatotopic_entry_t*)nimcp_calloc(
        adapter->somatosensory->map_capacity, sizeof(parietal_cortex_somatotopic_entry_t));
    adapter->somatosensory->input_capacity = adapter->config.max_somatotopic_regions * 2;
    adapter->somatosensory->input_buffer = (parietal_cortex_somatosensory_input_t*)nimcp_calloc(
        adapter->somatosensory->input_capacity, sizeof(parietal_cortex_somatosensory_input_t));
    adapter->somatosensory->enable_two_point_disc = adapter->config.enable_two_point_discrimination;
    adapter->somatosensory->min_discrimination_mm = 2.0f;  /* Fingertip acuity */

    if (!adapter->somatosensory->somatotopic_map || !adapter->somatosensory->input_buffer) {
        LOG_ERROR("[%s] Failed to allocate somatosensory buffers", PARIETAL_CORTEX_LOG_MODULE);
        parietal_cortex_adapter_destroy(adapter);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "parietal_cortex_adapter_create: required parameter is NULL (adapter->somatosensory->somatotopic_map, adapter->somatosensory->input_buffer)");
        return NULL;
    }

    /* Create spatial attention processor */
    LOG_DEBUG("[%s] Creating spatial attention processor", PARIETAL_CORTEX_LOG_MODULE);
    adapter->spatial_attention = (parietal_cortex_spatial_attention_processor_t*)nimcp_calloc(1, sizeof(parietal_cortex_spatial_attention_processor_t));
    if (!adapter->spatial_attention) {
        LOG_ERROR("[%s] Failed to create spatial attention processor", PARIETAL_CORTEX_LOG_MODULE);
        parietal_cortex_adapter_destroy(adapter);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "parietal_cortex_adapter_create: adapter->spatial_attention is NULL");
        return NULL;
    }

    adapter->spatial_attention->target_capacity = adapter->config.max_spatial_targets;
    adapter->spatial_attention->targets = (parietal_cortex_spatial_target_t*)nimcp_calloc(
        adapter->spatial_attention->target_capacity, sizeof(parietal_cortex_spatial_target_t));
    adapter->spatial_attention->focus_sigma = 0.3f;  /* Attention spread */
    adapter->spatial_attention->left_hemifield_gain = 1.0f;
    adapter->spatial_attention->right_hemifield_gain = 1.0f;

    if (!adapter->spatial_attention->targets) {
        LOG_ERROR("[%s] Failed to allocate spatial target buffer", PARIETAL_CORTEX_LOG_MODULE);
        parietal_cortex_adapter_destroy(adapter);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "parietal_cortex_adapter_create: adapter->spatial_attention->targets is NULL");
        return NULL;
    }

    /* Initialize attention map */
    memset(adapter->spatial_attention->attention_map, 0, sizeof(adapter->spatial_attention->attention_map));
    adapter->spatial_attention->focus_center.x = 0.0f;
    adapter->spatial_attention->focus_center.y = 0.0f;
    adapter->spatial_attention->focus_center.z = 0.0f;
    update_attention_map(adapter->spatial_attention);

    /* Create sensorimotor integrator */
    LOG_DEBUG("[%s] Creating sensorimotor integrator", PARIETAL_CORTEX_LOG_MODULE);
    adapter->sensorimotor = (parietal_cortex_sensorimotor_integrator_t*)nimcp_calloc(1, sizeof(parietal_cortex_sensorimotor_integrator_t));
    if (!adapter->sensorimotor) {
        LOG_ERROR("[%s] Failed to create sensorimotor integrator", PARIETAL_CORTEX_LOG_MODULE);
        parietal_cortex_adapter_destroy(adapter);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "parietal_cortex_adapter_create: adapter->sensorimotor is NULL");
        return NULL;
    }

    adapter->sensorimotor->plan_capacity = adapter->config.max_motor_plans;
    adapter->sensorimotor->plan_queue = (parietal_cortex_motor_plan_t*)nimcp_calloc(
        adapter->sensorimotor->plan_capacity, sizeof(parietal_cortex_motor_plan_t));
    adapter->sensorimotor->reach_speed = 0.5f;  /* 0.5 m/s default */
    adapter->sensorimotor->grip_force = 5.0f;   /* 5 N default */

    if (!adapter->sensorimotor->plan_queue) {
        LOG_ERROR("[%s] Failed to allocate motor plan buffer", PARIETAL_CORTEX_LOG_MODULE);
        parietal_cortex_adapter_destroy(adapter);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "parietal_cortex_adapter_create: adapter->sensorimotor->plan_queue is NULL");
        return NULL;
    }

    init_transform_matrices(adapter->sensorimotor);

    /* Initialize memory pool for hot-path allocations */
    LOG_DEBUG("[%s] Creating motor plan memory pool", PARIETAL_CORTEX_LOG_MODULE);
    memory_pool_config_t pool_config = {
        .block_size = adapter->config.max_motor_plans * sizeof(parietal_cortex_motor_plan_t),
        .num_blocks = 2,
        .alignment = 16,
        .enable_tracking = false,
        .enable_guard_pages = false
    };
    adapter->motor_plan_pool = memory_pool_create(&pool_config);
    if (!adapter->motor_plan_pool) {
        LOG_ERROR("[%s] Failed to create motor plan memory pool", PARIETAL_CORTEX_LOG_MODULE);
        parietal_cortex_adapter_destroy(adapter);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "parietal_cortex_adapter_create: adapter->motor_plan_pool is NULL");
        return NULL;
    }

    /* Initialize bio-async communication */
    adapter->bio_ctx = NULL;
    adapter->default_channel = adapter->config.default_channel;

    if (adapter->config.enable_bio_async && bio_router_is_initialized()) {
        LOG_DEBUG("[%s] Registering with bio-async router", PARIETAL_CORTEX_LOG_MODULE);

        bio_module_info_t bio_info = {
            .module_id = BIO_MODULE_PARIETAL_CORTEX,
            .module_name = "parietal_cortex",
            .inbox_capacity = 64,
            .user_data = adapter
        };

        adapter->bio_ctx = bio_router_register_module(&bio_info);
        if (adapter->bio_ctx) {
            LOG_INFO("[%s] Bio-async registration successful", PARIETAL_CORTEX_LOG_MODULE);
        } else {
            LOG_WARNING("[%s] Failed to register with bio-async router", PARIETAL_CORTEX_LOG_MODULE);
        }
    }

    /* Initialize state */
    adapter->status = PARIETAL_CORTEX_STATUS_IDLE;
    adapter->last_error = PARIETAL_CORTEX_ERROR_NONE;
    adapter->current_time_ms = 0.0;

    LOG_INFO("[%s] Parietal cortex adapter created successfully", PARIETAL_CORTEX_LOG_MODULE);
    return adapter;
}

void parietal_cortex_adapter_destroy(parietal_adapter_t* adapter) {
    if (!adapter) return;

    LOG_INFO("[%s] Destroying parietal cortex adapter", PARIETAL_CORTEX_LOG_MODULE);

    /* Unregister from bio-async router */
    if (adapter->bio_ctx) {
        LOG_DEBUG("[%s] Unregistering from bio-async router", PARIETAL_CORTEX_LOG_MODULE);
        bio_router_unregister_module(adapter->bio_ctx);
        adapter->bio_ctx = NULL;
    }

    /* Destroy somatosensory processor */
    if (adapter->somatosensory) {
        if (adapter->somatosensory->somatotopic_map) {
            nimcp_free(adapter->somatosensory->somatotopic_map);
        }
        if (adapter->somatosensory->input_buffer) {
            nimcp_free(adapter->somatosensory->input_buffer);
        }
        nimcp_free(adapter->somatosensory);
    }

    /* Destroy spatial attention processor */
    if (adapter->spatial_attention) {
        if (adapter->spatial_attention->targets) {
            nimcp_free(adapter->spatial_attention->targets);
        }
        nimcp_free(adapter->spatial_attention);
    }

    /* Destroy sensorimotor integrator */
    if (adapter->sensorimotor) {
        if (adapter->sensorimotor->plan_queue) {
            nimcp_free(adapter->sensorimotor->plan_queue);
        }
        nimcp_free(adapter->sensorimotor);
    }

    /* Destroy memory pool */
    if (adapter->motor_plan_pool) {
        memory_pool_destroy(adapter->motor_plan_pool);
    }

    LOG_DEBUG("[%s] Parietal cortex adapter destroyed", PARIETAL_CORTEX_LOG_MODULE);
    nimcp_free(adapter);
}

bool parietal_cortex_adapter_reset(parietal_adapter_t* adapter) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parietal_cortex_adapter_reset: adapter is NULL");
        return false;
    }

    LOG_DEBUG("[%s] Resetting adapter state", PARIETAL_CORTEX_LOG_MODULE);

    /* Reset somatosensory processor */
    if (adapter->somatosensory) {
        memset(adapter->somatosensory->somatotopic_map, 0,
               adapter->somatosensory->map_capacity * sizeof(parietal_cortex_somatotopic_entry_t));
        adapter->somatosensory->map_count = 0;
        adapter->somatosensory->input_count = 0;
    }

    /* Reset spatial attention processor */
    if (adapter->spatial_attention) {
        memset(adapter->spatial_attention->targets, 0,
               adapter->spatial_attention->target_capacity * sizeof(parietal_cortex_spatial_target_t));
        adapter->spatial_attention->target_count = 0;
        adapter->spatial_attention->focus_center.x = 0.0f;
        adapter->spatial_attention->focus_center.y = 0.0f;
        adapter->spatial_attention->focus_center.z = 0.0f;
        adapter->spatial_attention->left_hemifield_gain = 1.0f;
        adapter->spatial_attention->right_hemifield_gain = 1.0f;
        update_attention_map(adapter->spatial_attention);
    }

    /* Reset sensorimotor integrator */
    if (adapter->sensorimotor) {
        adapter->sensorimotor->plan_count = 0;
        adapter->sensorimotor->plan_head = 0;
    }

    /* Reset state */
    adapter->status = PARIETAL_CORTEX_STATUS_IDLE;
    adapter->last_error = PARIETAL_CORTEX_ERROR_NONE;

    LOG_DEBUG("[%s] Adapter reset complete", PARIETAL_CORTEX_LOG_MODULE);
    return true;
}

/*=============================================================================
 * SOMATOSENSORY PROCESSING
 *===========================================================================*/

bool parietal_cortex_add_somatosensory_input(parietal_adapter_t* adapter,
                                              const parietal_cortex_somatosensory_input_t* input) {
    if (!adapter || !input) {
        if (adapter) set_error(adapter, PARIETAL_CORTEX_ERROR_INVALID_INPUT);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "parietal_cortex_adapter_reset: validation failed");
        return false;
    }

    parietal_cortex_somatosensory_processor_t* soma = adapter->somatosensory;

    /* Add to input buffer */
    if (soma->input_count >= soma->input_capacity) {
        set_error(adapter, PARIETAL_CORTEX_ERROR_BUFFER_OVERFLOW);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "parietal_cortex_adapter_reset: capacity exceeded");
        return false;
    }

    soma->input_buffer[soma->input_count++] = *input;

    /* Update somatotopic map */
    bool found = false;
    for (uint32_t i = 0; i < soma->map_count; i++) {
        if (soma->somatotopic_map[i].region_id == input->body_region_id) {
            soma->somatotopic_map[i].activation = input->intensity;
            soma->somatotopic_map[i].last_update_ms = input->timestamp_ms;
            found = true;
            break;
        }
    }

    if (!found && soma->map_count < soma->map_capacity) {
        parietal_cortex_somatotopic_entry_t* entry = &soma->somatotopic_map[soma->map_count++];
        entry->region_id = input->body_region_id;
        entry->activation = input->intensity;
        entry->center = input->location;
        entry->receptive_field_size = (float)adapter->config.receptive_field_size;
        entry->last_update_ms = input->timestamp_ms;
    }

    adapter->status = PARIETAL_CORTEX_STATUS_SOMATOSENSORY;
    adapter->stats.somatosensory_samples++;

    return true;
}

float parietal_cortex_get_body_region_activation(const parietal_adapter_t* adapter,
                                                  uint32_t region_id) {
    if (!adapter || !adapter->somatosensory) return -1.0f;

    parietal_cortex_somatosensory_processor_t* soma = adapter->somatosensory;

    for (uint32_t i = 0; i < soma->map_count; i++) {
        if (soma->somatotopic_map[i].region_id == region_id) {
            return soma->somatotopic_map[i].activation;
        }
    }

    return 0.0f;  /* Not found = no activation */
}

bool parietal_cortex_two_point_discrimination(const parietal_adapter_t* adapter,
                                               uint32_t region_id,
                                               float separation_mm) {
    if (!adapter || !adapter->somatosensory) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parietal_cortex_adapter_reset: required parameter is NULL (adapter, adapter->somatosensory)");
        return false;
    }
    if (!adapter->somatosensory->enable_two_point_disc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parietal_cortex_adapter_reset: adapter->somatosensory->enable_two_point_disc is NULL");
        return false;
    }

    /* Find region and check if separation exceeds minimum */
    for (uint32_t i = 0; i < adapter->somatosensory->map_count; i++) {
        if (adapter->somatosensory->somatotopic_map[i].region_id == region_id) {
            /* Discrimination threshold varies by body region */
            /* Fingertip: ~2mm, Palm: ~10mm, Back: ~40mm */
            float threshold = adapter->somatosensory->min_discrimination_mm;
            return separation_mm >= threshold;
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "parietal_cortex_adapter_reset: validation failed");
    return false;
}

/*=============================================================================
 * SPATIAL ATTENTION AND NAVIGATION
 *===========================================================================*/

bool parietal_cortex_add_spatial_target(parietal_adapter_t* adapter,
                                         const parietal_cortex_spatial_target_t* target) {
    if (!adapter || !target) {
        if (adapter) set_error(adapter, PARIETAL_CORTEX_ERROR_INVALID_INPUT);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "parietal_cortex_adapter_reset: validation failed");
        return false;
    }

    parietal_cortex_spatial_attention_processor_t* spatial = adapter->spatial_attention;

    if (spatial->target_count >= spatial->target_capacity) {
        set_error(adapter, PARIETAL_CORTEX_ERROR_BUFFER_OVERFLOW);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "parietal_cortex_adapter_reset: capacity exceeded");
        return false;
    }

    spatial->targets[spatial->target_count++] = *target;
    adapter->stats.spatial_targets_tracked++;

    return true;
}

bool parietal_cortex_update_target_position(parietal_adapter_t* adapter,
                                             uint32_t target_id,
                                             const parietal_cortex_position_t* new_position) {
    if (!adapter || !new_position) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parietal_cortex_adapter_reset: required parameter is NULL (adapter, new_position)");
        return false;
    }

    parietal_cortex_spatial_attention_processor_t* spatial = adapter->spatial_attention;

    for (uint32_t i = 0; i < spatial->target_count; i++) {
        if (spatial->targets[i].target_id == target_id) {
            spatial->targets[i].position = *new_position;
            return true;
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "parietal_cortex_adapter_reset: validation failed");
    return false;  /* Target not found */
}

bool parietal_cortex_attend_to_location(parietal_adapter_t* adapter,
                                         uint32_t target_id,
                                         const parietal_cortex_position_t* position) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parietal_cortex_adapter_reset: adapter is NULL");
        return false;
    }

    parietal_cortex_spatial_attention_processor_t* spatial = adapter->spatial_attention;

    if (target_id != 0) {
        /* Find target and attend to it */
        for (uint32_t i = 0; i < spatial->target_count; i++) {
            if (spatial->targets[i].target_id == target_id) {
                spatial->focus_center = spatial->targets[i].position;
                spatial->targets[i].is_active = true;
                break;
            }
        }
    } else if (position) {
        spatial->focus_center = *position;
    } else {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "parietal_cortex_adapter_reset: validation failed");
        return false;
    }

    update_attention_map(spatial);
    adapter->status = PARIETAL_CORTEX_STATUS_SPATIAL_ATTENTION;
    adapter->stats.attention_shifts++;

    /* Invoke attention callback */
    if (adapter->attention_callback) {
        parietal_cortex_attention_result_t result;
        parietal_cortex_get_attention_map(adapter, &result);
        adapter->attention_callback(&result, adapter->attention_user_data);
    }

    return true;
}

bool parietal_cortex_covert_attention_shift(parietal_adapter_t* adapter,
                                             const parietal_cortex_position_t* new_focus,
                                             float transition_ms) {
    if (!adapter || !new_focus) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (adapter, new_focus)");
        return false;
    }
    if (!adapter->config.enable_covert_attention) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: adapter->config is NULL");
        return false;
    }

    parietal_cortex_spatial_attention_processor_t* spatial = adapter->spatial_attention;

    /* For simplicity, instant shift - could implement gradual transition */
    (void)transition_ms;

    spatial->focus_center = *new_focus;
    spatial->covert_mode = true;
    spatial->last_shift_ms = adapter->current_time_ms;

    update_attention_map(spatial);
    adapter->stats.attention_shifts++;

    return true;
}

bool parietal_cortex_get_attention_map(const parietal_adapter_t* adapter,
                                        parietal_cortex_attention_result_t* result) {
    if (!adapter || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (adapter, result)");
        return false;
    }

    const parietal_cortex_spatial_attention_processor_t* spatial = adapter->spatial_attention;

    memcpy(result->attention_map, spatial->attention_map, sizeof(result->attention_map));
    result->focus_center = spatial->focus_center;
    result->focus_spread = spatial->focus_sigma;
    result->left_hemifield_active = (spatial->left_hemifield_gain > 0.5f);
    result->right_hemifield_active = (spatial->right_hemifield_gain > 0.5f);
    result->num_attended = 0;

    /* Count attended targets */
    for (uint32_t i = 0; i < spatial->target_count; i++) {
        if (spatial->targets[i].is_active) {
            result->num_attended++;
        }
    }
    result->attended_targets = (parietal_cortex_spatial_target_t*)spatial->targets;

    return true;
}

bool parietal_cortex_transform_coordinates(parietal_adapter_t* adapter,
                                            const parietal_cortex_position_t* position,
                                            parietal_cortex_spatial_frame_t from_frame,
                                            parietal_cortex_spatial_frame_t to_frame,
                                            parietal_cortex_position_t* result) {
    if (!adapter || !position || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (adapter, position, result)");
        return false;
    }
    if (from_frame >= PARIETAL_CORTEX_SPATIAL_FRAME_COUNT || to_frame >= PARIETAL_CORTEX_SPATIAL_FRAME_COUNT) {
        set_error(adapter, PARIETAL_CORTEX_ERROR_COORDINATE_FAILURE);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "unknown: capacity exceeded");
        return false;
    }

    if (!adapter->config.enable_coordinate_transforms) {
        *result = *position;  /* Pass through unchanged */
        return true;
    }

    parietal_cortex_sensorimotor_integrator_t* motor = adapter->sensorimotor;
    const float* weights = motor->transform_weights[from_frame][to_frame];

    /* Apply linear transform (3x3 portion of 6x6 matrix) */
    result->x = weights[0] * position->x + weights[1] * position->y + weights[2] * position->z;
    result->y = weights[6] * position->x + weights[7] * position->y + weights[8] * position->z;
    result->z = weights[12] * position->x + weights[13] * position->y + weights[14] * position->z;

    adapter->status = PARIETAL_CORTEX_STATUS_COORDINATE_TRANSFORM;
    adapter->stats.coordinate_transforms++;

    return true;
}

/*=============================================================================
 * SENSORIMOTOR INTEGRATION
 *===========================================================================*/

bool parietal_cortex_plan_reach(parietal_adapter_t* adapter,
                                 const parietal_cortex_position_t* target_pos,
                                 uint8_t hand_id,
                                 parietal_cortex_motor_plan_t* plan) {
    if (!adapter || !target_pos || !plan) {
        if (adapter) set_error(adapter, PARIETAL_CORTEX_ERROR_INVALID_INPUT);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "unknown: validation failed");
        return false;
    }

    if (!adapter->config.enable_reaching) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: adapter->config is NULL");
        return false;
    }

    parietal_cortex_sensorimotor_integrator_t* motor = adapter->sensorimotor;

    memset(plan, 0, sizeof(parietal_cortex_motor_plan_t));
    plan->plan_id = adapter->stats.motor_plans_generated + 1;
    plan->start_pos = (hand_id == 0) ? motor->left_hand_pos : motor->right_hand_pos;
    plan->target_pos = *target_pos;
    plan->requires_grasp = false;

    /* Compute trajectory duration based on distance and speed */
    float dx = target_pos->x - plan->start_pos.x;
    float dy = target_pos->y - plan->start_pos.y;
    float dz = target_pos->z - plan->start_pos.z;
    float distance = sqrtf(dx*dx + dy*dy + dz*dz);
    plan->trajectory_duration_ms = (distance / motor->reach_speed) * 1000.0f;
    plan->confidence = 0.9f;

    /* Add to plan queue */
    if (motor->plan_count < motor->plan_capacity) {
        motor->plan_queue[motor->plan_count++] = *plan;
    }

    adapter->status = PARIETAL_CORTEX_STATUS_SENSORIMOTOR;
    adapter->stats.motor_plans_generated++;

    /* Invoke motor callback */
    if (adapter->motor_callback) {
        adapter->motor_callback(plan, adapter->motor_user_data);
    }

    return true;
}

bool parietal_cortex_plan_grasp(parietal_adapter_t* adapter,
                                 const parietal_cortex_position_t* target_pos,
                                 float object_size,
                                 parietal_cortex_motor_plan_t* plan) {
    if (!adapter || !target_pos || !plan) {
        if (adapter) set_error(adapter, PARIETAL_CORTEX_ERROR_INVALID_INPUT);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "unknown: validation failed");
        return false;
    }

    if (!adapter->config.enable_grasping) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: adapter->config is NULL");
        return false;
    }

    /* First plan reach to target */
    if (!parietal_cortex_plan_reach(adapter, target_pos, 1 /* right hand */, plan)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "unknown: parietal_cortex_plan_reach is NULL");
        return false;
    }

    /* Add grasp parameters */
    plan->requires_grasp = true;
    plan->grip_aperture = (uint8_t)(object_size * 255.0f);  /* Scale to 0-255 */
    plan->target_grip.w = 1.0f;  /* Identity orientation */
    plan->target_grip.x = 0.0f;
    plan->target_grip.y = 0.0f;
    plan->target_grip.z = 0.0f;

    /* Update the queued plan */
    if (adapter->sensorimotor->plan_count > 0) {
        adapter->sensorimotor->plan_queue[adapter->sensorimotor->plan_count - 1] = *plan;
    }

    return true;
}

bool parietal_cortex_process_integration(parietal_adapter_t* adapter,
                                          parietal_cortex_integration_result_t* result) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: adapter is NULL");
        return false;
    }

    parietal_cortex_integration_result_t local_result;
    memset(&local_result, 0, sizeof(local_result));

    /* Phase 1: Somatosensory processing summary */
    adapter->status = PARIETAL_CORTEX_STATUS_SOMATOSENSORY;
    local_result.has_touch_input = (adapter->somatosensory->input_count > 0);
    local_result.active_body_regions = adapter->somatosensory->map_count;

    /* Check for proprioceptive input */
    for (uint32_t i = 0; i < adapter->somatosensory->input_count; i++) {
        if (adapter->somatosensory->input_buffer[i].modality == PARIETAL_CORTEX_SOMATOSENSORY_PROPRIOCEPTION) {
            local_result.has_proprioceptive_input = true;
            break;
        }
    }

    /* Phase 2: Spatial attention summary */
    adapter->status = PARIETAL_CORTEX_STATUS_SPATIAL_ATTENTION;
    uint32_t attended = 0;
    for (uint32_t i = 0; i < adapter->spatial_attention->target_count; i++) {
        if (adapter->spatial_attention->targets[i].is_active) {
            attended++;
        }
    }
    local_result.attended_targets = attended;
    local_result.current_frame = PARIETAL_CORTEX_SPATIAL_FRAME_EGOCENTRIC;  /* Default */

    /* Phase 3: Motor planning summary */
    adapter->status = PARIETAL_CORTEX_STATUS_SENSORIMOTOR;
    local_result.motor_plan_count = adapter->sensorimotor->plan_count - adapter->sensorimotor->plan_head;
    local_result.ready_for_execution = (local_result.motor_plan_count > 0);
    local_result.integration_confidence = 0.85f;  /* Base confidence */

    /* Update statistics */
    adapter->stats.successful_integrations++;
    adapter->status = PARIETAL_CORTEX_STATUS_READY;

    if (result) *result = local_result;
    return true;
}

bool parietal_cortex_get_next_motor_plan(parietal_adapter_t* adapter,
                                          parietal_cortex_motor_plan_t* plan) {
    if (!adapter || !plan) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (adapter, plan)");
        return false;
    }

    parietal_cortex_sensorimotor_integrator_t* motor = adapter->sensorimotor;

    if (motor->plan_head >= motor->plan_count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "unknown: capacity exceeded");
        return false;  /* No more plans */
    }

    *plan = motor->plan_queue[motor->plan_head++];
    return true;
}

/*=============================================================================
 * CALLBACKS AND EVENT INTEGRATION
 *===========================================================================*/

bool parietal_cortex_set_motor_callback(parietal_adapter_t* adapter,
                                         parietal_cortex_motor_callback_t callback,
                                         void* user_data) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: adapter is NULL");
        return false;
    }
    adapter->motor_callback = callback;
    adapter->motor_user_data = user_data;
    return true;
}

bool parietal_cortex_set_attention_callback(parietal_adapter_t* adapter,
                                             parietal_cortex_attention_callback_t callback,
                                             void* user_data) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: adapter is NULL");
        return false;
    }
    adapter->attention_callback = callback;
    adapter->attention_user_data = user_data;
    return true;
}

bool parietal_cortex_set_event_callback(parietal_adapter_t* adapter,
                                         parietal_cortex_event_callback_t callback,
                                         void* user_data) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: adapter is NULL");
        return false;
    }
    adapter->event_callback = callback;
    adapter->event_user_data = user_data;
    return true;
}

/*=============================================================================
 * TRAINING INTERFACE
 *===========================================================================*/

bool parietal_cortex_train_transform(parietal_adapter_t* adapter,
                                      parietal_cortex_spatial_frame_t from_frame,
                                      parietal_cortex_spatial_frame_t to_frame,
                                      const parietal_cortex_position_t* input_pos,
                                      const parietal_cortex_position_t* target_pos,
                                      float learning_rate) {
    if (!adapter || !input_pos || !target_pos) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (adapter, input_pos, target_pos)");
        return false;
    }
    if (!adapter->config.enable_training) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: adapter->config is NULL");
        return false;
    }
    if (from_frame >= PARIETAL_CORTEX_SPATIAL_FRAME_COUNT || to_frame >= PARIETAL_CORTEX_SPATIAL_FRAME_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "unknown: capacity exceeded");
        return false;
    }

    if (learning_rate <= 0.0f) {
        learning_rate = adapter->config.learning_rate;
    }

    /* Compute current output */
    parietal_cortex_position_t current_output;
    parietal_cortex_transform_coordinates(adapter, input_pos, from_frame, to_frame, &current_output);

    /* Compute error */
    float error_x = target_pos->x - current_output.x;
    float error_y = target_pos->y - current_output.y;
    float error_z = target_pos->z - current_output.z;

    /* Simple gradient update on transform weights */
    float* weights = adapter->sensorimotor->transform_weights[from_frame][to_frame];
    weights[0] += learning_rate * error_x * input_pos->x;
    weights[1] += learning_rate * error_x * input_pos->y;
    weights[2] += learning_rate * error_x * input_pos->z;
    weights[6] += learning_rate * error_y * input_pos->x;
    weights[7] += learning_rate * error_y * input_pos->y;
    weights[8] += learning_rate * error_y * input_pos->z;
    weights[12] += learning_rate * error_z * input_pos->x;
    weights[13] += learning_rate * error_z * input_pos->y;
    weights[14] += learning_rate * error_z * input_pos->z;

    float loss = sqrtf(error_x*error_x + error_y*error_y + error_z*error_z);
    adapter->stats.training_iterations++;
    adapter->stats.training_loss = loss;

    return true;
}

bool parietal_cortex_train_reaching(parietal_adapter_t* adapter,
                                     const parietal_cortex_motor_plan_t* plan,
                                     const parietal_cortex_position_t* actual_endpoint,
                                     float learning_rate) {
    if (!adapter || !plan || !actual_endpoint) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (adapter, plan, actual_endpoint)");
        return false;
    }
    if (!adapter->config.enable_training) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: adapter->config is NULL");
        return false;
    }

    (void)learning_rate;  /* Would use for actual weight updates */

    /* Compute reaching error */
    float error_x = actual_endpoint->x - plan->target_pos.x;
    float error_y = actual_endpoint->y - plan->target_pos.y;
    float error_z = actual_endpoint->z - plan->target_pos.z;

    float loss = sqrtf(error_x*error_x + error_y*error_y + error_z*error_z);

    /* Would update inverse kinematics model here */
    /* For now, just track the error */
    adapter->stats.training_iterations++;
    adapter->stats.training_loss = loss;

    return true;
}

/*=============================================================================
 * STATUS AND DIAGNOSTICS
 *===========================================================================*/

parietal_cortex_status_t parietal_cortex_get_status(const parietal_adapter_t* adapter) {
    if (!adapter) return PARIETAL_CORTEX_STATUS_ERROR;
    return adapter->status;
}

parietal_cortex_error_t parietal_cortex_get_last_error(const parietal_adapter_t* adapter) {
    if (!adapter) return PARIETAL_CORTEX_ERROR_INTERNAL;
    return adapter->last_error;
}

const char* parietal_cortex_error_string(parietal_cortex_error_t error) {
    switch (error) {
        case PARIETAL_CORTEX_ERROR_NONE: return "No error";
        case PARIETAL_CORTEX_ERROR_INVALID_INPUT: return "Invalid input";
        case PARIETAL_CORTEX_ERROR_SOMATOSENSORY_FAILURE: return "Somatosensory processing failed";
        case PARIETAL_CORTEX_ERROR_SPATIAL_FAILURE: return "Spatial attention failed";
        case PARIETAL_CORTEX_ERROR_SENSORIMOTOR_FAILURE: return "Sensorimotor integration failed";
        case PARIETAL_CORTEX_ERROR_COORDINATE_FAILURE: return "Coordinate transform failed";
        case PARIETAL_CORTEX_ERROR_ATTENTION_FAILURE: return "Attention allocation failed";
        case PARIETAL_CORTEX_ERROR_BUFFER_OVERFLOW: return "Buffer overflow";
        case PARIETAL_CORTEX_ERROR_INTERNAL: return "Internal error";
        default: return "Unknown error";
    }
}

const char* parietal_cortex_status_string(parietal_cortex_status_t status) {
    switch (status) {
        case PARIETAL_CORTEX_STATUS_IDLE: return "Idle";
        case PARIETAL_CORTEX_STATUS_SOMATOSENSORY: return "Somatosensory processing";
        case PARIETAL_CORTEX_STATUS_SPATIAL_ATTENTION: return "Spatial attention";
        case PARIETAL_CORTEX_STATUS_SENSORIMOTOR: return "Sensorimotor integration";
        case PARIETAL_CORTEX_STATUS_COORDINATE_TRANSFORM: return "Coordinate transform";
        case PARIETAL_CORTEX_STATUS_READY: return "Ready";
        case PARIETAL_CORTEX_STATUS_ERROR: return "Error";
        default: return "Unknown";
    }
}

bool parietal_cortex_get_stats(const parietal_adapter_t* adapter, parietal_cortex_stats_t* stats) {
    if (!adapter || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parietal_cortex_get_stats: required parameter is NULL (adapter, stats)");
        return false;
    }
    *stats = adapter->stats;
    return true;
}

bool parietal_cortex_get_config(const parietal_adapter_t* adapter, parietal_cortex_config_t* config) {
    if (!adapter || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parietal_cortex_get_config: required parameter is NULL (adapter, config)");
        return false;
    }
    *config = adapter->config;
    return true;
}

/*=============================================================================
 * SUB-MODULE ACCESS
 *===========================================================================*/

parietal_cortex_somatosensory_processor_t* parietal_cortex_get_somatosensory_processor(parietal_adapter_t* adapter) {
    if (!adapter) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adapter is NULL");

        return NULL;

    }
    return adapter->somatosensory;
}

parietal_cortex_spatial_attention_processor_t* parietal_cortex_get_spatial_attention_processor(parietal_adapter_t* adapter) {
    if (!adapter) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adapter is NULL");

        return NULL;

    }
    return adapter->spatial_attention;
}

parietal_cortex_sensorimotor_integrator_t* parietal_cortex_get_sensorimotor_integrator(parietal_adapter_t* adapter) {
    if (!adapter) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adapter is NULL");

        return NULL;

    }
    return adapter->sensorimotor;
}

/*=============================================================================
 * BIO-ASYNC COMMUNICATION
 *===========================================================================*/

bio_module_context_t parietal_cortex_get_bio_context(parietal_adapter_t* adapter) {
    if (!adapter) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adapter is NULL");

        return NULL;

    }
    return adapter->bio_ctx;
}

uint32_t parietal_cortex_process_bio_messages(parietal_adapter_t* adapter, uint32_t max_messages) {
    if (!adapter || !adapter->bio_ctx) return 0;

    uint32_t processed = bio_router_process_inbox(adapter->bio_ctx, max_messages);
    if (processed > 0) {
        LOG_DEBUG("[%s] Processed %u bio-async messages", PARIETAL_CORTEX_LOG_MODULE, processed);
    }
    return processed;
}

nimcp_bio_future_t parietal_cortex_request_transform_async(
    parietal_adapter_t* adapter,
    const parietal_cortex_position_t* position,
    parietal_cortex_spatial_frame_t from_frame,
    parietal_cortex_spatial_frame_t to_frame) {

    if (!adapter || !adapter->bio_ctx || !position) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parietal_cortex_process_bio_messages: required parameter is NULL (adapter, adapter->bio_ctx, position)");
        return NULL;
    }

    /* Transform synchronously for now and return immediately */
    parietal_cortex_position_t result;
    parietal_cortex_transform_coordinates(adapter, position, from_frame, to_frame, &result);

    /* Would create async message in full implementation */
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "parietal_cortex_process_bio_messages: required parameter is NULL (adapter, adapter->bio_ctx, position)");
    return NULL;
}

nimcp_bio_future_t parietal_cortex_request_motor_plan_async(
    parietal_adapter_t* adapter,
    const parietal_cortex_position_t* target_pos,
    uint8_t action_type) {

    if (!adapter || !adapter->bio_ctx || !target_pos) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parietal_cortex_process_bio_messages: required parameter is NULL (adapter, adapter->bio_ctx, target_pos)");
        return NULL;
    }

    /* Plan synchronously for now */
    parietal_cortex_motor_plan_t plan;
    if (action_type == 0) {
        parietal_cortex_plan_reach(adapter, target_pos, 1, &plan);
    } else {
        parietal_cortex_plan_grasp(adapter, target_pos, 0.05f, &plan);
    }

    /* Would create async message in full implementation */
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "parietal_cortex_process_bio_messages: action_type is zero");
    return NULL;
}

nimcp_error_t parietal_cortex_broadcast_attention_shift(
    parietal_adapter_t* adapter,
    const parietal_cortex_attention_result_t* attention) {

    if (!adapter || !attention) {
        return NIMCP_BIO_ERROR_NOT_INITIALIZED;
    }

    if (!adapter->bio_ctx) {
        return NIMCP_SUCCESS;  /* Not an error if bio-async disabled */
    }

    LOG_INFO("[%s] Broadcasting attention shift (focus: %.2f, %.2f)",
             PARIETAL_CORTEX_LOG_MODULE, attention->focus_center.x, attention->focus_center.y);

    /* Would broadcast via bio_router in full implementation */
    return NIMCP_SUCCESS;
}
