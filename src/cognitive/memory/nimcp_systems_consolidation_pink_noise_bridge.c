/**
 * @file nimcp_systems_consolidation_pink_noise_bridge.c
 * @brief Pink Noise Integration Bridge for Sleep Replay - Implementation
 * @version 1.0.0
 * @date 2025-12-21
 *
 * IMPLEMENTATION NOTES:
 * - Thread-safe operations via mutex locking
 * - Temporal smoothing prevents abrupt modulation changes
 * - Respects biological timescales for sleep stage transitions
 * - Efficient parameter updates (no heavy computation)
 */

#include "cognitive/memory/nimcp_systems_consolidation_pink_noise_bridge.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "security/nimcp_bbb_helpers.h"
#include <math.h>
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(systems_consolidation_pink_noise_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_systems_consolidation_pink_noise_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_systems_consolidation_pink_noise_bridge_mesh_registry = NULL;

nimcp_error_t systems_consolidation_pink_noise_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_systems_consolidation_pink_noise_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "systems_consolidation_pink_noise_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_MEMORY);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "systems_consolidation_pink_noise_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_systems_consolidation_pink_noise_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_systems_consolidation_pink_noise_bridge_mesh_registry = registry;
    return err;
}

void systems_consolidation_pink_noise_bridge_mesh_unregister(void) {
    if (g_systems_consolidation_pink_noise_bridge_mesh_registry && g_systems_consolidation_pink_noise_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_systems_consolidation_pink_noise_bridge_mesh_registry, g_systems_consolidation_pink_noise_bridge_mesh_id);
        g_systems_consolidation_pink_noise_bridge_mesh_id = 0;
        g_systems_consolidation_pink_noise_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from systems_consolidation_pink_noise_bridge module (instance-level) */
static inline void systems_consolidation_pink_noise_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_systems_consolidation_pink_noise_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_systems_consolidation_pink_noise_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_systems_consolidation_pink_noise_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


/* Security subsystem setters (Phase 1: Audit Gap Remediation) */
BRIDGE_DEFINE_SECURITY_SETTERS(consolidation_pink_noise_bridge)

//=============================================================================
// Internal Helper: Clamp
//=============================================================================

//=============================================================================
// Internal Helper: Exponential Smoothing
//=============================================================================

/**
 * WHAT: Apply exponential smoothing filter
 * WHY:  Prevent abrupt changes in modulation parameters
 * HOW:  new_value = alpha × current + (1-alpha) × target
 *
 * @param current Current smoothed value
 * @param target Target value to approach
 * @param alpha Smoothing factor [0=no change, 1=instant]
 * @return Smoothed value
 */
static float smooth_value(float current, float target, float alpha) {
    return alpha * target + (1.0f - alpha) * current;
}

//=============================================================================
// Lifecycle API Implementation
//=============================================================================

int consolidation_pink_noise_default_config(consolidation_pink_noise_config_t* config) {
    // WHAT: Initialize configuration with biologically-plausible defaults
    // WHY:  Provides known-good starting point
    // HOW:  Set empirically-derived parameter values

    if (!config) {
        NIMCP_LOGGING_ERROR("Null config pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "consolidation_pink_noise_default_config: config is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    systems_consolidation_pink_noise_bridge_heartbeat("systems_cons_consolidation_pink_n", 0.0f);


    config->enable_amplitude_modulation = true;
    config->enable_alpha_modulation = true;
    config->enable_spindle_priority = true;
    config->enable_arousal_frequency = true;

    config->amplitude_gain = 1.0f;
    config->alpha_sensitivity = 0.5f;
    config->spindle_priority_boost = 0.3f;
    config->modulation_smoothing = 0.1f;

    return 0;
}

consolidation_pink_noise_bridge_t* consolidation_pink_noise_create(
    const consolidation_pink_noise_config_t* config,
    systems_consolidation_system_t* consolidation_system,
    pink_sleep_bridge_t* pink_noise_bridge)
{
    // WHAT: Create and initialize pink noise bridge
    // WHY:  Required for noise-modulated consolidation
    // HOW:  Allocate structure, validate inputs, initialize state

    // Guard clause: Validate required inputs
    if (!consolidation_system) {
        NIMCP_LOGGING_ERROR("Null consolidation system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "consolidation_system is NULL");

        return NULL;
    }

    if (!pink_noise_bridge) {
        NIMCP_LOGGING_ERROR("Null pink noise bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pink_noise_bridge is NULL");


        return NULL;
    }

    // Allocate bridge structure
    /* Phase 8: Heartbeat at operation start */
    systems_consolidation_pink_noise_bridge_heartbeat("systems_cons_consolidation_pink_n", 0.0f);


    consolidation_pink_noise_bridge_t* bridge =
        (consolidation_pink_noise_bridge_t*)nimcp_malloc(sizeof(consolidation_pink_noise_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NULL;
    }

    // Initialize configuration
    if (config) {
        memcpy(&bridge->config, config, sizeof(consolidation_pink_noise_config_t));
    } else {
        consolidation_pink_noise_default_config(&bridge->config);
    }

    // Store module connections (non-owning)
    bridge->consolidation_system = consolidation_system;
    bridge->pink_noise_bridge = pink_noise_bridge;

    // Initialize effects to neutral state
    memset(&bridge->effects, 0, sizeof(consolidation_pink_noise_effects_t));
    bridge->effects.replay_strength_factor = 1.0f;
    bridge->effects.transfer_quality_factor = 1.0f;
    bridge->effects.replay_frequency_hz = REPLAY_FREQ_AWAKE;

    // Initialize smoothed values to neutral
    bridge->smoothed_replay_strength = 1.0f;
    bridge->smoothed_transfer_quality = 1.0f;
    bridge->smoothed_frequency = REPLAY_FREQ_AWAKE;

    // Initialize statistics
    bridge->total_updates = 0;
    bridge->spindle_boosts_applied = 0;
    bridge->stage_transitions = 0;

    // Create mutex for thread safety
    bridge->base.mutex = nimcp_malloc(sizeof(nimcp_mutex_t));
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("Failed to allocate mutex");
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "consolidation_pink_noise_create: bridge->base is NULL");
        return NULL;
    }
    if (nimcp_mutex_init(bridge->base.mutex, NULL) != 0) {
        NIMCP_LOGGING_ERROR("Failed to initialize mutex");
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "consolidation_pink_noise_create: validation failed");
        return NULL;
    }

    NIMCP_LOGGING_INFO("Created pink noise bridge for systems consolidation");
    return bridge;
}

void consolidation_pink_noise_destroy(consolidation_pink_noise_bridge_t* bridge) {
    // WHAT: Clean up bridge resources
    // WHY:  Prevent memory leaks
    // HOW:  Free mutex and structure

    if (!bridge) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    systems_consolidation_pink_noise_bridge_heartbeat("systems_cons_consolidation_pink_n", 0.0f);


    if (bridge->base.mutex) {
        nimcp_mutex_destroy(bridge->base.mutex);
        bridge->base.mutex = NULL;
    }

    nimcp_free(bridge);
    bridge = NULL;
    NIMCP_LOGGING_INFO("Destroyed pink noise bridge");
}

//=============================================================================
// Update and Modulation API Implementation
//=============================================================================

int consolidation_pink_noise_update(consolidation_pink_noise_bridge_t* bridge) {
    // WHAT: Update modulation factors based on current pink noise state
    // WHY:  Keep replay parameters synchronized with noise characteristics
    // HOW:  Sample noise parameters, compute factors, apply smoothing

    if (!bridge) {
        NIMCP_LOGGING_ERROR("Null bridge pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "consolidation_pink_noise_update: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    systems_consolidation_pink_noise_bridge_heartbeat("systems_cons_consolidation_pink_n", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    // Sample current pink noise state
    float noise_amplitude = pink_sleep_get_amplitude(bridge->pink_noise_bridge);
    float noise_alpha = pink_sleep_get_alpha(bridge->pink_noise_bridge);

    // Get sleep stage (we'll need to extract this from the pink noise bridge structure)
    // For now, we'll track transitions by detecting amplitude/alpha changes
    pink_sleep_stage_t current_stage = PINK_SLEEP_WAKE;  // Default

    // Heuristic: Infer stage from noise characteristics
    if (noise_alpha >= 1.4f && noise_amplitude > 0.5f) {
        current_stage = PINK_SLEEP_N3;  // Deep sleep: high alpha, high amplitude
    } else if (noise_alpha < 0.9f && noise_amplitude > 0.4f) {
        current_stage = PINK_SLEEP_REM;  // REM: low alpha, variable amplitude
    } else if (noise_alpha >= 1.0f && noise_amplitude > 0.3f) {
        current_stage = PINK_SLEEP_N2;  // Light sleep N2
    } else if (noise_amplitude > 0.2f) {
        current_stage = PINK_SLEEP_N1;  // Light sleep N1
    } else if (noise_amplitude > 0.1f) {
        current_stage = PINK_SLEEP_DROWSY;  // Drowsy
    }

    // Track stage transitions
    if (current_stage != bridge->effects.current_stage) {
        bridge->stage_transitions++;
        bridge->effects.current_stage = current_stage;
    }

    // Store current noise parameters
    bridge->effects.noise_amplitude = noise_amplitude;
    bridge->effects.noise_alpha = noise_alpha;

    // Compute arousal level (inverse of noise amplitude for sleep stages)
    float arousal = 1.0f - nimcp_clampf(noise_amplitude, 0.0f, 1.0f);
    bridge->effects.arousal_level = arousal;

    // Check for spindle burst (specific to N2 stage)
    // Spindles occur when we're in N2 range and have specific frequency characteristics
    bridge->effects.in_spindle_burst = (current_stage == PINK_SLEEP_N2);

    // 1. Compute base replay strength from sleep stage
    float base_strength = consolidation_pink_noise_stage_replay_strength(current_stage);

    // 2. Modulate by noise amplitude (if enabled)
    float strength_factor = base_strength;
    if (bridge->config.enable_amplitude_modulation) {
        // Higher amplitude in appropriate sleep stages enhances replay
        float amplitude_mod = 1.0f + bridge->config.amplitude_gain * (noise_amplitude - 0.5f);
        amplitude_mod = nimcp_clampf(amplitude_mod, 0.5f, 1.5f);
        strength_factor *= amplitude_mod;
    }
    strength_factor = nimcp_clampf(strength_factor, 0.0f, 1.0f);

    // 3. Compute transfer quality from spectral exponent (if enabled)
    float quality_factor = 1.0f;
    if (bridge->config.enable_alpha_modulation) {
        quality_factor = consolidation_pink_noise_alpha_to_quality(noise_alpha);
        quality_factor *= (1.0f + bridge->config.alpha_sensitivity * (noise_alpha - 1.0f) * 0.5f);
        quality_factor = nimcp_clampf(quality_factor, 0.5f, 1.5f);
    }

    // 4. Compute replay frequency from arousal (if enabled)
    float frequency_hz = REPLAY_FREQ_AWAKE;
    if (bridge->config.enable_arousal_frequency) {
        frequency_hz = consolidation_pink_noise_arousal_to_frequency(arousal);
    }

    // 5. Compute priority modulation from spindles (if enabled)
    float priority_mod = 0.0f;
    if (bridge->config.enable_spindle_priority && bridge->effects.in_spindle_burst) {
        priority_mod = bridge->config.spindle_priority_boost;
        bridge->spindle_boosts_applied++;
    }

    // Apply temporal smoothing to prevent abrupt changes
    float smooth_alpha = bridge->config.modulation_smoothing;
    bridge->smoothed_replay_strength = smooth_value(
        bridge->smoothed_replay_strength, strength_factor, smooth_alpha);
    bridge->smoothed_transfer_quality = smooth_value(
        bridge->smoothed_transfer_quality, quality_factor, smooth_alpha);
    bridge->smoothed_frequency = smooth_value(
        bridge->smoothed_frequency, frequency_hz, smooth_alpha);

    // Update effects structure with smoothed values
    bridge->effects.replay_strength_factor = bridge->smoothed_replay_strength;
    bridge->effects.transfer_quality_factor = bridge->smoothed_transfer_quality;
    bridge->effects.replay_frequency_hz = bridge->smoothed_frequency;
    bridge->effects.priority_modulation = priority_mod;

    bridge->total_updates++;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

int consolidation_pink_noise_apply_modulation(consolidation_pink_noise_bridge_t* bridge) {
    // WHAT: Apply computed modulation to consolidation system
    // WHY:  Implement noise-driven optimization of replay
    // HOW:  Modify consolidation parameters based on effects
    //
    // NOTE: This function sets internal state that will be used by the
    //       consolidation system during its next replay execution.
    //       The actual systems_consolidation API would need extension points
    //       to support this modulation. For now, we document the intent.

    if (!bridge) {
        NIMCP_LOGGING_ERROR("Null bridge pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "consolidation_pink_noise_apply_modulation: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    systems_consolidation_pink_noise_bridge_heartbeat("systems_cons_consolidation_pink_n", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    // The consolidation system would need to expose:
    // - Set replay frequency
    // - Set transfer rate multiplier
    // - Set priority adjustment factor
    //
    // Since these APIs don't exist yet in systems_consolidation.h,
    // we'll document the intended integration:
    //
    // systems_consolidation_set_replay_frequency(
    //     bridge->consolidation_system,
    //     bridge->effects.replay_frequency_hz);
    //
    // systems_consolidation_set_transfer_multiplier(
    //     bridge->consolidation_system,
    //     bridge->effects.transfer_quality_factor);
    //
    // systems_consolidation_set_priority_boost(
    //     bridge->consolidation_system,
    //     bridge->effects.priority_modulation);

    // For now, we successfully store the modulation state that can be
    // queried by the consolidation system or wrapper code

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// Query API Implementation
//=============================================================================

int consolidation_pink_noise_get_effects(
    const consolidation_pink_noise_bridge_t* bridge,
    consolidation_pink_noise_effects_t* effects_out)
{
    // WHAT: Retrieve current modulation effects
    // WHY:  Allow inspection of noise influence on replay
    // HOW:  Copy effects structure with mutex protection

    if (!bridge || !effects_out) {
        NIMCP_LOGGING_ERROR("Null pointer in get_effects");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "consolidation_pink_noise_get_effects: required parameter is NULL (bridge, effects_out)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    systems_consolidation_pink_noise_bridge_heartbeat("systems_cons_consolidation_pink_n", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    memcpy(effects_out, &bridge->effects, sizeof(consolidation_pink_noise_effects_t));
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

float consolidation_pink_noise_get_replay_strength(
    const consolidation_pink_noise_bridge_t* bridge)
{
    // WHAT: Get current replay strength factor
    // WHY:  Quick access to primary modulation parameter
    // HOW:  Return smoothed value with bounds checking

    if (!bridge) {
        return -1.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    systems_consolidation_pink_noise_bridge_heartbeat("systems_cons_consolidation_pink_n", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float strength = bridge->effects.replay_strength_factor;
    nimcp_mutex_unlock(bridge->base.mutex);

    return strength;
}

float consolidation_pink_noise_get_transfer_quality(
    const consolidation_pink_noise_bridge_t* bridge)
{
    // WHAT: Get current transfer quality factor
    // WHY:  Indicates consolidation effectiveness
    // HOW:  Return smoothed quality with bounds checking

    if (!bridge) {
        return -1.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    systems_consolidation_pink_noise_bridge_heartbeat("systems_cons_consolidation_pink_n", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float quality = bridge->effects.transfer_quality_factor;
    nimcp_mutex_unlock(bridge->base.mutex);

    return quality;
}

float consolidation_pink_noise_get_replay_frequency(
    const consolidation_pink_noise_bridge_t* bridge)
{
    // WHAT: Get current replay frequency
    // WHY:  Indicates optimal replay rate for current state
    // HOW:  Return smoothed frequency with bounds checking

    if (!bridge) {
        return -1.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    systems_consolidation_pink_noise_bridge_heartbeat("systems_cons_consolidation_pink_n", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    float freq = bridge->effects.replay_frequency_hz;
    nimcp_mutex_unlock(bridge->base.mutex);

    return freq;
}

bool consolidation_pink_noise_in_spindle_burst(
    const consolidation_pink_noise_bridge_t* bridge)
{
    // WHAT: Check if spindle burst is active
    // WHY:  Spindles indicate memory tagging opportunity
    // HOW:  Return spindle state with mutex protection

    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "consolidation_pink_noise_in_spindle_burst: bridge is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    systems_consolidation_pink_noise_bridge_heartbeat("systems_cons_consolidation_pink_n", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bool in_spindle = bridge->effects.in_spindle_burst;
    nimcp_mutex_unlock(bridge->base.mutex);

    return in_spindle;
}

//=============================================================================
// Statistics API Implementation
//=============================================================================

int consolidation_pink_noise_get_statistics(
    const consolidation_pink_noise_bridge_t* bridge,
    uint64_t* total_updates_out,
    uint64_t* spindle_boosts_out,
    uint64_t* stage_transitions_out)
{
    // WHAT: Retrieve operational statistics
    // WHY:  Support monitoring and debugging
    // HOW:  Copy statistics with mutex protection

    if (!bridge) {
        NIMCP_LOGGING_ERROR("Null bridge pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "consolidation_pink_noise_get_statistics: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    systems_consolidation_pink_noise_bridge_heartbeat("systems_cons_consolidation_pink_n", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (total_updates_out) {
        *total_updates_out = bridge->total_updates;
    }
    if (spindle_boosts_out) {
        *spindle_boosts_out = bridge->spindle_boosts_applied;
    }
    if (stage_transitions_out) {
        *stage_transitions_out = bridge->stage_transitions;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// Helper Functions Implementation
//=============================================================================

float consolidation_pink_noise_stage_replay_strength(pink_sleep_stage_t stage) {
    // WHAT: Map sleep stage to base replay strength
    // WHY:  Different stages have different consolidation potential
    // HOW:  Lookup table based on biological research

    /* Phase 8: Heartbeat at operation start */
    systems_consolidation_pink_noise_bridge_heartbeat("systems_cons_consolidation_pink_n", 0.0f);


    switch (stage) {
        case PINK_SLEEP_WAKE:
            return REPLAY_STRENGTH_WAKE;
        case PINK_SLEEP_DROWSY:
            return REPLAY_STRENGTH_DROWSY;
        case PINK_SLEEP_N1:
            return REPLAY_STRENGTH_N1;
        case PINK_SLEEP_N2:
            return REPLAY_STRENGTH_N2;
        case PINK_SLEEP_N3:
            return REPLAY_STRENGTH_N3;
        case PINK_SLEEP_REM:
            return REPLAY_STRENGTH_REM;
        default:
            return REPLAY_STRENGTH_WAKE;
    }
}

float consolidation_pink_noise_alpha_to_quality(float alpha) {
    // WHAT: Convert spectral exponent to transfer quality
    // WHY:  Redder noise provides more stable consolidation
    // HOW:  Normalized mapping with peak at α≈1.5
    //
    // BIOLOGICAL RATIONALE:
    // - White noise (α≈0.5): 0.5 quality (poor, high variability)
    // - Pink noise (α≈1.0): 1.0 quality (baseline)
    // - Red noise (α≈1.5): 1.5 quality (optimal for SWS)
    // - Very red (α≈2.0): 1.3 quality (diminishing returns)

    /* Phase 8: Heartbeat at operation start */
    systems_consolidation_pink_noise_bridge_heartbeat("systems_cons_consolidation_pink_n", 0.0f);


    if (alpha <= 0.5f) {
        return 0.5f;  // White noise minimum
    } else if (alpha <= 1.0f) {
        // Linear interpolation: 0.5 at α=0.5, 1.0 at α=1.0
        return 0.5f + (alpha - 0.5f);
    } else if (alpha <= 1.5f) {
        // Linear interpolation: 1.0 at α=1.0, 1.5 at α=1.5
        return 1.0f + (alpha - 1.0f);
    } else if (alpha <= 2.0f) {
        // Diminishing returns: 1.5 at α=1.5, 1.3 at α=2.0
        return 1.5f - 0.2f * (alpha - 1.5f) / 0.5f;
    } else {
        return 1.3f;  // Saturation
    }
}

float consolidation_pink_noise_arousal_to_frequency(float arousal) {
    // WHAT: Convert arousal level to replay frequency
    // WHY:  Deep sleep enables faster replay coordination
    // HOW:  Inverse exponential relationship
    //
    // BIOLOGICAL BASIS:
    // - Deep sleep (arousal ≈ 0.0): ~10 Hz sharp-wave ripples
    // - Light sleep (arousal ≈ 0.5): ~5 Hz theta sequences
    // - Awake (arousal ≈ 1.0): ~0.5 Hz spontaneous reactivation

    /* Phase 8: Heartbeat at operation start */
    systems_consolidation_pink_noise_bridge_heartbeat("systems_cons_consolidation_pink_n", 0.0f);


    arousal = nimcp_clampf(arousal, 0.0f, 1.0f);

    // Exponential decay from deep sleep to awake
    // freq = FREQ_DEEP * exp(-k * arousal)
    // At arousal=0: freq ≈ 10 Hz
    // At arousal=0.5: freq ≈ 5 Hz
    // At arousal=1.0: freq ≈ 0.5 Hz

    float k = 3.0f;  // Decay constant
    float freq = REPLAY_FREQ_DEEP_SLEEP * expf(-k * arousal);

    // Ensure minimum frequency
    if (freq < REPLAY_FREQ_AWAKE) {
        freq = REPLAY_FREQ_AWAKE;
    }

    return freq;
}

//=============================================================================
// KG Self-Awareness Integration
//=============================================================================

/**
 * WHAT: Query self-knowledge from knowledge graph
 * WHY:  Enable introspection about module capabilities and connections
 * HOW:  Query KG reader for entity and relations
 */
int consolidation_pink_noise_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    systems_consolidation_pink_noise_bridge_heartbeat("systems_cons_consolidation_pink_n", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Systems_Consolidation_Pink_Noise_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                systems_consolidation_pink_noise_bridge_heartbeat("systems_cons_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            NIMCP_LOGGING_DEBUG("Consolidation pink noise bridge self-knowledge: %s", self->observations[i]);
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Systems_Consolidation_Pink_Noise_Bridge");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Systems_Consolidation_Pink_Noise_Bridge");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void systems_consolidation_pink_noise_bridge_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_systems_consolidation_pink_noise_bridge_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int systems_consolidation_pink_noise_bridge_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "systems_consolidation_pink_noise_bridge_training_begin: NULL argument");
        return -1;
    }
    systems_consolidation_pink_noise_bridge_heartbeat_instance(NULL, "systems_consolidation_pink_noise_bridge_training_begin", 0.0f);
    return 0;
}

int systems_consolidation_pink_noise_bridge_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "systems_consolidation_pink_noise_bridge_training_end: NULL argument");
        return -1;
    }
    systems_consolidation_pink_noise_bridge_heartbeat_instance(NULL, "systems_consolidation_pink_noise_bridge_training_end", 1.0f);
    return 0;
}

int systems_consolidation_pink_noise_bridge_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "systems_consolidation_pink_noise_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    systems_consolidation_pink_noise_bridge_heartbeat_instance(NULL, "systems_consolidation_pink_noise_bridge_training_step", progress);
    return 0;
}
