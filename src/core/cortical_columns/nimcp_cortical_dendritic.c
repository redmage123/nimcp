/**
 * @file nimcp_cortical_dendritic.c
 * @brief Implementation of dendritic computation for Layer V pyramidal cells
 * @version 1.0.0
 * @date 2025-12-15
 */

#include "core/cortical_columns/nimcp_cortical_dendritic.h"
#include "api/nimcp_api_exception.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(cortical_dendritic)

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * WHAT: Single dendritic compartment state
 * WHY:  Track voltage and synaptic state for one compartment
 * HOW:  Leaky integrator with capacitance and conductance
 */
typedef struct {
    float voltage;              /**< Current voltage (mV) */
    float input_current;        /**< Injected current */
    float capacitance;          /**< Membrane capacitance (pF) */
    float leak_conductance;     /**< Leak conductance (nS) */
} dendritic_compartment_state_t;

/**
 * WHAT: Single pyramidal cell state
 * WHY:  Encapsulate all compartments and soma for one neuron
 * HOW:  Three dendritic compartments + soma + output state
 */
typedef struct {
    dendritic_compartment_state_t basal;          /**< Basal dendrite */
    dendritic_compartment_state_t apical_oblique; /**< Apical oblique */
    dendritic_compartment_state_t apical_tuft;    /**< Apical tuft */
    float soma_voltage;                           /**< Somatic voltage (mV) */
    float soma_calcium;                           /**< Somatic calcium level */
    output_mode_t current_mode;                   /**< Current output mode */
    uint64_t last_spike_time;                     /**< Last spike timestamp (ms) */
    uint64_t last_basal_active_time;              /**< Last basal activation (ms) */
    uint64_t last_apical_active_time;             /**< Last apical activation (ms) */
    uint32_t burst_count;                         /**< Spikes in current burst */
    bool calcium_spike_active;                    /**< Ca²⁺ spike in progress */
    uint64_t calcium_spike_start_time;            /**< Ca²⁺ spike onset time (ms) */
} pyramidal_cell_state_t;

/**
 * WHAT: Complete dendritic computation system
 * WHY:  Manage all Layer V pyramidal cells and configuration
 * HOW:  Array of cells + config + stats + bio-async context
 */
struct cortical_dendritic {
    pyramidal_cell_state_t* cells;   /**< Array of pyramidal cells */
    uint32_t num_cells;              /**< Number of cells */
    dendritic_config_t config;       /**< Configuration parameters */
    dendritic_stats_t stats;         /**< Runtime statistics */
    uint64_t current_time_ms;        /**< Current simulation time (ms) */
    bio_module_context_t bio_ctx;    /**< Bio-async context */
    bool bio_async_enabled;          /**< Bio-async connected */
    nimcp_mutex_t mutex;             /**< Thread safety */
};

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * WHAT: Initialize dendritic compartment to resting state
 * WHY:  Set up compartment with realistic passive properties
 * HOW:  Set voltage to resting, assign capacitance and leak
 */
static void init_compartment(
    dendritic_compartment_state_t* comp,
    float resting_potential
) {
    if (!comp) return;

    comp->voltage = resting_potential;
    comp->input_current = 0.0f;
    comp->capacitance = 1.0f;  // Normalized capacitance
    comp->leak_conductance = 0.1f;  // Leak conductance
}

/**
 * WHAT: Update single compartment voltage with leaky integration
 * WHY:  Simulate passive membrane dynamics
 * HOW:  Leaky integrator: dV/dt = (I - g_leak*(V - V_rest)) / C
 */
static void update_compartment_voltage(
    dendritic_compartment_state_t* comp,
    float dt,
    float resting_potential
) {
    if (!comp) return;

    // Leaky integrator dynamics
    float leak_current = comp->leak_conductance * (comp->voltage - resting_potential);
    float dv_dt = (comp->input_current - leak_current) / comp->capacitance;

    comp->voltage += dv_dt * dt;
}

/**
 * WHAT: Apply NMDA voltage-dependent nonlinearity
 * WHY:  NMDA receptors have Mg²⁺ block removed at depolarized voltages
 * HOW:  Sigmoid function centered at NMDA threshold
 */
static float apply_nmda_nonlinearity(
    float input,
    float voltage,
    float nmda_threshold
) {
    // Sigmoid function for NMDA Mg²⁺ block removal
    // More depolarized → stronger NMDA conductance
    float v_diff = voltage - nmda_threshold;
    float gain = 1.0f / (1.0f + expf(-v_diff / 5.0f));

    return input * gain;
}

/**
 * WHAT: Check if basal and apical inputs are coincident
 * WHY:  Coincidence detection enables BAC firing
 * HOW:  Compare last activation times against window
 */
static bool check_coincidence(
    const pyramidal_cell_state_t* cell,
    uint64_t current_time,
    float window_ms
) {
    if (!cell) {
        return false;
    }

    // Check if both basal and apical were active recently
    uint64_t window_ticks = (uint64_t)window_ms;

    bool basal_recent = (current_time - cell->last_basal_active_time) < window_ticks;
    bool apical_recent = (current_time - cell->last_apical_active_time) < window_ticks;

    return basal_recent && apical_recent;
}

/**
 * WHAT: Determine output mode based on compartment states
 * WHY:  Implement burst vs single-spike decision logic
 * HOW:  Check coincidence, calcium spike, and thresholds
 */
static output_mode_t determine_output_mode(
    pyramidal_cell_state_t* cell,
    const dendritic_config_t* config,
    uint64_t current_time
) {
    if (!cell || !config) return OUTPUT_SILENT;

    // Check if soma is above threshold
    bool soma_above_threshold = (cell->soma_voltage >= config->soma_threshold);

    if (!soma_above_threshold) {
        return OUTPUT_SILENT;
    }

    // Check for coincidence (BAC firing condition)
    bool coincident = check_coincidence(cell, current_time, config->coincidence_window_ms);

    // If coincident AND calcium spike active → BURST
    if (coincident && cell->calcium_spike_active) {
        return OUTPUT_BURST;
    }

    // If soma above threshold but no coincidence → SINGLE SPIKE
    if (soma_above_threshold) {
        return OUTPUT_SINGLE_SPIKE;
    }

    return OUTPUT_SILENT;
}

/**
 * WHAT: Update calcium spike state
 * WHY:  Track Ca²⁺ spike duration and expiration
 * HOW:  Check elapsed time against duration
 */
static void update_calcium_spike(
    pyramidal_cell_state_t* cell,
    const dendritic_config_t* config,
    uint64_t current_time
) {
    if (!cell || !config) return;

    if (!cell->calcium_spike_active) return;

    // Check if calcium spike has expired
    uint64_t elapsed = current_time - cell->calcium_spike_start_time;
    uint64_t duration_ticks = (uint64_t)config->calcium_duration_ms;

    if (elapsed >= duration_ticks) {
        cell->calcium_spike_active = false;
    }
}

/**
 * WHAT: Generate calcium spike in apical tuft
 * WHY:  Simulate NMDA-dependent dendritic spike
 * HOW:  Set active flag and start time if above threshold
 */
static bool generate_calcium_spike_if_threshold_crossed(
    pyramidal_cell_state_t* cell,
    const dendritic_config_t* config,
    uint64_t current_time
) {
    if (!cell || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "generate_calcium_spike_if_threshold_crossed: required parameter is NULL (cell, config)");
        return false;
    }

    // Check if apical tuft voltage crossed Ca²⁺ threshold
    if (cell->apical_tuft.voltage >= config->calcium_threshold) {
        if (!cell->calcium_spike_active) {
            // Initiate new calcium spike
            cell->calcium_spike_active = true;
            cell->calcium_spike_start_time = current_time;
            return true;
        }
    }

    // Normal: voltage below threshold or spike already active — no new spike generated
    return false;
}

//=============================================================================
// Core API - Lifecycle
//=============================================================================

int cortical_dendritic_default_config(dendritic_config_t* config) {
    // Guard clause: validate input
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");
        NIMCP_LOGGING_ERROR("cortical_dendritic_default_config: config is NULL");
        return -1;
    }

    // Set biologically realistic defaults based on Larkum (2013)
    config->basal_weight = 0.7f;
    config->apical_weight = 0.3f;
    config->calcium_threshold = -35.0f;
    config->calcium_duration_ms = 30.0f;
    config->coincidence_window_ms = 20.0f;
    config->burst_threshold = 0.8f;
    config->enable_nmda_nonlinearity = true;
    config->nmda_voltage_threshold = -40.0f;
    config->soma_threshold = NIMCP_FIRING_THRESHOLD_MV;
    config->resting_potential = NIMCP_RESTING_POTENTIAL_MV;
    config->max_burst_spikes = 3;
    config->interburst_interval_ms = 4.0f;

    return 0;
}

cortical_dendritic_t* cortical_dendritic_create(
    const dendritic_config_t* config,
    uint32_t num_cells
) {
    // Guard clause: validate inputs
    if (num_cells == 0) {
        NIMCP_LOGGING_ERROR("cortical_dendritic_create: num_cells must be > 0");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cortical_dendritic_create: num_cells is zero");
        return NULL;
    }

    // Allocate main structure
    cortical_dendritic_t* dend = (cortical_dendritic_t*)nimcp_calloc(
        1, sizeof(cortical_dendritic_t)
    );
    if (!dend) {
        NIMCP_LOGGING_ERROR("cortical_dendritic_create: failed to allocate structure");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dend is NULL");

        return NULL;
    }

    // Set configuration
    if (config) {
        memcpy(&dend->config, config, sizeof(dendritic_config_t));
    } else {
        cortical_dendritic_default_config(&dend->config);
    }

    // Allocate cell array
    dend->num_cells = num_cells;
    dend->cells = (pyramidal_cell_state_t*)nimcp_calloc(
        num_cells, sizeof(pyramidal_cell_state_t)
    );
    if (!dend->cells) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate cells array");
        NIMCP_LOGGING_ERROR("cortical_dendritic_create: failed to allocate cells");
        nimcp_free(dend);
        return NULL;
    }

    // Initialize each cell
    for (uint32_t i = 0; i < num_cells; i++) {
        pyramidal_cell_state_t* cell = &dend->cells[i];

        init_compartment(&cell->basal, dend->config.resting_potential);
        init_compartment(&cell->apical_oblique, dend->config.resting_potential);
        init_compartment(&cell->apical_tuft, dend->config.resting_potential);

        cell->soma_voltage = dend->config.resting_potential;
        cell->soma_calcium = 0.0f;
        cell->current_mode = OUTPUT_SILENT;
        cell->last_spike_time = 0;
        cell->last_basal_active_time = 0;
        cell->last_apical_active_time = 0;
        cell->burst_count = 0;
        cell->calcium_spike_active = false;
        cell->calcium_spike_start_time = 0;
    }

    // Initialize statistics
    memset(&dend->stats, 0, sizeof(dendritic_stats_t));

    // Initialize time
    dend->current_time_ms = 0;

    // Initialize bio-async (not connected)
    dend->bio_ctx = NULL;
    dend->bio_async_enabled = false;

    // Initialize mutex
    if (nimcp_mutex_init(&dend->mutex, NULL) != 0) {
        NIMCP_LOGGING_WARN("cortical_dendritic_create: mutex init failed, continuing without locking");
    }

    NIMCP_LOGGING_INFO("Created dendritic system with %u cells", num_cells);

    return dend;
}

void cortical_dendritic_destroy(cortical_dendritic_t* dend) {
    // Guard clause: NULL-safe
    if (!dend) return;

    // Disconnect bio-async
    cortical_dendritic_disconnect_bio_async(dend);

    // Free cells
    if (dend->cells) {
        nimcp_free(dend->cells);
    }

    // Destroy mutex
    nimcp_mutex_destroy(&dend->mutex);

    // Free main structure
    nimcp_free(dend);

    NIMCP_LOGGING_INFO("Destroyed dendritic system");
}

//=============================================================================
// Core API - Input Setting
//=============================================================================

int cortical_dendritic_set_basal_input(
    cortical_dendritic_t* dend,
    uint32_t cell_idx,
    float input
) {
    // Guard clauses
    if (!dend) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dend is NULL");
        NIMCP_LOGGING_ERROR("cortical_dendritic_set_basal_input: dend is NULL");
        return -1;
    }
    if (cell_idx >= dend->num_cells) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "invalid cell_idx");
        NIMCP_LOGGING_ERROR("cortical_dendritic_set_basal_input: invalid cell_idx %u", cell_idx);
        return -1;
    }

    nimcp_mutex_lock(&dend->mutex);

    pyramidal_cell_state_t* cell = &dend->cells[cell_idx];
    cell->basal.input_current = input;

    // Update last active time if input is significant
    if (fabsf(input) > 0.01f) {
        cell->last_basal_active_time = dend->current_time_ms;
    }

    nimcp_mutex_unlock(&dend->mutex);

    return 0;
}

int cortical_dendritic_set_apical_input(
    cortical_dendritic_t* dend,
    uint32_t cell_idx,
    float input
) {
    // Guard clauses
    if (!dend) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dend is NULL");
        NIMCP_LOGGING_ERROR("cortical_dendritic_set_apical_input: dend is NULL");
        return -1;
    }
    if (cell_idx >= dend->num_cells) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "invalid cell_idx");
        NIMCP_LOGGING_ERROR("cortical_dendritic_set_apical_input: invalid cell_idx %u", cell_idx);
        return -1;
    }

    nimcp_mutex_lock(&dend->mutex);

    pyramidal_cell_state_t* cell = &dend->cells[cell_idx];
    cell->apical_tuft.input_current = input;

    // Update last active time if input is significant
    if (fabsf(input) > 0.01f) {
        cell->last_apical_active_time = dend->current_time_ms;
    }

    nimcp_mutex_unlock(&dend->mutex);

    return 0;
}

int cortical_dendritic_set_oblique_input(
    cortical_dendritic_t* dend,
    uint32_t cell_idx,
    float input
) {
    // Guard clauses
    if (!dend) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dend is NULL");
        NIMCP_LOGGING_ERROR("cortical_dendritic_set_oblique_input: dend is NULL");
        return -1;
    }
    if (cell_idx >= dend->num_cells) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "invalid cell_idx");
        NIMCP_LOGGING_ERROR("cortical_dendritic_set_oblique_input: invalid cell_idx %u", cell_idx);
        return -1;
    }

    nimcp_mutex_lock(&dend->mutex);

    pyramidal_cell_state_t* cell = &dend->cells[cell_idx];
    cell->apical_oblique.input_current = input;

    nimcp_mutex_unlock(&dend->mutex);

    return 0;
}

//=============================================================================
// Core API - Computation
//=============================================================================

int cortical_dendritic_update(cortical_dendritic_t* dend, float dt) {
    // Guard clauses
    if (!dend) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dend is NULL");
        NIMCP_LOGGING_ERROR("cortical_dendritic_update: dend is NULL");
        return -1;
    }
    if (dt <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dt must be > 0");
        NIMCP_LOGGING_ERROR("cortical_dendritic_update: dt must be > 0");
        return -1;
    }

    nimcp_mutex_lock(&dend->mutex);

    // Update simulation time
    dend->current_time_ms += (uint64_t)dt;

    // Update each cell
    for (uint32_t i = 0; i < dend->num_cells; i++) {
        pyramidal_cell_state_t* cell = &dend->cells[i];

        // 1. Update compartment voltages
        update_compartment_voltage(&cell->basal, dt, dend->config.resting_potential);
        update_compartment_voltage(&cell->apical_oblique, dt, dend->config.resting_potential);

        // Apply NMDA nonlinearity to apical tuft if enabled
        if (dend->config.enable_nmda_nonlinearity) {
            float nmda_input = apply_nmda_nonlinearity(
                cell->apical_tuft.input_current,
                cell->apical_tuft.voltage,
                dend->config.nmda_voltage_threshold
            );
            cell->apical_tuft.input_current = nmda_input;
        }
        update_compartment_voltage(&cell->apical_tuft, dt, dend->config.resting_potential);

        // 2. Check for calcium spike generation
        bool ca_spike_generated = generate_calcium_spike_if_threshold_crossed(
            cell, &dend->config, dend->current_time_ms
        );
        if (ca_spike_generated) {
            dend->stats.calcium_spikes_generated++;
        }

        // 3. Update calcium spike state (check expiration)
        update_calcium_spike(cell, &dend->config, dend->current_time_ms);

        // 4. Propagate inputs to soma (weighted sum)
        float soma_input =
            dend->config.basal_weight * cell->basal.voltage +
            dend->config.apical_weight * (cell->apical_tuft.voltage + cell->apical_oblique.voltage);

        // Add calcium spike contribution if active
        if (cell->calcium_spike_active) {
            soma_input += 20.0f;  // Ca²⁺ spike adds strong depolarization
        }

        // Update soma voltage
        float leak = 0.1f * (cell->soma_voltage - dend->config.resting_potential);
        cell->soma_voltage += dt * (soma_input - leak);

        // 5. Determine output mode
        output_mode_t prev_mode = cell->current_mode;
        cell->current_mode = determine_output_mode(cell, &dend->config, dend->current_time_ms);

        // 6. Update statistics if mode changed
        if (cell->current_mode != prev_mode) {
            switch (cell->current_mode) {
                case OUTPUT_BURST:
                    dend->stats.burst_outputs++;
                    // Check if this was BAC firing (coincidence-triggered)
                    if (check_coincidence(cell, dend->current_time_ms, dend->config.coincidence_window_ms)) {
                        dend->stats.bac_coincidences++;
                    }
                    break;
                case OUTPUT_SINGLE_SPIKE:
                    dend->stats.single_spike_outputs++;
                    break;
                case OUTPUT_SILENT:
                    dend->stats.silent_outputs++;
                    break;
            }
        }

        // 7. Update running averages
        dend->stats.mean_basal_activation += cell->basal.voltage;
        dend->stats.mean_apical_activation += cell->apical_tuft.voltage;
        dend->stats.mean_soma_voltage += cell->soma_voltage;
    }

    // Finalize statistics
    dend->stats.total_updates++;

    if (dend->num_cells > 0) {
        float inv_cells = 1.0f / (float)dend->num_cells;
        dend->stats.mean_basal_activation *= inv_cells;
        dend->stats.mean_apical_activation *= inv_cells;
        dend->stats.mean_soma_voltage *= inv_cells;
    }

    // Calculate rates
    uint64_t total_outputs = dend->stats.burst_outputs + dend->stats.single_spike_outputs + dend->stats.silent_outputs;
    if (total_outputs > 0) {
        dend->stats.burst_rate = (float)dend->stats.burst_outputs / (float)total_outputs;
    }

    if (dend->stats.calcium_spikes_generated > 0) {
        dend->stats.bac_success_rate = (float)dend->stats.bac_coincidences / (float)dend->stats.calcium_spikes_generated;
    }

    nimcp_mutex_unlock(&dend->mutex);

    return 0;
}

int cortical_dendritic_get_output_mode(
    const cortical_dendritic_t* dend,
    uint32_t cell_idx,
    output_mode_t* mode
) {
    // Guard clauses
    if (!dend) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dend is NULL");
        NIMCP_LOGGING_ERROR("cortical_dendritic_get_output_mode: dend is NULL");
        return -1;
    }
    if (cell_idx >= dend->num_cells) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "invalid cell_idx");
        NIMCP_LOGGING_ERROR("cortical_dendritic_get_output_mode: invalid cell_idx");
        return -1;
    }
    if (!mode) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mode is NULL");
        NIMCP_LOGGING_ERROR("cortical_dendritic_get_output_mode: mode is NULL");
        return -1;
    }

    nimcp_mutex_t* mutex_ptr = (nimcp_mutex_t*)&dend->mutex;
    nimcp_mutex_lock(mutex_ptr);

    *mode = dend->cells[cell_idx].current_mode;

    nimcp_mutex_unlock(mutex_ptr);

    return 0;
}

int cortical_dendritic_get_soma_voltage(
    const cortical_dendritic_t* dend,
    uint32_t cell_idx,
    float* voltage
) {
    // Guard clauses
    if (!dend) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dend is NULL");
        NIMCP_LOGGING_ERROR("cortical_dendritic_get_soma_voltage: dend is NULL");
        return -1;
    }
    if (cell_idx >= dend->num_cells) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "invalid cell_idx");
        NIMCP_LOGGING_ERROR("cortical_dendritic_get_soma_voltage: invalid cell_idx");
        return -1;
    }
    if (!voltage) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "voltage is NULL");
        NIMCP_LOGGING_ERROR("cortical_dendritic_get_soma_voltage: voltage is NULL");
        return -1;
    }

    nimcp_mutex_t* mutex_ptr = (nimcp_mutex_t*)&dend->mutex;
    nimcp_mutex_lock(mutex_ptr);

    *voltage = dend->cells[cell_idx].soma_voltage;

    nimcp_mutex_unlock(mutex_ptr);

    return 0;
}

int cortical_dendritic_is_calcium_spike_active(
    const cortical_dendritic_t* dend,
    uint32_t cell_idx,
    bool* active
) {
    // Guard clauses
    if (!dend) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dend is NULL");
        NIMCP_LOGGING_ERROR("cortical_dendritic_is_calcium_spike_active: dend is NULL");
        return -1;
    }
    if (cell_idx >= dend->num_cells) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "invalid cell_idx");
        NIMCP_LOGGING_ERROR("cortical_dendritic_is_calcium_spike_active: invalid cell_idx");
        return -1;
    }
    if (!active) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "active is NULL");
        NIMCP_LOGGING_ERROR("cortical_dendritic_is_calcium_spike_active: active is NULL");
        return -1;
    }

    nimcp_mutex_t* mutex_ptr = (nimcp_mutex_t*)&dend->mutex;
    nimcp_mutex_lock(mutex_ptr);

    *active = dend->cells[cell_idx].calcium_spike_active;

    nimcp_mutex_unlock(mutex_ptr);

    return 0;
}

//=============================================================================
// Statistics and Monitoring
//=============================================================================

int cortical_dendritic_get_stats(
    const cortical_dendritic_t* dend,
    dendritic_stats_t* stats
) {
    // Guard clauses
    if (!dend) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dend is NULL");
        NIMCP_LOGGING_ERROR("cortical_dendritic_get_stats: dend is NULL");
        return -1;
    }
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stats is NULL");
        NIMCP_LOGGING_ERROR("cortical_dendritic_get_stats: stats is NULL");
        return -1;
    }

    nimcp_mutex_t* mutex_ptr = (nimcp_mutex_t*)&dend->mutex;
    nimcp_mutex_lock(mutex_ptr);

    memcpy(stats, &dend->stats, sizeof(dendritic_stats_t));

    nimcp_mutex_unlock(mutex_ptr);

    return 0;
}

int cortical_dendritic_reset_stats(cortical_dendritic_t* dend) {
    // Guard clause
    if (!dend) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dend is NULL");
        NIMCP_LOGGING_ERROR("cortical_dendritic_reset_stats: dend is NULL");
        return -1;
    }

    nimcp_mutex_lock(&dend->mutex);

    memset(&dend->stats, 0, sizeof(dendritic_stats_t));

    nimcp_mutex_unlock(&dend->mutex);

    return 0;
}

//=============================================================================
// Bio-Async Integration
//=============================================================================

int cortical_dendritic_connect_bio_async(cortical_dendritic_t* dend) {
    // Guard clause
    if (!dend) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dend is NULL");
        NIMCP_LOGGING_ERROR("cortical_dendritic_connect_bio_async: dend is NULL");
        return -1;
    }

    nimcp_mutex_lock(&dend->mutex);

    // Check if already connected
    if (dend->bio_async_enabled) {
        nimcp_mutex_unlock(&dend->mutex);
        return 0;
    }

    // Note: Bio-async integration requires BIO_MODULE_CORTICAL_DENDRITIC
    // to be defined in nimcp_bio_messages.h
    // For now, we'll set a placeholder since the exact enum value (0x014F)
    // needs to be added to the header file

    NIMCP_LOGGING_INFO("Bio-async router not available, skipping registration");

    // Mark as "attempted" but not actually connected
    // This is expected behavior when bio-async is not available

    nimcp_mutex_unlock(&dend->mutex);

    return 1;  // Return 1 to indicate bio-async not available (not an error)
}

int cortical_dendritic_disconnect_bio_async(cortical_dendritic_t* dend) {
    // Guard clause
    if (!dend) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dend is NULL");

        return -1;

    }

    nimcp_mutex_lock(&dend->mutex);

    if (dend->bio_async_enabled && dend->bio_ctx) {
        // Unregister from bio-async router
        // (Would call bio_router_unregister_module if available)
        dend->bio_ctx = NULL;
        dend->bio_async_enabled = false;
    }

    nimcp_mutex_unlock(&dend->mutex);

    return 0;
}

bool cortical_dendritic_is_bio_async_connected(
    const cortical_dendritic_t* dend
) {
    if (!dend) {
        return false;
    }

    nimcp_mutex_t* mutex_ptr = (nimcp_mutex_t*)&dend->mutex;
    nimcp_mutex_lock(mutex_ptr);

    bool connected = dend->bio_async_enabled;

    nimcp_mutex_unlock(mutex_ptr);

    return connected;
}
