/**
 * @file nimcp_cerebellum_adapter.c
 * @brief Implementation of Cerebellum brain adapter
 *
 * WHAT: Unified adapter connecting cerebellum sub-modules to the brain system
 * WHY:  Enable motor coordination, timing, and error-based learning
 * HOW:  Orchestrates granule cells, Purkinje cells, and deep nuclei
 *
 * @version Phase B4: Cerebellum Brain Integration
 * @date 2025-12-30
 */

#include "core/brain/regions/cerebellum/nimcp_cerebellum_adapter.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/memory/nimcp_memory_pool.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/thread/nimcp_thread.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(cerebellum_adapter, MESH_ADAPTER_CATEGORY_COGNITIVE)


/*=============================================================================
 * LOGGING MODULE IDENTIFIER
 *===========================================================================*/

#define CEREBELLUM_LOG_MODULE "CEREBELLUM"

/*=============================================================================
 * INTERNAL STRUCTURES
 *===========================================================================*/

/**
 * @brief Granule cell state
 */
typedef struct {
    float activation;                /**< Current activation [0, 1] */
    float* mossy_weights;            /**< Weights from mossy fibers */
    uint32_t num_mossy_inputs;       /**< Number of mossy inputs */
    bool active;                     /**< Currently firing */
} granule_cell_t;

/**
 * @brief Purkinje cell state
 */
typedef struct {
    float simple_spike_rate;         /**< Simple spike firing rate */
    float complex_spike;             /**< Complex spike flag */
    float* parallel_weights;         /**< Weights from parallel fibers */
    uint32_t num_parallel_inputs;    /**< Number of parallel inputs */
    float inhibition_output;         /**< Output to deep nuclei */
    float learning_eligibility;      /**< Eligibility trace for learning */
} purkinje_cell_t;

/**
 * @brief Deep nucleus neuron state
 */
typedef struct {
    float activity;                  /**< Neuron activity level */
    float* purkinje_weights;         /**< Weights from Purkinje cells */
    uint32_t num_purkinje_inputs;    /**< Number of Purkinje inputs */
    float motor_output[8];           /**< Motor command output */
} nucleus_neuron_t;

/**
 * @brief Granule layer (internal implementation)
 */
struct granule_layer {
    granule_cell_t* cells;           /**< Granule cells array */
    uint32_t num_cells;              /**< Number of cells */
    float* mossy_input_buffer;       /**< Input from mossy fibers */
    uint32_t num_mossy_fibers;       /**< Number of mossy fibers */
    float sparse_coding_threshold;   /**< Threshold for sparse activation */
};

/**
 * @brief Purkinje layer (internal implementation)
 */
struct purkinje_layer {
    purkinje_cell_t* cells;          /**< Purkinje cells array */
    uint32_t num_cells;              /**< Number of cells */
    float* parallel_input_buffer;    /**< Input from parallel fibers */
    float ltd_rate;                  /**< LTD learning rate */
    float ltp_rate;                  /**< LTP learning rate */
};

/**
 * @brief Deep nuclei (internal implementation)
 */
struct deep_nuclei {
    nucleus_neuron_t* dentate;       /**< Dentate nucleus neurons */
    uint32_t num_dentate;            /**< Number of dentate neurons */
    nucleus_neuron_t* interposed;    /**< Interposed nucleus neurons */
    uint32_t num_interposed;         /**< Number of interposed neurons */
    nucleus_neuron_t* fastigial;     /**< Fastigial nucleus neurons */
    uint32_t num_fastigial;          /**< Number of fastigial neurons */
    float* output_buffer;            /**< Output command buffer */
};

/**
 * @brief Climbing fiber system (internal implementation)
 */
struct climbing_fiber_system {
    float* error_signals;            /**< Current error signals */
    uint32_t num_fibers;             /**< Number of climbing fibers */
    uint32_t* purkinje_targets;      /**< Target Purkinje cells */
    float error_threshold;           /**< Threshold to trigger learning */
};

/*=============================================================================
 * PHASE 1-2: NEW INTERNAL STRUCTURES
 *===========================================================================*/

/**
 * @brief Glomerular layer (Phase 1) - mossy fiber divergence
 */
struct glomerular_layer {
    glomerulus_t* glomeruli;         /**< Array of glomeruli */
    uint32_t num_glomeruli;          /**< Number of glomeruli (one per mossy fiber) */
    uint32_t granules_per_glomerulus;/**< Target divergence (50-100) */
    float divergence_factor;         /**< Actual divergence achieved */
    bool synaptic_dynamics_enabled;  /**< Whether to use synaptic dynamics */
};

/**
 * @brief Molecular layer interneurons (Phase 2) - basket and stellate cells
 */
struct molecular_layer_interneurons {
    basket_cell_t* basket_cells;     /**< Basket cell array */
    uint32_t num_basket_cells;       /**< Number of basket cells */
    stellate_cell_t* stellate_cells; /**< Stellate cell array */
    uint32_t num_stellate_cells;     /**< Number of stellate cells */
    float* parallel_input_buffer;    /**< Input from parallel fibers */
    float inhibition_strength;       /**< Global inhibition gain */
};

/**
 * @brief Golgi cell network (Phase 2) - feedback inhibition to granule layer
 */
struct golgi_cell_network {
    golgi_cell_t* cells;             /**< Golgi cell array */
    uint32_t num_cells;              /**< Number of Golgi cells */
    float feedback_delay_ms;         /**< Delay for feedback inhibition */
    float inhibition_decay_tau;      /**< Decay time constant (ms) */
    float* granule_activity_buffer;  /**< Granule cell activity feedback */
};

/**
 * @brief Metabolic modulation state (Phase 4)
 */
typedef struct {
    float motor_coordination;        /**< Scaling for motor coordination [0.3, 1.0] */
    float timing_precision;          /**< Scaling for timing [0.3, 1.0] */
    float learning_capacity;         /**< Scaling for learning [0.3, 1.0] */
    bool modulation_active;          /**< Whether modulation is applied */
} metabolic_modulation_t;

/**
 * @brief Forward model internal state
 */
typedef struct {
    float* weights;                  /**< Model weights */
    uint32_t input_dim;              /**< Input dimension */
    uint32_t output_dim;             /**< Output dimension */
    float learning_rate;             /**< Model learning rate */
    float last_prediction[8];        /**< Last prediction */
    float last_error;                /**< Last prediction error */
} forward_model_t;

/**
 * @brief Timing controller internal state
 */
typedef struct {
    float target_timing_ms;          /**< Target timing */
    float predicted_timing_ms;       /**< Predicted timing */
    float actual_timing_ms;          /**< Actual timing */
    float timing_error_history[16];  /**< Rolling error history */
    uint32_t error_history_idx;      /**< Current history index */
    float adaptation_rate;           /**< Timing adaptation rate */
} timing_controller_t;

/**
 * @brief Internal adapter structure
 */
struct cerebellum_adapter {
    /* Configuration */
    cerebellum_config_t config;

    /* Sub-modules */
    granule_layer_t* granule;
    purkinje_layer_t* purkinje;
    deep_nuclei_t* nuclei;
    climbing_fiber_system_t* climbing;

    /* Phase 1: New sub-modules */
    glomerular_layer_t* glomerular;              /**< Glomerular layer for divergence */

    /* Phase 2: Interneurons */
    molecular_layer_interneurons_t* molecular;   /**< Basket + stellate cells */
    golgi_cell_network_t* golgi;                 /**< Golgi feedback network */

    /* Phase 4: Metabolic modulation */
    metabolic_modulation_t metabolic;            /**< Metabolic state from substrate bridge */

    /* Forward model */
    forward_model_t forward_model;

    /* Timing controller */
    timing_controller_t timing;

    /* Output buffers */
    nuclei_output_t* output_buffer;
    uint32_t output_count;
    uint32_t output_capacity;

    /* Callbacks */
    cerebellum_motor_callback_t motor_callback;
    void* motor_user_data;
    cerebellum_error_callback_t error_callback;
    void* error_user_data;
    cerebellum_event_callback_t event_callback;
    void* event_user_data;

    /* State */
    cerebellum_status_t status;
    cerebellum_error_t last_error;
    double current_time_ms;
    float adaptation_level;

    /* Bio-async communication context */
    bio_module_context_t bio_ctx;

    /* Aggregate synapse tracking (used when glomerular layer not present) */
    cerebellar_synapse_state_t aggregate_synapse;

    /* Statistics */
    cerebellum_stats_t stats;

    /* S7-H4 fix (2026-05-19) — internal mutex.
     * Pre-fix the adapter had zero locking. cerebellum_predict_outcome
     * reads forward_model.weights; cerebellum_update_forward_model
     * reads+writes weights; cerebellum_broadcast_error writes
     * climbing.error_signals[] + Purkinje cell state. Slice 7 wires 3
     * concurrent callers from the cascade (motor + prosody + state
     * updates), and the brain-tick driver thread calls
     * cerebellum_process_bio_messages on its own thread. Race window
     * was real.
     * Lock-ordering: innermost. The cerebellum has no callers that hold
     * bridge/grounded_lang mutexes; bio-async dispatcher serializes its
     * own queue. Acquire inside predict / update / broadcast; never
     * acquire while holding any other adapter-level lock. */
    nimcp_mutex_t* mutex;
};

/*=============================================================================
 * INTERNAL HELPERS
 *===========================================================================*/

/**
 * @brief Set error state
 */
static void set_error(cerebellum_adapter_t* adapter, cerebellum_error_t error) {
    if (!adapter) return;
    adapter->last_error = error;
    if (error != CEREBELLUM_ERROR_NONE) {
        adapter->status = CEREBELLUM_STATUS_ERROR;
        LOG_ERROR("[%s] Error set: %d", CEREBELLUM_LOG_MODULE, error);
    }
}

/**
 * @brief Emit event to callback
 */
static void emit_event(cerebellum_adapter_t* adapter, uint32_t event_type, const void* data) {
    if (adapter->config.enable_events && adapter->event_callback) {
        adapter->event_callback(event_type, data, adapter->event_user_data);
    }
}

/*=============================================================================
 * SYNAPTIC DYNAMICS FUNCTIONS (Phase 1)
 *===========================================================================*/

cerebellar_synapse_config_t cerebellar_synapse_default_config(void) {
    cerebellar_synapse_config_t config;
    memset(&config, 0, sizeof(config));

    /* Vesicle pool configuration */
    config.rrp_capacity = CEREBELLUM_SYNAPSE_DEFAULT_RRP_CAPACITY;
    config.recycling_capacity = CEREBELLUM_SYNAPSE_DEFAULT_RECYCLING_CAP;
    config.release_probability = CEREBELLUM_SYNAPSE_DEFAULT_RELEASE_PROB;
    config.refill_rate = CEREBELLUM_SYNAPSE_DEFAULT_REFILL_RATE;

    /* Short-term plasticity */
    config.stp_U = CEREBELLUM_SYNAPSE_DEFAULT_STP_U;
    config.stp_tau_D = CEREBELLUM_SYNAPSE_DEFAULT_STP_TAU_D;
    config.stp_tau_F = CEREBELLUM_SYNAPSE_DEFAULT_STP_TAU_F;

    /* Calcium dynamics */
    config.ca_baseline = CEREBELLUM_SYNAPSE_DEFAULT_CA_BASELINE;
    config.ca_decay_tau = CEREBELLUM_SYNAPSE_DEFAULT_CA_DECAY_TAU;
    config.ca_influx_per_spike = CEREBELLUM_SYNAPSE_DEFAULT_CA_INFLUX;

    /* Enable flags */
    config.enable_vesicle_dynamics = false;
    config.enable_stp = false;
    config.enable_calcium_dynamics = false;

    return config;
}

void cerebellar_synapse_init(cerebellar_synapse_state_t* state,
                              const cerebellar_synapse_config_t* config) {
    if (!state) return;

    cerebellar_synapse_config_t cfg = config ? *config : cerebellar_synapse_default_config();

    /* Initialize vesicle pool */
    state->rrp_available = cfg.rrp_capacity;
    state->rrp_capacity = cfg.rrp_capacity;
    state->vesicle_depletion = 0.0f;

    /* Initialize STP state (Tsodyks-Markram model) */
    state->stp_x = 1.0f;  /* Full resources available */
    state->stp_u = cfg.stp_U;  /* Baseline utilization */
    state->last_spike_time = 0;

    /* Initialize calcium */
    state->calcium_concentration = cfg.ca_baseline;
    state->calcium_decay_tau = cfg.ca_decay_tau;

    /* Initialize facilitation/depression */
    state->facilitation_factor = 1.0f;
    state->depression_factor = 1.0f;
    state->effective_weight = 1.0f;
}

void cerebellar_synapse_update(cerebellar_synapse_state_t* state,
                                float dt_ms,
                                float presynaptic_activity) {
    if (!state || dt_ms <= 0.0f) return;

    /* Update STP state (Tsodyks-Markram model) */
    /* dx/dt = (1 - x) / tau_D  (resource recovery) */
    /* du/dt = (U - u) / tau_F  (facilitation decay) */
    float tau_D = CEREBELLUM_SYNAPSE_DEFAULT_STP_TAU_D;
    float tau_F = CEREBELLUM_SYNAPSE_DEFAULT_STP_TAU_F;
    float U = CEREBELLUM_SYNAPSE_DEFAULT_STP_U;

    /* Recovery of resources */
    float dx = ((1.0f - state->stp_x) / tau_D) * dt_ms;
    state->stp_x += dx;
    if (state->stp_x > 1.0f) state->stp_x = 1.0f;

    /* Decay of facilitation */
    float du = ((U - state->stp_u) / tau_F) * dt_ms;
    state->stp_u += du;
    if (state->stp_u < U) state->stp_u = U;

    /* Vesicle refill during no-activity periods */
    if (state->rrp_available < state->rrp_capacity) {
        float refill = CEREBELLUM_SYNAPSE_DEFAULT_REFILL_RATE * dt_ms;
        if (refill >= 1.0f) {
            uint32_t to_refill = (uint32_t)refill;
            if (state->rrp_available + to_refill > state->rrp_capacity) {
                state->rrp_available = state->rrp_capacity;
            } else {
                state->rrp_available += to_refill;
            }
        }
    }

    /* If presynaptic activity (spike), trigger release */
    if (presynaptic_activity > 0.5f) {
        /* On spike: u += U(1 - u), then x -= ux */
        float u_before = state->stp_u;
        state->stp_u = state->stp_u + U * (1.0f - state->stp_u);
        state->stp_x = state->stp_x - u_before * state->stp_x;
        if (state->stp_x < 0.0f) state->stp_x = 0.0f;

        /* Calcium influx on spike */
        state->calcium_concentration += CEREBELLUM_SYNAPSE_DEFAULT_CA_INFLUX;

        /* Vesicle release: deplete RRP based on release probability */
        if (state->rrp_available > 0) {
            float release_prob = CEREBELLUM_SYNAPSE_DEFAULT_RELEASE_PROB;
            uint32_t to_release = (uint32_t)(release_prob * (float)state->rrp_available);
            if (to_release < 1 && state->rrp_available > 0) to_release = 1;
            if (to_release > state->rrp_available) to_release = state->rrp_available;
            state->rrp_available -= to_release;
        }
    }

    /* Update vesicle depletion factor */
    if (state->rrp_capacity > 0) {
        state->vesicle_depletion = 1.0f - (float)state->rrp_available / (float)state->rrp_capacity;
    }

    /* Calcium decay */
    float ca_decay = (state->calcium_concentration - CEREBELLUM_SYNAPSE_DEFAULT_CA_BASELINE)
                     * (dt_ms / state->calcium_decay_tau);
    state->calcium_concentration -= ca_decay;
    if (state->calcium_concentration < CEREBELLUM_SYNAPSE_DEFAULT_CA_BASELINE) {
        state->calcium_concentration = CEREBELLUM_SYNAPSE_DEFAULT_CA_BASELINE;
    }

    /* Update facilitation/depression factors */
    state->facilitation_factor = state->stp_u / U;  /* > 1 if facilitated */
    state->depression_factor = state->stp_x;         /* < 1 if depressed */

    /* Compute effective weight */
    state->effective_weight = state->facilitation_factor * state->depression_factor;
}

float cerebellar_synapse_get_effective_weight(const cerebellar_synapse_state_t* state,
                                               float base_weight) {
    if (!state) return base_weight;
    return base_weight * state->effective_weight;
}

/**
 * @brief Create granule layer
 */
static granule_layer_t* create_granule_layer(uint32_t num_cells, uint32_t num_mossy) {
    granule_layer_t* layer = nimcp_calloc(1, sizeof(granule_layer_t));
    if (!layer) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "layer is NULL");

        return NULL;

    }

    layer->num_cells = num_cells;
    layer->num_mossy_fibers = num_mossy;
    layer->sparse_coding_threshold = 0.5f;

    layer->cells = nimcp_calloc(num_cells, sizeof(granule_cell_t));
    if (!layer->cells) {
        nimcp_free(layer);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "create_granule_layer: layer->cells is NULL");
        return NULL;
    }

    layer->mossy_input_buffer = nimcp_calloc(num_mossy, sizeof(float));
    if (!layer->mossy_input_buffer) {
        nimcp_free(layer->cells);
        nimcp_free(layer);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "create_granule_layer: layer->mossy_input_buffer is NULL");
        return NULL;
    }

    /* Initialize cells with random weights */
    for (uint32_t i = 0; i < num_cells; i++) {
        layer->cells[i].num_mossy_inputs = 4;  /* Each granule receives ~4 mossy inputs */
        layer->cells[i].mossy_weights = nimcp_calloc(4, sizeof(float));
        if (layer->cells[i].mossy_weights) {
            for (uint32_t j = 0; j < 4; j++) {
                layer->cells[i].mossy_weights[j] = 0.25f;  /* Uniform initial weights */
            }
        }
        layer->cells[i].activation = 0.0f;
        layer->cells[i].active = false;
    }

    return layer;
}

/**
 * @brief Destroy granule layer
 */
static void destroy_granule_layer(granule_layer_t* layer) {
    if (!layer) return;
    if (layer->cells) {
        for (uint32_t i = 0; i < layer->num_cells; i++) {
            if (layer->cells[i].mossy_weights) {
                nimcp_free(layer->cells[i].mossy_weights);
            }
        }
        nimcp_free(layer->cells);
    }
    if (layer->mossy_input_buffer) {
        nimcp_free(layer->mossy_input_buffer);
    }
    nimcp_free(layer);
}

/**
 * @brief Create Purkinje layer
 */
static purkinje_layer_t* create_purkinje_layer(uint32_t num_cells, uint32_t num_parallel,
                                                float ltd_rate, float ltp_rate) {
    purkinje_layer_t* layer = nimcp_calloc(1, sizeof(purkinje_layer_t));
    if (!layer) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "layer is NULL");

        return NULL;

    }

    layer->num_cells = num_cells;
    layer->ltd_rate = ltd_rate;
    layer->ltp_rate = ltp_rate;

    layer->cells = nimcp_calloc(num_cells, sizeof(purkinje_cell_t));
    if (!layer->cells) {
        nimcp_free(layer);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "destroy_granule_layer: layer->cells is NULL");
        return NULL;
    }

    layer->parallel_input_buffer = nimcp_calloc(num_parallel, sizeof(float));
    if (!layer->parallel_input_buffer) {
        nimcp_free(layer->cells);
        nimcp_free(layer);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "destroy_granule_layer: layer->parallel_input_buffer is NULL");
        return NULL;
    }

    /* Initialize Purkinje cells */
    for (uint32_t i = 0; i < num_cells; i++) {
        uint32_t inputs_per_cell = (num_cells > 0) ? (num_parallel / num_cells) : num_parallel;
        if (inputs_per_cell < 10) inputs_per_cell = 10;

        layer->cells[i].num_parallel_inputs = inputs_per_cell;
        layer->cells[i].parallel_weights = nimcp_calloc(inputs_per_cell, sizeof(float));
        if (layer->cells[i].parallel_weights) {
            for (uint32_t j = 0; j < inputs_per_cell; j++) {
                layer->cells[i].parallel_weights[j] = 0.5f;  /* Initial weights */
            }
        }
        layer->cells[i].simple_spike_rate = 0.0f;
        layer->cells[i].complex_spike = 0.0f;
        layer->cells[i].inhibition_output = 0.0f;
        layer->cells[i].learning_eligibility = 0.0f;
    }

    return layer;
}

/**
 * @brief Destroy Purkinje layer
 */
static void destroy_purkinje_layer(purkinje_layer_t* layer) {
    if (!layer) return;
    if (layer->cells) {
        for (uint32_t i = 0; i < layer->num_cells; i++) {
            if (layer->cells[i].parallel_weights) {
                nimcp_free(layer->cells[i].parallel_weights);
            }
        }
        nimcp_free(layer->cells);
    }
    if (layer->parallel_input_buffer) {
        nimcp_free(layer->parallel_input_buffer);
    }
    nimcp_free(layer);
}

/**
 * @brief Create deep nuclei
 */
static deep_nuclei_t* create_deep_nuclei(uint32_t num_dentate, uint32_t num_interposed,
                                          uint32_t num_fastigial, uint32_t num_purkinje) {
    deep_nuclei_t* nuclei = nimcp_calloc(1, sizeof(deep_nuclei_t));
    if (!nuclei) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nuclei is NULL");

        return NULL;

    }

    nuclei->num_dentate = num_dentate;
    nuclei->num_interposed = num_interposed;
    nuclei->num_fastigial = num_fastigial;

    /* Allocate dentate nucleus */
    nuclei->dentate = nimcp_calloc(num_dentate, sizeof(nucleus_neuron_t));
    if (!nuclei->dentate) {
        nimcp_free(nuclei);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "destroy_purkinje_layer: nuclei->dentate is NULL");
        return NULL;
    }

    /* Allocate interposed nucleus */
    nuclei->interposed = nimcp_calloc(num_interposed, sizeof(nucleus_neuron_t));
    if (!nuclei->interposed) {
        nimcp_free(nuclei->dentate);
        nimcp_free(nuclei);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "destroy_purkinje_layer: nuclei->interposed is NULL");
        return NULL;
    }

    /* Allocate fastigial nucleus */
    nuclei->fastigial = nimcp_calloc(num_fastigial, sizeof(nucleus_neuron_t));
    if (!nuclei->fastigial) {
        nimcp_free(nuclei->interposed);
        nimcp_free(nuclei->dentate);
        nimcp_free(nuclei);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "destroy_purkinje_layer: nuclei->fastigial is NULL");
        return NULL;
    }

    /* Allocate output buffer */
    uint32_t total_neurons = num_dentate + num_interposed + num_fastigial;
    nuclei->output_buffer = nimcp_calloc(total_neurons * 8, sizeof(float));

    /* Initialize neurons with Purkinje weights */
    uint32_t purkinje_per_neuron = (total_neurons > 0) ? (num_purkinje / total_neurons) : num_purkinje;
    if (purkinje_per_neuron < 1) purkinje_per_neuron = 1;

    for (uint32_t i = 0; i < num_dentate; i++) {
        nuclei->dentate[i].num_purkinje_inputs = purkinje_per_neuron;
        nuclei->dentate[i].purkinje_weights = nimcp_calloc(purkinje_per_neuron, sizeof(float));
        if (nuclei->dentate[i].purkinje_weights) {
            for (uint32_t j = 0; j < purkinje_per_neuron; j++) {
                nuclei->dentate[i].purkinje_weights[j] = -1.0f;  /* Inhibitory */
            }
        }
    }

    for (uint32_t i = 0; i < num_interposed; i++) {
        nuclei->interposed[i].num_purkinje_inputs = purkinje_per_neuron;
        nuclei->interposed[i].purkinje_weights = nimcp_calloc(purkinje_per_neuron, sizeof(float));
        if (nuclei->interposed[i].purkinje_weights) {
            for (uint32_t j = 0; j < purkinje_per_neuron; j++) {
                nuclei->interposed[i].purkinje_weights[j] = -1.0f;
            }
        }
    }

    for (uint32_t i = 0; i < num_fastigial; i++) {
        nuclei->fastigial[i].num_purkinje_inputs = purkinje_per_neuron;
        nuclei->fastigial[i].purkinje_weights = nimcp_calloc(purkinje_per_neuron, sizeof(float));
        if (nuclei->fastigial[i].purkinje_weights) {
            for (uint32_t j = 0; j < purkinje_per_neuron; j++) {
                nuclei->fastigial[i].purkinje_weights[j] = -1.0f;
            }
        }
    }

    return nuclei;
}

/**
 * @brief Destroy deep nuclei
 */
static void destroy_deep_nuclei(deep_nuclei_t* nuclei) {
    if (!nuclei) return;

    if (nuclei->dentate) {
        for (uint32_t i = 0; i < nuclei->num_dentate; i++) {
            if (nuclei->dentate[i].purkinje_weights) {
                nimcp_free(nuclei->dentate[i].purkinje_weights);
            }
        }
        nimcp_free(nuclei->dentate);
    }

    if (nuclei->interposed) {
        for (uint32_t i = 0; i < nuclei->num_interposed; i++) {
            if (nuclei->interposed[i].purkinje_weights) {
                nimcp_free(nuclei->interposed[i].purkinje_weights);
            }
        }
        nimcp_free(nuclei->interposed);
    }

    if (nuclei->fastigial) {
        for (uint32_t i = 0; i < nuclei->num_fastigial; i++) {
            if (nuclei->fastigial[i].purkinje_weights) {
                nimcp_free(nuclei->fastigial[i].purkinje_weights);
            }
        }
        nimcp_free(nuclei->fastigial);
    }

    if (nuclei->output_buffer) {
        nimcp_free(nuclei->output_buffer);
    }

    nimcp_free(nuclei);
}

/**
 * @brief Create climbing fiber system
 */
static climbing_fiber_system_t* create_climbing_fibers(uint32_t num_fibers, uint32_t num_purkinje) {
    climbing_fiber_system_t* cf = nimcp_calloc(1, sizeof(climbing_fiber_system_t));
    if (!cf) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cf is NULL");

        return NULL;

    }

    cf->num_fibers = num_fibers;
    cf->error_threshold = 0.1f;

    cf->error_signals = nimcp_calloc(num_fibers, sizeof(float));
    cf->purkinje_targets = nimcp_calloc(num_fibers, sizeof(uint32_t));

    if (!cf->error_signals || !cf->purkinje_targets) {
        if (cf->error_signals) nimcp_free(cf->error_signals);
        if (cf->purkinje_targets) nimcp_free(cf->purkinje_targets);
        nimcp_free(cf);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "create_climbing_fibers: validation failed");
        return NULL;
    }

    /* Assign climbing fibers to Purkinje cells (1:1 in adult cerebellum) */
    for (uint32_t i = 0; i < num_fibers; i++) {
        cf->purkinje_targets[i] = i % num_purkinje;
    }

    return cf;
}

/**
 * @brief Destroy climbing fiber system
 */
static void destroy_climbing_fibers(climbing_fiber_system_t* cf) {
    if (!cf) return;
    if (cf->error_signals) nimcp_free(cf->error_signals);
    if (cf->purkinje_targets) nimcp_free(cf->purkinje_targets);
    nimcp_free(cf);
}

/*=============================================================================
 * PHASE 1: GLOMERULAR LAYER
 *===========================================================================*/

/**
 * @brief Create glomerular layer for mossy fiber divergence
 */
static glomerular_layer_t* create_glomerular_layer(uint32_t num_mossy,
                                                    uint32_t num_granule,
                                                    uint32_t granules_per_glom,
                                                    bool enable_synaptic_dynamics) {
    glomerular_layer_t* layer = nimcp_calloc(1, sizeof(glomerular_layer_t));
    if (!layer) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "layer is NULL");

        return NULL;

    }

    layer->num_glomeruli = num_mossy;
    layer->granules_per_glomerulus = granules_per_glom;
    layer->synaptic_dynamics_enabled = enable_synaptic_dynamics;
    layer->divergence_factor = (float)granules_per_glom;

    /* Allocate glomeruli */
    layer->glomeruli = nimcp_calloc(num_mossy, sizeof(glomerulus_t));
    if (!layer->glomeruli) {
        nimcp_free(layer);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "destroy_climbing_fibers: layer->glomeruli is NULL");
        return NULL;
    }

    /* Initialize each glomerulus */
    for (uint32_t i = 0; i < num_mossy; i++) {
        glomerulus_t* glom = &layer->glomeruli[i];
        glom->mossy_fiber_id = i;
        glom->num_targets = granules_per_glom;
        glom->activity = 0.0f;

        /* Allocate targets - distribute across granule cells */
        glom->granule_targets = nimcp_calloc(granules_per_glom, sizeof(uint32_t));
        glom->synapse_weights = nimcp_calloc(granules_per_glom, sizeof(float));

        if (!glom->granule_targets || !glom->synapse_weights) {
            /* Cleanup on failure */
            for (uint32_t j = 0; j <= i; j++) {
                if (layer->glomeruli[j].granule_targets)
                    nimcp_free(layer->glomeruli[j].granule_targets);
                if (layer->glomeruli[j].synapse_weights)
                    nimcp_free(layer->glomeruli[j].synapse_weights);
            }
            nimcp_free(layer->glomeruli);
            nimcp_free(layer);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "destroy_climbing_fibers: validation failed");
            return NULL;
        }

        /* Assign granule targets with divergence */
        for (uint32_t j = 0; j < granules_per_glom; j++) {
            glom->granule_targets[j] = (i * granules_per_glom + j) % num_granule;
            glom->synapse_weights[j] = 0.25f;  /* Initial weight */
        }

        /* Initialize synaptic dynamics */
        cerebellar_synapse_init(&glom->synapse, NULL);
    }

    LOG_DEBUG("[%s] Created glomerular layer: %u glomeruli, %u targets each",
              CEREBELLUM_LOG_MODULE, num_mossy, granules_per_glom);

    return layer;
}

/**
 * @brief Destroy glomerular layer
 */
static void destroy_glomerular_layer(glomerular_layer_t* layer) {
    if (!layer) return;

    if (layer->glomeruli) {
        for (uint32_t i = 0; i < layer->num_glomeruli; i++) {
            if (layer->glomeruli[i].granule_targets)
                nimcp_free(layer->glomeruli[i].granule_targets);
            if (layer->glomeruli[i].synapse_weights)
                nimcp_free(layer->glomeruli[i].synapse_weights);
        }
        nimcp_free(layer->glomeruli);
    }

    nimcp_free(layer);
}

/*=============================================================================
 * PHASE 2: MOLECULAR LAYER INTERNEURONS
 *===========================================================================*/

/**
 * @brief Create molecular layer interneurons (basket and stellate cells)
 */
static molecular_layer_interneurons_t* create_molecular_interneurons(
    uint32_t num_basket, uint32_t purkinje_per_basket,
    uint32_t num_stellate, uint32_t purkinje_per_stellate,
    uint32_t num_purkinje, uint32_t num_parallel) {

    molecular_layer_interneurons_t* layer = nimcp_calloc(1, sizeof(molecular_layer_interneurons_t));
    if (!layer) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "layer is NULL");

        return NULL;

    }

    layer->num_basket_cells = num_basket;
    layer->num_stellate_cells = num_stellate;
    layer->inhibition_strength = 1.0f;

    /* Allocate parallel input buffer */
    layer->parallel_input_buffer = nimcp_calloc(num_parallel, sizeof(float));
    if (!layer->parallel_input_buffer) {
        nimcp_free(layer);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "destroy_glomerular_layer: layer->parallel_input_buffer is NULL");
        return NULL;
    }

    /* Allocate basket cells */
    if (num_basket > 0) {
        layer->basket_cells = nimcp_calloc(num_basket, sizeof(basket_cell_t));
        if (!layer->basket_cells) {
            nimcp_free(layer->parallel_input_buffer);
            nimcp_free(layer);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "destroy_glomerular_layer: layer->basket_cells is NULL");
            return NULL;
        }

        /* Initialize basket cells */
        for (uint32_t i = 0; i < num_basket; i++) {
            basket_cell_t* bc = &layer->basket_cells[i];
            bc->num_purkinje_targets = purkinje_per_basket;
            bc->activation = 0.0f;
            bc->firing_rate = 0.0f;
            bc->somatic_inhibition = 0.0f;

            bc->purkinje_weights = nimcp_calloc(purkinje_per_basket, sizeof(float));
            bc->target_purkinje_ids = nimcp_calloc(purkinje_per_basket, sizeof(uint32_t));

            if (bc->purkinje_weights && bc->target_purkinje_ids) {
                for (uint32_t j = 0; j < purkinje_per_basket; j++) {
                    bc->target_purkinje_ids[j] = (i * purkinje_per_basket + j) % num_purkinje;
                    bc->purkinje_weights[j] = -1.0f;  /* Inhibitory */
                }
            }

            cerebellar_synapse_init(&bc->synapse, NULL);
        }
    }

    /* Allocate stellate cells */
    if (num_stellate > 0) {
        layer->stellate_cells = nimcp_calloc(num_stellate, sizeof(stellate_cell_t));
        if (!layer->stellate_cells) {
            /* Cleanup basket cells */
            if (layer->basket_cells) {
                for (uint32_t i = 0; i < num_basket; i++) {
                    if (layer->basket_cells[i].purkinje_weights)
                        nimcp_free(layer->basket_cells[i].purkinje_weights);
                    if (layer->basket_cells[i].target_purkinje_ids)
                        nimcp_free(layer->basket_cells[i].target_purkinje_ids);
                }
                nimcp_free(layer->basket_cells);
            }
            nimcp_free(layer->parallel_input_buffer);
            nimcp_free(layer);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "destroy_glomerular_layer: validation failed");
            return NULL;
        }

        /* Initialize stellate cells */
        for (uint32_t i = 0; i < num_stellate; i++) {
            stellate_cell_t* sc = &layer->stellate_cells[i];
            sc->num_purkinje_targets = purkinje_per_stellate;
            sc->activation = 0.0f;
            sc->dendritic_inhibition = 0.0f;

            sc->purkinje_weights = nimcp_calloc(purkinje_per_stellate, sizeof(float));
            sc->target_purkinje_ids = nimcp_calloc(purkinje_per_stellate, sizeof(uint32_t));

            if (sc->purkinje_weights && sc->target_purkinje_ids) {
                for (uint32_t j = 0; j < purkinje_per_stellate; j++) {
                    sc->target_purkinje_ids[j] = (i * purkinje_per_stellate + j) % num_purkinje;
                    sc->purkinje_weights[j] = -0.5f;  /* Inhibitory, weaker than basket */
                }
            }

            cerebellar_synapse_init(&sc->synapse, NULL);
        }
    }

    LOG_DEBUG("[%s] Created molecular interneurons: %u basket, %u stellate",
              CEREBELLUM_LOG_MODULE, num_basket, num_stellate);

    return layer;
}

/**
 * @brief Destroy molecular layer interneurons
 */
static void destroy_molecular_interneurons(molecular_layer_interneurons_t* layer) {
    if (!layer) return;

    if (layer->basket_cells) {
        for (uint32_t i = 0; i < layer->num_basket_cells; i++) {
            if (layer->basket_cells[i].purkinje_weights)
                nimcp_free(layer->basket_cells[i].purkinje_weights);
            if (layer->basket_cells[i].target_purkinje_ids)
                nimcp_free(layer->basket_cells[i].target_purkinje_ids);
        }
        nimcp_free(layer->basket_cells);
    }

    if (layer->stellate_cells) {
        for (uint32_t i = 0; i < layer->num_stellate_cells; i++) {
            if (layer->stellate_cells[i].purkinje_weights)
                nimcp_free(layer->stellate_cells[i].purkinje_weights);
            if (layer->stellate_cells[i].target_purkinje_ids)
                nimcp_free(layer->stellate_cells[i].target_purkinje_ids);
        }
        nimcp_free(layer->stellate_cells);
    }

    if (layer->parallel_input_buffer)
        nimcp_free(layer->parallel_input_buffer);

    nimcp_free(layer);
}

/*=============================================================================
 * PHASE 2: GOLGI CELL NETWORK
 *===========================================================================*/

/**
 * @brief Create Golgi cell network for feedback inhibition
 */
static golgi_cell_network_t* create_golgi_network(uint32_t num_golgi,
                                                   uint32_t granules_per_golgi,
                                                   uint32_t num_granule) {
    golgi_cell_network_t* network = nimcp_calloc(1, sizeof(golgi_cell_network_t));
    if (!network) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "network is NULL");

        return NULL;

    }

    network->num_cells = num_golgi;
    network->feedback_delay_ms = 5.0f;
    network->inhibition_decay_tau = 50.0f;

    /* Allocate granule activity buffer */
    network->granule_activity_buffer = nimcp_calloc(num_granule, sizeof(float));
    if (!network->granule_activity_buffer) {
        nimcp_free(network);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "destroy_molecular_interneurons: network->granule_activity_buffer is NULL");
        return NULL;
    }

    /* Allocate Golgi cells */
    network->cells = nimcp_calloc(num_golgi, sizeof(golgi_cell_t));
    if (!network->cells) {
        nimcp_free(network->granule_activity_buffer);
        nimcp_free(network);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "destroy_molecular_interneurons: network->cells is NULL");
        return NULL;
    }

    /* Initialize Golgi cells */
    for (uint32_t i = 0; i < num_golgi; i++) {
        golgi_cell_t* gc = &network->cells[i];
        gc->num_granule_targets = granules_per_golgi;
        gc->activation = 0.0f;
        gc->feedback_inhibition = 0.0f;
        gc->parallel_fiber_input = 0.0f;
        gc->mossy_fiber_input = 0.0f;

        gc->granule_weights = nimcp_calloc(granules_per_golgi, sizeof(float));
        gc->target_granule_ids = nimcp_calloc(granules_per_golgi, sizeof(uint32_t));

        if (gc->granule_weights && gc->target_granule_ids) {
            for (uint32_t j = 0; j < granules_per_golgi; j++) {
                gc->target_granule_ids[j] = (i * granules_per_golgi + j) % num_granule;
                gc->granule_weights[j] = -0.5f;  /* Inhibitory */
            }
        }

        cerebellar_synapse_init(&gc->synapse, NULL);
    }

    LOG_DEBUG("[%s] Created Golgi network: %u cells, %u granule targets each",
              CEREBELLUM_LOG_MODULE, num_golgi, granules_per_golgi);

    return network;
}

/**
 * @brief Destroy Golgi cell network
 */
static void destroy_golgi_network(golgi_cell_network_t* network) {
    if (!network) return;

    if (network->cells) {
        for (uint32_t i = 0; i < network->num_cells; i++) {
            if (network->cells[i].granule_weights)
                nimcp_free(network->cells[i].granule_weights);
            if (network->cells[i].target_granule_ids)
                nimcp_free(network->cells[i].target_granule_ids);
        }
        nimcp_free(network->cells);
    }

    if (network->granule_activity_buffer)
        nimcp_free(network->granule_activity_buffer);

    nimcp_free(network);
}

/**
 * @brief Process granule layer
 */
static void process_granule_layer(cerebellum_adapter_t* adapter) {
    if (!adapter || !adapter->granule) return;

    granule_layer_t* g = adapter->granule;
    uint32_t active_count = 0;

    /* Sparse coding: only ~5% of granule cells active */
    float max_activation = 0.0f;
    for (uint32_t i = 0; i < g->num_cells; i++) {
        /* Compute activation from mossy inputs */
        float sum = 0.0f;
        for (uint32_t j = 0; j < g->cells[i].num_mossy_inputs && j < g->num_mossy_fibers; j++) {
            uint32_t idx = (i * 4 + j) % g->num_mossy_fibers;
            sum += g->mossy_input_buffer[idx] * g->cells[i].mossy_weights[j];
        }

        g->cells[i].activation = sum;
        if (sum > max_activation) max_activation = sum;
    }

    /* Apply sparse threshold */
    float threshold = max_activation * g->sparse_coding_threshold;
    for (uint32_t i = 0; i < g->num_cells; i++) {
        g->cells[i].active = (g->cells[i].activation > threshold);
        if (g->cells[i].active) {
            active_count++;
            adapter->stats.granule_activations++;
        }
    }

    /* Update aggregate synapse dynamics if enabled (for stats tracking) */
    if (adapter->config.enable_synaptic_dynamics && !adapter->glomerular) {
        /* Find the max mossy fiber activity to drive aggregate synapse */
        float max_mossy = 0.0f;
        for (uint32_t i = 0; i < g->num_mossy_fibers; i++) {
            if (g->mossy_input_buffer[i] > max_mossy) {
                max_mossy = g->mossy_input_buffer[i];
            }
        }
        cerebellar_synapse_update(&adapter->aggregate_synapse, 1.0f, max_mossy);
    }

    LOG_DEBUG("[%s] Granule layer: %u/%u cells active", CEREBELLUM_LOG_MODULE,
              active_count, g->num_cells);
}

/**
 * @brief Process Purkinje layer
 */
static void process_purkinje_layer(cerebellum_adapter_t* adapter) {
    if (!adapter || !adapter->purkinje || !adapter->granule) return;

    purkinje_layer_t* p = adapter->purkinje;
    granule_layer_t* g = adapter->granule;

    /* Transfer granule outputs to parallel fiber buffer */
    for (uint32_t i = 0; i < g->num_cells && i < p->num_cells * 10; i++) {
        p->parallel_input_buffer[i] = g->cells[i].active ? g->cells[i].activation : 0.0f;
    }

    /* Process each Purkinje cell */
    for (uint32_t i = 0; i < p->num_cells; i++) {
        /* Compute simple spike rate from parallel fiber inputs */
        float sum = 0.0f;
        for (uint32_t j = 0; j < p->cells[i].num_parallel_inputs; j++) {
            uint32_t idx = i * p->cells[i].num_parallel_inputs + j;
            if (idx < g->num_cells) {
                sum += p->parallel_input_buffer[idx] * p->cells[i].parallel_weights[j];
            }
        }

        /* Purkinje cells have high baseline firing (~50-100 Hz equivalent) */
        p->cells[i].simple_spike_rate = 0.5f + 0.5f * tanhf(sum);
        p->cells[i].inhibition_output = p->cells[i].simple_spike_rate;

        adapter->stats.purkinje_simple_spikes++;

        /* Update eligibility trace */
        p->cells[i].learning_eligibility *= 0.95f;  /* Decay */
        p->cells[i].learning_eligibility += sum * 0.1f;  /* Accumulate */
    }
}

/**
 * @brief Process deep nuclei
 */
static void process_deep_nuclei(cerebellum_adapter_t* adapter) {
    if (!adapter || !adapter->nuclei || !adapter->purkinje) return;

    deep_nuclei_t* n = adapter->nuclei;
    purkinje_layer_t* p = adapter->purkinje;

    /* Process dentate nucleus (lateral, planning) */
    for (uint32_t i = 0; i < n->num_dentate; i++) {
        float inhibition = 0.0f;
        for (uint32_t j = 0; j < n->dentate[i].num_purkinje_inputs && j < p->num_cells; j++) {
            inhibition += p->cells[j].inhibition_output * n->dentate[i].purkinje_weights[j];
        }
        /* Normalize inhibition by number of inputs */
        if (n->dentate[i].num_purkinje_inputs > 0) {
            inhibition /= (float)n->dentate[i].num_purkinje_inputs;
        }
        /* Baseline excitation from mossy fiber collaterals, minus Purkinje inhibition */
        n->dentate[i].activity = fmaxf(0.0f, 0.8f + inhibition);

        /* Generate motor command (simplified) */
        for (uint32_t d = 0; d < 8; d++) {
            n->dentate[i].motor_output[d] = n->dentate[i].activity * 0.1f;
        }
    }

    /* Process interposed nucleus (intermediate, execution) */
    for (uint32_t i = 0; i < n->num_interposed; i++) {
        float inhibition = 0.0f;
        uint32_t offset = n->num_dentate;
        for (uint32_t j = 0; j < n->interposed[i].num_purkinje_inputs; j++) {
            uint32_t idx = (offset + j) % p->num_cells;
            inhibition += p->cells[idx].inhibition_output * n->interposed[i].purkinje_weights[j];
        }
        if (n->interposed[i].num_purkinje_inputs > 0) {
            inhibition /= (float)n->interposed[i].num_purkinje_inputs;
        }
        n->interposed[i].activity = fmaxf(0.0f, 0.8f + inhibition);
    }

    /* Process fastigial nucleus (medial, balance/posture) */
    for (uint32_t i = 0; i < n->num_fastigial; i++) {
        float inhibition = 0.0f;
        uint32_t offset = n->num_dentate + n->num_interposed;
        for (uint32_t j = 0; j < n->fastigial[i].num_purkinje_inputs; j++) {
            uint32_t idx = (offset + j) % p->num_cells;
            inhibition += p->cells[idx].inhibition_output * n->fastigial[i].purkinje_weights[j];
        }
        if (n->fastigial[i].num_purkinje_inputs > 0) {
            inhibition /= (float)n->fastigial[i].num_purkinje_inputs;
        }
        n->fastigial[i].activity = fmaxf(0.0f, 0.8f + inhibition);
    }
}

/**
 * @brief Apply LTD learning
 */
static void apply_ltd_learning(cerebellum_adapter_t* adapter, uint32_t purkinje_id, float error) {
    if (!adapter || !adapter->purkinje) return;

    purkinje_layer_t* p = adapter->purkinje;
    if (purkinje_id >= p->num_cells) return;

    purkinje_cell_t* cell = &p->cells[purkinje_id];

    /* Apply LTD: depress parallel fiber synapses that were active during error */
    for (uint32_t i = 0; i < cell->num_parallel_inputs; i++) {
        /* Weight change proportional to eligibility and error */
        float dw = -p->ltd_rate * error * cell->learning_eligibility;
        cell->parallel_weights[i] += dw;

        /* Clamp weights */
        if (cell->parallel_weights[i] < 0.0f) cell->parallel_weights[i] = 0.0f;
        if (cell->parallel_weights[i] > 1.0f) cell->parallel_weights[i] = 1.0f;
    }

    /* Reset eligibility after learning */
    cell->learning_eligibility = 0.0f;

    adapter->stats.ltd_events++;
}

/*=============================================================================
 * PHASE 1: GLOMERULAR LAYER PROCESSING
 *===========================================================================*/

/**
 * @brief Process glomerular layer - distribute mossy input with enhanced divergence
 *
 * BIOLOGICAL CONTEXT:
 * Each mossy fiber terminates in glomeruli, which fan out to 50-100 granule cells.
 * This creates the massive expansion from mossy fibers to granule cells that is
 * characteristic of the cerebellar cortex.
 */
static void process_glomerular_layer(cerebellum_adapter_t* adapter) {
    if (!adapter || !adapter->glomerular || !adapter->granule) return;

    glomerular_layer_t* glom = adapter->glomerular;
    granule_layer_t* gran = adapter->granule;

    /* For each glomerulus, distribute mossy fiber input to target granules */
    for (uint32_t i = 0; i < glom->num_glomeruli; i++) {
        glomerulus_t* g = &glom->glomeruli[i];

        /* Get mossy fiber input */
        float mossy_input = 0.0f;
        if (g->mossy_fiber_id < gran->num_mossy_fibers) {
            mossy_input = gran->mossy_input_buffer[g->mossy_fiber_id];
        }

        /* Update glomerulus activity */
        g->activity = mossy_input;

        /* Update synaptic dynamics if enabled */
        if (glom->synaptic_dynamics_enabled) {
            cerebellar_synapse_update(&g->synapse, 1.0f, mossy_input);
        }

        /* Distribute to target granule cells with synaptic modulation */
        for (uint32_t j = 0; j < g->num_targets; j++) {
            uint32_t target_id = g->granule_targets[j];
            if (target_id < gran->num_cells) {
                float effective_weight = g->synapse_weights[j];

                /* Apply synaptic dynamics modulation if enabled */
                if (glom->synaptic_dynamics_enabled) {
                    effective_weight = cerebellar_synapse_get_effective_weight(
                        &g->synapse, effective_weight);
                }

                /* Add glomerular input to granule (accumulative from multiple glomeruli) */
                gran->cells[target_id].activation += mossy_input * effective_weight;
            }
        }
    }

    adapter->stats.glomerular_activations++;
}

/*=============================================================================
 * PHASE 2: INTERNEURON PROCESSING FUNCTIONS
 *===========================================================================*/

/**
 * @brief Process Golgi cells - feedback inhibition to granule layer
 *
 * BIOLOGICAL CONTEXT:
 * Golgi cells receive input from parallel fibers and mossy fibers, and provide
 * feedback inhibition to granule cells. This regulates the gain and timing of
 * granule cell activity.
 */
static void process_golgi_cells(cerebellum_adapter_t* adapter) {
    if (!adapter || !adapter->golgi || !adapter->granule) return;

    golgi_cell_network_t* golgi = adapter->golgi;
    granule_layer_t* gran = adapter->granule;

    /* First, compute Golgi cell activity from parallel fiber input */
    for (uint32_t i = 0; i < golgi->num_cells; i++) {
        golgi_cell_t* gc = &golgi->cells[i];

        /* Get input from parallel fibers (active granule cells) */
        float pf_input = 0.0f;
        for (uint32_t j = 0; j < gc->num_granule_targets; j++) {
            uint32_t gid = gc->target_granule_ids[j];
            if (gid < gran->num_cells && gran->cells[gid].active) {
                pf_input += gran->cells[gid].activation * 0.1f;
            }
        }

        /* Get input from mossy fibers (via mossy buffer) */
        float mossy_input = 0.0f;
        uint32_t mossy_idx = i % gran->num_mossy_fibers;
        mossy_input = gran->mossy_input_buffer[mossy_idx];
        gc->mossy_fiber_input = mossy_input;

        /* Golgi cell activation */
        float total_input = pf_input + mossy_input;
        gc->activation = tanhf(total_input);

        /* Update synaptic state */
        cerebellar_synapse_update(&gc->synapse, 1.0f, gc->activation);

        /* Compute feedback inhibition strength */
        gc->feedback_inhibition = gc->activation;

        if (gc->activation > 0.5f) {
            adapter->stats.golgi_cell_spikes++;
        }
    }
}

/**
 * @brief Apply Golgi cell inhibition to granule layer
 */
static void apply_golgi_inhibition(cerebellum_adapter_t* adapter) {
    if (!adapter || !adapter->golgi || !adapter->granule) return;

    golgi_cell_network_t* golgi = adapter->golgi;
    granule_layer_t* gran = adapter->granule;

    /* Each Golgi cell inhibits its target granule cells */
    for (uint32_t i = 0; i < golgi->num_cells; i++) {
        golgi_cell_t* gc = &golgi->cells[i];

        if (gc->feedback_inhibition < 0.1f) continue;

        for (uint32_t j = 0; j < gc->num_granule_targets; j++) {
            uint32_t target_id = gc->target_granule_ids[j];
            if (target_id < gran->num_cells) {
                /* Apply inhibition */
                float inh = gc->feedback_inhibition * fabsf(gc->granule_weights[j]);
                gran->cells[target_id].activation -= inh;
                if (gran->cells[target_id].activation < 0.0f) {
                    gran->cells[target_id].activation = 0.0f;
                }
            }
        }
    }
}

/**
 * @brief Process basket cells - somatic inhibition of Purkinje cells
 *
 * BIOLOGICAL CONTEXT:
 * Basket cells receive parallel fiber input and provide powerful somatic
 * inhibition to Purkinje cells. Their axons form "baskets" around Purkinje
 * cell bodies, enabling rapid and strong inhibition.
 */
static void process_basket_cells(cerebellum_adapter_t* adapter) {
    if (!adapter || !adapter->molecular || !adapter->granule) return;

    molecular_layer_interneurons_t* mol = adapter->molecular;
    granule_layer_t* gran = adapter->granule;

    if (!mol->basket_cells) return;

    /* Compute basket cell activity from parallel fiber (granule) input */
    for (uint32_t i = 0; i < mol->num_basket_cells; i++) {
        basket_cell_t* bc = &mol->basket_cells[i];

        /* Get parallel fiber input */
        float pf_input = 0.0f;
        uint32_t active_count = 0;
        for (uint32_t j = 0; j < gran->num_cells && j < 100; j++) {
            if (gran->cells[j].active) {
                pf_input += gran->cells[j].activation;
                active_count++;
            }
        }
        if (active_count > 0) {
            pf_input /= active_count;
        }

        /* Basket cell activation (fast-spiking interneuron characteristics) */
        bc->activation = tanhf(pf_input * 2.0f);  /* Higher gain than Purkinje */
        bc->firing_rate = bc->activation;

        /* Compute somatic inhibition */
        bc->somatic_inhibition = bc->activation * mol->inhibition_strength;

        /* Update synaptic state */
        cerebellar_synapse_update(&bc->synapse, 1.0f, bc->activation);

        if (bc->activation > 0.5f) {
            adapter->stats.basket_cell_spikes++;
        }
    }
}

/**
 * @brief Apply basket cell inhibition to Purkinje cell somata
 */
static void apply_basket_inhibition(cerebellum_adapter_t* adapter) {
    if (!adapter || !adapter->molecular || !adapter->purkinje) return;

    molecular_layer_interneurons_t* mol = adapter->molecular;
    purkinje_layer_t* pk = adapter->purkinje;

    if (!mol->basket_cells) return;

    /* Each basket cell inhibits its target Purkinje cells */
    for (uint32_t i = 0; i < mol->num_basket_cells; i++) {
        basket_cell_t* bc = &mol->basket_cells[i];

        if (bc->somatic_inhibition < 0.1f) continue;

        for (uint32_t j = 0; j < bc->num_purkinje_targets; j++) {
            uint32_t target_id = bc->target_purkinje_ids[j];
            if (target_id < pk->num_cells) {
                /* Apply somatic inhibition (affects simple spike rate) */
                float inh = bc->somatic_inhibition * fabsf(bc->purkinje_weights[j]);
                pk->cells[target_id].simple_spike_rate -= inh;
                if (pk->cells[target_id].simple_spike_rate < 0.0f) {
                    pk->cells[target_id].simple_spike_rate = 0.0f;
                }
            }
        }
    }
}

/**
 * @brief Process stellate cells - dendritic inhibition of Purkinje cells
 *
 * BIOLOGICAL CONTEXT:
 * Stellate cells are located in the outer molecular layer and inhibit
 * Purkinje cell dendrites. Their inhibition is weaker but more distributed
 * than basket cells.
 */
static void process_stellate_cells(cerebellum_adapter_t* adapter) {
    if (!adapter || !adapter->molecular || !adapter->granule) return;

    molecular_layer_interneurons_t* mol = adapter->molecular;
    granule_layer_t* gran = adapter->granule;

    if (!mol->stellate_cells) return;

    /* Compute stellate cell activity from parallel fiber input */
    for (uint32_t i = 0; i < mol->num_stellate_cells; i++) {
        stellate_cell_t* sc = &mol->stellate_cells[i];

        /* Get parallel fiber input */
        float pf_input = 0.0f;
        uint32_t active_count = 0;
        for (uint32_t j = 0; j < gran->num_cells && j < 50; j++) {
            if (gran->cells[j].active) {
                pf_input += gran->cells[j].activation;
                active_count++;
            }
        }
        if (active_count > 0) {
            pf_input /= active_count;
        }

        /* Stellate cell activation */
        sc->activation = tanhf(pf_input * 1.5f);

        /* Compute dendritic inhibition */
        sc->dendritic_inhibition = sc->activation * mol->inhibition_strength * 0.5f;

        /* Update synaptic state */
        cerebellar_synapse_update(&sc->synapse, 1.0f, sc->activation);

        if (sc->activation > 0.5f) {
            adapter->stats.stellate_cell_spikes++;
        }
    }
}

/**
 * @brief Apply stellate cell inhibition to Purkinje cell dendrites
 */
static void apply_stellate_inhibition(cerebellum_adapter_t* adapter) {
    if (!adapter || !adapter->molecular || !adapter->purkinje) return;

    molecular_layer_interneurons_t* mol = adapter->molecular;
    purkinje_layer_t* pk = adapter->purkinje;

    if (!mol->stellate_cells) return;

    /* Each stellate cell inhibits its target Purkinje cells */
    for (uint32_t i = 0; i < mol->num_stellate_cells; i++) {
        stellate_cell_t* sc = &mol->stellate_cells[i];

        if (sc->dendritic_inhibition < 0.1f) continue;

        for (uint32_t j = 0; j < sc->num_purkinje_targets; j++) {
            uint32_t target_id = sc->target_purkinje_ids[j];
            if (target_id < pk->num_cells) {
                /* Apply dendritic inhibition (affects parallel fiber integration) */
                float inh = sc->dendritic_inhibition * fabsf(sc->purkinje_weights[j]);
                pk->cells[target_id].simple_spike_rate -= inh * 0.3f;  /* Weaker effect */
                if (pk->cells[target_id].simple_spike_rate < 0.0f) {
                    pk->cells[target_id].simple_spike_rate = 0.0f;
                }
            }
        }
    }
}

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

cerebellum_config_t cerebellum_default_config(void) {
    cerebellum_config_t config;
    memset(&config, 0, sizeof(config));

    config.num_granule_cells = CEREBELLUM_DEFAULT_GRANULE_CELLS;
    config.num_purkinje_cells = CEREBELLUM_DEFAULT_PURKINJE_CELLS;
    config.num_parallel_fibers = CEREBELLUM_DEFAULT_PARALLEL_FIBERS;
    config.num_climbing_fibers = CEREBELLUM_DEFAULT_CLIMBING_FIBERS;
    config.num_mossy_fibers = 100;

    config.num_dentate_neurons = 20;
    config.num_interposed_neurons = 10;
    config.num_fastigial_neurons = 10;

    config.enable_timing = true;
    config.enable_error_learning = true;
    config.enable_forward_models = true;
    config.enable_motor_adaptation = true;

    config.ltd_rate = CEREBELLUM_DEFAULT_LTD_RATE;
    config.ltp_rate = CEREBELLUM_DEFAULT_LTP_RATE;
    config.learning_rate = NIMCP_LEARNING_RATE_DEFAULT;

    config.timing_resolution_ms = CEREBELLUM_DEFAULT_TIMING_RESOLUTION_MS;
    config.max_prediction_horizon_ms = 500.0f;

    config.enable_events = true;
    config.enable_bio_async = true;
    config.default_channel = BIO_CHANNEL_ACETYLCHOLINE;

    /*=== Phase 1: Synaptic Dynamics (disabled by default for backward compat) ===*/

    /* Synaptic dynamics configuration */
    config.synapse_config.rrp_capacity = CEREBELLUM_SYNAPSE_DEFAULT_RRP_CAPACITY;
    config.synapse_config.recycling_capacity = CEREBELLUM_SYNAPSE_DEFAULT_RECYCLING_CAP;
    config.synapse_config.release_probability = CEREBELLUM_SYNAPSE_DEFAULT_RELEASE_PROB;
    config.synapse_config.refill_rate = CEREBELLUM_SYNAPSE_DEFAULT_REFILL_RATE;
    config.synapse_config.stp_U = CEREBELLUM_SYNAPSE_DEFAULT_STP_U;
    config.synapse_config.stp_tau_D = CEREBELLUM_SYNAPSE_DEFAULT_STP_TAU_D;
    config.synapse_config.stp_tau_F = CEREBELLUM_SYNAPSE_DEFAULT_STP_TAU_F;
    config.synapse_config.ca_baseline = CEREBELLUM_SYNAPSE_DEFAULT_CA_BASELINE;
    config.synapse_config.ca_decay_tau = CEREBELLUM_SYNAPSE_DEFAULT_CA_DECAY_TAU;
    config.synapse_config.ca_influx_per_spike = CEREBELLUM_SYNAPSE_DEFAULT_CA_INFLUX;
    config.synapse_config.enable_vesicle_dynamics = false;
    config.synapse_config.enable_stp = false;
    config.synapse_config.enable_calcium_dynamics = false;
    config.enable_synaptic_dynamics = false;

    /* Glomerular divergence */
    config.granules_per_mossy_fiber = CEREBELLUM_DEFAULT_GRANULES_PER_MOSSY;
    config.enable_glomerular_divergence = false;

    /*=== Phase 2: Inhibitory Interneurons (disabled by default) ===*/

    /* Basket cells */
    config.num_basket_cells = CEREBELLUM_DEFAULT_BASKET_CELLS;
    config.purkinje_per_basket = CEREBELLUM_DEFAULT_PURKINJE_PER_BASKET;
    config.enable_basket_cells = false;

    /* Stellate cells */
    config.num_stellate_cells = CEREBELLUM_DEFAULT_STELLATE_CELLS;
    config.purkinje_per_stellate = CEREBELLUM_DEFAULT_PURKINJE_PER_STELLATE;
    config.enable_stellate_cells = false;

    /* Golgi cells */
    config.num_golgi_cells = CEREBELLUM_DEFAULT_GOLGI_CELLS;
    config.granules_per_golgi = CEREBELLUM_DEFAULT_GRANULES_PER_GOLGI;
    config.enable_golgi_cells = false;

    /*=== Phase 3: Vestibulocerebellum (disabled by default) ===*/

    config.num_flocculus_purkinje = 20;
    config.num_nodulus_purkinje = 10;
    config.enable_vestibulocerebellum = false;
    config.enable_vor_adaptation = false;
    config.vor_ltd_rate = 0.002f;

    return config;
}

cerebellum_adapter_t* cerebellum_create(const cerebellum_config_t* config) {
    LOG_INFO("[%s] Creating cerebellum adapter", CEREBELLUM_LOG_MODULE);

    cerebellum_adapter_t* adapter = nimcp_calloc(1, sizeof(cerebellum_adapter_t));
    if (!adapter) {
        LOG_ERROR("[%s] Failed to allocate adapter memory", CEREBELLUM_LOG_MODULE);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cerebellum_create: adapter is NULL");
        return NULL;
    }

    /* S7-H4 fix (2026-05-19): create internal mutex first so all
     * downstream init paths see a usable lock. NULL lock = adapter is
     * unusable; bail out via destroy. */
    adapter->mutex = nimcp_mutex_create(NULL);
    if (!adapter->mutex) {
        LOG_ERROR("[%s] Failed to create cerebellum mutex", CEREBELLUM_LOG_MODULE);
        nimcp_free(adapter);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
                              "cerebellum_create: mutex alloc failed");
        return NULL;
    }

    /* Set configuration */
    if (config) {
        adapter->config = *config;
        LOG_DEBUG("[%s] Using provided configuration", CEREBELLUM_LOG_MODULE);
    } else {
        adapter->config = cerebellum_default_config();
        LOG_DEBUG("[%s] Using default configuration", CEREBELLUM_LOG_MODULE);
    }

    /* Initialize aggregate synapse for stats tracking when glomerular layer not present */
    if (adapter->config.enable_synaptic_dynamics) {
        cerebellar_synapse_init(&adapter->aggregate_synapse, &adapter->config.synapse_config);
    }

    /* Create granule layer */
    LOG_DEBUG("[%s] Creating granule layer (%u cells)", CEREBELLUM_LOG_MODULE,
              adapter->config.num_granule_cells);
    adapter->granule = create_granule_layer(adapter->config.num_granule_cells,
                                             adapter->config.num_mossy_fibers);
    if (!adapter->granule) {
        LOG_ERROR("[%s] Failed to create granule layer", CEREBELLUM_LOG_MODULE);
        cerebellum_destroy(adapter);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cerebellum_create: adapter->granule is NULL");
        return NULL;
    }

    /* Create Purkinje layer */
    LOG_DEBUG("[%s] Creating Purkinje layer (%u cells)", CEREBELLUM_LOG_MODULE,
              adapter->config.num_purkinje_cells);
    adapter->purkinje = create_purkinje_layer(adapter->config.num_purkinje_cells,
                                               adapter->config.num_parallel_fibers,
                                               adapter->config.ltd_rate,
                                               adapter->config.ltp_rate);
    if (!adapter->purkinje) {
        LOG_ERROR("[%s] Failed to create Purkinje layer", CEREBELLUM_LOG_MODULE);
        cerebellum_destroy(adapter);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cerebellum_create: adapter->purkinje is NULL");
        return NULL;
    }

    /* Create deep nuclei */
    LOG_DEBUG("[%s] Creating deep nuclei", CEREBELLUM_LOG_MODULE);
    adapter->nuclei = create_deep_nuclei(adapter->config.num_dentate_neurons,
                                          adapter->config.num_interposed_neurons,
                                          adapter->config.num_fastigial_neurons,
                                          adapter->config.num_purkinje_cells);
    if (!adapter->nuclei) {
        LOG_ERROR("[%s] Failed to create deep nuclei", CEREBELLUM_LOG_MODULE);
        cerebellum_destroy(adapter);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cerebellum_create: adapter->nuclei is NULL");
        return NULL;
    }

    /* Create climbing fiber system */
    LOG_DEBUG("[%s] Creating climbing fiber system", CEREBELLUM_LOG_MODULE);
    adapter->climbing = create_climbing_fibers(adapter->config.num_climbing_fibers,
                                                adapter->config.num_purkinje_cells);
    if (!adapter->climbing) {
        LOG_ERROR("[%s] Failed to create climbing fiber system", CEREBELLUM_LOG_MODULE);
        cerebellum_destroy(adapter);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cerebellum_create: adapter->climbing is NULL");
        return NULL;
    }

    /* Initialize forward model */
    adapter->forward_model.input_dim = 8;
    adapter->forward_model.output_dim = 8;
    adapter->forward_model.learning_rate = adapter->config.learning_rate;
    adapter->forward_model.weights = nimcp_calloc(64, sizeof(float));
    if (!adapter->forward_model.weights) {
        LOG_ERROR("[%s] Failed to allocate forward model weights", CEREBELLUM_LOG_MODULE);
        cerebellum_destroy(adapter);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cerebellum_create: adapter->forward_model is NULL");
        return NULL;
    }
    /* Initialize as identity matrix */
    for (uint32_t i = 0; i < 8; i++) {
        adapter->forward_model.weights[i * 8 + i] = 1.0f;
    }

    /* Initialize timing controller */
    adapter->timing.target_timing_ms = 0.0f;
    adapter->timing.predicted_timing_ms = 0.0f;
    adapter->timing.adaptation_rate = 0.1f;

    /* Allocate output buffer */
    adapter->output_capacity = 32;
    adapter->output_buffer = nimcp_calloc(adapter->output_capacity, sizeof(nuclei_output_t));
    if (!adapter->output_buffer) {
        LOG_ERROR("[%s] Failed to allocate output buffer", CEREBELLUM_LOG_MODULE);
        cerebellum_destroy(adapter);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cerebellum_create: adapter->output_buffer is NULL");
        return NULL;
    }

    /*=== Phase 1: Create glomerular layer (if enabled) ===*/
    adapter->glomerular = NULL;
    if (adapter->config.enable_glomerular_divergence) {
        LOG_DEBUG("[%s] Creating glomerular layer (%u granules per mossy)",
                  CEREBELLUM_LOG_MODULE, adapter->config.granules_per_mossy_fiber);
        adapter->glomerular = create_glomerular_layer(
            adapter->config.num_mossy_fibers,
            adapter->config.num_granule_cells,
            adapter->config.granules_per_mossy_fiber,
            adapter->config.enable_synaptic_dynamics);
        if (!adapter->glomerular) {
            LOG_ERROR("[%s] Failed to create glomerular layer", CEREBELLUM_LOG_MODULE);
            cerebellum_destroy(adapter);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cerebellum_create: adapter->glomerular is NULL");
            return NULL;
        }
        /* Set initial divergence stat */
        adapter->stats.effective_divergence = (float)adapter->config.granules_per_mossy_fiber;
    }

    /*=== Phase 2: Create molecular layer interneurons (if enabled) ===*/
    adapter->molecular = NULL;
    if (adapter->config.enable_basket_cells || adapter->config.enable_stellate_cells) {
        uint32_t num_basket = adapter->config.enable_basket_cells ?
                              adapter->config.num_basket_cells : 0;
        uint32_t num_stellate = adapter->config.enable_stellate_cells ?
                                adapter->config.num_stellate_cells : 0;

        LOG_DEBUG("[%s] Creating molecular interneurons (basket=%u, stellate=%u)",
                  CEREBELLUM_LOG_MODULE, num_basket, num_stellate);
        adapter->molecular = create_molecular_interneurons(
            num_basket, adapter->config.purkinje_per_basket,
            num_stellate, adapter->config.purkinje_per_stellate,
            adapter->config.num_purkinje_cells,
            adapter->config.num_parallel_fibers);
        if (!adapter->molecular) {
            LOG_ERROR("[%s] Failed to create molecular interneurons", CEREBELLUM_LOG_MODULE);
            cerebellum_destroy(adapter);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cerebellum_create: adapter->molecular is NULL");
            return NULL;
        }
    }

    /*=== Phase 2: Create Golgi cell network (if enabled) ===*/
    adapter->golgi = NULL;
    if (adapter->config.enable_golgi_cells) {
        LOG_DEBUG("[%s] Creating Golgi network (%u cells)",
                  CEREBELLUM_LOG_MODULE, adapter->config.num_golgi_cells);
        adapter->golgi = create_golgi_network(
            adapter->config.num_golgi_cells,
            adapter->config.granules_per_golgi,
            adapter->config.num_granule_cells);
        if (!adapter->golgi) {
            LOG_ERROR("[%s] Failed to create Golgi network", CEREBELLUM_LOG_MODULE);
            cerebellum_destroy(adapter);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cerebellum_create: adapter->golgi is NULL");
            return NULL;
        }
    }

    /* Initialize metabolic modulation state (Phase 4) */
    adapter->metabolic.motor_coordination = 1.0f;
    adapter->metabolic.timing_precision = 1.0f;
    adapter->metabolic.learning_capacity = 1.0f;
    adapter->metabolic.modulation_active = false;

    /* Initialize bio-async communication */
    adapter->bio_ctx = NULL;
    if (adapter->config.enable_bio_async && bio_router_is_initialized()) {
        LOG_DEBUG("[%s] Registering with bio-async router", CEREBELLUM_LOG_MODULE);

        bio_module_info_t bio_info = {
            .module_id = BIO_MODULE_CEREBELLUM,
            .module_name = "cerebellum",
            .inbox_capacity = 64,
            .user_data = adapter
        };

        adapter->bio_ctx = bio_router_register_module(&bio_info);
        if (adapter->bio_ctx) {
            LOG_INFO("[%s] Bio-async registration successful", CEREBELLUM_LOG_MODULE);
        } else {
            LOG_WARNING("[%s] Failed to register with bio-async router", CEREBELLUM_LOG_MODULE);
        }
    }

    /* Initialize state */
    adapter->status = CEREBELLUM_STATUS_IDLE;
    adapter->last_error = CEREBELLUM_ERROR_NONE;
    adapter->current_time_ms = 0.0;
    adapter->adaptation_level = 0.0f;

    LOG_INFO("[%s] Cerebellum adapter created successfully", CEREBELLUM_LOG_MODULE);
    return adapter;
}

void cerebellum_destroy(cerebellum_adapter_t* adapter) {
    if (!adapter) return;

    LOG_INFO("[%s] Destroying cerebellum adapter", CEREBELLUM_LOG_MODULE);

    /* Unregister from bio-async router */
    if (adapter->bio_ctx) {
        LOG_DEBUG("[%s] Unregistering from bio-async router", CEREBELLUM_LOG_MODULE);
        bio_router_unregister_module(adapter->bio_ctx);
        adapter->bio_ctx = NULL;
    }

    /* Destroy core sub-modules */
    destroy_granule_layer(adapter->granule);
    destroy_purkinje_layer(adapter->purkinje);
    destroy_deep_nuclei(adapter->nuclei);
    destroy_climbing_fibers(adapter->climbing);

    /* Destroy Phase 1-2 sub-modules */
    destroy_glomerular_layer(adapter->glomerular);
    destroy_molecular_interneurons(adapter->molecular);
    destroy_golgi_network(adapter->golgi);

    /* Free forward model */
    if (adapter->forward_model.weights) {
        nimcp_free(adapter->forward_model.weights);
    }

    /* Free output buffer */
    if (adapter->output_buffer) {
        nimcp_free(adapter->output_buffer);
    }

    /* S7-H4 fix (2026-05-19): destroy internal mutex last so any final
     * unlock from a concurrent caller still has a live object. By this
     * point all sub-modules are torn down and the brain holds the only
     * reference to the adapter. */
    if (adapter->mutex) {
        nimcp_mutex_free(adapter->mutex);
        adapter->mutex = NULL;
    }

    LOG_DEBUG("[%s] Cerebellum adapter destroyed", CEREBELLUM_LOG_MODULE);
    nimcp_free(adapter);
}

bool cerebellum_reset(cerebellum_adapter_t* adapter) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cerebellum_reset: adapter is NULL");
        return false;
    }

    LOG_DEBUG("[%s] Resetting adapter state", CEREBELLUM_LOG_MODULE);

    /* Reset granule layer */
    if (adapter->granule) {
        for (uint32_t i = 0; i < adapter->granule->num_cells; i++) {
            adapter->granule->cells[i].activation = 0.0f;
            adapter->granule->cells[i].active = false;
        }
        memset(adapter->granule->mossy_input_buffer, 0,
               adapter->granule->num_mossy_fibers * sizeof(float));
    }

    /* Reset Purkinje layer */
    if (adapter->purkinje) {
        for (uint32_t i = 0; i < adapter->purkinje->num_cells; i++) {
            adapter->purkinje->cells[i].simple_spike_rate = 0.5f;
            adapter->purkinje->cells[i].complex_spike = 0.0f;
            adapter->purkinje->cells[i].learning_eligibility = 0.0f;
        }
    }

    /* Reset output buffer */
    adapter->output_count = 0;

    /* Reset timing */
    adapter->timing.target_timing_ms = 0.0f;
    adapter->timing.predicted_timing_ms = 0.0f;
    memset(adapter->timing.timing_error_history, 0, sizeof(adapter->timing.timing_error_history));
    adapter->timing.error_history_idx = 0;

    /* Reset state */
    adapter->status = CEREBELLUM_STATUS_IDLE;
    adapter->last_error = CEREBELLUM_ERROR_NONE;

    LOG_DEBUG("[%s] Adapter reset complete", CEREBELLUM_LOG_MODULE);
    return true;
}

/*=============================================================================
 * MOSSY FIBER INPUT PROCESSING
 *===========================================================================*/

bool cerebellum_process_mossy_input(cerebellum_adapter_t* adapter,
                                     const mossy_fiber_input_t* input) {
    if (!adapter || !input) {
        set_error(adapter, CEREBELLUM_ERROR_INVALID_INPUT);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cerebellum_reset: required parameter is NULL (adapter, input)");
        return false;
    }

    if (!adapter->granule) {
        set_error(adapter, CEREBELLUM_ERROR_GRANULE_FAILURE);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cerebellum_reset: adapter->granule is NULL");
        return false;
    }

    adapter->status = CEREBELLUM_STATUS_RECEIVING;

    /* Store input in mossy buffer */
    uint32_t idx = input->fiber_id % adapter->granule->num_mossy_fibers;
    adapter->granule->mossy_input_buffer[idx] = input->activity;

    adapter->stats.mossy_inputs_processed++;

    return true;
}

bool cerebellum_process_mossy_batch(cerebellum_adapter_t* adapter,
                                     const mossy_fiber_input_t* inputs,
                                     uint32_t count) {
    if (!adapter || !inputs || count == 0) {
        set_error(adapter, CEREBELLUM_ERROR_INVALID_INPUT);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cerebellum_reset: required parameter is NULL (adapter, inputs)");
        return false;
    }

    for (uint32_t i = 0; i < count; i++) {
        if (!cerebellum_process_mossy_input(adapter, &inputs[i])) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cerebellum_reset: cerebellum_process_mossy_input is NULL");
            return false;
        }
    }

    return true;
}

/*=============================================================================
 * CLIMBING FIBER ERROR PROCESSING
 *===========================================================================*/

bool cerebellum_process_climbing_signal(cerebellum_adapter_t* adapter,
                                         const climbing_fiber_signal_t* signal) {
    if (!adapter || !signal) {
        set_error(adapter, CEREBELLUM_ERROR_INVALID_INPUT);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cerebellum_reset: required parameter is NULL (adapter, signal)");
        return false;
    }

    if (!adapter->climbing || !adapter->purkinje) {
        set_error(adapter, CEREBELLUM_ERROR_LEARNING_FAILURE);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cerebellum_reset: required parameter is NULL (adapter->climbing, adapter->purkinje)");
        return false;
    }

    adapter->status = CEREBELLUM_STATUS_LEARNING;

    /* Store error signal */
    uint32_t cf_idx = signal->fiber_id % adapter->climbing->num_fibers;
    adapter->climbing->error_signals[cf_idx] = signal->error_signal;

    /* Trigger complex spike in target Purkinje cell */
    uint32_t pc_idx = signal->target_purkinje_id % adapter->purkinje->num_cells;
    adapter->purkinje->cells[pc_idx].complex_spike = 1.0f;
    adapter->stats.purkinje_complex_spikes++;

    /* Apply LTD learning if error exceeds threshold */
    if (fabsf(signal->error_signal) > adapter->climbing->error_threshold) {
        if (adapter->config.enable_error_learning) {
            apply_ltd_learning(adapter, pc_idx, signal->error_signal);
        }
    }

    adapter->stats.climbing_signals++;

    return true;
}

bool cerebellum_broadcast_error(cerebellum_adapter_t* adapter,
                                 float error_magnitude,
                                 uint8_t error_type) {
    if (!adapter) {
        set_error(adapter, CEREBELLUM_ERROR_INVALID_INPUT);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cerebellum_reset: adapter is NULL");
        return false;
    }

    /* S7-H4 fix (2026-05-19): serialize against concurrent
     * predict/update from cascade callers + bio-router tick. The lock
     * protects climbing->error_signals[] + Purkinje cell state mutated
     * by process_climbing_signal. */
    if (adapter->mutex) nimcp_mutex_lock(adapter->mutex);

    /* Create signal for each climbing fiber */
    for (uint32_t i = 0; i < adapter->climbing->num_fibers; i++) {
        climbing_fiber_signal_t signal;
        signal.fiber_id = i;
        signal.error_signal = error_magnitude;
        signal.timestamp_ms = (float)adapter->current_time_ms;
        signal.target_purkinje_id = adapter->climbing->purkinje_targets[i];
        signal.error_type = error_type;

        cerebellum_process_climbing_signal(adapter, &signal);
    }

    if (adapter->mutex) nimcp_mutex_unlock(adapter->mutex);
    return true;
}

/*=============================================================================
 * MOTOR COORDINATION PIPELINE
 *===========================================================================*/

bool cerebellum_begin_coordination(cerebellum_adapter_t* adapter) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cerebellum_begin_coordination: adapter is NULL");
        return false;
    }

    cerebellum_reset(adapter);
    adapter->status = CEREBELLUM_STATUS_IDLE;

    return true;
}

bool cerebellum_process(cerebellum_adapter_t* adapter,
                         motor_coordination_result_t* result) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cerebellum_begin_coordination: adapter is NULL");
        return false;
    }

    adapter->status = CEREBELLUM_STATUS_PROCESSING;

    /*=== PHASE 1: Glomerular layer processing (if enabled) ===*/
    /* Distributes mossy fiber input with enhanced divergence (50-100x) */
    if (adapter->config.enable_glomerular_divergence && adapter->glomerular) {
        process_glomerular_layer(adapter);
    }

    /*=== PHASE 2: Golgi cell processing (if enabled) ===*/
    /* Computes feedback inhibition from parallel fibers to granule layer */
    if (adapter->config.enable_golgi_cells && adapter->golgi) {
        process_golgi_cells(adapter);
    }

    /* Process granule layer (standard pathway) */
    process_granule_layer(adapter);

    /*=== Apply Golgi inhibition to granule cells (if enabled) ===*/
    if (adapter->config.enable_golgi_cells && adapter->golgi) {
        apply_golgi_inhibition(adapter);
    }

    /*=== PHASE 2: Molecular layer interneuron processing (if enabled) ===*/
    /* Basket cells provide somatic inhibition to Purkinje cells */
    if (adapter->config.enable_basket_cells && adapter->molecular) {
        process_basket_cells(adapter);
    }

    /* Stellate cells provide dendritic inhibition to Purkinje cells */
    if (adapter->config.enable_stellate_cells && adapter->molecular) {
        process_stellate_cells(adapter);
    }

    /* Process Purkinje layer */
    process_purkinje_layer(adapter);

    /*=== Apply interneuron inhibition to Purkinje cells (if enabled) ===*/
    if (adapter->config.enable_basket_cells && adapter->molecular) {
        apply_basket_inhibition(adapter);
    }
    if (adapter->config.enable_stellate_cells && adapter->molecular) {
        apply_stellate_inhibition(adapter);
    }

    /* Process deep nuclei */
    process_deep_nuclei(adapter);

    /* Timing computation */
    if (adapter->config.enable_timing) {
        adapter->status = CEREBELLUM_STATUS_TIMING;

        /* Predict timing based on Purkinje activity patterns */
        float timing_estimate = 0.0f;
        for (uint32_t i = 0; i < adapter->purkinje->num_cells; i++) {
            timing_estimate += adapter->purkinje->cells[i].simple_spike_rate;
        }
        timing_estimate = (adapter->purkinje->num_cells > 0) ?
            (timing_estimate / adapter->purkinje->num_cells * 100.0f) : 0.0f;  /* ms */

        adapter->timing.predicted_timing_ms = timing_estimate;
    }

    /* Fill result if provided */
    if (result) {
        memset(result, 0, sizeof(motor_coordination_result_t));

        result->predicted_timing_ms = adapter->timing.predicted_timing_ms;
        result->timing_error_ms = adapter->timing.predicted_timing_ms - adapter->timing.target_timing_ms;
        result->timing_correction = -result->timing_error_ms * 0.1f;

        /* Get motor gains from dentate nucleus */
        for (uint32_t i = 0; i < 8 && i < adapter->nuclei->num_dentate; i++) {
            result->motor_gain[i] = adapter->nuclei->dentate[i].activity;
        }
        result->num_motor_dims = 8;
        result->motor_ready = true;

        result->current_error = adapter->forward_model.last_error;
        result->learning_progress = 1.0f - fabsf(result->current_error);
        result->adaptation_active = adapter->config.enable_motor_adaptation;

        /* Forward model prediction */
        memcpy(result->predicted_outcome, adapter->forward_model.last_prediction,
               sizeof(result->predicted_outcome));
        result->confidence = 0.8f;  /* Base confidence */
    }

    /* Update synaptic dynamics stats from glomerular synapses */
    if (adapter->config.enable_synaptic_dynamics && adapter->glomerular) {
        uint64_t total_releases = 0;
        float total_calcium = 0.0f;
        uint32_t synapse_count = 0;

        for (uint32_t i = 0; i < adapter->glomerular->num_glomeruli; i++) {
            cerebellar_synapse_state_t* syn = &adapter->glomerular->glomeruli[i].synapse;
            /* Count vesicles that have been released (capacity - available) */
            if (syn->rrp_capacity > 0 && syn->rrp_available < syn->rrp_capacity) {
                total_releases += (syn->rrp_capacity - syn->rrp_available);
            }
            total_calcium += syn->calcium_concentration;
            synapse_count++;
        }

        adapter->stats.vesicle_releases += total_releases;
        if (synapse_count > 0) {
            adapter->stats.avg_calcium_concentration = total_calcium / synapse_count;
        }
    } else if (adapter->config.enable_synaptic_dynamics && !adapter->glomerular) {
        /* Use aggregate synapse for stats when glomerular layer not present */
        cerebellar_synapse_state_t* syn = &adapter->aggregate_synapse;
        if (syn->rrp_capacity > 0 && syn->rrp_available < syn->rrp_capacity) {
            adapter->stats.vesicle_releases += (syn->rrp_capacity - syn->rrp_available);
        }
        adapter->stats.avg_calcium_concentration = syn->calcium_concentration;
    }

    /* Update glomerular divergence stats */
    if (adapter->config.enable_glomerular_divergence && adapter->glomerular) {
        adapter->stats.effective_divergence = (float)adapter->config.granules_per_mossy_fiber;
    }

    /* Update interneuron stats */
    if (adapter->config.enable_basket_cells && adapter->molecular &&
        adapter->molecular->basket_cells) {
        float total_inh = 0.0f;
        float total_rate = 0.0f;
        for (uint32_t i = 0; i < adapter->molecular->num_basket_cells; i++) {
            total_inh += adapter->molecular->basket_cells[i].somatic_inhibition;
            total_rate += adapter->molecular->basket_cells[i].firing_rate;
        }
        adapter->stats.basket_inhibition_total += total_inh;
        if (adapter->molecular->num_basket_cells > 0) {
            /* Convert activation to Hz (scale by ~100 Hz max firing rate) */
            adapter->stats.avg_basket_firing_rate =
                (total_rate / adapter->molecular->num_basket_cells) * 100.0f;
        }
    }

    if (adapter->config.enable_stellate_cells && adapter->molecular &&
        adapter->molecular->stellate_cells) {
        float total_inh = 0.0f;
        float total_rate = 0.0f;
        for (uint32_t i = 0; i < adapter->molecular->num_stellate_cells; i++) {
            total_inh += adapter->molecular->stellate_cells[i].dendritic_inhibition;
            total_rate += adapter->molecular->stellate_cells[i].activation;
        }
        adapter->stats.stellate_inhibition_total += total_inh;
        if (adapter->molecular->num_stellate_cells > 0) {
            adapter->stats.avg_stellate_firing_rate =
                (total_rate / adapter->molecular->num_stellate_cells) * 100.0f;
        }
    }

    if (adapter->config.enable_golgi_cells && adapter->golgi) {
        float total_feedback = 0.0f;
        float total_rate = 0.0f;
        for (uint32_t i = 0; i < adapter->golgi->num_cells; i++) {
            total_feedback += adapter->golgi->cells[i].feedback_inhibition;
            total_rate += adapter->golgi->cells[i].activation;
        }
        adapter->stats.golgi_feedback_total += total_feedback;
        if (adapter->golgi->num_cells > 0) {
            adapter->stats.avg_golgi_firing_rate =
                (total_rate / adapter->golgi->num_cells) * 100.0f;
        }
    }

    adapter->status = CEREBELLUM_STATUS_OUTPUT_READY;
    adapter->stats.motor_commands_output++;

    emit_event(adapter, 1 /* CEREBELLUM_EVENT_PROCESSING_COMPLETE */, result);

    return true;
}

bool cerebellum_get_nuclei_output(cerebellum_adapter_t* adapter,
                                   nuclei_output_t* output) {
    if (!adapter || !output || !adapter->nuclei) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cerebellum_begin_coordination: required parameter is NULL (adapter, output, adapter->nuclei)");
        return false;
    }

    /* Aggregate output from all nuclei */
    memset(output, 0, sizeof(nuclei_output_t));

    output->nucleus_id = 0;  /* Combined output */
    output->timestamp_ms = (float)adapter->current_time_ms;

    /* Sum activity from all nuclei */
    float total_activity = 0.0f;

    for (uint32_t i = 0; i < adapter->nuclei->num_dentate; i++) {
        total_activity += adapter->nuclei->dentate[i].activity;
        for (uint32_t d = 0; d < 8; d++) {
            output->motor_command[d] += adapter->nuclei->dentate[i].motor_output[d];
        }
    }

    for (uint32_t i = 0; i < adapter->nuclei->num_interposed; i++) {
        total_activity += adapter->nuclei->interposed[i].activity;
    }

    for (uint32_t i = 0; i < adapter->nuclei->num_fastigial; i++) {
        total_activity += adapter->nuclei->fastigial[i].activity;
    }

    uint32_t total_neurons = adapter->nuclei->num_dentate +
                              adapter->nuclei->num_interposed +
                              adapter->nuclei->num_fastigial;
    output->activity = (total_neurons > 0) ? (total_activity / total_neurons) : 0.0f;
    output->num_dimensions = 8;

    /* Timing adjustment from Purkinje activity */
    output->timing_adjustment = adapter->timing.predicted_timing_ms - adapter->timing.target_timing_ms;

    /* Invoke callback if set */
    if (adapter->motor_callback) {
        adapter->motor_callback(output, adapter->motor_user_data);
    }

    return true;
}

/*=============================================================================
 * TIMING CONTROL
 *===========================================================================*/

bool cerebellum_set_target_timing(cerebellum_adapter_t* adapter, float target_ms) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cerebellum_set_target_timing: adapter is NULL");
        return false;
    }

    adapter->timing.target_timing_ms = target_ms;
    return true;
}

bool cerebellum_predict_timing(cerebellum_adapter_t* adapter,
                                float* predicted_ms,
                                float* confidence) {
    if (!adapter || !predicted_ms) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cerebellum_set_target_timing: required parameter is NULL (adapter, predicted_ms)");
        return false;
    }

    *predicted_ms = adapter->timing.predicted_timing_ms;

    if (confidence) {
        /* Confidence based on history consistency */
        float variance = 0.0f;
        float mean = 0.0f;
        uint32_t count = 0;

        for (uint32_t i = 0; i < 16; i++) {
            if (adapter->timing.timing_error_history[i] != 0.0f) {
                mean += adapter->timing.timing_error_history[i];
                count++;
            }
        }
        if (count > 0) {
            mean /= count;
            for (uint32_t i = 0; i < 16 && count > 1; i++) {
                float diff = adapter->timing.timing_error_history[i] - mean;
                variance += diff * diff;
            }
            variance /= count;
        }

        /* Lower variance = higher confidence */
        *confidence = 1.0f / (1.0f + sqrtf(variance) / 10.0f);
    }

    return true;
}

bool cerebellum_report_timing(cerebellum_adapter_t* adapter, float actual_ms) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cerebellum_report_timing: adapter is NULL");
        return false;
    }

    adapter->timing.actual_timing_ms = actual_ms;

    /* Compute timing error */
    float error = actual_ms - adapter->timing.target_timing_ms;

    /* Store in history */
    adapter->timing.timing_error_history[adapter->timing.error_history_idx] = error;
    adapter->timing.error_history_idx = (adapter->timing.error_history_idx + 1) % 16;

    /* Update stats */
    adapter->stats.avg_timing_error_ms = (adapter->stats.avg_timing_error_ms * 0.9f) +
                                          (fabsf(error) * 0.1f);
    if (fabsf(error) > adapter->stats.max_timing_error_ms) {
        adapter->stats.max_timing_error_ms = fabsf(error);
    }

    /* Trigger learning if error is significant */
    if (fabsf(error) > adapter->config.timing_resolution_ms) {
        /* Broadcast timing error as climbing fiber signal */
        if (adapter->config.enable_error_learning) {
            cerebellum_broadcast_error(adapter, error / 100.0f, 1 /* TIMING_ERROR */);
        }
    }

    return true;
}

/*=============================================================================
 * FORWARD MODEL OPERATIONS
 *===========================================================================*/

bool cerebellum_update_forward_model(cerebellum_adapter_t* adapter,
                                      const float* motor_command,
                                      const float* outcome,
                                      uint32_t num_dims) {
    if (!adapter || !motor_command || !outcome || num_dims == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cerebellum_report_timing: required parameter is NULL (adapter, motor_command, outcome)");
        return false;
    }
    if (!adapter->config.enable_forward_models) return true;

    /* S7-H4 fix (2026-05-19): serialize forward_model.weights mutation
     * against concurrent predict + broadcast callers. */
    if (adapter->mutex) nimcp_mutex_lock(adapter->mutex);

    forward_model_t* fm = &adapter->forward_model;

    /* Predict outcome */
    float prediction[8] = {0};
    uint32_t dims = (num_dims < 8) ? num_dims : 8;

    for (uint32_t i = 0; i < dims; i++) {
        for (uint32_t j = 0; j < dims; j++) {
            prediction[i] += motor_command[j] * fm->weights[i * 8 + j];
        }
    }

    /* Compute error */
    float error = 0.0f;
    for (uint32_t i = 0; i < dims; i++) {
        float diff = outcome[i] - prediction[i];
        error += diff * diff;

        /* Update weights (gradient descent) */
        for (uint32_t j = 0; j < dims; j++) {
            fm->weights[i * 8 + j] += fm->learning_rate * diff * motor_command[j];
        }
    }

    fm->last_error = sqrtf(error / dims);
    memcpy(fm->last_prediction, prediction, sizeof(prediction));

    /* Update stats */
    adapter->stats.avg_error_after_learning = fm->last_error;

    if (adapter->mutex) nimcp_mutex_unlock(adapter->mutex);
    return true;
}

bool cerebellum_predict_outcome(cerebellum_adapter_t* adapter,
                                 const float* motor_command,
                                 uint32_t num_dims,
                                 float* predicted_outcome,
                                 float* confidence) {
    if (!adapter || !motor_command || !predicted_outcome) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cerebellum_report_timing: required parameter is NULL (adapter, motor_command, predicted_outcome)");
        return false;
    }

    /* S7-H4 fix (2026-05-19): serialize forward_model.weights read
     * against concurrent update writer. The lock is held across the
     * prediction loop + last_prediction write so a concurrent update
     * can't observe a partial snapshot of weights. */
    if (adapter->mutex) nimcp_mutex_lock(adapter->mutex);

    forward_model_t* fm = &adapter->forward_model;
    uint32_t dims = (num_dims < 8) ? num_dims : 8;

    /* Compute prediction */
    memset(predicted_outcome, 0, dims * sizeof(float));
    for (uint32_t i = 0; i < dims; i++) {
        for (uint32_t j = 0; j < dims; j++) {
            predicted_outcome[i] += motor_command[j] * fm->weights[i * 8 + j];
        }
    }

    if (confidence) {
        /* Confidence based on recent error */
        *confidence = 1.0f / (1.0f + fm->last_error);
    }

    memcpy(fm->last_prediction, predicted_outcome, dims * sizeof(float));

    if (adapter->mutex) nimcp_mutex_unlock(adapter->mutex);
    return true;
}

/*=============================================================================
 * MOTOR ADAPTATION
 *===========================================================================*/

bool cerebellum_adapt_gains(cerebellum_adapter_t* adapter,
                             float* gains,
                             uint32_t num_dims,
                             float adaptation_rate) {
    if (!adapter || !gains || num_dims == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cerebellum_report_timing: required parameter is NULL (adapter, gains)");
        return false;
    }
    if (!adapter->config.enable_motor_adaptation) return true;

    uint32_t dims = (num_dims < 8) ? num_dims : 8;

    /* Get adaptation signal from deep nuclei */
    for (uint32_t i = 0; i < dims && i < adapter->nuclei->num_dentate; i++) {
        float adaptation = (adapter->nuclei->dentate[i].activity - 0.5f) * adaptation_rate;
        gains[i] += adaptation;

        /* Clamp gains */
        if (gains[i] < 0.1f) gains[i] = 0.1f;
        if (gains[i] > 2.0f) gains[i] = 2.0f;
    }

    /* Update adaptation level */
    adapter->adaptation_level = (adapter->adaptation_level * 0.99f) + (adaptation_rate * 0.01f);

    return true;
}

bool cerebellum_get_adaptation_state(cerebellum_adapter_t* adapter,
                                      float* adaptation_level) {
    if (!adapter || !adaptation_level) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cerebellum_report_timing: required parameter is NULL (adapter, adaptation_level)");
        return false;
    }

    *adaptation_level = adapter->adaptation_level;
    return true;
}

/*=============================================================================
 * CALLBACK REGISTRATION
 *===========================================================================*/

bool cerebellum_set_motor_callback(cerebellum_adapter_t* adapter,
                                    cerebellum_motor_callback_t callback,
                                    void* user_data) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cerebellum_report_timing: adapter is NULL");
        return false;
    }
    adapter->motor_callback = callback;
    adapter->motor_user_data = user_data;
    return true;
}

bool cerebellum_set_error_callback(cerebellum_adapter_t* adapter,
                                    cerebellum_error_callback_t callback,
                                    void* user_data) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cerebellum_report_timing: adapter is NULL");
        return false;
    }
    adapter->error_callback = callback;
    adapter->error_user_data = user_data;
    return true;
}

bool cerebellum_set_event_callback(cerebellum_adapter_t* adapter,
                                    cerebellum_event_callback_t callback,
                                    void* user_data) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cerebellum_report_timing: adapter is NULL");
        return false;
    }
    adapter->event_callback = callback;
    adapter->event_user_data = user_data;
    return true;
}

/*=============================================================================
 * STATUS AND DIAGNOSTICS
 *===========================================================================*/

cerebellum_status_t cerebellum_get_status(const cerebellum_adapter_t* adapter) {
    if (!adapter) return CEREBELLUM_STATUS_ERROR;
    return adapter->status;
}

cerebellum_error_t cerebellum_get_last_error(const cerebellum_adapter_t* adapter) {
    if (!adapter) return CEREBELLUM_ERROR_INTERNAL;
    return adapter->last_error;
}

const char* cerebellum_error_string(cerebellum_error_t error) {
    switch (error) {
        case CEREBELLUM_ERROR_NONE: return "No error";
        case CEREBELLUM_ERROR_INVALID_INPUT: return "Invalid input";
        case CEREBELLUM_ERROR_GRANULE_FAILURE: return "Granule layer failure";
        case CEREBELLUM_ERROR_PURKINJE_FAILURE: return "Purkinje layer failure";
        case CEREBELLUM_ERROR_TIMING_FAILURE: return "Timing computation failure";
        case CEREBELLUM_ERROR_LEARNING_FAILURE: return "Learning failure";
        case CEREBELLUM_ERROR_NUCLEI_FAILURE: return "Deep nuclei failure";
        case CEREBELLUM_ERROR_BUFFER_OVERFLOW: return "Buffer overflow";
        case CEREBELLUM_ERROR_INTERNAL: return "Internal error";
        default: return "Unknown error";
    }
}

const char* cerebellum_status_string(cerebellum_status_t status) {
    switch (status) {
        case CEREBELLUM_STATUS_IDLE: return "Idle";
        case CEREBELLUM_STATUS_RECEIVING: return "Receiving input";
        case CEREBELLUM_STATUS_PROCESSING: return "Processing";
        case CEREBELLUM_STATUS_TIMING: return "Timing computation";
        case CEREBELLUM_STATUS_LEARNING: return "Learning";
        case CEREBELLUM_STATUS_OUTPUT_READY: return "Output ready";
        case CEREBELLUM_STATUS_ERROR: return "Error";
        default: return "Unknown";
    }
}

bool cerebellum_get_stats(const cerebellum_adapter_t* adapter, cerebellum_stats_t* stats) {
    if (!adapter || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cerebellum_get_stats: required parameter is NULL (adapter, stats)");
        return false;
    }
    *stats = adapter->stats;
    return true;
}

bool cerebellum_get_config(const cerebellum_adapter_t* adapter, cerebellum_config_t* config) {
    if (!adapter || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cerebellum_get_config: required parameter is NULL (adapter, config)");
        return false;
    }
    *config = adapter->config;
    return true;
}

/*=============================================================================
 * SUB-MODULE ACCESS
 *===========================================================================*/

granule_layer_t* cerebellum_get_granule_layer(cerebellum_adapter_t* adapter) {
    if (!adapter) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adapter is NULL");

        return NULL;

    }
    return adapter->granule;
}

purkinje_layer_t* cerebellum_get_purkinje_layer(cerebellum_adapter_t* adapter) {
    if (!adapter) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adapter is NULL");

        return NULL;

    }
    return adapter->purkinje;
}

deep_nuclei_t* cerebellum_get_deep_nuclei(cerebellum_adapter_t* adapter) {
    if (!adapter) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adapter is NULL");

        return NULL;

    }
    return adapter->nuclei;
}

/*=============================================================================
 * BIO-ASYNC COMMUNICATION
 *===========================================================================*/

bio_module_context_t cerebellum_get_bio_context(cerebellum_adapter_t* adapter) {
    if (!adapter) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adapter is NULL");

        return NULL;

    }
    return adapter->bio_ctx;
}

uint32_t cerebellum_process_bio_messages(cerebellum_adapter_t* adapter, uint32_t max_messages) {
    if (!adapter || !adapter->bio_ctx) return 0;

    uint32_t processed = bio_router_process_inbox(adapter->bio_ctx, max_messages);
    if (processed > 0) {
        LOG_DEBUG("[%s] Processed %u bio-async messages", CEREBELLUM_LOG_MODULE, processed);
    }
    return processed;
}

nimcp_bio_future_t cerebellum_request_coordination_async(
    cerebellum_adapter_t* adapter,
    const float* motor_command,
    uint32_t num_dims) {

    if (!adapter || !adapter->bio_ctx || !motor_command) {
        LOG_WARNING("[%s] Cannot request coordination: invalid arguments", CEREBELLUM_LOG_MODULE);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cerebellum_process_bio_messages: required parameter is NULL (adapter, adapter->bio_ctx, motor_command)");
        return NULL;
    }

    LOG_DEBUG("[%s] Requesting motor coordination async", CEREBELLUM_LOG_MODULE);

    /* Create motor coordination request message */
    bio_msg_motor_command_request_t msg;
    memset(&msg, 0, sizeof(msg));

    msg.header.type = BIO_MSG_MOTOR_COMMAND_REQUEST;
    msg.header.source_module = BIO_MODULE_CEREBELLUM;
    msg.header.target_module = BIO_MODULE_MOTOR_CORTEX;
    msg.header.payload_size = sizeof(msg);
    msg.header.channel = adapter->config.default_channel;

    /* Store first motor command value */
    if (num_dims > 0) {
        msg.phoneme = (uint8_t)(motor_command[0] * 255.0f);  /* Reuse field */
    }

    nimcp_bio_promise_t promise = bio_router_send_async(
        adapter->bio_ctx, &msg, sizeof(msg), adapter->config.default_channel);

    if (!promise) {
        LOG_ERROR("[%s] Failed to send coordination request", CEREBELLUM_LOG_MODULE);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cerebellum_process_bio_messages: promise is NULL");
        return NULL;
    }

    return nimcp_bio_promise_get_future(promise);
}

nimcp_error_t cerebellum_broadcast_adaptation_complete(
    cerebellum_adapter_t* adapter,
    const motor_coordination_result_t* result) {

    if (!adapter || !result) {
        return NIMCP_BIO_ERROR_NOT_INITIALIZED;
    }

    if (!adapter->bio_ctx) {
        LOG_DEBUG("[%s] Cannot broadcast: bio-async not available", CEREBELLUM_LOG_MODULE);
        return NIMCP_SUCCESS;
    }

    LOG_INFO("[%s] Broadcasting adaptation complete (error=%.4f)",
             CEREBELLUM_LOG_MODULE, result->current_error);

    bio_msg_motor_command_result_t msg;
    memset(&msg, 0, sizeof(msg));

    msg.header.type = BIO_MSG_MOTOR_COMMAND_RESULT;
    msg.header.source_module = BIO_MODULE_CEREBELLUM;
    msg.header.target_module = 0;  /* Broadcast */
    msg.header.payload_size = sizeof(msg);
    msg.header.channel = adapter->config.default_channel;
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;

    /* Store results in available fields */
    msg.lip_aperture = result->motor_gain[0];
    msg.tongue_height = result->motor_gain[1];
    msg.timestamp_ms = result->predicted_timing_ms;

    return bio_router_broadcast(adapter->bio_ctx, &msg, sizeof(msg));
}

/*=============================================================================
 * PHASE 1-2: NEW SUB-MODULE ACCESS
 *===========================================================================*/

glomerular_layer_t* cerebellum_get_glomerular_layer(cerebellum_adapter_t* adapter) {
    if (!adapter) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adapter is NULL");

        return NULL;

    }
    return adapter->glomerular;
}

molecular_layer_interneurons_t* cerebellum_get_molecular_interneurons(
    cerebellum_adapter_t* adapter) {
    if (!adapter) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adapter is NULL");

        return NULL;

    }
    return adapter->molecular;
}

golgi_cell_network_t* cerebellum_get_golgi_network(cerebellum_adapter_t* adapter) {
    if (!adapter) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adapter is NULL");

        return NULL;

    }
    return adapter->golgi;
}

/*=============================================================================
 * PHASE 2: INTERNEURON QUERY FUNCTIONS
 *===========================================================================*/

bool cerebellum_get_basket_activity(const cerebellum_adapter_t* adapter,
                                     uint32_t basket_id,
                                     float* activation,
                                     float* firing_rate) {
    if (!adapter || !adapter->molecular || !adapter->molecular->basket_cells) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cerebellum_get_golgi_network: required parameter is NULL (adapter, adapter->molecular, adapter->molecular->basket_cells)");
        return false;
    }

    if (basket_id >= adapter->molecular->num_basket_cells) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cerebellum_get_golgi_network: capacity exceeded");
        return false;
    }

    basket_cell_t* bc = &adapter->molecular->basket_cells[basket_id];

    if (activation) {
        *activation = bc->activation;
    }
    if (firing_rate) {
        *firing_rate = bc->firing_rate;
    }

    return true;
}

bool cerebellum_get_purkinje_inhibition(const cerebellum_adapter_t* adapter,
                                         uint32_t purkinje_id,
                                         float* somatic_inhibition,
                                         float* dendritic_inhibition) {
    if (!adapter || !adapter->purkinje) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cerebellum_get_golgi_network: required parameter is NULL (adapter, adapter->purkinje)");
        return false;
    }

    if (purkinje_id >= adapter->purkinje->num_cells) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cerebellum_get_golgi_network: capacity exceeded");
        return false;
    }

    /* Calculate somatic inhibition from basket cells */
    float somatic = 0.0f;
    if (adapter->molecular && adapter->molecular->basket_cells) {
        for (uint32_t i = 0; i < adapter->molecular->num_basket_cells; i++) {
            basket_cell_t* bc = &adapter->molecular->basket_cells[i];
            for (uint32_t j = 0; j < bc->num_purkinje_targets; j++) {
                if (bc->target_purkinje_ids[j] == purkinje_id) {
                    somatic += bc->somatic_inhibition * fabsf(bc->purkinje_weights[j]);
                }
            }
        }
    }

    /* Calculate dendritic inhibition from stellate cells */
    float dendritic = 0.0f;
    if (adapter->molecular && adapter->molecular->stellate_cells) {
        for (uint32_t i = 0; i < adapter->molecular->num_stellate_cells; i++) {
            stellate_cell_t* sc = &adapter->molecular->stellate_cells[i];
            for (uint32_t j = 0; j < sc->num_purkinje_targets; j++) {
                if (sc->target_purkinje_ids[j] == purkinje_id) {
                    dendritic += sc->dendritic_inhibition * fabsf(sc->purkinje_weights[j]);
                }
            }
        }
    }

    if (somatic_inhibition) {
        *somatic_inhibition = somatic;
    }
    if (dendritic_inhibition) {
        *dendritic_inhibition = dendritic;
    }

    return true;
}

bool cerebellum_get_golgi_feedback(const cerebellum_adapter_t* adapter,
                                    float* total_feedback,
                                    float* avg_granule_inhibition) {
    if (!adapter || !adapter->golgi) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cerebellum_get_golgi_network: required parameter is NULL (adapter, adapter->golgi)");
        return false;
    }

    float total = 0.0f;
    for (uint32_t i = 0; i < adapter->golgi->num_cells; i++) {
        total += adapter->golgi->cells[i].feedback_inhibition;
    }

    if (total_feedback) {
        *total_feedback = total;
    }

    if (avg_granule_inhibition) {
        if (adapter->granule && adapter->granule->num_cells > 0) {
            *avg_granule_inhibition = total / adapter->granule->num_cells;
        } else {
            *avg_granule_inhibition = 0.0f;
        }
    }

    return true;
}

/*=============================================================================
 * PHASE 3-4: VESTIBULAR AND METABOLIC MODULATION (Stubs/Partial)
 *===========================================================================*/

bool cerebellum_process_vestibular_input(cerebellum_adapter_t* adapter,
                                          const vestibular_mossy_signal_t* input) {
    if (!adapter || !input) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cerebellum_get_golgi_network: required parameter is NULL (adapter, input)");
        return false;
    }

    if (!adapter->config.enable_vestibulocerebellum) {
        LOG_DEBUG("[%s] Vestibulocerebellum not enabled", CEREBELLUM_LOG_MODULE);
        return true;  /* Not an error, just disabled */
    }

    /* TODO: Phase 3 implementation - route to flocculus/nodulus */
    adapter->stats.vestibular_inputs++;

    return true;
}

bool cerebellum_get_vor_output(cerebellum_adapter_t* adapter,
                                float vor_output[3]) {
    if (!adapter || !vor_output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cerebellum_get_golgi_network: required parameter is NULL (adapter, vor_output)");
        return false;
    }

    if (!adapter->config.enable_vestibulocerebellum) {
        vor_output[0] = 0.0f;
        vor_output[1] = 0.0f;
        vor_output[2] = 0.0f;
        return true;  /* Not an error, just return zeros */
    }

    /* TODO: Phase 3 implementation - get from flocculus output */
    vor_output[0] = 0.0f;  /* Yaw */
    vor_output[1] = 0.0f;  /* Pitch */
    vor_output[2] = 0.0f;  /* Roll */

    return true;
}

bool cerebellum_set_metabolic_modulation(cerebellum_adapter_t* adapter,
                                          float motor_coordination,
                                          float timing_precision,
                                          float learning_capacity) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cerebellum_get_golgi_network: adapter is NULL");
        return false;
    }

    /* Clamp values to valid range [0.3, 1.0] per requirements */
    adapter->metabolic.motor_coordination = fmaxf(0.3f, fminf(1.0f, motor_coordination));
    adapter->metabolic.timing_precision = fmaxf(0.3f, fminf(1.0f, timing_precision));
    adapter->metabolic.learning_capacity = fmaxf(0.3f, fminf(1.0f, learning_capacity));
    adapter->metabolic.modulation_active = true;

    LOG_DEBUG("[%s] Metabolic modulation set: coord=%.2f, timing=%.2f, learning=%.2f",
              CEREBELLUM_LOG_MODULE,
              adapter->metabolic.motor_coordination,
              adapter->metabolic.timing_precision,
              adapter->metabolic.learning_capacity);

    return true;
}
