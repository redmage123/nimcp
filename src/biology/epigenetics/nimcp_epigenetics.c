/**
 * @file nimcp_epigenetics.c
 * @brief Epigenetics Module Implementation
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: Implements epigenetic modifications for neural plasticity
 * WHY:  Enable persistent, experience-dependent neural changes
 * HOW:  Track methylation, histone mods, chromatin state, imprints
 *
 * @author NIMCP Development Team
 */

#include "biology/epigenetics/nimcp_epigenetics.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(epigenetics)

//=============================================================================
// Internal Structures
//=============================================================================

typedef struct {
    nimcp_methylation_site_t site;
    float current_level;
    nimcp_methylation_state_t state;
    bool active;
} methylation_entry_t;

typedef struct {
    nimcp_histone_config_t config;
    float current_magnitude;
    uint64_t creation_time;
    bool active;
} histone_entry_t;

typedef struct {
    nimcp_chromatin_config_t config;
    nimcp_chromatin_state_t current_state;
    float activity_accumulator;
    bool in_critical_period;
    float critical_period_remaining;
    bool active;
} chromatin_entry_t;

typedef struct {
    nimcp_imprint_config_t config;
    float current_strength;
    float remaining_duration;
    uint32_t id;
    bool active;
} imprint_entry_t;

struct nimcp_epigenetics_struct {
    nimcp_epigenetics_config_t config;
    nimcp_brain_t brain;
    bool is_initialized;

    /* Methylation tracking */
    methylation_entry_t* methylations;
    uint32_t num_methylations;
    uint32_t max_methylations;

    /* Histone modifications */
    histone_entry_t* histones;
    uint32_t num_histones;
    uint32_t max_histones;

    /* Chromatin regions */
    chromatin_entry_t* regions;
    uint32_t num_regions;
    uint32_t max_regions;

    /* Imprints */
    imprint_entry_t* imprints;
    uint32_t num_imprints;
    uint32_t max_imprints;
    uint32_t next_imprint_id;

    /* Environmental factors */
    float stress_level;
    float enrichment_level;
    float environmental_modifier;

    /* State and stats */
    nimcp_epigenetics_state_t state;
    nimcp_epigenetics_stats_t stats;
    uint64_t current_time;
};

//=============================================================================
// Helper Functions
//=============================================================================

static nimcp_methylation_state_t level_to_state(float level) {
    if (level < 0.1f) return METHYL_STATE_UNMETHYLATED;
    if (level < 0.4f) return METHYL_STATE_HEMIMETHYLATED;
    if (level < 0.8f) return METHYL_STATE_METHYLATED;
    return METHYL_STATE_HYPERMETHYLATED;
}

static float state_to_plasticity(nimcp_chromatin_state_t state) {
    switch (state) {
        case CHROMATIN_STATE_OPEN: return 1.0f;
        case CHROMATIN_STATE_POISED: return 0.6f;
        case CHROMATIN_STATE_CLOSED: return 0.2f;
        case CHROMATIN_STATE_HETEROCHROMATIN: return 0.0f;
        default: return 0.5f;
    }
}

static void update_methylation(methylation_entry_t* m, float dt, float stability) {
    /* Methylation tends to be stable but can decay slowly */
    float decay_rate = 0.001f * (1.0f - stability);
    m->current_level *= (1.0f - decay_rate * dt);
    m->state = level_to_state(m->current_level);
}

static void update_histone(histone_entry_t* h, float dt) {
    /* Histone modifications decay over time */
    float decay = h->config.decay_rate * dt;
    h->current_magnitude *= expf(-decay);

    /* Remove if below threshold */
    if (h->current_magnitude < 0.01f) {
        h->active = false;
    }
}

static void update_chromatin(chromatin_entry_t* c, float dt, float activity) {
    /* Accumulate activity */
    c->activity_accumulator = c->activity_accumulator * 0.95f + activity * 0.05f;
    if (!isfinite(c->activity_accumulator)) c->activity_accumulator = activity;

    /* Critical period handling */
    if (c->in_critical_period) {
        c->critical_period_remaining -= dt;
        if (c->critical_period_remaining <= 0.0f) {
            c->in_critical_period = false;
            c->critical_period_remaining = 0.0f;
        }
    }

    /* State transitions based on activity */
    if (!c->in_critical_period) return;  /* Only change during critical periods */

    float threshold = c->config.transition_threshold;
    if (c->activity_accumulator > threshold * 1.5f) {
        if (c->current_state < CHROMATIN_STATE_HETEROCHROMATIN) {
            c->current_state = CHROMATIN_STATE_OPEN;
        }
    } else if (c->activity_accumulator < threshold * 0.5f) {
        if (c->current_state == CHROMATIN_STATE_OPEN) {
            c->current_state = CHROMATIN_STATE_POISED;
        } else if (c->current_state == CHROMATIN_STATE_POISED) {
            c->current_state = CHROMATIN_STATE_CLOSED;
        }
    }
}

static void update_imprint(imprint_entry_t* imp, float dt) {
    if (imp->config.duration_ms <= 0.0f) return;  /* Permanent imprint */

    imp->remaining_duration -= dt;
    if (imp->remaining_duration <= 0.0f) {
        imp->active = false;
        imp->remaining_duration = 0.0f;
    } else {
        /* Gradual strength decay */
        float progress = imp->remaining_duration / imp->config.duration_ms;
        imp->current_strength = imp->config.strength * progress;
    }
}

//=============================================================================
// Configuration
//=============================================================================

nimcp_epigenetics_config_t nimcp_epigenetics_default_config(void) {
    nimcp_epigenetics_config_t config = {
        .max_neurons = 1024,
        .max_sites_per_neuron = EPIGENETICS_MAX_SITES,
        .methylation_stability = EPIGENETICS_DEFAULT_STABILITY,
        .histone_decay_rate = 0.01f,
        .plasticity_window_ms = EPIGENETICS_WINDOW_DURATION_MS,
        .enable_environmental = true,
        .enable_inheritance = false,
        .enable_logging = false,
        .enable_metrics = true
    };
    return config;
}

//=============================================================================
// Lifecycle
//=============================================================================

nimcp_epigenetics_t nimcp_epigenetics_create(const nimcp_epigenetics_config_t* config) {
    nimcp_epigenetics_t epi = (nimcp_epigenetics_t)nimcp_calloc(1, sizeof(struct nimcp_epigenetics_struct));
    if (!epi) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(struct nimcp_epigenetics_struct), "Epigenetics allocation failed");
        return NULL;
    }

    epi->config = config ? *config : nimcp_epigenetics_default_config();

    /* Allocate methylation storage */
    epi->max_methylations = epi->config.max_neurons * 4;  /* Avg 4 per neuron */
    epi->methylations = (methylation_entry_t*)nimcp_calloc(epi->max_methylations, sizeof(methylation_entry_t));
    if (!epi->methylations) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, epi->max_methylations * sizeof(methylation_entry_t), "Methylation array allocation failed");
        nimcp_free(epi);
        return NULL;
    }

    /* Allocate histone storage */
    epi->max_histones = EPIGENETICS_MAX_HISTONES * 16;
    epi->histones = (histone_entry_t*)nimcp_calloc(epi->max_histones, sizeof(histone_entry_t));
    if (!epi->histones) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, epi->max_histones * sizeof(histone_entry_t), "Histone array allocation failed");
        nimcp_free(epi->methylations);
        nimcp_free(epi);
        return NULL;
    }

    /* Allocate chromatin regions */
    epi->max_regions = EPIGENETICS_MAX_REGIONS;
    epi->regions = (chromatin_entry_t*)nimcp_calloc(epi->max_regions, sizeof(chromatin_entry_t));
    if (!epi->regions) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, epi->max_regions * sizeof(chromatin_entry_t), "Chromatin regions array allocation failed");
        nimcp_free(epi->histones);
        nimcp_free(epi->methylations);
        nimcp_free(epi);
        return NULL;
    }

    /* Allocate imprints */
    epi->max_imprints = EPIGENETICS_MAX_IMPRINTS;
    epi->imprints = (imprint_entry_t*)nimcp_calloc(epi->max_imprints, sizeof(imprint_entry_t));
    if (!epi->imprints) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, epi->max_imprints * sizeof(imprint_entry_t), "Imprints array allocation failed");
        nimcp_free(epi->regions);
        nimcp_free(epi->histones);
        nimcp_free(epi->methylations);
        nimcp_free(epi);
        return NULL;
    }

    epi->next_imprint_id = 1;
    epi->environmental_modifier = 1.0f;
    epi->state.global_plasticity = 1.0f;

    return epi;
}

void nimcp_epigenetics_destroy(nimcp_epigenetics_t epi) {
    if (!epi) return;

    if (epi->is_initialized) {
        nimcp_epigenetics_shutdown(epi);
    }

    nimcp_free(epi->imprints);
    nimcp_free(epi->regions);
    nimcp_free(epi->histones);
    nimcp_free(epi->methylations);
    nimcp_free(epi);
}

nimcp_epigenetics_error_t nimcp_epigenetics_init(
    nimcp_epigenetics_t epi,
    nimcp_brain_t brain
) {
    if (!epi) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Epigenetics handle is NULL");
        return EPIGENETICS_ERR_NULL_PTR;
    }
    if (epi->is_initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "Epigenetics already initialized");
        return EPIGENETICS_ERR_ALREADY_INITIALIZED;
    }

    epi->brain = brain;
    epi->is_initialized = true;

    return EPIGENETICS_OK;
}

nimcp_epigenetics_error_t nimcp_epigenetics_shutdown(nimcp_epigenetics_t epi) {
    if (!epi) return EPIGENETICS_ERR_NULL_PTR;
    if (!epi->is_initialized) return EPIGENETICS_ERR_NOT_INITIALIZED;

    epi->is_initialized = false;
    epi->brain = NULL;

    return EPIGENETICS_OK;
}

//=============================================================================
// Update
//=============================================================================

nimcp_epigenetics_error_t nimcp_epigenetics_update(nimcp_epigenetics_t epi, float dt) {
    if (!epi) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "Epigenetics handle is NULL in update");
        return EPIGENETICS_ERR_NULL_PTR;
    }

    epi->current_time += (uint64_t)(dt * 1000.0f);

    /* Update methylations */
    uint32_t active_meth = 0;
    float total_meth = 0.0f;
    for (uint32_t i = 0; i < epi->num_methylations; i++) {
        if (epi->methylations[i].active) {
            update_methylation(&epi->methylations[i], dt, epi->config.methylation_stability);
            active_meth++;
            total_meth += epi->methylations[i].current_level;
        }
    }

    /* Update histones */
    uint32_t active_hist = 0;
    for (uint32_t i = 0; i < epi->num_histones; i++) {
        if (epi->histones[i].active) {
            update_histone(&epi->histones[i], dt);
            if (epi->histones[i].active) active_hist++;
        }
    }

    /* Update chromatin regions */
    uint32_t open_regions = 0;
    float activity = 0.5f;  /* Default activity if not connected to brain */
    for (uint32_t i = 0; i < epi->num_regions; i++) {
        if (epi->regions[i].active) {
            update_chromatin(&epi->regions[i], dt, activity);
            if (epi->regions[i].current_state == CHROMATIN_STATE_OPEN) {
                open_regions++;
            }
        }
    }

    /* Update imprints */
    uint32_t active_imp = 0;
    for (uint32_t i = 0; i < epi->num_imprints; i++) {
        if (epi->imprints[i].active) {
            update_imprint(&epi->imprints[i], dt);
            if (epi->imprints[i].active) active_imp++;
        }
    }

    /* Update environmental modifier */
    if (epi->config.enable_environmental) {
        float stress_effect = 1.0f - epi->stress_level * 0.3f;
        float enrichment_effect = 1.0f + epi->enrichment_level * 0.2f;
        epi->environmental_modifier = stress_effect * enrichment_effect;
    }

    /* Update state */
    epi->state.active_methylations = active_meth;
    epi->state.active_histones = active_hist;
    epi->state.open_regions = open_regions;
    epi->state.active_imprints = active_imp;
    epi->state.methylation_load = (active_meth > 0) ? total_meth / active_meth : 0.0f;

    /* Calculate global plasticity */
    float region_plasticity = (epi->num_regions > 0) ?
        (float)open_regions / epi->num_regions : 1.0f;
    float meth_effect = 1.0f - epi->state.methylation_load * 0.5f;
    epi->state.global_plasticity = region_plasticity * meth_effect * epi->environmental_modifier;

    /* Update stats */
    epi->stats.avg_plasticity = epi->stats.avg_plasticity * 0.99f +
                                epi->state.global_plasticity * 0.01f;
    if (!isfinite(epi->stats.avg_plasticity)) epi->stats.avg_plasticity = epi->state.global_plasticity;
    epi->stats.avg_methylation = epi->stats.avg_methylation * 0.99f +
                                 epi->state.methylation_load * 0.01f;
    if (!isfinite(epi->stats.avg_methylation)) epi->stats.avg_methylation = epi->state.methylation_load;

    epi->state.last_update_time = epi->current_time;

    return EPIGENETICS_OK;
}

//=============================================================================
// Plasticity Queries
//=============================================================================

nimcp_epigenetics_error_t nimcp_epigenetics_get_plasticity(
    nimcp_epigenetics_t epi,
    uint32_t neuron_id,
    float* plasticity
) {
    if (!epi || !plasticity) return EPIGENETICS_ERR_NULL_PTR;

    /* Start with global plasticity */
    float p = epi->state.global_plasticity;

    /* Apply neuron-specific methylation */
    for (uint32_t i = 0; i < epi->num_methylations; i++) {
        if (epi->methylations[i].active &&
            epi->methylations[i].site.neuron_id == neuron_id) {
            float meth_effect = 1.0f - epi->methylations[i].current_level * 0.8f;
            p *= meth_effect;
        }
    }

    /* Find which region this neuron belongs to */
    for (uint32_t i = 0; i < epi->num_regions; i++) {
        if (epi->regions[i].active &&
            neuron_id >= epi->regions[i].config.start_neuron &&
            neuron_id <= epi->regions[i].config.end_neuron) {
            float region_p = state_to_plasticity(epi->regions[i].current_state);

            /* Critical period bonus */
            if (epi->regions[i].in_critical_period) {
                region_p = fminf(1.0f, region_p * 1.5f);
            }

            p *= region_p;
            break;
        }
    }

    *plasticity = fmaxf(0.0f, fminf(1.0f, p));
    return EPIGENETICS_OK;
}

nimcp_epigenetics_error_t nimcp_epigenetics_is_critical_period(
    nimcp_epigenetics_t epi,
    uint32_t region_id,
    bool* is_critical
) {
    if (!epi || !is_critical) return EPIGENETICS_ERR_NULL_PTR;

    for (uint32_t i = 0; i < epi->num_regions; i++) {
        if (epi->regions[i].active && epi->regions[i].config.region_id == region_id) {
            *is_critical = epi->regions[i].in_critical_period;
            return EPIGENETICS_OK;
        }
    }

    *is_critical = false;
    return EPIGENETICS_ERR_SITE_NOT_FOUND;
}

//=============================================================================
// State and Stats
//=============================================================================

nimcp_epigenetics_error_t nimcp_epigenetics_get_state(
    nimcp_epigenetics_t epi,
    nimcp_epigenetics_state_t* state
) {
    if (!epi || !state) return EPIGENETICS_ERR_NULL_PTR;
    *state = epi->state;
    return EPIGENETICS_OK;
}

nimcp_epigenetics_error_t nimcp_epigenetics_get_stats(
    nimcp_epigenetics_t epi,
    nimcp_epigenetics_stats_t* stats
) {
    if (!epi || !stats) return EPIGENETICS_ERR_NULL_PTR;
    *stats = epi->stats;
    return EPIGENETICS_OK;
}

nimcp_epigenetics_error_t nimcp_epigenetics_reset_stats(nimcp_epigenetics_t epi) {
    if (!epi) return EPIGENETICS_ERR_NULL_PTR;
    memset(&epi->stats, 0, sizeof(epi->stats));
    return EPIGENETICS_OK;
}

//=============================================================================
// Methylation API
//=============================================================================

nimcp_epigenetics_error_t nimcp_epigenetics_add_methylation(
    nimcp_epigenetics_t epi,
    const nimcp_methylation_site_t* site
) {
    if (!epi || !site) return EPIGENETICS_ERR_NULL_PTR;
    if (epi->num_methylations >= epi->max_methylations) return EPIGENETICS_ERR_REGION_FULL;

    /* Find free slot */
    uint32_t slot = epi->max_methylations;
    for (uint32_t i = 0; i < epi->max_methylations; i++) {
        if (!epi->methylations[i].active) {
            slot = i;
            break;
        }
    }
    if (slot >= epi->max_methylations) return EPIGENETICS_ERR_REGION_FULL;

    epi->methylations[slot].site = *site;
    epi->methylations[slot].current_level = site->initial_level;
    epi->methylations[slot].state = level_to_state(site->initial_level);
    epi->methylations[slot].active = true;

    if (slot >= epi->num_methylations) {
        epi->num_methylations = slot + 1;
    }

    epi->stats.methylations_added++;

    return EPIGENETICS_OK;
}

nimcp_epigenetics_error_t nimcp_epigenetics_remove_methylation(
    nimcp_epigenetics_t epi,
    uint32_t site_id
) {
    if (!epi) return EPIGENETICS_ERR_NULL_PTR;

    for (uint32_t i = 0; i < epi->num_methylations; i++) {
        if (epi->methylations[i].active && epi->methylations[i].site.site_id == site_id) {
            epi->methylations[i].active = false;
            epi->stats.methylations_removed++;
            return EPIGENETICS_OK;
        }
    }

    return EPIGENETICS_ERR_SITE_NOT_FOUND;
}

nimcp_epigenetics_error_t nimcp_epigenetics_get_methylation(
    nimcp_epigenetics_t epi,
    uint32_t neuron_id,
    float* methylation_level,
    nimcp_methylation_state_t* state
) {
    if (!epi) return EPIGENETICS_ERR_NULL_PTR;

    float total = 0.0f;
    uint32_t count = 0;
    nimcp_methylation_state_t max_state = METHYL_STATE_UNMETHYLATED;

    for (uint32_t i = 0; i < epi->num_methylations; i++) {
        if (epi->methylations[i].active &&
            epi->methylations[i].site.neuron_id == neuron_id) {
            total += epi->methylations[i].current_level;
            count++;
            if (epi->methylations[i].state > max_state) {
                max_state = epi->methylations[i].state;
            }
        }
    }

    if (methylation_level) *methylation_level = (count > 0) ? total / count : 0.0f;
    if (state) *state = max_state;

    return EPIGENETICS_OK;
}

nimcp_epigenetics_error_t nimcp_epigenetics_methylate_synapse(
    nimcp_epigenetics_t epi,
    uint32_t synapse_id,
    float level
) {
    if (!epi) return EPIGENETICS_ERR_NULL_PTR;

    /* Check if synapse already has methylation */
    for (uint32_t i = 0; i < epi->num_methylations; i++) {
        if (epi->methylations[i].active &&
            epi->methylations[i].site.synapse_id == synapse_id) {
            epi->methylations[i].current_level = fmaxf(epi->methylations[i].current_level, level);
            epi->methylations[i].state = level_to_state(epi->methylations[i].current_level);
            return EPIGENETICS_OK;
        }
    }

    /* Create new methylation site */
    nimcp_methylation_site_t site = {
        .site_id = epi->num_methylations + 1,
        .neuron_id = 0,  /* Unknown */
        .synapse_id = synapse_id,
        .initial_level = level,
        .stability = epi->config.methylation_stability
    };

    return nimcp_epigenetics_add_methylation(epi, &site);
}

nimcp_epigenetics_error_t nimcp_epigenetics_demethylate_synapse(
    nimcp_epigenetics_t epi,
    uint32_t synapse_id
) {
    if (!epi) return EPIGENETICS_ERR_NULL_PTR;

    for (uint32_t i = 0; i < epi->num_methylations; i++) {
        if (epi->methylations[i].active &&
            epi->methylations[i].site.synapse_id == synapse_id) {
            epi->methylations[i].current_level = 0.0f;
            epi->methylations[i].state = METHYL_STATE_UNMETHYLATED;
            epi->stats.methylations_removed++;
            return EPIGENETICS_OK;
        }
    }

    return EPIGENETICS_ERR_SITE_NOT_FOUND;
}

//=============================================================================
// Histone Modification API
//=============================================================================

nimcp_epigenetics_error_t nimcp_epigenetics_modify_histone(
    nimcp_epigenetics_t epi,
    const nimcp_histone_config_t* config
) {
    if (!epi || !config) return EPIGENETICS_ERR_NULL_PTR;

    /* Find free slot */
    uint32_t slot = epi->max_histones;
    for (uint32_t i = 0; i < epi->max_histones; i++) {
        if (!epi->histones[i].active) {
            slot = i;
            break;
        }
    }
    if (slot >= epi->max_histones) return EPIGENETICS_ERR_REGION_FULL;

    epi->histones[slot].config = *config;
    epi->histones[slot].current_magnitude = config->magnitude;
    epi->histones[slot].creation_time = epi->current_time;
    epi->histones[slot].active = true;

    if (slot >= epi->num_histones) {
        epi->num_histones = slot + 1;
    }

    epi->stats.histone_modifications++;

    return EPIGENETICS_OK;
}

nimcp_epigenetics_error_t nimcp_epigenetics_get_histone_state(
    nimcp_epigenetics_t epi,
    uint32_t region_id,
    float* acetylation_level,
    float* methylation_level
) {
    if (!epi) return EPIGENETICS_ERR_NULL_PTR;

    float acet = 0.0f;
    float meth = 0.0f;

    for (uint32_t i = 0; i < epi->num_histones; i++) {
        if (epi->histones[i].active && epi->histones[i].config.region_id == region_id) {
            switch (epi->histones[i].config.type) {
                case HISTONE_MOD_ACETYLATION:
                    acet += epi->histones[i].current_magnitude;
                    break;
                case HISTONE_MOD_DEACETYLATION:
                    acet -= epi->histones[i].current_magnitude;
                    break;
                case HISTONE_MOD_METHYLATION:
                    meth += epi->histones[i].current_magnitude;
                    break;
                default:
                    break;
            }
        }
    }

    if (acetylation_level) *acetylation_level = fmaxf(0.0f, acet);
    if (methylation_level) *methylation_level = fmaxf(0.0f, meth);

    return EPIGENETICS_OK;
}

nimcp_epigenetics_error_t nimcp_epigenetics_clear_histones(
    nimcp_epigenetics_t epi,
    uint32_t region_id
) {
    if (!epi) return EPIGENETICS_ERR_NULL_PTR;

    for (uint32_t i = 0; i < epi->num_histones; i++) {
        if (epi->histones[i].active && epi->histones[i].config.region_id == region_id) {
            epi->histones[i].active = false;
        }
    }

    return EPIGENETICS_OK;
}

//=============================================================================
// Chromatin State API
//=============================================================================

nimcp_epigenetics_error_t nimcp_epigenetics_configure_region(
    nimcp_epigenetics_t epi,
    const nimcp_chromatin_config_t* config
) {
    if (!epi || !config) return EPIGENETICS_ERR_NULL_PTR;

    /* Check if region exists */
    for (uint32_t i = 0; i < epi->num_regions; i++) {
        if (epi->regions[i].active && epi->regions[i].config.region_id == config->region_id) {
            epi->regions[i].config = *config;
            return EPIGENETICS_OK;
        }
    }

    /* Create new region */
    if (epi->num_regions >= epi->max_regions) return EPIGENETICS_ERR_REGION_FULL;

    uint32_t slot = epi->num_regions;
    epi->regions[slot].config = *config;
    epi->regions[slot].current_state = config->initial_state;
    epi->regions[slot].activity_accumulator = 0.0f;
    epi->regions[slot].in_critical_period = false;
    epi->regions[slot].critical_period_remaining = 0.0f;
    epi->regions[slot].active = true;

    epi->num_regions++;

    return EPIGENETICS_OK;
}

nimcp_epigenetics_error_t nimcp_epigenetics_get_chromatin_state(
    nimcp_epigenetics_t epi,
    uint32_t region_id,
    nimcp_chromatin_state_t* state
) {
    if (!epi || !state) return EPIGENETICS_ERR_NULL_PTR;

    for (uint32_t i = 0; i < epi->num_regions; i++) {
        if (epi->regions[i].active && epi->regions[i].config.region_id == region_id) {
            *state = epi->regions[i].current_state;
            return EPIGENETICS_OK;
        }
    }

    return EPIGENETICS_ERR_SITE_NOT_FOUND;
}

nimcp_epigenetics_error_t nimcp_epigenetics_set_chromatin_state(
    nimcp_epigenetics_t epi,
    uint32_t region_id,
    nimcp_chromatin_state_t state
) {
    if (!epi) return EPIGENETICS_ERR_NULL_PTR;

    for (uint32_t i = 0; i < epi->num_regions; i++) {
        if (epi->regions[i].active && epi->regions[i].config.region_id == region_id) {
            epi->regions[i].current_state = state;
            epi->stats.chromatin_transitions++;
            return EPIGENETICS_OK;
        }
    }

    return EPIGENETICS_ERR_SITE_NOT_FOUND;
}

nimcp_epigenetics_error_t nimcp_epigenetics_open_region(
    nimcp_epigenetics_t epi,
    uint32_t region_id
) {
    return nimcp_epigenetics_set_chromatin_state(epi, region_id, CHROMATIN_STATE_OPEN);
}

nimcp_epigenetics_error_t nimcp_epigenetics_close_region(
    nimcp_epigenetics_t epi,
    uint32_t region_id
) {
    return nimcp_epigenetics_set_chromatin_state(epi, region_id, CHROMATIN_STATE_CLOSED);
}

//=============================================================================
// Imprinting API
//=============================================================================

nimcp_epigenetics_error_t nimcp_epigenetics_create_imprint(
    nimcp_epigenetics_t epi,
    const nimcp_imprint_config_t* config,
    uint32_t* imprint_id
) {
    if (!epi || !config) return EPIGENETICS_ERR_NULL_PTR;

    /* Find free slot */
    uint32_t slot = epi->max_imprints;
    for (uint32_t i = 0; i < epi->max_imprints; i++) {
        if (!epi->imprints[i].active) {
            slot = i;
            break;
        }
    }
    if (slot >= epi->max_imprints) return EPIGENETICS_ERR_REGION_FULL;

    epi->imprints[slot].config = *config;
    epi->imprints[slot].config.trigger_time = epi->current_time;
    epi->imprints[slot].current_strength = config->strength;
    epi->imprints[slot].remaining_duration = config->duration_ms;
    epi->imprints[slot].id = epi->next_imprint_id++;
    epi->imprints[slot].active = true;

    if (slot >= epi->num_imprints) {
        epi->num_imprints = slot + 1;
    }

    if (imprint_id) *imprint_id = epi->imprints[slot].id;

    epi->stats.imprinting_events++;

    return EPIGENETICS_OK;
}

nimcp_epigenetics_error_t nimcp_epigenetics_get_imprint(
    nimcp_epigenetics_t epi,
    uint32_t imprint_id,
    float* strength,
    float* remaining_duration
) {
    if (!epi) return EPIGENETICS_ERR_NULL_PTR;

    for (uint32_t i = 0; i < epi->num_imprints; i++) {
        if (epi->imprints[i].active && epi->imprints[i].id == imprint_id) {
            if (strength) *strength = epi->imprints[i].current_strength;
            if (remaining_duration) *remaining_duration = epi->imprints[i].remaining_duration;
            return EPIGENETICS_OK;
        }
    }

    return EPIGENETICS_ERR_SITE_NOT_FOUND;
}

nimcp_epigenetics_error_t nimcp_epigenetics_remove_imprint(
    nimcp_epigenetics_t epi,
    uint32_t imprint_id
) {
    if (!epi) return EPIGENETICS_ERR_NULL_PTR;

    for (uint32_t i = 0; i < epi->num_imprints; i++) {
        if (epi->imprints[i].active && epi->imprints[i].id == imprint_id) {
            epi->imprints[i].active = false;
            return EPIGENETICS_OK;
        }
    }

    return EPIGENETICS_ERR_SITE_NOT_FOUND;
}

nimcp_epigenetics_error_t nimcp_epigenetics_start_critical_period(
    nimcp_epigenetics_t epi,
    uint32_t region_id,
    float duration_ms
) {
    if (!epi) return EPIGENETICS_ERR_NULL_PTR;

    for (uint32_t i = 0; i < epi->num_regions; i++) {
        if (epi->regions[i].active && epi->regions[i].config.region_id == region_id) {
            epi->regions[i].in_critical_period = true;
            epi->regions[i].critical_period_remaining = duration_ms;
            epi->state.is_critical_period = true;
            epi->stats.critical_period_time += duration_ms;
            return EPIGENETICS_OK;
        }
    }

    return EPIGENETICS_ERR_SITE_NOT_FOUND;
}

nimcp_epigenetics_error_t nimcp_epigenetics_end_critical_period(
    nimcp_epigenetics_t epi,
    uint32_t region_id
) {
    if (!epi) return EPIGENETICS_ERR_NULL_PTR;

    for (uint32_t i = 0; i < epi->num_regions; i++) {
        if (epi->regions[i].active && epi->regions[i].config.region_id == region_id) {
            epi->regions[i].in_critical_period = false;
            epi->regions[i].critical_period_remaining = 0.0f;
            return EPIGENETICS_OK;
        }
    }

    return EPIGENETICS_ERR_SITE_NOT_FOUND;
}

//=============================================================================
// Environmental Factors
//=============================================================================

nimcp_epigenetics_error_t nimcp_epigenetics_apply_environment(
    nimcp_epigenetics_t epi,
    float stress_level,
    float enrichment_level
) {
    if (!epi) return EPIGENETICS_ERR_NULL_PTR;

    epi->stress_level = fmaxf(0.0f, fminf(1.0f, stress_level));
    epi->enrichment_level = fmaxf(0.0f, fminf(1.0f, enrichment_level));

    return EPIGENETICS_OK;
}

nimcp_epigenetics_error_t nimcp_epigenetics_get_environment_effect(
    nimcp_epigenetics_t epi,
    float* plasticity_modifier
) {
    if (!epi || !plasticity_modifier) return EPIGENETICS_ERR_NULL_PTR;

    *plasticity_modifier = epi->environmental_modifier;
    return EPIGENETICS_OK;
}

//=============================================================================
// Utility Functions
//=============================================================================

const char* nimcp_epigenetics_error_string(nimcp_epigenetics_error_t err) {
    switch (err) {
        case EPIGENETICS_OK: return "Success";
        case EPIGENETICS_ERR_NULL_PTR: return "Null pointer";
        case EPIGENETICS_ERR_INVALID_PARAM: return "Invalid parameter";
        case EPIGENETICS_ERR_NOT_INITIALIZED: return "Not initialized";
        case EPIGENETICS_ERR_ALREADY_INITIALIZED: return "Already initialized";
        case EPIGENETICS_ERR_NO_MEMORY: return "Out of memory";
        case EPIGENETICS_ERR_SITE_NOT_FOUND: return "Site not found";
        case EPIGENETICS_ERR_REGION_FULL: return "Region full";
        case EPIGENETICS_ERR_WINDOW_CLOSED: return "Window closed";
        default: return "Unknown error";
    }
}
