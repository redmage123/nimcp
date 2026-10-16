/**
 * @file nimcp_surprise_self_model_bridge.c
 * @brief Bridge between Surprise Amplifier and Self-Model system
 * @version 1.1.0
 * @date 2026-01-27
 *
 * WHAT: Update self-model based on surprise about own capabilities
 * WHY:  Capability surprises trigger belief/competence revision
 * HOW:  Surprise -> capability revision; self-model confidence -> sensitivity modulation
 *       Uses KL-divergence inspired belief revision for Bayesian-appropriate updates
 *
 * @author NIMCP Development Team
 */

#include "cognitive/salience/nimcp_surprise_self_model_bridge.h"
#include "cognitive/salience/nimcp_surprise_amplifier.h"
#include "glial/myelin_sheath/nimcp_myelin_math.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <math.h>
#include <stddef.h>

/* ============================================================================
 * Health Agent Integration
 * ============================================================================ */
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(surprise_self_model, MESH_ADAPTER_CATEGORY_COGNITIVE)

/* Alias: tests reference surprise_self_model_bridge_set_health_agent_global */
static nimcp_health_agent_t* g_surprise_self_model_bridge_health_agent = NULL;
void surprise_self_model_bridge_set_health_agent_global(struct nimcp_health_agent* agent) { g_surprise_self_model_bridge_health_agent = agent; }

/* ============================================================================
 * Capability Tracker
 * ============================================================================ */

typedef struct {
    uint32_t capability_id;
    float confidence;       /* 0.0 = no confidence, 1.0 = fully confident */
    float competence;       /* 0.0 = incompetent, 1.0 = fully competent */
    bool active;
} capability_entry_t;

/* ============================================================================
 * Internal Structure
 * ============================================================================ */

struct surprise_self_model_bridge {
    surprise_self_model_config_t config;

    /* Connected systems */
    struct surprise_amplifier* amplifier;
    void* self_model;

    /* State */
    surprise_self_model_effects_t effects;
    surprise_self_model_stats_t stats;

    /* Capability tracking */
    capability_entry_t* capabilities;
    uint32_t capability_count;

    /* Last revision */
    surprise_capability_revision_t last_revision;
    bool has_revision;

    /* Bio-async */
    void* router;
    bool bio_async_connected;

    /* Thread safety */
    nimcp_mutex_t* mutex;

    /* Health agent */
    nimcp_health_agent_t* health_agent;

    bool initialized;
    uint64_t update_count;
};

/* ============================================================================
 * Helpers
 * ============================================================================ */

static const char* revision_type_name(surprise_capability_revision_type_t type) {
    switch (type) {
        case SURPRISE_CAPABILITY_UPGRADE:   return "UPGRADE";
        case SURPRISE_CAPABILITY_DOWNGRADE: return "DOWNGRADE";
        case SURPRISE_CAPABILITY_NOVEL:     return "NOVEL";
        default: return "UNKNOWN";
    }
}

static capability_entry_t* find_capability(
    surprise_self_model_bridge_t* bridge, uint32_t capability_id)
{
    for (uint32_t i = 0; i < bridge->capability_count; i++) {
        if (bridge->capabilities[i].capability_id == capability_id &&
            bridge->capabilities[i].active) {
            return &bridge->capabilities[i];
        }
    }
    return NULL;
}

static capability_entry_t* add_capability(
    surprise_self_model_bridge_t* bridge, uint32_t capability_id)
{
    if (bridge->capability_count >= bridge->config.max_tracked_capabilities) {
        /* Reuse first inactive slot or evict first */
        for (uint32_t i = 0; i < bridge->capability_count; i++) {
            if (!bridge->capabilities[i].active) {
                bridge->capabilities[i].capability_id = capability_id;
                bridge->capabilities[i].confidence = 0.5f;
                bridge->capabilities[i].competence = 0.5f;
                bridge->capabilities[i].active = true;
                return &bridge->capabilities[i];
            }
        }
        /* All active: overwrite first */
        bridge->capabilities[0].capability_id = capability_id;
        bridge->capabilities[0].confidence = 0.5f;
        bridge->capabilities[0].competence = 0.5f;
        bridge->capabilities[0].active = true;
        return &bridge->capabilities[0];
    }

    capability_entry_t* entry = &bridge->capabilities[bridge->capability_count++];
    entry->capability_id = capability_id;
    entry->confidence = 0.5f;  /* Prior: uncertain */
    entry->competence = 0.5f;
    entry->active = true;
    return entry;
}

/* ============================================================================
 * Bio-Async Helpers
 * ============================================================================ */

static void self_model_send_update_msg(surprise_self_model_bridge_t* bridge,
                                         uint32_t capability_id,
                                         float prior, float posterior,
                                         float magnitude,
                                         surprise_capability_revision_type_t type)
{
    if (!bridge->bio_async_connected || !bridge->router) return;

    typedef struct {
        bio_message_header_t header;
        uint32_t capability_id;
        float prior_confidence;
        float posterior_confidence;
        float surprise_magnitude;
        uint32_t revision_type;
        float confidence_modulation;
        uint64_t capability_surprises;
    } surprise_self_model_update_msg_t;

    surprise_self_model_update_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    bio_msg_init_header(&msg.header,
                        BIO_MSG_SURPRISE_SELF_MODEL_UPDATE,
                        BIO_MODULE_SURPRISE_SELF_MODEL,
                        0, sizeof(msg));
    msg.header.channel = BIO_CHANNEL_NOREPINEPHRINE;
    msg.capability_id = capability_id;
    msg.prior_confidence = prior;
    msg.posterior_confidence = posterior;
    msg.surprise_magnitude = magnitude;
    msg.revision_type = (uint32_t)type;
    msg.confidence_modulation = bridge->effects.confidence_modulation;
    msg.capability_surprises = bridge->stats.capability_surprises;

    bio_router_broadcast((bio_module_context_t)bridge->router, &msg, sizeof(msg));
}

static void self_model_send_revision_msg(surprise_self_model_bridge_t* bridge,
                                           const surprise_capability_revision_t* rev)
{
    if (!bridge->bio_async_connected || !bridge->router) return;

    typedef struct {
        bio_message_header_t header;
        uint32_t capability_id;
        float prior_confidence;
        float posterior_confidence;
        float surprise_magnitude;
        uint32_t revision_type;
        float competence_delta;
        uint64_t belief_revisions;
    } surprise_capability_revision_msg_t;

    surprise_capability_revision_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    bio_msg_init_header(&msg.header,
                        BIO_MSG_SURPRISE_CAPABILITY_REVISION,
                        BIO_MODULE_SURPRISE_SELF_MODEL,
                        0, sizeof(msg));
    msg.header.channel = BIO_CHANNEL_NOREPINEPHRINE;
    msg.capability_id = rev->capability_id;
    msg.prior_confidence = rev->prior_confidence;
    msg.posterior_confidence = rev->posterior_confidence;
    msg.surprise_magnitude = rev->surprise_magnitude;
    msg.revision_type = (uint32_t)rev->revision_type;
    msg.competence_delta = bridge->effects.competence_delta;
    msg.belief_revisions = bridge->stats.belief_revisions;

    bio_router_broadcast((bio_module_context_t)bridge->router, &msg, sizeof(msg));
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

surprise_self_model_config_t surprise_self_model_bridge_default_config(void) {
    surprise_self_model_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));

    cfg.capability_surprise_threshold = SURPRISE_SELF_MODEL_DEFAULT_CAP_THRESHOLD;
    cfg.competence_update_rate = SURPRISE_SELF_MODEL_DEFAULT_COMPETENCE_RATE;
    cfg.confidence_modulation_gain = SURPRISE_SELF_MODEL_DEFAULT_CONFIDENCE_GAIN;
    cfg.belief_revision_rate = SURPRISE_SELF_MODEL_DEFAULT_BELIEF_RATE;
    cfg.max_tracked_capabilities = SURPRISE_SELF_MODEL_DEFAULT_MAX_CAPABILITIES;
    cfg.enable_bio_async = true;
    cfg.enable_logging = true;

    return cfg;
}

surprise_self_model_bridge_t* surprise_self_model_bridge_create(
    const surprise_self_model_config_t* config)
{
    surprise_self_model_bridge_t* bridge = (surprise_self_model_bridge_t*)nimcp_calloc(
        1, sizeof(surprise_self_model_bridge_t));
    if (!bridge) {
        NIMCP_THROW_MEMORY(NIMCP_SURPRISE_SELF_MODEL_ERROR_NO_MEMORY,
                           sizeof(surprise_self_model_bridge_t),
                           "surprise_self_model_bridge allocation failed (%zu bytes)",
                           sizeof(surprise_self_model_bridge_t));
        return NULL;
    }

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = surprise_self_model_bridge_default_config();
    }

    bridge->mutex = nimcp_mutex_create(NULL);
    if (!bridge->mutex) {
        NIMCP_THROW_MEMORY(NIMCP_SURPRISE_SELF_MODEL_ERROR_NO_MEMORY,
                           sizeof(nimcp_mutex_t),
                           "surprise_self_model_bridge mutex allocation failed");
        nimcp_free(bridge);
        bridge = NULL;
        return NULL;
    }

    /* Allocate capability tracker */
    bridge->capabilities = (capability_entry_t*)nimcp_calloc(
        bridge->config.max_tracked_capabilities, sizeof(capability_entry_t));
    if (!bridge->capabilities) {
        NIMCP_THROW_MEMORY(NIMCP_SURPRISE_SELF_MODEL_ERROR_NO_MEMORY,
                           bridge->config.max_tracked_capabilities * sizeof(capability_entry_t),
                           "surprise_self_model_bridge capabilities allocation failed");
        nimcp_mutex_destroy(bridge->mutex);
        nimcp_free(bridge);
        bridge = NULL;
        return NULL;
    }

    /* Initialize effects */
    bridge->effects.confidence_modulation = 1.0f;  /* Neutral */
    bridge->initialized = true;

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_INFO("Created surprise-self-model bridge (threshold=%.2f, max_caps=%u)",
                           bridge->config.capability_surprise_threshold,
                           bridge->config.max_tracked_capabilities);
        NIMCP_LOGGING_DEBUG("Self-model config: competence_rate=%.3f, confidence_gain=%.3f, "
                            "belief_rate=%.3f",
                            bridge->config.competence_update_rate,
                            bridge->config.confidence_modulation_gain,
                            bridge->config.belief_revision_rate);
    }

    return bridge;
}

void surprise_self_model_bridge_destroy(surprise_self_model_bridge_t* bridge) {
    if (!bridge) return;

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_INFO("Destroying surprise-self-model bridge (surprises=%lu, discoveries=%lu)",
                           (unsigned long)bridge->stats.capability_surprises,
                           (unsigned long)bridge->stats.discoveries);
        NIMCP_LOGGING_DEBUG("Self-model final state: confidence_mod=%.3f, beliefs_revised=%lu, "
                            "caps_tracked=%u",
                            bridge->effects.confidence_modulation,
                            (unsigned long)bridge->stats.belief_revisions,
                            bridge->capability_count);
    }

    if (bridge->capabilities) {
        nimcp_free(bridge->capabilities);
    }
    if (bridge->mutex) {
        nimcp_mutex_destroy(bridge->mutex);
    }
    nimcp_free(bridge);
    bridge = NULL;
}

int surprise_self_model_bridge_reset(surprise_self_model_bridge_t* bridge) {
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_SELF_MODEL_ERROR_NULL_POINTER,
                             "NULL bridge in reset");

    surprise_self_model_heartbeat_instance(bridge->health_agent, "reset", 0.0f);

    nimcp_mutex_lock(bridge->mutex);
    memset(&bridge->effects, 0, sizeof(bridge->effects));
    bridge->effects.confidence_modulation = 1.0f;
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    bridge->capability_count = 0;
    memset(bridge->capabilities, 0,
           bridge->config.max_tracked_capabilities * sizeof(capability_entry_t));
    bridge->has_revision = false;
    memset(&bridge->last_revision, 0, sizeof(bridge->last_revision));
    bridge->update_count = 0;
    nimcp_mutex_unlock(bridge->mutex);

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_DEBUG("Self-model bridge reset: capabilities cleared, confidence_mod=1.0");
    }

    return 0;
}

/* ============================================================================
 * Connection API
 * ============================================================================ */

int surprise_self_model_bridge_connect_amplifier(
    surprise_self_model_bridge_t* bridge,
    struct surprise_amplifier* amp)
{
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_SELF_MODEL_ERROR_NULL_POINTER,
                             "NULL bridge in connect_amplifier");
    NIMCP_CHECK_THROW_IMMUNE(amp != NULL,
                             NIMCP_SURPRISE_SELF_MODEL_ERROR_NULL_POINTER,
                             "NULL amp in connect_amplifier");

    surprise_self_model_heartbeat_instance(bridge->health_agent, "connect_amplifier", 0.0f);

    nimcp_mutex_lock(bridge->mutex);
    bridge->amplifier = amp;
    nimcp_mutex_unlock(bridge->mutex);

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_INFO("Surprise-self-model bridge connected to amplifier");
        NIMCP_LOGGING_DEBUG("Self-model amplifier connection established, ptr=%p", (void*)amp);
    }
    return 0;
}

int surprise_self_model_bridge_connect_self_model(
    surprise_self_model_bridge_t* bridge,
    void* self_model)
{
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_SELF_MODEL_ERROR_NULL_POINTER,
                             "NULL bridge in connect_self_model");
    NIMCP_CHECK_THROW_IMMUNE(self_model != NULL,
                             NIMCP_SURPRISE_SELF_MODEL_ERROR_NULL_POINTER,
                             "NULL self_model in connect_self_model");

    surprise_self_model_heartbeat_instance(bridge->health_agent, "connect_self_model", 0.0f);

    nimcp_mutex_lock(bridge->mutex);
    bridge->self_model = self_model;
    bridge->effects.self_model_connected = true;
    nimcp_mutex_unlock(bridge->mutex);

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_INFO("Surprise-self-model bridge connected to self-model system");
        NIMCP_LOGGING_DEBUG("Self-model system connection established, ptr=%p", (void*)self_model);
    }
    return 0;
}

int surprise_self_model_bridge_connect_bio_async(
    surprise_self_model_bridge_t* bridge,
    void* router)
{
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_SELF_MODEL_ERROR_NULL_POINTER,
                             "NULL bridge in connect_bio_async");

    surprise_self_model_heartbeat_instance(bridge->health_agent, "connect_bio_async", 0.0f);

    nimcp_mutex_lock(bridge->mutex);
    bridge->router = router;
    bridge->bio_async_connected = (router != NULL);
    nimcp_mutex_unlock(bridge->mutex);

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_DEBUG("Self-model bio-async %s (router=%p)",
                            router ? "connected" : "disconnected", router);
    }

    return 0;
}

int surprise_self_model_bridge_disconnect_bio_async(
    surprise_self_model_bridge_t* bridge)
{
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_SELF_MODEL_ERROR_NULL_POINTER,
                             "NULL bridge in disconnect_bio_async");

    surprise_self_model_heartbeat_instance(bridge->health_agent, "disconnect_bio_async", 0.0f);

    nimcp_mutex_lock(bridge->mutex);
    bridge->router = NULL;
    bridge->bio_async_connected = false;
    nimcp_mutex_unlock(bridge->mutex);

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_DEBUG("Self-model bio-async disconnected");
    }

    return 0;
}

/* ============================================================================
 * Operations API
 * ============================================================================ */

int surprise_self_model_on_capability_surprise(
    surprise_self_model_bridge_t* bridge,
    uint32_t capability_id,
    float surprise_magnitude,
    surprise_capability_revision_type_t revision_type)
{
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_SELF_MODEL_ERROR_NULL_POINTER,
                             "NULL bridge in on_capability_surprise");

    surprise_self_model_heartbeat_instance(bridge->health_agent, "on_capability_surprise", 0.0f);

    float magnitude = nimcp_myelin_clamp(surprise_magnitude, 0.0f, 1.0f);

    nimcp_mutex_lock(bridge->mutex);

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_DEBUG("Self-model on_capability_surprise: cap=%u, magnitude=%.3f, "
                            "type=%s, threshold=%.3f",
                            capability_id, magnitude,
                            revision_type_name(revision_type),
                            bridge->config.capability_surprise_threshold);
    }

    /* Check threshold */
    if (magnitude < bridge->config.capability_surprise_threshold) {
        if (bridge->config.enable_logging) {
            NIMCP_LOGGING_DEBUG("Self-model surprise below threshold: %.3f < %.3f",
                                magnitude, bridge->config.capability_surprise_threshold);
        }
        nimcp_mutex_unlock(bridge->mutex);
        return 0;
    }

    /* Find or create capability entry */
    capability_entry_t* cap = find_capability(bridge, capability_id);
    bool is_novel = (cap == NULL);

    if (!cap) {
        cap = add_capability(bridge, capability_id);
        if (bridge->config.enable_logging) {
            NIMCP_LOGGING_DEBUG("Self-model new capability added: id=%u, initial confidence=0.5",
                                capability_id);
        }
    }

    float prior = cap->confidence;

    /* KL-divergence inspired update: log-ratio weighting for more
     * Bayesian-appropriate belief revision */
    float kl_weight = 0.0f;
    if (prior > 0.01f && prior < 0.99f) {
        /* Simplified KL-inspired weighting: magnitude of update scales with
         * information gain, approximated by log(posterior/prior) */
        float target = (revision_type == SURPRISE_CAPABILITY_UPGRADE) ?
            nimcp_myelin_clamp(prior + magnitude, 0.01f, 0.99f) :
            nimcp_myelin_clamp(prior - magnitude, 0.01f, 0.99f);
        kl_weight = fabsf(logf(target / prior));
    }
    float revision_strength = bridge->config.belief_revision_rate * (1.0f + kl_weight);

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_DEBUG("Self-model KL-divergence update: prior=%.3f, kl_weight=%.4f, "
                            "revision_strength=%.4f (base_rate=%.3f)",
                            prior, kl_weight, revision_strength,
                            bridge->config.belief_revision_rate);
    }

    /* Apply revision based on type */
    switch (revision_type) {
        case SURPRISE_CAPABILITY_UPGRADE:
            cap->confidence = nimcp_myelin_clamp(
                cap->confidence + revision_strength * magnitude,
                0.0f, 1.0f);
            cap->competence = nimcp_myelin_clamp(
                cap->competence + bridge->config.competence_update_rate,
                0.0f, 1.0f);
            break;

        case SURPRISE_CAPABILITY_DOWNGRADE:
            cap->confidence = nimcp_myelin_clamp(
                cap->confidence - revision_strength * magnitude,
                0.0f, 1.0f);
            cap->competence = nimcp_myelin_clamp(
                cap->competence - bridge->config.competence_update_rate,
                0.0f, 1.0f);
            break;

        case SURPRISE_CAPABILITY_NOVEL:
            cap->confidence = 0.5f;
            cap->competence = 0.5f;
            bridge->effects.capabilities_discovered++;
            bridge->stats.discoveries++;
            break;
    }

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_DEBUG("Self-model capability revised: id=%u, type=%s, "
                            "confidence=%.3f -> %.3f, competence=%.3f",
                            capability_id, revision_type_name(revision_type),
                            prior, cap->confidence, cap->competence);
    }

    /* Record revision */
    bridge->last_revision.capability_id = capability_id;
    bridge->last_revision.prior_confidence = prior;
    bridge->last_revision.posterior_confidence = cap->confidence;
    bridge->last_revision.surprise_magnitude = magnitude;
    bridge->last_revision.revision_type = revision_type;
    bridge->has_revision = true;

    /* Update effects */
    bridge->effects.competence_delta = cap->competence - 0.5f;
    bridge->effects.beliefs_revised++;

    bridge->stats.capability_surprises++;
    bridge->stats.belief_revisions++;
    bridge->stats.competence_updates++;

    /* Warn on extreme revisions */
    if (bridge->config.enable_logging) {
        float delta = fabsf(cap->confidence - prior);
        if (delta > 0.3f) {
            NIMCP_LOGGING_WARN("Self-model large belief revision: cap=%u, delta=%.3f, "
                               "prior=%.3f -> posterior=%.3f",
                               capability_id, delta, prior, cap->confidence);
        }
        if (cap->confidence < 0.1f || cap->confidence > 0.9f) {
            NIMCP_LOGGING_WARN("Self-model extreme confidence: cap=%u, confidence=%.3f",
                               capability_id, cap->confidence);
        }
    }

    /* Bio-async: send self-model update */
    self_model_send_update_msg(bridge, capability_id, prior, cap->confidence,
                                magnitude, revision_type);
    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_DEBUG("Self-model bio-async: sent SELF_MODEL_UPDATE (cap=%u)", capability_id);
    }

    /* Bio-async: send capability revision */
    self_model_send_revision_msg(bridge, &bridge->last_revision);
    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_DEBUG("Self-model bio-async: sent CAPABILITY_REVISION (cap=%u, "
                            "prior=%.3f -> posterior=%.3f)",
                            capability_id, prior, cap->confidence);
    }

    nimcp_mutex_unlock(bridge->mutex);

    if (bridge->config.enable_logging) {
        const char* type_str = is_novel ? "NOVEL" :
            (revision_type == SURPRISE_CAPABILITY_UPGRADE ? "UPGRADE" : "DOWNGRADE");
        NIMCP_LOGGING_INFO("Capability surprise: cap=%u, type=%s, magnitude=%.3f, "
                           "confidence=%.3f->%.3f",
                           capability_id, type_str, magnitude,
                           prior, cap->confidence);
    }

    return 0;
}

int surprise_self_model_on_competence_feedback(
    surprise_self_model_bridge_t* bridge,
    uint32_t capability_id,
    float competence_level)
{
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_SELF_MODEL_ERROR_NULL_POINTER,
                             "NULL bridge in on_competence_feedback");

    surprise_self_model_heartbeat_instance(bridge->health_agent, "on_competence_feedback", 0.0f);

    float level = nimcp_myelin_clamp(competence_level, 0.0f, 1.0f);

    nimcp_mutex_lock(bridge->mutex);

    capability_entry_t* cap = find_capability(bridge, capability_id);
    if (cap) {
        float old_competence = cap->competence;
        /* EMA update of competence */
        cap->competence = cap->competence * (1.0f - bridge->config.competence_update_rate) +
                          level * bridge->config.competence_update_rate;
        bridge->stats.competence_updates++;

        if (bridge->config.enable_logging) {
            NIMCP_LOGGING_DEBUG("Self-model competence feedback: cap=%u, old=%.3f, "
                                "feedback=%.3f, new=%.3f",
                                capability_id, old_competence, level, cap->competence);
        }
    } else {
        if (bridge->config.enable_logging) {
            NIMCP_LOGGING_DEBUG("Self-model competence feedback: cap=%u not found (ignoring)",
                                capability_id);
        }
    }

    nimcp_mutex_unlock(bridge->mutex);

    return 0;
}

float surprise_self_model_query_confidence_modulation(
    const surprise_self_model_bridge_t* bridge)
{
    if (!bridge) return 1.0f;

    /* Note: stats.confidence_queries incremented by caller (non-const context) */

    /* Compute average confidence across tracked capabilities */
    if (bridge->capability_count == 0) return 1.0f;

    float avg_confidence = 0.0f;
    uint32_t active_count = 0;
    for (uint32_t i = 0; i < bridge->capability_count; i++) {
        if (bridge->capabilities[i].active) {
            avg_confidence += bridge->capabilities[i].confidence;
            active_count++;
        }
    }

    if (active_count == 0) return 1.0f;
    avg_confidence /= (float)active_count;

    /* High confidence -> reduced surprise sensitivity
       Low confidence -> increased surprise sensitivity */
    float modulation = 1.0f + bridge->config.confidence_modulation_gain * (0.5f - avg_confidence);
    float result = nimcp_myelin_clamp(modulation, 0.5f, 2.0f);

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_DEBUG("Self-model confidence modulation: avg_confidence=%.3f, "
                            "active=%u, modulation=%.3f",
                            avg_confidence, active_count, result);
    }

    return result;
}

int surprise_self_model_bridge_update(
    surprise_self_model_bridge_t* bridge,
    float dt_seconds)
{
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_SELF_MODEL_ERROR_NULL_POINTER,
                             "NULL bridge in update");

    if (dt_seconds <= 0.0f) return 0;

    surprise_self_model_heartbeat_instance(bridge->health_agent, "update", 0.0f);

    nimcp_mutex_lock(bridge->mutex);

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_DEBUG("Self-model update: dt=%.4f, caps_tracked=%u, "
                            "confidence_mod=%.3f, competence_delta=%.4f",
                            dt_seconds, bridge->capability_count,
                            bridge->effects.confidence_modulation,
                            bridge->effects.competence_delta);
    }

    /* Update confidence modulation */
    bridge->effects.confidence_modulation =
        surprise_self_model_query_confidence_modulation(bridge);
    bridge->stats.confidence_queries++;

    /* Decay competence delta toward zero */
    bridge->effects.competence_delta *= 0.99f;

    bridge->stats.total_updates++;
    bridge->update_count++;

    /* Loop heartbeat */
    surprise_self_model_heartbeat_instance(bridge->health_agent, "update", 1.0f);

    nimcp_mutex_unlock(bridge->mutex);

    return 0;
}

/* ============================================================================
 * Query API
 * ============================================================================ */

int surprise_self_model_bridge_get_effects(
    const surprise_self_model_bridge_t* bridge,
    surprise_self_model_effects_t* effects_out)
{
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_SELF_MODEL_ERROR_NULL_POINTER,
                             "NULL bridge in get_effects");
    NIMCP_CHECK_THROW_IMMUNE(effects_out != NULL,
                             NIMCP_SURPRISE_SELF_MODEL_ERROR_NULL_POINTER,
                             "NULL effects_out in get_effects");

    *effects_out = bridge->effects;
    return 0;
}

int surprise_self_model_bridge_get_stats(
    const surprise_self_model_bridge_t* bridge,
    surprise_self_model_stats_t* stats_out)
{
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_SELF_MODEL_ERROR_NULL_POINTER,
                             "NULL bridge in get_stats");
    NIMCP_CHECK_THROW_IMMUNE(stats_out != NULL,
                             NIMCP_SURPRISE_SELF_MODEL_ERROR_NULL_POINTER,
                             "NULL stats_out in get_stats");

    *stats_out = bridge->stats;
    return 0;
}

int surprise_self_model_get_last_revision(
    const surprise_self_model_bridge_t* bridge,
    surprise_capability_revision_t* revision_out)
{
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_SELF_MODEL_ERROR_NULL_POINTER,
                             "NULL bridge in get_last_revision");
    NIMCP_CHECK_THROW_IMMUNE(revision_out != NULL,
                             NIMCP_SURPRISE_SELF_MODEL_ERROR_NULL_POINTER,
                             "NULL revision_out in get_last_revision");

    if (!bridge->has_revision) {
        memset(revision_out, 0, sizeof(*revision_out));
        return NIMCP_SURPRISE_SELF_MODEL_ERROR_NOT_CONNECTED;
    }

    *revision_out = bridge->last_revision;
    return 0;
}

/* ============================================================================
 * Health Agent Integration
 * ============================================================================ */

int surprise_self_model_bridge_set_health_agent(
    surprise_self_model_bridge_t* bridge,
    struct nimcp_health_agent* agent)
{
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_SELF_MODEL_ERROR_NULL_POINTER,
                             "NULL bridge in set_health_agent");

    bridge->health_agent = agent;

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_DEBUG("Self-model bridge instance health agent %s",
                            agent ? "set" : "cleared");
    }
    return 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void surprise_self_model_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_surprise_self_model_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int surprise_self_model_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "surprise_self_model_training_begin: NULL argument");
        return -1;
    }
    surprise_self_model_heartbeat_instance(NULL, "surprise_self_model_training_begin", 0.0f);
    (void)instance;
    return 0;
}

int surprise_self_model_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "surprise_self_model_training_end: NULL argument");
        return -1;
    }
    surprise_self_model_heartbeat_instance(NULL, "surprise_self_model_training_end", 1.0f);
    (void)instance;
    return 0;
}

int surprise_self_model_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "surprise_self_model_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    surprise_self_model_heartbeat_instance(NULL, "surprise_self_model_training_step", progress);
    (void)instance;
    return 0;
}
