/**
 * @file nimcp_neurogenesis.c
 * @brief Neurogenesis Module Implementation
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: Implements neurogenesis for dynamic neural network growth
 * WHY:  Enable activity-dependent network expansion
 * HOW:  Track stem cells, differentiation, integration, pruning
 *
 * @author NIMCP Development Team
 */

#include "biology/neurogenesis/nimcp_neurogenesis.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/rng/nimcp_rand.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(neurogenesis)

//=============================================================================
// Internal Structures
//=============================================================================

typedef struct {
    nimcp_stem_state_t state;
    float proliferation_capacity;       /* 0 = depleted, 1 = full */
    float time_in_state;
    uint32_t divisions_completed;
    bool active;
} stem_cell_t;

typedef struct {
    nimcp_niche_config_t config;
    stem_cell_t* stem_cells;
    uint32_t num_stem_cells;
    uint32_t max_stem_cells;
    float local_modifier;
    bool active;
} niche_t;

typedef struct {
    uint32_t neuron_id;
    uint32_t niche_id;
    nimcp_new_neuron_type_t type;
    nimcp_neuron_stage_t stage;
    float maturity;
    float activity_accumulator;
    float recent_activity;
    uint32_t connections_formed;
    uint32_t integration_steps_remaining;
    float position[3];
    bool active;
} pending_neuron_t;

struct nimcp_neurogenesis_struct {
    nimcp_neurogenesis_config_t config;
    nimcp_brain_t brain;
    bool is_initialized;

    /* Niches */
    niche_t* niches;
    uint32_t num_niches;
    uint32_t max_niches;

    /* Pending neurons */
    pending_neuron_t* pending_neurons;
    uint32_t num_pending;
    uint32_t max_pending;
    uint32_t next_neuron_id;

    /* Environmental factors */
    float bdnf_level;
    float stress_level;
    float exercise_level;
    float enrichment_level;
    float environmental_modifier;

    /* Callbacks */
    nimcp_neuron_created_cb on_created;
    nimcp_neuron_matured_cb on_matured;
    nimcp_neuron_pruned_cb on_pruned;
    void* callback_user_data;

    /* State and stats */
    nimcp_neurogenesis_state_t state;
    nimcp_neurogenesis_stats_t stats;
    uint64_t current_time;
};

//=============================================================================
// Helper Functions
//=============================================================================

static float compute_environmental_modifier(nimcp_neurogenesis_t ng) {
    /* BDNF and exercise increase neurogenesis */
    float positive = (ng->bdnf_level + ng->exercise_level + ng->enrichment_level) / 3.0f;
    /* Stress decreases neurogenesis */
    float negative = ng->stress_level;

    float modifier = 1.0f + positive * 0.5f - negative * 0.4f;
    return fmaxf(0.1f, fminf(2.0f, modifier));
}

static void update_stem_cell(stem_cell_t* cell, float dt, float prolif_rate, float env_mod) {
    cell->time_in_state += dt;

    switch (cell->state) {
        case STEM_STATE_QUIESCENT:
            /* Random activation based on rate */
            if (nimcp_rand_uniform() < prolif_rate * env_mod * dt * 0.01f) {
                cell->state = STEM_STATE_ACTIVATED;
                cell->time_in_state = 0.0f;
            }
            break;

        case STEM_STATE_ACTIVATED:
            /* Transition to proliferating after delay */
            if (cell->time_in_state > 10.0f) {
                cell->state = STEM_STATE_PROLIFERATING;
                cell->time_in_state = 0.0f;
            }
            break;

        case STEM_STATE_PROLIFERATING:
            /* Ready to divide or differentiate */
            if (cell->time_in_state > 20.0f) {
                /* Decide: divide again or differentiate */
                if (cell->proliferation_capacity > 0.3f &&
                    nimcp_rand_uniform() < 0.5f) {
                    /* Self-renew */
                    cell->proliferation_capacity *= 0.95f;
                    cell->divisions_completed++;
                    cell->state = STEM_STATE_QUIESCENT;
                } else {
                    /* Differentiate */
                    cell->state = STEM_STATE_DIFFERENTIATING;
                }
                cell->time_in_state = 0.0f;
            }
            break;

        case STEM_STATE_DIFFERENTIATING:
            /* Will be consumed to create a neuron */
            break;

        case STEM_STATE_DEPLETED:
            /* Cannot recover */
            break;
    }

    /* Check for depletion */
    if (cell->proliferation_capacity < 0.1f &&
        cell->state != STEM_STATE_DEPLETED) {
        cell->state = STEM_STATE_DEPLETED;
    }
}

static void update_pending_neuron(pending_neuron_t* neuron, float dt,
                                  float survival_threshold, float pruning_rate) {
    if (!neuron->active) return;

    /* Update activity with decay */
    neuron->recent_activity = neuron->activity_accumulator;
    neuron->activity_accumulator *= 0.9f;

    switch (neuron->stage) {
        case NEURON_STAGE_PROGENITOR:
            neuron->maturity += dt * 0.005f;
            if (neuron->maturity > 0.2f) {
                neuron->stage = NEURON_STAGE_IMMATURE;
            }
            break;

        case NEURON_STAGE_IMMATURE:
            neuron->maturity += dt * 0.008f;
            if (neuron->maturity > 0.5f) {
                neuron->stage = NEURON_STAGE_INTEGRATING;
            }
            break;

        case NEURON_STAGE_INTEGRATING:
            neuron->maturity += dt * 0.01f;
            if (neuron->integration_steps_remaining > 0)
                neuron->integration_steps_remaining--;

            /* Form connections based on activity */
            if (neuron->recent_activity > 0.1f) {
                float conn_prob = neuron->recent_activity * dt * 0.1f;
                if (nimcp_rand_uniform() < conn_prob) {
                    neuron->connections_formed++;
                }
            }

            if (neuron->maturity >= 1.0f || neuron->integration_steps_remaining <= 0) {
                neuron->maturity = 1.0f;
                neuron->stage = NEURON_STAGE_MATURE;
            }
            break;

        case NEURON_STAGE_MATURE:
            /* Check for pruning due to low activity */
            if (neuron->recent_activity < survival_threshold) {
                float prune_prob = pruning_rate * dt * 0.1f;
                if (nimcp_rand_uniform() < prune_prob) {
                    neuron->stage = NEURON_STAGE_APOPTOTIC;
                }
            }
            break;

        case NEURON_STAGE_APOPTOTIC:
            /* Will be cleaned up */
            neuron->active = false;
            break;
    }
}

//=============================================================================
// Configuration
//=============================================================================

nimcp_neurogenesis_config_t nimcp_neurogenesis_default_config(void) {
    nimcp_neurogenesis_config_t config = {
        .max_niches = NEUROGENESIS_MAX_NICHES,
        .max_stem_cells_per_niche = NEUROGENESIS_MAX_STEM_CELLS,
        .max_pending_neurons = NEUROGENESIS_MAX_PENDING,
        .base_proliferation_rate = NEUROGENESIS_DEFAULT_PROLIF_RATE,
        .integration_duration = NEUROGENESIS_DEFAULT_INTEGRATION,
        .survival_threshold = NEUROGENESIS_SURVIVAL_THRESHOLD,
        .pruning_rate = 0.001f,
        .enable_activity_modulation = true,
        .enable_environmental_factors = true,
        .enable_logging = false,
        .enable_metrics = true
    };
    return config;
}

//=============================================================================
// Lifecycle
//=============================================================================

nimcp_neurogenesis_t nimcp_neurogenesis_create(const nimcp_neurogenesis_config_t* config) {
    nimcp_neurogenesis_t ng = (nimcp_neurogenesis_t)nimcp_calloc(1, sizeof(struct nimcp_neurogenesis_struct));
    if (!ng) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(struct nimcp_neurogenesis_struct), "Neurogenesis allocation failed");
        return NULL;
    }

    ng->config = config ? *config : nimcp_neurogenesis_default_config();

    /* Allocate niches */
    ng->max_niches = ng->config.max_niches;
    ng->niches = (niche_t*)nimcp_calloc(ng->max_niches, sizeof(niche_t));
    if (!ng->niches) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, ng->max_niches * sizeof(niche_t), "Niche array allocation failed");
        nimcp_free(ng);
        return NULL;
    }

    /* Allocate pending neurons */
    ng->max_pending = ng->config.max_pending_neurons;
    ng->pending_neurons = (pending_neuron_t*)nimcp_calloc(ng->max_pending, sizeof(pending_neuron_t));
    if (!ng->pending_neurons) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, ng->max_pending * sizeof(pending_neuron_t), "Pending neurons allocation failed");
        nimcp_free(ng->niches);
        nimcp_free(ng);
        return NULL;
    }

    ng->next_neuron_id = 100000;  /* Start from high ID to avoid conflicts */
    ng->environmental_modifier = 1.0f;
    ng->bdnf_level = 0.5f;
    ng->stress_level = 0.0f;
    ng->exercise_level = 0.0f;
    ng->enrichment_level = 0.5f;

    return ng;
}

void nimcp_neurogenesis_destroy(nimcp_neurogenesis_t ng) {
    if (!ng) return;

    if (ng->is_initialized) {
        nimcp_neurogenesis_shutdown(ng);
    }

    /* Free niche stem cells */
    for (uint32_t i = 0; i < ng->num_niches; i++) {
        nimcp_free(ng->niches[i].stem_cells);
    }

    nimcp_free(ng->pending_neurons);
    nimcp_free(ng->niches);
    nimcp_free(ng);
}

nimcp_neurogenesis_error_t nimcp_neurogenesis_init(
    nimcp_neurogenesis_t ng,
    nimcp_brain_t brain
) {
    if (!ng) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Neurogenesis handle is NULL");
        return NEUROGENESIS_ERR_NULL_PTR;
    }
    if (ng->is_initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "Neurogenesis already initialized");
        return NEUROGENESIS_ERR_ALREADY_INITIALIZED;
    }

    ng->brain = brain;
    ng->is_initialized = true;

    return NEUROGENESIS_OK;
}

nimcp_neurogenesis_error_t nimcp_neurogenesis_shutdown(nimcp_neurogenesis_t ng) {
    if (!ng) return NEUROGENESIS_ERR_NULL_PTR;
    if (!ng->is_initialized) return NEUROGENESIS_ERR_NOT_INITIALIZED;

    ng->is_initialized = false;
    ng->brain = NULL;

    return NEUROGENESIS_OK;
}

//=============================================================================
// Update
//=============================================================================

nimcp_neurogenesis_error_t nimcp_neurogenesis_update(nimcp_neurogenesis_t ng, float dt) {
    if (!ng) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Neurogenesis handle is NULL in update");
        return NEUROGENESIS_ERR_NULL_PTR;
    }

    ng->current_time += (uint64_t)(dt * 1000.0f);

    /* Update environmental modifier */
    if (ng->config.enable_environmental_factors) {
        ng->environmental_modifier = compute_environmental_modifier(ng);
    }

    /* Update stem cells in all niches */
    uint32_t total_stem = 0;
    uint32_t active_stem = 0;

    for (uint32_t n = 0; n < ng->num_niches; n++) {
        if (!ng->niches[n].active) continue;

        niche_t* niche = &ng->niches[n];
        float prolif_rate = niche->config.local_proliferation_rate *
                           ng->config.base_proliferation_rate *
                           niche->local_modifier;

        for (uint32_t s = 0; s < niche->num_stem_cells; s++) {
            stem_cell_t* cell = &niche->stem_cells[s];
            if (!cell->active) continue;

            total_stem++;
            if (cell->state == STEM_STATE_PROLIFERATING ||
                cell->state == STEM_STATE_ACTIVATED) {
                active_stem++;
            }

            update_stem_cell(cell, dt, prolif_rate, ng->environmental_modifier);

            /* Handle differentiation - create new neuron */
            if (cell->state == STEM_STATE_DIFFERENTIATING) {
                uint32_t new_id;
                nimcp_neurogenesis_create_neuron(ng, niche->config.niche_id, &new_id);
                cell->state = STEM_STATE_QUIESCENT;
                cell->time_in_state = 0.0f;
                cell->proliferation_capacity *= 0.9f;
                ng->stats.stem_cell_divisions++;

                if (cell->proliferation_capacity < 0.1f) {
                    cell->state = STEM_STATE_DEPLETED;
                    cell->active = false;
                    ng->stats.stem_cells_depleted++;
                }
            }
        }
    }

    /* Update pending neurons */
    uint32_t pending_count = 0;
    for (uint32_t i = 0; i < ng->num_pending; i++) {
        pending_neuron_t* neuron = &ng->pending_neurons[i];
        if (!neuron->active) continue;

        nimcp_neuron_stage_t prev_stage = neuron->stage;

        update_pending_neuron(neuron, dt,
                             ng->config.survival_threshold,
                             ng->config.pruning_rate);

        if (neuron->active) {
            pending_count++;

            /* Check for maturation */
            if (prev_stage != NEURON_STAGE_MATURE && neuron->stage == NEURON_STAGE_MATURE) {
                ng->stats.neurons_integrated++;
                ng->state.mature_neurons_created++;

                if (ng->on_matured) {
                    ng->on_matured(neuron->neuron_id, neuron->connections_formed,
                                  ng->callback_user_data);
                }
            }
        } else {
            /* Neuron was pruned */
            ng->stats.neurons_pruned++;
            ng->state.neurons_pruned++;

            if (ng->on_pruned) {
                ng->on_pruned(neuron->neuron_id, neuron->recent_activity,
                             ng->callback_user_data);
            }
        }
    }

    /* Update state */
    ng->state.total_stem_cells = total_stem;
    ng->state.active_stem_cells = active_stem;
    ng->state.pending_neurons = pending_count;
    ng->state.global_proliferation_rate = ng->config.base_proliferation_rate *
                                          ng->environmental_modifier;
    ng->state.environmental_modifier = ng->environmental_modifier;
    ng->state.last_update_time = ng->current_time;

    /* Update stats averages */
    ng->stats.avg_proliferation_rate = ng->stats.avg_proliferation_rate * 0.99f +
                                       ng->state.global_proliferation_rate * 0.01f;
    if (!isfinite(ng->stats.avg_proliferation_rate)) ng->stats.avg_proliferation_rate = ng->state.global_proliferation_rate;

    return NEUROGENESIS_OK;
}

//=============================================================================
// Callbacks
//=============================================================================

nimcp_neurogenesis_error_t nimcp_neurogenesis_set_callbacks(
    nimcp_neurogenesis_t ng,
    nimcp_neuron_created_cb on_created,
    nimcp_neuron_matured_cb on_matured,
    nimcp_neuron_pruned_cb on_pruned,
    void* user_data
) {
    if (!ng) return NEUROGENESIS_ERR_NULL_PTR;

    ng->on_created = on_created;
    ng->on_matured = on_matured;
    ng->on_pruned = on_pruned;
    ng->callback_user_data = user_data;

    return NEUROGENESIS_OK;
}

//=============================================================================
// State and Stats
//=============================================================================

nimcp_neurogenesis_error_t nimcp_neurogenesis_get_state(
    nimcp_neurogenesis_t ng,
    nimcp_neurogenesis_state_t* state
) {
    if (!ng || !state) return NEUROGENESIS_ERR_NULL_PTR;
    *state = ng->state;
    return NEUROGENESIS_OK;
}

nimcp_neurogenesis_error_t nimcp_neurogenesis_get_stats(
    nimcp_neurogenesis_t ng,
    nimcp_neurogenesis_stats_t* stats
) {
    if (!ng || !stats) return NEUROGENESIS_ERR_NULL_PTR;
    *stats = ng->stats;
    return NEUROGENESIS_OK;
}

nimcp_neurogenesis_error_t nimcp_neurogenesis_reset_stats(nimcp_neurogenesis_t ng) {
    if (!ng) return NEUROGENESIS_ERR_NULL_PTR;
    memset(&ng->stats, 0, sizeof(ng->stats));
    return NEUROGENESIS_OK;
}

//=============================================================================
// Niche API
//=============================================================================

nimcp_neurogenesis_error_t nimcp_neurogenesis_create_niche(
    nimcp_neurogenesis_t ng,
    const nimcp_niche_config_t* config
) {
    if (!ng || !config) return NEUROGENESIS_ERR_NULL_PTR;
    if (ng->num_niches >= ng->max_niches) return NEUROGENESIS_ERR_NICHE_FULL;

    uint32_t slot = ng->num_niches;
    niche_t* niche = &ng->niches[slot];

    niche->config = *config;
    niche->max_stem_cells = ng->config.max_stem_cells_per_niche;
    niche->stem_cells = (stem_cell_t*)nimcp_calloc(niche->max_stem_cells, sizeof(stem_cell_t));
    if (!niche->stem_cells) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, niche->max_stem_cells * sizeof(stem_cell_t), "Stem cells array allocation failed for niche");
        return NEUROGENESIS_ERR_NO_MEMORY;
    }

    /* Initialize stem cells */
    for (uint32_t i = 0; i < config->initial_stem_cells && i < niche->max_stem_cells; i++) {
        niche->stem_cells[i].state = STEM_STATE_QUIESCENT;
        niche->stem_cells[i].proliferation_capacity = 1.0f;
        niche->stem_cells[i].active = true;
        niche->num_stem_cells++;
    }

    niche->local_modifier = 1.0f;
    niche->active = true;

    ng->num_niches++;

    return NEUROGENESIS_OK;
}

nimcp_neurogenesis_error_t nimcp_neurogenesis_remove_niche(
    nimcp_neurogenesis_t ng,
    uint32_t niche_id
) {
    if (!ng) return NEUROGENESIS_ERR_NULL_PTR;

    for (uint32_t i = 0; i < ng->num_niches; i++) {
        if (ng->niches[i].active && ng->niches[i].config.niche_id == niche_id) {
            ng->niches[i].active = false;
            return NEUROGENESIS_OK;
        }
    }

    return NEUROGENESIS_ERR_NICHE_NOT_FOUND;
}

nimcp_neurogenesis_error_t nimcp_neurogenesis_get_niche_info(
    nimcp_neurogenesis_t ng,
    uint32_t niche_id,
    uint32_t* stem_cells,
    uint32_t* pending_neurons,
    float* proliferation_rate
) {
    if (!ng) return NEUROGENESIS_ERR_NULL_PTR;

    for (uint32_t i = 0; i < ng->num_niches; i++) {
        if (ng->niches[i].active && ng->niches[i].config.niche_id == niche_id) {
            niche_t* niche = &ng->niches[i];

            if (stem_cells) {
                uint32_t count = 0;
                for (uint32_t s = 0; s < niche->num_stem_cells; s++) {
                    if (niche->stem_cells[s].active) count++;
                }
                *stem_cells = count;
            }

            if (pending_neurons) {
                uint32_t count = 0;
                for (uint32_t p = 0; p < ng->num_pending; p++) {
                    if (ng->pending_neurons[p].active &&
                        ng->pending_neurons[p].niche_id == niche_id) {
                        count++;
                    }
                }
                *pending_neurons = count;
            }

            if (proliferation_rate) {
                *proliferation_rate = niche->config.local_proliferation_rate *
                                     ng->config.base_proliferation_rate *
                                     niche->local_modifier *
                                     ng->environmental_modifier;
            }

            return NEUROGENESIS_OK;
        }
    }

    return NEUROGENESIS_ERR_NICHE_NOT_FOUND;
}

nimcp_neurogenesis_error_t nimcp_neurogenesis_set_niche_rate(
    nimcp_neurogenesis_t ng,
    uint32_t niche_id,
    float rate
) {
    if (!ng) return NEUROGENESIS_ERR_NULL_PTR;

    for (uint32_t i = 0; i < ng->num_niches; i++) {
        if (ng->niches[i].active && ng->niches[i].config.niche_id == niche_id) {
            ng->niches[i].local_modifier = fmaxf(0.0f, fminf(2.0f, rate));
            return NEUROGENESIS_OK;
        }
    }

    return NEUROGENESIS_ERR_NICHE_NOT_FOUND;
}

//=============================================================================
// Stem Cell API
//=============================================================================

nimcp_neurogenesis_error_t nimcp_neurogenesis_add_stem_cells(
    nimcp_neurogenesis_t ng,
    uint32_t niche_id,
    uint32_t count
) {
    if (!ng) return NEUROGENESIS_ERR_NULL_PTR;

    for (uint32_t i = 0; i < ng->num_niches; i++) {
        if (ng->niches[i].active && ng->niches[i].config.niche_id == niche_id) {
            niche_t* niche = &ng->niches[i];

            for (uint32_t c = 0; c < count && niche->num_stem_cells < niche->max_stem_cells; c++) {
                uint32_t slot = niche->num_stem_cells;
                niche->stem_cells[slot].state = STEM_STATE_QUIESCENT;
                niche->stem_cells[slot].proliferation_capacity = 1.0f;
                niche->stem_cells[slot].active = true;
                niche->num_stem_cells++;
            }

            return NEUROGENESIS_OK;
        }
    }

    return NEUROGENESIS_ERR_NICHE_NOT_FOUND;
}

nimcp_neurogenesis_error_t nimcp_neurogenesis_get_stem_count(
    nimcp_neurogenesis_t ng,
    uint32_t niche_id,
    uint32_t* count
) {
    if (!ng || !count) return NEUROGENESIS_ERR_NULL_PTR;

    for (uint32_t i = 0; i < ng->num_niches; i++) {
        if (ng->niches[i].active && ng->niches[i].config.niche_id == niche_id) {
            uint32_t active_count = 0;
            for (uint32_t s = 0; s < ng->niches[i].num_stem_cells; s++) {
                if (ng->niches[i].stem_cells[s].active) active_count++;
            }
            *count = active_count;
            return NEUROGENESIS_OK;
        }
    }

    return NEUROGENESIS_ERR_NICHE_NOT_FOUND;
}

nimcp_neurogenesis_error_t nimcp_neurogenesis_activate_stem_cells(
    nimcp_neurogenesis_t ng,
    uint32_t niche_id,
    uint32_t count
) {
    if (!ng) return NEUROGENESIS_ERR_NULL_PTR;

    for (uint32_t i = 0; i < ng->num_niches; i++) {
        if (ng->niches[i].active && ng->niches[i].config.niche_id == niche_id) {
            niche_t* niche = &ng->niches[i];
            uint32_t activated = 0;

            for (uint32_t s = 0; s < niche->num_stem_cells && activated < count; s++) {
                if (niche->stem_cells[s].active &&
                    niche->stem_cells[s].state == STEM_STATE_QUIESCENT) {
                    niche->stem_cells[s].state = STEM_STATE_ACTIVATED;
                    niche->stem_cells[s].time_in_state = 0.0f;
                    activated++;
                }
            }

            return NEUROGENESIS_OK;
        }
    }

    return NEUROGENESIS_ERR_NICHE_NOT_FOUND;
}

nimcp_neurogenesis_error_t nimcp_neurogenesis_get_stem_distribution(
    nimcp_neurogenesis_t ng,
    uint32_t niche_id,
    uint32_t* quiescent,
    uint32_t* activated,
    uint32_t* proliferating,
    uint32_t* differentiating
) {
    if (!ng) return NEUROGENESIS_ERR_NULL_PTR;

    for (uint32_t i = 0; i < ng->num_niches; i++) {
        if (ng->niches[i].active && ng->niches[i].config.niche_id == niche_id) {
            uint32_t q = 0, a = 0, p = 0, d = 0;

            for (uint32_t s = 0; s < ng->niches[i].num_stem_cells; s++) {
                if (!ng->niches[i].stem_cells[s].active) continue;

                switch (ng->niches[i].stem_cells[s].state) {
                    case STEM_STATE_QUIESCENT: q++; break;
                    case STEM_STATE_ACTIVATED: a++; break;
                    case STEM_STATE_PROLIFERATING: p++; break;
                    case STEM_STATE_DIFFERENTIATING: d++; break;
                    default: break;
                }
            }

            if (quiescent) *quiescent = q;
            if (activated) *activated = a;
            if (proliferating) *proliferating = p;
            if (differentiating) *differentiating = d;

            return NEUROGENESIS_OK;
        }
    }

    return NEUROGENESIS_ERR_NICHE_NOT_FOUND;
}

//=============================================================================
// Neuron Generation API
//=============================================================================

nimcp_neurogenesis_error_t nimcp_neurogenesis_create_neuron(
    nimcp_neurogenesis_t ng,
    uint32_t niche_id,
    uint32_t* new_neuron_id
) {
    if (!ng) return NEUROGENESIS_ERR_NULL_PTR;
    if (ng->num_pending >= ng->max_pending) return NEUROGENESIS_ERR_NICHE_FULL;

    /* Find niche */
    niche_t* niche = NULL;
    for (uint32_t i = 0; i < ng->num_niches; i++) {
        if (ng->niches[i].active && ng->niches[i].config.niche_id == niche_id) {
            niche = &ng->niches[i];
            break;
        }
    }
    if (!niche) return NEUROGENESIS_ERR_NICHE_NOT_FOUND;

    /* Find free slot */
    uint32_t slot = ng->max_pending;
    for (uint32_t i = 0; i < ng->max_pending; i++) {
        if (!ng->pending_neurons[i].active) {
            slot = i;
            break;
        }
    }
    if (slot >= ng->max_pending) return NEUROGENESIS_ERR_NICHE_FULL;

    pending_neuron_t* neuron = &ng->pending_neurons[slot];
    neuron->neuron_id = ng->next_neuron_id++;
    neuron->niche_id = niche_id;
    neuron->type = niche->config.default_type;
    neuron->stage = NEURON_STAGE_PROGENITOR;
    neuron->maturity = 0.0f;
    neuron->activity_accumulator = 0.0f;
    neuron->recent_activity = 0.0f;
    neuron->connections_formed = 0;
    neuron->integration_steps_remaining = (uint32_t)ng->config.integration_duration;
    neuron->active = true;

    if (slot >= ng->num_pending) {
        ng->num_pending = slot + 1;
    }

    if (new_neuron_id) *new_neuron_id = neuron->neuron_id;

    ng->stats.neurons_created++;

    if (ng->on_created) {
        ng->on_created(neuron->neuron_id, neuron->type, niche_id,
                      ng->callback_user_data);
    }

    return NEUROGENESIS_OK;
}

nimcp_neurogenesis_error_t nimcp_neurogenesis_get_neuron_state(
    nimcp_neurogenesis_t ng,
    uint32_t neuron_id,
    nimcp_new_neuron_state_t* state
) {
    if (!ng || !state) return NEUROGENESIS_ERR_NULL_PTR;

    for (uint32_t i = 0; i < ng->num_pending; i++) {
        if (ng->pending_neurons[i].active &&
            ng->pending_neurons[i].neuron_id == neuron_id) {
            pending_neuron_t* n = &ng->pending_neurons[i];
            state->neuron_id = n->neuron_id;
            state->stage = n->stage;
            state->maturity = n->maturity;
            state->activity_level = n->recent_activity;
            state->connections_formed = n->connections_formed;
            state->integration_steps_remaining = n->integration_steps_remaining;
            state->is_active = n->active;
            return NEUROGENESIS_OK;
        }
    }

    return NEUROGENESIS_ERR_NEURON_NOT_FOUND;
}

nimcp_neurogenesis_error_t nimcp_neurogenesis_report_activity(
    nimcp_neurogenesis_t ng,
    uint32_t neuron_id,
    float activity
) {
    if (!ng) return NEUROGENESIS_ERR_NULL_PTR;

    for (uint32_t i = 0; i < ng->num_pending; i++) {
        if (ng->pending_neurons[i].active &&
            ng->pending_neurons[i].neuron_id == neuron_id) {
            ng->pending_neurons[i].activity_accumulator += activity;
            return NEUROGENESIS_OK;
        }
    }

    return NEUROGENESIS_ERR_NEURON_NOT_FOUND;
}

nimcp_neurogenesis_error_t nimcp_neurogenesis_force_mature(
    nimcp_neurogenesis_t ng,
    uint32_t neuron_id
) {
    if (!ng) return NEUROGENESIS_ERR_NULL_PTR;

    for (uint32_t i = 0; i < ng->num_pending; i++) {
        if (ng->pending_neurons[i].active &&
            ng->pending_neurons[i].neuron_id == neuron_id) {
            ng->pending_neurons[i].maturity = 1.0f;
            ng->pending_neurons[i].stage = NEURON_STAGE_MATURE;
            ng->pending_neurons[i].integration_steps_remaining = 0;
            return NEUROGENESIS_OK;
        }
    }

    return NEUROGENESIS_ERR_NEURON_NOT_FOUND;
}

nimcp_neurogenesis_error_t nimcp_neurogenesis_prune_neuron(
    nimcp_neurogenesis_t ng,
    uint32_t neuron_id
) {
    if (!ng) return NEUROGENESIS_ERR_NULL_PTR;

    for (uint32_t i = 0; i < ng->num_pending; i++) {
        if (ng->pending_neurons[i].active &&
            ng->pending_neurons[i].neuron_id == neuron_id) {
            ng->pending_neurons[i].stage = NEURON_STAGE_APOPTOTIC;
            ng->pending_neurons[i].active = false;
            ng->stats.neurons_pruned++;
            return NEUROGENESIS_OK;
        }
    }

    return NEUROGENESIS_ERR_NEURON_NOT_FOUND;
}

//=============================================================================
// Modulation API
//=============================================================================

nimcp_neurogenesis_error_t nimcp_neurogenesis_set_environment(
    nimcp_neurogenesis_t ng,
    float bdnf_level,
    float stress_level,
    float exercise_level,
    float enrichment_level
) {
    if (!ng) return NEUROGENESIS_ERR_NULL_PTR;

    ng->bdnf_level = fmaxf(0.0f, fminf(1.0f, bdnf_level));
    ng->stress_level = fmaxf(0.0f, fminf(1.0f, stress_level));
    ng->exercise_level = fmaxf(0.0f, fminf(1.0f, exercise_level));
    ng->enrichment_level = fmaxf(0.0f, fminf(1.0f, enrichment_level));

    return NEUROGENESIS_OK;
}

nimcp_neurogenesis_error_t nimcp_neurogenesis_get_environment(
    nimcp_neurogenesis_t ng,
    float* modifier
) {
    if (!ng || !modifier) return NEUROGENESIS_ERR_NULL_PTR;
    *modifier = ng->environmental_modifier;
    return NEUROGENESIS_OK;
}

nimcp_neurogenesis_error_t nimcp_neurogenesis_set_global_rate(
    nimcp_neurogenesis_t ng,
    float rate
) {
    if (!ng) return NEUROGENESIS_ERR_NULL_PTR;
    ng->config.base_proliferation_rate = fmaxf(0.0f, fminf(1.0f, rate));
    return NEUROGENESIS_OK;
}

//=============================================================================
// Utility Functions
//=============================================================================

const char* nimcp_neurogenesis_error_string(nimcp_neurogenesis_error_t err) {
    switch (err) {
        case NEUROGENESIS_OK: return "Success";
        case NEUROGENESIS_ERR_NULL_PTR: return "Null pointer";
        case NEUROGENESIS_ERR_INVALID_PARAM: return "Invalid parameter";
        case NEUROGENESIS_ERR_NOT_INITIALIZED: return "Not initialized";
        case NEUROGENESIS_ERR_ALREADY_INITIALIZED: return "Already initialized";
        case NEUROGENESIS_ERR_NO_MEMORY: return "Out of memory";
        case NEUROGENESIS_ERR_NICHE_NOT_FOUND: return "Niche not found";
        case NEUROGENESIS_ERR_NICHE_FULL: return "Niche full";
        case NEUROGENESIS_ERR_NEURON_NOT_FOUND: return "Neuron not found";
        case NEUROGENESIS_ERR_NO_STEM_CELLS: return "No stem cells available";
        default: return "Unknown error";
    }
}
