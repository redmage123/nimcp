/**
 * @file nimcp_world_model_kg_events.c
 * @brief Wave W8: KG event emitters for the world-model family.
 *
 * Mirrors the W6 memory_kg_events pattern, but for the
 * world-model / imagination / FEP / predictive / salience family.
 *
 * Registry §7 admin-elevate / write / restore-READ protocol is used for
 * every write; all entry points are NULL-safe so modules without a
 * brain pointer can fall back to the registered-brain pattern without
 * crashing unit tests that create bare subsystems.
 *
 * @date 2026-04-24
 * @version 1.0.0 (W8)
 */

#include "cognitive/world_model/nimcp_world_model_kg_events.h"
#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/nimcp_brain_kg.h"
#include "utils/logging/nimcp_logging.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define LOG_MODULE "world_model_kg_events"

/* =========================================================================
 * Registered brain pointer
 * ========================================================================= */

static struct brain_struct* g_registered_brain = NULL;

void world_model_kg_events_set_registered_brain(struct brain_struct* brain) {
    g_registered_brain = brain;
}

struct brain_struct* world_model_kg_events_get_registered_brain(void) {
    return g_registered_brain;
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
 * @brief Emit a timestamped event node linked back to a parent module root.
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
        description ? description : "world-model runtime event"
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

/* =========================================================================
 * Structural init
 * ========================================================================= */

static const char* const W8_ROOT_NAMES[] = {
    "cog_omni_wm_world_model",
    "cog_physics_intuitive",
    "cog_physics_scene_graph",
    "cog_physics_entity_tracker",
    "cog_physics_world_simulator",
    "cog_fep_free_energy",
    "cog_predictive",
    "cog_salience",
    "cog_imagination_workspace",
    "cog_jepa_bridges",
};

static const char* const W8_ROOT_DESCRIPTIONS[] = {
    "Omni world model (RSSM, DreamerV3-style dynamics)",
    "Intuitive rigid-body physics engine (Spelke core knowledge)",
    "Scene graph (support, containment, proximity relations)",
    "Entity tracker (persistent object identity, permanence)",
    "Unified world simulator (physics + chemistry + biology)",
    "Free Energy Principle (variational surprise minimization)",
    "Hierarchical predictive coding (prediction error layers)",
    "Salience evaluator (attention-weighted feature scoring)",
    "Imagination workspace (scratch buffer for mental simulation)",
    "JEPA brain bridges (latent prediction across substrates)",
};

#define W8_ROOT_COUNT (sizeof(W8_ROOT_NAMES) / sizeof(W8_ROOT_NAMES[0]))

int world_model_kg_init_roots(struct brain_struct* brain) {
    if (!brain) return -1;
    if (!brain->internal_kg_enabled) return 0;
    if (!brain->internal_kg) return -1;

    brain_kg_t* kg = brain->internal_kg;
    const uint64_t token = brain->internal_kg_admin_token;

    if (brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_ADMIN, token) != 0) {
        LOG_WARN("[%s] Failed to elevate access for root init", LOG_MODULE);
        return -1;
    }

    /* Cognitive umbrella (W6 created it; add_or_get keeps us safe) */
    brain_kg_node_id_t cog_umbrella = add_or_get_node(
        kg, "cognitive", BRAIN_KG_NODE_COGNITIVE,
        "Cognitive subsystems umbrella node"
    );

    int roots_created = 0;
    for (size_t i = 0; i < W8_ROOT_COUNT; i++) {
        brain_kg_node_id_t root = add_or_get_node(
            kg, W8_ROOT_NAMES[i], BRAIN_KG_NODE_COGNITIVE,
            W8_ROOT_DESCRIPTIONS[i]
        );
        if (root == BRAIN_KG_INVALID_NODE) continue;
        roots_created++;

        if (cog_umbrella != BRAIN_KG_INVALID_NODE) {
            brain_kg_add_edge(kg, cog_umbrella, root,
                BRAIN_KG_EDGE_INTEGRATES_WITH, "contains", 1.0f);
        }
    }

    /* Cross-edges to already-WIRED nodes from earlier waves (best-effort). */
    brain_kg_node_id_t imag = brain_kg_find_node(kg, "imagination_engine");
    brain_kg_node_id_t jepa = brain_kg_find_node(kg, "jepa_predictor");
    brain_kg_node_id_t wm_root = brain_kg_find_node(kg, "cog_omni_wm_world_model");
    brain_kg_node_id_t imag_ws = brain_kg_find_node(kg, "cog_imagination_workspace");
    brain_kg_node_id_t fep_root = brain_kg_find_node(kg, "cog_fep_free_energy");
    brain_kg_node_id_t pred_root = brain_kg_find_node(kg, "cog_predictive");
    brain_kg_node_id_t jepa_root = brain_kg_find_node(kg, "cog_jepa_bridges");

    if (imag != BRAIN_KG_INVALID_NODE && imag_ws != BRAIN_KG_INVALID_NODE) {
        brain_kg_add_edge(kg, imag, imag_ws,
            BRAIN_KG_EDGE_SENDS_TO, "uses_as_scratch", 0.9f);
    }
    if (jepa != BRAIN_KG_INVALID_NODE && jepa_root != BRAIN_KG_INVALID_NODE) {
        brain_kg_add_edge(kg, jepa_root, jepa,
            BRAIN_KG_EDGE_INTEGRATES_WITH, "drives", 0.8f);
    }
    if (wm_root != BRAIN_KG_INVALID_NODE && fep_root != BRAIN_KG_INVALID_NODE) {
        brain_kg_add_edge(kg, wm_root, fep_root,
            BRAIN_KG_EDGE_SENDS_TO, "surprise_target", 0.7f);
    }
    if (pred_root != BRAIN_KG_INVALID_NODE && fep_root != BRAIN_KG_INVALID_NODE) {
        brain_kg_add_edge(kg, pred_root, fep_root,
            BRAIN_KG_EDGE_INTEGRATES_WITH, "minimizes", 0.9f);
    }

    (void)brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_READ, 0);

    world_model_kg_events_set_registered_brain(brain);

    LOG_INFO("[%s] W8 roots initialized (%d/%zu)",
             LOG_MODULE, roots_created, W8_ROOT_COUNT);
    return 0;
}

/* =========================================================================
 * WRITE path — emitters
 * ========================================================================= */

void world_model_kg_emit_wm_step(struct brain_struct* brain,
                                 uint64_t step_id,
                                 float reward, float surprise) {
    char name[160];
    snprintf(name, sizeof(name),
             "cog_omni_wm_world_model_event_step_%" PRIu64 "_%" PRIu64,
             step_id, ts_us());
    char desc[192];
    snprintf(desc, sizeof(desc),
             "omni_wm step=%" PRIu64 " reward=%.3f surprise=%.3f",
             step_id, (double)reward, (double)surprise);
    float w = surprise;
    if (w < 0) w = -w;
    emit_event_linked(brain, name, desc,
                      "cog_omni_wm_world_model", w);
}

void world_model_kg_emit_physics_step(struct brain_struct* brain,
                                      uint64_t step_count,
                                      uint32_t active_objects,
                                      uint32_t contacts,
                                      float energy_drift) {
    char name[160];
    snprintf(name, sizeof(name),
             "cog_physics_intuitive_event_step_%" PRIu64 "_%" PRIu64,
             step_count, ts_us());
    char desc[224];
    snprintf(desc, sizeof(desc),
             "physics step=%" PRIu64 " objs=%u contacts=%u drift=%.4f",
             step_count, active_objects, contacts, (double)energy_drift);
    float w = energy_drift < 0 ? -energy_drift : energy_drift;
    if (w > 1.0f) w = 1.0f;
    emit_event_linked(brain, name, desc,
                      "cog_physics_intuitive", w);
}

void world_model_kg_emit_scene_rebuild(struct brain_struct* brain,
                                       uint32_t num_relations,
                                       uint64_t rebuild_count) {
    char name[160];
    snprintf(name, sizeof(name),
             "cog_physics_scene_graph_event_rebuild_%" PRIu64 "_%" PRIu64,
             rebuild_count, ts_us());
    char desc[192];
    snprintf(desc, sizeof(desc),
             "scene rebuild #%" PRIu64 " relations=%u",
             rebuild_count, num_relations);
    emit_event_linked(brain, name, desc,
                      "cog_physics_scene_graph",
                      num_relations > 0 ? 1.0f : 0.1f);
}

void world_model_kg_emit_entity_event(struct brain_struct* brain,
                                      uint32_t entity_id,
                                      int event_kind,
                                      float confidence) {
    const char* kind_name;
    switch (event_kind) {
        case 0: kind_name = "spawn";    break;
        case 1: kind_name = "occlude";  break;
        case 2: kind_name = "reappear"; break;
        case 3: kind_name = "lost";     break;
        default: kind_name = "unknown"; break;
    }
    char name[176];
    snprintf(name, sizeof(name),
             "cog_physics_entity_tracker_event_%s_%u_%" PRIu64,
             kind_name, entity_id, ts_us());
    char desc[192];
    snprintf(desc, sizeof(desc),
             "entity %s id=%u confidence=%.3f",
             kind_name, entity_id, (double)confidence);
    emit_event_linked(brain, name, desc,
                      "cog_physics_entity_tracker", confidence);
}

void world_model_kg_emit_sim_step(struct brain_struct* brain,
                                  uint64_t step_count,
                                  float dt, uint32_t num_engines) {
    char name[160];
    snprintf(name, sizeof(name),
             "cog_physics_world_simulator_event_step_%" PRIu64 "_%" PRIu64,
             step_count, ts_us());
    char desc[192];
    snprintf(desc, sizeof(desc),
             "world sim step=%" PRIu64 " dt=%.4f engines=%u",
             step_count, (double)dt, num_engines);
    emit_event_linked(brain, name, desc,
                      "cog_physics_world_simulator", 1.0f);
}

void world_model_kg_emit_surprise(struct brain_struct* brain,
                                  float free_energy,
                                  float surprise,
                                  float precision) {
    /* For high-surprise events use the canonical suffix that
     * `world_model_kg_has_recent_surprise` scans for. */
    float mag = surprise < 0 ? -surprise : surprise;
    char name[160];
    if (mag > 2.0f) {
        snprintf(name, sizeof(name),
                 "cog_fep_free_energy_event_high_surprise_%" PRIu64, ts_us());
    } else {
        snprintf(name, sizeof(name),
                 "cog_fep_free_energy_event_step_%" PRIu64, ts_us());
    }
    char desc[192];
    snprintf(desc, sizeof(desc),
             "FEP F=%.3f surprise=%.3f precision=%.3f",
             (double)free_energy, (double)surprise, (double)precision);
    float w = mag > 1.0f ? 1.0f : mag;
    emit_event_linked(brain, name, desc,
                      "cog_fep_free_energy", w);
}

void world_model_kg_emit_prediction_error(struct brain_struct* brain,
                                          uint32_t layer,
                                          float error_norm,
                                          float free_energy) {
    char name[160];
    if (error_norm > 5.0f) {
        snprintf(name, sizeof(name),
                 "cog_predictive_event_high_error_L%u_%" PRIu64,
                 layer, ts_us());
    } else {
        snprintf(name, sizeof(name),
                 "cog_predictive_event_step_L%u_%" PRIu64,
                 layer, ts_us());
    }
    char desc[192];
    snprintf(desc, sizeof(desc),
             "predictive layer=%u err=%.3f F=%.3f",
             layer, (double)error_norm, (double)free_energy);
    float w = error_norm;
    if (w > 1.0f) w = 1.0f;
    emit_event_linked(brain, name, desc,
                      "cog_predictive", w);
}

void world_model_kg_emit_salience_transition(struct brain_struct* brain,
                                             int old_level,
                                             int new_level,
                                             float combined_score) {
    char name[160];
    snprintf(name, sizeof(name),
             "cog_salience_event_transition_%d_to_%d_%" PRIu64,
             old_level, new_level, ts_us());
    char desc[192];
    snprintf(desc, sizeof(desc),
             "salience %d -> %d score=%.3f",
             old_level, new_level, (double)combined_score);
    emit_event_linked(brain, name, desc,
                      "cog_salience", combined_score);
}

void world_model_kg_emit_workspace_scenario(struct brain_struct* brain,
                                            uint64_t scenario_id,
                                            uint32_t num_steps,
                                            float vividness) {
    char name[160];
    snprintf(name, sizeof(name),
             "cog_imagination_workspace_event_scenario_%" PRIu64
             "_%" PRIu64, scenario_id, ts_us());
    char desc[192];
    snprintf(desc, sizeof(desc),
             "workspace scenario id=%" PRIu64 " steps=%u vividness=%.3f",
             scenario_id, num_steps, (double)vividness);
    emit_event_linked(brain, name, desc,
                      "cog_imagination_workspace", vividness);
}

void world_model_kg_emit_jepa_tick(struct brain_struct* brain,
                                   uint32_t bridges_active,
                                   float average_loss) {
    char name[160];
    snprintf(name, sizeof(name),
             "cog_jepa_bridges_event_tick_%" PRIu64, ts_us());
    char desc[192];
    snprintf(desc, sizeof(desc),
             "jepa tick bridges=%u avg_loss=%.4f",
             bridges_active, (double)average_loss);
    float w = average_loss;
    if (w < 0) w = -w;
    if (w > 1.0f) w = 1.0f;
    emit_event_linked(brain, name, desc,
                      "cog_jepa_bridges", w);
}

/* =========================================================================
 * READ path
 * ========================================================================= */

uint32_t world_model_kg_root_edge_count(struct brain_struct* brain,
                                        const char* root_name) {
    if (!brain || !brain->internal_kg_enabled || !brain->internal_kg) {
        return 0;
    }
    if (!root_name || !*root_name) return 0;

    brain_kg_node_id_t root =
        brain_kg_find_node(brain->internal_kg, root_name);
    if (root == BRAIN_KG_INVALID_NODE) return 0;

    brain_kg_edge_list_t* out =
        brain_kg_get_outgoing(brain->internal_kg, root);
    uint32_t count = out ? out->count : 0;
    if (out) brain_kg_edge_list_destroy(out);
    return count;
}

bool world_model_kg_has_recent_surprise(struct brain_struct* brain) {
    if (!brain || !brain->internal_kg_enabled || !brain->internal_kg) {
        return false;
    }
    brain_kg_node_list_t* list_s =
        brain_kg_search_nodes(brain->internal_kg,
                              "cog_fep_free_energy_event_high_surprise");
    bool found_s = (list_s && list_s->count > 0);
    if (list_s) brain_kg_node_list_destroy(list_s);
    if (found_s) return true;

    brain_kg_node_list_t* list_p =
        brain_kg_search_nodes(brain->internal_kg,
                              "cog_predictive_event_high_error");
    bool found_p = (list_p && list_p->count > 0);
    if (list_p) brain_kg_node_list_destroy(list_p);
    return found_p;
}

bool world_model_kg_has_partner(struct brain_struct* brain, const char* name) {
    if (!brain || !brain->internal_kg_enabled || !brain->internal_kg) {
        return false;
    }
    if (!name || !*name) return false;
    return brain_kg_find_node(brain->internal_kg, name) != BRAIN_KG_INVALID_NODE;
}
