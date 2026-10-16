/**
 * @file nimcp_gustatory.c
 * @brief Gustatory Cortex Implementation
 * @version Phase 6: Sensory Processing
 * @date 2026-01-12
 */

#include "core/brain/regions/gustatory/nimcp_gustatory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdatomic.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_learning_constants.h"
#include "constants/nimcp_threshold_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(gustatory, MESH_ADAPTER_CATEGORY_COGNITIVE)


static uint64_t gust_get_time_ms(void) {
    static _Atomic uint64_t counter = 0;
    return counter++;
}

gust_config_t gust_default_config(void) {
    gust_config_t config = {
        .num_insula_neurons = GUST_DEFAULT_INSULA_NEURONS,
        .num_ofc_neurons = GUST_DEFAULT_OFC_NEURONS,
        .max_receptors = GUST_MAX_TASTE_RECEPTORS,
        .adaptation_rate = 0.0002f,
        .bitter_sensitivity = 2.0f,
        .sweet_sensitivity = NIMCP_SENSITIVITY_DEFAULT,
        .enable_flavor_integration = true,
        .enable_reward_learning = true,
        .enable_all_bridges = true
    };
    return config;
}

nimcp_gustatory_t* gust_create(const gust_config_t* config) {
    gust_config_t default_config;
    if (!config) {
        default_config = gust_default_config();
        config = &default_config;
    }

    nimcp_gustatory_t* gust = (nimcp_gustatory_t*)nimcp_calloc(1, sizeof(nimcp_gustatory_t));
    if (!gust) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "gust_create: allocation failed");
        return NULL;
    }

    memcpy(&gust->config, config, sizeof(gust_config_t));
    gust->status = GUST_STATUS_IDLE;
    gust->last_error = GUST_ERROR_NONE;

    /* Allocate receptors */
    gust->receptors = (taste_receptor_t*)nimcp_calloc(config->max_receptors, sizeof(taste_receptor_t));
    if (!gust->receptors) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "gust_create: receptors allocation failed");
        nimcp_free(gust);
        return NULL;
    }
    gust->num_receptors = config->max_receptors;

    /* Initialize receptors with default sensitivity */
    for (uint32_t i = 0; i < gust->num_receptors; i++) {
        gust->receptors[i].receptor_id = i;
        gust->receptors[i].region = (tongue_region_t)(i % TONGUE_REGION_COUNT);
        gust->receptors[i].primary_taste = (basic_taste_t)(i % TASTE_COUNT);
        for (int t = 0; t < TASTE_COUNT; t++) {
            gust->receptors[i].sensitivity[t] = (t == gust->receptors[i].primary_taste) ? 1.0f : 0.2f;
        }
    }

    /* Allocate insula neurons */
    gust->insula_activation = (float*)nimcp_calloc(config->num_insula_neurons, sizeof(float));
    if (!gust->insula_activation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "gust_create: insula_activation allocation failed");
        nimcp_free(gust->receptors);
        nimcp_free(gust);
        return NULL;
    }
    gust->num_insula = config->num_insula_neurons;

    /* Allocate OFC neurons */
    gust->ofc_activation = (float*)nimcp_calloc(config->num_ofc_neurons, sizeof(float));
    if (!gust->ofc_activation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "gust_create: ofc_activation allocation failed");
        nimcp_free(gust->insula_activation);
        nimcp_free(gust->receptors);
        nimcp_free(gust);
        return NULL;
    }
    gust->num_ofc = config->num_ofc_neurons;

    /* Initialize adaptation */
    for (int i = 0; i < TASTE_COUNT; i++) {
        gust->adaptation_level[i] = 0.0f;
        gust->learned_preferences[i] = 0.5f;  /* Neutral preference */
    }
    gust->learned_preferences[TASTE_SWEET] = 0.8f;  /* Innate sweet preference */
    gust->learned_preferences[TASTE_BITTER] = 0.2f; /* Innate bitter aversion */

    gust->creation_time = gust_get_time_ms();
    gust->last_update_time = gust->creation_time;
    gust->status = GUST_STATUS_READY;

    return gust;
}

void gust_destroy(nimcp_gustatory_t* gust) {
    if (!gust) return;
    if (gust->receptors) nimcp_free(gust->receptors);
    if (gust->insula_activation) nimcp_free(gust->insula_activation);
    if (gust->ofc_activation) nimcp_free(gust->ofc_activation);
    nimcp_free(gust);
}

int gust_reset(nimcp_gustatory_t* gust) {
    if (!gust) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gust_reset: gust is NULL");
        return -1;
    }
    memset(gust->insula_activation, 0, gust->num_insula * sizeof(float));
    memset(gust->ofc_activation, 0, gust->num_ofc * sizeof(float));
    for (int i = 0; i < TASTE_COUNT; i++) {
        gust->adaptation_level[i] = 0.0f;
    }
    gust->updates_processed = 0;
    gust->status = GUST_STATUS_READY;
    gust->last_error = GUST_ERROR_NONE;
    return 0;
}

int gust_update(nimcp_gustatory_t* gust, float dt) {
    if (!gust) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gust_update: gust is NULL");
        return -1;
    }

    /* Decay activations */
    float decay = expf(-dt / 100.0f);
    for (uint32_t i = 0; i < gust->num_insula; i++) {
        gust->insula_activation[i] *= decay;
    }
    for (uint32_t i = 0; i < gust->num_ofc; i++) {
        gust->ofc_activation[i] *= decay;
    }

    /* Decay adaptation */
    float adapt_decay = expf(-dt / GUST_ADAPTATION_TAU);
    for (int i = 0; i < TASTE_COUNT; i++) {
        gust->adaptation_level[i] *= adapt_decay;
    }

    gust->updates_processed++;
    gust->last_update_time = gust_get_time_ms();
    return 0;
}

int gust_process_taste(nimcp_gustatory_t* gust, const taste_stimulus_t* stimulus) {
    if (!gust || !stimulus) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gust_process_taste: required parameter is NULL (gust, stimulus)");
        return -1;
    }

    gust->status = GUST_STATUS_TASTING;
    memcpy(&gust->current_stimulus, stimulus, sizeof(taste_stimulus_t));

    /* Apply adaptation and sensitivity */
    float tastes[TASTE_COUNT] = {
        stimulus->sweet * gust->config.sweet_sensitivity * (1.0f - gust->adaptation_level[TASTE_SWEET]),
        stimulus->salty * (1.0f - gust->adaptation_level[TASTE_SALTY]),
        stimulus->sour * (1.0f - gust->adaptation_level[TASTE_SOUR]),
        stimulus->bitter * gust->config.bitter_sensitivity * (1.0f - gust->adaptation_level[TASTE_BITTER]),
        stimulus->umami * (1.0f - gust->adaptation_level[TASTE_UMAMI])
    };

    /* Store perception */
    gust->current_perception.perceived_sweet = tastes[TASTE_SWEET];
    gust->current_perception.perceived_salty = tastes[TASTE_SALTY];
    gust->current_perception.perceived_sour = tastes[TASTE_SOUR];
    gust->current_perception.perceived_bitter = tastes[TASTE_BITTER];
    gust->current_perception.perceived_umami = tastes[TASTE_UMAMI];

    /* Calculate overall intensity */
    gust->current_perception.overall_intensity = 0.0f;
    for (int i = 0; i < TASTE_COUNT; i++) {
        gust->current_perception.overall_intensity += tastes[i];
    }
    gust->current_perception.overall_intensity /= TASTE_COUNT;

    /* Calculate palatability based on preferences */
    float palatability = 0.0f;
    for (int i = 0; i < TASTE_COUNT; i++) {
        palatability += tastes[i] * gust->learned_preferences[i];
    }
    gust->current_perception.palatability = palatability;

    /* Determine hedonic value */
    if (palatability < 0.2f) {
        gust->current_perception.hedonic_value = TASTE_HEDONIC_AVERSIVE;
    } else if (palatability < 0.4f) {
        gust->current_perception.hedonic_value = TASTE_HEDONIC_UNPLEASANT;
    } else if (palatability < 0.6f) {
        gust->current_perception.hedonic_value = TASTE_HEDONIC_NEUTRAL;
    } else if (palatability < 0.8f) {
        gust->current_perception.hedonic_value = TASTE_HEDONIC_PLEASANT;
    } else {
        gust->current_perception.hedonic_value = TASTE_HEDONIC_HIGHLY_PLEASANT;
    }

    /* Check for disgust (high bitter) */
    if (tastes[TASTE_BITTER] > 0.7f) {
        gust->current_perception.disgust = DISGUST_STRONG;
    } else if (tastes[TASTE_BITTER] > 0.5f) {
        gust->current_perception.disgust = DISGUST_MODERATE;
    } else if (tastes[TASTE_BITTER] > 0.3f) {
        gust->current_perception.disgust = DISGUST_MILD;
    } else {
        gust->current_perception.disgust = DISGUST_NONE;
    }

    /* Update insula (primary gustatory cortex) */
    for (uint32_t i = 0; i < gust->num_insula; i++) {
        int taste_idx = i % TASTE_COUNT;
        gust->insula_activation[i] += tastes[taste_idx] * 0.5f;
        if (gust->insula_activation[i] > 1.0f) {
            gust->insula_activation[i] = 1.0f;
        }
    }

    /* Update OFC */
    for (uint32_t i = 0; i < gust->num_ofc; i++) {
        gust->ofc_activation[i] = palatability * 0.5f;
    }

    /* Update adaptation */
    for (int i = 0; i < TASTE_COUNT; i++) {
        float taste_level = 0.0f;
        switch (i) {
            case TASTE_SWEET: taste_level = stimulus->sweet; break;
            case TASTE_SALTY: taste_level = stimulus->salty; break;
            case TASTE_SOUR: taste_level = stimulus->sour; break;
            case TASTE_BITTER: taste_level = stimulus->bitter; break;
            case TASTE_UMAMI: taste_level = stimulus->umami; break;
        }
        gust->adaptation_level[i] += gust->config.adaptation_rate * taste_level;
        if (gust->adaptation_level[i] > 0.9f) {
            gust->adaptation_level[i] = 0.9f;
        }
    }

    gust->current_perception.timestamp = gust_get_time_ms();
    gust->status = GUST_STATUS_READY;
    return 0;
}

int gust_get_perception(nimcp_gustatory_t* gust, taste_perception_t* perception) {
    if (!gust || !perception) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gust_get_perception: required parameter is NULL (gust, perception)");
        return -1;
    }
    memcpy(perception, &gust->current_perception, sizeof(taste_perception_t));
    return 0;
}

float gust_get_taste_intensity(nimcp_gustatory_t* gust, basic_taste_t taste) {
    if (!gust || taste >= TASTE_COUNT) return 0.0f;
    switch (taste) {
        case TASTE_SWEET: return gust->current_perception.perceived_sweet;
        case TASTE_SALTY: return gust->current_perception.perceived_salty;
        case TASTE_SOUR: return gust->current_perception.perceived_sour;
        case TASTE_BITTER: return gust->current_perception.perceived_bitter;
        case TASTE_UMAMI: return gust->current_perception.perceived_umami;
        default: return 0.0f;
    }
}

taste_hedonic_t gust_get_hedonic_value(nimcp_gustatory_t* gust) {
    if (!gust) return TASTE_HEDONIC_NEUTRAL;
    return gust->current_perception.hedonic_value;
}

float gust_get_palatability(nimcp_gustatory_t* gust) {
    if (!gust) return 0.0f;
    return gust->current_perception.palatability;
}

int gust_compute_reward(nimcp_gustatory_t* gust, food_reward_t* reward) {
    if (!gust || !reward) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gust_compute_reward: required parameter is NULL (gust, reward)");
        return -1;
    }

    float satiety_mod = 1.0f;
    if (gust->hypothalamus_bridge.initialized) {
        satiety_mod = 1.0f - gust->hypothalamus_bridge.satiety_level * 0.5f;
    }

    reward->reward_magnitude = gust->current_perception.palatability * satiety_mod;
    reward->satiety_modulation = satiety_mod;
    reward->is_toxic_warning = (gust->current_perception.disgust >= DISGUST_STRONG);

    /* Estimate nutritional value */
    reward->nutritional_value = gust->current_perception.perceived_sweet * 0.3f +
                                gust->current_perception.perceived_umami * 0.4f +
                                gust->current_stimulus.fat_content * 0.3f;

    memcpy(&gust->current_reward, reward, sizeof(food_reward_t));
    return 0;
}

int gust_learn_preference(nimcp_gustatory_t* gust, basic_taste_t taste, float preference_change) {
    if (!gust || taste >= TASTE_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "gust_learn_preference: gust is NULL");
        return -1;
    }
    gust->learned_preferences[taste] += preference_change;
    if (gust->learned_preferences[taste] < 0.0f) gust->learned_preferences[taste] = 0.0f;
    if (gust->learned_preferences[taste] > 1.0f) gust->learned_preferences[taste] = 1.0f;
    return 0;
}

disgust_level_t gust_evaluate_disgust(nimcp_gustatory_t* gust) {
    if (!gust) return DISGUST_NONE;
    return gust->current_perception.disgust;
}

bool gust_is_toxic_warning(nimcp_gustatory_t* gust) {
    if (!gust) {
        return false;
    }
    return gust->current_perception.perceived_bitter > 0.8f;
}

float gust_get_adaptation(nimcp_gustatory_t* gust, basic_taste_t taste) {
    if (!gust || taste >= TASTE_COUNT) return 0.0f;
    return gust->adaptation_level[taste];
}

int gust_reset_adaptation(nimcp_gustatory_t* gust) {
    if (!gust) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gust_reset_adaptation: gust is NULL");
        return -1;
    }
    for (int i = 0; i < TASTE_COUNT; i++) {
        gust->adaptation_level[i] = 0.0f;
    }
    return 0;
}

/* Bridge initialization */
int gust_init_prime_resonance_bridge(nimcp_gustatory_t* gust, void* pr_memory) {
    if (!gust) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gust_init_prime_resonance_bridge: gust is NULL");
        return -1;
    }
    gust->prime_resonance_bridge.pr_memory_ctx = pr_memory;
    gust->prime_resonance_bridge.initialized = true;
    return 0;
}

int gust_init_immune_bridge(nimcp_gustatory_t* gust, void* immune) {
    if (!gust) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gust_init_immune_bridge: gust is NULL");
        return -1;
    }
    gust->immune_bridge.immune_system = immune;
    gust->immune_bridge.initialized = true;
    gust->immune_bridge.health_score = 1.0f;
    return 0;
}

int gust_init_hypothalamus_bridge(nimcp_gustatory_t* gust, void* hypothalamus) {
    if (!gust) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gust_init_hypothalamus_bridge: gust is NULL");
        return -1;
    }
    gust->hypothalamus_bridge.hypothalamus = hypothalamus;
    gust->hypothalamus_bridge.initialized = true;
    return 0;
}

int gust_init_amygdala_bridge(nimcp_gustatory_t* gust, void* amygdala) {
    if (!gust) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gust_init_amygdala_bridge: gust is NULL");
        return -1;
    }
    gust->amygdala_bridge.amygdala = amygdala;
    gust->amygdala_bridge.initialized = true;
    return 0;
}

int gust_init_olfactory_bridge(nimcp_gustatory_t* gust, void* olfactory) {
    if (!gust) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gust_init_olfactory_bridge: gust is NULL");
        return -1;
    }
    gust->olfactory_bridge.olfactory = olfactory;
    gust->olfactory_bridge.initialized = true;
    return 0;
}

int gust_init_insula_bridge(nimcp_gustatory_t* gust, void* insula) {
    if (!gust) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gust_init_insula_bridge: gust is NULL");
        return -1;
    }
    gust->insula_bridge.insula = insula;
    gust->insula_bridge.initialized = true;
    return 0;
}

int gust_init_ofc_bridge(nimcp_gustatory_t* gust, void* ofc) {
    if (!gust) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gust_init_ofc_bridge: gust is NULL");
        return -1;
    }
    gust->ofc_bridge.orbitofrontal = ofc;
    gust->ofc_bridge.initialized = true;
    return 0;
}

int gust_init_logging_bridge(nimcp_gustatory_t* gust, void* logger) {
    if (!gust) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gust_init_logging_bridge: gust is NULL");
        return -1;
    }
    gust->logging_bridge.logger = logger;
    gust->logging_bridge.initialized = true;
    strncpy(gust->logging_bridge.log_prefix, "GUST", sizeof(gust->logging_bridge.log_prefix) - 1);
    return 0;
}

int gust_init_snn_bridge(nimcp_gustatory_t* gust, void* snn) {
    if (!gust) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gust_init_snn_bridge: gust is NULL");
        return -1;
    }
    gust->snn_bridge.snn = snn;
    gust->snn_bridge.neuron_ids = NULL;
    gust->snn_bridge.num_mapped_neurons = 0;
    gust->snn_bridge.snn_activation_gain = 1.0f;
    gust->snn_bridge.initialized = (snn != NULL);
    return 0;
}

int gust_init_plasticity_bridge(nimcp_gustatory_t* gust, void* plasticity, void* stdp) {
    if (!gust) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gust_init_plasticity_bridge: gust is NULL");
        return -1;
    }
    gust->plasticity_bridge.plasticity_coordinator = plasticity;
    gust->plasticity_bridge.stdp_context = stdp;
    gust->plasticity_bridge.learning_rate = NIMCP_LEARNING_RATE_DEFAULT;
    gust->plasticity_bridge.gustatory_plasticity_gate = 1.0f;
    gust->plasticity_bridge.hebbian_enabled = true;
    gust->plasticity_bridge.initialized = (plasticity != NULL || stdp != NULL);
    return 0;
}

int gust_init_bio_async_bridge(nimcp_gustatory_t* gust, void* runtime) {
    if (!gust) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gust_init_bio_async_bridge: gust is NULL");
        return -1;
    }
    gust->bio_async_embed.runtime = runtime;
    gust->bio_async_embed.subscription_mask = 0xFFFFFFFF; /* Subscribe to all */
    gust->bio_async_embed.messages_sent = 0;
    gust->bio_async_embed.messages_received = 0;
    gust->bio_async_embed.initialized = (runtime != NULL);
    return 0;
}

/* Bidirectional flow */
int gust_process_incoming(nimcp_gustatory_t* gust) {
    if (!gust) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gust_process_incoming: gust is NULL");
        return -1;
    }
    return 0;
}

int gust_send_outgoing(nimcp_gustatory_t* gust) {
    if (!gust) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gust_send_outgoing: gust is NULL");
        return -1;
    }
    return 0;
}

int gust_bidirectional_update(nimcp_gustatory_t* gust, float dt) {
    if (!gust) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gust_bidirectional_update: gust is NULL");
        return -1;
    }
    gust_process_incoming(gust);
    gust_update(gust, dt);
    gust_send_outgoing(gust);
    return 0;
}

int gust_sync_hypothalamus(nimcp_gustatory_t* gust) {
    if (!gust) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gust_sync_hypothalamus: gust is NULL");
        return -1;
    }
    if (!gust->hypothalamus_bridge.initialized) return 0;  /* No-op if not initialized */
    /* Sync with hypothalamus for satiety modulation */
    return 0;
}

int gust_sync_olfactory(nimcp_gustatory_t* gust) {
    if (!gust) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gust_sync_olfactory: gust is NULL");
        return -1;
    }
    if (!gust->olfactory_bridge.initialized) return 0;  /* No-op if not initialized */
    /* Sync with olfactory for flavor integration */
    return 0;
}

int gust_sync_ofc(nimcp_gustatory_t* gust) {
    if (!gust) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gust_sync_ofc: gust is NULL");
        return -1;
    }
    if (!gust->ofc_bridge.initialized) return 0;  /* No-op if not initialized */
    /* Sync with orbitofrontal cortex for valuation */
    return 0;
}

/* Status and diagnostics */
gust_status_t gust_get_status(nimcp_gustatory_t* gust) {
    if (!gust) return GUST_STATUS_ERROR;
    return gust->status;
}

gust_error_t gust_get_last_error(nimcp_gustatory_t* gust) {
    if (!gust) return GUST_ERROR_INTERNAL;
    return gust->last_error;
}

const char* gust_error_string(gust_error_t error) {
    switch (error) {
        case GUST_ERROR_NONE: return "No error";
        case GUST_ERROR_INVALID_INPUT: return "Invalid input";
        case GUST_ERROR_SATURATION: return "Saturation";
        case GUST_ERROR_PROCESSING_FAILED: return "Processing failed";
        case GUST_ERROR_BRIDGE_ERROR: return "Bridge error";
        case GUST_ERROR_INTERNAL: return "Internal error";
        default: return "Unknown error";
    }
}

const char* gust_status_string(gust_status_t status) {
    switch (status) {
        case GUST_STATUS_IDLE: return "Idle";
        case GUST_STATUS_READY: return "Ready";
        case GUST_STATUS_TASTING: return "Tasting";
        case GUST_STATUS_PROCESSING: return "Processing";
        case GUST_STATUS_INTEGRATING: return "Integrating";
        case GUST_STATUS_ERROR: return "Error";
        default: return "Unknown";
    }
}

int gust_get_stats(nimcp_gustatory_t* gust, gust_stats_t* stats) {
    if (!gust || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gust_get_stats: required parameter is NULL (gust, stats)");
        return -1;
    }
    stats->tastes_processed = gust->updates_processed;
    stats->avg_palatability = gust->current_perception.palatability;
    for (int i = 0; i < TASTE_COUNT; i++) {
        stats->current_adaptation[i] = gust->adaptation_level[i];
    }
    stats->last_update_time = gust->last_update_time;
    return 0;
}

float gust_get_health_status(nimcp_gustatory_t* gust) {
    if (!gust) return 0.0f;
    if (gust->status == GUST_STATUS_ERROR) return 0.0f;
    float health = 1.0f;
    if (gust->immune_bridge.initialized) {
        health *= gust->immune_bridge.health_score;
        if (gust->immune_bridge.taste_alteration) health *= 0.5f;
    }
    return health;
}

/* Utility functions */
const char* gust_taste_name(basic_taste_t taste) {
    static const char* names[] = {"Sweet", "Salty", "Sour", "Bitter", "Umami"};
    if (taste >= TASTE_COUNT) return "Unknown";
    return names[taste];
}

const char* gust_food_category_name(food_category_t category) {
    static const char* names[] = {
        "Unknown", "Carbohydrate", "Protein", "Fat", "Fruit",
        "Vegetable", "Dairy", "Toxic", "Spoiled"
    };
    if (category > FOOD_CAT_SPOILED) return "Unknown";
    return names[category];
}

const char* gust_disgust_name(disgust_level_t level) {
    static const char* names[] = {"None", "Mild", "Moderate", "Strong", "Extreme"};
    if (level > DISGUST_EXTREME) return "Unknown";
    return names[level];
}

const char* gust_hedonic_name(taste_hedonic_t hedonic) {
    static const char* names[] = {
        "Aversive", "Unpleasant", "Neutral", "Pleasant", "Highly Pleasant"
    };
    if (hedonic > TASTE_HEDONIC_HIGHLY_PLEASANT) return "Unknown";
    return names[hedonic];
}

/* Serialization */
size_t gust_get_serialization_size(nimcp_gustatory_t* gust) {
    if (!gust) return 0;
    return sizeof(gust_config_t) + sizeof(float) * TASTE_COUNT;  /* config + learned_preferences */
}

int gust_serialize(nimcp_gustatory_t* gust, uint8_t* buffer, size_t size, size_t* written) {
    if (!gust || !buffer || !written) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gust_serialize: required parameter is NULL (gust, buffer, written)");
        return -1;
    }
    size_t needed = gust_get_serialization_size(gust);
    if (size < needed) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "gust_serialize: validation failed");
        return -1;
    }

    size_t offset = 0;
    memcpy(buffer + offset, &gust->config, sizeof(gust_config_t));
    offset += sizeof(gust_config_t);
    memcpy(buffer + offset, gust->learned_preferences, sizeof(float) * TASTE_COUNT);
    offset += sizeof(float) * TASTE_COUNT;

    *written = offset;
    return 0;
}

nimcp_gustatory_t* gust_deserialize(const uint8_t* buffer, size_t size, size_t* bytes_read) {
    if (!buffer || !bytes_read) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gust_deserialize: required parameter is NULL (buffer, bytes_read)");
        return NULL;
    }
    size_t needed = sizeof(gust_config_t) + sizeof(float) * TASTE_COUNT;
    if (size < needed) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "gust_deserialize: validation failed");
        return NULL;
    }

    size_t offset = 0;
    gust_config_t config;
    memcpy(&config, buffer + offset, sizeof(gust_config_t));
    offset += sizeof(gust_config_t);

    nimcp_gustatory_t* gust = gust_create(&config);
    if (!gust) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "gust_deserialize: allocation failed");
        return NULL;
    }

    memcpy(gust->learned_preferences, buffer + offset, sizeof(float) * TASTE_COUNT);
    offset += sizeof(float) * TASTE_COUNT;

    *bytes_read = offset;
    return gust;
}
