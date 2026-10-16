/**
 * @file nimcp_emotion_substrate_bridge.c
 * @brief Neural substrate bridge for emotion processing implementation
 *
 * WHAT: Bidirectional integration between neural substrate and emotion system
 *
 * WHY: Emotions are metabolically expensive processes requiring ATP. Low ATP
 *      impairs emotion regulation, increases reactivity, and affects valence.
 *
 * HOW: Monitors substrate state (ATP, temperature, metabolic capacity) and
 *      computes effects on emotion intensity, regulation, reactivity, and valence.
 *
 * @author NIMCP Development Team
 * @date 2025-12-19
 * @version 1.0.0
 */

#include "cognitive/emotion/nimcp_emotion_substrate_bridge.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "cognitive/common/nimcp_metabolic_modulation.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "security/nimcp_bbb_helpers.h"
#include "glial/myelin_sheath/nimcp_myelin_math.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(emotion_substrate_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_emotion_substrate_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_emotion_substrate_bridge_mesh_registry = NULL;

nimcp_error_t emotion_substrate_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_emotion_substrate_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "emotion_substrate_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "emotion_substrate_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_emotion_substrate_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_emotion_substrate_bridge_mesh_registry = registry;
    return err;
}

void emotion_substrate_bridge_mesh_unregister(void) {
    if (g_emotion_substrate_bridge_mesh_registry && g_emotion_substrate_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_emotion_substrate_bridge_mesh_registry, g_emotion_substrate_bridge_mesh_id);
        g_emotion_substrate_bridge_mesh_id = 0;
        g_emotion_substrate_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from emotion_substrate_bridge module (instance-level) */
static inline void emotion_substrate_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_emotion_substrate_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_emotion_substrate_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_emotion_substrate_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


/* ========================================================================
 * Helper Functions
 * ======================================================================== */

/**
 * @brief Compute intensity modulation from metabolic state
 *
 * WHAT: Calculate emotion intensity scaling factor
 *
 * WHY: Low ATP causes initial hyperreactivity then blunting
 *
 * HOW: intensity_modulation = (atp > 0.5) ? 1.0 : (1.5 - atp)
 *      Clamped to [0.5, 1.5]
 *
 * BIOLOGICAL BASIS:
 * - High ATP (>0.5): Normal emotion intensity
 * - Low ATP (<0.5): Initial stress hyperreactivity, then blunting
 * - Very low ATP (<0.2): Severe emotional blunting
 */
static float compute_intensity_modulation(float atp_level, float sensitivity) {
    /* Guard: validate inputs */
    if (atp_level < 0.0f) atp_level = 0.0f;
    if (atp_level > 1.0f) atp_level = 1.0f;

    float intensity_mod = 0.0f;

    /* High ATP: normal intensity */
    if (atp_level > 0.5f) {
        intensity_mod = 1.0f;
    }
    /* Low ATP: hyperreactivity then blunting */
    else {
        intensity_mod = 1.5f - atp_level;
    }

    /* Apply sensitivity */
    intensity_mod = 1.0f + (intensity_mod - 1.0f) * sensitivity;

    /* Clamp to valid range */
    return nimcp_myelin_clamp(intensity_mod, 0.5f, 1.5f);
}

/**
 * @brief Compute emotion regulation capacity
 *
 * WHAT: Calculate prefrontal emotion regulation capacity
 *
 * WHY: Emotion regulation requires ATP for prefrontal control
 *
 * HOW: regulation_capacity = clamp(atp_level * metabolic_capacity, 0.2, 1.0)
 *
 * BIOLOGICAL BASIS:
 * - Prefrontal cortex requires ~25% ATP for effective regulation
 * - Low ATP → loss of top-down emotional control
 * - Metabolic capacity affects overall regulation ability
 */
static float compute_regulation_capacity(
    float atp_level,
    float metabolic_capacity,
    float sensitivity
) {
    /* Guard: validate inputs */
    if (atp_level < 0.0f) atp_level = 0.0f;
    if (atp_level > 1.0f) atp_level = 1.0f;
    if (metabolic_capacity < 0.0f) metabolic_capacity = 0.0f;
    if (metabolic_capacity > 1.0f) metabolic_capacity = 1.0f;

    /* Combined regulation capacity */
    float regulation = atp_level * metabolic_capacity;

    /* Apply sensitivity */
    regulation = regulation * sensitivity;

    /* Clamp to valid range */
    return nimcp_myelin_clamp(regulation, 0.2f, 1.0f);
}

/**
 * @brief Compute emotional reactivity threshold
 *
 * WHAT: Calculate threshold for emotional responses
 *
 * WHY: Low ATP increases amygdala reactivity (stress sensitization)
 *
 * HOW: reactivity_threshold = clamp(atp_level, 0.3, 1.0)
 *      Lower threshold = more reactive
 *
 * BIOLOGICAL BASIS:
 * - Low ATP → increased amygdala reactivity
 * - High temperature → reduced reactivity (emotional blunting)
 * - Threshold determines sensitivity to emotional triggers
 */
static float compute_reactivity_threshold(
    float atp_level,
    float temperature,
    float sensitivity
) {
    /* Guard: validate inputs */
    if (atp_level < 0.0f) atp_level = 0.0f;
    if (atp_level > 1.0f) atp_level = 1.0f;

    /* Base threshold from ATP */
    float threshold = atp_level;

    /* Temperature blunting: fever reduces reactivity */
    if (temperature > EMOTION_SUBSTRATE_TEMP_NORMAL) {
        float temp_delta = temperature - EMOTION_SUBSTRATE_TEMP_NORMAL;
        float blunting = temp_delta / 5.0f; /* Max blunting at +5°C */
        threshold += blunting * 0.3f; /* Increase threshold (less reactive) */
    }

    /* Apply sensitivity */
    threshold = threshold * sensitivity;

    /* Clamp to valid range */
    return nimcp_myelin_clamp(threshold, 0.3f, 1.0f);
}

/**
 * @brief Compute valence bias from metabolic state
 *
 * WHAT: Calculate bias toward positive or negative emotions
 *
 * WHY: Metabolic stress biases toward negative emotions
 *
 * HOW: valence_bias = (metabolic_capacity - 0.5) * 0.5
 *      Range: [-0.25, 0.25]
 *
 * BIOLOGICAL BASIS:
 * - Low metabolic capacity → negative bias (stress, depression-like)
 * - High metabolic capacity → positive bias (well-being)
 * - Serotonin/dopamine would modulate this further (future enhancement)
 */
static float compute_valence_bias(
    float metabolic_capacity,
    float sensitivity
) {
    /* Guard: validate inputs */
    if (metabolic_capacity < 0.0f) metabolic_capacity = 0.0f;
    if (metabolic_capacity > 1.0f) metabolic_capacity = 1.0f;

    /* Bias from metabolic capacity */
    float bias = (metabolic_capacity - 0.5f) * 0.5f;

    /* Apply sensitivity */
    bias = bias * sensitivity;

    /* Clamp to valid range */
    return nimcp_myelin_clamp(bias, -0.25f, 0.25f);
}

/**
 * @brief Update bridge statistics
 *
 * WHAT: Update statistical counters and averages
 * WHY: Enable monitoring and analysis
 * HOW: Increment counts, update running averages
 */
static void update_statistics(
    emotion_substrate_bridge_t* bridge,
    const emotion_substrate_effects_t* effects
) {
    /* Guard: validate inputs */
    if (!bridge || !effects) {
        return;
    }

    emotion_substrate_stats_t* stats = &bridge->stats;

    /* Update counts */
    stats->total_updates++;

    if (effects->is_impaired) {
        stats->impairment_count++;
    }

    /* Update intensity modulation average */
    float delta = effects->intensity_modulation - stats->avg_intensity_modulation;
    stats->avg_intensity_modulation += delta / (float)stats->total_updates;

    /* Track minimum regulation capacity */
    if (stats->total_updates == 1 ||
        effects->regulation_capacity < stats->min_regulation_capacity) {
        stats->min_regulation_capacity = effects->regulation_capacity;
    }

    /* Track maximum reactivity threshold */
    if (stats->total_updates == 1 ||
        effects->reactivity_threshold > stats->max_reactivity_threshold) {
        stats->max_reactivity_threshold = effects->reactivity_threshold;
    }

    /* Update valence bias average */
    delta = effects->valence_bias - stats->avg_valence_bias;
    stats->avg_valence_bias += delta / (float)stats->total_updates;

    /* Count specific events */
    if (effects->valence_bias < -0.1f) {
        stats->negative_bias_events++;
    }
    if (effects->valence_bias > 0.1f) {
        stats->positive_bias_events++;
    }
    if (effects->reactivity_threshold < 0.5f) {
        stats->high_reactivity_events++;
    }
    if (effects->intensity_modulation < 0.8f) {
        stats->blunting_events++;
    }
}

/* ========================================================================
 * Configuration Functions
 * ======================================================================== */

void emotion_substrate_default_config(emotion_substrate_config_t* config) {
    /* Guard: validate config pointer */
    if (!config) {
        NIMCP_LOGGING_ERROR("Cannot set default config: NULL pointer");
        return;
    }

    /* Enable all modulation features */
    /* Phase 8: Heartbeat at operation start */
    emotion_substrate_bridge_heartbeat("emotion_subs_emotion_substrate_de", 0.0f);


    config->enable_atp_modulation = true;
    config->enable_serotonin_modulation = true;
    config->enable_dopamine_modulation = true;
    config->enable_norepinephrine_modulation = true;
    config->enable_temperature_modulation = true;

    /* Set sensitivities */
    config->atp_sensitivity = 0.8f;
    config->neurotransmitter_sensitivity = 0.7f;
    config->temperature_sensitivity = 0.6f;

    /* Update interval */
    config->update_interval_ms = 100;

    /* Enable bio-async */
    config->enable_bio_async = true;

    NIMCP_LOGGING_DEBUG("Emotion substrate config initialized with defaults");
}

/* ========================================================================
 * Lifecycle Functions
 * ======================================================================== */

emotion_substrate_bridge_t* emotion_substrate_bridge_create(
    const emotion_substrate_config_t* config,
    emotional_system_t* emotion_system,
    neural_substrate_t* substrate
) {
    /* Guard: validate required pointers */
    if (!emotion_system) {
        NIMCP_LOGGING_ERROR("Cannot create emotion substrate bridge: NULL emotion_system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_system is NULL");

        return NULL;
    }

    if (!substrate) {
        NIMCP_LOGGING_ERROR("Cannot create emotion substrate bridge: NULL substrate");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate is NULL");


        return NULL;
    }

    /* Allocate bridge structure */
    /* Phase 8: Heartbeat at operation start */
    emotion_substrate_bridge_heartbeat("emotion_subs_create", 0.0f);


    emotion_substrate_bridge_t* bridge = (emotion_substrate_bridge_t*)
        nimcp_malloc(sizeof(emotion_substrate_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate emotion substrate bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NULL;
    }

    /* Zero initialize */
    memset(bridge, 0, sizeof(emotion_substrate_bridge_t));

    /* Set pointers */
    bridge->substrate = substrate;
    bridge->emotion_system = emotion_system;

    /* Set configuration */
    if (config) {
        memcpy(&bridge->config, config, sizeof(emotion_substrate_config_t));
    } else {
        emotion_substrate_default_config(&bridge->config);
    }

    /* Initialize effects with neutral values */
    bridge->effects.intensity_modulation = 1.0f;
    bridge->effects.regulation_capacity = 1.0f;
    bridge->effects.reactivity_threshold = 0.5f;
    bridge->effects.valence_bias = 0.0f;
    bridge->effects.is_impaired = false;

    /* Initialize statistics */
    bridge->stats.min_regulation_capacity = 1.0f;
    bridge->stats.max_reactivity_threshold = 0.0f;

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "emotion_substrate") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("Failed to create mutex for emotion substrate bridge");
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "emotion_substrate_bridge_create: bridge->base is NULL");
        return NULL;
    }

    /* Connect to bio-async if enabled */
    if (bridge->config.enable_bio_async) {
        emotion_substrate_connect_bio_async(bridge);
    }

    bridge->initialized = true;

    NIMCP_LOGGING_INFO("Emotion substrate bridge created successfully");

    return bridge;
}

void emotion_substrate_bridge_destroy(emotion_substrate_bridge_t* bridge) {
    /* Guard: validate bridge pointer */
    if (!bridge) {
        return;
    }

    /* Disconnect from bio-async */
    /* Phase 8: Heartbeat at operation start */
    emotion_substrate_bridge_heartbeat("emotion_subs_destroy", 0.0f);


    if (bridge->base.bio_async_enabled) {
        emotion_substrate_disconnect_bio_async(bridge);
    }

    /* Destroy mutex */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    /* Free bridge structure */
    nimcp_free(bridge);
    bridge = NULL;

    NIMCP_LOGGING_INFO("Emotion substrate bridge destroyed");
}

/* ========================================================================
 * Bio-Async Integration
 * ======================================================================== */

int emotion_substrate_connect_bio_async(emotion_substrate_bridge_t* bridge) {
    /* Guard: validate bridge pointer */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Cannot connect bio-async: NULL bridge");
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* Guard: check if already connected */
    /* Phase 8: Heartbeat at operation start */
    emotion_substrate_bridge_heartbeat("emotion_subs_emotion_substrate_co", 0.0f);


    if (bridge->base.bio_async_enabled) {
        NIMCP_LOGGING_DEBUG("Bio-async already connected");
        return NIMCP_SUCCESS;
    }

    /* Register with bio-async router */
    bio_module_info_t info = {
        .module_id = BIO_MODULE_SUBSTRATE_EMOTION,
        .module_name = "emotion_substrate_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Emotion substrate bridge connected to bio-async router");
        return NIMCP_SUCCESS;
    }

    /* Bio-async router not available - this is expected in many environments */
    NIMCP_LOGGING_DEBUG("Bio-async router not available, skipping registration");
    return NIMCP_SUCCESS;
}

int emotion_substrate_disconnect_bio_async(emotion_substrate_bridge_t* bridge) {
    /* Guard: validate bridge pointer */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Cannot disconnect bio-async: NULL bridge");
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* Guard: check if connected */
    if (!bridge->base.bio_async_enabled) {
        return NIMCP_SUCCESS;
    }

    /* Deregister from bio-async router */
    /* Phase 8: Heartbeat at operation start */
    emotion_substrate_bridge_heartbeat("emotion_subs_emotion_substrate_di", 0.0f);


    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }

    bridge->base.bio_async_enabled = false;

    NIMCP_LOGGING_INFO("Emotion substrate bridge disconnected from bio-async");

    return NIMCP_SUCCESS;
}

bool emotion_substrate_is_bio_async_connected(
    const emotion_substrate_bridge_t* bridge
) {
    /* Guard: validate bridge pointer */
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    emotion_substrate_bridge_heartbeat("emotion_subs_emotion_substrate_is", 0.0f);


    return bridge->base.bio_async_enabled;
}

/* ========================================================================
 * Update Functions
 * ======================================================================== */

int emotion_substrate_update(emotion_substrate_bridge_t* bridge) {
    /* Guard: validate bridge pointer */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Cannot update emotion substrate: NULL bridge");
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* Guard: check initialization */
    if (!bridge->initialized) {
        NIMCP_LOGGING_ERROR("Cannot update emotion substrate: not initialized");
        return NIMCP_ERROR_INVALID_STATE;
    }

    /* Lock for thread safety */
    /* Phase 8: Heartbeat at operation start */
    emotion_substrate_bridge_heartbeat("emotion_subs_emotion_substrate_up", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Query substrate metabolic state */
    substrate_metabolic_state_t metabolic;
    int ret = substrate_get_metabolic_state(bridge->substrate, &metabolic);
    if (ret != NIMCP_SUCCESS) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_LOGGING_ERROR("Failed to get substrate metabolic state");
        return ret;
    }

    /* Query substrate physical state */
    substrate_physical_state_t physical;
    ret = substrate_get_physical_state(bridge->substrate, &physical);
    if (ret != NIMCP_SUCCESS) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_LOGGING_ERROR("Failed to get substrate physical state");
        return ret;
    }

    /* Get configuration sensitivities */
    float atp_sensitivity = bridge->config.atp_sensitivity;
    float temp_sensitivity = bridge->config.temperature_sensitivity;

    /* Compute intensity modulation */
    if (bridge->config.enable_atp_modulation) {
        bridge->effects.intensity_modulation =
            compute_intensity_modulation(metabolic.atp_level, atp_sensitivity);
    } else {
        bridge->effects.intensity_modulation = 1.0f;
    }

    /* Compute regulation capacity */
    if (bridge->config.enable_atp_modulation) {
        bridge->effects.regulation_capacity =
            compute_regulation_capacity(
                metabolic.atp_level,
                metabolic.metabolic_capacity,
                atp_sensitivity
            );
    } else {
        bridge->effects.regulation_capacity = 1.0f;
    }

    /* Compute reactivity threshold */
    if (bridge->config.enable_atp_modulation &&
        bridge->config.enable_temperature_modulation) {
        bridge->effects.reactivity_threshold =
            compute_reactivity_threshold(
                metabolic.atp_level,
                physical.temperature,
                temp_sensitivity
            );
    } else if (bridge->config.enable_atp_modulation) {
        bridge->effects.reactivity_threshold =
            compute_reactivity_threshold(
                metabolic.atp_level,
                EMOTION_SUBSTRATE_TEMP_NORMAL,
                atp_sensitivity
            );
    } else {
        bridge->effects.reactivity_threshold = 0.5f;
    }

    /* Compute valence bias */
    if (bridge->config.enable_atp_modulation) {
        bridge->effects.valence_bias =
            compute_valence_bias(
                metabolic.metabolic_capacity,
                atp_sensitivity
            );
    } else {
        bridge->effects.valence_bias = 0.0f;
    }

    /* Determine impairment status */
    bridge->effects.is_impaired =
        (bridge->effects.regulation_capacity < 0.6f);

    /* Update statistics */
    update_statistics(bridge, &bridge->effects);

    /* Update timestamp */
    bridge->stats.last_update_time = nimcp_time_get_ms();

    /* Unlock */
    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Emotion substrate effects updated successfully");

    return NIMCP_SUCCESS;
}

/* ========================================================================
 * Query Functions
 * ======================================================================== */

float emotion_substrate_get_intensity_mod(
    const emotion_substrate_bridge_t* bridge
) {
    /* Guard: validate bridge pointer */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Cannot get intensity modulation: NULL bridge");
        return 1.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    emotion_substrate_bridge_heartbeat("emotion_subs_emotion_substrate_ge", 0.0f);

    float result;
    nimcp_mutex_lock(bridge->base.mutex);
    result = bridge->effects.intensity_modulation;
    nimcp_mutex_unlock(bridge->base.mutex);
    return result;
}

float emotion_substrate_get_regulation_capacity(
    const emotion_substrate_bridge_t* bridge
) {
    /* Guard: validate bridge pointer */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Cannot get regulation capacity: NULL bridge");
        return 1.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    emotion_substrate_bridge_heartbeat("emotion_subs_emotion_substrate_ge", 0.0f);

    float result;
    nimcp_mutex_lock(bridge->base.mutex);
    result = bridge->effects.regulation_capacity;
    nimcp_mutex_unlock(bridge->base.mutex);
    return result;
}

float emotion_substrate_get_reactivity_threshold(
    const emotion_substrate_bridge_t* bridge
) {
    /* Guard: validate bridge pointer */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Cannot get reactivity threshold: NULL bridge");
        return 0.5f;
    }

    /* Phase 8: Heartbeat at operation start */
    emotion_substrate_bridge_heartbeat("emotion_subs_emotion_substrate_ge", 0.0f);

    float result;
    nimcp_mutex_lock(bridge->base.mutex);
    result = bridge->effects.reactivity_threshold;
    nimcp_mutex_unlock(bridge->base.mutex);
    return result;
}

float emotion_substrate_get_valence_bias(
    const emotion_substrate_bridge_t* bridge
) {
    /* Guard: validate bridge pointer */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Cannot get valence bias: NULL bridge");
        return 0.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    emotion_substrate_bridge_heartbeat("emotion_subs_emotion_substrate_ge", 0.0f);

    float result;
    nimcp_mutex_lock(bridge->base.mutex);
    result = bridge->effects.valence_bias;
    nimcp_mutex_unlock(bridge->base.mutex);
    return result;
}

emotion_substrate_effects_t emotion_substrate_get_effects(
    const emotion_substrate_bridge_t* bridge
) {
    /* Phase 8: Heartbeat at operation start */
    emotion_substrate_bridge_heartbeat("emotion_subs_emotion_substrate_ge", 0.0f);

    emotion_substrate_effects_t effects = {
        .intensity_modulation = 1.0f,
        .regulation_capacity = 1.0f,
        .reactivity_threshold = 0.5f,
        .valence_bias = 0.0f,
        .is_impaired = false
    };

    /* Guard: validate bridge pointer */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Cannot get effects: NULL bridge");
        return effects;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    effects = bridge->effects;
    nimcp_mutex_unlock(bridge->base.mutex);
    return effects;
}

bool emotion_substrate_is_impaired(
    const emotion_substrate_bridge_t* bridge
) {
    /* Guard: validate bridge pointer */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Cannot check impairment: NULL bridge");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    emotion_substrate_bridge_heartbeat("emotion_subs_emotion_substrate_is", 0.0f);

    bool result;
    nimcp_mutex_lock(bridge->base.mutex);
    result = bridge->effects.is_impaired;
    nimcp_mutex_unlock(bridge->base.mutex);
    return result;
}

emotion_substrate_stats_t emotion_substrate_get_stats(
    const emotion_substrate_bridge_t* bridge
) {
    /* Phase 8: Heartbeat at operation start */
    emotion_substrate_bridge_heartbeat("emotion_subs_emotion_substrate_ge", 0.0f);

    emotion_substrate_stats_t stats;
    memset(&stats, 0, sizeof(emotion_substrate_stats_t));

    /* Guard: validate bridge pointer */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Cannot get stats: NULL bridge");
        return stats;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);
    return stats;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * WHAT: Query knowledge graph for Emotion Substrate Bridge self-knowledge
 * WHY:  Enable self-awareness about module's role and connections
 * HOW:  Query KG for entity observations and relations
 */
int emotion_substrate_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    emotion_substrate_bridge_heartbeat("emotion_subs_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Emotion_Substrate_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                emotion_substrate_bridge_heartbeat("emotion_subs_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            NIMCP_LOGGING_DEBUG("Emotion Substrate Bridge self-knowledge: %s", self->observations[i]);
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Emotion_Substrate_Bridge");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Emotion_Substrate_Bridge");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-level health agent setter
 * ============================================================================ */
void emotion_substrate_bridge_set_instance_health_agent(emotion_substrate_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (bridge) {
        /* Instance-level setter - substrate bridge struct is in header (opaque) */
        g_emotion_substrate_bridge_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training stubs
 * ============================================================================ */
int emotion_substrate_bridge_training_begin(emotion_substrate_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "emotion_substrate_bridge_training_begin: NULL argument");
        return -1;
    }
    emotion_substrate_bridge_heartbeat_instance(NULL, "emotion_subs_training_begin", 0.0f);
    return 0;
}

int emotion_substrate_bridge_training_end(emotion_substrate_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "emotion_substrate_bridge_training_end: NULL argument");
        return -1;
    }
    emotion_substrate_bridge_heartbeat_instance(NULL, "emotion_subs_training_end", 1.0f);
    return 0;
}

int emotion_substrate_bridge_training_step(emotion_substrate_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "emotion_substrate_bridge_training_step: NULL argument");
        return -1;
    }
    emotion_substrate_bridge_heartbeat_instance(NULL, "emotion_subs_training_step", progress);
    return 0;
}

/* ============================================================================
 * Security Integration (BBB)
 * ============================================================================ */
BRIDGE_DEFINE_SECURITY_SETTERS(emotion_substrate_bridge)
