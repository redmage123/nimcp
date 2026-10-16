//=============================================================================
// nimcp_pr_predictive_bridge.c - FEP/Predictive Processing Bridge Implementation
//=============================================================================
/**
 * @file nimcp_pr_predictive_bridge.c
 * @brief Implementation of FEP-PR Memory integration bridge
 *
 * WHAT: Implements bidirectional FEP ↔ PR Memory integration
 * WHY:  Memory shapes predictions; prediction errors modulate memory
 * HOW:  Precision-weighted PE collection, memory-based predictions,
 *       PE-driven memory consolidation/reconsolidation/encoding
 *
 * @author NIMCP Development Team
 * @date 2026-01-09
 * @version 1.0.0
 */

#include "cognitive/memory/core/nimcp_pr_predictive_bridge.h"
#include "constants/nimcp_buffer_constants.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "security/nimcp_bbb_helpers.h"

#include <string.h>
#include <math.h>
#include <float.h>
#include <stdio.h>

#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(pr_predictive_bridge, MESH_ADAPTER_CATEGORY_MEMORY)


/** @brief Send heartbeat from pr_predictive_bridge module (instance-level) */
static inline void pr_predictive_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_pr_predictive_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_pr_predictive_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_pr_predictive_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

/* Security subsystem setters (Phase 1: Audit Gap Remediation) */
BRIDGE_DEFINE_SECURITY_SETTERS(pr_predictive_bridge)

//=============================================================================
// Internal Constants
//=============================================================================

/** Module ID for bio-async registration */
#define PR_PRED_MODULE_ID           0x5052  /* "PR" in hex */

/** Module name for logging */
#define PR_PRED_MODULE_NAME         "pr_predictive_bridge"

/** Default max reconsolidation windows */
#define PR_PRED_DEFAULT_MAX_RECON_WINDOWS   32

/** PE history size for variance calculation */
#define PR_PRED_PE_HISTORY_SIZE     16

/** Complexity term weight for free energy */
#define PR_PRED_COMPLEXITY_WEIGHT   0.1f

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Internal PE history for variance calculation
 */
typedef struct {
    float values[PR_PRED_PE_HISTORY_SIZE];
    size_t idx;
    size_t count;
} pe_history_t;

//=============================================================================
// Static Variables
//=============================================================================

/** Last error message (thread-local for thread safety) */
static _Thread_local char s_last_error[NIMCP_ERROR_BUFFER_SIZE] = {0};

/** PE histories for variance calculation (thread-local for thread safety) */
static _Thread_local pe_history_t s_visual_pe_history = {0};
static _Thread_local pe_history_t s_audio_pe_history = {0};
static _Thread_local pe_history_t s_speech_pe_history = {0};

//=============================================================================
// Forward Declarations - Internal Functions
//=============================================================================

static void set_error(const char* msg);
static float compute_variance(const pe_history_t* history);
static void add_to_pe_history(pe_history_t* history, float value);
static float clamp_precision(float precision);
static float softmax_select(const float* values, size_t count, float temperature);
static pr_mem_update_mode_t determine_update_mode(
    const pr_predictive_bridge_t* bridge,
    float pe,
    float max_resonance
);
static pr_pred_error_t process_reconsolidation_windows(
    pr_predictive_bridge_t* bridge,
    uint64_t current_time_ms
);

//=============================================================================
// Configuration Functions
//=============================================================================

pr_predictive_bridge_config_t pr_predictive_bridge_config_default(void) {
    /* Phase 8: Heartbeat at operation start */
    pr_predictive_bridge_heartbeat("pr_predictiv_config_default", 0.0f);


    pr_predictive_bridge_config_t config = {
        /* PE thresholds */
        .pe_threshold_update = PR_PRED_DEFAULT_PE_THRESHOLD_UPDATE,
        .pe_threshold_new = PR_PRED_DEFAULT_PE_THRESHOLD_NEW,

        /* Initial precision values */
        .initial_visual_precision = PR_PRED_DEFAULT_VISUAL_PRECISION,
        .initial_audio_precision = PR_PRED_DEFAULT_AUDIO_PRECISION,
        .initial_speech_precision = PR_PRED_DEFAULT_SPEECH_PRECISION,

        /* Coupling parameters */
        .resonance_prediction_weight = PR_PRED_DEFAULT_RESONANCE_WEIGHT,
        .consolidation_boost = PR_PRED_CONSOLIDATION_BOOST,
        .reconsolidation_duration_ms = PR_PRED_RECONSOLIDATION_WINDOW_MS,

        /* Precision adaptation */
        .precision_adaptation_rate = PR_PRED_PRECISION_ADAPTATION_RATE,
        .enable_precision_adaptation = true,

        /* Feature enables */
        .enable_visual = true,
        .enable_audio = true,
        .enable_speech = true,
        .enable_active_inference = true,
        .enable_reconsolidation = true,
        .track_free_energy = true,

        /* Action selection */
        .action_temperature = PR_PRED_ACTION_TEMPERATURE,
        .max_pending_actions = PR_PRED_MAX_PENDING_ACTIONS
    };
    return config;
}

bool pr_predictive_bridge_config_validate(const pr_predictive_bridge_config_t* config) {
    if (!config) {
        set_error("NULL config pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_predictive_bridge_config_validate: config is NULL");
        return false;
    }

    /* Check thresholds */
    /* Phase 8: Heartbeat at operation start */
    pr_predictive_bridge_heartbeat("pr_predictiv_config_validate", 0.0f);


    if (config->pe_threshold_update <= 0.0f) {
        set_error("pe_threshold_update must be > 0");
        return false;
    }
    if (config->pe_threshold_new <= 0.0f) {
        set_error("pe_threshold_new must be > 0");
        return false;
    }
    if (config->pe_threshold_update >= config->pe_threshold_new) {
        set_error("pe_threshold_update must be < pe_threshold_new");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_predictive_bridge_config_validate: capacity exceeded");
        return false;
    }

    /* Check precisions */
    if (config->initial_visual_precision < PR_PRED_MIN_PRECISION ||
        config->initial_visual_precision > PR_PRED_MAX_PRECISION) {
        set_error("initial_visual_precision out of range");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_predictive_bridge_config_validate: capacity exceeded");
        return false;
    }
    if (config->initial_audio_precision < PR_PRED_MIN_PRECISION ||
        config->initial_audio_precision > PR_PRED_MAX_PRECISION) {
        set_error("initial_audio_precision out of range");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_predictive_bridge_config_validate: operation failed");
        return false;
    }
    if (config->initial_speech_precision < PR_PRED_MIN_PRECISION ||
        config->initial_speech_precision > PR_PRED_MAX_PRECISION) {
        set_error("initial_speech_precision out of range");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_predictive_bridge_config_validate: operation failed");
        return false;
    }

    /* Check weights */
    if (config->resonance_prediction_weight < 0.0f ||
        config->resonance_prediction_weight > 1.0f) {
        set_error("resonance_prediction_weight must be in [0, 1]");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_predictive_bridge_config_validate: operation failed");
        return false;
    }

    return true;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

pr_predictive_bridge_t* pr_predictive_bridge_create(
    const pr_predictive_bridge_config_t* config
) {
    /* Use default config if none provided */
    /* Phase 8: Heartbeat at operation start */
    pr_predictive_bridge_heartbeat("pr_predictiv_create", 0.0f);


    pr_predictive_bridge_config_t cfg;
    if (config) {
        cfg = *config;
    } else {
        cfg = pr_predictive_bridge_config_default();
    }

    /* Validate configuration */
    if (!pr_predictive_bridge_config_validate(&cfg)) {
        NIMCP_LOGGING_ERROR("Invalid predictive bridge config: %s", s_last_error);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_predictive_bridge_create: pr_predictive_bridge_config_validate is NULL");
        return NULL;
    }

    /* Allocate bridge */
    pr_predictive_bridge_t* bridge = nimcp_malloc(sizeof(pr_predictive_bridge_t));
    if (!bridge) {
        set_error("Failed to allocate bridge");
        NIMCP_LOGGING_ERROR("Failed to allocate predictive bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pr_predictive_bridge_create: bridge is NULL");
        return NULL;
    }
    memset(bridge, 0, sizeof(pr_predictive_bridge_t));

    /* Initialize base bridge */
    if (bridge_base_init(&bridge->base, PR_PRED_MODULE_ID, PR_PRED_MODULE_NAME) != 0) {
        nimcp_free(bridge);
        bridge = NULL;
        set_error("Failed to initialize base bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "pr_predictive_bridge_create: validation failed");
        return NULL;
    }

    /* Store configuration */
    bridge->config = cfg;

    /* Initialize PE state */
    bridge->pe_state.visual_precision = cfg.initial_visual_precision;
    bridge->pe_state.audio_precision = cfg.initial_audio_precision;
    bridge->pe_state.speech_precision = cfg.initial_speech_precision;

    /* Allocate reconsolidation windows */
    bridge->max_reconsolidation_windows = PR_PRED_DEFAULT_MAX_RECON_WINDOWS;
    bridge->reconsolidation_windows = nimcp_malloc(
        bridge->max_reconsolidation_windows * sizeof(pr_reconsolidation_window_t)
    );
    if (!bridge->reconsolidation_windows) {
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        bridge = NULL;
        set_error("Failed to allocate reconsolidation windows");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pr_predictive_bridge_create: bridge->reconsolidation_windows is NULL");
        return NULL;
    }
    memset(bridge->reconsolidation_windows, 0,
           bridge->max_reconsolidation_windows * sizeof(pr_reconsolidation_window_t));

    /* Initialize statistics */
    bridge->stats.last_reset_time_ms = pr_pred_current_time_ms();
    bridge->stats.min_combined_pe = FLT_MAX;

    NIMCP_LOGGING_INFO("Created predictive bridge");
    return bridge;
}

void pr_predictive_bridge_destroy(pr_predictive_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    /* Free reconsolidation windows */
    /* Phase 8: Heartbeat at operation start */
    pr_predictive_bridge_heartbeat("pr_predictiv_destroy", 0.0f);


    if (bridge->reconsolidation_windows) {
        nimcp_free(bridge->reconsolidation_windows);
    }

    /* Cleanup base bridge */
    bridge_base_cleanup(&bridge->base);

    /* Free bridge */
    nimcp_free(bridge);
    bridge = NULL;

    NIMCP_LOGGING_INFO("Destroyed predictive bridge");
}

pr_pred_error_t pr_predictive_bridge_reset(pr_predictive_bridge_t* bridge) {
    if (!bridge) {
        return PR_PRED_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_predictive_bridge_heartbeat("pr_predictiv_reset", 0.0f);


    BRIDGE_LOCK(bridge);

    /* Reset PE state */
    bridge->pe_state.visual_pe = 0.0f;
    bridge->pe_state.audio_pe = 0.0f;
    bridge->pe_state.speech_pe = 0.0f;
    bridge->pe_state.combined_pe = 0.0f;
    bridge->pe_state.visual_pe_variance = 0.0f;
    bridge->pe_state.audio_pe_variance = 0.0f;
    bridge->pe_state.speech_pe_variance = 0.0f;

    /* Reset precisions to initial values */
    bridge->pe_state.visual_precision = bridge->config.initial_visual_precision;
    bridge->pe_state.audio_precision = bridge->config.initial_audio_precision;
    bridge->pe_state.speech_precision = bridge->config.initial_speech_precision;

    /* Clear prediction sources */
    bridge->num_prediction_sources = 0;

    /* Clear reconsolidation windows */
    bridge->num_reconsolidation_windows = 0;
    memset(bridge->reconsolidation_windows, 0,
           bridge->max_reconsolidation_windows * sizeof(pr_reconsolidation_window_t));

    /* Clear free energy history */
    bridge->current_free_energy = 0.0f;
    bridge->history_idx = 0;
    bridge->history_count = 0;
    memset(bridge->free_energy_history, 0, sizeof(bridge->free_energy_history));

    /* Clear pending actions */
    bridge->num_pending_actions = 0;
    memset(bridge->pending_actions, 0, sizeof(bridge->pending_actions));

    /* Reset memory update state */
    memset(&bridge->mem_update_state, 0, sizeof(bridge->mem_update_state));

    /* Reset statistics */
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    bridge->stats.last_reset_time_ms = pr_pred_current_time_ms();
    bridge->stats.min_combined_pe = FLT_MAX;

    /* Reset base bridge stats */
    bridge_base_reset_unlocked(&bridge->base);

    BRIDGE_UNLOCK(bridge);

    NIMCP_LOGGING_DEBUG("Reset predictive bridge");
    return PR_PRED_SUCCESS;
}

//=============================================================================
// Connection Functions - FEP Bridges
//=============================================================================

pr_pred_error_t pr_predictive_bridge_connect_visual_fep(
    pr_predictive_bridge_t* bridge,
    visual_cortex_fep_bridge_t* visual_fep
) {
    if (!bridge) {
        return PR_PRED_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_predictive_bridge_heartbeat("pr_predictiv_connect_visual_fep", 0.0f);


    BRIDGE_LOCK(bridge);
    bridge->visual_fep = visual_fep;
    BRIDGE_UNLOCK(bridge);

    NIMCP_LOGGING_DEBUG("Connected visual FEP bridge");
    return PR_PRED_SUCCESS;
}

pr_pred_error_t pr_predictive_bridge_connect_audio_fep(
    pr_predictive_bridge_t* bridge,
    audio_cortex_fep_bridge_t* audio_fep
) {
    if (!bridge) {
        return PR_PRED_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_predictive_bridge_heartbeat("pr_predictiv_connect_audio_fep", 0.0f);


    BRIDGE_LOCK(bridge);
    bridge->audio_fep = audio_fep;
    BRIDGE_UNLOCK(bridge);

    NIMCP_LOGGING_DEBUG("Connected audio FEP bridge");
    return PR_PRED_SUCCESS;
}

pr_pred_error_t pr_predictive_bridge_connect_speech_fep(
    pr_predictive_bridge_t* bridge,
    speech_cortex_fep_bridge_t* speech_fep
) {
    if (!bridge) {
        return PR_PRED_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_predictive_bridge_heartbeat("pr_predictiv_connect_speech_fep", 0.0f);


    BRIDGE_LOCK(bridge);
    bridge->speech_fep = speech_fep;
    BRIDGE_UNLOCK(bridge);

    NIMCP_LOGGING_DEBUG("Connected speech FEP bridge");
    return PR_PRED_SUCCESS;
}

//=============================================================================
// Connection Functions - PR Bridges
//=============================================================================

pr_pred_error_t pr_predictive_bridge_connect_pr_visual(
    pr_predictive_bridge_t* bridge,
    pr_visual_bridge_t* pr_visual
) {
    if (!bridge) {
        return PR_PRED_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_predictive_bridge_heartbeat("pr_predictiv_connect_pr_visual", 0.0f);


    BRIDGE_LOCK(bridge);
    bridge->pr_visual = pr_visual;
    BRIDGE_UNLOCK(bridge);

    return PR_PRED_SUCCESS;
}

pr_pred_error_t pr_predictive_bridge_connect_pr_audio(
    pr_predictive_bridge_t* bridge,
    pr_audio_bridge_t* pr_audio
) {
    if (!bridge) {
        return PR_PRED_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_predictive_bridge_heartbeat("pr_predictiv_connect_pr_audio", 0.0f);


    BRIDGE_LOCK(bridge);
    bridge->pr_audio = pr_audio;
    BRIDGE_UNLOCK(bridge);

    return PR_PRED_SUCCESS;
}

pr_pred_error_t pr_predictive_bridge_connect_pr_speech(
    pr_predictive_bridge_t* bridge,
    pr_speech_bridge_t* pr_speech
) {
    if (!bridge) {
        return PR_PRED_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_predictive_bridge_heartbeat("pr_predictiv_connect_pr_speech", 0.0f);


    BRIDGE_LOCK(bridge);
    bridge->pr_speech = pr_speech;
    BRIDGE_UNLOCK(bridge);

    return PR_PRED_SUCCESS;
}

pr_pred_error_t pr_predictive_bridge_connect_pr_omni(
    pr_predictive_bridge_t* bridge,
    pr_omni_bridge_t* pr_omni
) {
    if (!bridge) {
        return PR_PRED_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_predictive_bridge_heartbeat("pr_predictiv_connect_pr_omni", 0.0f);


    BRIDGE_LOCK(bridge);
    bridge->pr_omni = pr_omni;
    BRIDGE_UNLOCK(bridge);

    return PR_PRED_SUCCESS;
}

pr_pred_error_t pr_predictive_bridge_connect_node_manager(
    pr_predictive_bridge_t* bridge,
    pr_node_manager_t node_manager
) {
    if (!bridge) {
        return PR_PRED_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_predictive_bridge_heartbeat("pr_predictiv_connect_node_manager", 0.0f);


    BRIDGE_LOCK(bridge);
    bridge->node_manager = node_manager;
    BRIDGE_UNLOCK(bridge);

    NIMCP_LOGGING_DEBUG("Connected PR node manager");
    return PR_PRED_SUCCESS;
}

//=============================================================================
// Main Update Functions
//=============================================================================

pr_pred_error_t pr_predictive_bridge_update(
    pr_predictive_bridge_t* bridge,
    uint64_t delta_ms
) {
    if (!bridge) {
        return PR_PRED_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_predictive_bridge_heartbeat("pr_predictiv_update", 0.0f);


    BRIDGE_LOCK(bridge);

    uint64_t current_time_ms = pr_pred_current_time_ms();
    pr_pred_error_t result = PR_PRED_SUCCESS;

    /* Step 1: Collect PE from FEP bridges */
    result = pr_predictive_bridge_collect_pe(bridge);
    if (result != PR_PRED_SUCCESS) {
        /* Non-fatal: may not have any connected bridges */
        NIMCP_LOGGING_DEBUG("PE collection returned: %d", result);
    }

    /* Step 2: Compute combined PE */
    float combined_pe = 0.0f;
    pr_predictive_bridge_compute_combined_pe(bridge, &combined_pe);

    /* Step 3: Update precision from PE variance (if enabled) */
    if (bridge->config.enable_precision_adaptation) {
        pr_predictive_bridge_update_precision(bridge);
    }

    /* Step 4: Process reconsolidation windows */
    if (bridge->config.enable_reconsolidation) {
        process_reconsolidation_windows(bridge, current_time_ms);
    }

    /* Step 5: Update memories based on PE */
    result = pr_predictive_bridge_update_memories_from_pe(bridge);

    /* Step 6: Compute free energy */
    if (bridge->config.track_free_energy) {
        float free_energy = 0.0f;
        pr_predictive_bridge_compute_free_energy(bridge, &free_energy);
    }

    /* Step 7: Generate active inference actions */
    if (bridge->config.enable_active_inference) {
        pr_predictive_bridge_generate_actions(bridge);
    }

    /* Update statistics */
    bridge->stats.total_updates++;
    bridge->base.total_updates++;
    bridge->base.last_update_time_ms = current_time_ms;

    /* Update PE statistics */
    if (combined_pe > 0.0f) {
        double n = (double)bridge->stats.pe_collections;
        bridge->stats.avg_combined_pe =
            (bridge->stats.avg_combined_pe * n + combined_pe) / (n + 1.0);
        bridge->stats.pe_collections++;

        if (combined_pe > bridge->stats.max_combined_pe) {
            bridge->stats.max_combined_pe = combined_pe;
        }
        if (combined_pe < bridge->stats.min_combined_pe) {
            bridge->stats.min_combined_pe = combined_pe;
        }
    }

    BRIDGE_UNLOCK(bridge);

    return PR_PRED_SUCCESS;
}

//=============================================================================
// Prediction Error Functions
//=============================================================================

pr_pred_error_t pr_predictive_bridge_collect_pe(pr_predictive_bridge_t* bridge) {
    if (!bridge) {
        return PR_PRED_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_predictive_bridge_heartbeat("pr_predictiv_collect_pe", 0.0f);


    uint64_t current_time_ms = pr_pred_current_time_ms();
    bool collected_any = false;

    /* Collect visual PE */
    if (bridge->config.enable_visual && bridge->visual_fep) {
        visual_cortex_fep_state_t visual_state;
        if (visual_cortex_fep_bridge_get_state(bridge->visual_fep, &visual_state) == 0) {
            bridge->pe_state.visual_pe = visual_state.current_visual_pe;
            bridge->pe_state.last_visual_update_ms = current_time_ms;
            add_to_pe_history(&s_visual_pe_history, visual_state.current_visual_pe);
            collected_any = true;
        }
    }

    /* Collect audio PE */
    if (bridge->config.enable_audio && bridge->audio_fep) {
        audio_cortex_fep_state_t audio_state;
        if (audio_cortex_fep_bridge_get_state(bridge->audio_fep, &audio_state) == 0) {
            bridge->pe_state.audio_pe = audio_state.current_auditory_pe;
            bridge->pe_state.last_audio_update_ms = current_time_ms;
            add_to_pe_history(&s_audio_pe_history, audio_state.current_auditory_pe);
            collected_any = true;
        }
    }

    /* Collect speech PE */
    if (bridge->config.enable_speech && bridge->speech_fep) {
        speech_cortex_fep_state_t speech_state;
        if (speech_cortex_fep_bridge_get_state(bridge->speech_fep, &speech_state) == 0) {
            bridge->pe_state.speech_pe = speech_state.current_phoneme_pe;
            bridge->pe_state.last_speech_update_ms = current_time_ms;
            add_to_pe_history(&s_speech_pe_history, speech_state.current_phoneme_pe);
            collected_any = true;
        }
    }

    if (!collected_any) {
        return PR_PRED_ERROR_NOT_CONNECTED;
    }

    return PR_PRED_SUCCESS;
}

pr_pred_error_t pr_predictive_bridge_compute_combined_pe(
    pr_predictive_bridge_t* bridge,
    float* combined_pe
) {
    if (!bridge || !combined_pe) {
        return PR_PRED_ERROR_NULL_POINTER;
    }

    /* Precision-weighted sum */
    /* Phase 8: Heartbeat at operation start */
    pr_predictive_bridge_heartbeat("pr_predictiv_compute_combined_pe", 0.0f);


    float weighted_sum = 0.0f;
    float precision_sum = 0.0f;

    /* Visual contribution */
    if (bridge->config.enable_visual && bridge->pe_state.visual_precision > PR_PRED_EPSILON) {
        weighted_sum += bridge->pe_state.visual_precision * bridge->pe_state.visual_pe;
        precision_sum += bridge->pe_state.visual_precision;
    }

    /* Audio contribution */
    if (bridge->config.enable_audio && bridge->pe_state.audio_precision > PR_PRED_EPSILON) {
        weighted_sum += bridge->pe_state.audio_precision * bridge->pe_state.audio_pe;
        precision_sum += bridge->pe_state.audio_precision;
    }

    /* Speech contribution */
    if (bridge->config.enable_speech && bridge->pe_state.speech_precision > PR_PRED_EPSILON) {
        weighted_sum += bridge->pe_state.speech_precision * bridge->pe_state.speech_pe;
        precision_sum += bridge->pe_state.speech_precision;
    }

    /* Compute weighted average */
    if (precision_sum > PR_PRED_EPSILON) {
        *combined_pe = weighted_sum / precision_sum;
    } else {
        *combined_pe = 0.0f;
    }

    bridge->pe_state.combined_pe = *combined_pe;
    return PR_PRED_SUCCESS;
}

pr_pred_error_t pr_predictive_bridge_update_precision(pr_predictive_bridge_t* bridge) {
    if (!bridge) {
        return PR_PRED_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_predictive_bridge_heartbeat("pr_predictiv_update_precision", 0.0f);


    float rate = bridge->config.precision_adaptation_rate;

    /* Update visual precision from variance */
    float visual_var = compute_variance(&s_visual_pe_history);
    bridge->pe_state.visual_pe_variance = visual_var;
    if (visual_var > PR_PRED_EPSILON) {
        /* precision = 1 / variance, adapted gradually */
        float target_precision = 1.0f / (visual_var + PR_PRED_EPSILON);
        target_precision = clamp_precision(target_precision);
        bridge->pe_state.visual_precision =
            bridge->pe_state.visual_precision * (1.0f - rate) + target_precision * rate;
    }

    /* Update audio precision */
    float audio_var = compute_variance(&s_audio_pe_history);
    bridge->pe_state.audio_pe_variance = audio_var;
    if (audio_var > PR_PRED_EPSILON) {
        float target_precision = 1.0f / (audio_var + PR_PRED_EPSILON);
        target_precision = clamp_precision(target_precision);
        bridge->pe_state.audio_precision =
            bridge->pe_state.audio_precision * (1.0f - rate) + target_precision * rate;
    }

    /* Update speech precision */
    float speech_var = compute_variance(&s_speech_pe_history);
    bridge->pe_state.speech_pe_variance = speech_var;
    if (speech_var > PR_PRED_EPSILON) {
        float target_precision = 1.0f / (speech_var + PR_PRED_EPSILON);
        target_precision = clamp_precision(target_precision);
        bridge->pe_state.speech_precision =
            bridge->pe_state.speech_precision * (1.0f - rate) + target_precision * rate;
    }

    return PR_PRED_SUCCESS;
}

pr_pred_error_t pr_predictive_bridge_set_precision(
    pr_predictive_bridge_t* bridge,
    float visual_precision,
    float audio_precision,
    float speech_precision
) {
    if (!bridge) {
        return PR_PRED_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_predictive_bridge_heartbeat("pr_predictiv_set_precision", 0.0f);


    BRIDGE_LOCK(bridge);

    if (!isnan(visual_precision)) {
        bridge->pe_state.visual_precision = clamp_precision(visual_precision);
    }
    if (!isnan(audio_precision)) {
        bridge->pe_state.audio_precision = clamp_precision(audio_precision);
    }
    if (!isnan(speech_precision)) {
        bridge->pe_state.speech_precision = clamp_precision(speech_precision);
    }

    BRIDGE_UNLOCK(bridge);
    return PR_PRED_SUCCESS;
}

//=============================================================================
// Prediction Generation Functions
//=============================================================================

pr_pred_error_t pr_predictive_bridge_generate_predictions(
    pr_predictive_bridge_t* bridge,
    const resonance_query_t* query
) {
    if (!bridge) {
        return PR_PRED_ERROR_NULL_POINTER;
    }

    /* Clear existing prediction sources */
    /* Phase 8: Heartbeat at operation start */
    pr_predictive_bridge_heartbeat("pr_predictiv_generate_predictions", 0.0f);


    bridge->num_prediction_sources = 0;

    /* If no node manager, we can't generate memory-based predictions */
    if (!bridge->node_manager) {
        return PR_PRED_SUCCESS;
    }

    /* This is a simplified implementation. A full implementation would:
     * 1. Query PR memory system for high-resonance memories
     * 2. Weight their contributions by resonance score
     * 3. Generate sensory predictions from weighted combination
     *
     * For now, we just increment the counter for tracking.
     */
    bridge->stats.predictions_generated++;

    return PR_PRED_SUCCESS;
}

pr_pred_error_t pr_predictive_bridge_add_prediction_source(
    pr_predictive_bridge_t* bridge,
    const pr_memory_node_t* node,
    float resonance_score
) {
    if (!bridge || !node) {
        return PR_PRED_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_predictive_bridge_heartbeat("pr_predictiv_add_prediction_sourc", 0.0f);


    if (bridge->num_prediction_sources >= PR_PRED_MAX_PREDICTION_SOURCES) {
        return PR_PRED_ERROR_BUFFER_FULL;
    }

    BRIDGE_LOCK(bridge);

    pr_prediction_source_t* source =
        &bridge->prediction_sources[bridge->num_prediction_sources];

    source->node_id = pr_memory_node_get_id(node);
    source->resonance_score = resonance_score;
    source->contribution_weight = resonance_score;  /* Simple: weight = resonance */
    source->state = pr_memory_node_get_state(node);

    bridge->num_prediction_sources++;

    BRIDGE_UNLOCK(bridge);
    return PR_PRED_SUCCESS;
}

pr_pred_error_t pr_predictive_bridge_clear_prediction_sources(
    pr_predictive_bridge_t* bridge
) {
    if (!bridge) {
        return PR_PRED_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_predictive_bridge_heartbeat("pr_predictiv_clear_prediction_sou", 0.0f);


    BRIDGE_LOCK(bridge);
    bridge->num_prediction_sources = 0;
    BRIDGE_UNLOCK(bridge);

    return PR_PRED_SUCCESS;
}

float pr_predictive_bridge_resonance_to_confidence(
    const pr_predictive_bridge_t* bridge,
    float resonance
) {
    if (!bridge) {
        return 0.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_predictive_bridge_heartbeat("pr_predictiv_resonance_to_confide", 0.0f);


    return bridge->config.resonance_prediction_weight * resonance;
}

//=============================================================================
// Memory Update Functions
//=============================================================================

pr_pred_error_t pr_predictive_bridge_update_memories_from_pe(
    pr_predictive_bridge_t* bridge
) {
    if (!bridge) {
        return PR_PRED_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_predictive_bridge_heartbeat("pr_predictiv_update_memories_from", 0.0f);


    float pe = bridge->pe_state.combined_pe;

    /* Find max resonance among prediction sources */
    float max_resonance = 0.0f;
    for (size_t i = 0; i < bridge->num_prediction_sources; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_prediction_sources > 256) {
            pr_predictive_bridge_heartbeat("pr_predictiv_loop",
                             (float)(i + 1) / (float)bridge->num_prediction_sources);
        }

        if (bridge->prediction_sources[i].resonance_score > max_resonance) {
            max_resonance = bridge->prediction_sources[i].resonance_score;
        }
    }

    /* Determine update mode */
    pr_mem_update_mode_t mode = determine_update_mode(bridge, pe, max_resonance);
    bridge->mem_update_state.last_mode = mode;
    bridge->mem_update_state.last_update_time_ms = pr_pred_current_time_ms();

    switch (mode) {
        case PR_MEM_UPDATE_CONSOLIDATE:
            /* Low PE + high resonance: strengthen existing memories */
            for (size_t i = 0; i < bridge->num_prediction_sources; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && bridge->num_prediction_sources > 256) {
                    pr_predictive_bridge_heartbeat("pr_predictiv_loop",
                                     (float)(i + 1) / (float)bridge->num_prediction_sources);
                }

                if (bridge->prediction_sources[i].resonance_score > 0.5f) {
                    /* Strengthen via quaternion.w boost */
                    /* In a full implementation, we'd fetch the actual node and modify it */
                    bridge->mem_update_state.consolidations++;
                    bridge->stats.memories_strengthened++;
                }
            }
            break;

        case PR_MEM_UPDATE_RECONSOLIDATE:
            /* High PE + high resonance: update existing memories */
            if (bridge->config.enable_reconsolidation) {
                for (size_t i = 0; i < bridge->num_prediction_sources; i++) {
                    /* Phase 8: Loop progress heartbeat */
                    if ((i & 0xFF) == 0 && bridge->num_prediction_sources > 256) {
                        pr_predictive_bridge_heartbeat("pr_predictiv_loop",
                                         (float)(i + 1) / (float)bridge->num_prediction_sources);
                    }

                    if (bridge->prediction_sources[i].resonance_score > 0.5f) {
                        pr_predictive_bridge_trigger_reconsolidation(
                            bridge,
                            bridge->prediction_sources[i].node_id
                        );
                        bridge->mem_update_state.reconsolidations++;
                        bridge->stats.memories_updated++;
                    }
                }
            }
            break;

        case PR_MEM_UPDATE_ENCODE_NEW:
            /* High PE + low resonance: would create new memory */
            /* In a full implementation, this would encode new memory */
            bridge->mem_update_state.new_encodings++;
            bridge->stats.memories_created++;
            break;

        case PR_MEM_UPDATE_NONE:
        default:
            /* No action needed */
            break;
    }

    return PR_PRED_SUCCESS;
}

pr_pred_error_t pr_predictive_bridge_trigger_reconsolidation(
    pr_predictive_bridge_t* bridge,
    uint64_t node_id
) {
    if (!bridge) {
        return PR_PRED_ERROR_NULL_POINTER;
    }

    /* Check if already in reconsolidation */
    /* Phase 8: Heartbeat at operation start */
    pr_predictive_bridge_heartbeat("pr_predictiv_trigger_reconsolidat", 0.0f);


    for (size_t i = 0; i < bridge->num_reconsolidation_windows; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_reconsolidation_windows > 256) {
            pr_predictive_bridge_heartbeat("pr_predictiv_loop",
                             (float)(i + 1) / (float)bridge->num_reconsolidation_windows);
        }

        if (bridge->reconsolidation_windows[i].active &&
            bridge->reconsolidation_windows[i].node_id == node_id) {
            /* Already open, just accumulate PE */
            bridge->reconsolidation_windows[i].accumulated_pe +=
                bridge->pe_state.combined_pe;
            return PR_PRED_SUCCESS;
        }
    }

    /* Find free slot */
    pr_reconsolidation_window_t* window = NULL;
    for (size_t i = 0; i < bridge->max_reconsolidation_windows; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_reconsolidation_windows > 256) {
            pr_predictive_bridge_heartbeat("pr_predictiv_loop",
                             (float)(i + 1) / (float)bridge->max_reconsolidation_windows);
        }

        if (!bridge->reconsolidation_windows[i].active) {
            window = &bridge->reconsolidation_windows[i];
            break;
        }
    }

    if (!window) {
        return PR_PRED_ERROR_BUFFER_FULL;
    }

    /* Open new reconsolidation window */
    window->node_id = node_id;
    window->window_start_ms = pr_pred_current_time_ms();
    window->window_duration_ms = (uint64_t)bridge->config.reconsolidation_duration_ms;
    window->accumulated_pe = bridge->pe_state.combined_pe;
    window->update_magnitude = 0.0f;
    window->active = true;

    bridge->num_reconsolidation_windows++;

    NIMCP_LOGGING_DEBUG("Opened reconsolidation window for node %lu", node_id);
    return PR_PRED_SUCCESS;
}

pr_pred_error_t pr_predictive_bridge_close_reconsolidation(
    pr_predictive_bridge_t* bridge,
    uint64_t node_id
) {
    if (!bridge) {
        return PR_PRED_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_predictive_bridge_heartbeat("pr_predictiv_close_reconsolidatio", 0.0f);


    for (size_t i = 0; i < bridge->max_reconsolidation_windows; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_reconsolidation_windows > 256) {
            pr_predictive_bridge_heartbeat("pr_predictiv_loop",
                             (float)(i + 1) / (float)bridge->max_reconsolidation_windows);
        }

        if (bridge->reconsolidation_windows[i].active &&
            bridge->reconsolidation_windows[i].node_id == node_id) {

            pr_reconsolidation_window_t* window = &bridge->reconsolidation_windows[i];

            /* Calculate update magnitude from accumulated PE */
            window->update_magnitude = window->accumulated_pe * 0.1f;  /* Scaled factor */

            /* In a full implementation, we'd apply the update to the actual memory node */
            bridge->mem_update_state.last_updated_node_id = node_id;

            /* Close window */
            window->active = false;
            bridge->num_reconsolidation_windows--;

            NIMCP_LOGGING_DEBUG("Closed reconsolidation window for node %lu, update=%.3f",
                               node_id, window->update_magnitude);
            return PR_PRED_SUCCESS;
        }
    }

    return PR_PRED_ERROR_RECONSOLIDATION;
}

pr_pred_error_t pr_predictive_bridge_strengthen_memory(
    pr_predictive_bridge_t* bridge,
    pr_memory_node_t* node,
    float boost
) {
    if (!bridge || !node) {
        return PR_PRED_ERROR_NULL_POINTER;
    }

    /* Get current state */
    /* Phase 8: Heartbeat at operation start */
    pr_predictive_bridge_heartbeat("pr_predictiv_strengthen_memory", 0.0f);


    nimcp_quaternion_t state = pr_memory_node_get_state(node);

    /* Boost consolidation (w component) */
    state.w += boost;
    if (state.w > 1.0f) {
        state.w = 1.0f;
    }

    /* Update node state */
    pr_node_error_t err = pr_memory_node_update_state(node, state);
    if (err != PR_NODE_SUCCESS) {
        return PR_PRED_ERROR_PR_FAILED;
    }

    bridge->stats.memories_strengthened++;
    return PR_PRED_SUCCESS;
}

pr_pred_error_t pr_predictive_bridge_encode_new_memory(
    pr_predictive_bridge_t* bridge,
    const void* data,
    size_t data_size,
    nimcp_quaternion_t initial_state,
    pr_memory_node_t** new_node
) {
    if (!bridge || !data || data_size == 0) {
        return PR_PRED_ERROR_NULL_POINTER;
    }

    if (!bridge->node_manager) {
        return PR_PRED_ERROR_NOT_CONNECTED;
    }

    /* Configure new node */
    /* Phase 8: Heartbeat at operation start */
    pr_predictive_bridge_heartbeat("pr_predictiv_encode_new_memory", 0.0f);


    pr_node_config_t config = pr_memory_node_default_config();
    config.initial_tier = PR_MEMORY_TIER_Z0;  /* Start in working memory */
    config.initial_strength = initial_state.w;
    config.emotional_valence = initial_state.x;
    config.salience = initial_state.y;
    config.accessibility = initial_state.z;

    /* Create node */
    pr_memory_node_t* node = pr_memory_node_create(
        bridge->node_manager, data, data_size, &config
    );
    if (!node) {
        return PR_PRED_ERROR_NO_MEMORY;
    }

    if (new_node) {
        *new_node = node;
    }

    bridge->stats.memories_created++;
    bridge->mem_update_state.new_encodings++;

    NIMCP_LOGGING_DEBUG("Created new memory node from high PE context");
    return PR_PRED_SUCCESS;
}

//=============================================================================
// Free Energy Functions
//=============================================================================

pr_pred_error_t pr_predictive_bridge_compute_free_energy(
    pr_predictive_bridge_t* bridge,
    float* free_energy
) {
    if (!bridge || !free_energy) {
        return PR_PRED_ERROR_NULL_POINTER;
    }

    /*
     * Simplified free energy: F = PE + complexity
     *
     * Full variational free energy would be:
     * F = -log P(o|m) + KL[q(s)||p(s|m)]
     *
     * where:
     * - -log P(o|m) ≈ PE (prediction error)
     * - KL term ≈ complexity of model
     */

    /* Phase 8: Heartbeat at operation start */
    pr_predictive_bridge_heartbeat("pr_predictiv_compute_free_energy", 0.0f);


    float pe = bridge->pe_state.combined_pe;

    /* Complexity term: based on number of active prediction sources */
    float complexity = PR_PRED_COMPLEXITY_WEIGHT *
                       (float)bridge->num_prediction_sources;

    *free_energy = pe + complexity;
    bridge->current_free_energy = *free_energy;

    /* Add to history */
    bridge->free_energy_history[bridge->history_idx] = *free_energy;
    bridge->history_idx = (bridge->history_idx + 1) % PR_PRED_FREE_ENERGY_HISTORY_SIZE;
    if (bridge->history_count < PR_PRED_FREE_ENERGY_HISTORY_SIZE) {
        bridge->history_count++;
    }

    /* Update statistics */
    double n = (double)bridge->stats.total_updates;
    if (n > 0) {
        bridge->stats.avg_free_energy =
            (bridge->stats.avg_free_energy * (n - 1) + *free_energy) / n;
    } else {
        bridge->stats.avg_free_energy = *free_energy;
    }
    bridge->stats.current_free_energy = *free_energy;

    return PR_PRED_SUCCESS;
}

pr_pred_error_t pr_predictive_bridge_get_free_energy_history(
    const pr_predictive_bridge_t* bridge,
    float* history,
    size_t* count
) {
    if (!bridge || !history || !count) {
        return PR_PRED_ERROR_NULL_POINTER;
    }

    *count = bridge->history_count;
    /* Phase 8: Heartbeat at operation start */
    pr_predictive_bridge_heartbeat("pr_predictiv_get_free_energy_hist", 0.0f);


    memcpy(history, bridge->free_energy_history,
           bridge->history_count * sizeof(float));

    return PR_PRED_SUCCESS;
}

//=============================================================================
// Active Inference Functions
//=============================================================================

pr_pred_error_t pr_predictive_bridge_select_action(
    pr_predictive_bridge_t* bridge,
    pr_active_inference_action_t* selected_action
) {
    if (!bridge || !selected_action) {
        return PR_PRED_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_predictive_bridge_heartbeat("pr_predictiv_select_action", 0.0f);


    if (bridge->num_pending_actions == 0) {
        memset(selected_action, 0, sizeof(pr_active_inference_action_t));
        return PR_PRED_SUCCESS;
    }

    /* Collect expected PE reductions */
    float pe_reductions[PR_PRED_MAX_PENDING_ACTIONS];
    for (size_t i = 0; i < bridge->num_pending_actions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->num_pending_actions > 256) {
            pr_predictive_bridge_heartbeat("pr_predictiv_loop",
                             (float)(i + 1) / (float)bridge->num_pending_actions);
        }

        pe_reductions[i] = bridge->pending_actions[i].expected_pe_reduction;
    }

    /* Softmax selection */
    size_t selected_idx = (size_t)softmax_select(
        pe_reductions,
        bridge->num_pending_actions,
        bridge->config.action_temperature
    );

    *selected_action = bridge->pending_actions[selected_idx];
    return PR_PRED_SUCCESS;
}

int pr_predictive_bridge_generate_actions(pr_predictive_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    /* Clear old actions */
    /* Phase 8: Heartbeat at operation start */
    pr_predictive_bridge_heartbeat("pr_predictiv_generate_actions", 0.0f);


    bridge->num_pending_actions = 0;

    float visual_pe = bridge->pe_state.visual_pe;
    float audio_pe = bridge->pe_state.audio_pe;
    float speech_pe = bridge->pe_state.speech_pe;
    uint64_t current_time = pr_pred_current_time_ms();

    /* Generate actions based on which modalities have high PE */

    /* Visual actions */
    if (bridge->config.enable_visual && visual_pe > bridge->config.pe_threshold_update) {
        if (bridge->num_pending_actions < bridge->config.max_pending_actions) {
            pr_active_inference_action_t* action =
                &bridge->pending_actions[bridge->num_pending_actions];
            action->type = PR_ACTION_SACCADE;
            action->expected_pe_reduction = visual_pe * 0.5f;
            action->priority = visual_pe;
            action->created_time_ms = current_time;
            action->executed = false;
            bridge->num_pending_actions++;
        }

        if (bridge->num_pending_actions < bridge->config.max_pending_actions) {
            pr_active_inference_action_t* action =
                &bridge->pending_actions[bridge->num_pending_actions];
            action->type = PR_ACTION_ATTEND_VISUAL;
            action->expected_pe_reduction = visual_pe * 0.3f;
            action->priority = visual_pe * 0.8f;
            action->created_time_ms = current_time;
            action->executed = false;
            bridge->num_pending_actions++;
        }
    }

    /* Audio actions */
    if (bridge->config.enable_audio && audio_pe > bridge->config.pe_threshold_update) {
        if (bridge->num_pending_actions < bridge->config.max_pending_actions) {
            pr_active_inference_action_t* action =
                &bridge->pending_actions[bridge->num_pending_actions];
            action->type = PR_ACTION_ORIENT_AUDIO;
            action->expected_pe_reduction = audio_pe * 0.4f;
            action->priority = audio_pe;
            action->created_time_ms = current_time;
            action->executed = false;
            bridge->num_pending_actions++;
        }

        if (bridge->num_pending_actions < bridge->config.max_pending_actions) {
            pr_active_inference_action_t* action =
                &bridge->pending_actions[bridge->num_pending_actions];
            action->type = PR_ACTION_ATTEND_AUDIO;
            action->expected_pe_reduction = audio_pe * 0.3f;
            action->priority = audio_pe * 0.8f;
            action->created_time_ms = current_time;
            action->executed = false;
            bridge->num_pending_actions++;
        }
    }

    /* Speech actions */
    if (bridge->config.enable_speech && speech_pe > bridge->config.pe_threshold_update) {
        if (bridge->num_pending_actions < bridge->config.max_pending_actions) {
            pr_active_inference_action_t* action =
                &bridge->pending_actions[bridge->num_pending_actions];
            action->type = PR_ACTION_ATTEND_SPEECH;
            action->expected_pe_reduction = speech_pe * 0.3f;
            action->priority = speech_pe;
            action->created_time_ms = current_time;
            action->executed = false;
            bridge->num_pending_actions++;
        }
    }

    /* Memory query action if any PE is high */
    float max_pe = visual_pe > audio_pe ? visual_pe : audio_pe;
    max_pe = max_pe > speech_pe ? max_pe : speech_pe;

    if (max_pe > bridge->config.pe_threshold_update &&
        bridge->num_pending_actions < bridge->config.max_pending_actions) {
        pr_active_inference_action_t* action =
            &bridge->pending_actions[bridge->num_pending_actions];
        action->type = PR_ACTION_QUERY_MEMORY;
        action->expected_pe_reduction = max_pe * 0.6f;
        action->priority = max_pe * 0.9f;
        action->created_time_ms = current_time;
        action->executed = false;
        bridge->num_pending_actions++;
    }

    bridge->stats.actions_generated += bridge->num_pending_actions;
    return (int)bridge->num_pending_actions;
}

pr_pred_error_t pr_predictive_bridge_execute_action(
    pr_predictive_bridge_t* bridge,
    size_t action_idx
) {
    if (!bridge) {
        return PR_PRED_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_predictive_bridge_heartbeat("pr_predictiv_execute_action", 0.0f);


    if (action_idx >= bridge->num_pending_actions) {
        return PR_PRED_ERROR_INVALID_ACTION;
    }

    BRIDGE_LOCK(bridge);
    bridge->pending_actions[action_idx].executed = true;
    bridge->stats.actions_executed++;
    BRIDGE_UNLOCK(bridge);

    return PR_PRED_SUCCESS;
}

int pr_predictive_bridge_get_pending_actions(
    const pr_predictive_bridge_t* bridge,
    pr_active_inference_action_t* actions,
    size_t max_actions
) {
    if (!bridge || !actions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_predictive_bridge_get_pending_actions: required parameter is NULL (bridge, actions)");
        return -1;
        BRIDGE_BBB_VALIDATE(bridge, actions, sizeof(*actions));
    }

    /* Phase 8: Heartbeat at operation start */
    pr_predictive_bridge_heartbeat("pr_predictiv_get_pending_actions", 0.0f);


    size_t count = bridge->num_pending_actions;
    if (count > max_actions) {
        count = max_actions;
    }

    memcpy(actions, bridge->pending_actions, count * sizeof(pr_active_inference_action_t));
    return (int)count;
}

pr_pred_error_t pr_predictive_bridge_clear_actions(pr_predictive_bridge_t* bridge) {
    if (!bridge) {
        return PR_PRED_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_predictive_bridge_heartbeat("pr_predictiv_clear_actions", 0.0f);


    BRIDGE_LOCK(bridge);
    bridge->num_pending_actions = 0;
    memset(bridge->pending_actions, 0, sizeof(bridge->pending_actions));
    BRIDGE_UNLOCK(bridge);

    return PR_PRED_SUCCESS;
}

//=============================================================================
// State and Statistics Functions
//=============================================================================

pr_pred_error_t pr_predictive_bridge_get_pe_state(
    const pr_predictive_bridge_t* bridge,
    pr_pred_error_state_t* pe_state
) {
    if (!bridge || !pe_state) {
        return PR_PRED_ERROR_NULL_POINTER;
    }

    *pe_state = bridge->pe_state;
    /* Phase 8: Heartbeat at operation start */
    pr_predictive_bridge_heartbeat("pr_predictiv_get_pe_state", 0.0f);


    return PR_PRED_SUCCESS;
}

pr_pred_error_t pr_predictive_bridge_get_mem_update_state(
    const pr_predictive_bridge_t* bridge,
    pr_mem_update_state_t* mem_state
) {
    if (!bridge || !mem_state) {
        return PR_PRED_ERROR_NULL_POINTER;
    }

    *mem_state = bridge->mem_update_state;
    /* Phase 8: Heartbeat at operation start */
    pr_predictive_bridge_heartbeat("pr_predictiv_get_mem_update_state", 0.0f);


    return PR_PRED_SUCCESS;
}

pr_pred_error_t pr_predictive_bridge_get_stats(
    const pr_predictive_bridge_t* bridge,
    pr_predictive_bridge_stats_t* stats
) {
    if (!bridge || !stats) {
        return PR_PRED_ERROR_NULL_POINTER;
    }

    *stats = bridge->stats;
    /* Phase 8: Heartbeat at operation start */
    pr_predictive_bridge_heartbeat("pr_predictiv_get_stats", 0.0f);


    return PR_PRED_SUCCESS;
}

pr_pred_error_t pr_predictive_bridge_reset_stats(pr_predictive_bridge_t* bridge) {
    if (!bridge) {
        return PR_PRED_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_predictive_bridge_heartbeat("pr_predictiv_reset_stats", 0.0f);


    BRIDGE_LOCK(bridge);
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    bridge->stats.last_reset_time_ms = pr_pred_current_time_ms();
    bridge->stats.min_combined_pe = FLT_MAX;
    BRIDGE_UNLOCK(bridge);

    return PR_PRED_SUCCESS;
}

//=============================================================================
// Utility Functions
//=============================================================================

const char* pr_pred_error_string(pr_pred_error_t error) {
    switch (error) {
        case PR_PRED_SUCCESS:
            return "Success";
        case PR_PRED_ERROR_NULL_POINTER:
            return "NULL pointer argument";
        case PR_PRED_ERROR_INVALID_CONFIG:
            return "Invalid configuration";
        case PR_PRED_ERROR_NO_MEMORY:
            return "Memory allocation failed";
        case PR_PRED_ERROR_NOT_CONNECTED:
            return "Required bridge not connected";
        case PR_PRED_ERROR_FEP_FAILED:
            return "FEP operation failed";
        case PR_PRED_ERROR_PR_FAILED:
            return "PR memory operation failed";
        case PR_PRED_ERROR_COMPUTE_FAILED:
            return "Computation failed";
        case PR_PRED_ERROR_BUFFER_FULL:
            return "Buffer full";
        case PR_PRED_ERROR_INVALID_ACTION:
            return "Invalid action";
        case PR_PRED_ERROR_RECONSOLIDATION:
            return "Reconsolidation operation failed";
        default:
            return "Unknown error";
    }
}

const char* pr_pred_action_name(pr_action_type_t action_type) {
    switch (action_type) {
        case PR_ACTION_NONE:
            return "NONE";
        case PR_ACTION_SACCADE:
            return "SACCADE";
        case PR_ACTION_ORIENT_AUDIO:
            return "ORIENT_AUDIO";
        case PR_ACTION_ARTICULATE:
            return "ARTICULATE";
        case PR_ACTION_QUERY_MEMORY:
            return "QUERY_MEMORY";
        case PR_ACTION_ATTEND_VISUAL:
            return "ATTEND_VISUAL";
        case PR_ACTION_ATTEND_AUDIO:
            return "ATTEND_AUDIO";
        case PR_ACTION_ATTEND_SPEECH:
            return "ATTEND_SPEECH";
        case PR_ACTION_SUPPRESS:
            return "SUPPRESS";
        default:
            return "UNKNOWN";
    }
}

const char* pr_pred_update_mode_name(pr_mem_update_mode_t mode) {
    switch (mode) {
        case PR_MEM_UPDATE_NONE:
            return "NONE";
        case PR_MEM_UPDATE_CONSOLIDATE:
            return "CONSOLIDATE";
        case PR_MEM_UPDATE_RECONSOLIDATE:
            return "RECONSOLIDATE";
        case PR_MEM_UPDATE_ENCODE_NEW:
            return "ENCODE_NEW";
        default:
            return "UNKNOWN";
    }
}

void pr_predictive_bridge_print_state(const pr_predictive_bridge_t* bridge) {
    if (!bridge) {
        printf("pr_predictive_bridge: NULL\n");
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_predictive_bridge_heartbeat("pr_predictiv_print_state", 0.0f);


    printf("=== PR Predictive Bridge State ===\n");
    printf("PE State:\n");
    printf("  Visual PE: %.4f (precision: %.4f)\n",
           bridge->pe_state.visual_pe, bridge->pe_state.visual_precision);
    printf("  Audio PE: %.4f (precision: %.4f)\n",
           bridge->pe_state.audio_pe, bridge->pe_state.audio_precision);
    printf("  Speech PE: %.4f (precision: %.4f)\n",
           bridge->pe_state.speech_pe, bridge->pe_state.speech_precision);
    printf("  Combined PE: %.4f\n", bridge->pe_state.combined_pe);
    printf("\n");

    printf("Free Energy: %.4f (avg: %.4f)\n",
           bridge->current_free_energy, bridge->stats.avg_free_energy);
    printf("\n");

    printf("Prediction Sources: %zu\n", bridge->num_prediction_sources);
    printf("Reconsolidation Windows: %zu active\n", bridge->num_reconsolidation_windows);
    printf("Pending Actions: %zu\n", bridge->num_pending_actions);
    printf("\n");

    printf("Memory Updates:\n");
    printf("  Consolidations: %lu\n", bridge->mem_update_state.consolidations);
    printf("  Reconsolidations: %lu\n", bridge->mem_update_state.reconsolidations);
    printf("  New Encodings: %lu\n", bridge->mem_update_state.new_encodings);
    printf("  Last Mode: %s\n", pr_pred_update_mode_name(bridge->mem_update_state.last_mode));
    printf("\n");

    printf("Statistics:\n");
    printf("  Total Updates: %lu\n", bridge->stats.total_updates);
    printf("  PE Collections: %lu\n", bridge->stats.pe_collections);
    printf("  Avg Combined PE: %.4f\n", bridge->stats.avg_combined_pe);
    printf("  Actions Generated: %lu\n", bridge->stats.actions_generated);
    printf("  Actions Executed: %lu\n", bridge->stats.actions_executed);
    printf("================================\n");
}

bool pr_predictive_bridge_is_ready(const pr_predictive_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    /* Need at least one FEP bridge connected */
    /* Phase 8: Heartbeat at operation start */
    pr_predictive_bridge_heartbeat("pr_predictiv_is_ready", 0.0f);


    bool has_fep =
        (bridge->config.enable_visual && bridge->visual_fep != NULL) ||
        (bridge->config.enable_audio && bridge->audio_fep != NULL) ||
        (bridge->config.enable_speech && bridge->speech_fep != NULL);

    return has_fep;
}

uint64_t pr_pred_current_time_ms(void) {
    /* Use PR node time function if available */
    /* Phase 8: Heartbeat at operation start */
    pr_predictive_bridge_heartbeat("pr_predictiv_pr_pred_current_time", 0.0f);


    return pr_node_current_time_ms();
}

//=============================================================================
// Internal Helper Functions
//=============================================================================

static void set_error(const char* msg) {
    if (msg) {
        strncpy(s_last_error, msg, sizeof(s_last_error) - 1);
        s_last_error[sizeof(s_last_error) - 1] = '\0';
    }
}

static float compute_variance(const pe_history_t* history) {
    if (!history || history->count < 2) {
        return 0.0f;
    }

    /* Compute mean */
    float sum = 0.0f;
    for (size_t i = 0; i < history->count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && history->count > 256) {
            pr_predictive_bridge_heartbeat("pr_predictiv_loop",
                             (float)(i + 1) / (float)history->count);
        }

        sum += history->values[i];
    }
    float mean = sum / (float)history->count;

    /* Compute variance */
    float var_sum = 0.0f;
    for (size_t i = 0; i < history->count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && history->count > 256) {
            pr_predictive_bridge_heartbeat("pr_predictiv_loop",
                             (float)(i + 1) / (float)history->count);
        }

        float diff = history->values[i] - mean;
        var_sum += diff * diff;
    }

    return var_sum / (float)(history->count - 1);
}

static void add_to_pe_history(pe_history_t* history, float value) {
    if (!history) {
        return;
    }

    history->values[history->idx] = value;
    history->idx = (history->idx + 1) % PR_PRED_PE_HISTORY_SIZE;
    if (history->count < PR_PRED_PE_HISTORY_SIZE) {
        history->count++;
    }
}

static float clamp_precision(float precision) {
    if (precision < PR_PRED_MIN_PRECISION) {
        return PR_PRED_MIN_PRECISION;
    }
    if (precision > PR_PRED_MAX_PRECISION) {
        return PR_PRED_MAX_PRECISION;
    }
    return precision;
}

static float softmax_select(const float* values, size_t count, float temperature) {
    if (!values || count == 0) {
        return 0.0f;
    }

    /* Find max for numerical stability */
    float max_val = values[0];
    for (size_t i = 1; i < count; i++) {
        if (values[i] > max_val) {
            max_val = values[i];
        }
    }

    /* Compute softmax probabilities */
    float sum_exp = 0.0f;
    float probs[PR_PRED_MAX_PENDING_ACTIONS];

    for (size_t i = 0; i < count && i < PR_PRED_MAX_PENDING_ACTIONS; i++) {
        probs[i] = expf((values[i] - max_val) / temperature);
        sum_exp += probs[i];
    }

    /* Normalize */
    for (size_t i = 0; i < count && i < PR_PRED_MAX_PENDING_ACTIONS; i++) {
        probs[i] /= sum_exp;
    }

    /* Select based on probabilities (simple max for now) */
    float max_prob = probs[0];
    size_t max_idx = 0;
    for (size_t i = 1; i < count && i < PR_PRED_MAX_PENDING_ACTIONS; i++) {
        if (probs[i] > max_prob) {
            max_prob = probs[i];
            max_idx = i;
        }
    }

    return (float)max_idx;
}

static pr_mem_update_mode_t determine_update_mode(
    const pr_predictive_bridge_t* bridge,
    float pe,
    float max_resonance
) {
    if (!bridge) {
        return PR_MEM_UPDATE_NONE;
    }

    float pe_update = bridge->config.pe_threshold_update;
    float pe_new = bridge->config.pe_threshold_new;
    float resonance_threshold = 0.5f;

    if (pe < pe_update) {
        /* Low PE */
        if (max_resonance > resonance_threshold) {
            /* Low PE + high resonance = consolidate */
            return PR_MEM_UPDATE_CONSOLIDATE;
        }
        /* Low PE + low resonance = nothing */
        return PR_MEM_UPDATE_NONE;
    } else if (pe < pe_new) {
        /* Medium-high PE */
        if (max_resonance > resonance_threshold) {
            /* High PE + high resonance = reconsolidate */
            return PR_MEM_UPDATE_RECONSOLIDATE;
        }
        /* High PE + low resonance at this level = nothing yet */
        return PR_MEM_UPDATE_NONE;
    } else {
        /* Very high PE */
        if (max_resonance < resonance_threshold) {
            /* Very high PE + low resonance = new memory */
            return PR_MEM_UPDATE_ENCODE_NEW;
        }
        /* Very high PE + high resonance = reconsolidate */
        return PR_MEM_UPDATE_RECONSOLIDATE;
    }
}

static pr_pred_error_t process_reconsolidation_windows(
    pr_predictive_bridge_t* bridge,
    uint64_t current_time_ms
) {
    if (!bridge) {
        return PR_PRED_ERROR_NULL_POINTER;
    }

    for (size_t i = 0; i < bridge->max_reconsolidation_windows; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->max_reconsolidation_windows > 256) {
            pr_predictive_bridge_heartbeat("pr_predictiv_loop",
                             (float)(i + 1) / (float)bridge->max_reconsolidation_windows);
        }

        pr_reconsolidation_window_t* window = &bridge->reconsolidation_windows[i];

        if (!window->active) {
            continue;
        }

        /* Check if window has expired */
        uint64_t elapsed = current_time_ms - window->window_start_ms;
        if (elapsed >= window->window_duration_ms) {
            /* Close window and apply accumulated updates */
            pr_predictive_bridge_close_reconsolidation(bridge, window->node_id);
        } else {
            /* Window still open - accumulate current PE */
            window->accumulated_pe += bridge->pe_state.combined_pe;
        }
    }

    return PR_PRED_SUCCESS;
}

//=============================================================================
// Instance Health Agent Setter (B25 Upgrade)
//=============================================================================

void pr_predictive_bridge_set_instance_health_agent(
    pr_predictive_bridge_t* bridge, nimcp_health_agent_t* agent)
{
    if (bridge) {
        bridge->health_agent = agent;
    }
}

//=============================================================================
// Training Hook Stubs (B25 Upgrade)
//=============================================================================

int pr_predictive_bridge_training_begin(pr_predictive_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "pr_predictive_bridge_training_begin: NULL argument");
        return -1;
    }
    pr_predictive_bridge_heartbeat_instance(bridge->health_agent, "pr_predictive_bridge_training_begin", 0.0f);
    return 0;
}

int pr_predictive_bridge_training_end(pr_predictive_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "pr_predictive_bridge_training_end: NULL argument");
        return -1;
    }
    pr_predictive_bridge_heartbeat_instance(bridge->health_agent, "pr_predictive_bridge_training_end", 1.0f);
    return 0;
}

int pr_predictive_bridge_training_step(pr_predictive_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "pr_predictive_bridge_training_step: NULL argument");
        return -1;
    }
    pr_predictive_bridge_heartbeat_instance(bridge->health_agent, "pr_predictive_bridge_training_step", progress);
    return 0;
}
