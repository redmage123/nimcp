/**
 * @file nimcp_wernicke_substrate_bridge.c
 * @brief Wernicke substrate bridge implementation
 *
 * Implements metabolic modulation of language comprehension based on
 * ATP availability, fatigue levels, and temperature.
 *
 * @version Phase W3: Wernicke's Area Bridges
 * @date 2026-01-04
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "core/brain/regions/wernicke/nimcp_wernicke_substrate_bridge.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

//=============================================================================
#include <stddef.h>
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_threshold_constants.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(wernicke_substrate_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define LOG_MODULE "WERNICKE_SUBSTRATE_BRIDGE"


/*=============================================================================
 * INTERNAL STRUCTURES
 *===========================================================================*/

/**
 * @brief Bridge internal state
 */
struct wernicke_substrate_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    wernicke_substrate_config_t config;

    /* Endpoint connections */
    void* wernicke;                  /**< Wernicke adapter handle */
    void* substrate;                 /**< Neural substrate handle */
    void* bio_router;                /**< Bio-async router */

    /* Current metabolic state */
    float atp_level;                 /**< Current ATP [0-1] */
    float fatigue_level;             /**< Current fatigue [0-1] */
    float temperature;               /**< Current temperature (Celsius) */

    /* Computed effects */
    wernicke_substrate_effects_t effects;

    /* Statistics */
    wernicke_substrate_stats_t stats;

    /* State flags */
    bool effects_valid;              /**< Effects have been computed */
    bool use_manual_state;           /**< Use manually set state */
};

/*=============================================================================
 * INTERNAL CONSTANTS
 *===========================================================================*/

/* ATP thresholds */
#define ATP_CRITICAL_LOW     0.2f
#define ATP_LOW              0.4f
#define ATP_NORMAL           0.7f
#define ATP_HIGH             0.9f

/* Fatigue thresholds */
#define FATIGUE_LOW          0.2f
#define FATIGUE_MODERATE     0.5f
#define FATIGUE_HIGH         0.7f
#define FATIGUE_SEVERE       0.9f

/* Temperature effects (optimal range 36-38 Celsius) */
#define TEMP_OPTIMAL_LOW     36.0f
#define TEMP_OPTIMAL_HIGH    38.0f
#define TEMP_HYPO_THRESHOLD  35.0f
#define TEMP_HYPER_THRESHOLD 39.0f

/*=============================================================================
 * INTERNAL HELPERS
 *===========================================================================*/

/**
 * @brief Compute ATP modulation factor
 *
 * Higher ATP = better performance
 * Below critical: severe impairment
 */
static float compute_atp_factor(float atp, float sensitivity) {
    if (atp >= ATP_NORMAL) {
        return 1.0f;
    } else if (atp >= ATP_LOW) {
        /* Linear degradation from 1.0 to 0.7 */
        float t = (atp - ATP_LOW) / (ATP_NORMAL - ATP_LOW);
        return 0.7f + 0.3f * t;
    } else if (atp >= ATP_CRITICAL_LOW) {
        /* Steeper degradation from 0.7 to 0.3 */
        float t = (atp - ATP_CRITICAL_LOW) / (ATP_LOW - ATP_CRITICAL_LOW);
        return 0.3f + 0.4f * t;
    } else {
        /* Critical: severe impairment */
        return 0.1f + 0.2f * (atp / ATP_CRITICAL_LOW);
    }
}

/**
 * @brief Compute fatigue modulation factor
 *
 * Lower fatigue = better performance
 * High fatigue: working memory and disambiguation suffer
 */
static float compute_fatigue_factor(float fatigue, float sensitivity) {
    if (fatigue <= FATIGUE_LOW) {
        return 1.0f;
    } else if (fatigue <= FATIGUE_MODERATE) {
        /* Mild degradation */
        float t = (fatigue - FATIGUE_LOW) / (FATIGUE_MODERATE - FATIGUE_LOW);
        return 1.0f - 0.2f * t * sensitivity;
    } else if (fatigue <= FATIGUE_HIGH) {
        /* Moderate degradation */
        float t = (fatigue - FATIGUE_MODERATE) / (FATIGUE_HIGH - FATIGUE_MODERATE);
        return 0.8f - 0.3f * t * sensitivity;
    } else {
        /* Severe degradation */
        float t = (fatigue - FATIGUE_HIGH) / (1.0f - FATIGUE_HIGH);
        return 0.5f - 0.3f * t * sensitivity;
    }
}

/**
 * @brief Compute temperature modulation factor
 *
 * Optimal range: 36-38 Celsius
 * Outside range: degraded performance
 */
static float compute_temperature_factor(float temp) {
    if (temp >= TEMP_OPTIMAL_LOW && temp <= TEMP_OPTIMAL_HIGH) {
        return 1.0f;
    } else if (temp < TEMP_OPTIMAL_LOW) {
        /* Hypothermia: slowed processing */
        if (temp < TEMP_HYPO_THRESHOLD) {
            return 0.5f;
        }
        float t = (temp - TEMP_HYPO_THRESHOLD) / (TEMP_OPTIMAL_LOW - TEMP_HYPO_THRESHOLD);
        return 0.5f + 0.5f * t;
    } else {
        /* Hyperthermia: erratic processing */
        if (temp > TEMP_HYPER_THRESHOLD) {
            return 0.4f;
        }
        float t = (temp - TEMP_OPTIMAL_HIGH) / (TEMP_HYPER_THRESHOLD - TEMP_OPTIMAL_HIGH);
        return 1.0f - 0.6f * t;
    }
}

/**
 * @brief Compute all metabolic effects
 */
static void compute_effects(wernicke_substrate_bridge_t* bridge) {
    float atp = bridge->atp_level;
    float fatigue = bridge->fatigue_level;
    float temp = bridge->temperature;

    float atp_sens = bridge->config.atp_sensitivity;
    float fatigue_sens = bridge->config.fatigue_sensitivity;
    float min_cap = bridge->config.min_capacity;

    /* Base factors */
    float atp_factor = compute_atp_factor(atp, atp_sens);
    float fatigue_factor = compute_fatigue_factor(fatigue, fatigue_sens);
    float temp_factor = compute_temperature_factor(temp);

    /* Combined base factor */
    float base = atp_factor * fatigue_factor * temp_factor;

    /* Compute specific effects with weights */
    wernicke_substrate_effects_t* fx = &bridge->effects;

    /* Phoneme recognition: primarily ATP-dependent */
    fx->phoneme_recognition = nimcp_clamp01(
        base * (1.0f + (atp_factor - 0.5f) * bridge->config.phoneme_atp_weight)
    );
    if (fx->phoneme_recognition < min_cap) fx->phoneme_recognition = min_cap;

    /* Word recognition: ATP + mild fatigue effect */
    fx->word_recognition = nimcp_clamp01(base);
    if (fx->word_recognition < min_cap) fx->word_recognition = min_cap;

    /* Semantic access: ATP-dependent */
    fx->semantic_access = nimcp_clamp01(
        base * (1.0f + (atp_factor - 0.5f) * bridge->config.semantic_atp_weight)
    );
    if (fx->semantic_access < min_cap) fx->semantic_access = min_cap;

    /* Disambiguation: fatigue-sensitive */
    fx->disambiguation_capacity = nimcp_clamp01(
        base * (1.0f + (fatigue_factor - 0.5f) * bridge->config.disambig_fatigue_weight)
    );
    if (fx->disambiguation_capacity < min_cap) fx->disambiguation_capacity = min_cap;

    /* Working memory span: highly fatigue-sensitive */
    fx->working_memory_span = nimcp_clamp01(
        fatigue_factor * temp_factor *
        (1.0f + (fatigue_factor - 0.5f) * bridge->config.wm_fatigue_weight)
    );
    if (fx->working_memory_span < min_cap) fx->working_memory_span = min_cap;

    /* Context integration: balanced */
    fx->context_integration = nimcp_clamp01(base);
    if (fx->context_integration < min_cap) fx->context_integration = min_cap;

    /* Overall comprehension: weighted average */
    fx->overall_comprehension = (
        0.20f * fx->phoneme_recognition +
        0.25f * fx->word_recognition +
        0.20f * fx->semantic_access +
        0.15f * fx->disambiguation_capacity +
        0.10f * fx->working_memory_span +
        0.10f * fx->context_integration
    );
    if (fx->overall_comprehension < min_cap) fx->overall_comprehension = min_cap;

    bridge->effects_valid = true;
}

/*=============================================================================
 * LIFECYCLE API
 *===========================================================================*/

wernicke_substrate_config_t wernicke_substrate_default_config(void) {
    return (wernicke_substrate_config_t){
        .enable_atp_modulation = true,
        .enable_fatigue_modulation = true,
        .enable_bio_async = false,
        .atp_sensitivity = NIMCP_SENSITIVITY_DEFAULT,
        .fatigue_sensitivity = NIMCP_SENSITIVITY_DEFAULT,
        .min_capacity = 0.2f,
        .phoneme_atp_weight = 0.8f,
        .semantic_atp_weight = 1.0f,
        .wm_fatigue_weight = 1.2f,
        .disambig_fatigue_weight = 0.9f
    };
}

wernicke_substrate_bridge_t* wernicke_substrate_bridge_create(
    void* wernicke,
    void* substrate,
    const wernicke_substrate_config_t* config
) {
    /* Wernicke adapter is required */
    if (!wernicke) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wernicke is NULL");

        return NULL;
    }

    wernicke_substrate_bridge_t* bridge = nimcp_calloc(1, sizeof(wernicke_substrate_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;

    }

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = wernicke_substrate_default_config();
    }

    /* Store endpoints */
    bridge->wernicke = wernicke;
    bridge->substrate = substrate;

    /* Initialize state */
    bridge->atp_level = 1.0f;          /* Start healthy */
    bridge->fatigue_level = 0.0f;      /* Start rested */
    bridge->temperature = 37.0f;       /* Normal body temp */
    bridge->effects_valid = false;
    bridge->use_manual_state = false;

    /* Initialize bridge base */
    if (bridge_base_init(&bridge->base, 0, "wernicke_substrate") != 0) {
        nimcp_free(bridge);
        return NULL;
    }

    /* Compute initial effects */
    compute_effects(bridge);

    return bridge;
}

void wernicke_substrate_bridge_destroy(wernicke_substrate_bridge_t* bridge) {
    if (!bridge) return;
    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

/*=============================================================================
 * UPDATE API
 *===========================================================================*/

int wernicke_substrate_bridge_update(wernicke_substrate_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wernicke_substrate_bridge_update: bridge is NULL");
        return -1;
    }

    /* If not using manual state, would read from substrate here */
    if (!bridge->use_manual_state && bridge->substrate) {
        /* TODO: Read actual substrate state when integrated */
        /* For now, use current values */
    }

    /* Recompute effects */
    compute_effects(bridge);

    /* Update statistics */
    bridge->stats.updates_processed++;

    if (bridge->atp_level < ATP_LOW) {
        bridge->stats.low_atp_events++;
    }

    if (bridge->fatigue_level > FATIGUE_HIGH) {
        bridge->stats.high_fatigue_events++;
    }

    /* Running averages with isfinite guards */
    float n = (float)bridge->stats.updates_processed;
    float new_avg_comp = ((n - 1.0f) * bridge->stats.avg_comprehension +
                          bridge->effects.overall_comprehension) / n;
    if (isfinite(new_avg_comp)) bridge->stats.avg_comprehension = new_avg_comp;
    float new_avg_wm = ((n - 1.0f) * bridge->stats.avg_wm_span +
                         bridge->effects.working_memory_span) / n;
    if (isfinite(new_avg_wm)) bridge->stats.avg_wm_span = new_avg_wm;

    if (bridge->effects.overall_comprehension < bridge->stats.min_observed_capacity ||
        bridge->stats.updates_processed == 1) {
        bridge->stats.min_observed_capacity = bridge->effects.overall_comprehension;
    }

    return 0;
}

int wernicke_substrate_bridge_get_effects(
    const wernicke_substrate_bridge_t* bridge,
    wernicke_substrate_effects_t* effects
) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wernicke_substrate_bridge_get_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }

    if (!bridge->effects_valid) {
        /* Compute on demand */
        compute_effects((wernicke_substrate_bridge_t*)bridge);
    }

    *effects = bridge->effects;
    return 0;
}

int wernicke_substrate_bridge_apply(wernicke_substrate_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wernicke_substrate_bridge_apply: bridge is NULL");
        return -1;
    }

    /* Would apply effects to Wernicke adapter here */
    /* For now, just ensure effects are computed */
    if (!bridge->effects_valid) {
        compute_effects(bridge);
    }

    /* TODO: Call Wernicke adapter modulation functions when available */
    /* Example:
     * wernicke_set_recognition_speed(bridge->wernicke, bridge->effects.word_recognition);
     * wernicke_set_wm_capacity(bridge->wernicke, bridge->effects.working_memory_span);
     */

    return 0;
}

/*=============================================================================
 * MANUAL OVERRIDE API
 *===========================================================================*/

int wernicke_substrate_bridge_set_atp(
    wernicke_substrate_bridge_t* bridge,
    float atp
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wernicke_substrate_bridge_set_atp: bridge is NULL");
        return -1;
    }

    bridge->atp_level = nimcp_clamp01(atp);
    bridge->use_manual_state = true;
    bridge->effects_valid = false;

    return 0;
}

int wernicke_substrate_bridge_set_fatigue(
    wernicke_substrate_bridge_t* bridge,
    float fatigue
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wernicke_substrate_bridge_set_fatigue: bridge is NULL");
        return -1;
    }

    bridge->fatigue_level = nimcp_clamp01(fatigue);
    bridge->use_manual_state = true;
    bridge->effects_valid = false;

    return 0;
}

int wernicke_substrate_bridge_set_temperature(
    wernicke_substrate_bridge_t* bridge,
    float temperature
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wernicke_substrate_bridge_set_temperature: bridge is NULL");
        return -1;
    }

    bridge->temperature = temperature;
    bridge->use_manual_state = true;
    bridge->effects_valid = false;

    return 0;
}

/*=============================================================================
 * BIO-ASYNC INTEGRATION
 *===========================================================================*/

int wernicke_substrate_bridge_connect_bio_async(
    wernicke_substrate_bridge_t* bridge,
    void* router
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wernicke_substrate_bridge_connect_bio_async: bridge is NULL");
        return -1;
    }

    bridge->bio_router = router;

    /* TODO: Register with bio-async router when integrated */
    /* bio_router_register(router, BIO_MODULE_SUBSTRATE_WERNICKE, ...); */

    return 0;
}

int wernicke_substrate_bridge_handle_message(
    wernicke_substrate_bridge_t* bridge,
    void* message
) {
    if (!bridge || !message) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wernicke_substrate_bridge_handle_message: required parameter is NULL (bridge, message)");
        return -1;
    }

    /* TODO: Handle bio-async messages when integrated */
    /* Parse message, update state, recompute effects */

    return 0;
}

/*=============================================================================
 * STATISTICS API
 *===========================================================================*/

int wernicke_substrate_bridge_get_stats(
    const wernicke_substrate_bridge_t* bridge,
    wernicke_substrate_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wernicke_substrate_bridge_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    *stats = bridge->stats;
    return 0;
}

void wernicke_substrate_bridge_reset_stats(wernicke_substrate_bridge_t* bridge) {
    if (!bridge) return;
    memset(&bridge->stats, 0, sizeof(wernicke_substrate_stats_t));
}

/*=============================================================================
 * CONFIGURATION API
 *===========================================================================*/

int wernicke_substrate_bridge_get_config(
    const wernicke_substrate_bridge_t* bridge,
    wernicke_substrate_config_t* config
) {
    if (!bridge || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wernicke_substrate_bridge_get_config: required parameter is NULL (bridge, config)");
        return -1;
    }
    *config = bridge->config;
    return 0;
}

int wernicke_substrate_bridge_set_config(
    wernicke_substrate_bridge_t* bridge,
    const wernicke_substrate_config_t* config
) {
    if (!bridge || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "wernicke_substrate_bridge_set_config: required parameter is NULL (bridge, config)");
        return -1;
    }

    bridge->config = *config;
    bridge->effects_valid = false;  /* Need to recompute */

    return 0;
}
