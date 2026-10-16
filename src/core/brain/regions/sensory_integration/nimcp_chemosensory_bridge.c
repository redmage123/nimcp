/**
 * @file nimcp_chemosensory_bridge.c
 * @brief Chemosensory Integration Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-12
 *
 * @author NIMCP Development Team
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "core/brain/regions/sensory_integration/nimcp_chemosensory_bridge.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdatomic.h>

//=============================================================================
#include <stddef.h>
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/thread/nimcp_thread_rand.h"

BRIDGE_BOILERPLATE_MESH_ONLY(chemosensory_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define LOG_MODULE "CHEMOSENSORY_BRIDGE"


/* ============================================================================
 * Internal Structure
 * ============================================================================ */

struct chemosensory_bridge_struct {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    chemosensory_config_t config;

    nimcp_olfactory_t* olfact;
    nimcp_gustatory_t* gust;

    bool is_connected;
    chemosensory_status_t status;

    /* Current binding state */
    chemosensory_flavor_t current_flavor;
    uint64_t last_odor_time;
    uint64_t last_taste_time;

    chemosensory_stats_t stats;
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static uint64_t get_timestamp(void) {
    static _Atomic uint64_t counter = 0;
    return counter++;
}

static float randf(void) {
    return (float)nimcp_tl_rand() / (float)RAND_MAX;
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int chemosensory_default_config(chemosensory_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "chemosensory_default_config: config is NULL");
        return -1;
    }

    memset(config, 0, sizeof(chemosensory_config_t));

    config->flavor_dim = CHEMOSENSORY_FLAVOR_DIM;
    config->binding_window_ms = CHEMOSENSORY_BINDING_WINDOW_MS;
    config->congruence_threshold = CHEMOSENSORY_CONGRUENCE_THRESHOLD;
    config->binding_decay_rate = 0.95f;
    config->enable_predictions = true;
    config->enable_memory_associations = true;
    config->enable_logging = false;

    return 0;
}

chemosensory_bridge_t* chemosensory_bridge_create(const chemosensory_config_t* config) {
    chemosensory_bridge_t* bridge = (chemosensory_bridge_t*)nimcp_calloc(1, sizeof(chemosensory_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "chemosensory_bridge_create: allocation failed");
        return NULL;
    }

    bridge_base_init(&bridge->base, 0, "chemosensory");

    if (config) {
        memcpy(&bridge->config, config, sizeof(chemosensory_config_t));
    } else {
        chemosensory_default_config(&bridge->config);
    }

    bridge->current_flavor.flavor_profile = (float*)nimcp_calloc(bridge->config.flavor_dim, sizeof(float));
    if (!bridge->current_flavor.flavor_profile) {
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "chemosensory_bridge_create: flavor_profile allocation failed");
        return NULL;
    }
    bridge->current_flavor.profile_dim = bridge->config.flavor_dim;

    bridge->is_connected = false;
    bridge->status = CHEMOSENSORY_STATUS_IDLE;

    NIMCP_LOGGING_INFO("Created %s bridge", "chemosensory");
    return bridge;
}

void chemosensory_bridge_destroy(chemosensory_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "chemosensory");
    nimcp_free(bridge->current_flavor.flavor_profile);
    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

/* ============================================================================
 * Connection Implementation
 * ============================================================================ */

int chemosensory_connect(chemosensory_bridge_t* bridge, nimcp_olfactory_t* olfact, nimcp_gustatory_t* gust) {
    if (!bridge || !olfact || !gust) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "chemosensory_connect: required parameter is NULL (bridge, olfact, gust)");
        return -1;
    }

    bridge->olfact = olfact;
    bridge->gust = gust;
    bridge->is_connected = true;
    bridge->status = CHEMOSENSORY_STATUS_IDLE;

    return 0;
}

int chemosensory_disconnect(chemosensory_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "chemosensory_disconnect: bridge is NULL");
        return -1;
    }

    bridge->olfact = NULL;
    bridge->gust = NULL;
    bridge->is_connected = false;

    return 0;
}

bool chemosensory_is_connected(const chemosensory_bridge_t* bridge) {
    return bridge && bridge->is_connected;
}

/* ============================================================================
 * Binding Implementation
 * ============================================================================ */

int chemosensory_bind_flavor(chemosensory_bridge_t* bridge, const odor_perception_t* odor,
                             const taste_perception_t* taste, chemosensory_flavor_t* flavor) {
    if (!bridge || !odor || !taste || !flavor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "chemosensory_is_connected: required parameter is NULL (bridge, odor, taste, flavor)");
        return -1;
    }

    bridge->status = CHEMOSENSORY_STATUS_BINDING;

    /* Initialize flavor */
    if (!flavor->flavor_profile) {
        flavor->flavor_profile = (float*)nimcp_calloc(bridge->config.flavor_dim, sizeof(float));
        if (!flavor->flavor_profile) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "chemosensory_is_connected: flavor->flavor_profile is NULL");
            return -1;
        }
    }
    flavor->profile_dim = bridge->config.flavor_dim;

    /* Combine taste and smell into flavor profile */
    /* First part from taste (5 basic tastes) */
    flavor->flavor_profile[0] = taste->perceived_sweet;
    flavor->flavor_profile[1] = taste->perceived_salty;
    flavor->flavor_profile[2] = taste->perceived_sour;
    flavor->flavor_profile[3] = taste->perceived_bitter;
    flavor->flavor_profile[4] = taste->perceived_umami;

    /* Rest from odor profile */
    if (odor->pattern && odor->pattern_dim > 0) {
        uint32_t copy_len = (odor->pattern_dim < bridge->config.flavor_dim - 5) ?
                           odor->pattern_dim : bridge->config.flavor_dim - 5;
        memcpy(&flavor->flavor_profile[5], odor->pattern, copy_len * sizeof(float));
    }

    /* Compute contributions */
    flavor->taste_contribution = taste->overall_intensity;
    flavor->smell_contribution = odor->intensity;

    /* Normalize contributions */
    float total = flavor->taste_contribution + flavor->smell_contribution;
    if (total > 0.0f) {
        flavor->taste_contribution /= total;
        flavor->smell_contribution /= total;
    }

    /* Evaluate congruence */
    float congruence_score = 0.0f;
    chemosensory_evaluate_congruence(bridge, odor, taste, &flavor->congruence, &congruence_score);

    flavor->binding_strength = congruence_score * 0.5f + 0.5f;

    /* Compute palatability */
    float taste_palatability = taste->palatability;
    float odor_hedonic = (odor->valence == HEDONIC_VERY_PLEASANT) ? 1.0f :
                        (odor->valence == HEDONIC_PLEASANT) ? 0.75f :
                        (odor->valence == HEDONIC_NEUTRAL) ? 0.5f :
                        (odor->valence == HEDONIC_UNPLEASANT) ? 0.25f : 0.0f;

    flavor->palatability = (taste_palatability + odor_hedonic) / 2.0f * flavor->binding_strength;
    flavor->familiarity = odor->familiarity * 0.5f + taste->identification_confidence * 0.5f;

    flavor->onset_time = get_timestamp();
    flavor->binding_latency_ms = 50;  /* Simulated latency */

    /* Copy to bridge state — free previous flavor_profile to avoid leak */
    if (bridge->current_flavor.flavor_profile) {
        nimcp_free(bridge->current_flavor.flavor_profile);
    }
    memcpy(&bridge->current_flavor, flavor, sizeof(chemosensory_flavor_t));
    bridge->current_flavor.flavor_profile = (float*)nimcp_calloc(flavor->profile_dim, sizeof(float));
    if (!bridge->current_flavor.flavor_profile) {
        bridge->status = CHEMOSENSORY_STATUS_IDLE;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "chemosensory_bind_flavor: flavor_profile allocation failed");
        return -1;
    }
    memcpy(bridge->current_flavor.flavor_profile, flavor->flavor_profile, flavor->profile_dim * sizeof(float));

    bridge->status = CHEMOSENSORY_STATUS_IDLE;
    bridge->stats.flavors_bound++;
    {
        float new_cong = bridge->stats.avg_congruence * 0.9f + congruence_score * 0.1f;
        if (isfinite(new_cong)) bridge->stats.avg_congruence = new_cong;
        float new_bind = bridge->stats.avg_binding_strength * 0.9f + flavor->binding_strength * 0.1f;
        if (isfinite(new_bind)) bridge->stats.avg_binding_strength = new_bind;
        float new_pal = bridge->stats.avg_palatability * 0.9f + flavor->palatability * 0.1f;
        if (isfinite(new_pal)) bridge->stats.avg_palatability = new_pal;
    }

    return 0;
}

int chemosensory_update_binding(chemosensory_bridge_t* bridge, float dt) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "chemosensory_update_binding: bridge is NULL");
        return -1;
    }
    (void)dt;

    /* Decay binding strength over time */
    bridge->current_flavor.binding_strength *= bridge->config.binding_decay_rate;

    return 0;
}

int chemosensory_get_current_flavor(chemosensory_bridge_t* bridge, chemosensory_flavor_t* flavor) {
    if (!bridge || !flavor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "chemosensory_get_current_flavor: required parameter is NULL (bridge, flavor)");
        return -1;
    }

    memcpy(flavor, &bridge->current_flavor, sizeof(chemosensory_flavor_t));

    /* Need to copy the profile separately */
    if (bridge->current_flavor.flavor_profile) {
        flavor->flavor_profile = (float*)nimcp_calloc(bridge->current_flavor.profile_dim, sizeof(float));
        if (flavor->flavor_profile) {
            memcpy(flavor->flavor_profile, bridge->current_flavor.flavor_profile,
                   bridge->current_flavor.profile_dim * sizeof(float));
        }
    }

    return 0;
}

/* ============================================================================
 * Prediction Implementation
 * ============================================================================ */

int chemosensory_predict_taste_from_smell(chemosensory_bridge_t* bridge,
                                          const odor_perception_t* odor,
                                          float* predicted_taste) {
    if (!bridge || !odor || !predicted_taste) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "chemosensory_get_current_flavor: required parameter is NULL (bridge, odor, predicted_taste)");
        return -1;
    }
    if (!bridge->config.enable_predictions) {
        return 0;  /* Feature disabled, not an error */
    }

    /* Simple prediction based on odor category */
    predicted_taste[0] = 0.3f;  /* Sweet - default moderate */
    predicted_taste[1] = 0.2f;  /* Salty */
    predicted_taste[2] = 0.2f;  /* Sour */
    predicted_taste[3] = 0.1f;  /* Bitter */
    predicted_taste[4] = 0.2f;  /* Umami */

    /* Adjust based on odor category */
    switch (odor->category) {
        case ODOR_CAT_FRUITY:
            predicted_taste[0] = 0.7f;  /* More sweet */
            predicted_taste[2] = 0.4f;  /* More sour */
            break;
        case ODOR_CAT_SPICY:
            predicted_taste[3] = 0.3f;  /* More bitter */
            break;
        case ODOR_CAT_CHEMICAL:
            predicted_taste[3] = 0.6f;  /* More bitter */
            break;
        default:
            break;
    }

    bridge->stats.predictions_made++;

    return 0;
}

int chemosensory_predict_smell_from_taste(chemosensory_bridge_t* bridge,
                                          const taste_perception_t* taste,
                                          float* predicted_smell) {
    if (!bridge || !taste || !predicted_smell) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "chemosensory_get_current_flavor: required parameter is NULL (bridge, taste, predicted_smell)");
        return -1;
    }
    if (!bridge->config.enable_predictions) {
        return 0;  /* Feature disabled, not an error */
    }

    /* Simple prediction based on dominant taste */
    memset(predicted_smell, 0, 32 * sizeof(float));

    if (taste->perceived_sweet > 0.5f) {
        predicted_smell[0] = 0.6f;  /* Fruity */
        predicted_smell[1] = 0.4f;  /* Floral */
    }
    if (taste->perceived_umami > 0.5f) {
        predicted_smell[5] = 0.5f;  /* Savory */
    }
    if (taste->perceived_bitter > 0.3f) {
        predicted_smell[10] = 0.4f;  /* Chemical/medicinal */
    }

    bridge->stats.predictions_made++;

    return 0;
}

int chemosensory_compute_prediction_error(chemosensory_bridge_t* bridge,
                                          const chemosensory_prediction_t* prediction,
                                          float* error) {
    if (!bridge || !prediction || !error) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "chemosensory_get_current_flavor: required parameter is NULL (bridge, prediction, error)");
        return -1;
    }
    (void)prediction;

    *error = randf() * 0.3f;  /* Placeholder */

    return 0;
}

/* ============================================================================
 * Congruence Implementation
 * ============================================================================ */

int chemosensory_evaluate_congruence(chemosensory_bridge_t* bridge,
                                     const odor_perception_t* odor,
                                     const taste_perception_t* taste,
                                     chemosensory_congruence_t* congruence,
                                     float* score) {
    if (!bridge || !odor || !taste || !congruence || !score) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "chemosensory_get_current_flavor: required parameter is NULL (bridge, odor, taste, congruence, score)");
        return -1;
    }

    /* Compute congruence based on typical taste-smell pairings */
    float congruence_score = 0.5f;  /* Base neutral */

    /* Sweet taste + fruity/floral odor = high congruence */
    if (taste->perceived_sweet > 0.5f &&
        (odor->category == ODOR_CAT_FRUITY || odor->category == ODOR_CAT_FLORAL)) {
        congruence_score += 0.3f;
    }

    /* Umami taste + savory odor = high congruence */
    if (taste->perceived_umami > 0.5f && odor->category == ODOR_CAT_SAVORY) {
        congruence_score += 0.2f;
    }

    /* Bitter taste + chemical odor = expected but unpleasant */
    if (taste->perceived_bitter > 0.5f && odor->category == ODOR_CAT_CHEMICAL) {
        congruence_score += 0.1f;
    }

    /* Mismatches reduce congruence */
    if (taste->perceived_sweet > 0.5f && odor->category == ODOR_CAT_DECAYED) {
        congruence_score -= 0.4f;
    }

    /* Clamp to [0, 1] */
    if (congruence_score < 0.0f) congruence_score = 0.0f;
    if (congruence_score > 1.0f) congruence_score = 1.0f;

    *score = congruence_score;

    /* Map to category */
    if (congruence_score >= 0.9f) {
        *congruence = CHEMOSENSORY_CONGRUENCE_PERFECT;
    } else if (congruence_score >= 0.7f) {
        *congruence = CHEMOSENSORY_CONGRUENCE_HIGH;
    } else if (congruence_score >= 0.5f) {
        *congruence = CHEMOSENSORY_CONGRUENCE_MEDIUM;
    } else if (congruence_score >= 0.3f) {
        *congruence = CHEMOSENSORY_CONGRUENCE_LOW;
    } else {
        *congruence = CHEMOSENSORY_CONGRUENCE_NONE;
    }

    return 0;
}

int chemosensory_get_congruent_pairs(chemosensory_bridge_t* bridge, uint32_t* pairs,
                                     uint32_t* num_pairs, uint32_t max_pairs) {
    if (!bridge || !pairs || !num_pairs) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "chemosensory_get_current_flavor: required parameter is NULL (bridge, pairs, num_pairs)");
        return -1;
    }

    /* Return known congruent pairs (encoded as taste_id << 16 | odor_id) */
    uint32_t idx = 0;

    if (idx < max_pairs) pairs[idx++] = (TASTE_SWEET << 16) | ODOR_CAT_FRUITY;
    if (idx < max_pairs) pairs[idx++] = (TASTE_SWEET << 16) | ODOR_CAT_FLORAL;
    if (idx < max_pairs) pairs[idx++] = (TASTE_UMAMI << 16) | ODOR_CAT_SAVORY;

    *num_pairs = idx;

    return 0;
}

/* ============================================================================
 * Memory Implementation
 * ============================================================================ */

int chemosensory_trigger_memory(chemosensory_bridge_t* bridge,
                                const chemosensory_flavor_t* flavor,
                                chemosensory_memory_t* memory) {
    if (!bridge || !flavor || !memory) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "chemosensory_trigger_memory: required parameter is NULL");
        return -1;
    }
    if (!bridge->config.enable_memory_associations) {
        return 0;  /* Feature disabled, not an error */
    }

    memory->memory_id = (uint32_t)(randf() * 1000);
    strncpy(memory->food_name, flavor->flavor_name, sizeof(memory->food_name) - 1);
    memory->association_strength = flavor->familiarity;
    memory->emotional_valence = flavor->palatability * 2.0f - 1.0f;  /* Map to [-1, 1] */
    memory->is_learned = true;

    bridge->stats.memories_triggered++;

    return 0;
}

int chemosensory_learn_association(chemosensory_bridge_t* bridge,
                                   const chemosensory_flavor_t* flavor,
                                   const char* food_name,
                                   float valence) {
    if (!bridge || !flavor || !food_name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "chemosensory_get_current_flavor: required parameter is NULL (bridge, flavor, food_name)");
        return -1;
    }
    if (!bridge->config.enable_memory_associations) {
        return 0;  /* Feature disabled, not an error */
    }
    (void)valence;

    /* Store association - in full implementation would update memory */

    return 0;
}

/* ============================================================================
 * Statistics Implementation
 * ============================================================================ */

int chemosensory_get_stats(const chemosensory_bridge_t* bridge, chemosensory_stats_t* stats) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "chemosensory_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    memcpy(stats, &bridge->stats, sizeof(chemosensory_stats_t));
    return 0;
}

int chemosensory_reset_stats(chemosensory_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "chemosensory_reset_stats: bridge is NULL");
        return -1;
    }
    memset(&bridge->stats, 0, sizeof(chemosensory_stats_t));
    return 0;
}

void chemosensory_print_summary(const chemosensory_bridge_t* bridge) {
    if (!bridge) return;

    printf("=== Chemosensory Bridge Summary ===\n");
    printf("Connected: %s\n", bridge->is_connected ? "Yes" : "No");
    printf("Flavors Bound: %lu\n", (unsigned long)bridge->stats.flavors_bound);
    printf("Predictions: %lu\n", (unsigned long)bridge->stats.predictions_made);
    printf("Memories Triggered: %lu\n", (unsigned long)bridge->stats.memories_triggered);
    printf("Avg Congruence: %.2f\n", bridge->stats.avg_congruence);
    printf("Avg Binding Strength: %.2f\n", bridge->stats.avg_binding_strength);
    printf("Avg Palatability: %.2f\n", bridge->stats.avg_palatability);
    printf("===================================\n");
}

/* ============================================================================
 * Cleanup
 * ============================================================================ */

void chemosensory_flavor_free(chemosensory_flavor_t* flavor) {
    if (!flavor) return;
    nimcp_free(flavor->flavor_profile);
    flavor->flavor_profile = NULL;
}

void chemosensory_prediction_free(chemosensory_prediction_t* prediction) {
    if (!prediction) return;
    nimcp_free(prediction->predicted_taste);
    nimcp_free(prediction->predicted_smell);
    prediction->predicted_taste = NULL;
    prediction->predicted_smell = NULL;
}
