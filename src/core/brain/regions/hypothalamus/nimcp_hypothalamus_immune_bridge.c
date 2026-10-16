/**
 * @file nimcp_hypothalamus_immune_bridge.c
 * @brief Hypothalamus-Immune System Bidirectional Bridge Implementation
 *
 * WHAT: Bidirectional integration between hypothalamus drives and brain immune system
 * WHY:  Implements neuroimmune crosstalk for sickness behavior and HPA axis regulation
 * HOW:  Cytokines modulate drive setpoints; HPA axis modulates immune intensity
 *
 * @version Phase 14: Brain Immune Integration
 * @date 2026-01-04
 */

#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_immune_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_math_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(hypothalamus_immune_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define LOG_MODULE "HYPOTHALAMUS_IMMUNE_BRIDGE"


/*=============================================================================
 * ORCHESTRATOR INTEGRATION (OPTIONAL)
 *
 * Note: We cannot include the orchestrator header due to a naming conflict
 * with hypo_drive_state_t. The orchestrator connection functionality is
 * implemented via bio-async messaging instead of direct function calls.
 *===========================================================================*/

/*=============================================================================
 * INTERNAL STRUCTURES
 *===========================================================================*/

/**
 * @brief Internal bridge state
 */
struct hypo_immune_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    /* External references */
    hypo_drive_system_handle_t* drives;
    brain_immune_system_t* immune;
    hypo_orchestrator_t orchestrator;

    /* Configuration */
    hypo_immune_config_t config;

    /* State */
    hypo_cytokine_state_t cytokines;
    hypo_hpa_axis_t hpa;
    hypo_sickness_state_t sickness;
    hypo_circadian_immune_t circadian_phase;
    float circadian_modulation;

    /* Drive modulations (one per drive type) */
    hypo_immune_modulation_t modulations[HYPO_DRIVE_COUNT];

    /* Safety mode */
    bool in_safety_mode;
    float safety_mode_level;

    /* Acute phase response */
    bool acute_phase_active;
    uint64_t acute_phase_start_us;

    /* Sleep-immune interaction */
    hypo_sleep_immune_t sleep_immune_state;
    float sleep_quality;
    bool is_sleeping;
    float sickness_sleep_drive;

    /* Bio-async */
    bio_module_context_t bio_ctx;
    bool bio_registered;

    /* Orchestrator registration */
    uint32_t orch_bridge_id;
    bool orch_registered;

    /* Last update time for delta calculations */
    uint64_t last_update_us;

    /* Statistics */
    hypo_immune_bridge_stats_t stats;

};

/*=============================================================================
 * FORWARD DECLARATIONS
 *===========================================================================*/

static nimcp_error_t immune_handle_cytokine_update(
    const void* msg, size_t msg_size, nimcp_bio_promise_t promise, void* ctx);
static nimcp_error_t immune_handle_inflammation(
    const void* msg, size_t msg_size, nimcp_bio_promise_t promise, void* ctx);
static nimcp_error_t immune_handle_antigen_detected(
    const void* msg, size_t msg_size, nimcp_bio_promise_t promise, void* ctx);

static void update_sickness_from_cytokines(hypo_immune_bridge_t* bridge);

/* Unlocked helper functions (for use within already-locked code) */
static hypo_sickness_level_t hypo_immune_bridge_compute_sickness_level_unlocked(
    hypo_immune_bridge_t* bridge);
static int hypo_immune_bridge_apply_cortisol_effects_unlocked(hypo_immune_bridge_t* bridge);
static int hypo_immune_end_acute_phase_unlocked(hypo_immune_bridge_t* bridge);
static void compute_hpa_dynamics(hypo_immune_bridge_t* bridge, float dt);
static float compute_pro_inflammatory_total(const hypo_cytokine_state_t* cyt);

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

void hypo_immune_bridge_default_config(hypo_immune_config_t* config) {
    if (!config) return;

    memset(config, 0, sizeof(*config));

    /* Cytokine sensitivity */
    config->fever_sensitivity = 1.0f;
    config->anorexia_sensitivity = 1.0f;
    config->fatigue_sensitivity = 1.0f;

    /* HPA axis parameters */
    config->cortisol_baseline = 0.2f;
    config->stress_threshold = 0.3f;
    config->recovery_rate = 0.05f;

    /* Circadian modulation */
    config->circadian_enabled = true;
    config->circadian_amplitude = 0.3f;

    /* Sickness behavior thresholds */
    config->mild_threshold = 0.2f;
    config->moderate_threshold = 0.4f;
    config->severe_threshold = 0.7f;
    config->storm_threshold = 0.9f;

    /* Alignment integration */
    config->use_as_safety_mode = true;
    config->safety_trigger = 0.6f;

    /* Extended configuration (bidirectional coupling) */
    config->cortisol_immune_suppression = HYPO_IMMUNE_CORTISOL_SUPPRESSION;
    config->cytokine_stress_sensitivity = 0.5f;
    config->fever_threshold = 0.3f;
    config->sickness_behavior_threshold = HYPO_IMMUNE_SICKNESS_ONSET_THRESHOLD;
    config->enable_bidirectional = true;
    config->enable_bio_async = true;

    /* Sleep-immune parameters */
    config->sleep_immune_coupling = HYPO_IMMUNE_SLEEP_FACTOR;
    config->enable_sleep_modulation = true;
}

hypo_immune_bridge_t* hypo_immune_bridge_create(
    hypo_drive_system_handle_t* drives,
    brain_immune_system_t* immune,
    const hypo_immune_config_t* config)
{
    if (!drives) {
        nimcp_log(LOG_LEVEL_ERROR, "hypo_immune_bridge_create: drives is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "drives is NULL");

        return NULL;
    }

    hypo_immune_bridge_t* bridge = nimcp_calloc(1, sizeof(hypo_immune_bridge_t));
    if (!bridge) {
        nimcp_log(LOG_LEVEL_ERROR, "hypo_immune_bridge_create: allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;
    }

    bridge->drives = drives;
    bridge->immune = immune;

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        hypo_immune_bridge_default_config(&bridge->config);
    }

    /* Initialize HPA axis */
    bridge->hpa.state = HPA_BASELINE;
    bridge->hpa.cortisol_level = bridge->config.cortisol_baseline;
    bridge->hpa.crh_level = 0.1f;
    bridge->hpa.acth_level = 0.1f;

    /* Initialize sickness state */
    bridge->sickness.level = SICKNESS_NONE;

    /* Initialize drive modulations */
    for (int i = 0; i < HYPO_DRIVE_COUNT; i++) {
        bridge->modulations[i].drive = (hypo_drive_type_t)i;
        bridge->modulations[i].baseline_setpoint = 0.5f;
        bridge->modulations[i].modulated_setpoint = 0.5f;
        bridge->modulations[i].modulation_factor = 1.0f;
        bridge->modulations[i].is_modulated = false;
    }

    /* Initialize sleep-immune state */
    bridge->sleep_immune_state = SLEEP_IMMUNE_NORMAL;
    bridge->sleep_quality = 1.0f;
    bridge->is_sleeping = false;
    bridge->sickness_sleep_drive = 0.0f;

    /* Initialize circadian modulation */
    bridge->circadian_modulation = 1.0f;

    /* Initialize timing */
    bridge->last_update_us = nimcp_time_get_us();

    /* Initialize bridge base (creates mutex) */
    if (bridge_base_init(&bridge->base, 0, "hypothalamus_immune") != 0) {
        nimcp_log(LOG_LEVEL_ERROR, "hypo_immune_bridge_create: bridge_base_init failed");
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hypo_immune_bridge_create: validation failed");
        return NULL;
    }

    nimcp_log(LOG_LEVEL_INFO, "hypo_immune_bridge: created successfully");
    return bridge;
}

void hypo_immune_bridge_destroy(hypo_immune_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "hypothalamus_immune");

    /* Note: Orchestrator unregistration handled via bio-async cleanup */
    bridge->orch_registered = false;

    if (bridge->bio_registered) {
        hypo_immune_bridge_unregister_bio(bridge);
    }

    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
    nimcp_log(LOG_LEVEL_INFO, "hypo_immune_bridge: destroyed");
}

/*=============================================================================
 * CYTOKINE → HYPOTHALAMUS (Immune → Drive Modulation)
 *===========================================================================*/

int hypo_immune_bridge_update_cytokines(
    hypo_immune_bridge_t* bridge,
    const hypo_cytokine_state_t* cytokines)
{
    if (!bridge || !cytokines) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_immune_bridge_update_cytokines: required parameter is NULL (bridge, cytokines)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->cytokines = *cytokines;
    bridge->cytokines.last_update_us = nimcp_time_get_us();
    bridge->stats.cytokine_updates++;

    /* Update sickness state based on new cytokine levels */
    update_sickness_from_cytokines(bridge);

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int hypo_immune_bridge_apply_cytokine_effects(hypo_immune_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    nimcp_mutex_lock(bridge->base.mutex);

    const hypo_cytokine_state_t* cyt = &bridge->cytokines;

    /* 1. Fever: IL-1β + IL-6 → Temperature setpoint ↑ */
    float fever_signal = (cyt->il1_beta + cyt->il6) * 0.5f;
    fever_signal *= bridge->config.fever_sensitivity;
    bridge->sickness.fever_magnitude = fminf(fever_signal * HYPO_IMMUNE_FEVER_FACTOR, 1.0f);

    if (bridge->sickness.fever_magnitude > 0.1f) {
        /* Increase temperature drive setpoint */
        hypo_drive_set_nucleus_input(bridge->drives,
            HYPO_NUCLEUS_ANTERIOR, bridge->sickness.fever_magnitude);
        bridge->stats.fever_episodes++;
    }

    /* 2. Anorexia: TNF-α → Hunger setpoint ↓ */
    float anorexia_signal = cyt->tnf_alpha * bridge->config.anorexia_sensitivity;
    bridge->sickness.anorexia_magnitude = fminf(anorexia_signal * HYPO_IMMUNE_ANOREXIA_FACTOR, 1.0f);

    if (bridge->sickness.anorexia_magnitude > 0.1f) {
        /* Decrease hunger drive via lateral hypothalamus */
        hypo_drive_set_nucleus_input(bridge->drives,
            HYPO_NUCLEUS_LATERAL, -bridge->sickness.anorexia_magnitude);
        bridge->stats.anorexia_episodes++;
    }

    /* 3. Fatigue: Pro-inflammatory cytokines → Fatigue ↑ */
    float pro_inflammatory = compute_pro_inflammatory_total(cyt);
    float fatigue_signal = pro_inflammatory * bridge->config.fatigue_sensitivity;
    bridge->sickness.fatigue_magnitude = fminf(fatigue_signal * HYPO_IMMUNE_FATIGUE_FACTOR, 1.0f);

    if (bridge->sickness.fatigue_magnitude > 0.1f) {
        /* Increase fatigue via preoptic area */
        hypo_drive_set_nucleus_input(bridge->drives,
            HYPO_NUCLEUS_PREOPTIC, bridge->sickness.fatigue_magnitude);
    }

    /* 4. Social withdrawal: High inflammation → Social drive ↓ */
    if (pro_inflammatory > 0.5f) {
        bridge->sickness.social_withdrawal = (pro_inflammatory - 0.5f) * HYPO_IMMUNE_SOCIAL_WITHDRAW_FACTOR;
        /* Suppress social via PVN (reduce oxytocin drive) */
        hypo_drive_set_nucleus_input(bridge->drives,
            HYPO_NUCLEUS_PARAVENTRICULAR, -bridge->sickness.social_withdrawal * 0.5f);
    }

    /* 5. Curiosity reduction: Sickness → Exploration ↓ */
    if (bridge->sickness.level >= SICKNESS_MODERATE) {
        bridge->sickness.curiosity_reduction = 0.3f + 0.2f * (bridge->sickness.level - SICKNESS_MODERATE);
        /* This would be applied via drive system direct modulation */
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

float hypo_immune_bridge_get_fever(const hypo_immune_bridge_t* bridge) {
    if (!bridge) return 0.0f;
    return bridge->sickness.fever_magnitude;
}

int hypo_immune_bridge_get_sickness_state(
    const hypo_immune_bridge_t* bridge,
    hypo_sickness_state_t* state)
{
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_immune_bridge_get_sickness_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    nimcp_mutex_lock(((hypo_immune_bridge_t*)bridge)->base.mutex);
    *state = bridge->sickness;
    nimcp_mutex_unlock(((hypo_immune_bridge_t*)bridge)->base.mutex);
    return 0;
}

/*=============================================================================
 * HYPOTHALAMUS → IMMUNE (HPA Axis → Immune Suppression)
 *===========================================================================*/

int hypo_immune_bridge_update_hpa(hypo_immune_bridge_t* bridge, float stress_input) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    nimcp_mutex_lock(bridge->base.mutex);

    uint64_t now = nimcp_time_get_us();
    float dt = (bridge->hpa.last_update_us > 0) ?
        (now - bridge->hpa.last_update_us) / 1000000.0f : 0.0f;
    bridge->hpa.last_update_us = now;

    /* Update stress levels */
    bridge->hpa.acute_stress = stress_input;

    /* Accumulate chronic stress with decay */
    const float chronic_accumulation = 0.001f;
    const float chronic_decay = 0.0005f;
    if (stress_input > bridge->config.stress_threshold) {
        bridge->hpa.chronic_stress += chronic_accumulation * dt;
        bridge->hpa.chronic_stress = fminf(bridge->hpa.chronic_stress, 1.0f);
    } else {
        bridge->hpa.chronic_stress -= chronic_decay * dt;
        bridge->hpa.chronic_stress = fmaxf(bridge->hpa.chronic_stress, 0.0f);
    }

    /* State transitions based on stress */
    if (bridge->hpa.chronic_stress > 0.7f) {
        if (bridge->hpa.state != HPA_CHRONIC_STRESS) {
            bridge->hpa.state = HPA_CHRONIC_STRESS;
        }
    } else if (stress_input > bridge->config.stress_threshold) {
        if (bridge->hpa.state == HPA_BASELINE) {
            bridge->hpa.state = HPA_ACUTE_STRESS;
            bridge->hpa.stress_onset_us = now;
            bridge->stats.stress_activations++;
        } else if (bridge->hpa.state == HPA_ACUTE_STRESS) {
            /* Check for prolonged stress */
            if ((now - bridge->hpa.stress_onset_us) > 300000000ULL) { /* 5 minutes */
                bridge->hpa.state = HPA_PROLONGED_STRESS;
            }
        }
    } else if (bridge->hpa.state != HPA_BASELINE) {
        bridge->hpa.state = HPA_RECOVERY;
    }

    /* Compute HPA dynamics */
    compute_hpa_dynamics(bridge, dt);

    /* Recovery to baseline */
    if (bridge->hpa.state == HPA_RECOVERY) {
        if (bridge->hpa.cortisol_level <= bridge->config.cortisol_baseline * 1.1f) {
            bridge->hpa.state = HPA_BASELINE;
            bridge->stats.recovery_cycles++;
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

float hypo_immune_bridge_get_immune_suppression(const hypo_immune_bridge_t* bridge) {
    if (!bridge) return 0.0f;

    /* Cortisol above baseline causes immune suppression */
    float excess_cortisol = bridge->hpa.cortisol_level - bridge->config.cortisol_baseline;
    if (excess_cortisol <= 0.0f) return 0.0f;

    return fminf(excess_cortisol * HYPO_IMMUNE_CORTISOL_SUPPRESSION, 1.0f);
}

int hypo_immune_bridge_get_hpa_state(
    const hypo_immune_bridge_t* bridge,
    hypo_hpa_axis_t* hpa)
{
    if (!bridge || !hpa) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_immune_bridge_get_hpa_state: required parameter is NULL (bridge, hpa)");
        return -1;
    }

    nimcp_mutex_lock(((hypo_immune_bridge_t*)bridge)->base.mutex);
    *hpa = bridge->hpa;
    nimcp_mutex_unlock(((hypo_immune_bridge_t*)bridge)->base.mutex);
    return 0;
}

/**
 * @brief Internal helper to apply cortisol effects (must be called with lock held)
 */
static int hypo_immune_bridge_apply_cortisol_effects_unlocked(hypo_immune_bridge_t* bridge) {
    float suppression = hypo_immune_bridge_get_immune_suppression(bridge);

    /* Update peak cortisol tracking */
    if (bridge->hpa.cortisol_level > bridge->stats.peak_cortisol) {
        bridge->stats.peak_cortisol = bridge->hpa.cortisol_level;
    }

    /* Returns suppression for caller to decide about broadcast outside lock */
    (void)suppression;
    return 0;
}

int hypo_immune_bridge_apply_cortisol_effects(hypo_immune_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    int result = hypo_immune_bridge_apply_cortisol_effects_unlocked(bridge);
    float suppression = hypo_immune_bridge_get_immune_suppression(bridge);
    bool should_broadcast = (suppression > 0.1f && bridge->bio_registered);
    nimcp_mutex_unlock(bridge->base.mutex);

    /* Broadcast cortisol state outside the lock to avoid external call under mutex */
    if (should_broadcast) {
        hypo_immune_bridge_broadcast_cortisol(bridge);
    }

    return result;
}

/*=============================================================================
 * CIRCADIAN → IMMUNE MODULATION
 *===========================================================================*/

int hypo_immune_bridge_update_circadian(
    hypo_immune_bridge_t* bridge,
    float scn_phase)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->config.circadian_enabled) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    /* Normalize phase to [0, 2π] */
    while (scn_phase < 0.0f) scn_phase += NIMCP_TWO_PI_F;
    while (scn_phase >= NIMCP_TWO_PI_F) scn_phase -= NIMCP_TWO_PI_F;

    /* Determine circadian immune phase */
    float normalized = scn_phase / (NIMCP_TWO_PI_F);
    if (normalized < 0.25f) {
        bridge->circadian_phase = CIRCADIAN_IMMUNE_MORNING;
    } else if (normalized < 0.5f) {
        bridge->circadian_phase = CIRCADIAN_IMMUNE_DAY;
    } else if (normalized < 0.75f) {
        bridge->circadian_phase = CIRCADIAN_IMMUNE_EVENING;
    } else {
        bridge->circadian_phase = CIRCADIAN_IMMUNE_NIGHT;
    }

    /* Compute modulation factor */
    /* Peak inflammation potential in morning, lowest at night */
    bridge->circadian_modulation = 1.0f + bridge->config.circadian_amplitude *
        cosf(scn_phase - 0.5f * NIMCP_PI_F);

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

hypo_circadian_immune_t hypo_immune_bridge_get_circadian_phase(
    const hypo_immune_bridge_t* bridge)
{
    if (!bridge) return CIRCADIAN_IMMUNE_DAY;
    return bridge->circadian_phase;
}

float hypo_immune_bridge_get_circadian_modulation(const hypo_immune_bridge_t* bridge) {
    if (!bridge) return 1.0f;
    return bridge->circadian_modulation;
}

/*=============================================================================
 * SICKNESS BEHAVIOR
 *===========================================================================*/

/**
 * @brief Internal helper to compute sickness level (must be called with lock held)
 */
static hypo_sickness_level_t hypo_immune_bridge_compute_sickness_level_unlocked(
    hypo_immune_bridge_t* bridge)
{
    float inflammation = bridge->sickness.inflammation_level;
    hypo_sickness_level_t level;

    if (inflammation >= bridge->config.storm_threshold) {
        level = SICKNESS_CRITICAL;
        bridge->sickness.cytokine_storm = true;
    } else if (inflammation >= bridge->config.severe_threshold) {
        level = SICKNESS_SEVERE;
    } else if (inflammation >= bridge->config.moderate_threshold) {
        level = SICKNESS_MODERATE;
    } else if (inflammation >= bridge->config.mild_threshold) {
        level = SICKNESS_MILD;
    } else {
        level = SICKNESS_NONE;
    }

    if (level != bridge->sickness.level) {
        if (level > SICKNESS_NONE && bridge->sickness.level == SICKNESS_NONE) {
            bridge->sickness.onset_us = nimcp_time_get_us();
            bridge->stats.sickness_episodes++;
        }
        if (level == SICKNESS_CRITICAL) {
            bridge->stats.cytokine_storms++;
        }
        bridge->sickness.level = level;
    }

    return level;
}

hypo_sickness_level_t hypo_immune_bridge_compute_sickness_level(
    hypo_immune_bridge_t* bridge)
{
    if (!bridge) return SICKNESS_NONE;

    nimcp_mutex_lock(bridge->base.mutex);
    hypo_sickness_level_t level = hypo_immune_bridge_compute_sickness_level_unlocked(bridge);
    nimcp_mutex_unlock(bridge->base.mutex);
    return level;
}

int hypo_immune_bridge_apply_sickness_behavior(hypo_immune_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* First update sickness level */
    hypo_immune_bridge_compute_sickness_level(bridge);

    /* Then apply cytokine effects */
    return hypo_immune_bridge_apply_cytokine_effects(bridge);
}

bool hypo_immune_bridge_is_cytokine_storm(const hypo_immune_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->sickness.cytokine_storm;
}

int hypo_immune_bridge_enter_safety_mode(
    hypo_immune_bridge_t* bridge,
    float threat_level)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->config.use_as_safety_mode) return -1;

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->in_safety_mode = true;
    bridge->safety_mode_level = fminf(threat_level, 1.0f);
    bridge->stats.safety_mode_entries++;

    /* Simulate sickness behavior for safety mode */
    /* Reduce exploration (curiosity) */
    bridge->sickness.curiosity_reduction = 0.5f + 0.5f * threat_level;

    /* Increase caution (safety drive up) */
    hypo_drive_set_nucleus_input(bridge->drives,
        HYPO_NUCLEUS_VENTROMEDIAL, threat_level * 0.5f);

    /* Reduce social engagement in severe cases */
    if (threat_level > 0.7f) {
        bridge->sickness.social_withdrawal = (threat_level - 0.5f) * 0.5f;
    }

    nimcp_log(LOG_LEVEL_WARN, "hypo_immune_bridge: entered safety mode (threat=%.2f)", threat_level);

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int hypo_immune_bridge_exit_safety_mode(hypo_immune_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->in_safety_mode = false;
    bridge->safety_mode_level = 0.0f;

    /* Reset safety-induced modulations */
    bridge->sickness.curiosity_reduction = 0.0f;
    bridge->sickness.social_withdrawal = 0.0f;

    /* Reset nucleus inputs */
    hypo_drive_set_nucleus_input(bridge->drives, HYPO_NUCLEUS_VENTROMEDIAL, 0.0f);

    nimcp_log(LOG_LEVEL_INFO, "hypo_immune_bridge: exited safety mode");

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/*=============================================================================
 * BIO-ASYNC INTEGRATION
 *===========================================================================*/

bool hypo_immune_bridge_register_bio(
    hypo_immune_bridge_t* bridge,
    bool use_kg_wiring)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_immune_bridge_register_bio: bridge is NULL");
        return false;
    }
    if (bridge->bio_registered) return true;

    (void)use_kg_wiring; /* Future KG wiring integration */

    bio_module_info_t info = {
        .module_id = HYPO_IMMUNE_BRIDGE_MODULE_ID,
        .module_name = "hypothalamus_immune_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->bio_ctx = bio_router_register_module(&info);
    if (!bridge->bio_ctx) {
        nimcp_log(LOG_LEVEL_ERROR, "hypo_immune_bridge: failed to register with bio-router");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_immune_bridge_register_bio: bridge->bio_ctx is NULL");
        return false;
    }

    /* Register handlers for immune messages */
    bio_router_register_handler(bridge->bio_ctx,
        BIO_MSG_CYTOKINE_UPDATE, immune_handle_cytokine_update);
    bio_router_register_handler(bridge->bio_ctx,
        BIO_MSG_INFLAMMATION_CHANGE, immune_handle_inflammation);
    bio_router_register_handler(bridge->bio_ctx,
        BIO_MSG_ANTIGEN_DETECTED, immune_handle_antigen_detected);

    bridge->bio_registered = true;
    nimcp_log(LOG_LEVEL_INFO, "hypo_immune_bridge: registered with bio-router");
    return true;
}

void hypo_immune_bridge_unregister_bio(hypo_immune_bridge_t* bridge) {
    if (!bridge || !bridge->bio_registered) return;

    bio_router_unregister_module(bridge->bio_ctx);
    bridge->bio_ctx = NULL;
    bridge->bio_registered = false;
    nimcp_log(LOG_LEVEL_INFO, "hypo_immune_bridge: unregistered from bio-router");
}

nimcp_error_t hypo_immune_bridge_broadcast_fever(hypo_immune_bridge_t* bridge) {
    if (!bridge || !bridge->bio_registered) return NIMCP_ERROR_INVALID_PARAM;

    struct {
        bio_message_header_t header;
        float fever_magnitude;
        float temperature_setpoint_delta;
    } msg;

    msg.header.type = BIO_MSG_HYPO_IMMUNE_FEVER_STATE;
    msg.header.timestamp_us = nimcp_time_get_us();
    msg.header.source_module = HYPO_IMMUNE_BRIDGE_MODULE_ID;
    msg.header.target_module = 0;  /* Broadcast */
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.header.payload_size = sizeof(msg) - sizeof(bio_message_header_t);

    msg.fever_magnitude = bridge->sickness.fever_magnitude;
    msg.temperature_setpoint_delta = bridge->sickness.fever_magnitude * 2.0f; /* °C */

    bridge->stats.messages_sent++;
    return bio_router_broadcast(bridge->bio_ctx, &msg.header, sizeof(msg));
}

nimcp_error_t hypo_immune_bridge_broadcast_sickness(hypo_immune_bridge_t* bridge) {
    if (!bridge || !bridge->bio_registered) return NIMCP_ERROR_INVALID_PARAM;

    struct {
        bio_message_header_t header;
        uint8_t sickness_level;
        float inflammation;
        uint8_t cytokine_storm;
    } msg;

    msg.header.type = BIO_MSG_HYPO_IMMUNE_SICKNESS_STATE;
    msg.header.timestamp_us = nimcp_time_get_us();
    msg.header.source_module = HYPO_IMMUNE_BRIDGE_MODULE_ID;
    msg.header.target_module = 0;
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.header.payload_size = sizeof(msg) - sizeof(bio_message_header_t);

    msg.sickness_level = (uint8_t)bridge->sickness.level;
    msg.inflammation = bridge->sickness.inflammation_level;
    msg.cytokine_storm = bridge->sickness.cytokine_storm ? 1 : 0;

    bridge->stats.messages_sent++;
    return bio_router_broadcast(bridge->bio_ctx, &msg.header, sizeof(msg));
}

nimcp_error_t hypo_immune_bridge_broadcast_cortisol(hypo_immune_bridge_t* bridge) {
    if (!bridge || !bridge->bio_registered) return NIMCP_ERROR_INVALID_PARAM;

    struct {
        bio_message_header_t header;
        float cortisol_level;
        float immune_suppression;
        uint8_t hpa_state;
    } msg;

    msg.header.type = BIO_MSG_HYPO_IMMUNE_CORTISOL_STATE;
    msg.header.timestamp_us = nimcp_time_get_us();
    msg.header.source_module = HYPO_IMMUNE_BRIDGE_MODULE_ID;
    msg.header.target_module = 0;
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.header.payload_size = sizeof(msg) - sizeof(bio_message_header_t);

    msg.cortisol_level = bridge->hpa.cortisol_level;
    msg.immune_suppression = hypo_immune_bridge_get_immune_suppression(bridge);
    msg.hpa_state = (uint8_t)bridge->hpa.state;

    bridge->stats.messages_sent++;
    return bio_router_broadcast(bridge->bio_ctx, &msg.header, sizeof(msg));
}

/*=============================================================================
 * STATISTICS
 *===========================================================================*/

int hypo_immune_bridge_get_stats(
    const hypo_immune_bridge_t* bridge,
    hypo_immune_bridge_stats_t* stats)
{
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_immune_bridge_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    nimcp_mutex_lock(((hypo_immune_bridge_t*)bridge)->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(((hypo_immune_bridge_t*)bridge)->base.mutex);
    return 0;
}

void hypo_immune_bridge_reset_stats(hypo_immune_bridge_t* bridge) {
    if (!bridge) return;

    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    nimcp_mutex_unlock(bridge->base.mutex);
}

/*=============================================================================
 * INTERNAL HELPERS
 *===========================================================================*/

static float compute_pro_inflammatory_total(const hypo_cytokine_state_t* cyt) {
    if (!cyt) return 0.0f;

    /* Weight pro-inflammatory cytokines */
    return 0.3f * cyt->il1_beta +
           0.3f * cyt->tnf_alpha +
           0.2f * cyt->il6 +
           0.2f * cyt->ifn_gamma;
}

static void update_sickness_from_cytokines(hypo_immune_bridge_t* bridge) {
    float pro_inflam = compute_pro_inflammatory_total(&bridge->cytokines);
    float anti_inflam = bridge->cytokines.il10 + bridge->cytokines.tgf_beta;

    /* Net inflammation */
    bridge->sickness.inflammation_level = fmaxf(0.0f,
        pro_inflam - 0.5f * anti_inflam);
    bridge->sickness.inflammation_level = fminf(bridge->sickness.inflammation_level, 1.0f);
}

static void compute_hpa_dynamics(hypo_immune_bridge_t* bridge, float dt) {
    if (dt <= 0.0f) return;

    const float crh_tau = 5.0f;  /* CRH time constant (seconds) */
    const float acth_tau = 10.0f;
    const float cortisol_tau = 30.0f;

    float stress = bridge->hpa.acute_stress;

    /* CRH dynamics: driven by stress */
    float crh_target = stress;
    bridge->hpa.crh_level += (crh_target - bridge->hpa.crh_level) * (dt / crh_tau);

    /* ACTH dynamics: driven by CRH */
    float acth_target = bridge->hpa.crh_level * 0.8f;
    bridge->hpa.acth_level += (acth_target - bridge->hpa.acth_level) * (dt / acth_tau);

    /* Cortisol dynamics: driven by ACTH, with recovery */
    float cortisol_target = bridge->config.cortisol_baseline + bridge->hpa.acth_level * 0.6f;
    if (bridge->hpa.state == HPA_RECOVERY) {
        cortisol_target = bridge->config.cortisol_baseline;
    }
    bridge->hpa.cortisol_level += (cortisol_target - bridge->hpa.cortisol_level) *
        (dt / cortisol_tau * (bridge->hpa.state == HPA_RECOVERY ? 2.0f : 1.0f));

    /* Clamp values */
    bridge->hpa.crh_level = fmaxf(0.0f, fminf(bridge->hpa.crh_level, 1.0f));
    bridge->hpa.acth_level = fmaxf(0.0f, fminf(bridge->hpa.acth_level, 1.0f));
    bridge->hpa.cortisol_level = fmaxf(0.0f, fminf(bridge->hpa.cortisol_level, 1.0f));
}

/*=============================================================================
 * BIO-ASYNC HANDLERS
 *===========================================================================*/

static nimcp_error_t immune_handle_cytokine_update(
    const void* msg, size_t msg_size, nimcp_bio_promise_t promise, void* ctx)
{
    hypo_immune_bridge_t* bridge = (hypo_immune_bridge_t*)ctx;
    (void)msg_size;
    (void)promise;

    if (!bridge || !msg) return NIMCP_ERROR_INVALID_PARAM;

    const struct {
        bio_message_header_t header;
        uint8_t cytokine_type;
        float concentration;
    }* cytokine_msg = msg;

    nimcp_mutex_lock(bridge->base.mutex);

    /* Update appropriate cytokine level */
    switch (cytokine_msg->cytokine_type) {
        case CYTOKINE_IL1B:
            bridge->cytokines.il1_beta = cytokine_msg->concentration;
            break;
        case CYTOKINE_IL6:
            bridge->cytokines.il6 = cytokine_msg->concentration;
            break;
        case CYTOKINE_TNFA:
            bridge->cytokines.tnf_alpha = cytokine_msg->concentration;
            break;
        case CYTOKINE_IL10:
            bridge->cytokines.il10 = cytokine_msg->concentration;
            break;
        case CYTOKINE_TGFB:
            bridge->cytokines.tgf_beta = cytokine_msg->concentration;
            break;
        default:
            break;
    }

    bridge->cytokines.last_update_us = nimcp_time_get_us();
    bridge->stats.messages_received++;

    update_sickness_from_cytokines(bridge);

    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

static nimcp_error_t immune_handle_inflammation(
    const void* msg, size_t msg_size, nimcp_bio_promise_t promise, void* ctx)
{
    hypo_immune_bridge_t* bridge = (hypo_immune_bridge_t*)ctx;
    (void)msg_size;
    (void)promise;

    if (!bridge || !msg) return NIMCP_ERROR_INVALID_PARAM;

    const struct {
        bio_message_header_t header;
        float inflammation_level;
        uint32_t region_id;
    }* inflam_msg = msg;

    nimcp_mutex_lock(bridge->base.mutex);

    /* Update inflammation level directly */
    bridge->sickness.inflammation_level = fmaxf(bridge->sickness.inflammation_level,
        inflam_msg->inflammation_level);
    bridge->stats.messages_received++;

    /* Recompute sickness level */
    hypo_immune_bridge_compute_sickness_level_unlocked(bridge);

    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

static nimcp_error_t immune_handle_antigen_detected(
    const void* msg, size_t msg_size, nimcp_bio_promise_t promise, void* ctx)
{
    hypo_immune_bridge_t* bridge = (hypo_immune_bridge_t*)ctx;
    (void)msg_size;
    (void)promise;

    if (!bridge || !msg) return NIMCP_ERROR_INVALID_PARAM;

    const struct {
        bio_message_header_t header;
        uint32_t antigen_id;
        float severity;
    }* antigen_msg = msg;

    bridge->stats.messages_received++;

    /* High-severity antigens trigger acute stress response */
    if (antigen_msg->severity > 0.7f) {
        hypo_immune_bridge_update_hpa(bridge, antigen_msg->severity);
    }

    return NIMCP_SUCCESS;
}

/*=============================================================================
 * CONNECTION & UPDATE FUNCTIONS
 *===========================================================================*/

int hypo_immune_connect(
    hypo_immune_bridge_t* bridge,
    hypo_orchestrator_t orch,
    brain_immune_system_t* immune)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->orchestrator = orch;
    bridge->immune = immune;

    /* Note: Direct orchestrator registration is not available due to header
     * conflicts. The bridge operates via bio-async messaging for orchestrator
     * communication. Set the registered flag based on orchestrator availability. */
    if (orch) {
        bridge->orch_registered = true;
        nimcp_log(LOG_LEVEL_INFO,
            "hypo_immune_bridge: connected to orchestrator (via bio-async)");
    }

    /* Register bio-async if enabled */
    if (bridge->config.enable_bio_async && !bridge->bio_registered) {
        hypo_immune_bridge_register_bio(bridge, false);
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int hypo_immune_update(hypo_immune_bridge_t* bridge, uint64_t delta_ms)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    nimcp_mutex_lock(bridge->base.mutex);

    float dt = (float)delta_ms / 1000.0f;
    uint64_t now = nimcp_time_get_us();

    /* Update HPA dynamics based on current stress and cytokine levels */
    float cytokine_stress = 0.0f;
    if (bridge->config.enable_bidirectional) {
        /* Cytokines induce HPA axis activation */
        float pro_inflam = compute_pro_inflammatory_total(&bridge->cytokines);
        cytokine_stress = pro_inflam * bridge->config.cytokine_stress_sensitivity;
    }
    compute_hpa_dynamics(bridge, dt);

    /* If cytokines are high, they contribute to stress response */
    if (cytokine_stress > 0.1f) {
        bridge->hpa.acute_stress = fmaxf(bridge->hpa.acute_stress, cytokine_stress);
    }

    /* Update sickness behavior */
    update_sickness_from_cytokines(bridge);
    hypo_immune_bridge_compute_sickness_level_unlocked(bridge);

    /* Update sleep-immune interaction */
    if (bridge->config.enable_sleep_modulation) {
        /* Compute sickness-induced sleep drive */
        float pro_inflam = compute_pro_inflammatory_total(&bridge->cytokines);
        bridge->sickness_sleep_drive = pro_inflam * HYPO_IMMUNE_SLEEP_FACTOR;

        /* Update sleep-immune state based on current conditions */
        if (bridge->sickness.level >= SICKNESS_MODERATE) {
            bridge->sleep_immune_state = SLEEP_IMMUNE_SICKNESS;
        } else if (bridge->sleep_quality < 0.3f && !bridge->is_sleeping) {
            bridge->sleep_immune_state = SLEEP_IMMUNE_DEPRIVED;
        } else if (bridge->is_sleeping && bridge->sleep_quality > 0.7f) {
            bridge->sleep_immune_state = SLEEP_IMMUNE_RESTORATIVE;
        } else {
            bridge->sleep_immune_state = SLEEP_IMMUNE_NORMAL;
        }
    }

    /* Check acute phase duration */
    if (bridge->acute_phase_active) {
        uint64_t duration = now - bridge->acute_phase_start_us;
        if (duration > HYPO_IMMUNE_ACUTE_PHASE_DURATION_US) {
            /* Auto-end acute phase after duration */
            hypo_immune_end_acute_phase_unlocked(bridge);
        }
    }

    /* Apply effects if bidirectional coupling enabled */
    if (bridge->config.enable_bidirectional && bridge->immune) {
        hypo_immune_bridge_apply_cortisol_effects_unlocked(bridge);
    }

    bridge->last_update_us = now;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int hypo_immune_receive_cytokines(
    hypo_immune_bridge_t* bridge,
    const hypo_immune_cytokines_t* cytokines)
{
    if (!bridge || !cytokines) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_immune_receive_cytokines: required parameter is NULL (bridge, cytokines)");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Convert simplified cytokines to internal format */
    bridge->cytokines.il1_beta = cytokines->il1_beta;
    bridge->cytokines.il6 = cytokines->il6;
    bridge->cytokines.tnf_alpha = cytokines->tnf_alpha;
    bridge->cytokines.il10 = cytokines->il10;
    bridge->cytokines.ifn_gamma = cytokines->ifn_gamma;
    /* Note: cortisol comes from HPA, not external */
    bridge->cytokines.last_update_us = nimcp_time_get_us();

    bridge->stats.cytokine_updates++;

    /* Update derived state */
    update_sickness_from_cytokines(bridge);

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int hypo_immune_send_cortisol(hypo_immune_bridge_t* bridge, float cortisol)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }
    if (cortisol < 0.0f || cortisol > 1.0f) return -1;

    nimcp_mutex_lock(bridge->base.mutex);

    /* Update internal cortisol level */
    bridge->hpa.cortisol_level = cortisol;

    /* Check if we should broadcast (decision made under lock, action outside) */
    bool should_broadcast = (bridge->immune && bridge->config.enable_bidirectional &&
                             bridge->bio_registered);

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Broadcast via bio-async outside the lock */
    if (should_broadcast) {
        hypo_immune_bridge_broadcast_cortisol(bridge);
    }

    return 0;
}

/*=============================================================================
 * UNIFIED STATE QUERIES
 *===========================================================================*/

int hypo_immune_get_state(
    const hypo_immune_bridge_t* bridge,
    hypo_immune_state_t* state)
{
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_immune_get_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    nimcp_mutex_lock(((hypo_immune_bridge_t*)bridge)->base.mutex);

    memset(state, 0, sizeof(*state));

    /* Copy cytokine levels */
    state->cytokines.il1_beta = bridge->cytokines.il1_beta;
    state->cytokines.il6 = bridge->cytokines.il6;
    state->cytokines.tnf_alpha = bridge->cytokines.tnf_alpha;
    state->cytokines.il10 = bridge->cytokines.il10;
    state->cytokines.ifn_gamma = bridge->cytokines.ifn_gamma;
    state->cytokines.cortisol = bridge->hpa.cortisol_level;

    /* Copy inflammation and sickness */
    state->inflammation_level = bridge->sickness.inflammation_level;
    state->fever_signal = bridge->sickness.fever_magnitude;
    state->sickness_level = bridge->sickness.level;

    /* Compute overall sickness behavior intensity */
    state->sickness_behavior = (bridge->sickness.fever_magnitude +
                                bridge->sickness.anorexia_magnitude +
                                bridge->sickness.fatigue_magnitude +
                                bridge->sickness.social_withdrawal) / 4.0f;

    /* Compute immune activation from pro-inflammatory cytokines */
    state->immune_activation = compute_pro_inflammatory_total(&bridge->cytokines);

    /* Acute phase state */
    state->acute_phase_response = bridge->acute_phase_active;

    /* HPA and circadian state */
    state->hpa_state = bridge->hpa.state;
    state->circadian = bridge->circadian_phase;
    state->sleep_immune = bridge->sleep_immune_state;

    /* Timing */
    state->last_update_us = bridge->last_update_us;

    nimcp_mutex_unlock(((hypo_immune_bridge_t*)bridge)->base.mutex);
    return 0;
}

float hypo_immune_get_inflammation(const hypo_immune_bridge_t* bridge)
{
    if (!bridge) return 0.0f;
    return bridge->sickness.inflammation_level;
}

float hypo_immune_get_fever_signal(const hypo_immune_bridge_t* bridge)
{
    if (!bridge) return 0.0f;
    return bridge->sickness.fever_magnitude;
}

bool hypo_immune_is_sickness_behavior(const hypo_immune_bridge_t* bridge)
{
    if (!bridge) {
        return false;
    }
    return bridge->sickness.level >= SICKNESS_MILD;
}

/*=============================================================================
 * MODULATION & CONTROL
 *===========================================================================*/

int hypo_immune_modulate_immune_response(
    hypo_immune_bridge_t* bridge,
    float suppression)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }
    if (suppression < 0.0f || suppression > 1.0f) return -1;

    /* Prepare broadcast message under lock, send outside */
    struct {
        bio_message_header_t header;
        float cortisol_level;
        float immune_suppression;
        uint8_t hpa_state;
    } msg;
    bool should_broadcast = false;

    nimcp_mutex_lock(bridge->base.mutex);

    if (bridge->immune && bridge->bio_registered) {
        msg.header.type = BIO_MSG_HYPO_IMMUNE_CORTISOL_STATE;
        msg.header.timestamp_us = nimcp_time_get_us();
        msg.header.source_module = HYPO_IMMUNE_BRIDGE_MODULE_ID;
        msg.header.target_module = 0;
        msg.header.flags = BIO_MSG_FLAG_BROADCAST;
        msg.header.payload_size = sizeof(msg) - sizeof(bio_message_header_t);

        msg.cortisol_level = bridge->hpa.cortisol_level;
        msg.immune_suppression = suppression;
        msg.hpa_state = (uint8_t)bridge->hpa.state;

        should_broadcast = true;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Broadcast outside the lock to avoid external call under mutex */
    if (should_broadcast) {
        bio_router_broadcast(bridge->bio_ctx, &msg.header, sizeof(msg));
        /* Stats update is benign race but acceptable */
        bridge->stats.messages_sent++;
    }

    return 0;
}

int hypo_immune_trigger_acute_phase(hypo_immune_bridge_t* bridge)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    nimcp_mutex_lock(bridge->base.mutex);

    if (bridge->acute_phase_active) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;  /* Already active */
    }

    bridge->acute_phase_active = true;
    bridge->acute_phase_start_us = nimcp_time_get_us();

    /* Elevate pro-inflammatory cytokines */
    bridge->cytokines.il1_beta = fminf(bridge->cytokines.il1_beta + 0.3f, 1.0f);
    bridge->cytokines.il6 = fminf(bridge->cytokines.il6 + 0.4f, 1.0f);
    bridge->cytokines.tnf_alpha = fminf(bridge->cytokines.tnf_alpha + 0.2f, 1.0f);

    /* Trigger sickness behavior */
    update_sickness_from_cytokines(bridge);
    hypo_immune_bridge_compute_sickness_level_unlocked(bridge);
    /* Note: apply_sickness_behavior calls compute_sickness_level and apply_cytokine_effects
     * which take their own locks, so we need to unlock first, apply, then relock */
    bool should_broadcast = bridge->bio_registered;
    nimcp_mutex_unlock(bridge->base.mutex);

    hypo_immune_bridge_apply_sickness_behavior(bridge);

    /* Broadcast sickness state outside the lock to avoid external call under mutex */
    if (should_broadcast) {
        hypo_immune_bridge_broadcast_sickness(bridge);
        hypo_immune_bridge_broadcast_fever(bridge);
    }

    nimcp_log(LOG_LEVEL_INFO, "hypo_immune_bridge: acute phase response triggered");

    return 0;
}

/**
 * @brief Internal helper to end acute phase (must be called with lock held)
 */
static int hypo_immune_end_acute_phase_unlocked(hypo_immune_bridge_t* bridge)
{
    if (!bridge->acute_phase_active) {
        return 0;  /* Not active */
    }

    bridge->acute_phase_active = false;

    /* Elevate anti-inflammatory cytokines for resolution */
    bridge->cytokines.il10 = fminf(bridge->cytokines.il10 + 0.3f, 1.0f);
    bridge->cytokines.tgf_beta = fminf(bridge->cytokines.tgf_beta + 0.2f, 1.0f);

    /* Gradually reduce pro-inflammatory (will decay naturally) */
    bridge->cytokines.il1_beta *= 0.7f;
    bridge->cytokines.il6 *= 0.7f;
    bridge->cytokines.tnf_alpha *= 0.7f;

    /* Update state */
    update_sickness_from_cytokines(bridge);
    hypo_immune_bridge_compute_sickness_level_unlocked(bridge);

    /* Note: broadcast moved to public wrapper to avoid external call under lock */

    nimcp_log(LOG_LEVEL_INFO, "hypo_immune_bridge: acute phase response ended");
    return 0;
}

int hypo_immune_end_acute_phase(hypo_immune_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bool was_active = bridge->acute_phase_active;
    int result = hypo_immune_end_acute_phase_unlocked(bridge);
    bool should_broadcast = (was_active && bridge->bio_registered);
    nimcp_mutex_unlock(bridge->base.mutex);

    /* Broadcast resolution outside the lock */
    if (should_broadcast) {
        hypo_immune_bridge_broadcast_sickness(bridge);
    }

    return result;
}

/*=============================================================================
 * SLEEP-IMMUNE INTERACTION
 *===========================================================================*/

int hypo_immune_update_sleep(
    hypo_immune_bridge_t* bridge,
    float sleep_quality,
    bool is_sleeping)
{
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (sleep_quality < 0.0f || sleep_quality > 1.0f) return -1;

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->sleep_quality = sleep_quality;
    bridge->is_sleeping = is_sleeping;

    /* Sleep deprivation increases pro-inflammatory cytokines */
    if (!is_sleeping && sleep_quality < 0.3f) {
        float deprivation_factor = (0.3f - sleep_quality) * bridge->config.sleep_immune_coupling;
        bridge->cytokines.il1_beta = fminf(bridge->cytokines.il1_beta + deprivation_factor * 0.1f, 1.0f);
        bridge->cytokines.il6 = fminf(bridge->cytokines.il6 + deprivation_factor * 0.1f, 1.0f);
        bridge->sleep_immune_state = SLEEP_IMMUNE_DEPRIVED;
    }
    /* Restorative sleep enhances immune function (reduces inflammation) */
    else if (is_sleeping && sleep_quality > 0.7f) {
        float restoration_factor = (sleep_quality - 0.7f) * bridge->config.sleep_immune_coupling;
        bridge->cytokines.il10 = fminf(bridge->cytokines.il10 + restoration_factor * 0.05f, 1.0f);
        bridge->sleep_immune_state = SLEEP_IMMUNE_RESTORATIVE;
    }
    /* Sickness induces hypersomnia */
    else if (bridge->sickness.level >= SICKNESS_MODERATE) {
        bridge->sleep_immune_state = SLEEP_IMMUNE_SICKNESS;
    }
    else {
        bridge->sleep_immune_state = SLEEP_IMMUNE_NORMAL;
    }

    /* Update derived state */
    update_sickness_from_cytokines(bridge);

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

hypo_sleep_immune_t hypo_immune_get_sleep_state(const hypo_immune_bridge_t* bridge)
{
    if (!bridge) return SLEEP_IMMUNE_NORMAL;
    return bridge->sleep_immune_state;
}

float hypo_immune_get_sickness_sleep_drive(const hypo_immune_bridge_t* bridge)
{
    if (!bridge) return 0.0f;
    return bridge->sickness_sleep_drive;
}
