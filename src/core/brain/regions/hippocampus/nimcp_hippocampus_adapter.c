/**
 * @file nimcp_hippocampus_adapter.c
 * @brief Implementation of Hippocampus brain adapter
 *
 * WHAT: Unified adapter connecting hippocampal sub-modules to the brain system
 * WHY:  Enable seamless integration with cognitive layers, memory, and spatial navigation
 * HOW:  Orchestrates place cells, grid cells, pattern separation/completion
 *
 * @version Phase H1: Hippocampus Brain Integration
 * @date 2025-12-30
 */

#include "core/brain/regions/hippocampus/nimcp_hippocampus_adapter.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/memory/nimcp_memory_pool.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_wiring_helpers.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/thread/nimcp_thread_rand.h"
#include "constants/nimcp_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(hippocampus_adapter, MESH_ADAPTER_CATEGORY_COGNITIVE)


/*=============================================================================
 * LOGGING MODULE IDENTIFIER
 *===========================================================================*/

#define HIPPOCAMPUS_LOG_MODULE "HIPPOCAMPUS"

/*=============================================================================
 * INTERNAL STRUCTURES
 *===========================================================================*/

/**
 * @brief Place cell representation
 */
struct place_cell_network {
    uint32_t num_cells;
    float* activations;              /**< Current activation levels */
    hippocampus_location_t* centers; /**< Place field centers */
    float* field_radii;              /**< Place field radii */
    float* features;                 /**< Associated features per cell */
    uint32_t feature_dim;
};

/**
 * @brief Grid cell representation
 */
struct grid_cell_network {
    uint32_t num_cells;
    float* activations;              /**< Current activation levels */
    float* spacings;                 /**< Grid spacing per module */
    float* orientations;             /**< Grid orientation per module */
    float* phases_x;                 /**< Phase offsets X */
    float* phases_y;                 /**< Phase offsets Y */
    uint32_t num_modules;            /**< Number of grid modules */
};

/**
 * @brief Pattern separator (Dentate Gyrus model)
 */
struct pattern_separator {
    uint32_t input_size;
    uint32_t output_size;
    float* weights;                  /**< Input -> DG weights */
    float* activations;              /**< DG output activations */
    float sparsity_target;           /**< Target sparsity level */
};

/**
 * @brief Memory encoder (CA3/CA1 model)
 */
struct memory_encoder {
    uint32_t ca3_size;
    uint32_t ca1_size;
    float* ca3_weights;              /**< Recurrent CA3 weights */
    float* ca3_to_ca1_weights;       /**< CA3 -> CA1 projection */
    float* ca3_activations;
    float* ca1_activations;
};

/**
 * @brief Memory storage entry
 */
typedef struct memory_entry {
    hippocampus_memory_t memory;
    float* ca3_pattern;              /**< Stored CA3 representation */
    struct memory_entry* next;       /**< Hash collision chain */
} memory_entry_t;

/**
 * @brief Internal adapter structure
 */
struct hippocampus_adapter {
    /* Configuration */
    hippocampus_config_t config;

    /* Sub-modules */
    place_cell_network_t* place_cells;
    grid_cell_network_t* grid_cells;
    pattern_separator_t* pattern_separator;
    memory_encoder_t* memory_encoder;

    /* Memory storage (hash table) */
    memory_entry_t** memory_store;
    uint32_t memory_capacity;
    uint32_t memory_count;
    uint32_t next_memory_id;

    /* Current spatial state */
    hippocampus_location_t current_location;
    hippocampus_location_t goal_location;
    bool has_goal;

    /* Callbacks */
    hippocampus_consolidation_callback_t consolidation_callback;
    void* consolidation_user_data;
    hippocampus_position_callback_t position_callback;
    void* position_user_data;
    hippocampus_event_callback_t event_callback;
    void* event_user_data;

    /* State */
    hippocampus_status_t status;
    hippocampus_error_t last_error;
    double current_time_ms;

    /* Memory pool for hot-path allocations */
    memory_pool_t pattern_pool;

    /* Bio-async communication context */
    bio_module_context_t bio_ctx;
    nimcp_bio_channel_type_t default_channel;

    /* Statistics */
    hippocampus_stats_t stats;
};

/*=============================================================================
 * INTERNAL HELPERS
 *===========================================================================*/

/**
 * @brief Simple hash function for memory ID
 */
static uint32_t hash_memory_id(uint32_t memory_id, uint32_t capacity) {
    return memory_id % capacity;
}

/**
 * @brief Compute place cell activation for location
 */
static float compute_place_activation(
    const hippocampus_location_t* center,
    float radius,
    const hippocampus_location_t* location
) {
    float dx = location->x - center->x;
    float dy = location->y - center->y;
    float dist_sq = dx * dx + dy * dy;
    float radius_sq = radius * radius;

    /* Gaussian tuning curve */
    return expf(-dist_sq / (2.0f * radius_sq));
}

/**
 * @brief Compute grid cell activation for location
 */
static float compute_grid_activation(
    float spacing,
    float orientation,
    float phase_x,
    float phase_y,
    const hippocampus_location_t* location
) {
    /* Rotate coordinates by orientation */
    float cos_theta = cosf(orientation);
    float sin_theta = sinf(orientation);
    float x_rot = location->x * cos_theta + location->y * sin_theta;
    float y_rot = -location->x * sin_theta + location->y * cos_theta;

    /* Triangular grid pattern (sum of 3 cosines at 60 degree intervals) */
    float k = 2.0f * M_PI / spacing;
    float theta1 = 0.0f;
    float theta2 = 2.0f * M_PI / 3.0f;
    float theta3 = 4.0f * M_PI / 3.0f;

    float activation = cosf(k * (x_rot * cosf(theta1) + y_rot * sinf(theta1) + phase_x));
    activation += cosf(k * (x_rot * cosf(theta2) + y_rot * sinf(theta2) + phase_y));
    activation += cosf(k * (x_rot * cosf(theta3) + y_rot * sinf(theta3)));

    /* Normalize to [0, 1] */
    return (activation + 3.0f) / 6.0f;
}

/**
 * @brief Emit event to callback
 */
static void emit_event(hippocampus_adapter_t* adapter, uint32_t event_type, const void* data) {
    if (adapter->config.enable_events && adapter->event_callback) {
        adapter->event_callback(event_type, data, adapter->event_user_data);
    }
}

/**
 * @brief Set error state
 */
static void set_error(hippocampus_adapter_t* adapter, hippocampus_error_t error) {
    if (!adapter) return;
    adapter->last_error = error;
    if (error != HIPPOCAMPUS_ERROR_NONE) {
        adapter->status = HIPPOCAMPUS_STATUS_ERROR;
        LOG_ERROR("[%s] Error set: %d", HIPPOCAMPUS_LOG_MODULE, error);
    }
}

/**
 * @brief Compute cosine similarity between vectors
 */
static float cosine_similarity(const float* a, const float* b, uint32_t size) {
    float dot = 0.0f, norm_a = 0.0f, norm_b = 0.0f;
    for (uint32_t i = 0; i < size; i++) {
        dot += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }
    float denom = sqrtf(norm_a) * sqrtf(norm_b);
    return denom > 0.0f ? dot / denom : 0.0f;
}

/*=============================================================================
 * SUB-MODULE CREATION
 *===========================================================================*/

static place_cell_network_t* create_place_cells(const hippocampus_config_t* config) {
    place_cell_network_t* net = nimcp_calloc(1, sizeof(place_cell_network_t));
    if (!net) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "net is NULL");

        return NULL;

    }

    net->num_cells = config->num_place_cells;
    net->feature_dim = config->feature_dim;

    net->activations = nimcp_calloc(net->num_cells, sizeof(float));
    net->centers = nimcp_calloc(net->num_cells, sizeof(hippocampus_location_t));
    net->field_radii = nimcp_calloc(net->num_cells, sizeof(float));
    net->features = nimcp_calloc(net->num_cells * net->feature_dim, sizeof(float));

    if (!net->activations || !net->centers || !net->field_radii || !net->features) {
        if (net->activations) nimcp_free(net->activations);
        if (net->centers) nimcp_free(net->centers);
        if (net->field_radii) nimcp_free(net->field_radii);
        if (net->features) nimcp_free(net->features);
        nimcp_free(net);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "create_place_cells: validation failed");
        return NULL;
    }

    /* Initialize place fields randomly */
    for (uint32_t i = 0; i < net->num_cells; i++) {
        net->centers[i].x = (float)(nimcp_tl_rand() % 100) - 50.0f;
        net->centers[i].y = (float)(nimcp_tl_rand() % 100) - 50.0f;
        net->field_radii[i] = config->place_field_radius;
    }

    return net;
}

static void destroy_place_cells(place_cell_network_t* net) {
    if (!net) return;
    if (net->activations) nimcp_free(net->activations);
    if (net->centers) nimcp_free(net->centers);
    if (net->field_radii) nimcp_free(net->field_radii);
    if (net->features) nimcp_free(net->features);
    nimcp_free(net);
}

static grid_cell_network_t* create_grid_cells(const hippocampus_config_t* config) {
    grid_cell_network_t* net = nimcp_calloc(1, sizeof(grid_cell_network_t));
    if (!net) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "net is NULL");

        return NULL;

    }

    net->num_cells = config->num_grid_cells;
    net->num_modules = config->num_grid_scales;

    net->activations = nimcp_calloc(net->num_cells, sizeof(float));
    net->spacings = nimcp_calloc(net->num_modules, sizeof(float));
    net->orientations = nimcp_calloc(net->num_modules, sizeof(float));
    net->phases_x = nimcp_calloc(net->num_cells, sizeof(float));
    net->phases_y = nimcp_calloc(net->num_cells, sizeof(float));

    if (!net->activations || !net->spacings || !net->orientations ||
        !net->phases_x || !net->phases_y) {
        if (net->activations) nimcp_free(net->activations);
        if (net->spacings) nimcp_free(net->spacings);
        if (net->orientations) nimcp_free(net->orientations);
        if (net->phases_x) nimcp_free(net->phases_x);
        if (net->phases_y) nimcp_free(net->phases_y);
        nimcp_free(net);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "create_grid_cells: validation failed");
        return NULL;
    }

    /* Initialize grid modules with increasing spacing */
    float base_spacing = config->grid_spacing;
    for (uint32_t m = 0; m < net->num_modules; m++) {
        net->spacings[m] = base_spacing * powf(1.42f, (float)m);  /* ~sqrt(2) scaling */
        net->orientations[m] = (float)m * 7.5f * M_PI / 180.0f;   /* 7.5 degree offsets */
    }

    /* Initialize random phases */
    for (uint32_t i = 0; i < net->num_cells; i++) {
        net->phases_x[i] = (float)(nimcp_tl_rand() % 1000) / 1000.0f * 2.0f * M_PI;
        net->phases_y[i] = (float)(nimcp_tl_rand() % 1000) / 1000.0f * 2.0f * M_PI;
    }

    return net;
}

static void destroy_grid_cells(grid_cell_network_t* net) {
    if (!net) return;
    if (net->activations) nimcp_free(net->activations);
    if (net->spacings) nimcp_free(net->spacings);
    if (net->orientations) nimcp_free(net->orientations);
    if (net->phases_x) nimcp_free(net->phases_x);
    if (net->phases_y) nimcp_free(net->phases_y);
    nimcp_free(net);
}

static pattern_separator_t* create_pattern_separator(const hippocampus_config_t* config) {
    pattern_separator_t* sep = nimcp_calloc(1, sizeof(pattern_separator_t));
    if (!sep) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sep is NULL");

        return NULL;

    }

    sep->input_size = config->ec_size;
    sep->output_size = config->dg_size;
    sep->sparsity_target = 0.02f;  /* ~2% sparsity (biological DG) */

    sep->weights = nimcp_calloc(sep->input_size * sep->output_size, sizeof(float));
    sep->activations = nimcp_calloc(sep->output_size, sizeof(float));

    if (!sep->weights || !sep->activations) {
        if (sep->weights) nimcp_free(sep->weights);
        if (sep->activations) nimcp_free(sep->activations);
        nimcp_free(sep);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "create_pattern_separator: validation failed");
        return NULL;
    }

    /* Initialize weights randomly */
    for (uint32_t i = 0; i < sep->input_size * sep->output_size; i++) {
        sep->weights[i] = ((float)(nimcp_tl_rand() % 1000) / 1000.0f - 0.5f) * 0.1f;
    }

    return sep;
}

static void destroy_pattern_separator(pattern_separator_t* sep) {
    if (!sep) return;
    if (sep->weights) nimcp_free(sep->weights);
    if (sep->activations) nimcp_free(sep->activations);
    nimcp_free(sep);
}

static memory_encoder_t* create_memory_encoder(const hippocampus_config_t* config) {
    memory_encoder_t* enc = nimcp_calloc(1, sizeof(memory_encoder_t));
    if (!enc) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "enc is NULL");

        return NULL;

    }

    enc->ca3_size = config->ca3_size;
    enc->ca1_size = config->ca1_size;

    enc->ca3_weights = nimcp_calloc(enc->ca3_size * enc->ca3_size, sizeof(float));
    enc->ca3_to_ca1_weights = nimcp_calloc(enc->ca3_size * enc->ca1_size, sizeof(float));
    enc->ca3_activations = nimcp_calloc(enc->ca3_size, sizeof(float));
    enc->ca1_activations = nimcp_calloc(enc->ca1_size, sizeof(float));

    if (!enc->ca3_weights || !enc->ca3_to_ca1_weights ||
        !enc->ca3_activations || !enc->ca1_activations) {
        if (enc->ca3_weights) nimcp_free(enc->ca3_weights);
        if (enc->ca3_to_ca1_weights) nimcp_free(enc->ca3_to_ca1_weights);
        if (enc->ca3_activations) nimcp_free(enc->ca3_activations);
        if (enc->ca1_activations) nimcp_free(enc->ca1_activations);
        nimcp_free(enc);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "create_memory_encoder: validation failed");
        return NULL;
    }

    /* Initialize weights */
    for (uint32_t i = 0; i < enc->ca3_size * enc->ca3_size; i++) {
        enc->ca3_weights[i] = ((float)(nimcp_tl_rand() % 1000) / 1000.0f - 0.5f) * 0.01f;
    }
    for (uint32_t i = 0; i < enc->ca3_size * enc->ca1_size; i++) {
        enc->ca3_to_ca1_weights[i] = ((float)(nimcp_tl_rand() % 1000) / 1000.0f - 0.5f) * 0.1f;
    }

    return enc;
}

static void destroy_memory_encoder(memory_encoder_t* enc) {
    if (!enc) return;
    if (enc->ca3_weights) nimcp_free(enc->ca3_weights);
    if (enc->ca3_to_ca1_weights) nimcp_free(enc->ca3_to_ca1_weights);
    if (enc->ca3_activations) nimcp_free(enc->ca3_activations);
    if (enc->ca1_activations) nimcp_free(enc->ca1_activations);
    nimcp_free(enc);
}

/*=============================================================================
 * BIO-ASYNC MESSAGE HANDLERS (Forward declarations)
 *===========================================================================*/

static nimcp_error_t handle_memory_encode_request(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data);

static nimcp_error_t handle_memory_retrieve_request(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data);

static nimcp_error_t handle_consolidation_request(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data);

static nimcp_error_t handle_position_update(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data);

/*=============================================================================
 * KG-DRIVEN WIRING CALLBACK
 *===========================================================================*/

/**
 * @brief KG-driven wiring handler callback
 *
 * WHAT: Register message handlers based on discovered wiring from KG
 * WHY:  Enables runtime assembly - module discovers its handlers from KG
 * HOW:  Orchestrator invokes this with message types from HANDLES_MESSAGE relations
 *
 * @param ctx Bio-async module context
 * @param message_types Array of message types to handle (from KG)
 * @param message_count Number of message types
 * @param user_data Hippocampus adapter pointer
 * @return 0 on success, -1 on error
 */
static int hippocampus_wiring_handler_callback(
    bio_module_context_t ctx,
    const bio_message_type_t* message_types,
    uint32_t message_count,
    void* user_data
) {
    if (!ctx || !message_types || message_count == 0) {
        return 0;  /* No handlers to register */
    }

    LOG_INFO("[%s] hippocampus_wiring_handler_callback: registering %u handlers from KG",
             HIPPOCAMPUS_LOG_MODULE, message_count);

    for (uint32_t i = 0; i < message_count; i++) {
        switch (message_types[i]) {
            case BIO_MSG_MEMORY_ENCODE_REQUEST:
                bio_router_register_handler(ctx, message_types[i], handle_memory_encode_request);
                LOG_DEBUG("[%s]   Registered handler for BIO_MSG_MEMORY_ENCODE_REQUEST", HIPPOCAMPUS_LOG_MODULE);
                break;

            case BIO_MSG_MEMORY_RETRIEVE_REQUEST:
                bio_router_register_handler(ctx, message_types[i], handle_memory_retrieve_request);
                LOG_DEBUG("[%s]   Registered handler for BIO_MSG_MEMORY_RETRIEVE_REQUEST", HIPPOCAMPUS_LOG_MODULE);
                break;

            case BIO_MSG_CONSOLIDATION_REQUEST:
                bio_router_register_handler(ctx, message_types[i], handle_consolidation_request);
                LOG_DEBUG("[%s]   Registered handler for BIO_MSG_CONSOLIDATION_REQUEST", HIPPOCAMPUS_LOG_MODULE);
                break;

            case BIO_MSG_POSITION_UPDATE:
                bio_router_register_handler(ctx, message_types[i], handle_position_update);
                LOG_DEBUG("[%s]   Registered handler for BIO_MSG_POSITION_UPDATE", HIPPOCAMPUS_LOG_MODULE);
                break;

            default:
                LOG_DEBUG("[%s]   Unknown message type %u - skipping", HIPPOCAMPUS_LOG_MODULE, message_types[i]);
                break;
        }
    }

    return 0;
}

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

hippocampus_config_t hippocampus_default_config(void) {
    hippocampus_config_t config;
    memset(&config, 0, sizeof(config));

    config.num_place_cells = HIPPOCAMPUS_DEFAULT_NUM_PLACE_CELLS;
    config.num_grid_cells = HIPPOCAMPUS_DEFAULT_NUM_GRID_CELLS;
    config.ca1_size = HIPPOCAMPUS_DEFAULT_CA1_SIZE;
    config.ca3_size = HIPPOCAMPUS_DEFAULT_CA3_SIZE;
    config.dg_size = HIPPOCAMPUS_DEFAULT_DG_SIZE;
    config.ec_size = HIPPOCAMPUS_DEFAULT_EC_SIZE;
    config.memory_capacity = HIPPOCAMPUS_DEFAULT_MEMORY_CAPACITY;
    config.memory_decay_rate = 0.001f;
    config.consolidation_threshold = 0.8f;
    config.spatial_dim = HIPPOCAMPUS_DEFAULT_SPATIAL_DIM;
    config.place_field_radius = 5.0f;
    config.grid_spacing = 30.0f;
    config.num_grid_scales = 4;
    config.feature_dim = HIPPOCAMPUS_DEFAULT_FEATURE_DIM;
    config.enable_pattern_separation = true;
    config.enable_pattern_completion = true;
    config.enable_replay = true;
    config.enable_spatial_navigation = true;
    config.enable_events = true;
    config.enable_training = false;
    config.learning_rate = NIMCP_LEARNING_RATE_DEFAULT;
    config.enable_bio_async = true;
    config.default_channel = BIO_CHANNEL_ACETYLCHOLINE;

    return config;
}

hippocampus_adapter_t* hippocampus_create(const hippocampus_config_t* config) {
    LOG_INFO("[%s] Creating hippocampus adapter", HIPPOCAMPUS_LOG_MODULE);

    hippocampus_adapter_t* adapter = nimcp_calloc(1, sizeof(hippocampus_adapter_t));
    if (!adapter) {
        LOG_ERROR("[%s] Failed to allocate adapter memory", HIPPOCAMPUS_LOG_MODULE);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hippocampus_create: adapter is NULL");
        return NULL;
    }

    /* Set configuration */
    if (config) {
        adapter->config = *config;
        LOG_DEBUG("[%s] Using provided configuration", HIPPOCAMPUS_LOG_MODULE);
    } else {
        adapter->config = hippocampus_default_config();
        LOG_DEBUG("[%s] Using default configuration", HIPPOCAMPUS_LOG_MODULE);
    }

    /* Create place cell network */
    LOG_DEBUG("[%s] Creating place cell network (n=%u)", HIPPOCAMPUS_LOG_MODULE,
              adapter->config.num_place_cells);
    adapter->place_cells = create_place_cells(&adapter->config);
    if (!adapter->place_cells) {
        LOG_ERROR("[%s] Failed to create place cell network", HIPPOCAMPUS_LOG_MODULE);
        hippocampus_destroy(adapter);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hippocampus_create: adapter->place_cells is NULL");
        return NULL;
    }

    /* Create grid cell network */
    LOG_DEBUG("[%s] Creating grid cell network (n=%u)", HIPPOCAMPUS_LOG_MODULE,
              adapter->config.num_grid_cells);
    adapter->grid_cells = create_grid_cells(&adapter->config);
    if (!adapter->grid_cells) {
        LOG_ERROR("[%s] Failed to create grid cell network", HIPPOCAMPUS_LOG_MODULE);
        hippocampus_destroy(adapter);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hippocampus_create: adapter->grid_cells is NULL");
        return NULL;
    }

    /* Create pattern separator (DG) */
    if (adapter->config.enable_pattern_separation) {
        LOG_DEBUG("[%s] Creating pattern separator (DG size=%u)", HIPPOCAMPUS_LOG_MODULE,
                  adapter->config.dg_size);
        adapter->pattern_separator = create_pattern_separator(&adapter->config);
        if (!adapter->pattern_separator) {
            LOG_ERROR("[%s] Failed to create pattern separator", HIPPOCAMPUS_LOG_MODULE);
            hippocampus_destroy(adapter);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hippocampus_create: adapter->pattern_separator is NULL");
            return NULL;
        }
    }

    /* Create memory encoder (CA3/CA1) */
    LOG_DEBUG("[%s] Creating memory encoder (CA3=%u, CA1=%u)", HIPPOCAMPUS_LOG_MODULE,
              adapter->config.ca3_size, adapter->config.ca1_size);
    adapter->memory_encoder = create_memory_encoder(&adapter->config);
    if (!adapter->memory_encoder) {
        LOG_ERROR("[%s] Failed to create memory encoder", HIPPOCAMPUS_LOG_MODULE);
        hippocampus_destroy(adapter);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hippocampus_create: adapter->memory_encoder is NULL");
        return NULL;
    }

    /* Initialize memory storage */
    adapter->memory_capacity = adapter->config.memory_capacity;
    adapter->memory_store = nimcp_calloc(adapter->memory_capacity, sizeof(memory_entry_t*));
    if (!adapter->memory_store) {
        LOG_ERROR("[%s] Failed to allocate memory store", HIPPOCAMPUS_LOG_MODULE);
        hippocampus_destroy(adapter);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hippocampus_create: adapter->memory_store is NULL");
        return NULL;
    }
    adapter->next_memory_id = 1;

    /* Initialize memory pool */
    LOG_DEBUG("[%s] Creating pattern memory pool", HIPPOCAMPUS_LOG_MODULE);
    memory_pool_config_t pool_config = {
        .block_size = adapter->config.ca3_size * sizeof(float),
        .num_blocks = 4,
        .alignment = 16,
        .enable_tracking = false,
        .enable_guard_pages = false
    };
    adapter->pattern_pool = memory_pool_create(&pool_config);
    if (!adapter->pattern_pool) {
        LOG_ERROR("[%s] Failed to create pattern memory pool", HIPPOCAMPUS_LOG_MODULE);
        hippocampus_destroy(adapter);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hippocampus_create: adapter->pattern_pool is NULL");
        return NULL;
    }

    /* Initialize bio-async communication */
    adapter->bio_ctx = NULL;
    adapter->default_channel = adapter->config.default_channel;

    if (adapter->config.enable_bio_async && bio_router_is_initialized()) {
        LOG_DEBUG("[%s] Registering with bio-async router", HIPPOCAMPUS_LOG_MODULE);

        bio_module_info_t bio_info = {
            .module_id = BIO_MODULE_HIPPOCAMPUS,
            .module_name = "hippocampus",
            .inbox_capacity = 64,
            .user_data = adapter
        };

        adapter->bio_ctx = bio_router_register_module(&bio_info);
        if (adapter->bio_ctx) {
            /* KG-Driven Wiring: Register callback for orchestrator to invoke
             * When orchestrator starts, it discovers HANDLES_MESSAGE relations
             * from the KG and invokes this callback with the message types */
            nimcp_error_t cb_result = bio_router_register_wiring_callback(
                BIO_MODULE_HIPPOCAMPUS,
                (void*)hippocampus_wiring_handler_callback,
                adapter
            );

            if (cb_result != NIMCP_SUCCESS) {
                /* Fallback: Direct registration if orchestrator not available
                 * This ensures backward compatibility with non-KG systems */
                LEGACY_HANDLER_REGISTRATION(
                    bio_router_register_handler(adapter->bio_ctx,
                        BIO_MSG_MEMORY_ENCODE_REQUEST, handle_memory_encode_request)
                );
                LEGACY_HANDLER_REGISTRATION(
                    bio_router_register_handler(adapter->bio_ctx,
                        BIO_MSG_MEMORY_RETRIEVE_REQUEST, handle_memory_retrieve_request)
                );
                LEGACY_HANDLER_REGISTRATION(
                    bio_router_register_handler(adapter->bio_ctx,
                        BIO_MSG_CONSOLIDATION_REQUEST, handle_consolidation_request)
                );
                LEGACY_HANDLER_REGISTRATION(
                    bio_router_register_handler(adapter->bio_ctx,
                        BIO_MSG_POSITION_UPDATE, handle_position_update)
                );
                LOG_INFO("[%s] Bio-async enabled (legacy direct registration)", HIPPOCAMPUS_LOG_MODULE);
            } else {
                LOG_INFO("[%s] Bio-async enabled (KG-driven wiring callback registered)", HIPPOCAMPUS_LOG_MODULE);
            }
        } else {
            LOG_WARNING("[%s] Failed to register with bio-async router", HIPPOCAMPUS_LOG_MODULE);
        }
    }

    /* Initialize state */
    adapter->status = HIPPOCAMPUS_STATUS_IDLE;
    adapter->last_error = HIPPOCAMPUS_ERROR_NONE;
    adapter->has_goal = false;
    memset(&adapter->current_location, 0, sizeof(hippocampus_location_t));
    memset(&adapter->goal_location, 0, sizeof(hippocampus_location_t));

    LOG_INFO("[%s] Hippocampus adapter created successfully", HIPPOCAMPUS_LOG_MODULE);
    return adapter;
}

void hippocampus_destroy(hippocampus_adapter_t* adapter) {
    if (!adapter) return;

    LOG_INFO("[%s] Destroying hippocampus adapter", HIPPOCAMPUS_LOG_MODULE);

    /* Unregister from bio-async router */
    if (adapter->bio_ctx) {
        LOG_DEBUG("[%s] Unregistering from bio-async router", HIPPOCAMPUS_LOG_MODULE);
        bio_router_unregister_module(adapter->bio_ctx);
        adapter->bio_ctx = NULL;
    }

    /* Destroy sub-modules */
    destroy_place_cells(adapter->place_cells);
    destroy_grid_cells(adapter->grid_cells);
    destroy_pattern_separator(adapter->pattern_separator);
    destroy_memory_encoder(adapter->memory_encoder);

    /* Free memory store */
    if (adapter->memory_store) {
        for (uint32_t i = 0; i < adapter->memory_capacity; i++) {
            memory_entry_t* entry = adapter->memory_store[i];
            while (entry) {
                memory_entry_t* next = entry->next;
                if (entry->memory.features) nimcp_free(entry->memory.features);
                if (entry->ca3_pattern) nimcp_free(entry->ca3_pattern);
                nimcp_free(entry);
                entry = next;
            }
        }
        nimcp_free(adapter->memory_store);
    }

    /* Destroy memory pool */
    if (adapter->pattern_pool) {
        memory_pool_destroy(adapter->pattern_pool);
    }

    LOG_DEBUG("[%s] Hippocampus adapter destroyed", HIPPOCAMPUS_LOG_MODULE);
    nimcp_free(adapter);
}

bool hippocampus_reset(hippocampus_adapter_t* adapter) {
    NIMCP_CHECK_NULL_BOOL(adapter, "adapter");

    LOG_DEBUG("[%s] Resetting adapter state", HIPPOCAMPUS_LOG_MODULE);

    /* Reset place cell activations */
    if (adapter->place_cells) {
        memset(adapter->place_cells->activations, 0,
               adapter->place_cells->num_cells * sizeof(float));
    }

    /* Reset grid cell activations */
    if (adapter->grid_cells) {
        memset(adapter->grid_cells->activations, 0,
               adapter->grid_cells->num_cells * sizeof(float));
    }

    /* Reset pattern separator */
    if (adapter->pattern_separator) {
        memset(adapter->pattern_separator->activations, 0,
               adapter->pattern_separator->output_size * sizeof(float));
    }

    /* Reset memory encoder */
    if (adapter->memory_encoder) {
        memset(adapter->memory_encoder->ca3_activations, 0,
               adapter->memory_encoder->ca3_size * sizeof(float));
        memset(adapter->memory_encoder->ca1_activations, 0,
               adapter->memory_encoder->ca1_size * sizeof(float));
    }

    /* Reset spatial state */
    memset(&adapter->current_location, 0, sizeof(hippocampus_location_t));
    memset(&adapter->goal_location, 0, sizeof(hippocampus_location_t));
    adapter->has_goal = false;

    /* Reset state */
    adapter->status = HIPPOCAMPUS_STATUS_IDLE;
    adapter->last_error = HIPPOCAMPUS_ERROR_NONE;

    LOG_DEBUG("[%s] Adapter reset complete", HIPPOCAMPUS_LOG_MODULE);
    return true;
}

/*=============================================================================
 * MEMORY ENCODING API
 *===========================================================================*/

uint32_t hippocampus_encode_memory(
    hippocampus_adapter_t* adapter,
    const float* features,
    uint32_t num_features,
    const hippocampus_location_t* location,
    float emotional_valence
) {
    if (!adapter || !features || num_features == 0) {
        set_error(adapter, HIPPOCAMPUS_ERROR_INVALID_INPUT);
        return 0;
    }

    if (adapter->memory_count >= adapter->memory_capacity) {
        set_error(adapter, HIPPOCAMPUS_ERROR_MEMORY_FULL);
        return 0;
    }

    adapter->status = HIPPOCAMPUS_STATUS_ENCODING;
    LOG_DEBUG("[%s] Encoding new memory (features=%u)", HIPPOCAMPUS_LOG_MODULE, num_features);

    /* Pattern separation through DG if enabled */
    float* sparse_representation = NULL;
    uint32_t sparse_size = 0;

    if (adapter->config.enable_pattern_separation && adapter->pattern_separator) {
        pattern_separation_result_t sep_result;
        if (hippocampus_pattern_separate(adapter, features, num_features, &sep_result)) {
            sparse_representation = sep_result.sparse_code;
            sparse_size = sep_result.sparse_size;
        }
    }

    /* Encode in CA3 */
    float* ca3_pattern = nimcp_calloc(adapter->memory_encoder->ca3_size, sizeof(float));
    if (!ca3_pattern) {
        set_error(adapter, HIPPOCAMPUS_ERROR_ENCODING_FAILURE);
        return 0;
    }

    /* Project input to CA3 (simplified: use features directly with random projection) */
    for (uint32_t i = 0; i < adapter->memory_encoder->ca3_size; i++) {
        float sum = 0.0f;
        for (uint32_t j = 0; j < num_features && j < adapter->config.ec_size; j++) {
            sum += features[j] * (((float)(nimcp_tl_rand() % 1000) / 1000.0f) - 0.5f);
        }
        ca3_pattern[i] = tanhf(sum);
    }

    /* Create memory entry */
    memory_entry_t* entry = nimcp_calloc(1, sizeof(memory_entry_t));
    if (!entry) {
        nimcp_free(ca3_pattern);
        set_error(adapter, HIPPOCAMPUS_ERROR_ENCODING_FAILURE);
        return 0;
    }

    uint32_t memory_id = adapter->next_memory_id++;
    entry->memory.memory_id = memory_id;
    entry->memory.features = nimcp_calloc(num_features, sizeof(float));
    if (!entry->memory.features) {
        nimcp_free(entry);
        nimcp_free(ca3_pattern);
        set_error(adapter, HIPPOCAMPUS_ERROR_ENCODING_FAILURE);
        return 0;
    }
    memcpy(entry->memory.features, features, num_features * sizeof(float));
    entry->memory.feature_count = num_features;
    if (location) {
        entry->memory.location = *location;
    }
    entry->memory.emotional_valence = emotional_valence;
    entry->memory.strength = 1.0f;
    entry->memory.timestamp_ms = (uint64_t)adapter->current_time_ms;
    entry->memory.is_consolidated = false;
    entry->ca3_pattern = ca3_pattern;

    /* Store in hash table */
    uint32_t idx = hash_memory_id(memory_id, adapter->memory_capacity);
    entry->next = adapter->memory_store[idx];
    adapter->memory_store[idx] = entry;
    adapter->memory_count++;

    /* Update statistics */
    adapter->stats.memories_encoded++;
    adapter->stats.current_memory_count = adapter->memory_count;

    /* Emit event */
    emit_event(adapter, 1 /* HIPPOCAMPUS_EVENT_MEMORY_ENCODED */, &memory_id);

    adapter->status = HIPPOCAMPUS_STATUS_IDLE;
    LOG_DEBUG("[%s] Memory encoded with ID %u", HIPPOCAMPUS_LOG_MODULE, memory_id);

    if (sparse_representation) {
        nimcp_free(sparse_representation);
    }

    return memory_id;
}

bool hippocampus_retrieve_by_cue(
    hippocampus_adapter_t* adapter,
    const float* cue,
    uint32_t cue_size,
    uint32_t max_results,
    retrieval_result_t* result
) {
    if (!adapter || !cue || cue_size == 0 || !result) {
        set_error(adapter, HIPPOCAMPUS_ERROR_INVALID_INPUT);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippocampus_retrieve_by_cue: required parameter is NULL (adapter, cue, result)");
        return false;
    }

    adapter->status = HIPPOCAMPUS_STATUS_RETRIEVING;
    LOG_DEBUG("[%s] Retrieving memories by cue (cue_size=%u, max=%u)",
              HIPPOCAMPUS_LOG_MODULE, cue_size, max_results);

    memset(result, 0, sizeof(retrieval_result_t));

    /* Pattern completion if enabled */
    float* completed_cue = NULL;
    if (adapter->config.enable_pattern_completion) {
        pattern_completion_result_t comp_result;
        if (hippocampus_pattern_complete(adapter, cue, cue_size, &comp_result)) {
            completed_cue = comp_result.completed_pattern;
            /* Use completed pattern for retrieval */
        }
    }

    /* Allocate result arrays */
    result->memories = nimcp_calloc(max_results, sizeof(hippocampus_memory_t));
    result->similarities = nimcp_calloc(max_results, sizeof(float));
    if (!result->memories || !result->similarities) {
        if (result->memories) nimcp_free(result->memories);
        if (result->similarities) nimcp_free(result->similarities);
        if (completed_cue) nimcp_free(completed_cue);
        set_error(adapter, HIPPOCAMPUS_ERROR_RETRIEVAL_FAILURE);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hippocampus_retrieve_by_cue: validation failed");
        return false;
    }

    /* Search all memories for similarity */
    typedef struct {
        memory_entry_t* entry;
        float similarity;
    } candidate_t;

    candidate_t* candidates = nimcp_calloc(adapter->memory_count, sizeof(candidate_t));
    if (!candidates) {
        nimcp_free(result->memories);
        nimcp_free(result->similarities);
        if (completed_cue) nimcp_free(completed_cue);
        set_error(adapter, HIPPOCAMPUS_ERROR_RETRIEVAL_FAILURE);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hippocampus_retrieve_by_cue: validation failed");
        return false;
    }

    uint32_t num_candidates = 0;
    for (uint32_t i = 0; i < adapter->memory_capacity; i++) {
        memory_entry_t* entry = adapter->memory_store[i];
        while (entry) {
            uint32_t compare_size = (cue_size < entry->memory.feature_count) ?
                                    cue_size : entry->memory.feature_count;
            float sim = cosine_similarity(cue, entry->memory.features, compare_size);
            candidates[num_candidates].entry = entry;
            candidates[num_candidates].similarity = sim;
            num_candidates++;
            entry = entry->next;
        }
    }

    /* Sort by similarity (simple selection sort for small N) */
    for (uint32_t i = 0; i < num_candidates && i < max_results; i++) {
        uint32_t max_idx = i;
        for (uint32_t j = i + 1; j < num_candidates; j++) {
            if (candidates[j].similarity > candidates[max_idx].similarity) {
                max_idx = j;
            }
        }
        if (max_idx != i) {
            candidate_t tmp = candidates[i];
            candidates[i] = candidates[max_idx];
            candidates[max_idx] = tmp;
        }
    }

    /* Copy top results */
    result->count = (num_candidates < max_results) ? num_candidates : max_results;
    for (uint32_t i = 0; i < result->count; i++) {
        result->memories[i] = candidates[i].entry->memory;
        result->similarities[i] = candidates[i].similarity;
    }
    result->retrieval_success = (result->count > 0);

    nimcp_free(candidates);
    if (completed_cue) nimcp_free(completed_cue);

    /* Update statistics */
    adapter->stats.memories_retrieved++;
    if (result->retrieval_success) {
        adapter->stats.successful_retrievals++;
    }

    adapter->status = HIPPOCAMPUS_STATUS_IDLE;
    LOG_DEBUG("[%s] Retrieved %u memories", HIPPOCAMPUS_LOG_MODULE, result->count);

    return true;
}

bool hippocampus_retrieve_by_location(
    hippocampus_adapter_t* adapter,
    const hippocampus_location_t* location,
    float radius,
    uint32_t max_results,
    retrieval_result_t* result
) {
    if (!adapter || !location || !result) {
        set_error(adapter, HIPPOCAMPUS_ERROR_INVALID_INPUT);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippocampus_retrieve_by_location: required parameter is NULL (adapter, location, result)");
        return false;
    }

    adapter->status = HIPPOCAMPUS_STATUS_RETRIEVING;
    LOG_DEBUG("[%s] Retrieving memories by location (x=%.2f, y=%.2f, r=%.2f)",
              HIPPOCAMPUS_LOG_MODULE, location->x, location->y, radius);

    memset(result, 0, sizeof(retrieval_result_t));

    /* Allocate result arrays */
    result->memories = nimcp_calloc(max_results, sizeof(hippocampus_memory_t));
    result->similarities = nimcp_calloc(max_results, sizeof(float));
    if (!result->memories || !result->similarities) {
        if (result->memories) nimcp_free(result->memories);
        if (result->similarities) nimcp_free(result->similarities);
        set_error(adapter, HIPPOCAMPUS_ERROR_RETRIEVAL_FAILURE);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hippocampus_retrieve_by_location: validation failed");
        return false;
    }

    /* Search memories within radius */
    uint32_t found = 0;
    float radius_sq = radius * radius;

    for (uint32_t i = 0; i < adapter->memory_capacity && found < max_results; i++) {
        memory_entry_t* entry = adapter->memory_store[i];
        while (entry && found < max_results) {
            float dx = location->x - entry->memory.location.x;
            float dy = location->y - entry->memory.location.y;
            float dist_sq = dx * dx + dy * dy;

            if (dist_sq <= radius_sq) {
                result->memories[found] = entry->memory;
                result->similarities[found] = 1.0f - sqrtf(dist_sq) / radius;
                found++;
            }
            entry = entry->next;
        }
    }

    result->count = found;
    result->retrieval_success = (found > 0);

    /* Update statistics */
    adapter->stats.memories_retrieved++;
    if (result->retrieval_success) {
        adapter->stats.successful_retrievals++;
    }

    adapter->status = HIPPOCAMPUS_STATUS_IDLE;
    LOG_DEBUG("[%s] Retrieved %u memories by location", HIPPOCAMPUS_LOG_MODULE, found);

    return true;
}

bool hippocampus_get_memory(
    const hippocampus_adapter_t* adapter,
    uint32_t memory_id,
    hippocampus_memory_t* memory
) {
    if (!adapter || memory_id == 0 || !memory) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hippocampus_get_memory: required parameter is NULL (adapter, memory)");
        return false;
    }

    uint32_t idx = hash_memory_id(memory_id, adapter->memory_capacity);
    memory_entry_t* entry = adapter->memory_store[idx];

    while (entry) {
        if (entry->memory.memory_id == memory_id) {
            *memory = entry->memory;
            return true;
        }
        entry = entry->next;
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hippocampus_get_memory: validation failed");
    return false;
}

/*=============================================================================
 * PATTERN SEPARATION AND COMPLETION
 *===========================================================================*/

bool hippocampus_pattern_separate(
    hippocampus_adapter_t* adapter,
    const float* input,
    uint32_t input_size,
    pattern_separation_result_t* result
) {
    if (!adapter || !input || !result || !adapter->pattern_separator) {
        set_error(adapter, HIPPOCAMPUS_ERROR_INVALID_INPUT);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippocampus_pattern_separate: required parameter is NULL (adapter, input, result, adapter->pattern_separator)");
        return false;
    }

    pattern_separator_t* sep = adapter->pattern_separator;
    LOG_DEBUG("[%s] Performing pattern separation (input_size=%u)", HIPPOCAMPUS_LOG_MODULE, input_size);

    /* Compute DG activations (feedforward) */
    memset(sep->activations, 0, sep->output_size * sizeof(float));
    uint32_t use_size = (input_size < sep->input_size) ? input_size : sep->input_size;

    for (uint32_t i = 0; i < sep->output_size; i++) {
        float sum = 0.0f;
        for (uint32_t j = 0; j < use_size; j++) {
            sum += input[j] * sep->weights[j * sep->output_size + i];
        }
        sep->activations[i] = sum;
    }

    /* Apply k-winners-take-all for sparsity */
    uint32_t k = (uint32_t)(sep->sparsity_target * sep->output_size);
    if (k < 1) k = 1;
    if (k > sep->output_size) k = sep->output_size;  /* Prevent OOB in sorted[k-1] */

    /* Find k-th largest activation (simple sort for small k) */
    float* sorted = nimcp_calloc(sep->output_size, sizeof(float));
    if (!sorted) {
        set_error(adapter, HIPPOCAMPUS_ERROR_PATTERN_SEPARATION_FAILURE);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hippocampus_pattern_separate: sorted is NULL");
        return false;
    }
    memcpy(sorted, sep->activations, sep->output_size * sizeof(float));

    /* Partial sort to find threshold */
    for (uint32_t i = 0; i < k; i++) {
        uint32_t max_idx = i;
        for (uint32_t j = i + 1; j < sep->output_size; j++) {
            if (sorted[j] > sorted[max_idx]) {
                max_idx = j;
            }
        }
        float tmp = sorted[i];
        sorted[i] = sorted[max_idx];
        sorted[max_idx] = tmp;
    }
    float threshold = sorted[k - 1];
    nimcp_free(sorted);

    /* Apply threshold */
    uint32_t active_count = 0;
    for (uint32_t i = 0; i < sep->output_size; i++) {
        if (sep->activations[i] >= threshold) {
            sep->activations[i] = 1.0f;
            active_count++;
        } else {
            sep->activations[i] = 0.0f;
        }
    }

    /* Fill result */
    result->sparse_code = nimcp_calloc(sep->output_size, sizeof(float));
    if (!result->sparse_code) {
        set_error(adapter, HIPPOCAMPUS_ERROR_PATTERN_SEPARATION_FAILURE);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hippocampus_pattern_separate: result->sparse_code is NULL");
        return false;
    }
    memcpy(result->sparse_code, sep->activations, sep->output_size * sizeof(float));
    result->sparse_size = sep->output_size;
    result->sparsity = (float)active_count / (float)sep->output_size;
    result->separation_strength = 1.0f - result->sparsity;  /* Lower sparsity = better separation */

    adapter->stats.separations_performed++;
    LOG_DEBUG("[%s] Pattern separation complete (sparsity=%.3f)", HIPPOCAMPUS_LOG_MODULE, result->sparsity);

    return true;
}

bool hippocampus_pattern_complete(
    hippocampus_adapter_t* adapter,
    const float* partial_cue,
    uint32_t cue_size,
    pattern_completion_result_t* result
) {
    if (!adapter || !partial_cue || !result || !adapter->memory_encoder) {
        set_error(adapter, HIPPOCAMPUS_ERROR_INVALID_INPUT);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hippocampus_pattern_complete: required parameter is NULL (adapter, partial_cue, result, adapter->memory_encoder)");
        return false;
    }

    memory_encoder_t* enc = adapter->memory_encoder;
    LOG_DEBUG("[%s] Performing pattern completion (cue_size=%u)", HIPPOCAMPUS_LOG_MODULE, cue_size);

    /* Initialize CA3 with cue */
    memset(enc->ca3_activations, 0, enc->ca3_size * sizeof(float));
    uint32_t use_size = (cue_size < enc->ca3_size) ? cue_size : enc->ca3_size;
    for (uint32_t i = 0; i < use_size; i++) {
        enc->ca3_activations[i] = partial_cue[i];
    }

    /* Iterative settling (recurrent CA3 dynamics) */
    float* new_activations = nimcp_calloc(enc->ca3_size, sizeof(float));
    if (!new_activations) {
        set_error(adapter, HIPPOCAMPUS_ERROR_PATTERN_COMPLETION_FAILURE);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hippocampus_pattern_complete: new_activations is NULL");
        return false;
    }

    const uint32_t num_iterations = 10;
    for (uint32_t iter = 0; iter < num_iterations; iter++) {
        memset(new_activations, 0, enc->ca3_size * sizeof(float));

        for (uint32_t i = 0; i < enc->ca3_size; i++) {
            float sum = 0.0f;
            for (uint32_t j = 0; j < enc->ca3_size; j++) {
                sum += enc->ca3_activations[j] * enc->ca3_weights[j * enc->ca3_size + i];
            }
            new_activations[i] = tanhf(sum + enc->ca3_activations[i] * 0.5f);  /* Recurrent + bias */
        }

        memcpy(enc->ca3_activations, new_activations, enc->ca3_size * sizeof(float));
    }
    nimcp_free(new_activations);

    /* Fill result */
    result->completed_pattern = nimcp_calloc(enc->ca3_size, sizeof(float));
    if (!result->completed_pattern) {
        set_error(adapter, HIPPOCAMPUS_ERROR_PATTERN_COMPLETION_FAILURE);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hippocampus_pattern_complete: result->completed_pattern is NULL");
        return false;
    }
    memcpy(result->completed_pattern, enc->ca3_activations, enc->ca3_size * sizeof(float));
    result->pattern_size = enc->ca3_size;

    /* Estimate confidence based on activation magnitude */
    float sum_sq = 0.0f;
    for (uint32_t i = 0; i < enc->ca3_size; i++) {
        sum_sq += enc->ca3_activations[i] * enc->ca3_activations[i];
    }
    result->completion_confidence = sqrtf(sum_sq / (float)enc->ca3_size);
    result->matched_memory_id = 0;  /* Would need similarity search to find match */

    adapter->stats.completions_performed++;
    LOG_DEBUG("[%s] Pattern completion done (confidence=%.3f)", HIPPOCAMPUS_LOG_MODULE,
              result->completion_confidence);

    return true;
}

/*=============================================================================
 * SPATIAL NAVIGATION API
 *===========================================================================*/

bool hippocampus_update_position(
    hippocampus_adapter_t* adapter,
    const hippocampus_location_t* location
) {
    if (!adapter || !location) {
        set_error(adapter, HIPPOCAMPUS_ERROR_INVALID_INPUT);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippocampus_update_position: required parameter is NULL (adapter, location)");
        return false;
    }

    adapter->current_location = *location;

    /* Update place cell activations */
    if (adapter->place_cells) {
        for (uint32_t i = 0; i < adapter->place_cells->num_cells; i++) {
            adapter->place_cells->activations[i] = compute_place_activation(
                &adapter->place_cells->centers[i],
                adapter->place_cells->field_radii[i],
                location
            );
        }
    }

    /* Update grid cell activations */
    if (adapter->grid_cells) {
        uint32_t cells_per_module = adapter->grid_cells->num_cells / adapter->grid_cells->num_modules;
        for (uint32_t i = 0; i < adapter->grid_cells->num_cells; i++) {
            uint32_t module = i / cells_per_module;
            if (module >= adapter->grid_cells->num_modules) module = adapter->grid_cells->num_modules - 1;

            adapter->grid_cells->activations[i] = compute_grid_activation(
                adapter->grid_cells->spacings[module],
                adapter->grid_cells->orientations[module],
                adapter->grid_cells->phases_x[i],
                adapter->grid_cells->phases_y[i],
                location
            );
        }
    }

    /* Invoke position callback */
    if (adapter->position_callback) {
        adapter->position_callback(location, adapter->position_user_data);
    }

    adapter->stats.navigation_steps++;
    return true;
}

bool hippocampus_get_position_estimate(
    const hippocampus_adapter_t* adapter,
    hippocampus_location_t* location
) {
    if (!adapter || !location || !adapter->place_cells) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippocampus_get_position_estimate: required parameter is NULL (adapter, location, adapter->place_cells)");
        return false;
    }

    /* Population vector decoding from place cells */
    float sum_x = 0.0f, sum_y = 0.0f, sum_weight = 0.0f;

    for (uint32_t i = 0; i < adapter->place_cells->num_cells; i++) {
        float activation = adapter->place_cells->activations[i];
        if (activation > 0.1f) {  /* Threshold */
            sum_x += activation * adapter->place_cells->centers[i].x;
            sum_y += activation * adapter->place_cells->centers[i].y;
            sum_weight += activation;
        }
    }

    if (sum_weight > 0.0f) {
        location->x = sum_x / sum_weight;
        location->y = sum_y / sum_weight;
        location->z = 0.0f;
        location->heading = adapter->current_location.heading;
        location->velocity = adapter->current_location.velocity;
        return true;
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hippocampus_get_position_estimate: validation failed");
    return false;
}

bool hippocampus_set_navigation_goal(
    hippocampus_adapter_t* adapter,
    const hippocampus_location_t* goal
) {
    if (!adapter || !goal) {
        set_error(adapter, HIPPOCAMPUS_ERROR_INVALID_INPUT);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippocampus_set_navigation_goal: required parameter is NULL (adapter, goal)");
        return false;
    }

    adapter->goal_location = *goal;
    adapter->has_goal = true;
    adapter->status = HIPPOCAMPUS_STATUS_NAVIGATING;

    LOG_DEBUG("[%s] Navigation goal set (x=%.2f, y=%.2f)",
              HIPPOCAMPUS_LOG_MODULE, goal->x, goal->y);
    return true;
}

bool hippocampus_get_navigation_guidance(
    hippocampus_adapter_t* adapter,
    navigation_result_t* result
) {
    if (!adapter || !result) {
        set_error(adapter, HIPPOCAMPUS_ERROR_INVALID_INPUT);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippocampus_get_navigation_guidance: required parameter is NULL (adapter, result)");
        return false;
    }

    if (!adapter->has_goal) {
        set_error(adapter, HIPPOCAMPUS_ERROR_NAVIGATION_FAILURE);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippocampus_get_navigation_guidance: adapter->has_goal is NULL");
        return false;
    }

    memset(result, 0, sizeof(navigation_result_t));

    result->current = adapter->current_location;
    result->goal = adapter->goal_location;

    /* Compute vector to goal */
    float dx = adapter->goal_location.x - adapter->current_location.x;
    float dy = adapter->goal_location.y - adapter->current_location.y;
    result->distance_to_goal = sqrtf(dx * dx + dy * dy);

    /* Compute heading error */
    float goal_heading = atan2f(dy, dx);
    result->heading_error = goal_heading - adapter->current_location.heading;

    /* Normalize heading error to [-pi, pi] */
    while (result->heading_error > M_PI) result->heading_error -= 2.0f * M_PI;
    while (result->heading_error < -M_PI) result->heading_error += 2.0f * M_PI;

    /* Simple path: direct line (in practice, would use place cell replay) */
    result->path = nimcp_calloc(4, sizeof(float));  /* 2 waypoints */
    if (result->path) {
        result->path[0] = adapter->current_location.x;
        result->path[1] = adapter->current_location.y;
        result->path[2] = adapter->goal_location.x;
        result->path[3] = adapter->goal_location.y;
        result->path_length = 2;
    }

    return true;
}

bool hippocampus_get_place_cell_activity(
    const hippocampus_adapter_t* adapter,
    place_cell_activity_t* activities,
    uint32_t max_count,
    uint32_t* count
) {
    if (!adapter || !activities || !count || !adapter->place_cells) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippocampus_get_place_cell_activity: required parameter is NULL (adapter, activities, count, adapter->place_cells)");
        return false;
    }

    uint32_t to_copy = (max_count < adapter->place_cells->num_cells) ?
                       max_count : adapter->place_cells->num_cells;

    for (uint32_t i = 0; i < to_copy; i++) {
        activities[i].cell_id = i;
        activities[i].activation = adapter->place_cells->activations[i];
        activities[i].center = adapter->place_cells->centers[i];
        activities[i].field_radius = adapter->place_cells->field_radii[i];
    }

    *count = to_copy;
    return true;
}

bool hippocampus_get_grid_cell_activity(
    const hippocampus_adapter_t* adapter,
    grid_cell_activity_t* activities,
    uint32_t max_count,
    uint32_t* count
) {
    if (!adapter || !activities || !count || !adapter->grid_cells) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippocampus_get_grid_cell_activity: required parameter is NULL (adapter, activities, count, adapter->grid_cells)");
        return false;
    }

    uint32_t to_copy = (max_count < adapter->grid_cells->num_cells) ?
                       max_count : adapter->grid_cells->num_cells;
    uint32_t cells_per_module = adapter->grid_cells->num_cells / adapter->grid_cells->num_modules;

    for (uint32_t i = 0; i < to_copy; i++) {
        uint32_t module = i / cells_per_module;
        if (module >= adapter->grid_cells->num_modules) module = adapter->grid_cells->num_modules - 1;

        activities[i].cell_id = i;
        activities[i].activation = adapter->grid_cells->activations[i];
        activities[i].spacing = adapter->grid_cells->spacings[module];
        activities[i].orientation = adapter->grid_cells->orientations[module];
        activities[i].phase_x = adapter->grid_cells->phases_x[i];
        activities[i].phase_y = adapter->grid_cells->phases_y[i];
    }

    *count = to_copy;
    return true;
}

/*=============================================================================
 * MEMORY CONSOLIDATION API
 *===========================================================================*/

uint32_t hippocampus_consolidate_memories(
    hippocampus_adapter_t* adapter,
    float strength_threshold
) {
    if (!adapter) {
        NIMCP_ERROR_SET(NIMCP_ERROR_NULL_POINTER, "NULL pointer: adapter");
        return 0;
    }

    adapter->status = HIPPOCAMPUS_STATUS_CONSOLIDATING;
    LOG_DEBUG("[%s] Consolidating memories (threshold=%.2f)", HIPPOCAMPUS_LOG_MODULE, strength_threshold);

    uint32_t consolidated = 0;

    for (uint32_t i = 0; i < adapter->memory_capacity; i++) {
        memory_entry_t* entry = adapter->memory_store[i];
        while (entry) {
            if (entry->memory.strength >= strength_threshold && !entry->memory.is_consolidated) {
                /* Mark as consolidated */
                entry->memory.is_consolidated = true;
                consolidated++;

                /* Invoke callback for cortical storage */
                if (adapter->consolidation_callback) {
                    adapter->consolidation_callback(&entry->memory, adapter->consolidation_user_data);
                }
            }
            entry = entry->next;
        }
    }

    adapter->stats.consolidated_count += consolidated;
    adapter->status = HIPPOCAMPUS_STATUS_IDLE;

    LOG_DEBUG("[%s] Consolidated %u memories", HIPPOCAMPUS_LOG_MODULE, consolidated);
    return consolidated;
}

bool hippocampus_set_consolidation_callback(
    hippocampus_adapter_t* adapter,
    hippocampus_consolidation_callback_t callback,
    void* user_data
) {
    NIMCP_CHECK_NULL_BOOL(adapter, "adapter");
    adapter->consolidation_callback = callback;
    adapter->consolidation_user_data = user_data;
    return true;
}

bool hippocampus_trigger_replay(
    hippocampus_adapter_t* adapter,
    bool reverse,
    uint32_t num_episodes
) {
    NIMCP_CHECK_NULL_BOOL(adapter, "adapter");

    adapter->status = HIPPOCAMPUS_STATUS_REPLAYING;
    LOG_DEBUG("[%s] Triggering replay (reverse=%d, episodes=%u)",
              HIPPOCAMPUS_LOG_MODULE, reverse, num_episodes);

    /* Simplified replay: reactivate stored patterns in sequence */
    uint32_t replayed = 0;
    for (uint32_t i = 0; i < adapter->memory_capacity && replayed < num_episodes; i++) {
        uint32_t idx = reverse ? (adapter->memory_capacity - 1 - i) : i;
        memory_entry_t* entry = adapter->memory_store[idx];

        while (entry && replayed < num_episodes) {
            /* Reactivate CA3 pattern */
            if (entry->ca3_pattern && adapter->memory_encoder) {
                memcpy(adapter->memory_encoder->ca3_activations,
                       entry->ca3_pattern,
                       adapter->memory_encoder->ca3_size * sizeof(float));
            }
            replayed++;
            entry = entry->next;
        }
    }

    adapter->stats.replay_episodes += replayed;
    adapter->status = HIPPOCAMPUS_STATUS_IDLE;

    LOG_DEBUG("[%s] Replayed %u episodes", HIPPOCAMPUS_LOG_MODULE, replayed);
    return true;
}

/*=============================================================================
 * EVENT INTEGRATION
 *===========================================================================*/

bool hippocampus_set_event_callback(
    hippocampus_adapter_t* adapter,
    hippocampus_event_callback_t callback,
    void* user_data
) {
    NIMCP_CHECK_NULL_BOOL(adapter, "adapter");
    adapter->event_callback = callback;
    adapter->event_user_data = user_data;
    return true;
}

/*=============================================================================
 * TRAINING INTERFACE
 *===========================================================================*/

bool hippocampus_train_association(
    hippocampus_adapter_t* adapter,
    const float* cue,
    uint32_t cue_size,
    uint32_t target_memory_id,
    float learning_rate
) {
    if (!adapter) {
        NIMCP_ERROR_SET(NIMCP_ERROR_NULL_POINTER, "NULL pointer: adapter");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippocampus_train_association: adapter is NULL");
        return false;
    }
    if (!cue) {
        NIMCP_ERROR_SET(NIMCP_ERROR_NULL_POINTER, "NULL pointer: cue");
        set_error(adapter, HIPPOCAMPUS_ERROR_INVALID_INPUT);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippocampus_train_association: cue is NULL");
        return false;
    }
    if (cue_size == 0) {
        NIMCP_ERROR_SET(NIMCP_ERROR_INVALID_PARAMETER, "Invalid parameter: cue_size must be > 0");
        set_error(adapter, HIPPOCAMPUS_ERROR_INVALID_INPUT);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hippocampus_train_association: cue_size is zero");
        return false;
    }
    if (target_memory_id == 0) {
        NIMCP_ERROR_SET(NIMCP_ERROR_INVALID_PARAMETER, "Invalid parameter: target_memory_id must be > 0");
        set_error(adapter, HIPPOCAMPUS_ERROR_INVALID_INPUT);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hippocampus_train_association: target_memory_id is zero");
        return false;
    }

    if (!adapter->config.enable_training) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippocampus_train_association: adapter->config is NULL");
        return false;
    }

    if (learning_rate <= 0.0f) {
        learning_rate = adapter->config.learning_rate;
    }

    /* Find target memory */
    hippocampus_memory_t target;
    if (!hippocampus_get_memory(adapter, target_memory_id, &target)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hippocampus_train_association: hippocampus_get_memory is NULL");
        return false;
    }

    /* Hebbian learning in CA3 */
    /* (Simplified: strengthen connections between cue and target pattern) */
    adapter->stats.training_iterations++;

    LOG_DEBUG("[%s] Trained association (cue_size=%u, target=%u, lr=%.4f)",
              HIPPOCAMPUS_LOG_MODULE, cue_size, target_memory_id, learning_rate);
    return true;
}

bool hippocampus_train_place_field(
    hippocampus_adapter_t* adapter,
    const hippocampus_location_t* location,
    const float* features,
    uint32_t num_features
) {
    if (!adapter) {
        NIMCP_ERROR_SET(NIMCP_ERROR_NULL_POINTER, "NULL pointer: adapter");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippocampus_train_place_field: adapter is NULL");
        return false;
    }
    if (!location) {
        NIMCP_ERROR_SET(NIMCP_ERROR_NULL_POINTER, "NULL pointer: location");
        set_error(adapter, HIPPOCAMPUS_ERROR_INVALID_INPUT);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippocampus_train_place_field: location is NULL");
        return false;
    }
    if (!features) {
        NIMCP_ERROR_SET(NIMCP_ERROR_NULL_POINTER, "NULL pointer: features");
        set_error(adapter, HIPPOCAMPUS_ERROR_INVALID_INPUT);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippocampus_train_place_field: features is NULL");
        return false;
    }
    if (num_features == 0) {
        NIMCP_ERROR_SET(NIMCP_ERROR_INVALID_PARAMETER, "Invalid parameter: num_features must be > 0");
        set_error(adapter, HIPPOCAMPUS_ERROR_INVALID_INPUT);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hippocampus_train_place_field: num_features is zero");
        return false;
    }

    if (!adapter->config.enable_training || !adapter->place_cells) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippocampus_train_place_field: required parameter is NULL (adapter->config, adapter->place_cells)");
        return false;
    }

    /* Find most active place cell and associate features */
    uint32_t max_idx = 0;
    float max_activation = 0.0f;

    for (uint32_t i = 0; i < adapter->place_cells->num_cells; i++) {
        float activation = compute_place_activation(
            &adapter->place_cells->centers[i],
            adapter->place_cells->field_radii[i],
            location
        );
        if (activation > max_activation) {
            max_activation = activation;
            max_idx = i;
        }
    }

    /* Update place field center (competitive learning) */
    float lr = adapter->config.learning_rate;
    adapter->place_cells->centers[max_idx].x +=
        lr * (location->x - adapter->place_cells->centers[max_idx].x);
    adapter->place_cells->centers[max_idx].y +=
        lr * (location->y - adapter->place_cells->centers[max_idx].y);

    /* Associate features */
    uint32_t copy_count = (num_features < adapter->place_cells->feature_dim) ?
                          num_features : adapter->place_cells->feature_dim;
    float* cell_features = &adapter->place_cells->features[max_idx * adapter->place_cells->feature_dim];
    for (uint32_t i = 0; i < copy_count; i++) {
        cell_features[i] += lr * (features[i] - cell_features[i]);
    }

    adapter->stats.training_iterations++;
    return true;
}

/*=============================================================================
 * STATUS AND DIAGNOSTICS
 *===========================================================================*/

hippocampus_status_t hippocampus_get_status(const hippocampus_adapter_t* adapter) {
    if (!adapter) return HIPPOCAMPUS_STATUS_ERROR;
    return adapter->status;
}

hippocampus_error_t hippocampus_get_last_error(const hippocampus_adapter_t* adapter) {
    if (!adapter) return HIPPOCAMPUS_ERROR_INTERNAL;
    return adapter->last_error;
}

const char* hippocampus_error_string(hippocampus_error_t error) {
    switch (error) {
        case HIPPOCAMPUS_ERROR_NONE: return "No error";
        case HIPPOCAMPUS_ERROR_INVALID_INPUT: return "Invalid input";
        case HIPPOCAMPUS_ERROR_ENCODING_FAILURE: return "Memory encoding failed";
        case HIPPOCAMPUS_ERROR_RETRIEVAL_FAILURE: return "Memory retrieval failed";
        case HIPPOCAMPUS_ERROR_NAVIGATION_FAILURE: return "Navigation failed";
        case HIPPOCAMPUS_ERROR_MEMORY_FULL: return "Memory capacity full";
        case HIPPOCAMPUS_ERROR_PATTERN_SEPARATION_FAILURE: return "Pattern separation failed";
        case HIPPOCAMPUS_ERROR_PATTERN_COMPLETION_FAILURE: return "Pattern completion failed";
        case HIPPOCAMPUS_ERROR_BUFFER_OVERFLOW: return "Buffer overflow";
        case HIPPOCAMPUS_ERROR_INTERNAL: return "Internal error";
        default: return "Unknown error";
    }
}

const char* hippocampus_status_string(hippocampus_status_t status) {
    switch (status) {
        case HIPPOCAMPUS_STATUS_IDLE: return "Idle";
        case HIPPOCAMPUS_STATUS_ENCODING: return "Encoding memory";
        case HIPPOCAMPUS_STATUS_RETRIEVING: return "Retrieving memory";
        case HIPPOCAMPUS_STATUS_NAVIGATING: return "Navigating";
        case HIPPOCAMPUS_STATUS_CONSOLIDATING: return "Consolidating";
        case HIPPOCAMPUS_STATUS_REPLAYING: return "Replaying";
        case HIPPOCAMPUS_STATUS_READY: return "Ready";
        case HIPPOCAMPUS_STATUS_ERROR: return "Error";
        default: return "Unknown";
    }
}

bool hippocampus_get_stats(const hippocampus_adapter_t* adapter, hippocampus_stats_t* stats) {
    NIMCP_CHECK_NULL_BOOL(adapter, "adapter");
    NIMCP_CHECK_NULL_BOOL(stats, "stats");
    *stats = adapter->stats;
    return true;
}

bool hippocampus_get_config(const hippocampus_adapter_t* adapter, hippocampus_config_t* config) {
    NIMCP_CHECK_NULL_BOOL(adapter, "adapter");
    NIMCP_CHECK_NULL_BOOL(config, "config");
    *config = adapter->config;
    return true;
}

/*=============================================================================
 * SUB-MODULE ACCESS
 *===========================================================================*/

place_cell_network_t* hippocampus_get_place_cells(hippocampus_adapter_t* adapter) {
    if (!adapter) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adapter is NULL");

        return NULL;

    }
    return adapter->place_cells;
}

grid_cell_network_t* hippocampus_get_grid_cells(hippocampus_adapter_t* adapter) {
    if (!adapter) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adapter is NULL");

        return NULL;

    }
    return adapter->grid_cells;
}

pattern_separator_t* hippocampus_get_pattern_separator(hippocampus_adapter_t* adapter) {
    if (!adapter) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adapter is NULL");

        return NULL;

    }
    return adapter->pattern_separator;
}

memory_encoder_t* hippocampus_get_memory_encoder(hippocampus_adapter_t* adapter) {
    if (!adapter) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adapter is NULL");

        return NULL;

    }
    return adapter->memory_encoder;
}

/*=============================================================================
 * BIO-ASYNC COMMUNICATION API
 *===========================================================================*/

bio_module_context_t hippocampus_get_bio_context(hippocampus_adapter_t* adapter) {
    if (!adapter) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adapter is NULL");

        return NULL;

    }
    return adapter->bio_ctx;
}

uint32_t hippocampus_process_bio_messages(hippocampus_adapter_t* adapter, uint32_t max_messages) {
    if (!adapter || !adapter->bio_ctx) return 0;

    uint32_t processed = bio_router_process_inbox(adapter->bio_ctx, max_messages);
    if (processed > 0) {
        LOG_DEBUG("[%s] Processed %u bio-async messages", HIPPOCAMPUS_LOG_MODULE, processed);
    }
    return processed;
}

nimcp_bio_future_t hippocampus_request_encode_async(
    hippocampus_adapter_t* adapter,
    const float* features,
    uint32_t num_features,
    const hippocampus_location_t* location
) {
    if (!adapter || !adapter->bio_ctx || !features) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippocampus_request_encode_async: required parameter is NULL (adapter, adapter->bio_ctx, features)");
        return NULL;
    }

    LOG_DEBUG("[%s] Requesting async memory encode (features=%u)", HIPPOCAMPUS_LOG_MODULE, num_features);

    /* Build encode request message */
    /* Using a generic message structure for simplicity */
    struct {
        bio_message_header_t header;
        uint32_t num_features;
        float location_x;
        float location_y;
    } msg;

    memset(&msg, 0, sizeof(msg));
    msg.header.type = BIO_MSG_MEMORY_ENCODE_REQUEST;
    msg.header.source_module = BIO_MODULE_HIPPOCAMPUS;
    msg.header.target_module = BIO_MODULE_HIPPOCAMPUS;
    msg.header.payload_size = sizeof(msg);
    msg.header.channel = adapter->default_channel;
    msg.num_features = num_features;
    if (location) {
        msg.location_x = location->x;
        msg.location_y = location->y;
    }

    nimcp_bio_promise_t promise = bio_router_send_async(
        adapter->bio_ctx, &msg, sizeof(msg), adapter->default_channel);

    if (!promise) {
        LOG_ERROR("[%s] Failed to send encode request", HIPPOCAMPUS_LOG_MODULE);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippocampus_request_encode_async: promise is NULL");
        return NULL;
    }

    return nimcp_bio_promise_get_future(promise);
}

nimcp_bio_future_t hippocampus_request_retrieve_async(
    hippocampus_adapter_t* adapter,
    const float* cue,
    uint32_t cue_size
) {
    if (!adapter || !adapter->bio_ctx || !cue) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippocampus_request_retrieve_async: required parameter is NULL (adapter, adapter->bio_ctx, cue)");
        return NULL;
    }

    LOG_DEBUG("[%s] Requesting async memory retrieval (cue_size=%u)", HIPPOCAMPUS_LOG_MODULE, cue_size);

    struct {
        bio_message_header_t header;
        uint32_t cue_size;
    } msg;

    memset(&msg, 0, sizeof(msg));
    msg.header.type = BIO_MSG_MEMORY_RETRIEVE_REQUEST;
    msg.header.source_module = BIO_MODULE_HIPPOCAMPUS;
    msg.header.target_module = BIO_MODULE_HIPPOCAMPUS;
    msg.header.payload_size = sizeof(msg);
    msg.header.channel = adapter->default_channel;
    msg.cue_size = cue_size;

    nimcp_bio_promise_t promise = bio_router_send_async(
        adapter->bio_ctx, &msg, sizeof(msg), adapter->default_channel);

    if (!promise) {
        LOG_ERROR("[%s] Failed to send retrieve request", HIPPOCAMPUS_LOG_MODULE);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippocampus_request_retrieve_async: promise is NULL");
        return NULL;
    }

    return nimcp_bio_promise_get_future(promise);
}

nimcp_error_t hippocampus_broadcast_memory_encoded(
    hippocampus_adapter_t* adapter,
    uint32_t memory_id
) {
    if (!adapter) return NIMCP_BIO_ERROR_NOT_INITIALIZED;
    if (!adapter->bio_ctx) return NIMCP_SUCCESS;  /* Not an error if bio-async disabled */

    LOG_INFO("[%s] Broadcasting memory encoded (id=%u)", HIPPOCAMPUS_LOG_MODULE, memory_id);

    struct {
        bio_message_header_t header;
        uint32_t memory_id;
    } msg;

    memset(&msg, 0, sizeof(msg));
    msg.header.type = BIO_MSG_MEMORY_ENCODED;
    msg.header.source_module = BIO_MODULE_HIPPOCAMPUS;
    msg.header.target_module = 0;  /* Broadcast */
    msg.header.payload_size = sizeof(msg);
    msg.header.channel = adapter->default_channel;
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.memory_id = memory_id;

    return bio_router_broadcast(adapter->bio_ctx, &msg, sizeof(msg));
}

nimcp_error_t hippocampus_handle_consolidation_request(
    hippocampus_adapter_t* adapter,
    bool from_cortex
) {
    if (!adapter) return NIMCP_BIO_ERROR_NOT_INITIALIZED;

    LOG_DEBUG("[%s] Handling consolidation request (from_cortex=%d)",
              HIPPOCAMPUS_LOG_MODULE, from_cortex);

    /* Trigger consolidation with default threshold */
    float threshold = from_cortex ? 0.5f : adapter->config.consolidation_threshold;
    hippocampus_consolidate_memories(adapter, threshold);

    return NIMCP_SUCCESS;
}

/*=============================================================================
 * BIO-ASYNC MESSAGE HANDLERS (Implementation)
 *===========================================================================*/

static nimcp_error_t handle_memory_encode_request(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data
) {
    hippocampus_adapter_t* adapter = (hippocampus_adapter_t*)user_data;
    (void)msg;
    (void)msg_size;

    if (!adapter) {
        LOG_ERROR("[%s] Invalid encode request", HIPPOCAMPUS_LOG_MODULE);
        return NIMCP_BIO_ERROR_NOT_INITIALIZED;
    }

    LOG_DEBUG("[%s] Handling memory encode request", HIPPOCAMPUS_LOG_MODULE);

    /* Simplified: would extract features from message in real implementation */
    float dummy_features[16] = {0};
    uint32_t memory_id = hippocampus_encode_memory(adapter, dummy_features, 16, NULL, 0.0f);

    /* Build response */
    struct {
        bio_message_header_t header;
        uint32_t memory_id;
        bool success;
    } response;

    memset(&response, 0, sizeof(response));
    response.header.type = BIO_MSG_MEMORY_ENCODE_RESPONSE;
    response.header.source_module = BIO_MODULE_HIPPOCAMPUS;
    response.header.payload_size = sizeof(response);
    response.memory_id = memory_id;
    response.success = (memory_id != 0);

    if (response_promise) {
        nimcp_bio_promise_complete(response_promise, &response);
    }

    return NIMCP_SUCCESS;
}

static nimcp_error_t handle_memory_retrieve_request(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data
) {
    hippocampus_adapter_t* adapter = (hippocampus_adapter_t*)user_data;
    (void)msg;
    (void)msg_size;

    if (!adapter) {
        LOG_ERROR("[%s] Invalid retrieve request", HIPPOCAMPUS_LOG_MODULE);
        return NIMCP_BIO_ERROR_NOT_INITIALIZED;
    }

    LOG_DEBUG("[%s] Handling memory retrieve request", HIPPOCAMPUS_LOG_MODULE);

    /* Simplified retrieval */
    float dummy_cue[16] = {0};
    retrieval_result_t result;
    hippocampus_retrieve_by_cue(adapter, dummy_cue, 16, 5, &result);

    /* Build response */
    struct {
        bio_message_header_t header;
        uint32_t count;
        bool success;
    } response;

    memset(&response, 0, sizeof(response));
    response.header.type = BIO_MSG_MEMORY_RETRIEVE_RESPONSE;
    response.header.source_module = BIO_MODULE_HIPPOCAMPUS;
    response.header.payload_size = sizeof(response);
    response.count = result.count;
    response.success = result.retrieval_success;

    /* Clean up result */
    if (result.memories) nimcp_free(result.memories);
    if (result.similarities) nimcp_free(result.similarities);

    if (response_promise) {
        nimcp_bio_promise_complete(response_promise, &response);
    }

    return NIMCP_SUCCESS;
}

static nimcp_error_t handle_consolidation_request(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data
) {
    hippocampus_adapter_t* adapter = (hippocampus_adapter_t*)user_data;
    (void)msg;
    (void)msg_size;

    if (!adapter) {
        LOG_ERROR("[%s] Invalid consolidation request", HIPPOCAMPUS_LOG_MODULE);
        return NIMCP_BIO_ERROR_NOT_INITIALIZED;
    }

    LOG_DEBUG("[%s] Handling consolidation request", HIPPOCAMPUS_LOG_MODULE);

    uint32_t consolidated = hippocampus_consolidate_memories(
        adapter, adapter->config.consolidation_threshold);

    /* Build response */
    struct {
        bio_message_header_t header;
        uint32_t consolidated_count;
    } response;

    memset(&response, 0, sizeof(response));
    response.header.type = BIO_MSG_CONSOLIDATION_RESPONSE;
    response.header.source_module = BIO_MODULE_HIPPOCAMPUS;
    response.header.payload_size = sizeof(response);
    response.consolidated_count = consolidated;

    if (response_promise) {
        nimcp_bio_promise_complete(response_promise, &response);
    }

    return NIMCP_SUCCESS;
}

static nimcp_error_t handle_position_update(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data
) {
    hippocampus_adapter_t* adapter = (hippocampus_adapter_t*)user_data;
    (void)msg_size;
    (void)response_promise;

    if (!adapter || !msg) {
        LOG_ERROR("[%s] Invalid position update", HIPPOCAMPUS_LOG_MODULE);
        return NIMCP_BIO_ERROR_NOT_INITIALIZED;
    }

    /* Extract location from message (simplified) */
    const struct {
        bio_message_header_t header;
        float x;
        float y;
        float heading;
    }* pos_msg = msg;

    hippocampus_location_t location = {
        .x = pos_msg->x,
        .y = pos_msg->y,
        .z = 0.0f,
        .heading = pos_msg->heading,
        .velocity = 0.0f
    };

    hippocampus_update_position(adapter, &location);

    return NIMCP_SUCCESS;
}
