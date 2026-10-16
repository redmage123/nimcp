//=============================================================================
// nimcp_wave10_affective_kg.c - Wave W10 Affective / Social KG wiring
//=============================================================================
/**
 * See include/cognitive/kg/nimcp_wave10_affective_kg.h for scope + contract.
 *
 * Admin-token self-elevate pattern — all writes follow the
 * kg-node-naming-registry.md §7 rule: set ADMIN with
 * brain->internal_kg_admin_token, write, restore READ.
 */

#include "cognitive/kg/nimcp_wave10_affective_kg.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/nimcp_brain_kg.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(wave10_affective_kg, MESH_ADAPTER_CATEGORY_COGNITIVE)

/* ========================================================================= */
/* Internal helpers                                                          */
/* ========================================================================= */

static int w10_kg_usable(struct brain_struct* brain) {
    return (brain && brain->internal_kg_enabled && brain->internal_kg);
}

static uint64_t w10_ts_us(void) {
    return (uint64_t)time(NULL) * 1000000ULL;
}

/** Add a node if missing. Caller must hold ADMIN. */
static brain_kg_node_id_t w10_ensure_node(
    brain_kg_t* kg, const char* name,
    brain_kg_node_type_t type, const char* desc)
{
    brain_kg_node_id_t id = brain_kg_find_node(kg, name);
    if (id != BRAIN_KG_INVALID_NODE) return id;
    return brain_kg_add_node(kg, name, type, desc);
}

/** Emit an event node + back-edge to a parent (produced_by). Elevates. */
static void w10_emit_linked(
    struct brain_struct* brain,
    const char* event_name,
    const char* description,
    const char* parent_name,
    float weight)
{
    if (!w10_kg_usable(brain) || !event_name || !parent_name) return;

    brain_kg_t* kg = brain->internal_kg;
    const uint64_t token = brain->internal_kg_admin_token;

    brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_ADMIN, token);

    brain_kg_node_id_t ev = brain_kg_add_node(kg, event_name,
        BRAIN_KG_NODE_COGNITIVE,
        description ? description : "wave10 affective/social runtime event");

    if (ev != BRAIN_KG_INVALID_NODE) {
        brain_kg_node_id_t parent = brain_kg_find_node(kg, parent_name);
        if (parent != BRAIN_KG_INVALID_NODE) {
            brain_kg_add_edge(kg, parent, ev, BRAIN_KG_EDGE_SENDS_TO,
                              "produced_by", weight);
        }
    }

    brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_READ, 0);
}

/** Store numeric metadata on a named root. Elevates + restores. */
static void w10_store_meta_f(
    struct brain_struct* brain,
    const char* root_name,
    const char* key,
    float value)
{
    if (!w10_kg_usable(brain) || !root_name || !key) return;
    brain_kg_t* kg = brain->internal_kg;
    brain_kg_node_id_t nid = brain_kg_find_node(kg, root_name);
    if (nid == BRAIN_KG_INVALID_NODE) return;

    const uint64_t token = brain->internal_kg_admin_token;
    brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_ADMIN, token);

    char buf[32];
    snprintf(buf, sizeof(buf), "%.4f", value);
    brain_kg_add_metadata(kg, nid, key, buf);

    brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_READ, 0);
}

/** Read a float metadata value from a named root; default on miss. */
static float w10_read_meta_f(
    struct brain_struct* brain,
    const char* root_name,
    const char* key,
    float default_v)
{
    if (!w10_kg_usable(brain) || !root_name || !key) return default_v;
    brain_kg_t* kg = brain->internal_kg;
    brain_kg_node_id_t nid = brain_kg_find_node(kg, root_name);
    if (nid == BRAIN_KG_INVALID_NODE) return default_v;
    const brain_kg_node_t* n = brain_kg_get_node(kg, nid);
    if (!n) return default_v;
    for (uint32_t i = 0; i < n->metadata_count; ++i) {
        if (strcmp(n->metadata[i].key, key) == 0) {
            float v = default_v;
            if (sscanf(n->metadata[i].value, "%f", &v) == 1) return v;
            return default_v;
        }
    }
    return default_v;
}

/* ========================================================================= */
/* Structural init                                                           */
/* ========================================================================= */

/**
 * Register all 10 structural roots + a handful of obvious cross-edges.
 * Idempotent.
 */
int nimcp_wave10_affective_kg_init(struct brain_struct* brain) {
    if (!w10_kg_usable(brain)) return 0;

    brain_kg_t* kg = brain->internal_kg;
    brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_ADMIN,
                              brain->internal_kg_admin_token);

    struct { const char* name; const char* desc; } roots[] = {
        { "cog_emotions",             "W10: Russell circumplex dimensional emotion system (valence+arousal)" },
        { "cog_theory_of_mind",       "W10: Theory of Mind — false belief, emotion/goal inference" },
        { "cog_mirror_neurons",       "W10: Mirror neurons — observation/imitation/empathy substrate" },
        { "cog_social_interaction",   "W10: Multi-agent social interaction framework" },
        { "cog_collective_cognition", "W10: Collective cognition / extended mind / phi" },
        { "cog_personality",          "W10: Big-5 personality + identity system" },
        { "cog_empathetic_response",  "W10: Empathetic response generation + strategy" },
        { "cog_emotion_recognition",  "W10: Detect emotion in text / other-agent observations" },
        { "cog_grief",                "W10: Grief, loss, and bereavement processing" },
        { "cog_shadow_emotions",      "W10: Shadow emotions (Dark Triad + CBT-style interventions)" },
    };

    const size_t n = sizeof(roots) / sizeof(roots[0]);
    brain_kg_node_id_t ids[10];
    for (size_t i = 0; i < n; ++i) {
        ids[i] = w10_ensure_node(kg, roots[i].name,
                                 BRAIN_KG_NODE_COGNITIVE, roots[i].desc);
    }

    /* Cross-edges: affective primitives that are always-true (not runtime). */
    /* emotion -> mirror resonance */
    if (ids[0] != BRAIN_KG_INVALID_NODE && ids[2] != BRAIN_KG_INVALID_NODE) {
        brain_kg_add_edge(kg, ids[0], ids[2], BRAIN_KG_EDGE_MODULATES,
                          "emotion modulates mirror resonance", 0.7f);
    }
    /* mirror -> ToM */
    if (ids[2] != BRAIN_KG_INVALID_NODE && ids[1] != BRAIN_KG_INVALID_NODE) {
        brain_kg_add_edge(kg, ids[2], ids[1], BRAIN_KG_EDGE_PROVIDES_TO,
                          "mirror feeds simulation in ToM", 0.8f);
    }
    /* ToM -> empathetic_response */
    if (ids[1] != BRAIN_KG_INVALID_NODE && ids[6] != BRAIN_KG_INVALID_NODE) {
        brain_kg_add_edge(kg, ids[1], ids[6], BRAIN_KG_EDGE_PROVIDES_TO,
                          "ToM informs empathy target model", 0.85f);
    }
    /* emotion_recognition -> empathetic_response */
    if (ids[7] != BRAIN_KG_INVALID_NODE && ids[6] != BRAIN_KG_INVALID_NODE) {
        brain_kg_add_edge(kg, ids[7], ids[6], BRAIN_KG_EDGE_PROVIDES_TO,
                          "recognized emotion drives response", 0.9f);
    }
    /* personality -> social_interaction */
    if (ids[5] != BRAIN_KG_INVALID_NODE && ids[3] != BRAIN_KG_INVALID_NODE) {
        brain_kg_add_edge(kg, ids[5], ids[3], BRAIN_KG_EDGE_MODULATES,
                          "personality traits modulate social behavior", 0.7f);
    }
    /* grief -> emotion (couples affect) */
    if (ids[8] != BRAIN_KG_INVALID_NODE && ids[0] != BRAIN_KG_INVALID_NODE) {
        brain_kg_add_edge(kg, ids[8], ids[0], BRAIN_KG_EDGE_MODULATES,
                          "grief modulates emotion state", 0.8f);
    }
    /* shadow -> emotion */
    if (ids[9] != BRAIN_KG_INVALID_NODE && ids[0] != BRAIN_KG_INVALID_NODE) {
        brain_kg_add_edge(kg, ids[9], ids[0], BRAIN_KG_EDGE_MODULATES,
                          "shadow patterns reshape emotion", 0.6f);
    }
    /* collective -> social */
    if (ids[4] != BRAIN_KG_INVALID_NODE && ids[3] != BRAIN_KG_INVALID_NODE) {
        brain_kg_add_edge(kg, ids[4], ids[3], BRAIN_KG_EDGE_INTEGRATES_WITH,
                          "collective emerges from social interactions", 0.7f);
    }

    brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_READ, 0);

    wave10_affective_kg_heartbeat("wave10_init", 1.0f);
    return 0;
}

/* ========================================================================= */
/* 1. emotions                                                               */
/* ========================================================================= */

void wave10_emotions_emit_state_change(
    struct brain_struct* brain,
    float valence, float arousal, float intensity)
{
    if (!w10_kg_usable(brain)) return;
    char name[128];
    snprintf(name, sizeof(name),
             "cog_emotions_event_state_%llu",
             (unsigned long long)w10_ts_us());
    char desc[192];
    snprintf(desc, sizeof(desc),
             "emotion state: valence=%.3f arousal=%.3f intensity=%.3f",
             valence, arousal, intensity);
    w10_emit_linked(brain, name, desc, "cog_emotions", intensity);
    /* Store arousal as bias metadata for downstream queries. */
    w10_store_meta_f(brain, "cog_emotions", "arousal", arousal);
    w10_store_meta_f(brain, "cog_emotions", "valence", valence);
    w10_store_meta_f(brain, "cog_emotions", "intensity", intensity);
}

float wave10_emotions_query_arousal_bias(struct brain_struct* brain) {
    return w10_read_meta_f(brain, "cog_emotions", "arousal", 0.5f);
}

/* ========================================================================= */
/* 2. theory_of_mind                                                         */
/* ========================================================================= */

void wave10_tom_emit_belief_event(
    struct brain_struct* brain,
    uint32_t agent_id,
    const char* belief_kind,
    float confidence)
{
    if (!w10_kg_usable(brain)) return;
    const char* kind = (belief_kind && *belief_kind) ? belief_kind : "belief";
    char name[160];
    snprintf(name, sizeof(name),
             "cog_tom_event_%.32s_%u_%llu",
             kind, agent_id, (unsigned long long)w10_ts_us());
    char desc[192];
    snprintf(desc, sizeof(desc),
             "ToM belief event: agent=%u kind=%s confidence=%.3f",
             agent_id, kind, confidence);
    w10_emit_linked(brain, name, desc, "cog_theory_of_mind", confidence);
    w10_store_meta_f(brain, "cog_theory_of_mind",
                     "last_belief_confidence", confidence);
    /* Track a simple model counter for the query. */
    float prev = w10_read_meta_f(brain, "cog_theory_of_mind", "model_count", 0.0f);
    w10_store_meta_f(brain, "cog_theory_of_mind", "model_count", prev + 1.0f);
}

float wave10_tom_query_model_count_bias(struct brain_struct* brain) {
    float c = w10_read_meta_f(brain, "cog_theory_of_mind",
                              "model_count", 0.0f);
    return (c > 0.0f) ? 1.0f : 0.5f;
}

/* ========================================================================= */
/* 3. mirror_neurons                                                         */
/* ========================================================================= */

void wave10_mirror_emit_activation(
    struct brain_struct* brain,
    uint32_t action_id,
    float activation)
{
    if (!w10_kg_usable(brain)) return;
    char name[160];
    snprintf(name, sizeof(name),
             "cog_mirror_event_activate_%u_%llu",
             action_id, (unsigned long long)w10_ts_us());
    char desc[192];
    snprintf(desc, sizeof(desc),
             "mirror activation: action=%u activation=%.3f",
             action_id, activation);
    w10_emit_linked(brain, name, desc, "cog_mirror_neurons", activation);
    w10_store_meta_f(brain, "cog_mirror_neurons",
                     "last_activation", activation);
}

float wave10_mirror_query_activity_bias(struct brain_struct* brain) {
    float a = w10_read_meta_f(brain, "cog_mirror_neurons",
                              "last_activation", 0.0f);
    return (a > 0.05f) ? a : 0.5f;
}

/* ========================================================================= */
/* 4. social_interaction                                                     */
/* ========================================================================= */

void wave10_social_emit_episode_outcome(
    struct brain_struct* brain,
    uint32_t num_agents,
    float convergence,
    float reward)
{
    if (!w10_kg_usable(brain)) return;
    char name[160];
    snprintf(name, sizeof(name),
             "cog_social_event_episode_%u_%llu",
             num_agents, (unsigned long long)w10_ts_us());
    char desc[192];
    snprintf(desc, sizeof(desc),
             "social episode: agents=%u convergence=%.3f reward=%.3f",
             num_agents, convergence, reward);
    w10_emit_linked(brain, name, desc, "cog_social_interaction", convergence);
    w10_store_meta_f(brain, "cog_social_interaction",
                     "convergence", convergence);
    w10_store_meta_f(brain, "cog_social_interaction", "reward", reward);
}

float wave10_social_query_convergence_bias(struct brain_struct* brain) {
    return w10_read_meta_f(brain, "cog_social_interaction",
                           "convergence", 0.5f);
}

/* ========================================================================= */
/* 5. collective_cognition                                                   */
/* ========================================================================= */

void wave10_collective_emit_level_change(
    struct brain_struct* brain,
    int level,
    float phi)
{
    if (!w10_kg_usable(brain)) return;
    char name[160];
    snprintf(name, sizeof(name),
             "cog_collective_event_level_%d_%llu",
             level, (unsigned long long)w10_ts_us());
    char desc[192];
    snprintf(desc, sizeof(desc),
             "collective consciousness level=%d phi=%.3f", level, phi);
    w10_emit_linked(brain, name, desc, "cog_collective_cognition", phi);
    w10_store_meta_f(brain, "cog_collective_cognition", "phi", phi);
}

float wave10_collective_query_phi_bias(struct brain_struct* brain) {
    return w10_read_meta_f(brain, "cog_collective_cognition", "phi", 0.5f);
}

/* ========================================================================= */
/* 6. personality                                                            */
/* ========================================================================= */

void wave10_personality_emit_trait_expression(
    struct brain_struct* brain,
    float openness, float conscientiousness, float extraversion,
    float agreeableness, float neuroticism)
{
    if (!w10_kg_usable(brain)) return;
    char name[160];
    snprintf(name, sizeof(name),
             "cog_personality_event_traits_%llu",
             (unsigned long long)w10_ts_us());
    char desc[192];
    snprintf(desc, sizeof(desc),
             "traits: O=%.2f C=%.2f E=%.2f A=%.2f N=%.2f",
             openness, conscientiousness, extraversion,
             agreeableness, neuroticism);
    w10_emit_linked(brain, name, desc, "cog_personality", extraversion);
    w10_store_meta_f(brain, "cog_personality", "openness",          openness);
    w10_store_meta_f(brain, "cog_personality", "conscientiousness", conscientiousness);
    w10_store_meta_f(brain, "cog_personality", "extraversion",      extraversion);
    w10_store_meta_f(brain, "cog_personality", "agreeableness",     agreeableness);
    w10_store_meta_f(brain, "cog_personality", "neuroticism",       neuroticism);
}

float wave10_personality_query_extraversion_bias(struct brain_struct* brain) {
    return w10_read_meta_f(brain, "cog_personality", "extraversion", 0.5f);
}

/* ========================================================================= */
/* 7. empathetic_response                                                    */
/* ========================================================================= */

void wave10_empathy_emit_response(
    struct brain_struct* brain,
    int strategy,
    bool crisis_detected,
    float predicted_safety)
{
    if (!w10_kg_usable(brain)) return;
    char name[160];
    snprintf(name, sizeof(name),
             "cog_empathy_event_response_%d_%llu",
             strategy, (unsigned long long)w10_ts_us());
    char desc[192];
    snprintf(desc, sizeof(desc),
             "empathy response: strategy=%d crisis=%d safety=%.3f",
             strategy, crisis_detected ? 1 : 0, predicted_safety);
    w10_emit_linked(brain, name, desc, "cog_empathetic_response",
                    predicted_safety);
    w10_store_meta_f(brain, "cog_empathetic_response",
                     "predicted_safety", predicted_safety);
}

void wave10_empathy_emit_effectiveness(
    struct brain_struct* brain,
    float effectiveness_score)
{
    if (!w10_kg_usable(brain)) return;
    char name[160];
    snprintf(name, sizeof(name),
             "cog_empathy_event_effectiveness_%llu",
             (unsigned long long)w10_ts_us());
    char desc[192];
    snprintf(desc, sizeof(desc),
             "empathy effectiveness: score=%.3f", effectiveness_score);
    w10_emit_linked(brain, name, desc, "cog_empathetic_response",
                    effectiveness_score);
    w10_store_meta_f(brain, "cog_empathetic_response",
                     "effectiveness", effectiveness_score);
}

float wave10_empathy_query_safety_bias(struct brain_struct* brain) {
    return w10_read_meta_f(brain, "cog_empathetic_response",
                           "predicted_safety", 0.5f);
}

/* ========================================================================= */
/* 8. emotion_recognition                                                    */
/* ========================================================================= */

void wave10_emorec_emit_detection(
    struct brain_struct* brain,
    const char* emotion_label,
    float confidence)
{
    if (!w10_kg_usable(brain)) return;
    const char* lbl = (emotion_label && *emotion_label) ? emotion_label : "unknown";
    char name[192];
    snprintf(name, sizeof(name),
             "cog_emorec_event_detect_%.48s_%llu",
             lbl, (unsigned long long)w10_ts_us());
    char desc[192];
    snprintf(desc, sizeof(desc),
             "emotion recognition: label=%s confidence=%.3f", lbl, confidence);
    w10_emit_linked(brain, name, desc, "cog_emotion_recognition", confidence);
    w10_store_meta_f(brain, "cog_emotion_recognition",
                     "last_confidence", confidence);
}

float wave10_emorec_query_detection_bias(struct brain_struct* brain) {
    return w10_read_meta_f(brain, "cog_emotion_recognition",
                           "last_confidence", 0.5f);
}

/* ========================================================================= */
/* 9. grief                                                                  */
/* ========================================================================= */

void wave10_grief_emit_stage_transition(
    struct brain_struct* brain,
    int from_stage, int to_stage,
    float pain_intensity)
{
    if (!w10_kg_usable(brain)) return;
    char name[160];
    snprintf(name, sizeof(name),
             "cog_grief_event_stage_%d_%d_%llu",
             from_stage, to_stage, (unsigned long long)w10_ts_us());
    char desc[192];
    snprintf(desc, sizeof(desc),
             "grief stage transition: %d -> %d pain=%.3f",
             from_stage, to_stage, pain_intensity);
    w10_emit_linked(brain, name, desc, "cog_grief", pain_intensity);
    w10_store_meta_f(brain, "cog_grief", "pain", pain_intensity);
    w10_store_meta_f(brain, "cog_grief", "stage", (float)to_stage);
}

float wave10_grief_query_pain_bias(struct brain_struct* brain) {
    return w10_read_meta_f(brain, "cog_grief", "pain", 0.5f);
}

/* ========================================================================= */
/* 10. shadow_emotions                                                       */
/* ========================================================================= */

void wave10_shadow_emit_activation(
    struct brain_struct* brain,
    const char* shadow_name,
    float intensity)
{
    if (!w10_kg_usable(brain)) return;
    const char* s = (shadow_name && *shadow_name) ? shadow_name : "shadow";
    char name[192];
    snprintf(name, sizeof(name),
             "cog_shadow_event_activate_%.32s_%llu",
             s, (unsigned long long)w10_ts_us());
    char desc[192];
    snprintf(desc, sizeof(desc),
             "shadow activation: %s intensity=%.3f", s, intensity);
    w10_emit_linked(brain, name, desc, "cog_shadow_emotions", intensity);
    /* Keep max intensity across shadows as the query bias. */
    float cur = w10_read_meta_f(brain, "cog_shadow_emotions",
                                "max_intensity", 0.0f);
    if (intensity > cur) {
        w10_store_meta_f(brain, "cog_shadow_emotions",
                         "max_intensity", intensity);
    }
}

float wave10_shadow_query_intensity_bias(struct brain_struct* brain) {
    float v = w10_read_meta_f(brain, "cog_shadow_emotions",
                              "max_intensity", 0.0f);
    return (v > 0.0f) ? v : 0.5f;
}

/* Suppress unused bridge-boilerplate helpers. */
__attribute__((unused))
static void wave10_affective_kg_suppress_unused(void) {
    (void)wave10_affective_kg_set_health_agent;
    (void)wave10_affective_kg_heartbeat;
}
