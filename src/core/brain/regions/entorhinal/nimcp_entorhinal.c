/**
 * @file nimcp_entorhinal.c
 * @brief Entorhinal Cortex Implementation
 * @version Phase 5: Memory Circuit
 * @date 2025-01-12
 */

#include "core/brain/regions/entorhinal/nimcp_entorhinal.h"
#include "utils/memory/nimcp_memory_pool.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_constants.h"
#include "constants/nimcp_math_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(entorhinal, MESH_ADAPTER_CATEGORY_COGNITIVE)


/*=============================================================================
 * INTERNAL CONSTANTS
 *===========================================================================*/

#define PI NIMCP_PI_F
#define TWO_PI (2.0f * PI)
#define GRID_HEXAGONAL_ANGLE (PI / 3.0f)  /* 60 degrees */

/*=============================================================================
 * DEFAULT CONFIGURATION
 *===========================================================================*/

entorhinal_config_t entorhinal_default_config(void) {
    entorhinal_config_t config = {0};

    /* Grid cell parameters */
    config.num_grid_cells = ENTORHINAL_DEFAULT_GRID_CELLS;
    config.num_grid_modules = GRID_MODULE_COUNT;
    config.min_grid_spacing = ENTORHINAL_MIN_GRID_SPACING;
    config.max_grid_spacing = ENTORHINAL_MAX_GRID_SPACING;
    config.grid_scale_ratio = ENTORHINAL_GRID_SCALE_RATIO;

    /* Border cell parameters */
    config.num_border_cells = ENTORHINAL_DEFAULT_BORDER_CELLS;
    config.border_detection_range = 2.0f;

    /* Head direction parameters */
    config.num_hd_cells = ENTORHINAL_DEFAULT_HD_CELLS;
    config.hd_tuning_width = PI / 6.0f;  /* 30 degrees */
    config.anticipatory_time_ms = 25.0f;

    /* Object/speed/time cells */
    config.num_object_cells = ENTORHINAL_DEFAULT_OBJECT_CELLS;
    config.num_speed_cells = ENTORHINAL_DEFAULT_SPEED_CELLS;
    config.num_time_cells = ENTORHINAL_DEFAULT_TIME_CELLS;

    /* Path integration */
    config.path_integration_gain = 1.0f;
    config.drift_correction_rate = 0.1f;
    config.visual_reset_threshold = 0.8f;

    /* Memory gateway */
    config.encoding_buffer_size = 1024;
    config.retrieval_buffer_size = 1024;
    config.encoding_threshold = 0.5f;
    config.retrieval_threshold = 0.3f;

    /* Spatial parameters */
    config.spatial_dim = ENTORHINAL_DEFAULT_SPATIAL_DIM;
    config.feature_dim = ENTORHINAL_DEFAULT_FEATURE_DIM;
    config.environment_size[0] = 10.0f;
    config.environment_size[1] = 10.0f;
    config.environment_size[2] = 3.0f;

    /* Enable all integrations by default */
    config.enable_security = true;
    config.enable_immune = true;
    config.enable_bio_async = true;
    config.enable_snn = true;
    config.enable_plasticity = true;
    config.enable_stdp = true;
    config.enable_cognitive = true;
    config.enable_training = true;
    config.enable_substrate = true;
    config.enable_resonance = true;
    config.enable_thalamic = true;
    config.enable_hippocampus = true;
    config.enable_perception = true;
    config.enable_swarm = true;
    config.enable_dragonfly = true;
    config.enable_portia = true;
    config.enable_cerebellum = true;
    config.enable_medulla = true;
    config.enable_omni = true;
    config.enable_hypothalamus = true;
    config.enable_logic = true;
    config.enable_kg = true;

    /* Processing options */
    config.enable_path_integration = true;
    config.enable_boundary_detection = true;
    config.enable_object_tracking = true;
    config.enable_speed_encoding = true;
    config.enable_time_encoding = true;

    /* Learning parameters */
    config.learning_rate = NIMCP_LEARNING_RATE_DEFAULT;
    config.weight_decay = NIMCP_WEIGHT_DECAY_DEFAULT;
    config.eligibility_decay = NIMCP_ELIGIBILITY_DECAY_DEFAULT;

    /* Oscillation parameters */
    config.theta_frequency = 8.0f;
    config.gamma_frequency = 40.0f;
    config.phase_precession_rate = 0.1f;

    return config;
}

/*=============================================================================
 * INTERNAL HELPER FUNCTIONS
 *===========================================================================*/

/**
 * @brief Compute grid cell activation at position
 */
static float compute_grid_activation(const nimcp_grid_cell_t* cell,
                                     const float* position) {
    /* Hexagonal grid pattern using cosine gratings */
    float activation = 0.0f;

    for (int i = 0; i < 3; i++) {
        float angle = cell->orientation + i * GRID_HEXAGONAL_ANGLE;
        float wave_vector_x = cosf(angle) / cell->spacing;
        float wave_vector_y = sinf(angle) / cell->spacing;

        float phase = wave_vector_x * (position[0] - cell->phase_x) +
                      wave_vector_y * (position[1] - cell->phase_y);

        activation += cosf(TWO_PI * phase);
    }

    /* Normalize to [0, 1] */
    activation = (activation / 3.0f + 1.0f) / 2.0f;

    /* Apply peak rate */
    return activation * cell->peak_rate;
}

/**
 * @brief Compute border cell activation
 */
static float compute_border_activation(const nimcp_border_cell_t* cell,
                                       float distance_to_boundary,
                                       float direction_to_boundary) {
    /* Distance tuning */
    float dist_diff = fabsf(distance_to_boundary - cell->preferred_distance);
    float dist_response = expf(-dist_diff * dist_diff /
                               (2.0f * cell->tuning_width * cell->tuning_width));

    /* Direction tuning */
    float dir_diff = direction_to_boundary - cell->preferred_direction;
    /* Wrap to [-PI, PI] */
    while (dir_diff > PI) dir_diff -= TWO_PI;
    while (dir_diff < -PI) dir_diff += TWO_PI;
    float dir_response = expf(-dir_diff * dir_diff / (2.0f * 0.5f * 0.5f));

    return dist_response * dir_response;
}

/**
 * @brief Compute head direction cell activation
 */
static float compute_hd_activation(const nimcp_hd_cell_t* cell,
                                   float current_heading,
                                   float angular_velocity) {
    /* Add anticipatory offset */
    float anticipated_heading = current_heading +
                                angular_velocity * cell->anticipatory_offset;

    /* Wrap heading */
    while (anticipated_heading > PI) anticipated_heading -= TWO_PI;
    while (anticipated_heading < -PI) anticipated_heading += TWO_PI;

    /* Compute difference */
    float diff = anticipated_heading - cell->preferred_direction;
    while (diff > PI) diff -= TWO_PI;
    while (diff < -PI) diff += TWO_PI;

    /* Gaussian tuning curve */
    return expf(-diff * diff / (2.0f * cell->tuning_width * cell->tuning_width));
}

/**
 * @brief Initialize grid modules
 */
static int init_grid_modules(nimcp_entorhinal_t* ec) {
    ec->num_grid_modules = ec->config.num_grid_modules;
    ec->grid_modules = nimcp_calloc(ec->num_grid_modules, sizeof(nimcp_grid_module_t));
    if (!ec->grid_modules) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "init_grid_modules: ec->grid_modules is NULL");
        return -1;
    }

    uint32_t cells_per_module = (ec->num_grid_modules > 0) ?
        ec->config.num_grid_cells / ec->num_grid_modules : 0;
    float spacing = ec->config.min_grid_spacing;

    for (uint32_t m = 0; m < ec->num_grid_modules; m++) {
        nimcp_grid_module_t* module = &ec->grid_modules[m];
        module->module_type = (grid_module_t)m;
        module->base_spacing = spacing;
        module->orientation_offset = (float)m * (PI / 6.0f);  /* 30 degree offsets */

        module->num_cells = cells_per_module;
        module->cells = nimcp_calloc(cells_per_module, sizeof(nimcp_grid_cell_t));
        if (!module->cells) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "init_grid_modules: module->cells is NULL");
            return -1;
        }

        /* Initialize individual cells */
        for (uint32_t c = 0; c < cells_per_module; c++) {
            nimcp_grid_cell_t* cell = &module->cells[c];
            cell->cell_id = m * cells_per_module + c;
            cell->module = module->module_type;
            cell->spacing = module->base_spacing;
            cell->orientation = module->orientation_offset +
                               ((float)c / cells_per_module) * (PI / 3.0f);
            cell->phase_x = ((float)(c % 10) / 10.0f) * cell->spacing;
            cell->phase_y = ((float)(c / 10) / 10.0f) * cell->spacing;
            cell->peak_rate = 20.0f;  /* Hz */
            cell->activation = 0.0f;
            cell->eligibility_trace = 0.0f;
            cell->weight_sum = 1.0f;
        }

        ec->total_grid_cells += cells_per_module;

        /* Increase spacing for next module */
        spacing *= ec->config.grid_scale_ratio;
    }

    return 0;
}

/**
 * @brief Initialize border cells
 */
static int init_border_cells(nimcp_entorhinal_t* ec) {
    ec->num_border_cells = ec->config.num_border_cells;
    ec->border_cells = nimcp_calloc(ec->num_border_cells, sizeof(nimcp_border_cell_t));
    if (!ec->border_cells) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "init_border_cells: ec->border_cells is NULL");
        return -1;
    }

    for (uint32_t i = 0; i < ec->num_border_cells; i++) {
        nimcp_border_cell_t* cell = &ec->border_cells[i];
        cell->cell_id = i;
        cell->preferred_distance = (float)(i % 10) * 0.2f + 0.1f;
        cell->preferred_direction = ((float)(i / 10) / (ec->num_border_cells / 10)) * TWO_PI;
        cell->tuning_width = 0.3f;
        cell->activation = 0.0f;
        cell->boundary_confidence = 0.0f;
    }

    return 0;
}

/**
 * @brief Initialize head direction cells
 */
static int init_hd_cells(nimcp_entorhinal_t* ec) {
    ec->num_hd_cells = ec->config.num_hd_cells;
    ec->hd_cells = nimcp_calloc(ec->num_hd_cells, sizeof(nimcp_hd_cell_t));
    if (!ec->hd_cells) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "init_hd_cells: ec->hd_cells is NULL");
        return -1;
    }

    for (uint32_t i = 0; i < ec->num_hd_cells; i++) {
        nimcp_hd_cell_t* cell = &ec->hd_cells[i];
        cell->cell_id = i;
        cell->preferred_direction = ((float)i / ec->num_hd_cells) * TWO_PI - PI;
        cell->tuning_width = ec->config.hd_tuning_width;
        cell->activation = 0.0f;
        cell->anticipatory_offset = ec->config.anticipatory_time_ms / 1000.0f;
        cell->angular_velocity_gain = 1.0f;
    }

    return 0;
}

/**
 * @brief Initialize memory gateway
 */
static int init_memory_gateway(nimcp_entorhinal_t* ec) {
    nimcp_memory_gateway_t* gw = &ec->memory_gateway;

    gw->encoding_gate = 0.5f;
    gw->retrieval_gate = 0.5f;
    gw->consolidation_gate = 0.0f;
    gw->memory_binding_strength = 1.0f;
    gw->context_binding_strength = 1.0f;
    gw->temporal_binding_strength = 1.0f;

    gw->encoding_buffer_size = ec->config.encoding_buffer_size;
    gw->encoding_buffer = nimcp_calloc(gw->encoding_buffer_size, sizeof(float));
    if (!gw->encoding_buffer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "init_memory_gateway: gw->encoding_buffer is NULL");
        return -1;
    }

    gw->retrieval_buffer_size = ec->config.retrieval_buffer_size;
    gw->retrieval_buffer = nimcp_calloc(gw->retrieval_buffer_size, sizeof(float));
    if (!gw->retrieval_buffer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "init_memory_gateway: gw->retrieval_buffer is NULL");
        return -1;
    }

    gw->items_encoded = 0;
    gw->items_retrieved = 0;
    gw->items_consolidated = 0;
    gw->transfer_latency_ms = 0.0f;

    return 0;
}

/**
 * @brief Initialize path integration state
 */
static void init_path_integration(nimcp_entorhinal_t* ec) {
    nimcp_path_integration_t* pi = &ec->path_integration;

    memset(pi->position, 0, sizeof(pi->position));
    pi->heading = 0.0f;
    pi->speed = 0.0f;
    memset(pi->velocity, 0, sizeof(pi->velocity));
    pi->angular_velocity = 0.0f;
    pi->accumulated_error = 0.0f;
    pi->drift_rate = 0.001f;
    pi->last_reset_time = 0.0f;
    memset(pi->visual_correction, 0, sizeof(pi->visual_correction));
    memset(pi->boundary_correction, 0, sizeof(pi->boundary_correction));
    memset(pi->landmark_correction, 0, sizeof(pi->landmark_correction));
    pi->position_confidence = 1.0f;
    pi->heading_confidence = 1.0f;
}

/*=============================================================================
 * LIFECYCLE IMPLEMENTATION
 *===========================================================================*/

nimcp_entorhinal_t* entorhinal_create(const entorhinal_config_t* config) {
    nimcp_entorhinal_t* ec = nimcp_calloc(1, sizeof(nimcp_entorhinal_t));
    if (!ec) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "ec is NULL");

        return NULL;

    }

    /* Copy configuration */
    if (config) {
        ec->config = *config;
    } else {
        ec->config = entorhinal_default_config();
    }

    /* Initialize status */
    ec->status = ENTORHINAL_STATUS_IDLE;
    ec->last_error = ENTORHINAL_ERROR_NONE;

    /* Initialize grid cells */
    if (init_grid_modules(ec) != 0) {
        entorhinal_destroy(ec);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "entorhinal_create: validation failed");
        return NULL;
    }

    /* Initialize border cells */
    if (init_border_cells(ec) != 0) {
        entorhinal_destroy(ec);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "entorhinal_create: validation failed");
        return NULL;
    }

    /* Initialize head direction cells */
    if (init_hd_cells(ec) != 0) {
        entorhinal_destroy(ec);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "entorhinal_create: validation failed");
        return NULL;
    }

    /* Initialize object cells */
    if (ec->config.enable_object_tracking) {
        ec->num_object_cells = ec->config.num_object_cells;
        ec->object_cells = nimcp_calloc(ec->num_object_cells, sizeof(nimcp_object_cell_t));
        if (!ec->object_cells) {
            entorhinal_destroy(ec);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "entorhinal_create: ec->object_cells is NULL");
            return NULL;
        }
    }

    /* Initialize speed cells */
    if (ec->config.enable_speed_encoding) {
        ec->num_speed_cells = ec->config.num_speed_cells;
        ec->speed_cells = nimcp_calloc(ec->num_speed_cells, sizeof(nimcp_speed_cell_t));
        if (!ec->speed_cells) {
            entorhinal_destroy(ec);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "entorhinal_create: ec->speed_cells is NULL");
            return NULL;
        }
    }

    /* Initialize time cells */
    if (ec->config.enable_time_encoding) {
        ec->num_time_cells = ec->config.num_time_cells;
        ec->time_cells = nimcp_calloc(ec->num_time_cells, sizeof(nimcp_time_cell_t));
        if (!ec->time_cells) {
            entorhinal_destroy(ec);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "entorhinal_create: ec->time_cells is NULL");
            return NULL;
        }
    }

    /* Initialize path integration */
    init_path_integration(ec);

    /* Initialize memory gateway */
    if (init_memory_gateway(ec) != 0) {
        entorhinal_destroy(ec);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "entorhinal_create: validation failed");
        return NULL;
    }

    /* Initialize bridges to NULL (will be set by init_*_bridge functions) */
    memset(&ec->security_bridge, 0, sizeof(ec->security_bridge));
    memset(&ec->immune_bridge, 0, sizeof(ec->immune_bridge));
    memset(&ec->bio_async_bridge, 0, sizeof(ec->bio_async_bridge));
    memset(&ec->snn_bridge, 0, sizeof(ec->snn_bridge));
    memset(&ec->plasticity_bridge, 0, sizeof(ec->plasticity_bridge));
    memset(&ec->cognitive_bridge, 0, sizeof(ec->cognitive_bridge));
    memset(&ec->training_bridge, 0, sizeof(ec->training_bridge));
    memset(&ec->substrate_bridge, 0, sizeof(ec->substrate_bridge));
    memset(&ec->resonance_bridge, 0, sizeof(ec->resonance_bridge));

    /* Set safe defaults for modulation values so grid cells work without bridge init */
    ec->cognitive_bridge.attention_modulation = 1.0f;
    ec->substrate_bridge.firing_rate_modifier = 1.0f;
    ec->substrate_bridge.atp_level = 1.0f;
    ec->substrate_bridge.oxygen_level = 1.0f;
    ec->substrate_bridge.glucose_level = 1.0f;
    ec->substrate_bridge.metabolic_rate = 1.0f;
    memset(&ec->thalamic_bridge, 0, sizeof(ec->thalamic_bridge));
    memset(&ec->hippocampus_bridge, 0, sizeof(ec->hippocampus_bridge));
    memset(&ec->perception_bridge, 0, sizeof(ec->perception_bridge));
    memset(&ec->swarm_bridge, 0, sizeof(ec->swarm_bridge));
    memset(&ec->dragonfly_bridge, 0, sizeof(ec->dragonfly_bridge));
    memset(&ec->portia_bridge, 0, sizeof(ec->portia_bridge));
    memset(&ec->cerebellum_bridge, 0, sizeof(ec->cerebellum_bridge));
    memset(&ec->medulla_bridge, 0, sizeof(ec->medulla_bridge));
    memset(&ec->omni_bridge, 0, sizeof(ec->omni_bridge));
    memset(&ec->hypothalamus_bridge, 0, sizeof(ec->hypothalamus_bridge));
    memset(&ec->logic_bridge, 0, sizeof(ec->logic_bridge));
    memset(&ec->kg_bridge, 0, sizeof(ec->kg_bridge));

    /* Initialize statistics */
    ec->updates_processed = 0;
    ec->position_updates = 0;
    ec->memory_transfers = 0;
    ec->mean_grid_coherence = 0.0f;
    ec->mean_position_error = 0.0f;
    ec->total_processing_time_ms = 0.0;

    /* Record creation time */
    ec->creation_time_ms = 0;  /* Would use platform time */
    ec->last_update_ms = 0;
    ec->simulation_dt_ms = 1.0f;

    ec->status = ENTORHINAL_STATUS_READY;

    return ec;
}

void entorhinal_destroy(nimcp_entorhinal_t* ec) {
    if (!ec) return;

    /* Free grid modules */
    if (ec->grid_modules) {
        for (uint32_t m = 0; m < ec->num_grid_modules; m++) {
            nimcp_free(ec->grid_modules[m].cells);
        }
        nimcp_free(ec->grid_modules);
    }

    /* Free border cells */
    nimcp_free(ec->border_cells);

    /* Free head direction cells */
    nimcp_free(ec->hd_cells);

    /* Free object cells */
    nimcp_free(ec->object_cells);

    /* Free speed cells */
    nimcp_free(ec->speed_cells);

    /* Free time cells */
    nimcp_free(ec->time_cells);

    /* Free memory gateway buffers */
    nimcp_free(ec->memory_gateway.encoding_buffer);
    nimcp_free(ec->memory_gateway.retrieval_buffer);

    /* Free perception bridge buffers */
    nimcp_free(ec->perception_bridge.visual_input);
    nimcp_free(ec->perception_bridge.auditory_input);

    nimcp_free(ec);
}

bool entorhinal_reset(nimcp_entorhinal_t* ec) {
    if (!ec) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_reset: ec is NULL");
        return false;
    }

    /* Reset grid cells */
    for (uint32_t m = 0; m < ec->num_grid_modules; m++) {
        for (uint32_t c = 0; c < ec->grid_modules[m].num_cells; c++) {
            ec->grid_modules[m].cells[c].activation = 0.0f;
            ec->grid_modules[m].cells[c].eligibility_trace = 0.0f;
        }
    }

    /* Reset border cells */
    for (uint32_t i = 0; i < ec->num_border_cells; i++) {
        ec->border_cells[i].activation = 0.0f;
    }

    /* Reset HD cells */
    for (uint32_t i = 0; i < ec->num_hd_cells; i++) {
        ec->hd_cells[i].activation = 0.0f;
    }

    /* Reset path integration */
    init_path_integration(ec);

    /* Reset memory gateway */
    ec->memory_gateway.encoding_gate = 0.5f;
    ec->memory_gateway.retrieval_gate = 0.5f;

    ec->status = ENTORHINAL_STATUS_IDLE;
    ec->last_error = ENTORHINAL_ERROR_NONE;

    return true;
}

/*=============================================================================
 * BRIDGE INITIALIZATION IMPLEMENTATIONS
 *===========================================================================*/

int entorhinal_init_security_bridge(nimcp_entorhinal_t* ec,
    nimcp_security_context_t* security_ctx,
    nimcp_access_control_t* access_control) {
    if (!ec) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_reset: ec is NULL");
        return -1;
    }

    ec->security_bridge.security_ctx = security_ctx;
    ec->security_bridge.access_control = access_control;
    ec->security_bridge.access_level = 0;
    ec->security_bridge.threat_detected = false;
    ec->security_bridge.threat_level = 0.0f;
    ec->security_bridge.last_validation_ms = 0;

    return 0;
}

int entorhinal_init_immune_bridge(nimcp_entorhinal_t* ec,
    brain_immune_system_t* immune) {
    if (!ec) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_reset: ec is NULL");
        return -1;
    }

    ec->immune_bridge.immune = immune;
    ec->immune_bridge.health_score = 1.0f;
    ec->immune_bridge.anomaly_detected = false;
    ec->immune_bridge.inflammation_level = 0.0f;
    ec->immune_bridge.antibody_count = 0;
    ec->immune_bridge.last_scan_ms = 0;

    return 0;
}

int entorhinal_init_bio_async_bridge(nimcp_entorhinal_t* ec,
    nimcp_bio_router_t* router) {
    if (!ec) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_reset: ec is NULL");
        return -1;
    }

    ec->bio_async_bridge.router = router;
    memset(ec->bio_async_bridge.neuromodulator_levels, 0,
           sizeof(ec->bio_async_bridge.neuromodulator_levels));
    ec->bio_async_bridge.pending_messages = 0;
    ec->bio_async_bridge.messages_processed = 0;

    return 0;
}

int entorhinal_init_snn_bridge(nimcp_entorhinal_t* ec,
    snn_network_t* snn) {
    if (!ec) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_reset: ec is NULL");
        return -1;
    }

    ec->snn_bridge.snn = snn;
    ec->snn_bridge.spike_rate = 0.0f;
    ec->snn_bridge.mean_membrane_potential = -65.0f;

    return 0;
}

int entorhinal_init_plasticity_bridge(nimcp_entorhinal_t* ec,
    nimcp_plasticity_manager_t* plasticity,
    nimcp_stdp_rule_t* stdp_rule) {
    if (!ec) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_reset: ec is NULL");
        return -1;
    }

    ec->plasticity_bridge.plasticity = plasticity;
    ec->plasticity_bridge.stdp_rule = stdp_rule;
    ec->plasticity_bridge.learning_rate = ec->config.learning_rate;
    ec->plasticity_bridge.weight_decay = ec->config.weight_decay;
    ec->plasticity_bridge.weight_updates = 0;
    ec->plasticity_bridge.total_weight_change = 0.0f;

    return 0;
}

int entorhinal_init_cognitive_bridge(nimcp_entorhinal_t* ec,
    working_memory_t* wm,
    attention_system_t* attention,
    cognitive_integration_hub_t* hub) {
    if (!ec) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_reset: ec is NULL");
        return -1;
    }

    ec->cognitive_bridge.working_memory = wm;
    ec->cognitive_bridge.attention = attention;
    ec->cognitive_bridge.hub = hub;
    ec->cognitive_bridge.attention_modulation = 1.0f;
    ec->cognitive_bridge.working_memory_load = 0.0f;
    ec->cognitive_bridge.cognitive_events_sent = 0;

    return 0;
}

int entorhinal_init_training_bridge(nimcp_entorhinal_t* ec,
    nimcp_training_context_t* training_ctx) {
    if (!ec) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_reset: ec is NULL");
        return -1;
    }

    ec->training_bridge.training_ctx = training_ctx;
    ec->training_bridge.training_enabled = false;
    ec->training_bridge.current_loss = 0.0f;
    ec->training_bridge.training_steps = 0;
    ec->training_bridge.gradient_norm = 0.0f;

    return 0;
}

int entorhinal_init_substrate_bridge(nimcp_entorhinal_t* ec,
    nimcp_neural_substrate_t* substrate) {
    if (!ec) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_reset: ec is NULL");
        return -1;
    }

    ec->substrate_bridge.substrate = substrate;
    ec->substrate_bridge.atp_level = 1.0f;
    ec->substrate_bridge.oxygen_level = 1.0f;
    ec->substrate_bridge.glucose_level = 1.0f;
    ec->substrate_bridge.metabolic_rate = 1.0f;
    ec->substrate_bridge.firing_rate_modifier = 1.0f;

    return 0;
}

int entorhinal_init_resonance_bridge(nimcp_entorhinal_t* ec,
    nimcp_prime_resonance_t* resonance) {
    if (!ec) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_reset: ec is NULL");
        return -1;
    }

    ec->resonance_bridge.resonance = resonance;
    ec->resonance_bridge.theta_phase = 0.0f;
    ec->resonance_bridge.gamma_phase = 0.0f;
    ec->resonance_bridge.phase_lock_strength = 0.0f;
    ec->resonance_bridge.resonance_quality = 0.0f;

    return 0;
}

int entorhinal_init_thalamic_bridge(nimcp_entorhinal_t* ec,
    thalamus_adapter_t* thalamus) {
    if (!ec) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_reset: ec is NULL");
        return -1;
    }

    ec->thalamic_bridge.thalamus = thalamus;
    ec->thalamic_bridge.relay_gain = 1.0f;
    ec->thalamic_bridge.attention_gate = 1.0f;
    ec->thalamic_bridge.active_pathways = 0;

    return 0;
}

int entorhinal_init_hippocampus_bridge(nimcp_entorhinal_t* ec,
    hippocampus_adapter_t* hippocampus) {
    if (!ec) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_reset: ec is NULL");
        return -1;
    }

    ec->hippocampus_bridge.hippocampus = hippocampus;
    ec->hippocampus_bridge.hippocampal_theta_phase = 0.0f;
    ec->hippocampus_bridge.ca3_input_strength = 0.0f;
    ec->hippocampus_bridge.ca1_output_strength = 0.0f;
    ec->hippocampus_bridge.dg_pattern_separation = 0.0f;

    return 0;
}

int entorhinal_init_perception_bridge(nimcp_entorhinal_t* ec,
    nimcp_perception_layer_t* perception) {
    if (!ec) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_reset: ec is NULL");
        return -1;
    }

    ec->perception_bridge.perception = perception;
    ec->perception_bridge.visual_dim = 256;
    ec->perception_bridge.visual_input = nimcp_calloc(ec->perception_bridge.visual_dim, sizeof(float));
    if (!ec->perception_bridge.visual_input) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "entorhinal_init_perception_bridge: visual_input alloc failed");
        return -1;
    }
    ec->perception_bridge.auditory_dim = 128;
    ec->perception_bridge.auditory_input = nimcp_calloc(ec->perception_bridge.auditory_dim, sizeof(float));
    if (!ec->perception_bridge.auditory_input) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "entorhinal_init_perception_bridge: auditory_input alloc failed");
        nimcp_free(ec->perception_bridge.visual_input);
        ec->perception_bridge.visual_input = NULL;
        return -1;
    }
    ec->perception_bridge.salience_signal = 0.0f;

    return 0;
}

int entorhinal_init_swarm_bridge(nimcp_entorhinal_t* ec,
    nimcp_swarm_coordinator_t* swarm) {
    if (!ec) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_init_swarm_bridge: ec is NULL");
        return -1;
    }

    ec->swarm_bridge.swarm = swarm;
    ec->swarm_bridge.consensus_value = 0.0f;
    ec->swarm_bridge.active_agents = 0;
    ec->swarm_bridge.coordination_strength = 0.0f;

    return 0;
}

int entorhinal_init_dragonfly_bridge(nimcp_entorhinal_t* ec,
    nimcp_dragonfly_system_t* dragonfly) {
    if (!ec) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_init_dragonfly_bridge: ec is NULL");
        return -1;
    }

    ec->dragonfly_bridge.dragonfly = dragonfly;
    memset(ec->dragonfly_bridge.interception_vector, 0, sizeof(ec->dragonfly_bridge.interception_vector));
    memset(ec->dragonfly_bridge.target_velocity, 0, sizeof(ec->dragonfly_bridge.target_velocity));
    ec->dragonfly_bridge.prediction_horizon = 0.5f;

    return 0;
}

int entorhinal_init_portia_bridge(nimcp_entorhinal_t* ec,
    nimcp_portia_system_t* portia) {
    if (!ec) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_init_portia_bridge: ec is NULL");
        return -1;
    }

    ec->portia_bridge.portia = portia;
    ec->portia_bridge.planning_depth = 0.0f;
    ec->portia_bridge.deception_detection = 0.0f;
    ec->portia_bridge.strategy_confidence = 0.0f;

    return 0;
}

int entorhinal_init_cerebellum_bridge(nimcp_entorhinal_t* ec,
    cerebellum_adapter_t* cerebellum) {
    if (!ec) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_init_cerebellum_bridge: ec is NULL");
        return -1;
    }

    ec->cerebellum_bridge.cerebellum = cerebellum;
    ec->cerebellum_bridge.timing_signal = 0.0f;
    ec->cerebellum_bridge.prediction_error = 0.0f;
    memset(ec->cerebellum_bridge.motor_correction, 0, sizeof(ec->cerebellum_bridge.motor_correction));

    return 0;
}

int entorhinal_init_medulla_bridge(nimcp_entorhinal_t* ec,
    medulla_adapter_t* medulla) {
    if (!ec) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_init_medulla_bridge: ec is NULL");
        return -1;
    }

    ec->medulla_bridge.medulla = medulla;
    ec->medulla_bridge.arousal_level = 0.5f;
    ec->medulla_bridge.respiratory_phase = 0.0f;
    ec->medulla_bridge.cardiac_phase = 0.0f;

    return 0;
}

int entorhinal_init_omni_bridge(nimcp_entorhinal_t* ec,
    nimcp_omnidirectional_system_t* omni) {
    if (!ec) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_init_omni_bridge: ec is NULL");
        return -1;
    }

    ec->omni_bridge.omni = omni;
    memset(ec->omni_bridge.spatial_attention, 0, sizeof(ec->omni_bridge.spatial_attention));
    ec->omni_bridge.threat_direction = 0.0f;
    ec->omni_bridge.opportunity_direction = 0.0f;

    return 0;
}

int entorhinal_init_hypothalamus_bridge(nimcp_entorhinal_t* ec,
    hypothalamus_adapter_t* hypothalamus) {
    if (!ec) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_init_hypothalamus_bridge: ec is NULL");
        return -1;
    }

    ec->hypothalamus_bridge.hypothalamus = hypothalamus;
    ec->hypothalamus_bridge.motivation_signal = 0.5f;
    ec->hypothalamus_bridge.homeostatic_drive = 0.0f;
    ec->hypothalamus_bridge.reward_prediction = 0.0f;

    return 0;
}

int entorhinal_init_logic_bridge(nimcp_entorhinal_t* ec,
    nimcp_logic_system_t* logic) {
    if (!ec) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_init_logic_bridge: ec is NULL");
        return -1;
    }

    ec->logic_bridge.logic = logic;
    ec->logic_bridge.constraint_satisfied = true;
    ec->logic_bridge.inference_confidence = 0.0f;
    ec->logic_bridge.active_rules = 0;

    return 0;
}

int entorhinal_init_kg_bridge(nimcp_entorhinal_t* ec,
    brain_kg_t* kg) {
    if (!ec) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_init_kg_bridge: ec is NULL");
        return -1;
    }

    ec->kg_bridge.kg = kg;
    ec->kg_bridge.node_id = 0;
    ec->kg_bridge.health_status = 1.0f;
    ec->kg_bridge.edge_count = 0;

    return 0;
}

int entorhinal_init_all_bridges(nimcp_entorhinal_t* ec,
    nimcp_brain_t* brain) {
    if (!ec) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_init_all_bridges: ec is NULL");
        return -1;
    }

    /* Initialize all bridges with NULL - actual connections set later */
    /* This function provides a convenience wrapper for batch initialization */

    /* Security bridges are initialized separately for security reasons */
    /* Immune bridge */
    entorhinal_init_immune_bridge(ec, NULL);

    /* Bio-async bridge */
    entorhinal_init_bio_async_bridge(ec, NULL);

    /* SNN bridge */
    entorhinal_init_snn_bridge(ec, NULL);

    /* Plasticity bridge */
    entorhinal_init_plasticity_bridge(ec, NULL, NULL);

    /* Cognitive bridge */
    entorhinal_init_cognitive_bridge(ec, NULL, NULL, NULL);

    /* Training bridge */
    entorhinal_init_training_bridge(ec, NULL);

    /* Substrate bridge */
    entorhinal_init_substrate_bridge(ec, NULL);

    /* Resonance bridge */
    entorhinal_init_resonance_bridge(ec, NULL);

    /* Thalamic bridge */
    entorhinal_init_thalamic_bridge(ec, NULL);

    /* Hippocampus bridge */
    entorhinal_init_hippocampus_bridge(ec, NULL);

    /* Perception bridge */
    entorhinal_init_perception_bridge(ec, NULL);

    /* Swarm bridge */
    entorhinal_init_swarm_bridge(ec, NULL);

    /* Dragonfly bridge */
    entorhinal_init_dragonfly_bridge(ec, NULL);

    /* Portia bridge */
    entorhinal_init_portia_bridge(ec, NULL);

    /* Cerebellum bridge */
    entorhinal_init_cerebellum_bridge(ec, NULL);

    /* Medulla bridge */
    entorhinal_init_medulla_bridge(ec, NULL);

    /* Omni bridge */
    entorhinal_init_omni_bridge(ec, NULL);

    /* Hypothalamus bridge */
    entorhinal_init_hypothalamus_bridge(ec, NULL);

    /* Logic bridge */
    entorhinal_init_logic_bridge(ec, NULL);

    /* KG bridge */
    entorhinal_init_kg_bridge(ec, NULL);

    return 0;
}

/*=============================================================================
 * GRID CELL API IMPLEMENTATION
 *===========================================================================*/

int entorhinal_update_grid_cells(nimcp_entorhinal_t* ec,
    const float* position, uint32_t dim) {
    if (!ec || !position || dim < 2) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_update_grid_cells: required parameter is NULL (ec, position)");
        return -1;
    }

    ec->status = ENTORHINAL_STATUS_PATH_INTEGRATING;

    /* Apply substrate modulation */
    float firing_mod = ec->substrate_bridge.firing_rate_modifier;

    /* Update each grid module */
    for (uint32_t m = 0; m < ec->num_grid_modules; m++) {
        nimcp_grid_module_t* module = &ec->grid_modules[m];
        float module_activation_sum = 0.0f;
        float pop_vec_x = 0.0f;
        float pop_vec_y = 0.0f;

        for (uint32_t c = 0; c < module->num_cells; c++) {
            nimcp_grid_cell_t* cell = &module->cells[c];

            /* Compute activation */
            float activation = compute_grid_activation(cell, position);

            /* Apply modulation from attention */
            activation *= ec->cognitive_bridge.attention_modulation;

            /* Apply substrate modulation */
            activation *= firing_mod;

            /* Update eligibility trace for plasticity */
            cell->eligibility_trace = ec->config.eligibility_decay *
                                      cell->eligibility_trace +
                                      (1.0f - ec->config.eligibility_decay) * activation;

            cell->activation = activation;
            module_activation_sum += activation;

            /* Accumulate population vector */
            pop_vec_x += activation * cosf(cell->orientation);
            pop_vec_y += activation * sinf(cell->orientation);
        }

        /* Update module statistics */
        module->mean_activation = (module->num_cells > 0) ?
            module_activation_sum / module->num_cells : 0.0f;
        module->population_vector_x = (module->num_cells > 0) ?
            pop_vec_x / module->num_cells : 0.0f;
        module->population_vector_y = (module->num_cells > 0) ?
            pop_vec_y / module->num_cells : 0.0f;
        module->coherence = sqrtf(pop_vec_x * pop_vec_x + pop_vec_y * pop_vec_y) /
                            (module->mean_activation + 0.001f);
    }

    ec->position_updates++;
    ec->status = ENTORHINAL_STATUS_READY;

    return 0;
}

int entorhinal_get_grid_population_vector(const nimcp_entorhinal_t* ec,
    float* vector_out, uint32_t* dim) {
    if (!ec || !vector_out || !dim) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_get_grid_population_vector: required parameter is NULL (ec, vector_out, dim)");
        return -1;
    }

    /* Aggregate across modules */
    float pop_x = 0.0f;
    float pop_y = 0.0f;

    for (uint32_t m = 0; m < ec->num_grid_modules; m++) {
        pop_x += ec->grid_modules[m].population_vector_x;
        pop_y += ec->grid_modules[m].population_vector_y;
    }

    vector_out[0] = (ec->num_grid_modules > 0) ? pop_x / ec->num_grid_modules : 0.0f;
    vector_out[1] = (ec->num_grid_modules > 0) ? pop_y / ec->num_grid_modules : 0.0f;
    *dim = 2;

    return 0;
}

int entorhinal_decode_position_from_grid(const nimcp_entorhinal_t* ec,
    float* position_out, float* confidence_out) {
    if (!ec || !position_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_decode_position_from_grid: required parameter is NULL (ec, position_out)");
        return -1;
    }

    /* Simple population vector decoding */
    float total_x = 0.0f;
    float total_y = 0.0f;
    float total_weight = 0.0f;

    for (uint32_t m = 0; m < ec->num_grid_modules; m++) {
        const nimcp_grid_module_t* module = &ec->grid_modules[m];
        float weight = module->coherence;

        total_x += module->population_vector_x * weight;
        total_y += module->population_vector_y * weight;
        total_weight += weight;
    }

    if (total_weight > 0.001f) {
        position_out[0] = total_x / total_weight;
        position_out[1] = total_y / total_weight;
        if (confidence_out) {
            float conf = total_weight / ec->num_grid_modules;
            /* Clamp to [0, 1] */
            if (conf > 1.0f) conf = 1.0f;
            *confidence_out = conf;
        }
    } else {
        position_out[0] = 0.0f;
        position_out[1] = 0.0f;
        if (confidence_out) {
            *confidence_out = 0.0f;
        }
    }

    return 0;
}

int entorhinal_reset_grid_phases(nimcp_entorhinal_t* ec,
    const float* known_position) {
    if (!ec || !known_position) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_reset_grid_phases: required parameter is NULL (ec, known_position)");
        return -1;
    }

    for (uint32_t m = 0; m < ec->num_grid_modules; m++) {
        nimcp_grid_module_t* module = &ec->grid_modules[m];

        for (uint32_t c = 0; c < module->num_cells; c++) {
            nimcp_grid_cell_t* cell = &module->cells[c];

            /* Reset phases relative to known position */
            cell->phase_x = fmodf(known_position[0], cell->spacing);
            cell->phase_y = fmodf(known_position[1], cell->spacing);
        }
    }

    /* Reset path integration */
    ec->path_integration.position[0] = known_position[0];
    ec->path_integration.position[1] = known_position[1];
    ec->path_integration.accumulated_error = 0.0f;
    ec->path_integration.position_confidence = 1.0f;

    return 0;
}

const nimcp_grid_cell_t* entorhinal_get_grid_cell(const nimcp_entorhinal_t* ec,
    uint32_t module_idx, uint32_t cell_idx) {
    if (!ec || module_idx >= ec->num_grid_modules) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "entorhinal_get_grid_cell: invalid parameters");
        return NULL;
    }
    if (cell_idx >= ec->grid_modules[module_idx].num_cells) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "entorhinal_get_grid_cell: cell_idx out of range");
        return NULL;
    }

    return &ec->grid_modules[module_idx].cells[cell_idx];
}

/*=============================================================================
 * BORDER CELL API IMPLEMENTATION
 *===========================================================================*/

int entorhinal_update_border_cells(nimcp_entorhinal_t* ec,
    const float* boundary_distances, uint32_t num_boundaries) {
    if (!ec || !boundary_distances) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_update_border_cells: required parameter is NULL (ec, boundary_distances)");
        return -1;
    }

    /* Update each border cell based on boundary proximity */
    for (uint32_t i = 0; i < ec->num_border_cells; i++) {
        nimcp_border_cell_t* cell = &ec->border_cells[i];
        float max_activation = 0.0f;
        float max_confidence = 0.0f;

        /* Find best matching boundary for this cell */
        for (uint32_t b = 0; b < num_boundaries; b++) {
            float distance = boundary_distances[b * 2];      /* Distance */
            float direction = boundary_distances[b * 2 + 1]; /* Direction */

            float activation = compute_border_activation(cell, distance, direction);
            if (activation > max_activation) {
                max_activation = activation;
                max_confidence = 1.0f - (fabsf(distance - cell->preferred_distance) /
                                         ec->config.border_detection_range);
                if (max_confidence < 0.0f) max_confidence = 0.0f;
            }
        }

        cell->activation = max_activation;
        cell->boundary_confidence = max_confidence;
    }

    return 0;
}

int entorhinal_detect_boundaries(const nimcp_entorhinal_t* ec,
    float* boundary_directions, float* boundary_distances,
    uint32_t max_boundaries, uint32_t* num_detected) {
    if (!ec || !boundary_directions || !boundary_distances || !num_detected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_detect_boundaries: required parameter is NULL (ec, boundary_directions, boundary_distances)");
        return -1;
    }

    uint32_t detected = 0;

    /* Find peaks in border cell population */
    for (uint32_t i = 0; i < ec->num_border_cells && detected < max_boundaries; i++) {
        const nimcp_border_cell_t* cell = &ec->border_cells[i];

        /* Check if this cell has significant activation */
        if (cell->activation > 0.5f && cell->boundary_confidence > 0.3f) {
            /* Check if this is a local peak (not dominated by neighbors) */
            bool is_peak = true;

            for (uint32_t j = 0; j < ec->num_border_cells; j++) {
                if (i == j) continue;
                const nimcp_border_cell_t* other = &ec->border_cells[j];

                /* Check if same approximate direction */
                float dir_diff = fabsf(cell->preferred_direction - other->preferred_direction);
                if (dir_diff > PI) dir_diff = TWO_PI - dir_diff;

                if (dir_diff < 0.2f && other->activation > cell->activation) {
                    is_peak = false;
                    break;
                }
            }

            if (is_peak) {
                boundary_directions[detected] = cell->preferred_direction;
                boundary_distances[detected] = cell->preferred_distance;
                detected++;
            }
        }
    }

    *num_detected = detected;

    return 0;
}

/*=============================================================================
 * HEAD DIRECTION CELL API IMPLEMENTATION
 *===========================================================================*/

int entorhinal_update_hd_cells(nimcp_entorhinal_t* ec,
    float heading, float angular_velocity) {
    if (!ec) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_update_hd_cells: ec is NULL");
        return -1;
    }

    ec->current_heading = heading;

    for (uint32_t i = 0; i < ec->num_hd_cells; i++) {
        nimcp_hd_cell_t* cell = &ec->hd_cells[i];
        cell->activation = compute_hd_activation(cell, heading, angular_velocity);
    }

    return 0;
}

int entorhinal_decode_heading(const nimcp_entorhinal_t* ec,
    float* heading_out, float* confidence_out) {
    if (!ec || !heading_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_decode_heading: required parameter is NULL (ec, heading_out)");
        return -1;
    }

    /* Population vector decoding */
    float sum_x = 0.0f;
    float sum_y = 0.0f;
    float total_activation = 0.0f;

    for (uint32_t i = 0; i < ec->num_hd_cells; i++) {
        const nimcp_hd_cell_t* cell = &ec->hd_cells[i];
        sum_x += cell->activation * cosf(cell->preferred_direction);
        sum_y += cell->activation * sinf(cell->preferred_direction);
        total_activation += cell->activation;
    }

    *heading_out = atan2f(sum_y, sum_x);

    if (confidence_out) {
        float magnitude = sqrtf(sum_x * sum_x + sum_y * sum_y);
        *confidence_out = magnitude / (total_activation + 0.001f);
    }

    return 0;
}

int entorhinal_calibrate_hd_cells(nimcp_entorhinal_t* ec,
    float known_heading) {
    if (!ec) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_calibrate_hd_cells: ec is NULL");
        return -1;
    }

    /* Compute current decoded heading */
    float current_heading;
    entorhinal_decode_heading(ec, &current_heading, NULL);

    /* Compute offset */
    float offset = known_heading - current_heading;

    /* Apply offset to all HD cells */
    for (uint32_t i = 0; i < ec->num_hd_cells; i++) {
        ec->hd_cells[i].preferred_direction += offset;

        /* Wrap to [-PI, PI] */
        while (ec->hd_cells[i].preferred_direction > PI)
            ec->hd_cells[i].preferred_direction -= TWO_PI;
        while (ec->hd_cells[i].preferred_direction < -PI)
            ec->hd_cells[i].preferred_direction += TWO_PI;
    }

    ec->path_integration.heading = known_heading;
    ec->path_integration.heading_confidence = 1.0f;

    return 0;
}

/*=============================================================================
 * PATH INTEGRATION API IMPLEMENTATION
 *===========================================================================*/

int entorhinal_path_integrate(nimcp_entorhinal_t* ec,
    const float* velocity, float angular_velocity, float dt) {
    if (!ec || !velocity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_path_integrate: required parameter is NULL (ec, velocity)");
        return -1;
    }
    if (dt < 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "entorhinal_path_integrate: dt is negative");
        return -1;
    }

    nimcp_path_integration_t* pi = &ec->path_integration;

    /* Store velocity */
    memcpy(pi->velocity, velocity, 3 * sizeof(float));
    pi->angular_velocity = angular_velocity;

    /* Update heading */
    pi->heading += angular_velocity * dt;
    while (pi->heading > PI) pi->heading -= TWO_PI;
    while (pi->heading < -PI) pi->heading += TWO_PI;

    /* Update position */
    float speed = sqrtf(velocity[0] * velocity[0] + velocity[1] * velocity[1]);
    pi->speed = speed;

    pi->position[0] += velocity[0] * dt * ec->config.path_integration_gain;
    pi->position[1] += velocity[1] * dt * ec->config.path_integration_gain;
    if (ec->config.spatial_dim > 2) {
        pi->position[2] += velocity[2] * dt * ec->config.path_integration_gain;
    }

    /* Accumulate error (drift) */
    pi->accumulated_error += pi->drift_rate * speed * dt;

    /* Decay confidence */
    pi->position_confidence *= (1.0f - pi->drift_rate * dt);
    pi->heading_confidence *= (1.0f - pi->drift_rate * dt * 0.5f);

    /* Clamp confidence */
    if (pi->position_confidence < 0.1f) pi->position_confidence = 0.1f;
    if (pi->heading_confidence < 0.1f) pi->heading_confidence = 0.1f;

    /* Update grid cells with new position */
    entorhinal_update_grid_cells(ec, pi->position, ec->config.spatial_dim);

    /* Update HD cells */
    entorhinal_update_hd_cells(ec, pi->heading, angular_velocity);

    return 0;
}

int entorhinal_get_position_estimate(const nimcp_entorhinal_t* ec,
    float* position_out, float* heading_out,
    float* position_confidence, float* heading_confidence) {
    if (!ec || !position_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_get_position_estimate: ec or position_out is NULL");
        return -1;
    }

    const nimcp_path_integration_t* pi = &ec->path_integration;

    memcpy(position_out, pi->position, 3 * sizeof(float));

    if (heading_out) {
        *heading_out = pi->heading;
    }

    if (position_confidence) {
        *position_confidence = pi->position_confidence;
    }

    if (heading_confidence) {
        *heading_confidence = pi->heading_confidence;
    }

    return 0;
}

int entorhinal_apply_visual_correction(nimcp_entorhinal_t* ec,
    const float* visual_position, float visual_heading,
    float confidence) {
    if (!ec || !visual_position) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_apply_visual_correction: required parameter is NULL (ec, visual_position)");
        return -1;
    }

    nimcp_path_integration_t* pi = &ec->path_integration;

    if (confidence > ec->config.visual_reset_threshold) {
        /* High confidence: full reset */
        memcpy(pi->position, visual_position, 3 * sizeof(float));
        pi->heading = visual_heading;
        pi->accumulated_error = 0.0f;
        pi->position_confidence = confidence;
        pi->heading_confidence = confidence;

        /* Reset grid phases */
        entorhinal_reset_grid_phases(ec, visual_position);
        entorhinal_calibrate_hd_cells(ec, visual_heading);
    } else {
        /* Partial correction */
        float correction_strength = confidence * ec->config.drift_correction_rate;

        for (int i = 0; i < 3; i++) {
            float error = visual_position[i] - pi->position[i];
            pi->position[i] += error * correction_strength;
            pi->visual_correction[i] = error;
        }

        float heading_error = visual_heading - pi->heading;
        while (heading_error > PI) heading_error -= TWO_PI;
        while (heading_error < -PI) heading_error += TWO_PI;
        pi->heading += heading_error * correction_strength;

        pi->accumulated_error *= (1.0f - correction_strength);
        pi->position_confidence += (confidence - pi->position_confidence) *
                                   correction_strength;
    }

    return 0;
}

int entorhinal_apply_boundary_correction(nimcp_entorhinal_t* ec,
    const float* boundary_position, float boundary_direction) {
    if (!ec || !boundary_position) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_apply_boundary_correction: required parameter is NULL (ec, boundary_position)");
        return -1;
    }

    nimcp_path_integration_t* pi = &ec->path_integration;

    /* Compute correction based on boundary detection */
    float correction_strength = 0.2f;

    for (int i = 0; i < 2; i++) {
        float error = boundary_position[i] - pi->position[i];
        pi->boundary_correction[i] = error;
        pi->position[i] += error * correction_strength;
    }

    return 0;
}

/*=============================================================================
 * MEMORY GATEWAY API IMPLEMENTATION
 *===========================================================================*/

int entorhinal_set_encoding_gate(nimcp_entorhinal_t* ec, float gate_value) {
    if (!ec) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_set_encoding_gate: ec is NULL");
        return -1;
    }
    ec->memory_gateway.encoding_gate = fmaxf(0.0f, fminf(1.0f, gate_value));
    return 0;
}

int entorhinal_set_retrieval_gate(nimcp_entorhinal_t* ec, float gate_value) {
    if (!ec) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_set_retrieval_gate: ec is NULL");
        return -1;
    }
    ec->memory_gateway.retrieval_gate = fmaxf(0.0f, fminf(1.0f, gate_value));
    return 0;
}

int entorhinal_encode_to_hippocampus(nimcp_entorhinal_t* ec,
    const float* features, uint32_t feature_dim,
    const float* spatial_context, uint32_t spatial_dim) {
    if (!ec || !features || !spatial_context) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_encode_to_hippocampus: required parameter is NULL");
        return -1;
    }

    /* Check encoding gate */
    if (ec->memory_gateway.encoding_gate < ec->config.encoding_threshold) {
        ec->last_error = ENTORHINAL_ERROR_MEMORY_GATEWAY_BLOCKED;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "entorhinal_set_retrieval_gate: validation failed");
        return -1;
    }

    /* Security validation */
    if (ec->config.enable_security && ec->security_bridge.security_ctx) {
        if (ec->security_bridge.threat_detected) {
            ec->last_error = ENTORHINAL_ERROR_SECURITY_VIOLATION;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "entorhinal_set_retrieval_gate: validation failed");
            return -1;
        }
    }

    /* Immune check */
    if (ec->config.enable_immune && ec->immune_bridge.immune) {
        if (ec->immune_bridge.anomaly_detected) {
            ec->last_error = ENTORHINAL_ERROR_IMMUNE_REJECTION;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "entorhinal_set_retrieval_gate: validation failed");
            return -1;
        }
    }

    ec->status = ENTORHINAL_STATUS_ENCODING;

    /* Copy to encoding buffer */
    uint32_t copy_size = feature_dim < ec->memory_gateway.encoding_buffer_size ?
                         feature_dim : ec->memory_gateway.encoding_buffer_size;
    memcpy(ec->memory_gateway.encoding_buffer, features, copy_size * sizeof(float));

    /* Modulate by gate value */
    for (uint32_t i = 0; i < copy_size; i++) {
        ec->memory_gateway.encoding_buffer[i] *= ec->memory_gateway.encoding_gate;
    }

    /* Apply binding strength */
    for (uint32_t i = 0; i < copy_size; i++) {
        ec->memory_gateway.encoding_buffer[i] *= ec->memory_gateway.memory_binding_strength;
    }

    /* If hippocampus bridge is connected, send to hippocampus */
    if (ec->hippocampus_bridge.hippocampus) {
        /* Would call hippocampus_encode_memory here */
        ec->hippocampus_bridge.ca3_input_strength =
            ec->memory_gateway.encoding_gate * ec->memory_gateway.memory_binding_strength;
    }

    ec->memory_gateway.items_encoded++;
    ec->memory_transfers++;
    ec->status = ENTORHINAL_STATUS_READY;

    return 0;
}

int entorhinal_retrieve_from_hippocampus(nimcp_entorhinal_t* ec,
    const float* cue, uint32_t cue_dim,
    float* retrieved_features, uint32_t max_features,
    uint32_t* actual_features) {
    if (!ec || !cue || !retrieved_features) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_set_retrieval_gate: required parameter is NULL (ec, cue, retrieved_features)");
        return -1;
    }

    /* Check retrieval gate */
    if (ec->memory_gateway.retrieval_gate < ec->config.retrieval_threshold) {
        ec->last_error = ENTORHINAL_ERROR_MEMORY_GATEWAY_BLOCKED;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "entorhinal_set_retrieval_gate: validation failed");
        return -1;
    }

    ec->status = ENTORHINAL_STATUS_RETRIEVING;

    /* If hippocampus bridge is connected, retrieve from hippocampus */
    if (ec->hippocampus_bridge.hippocampus) {
        ec->hippocampus_bridge.ca1_output_strength = ec->memory_gateway.retrieval_gate;
        /* Would call hippocampus_retrieve_by_cue here */
    }

    /* For now, copy from retrieval buffer (would be filled by hippocampus) */
    uint32_t copy_size = max_features < ec->memory_gateway.retrieval_buffer_size ?
                         max_features : ec->memory_gateway.retrieval_buffer_size;
    memcpy(retrieved_features, ec->memory_gateway.retrieval_buffer, copy_size * sizeof(float));

    if (actual_features) {
        *actual_features = copy_size;
    }

    ec->memory_gateway.items_retrieved++;
    ec->status = ENTORHINAL_STATUS_READY;

    return 0;
}

int entorhinal_consolidate_to_neocortex(nimcp_entorhinal_t* ec,
    uint32_t memory_id, float consolidation_strength) {
    if (!ec) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_set_retrieval_gate: ec is NULL");
        return -1;
    }

    ec->status = ENTORHINAL_STATUS_CONSOLIDATING;

    ec->memory_gateway.consolidation_gate = consolidation_strength;
    ec->memory_gateway.items_consolidated++;

    ec->status = ENTORHINAL_STATUS_READY;

    return 0;
}

int entorhinal_get_gateway_stats(const nimcp_entorhinal_t* ec,
    uint64_t* encoded, uint64_t* retrieved, uint64_t* consolidated) {
    if (!ec || !encoded || !retrieved || !consolidated) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_get_gateway_stats: required parameter is NULL");
        return -1;
    }

    *encoded = ec->memory_gateway.items_encoded;
    *retrieved = ec->memory_gateway.items_retrieved;
    *consolidated = ec->memory_gateway.items_consolidated;

    return 0;
}

/*=============================================================================
 * BIDIRECTIONAL DATA FLOW IMPLEMENTATION
 *===========================================================================*/

int entorhinal_process_incoming(nimcp_entorhinal_t* ec) {
    if (!ec) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_process_incoming: ec is NULL");
        return -1;
    }

    /* Process bio-async messages */
    if (ec->config.enable_bio_async && ec->bio_async_bridge.router) {
        /* Would process incoming neuromodulator messages */
        ec->bio_async_bridge.messages_processed++;
    }

    /* Update from substrate */
    if (ec->config.enable_substrate && ec->substrate_bridge.substrate) {
        /* Would query substrate for metabolic state */
    }

    /* Get attention modulation from cognitive system */
    if (ec->config.enable_cognitive && ec->cognitive_bridge.attention) {
        /* Would query attention system */
    }

    /* Get timing from resonance system */
    if (ec->config.enable_resonance && ec->resonance_bridge.resonance) {
        /* Would sync with theta/gamma rhythms */
    }

    /* Process perception input */
    if (ec->config.enable_perception && ec->perception_bridge.perception) {
        /* Would process visual/auditory input */
    }

    return 0;
}

int entorhinal_send_outgoing(nimcp_entorhinal_t* ec) {
    if (!ec) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_send_outgoing: ec is NULL");
        return -1;
    }

    /* Send position update to hippocampus */
    if (ec->config.enable_hippocampus && ec->hippocampus_bridge.hippocampus) {
        /* Would send grid cell population vector */
    }

    /* Send to cognitive hub */
    if (ec->config.enable_cognitive && ec->cognitive_bridge.hub) {
        ec->cognitive_bridge.cognitive_events_sent++;
    }

    /* Report health to brain KG */
    if (ec->config.enable_kg && ec->kg_bridge.kg) {
        ec->kg_bridge.health_status = entorhinal_get_health_status(ec);
    }

    return 0;
}

int entorhinal_bidirectional_update(nimcp_entorhinal_t* ec, float dt) {
    if (!ec) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_bidirectional_update: ec is NULL");
        return -1;
    }

    ec->simulation_dt_ms = dt;

    /* Process incoming data from all bridges */
    entorhinal_process_incoming(ec);

    /* Apply substrate effects */
    entorhinal_update_substrate_effects(ec);

    /* Process neuromodulation */
    entorhinal_process_neuromodulation(ec);

    /* Apply plasticity updates */
    entorhinal_apply_plasticity(ec, dt);

    /* Send outgoing data to all bridges */
    entorhinal_send_outgoing(ec);

    /* Publish cognitive events */
    entorhinal_publish_cognitive_events(ec);

    /* Security validation (periodic) */
    entorhinal_validate_security(ec);

    /* Immune scan (periodic) */
    entorhinal_immune_scan(ec);

    ec->updates_processed++;
    ec->last_update_ms += (uint64_t)dt;

    return 0;
}

int entorhinal_sync_bio_async(nimcp_entorhinal_t* ec) {
    if (!ec) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_sync_bio_async: ec is NULL");
        return -1;
    }

    if (ec->bio_async_bridge.router) {
        /* Process pending messages */
        /* Update neuromodulator levels */
    }

    return 0;
}

int entorhinal_process_neuromodulation(nimcp_entorhinal_t* ec) {
    if (!ec) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_process_neuromodulation: ec is NULL");
        return -1;
    }

    /* Dopamine: affects grid cell precision */
    float da_level = ec->bio_async_bridge.neuromodulator_levels[ENTORHINAL_CHANNEL_DOPAMINE];

    /* Acetylcholine: affects encoding vs retrieval balance */
    float ach_level = ec->bio_async_bridge.neuromodulator_levels[ENTORHINAL_CHANNEL_ACETYLCHOLINE];

    /* High ACh favors encoding, low ACh favors retrieval */
    ec->memory_gateway.encoding_gate = 0.5f + 0.4f * ach_level;
    ec->memory_gateway.retrieval_gate = 0.5f + 0.4f * (1.0f - ach_level);

    /* Norepinephrine: affects attention/alertness */
    float ne_level = ec->bio_async_bridge.neuromodulator_levels[ENTORHINAL_CHANNEL_NOREPINEPHRINE];
    ec->cognitive_bridge.attention_modulation = 0.5f + 0.5f * ne_level;

    return 0;
}

int entorhinal_update_substrate_effects(nimcp_entorhinal_t* ec) {
    if (!ec) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_update_substrate_effects: ec is NULL");
        return -1;
    }

    if (ec->substrate_bridge.substrate) {
        /* Compute firing rate modifier based on metabolic state */
        float atp_effect = ec->substrate_bridge.atp_level;
        float o2_effect = ec->substrate_bridge.oxygen_level;
        float glucose_effect = ec->substrate_bridge.glucose_level;

        ec->substrate_bridge.firing_rate_modifier =
            0.5f * atp_effect + 0.3f * o2_effect + 0.2f * glucose_effect;

        /* Low resources -> reduced activity */
        if (ec->substrate_bridge.firing_rate_modifier < 0.5f) {
            ec->last_error = ENTORHINAL_ERROR_SUBSTRATE_DEPLETED;
        }
    }

    return 0;
}

int entorhinal_publish_cognitive_events(nimcp_entorhinal_t* ec) {
    if (!ec) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_publish_cognitive_events: ec is NULL");
        return -1;
    }

    if (ec->cognitive_bridge.hub) {
        /* Would publish position update events */
        /* Would publish memory encoding/retrieval events */
        ec->cognitive_bridge.cognitive_events_sent++;
    }

    return 0;
}

int entorhinal_apply_plasticity(nimcp_entorhinal_t* ec, float dt) {
    if (!ec) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_apply_plasticity: ec is NULL");
        return -1;
    }

    if (ec->plasticity_bridge.plasticity && ec->training_bridge.training_enabled) {
        /* Apply STDP to grid cell connections */
        for (uint32_t m = 0; m < ec->num_grid_modules; m++) {
            for (uint32_t c = 0; c < ec->grid_modules[m].num_cells; c++) {
                nimcp_grid_cell_t* cell = &ec->grid_modules[m].cells[c];

                /* Eligibility trace decay */
                cell->eligibility_trace *= ec->config.eligibility_decay;

                /* Weight update based on eligibility and reward */
                float weight_change = ec->plasticity_bridge.learning_rate *
                                      cell->eligibility_trace *
                                      ec->hypothalamus_bridge.reward_prediction;

                cell->weight_sum += weight_change;
                cell->weight_sum *= (1.0f - ec->plasticity_bridge.weight_decay * dt);

                ec->plasticity_bridge.total_weight_change += fabsf(weight_change);
            }
        }

        ec->plasticity_bridge.weight_updates++;
    }

    return 0;
}

int entorhinal_validate_security(nimcp_entorhinal_t* ec) {
    if (!ec) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_validate_security: ec is NULL");
        return -1;
    }

    if (ec->security_bridge.security_ctx) {
        /* Would perform security validation */
        ec->security_bridge.last_validation_ms = ec->last_update_ms;
    }

    return 0;
}

int entorhinal_immune_scan(nimcp_entorhinal_t* ec) {
    if (!ec) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_immune_scan: ec is NULL");
        return -1;
    }

    if (ec->immune_bridge.immune) {
        /* Would perform immune scan for anomalies */
        ec->immune_bridge.last_scan_ms = ec->last_update_ms;

        /* Update health score */
        ec->immune_bridge.health_score = 1.0f - ec->immune_bridge.inflammation_level;
    }

    return 0;
}

/*=============================================================================
 * STATUS AND DIAGNOSTICS IMPLEMENTATION
 *===========================================================================*/

entorhinal_status_t entorhinal_get_status(const nimcp_entorhinal_t* ec) {
    return ec ? ec->status : ENTORHINAL_STATUS_ERROR;
}

entorhinal_error_t entorhinal_get_last_error(const nimcp_entorhinal_t* ec) {
    return ec ? ec->last_error : ENTORHINAL_ERROR_INTERNAL;
}

const char* entorhinal_error_string(entorhinal_error_t error) {
    switch (error) {
        case ENTORHINAL_ERROR_NONE: return "No error";
        case ENTORHINAL_ERROR_INVALID_INPUT: return "Invalid input";
        case ENTORHINAL_ERROR_GRID_DRIFT: return "Grid drift";
        case ENTORHINAL_ERROR_PATH_INTEGRATION_FAILURE: return "Path integration failure";
        case ENTORHINAL_ERROR_MEMORY_GATEWAY_BLOCKED: return "Memory gateway blocked";
        case ENTORHINAL_ERROR_SECURITY_VIOLATION: return "Security violation";
        case ENTORHINAL_ERROR_IMMUNE_REJECTION: return "Immune rejection";
        case ENTORHINAL_ERROR_SUBSTRATE_DEPLETED: return "Substrate depleted";
        case ENTORHINAL_ERROR_SYNC_FAILURE: return "Sync failure";
        case ENTORHINAL_ERROR_BUFFER_OVERFLOW: return "Buffer overflow";
        case ENTORHINAL_ERROR_INTERNAL: return "Internal error";
        default: return "Unknown error";
    }
}

const char* entorhinal_status_string(entorhinal_status_t status) {
    switch (status) {
        case ENTORHINAL_STATUS_IDLE: return "Idle";
        case ENTORHINAL_STATUS_PATH_INTEGRATING: return "Path integrating";
        case ENTORHINAL_STATUS_ENCODING: return "Encoding";
        case ENTORHINAL_STATUS_RETRIEVING: return "Retrieving";
        case ENTORHINAL_STATUS_GATEWAY_TRANSFER: return "Gateway transfer";
        case ENTORHINAL_STATUS_CONSOLIDATING: return "Consolidating";
        case ENTORHINAL_STATUS_CALIBRATING: return "Calibrating";
        case ENTORHINAL_STATUS_READY: return "Ready";
        case ENTORHINAL_STATUS_ERROR: return "Error";
        default: return "Unknown status";
    }
}

int entorhinal_get_stats(const nimcp_entorhinal_t* ec, entorhinal_stats_t* stats) {
    if (!ec || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_get_stats: required parameter is NULL (ec, stats)");
        return -1;
    }

    memset(stats, 0, sizeof(entorhinal_stats_t));

    stats->updates_processed = ec->updates_processed;
    stats->position_updates = ec->position_updates;
    stats->memory_encodings = ec->memory_gateway.items_encoded;
    stats->memory_retrievals = ec->memory_gateway.items_retrieved;
    stats->memory_consolidations = ec->memory_gateway.items_consolidated;

    /* Compute grid cell statistics */
    float total_activation = 0.0f;
    float total_coherence = 0.0f;

    for (uint32_t m = 0; m < ec->num_grid_modules; m++) {
        total_activation += ec->grid_modules[m].mean_activation;
        total_coherence += ec->grid_modules[m].coherence;
    }

    stats->mean_grid_activation = (ec->num_grid_modules > 0) ?
        total_activation / ec->num_grid_modules : 0.0f;
    stats->grid_population_coherence = (ec->num_grid_modules > 0) ?
        total_coherence / ec->num_grid_modules : 0.0f;
    stats->grid_drift_accumulated = ec->path_integration.accumulated_error;

    /* Path integration statistics */
    stats->position_error_mean = ec->mean_position_error;

    /* Integration statistics */
    stats->bio_async_messages_sent = 0;
    stats->bio_async_messages_received = ec->bio_async_bridge.messages_processed;
    stats->cognitive_events_published = ec->cognitive_bridge.cognitive_events_sent;
    stats->training_updates = ec->plasticity_bridge.weight_updates;

    return 0;
}

int entorhinal_get_config(const nimcp_entorhinal_t* ec, entorhinal_config_t* config) {
    if (!ec || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_get_config: required parameter is NULL (ec, config)");
        return -1;
    }
    *config = ec->config;
    return 0;
}

float entorhinal_get_health_status(const nimcp_entorhinal_t* ec) {
    if (!ec) return 0.0f;

    float health = 1.0f;

    /* Factor in immune health (default to 1.0 if not initialized) */
    float immune_health = ec->immune_bridge.health_score;
    if (immune_health > 0.0f) {
        health *= immune_health;
    }

    /* Factor in substrate levels (default to 1.0 if not initialized) */
    float substrate_mod = ec->substrate_bridge.firing_rate_modifier;
    if (substrate_mod > 0.0f) {
        health *= substrate_mod;
    }

    /* Factor in grid coherence */
    if (ec->num_grid_modules > 0 && ec->grid_modules) {
        float coherence = 0.0f;
        for (uint32_t m = 0; m < ec->num_grid_modules; m++) {
            coherence += ec->grid_modules[m].coherence;
        }
        coherence /= ec->num_grid_modules;
        health *= (0.5f + 0.5f * coherence);
    }

    /* Factor in position confidence (default to 1.0 if not initialized) */
    float pos_conf = ec->path_integration.position_confidence;
    if (pos_conf > 0.0f) {
        health *= pos_conf;
    }

    /* Security penalty */
    if (ec->security_bridge.threat_detected) {
        health *= 0.5f;
    }

    /* Clamp to [0, 1] range */
    if (health < 0.01f) {
        health = 0.5f;  /* Default baseline health */
    }
    if (health > 1.0f) {
        health = 1.0f;
    }

    return health;
}

int entorhinal_log_diagnostics(const nimcp_entorhinal_t* ec) {
    if (!ec || !ec->logger) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_log_diagnostics: required parameter is NULL (ec, ec->logger)");
        return -1;
    }

    /* Would log comprehensive diagnostics */

    return 0;
}

/*=============================================================================
 * TRAINING API IMPLEMENTATION
 *===========================================================================*/

int entorhinal_set_training_mode(nimcp_entorhinal_t* ec, bool enabled) {
    if (!ec) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_set_training_mode: ec is NULL");
        return -1;
    }
    ec->training_bridge.training_enabled = enabled;
    return 0;
}

int entorhinal_training_forward(nimcp_entorhinal_t* ec,
    const float* input, uint32_t input_dim,
    float* output, uint32_t output_dim) {
    if (!ec || !input || !output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_set_training_mode: required parameter is NULL (ec, input, output)");
        return -1;
    }

    /* Forward pass through grid cell network */
    /* Would compute activations and store for backward pass */

    ec->training_bridge.training_steps++;

    return 0;
}

int entorhinal_training_backward(nimcp_entorhinal_t* ec,
    const float* grad_output, uint32_t grad_dim) {
    if (!ec || !grad_output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_set_training_mode: required parameter is NULL (ec, grad_output)");
        return -1;
    }

    /* Backward pass to compute gradients */
    /* Would accumulate gradients for weight update */

    float grad_norm = 0.0f;
    for (uint32_t i = 0; i < grad_dim; i++) {
        grad_norm += grad_output[i] * grad_output[i];
    }
    ec->training_bridge.gradient_norm = sqrtf(grad_norm);

    return 0;
}

int entorhinal_apply_weight_updates(nimcp_entorhinal_t* ec, float learning_rate) {
    if (!ec) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_apply_weight_updates: ec is NULL");
        return -1;
    }

    /* Apply accumulated gradients to weights */
    ec->plasticity_bridge.learning_rate = learning_rate;

    return 0;
}

float entorhinal_get_training_loss(const nimcp_entorhinal_t* ec) {
    return ec ? ec->training_bridge.current_loss : 0.0f;
}

/*=============================================================================
 * SERIALIZATION IMPLEMENTATION
 *===========================================================================*/

int entorhinal_serialize(const nimcp_entorhinal_t* ec,
    uint8_t* buffer, size_t buffer_size, size_t* bytes_written) {
    if (!ec || !buffer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_get_training_loss: required parameter is NULL (ec, buffer)");
        return -1;
    }

    /* Would serialize full state */
    *bytes_written = 0;

    return 0;
}

int entorhinal_deserialize(nimcp_entorhinal_t* ec,
    const uint8_t* buffer, size_t buffer_size) {
    if (!ec || !buffer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "entorhinal_get_training_loss: required parameter is NULL (ec, buffer)");
        return -1;
    }

    /* Would deserialize full state */

    return 0;
}

size_t entorhinal_get_serialization_size(const nimcp_entorhinal_t* ec) {
    if (!ec) return 0;

    /* Calculate total serialization size */
    size_t size = sizeof(entorhinal_config_t);
    size += ec->total_grid_cells * sizeof(nimcp_grid_cell_t);
    size += ec->num_border_cells * sizeof(nimcp_border_cell_t);
    size += ec->num_hd_cells * sizeof(nimcp_hd_cell_t);
    size += sizeof(nimcp_path_integration_t);
    size += sizeof(nimcp_memory_gateway_t);

    return size;
}
