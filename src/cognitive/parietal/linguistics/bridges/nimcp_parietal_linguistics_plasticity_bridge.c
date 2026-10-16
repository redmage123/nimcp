/**
 * @file nimcp_parietal_linguistics_plasticity_bridge.c
 * @brief Plasticity Integration Bridge Implementation for Parietal Linguistics
 * @version 1.0.0
 * @date 2026-01-31
 */

#include "cognitive/parietal/linguistics/bridges/nimcp_parietal_linguistics_plasticity_bridge.h"
#include "constants/nimcp_buffer_constants.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <math.h>
#include <time.h>
#include "utils/thread/nimcp_thread_rand.h"
#include "constants/nimcp_constants.h"

/* ============================================================================
 * PRIVATE CONSTANTS
 * ============================================================================ */

#define PLASTICITY_MAGIC 0x4C504C41  /* "LPLA" */

/* Hash table sizes */
#define WORD_SYNAPSE_HASH_SIZE 4096
#define SEQUENCE_SYNAPSE_HASH_SIZE 2048
#define BCM_SYNAPSE_HASH_SIZE 2048

/* Thread-local error message */
static _Thread_local char g_last_error[NIMCP_ERROR_BUFFER_SIZE] = {0};

/* ============================================================================
 * PRIVATE HELPER FUNCTIONS
 * ============================================================================ */

static void set_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(g_last_error, sizeof(g_last_error), fmt, args);
    va_end(args);
}

static uint32_t hash_pair(uint32_t a, uint32_t b) {
    /* Cantor pairing function */
    uint32_t sum = a + b;
    return (sum * (sum + 1)) / 2 + b;
}

/* ============================================================================
 * INTERNAL STRUCTURES
 * ============================================================================ */

/**
 * @brief Hash table entry for word synapses
 */
typedef struct word_synapse_entry_s {
    ling_word_synapse_t synapse;
    struct word_synapse_entry_s* next;
    bool active;
} word_synapse_entry_t;

/**
 * @brief Hash table entry for sequence synapses
 */
typedef struct sequence_synapse_entry_s {
    ling_sequence_synapse_t synapse;
    struct sequence_synapse_entry_s* next;
    bool active;
} sequence_synapse_entry_t;

/**
 * @brief Hash table entry for BCM synapses
 */
typedef struct bcm_synapse_entry_s {
    ling_bcm_synapse_t synapse;
    struct bcm_synapse_entry_s* next;
    bool active;
} bcm_synapse_entry_t;

/**
 * @brief Plasticity bridge internal structure
 */
struct ling_plasticity_bridge_s {
    uint32_t magic;

    /* Configuration */
    ling_plasticity_config_t config;

    /* Word synapse hash table */
    word_synapse_entry_t* word_synapses[WORD_SYNAPSE_HASH_SIZE];
    uint32_t word_synapse_count;

    /* Sequence synapse hash table */
    sequence_synapse_entry_t* sequence_synapses[SEQUENCE_SYNAPSE_HASH_SIZE];
    uint32_t sequence_synapse_count;

    /* BCM synapse hash table */
    bcm_synapse_entry_t* bcm_synapses[BCM_SYNAPSE_HASH_SIZE];
    uint32_t bcm_synapse_count;

    /* Global state */
    float global_bcm_threshold;
    float homeostatic_scaling_factor;
    float mean_firing_rate;
    uint64_t current_time_ms;

    /* Statistics */
    ling_plasticity_stats_t stats;

    /* Mesh integration */
    linguistics_mesh_t* mesh;

    /* Callbacks */
    ling_plasticity_callback_t callback;
    void* callback_data;
};

/* ============================================================================
 * DEFAULT CONFIGURATION
 * ============================================================================ */

ling_plasticity_config_t ling_plasticity_config_default(void) {
    ling_plasticity_config_t config;
    memset(&config, 0, sizeof(config));

    /* STDP configuration (Bi & Poo 1998) */
    config.stdp.pairwise_lr = LING_PLASTICITY_DEFAULT_LR;
    config.stdp.a_plus = 0.005f;
    config.stdp.a_minus = 0.00525f;
    config.stdp.tau_plus = NIMCP_STDP_TAU_PLUS_MS;
    config.stdp.tau_minus = NIMCP_STDP_TAU_MINUS_MS;
    config.stdp.enable_da_modulation = true;
    config.stdp.da_gain = 100.0f;
    config.stdp.burst_amplification = 3.0f;
    config.stdp.burst_threshold = LING_PLASTICITY_BURST_THRESHOLD;
    config.stdp.trace_decay = LING_PLASTICITY_DEFAULT_TRACE_DECAY;
    config.stdp.trace_lr_mult = 1.0f;

    /* Triplet STDP configuration (Pfister & Gerstner 2006) */
    config.triplet.A2_plus = TRIPLET_STDP_DEFAULT_A2_PLUS;
    config.triplet.A2_minus = TRIPLET_STDP_DEFAULT_A2_MINUS;
    config.triplet.A3_plus = TRIPLET_STDP_DEFAULT_A3_PLUS;
    config.triplet.A3_minus = TRIPLET_STDP_DEFAULT_A3_MINUS;
    config.triplet.tau_plus = TRIPLET_STDP_DEFAULT_TAU_PLUS;
    config.triplet.tau_minus = TRIPLET_STDP_DEFAULT_TAU_MINUS;
    config.triplet.tau_x = TRIPLET_STDP_DEFAULT_TAU_X;
    config.triplet.tau_y = TRIPLET_STDP_DEFAULT_TAU_Y;
    config.triplet.low_freq_threshold = 10.0f;
    config.triplet.high_freq_threshold = 40.0f;

    /* BCM configuration */
    config.bcm.learning_rate = NIMCP_LEARNING_RATE_DEFAULT;
    config.bcm.threshold_tau = 10000.0f;
    config.bcm.initial_threshold = LING_PLASTICITY_BCM_THRESHOLD;
    config.bcm.competition_strength = 0.8f;
    config.bcm.enable_winner_take_all = true;

    /* Structural plasticity configuration */
    config.structural.formation_threshold = 20.0f;
    config.structural.stabilization_threshold = 5.0f;
    config.structural.pruning_threshold = 0.5f;
    config.structural.pruning_timeout_ms = 86400000;  /* 1 day */
    config.structural.nascent_duration_ms = 3600000;  /* 1 hour */
    config.structural.maturation_time_ms = 604800000; /* 7 days */
    config.structural.stabilization_prob = 0.8f;
    config.structural.potentiation_prob = 0.6f;
    config.structural.recovery_prob = 0.3f;

    /* Homeostatic configuration */
    config.homeostatic.target_rate = 5.0f;
    config.homeostatic.scaling_tau = 3600000.0f;  /* 1 hour */
    config.homeostatic.scaling_exponent = 1.0f;
    config.homeostatic.ip_tau = 1000.0f;
    config.homeostatic.min_threshold = 0.1f;
    config.homeostatic.max_threshold = 0.9f;
    config.homeostatic.bcm_tau = 10000.0f;

    /* General settings */
    config.enable_mesh = true;
    config.max_vocabulary_size = LING_PLASTICITY_MAX_VOCABULARY;
    config.update_interval_ms = 10;

    /* Enable all mechanisms by default */
    config.enable_pairwise_stdp = true;
    config.enable_triplet_stdp = true;
    config.enable_r_stdp = true;
    config.enable_bcm = true;
    config.enable_structural = true;
    config.enable_homeostatic = true;

    return config;
}

/* ============================================================================
 * LIFECYCLE API
 * ============================================================================ */

ling_plasticity_bridge_t* ling_plasticity_create(const ling_plasticity_config_t* config) {
    ling_plasticity_bridge_t* bridge = nimcp_calloc(1, sizeof(ling_plasticity_bridge_t));
    if (!bridge) {
        set_error("Failed to allocate plasticity bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "ling_plasticity_create: bridge is NULL");
        return NULL;
    }

    bridge->magic = PLASTICITY_MAGIC;

    /* Use provided config or defaults */
    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = ling_plasticity_config_default();
    }

    /* Initialize global state */
    bridge->global_bcm_threshold = bridge->config.bcm.initial_threshold;
    bridge->homeostatic_scaling_factor = 1.0f;
    bridge->mean_firing_rate = bridge->config.homeostatic.target_rate;
    bridge->current_time_ms = 0;

    /* Initialize hash tables */
    memset(bridge->word_synapses, 0, sizeof(bridge->word_synapses));
    memset(bridge->sequence_synapses, 0, sizeof(bridge->sequence_synapses));
    memset(bridge->bcm_synapses, 0, sizeof(bridge->bcm_synapses));

    return bridge;
}

void ling_plasticity_destroy(ling_plasticity_bridge_t* bridge) {
    if (!bridge) return;
    if (bridge->magic != PLASTICITY_MAGIC) return;

    /* Free word synapses */
    for (uint32_t i = 0; i < WORD_SYNAPSE_HASH_SIZE; i++) {
        word_synapse_entry_t* entry = bridge->word_synapses[i];
        while (entry) {
            word_synapse_entry_t* next = entry->next;
            nimcp_free(entry);
            entry = NULL;
            entry = next;
        }
    }

    /* Free sequence synapses */
    for (uint32_t i = 0; i < SEQUENCE_SYNAPSE_HASH_SIZE; i++) {
        sequence_synapse_entry_t* entry = bridge->sequence_synapses[i];
        while (entry) {
            sequence_synapse_entry_t* next = entry->next;
            nimcp_free(entry);
            entry = NULL;
            entry = next;
        }
    }

    /* Free BCM synapses */
    for (uint32_t i = 0; i < BCM_SYNAPSE_HASH_SIZE; i++) {
        bcm_synapse_entry_t* entry = bridge->bcm_synapses[i];
        while (entry) {
            bcm_synapse_entry_t* next = entry->next;
            nimcp_free(entry);
            entry = NULL;
            entry = next;
        }
    }

    bridge->magic = 0;
    nimcp_free(bridge);
    bridge = NULL;
}

int ling_plasticity_reset(ling_plasticity_bridge_t* bridge) {
    if (!bridge || bridge->magic != PLASTICITY_MAGIC) {
        set_error("Invalid bridge");
        return LING_PLASTICITY_ERR_NULL;
    }

    /* Reset all word synapses to initial state */
    for (uint32_t i = 0; i < WORD_SYNAPSE_HASH_SIZE; i++) {
        word_synapse_entry_t* entry = bridge->word_synapses[i];
        while (entry) {
            if (entry->active) {
                stdp_synapse_init(&entry->synapse.stdp);
                entry->synapse.eligibility_trace = 0.0f;
                entry->synapse.spine_state = LING_SPINE_NASCENT;
                entry->synapse.last_activation = 0;
                entry->synapse.activation_count = 0;
                entry->synapse.dopamine_tagged = false;
            }
            entry = entry->next;
        }
    }

    /* Reset sequence synapses */
    for (uint32_t i = 0; i < SEQUENCE_SYNAPSE_HASH_SIZE; i++) {
        sequence_synapse_entry_t* entry = bridge->sequence_synapses[i];
        while (entry) {
            if (entry->active) {
                triplet_stdp_synapse_reset(&entry->synapse.triplet);
                entry->synapse.sequence_strength = 0.0f;
                entry->synapse.spine_state = LING_SPINE_NASCENT;
                entry->synapse.last_activation = 0;
            }
            entry = entry->next;
        }
    }

    /* Reset BCM synapses */
    for (uint32_t i = 0; i < BCM_SYNAPSE_HASH_SIZE; i++) {
        bcm_synapse_entry_t* entry = bridge->bcm_synapses[i];
        while (entry) {
            if (entry->active) {
                entry->synapse.bcm = bcm_synapse_init(0.5f, bridge->config.bcm.initial_threshold);
                entry->synapse.competition_score = 0.0f;
                entry->synapse.last_selection = 0;
            }
            entry = entry->next;
        }
    }

    /* Reset global state */
    bridge->global_bcm_threshold = bridge->config.bcm.initial_threshold;
    bridge->homeostatic_scaling_factor = 1.0f;
    bridge->mean_firing_rate = bridge->config.homeostatic.target_rate;

    /* Reset statistics */
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    return LING_PLASTICITY_OK;
}

/* ============================================================================
 * WORD SYNAPSE HELPERS
 * ============================================================================ */

static word_synapse_entry_t* find_word_synapse(
    ling_plasticity_bridge_t* bridge,
    uint32_t word_id,
    uint32_t meaning_id
) {
    uint32_t hash = hash_pair(word_id, meaning_id) % WORD_SYNAPSE_HASH_SIZE;
    word_synapse_entry_t* entry = bridge->word_synapses[hash];

    while (entry) {
        if (entry->active &&
            entry->synapse.word_id == word_id &&
            entry->synapse.meaning_id == meaning_id) {
            return entry;
        }
        entry = entry->next;
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_word_synapse: operation failed");
    return NULL;
}

static void emit_plasticity_event(
    ling_plasticity_bridge_t* bridge,
    ling_plasticity_event_type_t type,
    uint32_t word_id,
    uint32_t target_id,
    float weight_change,
    float new_weight,
    ling_spine_state_t spine_state
) {
    if (!bridge->callback) return;

    ling_plasticity_event_t event = {
        .type = type,
        .word_id = word_id,
        .target_id = target_id,
        .weight_change = weight_change,
        .new_weight = new_weight,
        .spine_state = spine_state,
        .timestamp_ms = bridge->current_time_ms
    };

    bridge->callback(&event, bridge->callback_data);
}

/* ============================================================================
 * MESH INTEGRATION
 * ============================================================================ */

static int plasticity_mesh_process(
    void* ctx,
    const linguistics_request_t* request,
    linguistics_belief_t* belief
) {
    ling_plasticity_bridge_t* bridge = (ling_plasticity_bridge_t*)ctx;
    if (!bridge || !request || !belief) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "plasticity_mesh_process: required parameter is NULL (bridge, request, belief)");
        return -1;
    }

    /* Generate belief based on plasticity state */
    belief->certainty = bridge->homeostatic_scaling_factor;
    belief->precision = 1.0f / (1.0f + bridge->stats.weight_variance);

    /* Encode plasticity state in belief vector */
    memset(belief->belief_vector, 0, sizeof(float) * LINGUISTICS_BELIEF_VEC_SIZE);
    belief->belief_vector[0] = bridge->global_bcm_threshold;
    belief->belief_vector[1] = bridge->homeostatic_scaling_factor;
    belief->belief_vector[2] = bridge->mean_firing_rate;
    belief->belief_vector[3] = (float)bridge->stats.stable_count /
                               (float)(bridge->word_synapse_count + 1);

    return 0;
}

static int plasticity_mesh_update(
    void* ctx,
    const linguistics_belief_t* neighbors,
    uint32_t neighbor_count,
    linguistics_belief_t* updated
) {
    ling_plasticity_bridge_t* bridge = (ling_plasticity_bridge_t*)ctx;
    if (!bridge || !updated) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "plasticity_mesh_update: required parameter is NULL (bridge, updated)");
        return -1;
    }

    /* FEP-style precision-weighted belief update */
    float lr = NIMCP_LEARNING_RATE_COARSE;  /* FEP_DEFAULT_BELIEF_LR */

    for (uint32_t i = 0; i < neighbor_count && i < 16; i++) {
        float error = neighbors[i].certainty - updated->certainty;
        float weight = neighbors[i].precision;
        updated->certainty += lr * weight * error;
    }

    /* Clamp certainty to valid range */
    if (updated->certainty < 0.0f) updated->certainty = 0.0f;
    if (updated->certainty > 1.0f) updated->certainty = 1.0f;

    return 0;
}

static float plasticity_mesh_get_precision(void* ctx) {
    ling_plasticity_bridge_t* bridge = (ling_plasticity_bridge_t*)ctx;
    if (!bridge) return 0.5f;

    /* Precision based on learning stability */
    float stability = 1.0f / (1.0f + bridge->stats.weight_variance);
    float consolidation = (float)bridge->stats.stable_count /
                          (float)(bridge->word_synapse_count + 1);

    return 0.5f * stability + 0.5f * consolidation;
}

int ling_plasticity_register_mesh(
    ling_plasticity_bridge_t* bridge,
    linguistics_mesh_t* mesh
) {
    if (!bridge || bridge->magic != PLASTICITY_MAGIC) {
        set_error("Invalid bridge");
        return LING_PLASTICITY_ERR_NULL;
    }

    bridge->mesh = mesh;

    if (mesh && bridge->config.enable_mesh) {
        linguistics_mesh_handler_t handler;
        handler.process = plasticity_mesh_process;
        handler.update = plasticity_mesh_update;
        handler.get_precision = plasticity_mesh_get_precision;
        handler.ctx = bridge;

        int ret = linguistics_mesh_register_participant(
            mesh, BIO_MODULE_LINGUISTICS_PLASTICITY_BRIDGE,
            "plasticity_bridge", handler);

        if (ret != 0) {
            set_error("Failed to register with mesh");
            return LING_PLASTICITY_ERR_MESH;
        }
    }

    return LING_PLASTICITY_OK;
}

int ling_plasticity_get_mesh_handler(
    ling_plasticity_bridge_t* bridge,
    linguistics_mesh_handler_t* handler
) {
    if (!bridge || !handler || bridge->magic != PLASTICITY_MAGIC) {
        set_error("Invalid arguments");
        return LING_PLASTICITY_ERR_NULL;
    }

    handler->process = plasticity_mesh_process;
    handler->update = plasticity_mesh_update;
    handler->get_precision = plasticity_mesh_get_precision;
    handler->ctx = bridge;

    return LING_PLASTICITY_OK;
}

/* ============================================================================
 * WORD SYNAPSE API (Pairwise/R-STDP)
 * ============================================================================ */

int ling_plasticity_create_word_synapse(
    ling_plasticity_bridge_t* bridge,
    uint32_t word_id,
    uint32_t meaning_id,
    float initial_weight
) {
    if (!bridge || bridge->magic != PLASTICITY_MAGIC) {
        set_error("Invalid bridge");
        return LING_PLASTICITY_ERR_NULL;
    }

    /* Check if synapse already exists */
    if (find_word_synapse(bridge, word_id, meaning_id)) {
        return LING_PLASTICITY_OK;  /* Already exists */
    }

    /* Check capacity */
    if (bridge->word_synapse_count >= bridge->config.max_vocabulary_size *
        LING_PLASTICITY_MAX_SYNAPSES_PER_WORD) {
        set_error("Word synapse capacity exceeded");
        return LING_PLASTICITY_ERR_CAPACITY;
    }

    /* Create new entry */
    word_synapse_entry_t* entry = nimcp_calloc(1, sizeof(word_synapse_entry_t));
    if (!entry) {
        set_error("Failed to allocate word synapse");
        return LING_PLASTICITY_ERR_NO_MEMORY;
    }

    /* Initialize synapse */
    entry->synapse.word_id = word_id;
    entry->synapse.meaning_id = meaning_id;
    stdp_synapse_init(&entry->synapse.stdp);
    entry->synapse.stdp.weight = initial_weight;
    entry->synapse.stdp.learning_rate = bridge->config.stdp.pairwise_lr;
    entry->synapse.stdp.a_plus = bridge->config.stdp.a_plus;
    entry->synapse.stdp.a_minus = bridge->config.stdp.a_minus;
    entry->synapse.stdp.tau_plus = bridge->config.stdp.tau_plus;
    entry->synapse.stdp.tau_minus = bridge->config.stdp.tau_minus;
    entry->synapse.stdp.enable_da_modulation = bridge->config.stdp.enable_da_modulation;
    entry->synapse.stdp.da_modulation_gain = bridge->config.stdp.da_gain;
    entry->synapse.stdp.burst_amplification = bridge->config.stdp.burst_amplification;
    entry->synapse.eligibility_trace = 0.0f;
    entry->synapse.spine_state = LING_SPINE_NASCENT;
    entry->synapse.last_activation = bridge->current_time_ms;
    entry->synapse.activation_count = 0;
    entry->synapse.dopamine_tagged = false;
    entry->active = true;

    /* Insert into hash table */
    uint32_t hash = hash_pair(word_id, meaning_id) % WORD_SYNAPSE_HASH_SIZE;
    entry->next = bridge->word_synapses[hash];
    bridge->word_synapses[hash] = entry;
    bridge->word_synapse_count++;

    /* Update statistics */
    bridge->stats.total_word_synapses = bridge->word_synapse_count;
    bridge->stats.nascent_count++;
    bridge->stats.total_formations++;

    /* Emit event */
    emit_plasticity_event(bridge, LING_PLASTICITY_EVENT_FORMATION,
                          word_id, meaning_id, 0.0f, initial_weight,
                          LING_SPINE_NASCENT);

    return LING_PLASTICITY_OK;
}

float ling_plasticity_word_pre_spike(
    ling_plasticity_bridge_t* bridge,
    uint32_t word_id,
    uint32_t meaning_id,
    float time_ms
) {
    if (!bridge || bridge->magic != PLASTICITY_MAGIC) return 0.0f;
    if (!bridge->config.enable_pairwise_stdp) return 0.0f;

    word_synapse_entry_t* entry = find_word_synapse(bridge, word_id, meaning_id);
    if (!entry) return 0.0f;

    /* Update eligibility trace */
    entry->synapse.eligibility_trace = 1.0f;

    /* Process STDP pre-spike */
    float dw = stdp_pre_spike(&entry->synapse.stdp, time_ms);

    /* Update activation tracking */
    entry->synapse.last_activation = (uint64_t)time_ms;
    entry->synapse.activation_count++;

    /* Update statistics */
    if (dw < 0.0f) {
        bridge->stats.total_ltd_events++;
        emit_plasticity_event(bridge, LING_PLASTICITY_EVENT_LTD,
                              word_id, meaning_id, dw,
                              entry->synapse.stdp.weight,
                              entry->synapse.spine_state);
    }

    return dw;
}

float ling_plasticity_word_post_spike(
    ling_plasticity_bridge_t* bridge,
    uint32_t word_id,
    uint32_t meaning_id,
    float time_ms
) {
    if (!bridge || bridge->magic != PLASTICITY_MAGIC) return 0.0f;
    if (!bridge->config.enable_pairwise_stdp) return 0.0f;

    word_synapse_entry_t* entry = find_word_synapse(bridge, word_id, meaning_id);
    if (!entry) return 0.0f;

    /* Update eligibility trace */
    entry->synapse.eligibility_trace = 1.0f;

    /* Process STDP post-spike */
    float dw = stdp_post_spike(&entry->synapse.stdp, time_ms);

    /* Update activation tracking */
    entry->synapse.last_activation = (uint64_t)time_ms;
    entry->synapse.activation_count++;

    /* Update statistics */
    if (dw > 0.0f) {
        bridge->stats.total_ltp_events++;
        emit_plasticity_event(bridge, LING_PLASTICITY_EVENT_LTP,
                              word_id, meaning_id, dw,
                              entry->synapse.stdp.weight,
                              entry->synapse.spine_state);
    }

    return dw;
}

float ling_plasticity_word_reward(
    ling_plasticity_bridge_t* bridge,
    uint32_t word_id,
    uint32_t meaning_id,
    float reward,
    float dopamine_level
) {
    if (!bridge || bridge->magic != PLASTICITY_MAGIC) return 0.0f;
    if (!bridge->config.enable_r_stdp) return 0.0f;

    word_synapse_entry_t* entry = find_word_synapse(bridge, word_id, meaning_id);
    if (!entry) return 0.0f;

    /* R-STDP: Weight change = eligibility × reward × dopamine */
    float eligibility = entry->synapse.eligibility_trace;
    float da_modulation = 1.0f + bridge->config.stdp.da_gain * dopamine_level;

    /* Check for burst amplification */
    if (dopamine_level > bridge->config.stdp.burst_threshold) {
        da_modulation *= bridge->config.stdp.burst_amplification;
        entry->synapse.dopamine_tagged = true;
        bridge->stats.total_da_burst_events++;
    }

    /* Compute weight change */
    float dw = eligibility * reward * da_modulation *
               bridge->config.stdp.pairwise_lr *
               bridge->config.stdp.trace_lr_mult;

    /* Apply weight change with bounds */
    float new_weight = entry->synapse.stdp.weight + dw;
    if (new_weight < entry->synapse.stdp.w_min) {
        new_weight = entry->synapse.stdp.w_min;
    }
    if (new_weight > entry->synapse.stdp.w_max) {
        new_weight = entry->synapse.stdp.w_max;
    }

    float actual_dw = new_weight - entry->synapse.stdp.weight;
    entry->synapse.stdp.weight = new_weight;

    /* Update statistics */
    if (actual_dw > 0.0f) {
        bridge->stats.total_ltp_events++;
        emit_plasticity_event(bridge, LING_PLASTICITY_EVENT_LTP,
                              word_id, meaning_id, actual_dw, new_weight,
                              entry->synapse.spine_state);
    } else if (actual_dw < 0.0f) {
        bridge->stats.total_ltd_events++;
        emit_plasticity_event(bridge, LING_PLASTICITY_EVENT_LTD,
                              word_id, meaning_id, actual_dw, new_weight,
                              entry->synapse.spine_state);
    }

    return actual_dw;
}

int ling_plasticity_get_word_weight(
    const ling_plasticity_bridge_t* bridge,
    uint32_t word_id,
    uint32_t meaning_id,
    float* weight
) {
    if (!bridge || !weight || bridge->magic != PLASTICITY_MAGIC) {
        return LING_PLASTICITY_ERR_NULL;
    }

    word_synapse_entry_t* entry = find_word_synapse(
        (ling_plasticity_bridge_t*)bridge, word_id, meaning_id);

    if (!entry) {
        set_error("Word synapse not found");
        return LING_PLASTICITY_ERR_NOT_FOUND;
    }

    *weight = entry->synapse.stdp.weight;
    return LING_PLASTICITY_OK;
}

/* ============================================================================
 * SEQUENCE SYNAPSE API (Triplet STDP)
 * ============================================================================ */

static sequence_synapse_entry_t* find_sequence_synapse(
    ling_plasticity_bridge_t* bridge,
    uint32_t word_a,
    uint32_t word_b
) {
    uint32_t hash = hash_pair(word_a, word_b) % SEQUENCE_SYNAPSE_HASH_SIZE;
    sequence_synapse_entry_t* entry = bridge->sequence_synapses[hash];

    while (entry) {
        if (entry->active &&
            entry->synapse.word_a_id == word_a &&
            entry->synapse.word_b_id == word_b) {
            return entry;
        }
        entry = entry->next;
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_sequence_synapse: operation failed");
    return NULL;
}

int ling_plasticity_create_sequence_synapse(
    ling_plasticity_bridge_t* bridge,
    uint32_t word_a,
    uint32_t word_b,
    float initial_weight
) {
    if (!bridge || bridge->magic != PLASTICITY_MAGIC) {
        set_error("Invalid bridge");
        return LING_PLASTICITY_ERR_NULL;
    }

    /* Check if synapse already exists */
    if (find_sequence_synapse(bridge, word_a, word_b)) {
        return LING_PLASTICITY_OK;
    }

    /* Create new entry */
    sequence_synapse_entry_t* entry = nimcp_calloc(1, sizeof(sequence_synapse_entry_t));
    if (!entry) {
        set_error("Failed to allocate sequence synapse");
        return LING_PLASTICITY_ERR_NO_MEMORY;
    }

    /* Initialize synapse */
    entry->synapse.word_a_id = word_a;
    entry->synapse.word_b_id = word_b;

    /* Create triplet STDP synapse */
    triplet_stdp_config_t triplet_config = {
        .A2_plus = bridge->config.triplet.A2_plus,
        .A2_minus = bridge->config.triplet.A2_minus,
        .A3_plus = bridge->config.triplet.A3_plus,
        .A3_minus = bridge->config.triplet.A3_minus,
        .tau_plus = bridge->config.triplet.tau_plus,
        .tau_minus = bridge->config.triplet.tau_minus,
        .tau_x = bridge->config.triplet.tau_x,
        .tau_y = bridge->config.triplet.tau_y,
        .w_max = 1.0f,
        .w_min = 0.0f
    };

    triplet_stdp_synapse_t* triplet = triplet_stdp_synapse_create(&triplet_config, initial_weight);
    if (!triplet) {
        nimcp_free(entry);
        entry = NULL;
        set_error("Failed to create triplet STDP synapse");
        return LING_PLASTICITY_ERR_STDP;
    }

    entry->synapse.triplet = *triplet;
    triplet_stdp_synapse_destroy(triplet);

    entry->synapse.sequence_strength = initial_weight;
    entry->synapse.spine_state = LING_SPINE_NASCENT;
    entry->synapse.last_activation = bridge->current_time_ms;
    entry->active = true;

    /* Insert into hash table */
    uint32_t hash = hash_pair(word_a, word_b) % SEQUENCE_SYNAPSE_HASH_SIZE;
    entry->next = bridge->sequence_synapses[hash];
    bridge->sequence_synapses[hash] = entry;
    bridge->sequence_synapse_count++;

    /* Update statistics */
    bridge->stats.total_sequence_synapses = bridge->sequence_synapse_count;

    return LING_PLASTICITY_OK;
}

float ling_plasticity_sequence_spike(
    ling_plasticity_bridge_t* bridge,
    uint32_t word_a,
    uint32_t word_b,
    float time_ms,
    bool is_post
) {
    if (!bridge || bridge->magic != PLASTICITY_MAGIC) return 0.0f;
    if (!bridge->config.enable_triplet_stdp) return 0.0f;

    sequence_synapse_entry_t* entry = find_sequence_synapse(bridge, word_a, word_b);
    if (!entry) return 0.0f;

    float dw = 0.0f;
    if (is_post) {
        dw = triplet_stdp_post_spike(&entry->synapse.triplet, time_ms);
    } else {
        dw = triplet_stdp_pre_spike(&entry->synapse.triplet, time_ms);
    }

    /* Update sequence strength */
    entry->synapse.sequence_strength += dw;
    if (entry->synapse.sequence_strength < 0.0f) {
        entry->synapse.sequence_strength = 0.0f;
    }
    if (entry->synapse.sequence_strength > 1.0f) {
        entry->synapse.sequence_strength = 1.0f;
    }

    entry->synapse.last_activation = (uint64_t)time_ms;

    return dw;
}

float ling_plasticity_learn_sequence(
    ling_plasticity_bridge_t* bridge,
    const uint32_t* word_ids,
    uint32_t num_words,
    float frequency
) {
    if (!bridge || !word_ids || num_words < 2) return 0.0f;
    if (!bridge->config.enable_triplet_stdp) return 0.0f;

    float total_dw = 0.0f;
    float inter_spike_interval = 1000.0f / frequency;  /* ms */

    /* Create/reinforce sequence synapses for consecutive pairs */
    for (uint32_t i = 0; i < num_words - 1; i++) {
        /* Ensure synapse exists */
        int ret = ling_plasticity_create_sequence_synapse(
            bridge, word_ids[i], word_ids[i + 1], 0.1f);
        if (ret != LING_PLASTICITY_OK) continue;

        /* Simulate spike timing at the given frequency */
        float time_a = (float)i * inter_spike_interval;
        float time_b = (float)(i + 1) * inter_spike_interval;

        /* Pre-spike from word_a */
        total_dw += ling_plasticity_sequence_spike(
            bridge, word_ids[i], word_ids[i + 1], time_a, false);

        /* Post-spike from word_b (triplet effect kicks in at high freq) */
        total_dw += ling_plasticity_sequence_spike(
            bridge, word_ids[i], word_ids[i + 1], time_b, true);
    }

    return total_dw;
}

/* ============================================================================
 * BCM COMPETITIVE LEARNING API
 * ============================================================================ */

static bcm_synapse_entry_t* find_bcm_synapse(
    ling_plasticity_bridge_t* bridge,
    uint32_t word_id
) {
    uint32_t hash = word_id % BCM_SYNAPSE_HASH_SIZE;
    bcm_synapse_entry_t* entry = bridge->bcm_synapses[hash];

    while (entry) {
        if (entry->active && entry->synapse.word_id == word_id) {
            return entry;
        }
        entry = entry->next;
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_bcm_synapse: validation failed");
    return NULL;
}

int ling_plasticity_create_bcm_synapse(
    ling_plasticity_bridge_t* bridge,
    uint32_t word_id,
    float initial_weight
) {
    if (!bridge || bridge->magic != PLASTICITY_MAGIC) {
        set_error("Invalid bridge");
        return LING_PLASTICITY_ERR_NULL;
    }

    /* Check if synapse already exists */
    if (find_bcm_synapse(bridge, word_id)) {
        return LING_PLASTICITY_OK;
    }

    /* Create new entry */
    bcm_synapse_entry_t* entry = nimcp_calloc(1, sizeof(bcm_synapse_entry_t));
    if (!entry) {
        set_error("Failed to allocate BCM synapse");
        return LING_PLASTICITY_ERR_NO_MEMORY;
    }

    /* Initialize synapse */
    entry->synapse.word_id = word_id;
    entry->synapse.bcm = bcm_synapse_init(initial_weight, bridge->config.bcm.initial_threshold);
    entry->synapse.bcm.threshold = bridge->config.bcm.initial_threshold;
    entry->synapse.competition_score = 0.0f;
    entry->synapse.last_selection = 0;
    entry->active = true;

    /* Insert into hash table */
    uint32_t hash = word_id % BCM_SYNAPSE_HASH_SIZE;
    entry->next = bridge->bcm_synapses[hash];
    bridge->bcm_synapses[hash] = entry;
    bridge->bcm_synapse_count++;

    /* Update statistics */
    bridge->stats.total_bcm_synapses = bridge->bcm_synapse_count;

    return LING_PLASTICITY_OK;
}

float ling_plasticity_bcm_update(
    ling_plasticity_bridge_t* bridge,
    uint32_t word_id,
    float pre_activity,
    float post_activity
) {
    if (!bridge || bridge->magic != PLASTICITY_MAGIC) return 0.0f;
    if (!bridge->config.enable_bcm) return 0.0f;

    bcm_synapse_entry_t* entry = find_bcm_synapse(bridge, word_id);
    if (!entry) return 0.0f;

    /* BCM rule: dw = lr × post × (post - θ) × pre */
    float theta = entry->synapse.bcm.threshold;
    float dw = bridge->config.bcm.learning_rate *
               post_activity * (post_activity - theta) * pre_activity;

    /* Apply weight change with bounds */
    float new_weight = entry->synapse.bcm.weight + dw;
    if (new_weight < 0.0f) new_weight = 0.0f;
    if (new_weight > 1.0f) new_weight = 1.0f;

    entry->synapse.bcm.weight = new_weight;

    /* Update threshold sliding: θ' = θ + (post² - θ) / τ */
    float dt = (float)bridge->config.update_interval_ms;
    float dtheta = (post_activity * post_activity - theta) *
                   dt / bridge->config.bcm.threshold_tau;
    entry->synapse.bcm.threshold += dtheta;

    /* Update competition score */
    entry->synapse.competition_score = new_weight * post_activity;

    return dw;
}

int ling_plasticity_bcm_compete(
    ling_plasticity_bridge_t* bridge,
    const uint32_t* candidate_ids,
    uint32_t num_candidates,
    uint32_t* winner_id
) {
    if (!bridge || !candidate_ids || !winner_id || num_candidates == 0) {
        return LING_PLASTICITY_ERR_NULL;
    }
    if (!bridge->config.enable_bcm) {
        *winner_id = candidate_ids[0];
        return LING_PLASTICITY_OK;
    }

    float max_score = -1e30f;
    uint32_t winner = candidate_ids[0];

    for (uint32_t i = 0; i < num_candidates; i++) {
        bcm_synapse_entry_t* entry = find_bcm_synapse(bridge, candidate_ids[i]);
        if (!entry) continue;

        float score = entry->synapse.competition_score;

        /* Apply lateral inhibition from other candidates */
        if (bridge->config.bcm.enable_winner_take_all) {
            for (uint32_t j = 0; j < num_candidates; j++) {
                if (i == j) continue;
                bcm_synapse_entry_t* other = find_bcm_synapse(bridge, candidate_ids[j]);
                if (other) {
                    score -= bridge->config.bcm.competition_strength *
                             other->synapse.competition_score;
                }
            }
        }

        if (score > max_score) {
            max_score = score;
            winner = candidate_ids[i];
        }
    }

    *winner_id = winner;
    bridge->stats.total_bcm_competitions++;

    /* Emit events for winner/loser */
    for (uint32_t i = 0; i < num_candidates; i++) {
        bcm_synapse_entry_t* entry = find_bcm_synapse(bridge, candidate_ids[i]);
        if (!entry) continue;

        if (candidate_ids[i] == winner) {
            entry->synapse.last_selection = bridge->current_time_ms;
            emit_plasticity_event(bridge, LING_PLASTICITY_EVENT_BCM_WIN,
                                  candidate_ids[i], 0, 0.0f,
                                  entry->synapse.bcm.weight, LING_SPINE_STABLE);
        } else {
            emit_plasticity_event(bridge, LING_PLASTICITY_EVENT_BCM_LOSE,
                                  candidate_ids[i], 0, 0.0f,
                                  entry->synapse.bcm.weight, LING_SPINE_STABLE);
        }
    }

    return LING_PLASTICITY_OK;
}

/* ============================================================================
 * STRUCTURAL PLASTICITY API
 * ============================================================================ */

int ling_plasticity_get_spine_state(
    const ling_plasticity_bridge_t* bridge,
    uint32_t word_id,
    uint32_t meaning_id,
    ling_spine_state_t* state
) {
    if (!bridge || !state || bridge->magic != PLASTICITY_MAGIC) {
        return LING_PLASTICITY_ERR_NULL;
    }

    word_synapse_entry_t* entry = find_word_synapse(
        (ling_plasticity_bridge_t*)bridge, word_id, meaning_id);

    if (!entry) {
        set_error("Word synapse not found");
        return LING_PLASTICITY_ERR_NOT_FOUND;
    }

    *state = entry->synapse.spine_state;
    return LING_PLASTICITY_OK;
}

uint32_t ling_plasticity_structural_update(
    ling_plasticity_bridge_t* bridge,
    float dt_ms
) {
    if (!bridge || bridge->magic != PLASTICITY_MAGIC) return 0;
    if (!bridge->config.enable_structural) return 0;

    uint32_t transitions = 0;
    bridge->current_time_ms += (uint64_t)dt_ms;

    /* Reset spine state counts */
    bridge->stats.nascent_count = 0;
    bridge->stats.stable_count = 0;
    bridge->stats.potentiated_count = 0;
    bridge->stats.pruning_count = 0;

    /* Process all word synapses */
    for (uint32_t i = 0; i < WORD_SYNAPSE_HASH_SIZE; i++) {
        word_synapse_entry_t* entry = bridge->word_synapses[i];
        while (entry) {
            if (!entry->active) {
                entry = entry->next;
                continue;
            }

            uint64_t age = bridge->current_time_ms - entry->synapse.last_activation;
            ling_spine_state_t old_state = entry->synapse.spine_state;
            ling_spine_state_t new_state = old_state;

            switch (old_state) {
                case LING_SPINE_NASCENT:
                    /* Check for stabilization */
                    if (entry->synapse.activation_count >=
                        (uint32_t)bridge->config.structural.stabilization_threshold) {
                        float r = (float)nimcp_tl_rand() / (float)RAND_MAX;
                        if (r < bridge->config.structural.stabilization_prob) {
                            new_state = LING_SPINE_STABLE;
                        }
                    }
                    /* Check for timeout */
                    else if (age > bridge->config.structural.nascent_duration_ms) {
                        new_state = LING_SPINE_ELIMINATED;
                    }
                    bridge->stats.nascent_count++;
                    break;

                case LING_SPINE_STABLE:
                    /* Check for potentiation */
                    if (entry->synapse.dopamine_tagged ||
                        entry->synapse.stdp.weight > 0.8f) {
                        float r = (float)nimcp_tl_rand() / (float)RAND_MAX;
                        if (r < bridge->config.structural.potentiation_prob) {
                            new_state = LING_SPINE_POTENTIATED;
                        }
                    }
                    /* Check for pruning due to inactivity */
                    else if (age > bridge->config.structural.pruning_timeout_ms) {
                        new_state = LING_SPINE_PRUNING;
                    }
                    bridge->stats.stable_count++;
                    break;

                case LING_SPINE_POTENTIATED:
                    /* Potentiated spines are very stable */
                    /* Decay back to stable if not recently activated */
                    if (age > bridge->config.structural.maturation_time_ms) {
                        new_state = LING_SPINE_STABLE;
                    }
                    bridge->stats.potentiated_count++;
                    break;

                case LING_SPINE_PRUNING:
                    /* Check for recovery */
                    if (entry->synapse.activation_count > 0 &&
                        age < bridge->config.structural.nascent_duration_ms) {
                        float r = (float)nimcp_tl_rand() / (float)RAND_MAX;
                        if (r < bridge->config.structural.recovery_prob) {
                            new_state = LING_SPINE_STABLE;
                        }
                    }
                    /* Check for elimination */
                    else if (age > bridge->config.structural.pruning_timeout_ms * 2) {
                        new_state = LING_SPINE_ELIMINATED;
                    }
                    bridge->stats.pruning_count++;
                    break;

                case LING_SPINE_ELIMINATED:
                    /* Already eliminated, nothing to do */
                    entry->active = false;
                    bridge->word_synapse_count--;
                    bridge->stats.total_eliminations++;
                    break;
            }

            /* Apply state transition */
            if (new_state != old_state) {
                entry->synapse.spine_state = new_state;
                transitions++;

                ling_plasticity_event_type_t event_type;
                switch (new_state) {
                    case LING_SPINE_STABLE:
                        event_type = LING_PLASTICITY_EVENT_STABILIZATION;
                        break;
                    case LING_SPINE_POTENTIATED:
                        event_type = LING_PLASTICITY_EVENT_POTENTIATION;
                        break;
                    case LING_SPINE_PRUNING:
                        event_type = LING_PLASTICITY_EVENT_PRUNING;
                        break;
                    case LING_SPINE_ELIMINATED:
                        event_type = LING_PLASTICITY_EVENT_ELIMINATION;
                        break;
                    default:
                        event_type = LING_PLASTICITY_EVENT_STABILIZATION;
                }

                emit_plasticity_event(bridge, event_type,
                                      entry->synapse.word_id,
                                      entry->synapse.meaning_id,
                                      0.0f, entry->synapse.stdp.weight,
                                      new_state);
            }

            entry = entry->next;
        }
    }

    return transitions;
}

int ling_plasticity_tag_for_consolidation(
    ling_plasticity_bridge_t* bridge,
    uint32_t word_id,
    uint32_t meaning_id
) {
    if (!bridge || bridge->magic != PLASTICITY_MAGIC) {
        return LING_PLASTICITY_ERR_NULL;
    }

    word_synapse_entry_t* entry = find_word_synapse(bridge, word_id, meaning_id);
    if (!entry) {
        set_error("Word synapse not found");
        return LING_PLASTICITY_ERR_NOT_FOUND;
    }

    entry->synapse.dopamine_tagged = true;
    return LING_PLASTICITY_OK;
}

uint32_t ling_plasticity_sleep_consolidation(
    ling_plasticity_bridge_t* bridge,
    sleep_state_t sleep_state,
    float duration_ms
) {
    if (!bridge || bridge->magic != PLASTICITY_MAGIC) return 0;
    if (!bridge->config.enable_structural) return 0;

    uint32_t consolidated = 0;

    /* NREM sleep: strengthen tagged spines */
    /* REM sleep: prune weak spines */
    bool is_nrem = (sleep_state == SLEEP_STATE_LIGHT_NREM ||
                    sleep_state == SLEEP_STATE_DEEP_NREM);
    bool is_rem = (sleep_state == SLEEP_STATE_REM);

    for (uint32_t i = 0; i < WORD_SYNAPSE_HASH_SIZE; i++) {
        word_synapse_entry_t* entry = bridge->word_synapses[i];
        while (entry) {
            if (!entry->active) {
                entry = entry->next;
                continue;
            }

            if (is_nrem && entry->synapse.dopamine_tagged) {
                /* Consolidate tagged spines */
                if (entry->synapse.spine_state == LING_SPINE_NASCENT) {
                    entry->synapse.spine_state = LING_SPINE_STABLE;
                    consolidated++;
                } else if (entry->synapse.spine_state == LING_SPINE_STABLE) {
                    entry->synapse.spine_state = LING_SPINE_POTENTIATED;
                    consolidated++;
                }
                entry->synapse.dopamine_tagged = false;
            } else if (is_rem) {
                /* Prune weak untagged spines */
                if (!entry->synapse.dopamine_tagged &&
                    entry->synapse.stdp.weight < 0.2f &&
                    entry->synapse.spine_state != LING_SPINE_POTENTIATED) {
                    entry->synapse.spine_state = LING_SPINE_PRUNING;
                    consolidated++;
                }
            }

            entry = entry->next;
        }
    }

    return consolidated;
}

/* ============================================================================
 * HOMEOSTATIC API
 * ============================================================================ */

float ling_plasticity_apply_scaling(
    ling_plasticity_bridge_t* bridge,
    float target_rate
) {
    if (!bridge || bridge->magic != PLASTICITY_MAGIC) return 1.0f;
    if (!bridge->config.enable_homeostatic) return 1.0f;

    /* Compute mean weight */
    float sum_weight = 0.0f;
    uint32_t count = 0;

    for (uint32_t i = 0; i < WORD_SYNAPSE_HASH_SIZE; i++) {
        word_synapse_entry_t* entry = bridge->word_synapses[i];
        while (entry) {
            if (entry->active) {
                sum_weight += entry->synapse.stdp.weight;
                count++;
            }
            entry = entry->next;
        }
    }

    if (count == 0) return 1.0f;

    float mean_weight = sum_weight / (float)count;

    /* Compute scaling factor: (target / actual)^α */
    float actual_rate = mean_weight * 10.0f;  /* Approximate rate from weight */
    if (actual_rate < 0.01f) actual_rate = 0.01f;

    float scaling = powf(target_rate / actual_rate,
                         bridge->config.homeostatic.scaling_exponent);

    /* Clamp scaling factor */
    if (scaling < 0.1f) scaling = 0.1f;
    if (scaling > 10.0f) scaling = 10.0f;

    /* Apply scaling to all weights */
    for (uint32_t i = 0; i < WORD_SYNAPSE_HASH_SIZE; i++) {
        word_synapse_entry_t* entry = bridge->word_synapses[i];
        while (entry) {
            if (entry->active) {
                float new_weight = entry->synapse.stdp.weight * scaling;
                if (new_weight < 0.0f) new_weight = 0.0f;
                if (new_weight > 1.0f) new_weight = 1.0f;
                entry->synapse.stdp.weight = new_weight;
            }
            entry = entry->next;
        }
    }

    bridge->homeostatic_scaling_factor = scaling;

    emit_plasticity_event(bridge, LING_PLASTICITY_EVENT_SCALING,
                          0, 0, scaling - 1.0f, mean_weight * scaling,
                          LING_SPINE_STABLE);

    return scaling;
}

void ling_plasticity_intrinsic_update(
    ling_plasticity_bridge_t* bridge,
    float dt_ms
) {
    if (!bridge || bridge->magic != PLASTICITY_MAGIC) return;
    if (!bridge->config.enable_homeostatic) return;

    /* This would update neuronal thresholds based on activity */
    /* For now, update BCM thresholds for all synapses */
    float alpha = dt_ms / bridge->config.homeostatic.ip_tau;

    for (uint32_t i = 0; i < BCM_SYNAPSE_HASH_SIZE; i++) {
        bcm_synapse_entry_t* entry = bridge->bcm_synapses[i];
        while (entry) {
            if (entry->active) {
                float target = bridge->config.homeostatic.target_rate;
                float actual = entry->synapse.competition_score * 10.0f;
                float dtheta = alpha * (actual - target);

                entry->synapse.bcm.threshold += dtheta;

                /* Clamp threshold */
                if (entry->synapse.bcm.threshold <
                    bridge->config.homeostatic.min_threshold) {
                    entry->synapse.bcm.threshold =
                        bridge->config.homeostatic.min_threshold;
                }
                if (entry->synapse.bcm.threshold >
                    bridge->config.homeostatic.max_threshold) {
                    entry->synapse.bcm.threshold =
                        bridge->config.homeostatic.max_threshold;
                }
            }
            entry = entry->next;
        }
    }
}

void ling_plasticity_bcm_threshold_update(
    ling_plasticity_bridge_t* bridge,
    float mean_activity
) {
    if (!bridge || bridge->magic != PLASTICITY_MAGIC) return;
    if (!bridge->config.enable_bcm) return;

    /* BCM sliding threshold: θ = <r²> */
    float alpha = (float)bridge->config.update_interval_ms /
                  bridge->config.homeostatic.bcm_tau;

    bridge->global_bcm_threshold +=
        alpha * (mean_activity * mean_activity - bridge->global_bcm_threshold);

    bridge->stats.bcm_threshold = bridge->global_bcm_threshold;
}

/* ============================================================================
 * ELIGIBILITY TRACE API
 * ============================================================================ */

int ling_plasticity_create_eligibility_trace(
    ling_plasticity_bridge_t* bridge,
    uint32_t word_id,
    uint32_t meaning_id
) {
    if (!bridge || bridge->magic != PLASTICITY_MAGIC) {
        return LING_PLASTICITY_ERR_NULL;
    }

    word_synapse_entry_t* entry = find_word_synapse(bridge, word_id, meaning_id);
    if (!entry) {
        /* Create the synapse first */
        int ret = ling_plasticity_create_word_synapse(bridge, word_id, meaning_id, 0.1f);
        if (ret != LING_PLASTICITY_OK) return ret;
        entry = find_word_synapse(bridge, word_id, meaning_id);
        if (!entry) return LING_PLASTICITY_ERR_NOT_FOUND;
    }

    entry->synapse.eligibility_trace = 1.0f;
    return LING_PLASTICITY_OK;
}

void ling_plasticity_decay_traces(
    ling_plasticity_bridge_t* bridge,
    float dt_ms
) {
    if (!bridge || bridge->magic != PLASTICITY_MAGIC) return;

    float decay = expf(-dt_ms / (1000.0f * (1.0f - bridge->config.stdp.trace_decay)));

    for (uint32_t i = 0; i < WORD_SYNAPSE_HASH_SIZE; i++) {
        word_synapse_entry_t* entry = bridge->word_synapses[i];
        while (entry) {
            if (entry->active) {
                entry->synapse.eligibility_trace *= decay;
                if (entry->synapse.eligibility_trace < 0.001f) {
                    entry->synapse.eligibility_trace = 0.0f;
                }
            }
            entry = entry->next;
        }
    }
}

float ling_plasticity_apply_reward_to_traces(
    ling_plasticity_bridge_t* bridge,
    float reward,
    float dopamine_level
) {
    if (!bridge || bridge->magic != PLASTICITY_MAGIC) return 0.0f;
    if (!bridge->config.enable_r_stdp) return 0.0f;

    float total_dw = 0.0f;

    for (uint32_t i = 0; i < WORD_SYNAPSE_HASH_SIZE; i++) {
        word_synapse_entry_t* entry = bridge->word_synapses[i];
        while (entry) {
            if (entry->active && entry->synapse.eligibility_trace > 0.01f) {
                float dw = ling_plasticity_word_reward(
                    bridge,
                    entry->synapse.word_id,
                    entry->synapse.meaning_id,
                    reward,
                    dopamine_level
                );
                total_dw += dw;
            }
            entry = entry->next;
        }
    }

    return total_dw;
}

/* ============================================================================
 * BATCH OPERATIONS
 * ============================================================================ */

void ling_plasticity_update_traces(
    ling_plasticity_bridge_t* bridge,
    float dt_ms
) {
    if (!bridge || bridge->magic != PLASTICITY_MAGIC) return;

    /* Convert dt from ms to seconds for STDP trace update */
    float dt_s = dt_ms / 1000.0f;

    /* Update STDP traces for all word synapses */
    for (uint32_t i = 0; i < WORD_SYNAPSE_HASH_SIZE; i++) {
        word_synapse_entry_t* entry = bridge->word_synapses[i];
        while (entry) {
            if (entry->active) {
                stdp_update_traces(&entry->synapse.stdp, dt_s);
            }
            entry = entry->next;
        }
    }

    /* Update triplet traces for sequence synapses */
    for (uint32_t i = 0; i < SEQUENCE_SYNAPSE_HASH_SIZE; i++) {
        sequence_synapse_entry_t* entry = bridge->sequence_synapses[i];
        while (entry) {
            if (entry->active) {
                triplet_stdp_update_traces(&entry->synapse.triplet, dt_ms);
            }
            entry = entry->next;
        }
    }

    /* Decay eligibility traces */
    ling_plasticity_decay_traces(bridge, dt_ms);
}

int ling_plasticity_full_update(
    ling_plasticity_bridge_t* bridge,
    float dt_ms
) {
    if (!bridge || bridge->magic != PLASTICITY_MAGIC) {
        return LING_PLASTICITY_ERR_NULL;
    }

    /* Update all traces */
    ling_plasticity_update_traces(bridge, dt_ms);

    /* Structural plasticity update */
    ling_plasticity_structural_update(bridge, dt_ms);

    /* Homeostatic updates (less frequently) */
    static uint64_t last_homeostatic_update = 0;
    if (bridge->current_time_ms - last_homeostatic_update > 1000) {
        ling_plasticity_intrinsic_update(bridge, 1000.0f);
        last_homeostatic_update = bridge->current_time_ms;
    }

    /* Update statistics */
    bridge->stats.current_effective_lr =
        bridge->config.stdp.pairwise_lr * bridge->homeostatic_scaling_factor;
    bridge->stats.homeostatic_scaling_factor = bridge->homeostatic_scaling_factor;
    bridge->stats.total_word_synapses = bridge->word_synapse_count;

    return LING_PLASTICITY_OK;
}

/* ============================================================================
 * STATISTICS & MONITORING
 * ============================================================================ */

int ling_plasticity_get_stats(
    const ling_plasticity_bridge_t* bridge,
    ling_plasticity_stats_t* stats
) {
    if (!bridge || !stats || bridge->magic != PLASTICITY_MAGIC) {
        return LING_PLASTICITY_ERR_NULL;
    }

    /* Copy current statistics */
    *stats = bridge->stats;

    /* Compute additional statistics */
    float sum_weight = 0.0f;
    float sum_sq_weight = 0.0f;
    float max_w = 0.0f;
    float min_w = 1.0f;
    uint32_t count = 0;

    for (uint32_t i = 0; i < WORD_SYNAPSE_HASH_SIZE; i++) {
        word_synapse_entry_t* entry = bridge->word_synapses[i];
        while (entry) {
            if (entry->active) {
                float w = entry->synapse.stdp.weight;
                sum_weight += w;
                sum_sq_weight += w * w;
                if (w > max_w) max_w = w;
                if (w < min_w) min_w = w;
                count++;
            }
            entry = entry->next;
        }
    }

    if (count > 0) {
        stats->mean_weight = sum_weight / (float)count;
        stats->weight_variance = sum_sq_weight / (float)count -
                                 stats->mean_weight * stats->mean_weight;
        stats->max_weight = max_w;
        stats->min_weight = min_w;
    }

    stats->mesh_precision = plasticity_mesh_get_precision(
        (void*)bridge);

    return LING_PLASTICITY_OK;
}

int ling_plasticity_reset_stats(ling_plasticity_bridge_t* bridge) {
    if (!bridge || bridge->magic != PLASTICITY_MAGIC) {
        return LING_PLASTICITY_ERR_NULL;
    }

    /* Keep spine counts, reset event counts */
    bridge->stats.total_ltp_events = 0;
    bridge->stats.total_ltd_events = 0;
    bridge->stats.total_da_burst_events = 0;
    bridge->stats.total_formations = 0;
    bridge->stats.total_eliminations = 0;
    bridge->stats.total_bcm_competitions = 0;
    bridge->stats.mesh_updates = 0;

    return LING_PLASTICITY_OK;
}

int ling_plasticity_register_callback(
    ling_plasticity_bridge_t* bridge,
    ling_plasticity_callback_t callback,
    void* user_data
) {
    if (!bridge || bridge->magic != PLASTICITY_MAGIC) {
        return LING_PLASTICITY_ERR_NULL;
    }

    bridge->callback = callback;
    bridge->callback_data = user_data;

    return LING_PLASTICITY_OK;
}

/* ============================================================================
 * UTILITY FUNCTIONS
 * ============================================================================ */

const char* ling_plasticity_get_last_error(void) {
    return g_last_error;
}

const char* ling_plasticity_spine_state_name(ling_spine_state_t state) {
    static const char* names[] = {
        "NASCENT",
        "STABLE",
        "POTENTIATED",
        "PRUNING",
        "ELIMINATED"
    };

    if (state >= sizeof(names) / sizeof(names[0])) {
        return "UNKNOWN";
    }
    return names[state];
}

const char* ling_plasticity_rule_name(ling_plasticity_rule_t rule) {
    static const char* names[] = {
        "PAIRWISE_STDP",
        "TRIPLET_STDP",
        "R_STDP",
        "BCM",
        "COMBINED"
    };

    if (rule >= sizeof(names) / sizeof(names[0])) {
        return "UNKNOWN";
    }
    return names[rule];
}
