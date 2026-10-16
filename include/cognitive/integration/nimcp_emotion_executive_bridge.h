/**
 * @file nimcp_emotion_executive_bridge.h
 * @brief Bridge between Emotion and Executive Control systems
 *
 * WHAT: Bidirectional integration where emotions influence executive decisions
 *       and executive functions regulate emotional responses.
 *
 * WHY: Emotions and executive control are deeply interdependent. Emotions provide
 *      motivational salience and valuation for decisions; executive functions
 *      enable top-down regulation of emotional responses.
 *
 * HOW: Emotional state biases decision-making through valuation and urgency signals.
 *      Executive control exerts top-down regulation to modulate emotional intensity
 *      and select appropriate responses.
 *
 * BIOLOGICAL BASIS:
 * - Emotions involve amygdala, ventromedial PFC (vmPFC), and insula
 * - Executive control relies on dorsolateral PFC (dlPFC) and ACC
 * - Emotion -> Executive: Amygdala/vmPFC signals influence dlPFC decisions
 * - Executive -> Emotion: dlPFC exerts top-down inhibition of amygdala
 * - ACC monitors conflict between emotional impulses and executive goals
 *
 * Integration Pattern:
 * Emotion -> Executive:
 *   - Emotional valence biases option evaluation
 *   - Emotional urgency modulates decision speed
 *   - Emotional state affects risk tolerance
 *   - Somatic markers guide gut-level decisions
 *
 * Executive -> Emotion:
 *   - Cognitive reappraisal changes emotional meaning
 *   - Attention deployment redirects emotional focus
 *   - Response modulation suppresses/enhances expression
 *   - Situation selection avoids emotional triggers
 *
 * @author NIMCP Development Team
 * @date 2025-01
 */

#ifndef NIMCP_EMOTION_EXECUTIVE_BRIDGE_H
#define NIMCP_EMOTION_EXECUTIVE_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Opaque Type
 * ============================================================================ */

/**
 * @brief Opaque Emotion-Executive bridge structure
 *
 * WHAT: Forward declaration for Emotion-Executive bridge
 * WHY: Encapsulates implementation details
 * HOW: Full definition in implementation file
 */
typedef struct emotion_executive_bridge emotion_executive_bridge_t;

/* ============================================================================
 * Constants
 * ============================================================================ */

/**
 * @brief Default emotion influence weight
 */
#define EMOTION_EXECUTIVE_DEFAULT_INFLUENCE_WEIGHT 0.3f

/**
 * @brief Default regulation strength
 */
#define EMOTION_EXECUTIVE_DEFAULT_REGULATION_STRENGTH 0.5f

/**
 * @brief Emotion type identifiers
 */
typedef enum {
    EMOTION_EXECUTIVE_TYPE_JOY = 0,        /**< Joy/happiness */
    EMOTION_EXECUTIVE_TYPE_SADNESS,        /**< Sadness/grief */
    EMOTION_EXECUTIVE_TYPE_FEAR,           /**< Fear/anxiety */
    EMOTION_EXECUTIVE_TYPE_ANGER,          /**< Anger/frustration */
    EMOTION_EXECUTIVE_TYPE_DISGUST,        /**< Disgust/aversion */
    EMOTION_EXECUTIVE_TYPE_SURPRISE,       /**< Surprise/startle */
    EMOTION_EXECUTIVE_TYPE_TRUST,          /**< Trust/acceptance */
    EMOTION_EXECUTIVE_TYPE_ANTICIPATION    /**< Anticipation/interest */
} emotion_executive_emotion_type_t;

/**
 * @brief Regulation strategy identifiers
 */
typedef enum {
    EMOTION_EXECUTIVE_REG_REAPPRAISAL = 0, /**< Cognitive reappraisal */
    EMOTION_EXECUTIVE_REG_SUPPRESSION,     /**< Expression suppression */
    EMOTION_EXECUTIVE_REG_DISTRACTION,     /**< Attention distraction */
    EMOTION_EXECUTIVE_REG_ACCEPTANCE,      /**< Acceptance/allowing */
    EMOTION_EXECUTIVE_REG_SITUATION_MOD    /**< Situation modification */
} emotion_executive_regulation_type_t;

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief Configuration for Emotion-Executive bridge
 *
 * WHAT: Parameters controlling emotion-executive integration
 *
 * WHY: Different scenarios require different balances between emotional
 *      influence on decisions and executive regulation of emotions
 *
 * HOW: Configure influence weights, regulation strength, and thresholds
 */
typedef struct {
    /** Weight of emotional input on decisions [0-1] (default: 0.3)
     *  Higher values make decisions more emotion-influenced */
    float emotion_influence_weight;

    /** Strength of executive regulation [0-1] (default: 0.5)
     *  Higher values enable stronger emotion regulation */
    float regulation_strength;

    /** Threshold for decision commitment [0-1] (default: 0.7)
     *  Higher values require more certainty before deciding */
    float decision_threshold;

    /** Enable automatic emotion-based decision biasing */
    bool enable_emotional_biasing;

    /** Enable automatic regulation when emotions exceed threshold */
    bool enable_auto_regulation;

    /** Emotion intensity threshold for auto-regulation [0-1] */
    float regulation_trigger_threshold;

    /** Maximum regulation attempts per emotion episode */
    uint32_t max_regulation_attempts;
} emotion_executive_config_t;

/**
 * @brief Decision context for emotional influence
 *
 * WHAT: Context information for a pending decision
 *
 * WHY: Emotional influence depends on decision characteristics
 *
 * HOW: Contains decision parameters that emotions can bias
 */
typedef struct {
    /** Decision identifier */
    uint32_t decision_id;

    /** Number of options being considered */
    uint32_t option_count;

    /** Time pressure [0-1] (higher = more urgent) */
    float time_pressure;

    /** Stakes/importance [0-1] (higher = more important) */
    float stakes;

    /** Uncertainty level [0-1] (higher = less certain) */
    float uncertainty;

    /** Risk level [0-1] (higher = riskier options) */
    float risk_level;
} emotion_executive_decision_context_t;

/**
 * @brief Emotional bias on decision
 *
 * WHAT: Emotional influence on a pending decision
 *
 * WHY: Emotions provide valuation signals that bias decisions
 *
 * HOW: Contains bias direction, magnitude, and urgency
 */
typedef struct {
    /** Valence bias [-1 to +1] (negative/positive option preference) */
    float valence_bias;

    /** Approach/avoid tendency [-1 to +1] (avoid/approach) */
    float approach_avoid;

    /** Urgency modifier [0-2] (1.0 = no change) */
    float urgency_modifier;

    /** Risk tolerance modifier [0-2] (1.0 = no change) */
    float risk_tolerance_modifier;

    /** Confidence in emotional signal [0-1] */
    float signal_confidence;

    /** Dominant emotion type */
    emotion_executive_emotion_type_t dominant_emotion;

    /** Intensity of dominant emotion [0-1] */
    float emotion_intensity;
} emotion_executive_emotional_bias_t;

/**
 * @brief Decision outcome for emotional response
 *
 * WHAT: Outcome of an executive decision
 *
 * WHY: Decision outcomes trigger emotional responses
 *
 * HOW: Contains outcome valence and expectation violation
 */
typedef struct {
    /** Decision identifier */
    uint32_t decision_id;

    /** Outcome valence [-1 to +1] (bad/good outcome) */
    float outcome_valence;

    /** Expectation violation [-1 to +1] (worse/better than expected) */
    float expectation_violation;

    /** Was decision successful */
    bool success;

    /** Time taken for decision (ms) */
    uint32_t decision_time_ms;
} emotion_executive_decision_outcome_t;

/**
 * @brief Regulation target specification
 *
 * WHAT: Target parameters for emotion regulation
 *
 * WHY: Executive control needs specific regulation goals
 *
 * HOW: Contains target emotion and desired intensity
 */
typedef struct {
    /** Target emotion type */
    emotion_executive_emotion_type_t target_emotion;

    /** Target intensity [0-1] (desired level) */
    float target_intensity;

    /** Regulation strategy to use */
    emotion_executive_regulation_type_t strategy;

    /** Maximum time for regulation (ms) */
    uint32_t max_duration_ms;
} emotion_executive_regulation_target_t;

/**
 * @brief Current emotional state
 *
 * WHAT: Summary of current emotional state
 *
 * WHY: Enables querying overall emotional influence on executive
 *
 * HOW: Aggregates key emotional metrics
 */
typedef struct {
    /** Overall valence [-1 to +1] */
    float valence;

    /** Overall arousal [0-1] */
    float arousal;

    /** Dominant emotion type */
    emotion_executive_emotion_type_t dominant_emotion;

    /** Dominant emotion intensity [0-1] */
    float dominant_intensity;

    /** Emotional stability [0-1] (higher = more stable) */
    float stability;

    /** Is regulation currently active */
    bool regulation_active;

    /** Current regulation effectiveness [0-1] */
    float regulation_effectiveness;
} emotion_executive_emotional_state_t;

/**
 * @brief Statistics for Emotion-Executive bridge
 *
 * WHAT: Performance and activity metrics for the bridge
 *
 * WHY: Monitor integration health and emotion-decision dynamics
 *
 * HOW: Accumulates counts during bridge operation
 */
typedef struct {
    /** Number of decisions influenced by emotion */
    uint64_t decisions_influenced;

    /** Number of emotions triggered by decisions */
    uint64_t emotions_triggered;

    /** Number of regulation attempts applied */
    uint64_t regulations_applied;

    /** Number of successful regulations */
    uint64_t successful_regulations;

    /** Average emotional bias magnitude */
    float avg_bias_magnitude;

    /** Average regulation effectiveness [0-1] */
    float avg_regulation_effectiveness;

    /** Number of regulation failures */
    uint64_t regulation_failures;

    /** Number of emotion-decision conflicts detected */
    uint64_t conflicts_detected;
} emotion_executive_stats_t;

/* ============================================================================
 * API Functions
 * ============================================================================ */

/**
 * @brief Initialize default Emotion-Executive configuration
 *
 * WHAT: Sets default parameters for Emotion-Executive bridge
 * WHY: Provides sensible defaults for typical use cases
 * HOW: Initializes config with balanced parameters
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
int emotion_executive_default_config(emotion_executive_config_t* config);

/**
 * @brief Create Emotion-Executive bridge
 *
 * WHAT: Allocates and initializes Emotion-Executive integration bridge
 * WHY: Establishes bidirectional link between emotion and executive systems
 * HOW: Creates bridge, initializes state tracking, sets up regulation pipeline
 *
 * @param config Bridge configuration (NULL for defaults)
 * @return Pointer to created bridge, NULL on failure
 */
emotion_executive_bridge_t* emotion_executive_bridge_create(
    const emotion_executive_config_t* config
);

/**
 * @brief Destroy Emotion-Executive bridge
 *
 * WHAT: Cleans up and deallocates bridge
 * WHY: Prevents memory leaks and releases resources
 * HOW: Clears state, frees memory, deallocates bridge
 *
 * @param bridge Bridge to destroy
 */
void emotion_executive_bridge_destroy(emotion_executive_bridge_t* bridge);

/**
 * @brief Get emotional influence on decision
 *
 * WHAT: Computes emotional bias for a pending decision
 * WHY: Emotions provide valuation signals that should inform decisions
 * HOW: Queries emotional state, computes bias based on decision context
 *
 * @param bridge Bridge instance
 * @param decision_context Context of the pending decision
 * @param emotional_bias_out Output buffer for emotional bias
 * @return 0 on success, -1 on error
 *
 * BIOLOGICAL BASIS: Amygdala/vmPFC provide somatic marker signals that
 *                   bias dlPFC decision processes toward certain options.
 */
int emotion_executive_influence_decision(
    emotion_executive_bridge_t* bridge,
    const emotion_executive_decision_context_t* decision_context,
    emotion_executive_emotional_bias_t* emotional_bias_out
);

/**
 * @brief Process decision outcome to trigger emotional response
 *
 * WHAT: Decision outcome triggers appropriate emotional response
 * WHY: Decision outcomes have emotional consequences
 * HOW: Evaluates outcome, generates emotional response
 *
 * @param bridge Bridge instance
 * @param decision_id ID of the completed decision
 * @param outcome Outcome of the decision
 * @return 0 on success, -1 on error
 *
 * BIOLOGICAL BASIS: Decision outcomes processed by vmPFC/ACC generate
 *                   reward/punishment signals that update emotional state.
 */
int emotion_executive_on_decision(
    emotion_executive_bridge_t* bridge,
    uint32_t decision_id,
    const emotion_executive_decision_outcome_t* outcome
);

/**
 * @brief Apply executive regulation to emotion
 *
 * WHAT: Executive control regulates emotional intensity/expression
 * WHY: Top-down regulation enables adaptive emotional responses
 * HOW: Applies specified regulation strategy to target emotion
 *
 * @param bridge Bridge instance
 * @param emotion_type Emotion type to regulate
 * @param regulation_target Target parameters for regulation
 * @return 0 on success, -1 on error
 *
 * BIOLOGICAL BASIS: dlPFC exerts top-down inhibition of amygdala activity,
 *                   enabling cognitive reappraisal and response modulation.
 */
int emotion_executive_regulate_emotion(
    emotion_executive_bridge_t* bridge,
    emotion_executive_emotion_type_t emotion_type,
    const emotion_executive_regulation_target_t* regulation_target
);

/**
 * @brief Get current emotional state affecting executive
 *
 * WHAT: Retrieves current emotional state summary
 * WHY: Enables monitoring of emotional influence on executive
 * HOW: Copies current emotional state to output buffer
 *
 * @param bridge Bridge instance
 * @param state_out Output buffer for emotional state
 * @return 0 on success, -1 on error
 */
int emotion_executive_get_emotional_state(
    emotion_executive_bridge_t* bridge,
    emotion_executive_emotional_state_t* state_out
);

/**
 * @brief Get bridge statistics
 *
 * WHAT: Retrieves performance and activity metrics
 * WHY: Monitor bridge health and emotion-decision dynamics
 * HOW: Copies current statistics to output buffer
 *
 * @param bridge Bridge instance
 * @param stats_out Output buffer for statistics
 * @return 0 on success, -1 on error
 */
int emotion_executive_get_stats(
    const emotion_executive_bridge_t* bridge,
    emotion_executive_stats_t* stats_out
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_EMOTION_EXECUTIVE_BRIDGE_H */
