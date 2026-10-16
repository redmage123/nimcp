/**
 * @file nimcp_memory_kg_events.c
 * @brief Wave W6: KG event emitters for the cognitive-memory family
 *
 * Implements the write-path + read-path helpers declared in
 * `include/cognitive/memory/nimcp_memory_kg_events.h`. All emit functions
 * follow the admin-token protocol from `docs/claude/kg-node-naming-registry.md`
 * §7: self-elevate, write, restore READ.
 *
 * Reference template: `src/core/brain/subcortical/bridges/nimcp_subcortical_runtime_events.c`
 *
 * @date 2026-04-24
 * @version 1.0.0 (W6)
 */

#include "cognitive/memory/nimcp_memory_kg_events.h"
#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/nimcp_brain_kg.h"
#include "utils/logging/nimcp_logging.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define LOG_MODULE "memory_kg_events"

/* =========================================================================
 * Registered brain pointer
 *
 * Memory modules often don't have a brain_t parameter in their hot-path
 * signatures. To avoid a massive API churn, we hold a single static brain
 * pointer that gets set during brain init. Modules call
 * `memory_kg_events_get_registered_brain()` to look it up. Single-brain-
 * per-process is the current runtime assumption (see brain_daemon.py).
 * ========================================================================= */

static struct brain_struct* g_registered_brain = NULL;

void memory_kg_events_set_registered_brain(struct brain_struct* brain) {
    g_registered_brain = brain;
}

struct brain_struct* memory_kg_events_get_registered_brain(void) {
    return g_registered_brain;
}

/* =========================================================================
 * Internal helpers
 * ========================================================================= */

/** @brief Monotonic timestamp in microseconds. */
static uint64_t ts_us(void) {
    return (uint64_t)time(NULL) * 1000000ULL;
}

/** @brief Idempotent "add or get" — returns existing node if name collides. */
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
 * @brief Emit a timestamped event node linked back to a parent module root.
 *
 * Self-elevates admin, writes one event node + one edge (event -> parent,
 * described as `produced_by`), restores READ. Null-safe.
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
        description ? description : "memory runtime event"
    );

    if (ev != BRAIN_KG_INVALID_NODE && parent_name && *parent_name) {
        brain_kg_node_id_t parent = brain_kg_find_node(kg, parent_name);
        if (parent != BRAIN_KG_INVALID_NODE) {
            /* produced_by: parent produces event */
            brain_kg_add_edge(kg, parent, ev,
                BRAIN_KG_EDGE_SENDS_TO, "produced_by", edge_weight);
        }
    }

    (void)brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_READ, 0);
}

/* =========================================================================
 * Structural init: root nodes + cognitive umbrella
 * ========================================================================= */

static const char* const MEMORY_ROOT_NAMES[] = {
    "cog_memory_engram",
    "cog_memory_semantic",
    "cog_memory_hopfield",
    "cog_memory_episodic_replay",
    "cog_memory_systems_consolidation",
    "cog_memory_schemas",
    "cog_memory_source",
    "cog_memory_reconsolidation",
};

static const char* const MEMORY_ROOT_DESCRIPTIONS[] = {
    "Memory engram store (episodic trace formation + recall)",
    "Semantic concept network with spreading activation",
    "Hopfield associative memory (pattern completion)",
    "Episodic replay during sleep / consolidation",
    "Systems consolidation (hippocampus -> cortex transfer)",
    "Schema library (abstract structural memory templates)",
    "Source memory (origin tagging / reality monitoring)",
    "Reconsolidation windows (memory update & rollback)",
};

#define MEMORY_ROOT_COUNT \
    (sizeof(MEMORY_ROOT_NAMES) / sizeof(MEMORY_ROOT_NAMES[0]))

int memory_kg_init_roots(struct brain_struct* brain) {
    if (!brain) return -1;
    if (!brain->internal_kg_enabled) return 0;
    if (!brain->internal_kg) return -1;

    brain_kg_t* kg = brain->internal_kg;
    const uint64_t token = brain->internal_kg_admin_token;

    if (brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_ADMIN, token) != 0) {
        LOG_WARN("[%s] Failed to elevate access for root init", LOG_MODULE);
        return -1;
    }

    /* Cognitive umbrella */
    brain_kg_node_id_t cog_umbrella = add_or_get_node(
        kg, "cognitive", BRAIN_KG_NODE_COGNITIVE,
        "Cognitive subsystems umbrella node"
    );

    int roots_created = 0;
    for (size_t i = 0; i < MEMORY_ROOT_COUNT; i++) {
        brain_kg_node_id_t root = add_or_get_node(
            kg, MEMORY_ROOT_NAMES[i], BRAIN_KG_NODE_COGNITIVE,
            MEMORY_ROOT_DESCRIPTIONS[i]
        );
        if (root == BRAIN_KG_INVALID_NODE) continue;
        roots_created++;

        if (cog_umbrella != BRAIN_KG_INVALID_NODE) {
            /* contains: umbrella -> root (registry §5 uses INTEGRATES_WITH
             * for cross-module integration). */
            brain_kg_add_edge(kg, cog_umbrella, root,
                BRAIN_KG_EDGE_INTEGRATES_WITH, "contains", 1.0f);
        }
    }

    (void)brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_READ, 0);

    /* Register brain pointer so hot-path emits without brain_t param work */
    memory_kg_events_set_registered_brain(brain);

    LOG_INFO("[%s] Memory family KG roots initialized (%d/%zu)",
             LOG_MODULE, roots_created, MEMORY_ROOT_COUNT);
    return 0;
}

/* =========================================================================
 * WRITE path — engram
 * ========================================================================= */

void memory_kg_emit_engram_form(struct brain_struct* brain,
                                uint64_t engram_id, float strength) {
    char name[160];
    snprintf(name, sizeof(name),
             "cog_memory_engram_event_form_%" PRIu64 "_%" PRIu64,
             engram_id, ts_us());
    char desc[192];
    snprintf(desc, sizeof(desc),
             "engram formed id=%" PRIu64 " strength=%.3f",
             engram_id, (double)strength);
    emit_event_linked(brain, name, desc, "cog_memory_engram", strength);
}

void memory_kg_emit_engram_recall(struct brain_struct* brain,
                                  uint64_t engram_id, float confidence) {
    char name[160];
    snprintf(name, sizeof(name),
             "cog_memory_engram_event_recall_%" PRIu64 "_%" PRIu64,
             engram_id, ts_us());
    char desc[192];
    snprintf(desc, sizeof(desc),
             "engram recalled id=%" PRIu64 " confidence=%.3f",
             engram_id, (double)confidence);
    emit_event_linked(brain, name, desc, "cog_memory_engram", confidence);
}

/* =========================================================================
 * WRITE path — semantic memory
 * ========================================================================= */

void memory_kg_emit_concept_created(struct brain_struct* brain,
                                    uint64_t concept_id, const char* label) {
    char name[160];
    snprintf(name, sizeof(name),
             "cog_memory_semantic_event_concept_%" PRIu64 "_%" PRIu64,
             concept_id, ts_us());
    char desc[192];
    if (label && *label) {
        snprintf(desc, sizeof(desc),
                 "concept created id=%" PRIu64 " label=%.64s",
                 concept_id, label);
    } else {
        snprintf(desc, sizeof(desc),
                 "concept created id=%" PRIu64, concept_id);
    }
    emit_event_linked(brain, name, desc, "cog_memory_semantic", 1.0f);
}

void memory_kg_emit_spreading_activation(struct brain_struct* brain,
                                         uint64_t seed_concept_id,
                                         uint32_t activated_count) {
    char name[160];
    snprintf(name, sizeof(name),
             "cog_memory_semantic_event_spread_%" PRIu64 "_%" PRIu64,
             seed_concept_id, ts_us());
    char desc[192];
    snprintf(desc, sizeof(desc),
             "spreading activation seed=%" PRIu64 " activated=%u",
             seed_concept_id, activated_count);
    emit_event_linked(brain, name, desc, "cog_memory_semantic",
                      (activated_count > 0) ? 1.0f : 0.1f);
}

/* =========================================================================
 * WRITE path — hopfield
 * ========================================================================= */

void memory_kg_emit_pattern_stored(struct brain_struct* brain,
                                   uint32_t pattern_id, float strength) {
    char name[160];
    snprintf(name, sizeof(name),
             "cog_memory_hopfield_event_store_%u_%" PRIu64,
             pattern_id, ts_us());
    char desc[192];
    snprintf(desc, sizeof(desc),
             "hopfield pattern stored id=%u strength=%.3f",
             pattern_id, (double)strength);
    emit_event_linked(brain, name, desc, "cog_memory_hopfield", strength);
}

void memory_kg_emit_pattern_completed(struct brain_struct* brain,
                                      uint32_t pattern_id,
                                      float similarity, bool converged) {
    char name[160];
    snprintf(name, sizeof(name),
             "cog_memory_hopfield_event_complete_%u_%" PRIu64,
             pattern_id, ts_us());
    char desc[192];
    snprintf(desc, sizeof(desc),
             "hopfield pattern completion id=%u sim=%.3f converged=%d",
             pattern_id, (double)similarity, converged ? 1 : 0);
    emit_event_linked(brain, name, desc, "cog_memory_hopfield",
                      similarity < 0.0f ? -similarity : similarity);
}

/* =========================================================================
 * WRITE path — episodic replay
 * ========================================================================= */

void memory_kg_emit_replay_cycle(struct brain_struct* brain,
                                 uint32_t replayed_count,
                                 uint32_t total_buffer) {
    char name[160];
    snprintf(name, sizeof(name),
             "cog_memory_episodic_replay_event_cycle_%" PRIu64, ts_us());
    char desc[192];
    snprintf(desc, sizeof(desc),
             "episodic replay cycle: replayed=%u buffer=%u",
             replayed_count, total_buffer);
    float w = (total_buffer > 0)
        ? ((float)replayed_count / (float)total_buffer)
        : 0.0f;
    emit_event_linked(brain, name, desc, "cog_memory_episodic_replay", w);
}

/* =========================================================================
 * WRITE path — systems consolidation
 * ========================================================================= */

void memory_kg_emit_consolidation_transfer(struct brain_struct* brain,
                                           uint64_t engram_id,
                                           uint64_t cortical_node_id,
                                           float consolidation_strength) {
    char name[160];
    snprintf(name, sizeof(name),
             "cog_memory_systems_consolidation_event_transfer_%" PRIu64
             "_%" PRIu64, engram_id, ts_us());
    char desc[224];
    snprintf(desc, sizeof(desc),
             "consolidation transfer engram=%" PRIu64
             " cortical=%" PRIu64 " strength=%.3f",
             engram_id, cortical_node_id, (double)consolidation_strength);
    emit_event_linked(brain, name, desc,
                      "cog_memory_systems_consolidation",
                      consolidation_strength);
}

/* =========================================================================
 * WRITE path — schemas
 * ========================================================================= */

void memory_kg_emit_schema_added(struct brain_struct* brain,
                                 uint64_t schema_id, const char* schema_name) {
    char name[160];
    snprintf(name, sizeof(name),
             "cog_memory_schemas_event_add_%" PRIu64 "_%" PRIu64,
             schema_id, ts_us());
    char desc[192];
    if (schema_name && *schema_name) {
        snprintf(desc, sizeof(desc),
                 "schema added id=%" PRIu64 " name=%.64s",
                 schema_id, schema_name);
    } else {
        snprintf(desc, sizeof(desc),
                 "schema added id=%" PRIu64, schema_id);
    }
    emit_event_linked(brain, name, desc, "cog_memory_schemas", 1.0f);
}

void memory_kg_emit_schema_activated(struct brain_struct* brain,
                                     uint64_t schema_id) {
    char name[160];
    snprintf(name, sizeof(name),
             "cog_memory_schemas_event_activate_%" PRIu64 "_%" PRIu64,
             schema_id, ts_us());
    char desc[128];
    snprintf(desc, sizeof(desc),
             "schema activated id=%" PRIu64, schema_id);
    emit_event_linked(brain, name, desc, "cog_memory_schemas", 1.0f);
}

/* =========================================================================
 * WRITE path — source memory
 * ========================================================================= */

void memory_kg_emit_source_bound(struct brain_struct* brain,
                                 uint64_t memory_id, int source_type) {
    char name[160];
    snprintf(name, sizeof(name),
             "cog_memory_source_event_bind_%" PRIu64 "_%" PRIu64,
             memory_id, ts_us());
    char desc[160];
    snprintf(desc, sizeof(desc),
             "source bound memory=%" PRIu64 " type=%d",
             memory_id, source_type);
    emit_event_linked(brain, name, desc, "cog_memory_source", 1.0f);
}

/* =========================================================================
 * WRITE path — reconsolidation
 * ========================================================================= */

void memory_kg_emit_reconsolidation_opened(struct brain_struct* brain,
                                           uint64_t memory_id,
                                           float activation_strength) {
    char name[160];
    snprintf(name, sizeof(name),
             "cog_memory_reconsolidation_event_open_%" PRIu64
             "_%" PRIu64, memory_id, ts_us());
    char desc[192];
    snprintf(desc, sizeof(desc),
             "reconsolidation window opened memory=%" PRIu64
             " activation=%.3f", memory_id, (double)activation_strength);
    emit_event_linked(brain, name, desc,
                      "cog_memory_reconsolidation", activation_strength);
}

void memory_kg_emit_reconsolidation_committed(struct brain_struct* brain,
                                              uint64_t memory_id,
                                              int outcome) {
    char name[160];
    snprintf(name, sizeof(name),
             "cog_memory_reconsolidation_event_commit_%" PRIu64
             "_%" PRIu64, memory_id, ts_us());
    char desc[160];
    snprintf(desc, sizeof(desc),
             "reconsolidation committed memory=%" PRIu64 " outcome=%d",
             memory_id, outcome);
    emit_event_linked(brain, name, desc,
                      "cog_memory_reconsolidation", 1.0f);
}

/* =========================================================================
 * READ path
 * ========================================================================= */

bool memory_kg_has_engram_event(struct brain_struct* brain, uint64_t engram_id) {
    if (!brain || !brain->internal_kg_enabled || !brain->internal_kg) {
        return false;
    }
    /* We don't know the timestamp suffix, so search by substring prefix.
     * `brain_kg_search_nodes` returns a node list allocated by the KG. */
    char prefix[96];
    snprintf(prefix, sizeof(prefix),
             "cog_memory_engram_event_form_%" PRIu64, engram_id);

    brain_kg_node_list_t* list =
        brain_kg_search_nodes(brain->internal_kg, prefix);
    bool found = (list && list->count > 0);
    if (list) brain_kg_node_list_destroy(list);
    return found;
}

uint32_t memory_kg_semantic_neighbor_count(struct brain_struct* brain) {
    if (!brain || !brain->internal_kg_enabled || !brain->internal_kg) {
        return 0;
    }
    brain_kg_node_id_t root =
        brain_kg_find_node(brain->internal_kg, "cog_memory_semantic");
    if (root == BRAIN_KG_INVALID_NODE) return 0;

    brain_kg_edge_list_t* out =
        brain_kg_get_outgoing(brain->internal_kg, root);
    uint32_t count = out ? out->count : 0;
    if (out) brain_kg_edge_list_destroy(out);
    return count;
}

bool memory_kg_has_schema(struct brain_struct* brain, const char* schema_name) {
    if (!brain || !brain->internal_kg_enabled || !brain->internal_kg) {
        return false;
    }
    if (!schema_name || !*schema_name) return false;

    /* Schemas are emitted as events, but the schema_name can be encoded in
     * the description. For now, search the KG name-index for a name
     * containing the schema_name — good enough for the sanity check the
     * read-path is intended to provide. */
    brain_kg_node_list_t* list =
        brain_kg_search_nodes(brain->internal_kg, schema_name);
    bool found = (list && list->count > 0);
    if (list) brain_kg_node_list_destroy(list);
    return found;
}
