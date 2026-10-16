/**
 * @file nimcp_w9kg_events.c
 * @brief Wave W9-kg implementation: shared KG helpers for the cognitive-
 *        control family (attention / working memory / executive /
 *        introspection / self_model / global_workspace /
 *        self_awareness_extended / autobiographical_memory).
 *
 * Pattern lifted from:
 *   - `src/cognitive/memory/nimcp_memory_kg_events.c` (W6)
 *   - `src/cognitive/world_model/nimcp_world_model_kg_events.c` (W8)
 *
 * Admin-token protocol per
 * `docs/claude/kg-node-naming-registry.md` §7: elevate, write, restore READ.
 *
 * @date 2026-04-24
 */

#include "cognitive/executive/nimcp_w9kg_events.h"
#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/nimcp_brain_kg.h"
#include "utils/logging/nimcp_logging.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define LOG_MODULE "w9kg_events"

/* =========================================================================
 * Registered brain pointer (single-brain-per-process)
 * ========================================================================= */

static struct brain_struct* g_w9kg_registered_brain = NULL;

void w9kg_set_registered_brain(struct brain_struct* brain) {
    g_w9kg_registered_brain = brain;
}

struct brain_struct* w9kg_get_registered_brain(void) {
    return g_w9kg_registered_brain;
}

/* =========================================================================
 * Internal helpers
 * ========================================================================= */

static uint64_t ts_us(void) {
    return (uint64_t)time(NULL) * 1000000ULL;
}

static brain_kg_node_id_t add_or_get_node(
    brain_kg_t* kg,
    const char* name,
    brain_kg_node_type_t type,
    const char* description
) {
    brain_kg_node_id_t existing = brain_kg_find_node(kg, name);
    if (existing != BRAIN_KG_INVALID_NODE) return existing;
    return brain_kg_add_node(kg, name, type, description);
}

/**
 * @brief Emit a timestamped event node linked back to its module root.
 *        Self-elevates admin, writes node + edge, restores READ. Null-safe.
 */
static void emit_event_linked(
    struct brain_struct* brain,
    const char* event_name,
    const char* description,
    const char* parent_name,
    float edge_weight
) {
    if (!brain) return;
    if (!brain->internal_kg_enabled) return;
    if (!brain->internal_kg) return;
    if (!event_name) return;

    brain_kg_t* kg = brain->internal_kg;
    const uint64_t token = brain->internal_kg_admin_token;

    if (brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_ADMIN, token) != 0) {
        return;
    }

    brain_kg_node_id_t ev = brain_kg_add_node(
        kg, event_name, BRAIN_KG_NODE_COGNITIVE,
        description ? description : "w9kg runtime event"
    );

    if (ev != BRAIN_KG_INVALID_NODE && parent_name && *parent_name) {
        brain_kg_node_id_t parent = brain_kg_find_node(kg, parent_name);
        if (parent != BRAIN_KG_INVALID_NODE) {
            brain_kg_add_edge(kg, parent, ev,
                BRAIN_KG_EDGE_SENDS_TO, "produced_by", edge_weight);
        }
    }

    (void)brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_READ, 0);
}

/**
 * @brief Count nodes whose name starts with `prefix`. Used by read-paths.
 */
static uint32_t count_nodes_prefix(brain_kg_t* kg, const char* prefix) {
    if (!kg || !prefix) return 0;
    brain_kg_node_list_t* list = brain_kg_search_nodes(kg, prefix);
    uint32_t count = list ? list->count : 0;
    if (list) brain_kg_node_list_destroy(list);
    return count;
}

/* =========================================================================
 * Structural init
 * ========================================================================= */

static const char* const W9KG_ROOT_NAMES[] = {
    "cog_attention",
    "cog_working_memory",
    "cog_executive",
    "cog_introspection",
    "cog_self_model",
    "cog_global_workspace",
    "cog_self_awareness_extended",
    "cog_autobiographical_memory",
};

static const char* const W9KG_ROOT_DESCRIPTIONS[] = {
    "Multihead attention (cortical-column attention + thalamic gating)",
    "Working memory store (bounded capacity + priority eviction)",
    "Executive controller (task switching + goal coordination)",
    "Introspection (self-reporting + pattern registry)",
    "Self-model (capability / trait self-representation)",
    "Global workspace (cognitive module competition + broadcast)",
    "Self-awareness extended (metacognition + narrative + agency)",
    "Autobiographical memory (episodic self-memory store)",
};

#define W9KG_ROOT_COUNT \
    (sizeof(W9KG_ROOT_NAMES) / sizeof(W9KG_ROOT_NAMES[0]))

int w9kg_init_roots(struct brain_struct* brain) {
    if (!brain) return -1;
    if (!brain->internal_kg_enabled) return 0;
    if (!brain->internal_kg) return -1;

    brain_kg_t* kg = brain->internal_kg;
    const uint64_t token = brain->internal_kg_admin_token;

    if (brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_ADMIN, token) != 0) {
        LOG_WARN("[%s] Failed to elevate access for root init", LOG_MODULE);
        return -1;
    }

    brain_kg_node_id_t cog_umbrella = add_or_get_node(
        kg, "cognitive", BRAIN_KG_NODE_COGNITIVE,
        "Cognitive subsystems umbrella node"
    );

    int roots_created = 0;
    for (size_t i = 0; i < W9KG_ROOT_COUNT; i++) {
        brain_kg_node_id_t root = add_or_get_node(
            kg, W9KG_ROOT_NAMES[i], BRAIN_KG_NODE_COGNITIVE,
            W9KG_ROOT_DESCRIPTIONS[i]
        );
        if (root == BRAIN_KG_INVALID_NODE) continue;
        roots_created++;

        if (cog_umbrella != BRAIN_KG_INVALID_NODE) {
            brain_kg_add_edge(kg, cog_umbrella, root,
                BRAIN_KG_EDGE_INTEGRATES_WITH, "contains", 1.0f);
        }
    }

    (void)brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_READ, 0);

    w9kg_set_registered_brain(brain);

    LOG_INFO("[%s] W9-kg roots initialized (%d/%zu)",
             LOG_MODULE, roots_created, W9KG_ROOT_COUNT);
    return 0;
}

/* =========================================================================
 * WRITE-path
 * ========================================================================= */

/* --- attention --- */

void w9kg_emit_attention_salience(struct brain_struct* brain,
                                  uint32_t head_idx, float salience) {
    char name[160];
    snprintf(name, sizeof(name),
             "cog_attention_event_salience_%u_%" PRIu64,
             head_idx, ts_us());
    char desc[160];
    snprintf(desc, sizeof(desc),
             "attention salience head=%u score=%.3f",
             head_idx, (double)salience);
    emit_event_linked(brain, name, desc, "cog_attention",
                      salience < 0.0f ? -salience : salience);
}

void w9kg_emit_attention_focus_shift(struct brain_struct* brain,
                                     uint32_t from_module,
                                     uint32_t to_module,
                                     float strength) {
    char name[160];
    snprintf(name, sizeof(name),
             "cog_attention_event_shift_%u_%u_%" PRIu64,
             from_module, to_module, ts_us());
    char desc[192];
    snprintf(desc, sizeof(desc),
             "attention shift from=%u to=%u strength=%.3f",
             from_module, to_module, (double)strength);
    emit_event_linked(brain, name, desc, "cog_attention", strength);
}

/* --- working memory --- */

void w9kg_emit_wm_item_stored(struct brain_struct* brain,
                              uint64_t item_id, float priority) {
    char name[160];
    snprintf(name, sizeof(name),
             "cog_working_memory_event_store_%" PRIu64 "_%" PRIu64,
             item_id, ts_us());
    char desc[160];
    snprintf(desc, sizeof(desc),
             "WM item stored id=%" PRIu64 " priority=%.3f",
             item_id, (double)priority);
    emit_event_linked(brain, name, desc, "cog_working_memory", priority);
}

void w9kg_emit_wm_item_evicted(struct brain_struct* brain, uint64_t item_id) {
    char name[160];
    snprintf(name, sizeof(name),
             "cog_working_memory_event_evict_%" PRIu64 "_%" PRIu64,
             item_id, ts_us());
    char desc[128];
    snprintf(desc, sizeof(desc),
             "WM item evicted id=%" PRIu64, item_id);
    emit_event_linked(brain, name, desc, "cog_working_memory", 1.0f);
}

/* --- executive --- */

void w9kg_emit_executive_decision(struct brain_struct* brain,
                                  uint64_t decision_id, float confidence) {
    char name[160];
    snprintf(name, sizeof(name),
             "cog_executive_event_decision_%" PRIu64 "_%" PRIu64,
             decision_id, ts_us());
    char desc[160];
    snprintf(desc, sizeof(desc),
             "executive decision id=%" PRIu64 " confidence=%.3f",
             decision_id, (double)confidence);
    emit_event_linked(brain, name, desc, "cog_executive", confidence);
}

void w9kg_emit_executive_task_switch(struct brain_struct* brain,
                                     uint32_t from_task, uint32_t to_task) {
    char name[160];
    snprintf(name, sizeof(name),
             "cog_executive_event_task_switch_%u_%u_%" PRIu64,
             from_task, to_task, ts_us());
    char desc[160];
    snprintf(desc, sizeof(desc),
             "executive task switch from=%u to=%u",
             from_task, to_task);
    emit_event_linked(brain, name, desc, "cog_executive", 1.0f);
}

/* --- introspection --- */

void w9kg_emit_introspection_report(struct brain_struct* brain,
                                    uint64_t report_id, float health_score) {
    char name[160];
    snprintf(name, sizeof(name),
             "cog_introspection_event_report_%" PRIu64 "_%" PRIu64,
             report_id, ts_us());
    char desc[160];
    snprintf(desc, sizeof(desc),
             "introspection report id=%" PRIu64 " health=%.3f",
             report_id, (double)health_score);
    emit_event_linked(brain, name, desc, "cog_introspection", health_score);
}

/* --- self_model --- */

void w9kg_emit_self_model_update(struct brain_struct* brain,
                                 uint64_t update_id, float drift) {
    char name[160];
    snprintf(name, sizeof(name),
             "cog_self_model_event_update_%" PRIu64 "_%" PRIu64,
             update_id, ts_us());
    char desc[160];
    snprintf(desc, sizeof(desc),
             "self_model update id=%" PRIu64 " drift=%.3f",
             update_id, (double)drift);
    emit_event_linked(brain, name, desc, "cog_self_model",
                      drift < 0.0f ? -drift : drift);
}

/* --- global workspace --- */

void w9kg_emit_gws_broadcast(struct brain_struct* brain,
                             uint32_t winner_module, float strength,
                             uint32_t broadcast_id) {
    char name[160];
    snprintf(name, sizeof(name),
             "cog_global_workspace_event_broadcast_%u_%u_%" PRIu64,
             broadcast_id, winner_module, ts_us());
    char desc[192];
    snprintf(desc, sizeof(desc),
             "GWS broadcast id=%u winner=%u strength=%.3f",
             broadcast_id, winner_module, (double)strength);
    emit_event_linked(brain, name, desc, "cog_global_workspace", strength);
}

void w9kg_emit_gws_competition(struct brain_struct* brain,
                               uint32_t num_competitors) {
    char name[160];
    snprintf(name, sizeof(name),
             "cog_global_workspace_event_competition_%u_%" PRIu64,
             num_competitors, ts_us());
    char desc[160];
    snprintf(desc, sizeof(desc),
             "GWS competition competitors=%u", num_competitors);
    emit_event_linked(brain, name, desc, "cog_global_workspace",
                      (num_competitors > 0) ? 1.0f : 0.1f);
}

/* --- self_awareness_extended --- */

void w9kg_emit_self_awareness_reflection(struct brain_struct* brain,
                                         uint32_t reflection_id,
                                         float confidence) {
    char name[160];
    snprintf(name, sizeof(name),
             "cog_self_awareness_extended_event_reflection_%u_%" PRIu64,
             reflection_id, ts_us());
    char desc[192];
    snprintf(desc, sizeof(desc),
             "self_awareness reflection id=%u confidence=%.3f",
             reflection_id, (double)confidence);
    emit_event_linked(brain, name, desc, "cog_self_awareness_extended",
                      confidence);
}

/* --- autobiographical memory --- */

void w9kg_emit_autobio_stored(struct brain_struct* brain,
                              uint64_t memory_id, float importance) {
    char name[160];
    snprintf(name, sizeof(name),
             "cog_autobiographical_memory_event_store_%" PRIu64 "_%" PRIu64,
             memory_id, ts_us());
    char desc[192];
    snprintf(desc, sizeof(desc),
             "autobio memory stored id=%" PRIu64 " importance=%.3f",
             memory_id, (double)importance);
    emit_event_linked(brain, name, desc, "cog_autobiographical_memory",
                      importance);
}

void w9kg_emit_autobio_retrieved(struct brain_struct* brain,
                                 uint64_t memory_id) {
    char name[160];
    snprintf(name, sizeof(name),
             "cog_autobiographical_memory_event_retrieve_%" PRIu64
             "_%" PRIu64, memory_id, ts_us());
    char desc[128];
    snprintf(desc, sizeof(desc),
             "autobio memory retrieved id=%" PRIu64, memory_id);
    emit_event_linked(brain, name, desc,
                      "cog_autobiographical_memory", 1.0f);
}

/* =========================================================================
 * READ-path
 * ========================================================================= */

uint32_t w9kg_query_attention_salience_count(struct brain_struct* brain) {
    if (!brain || !brain->internal_kg_enabled || !brain->internal_kg) {
        return 0;
    }
    return count_nodes_prefix(brain->internal_kg,
                              "cog_attention_event_salience_");
}

bool w9kg_query_wm_has_semantic(struct brain_struct* brain,
                                const char* concept_name) {
    if (!brain || !brain->internal_kg_enabled || !brain->internal_kg) {
        return false;
    }
    if (!concept_name || !*concept_name) return false;
    brain_kg_node_list_t* list =
        brain_kg_search_nodes(brain->internal_kg, concept_name);
    bool found = (list && list->count > 0);
    if (list) brain_kg_node_list_destroy(list);
    return found;
}

uint32_t w9kg_query_executive_ethics_events(struct brain_struct* brain) {
    if (!brain || !brain->internal_kg_enabled || !brain->internal_kg) {
        return 0;
    }
    /* Ethics events share common prefix patterns from W11/W6/W8.
     * Count any node whose name mentions "ethics". */
    brain_kg_node_list_t* list =
        brain_kg_search_nodes(brain->internal_kg, "ethics");
    uint32_t count = list ? list->count : 0;
    if (list) brain_kg_node_list_destroy(list);
    return count;
}

uint32_t w9kg_query_introspection_wellbeing(struct brain_struct* brain) {
    if (!brain || !brain->internal_kg_enabled || !brain->internal_kg) {
        return 0;
    }
    brain_kg_node_list_t* list =
        brain_kg_search_nodes(brain->internal_kg, "wellbeing");
    uint32_t count = list ? list->count : 0;
    if (list) brain_kg_node_list_destroy(list);
    return count;
}

uint32_t w9kg_query_self_model_autobio(struct brain_struct* brain) {
    if (!brain || !brain->internal_kg_enabled || !brain->internal_kg) {
        return 0;
    }
    return count_nodes_prefix(brain->internal_kg,
                              "cog_autobiographical_memory_event_");
}

uint32_t w9kg_query_gws_winners_by_salience(struct brain_struct* brain) {
    if (!brain || !brain->internal_kg_enabled || !brain->internal_kg) {
        return 0;
    }
    brain_kg_node_id_t root =
        brain_kg_find_node(brain->internal_kg, "cog_global_workspace");
    if (root == BRAIN_KG_INVALID_NODE) return 0;
    brain_kg_edge_list_t* out =
        brain_kg_get_outgoing(brain->internal_kg, root);
    uint32_t count = out ? out->count : 0;
    if (out) brain_kg_edge_list_destroy(out);
    return count;
}

uint32_t w9kg_query_self_awareness_events(struct brain_struct* brain) {
    if (!brain || !brain->internal_kg_enabled || !brain->internal_kg) {
        return 0;
    }
    return count_nodes_prefix(brain->internal_kg,
                              "cog_introspection_event_");
}

uint32_t w9kg_query_autobio_event_count(struct brain_struct* brain) {
    if (!brain || !brain->internal_kg_enabled || !brain->internal_kg) {
        return 0;
    }
    return count_nodes_prefix(brain->internal_kg,
                              "cog_autobiographical_memory_event_");
}
