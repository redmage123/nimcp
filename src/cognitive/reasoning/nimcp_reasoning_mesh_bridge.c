/**
 * @file nimcp_reasoning_mesh_bridge.c
 * @brief Reasoning-Mesh Bridge — distributed evidence gathering via mesh network
 *
 * WHAT: Bridges reasoning with mesh for distributed evidence gathering
 * WHY:  Enable consensus-validated evidence from multiple brain modules
 * HOW:  Checks mesh availability via global bootstrap pointer, queries
 *       LEFT_HEMISPHERE channel for beliefs, aggregates evidence
 *
 * GRACEFUL DEGRADATION:
 * If mesh is not initialized (no bootstrap set), all functions
 * return empty/neutral results. The reasoning engine continues normally.
 *
 * MESH ACCESS:
 * The mesh bootstrap is stored as a process-global (set by medulla init).
 * We store our own reference via reasoning_mesh_set_bootstrap() called
 * during brain init, or check it lazily.
 *
 * @version 1.0.0
 * @date 2026-02-25
 */

#include "cognitive/reasoning/nimcp_reasoning_mesh_bridge.h"
#include "cognitive/reasoning/nimcp_reasoning_chain.h"
#include "core/brain/nimcp_brain.h"
#include "utils/logging/nimcp_logging.h"
#include <string.h>
#include <stdio.h>

/* Mesh headers — only types and channel (avoid bootstrap header
 * to prevent astrocyte_network_t type conflict with glial header) */
#include "mesh/nimcp_mesh_types.h"
#include "mesh/nimcp_mesh_channel.h"

/* Forward-declare bootstrap functions we need (avoids full header include) */
struct mesh_bootstrap;
typedef struct mesh_bootstrap mesh_bootstrap_t;
mesh_channel_t* mesh_bootstrap_get_channel(mesh_bootstrap_t* bootstrap,
                                            mesh_channel_id_t channel_id);

#define LOG_MODULE "reasoning_mesh"

/*=============================================================================
 * GLOBAL STATE
 *===========================================================================*/

/** Global mesh bootstrap reference (set by brain init or explicit call) */
static mesh_bootstrap_t* g_reasoning_mesh_bootstrap = NULL;

/*=============================================================================
 * BOOTSTRAP SETTER (called during system init)
 *===========================================================================*/

void reasoning_mesh_set_bootstrap(mesh_bootstrap_t* bootstrap) {
    g_reasoning_mesh_bootstrap = bootstrap;
}

/*=============================================================================
 * INTERNAL: Check mesh availability
 *===========================================================================*/

static mesh_channel_t* get_reasoning_channel(void) {
    if (!g_reasoning_mesh_bootstrap) return NULL;
    return mesh_bootstrap_get_channel(g_reasoning_mesh_bootstrap,
                                      MESH_CHANNEL_LEFT_HEMISPHERE);
}

/*=============================================================================
 * EMPTY RESULT
 *===========================================================================*/

reasoning_mesh_result_t reasoning_mesh_empty_result(void) {
    reasoning_mesh_result_t result;
    memset(&result, 0, sizeof(result));
    result.mesh_available = false;
    result.consensus_confidence = 0.0f;
    result.coherence = 0.0f;
    return result;
}

/*=============================================================================
 * GATHER EVIDENCE
 *===========================================================================*/

reasoning_mesh_result_t reasoning_mesh_gather_evidence(
    brain_t brain,
    const char* query,
    uint32_t timeout_ms
) {
    (void)brain;      /* Brain context not needed — mesh is global */
    (void)timeout_ms;  /* Reserved for future async gather */

    if (!query) {
        return reasoning_mesh_empty_result();
    }

    mesh_channel_t* channel = get_reasoning_channel();
    if (!channel) {
        NIMCP_LOGGING_DEBUG("Mesh not available — returning empty evidence");
        return reasoning_mesh_empty_result();
    }

    reasoning_mesh_result_t result;
    memset(&result, 0, sizeof(result));
    result.mesh_available = true;

    /* Get channel stats */
    result.channel_participant_count = mesh_channel_get_participant_count(channel);
    result.coherence = mesh_channel_get_world_state_coherence(channel);

    /*---------------------------------------------------------------
     * Get consensus beliefs from the channel as evidence
     *---------------------------------------------------------------*/
    mesh_belief_t beliefs[REASONING_MESH_MAX_ENDORSEMENTS];
    size_t belief_count = 0;

    nimcp_error_t err = mesh_channel_get_consensus_beliefs(
        channel, beliefs, REASONING_MESH_MAX_ENDORSEMENTS, &belief_count);

    if (err == NIMCP_SUCCESS && belief_count > 0) {
        for (size_t i = 0; i < belief_count &&
             result.evidence_count < REASONING_MESH_MAX_ENDORSEMENTS; i++) {
            reasoning_mesh_evidence_t* ev =
                &result.evidence[result.evidence_count];

            ev->source_id = beliefs[i].source;
            ev->confidence = beliefs[i].certainty;
            ev->relevance = beliefs[i].certainty;
            ev->endorsed = (beliefs[i].certainty > 0.5f);

            /* Map participant channel to evidence source */
            mesh_channel_id_t src_chan = mesh_get_channel(beliefs[i].source);
            switch (src_chan) {
                case MESH_CHANNEL_SUBCORTICAL:
                    ev->source = REASONING_EVIDENCE_MEMORY;
                    break;
                case MESH_CHANNEL_LEFT_HEMISPHERE:
                    ev->source = REASONING_EVIDENCE_KNOWLEDGE;
                    break;
                case MESH_CHANNEL_RIGHT_HEMISPHERE:
                    ev->source = REASONING_EVIDENCE_SENSORY;
                    break;
                default:
                    ev->source = REASONING_EVIDENCE_EXECUTIVE;
                    break;
            }

            snprintf(ev->description, sizeof(ev->description),
                     "Mesh belief from participant 0x%lx (certainty=%.2f)",
                     (unsigned long)beliefs[i].source,
                     (double)beliefs[i].certainty);

            result.evidence_count++;
            result.endorsements_received++;
            if (ev->endorsed) {
                result.endorsements_approved++;
            }
        }
    }

    /*---------------------------------------------------------------
     * Compute aggregate consensus confidence
     *---------------------------------------------------------------*/
    if (result.evidence_count > 0) {
        float total_conf = 0.0f;
        for (uint32_t i = 0; i < result.evidence_count; i++) {
            total_conf += result.evidence[i].confidence;
        }
        result.consensus_confidence = total_conf / (float)result.evidence_count;
    }

    NIMCP_LOGGING_DEBUG("Mesh evidence: %u items, consensus=%.2f, coherence=%.2f",
                        result.evidence_count,
                        (double)result.consensus_confidence,
                        (double)result.coherence);

    return result;
}

/*=============================================================================
 * APPLY EVIDENCE TO CHAIN
 *===========================================================================*/

int reasoning_mesh_apply_evidence(
    reasoning_chain_t* chain,
    const reasoning_mesh_result_t* result
) {
    if (!chain || !result) return -1;
    if (!result->mesh_available) return 0;
    if (result->evidence_count == 0) return 0;

    int steps_added = 0;

    for (uint32_t i = 0; i < result->evidence_count; i++) {
        const reasoning_mesh_evidence_t* ev = &result->evidence[i];

        /* Only add endorsed evidence */
        if (!ev->endorsed) continue;

        /* Map evidence source to reasoning step type */
        reasoning_step_type_t step_type;
        switch (ev->source) {
            case REASONING_EVIDENCE_MEMORY:
                step_type = REASONING_STEP_RECALL;
                break;
            case REASONING_EVIDENCE_KNOWLEDGE:
                step_type = REASONING_STEP_KNOWLEDGE;
                break;
            case REASONING_EVIDENCE_PLANNING:
                step_type = REASONING_STEP_INFERENCE;
                break;
            case REASONING_EVIDENCE_EMOTIONAL:
                step_type = REASONING_STEP_UNCERTAINTY;
                break;
            case REASONING_EVIDENCE_SENSORY:
                step_type = REASONING_STEP_ANALOGY;
                break;
            case REASONING_EVIDENCE_EXECUTIVE:
                step_type = REASONING_STEP_VERIFICATION;
                break;
            default:
                step_type = REASONING_STEP_KNOWLEDGE;
                break;
        }

        reasoning_step_t step;
        memset(&step, 0, sizeof(step));
        step.step_id = chain->num_steps;
        step.type = step_type;
        step.confidence = ev->confidence;
        step.relevance = ev->relevance;
        snprintf(step.description, sizeof(step.description), "%s", ev->description);

        if (reasoning_chain_add_step(chain, &step) < 0) {
            NIMCP_LOGGING_WARN("reasoning_mesh: step add failed (chain full, steps=%u)", chain->num_steps);
        }
        steps_added++;
    }

    return steps_added;
}

/*=============================================================================
 * APPLY CONSENSUS TO CONFIG
 *===========================================================================*/

int reasoning_mesh_apply_consensus(
    reasoning_engine_config_t* config,
    const reasoning_mesh_result_t* result
) {
    if (!config || !result) return -1;
    if (!result->mesh_available) return 0;

    /* Strong consensus → slightly boost confidence threshold */
    if (result->endorsements_received > 0) {
        float approval_rate = (float)result->endorsements_approved /
                              (float)result->endorsements_received;
        if (approval_rate > 0.8f && result->consensus_confidence > 0.7f) {
            float boost = (approval_rate - 0.8f) * 0.25f;  /* max 0.05 */
            float new_threshold = config->confidence_threshold + boost;
            if (new_threshold > 0.95f) new_threshold = 0.95f;
            config->confidence_threshold = new_threshold;
        }
    }

    return 0;
}

/*=============================================================================
 * RESULT SUMMARY
 *===========================================================================*/

int reasoning_mesh_result_summary(
    const reasoning_mesh_result_t* result,
    char* buffer,
    uint32_t buffer_size
) {
    if (!result || !buffer || buffer_size == 0) return -1;

    const char* status = result->mesh_available ? "AVAILABLE" : "UNAVAILABLE";

    int written = snprintf(buffer, buffer_size,
        "Mesh reasoning [%s]:"
        " evidence=%u"
        " endorsed=%u/%u"
        " consensus=%.2f"
        " coherence=%.2f"
        " participants=%lu"
        " gather_time=%.1fms",
        status,
        result->evidence_count,
        result->endorsements_approved,
        result->endorsements_received,
        (double)result->consensus_confidence,
        (double)result->coherence,
        (unsigned long)result->channel_participant_count,
        (double)result->gather_time_ms);

    return written;
}

/*=============================================================================
 * MESH AVAILABILITY CHECK
 *===========================================================================*/

bool reasoning_mesh_is_available(void) {
    return get_reasoning_channel() != NULL;
}

/*=============================================================================
 * CHANNEL STATS
 *===========================================================================*/

int reasoning_mesh_get_channel_stats(
    uint32_t* participants_out,
    float* coherence_out
) {
    mesh_channel_t* channel = get_reasoning_channel();
    if (!channel) {
        if (participants_out) *participants_out = 0;
        if (coherence_out) *coherence_out = 0.0f;
        return -1;
    }

    if (participants_out) {
        *participants_out = (uint32_t)mesh_channel_get_participant_count(channel);
    }
    if (coherence_out) {
        *coherence_out = mesh_channel_get_world_state_coherence(channel);
    }
    return 0;
}
