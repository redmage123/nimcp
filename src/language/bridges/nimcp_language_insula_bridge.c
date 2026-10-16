#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_language_insula_bridge.c - Language-Insula Articulatory Bridge
//=============================================================================

#include "utils/bridge/nimcp_bridge_base.h"
#include "language/bridges/nimcp_language_insula_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(language_insula_bridge)

#define LOG_MODULE "LANGUAGE_INSULA_BRIDGE"


//=============================================================================
// Internal Structure
//=============================================================================

struct language_insula_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    language_orchestrator_t* language;
    insula_adapter_t* insula;
    language_insula_config_t config;

    /* State */
    li_insula_state_t state;
    uint64_t last_update_ms;

    /* Current prosody state */
    prosody_state_t prosody;

    /* Current articulation plan */
    articulation_plan_t current_plan;
    bool has_active_plan;

    /* Statistics */
    language_insula_stats_t stats;
    float total_quality;
    uint32_t quality_samples;
};

//=============================================================================
// Prosody Parameters
//=============================================================================

static const struct {
    float pitch_mod;
    float tempo_mod;
} prosody_params[PROSODY_COUNT] = {
    [PROSODY_NEUTRAL]   = { 0.0f,  0.0f },
    [PROSODY_HAPPY]     = { 0.2f,  0.1f },
    [PROSODY_SAD]       = {-0.2f, -0.2f },
    [PROSODY_ANGRY]     = { 0.3f,  0.2f },
    [PROSODY_FEARFUL]   = { 0.1f,  0.3f },
    [PROSODY_SURPRISED] = { 0.4f,  0.0f },
    [PROSODY_DISGUSTED] = {-0.1f, -0.1f }
};

//=============================================================================
// Public API
//=============================================================================

language_insula_config_t language_insula_default_config(void) {
    language_insula_config_t config = {
        .update_interval_ms = 50,
        .enable_articulatory_planning = true,
        .enable_emotional_prosody = true,
        .enable_interoceptive_feedback = true,
        .enable_bio_async = false
    };
    return config;
}

language_insula_bridge_t* language_insula_bridge_create(
    language_orchestrator_t* language,
    insula_adapter_t* insula,
    const language_insula_config_t* config)
{
    if (!language || !insula) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_insula_bridge_create: required parameter is NULL (language, insula)");
        return NULL;
    }

    language_insula_bridge_t* bridge = nimcp_calloc(1, sizeof(language_insula_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;
    }

    bridge->language = language;
    bridge->insula = insula;
    bridge->config = config ? *config : language_insula_default_config();
    bridge->state = LI_STATE_IDLE;
    bridge->last_update_ms = nimcp_time_now_us() / 1000;

    /* Initialize prosody to neutral */
    bridge->prosody.current = PROSODY_NEUTRAL;
    bridge->prosody.intensity = 0.5f;
    bridge->prosody.pitch_modulation = 0.0f;
    bridge->prosody.tempo_modulation = 0.0f;

    bridge->has_active_plan = false;
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    bridge->total_quality = 0.0f;
    bridge->quality_samples = 0;

    return bridge;
}

void language_insula_bridge_destroy(language_insula_bridge_t* bridge) {
    if (bridge) {
        nimcp_free(bridge);
    }
}

int language_insula_bridge_update(language_insula_bridge_t* bridge, uint64_t timestamp_ms) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    uint64_t elapsed = timestamp_ms - bridge->last_update_ms;
    if (elapsed < bridge->config.update_interval_ms) {
        return 0;
    }

    bridge->last_update_ms = timestamp_ms;
    bridge->stats.state = bridge->state;

    /* Update average quality metric */
    if (bridge->quality_samples > 0) {
        bridge->stats.avg_articulation_quality =
            bridge->total_quality / (float)bridge->quality_samples;
    }

    return 0;
}

int language_insula_create_plan(
    language_insula_bridge_t* bridge,
    const char* phonemes,
    articulation_style_t style,
    articulation_plan_t* plan)
{
    if (!bridge || !phonemes || !plan) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_insula_create_plan: required parameter is NULL (bridge, phonemes, plan)");
        return -1;
    }

    if (!bridge->config.enable_articulatory_planning) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_insula_create_plan: bridge->config is NULL");
        return -1;
    }

    bridge->state = LI_STATE_PLANNING;

    memset(plan, 0, sizeof(*plan));
    strncpy(plan->phoneme_sequence, phonemes, sizeof(plan->phoneme_sequence) - 1);
    plan->phoneme_count = strlen(phonemes);
    plan->style = style;
    plan->prosody = bridge->prosody.current;

    /* Set tempo based on articulation style */
    switch (style) {
        case ARTICULATION_SLOW:
            plan->tempo = 0.7f;
            plan->intensity = 0.8f;
            break;
        case ARTICULATION_FAST:
            plan->tempo = 1.5f;
            plan->intensity = 0.9f;
            break;
        case ARTICULATION_EMPHATIC:
            plan->tempo = 0.9f;
            plan->intensity = 1.0f;
            break;
        case ARTICULATION_WHISPERED:
            plan->tempo = 0.8f;
            plan->intensity = 0.3f;
            break;
        default:
            plan->tempo = 1.0f;
            plan->intensity = 0.7f;
            break;
    }

    /* Apply prosody modulation to tempo */
    plan->tempo *= (1.0f + bridge->prosody.tempo_modulation * bridge->prosody.intensity);

    bridge->stats.plans_created++;
    bridge->state = LI_STATE_IDLE;

    return 0;
}

int language_insula_execute_plan(
    language_insula_bridge_t* bridge,
    const articulation_plan_t* plan)
{
    if (!bridge || !plan) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_insula_execute_plan: required parameter is NULL (bridge, plan)");
        return -1;
    }

    bridge->state = LI_STATE_ARTICULATING;
    bridge->current_plan = *plan;
    bridge->has_active_plan = true;

    /* Simulate articulation quality based on plan parameters */
    float quality = 0.8f;

    /* Fast speech reduces quality slightly */
    if (plan->tempo > 1.3f) {
        quality -= 0.1f;
    }

    /* High intensity improves clarity */
    quality += plan->intensity * 0.1f;

    /* Clamp quality */
    if (quality > 1.0f) quality = 1.0f;
    if (quality < 0.0f) quality = 0.0f;

    bridge->total_quality += quality;
    bridge->quality_samples++;

    bridge->has_active_plan = false;
    bridge->state = LI_STATE_IDLE;

    return 0;
}

int language_insula_set_prosody(
    language_insula_bridge_t* bridge,
    emotional_prosody_t prosody,
    float intensity)
{
    if (!bridge || prosody >= PROSODY_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "language_insula_set_prosody: bridge is NULL");
        return -1;
    }

    if (!bridge->config.enable_emotional_prosody) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_insula_set_prosody: bridge->config is NULL");
        return -1;
    }

    bridge->state = LI_STATE_PROSODY_ACTIVE;

    bridge->prosody.current = prosody;
    bridge->prosody.intensity = (intensity < 0.0f) ? 0.0f :
                                (intensity > 1.0f) ? 1.0f : intensity;
    bridge->prosody.pitch_modulation = prosody_params[prosody].pitch_mod;
    bridge->prosody.tempo_modulation = prosody_params[prosody].tempo_mod;

    bridge->stats.prosody_changes++;
    bridge->state = LI_STATE_IDLE;

    return 0;
}

int language_insula_get_prosody(
    const language_insula_bridge_t* bridge,
    prosody_state_t* state)
{
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_insula_get_prosody: required parameter is NULL (bridge, state)");
        return -1;
    }

    *state = bridge->prosody;
    return 0;
}

int language_insula_modulate_speech(
    language_insula_bridge_t* bridge,
    float* pitch,
    float* tempo)
{
    if (!bridge || !pitch || !tempo) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_insula_modulate_speech: required parameter is NULL (bridge, pitch, tempo)");
        return -1;
    }

    /* Apply prosody modulation to pitch and tempo */
    *pitch = bridge->prosody.pitch_modulation * bridge->prosody.intensity;
    *tempo = bridge->prosody.tempo_modulation * bridge->prosody.intensity;

    return 0;
}

int language_insula_get_stats(
    const language_insula_bridge_t* bridge,
    language_insula_stats_t* stats)
{
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_insula_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    *stats = bridge->stats;
    return 0;
}

li_insula_state_t language_insula_get_state(const language_insula_bridge_t* bridge) {
    if (!bridge) {
        return LI_STATE_ERROR;
    }
    return bridge->state;
}
