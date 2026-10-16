/**
 * @file nimcp_brain_regions.c
 * @brief Implementation of modular brain architecture
 *
 * Provides hierarchical organization of neurons into:
 * - Brain regions (V1, A1, M1, etc.)
 * - Cortical layers (1-6)
 * - Minicolumns (vertical processing units)
 * - Inter-region connectivity
 */

#include "core/brain_regions/nimcp_brain_regions.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "utils/error/nimcp_error_codes.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"

#include "utils/memory/nimcp_unified_memory.h"
#include "core/neuron_types/nimcp_neuron_types.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/validation/nimcp_validate.h"
#include "utils/logging/nimcp_logging.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/thread/nimcp_atomic.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <pthread.h>

/* Forward declaration - defined in nimcp_brain_strategy.c */
extern void set_error(const char* format, ...);

#define LOG_MODULE "brain_regions"

#include <stddef.h>  /* for NULL */
#include "utils/thread/nimcp_thread.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(brain_regions)

// ============================================================================
// Bio-Async Module Context (Thread-Safe Initialization)
// ============================================================================

static bio_module_context_t bio_ctx = NULL;
static bool bio_async_enabled = false;
static pthread_once_t bio_init_once = PTHREAD_ONCE_INIT;
static nimcp_mutex_t bio_cleanup_mutex = NIMCP_MUTEX_INITIALIZER;

static void brain_regions_bio_init_impl(void) {
    if (!bio_router_is_initialized()) {
        return;
    }

    bio_module_info_t bio_info = {
        .module_id = BIO_MODULE_BRAIN_REGION,
        .module_name = "brain_regions",
        .inbox_capacity = 128,
        .user_data = NULL
    };

    bio_ctx = bio_router_register_module(&bio_info);
    if (bio_ctx) {
        bio_async_enabled = true;
        LOG_INFO(LOG_MODULE, "Bio-async registered for brain_regions module");
    }
}

__attribute__((constructor))
static void brain_regions_bio_init(void) {
    pthread_once(&bio_init_once, brain_regions_bio_init_impl);
}

__attribute__((destructor))
static void brain_regions_bio_cleanup(void) {
    nimcp_mutex_lock(&bio_cleanup_mutex);
    if (bio_async_enabled && bio_ctx) {
        bio_router_unregister_module(bio_ctx);
        bio_ctx = NULL;
        bio_async_enabled = false;
        LOG_DEBUG(LOG_MODULE, "Bio-async unregistered for brain_regions module");
    }
    nimcp_mutex_unlock(&bio_cleanup_mutex);
}

// ============================================================================
// INTERNAL HELPER FUNCTIONS
// ============================================================================

/**
 * @brief Generate unique region ID (thread-safe)
 *
 * THREAD SAFETY: Uses atomic fetch-add to ensure unique IDs across threads
 */
static nimcp_atomic_uint32_t g_region_id_counter = {1};

static uint32_t generate_region_id(void) {
    // Atomically increment and return the previous value (which becomes the ID)
    return nimcp_atomic_fetch_add_u32(&g_region_id_counter, 1, NIMCP_MEMORY_ORDER_SEQ_CST);
}

/**
 * @brief Get default layer proportions for region type
 *
 * Based on actual cortical layer thickness measurements
 */
static void get_layer_proportions(brain_region_type_t type, float proportions[LAYER_COUNT]) {
    // Default cortical proportions (approximate)
    switch (type) {
        case REGION_VISUAL_V1:
            // V1 has prominent Layer 4 (input from LGN)
            proportions[LAYER_1] = 0.05F;  // Thin
            proportions[LAYER_2] = 0.10F;
            proportions[LAYER_3] = 0.15F;
            proportions[LAYER_4] = 0.35F;  // Thick in V1
            proportions[LAYER_5] = 0.20F;
            proportions[LAYER_6] = 0.15F;
            break;

        case REGION_AUDITORY_A1:
            // A1 also has prominent Layer 4
            proportions[LAYER_1] = 0.05F;
            proportions[LAYER_2] = 0.12F;
            proportions[LAYER_3] = 0.18F;
            proportions[LAYER_4] = 0.30F;
            proportions[LAYER_5] = 0.20F;
            proportions[LAYER_6] = 0.15F;
            break;

        case REGION_MOTOR_M1:
            // M1 has prominent Layer 5 (output to spinal cord)
            proportions[LAYER_1] = 0.05F;
            proportions[LAYER_2] = 0.10F;
            proportions[LAYER_3] = 0.15F;
            proportions[LAYER_4] = 0.15F;
            proportions[LAYER_5] = 0.35F;  // Thick in M1
            proportions[LAYER_6] = 0.20F;
            break;

        case REGION_PREFRONTAL:
            // Prefrontal has large Layer 3 (associative)
            proportions[LAYER_1] = 0.05F;
            proportions[LAYER_2] = 0.15F;
            proportions[LAYER_3] = 0.30F;  // Thick
            proportions[LAYER_4] = 0.15F;
            proportions[LAYER_5] = 0.20F;
            proportions[LAYER_6] = 0.15F;
            break;

        default:
            // Standard cortical proportions
            proportions[LAYER_1] = 0.05F;
            proportions[LAYER_2] = 0.15F;
            proportions[LAYER_3] = 0.20F;
            proportions[LAYER_4] = 0.20F;
            proportions[LAYER_5] = 0.25F;
            proportions[LAYER_6] = 0.15F;
            break;
    }
}

/**
 * @brief Get appropriate neuron type for layer
 */
static neuron_type_extended_t get_layer_neuron_type(brain_region_type_t region_type,
                                                      cortical_layer_t layer) {
    switch (region_type) {
        case REGION_VISUAL_V1:
        case REGION_VISUAL_V2:
        case REGION_VISUAL_V4:
            // Visual cortex uses specialized visual neurons
            if (layer == LAYER_4) {
                return NEURON_VISUAL_EDGE; // Input layer: edge detectors
            } else if (layer == LAYER_2 || layer == LAYER_3) {
                return NEURON_VISUAL_ORIENTATION; // Orientation-selective
            } else if (layer == LAYER_5 || layer == LAYER_6) {
                return NEURON_PYRAMIDAL_L5_THICK; // Output layers
            }
            break;

        case REGION_VISUAL_MT:
            return NEURON_VISUAL_DIRECTION; // Motion-selective

        case REGION_AUDITORY_A1:
        case REGION_AUDITORY_A2:
            // Auditory cortex uses frequency-tuned neurons
            if (layer == LAYER_4) {
                return NEURON_AUDITORY_FREQUENCY; // Frequency-tuned input
            } else if (layer == LAYER_2 || layer == LAYER_3) {
                return NEURON_AUDITORY_ONSET; // Temporal features
            }
            break;

        case REGION_MOTOR_M1:
            if (layer == LAYER_5) {
                return NEURON_MOTOR_ALPHA; // Motoneurons
            } else if (layer == LAYER_2 || layer == LAYER_3) {
                return NEURON_MOTOR_PATTERN_GEN; // Pattern generators
            }
            break;

        default:
            break;
    }

    // Default: pyramidal neurons
    if (layer == LAYER_2 || layer == LAYER_3) {
        return NEURON_PYRAMIDAL_L23;
    } else if (layer == LAYER_5) {
        return NEURON_PYRAMIDAL_L5_THICK;
    } else if (layer == LAYER_6) {
        return NEURON_PYRAMIDAL_L6;
    }

    return NEURON_EXCITATORY; // Fallback
}

// ============================================================================
// BRAIN MODULE MANAGEMENT
// ============================================================================

brain_module_t* brain_module_create(uint32_t max_regions) {
    brain_module_t* brain = (brain_module_t*)nimcp_calloc(1, sizeof(brain_module_t));
    if (!brain) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "brain is NULL");

        return NULL;

    }

    brain->id = 1; // Fixed ID for now
    brain->max_regions = max_regions;

    brain->regions = (brain_region_t**)nimcp_calloc(max_regions, sizeof(brain_region_t*));
    if (!brain->regions) {
        nimcp_free(brain);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "brain_module_create: brain->regions is NULL");
        return NULL;
    }

    brain->connections = NULL; // Allocated on demand
    brain->num_connections = 0;

    brain->enable_plasticity = true;
    brain->enable_glial = true;

    nimcp_mutex_init(&brain->lock, NULL);

    return brain;
}

void brain_module_destroy(brain_module_t* brain) {
    if (!brain) return;

    // Destroy all regions
    for (uint32_t i = 0; i < brain->num_regions; i++) {
        brain_region_destroy(brain->regions[i]);
    }

    nimcp_free(brain->regions);

    // Destroy connections
    if (brain->connections) {
        for (uint32_t i = 0; i < brain->num_connections; i++) {
            nimcp_free(brain->connections[i]);
        }
        nimcp_free(brain->connections);
        brain->connections = NULL;  // BUGFIX: Prevent use-after-free
        brain->num_connections = 0;
    }

    nimcp_mutex_destroy(&brain->lock);
    nimcp_free(brain);
}

nimcp_result_t brain_module_add_region(brain_module_t* brain, brain_region_t* region) {
    if (!brain || !region) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (brain->num_regions >= brain->max_regions) {
        return NIMCP_ERROR_OUT_OF_RANGE; // Full
    }

    nimcp_mutex_lock(&brain->lock);

    brain->regions[brain->num_regions] = region;
    brain->num_regions++;
    brain->total_neurons += region->total_neurons;

    nimcp_mutex_unlock(&brain->lock);

    return NIMCP_SUCCESS;
}

brain_region_t* brain_module_get_region(brain_module_t* brain, uint32_t region_id) {
    if (!brain) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain is NULL");

        return NULL;

    }

    for (uint32_t i = 0; i < brain->num_regions; i++) {
        if (brain->regions[i]->id == region_id) {
            return brain->regions[i];
        }
    }

    set_error("brain_module_get_region: region_id %u not found", region_id);
    return NULL;
}

brain_region_t* brain_module_get_region_by_type(brain_module_t* brain,
                                                 brain_region_type_t type) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_module_get_region_by_type: brain is NULL");
        return NULL;
    }

    for (uint32_t i = 0; i < brain->num_regions; i++) {
        if (brain->regions[i]->type == type) {
            return brain->regions[i];
        }
    }

    set_error("brain_module_get_region_by_type: type %d not found", (int)type);
    return NULL;
}

// ============================================================================
// BRAIN REGION MANAGEMENT
// ============================================================================

brain_region_t* brain_region_create(brain_region_type_t type, uint32_t num_neurons) {
    brain_region_t* region = (brain_region_t*)nimcp_calloc(1, sizeof(brain_region_t));
    if (!region) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "region is NULL");

        return NULL;

    }

    region->id = generate_region_id();
    region->type = type;
    region->total_neurons = num_neurons;

    // Set human-readable name
    const char* name = brain_region_get_name(type);
    strncpy(region->name, name, sizeof(region->name) - 1);

    // Create underlying neural network using public API
    network_config_t config = {0};
    config.num_neurons = num_neurons;
    config.ei_ratio = 0.8F;  // 80% excitatory
    config.learning_rate = 0.01F;
    config.target_activity = 0.1F;
    config.min_weight = -1.0F;
    config.max_weight = 1.0F;
    // REQUIRED: Set input/output dimensions (needed by neural_network_create validation)
    config.input_size = 100;  // Default input dimension for brain regions
    config.output_size = 10;  // Default output dimension for brain regions

    region->network = neural_network_create(&config);
    if (!region->network) {
        nimcp_free(region);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "brain_region_create: region->network is NULL");
        return NULL;
    }

    // Organize into layers with biologically realistic proportions
    float proportions[LAYER_COUNT];
    get_layer_proportions(type, proportions);

    for (int layer = 0; layer < LAYER_COUNT; layer++) {
        region->layer_sizes[layer] = (uint32_t)(num_neurons * proportions[layer]);
    }

    // Adjust for rounding errors - give extra neurons to Layer 4 (input layer)
    uint32_t allocated = 0;
    for (int layer = 0; layer < LAYER_COUNT; layer++) {
        allocated += region->layer_sizes[layer];
    }
    if (allocated < num_neurons) {
        region->layer_sizes[LAYER_4] += (num_neurons - allocated);
    }

    // Allocate extended neuron type arrays
    region->neuron_extended_types = (neuron_type_extended_t*)nimcp_calloc(num_neurons, sizeof(neuron_type_extended_t));
    region->neuron_type_params = (neuron_type_params_t*)nimcp_calloc(num_neurons, sizeof(neuron_type_params_t));
    if (!region->neuron_extended_types || !region->neuron_type_params) {
        nimcp_free(region->neuron_extended_types);
        nimcp_free(region->neuron_type_params);
        neural_network_destroy(region->network);
        nimcp_free(region);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_region_create: required parameter is NULL (region->neuron_extended_types, region->neuron_type_params)");
        return NULL;
    }

    // Assign neuron types based on region and layer
    uint32_t neuron_idx = 0;
    for (int layer = 0; layer < LAYER_COUNT; layer++) {
        for (uint32_t i = 0; i < region->layer_sizes[layer]; i++) {
            if (neuron_idx < num_neurons) {
                // Get specialized type for this layer
                neuron_type_extended_t ext_type = get_layer_neuron_type(type, (cortical_layer_t)layer);
                region->neuron_extended_types[neuron_idx] = ext_type;

                // Get default parameters for this type
                neuron_type_get_default_params(ext_type, &region->neuron_type_params[neuron_idx]);

                neuron_idx++;
            }
        }
    }

    region->minicolumns = NULL;
    region->num_minicolumns = 0;

    region->activity_level = 0.0F;
    region->last_update = 0;

    nimcp_mutex_init(&region->lock, NULL);

    return region;
}

void brain_region_destroy(brain_region_t* region) {
    if (!region) return;

    // Destroy minicolumns
    if (region->minicolumns) {
        for (uint32_t i = 0; i < region->num_minicolumns; i++) {
            if (region->minicolumns[i]) {
                for (int layer = 0; layer < LAYER_COUNT; layer++) {
                    nimcp_free(region->minicolumns[i]->layer_neuron_ids[layer]);
                }
                nimcp_free(region->minicolumns[i]);
            }
        }
        nimcp_free(region->minicolumns);
    }

    // Free extended neuron types
    nimcp_free(region->neuron_extended_types);
    nimcp_free(region->neuron_type_params);

    // Destroy glial integration
    if (region->glial) {
        glial_integration_destroy(region->glial);
        region->glial = NULL;  // BUGFIX: Prevent use-after-free
    }

    // Destroy neural network
    if (region->network) {
        neural_network_destroy(region->network);
        region->network = NULL;  // BUGFIX: Prevent use-after-free
    }

    nimcp_free(region->input_regions);
    nimcp_free(region->output_regions);

    nimcp_mutex_destroy(&region->lock);
    nimcp_free(region);
}

nimcp_result_t brain_region_organize_columns(brain_region_t* region,
                                               uint32_t columns_x,
                                               uint32_t columns_y) {
    if (!region || columns_x == 0 || columns_y == 0) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    uint32_t num_columns = columns_x * columns_y;

    region->minicolumns = (brain_minicolumn_t**)nimcp_calloc(num_columns, sizeof(brain_minicolumn_t*));
    if (!region->minicolumns) {
        return NIMCP_ERROR_NO_MEMORY;
    }

    region->num_minicolumns = num_columns;
    region->minicolumns_x = columns_x;
    region->minicolumns_y = columns_y;

    uint32_t neurons_per_column = region->total_neurons / num_columns;

    // Create each minicolumn
    for (uint32_t col_idx = 0; col_idx < num_columns; col_idx++) {
        brain_minicolumn_t* col = (brain_minicolumn_t*)nimcp_calloc(1, sizeof(brain_minicolumn_t));
        if (!col) continue;

        col->id = col_idx;
        col->region_id = region->id;

        // Position in 2D grid
        col->x = (float)(col_idx % columns_x) / (float)columns_x;
        col->y = (float)(col_idx / columns_x) / (float)columns_y;

        // Distribute neurons across layers proportionally
        uint32_t neuron_offset = col_idx * neurons_per_column;

        for (int layer = 0; layer < LAYER_COUNT; layer++) {
            uint32_t layer_neurons_in_col = region->layer_sizes[layer] / num_columns;
            if (layer_neurons_in_col > 0) {
                col->layer_neuron_ids[layer] = (uint32_t*)nimcp_calloc(layer_neurons_in_col, sizeof(uint32_t));
                col->layer_neuron_counts[layer] = layer_neurons_in_col;

                // Assign neuron IDs
                for (uint32_t n = 0; n < layer_neurons_in_col; n++) {
                    col->layer_neuron_ids[layer][n] = neuron_offset + n;
                }
                neuron_offset += layer_neurons_in_col;
            } else {
                col->layer_neuron_ids[layer] = NULL;
                col->layer_neuron_counts[layer] = 0;
            }
        }

        region->minicolumns[col_idx] = col;
    }

    return NIMCP_SUCCESS;
}

uint32_t brain_region_get_layer_neurons(brain_region_t* region,
                                         cortical_layer_t layer,
                                         uint32_t* out_neuron_ids,
                                         uint32_t max_neurons) {
    if (!region || !out_neuron_ids || layer >= LAYER_COUNT) {
        return 0;
    }

    uint32_t layer_size = region->layer_sizes[layer];
    uint32_t count = (layer_size < max_neurons) ? layer_size : max_neurons;

    // Calculate starting neuron ID for this layer
    uint32_t start_id = 0;
    for (int l = 0; l < layer; l++) {
        start_id += region->layer_sizes[l];
    }

    for (uint32_t i = 0; i < count; i++) {
        out_neuron_ids[i] = start_id + i;
    }

    return count;
}

// ============================================================================
// INTER-REGION CONNECTIVITY
// ============================================================================

nimcp_result_t brain_module_connect_regions(brain_module_t* brain,
                                             uint32_t source_region_id,
                                             uint32_t target_region_id,
                                             float connection_density) {
    if (!brain) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    brain_region_t* source = brain_module_get_region(brain, source_region_id);
    brain_region_t* target = brain_module_get_region(brain, target_region_id);

    if (!source || !target) {
        return NIMCP_ERROR_NOT_FOUND; // Region not found
    }

    // Create connection record
    brain_connection_t* conn = (brain_connection_t*)nimcp_calloc(1, sizeof(brain_connection_t));
    if (!conn) {
        return NIMCP_ERROR_NO_MEMORY;
    }

    conn->source_region_id = source_region_id;
    conn->target_region_id = target_region_id;
    conn->connection_strength = connection_density;
    conn->feedforward = true;

    // Default: Layer 2/3 → Layer 4 (feedforward)
    conn->source_layer = LAYER_3;
    conn->target_layer = LAYER_4;

    // Add to brain's connection list
    brain_connection_t** new_connections = (brain_connection_t**)nimcp_realloc(
        brain->connections,
        (brain->num_connections + 1) * sizeof(brain_connection_t*));
    if (!new_connections) {
        nimcp_free(conn);
        return NIMCP_ERROR_NO_MEMORY;
    }
    brain->connections = new_connections;
    brain->connections[brain->num_connections] = conn;
    brain->num_connections++;

    return NIMCP_SUCCESS;
}

nimcp_result_t brain_module_connect_layers(brain_module_t* brain,
                                            uint32_t source_region_id,
                                            cortical_layer_t source_layer,
                                            uint32_t target_region_id,
                                            cortical_layer_t target_layer,
                                            float connection_density) {
    if (!brain) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    brain_connection_t* conn = (brain_connection_t*)nimcp_calloc(1, sizeof(brain_connection_t));
    if (!conn) {
        return NIMCP_ERROR_NO_MEMORY;
    }

    conn->source_region_id = source_region_id;
    conn->source_layer = source_layer;
    conn->target_region_id = target_region_id;
    conn->target_layer = target_layer;
    conn->connection_strength = connection_density;
    conn->feedforward = (target_layer == LAYER_4);

    brain_connection_t** new_connections = (brain_connection_t**)nimcp_realloc(
        brain->connections,
        (brain->num_connections + 1) * sizeof(brain_connection_t*));
    if (!new_connections) {
        nimcp_free(conn);
        return NIMCP_ERROR_NO_MEMORY;
    }
    brain->connections = new_connections;
    brain->connections[brain->num_connections] = conn;
    brain->num_connections++;

    return NIMCP_SUCCESS;
}

// ============================================================================
// SENSORY INPUT & PROCESSING
// ============================================================================

nimcp_result_t brain_region_process_input(brain_region_t* region,
                                           const float* input,
                                           uint32_t input_size,
                                           uint64_t timestamp) {
    if (!region || !input) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    // Distribute input to Layer 4 neurons (thalamic input layer)
    uint32_t layer4_size = region->layer_sizes[LAYER_4];
    if (layer4_size == 0) {
        return NIMCP_ERROR_INVALID_STATE; // No input layer
    }

    // Calculate starting index for Layer 4
    uint32_t layer4_start = 0;
    for (int l = 0; l < LAYER_4; l++) {
        layer4_start += region->layer_sizes[l];
    }

    // Distribute input across Layer 4 neurons using public API
    for (uint32_t i = 0; i < layer4_size && i < input_size; i++) {
        uint32_t neuron_idx = layer4_start + i;
        if (neuron_idx < region->total_neurons) {
            float current_state = 0.0F;
            neural_network_get_neuron_state(region->network, neuron_idx, &current_state);
            float new_state = current_state + input[i % input_size];
            neural_network_update_neuron(region->network, neuron_idx, new_state, timestamp);
        }
    }

    region->last_update = timestamp;

    return NIMCP_SUCCESS;
}

uint32_t brain_region_get_output(brain_region_t* region,
                                  float* output,
                                  uint32_t output_size) {
    if (!region || !output) {
        return 0;
    }

    // Collect activity from Layer 5 neurons (output layer)
    uint32_t layer5_size = region->layer_sizes[LAYER_5];
    if (layer5_size == 0) {
        return 0;
    }

    // Calculate starting index for Layer 5
    uint32_t layer5_start = 0;
    for (int l = 0; l < LAYER_5; l++) {
        layer5_start += region->layer_sizes[l];
    }

    uint32_t count = (layer5_size < output_size) ? layer5_size : output_size;

    for (uint32_t i = 0; i < count; i++) {
        uint32_t neuron_idx = layer5_start + i;
        if (neuron_idx < region->total_neurons) {
            neural_network_get_neuron_state(region->network, neuron_idx, &output[i]);
        }
    }

    return count;
}

// ============================================================================
// SIMULATION & STEPPING
// ============================================================================

/**
 * @brief Propagate activity signals between brain regions via connections
 *
 * WHAT: Transfer activity from source to target regions through inter-region connections
 * WHY:  Enable hierarchical processing and information flow across cortical areas
 * HOW:  For each connection, compute signal strength and inject into target layer
 *
 * BIOLOGICAL RATIONALE:
 * - Cortical areas communicate via cortico-cortical projections
 * - Different layers project to different target layers (feedforward/feedback)
 * - Connection strength represents axonal density and synaptic efficacy
 * - Activity propagates as weighted sum of source neuron firing rates
 *
 * PERFORMANCE: O(num_connections * avg_neurons_per_layer)
 *              Typically O(num_regions²) for fully connected modules
 *
 * @param brain Brain module containing regions and connections
 * @param delta_t Time step in microseconds
 * @return NIMCP_SUCCESS on success, error code otherwise
 */
static nimcp_result_t propagate_inter_region_signals(brain_module_t* brain, uint64_t delta_t) {
    if (!brain) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    for (uint32_t c = 0; c < brain->num_connections; c++) {
        brain_connection_t* conn = brain->connections[c];
        if (!conn) continue;

        // Find source and target regions
        brain_region_t* source = NULL;
        brain_region_t* target = NULL;

        for (uint32_t i = 0; i < brain->num_regions; i++) {
            if (brain->regions[i]->id == conn->source_region_id) {
                source = brain->regions[i];
            }
            if (brain->regions[i]->id == conn->target_region_id) {
                target = brain->regions[i];
            }
        }

        // Validate connection endpoints
        if (!source || !target || !source->network || !target->network) {
            continue;
        }

        // Compute signal strength = source_activity * connection_strength
        float signal = source->activity_level * conn->connection_strength;
        signal = fmaxf(0.0F, fminf(1.0F, signal));  // Clamp to [0, 1]

        if (signal < 0.01F) {
            continue;  // Skip negligible signals
        }

        // Compute target layer boundaries
        uint32_t layer_start = 0;
        for (uint32_t layer = 0; layer < (uint32_t)conn->target_layer; layer++) {
            layer_start += target->layer_sizes[layer];
        }
        uint32_t layer_end = layer_start + target->layer_sizes[conn->target_layer];

        // Inject signal into target neurons
        for (uint32_t nid = layer_start; nid < layer_end; nid++) {
            float current = signal * (float)conn->num_synapses / 100.0F;
            neural_network_update_neuron(target->network, nid, current, delta_t);
        }
    }

    return NIMCP_SUCCESS;
}

nimcp_result_t brain_module_step(brain_module_t* brain, uint64_t delta_t) {
    if (!brain) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    // Process pending bio-async messages
    if (bio_async_enabled && bio_ctx) {
        bio_router_process_inbox(bio_ctx, 5);
    }

    brain->current_time += delta_t;

    // Step all regions
    for (uint32_t i = 0; i < brain->num_regions; i++) {
        brain_region_step(brain->regions[i], delta_t);
    }

    // Propagate signals between regions
    return propagate_inter_region_signals(brain, delta_t);
}

nimcp_result_t brain_region_step(brain_region_t* region, uint64_t delta_t) {
    if (!region) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    // Update region timestamp
    region->last_update += delta_t;

    // IMPLEMENTATION: Step underlying neural network
    // WHAT: Compute neural activity for this time step
    // WHY:  Neurons process inputs and generate spikes
    // HOW:  Call neural_network_compute_step() with delta_t
    //
    // RATIONALE: Neural network stepping is the core computation of each brain region.
    //            Activity level is tracked as an exponential moving average (EMA) of
    //            spike rate to smooth out instantaneous fluctuations.
    if (region->network) {
        // Compute network step with the given delta_t
        uint32_t spikes = neural_network_compute_step(region->network, delta_t);

        // Update activity level based on spiking activity
        // Activity = (spikes / total_neurons) smoothed with exponential moving average
        if (region->total_neurons > 0) {
            float spike_rate = (float)spikes / (float)region->total_neurons;
            // EMA with alpha=0.1 for smoothing
            region->activity_level = 0.9F * region->activity_level + 0.1F * spike_rate;
        }
    }

    // IMPLEMENTATION: Update glial cells
    // WHAT: Step astrocytes, microglia, and oligodendrocytes
    // WHY:  Glial cells modulate synaptic transmission, provide metabolic support,
    //       and prune weak connections
    // HOW:  Call glial_integration_step() with current timestamp
    //
    // RATIONALE: Glial integration handles all glial-neural interactions including:
    //            - Astrocyte calcium dynamics and synaptic modulation (0.8x - 1.2x)
    //            - Oligodendrocyte adaptive myelination (reduces conduction delay)
    //            - Microglia activity monitoring and synaptic pruning
    //            The modulation effects are automatically applied to the network.
    if (region->glial) {
        // Update all glial cells and apply modulation to network
        glial_integration_step(region->glial, region->last_update);
    }

    return NIMCP_SUCCESS;
}

// ============================================================================
// STATISTICS & MONITORING
// ============================================================================

nimcp_result_t brain_region_get_stats(brain_region_t* region,
                                       brain_region_stats_t* stats) {
    if (!region || !stats) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    memset(stats, 0, sizeof(brain_region_stats_t));

    stats->total_neurons = region->total_neurons;
    stats->num_minicolumns = region->num_minicolumns;

    // Calculate activity per layer
    for (int layer = 0; layer < LAYER_COUNT; layer++) {
        uint32_t layer_start = 0;
        for (int l = 0; l < layer; l++) {
            layer_start += region->layer_sizes[l];
        }

        float layer_activity = 0.0F;
        uint32_t active_count = 0;
        for (uint32_t i = 0; i < region->layer_sizes[layer]; i++) {
            uint32_t neuron_idx = layer_start + i;
            if (neuron_idx < region->total_neurons) {
                float state = 0.0F;
                neural_network_get_neuron_state(region->network, neuron_idx, &state);
                // Normalize: treat state > threshold (1.0) as active
                // Use sigmoid-like normalization: tanh(state) to map to [0,1]
                float normalized = tanhf(fabsf(state));
                layer_activity += normalized;
                if (fabsf(state) > 0.5F) {
                    active_count++;
                }
            }
        }

        if (region->layer_sizes[layer] > 0) {
            stats->layer_activity[layer] = layer_activity / region->layer_sizes[layer];
        }
    }

    // Overall average
    float total_activity = 0.0F;
    for (int l = 0; l < LAYER_COUNT; l++) {
        total_activity += stats->layer_activity[l];
    }
    stats->avg_activity = total_activity / LAYER_COUNT;

    return NIMCP_SUCCESS;
}

const char* brain_region_get_name(brain_region_type_t type) {
    switch (type) {
        case REGION_VISUAL_V1:       return "Primary Visual Cortex (V1)";
        case REGION_VISUAL_V2:       return "Secondary Visual Cortex (V2)";
        case REGION_VISUAL_V4:       return "Visual Area V4";
        case REGION_VISUAL_MT:       return "Middle Temporal (MT/V5)";
        case REGION_VISUAL_IT:       return "Inferior Temporal (IT)";

        case REGION_AUDITORY_A1:     return "Primary Auditory Cortex (A1)";
        case REGION_AUDITORY_A2:     return "Secondary Auditory Cortex (A2)";
        case REGION_AUDITORY_BELT:   return "Auditory Belt Region";
        case REGION_AUDITORY_PARABELT: return "Auditory Parabelt";

        case REGION_MOTOR_M1:        return "Primary Motor Cortex (M1)";
        case REGION_MOTOR_PREMOTOR:  return "Premotor Cortex";
        case REGION_MOTOR_SMA:       return "Supplementary Motor Area (SMA)";

        case REGION_SOMATOSENSORY_S1: return "Primary Somatosensory Cortex (S1)";
        case REGION_SOMATOSENSORY_S2: return "Secondary Somatosensory Cortex (S2)";

        case REGION_PREFRONTAL:      return "Prefrontal Cortex (PFC)";
        case REGION_PARIETAL:        return "Parietal Cortex";
        case REGION_TEMPORAL:        return "Temporal Cortex";

        case REGION_THALAMUS:        return "Thalamus";
        case REGION_HIPPOCAMPUS:     return "Hippocampus";
        case REGION_BASAL_GANGLIA:   return "Basal Ganglia";
        case REGION_CEREBELLUM:      return "Cerebellum";

        default:                     return "Unknown Region";
    }
}

// ============================================================================
// KNOWLEDGE GRAPH SELF-AWARENESS INTEGRATION
// ============================================================================

/**
 * @brief Query self-knowledge from knowledge graph
 *
 * WHAT: Allow brain regions module to introspect its own structure and capabilities
 * WHY:  Enable self-awareness - the system can query what brain regions it has
 * HOW:  Use KG reader to look up Brain_Regions entity and related region entities
 *
 * @param kg Knowledge graph reader instance
 * @return 1 if self-knowledge found, 0 otherwise
 */
int brain_regions_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    // Query for our own module entity
    const kg_entity_t* self = kg_reader_get_entity(kg, "Brain_Regions");
    if (self) {
        // Brain regions module now has access to its documented structure
        LOG_DEBUG(LOG_MODULE, "Self-knowledge found: %s (%u observations)",
                  self->name, self->num_observations);
    }

    // Query all region-related entities for enumeration
    kg_entity_list_t* regions = kg_reader_search_entities(kg, "region");
    if (regions) {
        LOG_DEBUG(LOG_MODULE, "Found %u region-related entities in KG", regions->count);
        kg_entity_list_destroy(regions);
    }

    // Query for cortical layer information
    kg_entity_list_t* layers = kg_reader_search_entities(kg, "layer");
    if (layers) {
        LOG_DEBUG(LOG_MODULE, "Found %u layer-related entities in KG", layers->count);
        kg_entity_list_destroy(layers);
    }

    return self ? 1 : 0;
}

/**
 * @brief Get module capabilities from knowledge graph
 *
 * @param kg Knowledge graph reader instance
 * @return Capability description string or NULL
 */
const char* brain_regions_get_capabilities(kg_reader_t* kg) {
    if (!kg) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg is NULL");

        return NULL;

    }
    return kg_reader_get_module_capabilities(kg, "Brain_Regions");
}

/**
 * @brief Get module integrations from knowledge graph
 *
 * @param kg Knowledge graph reader instance
 * @return Relation list showing integrations (caller must free)
 */
kg_relation_list_t* brain_regions_get_integrations(kg_reader_t* kg) {
    if (!kg) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kg is NULL");

        return NULL;

    }
    return kg_reader_get_module_integrations(kg, "Brain_Regions");
}
