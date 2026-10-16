/**
 * @file nimcp_trigeminal_oral_bridge.c
 * @brief Trigeminal-Oral Sensory Integration Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-12
 *
 * @author NIMCP Development Team
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "core/brain/regions/sensory_integration/nimcp_trigeminal_oral_bridge.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

//=============================================================================
#include <stddef.h>
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/thread/nimcp_thread_rand.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(trigeminal_oral_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define LOG_MODULE "TRIGEMINAL_ORAL_BRIDGE"


/* ============================================================================
 * Internal Structure
 * ============================================================================ */

struct trigeminal_oral_bridge_struct {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    trigeminal_oral_config_t config;

    /* Connected modules */
    nimcp_somatosensory_t* soma;
    nimcp_gustatory_t* gust;
    bool is_connected;

    /* Status */
    trigeminal_status_t status;

    /* Current state */
    oral_soma_input_t current_inputs[ORAL_REGION_COUNT];
    chemesthesis_t current_chemesthesis;
    texture_perception_t current_texture;
    mouthfeel_t current_mouthfeel;
    spiciness_perception_t current_spiciness;

    /* Cooling state */
    bool cooling_active;
    float cooling_intensity;

    /* Mastication state */
    bool masticating;
    uint32_t chew_count;
    float food_hardness;
    float breakdown_progress;
    float prev_bite_force;  /* For chew cycle detection */

    /* Adaptation */
    float spice_adaptation;
    float cold_adaptation;

    /* Statistics */
    trigeminal_oral_stats_t stats;
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static float randf(void) {
    return (float)nimcp_tl_rand() / (float)RAND_MAX;
}

static temp_perception_t classify_temperature(float temp_c) {
    if (temp_c < 20.0f) return TEMP_PERCEPTION_COLD;
    if (temp_c < 30.0f) return TEMP_PERCEPTION_COOL;
    if (temp_c < 40.0f) return TEMP_PERCEPTION_NEUTRAL;
    if (temp_c < 50.0f) return TEMP_PERCEPTION_WARM;
    return TEMP_PERCEPTION_HOT;
}

/* ============================================================================
 * Lifecycle API Implementation
 * ============================================================================ */

int trigeminal_oral_default_config(trigeminal_oral_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "trigeminal_oral_default_config: config is NULL");
        return -1;
    }

    memset(config, 0, sizeof(trigeminal_oral_config_t));

    config->enable_chemesthesis = true;
    config->enable_texture = true;
    config->enable_temp_taste = true;
    config->enable_mouthfeel = true;

    config->spice_sensitivity = 0.5f;
    config->cold_sensitivity = 0.5f;
    config->texture_sensitivity = 0.5f;

    config->spice_tolerance = 0.3f;

    config->integration_window_ms = 200;
    config->adaptation_rate = 0.1f;

    config->enable_logging = false;

    return 0;
}

trigeminal_oral_bridge_t* trigeminal_oral_bridge_create(const trigeminal_oral_config_t* config) {
    trigeminal_oral_bridge_t* bridge = (trigeminal_oral_bridge_t*)nimcp_calloc(1, sizeof(trigeminal_oral_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "trigeminal_oral_bridge_create: allocation failed");
        return NULL;
    }

    bridge_base_init(&bridge->base, 0, "trigeminal_oral");

    if (config) {
        memcpy(&bridge->config, config, sizeof(trigeminal_oral_config_t));
    } else {
        trigeminal_oral_default_config(&bridge->config);
    }

    bridge->is_connected = false;
    bridge->status = TRIGEMINAL_STATUS_IDLE;
    bridge->masticating = false;
    bridge->cooling_active = false;

    NIMCP_LOGGING_INFO("Created %s bridge", "trigeminal_oral");
    return bridge;
}

void trigeminal_oral_bridge_destroy(trigeminal_oral_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "trigeminal_oral");

    /* Free any allocated texture/mouthfeel profiles */
    if (bridge->current_texture.texture_profile) {
        nimcp_free(bridge->current_texture.texture_profile);
    }
    if (bridge->current_mouthfeel.mouthfeel_profile) {
        nimcp_free(bridge->current_mouthfeel.mouthfeel_profile);
    }

    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

/* ============================================================================
 * Connection API Implementation
 * ============================================================================ */

int trigeminal_oral_connect(trigeminal_oral_bridge_t* bridge,
                            nimcp_somatosensory_t* soma,
                            nimcp_gustatory_t* gust) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "trigeminal_oral_bridge_destroy: bridge is NULL");
        return -1;
    }

    bridge->soma = soma;
    bridge->gust = gust;
    bridge->is_connected = true;
    bridge->status = TRIGEMINAL_STATUS_IDLE;

    return 0;
}

int trigeminal_oral_disconnect(trigeminal_oral_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "trigeminal_oral_disconnect: bridge is NULL");
        return -1;
    }

    bridge->soma = NULL;
    bridge->gust = NULL;
    bridge->is_connected = false;

    return 0;
}

bool trigeminal_oral_is_connected(const trigeminal_oral_bridge_t* bridge) {
    return bridge && bridge->is_connected;
}

trigeminal_status_t trigeminal_oral_get_status(const trigeminal_oral_bridge_t* bridge) {
    if (!bridge) return TRIGEMINAL_STATUS_ERROR;
    return bridge->status;
}

/* ============================================================================
 * Oral Input Processing Implementation
 * ============================================================================ */

int trigeminal_oral_process_input(trigeminal_oral_bridge_t* bridge,
                                  const oral_soma_input_t* input) {
    if (!bridge || !input) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "trigeminal_oral_get_status: required parameter is NULL (bridge, input)");
        return -1;
    }
    if (input->region >= ORAL_REGION_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BUFFER_OVERFLOW, "trigeminal_oral_get_status: capacity exceeded");
        return -1;
    }

    bridge->status = TRIGEMINAL_STATUS_PROCESSING;

    /* Store input for this region */
    memcpy(&bridge->current_inputs[input->region], input, sizeof(oral_soma_input_t));

    bridge->stats.oral_inputs_processed++;
    bridge->status = TRIGEMINAL_STATUS_IDLE;

    return 0;
}

int trigeminal_oral_process_multi(trigeminal_oral_bridge_t* bridge,
                                  const oral_soma_input_t* inputs,
                                  uint32_t num_inputs) {
    if (!bridge || !inputs) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "trigeminal_oral_get_status: required parameter is NULL (bridge, inputs)");
        return -1;
    }

    for (uint32_t i = 0; i < num_inputs; i++) {
        int ret = trigeminal_oral_process_input(bridge, &inputs[i]);
        if (ret != 0) return ret;
    }

    return 0;
}

int trigeminal_oral_update(trigeminal_oral_bridge_t* bridge, float dt) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "trigeminal_oral_update: bridge is NULL");
        return -1;
    }

    /* Update adaptation */
    bridge->spice_adaptation *= (1.0f - bridge->config.adaptation_rate * dt);
    bridge->cold_adaptation *= (1.0f - bridge->config.adaptation_rate * dt);

    /* Decay cooling if no active cooling input */
    if (bridge->cooling_active) {
        bridge->cooling_intensity *= (1.0f - 0.05f * dt);
        if (bridge->cooling_intensity < 0.01f) {
            bridge->cooling_active = false;
            bridge->cooling_intensity = 0.0f;
        }
    }

    /* Update spiciness decay */
    if (bridge->current_spiciness.heat_level > 0.0f) {
        bridge->current_spiciness.heat_level *= (1.0f - bridge->current_spiciness.decay_rate * dt);
    }

    return 0;
}

/* ============================================================================
 * Chemesthesis API Implementation
 * ============================================================================ */

int trigeminal_oral_detect_chemesthesis(trigeminal_oral_bridge_t* bridge,
                                        chemesthesis_type_t compound_type,
                                        float concentration,
                                        oral_region_t region,
                                        chemesthesis_t* result) {
    if (!bridge || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "trigeminal_oral_update: required parameter is NULL (bridge, result)");
        return -1;
    }
    if (!bridge->config.enable_chemesthesis) {
        return 0;  /* Feature disabled, not an error */
    }

    memset(result, 0, sizeof(chemesthesis_t));
    result->type = compound_type;
    result->primary_region = region;
    result->affected_regions = (1u << region);

    /* Calculate intensity based on type and sensitivity */
    float base_intensity = concentration;

    switch (compound_type) {
        case CHEMESTHESIS_SPICY_HEAT:
            /* Apply spice sensitivity and tolerance */
            base_intensity *= bridge->config.spice_sensitivity;
            base_intensity *= (1.0f - bridge->config.spice_tolerance);
            result->scoville_equiv = concentration * TRIGEMINAL_SCOVILLE_SCALE;
            result->onset_rate = 0.3f;
            result->duration_s = 30.0f + concentration * 60.0f;

            /* Update spiciness state */
            bridge->current_spiciness.heat_level = base_intensity;
            bridge->current_spiciness.scoville_estimate = result->scoville_equiv;
            bridge->current_spiciness.peak_intensity = base_intensity;
            bridge->current_spiciness.decay_rate = 0.02f;
            bridge->current_spiciness.tolerance_factor = bridge->config.spice_tolerance;
            bridge->current_spiciness.causes_endorphin_release = (base_intensity > 0.5f);
            break;

        case CHEMESTHESIS_COOLING:
            base_intensity *= bridge->config.cold_sensitivity;
            result->cooling_intensity = base_intensity;
            result->onset_rate = 0.5f;
            result->duration_s = 10.0f + concentration * 20.0f;

            /* Update cooling state */
            bridge->cooling_active = true;
            bridge->cooling_intensity = base_intensity;
            break;

        case CHEMESTHESIS_TINGLING:
            result->onset_rate = 0.4f;
            result->duration_s = 5.0f + concentration * 10.0f;
            break;

        case CHEMESTHESIS_ASTRINGENT:
            result->onset_rate = 0.2f;
            result->duration_s = 20.0f + concentration * 30.0f;
            break;

        case CHEMESTHESIS_CARBONATION:
            result->onset_rate = 0.8f;
            result->duration_s = 2.0f + concentration * 5.0f;
            /* Carbonation spreads across mouth */
            result->affected_regions = (1u << ORAL_REGION_TONGUE_TIP) |
                                       (1u << ORAL_REGION_TONGUE_BODY) |
                                       (1u << ORAL_REGION_PALATE_HARD);
            break;

        case CHEMESTHESIS_IRRITANT:
            base_intensity *= bridge->config.spice_sensitivity;
            result->onset_rate = 0.7f;
            result->duration_s = 5.0f + concentration * 15.0f;
            /* Irritants spread to nasal passages */
            result->spread = 0.8f;
            break;

        default:
            break;
    }

    result->intensity = nimcp_clampf(base_intensity, 0.0f, 1.0f);
    result->spread = nimcp_clampf(concentration * 0.5f, 0.0f, 1.0f);

    /* Store current chemesthesis */
    memcpy(&bridge->current_chemesthesis, result, sizeof(chemesthesis_t));

    bridge->stats.chemesthesis_detected++;

    return 0;
}

int trigeminal_oral_get_spiciness(trigeminal_oral_bridge_t* bridge,
                                  spiciness_perception_t* spiciness) {
    if (!bridge || !spiciness) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "trigeminal_oral_update: required parameter is NULL (bridge, spiciness)");
        return -1;
    }

    memcpy(spiciness, &bridge->current_spiciness, sizeof(spiciness_perception_t));
    return 0;
}

int trigeminal_oral_update_spice_tolerance(trigeminal_oral_bridge_t* bridge,
                                           float exposure_intensity,
                                           float duration_s) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "trigeminal_oral_update: bridge is NULL");
        return -1;
    }

    /* Tolerance increases with exposure */
    float tolerance_gain = exposure_intensity * duration_s * 0.001f;
    bridge->config.spice_tolerance = nimcp_clampf(
        bridge->config.spice_tolerance + tolerance_gain, 0.0f, 0.9f);

    bridge->stats.spice_tolerance_updates++;

    return 0;
}

bool trigeminal_oral_is_cooling_active(const trigeminal_oral_bridge_t* bridge) {
    return bridge && bridge->cooling_active;
}

float trigeminal_oral_get_cooling_intensity(const trigeminal_oral_bridge_t* bridge) {
    if (!bridge) return 0.0f;
    return bridge->cooling_intensity;
}

/* ============================================================================
 * Texture API Implementation
 * ============================================================================ */

int trigeminal_oral_analyze_texture(trigeminal_oral_bridge_t* bridge,
                                    const oral_soma_input_t* input,
                                    texture_perception_t* texture) {
    if (!bridge || !input || !texture) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "trigeminal_oral_get_cooling_intensity: required parameter is NULL (bridge, input, texture)");
        return -1;
    }
    if (!bridge->config.enable_texture) {
        return 0;  /* Feature disabled, not an error */
    }

    memset(texture, 0, sizeof(texture_perception_t));

    /* Analyze mechanical properties to determine texture */
    float roughness = input->texture_roughness;
    float hardness = input->hardness;
    float viscosity = input->viscosity;
    float pressure = input->pressure;

    /* Classify primary texture */
    if (viscosity > 0.7f) {
        texture->primary = TEXTURE_LIQUID;
        if (viscosity > 0.9f) {
            texture->secondary = TEXTURE_CREAMY;
        }
    } else if (hardness > 0.8f && roughness > 0.5f) {
        texture->primary = TEXTURE_CRUNCHY;
        texture->crunchiness = hardness * 0.8f + roughness * 0.2f;
    } else if (hardness > 0.6f && roughness < 0.3f) {
        texture->primary = TEXTURE_CRISPY;
        texture->crunchiness = hardness * 0.7f;
    } else if (hardness > 0.5f && roughness < 0.2f) {
        texture->primary = TEXTURE_CHEWY;
        texture->chewiness = hardness * 0.9f;
    } else if (roughness < 0.2f && viscosity > 0.3f) {
        texture->primary = TEXTURE_SMOOTH;
        texture->smoothness = 1.0f - roughness;
    } else if (roughness > 0.6f) {
        texture->primary = TEXTURE_ROUGH;
        texture->graininess = roughness;
    } else if (roughness > 0.3f && roughness < 0.6f) {
        texture->primary = TEXTURE_GRAINY;
        texture->graininess = roughness;
    } else {
        texture->primary = TEXTURE_SMOOTH;
        texture->smoothness = 1.0f - roughness;
    }

    /* Compute moisture */
    texture->moisture = nimcp_clampf(1.0f - hardness + viscosity * 0.5f, 0.0f, 1.0f);

    /* Compute breakdown rate */
    texture->breakdown_rate = (1.0f - hardness) * 0.5f + viscosity * 0.3f;
    texture->changes_with_chewing = (hardness > 0.3f);

    /* Allocate and fill texture profile */
    texture->profile_dim = TRIGEMINAL_TEXTURE_DIM;
    texture->texture_profile = (float*)nimcp_calloc(texture->profile_dim, sizeof(float));
    if (texture->texture_profile) {
        texture->texture_profile[0] = roughness;
        texture->texture_profile[1] = hardness;
        texture->texture_profile[2] = viscosity;
        texture->texture_profile[3] = texture->crunchiness;
        texture->texture_profile[4] = texture->chewiness;
        texture->texture_profile[5] = texture->smoothness;
        texture->texture_profile[6] = texture->graininess;
        texture->texture_profile[7] = texture->moisture;
    }

    /* Store current texture */
    if (bridge->current_texture.texture_profile) {
        nimcp_free(bridge->current_texture.texture_profile);
    }
    memcpy(&bridge->current_texture, texture, sizeof(texture_perception_t));
    bridge->current_texture.texture_profile = NULL;  /* Don't share pointer */

    bridge->stats.textures_analyzed++;

    return 0;
}

int trigeminal_oral_classify_texture(trigeminal_oral_bridge_t* bridge,
                                     const float* texture_features,
                                     uint32_t num_features,
                                     texture_category_t* category,
                                     float* confidence) {
    if (!bridge || !texture_features || !category || !confidence) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "trigeminal_oral_get_cooling_intensity: required parameter is NULL (bridge, texture_features, category, confidence)");
        return -1;
    }
    if (num_features < 3) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "trigeminal_oral_get_cooling_intensity: validation failed");
        return -1;
    }

    float roughness = texture_features[0];
    float hardness = texture_features[1];
    float viscosity = num_features > 2 ? texture_features[2] : 0.0f;

    /* Simple classification */
    if (viscosity > 0.7f) {
        *category = TEXTURE_LIQUID;
        *confidence = viscosity;
    } else if (hardness > 0.7f && roughness > 0.4f) {
        *category = TEXTURE_CRUNCHY;
        *confidence = (hardness + roughness) / 2.0f;
    } else if (hardness > 0.5f) {
        *category = TEXTURE_CHEWY;
        *confidence = hardness;
    } else if (roughness < 0.2f) {
        *category = TEXTURE_SMOOTH;
        *confidence = 1.0f - roughness;
    } else {
        *category = TEXTURE_GRAINY;
        *confidence = roughness;
    }

    return 0;
}

int trigeminal_oral_get_texture(trigeminal_oral_bridge_t* bridge,
                                texture_perception_t* texture) {
    if (!bridge || !texture) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "trigeminal_oral_get_cooling_intensity: required parameter is NULL (bridge, texture)");
        return -1;
    }

    memcpy(texture, &bridge->current_texture, sizeof(texture_perception_t));
    texture->texture_profile = NULL;  /* Don't expose internal pointer */

    return 0;
}

/* ============================================================================
 * Temperature-Taste Interaction Implementation
 * ============================================================================ */

int trigeminal_oral_compute_temp_taste(trigeminal_oral_bridge_t* bridge,
                                       float temperature_c,
                                       temp_taste_interaction_t* interaction) {
    if (!bridge || !interaction) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "trigeminal_oral_get_cooling_intensity: required parameter is NULL (bridge, interaction)");
        return -1;
    }
    if (!bridge->config.enable_temp_taste) {
        return 0;  /* Feature disabled, not an error */
    }

    memset(interaction, 0, sizeof(temp_taste_interaction_t));

    interaction->temperature = classify_temperature(temperature_c);
    interaction->temp_celsius = temperature_c;

    /* Temperature effects on taste (based on research):
     * - Cold suppresses sweet perception
     * - Warmth enhances sweet
     * - Cold enhances bitter
     * - Temperature has less effect on salty/sour
     */

    float temp_factor = (temperature_c - 20.0f) / 40.0f;  /* Normalize around 20-60C */
    temp_factor = nimcp_clampf(temp_factor, -1.0f, 1.0f);

    /* Sweet: enhanced by warmth, suppressed by cold */
    interaction->sweet_modulation = 1.0f + temp_factor * 0.3f;

    /* Bitter: enhanced by cold, reduced by warmth */
    interaction->bitter_modulation = 1.0f - temp_factor * 0.2f;

    /* Salty: slight enhancement at room temp */
    float salt_opt = 1.0f - fabsf(temperature_c - 30.0f) / 30.0f * 0.15f;
    interaction->salty_modulation = salt_opt;

    /* Sour: relatively stable */
    interaction->sour_modulation = 1.0f - fabsf(temp_factor) * 0.1f;

    /* Umami: enhanced by warmth */
    interaction->umami_modulation = 1.0f + temp_factor * 0.2f;

    /* Overall intensity */
    interaction->intensity_modulation = 1.0f - fabsf(temp_factor) * 0.1f;

    /* Detection threshold (cold raises threshold) */
    interaction->threshold_shift = -temp_factor * 0.2f;

    bridge->stats.temp_taste_interactions++;

    return 0;
}

int trigeminal_oral_modulate_taste(trigeminal_oral_bridge_t* bridge,
                                   float temperature_c,
                                   float* sweet, float* salty,
                                   float* sour, float* bitter,
                                   float* umami) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "trigeminal_oral_get_cooling_intensity: bridge is NULL");
        return -1;
    }

    temp_taste_interaction_t interaction;
    int ret = trigeminal_oral_compute_temp_taste(bridge, temperature_c, &interaction);
    if (ret != 0) return ret;

    if (sweet) *sweet *= interaction.sweet_modulation;
    if (salty) *salty *= interaction.salty_modulation;
    if (sour) *sour *= interaction.sour_modulation;
    if (bitter) *bitter *= interaction.bitter_modulation;
    if (umami) *umami *= interaction.umami_modulation;

    return 0;
}

int trigeminal_oral_optimal_temperature(trigeminal_oral_bridge_t* bridge,
                                        const taste_perception_t* taste,
                                        float* optimal_temp_c) {
    if (!bridge || !taste || !optimal_temp_c) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (bridge, taste, optimal_temp_c)");
        return -1;
    }

    /* Heuristic for optimal serving temperature based on taste profile */
    float sweet_weight = taste->perceived_sweet;
    float bitter_weight = taste->perceived_bitter;
    float umami_weight = taste->perceived_umami;
    float sour_weight = taste->perceived_sour;

    /* Sweet/umami dominated: serve warmer */
    /* Bitter/sour dominated: serve cooler */
    float temp = 25.0f;  /* Base temperature */
    temp += sweet_weight * 15.0f;   /* Sweet -> warmer */
    temp += umami_weight * 10.0f;   /* Umami -> warmer */
    temp -= bitter_weight * 10.0f;  /* Bitter -> cooler */
    temp -= sour_weight * 5.0f;     /* Sour -> slightly cooler */

    *optimal_temp_c = nimcp_clampf(temp, 5.0f, 65.0f);

    return 0;
}

/* ============================================================================
 * Mouthfeel Integration Implementation
 * ============================================================================ */

int trigeminal_oral_compute_mouthfeel(trigeminal_oral_bridge_t* bridge,
                                      const oral_soma_input_t* soma_input,
                                      const taste_perception_t* taste_input,
                                      mouthfeel_t* mouthfeel) {
    if (!bridge || !mouthfeel) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (bridge, mouthfeel)");
        return -1;
    }
    if (!bridge->config.enable_mouthfeel) {
        return 0;  /* Feature disabled, not an error */
    }

    memset(mouthfeel, 0, sizeof(mouthfeel_t));

    /* Analyze texture from soma input */
    if (soma_input) {
        trigeminal_oral_analyze_texture(bridge, soma_input, &mouthfeel->texture);

        /* Temperature perception */
        mouthfeel->temperature = classify_temperature(soma_input->temperature_c);
    }

    /* Copy current chemesthesis */
    memcpy(&mouthfeel->chemesthesis, &bridge->current_chemesthesis, sizeof(chemesthesis_t));

    /* Determine primary mouthfeel quality */
    if (soma_input) {
        if (bridge->current_chemesthesis.type == CHEMESTHESIS_SPICY_HEAT &&
            bridge->current_chemesthesis.intensity > 0.5f) {
            mouthfeel->primary_quality = MOUTHFEEL_BURNING;
        } else if (bridge->cooling_active) {
            mouthfeel->primary_quality = MOUTHFEEL_COOLING;
        } else if (bridge->current_chemesthesis.type == CHEMESTHESIS_ASTRINGENT) {
            mouthfeel->primary_quality = MOUTHFEEL_ASTRINGENT;
        } else if (soma_input->viscosity > 0.5f && soma_input->texture_roughness < 0.3f) {
            mouthfeel->primary_quality = MOUTHFEEL_CREAMY;
        } else if (soma_input->viscosity > 0.4f) {
            mouthfeel->primary_quality = MOUTHFEEL_OILY;
        } else if (soma_input->texture_roughness > 0.6f) {
            mouthfeel->primary_quality = MOUTHFEEL_DRY;
        } else {
            mouthfeel->primary_quality = MOUTHFEEL_NEUTRAL;
        }
    }

    /* Compute taste enhancement from texture */
    if (taste_input && soma_input) {
        /* Crunchy textures enhance flavor perception */
        mouthfeel->taste_enhancement = mouthfeel->texture.crunchiness * 0.3f +
                                       (1.0f - soma_input->viscosity) * 0.2f;

        /* Flavor release increases with chewing */
        mouthfeel->flavor_release = mouthfeel->texture.breakdown_rate * 0.5f +
                                    bridge->breakdown_progress * 0.5f;
    }

    /* Compute pleasantness (simple heuristic) */
    float pleasantness = 0.5f;
    if (taste_input) {
        pleasantness += taste_input->palatability * 0.3f;
    }
    /* Extreme temperatures reduce pleasantness */
    if (soma_input) {
        if (soma_input->temperature_c < 5.0f || soma_input->temperature_c > 60.0f) {
            pleasantness -= 0.3f;
        }
        /* Pain reduces pleasantness */
        pleasantness -= soma_input->pain_level * 0.5f;
    }
    /* Chemesthesis can enhance or reduce based on tolerance */
    if (bridge->current_chemesthesis.type == CHEMESTHESIS_SPICY_HEAT) {
        if (bridge->config.spice_tolerance > 0.5f) {
            pleasantness += bridge->current_chemesthesis.intensity * 0.2f;
        } else {
            pleasantness -= bridge->current_chemesthesis.intensity * 0.3f;
        }
    }
    mouthfeel->pleasantness = nimcp_clampf(pleasantness, -1.0f, 1.0f);

    /* Familiarity (placeholder) */
    mouthfeel->familiarity = 0.5f + randf() * 0.3f;

    /* Temporal dynamics */
    mouthfeel->onset_time_ms = 50.0f + mouthfeel->texture.crunchiness * 50.0f;
    mouthfeel->peak_time_ms = 200.0f + mouthfeel->texture.chewiness * 300.0f;
    mouthfeel->linger_time_ms = 500.0f + bridge->current_chemesthesis.duration_s * 100.0f;

    mouthfeel->timestamp = soma_input ? soma_input->timestamp : 0;

    /* Allocate mouthfeel profile */
    mouthfeel->profile_dim = TRIGEMINAL_MOUTHFEEL_DIM;
    mouthfeel->mouthfeel_profile = (float*)nimcp_calloc(mouthfeel->profile_dim, sizeof(float));
    if (mouthfeel->mouthfeel_profile) {
        mouthfeel->mouthfeel_profile[0] = (float)mouthfeel->primary_quality / MOUTHFEEL_COUNT;
        mouthfeel->mouthfeel_profile[1] = mouthfeel->taste_enhancement;
        mouthfeel->mouthfeel_profile[2] = mouthfeel->flavor_release;
        mouthfeel->mouthfeel_profile[3] = mouthfeel->pleasantness;
        mouthfeel->mouthfeel_profile[4] = mouthfeel->texture.crunchiness;
        mouthfeel->mouthfeel_profile[5] = mouthfeel->texture.chewiness;
        mouthfeel->mouthfeel_profile[6] = mouthfeel->texture.smoothness;
        mouthfeel->mouthfeel_profile[7] = bridge->current_chemesthesis.intensity;
        mouthfeel->mouthfeel_profile[8] = bridge->cooling_intensity;
        mouthfeel->mouthfeel_profile[9] = (float)mouthfeel->temperature / TEMP_PERCEPTION_COUNT;
        mouthfeel->mouthfeel_profile[10] = mouthfeel->familiarity;
        mouthfeel->mouthfeel_profile[11] = mouthfeel->onset_time_ms / 1000.0f;
    }

    /* Store current mouthfeel */
    if (bridge->current_mouthfeel.mouthfeel_profile) {
        nimcp_free(bridge->current_mouthfeel.mouthfeel_profile);
    }
    memcpy(&bridge->current_mouthfeel, mouthfeel, sizeof(mouthfeel_t));
    bridge->current_mouthfeel.mouthfeel_profile = NULL;

    bridge->stats.mouthfeels_computed++;
    {
        float new_avg = (bridge->stats.avg_mouthfeel_rating *
            (bridge->stats.mouthfeels_computed - 1) + mouthfeel->pleasantness) /
            bridge->stats.mouthfeels_computed;
        if (isfinite(new_avg)) bridge->stats.avg_mouthfeel_rating = new_avg;
    }

    return 0;
}

int trigeminal_oral_get_mouthfeel(trigeminal_oral_bridge_t* bridge,
                                  mouthfeel_t* mouthfeel) {
    if (!bridge || !mouthfeel) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (bridge, mouthfeel)");
        return -1;
    }

    memcpy(mouthfeel, &bridge->current_mouthfeel, sizeof(mouthfeel_t));
    mouthfeel->mouthfeel_profile = NULL;
    mouthfeel->texture.texture_profile = NULL;

    return 0;
}

int trigeminal_oral_predict_pleasantness(trigeminal_oral_bridge_t* bridge,
                                         const mouthfeel_t* mouthfeel,
                                         float* pleasantness) {
    if (!bridge || !mouthfeel || !pleasantness) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (bridge, mouthfeel, pleasantness)");
        return -1;
    }

    /* Simple pleasantness prediction */
    *pleasantness = mouthfeel->pleasantness;

    return 0;
}

int trigeminal_oral_flavor_texture_synergy(trigeminal_oral_bridge_t* bridge,
                                           const taste_perception_t* taste,
                                           const texture_perception_t* texture,
                                           float* synergy_score) {
    if (!bridge || !taste || !texture || !synergy_score) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (bridge, taste, texture, synergy_score)");
        return -1;
    }

    /* Compute synergy between flavor and texture */
    float synergy = 0.5f;

    /* Crunchy + salty = high synergy (chips) */
    if (texture->primary == TEXTURE_CRUNCHY && taste->perceived_salty > 0.5f) {
        synergy += 0.3f;
    }

    /* Smooth + sweet = high synergy (chocolate) */
    if (texture->primary == TEXTURE_SMOOTH && taste->perceived_sweet > 0.5f) {
        synergy += 0.3f;
    }

    /* Creamy + umami = high synergy (cheese) */
    if (texture->primary == TEXTURE_CREAMY && taste->perceived_umami > 0.5f) {
        synergy += 0.25f;
    }

    /* Chewy + sweet = moderate synergy (caramel) */
    if (texture->primary == TEXTURE_CHEWY && taste->perceived_sweet > 0.5f) {
        synergy += 0.2f;
    }

    /* Grainy + bitter = low synergy */
    if (texture->primary == TEXTURE_GRAINY && taste->perceived_bitter > 0.5f) {
        synergy -= 0.2f;
    }

    *synergy_score = nimcp_clampf(synergy, 0.0f, 1.0f);

    return 0;
}

/* ============================================================================
 * Mastication Implementation
 * ============================================================================ */

int trigeminal_oral_start_mastication(trigeminal_oral_bridge_t* bridge,
                                      float initial_hardness) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: bridge is NULL");
        return -1;
    }

    bridge->masticating = true;
    bridge->chew_count = 0;
    bridge->food_hardness = initial_hardness;
    bridge->breakdown_progress = 0.0f;
    bridge->prev_bite_force = 0.0f;  /* Reset for chew detection */

    return 0;
}

int trigeminal_oral_update_mastication(trigeminal_oral_bridge_t* bridge,
                                       float bite_force,
                                       float jaw_position) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "trigeminal_oral_update_mastication: bridge is NULL");
        return -1;
    }
    if (!bridge->masticating) {
        return -1;  /* Not masticating, not an error worth reporting to immune */
    }

    (void)jaw_position;

    /* Detect chew cycle - count when force transitions from open to closed */
    if (bite_force > 0.5f && bridge->prev_bite_force <= 0.5f) {
        bridge->chew_count++;

        /* Update breakdown based on bite force and hardness */
        float breakdown_per_chew = bite_force * (1.0f - bridge->food_hardness * 0.5f) * 0.1f;
        bridge->breakdown_progress = nimcp_clampf(
            bridge->breakdown_progress + breakdown_per_chew, 0.0f, 1.0f);
    }
    bridge->prev_bite_force = bite_force;

    return 0;
}

int trigeminal_oral_get_mastication_state(trigeminal_oral_bridge_t* bridge,
                                          uint32_t* chew_count,
                                          float* breakdown_progress,
                                          bool* ready_to_swallow) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: bridge is NULL");
        return -1;
    }

    if (chew_count) *chew_count = bridge->chew_count;
    if (breakdown_progress) *breakdown_progress = bridge->breakdown_progress;
    if (ready_to_swallow) *ready_to_swallow = (bridge->breakdown_progress > 0.8f);

    return 0;
}

int trigeminal_oral_end_mastication(trigeminal_oral_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "trigeminal_oral_end_mastication: bridge is NULL");
        return -1;
    }

    bridge->masticating = false;

    return 0;
}

/* ============================================================================
 * Statistics Implementation
 * ============================================================================ */

int trigeminal_oral_get_stats(const trigeminal_oral_bridge_t* bridge,
                              trigeminal_oral_stats_t* stats) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "trigeminal_oral_end_mastication: required parameter is NULL (bridge, stats)");
        return -1;
    }
    memcpy(stats, &bridge->stats, sizeof(trigeminal_oral_stats_t));
    return 0;
}

int trigeminal_oral_reset_stats(trigeminal_oral_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "trigeminal_oral_reset_stats: bridge is NULL");
        return -1;
    }
    memset(&bridge->stats, 0, sizeof(trigeminal_oral_stats_t));
    return 0;
}

void trigeminal_oral_print_summary(const trigeminal_oral_bridge_t* bridge) {
    if (!bridge) return;

    printf("=== Trigeminal-Oral Bridge Summary ===\n");
    printf("Connected: %s\n", bridge->is_connected ? "Yes" : "No");
    printf("Inputs processed: %lu\n", (unsigned long)bridge->stats.oral_inputs_processed);
    printf("Chemesthesis detected: %lu\n", (unsigned long)bridge->stats.chemesthesis_detected);
    printf("Textures analyzed: %lu\n", (unsigned long)bridge->stats.textures_analyzed);
    printf("Mouthfeels computed: %lu\n", (unsigned long)bridge->stats.mouthfeels_computed);
    printf("Spice tolerance: %.2f\n", bridge->config.spice_tolerance);
    printf("Cooling active: %s (%.2f)\n",
           bridge->cooling_active ? "Yes" : "No", bridge->cooling_intensity);
    printf("======================================\n");
}

/* ============================================================================
 * Utility Functions Implementation
 * ============================================================================ */

const char* trigeminal_oral_region_name(oral_region_t region) {
    switch (region) {
        case ORAL_REGION_TONGUE_TIP:   return "Tongue Tip";
        case ORAL_REGION_TONGUE_BODY:  return "Tongue Body";
        case ORAL_REGION_TONGUE_BACK:  return "Tongue Back";
        case ORAL_REGION_TONGUE_SIDES: return "Tongue Sides";
        case ORAL_REGION_PALATE_HARD:  return "Hard Palate";
        case ORAL_REGION_PALATE_SOFT:  return "Soft Palate";
        case ORAL_REGION_GUMS_UPPER:   return "Upper Gums";
        case ORAL_REGION_GUMS_LOWER:   return "Lower Gums";
        case ORAL_REGION_INNER_CHEEK:  return "Inner Cheek";
        case ORAL_REGION_LIPS:         return "Lips";
        case ORAL_REGION_THROAT:       return "Throat";
        default:                        return "Unknown";
    }
}

const char* trigeminal_chemesthesis_name(chemesthesis_type_t type) {
    switch (type) {
        case CHEMESTHESIS_NONE:        return "None";
        case CHEMESTHESIS_SPICY_HEAT:  return "Spicy Heat";
        case CHEMESTHESIS_COOLING:     return "Cooling";
        case CHEMESTHESIS_TINGLING:    return "Tingling";
        case CHEMESTHESIS_ASTRINGENT:  return "Astringent";
        case CHEMESTHESIS_CARBONATION: return "Carbonation";
        case CHEMESTHESIS_IRRITANT:    return "Irritant";
        default:                        return "Unknown";
    }
}

const char* trigeminal_texture_name(texture_category_t texture) {
    switch (texture) {
        case TEXTURE_SMOOTH:    return "Smooth";
        case TEXTURE_ROUGH:     return "Rough";
        case TEXTURE_CRUNCHY:   return "Crunchy";
        case TEXTURE_CHEWY:     return "Chewy";
        case TEXTURE_CRISPY:    return "Crispy";
        case TEXTURE_CREAMY:    return "Creamy";
        case TEXTURE_GRAINY:    return "Grainy";
        case TEXTURE_FIBROUS:   return "Fibrous";
        case TEXTURE_GELATINOUS: return "Gelatinous";
        case TEXTURE_LIQUID:    return "Liquid";
        default:                 return "Unknown";
    }
}

const char* trigeminal_mouthfeel_name(mouthfeel_quality_t quality) {
    switch (quality) {
        case MOUTHFEEL_NEUTRAL:    return "Neutral";
        case MOUTHFEEL_CREAMY:     return "Creamy";
        case MOUTHFEEL_OILY:       return "Oily";
        case MOUTHFEEL_DRY:        return "Dry";
        case MOUTHFEEL_ASTRINGENT: return "Astringent";
        case MOUTHFEEL_BURNING:    return "Burning";
        case MOUTHFEEL_COOLING:    return "Cooling";
        default:                   return "Unknown";
    }
}

float trigeminal_scoville_to_normalized(float scoville) {
    /* Logarithmic scaling for Scoville */
    if (scoville <= 0.0f) return 0.0f;
    float normalized = logf(scoville + 1.0f) / logf(TRIGEMINAL_SCOVILLE_SCALE + 1.0f);
    return nimcp_clampf(normalized, 0.0f, 1.0f);
}

float trigeminal_normalized_to_scoville(float normalized) {
    if (normalized <= 0.0f) return 0.0f;
    float scoville = expf(normalized * logf(TRIGEMINAL_SCOVILLE_SCALE + 1.0f)) - 1.0f;
    return scoville;
}

/* ============================================================================
 * Cleanup Implementation
 * ============================================================================ */

void trigeminal_texture_free(texture_perception_t* texture) {
    if (!texture) return;
    nimcp_free(texture->texture_profile);
    texture->texture_profile = NULL;
}

void trigeminal_mouthfeel_free(mouthfeel_t* mouthfeel) {
    if (!mouthfeel) return;
    nimcp_free(mouthfeel->mouthfeel_profile);
    mouthfeel->mouthfeel_profile = NULL;
    trigeminal_texture_free(&mouthfeel->texture);
}
