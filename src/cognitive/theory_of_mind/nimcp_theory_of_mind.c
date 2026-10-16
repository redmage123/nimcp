/**
 * @file nimcp_theory_of_mind.c
 * @brief Theory of Mind implementation - inferring mental states of others
 *
 * WHAT: Belief-Desire-Intention (BDI) model for social cognition
 * WHY:  Enable understanding of others' beliefs, goals, emotions, and intentions
 * HOW:  Simulate other agents' mental states through perspective-taking
 *
 * BIOLOGICAL BASIS:
 * - Temporoparietal junction (TPJ) supports perspective-taking
 * - Medial prefrontal cortex (mPFC) models others' mental states
 * - Mirror neuron system enables empathy and action understanding
 *
 * PHASE: 10.6 (Theory of Mind)
 * DEPENDENCIES: Emotional Tagging (Phase 10.3), Executive Functions (Phase 10.3)
 * TRAINING_IMPACT: None (inference-only, meta-cognitive reasoning)
 *
 * @author NIMCP Development Team - Phase 10.6
 * @date 2025-11-09
 * @version 2.7.0 Phase 10.6
 */

#include "cognitive/nimcp_theory_of_mind.h"
#include "constants/nimcp_buffer_constants.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "cognitive/kg/nimcp_wave10_affective_kg.h"  /* W10: ToM belief events */
#include "cognitive/immune/nimcp_brain_immune.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "utils/memory/nimcp_memory.h"  // nimcp_malloc/nimcp_free
#include "utils/time/nimcp_time.h"       // get_current_time_ms
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <math.h>
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/exception/nimcp_exception_macros.h"

/* Bridge integrations */
#include "cognitive/theory_of_mind/nimcp_tom_snn_bridge.h"
#include "cognitive/theory_of_mind/nimcp_tom_plasticity_bridge.h"
#include "cognitive/theory_of_mind/nimcp_tom_fep_bridge.h"
#include "cognitive/theory_of_mind/nimcp_theory_of_mind_substrate_bridge.h"
#include "cognitive/theory_of_mind/nimcp_theory_of_mind_thalamic_bridge.h"

#define LOG_MODULE "cognitive.theory_of_mind"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_threshold_constants.h"

BRIDGE_BOILERPLATE(theory_of_mind, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define BIO_MODULE_COGNITIVE_THEORY_OF_MIND 0x034E


//=============================================================================
// Constants
//=============================================================================

#define MAX_BELIEFS 16                    /**< Maximum tracked beliefs */
#define MAX_DESIRES 8                     /**< Maximum tracked desires */
#define MAX_INTENTIONS 8                  /**< Maximum tracked intentions */
#define DEFAULT_CONFIDENCE 0.5f           /**< Default inference confidence */
#define OBSERVATION_DECAY_MS 5000         /**< Observations decay after 5 seconds */
#define MIN_CONFIDENCE_THRESHOLD NIMCP_CONFIDENCE_LOW     /**< Minimum confidence for valid inference */

// Confidence levels for emotion inference
#define CONFIDENCE_DIRECT_OBSERVATION 0.9f /**< High confidence: emotion directly observed */
#define CONFIDENCE_KEYWORD_MATCH 0.7f      /**< Medium confidence: keyword-based inference */
#define CONFIDENCE_NEUTRAL 0.3f            /**< Low confidence: neutral/unknown emotion */
#define CONFIDENCE_LOW 0.2f                /**< Very low confidence: weak inference */

// Weighting factors for action prediction
#define WEIGHT_PRIMARY 0.6f                /**< Primary factor weight in composite scoring */
#define WEIGHT_SECONDARY 0.4f              /**< Secondary factor weight in composite scoring */

// Initial state values
#define INITIAL_PERSPECTIVE_SCORE 1.0f     /**< Perfect perspective-taking by default */
#define INITIAL_DESIRE_SATISFACTION 0.0f   /**< Desires unsatisfied initially */

//=============================================================================
// Internal Structures
//=============================================================================

// Maximum agents to track (Phase 10.6.1)
#define MAX_TRACKED_AGENTS 32

/**
 * @brief Per-agent mental model (Phase 10.6.1)
 *
 * WHAT: Complete BDI + emotion model for one agent
 * WHY:  Track multiple agents simultaneously
 * HOW:  Store agent-specific state
 */
typedef struct {
    agent_id_t agent_id;            /**< Agent identifier */
    bool active;                    /**< Is this slot in use? */

    // BDI Model
    tom_belief_t belief;
    tom_desire_t desire;
    tom_intention_t intention;

    // Current inferences
    tom_emotion_t current_emotion;
    float emotion_confidence;
    char current_goal[NIMCP_ERROR_BUFFER_SIZE];
    float goal_confidence;

    // Last update time
    uint64_t last_update_ms;
} agent_model_t;

/**
 * @brief Theory of Mind internal state
 *
 * WHAT: Complete mental model of another agent
 * WHY:  Track beliefs, desires, intentions, and emotional states
 * HOW:  BDI model + emotion tracking + statistics
 */
struct theory_of_mind_s {
    // Reference to self (for simulation)
    brain_t self_brain;

    // BDI Model (legacy single agent - kept for backward compatibility)
    tom_belief_t beliefs[MAX_BELIEFS];
    uint32_t num_beliefs;

    tom_desire_t desires[MAX_DESIRES];
    uint32_t num_desires;

    tom_intention_t intentions[MAX_INTENTIONS];
    uint32_t num_intentions;

    // Current inferences (legacy single agent)
    tom_emotion_t current_emotion;
    float emotion_confidence;
    char current_goal[NIMCP_ERROR_BUFFER_SIZE];
    float goal_confidence;

    // Observation tracking
    uint64_t last_observation_ms;
    uint32_t observation_count;

    // Statistics
    tom_statistics_t stats;

    // Perspective-taking score
    float perspective_score;

    // Bio-async integration
    bio_module_context_t bio_ctx;   /**< Bio-async module context */
    bool bio_async_enabled;         /**< Bio-async registration status */

    // Multi-agent tracking (Phase 10.6.1)
    agent_model_t agent_models[MAX_TRACKED_AGENTS];
    uint32_t num_active_agents;

    // Brain immune integration
    brain_immune_system_t* immune_system;  /**< Connected immune system */
    float immune_impairment;                /**< Current inflammation impairment [0,1] */
    uint64_t last_immune_update_ms;        /**< Last immune state check */
    uint32_t social_stress_events;         /**< Count of social stress triggers */

    // Bridge integrations
    tom_snn_bridge_t* snn_bridge;           /**< SNN bridge for neural processing */
    tom_plasticity_bridge_t* plasticity_bridge; /**< Plasticity bridge for learning */
    tom_fep_bridge_t* fep_bridge;           /**< FEP bridge for free energy */
    tom_substrate_bridge_t* substrate_bridge; /**< Substrate bridge for metabolic state */
    tom_thalamic_bridge_t* thalamic_bridge; /**< Thalamic bridge for attention routing */
};

//=============================================================================
// Error Handling
//=============================================================================

static _Thread_local char last_error[NIMCP_ERROR_BUFFER_SIZE] = {0};

/**
 * @brief Set error message
 *
 * WHAT: Store formatted error message in thread-local storage
 * WHY:  Provide diagnostic information for failures
 * HOW:  Use vsnprintf for formatted strings
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (thread-local storage)
 */
static void set_error(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsnprintf(last_error, sizeof(last_error), fmt, args);
    va_end(args);
}

/**
 * @brief Get last error message
 *
 * WHAT: Retrieve most recent error
 * WHY:  Debugging failed operations
 * HOW:  Return thread-local error buffer
 *
 * @return Error string (never NULL, may be empty)
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (thread-local storage)
 */
const char* tom_get_last_error(void)
{
    return last_error;
}

//=============================================================================
// Forward Declarations for Immune Integration
//=============================================================================

static void apply_immune_impairment_to_emotion(theory_of_mind_t tom, float* confidence);
static void apply_immune_impairment_to_goal(theory_of_mind_t tom, float* confidence);

//=============================================================================
// Helper Functions: BDI Management
//=============================================================================

/**
 * @brief Add or update belief
 *
 * WHAT: Store new belief or update existing one
 * WHY:  Track agent's beliefs over time
 * HOW:  Search for existing → update or add new
 *
 * COMPLEXITY: O(n) where n = number of beliefs
 */
static bool add_belief(theory_of_mind_t tom, const char* content, float confidence, bool is_false)
{
    if (!tom || !content) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "add_belief: required parameter is NULL (tom, content)");
        return false;
    }

    // Search for existing belief with same content
    for (uint32_t i = 0; i < tom->num_beliefs; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && tom->num_beliefs > 256) {
            theory_of_mind_heartbeat("theory_of_mi_loop",
                             (float)(i + 1) / (float)tom->num_beliefs);
        }

        if (strcmp(tom->beliefs[i].belief_content, content) == 0) {
            // Update existing
            tom->beliefs[i].confidence = confidence;
            tom->beliefs[i].is_false_belief = is_false;
            tom->beliefs[i].last_updated_ms = nimcp_time_monotonic_ms();
            return true;
        }
    }

    // Add new belief if space available
    if (tom->num_beliefs < MAX_BELIEFS) {
        strncpy(tom->beliefs[tom->num_beliefs].belief_content, content, 255);
        tom->beliefs[tom->num_beliefs].belief_content[255] = '\0';
        tom->beliefs[tom->num_beliefs].confidence = confidence;
        tom->beliefs[tom->num_beliefs].is_false_belief = is_false;
        tom->beliefs[tom->num_beliefs].last_updated_ms = nimcp_time_monotonic_ms();
        tom->num_beliefs++;
        return true;
    }

    // No space - replace oldest belief
    uint32_t oldest_idx = 0;
    uint64_t oldest_time = tom->beliefs[0].last_updated_ms;
    for (uint32_t i = 1; i < tom->num_beliefs; i++) {
        if (tom->beliefs[i].last_updated_ms < oldest_time) {
            oldest_time = tom->beliefs[i].last_updated_ms;
            oldest_idx = i;
        }
    }

    strncpy(tom->beliefs[oldest_idx].belief_content, content, 255);
    tom->beliefs[oldest_idx].belief_content[255] = '\0';
    tom->beliefs[oldest_idx].confidence = confidence;
    tom->beliefs[oldest_idx].is_false_belief = is_false;
    tom->beliefs[oldest_idx].last_updated_ms = nimcp_time_monotonic_ms();

    return true;
}

/**
 * @brief Update current desire/goal
 *
 * WHAT: Set agent's current goal
 * WHY:  Goals drive behavior prediction
 * HOW:  Store in desires array
 *
 * COMPLEXITY: O(1)
 */
static bool update_desire(theory_of_mind_t tom, const char* goal, float intensity)
{
    if (!tom || !goal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "update_desire: required parameter is NULL (tom, goal)");
        return false;
    }

    // For simplicity, just update first desire slot as "current goal"
    if (tom->num_desires == 0) {
        tom->num_desires = 1;
    }

    strncpy(tom->desires[0].goal_description, goal, 255);
    tom->desires[0].goal_description[255] = '\0';
    tom->desires[0].intensity = intensity;
    tom->desires[0].satisfaction = INITIAL_DESIRE_SATISFACTION;  // Assume unsatisfied initially

    return true;
}

/**
 * @brief Update current intention
 *
 * WHAT: Set agent's planned action
 * WHY:  Intentions predict near-term behavior
 * HOW:  Store in intentions array
 *
 * COMPLEXITY: O(1)
 * NOTE: Reserved for future use in action prediction
 */
__attribute__((unused))
static bool update_intention(theory_of_mind_t tom, const char* action, float likelihood, bool in_progress)
{
    if (!tom || !action) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "update_intention: required parameter is NULL (tom, action)");
        return false;
    }

    // For simplicity, use first intention slot as "current intention"
    if (tom->num_intentions == 0) {
        tom->num_intentions = 1;
    }

    strncpy(tom->intentions[0].action_description, action, 255);
    tom->intentions[0].action_description[255] = '\0';
    tom->intentions[0].likelihood = likelihood;
    tom->intentions[0].action_in_progress = in_progress;

    return true;
}

//=============================================================================
// Helper Functions: Inference
//=============================================================================

/**
 * @brief Infer emotion from behavioral observation
 *
 * WHAT: Map observed behavior to emotion category
 * WHY:  Emotion understanding guides social response
 * HOW:  Heuristic based on context and action patterns
 *
 * ALGORITHM:
 * - Verbal context analysis (keywords)
 * - Observed emotion (if provided)
 * - Action patterns (high/low energy)
 *
 * COMPLEXITY: O(1)
 */
static tom_emotion_t infer_emotion_from_observation(const tom_observation_t* obs, float* confidence)
{
    if (!obs || !confidence) {
        if (confidence) *confidence = 0.0F;
        return TOM_EMOTION_UNKNOWN;
    }

    // If emotion directly observed, use it
    if (obs->observed_emotion != TOM_EMOTION_UNKNOWN && obs->observed_emotion != TOM_EMOTION_NEUTRAL) {
        *confidence = CONFIDENCE_DIRECT_OBSERVATION;
        return obs->observed_emotion;
    }

    // Analyze verbal context for emotional keywords
    // NOTE: Check negative emotions first to avoid false matches (e.g., "unhappy" contains "happy")
    if (obs->verbal_context) {
        if (strstr(obs->verbal_context, "sad") || strstr(obs->verbal_context, "depressed") ||
            strstr(obs->verbal_context, "unhappy")) {
            *confidence = CONFIDENCE_KEYWORD_MATCH;
            return TOM_EMOTION_SADNESS;
        }
        if (strstr(obs->verbal_context, "happy") || strstr(obs->verbal_context, "joy") ||
            strstr(obs->verbal_context, "excited") || strstr(obs->verbal_context, "great")) {
            *confidence = CONFIDENCE_KEYWORD_MATCH;
            return TOM_EMOTION_JOY;
        }
        if (strstr(obs->verbal_context, "angry") || strstr(obs->verbal_context, "mad") ||
            strstr(obs->verbal_context, "furious")) {
            *confidence = CONFIDENCE_KEYWORD_MATCH;
            return TOM_EMOTION_ANGER;
        }
        if (strstr(obs->verbal_context, "afraid") || strstr(obs->verbal_context, "scared") ||
            strstr(obs->verbal_context, "fear")) {
            *confidence = CONFIDENCE_KEYWORD_MATCH;
            return TOM_EMOTION_FEAR;
        }
        if (strstr(obs->verbal_context, "worried") || strstr(obs->verbal_context, "anxious") ||
            strstr(obs->verbal_context, "nervous")) {
            *confidence = CONFIDENCE_KEYWORD_MATCH;
            return TOM_EMOTION_ANXIETY;
        }
    }

    // Default: neutral with low confidence
    *confidence = CONFIDENCE_NEUTRAL;
    return TOM_EMOTION_NEUTRAL;
}

/**
 * @brief Infer goal from behavioral observation
 *
 * WHAT: Deduce agent's goal from actions
 * WHY:  Goals explain behavior
 * HOW:  Context analysis and goal keywords
 *
 * COMPLEXITY: O(1)
 */
static bool infer_goal_from_observation(const tom_observation_t* obs, char* goal_buffer, size_t buffer_size, float* confidence)
{
    if (!obs || !goal_buffer || buffer_size == 0 || !confidence) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "infer_goal_from_observation: required parameter is NULL (obs, goal_buffer, confidence)");
        return false;
    }

    *confidence = DEFAULT_CONFIDENCE;

    // Analyze verbal context for goal indicators
    if (obs->verbal_context) {
        if (strstr(obs->verbal_context, "want") || strstr(obs->verbal_context, "need")) {
            strncpy(goal_buffer, "Satisfy expressed desire", buffer_size - 1);
            goal_buffer[buffer_size - 1] = '\0';
            *confidence = CONFIDENCE_KEYWORD_MATCH;
            return true;
        }
        if (strstr(obs->verbal_context, "help")) {
            strncpy(goal_buffer, "Seek assistance", buffer_size - 1);
            goal_buffer[buffer_size - 1] = '\0';
            *confidence = CONFIDENCE_KEYWORD_MATCH;
            return true;
        }
        if (strstr(obs->verbal_context, "question") || strstr(obs->verbal_context, "?")) {
            strncpy(goal_buffer, "Obtain information", buffer_size - 1);
            goal_buffer[buffer_size - 1] = '\0';
            *confidence = CONFIDENCE_KEYWORD_MATCH;
            return true;
        }
    }

    // Default: unknown goal
    strncpy(goal_buffer, "Unknown goal", buffer_size - 1);
    goal_buffer[buffer_size - 1] = '\0';
    *confidence = CONFIDENCE_LOW;
    return true;
}

//=============================================================================
// Core API: Lifecycle
//=============================================================================

theory_of_mind_t tom_create(brain_t self_brain)
{
    // Allocate ToM structure
    /* Phase 8: Heartbeat at operation start */
    theory_of_mind_heartbeat("theory_of_mi_tom_create", 0.0f);


    theory_of_mind_t tom = (theory_of_mind_t)nimcp_malloc(sizeof(struct theory_of_mind_s));
    if (!tom) {
        set_error("Failed to allocate Theory of Mind structure");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "tom_create: tom is NULL");
        return NULL;
    }

    // Initialize structure
    memset(tom, 0, sizeof(struct theory_of_mind_s));

    tom->self_brain = self_brain;
    tom->current_emotion = TOM_EMOTION_NEUTRAL;
    tom->emotion_confidence = DEFAULT_CONFIDENCE;
    tom->goal_confidence = DEFAULT_CONFIDENCE;
    tom->perspective_score = INITIAL_PERSPECTIVE_SCORE;  // Perfect perspective-taking by default

    strncpy(tom->current_goal, "Unknown", sizeof(tom->current_goal) - 1);
    tom->current_goal[sizeof(tom->current_goal) - 1] = '\0';  // Ensure null termination

    // Bio-async registration
    tom->bio_ctx = NULL;
    tom->bio_async_enabled = false;
    if (bio_router_is_initialized()) {
        bio_module_info_t bio_info = {
            .module_id = BIO_MODULE_INTROSPECTION_THEORY_OF_MIND,
            .module_name = "theory_of_mind",
            .inbox_capacity = 32,
            .user_data = tom
        };
        tom->bio_ctx = bio_router_register_module(&bio_info);
        if (tom->bio_ctx) {
            tom->bio_async_enabled = true;
        }
    }

    // Initialize bridges
    tom->snn_bridge = NULL;
    tom->plasticity_bridge = NULL;
    tom->fep_bridge = NULL;
    tom->substrate_bridge = NULL;
    tom->thalamic_bridge = NULL;

    // Create SNN bridge with default config
    tom_snn_config_t snn_config = tom_snn_config_default();
    tom->snn_bridge = tom_snn_create(&snn_config);
    if (tom->snn_bridge) {
        LOG_DEBUG("SNN bridge created");
    }

    // Create plasticity bridge with default config
    tom_plasticity_config_t plasticity_config = tom_plasticity_config_default();
    tom->plasticity_bridge = tom_plasticity_create(&plasticity_config);
    if (tom->plasticity_bridge) {
        LOG_DEBUG("Plasticity bridge created");
    }

    // Create FEP bridge with default config
    tom_fep_config_t fep_config;
    tom_fep_bridge_default_config(&fep_config);
    tom->fep_bridge = tom_fep_bridge_create(&fep_config);
    if (tom->fep_bridge) {
        tom_fep_bridge_connect_tom(tom->fep_bridge, tom);
        LOG_DEBUG("FEP bridge created and connected");
    }

    /* Substrate bridge is wired later by brain init once the neural substrate
     * exists. Creating it here with NULL substrate logs a spurious ERROR every
     * time ToM initializes. Leave it NULL and let the brain init path call
     * tom_substrate_bridge_create with the real substrate once available. */
    tom->substrate_bridge = NULL;

    // Create thalamic bridge (requires external router, initialize NULL)
    tom_thalamic_config_t thalamic_config = tom_thalamic_default_config();
    tom->thalamic_bridge = tom_thalamic_bridge_create(tom, NULL, &thalamic_config);
    if (tom->thalamic_bridge) {
        LOG_DEBUG("Thalamic bridge created");
    }

    return tom;
}

void tom_destroy(theory_of_mind_t tom)
{
    /* Phase 8: Heartbeat at operation start */
    theory_of_mind_heartbeat("theory_of_mi_tom_destroy", 0.0f);


    LOG_DEBUG("Destroying module");
    if (!tom) {
        return;
    }

    // Destroy bridges
    if (tom->snn_bridge) {
        tom_snn_destroy(tom->snn_bridge);
        tom->snn_bridge = NULL;
    }
    if (tom->plasticity_bridge) {
        tom_plasticity_destroy(tom->plasticity_bridge);
        tom->plasticity_bridge = NULL;
    }
    if (tom->fep_bridge) {
        tom_fep_bridge_destroy(tom->fep_bridge);
        tom->fep_bridge = NULL;
    }
    if (tom->substrate_bridge) {
        tom_substrate_bridge_destroy(tom->substrate_bridge);
        tom->substrate_bridge = NULL;
    }
    if (tom->thalamic_bridge) {
        tom_thalamic_bridge_destroy(tom->thalamic_bridge);
        tom->thalamic_bridge = NULL;
    }

    // Free structure
    nimcp_free(tom);
    tom = NULL;
}

//=============================================================================
// Core API: Observation & Inference
//=============================================================================

bool tom_observe(theory_of_mind_t tom, const tom_observation_t* observation)
{
    // Process pending bio-async messages
    /* Phase 8: Heartbeat at operation start */
    theory_of_mind_heartbeat("theory_of_mi_tom_observe", 0.0f);


    if (tom && tom->bio_ctx) {
        bio_router_process_inbox(tom->bio_ctx, 5);
    }

    if (!tom || !observation) {
        set_error("NULL tom or observation in tom_observe");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom_observe: required parameter is NULL (tom, observation)");
        return false;
    }

    // Update observation tracking
    tom->last_observation_ms = nimcp_time_monotonic_ms();
    tom->observation_count++;
    tom->stats.total_observations++;

    // Infer emotion from observation
    float emotion_conf = 0.0F;
    tom_emotion_t inferred_emotion = infer_emotion_from_observation(observation, &emotion_conf);

    // Apply immune impairment to emotion inference
    apply_immune_impairment_to_emotion(tom, &emotion_conf);

    if (inferred_emotion != TOM_EMOTION_UNKNOWN) {
        tom->current_emotion = inferred_emotion;
        tom->emotion_confidence = emotion_conf;
        tom->stats.emotion_inferences++;
    }

    // Infer goal from observation
    char goal_buffer[NIMCP_ERROR_BUFFER_SIZE];
    float goal_conf = 0.0F;
    if (infer_goal_from_observation(observation, goal_buffer, sizeof(goal_buffer), &goal_conf)) {
        // Apply immune impairment to goal inference
        apply_immune_impairment_to_goal(tom, &goal_conf);

        strncpy(tom->current_goal, goal_buffer, sizeof(tom->current_goal) - 1);
        tom->current_goal[sizeof(tom->current_goal) - 1] = '\0';
        tom->goal_confidence = goal_conf;
        tom->stats.goal_inferences++;

        // Update desire based on inferred goal
        update_desire(tom, goal_buffer, goal_conf);
    }

    // Update statistics
    float total_conf = emotion_conf + goal_conf;
    if (total_conf > 0.0F) {
        tom->stats.average_inference_confidence =
            (tom->stats.average_inference_confidence * (tom->stats.total_observations - 1) +
             (total_conf / 2.0F)) / tom->stats.total_observations;
    }

    //=========================================================================
    // Bridge Processing
    //=========================================================================

    // SNN bridge: encode social context into spiking network
    if (tom->snn_bridge) {
        float dimensions[TOM_DIM_COUNT];
        dimensions[TOM_DIM_BELIEF_STATE] = emotion_conf;
        dimensions[TOM_DIM_DESIRE_STATE] = goal_conf;
        dimensions[TOM_DIM_INTENTION] = tom->goal_confidence;
        dimensions[TOM_DIM_PERSPECTIVE] = tom->perspective_score;
        dimensions[TOM_DIM_EMOTION_INFERENCE] = emotion_conf;
        dimensions[TOM_DIM_SOCIAL_CONTEXT] = 0.5f;  // Default social context
        dimensions[TOM_DIM_DECEPTION_DETECTION] = 0.0f;
        dimensions[TOM_DIM_SHARED_ATTENTION] = 0.5f;
        dimensions[TOM_DIM_EMPATHIC_ACCURACY] = emotion_conf;
        dimensions[TOM_DIM_MENTAL_SIMULATION] = tom->perspective_score;

        tom_snn_encode_context(tom->snn_bridge, dimensions, TOM_DIM_COUNT);
        tom_snn_step(tom->snn_bridge);
    }

    // Plasticity bridge: apply learning from observation
    if (tom->plasticity_bridge) {
        // Update eligibility traces based on observation
        tom_plasticity_update_traces(tom->plasticity_bridge, 1.0f);

        // If emotion inference was high confidence, apply learning
        if (emotion_conf > 0.7f) {
            tom_plasticity_learn(tom->plasticity_bridge,
                TOM_LEARN_CORRECT_BELIEF,
                emotion_conf,
                0,  // Default synapse
                1.0f);
        }
    }

    // FEP bridge: update free energy predictions
    if (tom->fep_bridge) {
        // Calculate prediction error from observation vs expectation
        float pe = fabsf(emotion_conf - 0.5f);  // Simple PE calculation
        if (pe > 0.3f) {
            tom_fep_infer_belief(tom->fep_bridge, pe);
        }

        // Activate empathy if emotion was detected
        if (inferred_emotion != TOM_EMOTION_UNKNOWN &&
            inferred_emotion != TOM_EMOTION_NEUTRAL) {
            tom_fep_activate_empathy(tom->fep_bridge, inferred_emotion);
        }

        // Update bridge state
        tom_fep_bridge_update(tom->fep_bridge, 1);
    }

    // Substrate bridge: update based on metabolic state
    if (tom->substrate_bridge) {
        tom_substrate_bridge_update(tom->substrate_bridge);
        tom_substrate_bridge_apply_effects(tom->substrate_bridge);
    }

    // Thalamic bridge: route inference through attention mechanism
    if (tom->thalamic_bridge) {
        tom_thalamic_signal_t signal = {
            .signal_type = TOM_SIGNAL_BELIEF,
            .social_salience = emotion_conf,
            .confidence = goal_conf,
            .urgency = (inferred_emotion == TOM_EMOTION_FEAR ||
                       inferred_emotion == TOM_EMOTION_ANGER) ? 0.9f : 0.5f,
            .mental_state_data = NULL,
            .data_size = 0,
            .timestamp_us = nimcp_time_monotonic_ms() * 1000
        };
        tom_thalamic_route_inference(tom->thalamic_bridge, &signal);
    }

    return true;
}

tom_emotion_t tom_infer_emotion(theory_of_mind_t tom, float* confidence)
{
    if (!tom) {
        if (confidence) *confidence = 0.0F;
        return TOM_EMOTION_UNKNOWN;
    }

    /* Phase 8: Heartbeat at operation start */
    theory_of_mind_heartbeat("theory_of_mi_tom_infer_emotion", 0.0f);


    if (confidence) {
        *confidence = tom->emotion_confidence;
    }

    /* W10: emit ToM emotion-inference event + query model-count bias. */
    if (tom->self_brain) {
        wave10_tom_emit_belief_event(tom->self_brain, 0u, "emotion",
                                     tom->emotion_confidence);
        (void)wave10_tom_query_model_count_bias(tom->self_brain);
    }

    return tom->current_emotion;
}

bool tom_infer_goal(theory_of_mind_t tom, char* goal_buffer, size_t buffer_size, float* confidence)
{
    if (!tom || !goal_buffer || buffer_size == 0) {
        set_error("Invalid parameters in tom_infer_goal");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom_infer_goal: required parameter is NULL (tom, goal_buffer)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    theory_of_mind_heartbeat("theory_of_mi_tom_infer_goal", 0.0f);


    strncpy(goal_buffer, tom->current_goal, buffer_size - 1);
    goal_buffer[buffer_size - 1] = '\0';

    if (confidence) {
        *confidence = tom->goal_confidence;
    }

    return true;
}

bool tom_predict_action(theory_of_mind_t tom, char* predicted_action, size_t action_buffer_size, float* likelihood)
{
    if (!tom || !predicted_action || action_buffer_size == 0) {
        set_error("Invalid parameters in tom_predict_action");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom_predict_action: required parameter is NULL (tom, predicted_action)");
        return false;
    }

    // Use current intention if available
    /* Phase 8: Heartbeat at operation start */
    theory_of_mind_heartbeat("theory_of_mi_tom_predict_action", 0.0f);


    if (tom->num_intentions > 0) {
        strncpy(predicted_action, tom->intentions[0].action_description, action_buffer_size - 1);
        predicted_action[action_buffer_size - 1] = '\0';

        if (likelihood) {
            *likelihood = tom->intentions[0].likelihood;
        }

        tom->stats.action_predictions++;
        return true;
    }

    // Otherwise, predict action from current goal
    if (strlen(tom->current_goal) > 0 && strcmp(tom->current_goal, "Unknown") != 0) {
        snprintf(predicted_action, action_buffer_size, "Action toward: %s", tom->current_goal);

        if (likelihood) {
            *likelihood = tom->goal_confidence * WEIGHT_PRIMARY;  // Moderate confidence
        }

        tom->stats.action_predictions++;
        return true;
    }

    // No basis for prediction
    strncpy(predicted_action, "Unknown action", action_buffer_size - 1);
    predicted_action[action_buffer_size - 1] = '\0';

    if (likelihood) {
        *likelihood = CONFIDENCE_LOW / 2.0F;  // Very low confidence (0.1)
    }

    return true;
}

bool tom_empathize(theory_of_mind_t tom, tom_emotion_t observed_emotion, tom_emotion_t* empathy_response)
{
    if (!tom || !empathy_response) {
        set_error("Invalid parameters in tom_empathize");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom_empathize: required parameter is NULL (tom, empathy_response)");
        return false;
    }

    // Simple empathy mapping: mirror emotions or provide complementary response
    /* Phase 8: Heartbeat at operation start */
    theory_of_mind_heartbeat("theory_of_mi_tom_empathize", 0.0f);


    switch (observed_emotion) {
        case TOM_EMOTION_JOY:
            *empathy_response = TOM_EMOTION_JOY;  // Share joy
            break;

        case TOM_EMOTION_SADNESS:
            *empathy_response = TOM_EMOTION_SADNESS;  // Mirror sadness (compassion)
            break;

        case TOM_EMOTION_ANGER:
            *empathy_response = TOM_EMOTION_CALM;  // Calming response
            break;

        case TOM_EMOTION_FEAR:
            *empathy_response = TOM_EMOTION_CALM;  // Reassuring response
            break;

        case TOM_EMOTION_ANXIETY:
            *empathy_response = TOM_EMOTION_CALM;  // Calming response
            break;

        case TOM_EMOTION_PRIDE:
            *empathy_response = TOM_EMOTION_JOY;  // Share in pride
            break;

        case TOM_EMOTION_SHAME:
            *empathy_response = TOM_EMOTION_CALM;  // Supportive response
            break;

        case TOM_EMOTION_NEUTRAL:
        case TOM_EMOTION_SURPRISE:
        case TOM_EMOTION_DISGUST:
        default:
            *empathy_response = TOM_EMOTION_NEUTRAL;
            break;
    }

    tom->stats.empathy_responses++;
    return true;
}

bool tom_detect_false_belief(theory_of_mind_t tom,
                             const char* reality_state,
                             const char* believed_state,
                             bool* is_false_belief)
{
    if (!tom || !reality_state || !believed_state || !is_false_belief) {
        set_error("Invalid parameters in tom_detect_false_belief");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom_detect_false_belief: required parameter is NULL (tom, reality_state, believed_state, is_false_belief)");
        return false;
    }

    // Compare reality vs. belief
    *is_false_belief = (strcmp(reality_state, believed_state) != 0);

    /* Phase 8: Heartbeat at operation start */
    theory_of_mind_heartbeat("theory_of_mi_tom_detect_false_bel", 0.0f);


    if (*is_false_belief) {
        // Record false belief
        add_belief(tom, believed_state, 0.8F, true);
        tom->stats.false_beliefs_detected++;

        // High perspective-taking score (recognized mismatch)
        tom->perspective_score = fminf(1.0F, tom->perspective_score + 0.1F);
    }

    return true;
}

bool tom_get_bdi_state(theory_of_mind_t tom,
                       tom_belief_t* belief,
                       tom_desire_t* desire,
                       tom_intention_t* intention)
{
    if (!tom) {
        set_error("NULL tom in tom_get_bdi_state");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom_get_bdi_state: tom is NULL");
        return false;
    }

    // Copy most recent/relevant belief
    /* Phase 8: Heartbeat at operation start */
    theory_of_mind_heartbeat("theory_of_mi_tom_get_bdi_state", 0.0f);


    if (belief && tom->num_beliefs > 0) {
        *belief = tom->beliefs[0];
    }

    // Copy current desire
    if (desire && tom->num_desires > 0) {
        *desire = tom->desires[0];
    }

    // Copy current intention
    if (intention && tom->num_intentions > 0) {
        *intention = tom->intentions[0];
    }

    return true;
}

float tom_get_perspective_score(theory_of_mind_t tom)
{
    if (!tom) {
        return 0.0F;
    }

    /* Phase 8: Heartbeat at operation start */
    theory_of_mind_heartbeat("theory_of_mi_tom_get_perspective_", 0.0f);


    return tom->perspective_score;
}

//=============================================================================
// Statistics & Diagnostics
//=============================================================================

bool tom_get_statistics(theory_of_mind_t tom, tom_statistics_t* stats)
{
    if (!tom || !stats) {
        set_error("Invalid parameters in tom_get_statistics");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom_get_statistics: required parameter is NULL (tom, stats)");
        return false;
    }

    *stats = tom->stats;
    /* Phase 8: Heartbeat at operation start */
    theory_of_mind_heartbeat("theory_of_mi_tom_get_statistics", 0.0f);


    stats->perspective_taking_score = tom->perspective_score;

    return true;
}

/**
 * @brief Update self-model with own decision
 *
 * WHAT: Record brain's own decision to build self-model
 * WHY:  Understanding self is required for understanding others
 * HOW:  Store decision as observation, increment stats
 *
 * COMPLEXITY: O(1)
 */
bool tom_update_self_model(theory_of_mind_t tom,
                           const float* features,
                           uint32_t num_features,
                           const char* action_label,
                           float confidence)
{
    // Guard: Validate parameters
    if (!tom) {
        set_error("NULL tom in tom_update_self_model");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom_update_self_model: tom is NULL");
        return false;
    }
    if (!features) {
        set_error("NULL features in tom_update_self_model");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom_update_self_model: features is NULL");
        return false;
    }
    if (!action_label) {
        set_error("NULL action_label in tom_update_self_model");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom_update_self_model: action_label is NULL");
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    theory_of_mind_heartbeat("theory_of_mi_tom_update_self_mode", 0.0f);


    if (num_features == 0) {
        set_error("Zero num_features in tom_update_self_model");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "tom_update_self_model: num_features is zero");
        return false;
    }
    if (confidence < 0.0F || confidence > 1.0F) {
        set_error("Invalid confidence in tom_update_self_model");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "tom_update_self_model: validation failed");
        return false;
    }

    // Update observation count (self-model observations)
    tom->stats.total_observations++;
    tom->last_observation_ms = nimcp_time_get_ms();

    return true;
}

bool tom_reset(theory_of_mind_t tom)
{
    if (!tom) {
        set_error("NULL tom in tom_reset");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom_reset: tom is NULL");
        return false;
    }

    // Clear BDI state
    /* Phase 8: Heartbeat at operation start */
    theory_of_mind_heartbeat("theory_of_mi_tom_reset", 0.0f);


    tom->num_beliefs = 0;
    tom->num_desires = 0;
    tom->num_intentions = 0;

    // Reset inferences
    tom->current_emotion = TOM_EMOTION_NEUTRAL;
    tom->emotion_confidence = DEFAULT_CONFIDENCE;
    tom->goal_confidence = DEFAULT_CONFIDENCE;
    strncpy(tom->current_goal, "Unknown", sizeof(tom->current_goal));

    // Reset observation tracking
    tom->observation_count = 0;

    // Preserve statistics and perspective score

    return true;
}

const char* tom_emotion_to_string(tom_emotion_t emotion)
{
    switch (emotion) {
        case TOM_EMOTION_UNKNOWN:   return "Unknown";
        case TOM_EMOTION_NEUTRAL:   return "Neutral";
        case TOM_EMOTION_JOY:       return "Joy";
        case TOM_EMOTION_SADNESS:   return "Sadness";
        case TOM_EMOTION_ANGER:     return "Anger";
        case TOM_EMOTION_FEAR:      return "Fear";
        case TOM_EMOTION_SURPRISE:  return "Surprise";
        case TOM_EMOTION_DISGUST:   return "Disgust";
        case TOM_EMOTION_ANXIETY:   return "Anxiety";
        case TOM_EMOTION_PRIDE:     return "Pride";
        case TOM_EMOTION_SHAME:     return "Shame";
        case TOM_EMOTION_CALM:      return "Calm";
        default:                    return "Unknown";
    }
}

// NOTE: tom_get_last_error already defined above - removed duplicate

//=============================================================================
// Multi-Agent Helper Functions (Phase 10.6.1)
//=============================================================================

/**
 * @brief Find agent model by ID
 *
 * WHAT: Locate agent model in tracking array
 * WHY:  Need to access agent-specific state
 * HOW:  Linear search through active agents
 *
 * COMPLEXITY: O(n) where n = num_active_agents
 */
static agent_model_t* find_agent_model(theory_of_mind_t tom, agent_id_t agent_id)
{
    if (!tom) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom is NULL");

        return NULL;

    }

    for (uint32_t i = 0; i < MAX_TRACKED_AGENTS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && MAX_TRACKED_AGENTS > 256) {
            theory_of_mind_heartbeat("theory_of_mi_loop",
                             (float)(i + 1) / (float)MAX_TRACKED_AGENTS);
        }

        if (tom->agent_models[i].active && tom->agent_models[i].agent_id == agent_id) {
            return &tom->agent_models[i];
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_agent_model: validation failed");
    return NULL;
}

/**
 * @brief Get or create agent model
 *
 * WHAT: Find existing agent model or allocate new slot
 * WHY:  Need to track agent state, create if first encounter
 * HOW:  Search for existing, then find free slot
 *
 * COMPLEXITY: O(n) where n = MAX_TRACKED_AGENTS
 */
static agent_model_t* get_or_create_agent_model(theory_of_mind_t tom, agent_id_t agent_id)
{
    if (!tom) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom is NULL");

        return NULL;

    }

    // First, try to find existing
    agent_model_t* existing = find_agent_model(tom, agent_id);
    if (existing) return existing;

    // Find free slot
    for (uint32_t i = 0; i < MAX_TRACKED_AGENTS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && MAX_TRACKED_AGENTS > 256) {
            theory_of_mind_heartbeat("theory_of_mi_loop",
                             (float)(i + 1) / (float)MAX_TRACKED_AGENTS);
        }

        if (!tom->agent_models[i].active) {
            tom->agent_models[i].active = true;
            tom->agent_models[i].agent_id = agent_id;
            tom->agent_models[i].current_emotion = TOM_EMOTION_NEUTRAL;
            tom->agent_models[i].emotion_confidence = DEFAULT_CONFIDENCE;
            tom->agent_models[i].goal_confidence = DEFAULT_CONFIDENCE;
            tom->agent_models[i].last_update_ms = nimcp_time_get_ms();
            strncpy(tom->agent_models[i].current_goal, "Unknown",
                    sizeof(tom->agent_models[i].current_goal));
            tom->num_active_agents++;
            return &tom->agent_models[i];
        }
    }

    set_error("Agent tracking array full (max %d agents)", MAX_TRACKED_AGENTS);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "get_or_create_agent_model: operation failed");
    return NULL;
}

//=============================================================================
// Brain Immune Integration API
//=============================================================================

/**
 * @brief Cytokine callback for immune system effects on ToM
 *
 * WHAT: Receive cytokine signals and adjust ToM capacity
 * WHY:  Inflammation impairs social cognition (sickness behavior)
 * HOW:  Pro-inflammatory cytokines reduce perspective score and confidence
 *
 * BIOLOGICAL BASIS:
 * IL-6, TNF-α, and IL-1β impair mPFC and TPJ function, reducing
 * theory of mind performance during inflammation.
 */
static void tom_immune_cytokine_callback(
    brain_immune_system_t* system,
    const brain_cytokine_t* cytokine,
    void* user_data)
{
    theory_of_mind_t tom = (theory_of_mind_t)user_data;
    if (!tom || !cytokine) return;

    // Map cytokine type to impairment
    float impairment_delta = 0.0f;

    switch (cytokine->type) {
        case CYTOKINE_IL1B:
            // IL-1: Moderate pro-inflammatory, mild impairment
            impairment_delta = cytokine->concentration * 0.15f;
            break;

        case CYTOKINE_IL6:
            // IL-6: Strong pro-inflammatory, moderate impairment
            impairment_delta = cytokine->concentration * 0.25f;
            break;

        case CYTOKINE_TNFA:
            // TNF-α: Severe pro-inflammatory, strong impairment
            impairment_delta = cytokine->concentration * 0.35f;
            break;

        case CYTOKINE_IL10:
            // IL-10: Anti-inflammatory, reduces impairment
            impairment_delta = -cytokine->concentration * 0.20f;
            break;

        case BRAIN_CYTOKINE_IFN_GAMMA:
            // IFN-γ: Moderate pro-inflammatory
            impairment_delta = cytokine->concentration * 0.20f;
            break;

        default:
            return;
    }

    // Update impairment level (clamped to [0,1])
    tom->immune_impairment = fmaxf(0.0f, fminf(1.0f,
        tom->immune_impairment + impairment_delta));

    // High inflammation reduces perspective-taking score
    if (tom->immune_impairment > 0.5f) {
        float reduction = (tom->immune_impairment - 0.5f) * 0.4f;  // Max 0.2 reduction
        tom->perspective_score = fmaxf(0.5f, tom->perspective_score - reduction);
    }

    tom->last_immune_update_ms = nimcp_time_monotonic_ms();
}

/**
 * @brief Inflammation callback for immune system effects on ToM
 *
 * WHAT: Receive inflammation level changes
 * WHY:  Severe inflammation (cytokine storm) severely impairs social cognition
 * HOW:  Map inflammation level to impairment multiplier
 */
static void tom_immune_inflammation_callback(
    brain_immune_system_t* system,
    const brain_inflammation_site_t* site,
    void* user_data)
{
    theory_of_mind_t tom = (theory_of_mind_t)user_data;
    if (!tom || !site) return;

    // Map inflammation level to impairment
    float base_impairment = 0.0f;

    switch (site->level) {
        case INFLAMMATION_NONE:
            base_impairment = 0.0f;
            break;
        case INFLAMMATION_LOCAL:
            base_impairment = 0.1f;
            break;
        case INFLAMMATION_REGIONAL:
            base_impairment = 0.3f;
            break;
        case INFLAMMATION_SYSTEMIC:
            base_impairment = 0.6f;
            break;
        case INFLAMMATION_STORM:
            // Cytokine storm severely impairs all cognition
            base_impairment = 0.9f;
            break;
    }

    // Gradually update impairment (smooth transition)
    tom->immune_impairment = tom->immune_impairment * 0.7f + base_impairment * 0.3f;
    tom->last_immune_update_ms = nimcp_time_monotonic_ms();
}

bool tom_connect_immune(theory_of_mind_t tom, brain_immune_system_t* immune)
{
    // Guard: validate parameters
    if (!tom) {
        set_error("NULL tom in tom_connect_immune");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom_connect_immune: tom is NULL");
        return false;
    }
    if (!immune) {
        set_error("NULL immune in tom_connect_immune");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom_connect_immune: immune is NULL");
        return false;
    }

    // Store immune system handle
    /* Phase 8: Heartbeat at operation start */
    theory_of_mind_heartbeat("theory_of_mi_tom_connect_immune", 0.0f);


    tom->immune_system = immune;
    tom->immune_impairment = 0.0f;
    tom->last_immune_update_ms = nimcp_time_monotonic_ms();
    tom->social_stress_events = 0;

    // Register callbacks with immune system
    brain_immune_set_cytokine_callback(immune, tom_immune_cytokine_callback, tom);
    brain_immune_set_inflammation_callback(immune, tom_immune_inflammation_callback, tom);

    LOG_INFO("ToM connected to brain immune system");
    return true;
}

float tom_get_immune_impairment(theory_of_mind_t tom)
{
    // Guard: validate tom
    if (!tom) {
        return 0.0f;
    }

    // Return current impairment level
    /* Phase 8: Heartbeat at operation start */
    theory_of_mind_heartbeat("theory_of_mi_tom_get_immune_impai", 0.0f);


    return tom->immune_impairment;
}

bool tom_trigger_social_stress(theory_of_mind_t tom,
                                float prediction_error,
                                bool is_social_rejection)
{
    // Guard: validate parameters
    if (!tom) {
        set_error("NULL tom in tom_trigger_social_stress");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom_trigger_social_stress: tom is NULL");
        return false;
    }
    if (!tom->immune_system) {
        set_error("No immune system connected");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom_trigger_social_stress: tom->immune_system is NULL");
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    theory_of_mind_heartbeat("theory_of_mi_tom_trigger_social_s", 0.0f);


    if (prediction_error < 0.0f || prediction_error > 1.0f) {
        set_error("Invalid prediction_error (must be [0,1])");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "tom_trigger_social_stress: validation failed");
        return false;
    }

    // Track social stress event
    tom->social_stress_events++;

    // Determine cytokine type and concentration based on stress severity
    brain_cytokine_type_t cytokine_type;
    float concentration = 0.0f;

    if (is_social_rejection) {
        // Social rejection triggers stronger inflammatory response
        cytokine_type = CYTOKINE_IL6;  // IL-6 associated with social stress
        concentration = fminf(1.0f, prediction_error * 1.5f);
    } else {
        // General prediction error triggers milder response
        cytokine_type = CYTOKINE_IL1B;  // IL-1 for general stress
        concentration = prediction_error;
    }

    // Release cytokine via immune system
    uint32_t cytokine_id = 0;
    int result = brain_immune_release_cytokine(
        tom->immune_system,
        cytokine_type,
        0,  // Source cell ID (0 = from cognitive system)
        concentration,
        0,  // Target region (0 = broadcast)
        &cytokine_id
    );

    if (result != 0) {
        set_error("Failed to release stress cytokine");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "tom_trigger_social_stress: validation failed");
        return false;
    }

    LOG_DEBUG("Social stress triggered: error=%.2f, rejection=%d, cytokine=%s (%.2f)",
              prediction_error, is_social_rejection,
              brain_immune_cytokine_to_string(cytokine_type), concentration);

    return true;
}

/**
 * @brief Apply immune impairment to emotion inference
 *
 * WHAT: Reduce emotion inference confidence based on inflammation
 * WHY:  Inflammation impairs social-emotional processing
 * HOW:  Scale confidence by (1 - impairment)
 */
static void apply_immune_impairment_to_emotion(theory_of_mind_t tom, float* confidence)
{
    if (!tom || !confidence || !tom->immune_system) return;

    // Reduce confidence proportional to impairment
    float reduction_factor = 1.0f - (tom->immune_impairment * 0.7f);  // Max 70% reduction
    *confidence *= reduction_factor;
}

/**
 * @brief Apply immune impairment to goal inference
 *
 * WHAT: Reduce goal inference confidence based on inflammation
 * WHY:  Inflammation impairs mentalizing and intention understanding
 * HOW:  Scale confidence by (1 - impairment)
 */
static void apply_immune_impairment_to_goal(theory_of_mind_t tom, float* confidence)
{
    if (!tom || !confidence || !tom->immune_system) return;

    // Reduce confidence proportional to impairment
    float reduction_factor = 1.0f - (tom->immune_impairment * 0.8f);  // Max 80% reduction
    *confidence *= reduction_factor;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int theory_of_mind_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    theory_of_mind_heartbeat("theory_of_mi_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Theory_Of_Mind");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                theory_of_mind_heartbeat("theory_of_mi_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Theory_Of_Mind");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Theory_Of_Mind");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-level health agent setter
 * ============================================================================ */
void theory_of_mind_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_theory_of_mind_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training stubs
 *
 * Stub: training integration planned — these are intentional no-ops that
 * provide heartbeat signaling only. Full training hooks will wire into the
 * training-immune bridge when per-module gradient propagation is implemented.
 * ============================================================================ */
int theory_of_mind_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "theory_of_mind_training_begin: NULL argument");
        return -1;
    }
    theory_of_mind_heartbeat("theory_of_mind_training_begin", 0.0f);
    return 0;
}

int theory_of_mind_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "theory_of_mind_training_end: NULL argument");
        return -1;
    }
    theory_of_mind_heartbeat("theory_of_mind_training_end", 1.0f);
    return 0;
}

int theory_of_mind_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "theory_of_mind_training_step: NULL argument");
        return -1;
    }
    theory_of_mind_heartbeat("theory_of_mind_training_step", progress);
    return 0;
}
