//=============================================================================
// nimcp_wave13_metacog_kg.c - Wave W13 Meta-cognitive KG wiring
//=============================================================================
/**
 * See include/cognitive/kg/nimcp_wave13_metacog_kg.h for scope + contract.
 *
 * Admin-token self-elevate pattern — all writes follow the
 * kg-node-naming-registry.md §7 rule: set ADMIN with
 * brain->internal_kg_admin_token, write, restore READ.
 *
 * Template: nimcp_wave10_affective_kg.c (shared-helper pattern).
 */

#include "cognitive/kg/nimcp_wave13_metacog_kg.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <math.h>

#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/nimcp_brain_kg.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(wave13_metacog_kg, MESH_ADAPTER_CATEGORY_COGNITIVE)

/* ========================================================================= */
/* Internal helpers                                                          */
/* ========================================================================= */

static int w13_kg_usable(struct brain_struct* brain) {
    return (brain && brain->internal_kg_enabled && brain->internal_kg);
}

static uint64_t w13_ts_us(void) {
    return (uint64_t)time(NULL) * 1000000ULL;
}

/** Add a node if missing. Caller must hold ADMIN. */
static brain_kg_node_id_t w13_ensure_node(
    brain_kg_t* kg, const char* name,
    brain_kg_node_type_t type, const char* desc)
{
    brain_kg_node_id_t id = brain_kg_find_node(kg, name);
    if (id != BRAIN_KG_INVALID_NODE) return id;
    return brain_kg_add_node(kg, name, type, desc);
}

/** Emit an event node + back-edge to a parent (produced_by). Elevates. */
static void w13_emit_linked(
    struct brain_struct* brain,
    const char* event_name,
    const char* description,
    const char* parent_name,
    float weight)
{
    if (!w13_kg_usable(brain) || !event_name || !parent_name) return;

    brain_kg_t* kg = brain->internal_kg;
    const uint64_t token = brain->internal_kg_admin_token;

    brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_ADMIN, token);

    brain_kg_node_id_t ev = brain_kg_add_node(kg, event_name,
        BRAIN_KG_NODE_COGNITIVE,
        description ? description : "wave13 meta-cognitive runtime event");

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
static void w13_store_meta_f(
    struct brain_struct* brain,
    const char* root_name,
    const char* key,
    float value)
{
    if (!w13_kg_usable(brain) || !root_name || !key) return;
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
static float w13_read_meta_f(
    struct brain_struct* brain,
    const char* root_name,
    const char* key,
    float default_v)
{
    if (!w13_kg_usable(brain) || !root_name || !key) return default_v;
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

int nimcp_wave13_metacog_kg_init(struct brain_struct* brain) {
    if (!w13_kg_usable(brain)) return 0;

    brain_kg_t* kg = brain->internal_kg;
    brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_ADMIN,
                              brain->internal_kg_admin_token);

    struct { const char* name; const char* desc; } roots[] = {
        { "cog_curiosity",                 "W13: Curiosity engine (gaps, novelty, learning-potential)" },
        { "cog_information_forager",       "W13: Information forager (autonomous learn-tick loop)" },
        { "cog_meta_learning",             "W13: Meta-learning (MAML + per-region LR adaptation)" },
        { "cog_consolidation_cognitive",   "W13: Cognitive consolidation (synapse weighting, pruning)" },
        { "cog_sleep_wake",                "W13: Sleep-wake system (NREM/REM cycles + pressure)" },
        { "cog_self_curriculum",           "W13: Self-curriculum (uncertainty-driven item generation)" },
        { "cog_analogical_transfer",       "W13: Analogical transfer (structural similarity matching)" },
        { "cog_multiscale_memory",         "W13: Multi-timescale memory (immediate/short/long buffers)" },
        { "cog_contrastive_self",          "W13: Contrastive self-learning (hard negatives)" },
    };

    const size_t n = sizeof(roots) / sizeof(roots[0]);
    brain_kg_node_id_t ids[9];
    for (size_t i = 0; i < n; ++i) {
        ids[i] = w13_ensure_node(kg, roots[i].name,
                                 BRAIN_KG_NODE_COGNITIVE, roots[i].desc);
    }

    /* Cross-edges: meta-cognitive primitives that are always-true. */
    /* curiosity -> meta_learning (curiosity drives learning rate) */
    if (ids[0] != BRAIN_KG_INVALID_NODE && ids[2] != BRAIN_KG_INVALID_NODE) {
        brain_kg_add_edge(kg, ids[0], ids[2], BRAIN_KG_EDGE_MODULATES,
                          "curiosity modulates learning rate", 0.7f);
    }
    /* curiosity -> information_forager (forager pursues gaps) */
    if (ids[0] != BRAIN_KG_INVALID_NODE && ids[1] != BRAIN_KG_INVALID_NODE) {
        brain_kg_add_edge(kg, ids[0], ids[1], BRAIN_KG_EDGE_SENDS_TO,
                          "curiosity gaps direct forager", 0.85f);
    }
    /* sleep_wake -> consolidation (NREM/REM drives consolidation) */
    if (ids[4] != BRAIN_KG_INVALID_NODE && ids[3] != BRAIN_KG_INVALID_NODE) {
        brain_kg_add_edge(kg, ids[4], ids[3], BRAIN_KG_EDGE_SENDS_TO,
                          "sleep triggers consolidation", 0.9f);
    }
    /* self_curriculum -> curiosity (curriculum seeds curiosity targets) */
    if (ids[5] != BRAIN_KG_INVALID_NODE && ids[0] != BRAIN_KG_INVALID_NODE) {
        brain_kg_add_edge(kg, ids[5], ids[0], BRAIN_KG_EDGE_SENDS_TO,
                          "curriculum seeds curiosity targets", 0.65f);
    }
    /* analogical_transfer -> meta_learning (analogies accelerate meta-adapt) */
    if (ids[6] != BRAIN_KG_INVALID_NODE && ids[2] != BRAIN_KG_INVALID_NODE) {
        brain_kg_add_edge(kg, ids[6], ids[2], BRAIN_KG_EDGE_PROVIDES_TO,
                          "analogy accelerates meta-adaptation", 0.6f);
    }
    /* multiscale_memory -> consolidation (buffers feed consolidation) */
    if (ids[7] != BRAIN_KG_INVALID_NODE && ids[3] != BRAIN_KG_INVALID_NODE) {
        brain_kg_add_edge(kg, ids[7], ids[3], BRAIN_KG_EDGE_SENDS_TO,
                          "multiscale buffers feed consolidation", 0.75f);
    }
    /* contrastive_self -> meta_learning (hard negatives refine meta) */
    if (ids[8] != BRAIN_KG_INVALID_NODE && ids[2] != BRAIN_KG_INVALID_NODE) {
        brain_kg_add_edge(kg, ids[8], ids[2], BRAIN_KG_EDGE_PROVIDES_TO,
                          "hard negatives refine meta-learning", 0.5f);
    }
    /* consolidation -> multiscale_memory (consolidation promotes items) */
    if (ids[3] != BRAIN_KG_INVALID_NODE && ids[7] != BRAIN_KG_INVALID_NODE) {
        brain_kg_add_edge(kg, ids[3], ids[7], BRAIN_KG_EDGE_MODULATES,
                          "consolidation promotes across timescales", 0.7f);
    }

    brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_READ, 0);

    wave13_metacog_kg_heartbeat("wave13_init", 1.0f);
    return 0;
}

/* ========================================================================= */
/* 1. curiosity                                                              */
/* ========================================================================= */

void wave13_curiosity_emit_gap(
    struct brain_struct* brain,
    const char* concept_str,
    float gap_size,
    float curiosity_intensity)
{
    if (!w13_kg_usable(brain)) return;
    const char* c = (concept_str && *concept_str) ? concept_str : "unknown";
    char name[192];
    snprintf(name, sizeof(name),
             "cog_curiosity_event_gap_%.48s_%llu",
             c, (unsigned long long)w13_ts_us());
    char desc[192];
    snprintf(desc, sizeof(desc),
             "curiosity gap: concept=%s gap=%.3f intensity=%.3f",
             c, gap_size, curiosity_intensity);
    w13_emit_linked(brain, name, desc, "cog_curiosity", curiosity_intensity);
    w13_store_meta_f(brain, "cog_curiosity",
                     "last_intensity", curiosity_intensity);
    w13_store_meta_f(brain, "cog_curiosity",
                     "last_surprise", gap_size);
}

float wave13_curiosity_query_spike_bias(struct brain_struct* brain) {
    float v = w13_read_meta_f(brain, "cog_curiosity",
                              "last_intensity", 0.0f);
    return (v > 0.0f) ? v : 0.5f;
}

float wave13_curiosity_query_recent_surprise(struct brain_struct* brain) {
    return w13_read_meta_f(brain, "cog_curiosity", "last_surprise", 0.0f);
}

/* ========================================================================= */
/* 2. information_forager                                                    */
/* ========================================================================= */

void wave13_forager_emit_learn(
    struct brain_struct* brain,
    const char* topic,
    float quality_score,
    int result_code)
{
    if (!w13_kg_usable(brain)) return;
    const char* t = (topic && *topic) ? topic : "topic";
    char name[192];
    snprintf(name, sizeof(name),
             "cog_forager_event_learn_%.48s_%llu",
             t, (unsigned long long)w13_ts_us());
    char desc[192];
    snprintf(desc, sizeof(desc),
             "forager learn: topic=%s quality=%.3f rc=%d",
             t, quality_score, result_code);
    w13_emit_linked(brain, name, desc, "cog_information_forager", quality_score);
    w13_store_meta_f(brain, "cog_information_forager",
                     "last_quality", quality_score);
}

float wave13_forager_query_quality_bias(struct brain_struct* brain) {
    float v = w13_read_meta_f(brain, "cog_information_forager",
                              "last_quality", 0.0f);
    return (v > 0.0f) ? v : 0.5f;
}

/* ========================================================================= */
/* 3. meta_learning                                                          */
/* ========================================================================= */

void wave13_meta_emit_lr_adaptation(
    struct brain_struct* brain,
    int region_type,
    float old_lr,
    float new_lr,
    float loss)
{
    if (!w13_kg_usable(brain)) return;
    char name[160];
    snprintf(name, sizeof(name),
             "cog_meta_event_lr_adapt_%d_%llu",
             region_type, (unsigned long long)w13_ts_us());
    char desc[192];
    snprintf(desc, sizeof(desc),
             "meta LR adapt: region=%d old=%.5f new=%.5f loss=%.4f",
             region_type, old_lr, new_lr, loss);
    w13_emit_linked(brain, name, desc, "cog_meta_learning", new_lr);

    /* Adaptation magnitude bias: |delta| / old_lr, clamped [0,1]. */
    float denom = (fabsf(old_lr) > 1e-12f) ? fabsf(old_lr) : 1.0f;
    float mag = fabsf(new_lr - old_lr) / denom;
    if (mag > 1.0f) mag = 1.0f;
    w13_store_meta_f(brain, "cog_meta_learning", "adapt_magnitude", mag);
    w13_store_meta_f(brain, "cog_meta_learning", "last_lr", new_lr);
}

void wave13_meta_emit_inner_loop(
    struct brain_struct* brain,
    uint32_t num_steps,
    float final_loss)
{
    if (!w13_kg_usable(brain)) return;
    char name[160];
    snprintf(name, sizeof(name),
             "cog_meta_event_inner_loop_%u_%llu",
             num_steps, (unsigned long long)w13_ts_us());
    char desc[192];
    snprintf(desc, sizeof(desc),
             "meta inner-loop: steps=%u final_loss=%.4f",
             num_steps, final_loss);
    w13_emit_linked(brain, name, desc, "cog_meta_learning", final_loss);
    w13_store_meta_f(brain, "cog_meta_learning", "last_final_loss", final_loss);
}

float wave13_meta_query_adaptation_bias(struct brain_struct* brain) {
    float v = w13_read_meta_f(brain, "cog_meta_learning",
                              "adapt_magnitude", -1.0f);
    return (v >= 0.0f) ? v : 0.5f;
}

/* ========================================================================= */
/* 4. consolidation (cognitive — separate from W6 memory consolidation)      */
/* ========================================================================= */

void wave13_consolidation_emit_cycle(
    struct brain_struct* brain,
    uint32_t synapses_consolidated,
    uint32_t synapses_pruned,
    float duration_ms)
{
    if (!w13_kg_usable(brain)) return;
    char name[160];
    snprintf(name, sizeof(name),
             "cog_consolidation_event_cycle_%u_%llu",
             synapses_consolidated, (unsigned long long)w13_ts_us());
    char desc[192];
    snprintf(desc, sizeof(desc),
             "consolidation cycle: consolidated=%u pruned=%u dur_ms=%.1f",
             synapses_consolidated, synapses_pruned, duration_ms);
    /* Weight: total activity / 1M clamp into [0,1]. */
    float w = (float)(synapses_consolidated + synapses_pruned) / 1.0e6f;
    if (w > 1.0f) w = 1.0f;
    w13_emit_linked(brain, name, desc, "cog_consolidation_cognitive", w);
    w13_store_meta_f(brain, "cog_consolidation_cognitive", "load", w);
    w13_store_meta_f(brain, "cog_consolidation_cognitive",
                     "last_duration_ms", duration_ms);
}

float wave13_consolidation_query_load_bias(struct brain_struct* brain) {
    float v = w13_read_meta_f(brain, "cog_consolidation_cognitive",
                              "load", -1.0f);
    return (v >= 0.0f) ? v : 0.5f;
}

/* ========================================================================= */
/* 5. sleep_wake                                                             */
/* ========================================================================= */

void wave13_sleep_emit_stage_transition(
    struct brain_struct* brain,
    int from_state,
    int to_state,
    float pressure)
{
    if (!w13_kg_usable(brain)) return;
    char name[160];
    snprintf(name, sizeof(name),
             "cog_sleep_event_stage_%d_%d_%llu",
             from_state, to_state, (unsigned long long)w13_ts_us());
    char desc[192];
    snprintf(desc, sizeof(desc),
             "sleep stage transition: %d -> %d pressure=%.3f",
             from_state, to_state, pressure);
    w13_emit_linked(brain, name, desc, "cog_sleep_wake", pressure);
    w13_store_meta_f(brain, "cog_sleep_wake", "pressure", pressure);
    w13_store_meta_f(brain, "cog_sleep_wake", "state", (float)to_state);
}

float wave13_sleep_query_pressure_bias(struct brain_struct* brain) {
    float v = w13_read_meta_f(brain, "cog_sleep_wake", "pressure", -1.0f);
    return (v >= 0.0f) ? v : 0.5f;
}

/* ========================================================================= */
/* 6. self_curriculum                                                        */
/* ========================================================================= */

void wave13_curriculum_emit_uncertainty_update(
    struct brain_struct* brain,
    const char* domain,
    float uncertainty)
{
    if (!w13_kg_usable(brain)) return;
    const char* d = (domain && *domain) ? domain : "unknown";
    char name[192];
    snprintf(name, sizeof(name),
             "cog_curriculum_event_uncertain_%.48s_%llu",
             d, (unsigned long long)w13_ts_us());
    char desc[192];
    snprintf(desc, sizeof(desc),
             "curriculum uncertainty: domain=%s u=%.3f", d, uncertainty);
    w13_emit_linked(brain, name, desc, "cog_self_curriculum", uncertainty);
    w13_store_meta_f(brain, "cog_self_curriculum",
                     "last_uncertainty", uncertainty);
}

void wave13_curriculum_emit_item_generated(
    struct brain_struct* brain,
    const char* domain,
    uint32_t item_count)
{
    if (!w13_kg_usable(brain)) return;
    const char* d = (domain && *domain) ? domain : "unknown";
    char name[192];
    snprintf(name, sizeof(name),
             "cog_curriculum_event_gen_%.48s_%u_%llu",
             d, item_count, (unsigned long long)w13_ts_us());
    char desc[192];
    snprintf(desc, sizeof(desc),
             "curriculum generated: domain=%s items=%u", d, item_count);
    w13_emit_linked(brain, name, desc, "cog_self_curriculum",
                    (float)item_count / 64.0f);
    w13_store_meta_f(brain, "cog_self_curriculum",
                     "last_items_generated", (float)item_count);
}

float wave13_curriculum_query_uncertainty_bias(struct brain_struct* brain) {
    float v = w13_read_meta_f(brain, "cog_self_curriculum",
                              "last_uncertainty", -1.0f);
    return (v >= 0.0f) ? v : 0.5f;
}

/* ========================================================================= */
/* 7. analogical_transfer                                                    */
/* ========================================================================= */

void wave13_analogy_emit_match(
    struct brain_struct* brain,
    const char* label,
    float similarity,
    float success_score)
{
    if (!w13_kg_usable(brain)) return;
    const char* l = (label && *label) ? label : "pattern";
    char name[192];
    snprintf(name, sizeof(name),
             "cog_analogy_event_match_%.48s_%llu",
             l, (unsigned long long)w13_ts_us());
    char desc[192];
    snprintf(desc, sizeof(desc),
             "analogy match: label=%s sim=%.3f success=%.3f",
             l, similarity, success_score);
    w13_emit_linked(brain, name, desc, "cog_analogical_transfer", similarity);
    w13_store_meta_f(brain, "cog_analogical_transfer",
                     "last_similarity", similarity);
    w13_store_meta_f(brain, "cog_analogical_transfer",
                     "last_success", success_score);
}

float wave13_analogy_query_similarity_bias(struct brain_struct* brain) {
    float v = w13_read_meta_f(brain, "cog_analogical_transfer",
                              "last_similarity", -1.0f);
    return (v >= 0.0f) ? v : 0.5f;
}

/* ========================================================================= */
/* 8. multiscale_memory                                                      */
/* ========================================================================= */

void wave13_multiscale_emit_push(
    struct brain_struct* brain,
    const char* label,
    float importance)
{
    if (!w13_kg_usable(brain)) return;
    const char* l = (label && *label) ? label : "item";
    char name[192];
    snprintf(name, sizeof(name),
             "cog_multiscale_event_push_%.48s_%llu",
             l, (unsigned long long)w13_ts_us());
    char desc[192];
    snprintf(desc, sizeof(desc),
             "multiscale push: label=%s importance=%.3f", l, importance);
    w13_emit_linked(brain, name, desc, "cog_multiscale_memory", importance);
    w13_store_meta_f(brain, "cog_multiscale_memory",
                     "last_importance", importance);
}

float wave13_multiscale_query_importance_bias(struct brain_struct* brain) {
    float v = w13_read_meta_f(brain, "cog_multiscale_memory",
                              "last_importance", -1.0f);
    return (v >= 0.0f) ? v : 0.5f;
}

/* ========================================================================= */
/* 9. contrastive_self                                                       */
/* ========================================================================= */

void wave13_contrastive_emit_record(
    struct brain_struct* brain,
    const char* label,
    uint32_t buffer_count)
{
    if (!w13_kg_usable(brain)) return;
    const char* l = (label && *label) ? label : "sample";
    char name[192];
    snprintf(name, sizeof(name),
             "cog_contrastive_event_rec_%.48s_%u_%llu",
             l, buffer_count, (unsigned long long)w13_ts_us());
    char desc[192];
    snprintf(desc, sizeof(desc),
             "contrastive record: label=%s buffer=%u", l, buffer_count);
    /* Normalize buffer fill to [0,1] with 1000 sample ceiling. */
    float fill = (float)buffer_count / 1000.0f;
    if (fill > 1.0f) fill = 1.0f;
    w13_emit_linked(brain, name, desc, "cog_contrastive_self", fill);
    w13_store_meta_f(brain, "cog_contrastive_self", "buffer_fill", fill);
}

float wave13_contrastive_query_buffer_bias(struct brain_struct* brain) {
    float v = w13_read_meta_f(brain, "cog_contrastive_self",
                              "buffer_fill", -1.0f);
    return (v >= 0.0f) ? v : 0.5f;
}

/* Suppress unused bridge-boilerplate helpers. */
__attribute__((unused))
static void wave13_metacog_kg_suppress_unused(void) {
    (void)wave13_metacog_kg_set_health_agent;
    (void)wave13_metacog_kg_heartbeat;
}
