//=============================================================================
// nimcp_w12_language_kg_events.c - Wave W12 Language / Communication KG Events
//=============================================================================
/**
 * @file nimcp_w12_language_kg_events.c
 * @brief Implementation of W12 language KG emitters.
 *
 * Mirrors `nimcp_w11_safety_kg_events.c` for layout:
 *   - NULL/disabled early-return
 *   - lazy structural-root creation on first emit
 *   - self-elevate to ADMIN via `brain->internal_kg_admin_token`
 *   - add_node + add_edge to the structural parent
 *   - restore READ access
 *
 * Rate-limiting: the `_tokenizer_event` emitter uses a static counter to
 * keep at most ~1 in 512 token-level events; other emitters run per-call
 * because they fire at utterance / refinement granularity, not per token.
 *
 * @date 2026-04-24
 */

#include "cognitive/language/nimcp_w12_language_kg_events.h"
#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/nimcp_brain_kg.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

BRIDGE_BOILERPLATE_MESH_ONLY(w12_language_kg_events, MESH_ADAPTER_CATEGORY_COGNITIVE)

/* ------------------------------------------------------------------------- *
 * Internal helpers                                                          *
 * ------------------------------------------------------------------------- */

/** @brief Microsecond timestamp (resolution-limited by time()). */
static uint64_t w12_ts_us(void) {
    return (uint64_t)time(NULL) * 1000000ULL;
}

/** @brief Return true iff brain has a usable, enabled internal KG. */
static bool w12_kg_ready(const struct brain_struct* brain) {
    if (!brain) return false;
    if (!brain->internal_kg_enabled) return false;
    if (!brain->internal_kg) return false;
    return true;
}

/**
 * @brief Emit a single event node + back-edge to a named structural parent.
 *
 * NULL/disabled safe. Self-elevates to ADMIN for the duration of the writes.
 * Back-edge uses BRAIN_KG_EDGE_SENDS_TO with description "produced_by"
 * (registry §5: parent -> event signals "parent produced this event").
 */
static void w12_emit_linked(
    struct brain_struct* brain,
    const char* event_name,
    const char* description,
    const char* parent_name,
    float edge_weight
) {
    if (!w12_kg_ready(brain)) return;
    if (!event_name) return;

    brain_kg_t* kg = brain->internal_kg;
    const uint64_t token = brain->internal_kg_admin_token;

    brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_ADMIN, token);

    brain_kg_node_id_t ev = brain_kg_add_node(kg, event_name,
        BRAIN_KG_NODE_COGNITIVE,
        description ? description : "language runtime event");

    if (ev != BRAIN_KG_INVALID_NODE && parent_name && *parent_name) {
        brain_kg_node_id_t parent = brain_kg_find_node(kg, parent_name);
        if (parent != BRAIN_KG_INVALID_NODE) {
            brain_kg_add_edge(kg, parent, ev,
                BRAIN_KG_EDGE_SENDS_TO, "produced_by", edge_weight);
        }
    }

    brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_READ, 0);
}

/** @brief Ensure one structural root exists; idempotent. Caller must hold ADMIN. */
static void w12_ensure_root(
    brain_kg_t* kg,
    const char* name,
    const char* description
) {
    if (brain_kg_find_node(kg, name) == BRAIN_KG_INVALID_NODE) {
        brain_kg_add_node(kg, name, BRAIN_KG_NODE_COGNITIVE,
                          description ? description : "W12 language root");
    }
}

/* ------------------------------------------------------------------------- *
 * Registered-brain table                                                    *
 * ------------------------------------------------------------------------- *
 * Language modules (emergent / inner speech / native / tokenizer) do not
 * hold a brain_t handle; the brain init path registers the current brain
 * here and the `_auto` wrappers below read from it.
 * Stored as a plain pointer with __atomic_* accessors for basic
 * safety in the presence of late re-registration.
 */
static struct brain_struct* s_w12_registered_brain = NULL;

void w12_language_kg_register_brain(struct brain_struct* brain) {
    __atomic_store_n(&s_w12_registered_brain, brain, __ATOMIC_RELAXED);
}

struct brain_struct* w12_language_kg_get_brain(void) {
    return __atomic_load_n(&s_w12_registered_brain, __ATOMIC_RELAXED);
}

/* ------------------------------------------------------------------------- *
 * Public: structural-root seeding                                           *
 * ------------------------------------------------------------------------- */

void w12_language_ensure_roots(struct brain_struct* brain) {
    if (!w12_kg_ready(brain)) return;

    brain_kg_t* kg = brain->internal_kg;
    const uint64_t token = brain->internal_kg_admin_token;

    brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_ADMIN, token);

    w12_ensure_root(kg, "cog_language_emergent",
        "Emergent language - brain-native vocabulary discovered from activation patterns");
    w12_ensure_root(kg, "cog_language_inner_speech",
        "Inner speech - iterative generate/re-encode self-refinement loop");
    w12_ensure_root(kg, "cog_language_native",
        "Native language - autoregressive decoding from brain embedding");
    w12_ensure_root(kg, "cog_language_tokenizer",
        "Brain-native tokenizer with BPE-style subword merging");
    w12_ensure_root(kg, "broca_discourse",
        "Broca discourse manager - turns, referents, topic tracking");
    w12_ensure_root(kg, "broca_syntax",
        "Broca syntax processor - CYK chart parsing + phrase structure rules");
    w12_ensure_root(kg, "broca_pragmatics",
        "Broca pragmatics processor - speech acts + Gricean maxims + implicatures");
    w12_ensure_root(kg, "broca_multimodal",
        "Broca multimodal language - synchronized speech + gesture + expression plans");

    brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_READ, 0);
}

/* ------------------------------------------------------------------------- *
 * 1. Emergent language                                                      *
 * ------------------------------------------------------------------------- */

void w12_emit_emergent_vocab(
    struct brain_struct* brain,
    const char* kind,
    uint32_t token_id,
    float specificity
) {
    if (!w12_kg_ready(brain)) return;
    w12_language_ensure_roots(brain);

    char name[160];
    snprintf(name, sizeof(name),
             "cog_language_emergent_event_%s_%llu",
             kind ? kind : "obs",
             (unsigned long long)w12_ts_us());
    char desc[192];
    snprintf(desc, sizeof(desc),
             "emergent vocab %s: token_id=%u specificity=%.3f",
             kind ? kind : "observe",
             token_id, specificity);
    w12_emit_linked(brain, name, desc, "cog_language_emergent", specificity);
}

/* ------------------------------------------------------------------------- *
 * 2. Inner speech                                                           *
 * ------------------------------------------------------------------------- */

void w12_emit_inner_speech_refine(
    struct brain_struct* brain,
    uint32_t iterations,
    float convergence
) {
    if (!w12_kg_ready(brain)) return;
    w12_language_ensure_roots(brain);

    char name[160];
    snprintf(name, sizeof(name),
             "cog_language_inner_speech_event_refine_%llu",
             (unsigned long long)w12_ts_us());
    char desc[192];
    snprintf(desc, sizeof(desc),
             "inner speech refine: iters=%u cosine=%.4f",
             iterations, convergence);
    w12_emit_linked(brain, name, desc,
                    "cog_language_inner_speech", convergence);
}

/* ------------------------------------------------------------------------- *
 * 3. Native language                                                        *
 * ------------------------------------------------------------------------- */

void w12_emit_native_generate(
    struct brain_struct* brain,
    uint32_t token_count,
    float mean_score
) {
    if (!w12_kg_ready(brain)) return;
    w12_language_ensure_roots(brain);

    char name[160];
    snprintf(name, sizeof(name),
             "cog_language_native_event_generate_%llu",
             (unsigned long long)w12_ts_us());
    char desc[192];
    snprintf(desc, sizeof(desc),
             "native generation: tokens=%u mean_score=%.3f",
             token_count, mean_score);
    w12_emit_linked(brain, name, desc,
                    "cog_language_native",
                    token_count > 0 ? 1.0f : 0.1f);
}

/* ------------------------------------------------------------------------- *
 * 4. Tokenizer (rate-limited)                                               *
 * ------------------------------------------------------------------------- */

void w12_emit_tokenizer_event(
    struct brain_struct* brain,
    const char* kind,
    uint32_t vocab_size,
    uint32_t delta
) {
    if (!w12_kg_ready(brain)) return;

    /* Rate-limit: emit at most 1 in W12_TOKENIZER_EMIT_PERIOD events for
     * "encode" — merges/learns are structurally rare so always emit. */
    static const uint32_t W12_TOKENIZER_EMIT_PERIOD = 512u;
    static uint32_t s_encode_counter = 0;
    const bool is_encode = (kind && strcmp(kind, "encode") == 0);
    if (is_encode) {
        uint32_t c = ++s_encode_counter;
        if ((c % W12_TOKENIZER_EMIT_PERIOD) != 1u) return;
    }

    w12_language_ensure_roots(brain);

    char name[160];
    snprintf(name, sizeof(name),
             "cog_language_tokenizer_event_%s_%llu",
             kind ? kind : "tok",
             (unsigned long long)w12_ts_us());
    char desc[192];
    snprintf(desc, sizeof(desc),
             "tokenizer %s: vocab=%u delta=%u",
             kind ? kind : "op", vocab_size, delta);
    w12_emit_linked(brain, name, desc,
                    "cog_language_tokenizer",
                    (float)delta);
}

/* ------------------------------------------------------------------------- *
 * 5. Discourse manager                                                      *
 * ------------------------------------------------------------------------- */

void w12_emit_discourse_turn(
    struct brain_struct* brain,
    uint32_t turn_id,
    uint32_t speaker_id,
    int topic_shift,
    float coherence_score
) {
    if (!w12_kg_ready(brain)) return;
    w12_language_ensure_roots(brain);

    char name[160];
    snprintf(name, sizeof(name),
             "broca_discourse_event_turn_%u_%llu",
             turn_id,
             (unsigned long long)w12_ts_us());
    char desc[192];
    snprintf(desc, sizeof(desc),
             "discourse turn: id=%u speaker=%u topic_shift=%d coherence=%.3f",
             turn_id, speaker_id, topic_shift, coherence_score);
    w12_emit_linked(brain, name, desc, "broca_discourse", coherence_score);
}

/* ------------------------------------------------------------------------- *
 * 6. Syntax processor                                                       *
 * ------------------------------------------------------------------------- */

void w12_emit_syntax_parse(
    struct brain_struct* brain,
    uint32_t unit_count,
    uint32_t tree_depth,
    bool success
) {
    if (!w12_kg_ready(brain)) return;
    w12_language_ensure_roots(brain);

    char name[160];
    snprintf(name, sizeof(name),
             "broca_syntax_event_parse_%llu",
             (unsigned long long)w12_ts_us());
    char desc[192];
    snprintf(desc, sizeof(desc),
             "syntax parse: units=%u depth=%u success=%s",
             unit_count, tree_depth, success ? "true" : "false");
    w12_emit_linked(brain, name, desc, "broca_syntax",
                    success ? 1.0f : 0.2f);
}

/* ------------------------------------------------------------------------- *
 * 7. Pragmatics processor                                                   *
 * ------------------------------------------------------------------------- */

void w12_emit_pragmatics_analyze(
    struct brain_struct* brain,
    int speech_act_type,
    uint32_t speaker_id,
    uint32_t implicature_cnt,
    float relevance
) {
    if (!w12_kg_ready(brain)) return;
    w12_language_ensure_roots(brain);

    char name[160];
    snprintf(name, sizeof(name),
             "broca_pragmatics_event_analyze_%llu",
             (unsigned long long)w12_ts_us());
    char desc[192];
    snprintf(desc, sizeof(desc),
             "pragmatics analyze: act=%d speaker=%u implicatures=%u relevance=%.3f",
             speech_act_type, speaker_id, implicature_cnt, relevance);
    w12_emit_linked(brain, name, desc, "broca_pragmatics", relevance);
}

/* ------------------------------------------------------------------------- *
 * 8. Multimodal language                                                    *
 * ------------------------------------------------------------------------- */

void w12_emit_multimodal_plan(
    struct brain_struct* brain,
    uint32_t gesture_count,
    uint32_t expression_count,
    float sync_score
) {
    if (!w12_kg_ready(brain)) return;
    w12_language_ensure_roots(brain);

    char name[160];
    snprintf(name, sizeof(name),
             "broca_multimodal_event_plan_%llu",
             (unsigned long long)w12_ts_us());
    char desc[192];
    snprintf(desc, sizeof(desc),
             "multimodal plan: gestures=%u expressions=%u sync=%.3f",
             gesture_count, expression_count, sync_score);
    w12_emit_linked(brain, name, desc, "broca_multimodal", sync_score);
}

/* ------------------------------------------------------------------------- *
 * Auto wrappers — use registered brain                                      *
 * ------------------------------------------------------------------------- */

void w12_emit_emergent_vocab_auto(
    const char* kind, uint32_t token_id, float specificity
) {
    struct brain_struct* b = w12_language_kg_get_brain();
    if (!b) return;
    w12_emit_emergent_vocab(b, kind, token_id, specificity);
}

void w12_emit_inner_speech_refine_auto(
    uint32_t iterations, float convergence
) {
    struct brain_struct* b = w12_language_kg_get_brain();
    if (!b) return;
    w12_emit_inner_speech_refine(b, iterations, convergence);
}

void w12_emit_native_generate_auto(
    uint32_t token_count, float mean_score
) {
    struct brain_struct* b = w12_language_kg_get_brain();
    if (!b) return;
    w12_emit_native_generate(b, token_count, mean_score);
}

void w12_emit_tokenizer_event_auto(
    const char* kind, uint32_t vocab_size, uint32_t delta
) {
    struct brain_struct* b = w12_language_kg_get_brain();
    if (!b) return;
    w12_emit_tokenizer_event(b, kind, vocab_size, delta);
}

void w12_emit_discourse_turn_auto(
    uint32_t turn_id, uint32_t speaker_id,
    int topic_shift, float coherence_score
) {
    struct brain_struct* b = w12_language_kg_get_brain();
    if (!b) return;
    w12_emit_discourse_turn(b, turn_id, speaker_id,
                            topic_shift, coherence_score);
}

void w12_emit_syntax_parse_auto(
    uint32_t unit_count, uint32_t tree_depth, bool success
) {
    struct brain_struct* b = w12_language_kg_get_brain();
    if (!b) return;
    w12_emit_syntax_parse(b, unit_count, tree_depth, success);
}

void w12_emit_pragmatics_analyze_auto(
    int speech_act_type, uint32_t speaker_id,
    uint32_t implicature_cnt, float relevance
) {
    struct brain_struct* b = w12_language_kg_get_brain();
    if (!b) return;
    w12_emit_pragmatics_analyze(b, speech_act_type, speaker_id,
                                implicature_cnt, relevance);
}

void w12_emit_multimodal_plan_auto(
    uint32_t gesture_count, uint32_t expression_count, float sync_score
) {
    struct brain_struct* b = w12_language_kg_get_brain();
    if (!b) return;
    w12_emit_multimodal_plan(b, gesture_count, expression_count, sync_score);
}
