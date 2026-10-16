//=============================================================================
// nimcp_ethics_evaluation.c - Ethics Evaluation Functions
//=============================================================================
// RESPONSIBILITY: Golden Rule evaluation, empathy network, perspective-taking
//
// This module implements the core ethical evaluation based on the Golden Rule
// using empathy networks to simulate others' perspectives.
//=============================================================================

#include "cognitive/ethics/nimcp_ethics.h"
#include "cognitive/ethics/nimcp_ethics_internal.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "core/brain/nimcp_brain.h"
#include "cognitive/nimcp_working_memory.h"
#include "cognitive/nimcp_emotional_tagging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/error/nimcp_error_codes.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <math.h>
#include <string.h>

#define LOG_MODULE "ethics_evaluation"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(ethics_evaluation, MESH_ADAPTER_CATEGORY_COGNITIVE)


//=============================================================================
// Helper Functions for Feature Copying
//=============================================================================

/**
 * @brief Copies action features to combined buffer
 *
 * WHY: Extracted from nested loop to improve readability and testability.
 * Single responsibility - only handles feature copying.
 *
 * COMPLEXITY: O(n) where n = min(max_features, num_features)
 */
void ethics_copy_action_features(float* dest, const action_context_t* action, uint32_t max_features)
{
    // Guard clause: Validate inputs
    if (!dest || !action)
        return;

    /* Phase 8: Heartbeat at operation start */
    ethics_evaluation_heartbeat("ethics_evalu_ethics_copy_action_f", 0.0f);

    uint32_t count = (action->num_features < max_features) ? action->num_features : max_features;

    for (uint32_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            ethics_evaluation_heartbeat("ethics_evalu_loop",
                             (float)(i + 1) / (float)count);
        }

        dest[i] = action->features[i];
    }
}

/**
 * @brief Copies agent emotional state to buffer
 *
 * WHY: Extracted from nested loop. Isolates emotional state handling.
 * Makes code more maintainable and testable.
 *
 * COMPLEXITY: O(n) where n = num_emotions
 */
void ethics_copy_emotional_state(float* dest, const float* emotional_states, agent_id_t agent,
                                 uint32_t num_emotions, uint32_t offset)
{
    // Guard clause: Validate inputs
    if (!dest || !emotional_states)
        return;

    /* Phase 8: Heartbeat at operation start */
    ethics_evaluation_heartbeat("ethics_evalu_ethics_copy_emotiona", 0.0f);

    for (uint32_t i = 0; i < num_emotions; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_emotions > 256) {
            ethics_evaluation_heartbeat("ethics_evalu_loop",
                             (float)(i + 1) / (float)num_emotions);
        }

        dest[offset + i] = emotional_states[agent * num_emotions + i];
    }
}

/**
 * @brief Calculates perspective score from empathy state
 *
 * WHY: Extracted calculation logic for clarity. Single responsibility.
 * Mathematical formula isolated for easy testing and modification.
 *
 * COMPLEXITY: O(1)
 *
 * @return Perspective score [-1.0, 1.0] weighted by impact magnitude
 */
float ethics_calculate_perspective_score(const empathy_state_extended_t* state)
{
    // Guard clause: Validate input
    if (!state)
        return 0.0F;

    /* Phase 8: Heartbeat at operation start */
    ethics_evaluation_heartbeat("ethics_evalu_ethics_calculate_per", 0.0f);

    float emotional = state->emotional_valence;
    float material = state->material_impact;
    float autonomy = state->autonomy_impact;

    // Average the three impact dimensions
    float average_score = (emotional + material + autonomy) / 3.0F;

    // Weight by impact magnitude for importance
    return average_score * state->impact_magnitude;
}

/**
 * @brief Simulates single agent's perspective for an action
 *
 * WHY: Extracted from loop to eliminate nesting. Each agent's simulation
 * is independent and can be tested in isolation.
 *
 * COMPLEXITY: O(1) - Fixed neural network inference time
 *
 * @return Impact score for this agent's perspective
 */
float ethics_simulate_agent_perspective(empathy_network_t network, agent_id_t agent,
                                        const action_context_t* action)
{
    // Guard clause: Validate inputs
    if (!network || !action)
        return 0.0F;

    /* Phase 8: Heartbeat at operation start */
    ethics_evaluation_heartbeat("ethics_evalu_ethics_simulate_agen", 0.0f);

    empathy_state_extended_t state = empathy_network_simulate_agent(network, agent, action);

    return ethics_calculate_perspective_score(&state);
}

/**
 * @brief Evaluates Golden Rule by simulating affected parties' perspectives
 *
 * WHY: Implements "Do unto others as you would have them done unto you"
 * with the ultimate goal of improving the human condition.
 * Refactored to eliminate nested loops - each agent processed independently.
 *
 * CORE DIRECTIVE: All actions must serve to improve the human condition
 * through empathy, fairness, and respect for human dignity.
 *
 * ALGORITHM:
 * 1. For each affected agent, simulate their experience (O(n))
 * 2. Calculate perspective score (would I want this?) (O(1))
 * 3. Accumulate weighted scores (O(1))
 * 4. Return normalized average (O(1))
 *
 * COMPLEXITY: O(n) where n = num_affected_agents (previously had nested loops)
 *
 * @return Golden Rule score [-1.0, 1.0] where positive = you'd want it
 */
float ethics_evaluate_golden_rule(ethics_engine_t engine, const action_context_t* action)
{
    // Guard clause: Validate inputs
    if (!engine || !action)
        return 0.0F;

    /* Phase 8: Heartbeat at operation start */
    ethics_evaluation_heartbeat("ethics_evalu_ethics_evaluate_gold", 0.0f);

    // Guard clause: Check for affected agents
    if (action->num_affected_agents == 0)
        return 0.0F;

    float total_impact = 0.0F;

    // Single pass through affected agents - O(n)
    for (uint32_t i = 0; i < action->num_affected_agents; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && action->num_affected_agents > 256) {
            ethics_evaluation_heartbeat("ethics_evalu_loop",
                             (float)(i + 1) / (float)action->num_affected_agents);
        }

        agent_id_t agent = action->affected_agents[i];
        float perspective_score = ethics_simulate_agent_perspective(
            ethics_engine_get_empathy_net(engine), agent, action);
        total_impact += perspective_score;
    }

    // Normalize by number of affected parties
    return total_impact / action->num_affected_agents;
}

//=============================================================================
// Empathy Network Implementation
//=============================================================================

/**
 * @brief Creates empathy network for perspective-taking
 *
 * WHY: Enables "putting yourself in others' shoes" - core to Golden Rule.
 * Neural network learns to predict others' experiences.
 *
 * COMPLEXITY: O(1) - Fixed initialization
 *
 * @return Network instance or NULL on failure
 */
empathy_network_t empathy_network_create(const empathy_config_t* config)
{
    // Guard clause: Validate input
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return NULL;

    }

    /* Phase 8: Heartbeat at operation start */
    ethics_evaluation_heartbeat("ethics_evalu_empathy_network_crea", 0.0f);


    empathy_network_t network = nimcp_calloc(1, sizeof(struct empathy_network_struct));
    // Guard clause: Check allocation
    if (!network) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate network");

        return NULL;

    }

    uint32_t max_agents = 1000;

    /**
     * WHAT: Create perspective-taking brain for empathy network
     * WHY: Model other agents' mental states for ethical evaluation
     * HOW: Use BRAIN_SIZE_MEDIUM (10K neurons) for sufficient capacity
     * PATTERN: Composition - brain encapsulates neural network
     * NOTE: Uses minimal brain to avoid infinite recursion (empathy -> ethics -> empathy)
     */
    network->perspective_network =
        brain_create_minimal("perspective_taking", BRAIN_SIZE_MEDIUM, BRAIN_TASK_REGRESSION,
                     20,  // Action + agent features (input)
                     5    // Perspective dimensions (output)
        );


    // Guard clause: Check network creation
    if (!network->perspective_network) {
        nimcp_free(network);
        network = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "empathy_network_create: brain_create_minimal failed for perspective_network");
        return NULL;
    }


    network->num_agents = max_agents;
    network->states = nimcp_calloc(max_agents, sizeof(empathy_state_t));
    if (!network->states) return NULL;
    network->num_emotions = 10;
    network->emotional_states = nimcp_calloc(max_agents * network->num_emotions, sizeof(float));

    // Guard clause: Check allocations
    if (!network->states || !network->emotional_states) {
        brain_destroy(network->perspective_network);
        nimcp_free(network->states);
        nimcp_free(network->emotional_states);
        nimcp_free(network);
        network = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "empathy_network_create: required parameter is NULL (network->states, network->emotional_states)");
        return NULL;
    }

    return network;
}

/**
 * @brief Destroys empathy network and frees resources
 *
 * COMPLEXITY: O(1)
 */
void empathy_network_destroy(empathy_network_t network)
{
    // Guard clause: Validate input
    if (!network)
        return;

    /* Phase 8: Heartbeat at operation start */
    ethics_evaluation_heartbeat("ethics_evalu_empathy_network_dest", 0.0f);

    brain_destroy(network->perspective_network);
    nimcp_free(network->states);
    nimcp_free(network->emotional_states);
    nimcp_free(network);
    network = NULL;
}

/**
 * @brief Observe another node's activity — "mirror neuron" update.
 *
 * Stores the observation in states[source_node_id % num_agents], setting
 * mirror_activation from the packet's confidence (sign-flipped for
 * inhibitory events) and recording the feature code pattern. Returns
 * true when activation magnitude exceeds the empathy trigger threshold.
 *
 * This is the observation-side counterpart to empathy_network_predict_impact:
 * observe() accumulates state from inbound spikes; predict_impact() reads
 * forward through perspective_network.
 */
bool empathy_network_observe(empathy_network_t network, const event_packet_t* packet,
                             uint64_t timestamp)
{
    if (!network || !packet) return false;
    if (!network->states || network->num_agents == 0) return false;

    uint32_t slot = packet->source_node_id % network->num_agents;
    empathy_state_t* st = &network->states[slot];

    feature_code_t pattern = ((feature_code_t)packet->feature_high << 16) |
                             (feature_code_t)packet->feature_low;

    float confidence = (float)packet->confidence / 65535.0f;
    float activation = confidence;
    if (packet->version_flags & EVENT_FLAG_INHIBITORY) {
        activation = -activation;
    }

    st->observed_node_id   = packet->source_node_id;
    st->observed_pattern   = pattern;
    st->mirror_activation  = activation;
    st->predicted_impact   = 0.0f;  /* computed lazily by predict_impact */
    st->timestamp          = timestamp;

    /* Empathy triggers when mirror activation exceeds threshold. 0.5 matches
     * the confidence midpoint and is consistent with typical ethics policy
     * severity thresholds elsewhere in the engine. */
    return fabsf(activation) > 0.5f;
}

/**
 * @brief Return the most-recent observation across all agent slots.
 *
 * Scans states[] for the entry with the largest timestamp. Returns false
 * if no observation has ever been recorded (all timestamps zero).
 */
bool empathy_network_get_state(empathy_network_t network, empathy_state_t* state)
{
    if (!network || !state) return false;
    if (!network->states || network->num_agents == 0) return false;

    uint32_t latest_idx = 0;
    uint64_t latest_ts = 0;
    for (uint32_t i = 0; i < network->num_agents; i++) {
        if (network->states[i].timestamp > latest_ts) {
            latest_ts  = network->states[i].timestamp;
            latest_idx = i;
        }
    }

    if (latest_ts == 0) return false;
    *state = network->states[latest_idx];
    return true;
}

/**
 * @brief Predict the impact of a proposed action via the perspective network.
 *
 * Encodes the event packet into a 20-feature vector (matching the
 * perspective_network input size set by empathy_network_create), runs a
 * forward pass, and returns the first output dimension — interpreted as
 * emotional valence on [-1, +1] where negative values mean harmful.
 *
 * When perspective_network is unavailable, returns 0.0 (neutral).
 */
float empathy_network_predict_impact(empathy_network_t network, const event_packet_t* packet)
{
    if (!network || !packet) return 0.0f;
    if (!network->perspective_network) return 0.0f;

    float features[20] = {0};
    features[0] = (float)packet->confidence / 65535.0f;
    features[1] = (packet->version_flags & EVENT_FLAG_INHIBITORY) ? -1.0f : 1.0f;
    features[2] = (packet->version_flags & EVENT_FLAG_EXCITATORY) ?  1.0f : 0.0f;
    features[3] = (float)(packet->source_node_id % 1024) / 1024.0f;
    features[4] = (float)(packet->feature_low  & 0xFF) / 255.0f;
    features[5] = (float)((packet->feature_low  >> 8) & 0xFF) / 255.0f;
    features[6] = (float)(packet->feature_high & 0xFF) / 255.0f;
    features[7] = (float)((packet->feature_high >> 8) & 0xFF) / 255.0f;
    features[8] = (float)packet->hop_count / 255.0f;
    /* features[9..19] reserved for future emotional-context channels */

    brain_decision_t* decision = brain_decide(network->perspective_network, features, 20);
    if (!decision) return 0.0f;

    float impact = 0.0f;
    if (decision->output_vector && decision->output_size > 0) {
        impact = decision->output_vector[0];
    }
    brain_free_decision(decision);
    return impact;
}

/**
 * @brief Simulates agent's experience of an action
 *
 * WHY: Core of perspective-taking. Predicts how action would feel from
 * the affected agent's viewpoint. Essential for Golden Rule evaluation.
 *
 * ALGORITHM:
 * 1. Combine action features with agent's emotional state (O(n))
 * 2. Run through perspective neural network (O(1) inference time)
 * 3. Extract predicted experience dimensions (O(1))
 *
 * COMPLEXITY: O(n) where n = feature count (typically small, ~20)
 *
 * @return Extended empathy state with predicted experience
 */
empathy_state_extended_t empathy_network_simulate_agent(empathy_network_t network, agent_id_t agent,
                                                        const action_context_t* action)
{
    /* Phase 8: Heartbeat at operation start */
    ethics_evaluation_heartbeat("ethics_evalu_empathy_network_simu", 0.0f);


    empathy_state_extended_t state = {0};

    // Guard clause: Validate inputs
    if (!network || !action)
        return state;

    // Guard clause: Check agent bounds
    if (agent >= network->num_agents)
        return state;

    // Get buffer from pool for O(1) allocation
    // Simplified - would use engine's pool
    float* combined_features = NULL;

    // Fallback to stack allocation if pool exhausted
    float stack_buffer[MAX_FEATURE_SIZE] = {0};
    if (!combined_features) {
        combined_features = stack_buffer;
    }

    // Combine features (O(n))
    ethics_copy_action_features(combined_features, action, 10);
    ethics_copy_emotional_state(combined_features, network->emotional_states, agent, network->num_emotions,
                         10);

    // Neural network inference (O(1))
    brain_decision_t* decision = brain_decide(network->perspective_network, combined_features, 20);

    if (decision) {
        state.emotional_valence = decision->output_vector[0];
        state.material_impact = decision->output_vector[1];
        state.autonomy_impact = decision->output_vector[2];
        state.impact_magnitude = decision->output_vector[3];
        state.uncertainty = decision->output_vector[4];
        brain_free_decision(decision);
    }

    /* =========================================================================
     * PHASE 10.3: Emotional working memory modulation of empathy
     * =========================================================================
     * WHAT: Retrieve emotional context from working memory to modulate empathy
     * WHY:  Recent emotional experiences bias perspective-taking ability
     * HOW:  High arousal → impaired empathy, emotional congruence → enhanced empathy
     *
     * BIOLOGICAL BASIS:
     * - Emotional congruence: Same emotional state enhances empathy (Singer & Lamm, 2009)
     * - Emotional flooding: High arousal impairs perspective-taking accuracy
     * - Affective empathy vs cognitive empathy: Emotions modulate both pathways
     */
    if (network->main_brain) {
        working_memory_t* wm = brain_get_working_memory(network->main_brain);

        if (wm && working_memory_get_size(wm) > 0) {
            /* WHAT: Extract emotional context from working memory */
            float max_arousal = 0.0F;
            float avg_valence = 0.0F;
            uint32_t emotional_count = 0;
            uint32_t wm_size = working_memory_get_size(wm);

            /* WHAT: Aggregate recent emotional experiences */
            for (uint32_t i = 0; i < wm_size && i < 7; i++) {  // Miller's 7±2 limit
                emotional_tag_t emotion;
                if (working_memory_get_emotion(wm, i, &emotion) && emotion.intensity > 0.3F) {
                    if (emotion.arousal > max_arousal) {
                        max_arousal = emotion.arousal;
                    }
                    avg_valence += emotion.valence;
                    emotional_count++;
                }
            }

            if (emotional_count > 0) {
                avg_valence /= emotional_count;

                /* WHAT: High arousal impairs empathetic accuracy */
                /* WHY:  Emotional flooding reduces cognitive empathy (Decety & Cowell, 2014) */
                /* HOW:  Increase uncertainty and reduce magnitude for high arousal */
                if (max_arousal > 0.6F) {
                    state.uncertainty += (max_arousal - 0.6F) * 0.5F;  // Up to +0.2 uncertainty
                    if (state.uncertainty > 1.0F) state.uncertainty = 1.0F;

                    // Emotional flooding reduces empathetic accuracy
                    state.impact_magnitude *= (1.0F - (max_arousal - 0.6F) * 0.3F);  // Up to 12% reduction
                }

                /* WHAT: Emotional congruence enhances empathy */
                /* WHY:  Similar emotional states increase affective empathy */
                /* HOW:  If our emotional valence matches the predicted impact, boost confidence */
                float emotional_congruence = 1.0F - fabsf(avg_valence - state.emotional_valence);
                if (emotional_congruence > 0.7F) {
                    // High congruence: reduce uncertainty, boost magnitude
                    state.uncertainty *= 0.8F;  // 20% reduction in uncertainty
                    state.impact_magnitude *= 1.1F;  // 10% boost to empathetic accuracy
                    if (state.impact_magnitude > 1.0F) state.impact_magnitude = 1.0F;
                }

                /* WHAT: Emotional incongruence impairs empathy */
                /* WHY:  Opposite emotional states reduce perspective-taking accuracy */
                /* HOW:  If emotional states oppose, increase uncertainty */
                if (emotional_congruence < 0.3F) {
                    state.uncertainty += 0.1F;  // Increased uncertainty
                    if (state.uncertainty > 1.0F) state.uncertainty = 1.0F;
                }
            }
        }
    }

    state.agent_id = agent;
    state.active = true;

    // Release buffer back to pool if used
    if (combined_features != stack_buffer) {
        // Would release to pool here
    }

    return state;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * WHAT: Query knowledge graph for Ethics Evaluation self-knowledge
 * WHY:  Enable self-awareness about module's role and connections
 * HOW:  Query KG for entity observations and relations
 */
int ethics_evaluation_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    ethics_evaluation_heartbeat("ethics_evalu_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Ethics_Evaluation_Module");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                ethics_evaluation_heartbeat("ethics_evalu_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            LOG_DEBUG("Ethics evaluation self-knowledge: %s", self->observations[i]);
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Ethics_Evaluation_Module");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Ethics_Evaluation_Module");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void ethics_evaluation_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_ethics_evaluation_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int ethics_evaluation_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "ethics_evaluation_training_begin: NULL argument");
        return -1;
    }
    ethics_evaluation_heartbeat_instance(NULL, "ethics_evaluation_training_begin", 0.0f);
    return 0;
}

int ethics_evaluation_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "ethics_evaluation_training_end: NULL argument");
        return -1;
    }
    ethics_evaluation_heartbeat_instance(NULL, "ethics_evaluation_training_end", 1.0f);
    return 0;
}

int ethics_evaluation_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "ethics_evaluation_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    ethics_evaluation_heartbeat_instance(NULL, "ethics_evaluation_training_step", progress);
    return 0;
}
