/**
 * @file nimcp_basal_ganglia_amygdala_bridge.c
 * @brief Basal ganglia-amygdala emotional modulation bridge implementation
 */

#include "core/brain/subcortical/nimcp_basal_ganglia_amygdala_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/math/nimcp_math_helpers.h"
#include <string.h>
#include <math.h>

//=============================================================================
#include <stddef.h>
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(basal_ganglia_amygdala_bridge, MESH_ADAPTER_CATEGORY_SUBCORTICAL)


#define LOG_MODULE "BASAL_GANGLIA_AMYGDALA_BRIDGE"


/* ============================================================================
 * Static Helpers
 * ============================================================================ */

static bga_influence_type_t determine_influence(float fear, float anxiety,
                                                 amyg_threat_level_t threat) {
    if (fear >= BGA_FREEZE_THRESHOLD) {
        return BGA_INFLUENCE_FREEZE;
    }
    if (threat >= AMYG_THREAT_HIGH) {
        return BGA_INFLUENCE_FLIGHT;
    }
    if (threat >= AMYG_THREAT_MODERATE || fear > 0.3f) {
        return BGA_INFLUENCE_AVOIDANCE;
    }
    if (anxiety > 0.5f) {
        return BGA_INFLUENCE_AVOIDANCE;
    }
    return BGA_INFLUENCE_NONE;
}

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

void bga_bridge_default_config(bga_bridge_config_t* config) {
    if (!config) return;

    config->fear_weight = BGA_DEFAULT_FEAR_WEIGHT;
    config->anxiety_weight = BGA_DEFAULT_ANXIETY_WEIGHT;
    config->threat_stn_gain = 0.5f;
    config->avoidance_bias = 0.3f;
    config->freeze_threshold = BGA_FREEZE_THRESHOLD;
    config->habituation_rate = 0.01f;
    config->enable_freeze_response = true;
    config->enable_flight_bias = true;
    config->enable_feedback = true;
}

bga_bridge_t* bga_bridge_create(const bga_bridge_config_t* config) {
    bga_bridge_t* bridge = nimcp_calloc(1, sizeof(bga_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bga_bridge_create: failed to allocate bridge");
        return NULL;
    }

    /* Apply config */
    if (config) {
        bridge->config = *config;
    } else {
        bga_bridge_default_config(&bridge->config);
    }

    /* Allocate modulations array - start with default size */
    bridge->num_actions = 32;
    bridge->modulations = nimcp_calloc(bridge->num_actions,
                                        sizeof(bga_action_modulation_t));
    if (!bridge->modulations) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bga_bridge_create: failed to allocate modulations");
        nimcp_free(bridge);
        return NULL;
    }

    /* Initialize modulations */
    for (uint32_t i = 0; i < bridge->num_actions; i++) {
        bridge->modulations[i].action_id = i;
        bridge->modulations[i].tag = BGA_TAG_NEUTRAL;
        bridge->modulations[i].fear_bias = 0.0f;
        bridge->modulations[i].value_modulation = 1.0f;
        bridge->modulations[i].is_active = false;
    }

    /* Initialize state */
    bridge->state.fear_level = 0.0f;
    bridge->state.anxiety_level = 0.0f;
    bridge->state.threat = AMYG_THREAT_NONE;
    bridge->state.valence = AMYG_VALENCE_NEUTRAL;
    bridge->state.influence = BGA_INFLUENCE_NONE;

    bridge->num_threat_actions = 0;
    bridge->num_safe_actions = 0;
    bridge->stn_boost = 0.0f;

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "basal_ganglia_amygdala") != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "bga_bridge_create: failed to initialize bridge base");
        nimcp_free(bridge->modulations);
        nimcp_free(bridge);
        return NULL;
    }

    NIMCP_LOGGING_INFO("Created %s bridge", "basal_ganglia_amygdala");
    return bridge;
}

void bga_bridge_destroy(bga_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "basal_ganglia_amygdala");

    if (bridge->base.mutex) bridge_base_cleanup(&bridge->base);
    if (bridge->modulations) nimcp_free(bridge->modulations);

    nimcp_free(bridge);
}

int bga_bridge_reset(bga_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bga_bridge_reset: bridge is NULL");
        return -1;
    }

    bridge->state.fear_level = 0.0f;
    bridge->state.anxiety_level = 0.0f;
    bridge->state.threat = AMYG_THREAT_NONE;
    bridge->state.valence = AMYG_VALENCE_NEUTRAL;
    bridge->state.influence = BGA_INFLUENCE_NONE;

    bridge->stn_boost = 0.0f;
    bridge->num_threat_actions = 0;
    bridge->num_safe_actions = 0;

    /* Reset modulations */
    for (uint32_t i = 0; i < bridge->num_actions; i++) {
        bridge->modulations[i].tag = BGA_TAG_NEUTRAL;
        bridge->modulations[i].fear_bias = 0.0f;
        bridge->modulations[i].value_modulation = 1.0f;
        bridge->modulations[i].is_active = false;
    }

    memset(&bridge->stats, 0, sizeof(bga_bridge_stats_t));

    return 0;
}

/* ============================================================================
 * Connection Functions
 * ============================================================================ */

int bga_bridge_connect_bg(bga_bridge_t* bridge, basal_ganglia_t* bg) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bga_bridge_connect_bg: bridge is NULL");
        return -1;
    }
    bridge->bg = bg;

    /* Update num_actions if BG has different count */
    if (bg && bg->num_actions > bridge->num_actions) {
        bga_action_modulation_t* new_mods = nimcp_realloc(
            bridge->modulations,
            bg->num_actions * sizeof(bga_action_modulation_t)
        );
        if (new_mods) {
            bridge->modulations = new_mods;
            for (uint32_t i = bridge->num_actions; i < bg->num_actions; i++) {
                bridge->modulations[i].action_id = i;
                bridge->modulations[i].tag = BGA_TAG_NEUTRAL;
                bridge->modulations[i].fear_bias = 0.0f;
                bridge->modulations[i].value_modulation = 1.0f;
                bridge->modulations[i].is_active = false;
            }
            bridge->num_actions = bg->num_actions;
        }
    }

    return 0;
}

int bga_bridge_connect_amygdala(bga_bridge_t* bridge, amygdala_t* amygdala) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bga_bridge_connect_amygdala: bridge is NULL");
        return -1;
    }
    bridge->amygdala = amygdala;
    return 0;
}

bool bga_bridge_is_connected(const bga_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->bg != NULL && bridge->amygdala != NULL;
}

/* ============================================================================
 * Action Tagging Functions
 * ============================================================================ */

int bga_bridge_tag_threat_action(bga_bridge_t* bridge, uint32_t action_id) {
    if (!bridge || action_id >= bridge->num_actions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bga_bridge_tag_threat_action: bridge is NULL");
        return -1;
    }

    bridge->modulations[action_id].tag = BGA_TAG_THREATENING;
    bridge->modulations[action_id].is_active = true;

    /* Add to threat actions list */
    if (bridge->num_threat_actions < BGA_MAX_THREAT_ACTIONS) {
        bridge->threat_actions[bridge->num_threat_actions++] = action_id;
    }

    return 0;
}

int bga_bridge_tag_safe_action(bga_bridge_t* bridge, uint32_t action_id) {
    if (!bridge || action_id >= bridge->num_actions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bga_bridge_tag_safe_action: bridge is NULL");
        return -1;
    }

    bridge->modulations[action_id].tag = BGA_TAG_SAFE;
    bridge->modulations[action_id].is_active = true;

    /* Add to safe actions list */
    if (bridge->num_safe_actions < BGA_MAX_SAFE_ACTIONS) {
        bridge->safe_actions[bridge->num_safe_actions++] = action_id;
    }

    return 0;
}

int bga_bridge_tag_escape_action(bga_bridge_t* bridge, uint32_t action_id) {
    if (!bridge || action_id >= bridge->num_actions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bga_bridge_tag_escape_action: bridge is NULL");
        return -1;
    }

    bridge->modulations[action_id].tag = BGA_TAG_ESCAPE;
    bridge->modulations[action_id].is_active = true;

    return 0;
}

bga_action_tag_t bga_bridge_get_action_tag(
    const bga_bridge_t* bridge,
    uint32_t action_id
) {
    if (!bridge || action_id >= bridge->num_actions) return BGA_TAG_NEUTRAL;
    return bridge->modulations[action_id].tag;
}

/* ============================================================================
 * Modulation Functions
 * ============================================================================ */

int bga_bridge_update_modulation(bga_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bga_bridge_update_modulation: bridge is NULL");
        return -1;
    }

    /* Read amygdala state if connected */
    if (bridge->amygdala) {
        bridge->state.fear_level = amygdala_get_fear_level(bridge->amygdala);
        bridge->state.anxiety_level = amygdala_get_anxiety_level(bridge->amygdala);
        bridge->state.threat = amygdala_get_threat_level(bridge->amygdala);
    }

    /* Determine influence type */
    bridge->state.influence = determine_influence(
        bridge->state.fear_level,
        bridge->state.anxiety_level,
        bridge->state.threat
    );

    /* Compute STN boost from anxiety */
    bridge->stn_boost = bridge->state.anxiety_level * bridge->config.threat_stn_gain;

    /* Update per-action modulations */
    for (uint32_t i = 0; i < bridge->num_actions; i++) {
        bga_action_modulation_t* mod = &bridge->modulations[i];

        switch (mod->tag) {
            case BGA_TAG_THREATENING:
                /* Threatening actions get suppressed by fear */
                mod->fear_bias = -bridge->state.fear_level * bridge->config.fear_weight;
                mod->value_modulation = 1.0f - bridge->state.fear_level * 0.5f;
                break;

            case BGA_TAG_SAFE:
                /* Safe actions get boosted when anxious */
                mod->fear_bias = bridge->state.anxiety_level * 0.2f;
                mod->value_modulation = 1.0f + bridge->state.anxiety_level * 0.3f;
                break;

            case BGA_TAG_ESCAPE:
                /* Escape actions get strong boost during threat */
                if (bridge->state.influence == BGA_INFLUENCE_FLIGHT) {
                    mod->fear_bias = bridge->state.fear_level * bridge->config.avoidance_bias;
                    mod->value_modulation = 1.0f + bridge->state.fear_level * 0.5f;
                } else {
                    mod->fear_bias = 0.0f;
                    mod->value_modulation = 1.0f;
                }
                break;

            case BGA_TAG_DEFENSIVE:
                /* Defensive actions boosted at high threat */
                if (bridge->state.threat >= AMYG_THREAT_HIGH) {
                    mod->fear_bias = 0.3f;
                    mod->value_modulation = 1.2f;
                }
                break;

            default:
                /* Neutral actions: slight avoidance bias when fearful */
                mod->fear_bias = -bridge->state.fear_level * 0.1f;
                mod->value_modulation = 1.0f;
                break;
        }
    }

    bridge->stats.total_modulations++;

    return 0;
}

int bga_bridge_apply_modulation(
    bga_bridge_t* bridge,
    float* action_values,
    uint32_t num_actions
) {
    if (!bridge || !action_values) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bga_bridge_apply_modulation: required parameter is NULL (bridge, action_values)");
        return -1;
    }

    uint32_t n = num_actions < bridge->num_actions ? num_actions : bridge->num_actions;

    /* Check for freeze response */
    if (bridge->config.enable_freeze_response &&
        bridge->state.influence == BGA_INFLUENCE_FREEZE) {
        for (uint32_t i = 0; i < n; i++) {
            action_values[i] *= 0.1f;  /* Severe suppression */
        }
        bridge->stats.freeze_events++;
        return 0;
    }

    /* Apply per-action modulation */
    for (uint32_t i = 0; i < n; i++) {
        bga_action_modulation_t* mod = &bridge->modulations[i];

        /* Apply value modulation */
        action_values[i] *= mod->value_modulation;

        /* Apply fear bias */
        action_values[i] += mod->fear_bias;

        /* Clamp to valid range */
        action_values[i] = nimcp_clampf(action_values[i], 0.0f, 1.0f);
    }

    /* Update statistics */
    if (bridge->state.fear_level > 0.3f) {
        bridge->stats.fear_triggered++;
    }
    if (bridge->state.influence == BGA_INFLUENCE_AVOIDANCE) {
        bridge->stats.avoidance_biases++;
    }
    bridge->stats.avg_fear_influence =
        (bridge->stats.avg_fear_influence * (bridge->stats.total_modulations - 1) +
         bridge->state.fear_level * bridge->config.fear_weight) /
        bridge->stats.total_modulations;
    bridge->stats.avg_threat_level =
        (bridge->stats.avg_threat_level * (bridge->stats.total_modulations - 1) +
         (float)bridge->state.threat / 4.0f) / bridge->stats.total_modulations;

    return 0;
}

float bga_bridge_get_action_modulation(
    const bga_bridge_t* bridge,
    uint32_t action_id
) {
    if (!bridge || action_id >= bridge->num_actions) return 1.0f;
    return bridge->modulations[action_id].value_modulation;
}

bool bga_bridge_is_frozen(const bga_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->state.influence == BGA_INFLUENCE_FREEZE;
}

float bga_bridge_get_stn_boost(const bga_bridge_t* bridge) {
    if (!bridge) return 0.0f;
    return bridge->stn_boost;
}

/* ============================================================================
 * Feedback Functions
 * ============================================================================ */

int bga_bridge_send_outcome(
    bga_bridge_t* bridge,
    uint32_t action_id,
    float outcome,
    bool was_threat
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bga_bridge_send_outcome: bridge is NULL");
        return -1;
    }
    if (!bridge->config.enable_feedback) return 0;
    if (!bridge->amygdala) return 0;

    /* Update amygdala based on outcome */
    if (was_threat) {
        if (outcome > 0.0f) {
            /* Successful escape/avoidance → reduce threat */
            float current_fear = amygdala_get_fear_level(bridge->amygdala);
            amygdala_set_fear_level(bridge->amygdala,
                                    current_fear * (1.0f - outcome * bridge->config.habituation_rate));
        } else {
            /* Failed avoidance → increase fear */
            float current_fear = amygdala_get_fear_level(bridge->amygdala);
            amygdala_set_fear_level(bridge->amygdala,
                                    nimcp_clampf(current_fear - outcome * 0.1f, 0.0f, 1.0f));
        }
    }

    bridge->stats.feedback_signals++;

    return 0;
}

/* ============================================================================
 * State Query Functions
 * ============================================================================ */

int bga_bridge_get_state(
    const bga_bridge_t* bridge,
    bga_emotional_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bga_bridge_get_state: required parameter is NULL (bridge, state)");
        return -1;
    }
    *state = bridge->state;
    return 0;
}

bga_influence_type_t bga_bridge_get_influence(const bga_bridge_t* bridge) {
    if (!bridge) return BGA_INFLUENCE_NONE;
    return bridge->state.influence;
}

/* ============================================================================
 * Statistics Functions
 * ============================================================================ */

int bga_bridge_get_stats(
    const bga_bridge_t* bridge,
    bga_bridge_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bga_bridge_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    *stats = bridge->stats;
    return 0;
}

void bga_bridge_reset_stats(bga_bridge_t* bridge) {
    if (!bridge) return;
    memset(&bridge->stats, 0, sizeof(bga_bridge_stats_t));
}

const char* bga_influence_type_name(bga_influence_type_t type) {
    switch (type) {
        case BGA_INFLUENCE_NONE: return "none";
        case BGA_INFLUENCE_AVOIDANCE: return "avoidance";
        case BGA_INFLUENCE_APPROACH: return "approach";
        case BGA_INFLUENCE_FREEZE: return "freeze";
        case BGA_INFLUENCE_FLIGHT: return "flight";
        case BGA_INFLUENCE_FIGHT: return "fight";
        default: return "unknown";
    }
}

const char* bga_action_tag_name(bga_action_tag_t tag) {
    switch (tag) {
        case BGA_TAG_NEUTRAL: return "neutral";
        case BGA_TAG_THREATENING: return "threatening";
        case BGA_TAG_SAFE: return "safe";
        case BGA_TAG_ESCAPE: return "escape";
        case BGA_TAG_DEFENSIVE: return "defensive";
        default: return "unknown";
    }
}
