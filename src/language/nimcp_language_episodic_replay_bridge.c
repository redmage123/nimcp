/**
 * @file nimcp_language_episodic_replay_bridge.c
 * @brief Episodic replay/consolidation integration
 * @date 2026-10-21
 */
#include "language/nimcp_grounded_language.h"
#include "core/brain/nimcp_brain_internal.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include <math.h>

typedef struct {
    float replay_activity; float consolidation_strength;
    float memory_reactivation; float knowledge_extraction;
} episodic_replay_state_t;

int episodic_replay_extract_consolidation(brain_t brain, episodic_replay_state_t* out) {
    if (!brain || !out) return -1;
    out->replay_activity = 0.5f; out->consolidation_strength = 0.6f;
    out->memory_reactivation = 0.55f; out->knowledge_extraction = 0.65f;
    return 0;
}

int grounded_language_integrate_episodic_replay(brain_t brain, void* gl, void* state) {
    if (!brain || !gl || !state) return -1;
    extern bool grounded_language_episodic_replay_enabled(const void* gl);
    if (!grounded_language_episodic_replay_enabled(gl)) return 0;

    typedef struct { float* content_intent; uint32_t dim; } cs_t;
    cs_t* cs = (cs_t*)state;
    episodic_replay_state_t replay = {0};
    if (episodic_replay_extract_consolidation(brain, &replay) != 0) return -1;

    const float w = 0.18f;
    if (cs->dim > 850) {
        cs->content_intent[825] += w * replay.replay_activity;
        cs->content_intent[826] += w * replay.consolidation_strength;
        cs->content_intent[827] += w * replay.memory_reactivation;
        cs->content_intent[828] += w * replay.knowledge_extraction;
    }
    NIMCP_LOGGING_DEBUG("Episodic replay: consolidation=%.2f knowledge=%.2f",
                        replay.consolidation_strength, replay.knowledge_extraction);
    return 0;
}

bool grounded_language_episodic_replay_enabled(const void* gl) { return (gl != NULL); }
