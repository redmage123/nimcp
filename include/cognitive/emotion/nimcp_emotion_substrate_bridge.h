/**
 * @file nimcp_emotion_substrate_bridge.h
 * @brief Neural substrate bridge for emotion processing
 *
 * WHAT: Bidirectional integration between neural substrate (ATP, neurotransmitters,
 *       temperature) and emotion system, modeling metabolic effects on emotional
 *       processing and regulation.
 *
 * WHY: Emotions are metabolically expensive processes. Amygdala activity requires
 *      significant ATP, emotion regulation via prefrontal cortex is energy-intensive,
 *      and metabolic stress affects emotional reactivity and valence processing.
 *
 * HOW: Monitors substrate state (ATP, dopamine, serotonin, temperature) and computes
 *      effects on emotion intensity, regulation capacity, reactivity thresholds, and
 *      valence bias. Uses bio-async messaging for inter-module coordination.
 *
 * BIOLOGICAL BASIS:
 * - Amygdala hyperactivity under metabolic stress (anxiety, fear amplification)
 * - ATP depletion impairs prefrontal emotion regulation (reduced control)
 * - Low serotonin causes negative valence bias (depression-like states)
 * - Dopamine modulates emotional reward processing and motivation
 * - Temperature affects emotional reactivity (fever → emotional blunting)
 * - Neurotransmitter balance determines emotional stability
 *
 * Integration Pattern:
 * Substrate → Emotion:
 *   - Low ATP → Reduced regulation, increased reactivity
 *   - Low serotonin → Negative valence bias
 *   - Low dopamine → Reduced positive emotion intensity
 *   - High temperature → Emotional blunting, reduced reactivity
 *
 * Emotion → Substrate:
 *   - High emotion intensity → Increased ATP consumption
 *   - Emotion regulation attempts → ATP drain
 *   - Sustained negative emotion → Serotonin depletion
 */

#ifndef NIMCP_EMOTION_SUBSTRATE_BRIDGE_H
#define NIMCP_EMOTION_SUBSTRATE_BRIDGE_H

#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/nimcp_emotional_system.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/thread/nimcp_thread.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * Bio-Async Module ID
 * ======================================================================== */

/**
 * Bio-async module identifier for emotion substrate bridge
 * Range: 0x1200-0x12FF (Substrate bridges)
 */
#define BIO_MODULE_SUBSTRATE_EMOTION 0x1203

/* ========================================================================
 * ATP Thresholds for Emotion Processing
 * ======================================================================== */

/**
 * ATP thresholds modeling metabolic requirements for emotion processing
 *
 * BIOLOGICAL BASIS:
 * - Amygdala activation requires ~15% baseline ATP
 * - Prefrontal emotion regulation requires ~25% baseline ATP
 * - Emotional reactivity increases below 30% ATP (stress response)
 * - Critical impairment below 10% ATP (emergency mode)
 */

/** Minimum ATP for prefrontal emotion regulation (25%) */
#define EMOTION_SUBSTRATE_ATP_REGULATION_THRESHOLD 0.25f

/** ATP level where emotional reactivity increases (30%) */
#define EMOTION_SUBSTRATE_ATP_REACTIVITY_THRESHOLD 0.30f

/** ATP level for amygdala activation (15%) */
#define EMOTION_SUBSTRATE_ATP_AMYGDALA_THRESHOLD 0.15f

/** Critical ATP level - severe emotion dysregulation (10%) */
#define EMOTION_SUBSTRATE_ATP_CRITICAL_THRESHOLD 0.10f

/** Optimal ATP for balanced emotional processing (70%+) */
#define EMOTION_SUBSTRATE_ATP_OPTIMAL_THRESHOLD 0.70f

/* ========================================================================
 * Neurotransmitter Thresholds
 * ======================================================================== */

/**
 * Neurotransmitter thresholds for emotional modulation
 *
 * BIOLOGICAL BASIS:
 * - Serotonin: 5-HT receptors modulate mood and valence
 * - Dopamine: Reward processing, motivation, positive emotions
 * - Low serotonin (<0.4) → negative valence bias (depression)
 * - Low dopamine (<0.3) → anhedonia, reduced positive affect
 */

/** Serotonin threshold for negative valence bias (40%) */
#define EMOTION_SUBSTRATE_SEROTONIN_VALENCE_THRESHOLD 0.40f

/** Dopamine threshold for positive emotion processing (30%) */
#define EMOTION_SUBSTRATE_DOPAMINE_POSITIVE_THRESHOLD 0.30f

/** Norepinephrine threshold for emotional arousal (50%) */
#define EMOTION_SUBSTRATE_NOREPINEPHRINE_AROUSAL_THRESHOLD 0.50f

/* ========================================================================
 * Temperature Thresholds
 * ======================================================================== */

/**
 * Temperature thresholds for emotional reactivity
 *
 * BIOLOGICAL BASIS:
 * - Normal: 37.0°C - optimal emotional processing
 * - Mild fever: 37.5°C - slight emotional blunting
 * - Moderate fever: 38.5°C - reduced emotional reactivity
 * - High fever: 39.5°C+ - severe emotional suppression
 */

/** Normal body temperature (Celsius) */
#define EMOTION_SUBSTRATE_TEMP_NORMAL 37.0f

/** Mild fever threshold - slight emotional blunting */
#define EMOTION_SUBSTRATE_TEMP_MILD_FEVER 37.5f

/** Moderate fever - reduced emotional reactivity */
#define EMOTION_SUBSTRATE_TEMP_MODERATE_FEVER 38.5f

/** High fever - severe emotional suppression */
#define EMOTION_SUBSTRATE_TEMP_HIGH_FEVER 39.5f

/* ========================================================================
 * Substrate Effects on Emotion
 * ======================================================================== */

/**
 * @brief Computed substrate effects on emotion processing
 *
 * WHAT: Quantifies how metabolic state affects emotional processing
 *
 * WHY: Substrate state determines emotional capacity, stability, and regulation
 *
 * HOW: Computed from ATP, neurotransmitters, and temperature measurements
 */
typedef struct {
    /** Emotion intensity scaling factor [0-2]
     * <0.5: Blunted emotions (metabolic stress, high fever)
     * 0.5-1.5: Normal range
     * >1.5: Heightened emotions (low ATP, stress response) */
    float intensity_modulation;

    /** Emotion regulation capacity [0-1]
     * Prefrontal control over emotional responses
     * Requires ATP > 25% for effective regulation */
    float regulation_capacity;

    /** Reactivity threshold [0-1]
     * Lower = more reactive to emotional stimuli
     * Increases with low ATP (stress sensitization) */
    float reactivity_threshold;

    /** Valence bias [-1 to +1]
     * Negative: Bias toward negative emotions (low serotonin)
     * Positive: Bias toward positive emotions (high dopamine)
     * Zero: Balanced processing */
    float valence_bias;

    /** Emotion processing impairment flag
     * True when ATP < critical threshold */
    bool is_impaired;
} emotion_substrate_effects_t;

/* ========================================================================
 * Bridge Configuration
 * ======================================================================== */

/**
 * @brief Configuration for emotion substrate bridge
 *
 * WHAT: Controls which substrate features affect emotion processing
 *
 * WHY: Allows selective testing and tuning of substrate-emotion interactions
 *
 * HOW: Enable/disable specific features and set sensitivity parameters
 */
typedef struct {
    /** Enable ATP-based emotion modulation */
    bool enable_atp_modulation;

    /** Enable serotonin valence bias */
    bool enable_serotonin_modulation;

    /** Enable dopamine positive emotion modulation */
    bool enable_dopamine_modulation;

    /** Enable norepinephrine arousal modulation */
    bool enable_norepinephrine_modulation;

    /** Enable temperature-based emotional blunting */
    bool enable_temperature_modulation;

    /** Sensitivity to ATP changes [0-1], default 0.8 */
    float atp_sensitivity;

    /** Sensitivity to neurotransmitter changes [0-1], default 0.7 */
    float neurotransmitter_sensitivity;

    /** Sensitivity to temperature changes [0-1], default 0.6 */
    float temperature_sensitivity;

    /** Update interval in milliseconds (default 100ms) */
    uint32_t update_interval_ms;

    /** Enable bio-async messaging */
    bool enable_bio_async;
} emotion_substrate_config_t;

/* ========================================================================
 * Bridge Statistics
 * ======================================================================== */

/**
 * @brief Statistics for emotion substrate bridge monitoring
 *
 * WHAT: Tracks substrate effects on emotion processing over time
 *
 * WHY: Enables analysis of metabolic-emotional interactions
 *
 * HOW: Accumulates min/max/average effects during operation
 */
typedef struct {
    /** Total number of substrate updates processed */
    uint64_t total_updates;

    /** Number of times emotion was impaired (low ATP) */
    uint64_t impairment_count;

    /** Average intensity modulation across updates */
    float avg_intensity_modulation;

    /** Minimum regulation capacity observed */
    float min_regulation_capacity;

    /** Maximum reactivity threshold observed */
    float max_reactivity_threshold;

    /** Average valence bias (positive/negative tendency) */
    float avg_valence_bias;

    /** Number of negative valence bias events (low serotonin) */
    uint64_t negative_bias_events;

    /** Number of positive valence bias events (high dopamine) */
    uint64_t positive_bias_events;

    /** Number of high reactivity events (low ATP stress) */
    uint64_t high_reactivity_events;

    /** Number of emotional blunting events (fever) */
    uint64_t blunting_events;

    /** Timestamp of last update (milliseconds) */
    uint64_t last_update_time;
} emotion_substrate_stats_t;

/* ========================================================================
 * Bridge Structure
 * ======================================================================== */

/**
 * @brief Emotion substrate bridge instance
 *
 * WHAT: Manages bidirectional integration between substrate and emotion
 *
 * WHY: Coordinates metabolic effects on emotional processing
 *
 * HOW: Monitors substrate state, computes effects, applies to emotion system
 */
typedef struct emotion_substrate_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    /** Pointer to neural substrate */
    neural_substrate_t* substrate;

    /** Pointer to emotion system */
    emotional_system_t* emotion_system;

    /** Bridge configuration */
    emotion_substrate_config_t config;

    /** Computed substrate effects on emotion */
    emotion_substrate_effects_t effects;

    /** Bridge statistics */
    emotion_substrate_stats_t stats;

    /** Bio-async module context */
    /** Bio-async enabled flag */
    /** Bridge initialization flag */
    bool initialized;
} emotion_substrate_bridge_t;

/* ========================================================================
 * Configuration Functions
 * ======================================================================== */

/**
 * @brief Initialize emotion substrate configuration with default values
 *
 * WHAT: Sets recommended default configuration for emotion substrate bridge
 *
 * WHY: Provides scientifically-grounded baseline parameters
 *
 * HOW: Enables all modulation features with balanced sensitivities
 *
 * @param config Configuration structure to initialize
 *
 * Default values:
 * - All modulation features enabled
 * - ATP sensitivity: 0.8 (high sensitivity to metabolic state)
 * - Neurotransmitter sensitivity: 0.7 (moderate-high)
 * - Temperature sensitivity: 0.6 (moderate)
 * - Update interval: 100ms
 * - Bio-async enabled
 */
void emotion_substrate_default_config(emotion_substrate_config_t* config);

/* ========================================================================
 * Lifecycle Functions
 * ======================================================================== */

/**
 * @brief Create emotion substrate bridge
 *
 * WHAT: Allocates and initializes emotion substrate bridge
 *
 * WHY: Establishes connection between substrate and emotion systems
 *
 * HOW: Creates bridge structure, initializes effects, connects bio-async
 *
 * @param config Bridge configuration (NULL for defaults)
 * @param emotion_system Emotion system instance
 * @param substrate Neural substrate instance
 * @return Pointer to created bridge, NULL on failure
 *
 * BIOLOGICAL INTEGRATION:
 * - Connects metabolic monitoring to emotional processing
 * - Establishes ATP-emotion regulation pathway
 * - Links neurotransmitter levels to valence processing
 */
emotion_substrate_bridge_t* emotion_substrate_bridge_create(
    const emotion_substrate_config_t* config,
    emotional_system_t* emotion_system,
    neural_substrate_t* substrate
);

/**
 * @brief Destroy emotion substrate bridge
 *
 * WHAT: Cleanup and deallocate emotion substrate bridge
 *
 * WHY: Proper resource management and bio-async disconnection
 *
 * HOW: Disconnects bio-async, releases mutex, frees memory
 *
 * @param bridge Bridge instance to destroy
 */
void emotion_substrate_bridge_destroy(emotion_substrate_bridge_t* bridge);

/* ========================================================================
 * Bio-Async Integration
 * ======================================================================== */

/**
 * @brief Connect bridge to bio-async messaging system
 *
 * WHAT: Registers bridge with bio-async router for inter-module communication
 *
 * WHY: Enables coordination with other substrate bridges and emotion modules
 *
 * HOW: Registers module ID, creates inbox, establishes message routes
 *
 * @param bridge Bridge instance
 * @return 0 on success, negative on failure
 */
int emotion_substrate_connect_bio_async(emotion_substrate_bridge_t* bridge);

/**
 * @brief Disconnect bridge from bio-async system
 *
 * WHAT: Unregisters bridge from bio-async router
 *
 * WHY: Clean shutdown of messaging infrastructure
 *
 * HOW: Deregisters module, clears inbox, releases resources
 *
 * @param bridge Bridge instance
 * @return 0 on success, negative on failure
 */
int emotion_substrate_disconnect_bio_async(emotion_substrate_bridge_t* bridge);

/**
 * @brief Check if bridge is connected to bio-async
 *
 * @param bridge Bridge instance
 * @return true if connected, false otherwise
 */
bool emotion_substrate_is_bio_async_connected(const emotion_substrate_bridge_t* bridge);

/* ========================================================================
 * Update Functions
 * ======================================================================== */

/**
 * @brief Update substrate effects on emotion processing
 *
 * WHAT: Recomputes emotion effects based on current substrate state
 *
 * WHY: Keeps emotion modulation synchronized with metabolic state
 *
 * HOW: Reads ATP, neurotransmitters, temperature; computes effects
 *
 * @param bridge Bridge instance
 * @return 0 on success, negative on failure
 *
 * COMPUTATION:
 * 1. Read substrate state (ATP, serotonin, dopamine, temperature)
 * 2. Compute intensity modulation (ATP-dependent)
 * 3. Compute regulation capacity (prefrontal ATP requirement)
 * 4. Compute reactivity threshold (stress sensitization)
 * 5. Compute valence bias (serotonin/dopamine balance)
 * 6. Set impairment flag if ATP < critical
 * 7. Update statistics
 */
int emotion_substrate_update(emotion_substrate_bridge_t* bridge);

/* ========================================================================
 * Query Functions
 * ======================================================================== */

/**
 * @brief Get emotion intensity modulation factor
 *
 * WHAT: Returns scaling factor for emotion intensity
 *
 * WHY: Emotion system needs to scale responses based on metabolic state
 *
 * HOW: Returns computed intensity modulation [0-2]
 *
 * @param bridge Bridge instance
 * @return Intensity modulation factor (1.0 = normal, <1 = blunted, >1 = heightened)
 *
 * INTERPRETATION:
 * - 0.0-0.5: Severe blunting (high fever, extreme stress)
 * - 0.5-0.8: Mild blunting (fever, fatigue)
 * - 0.8-1.2: Normal range
 * - 1.2-1.5: Heightened (mild stress, increased arousal)
 * - 1.5-2.0: Hyperreactive (low ATP stress response)
 */
float emotion_substrate_get_intensity_mod(const emotion_substrate_bridge_t* bridge);

/**
 * @brief Get emotion regulation capacity
 *
 * WHAT: Returns capacity for prefrontal emotion regulation
 *
 * WHY: Determines ability to control emotional responses
 *
 * HOW: Returns computed regulation capacity [0-1]
 *
 * @param bridge Bridge instance
 * @return Regulation capacity (0 = no control, 1 = full control)
 *
 * BIOLOGICAL BASIS:
 * - Requires ATP > 25% for effective regulation
 * - Prefrontal cortex is metabolically expensive
 * - Low ATP → loss of top-down emotional control
 */
float emotion_substrate_get_regulation_capacity(const emotion_substrate_bridge_t* bridge);

/**
 * @brief Get emotional reactivity threshold
 *
 * WHAT: Returns threshold for emotional responses to stimuli
 *
 * WHY: Determines sensitivity to emotional triggers
 *
 * HOW: Returns computed reactivity threshold [0-1]
 *
 * @param bridge Bridge instance
 * @return Reactivity threshold (lower = more reactive)
 *
 * BIOLOGICAL BASIS:
 * - Low ATP → increased amygdala reactivity (stress sensitization)
 * - High temperature → reduced reactivity (emotional blunting)
 * - Norepinephrine → increased arousal and reactivity
 */
float emotion_substrate_get_reactivity_threshold(const emotion_substrate_bridge_t* bridge);

/**
 * @brief Get valence bias
 *
 * WHAT: Returns bias toward positive or negative emotions
 *
 * WHY: Neurotransmitter imbalances affect emotional valence processing
 *
 * HOW: Returns computed valence bias [-1 to +1]
 *
 * @param bridge Bridge instance
 * @return Valence bias (-1 = negative bias, 0 = balanced, +1 = positive bias)
 *
 * BIOLOGICAL BASIS:
 * - Low serotonin → negative bias (depression-like)
 * - High dopamine → positive bias (reward-seeking)
 * - Serotonin-dopamine balance determines valence processing
 */
float emotion_substrate_get_valence_bias(const emotion_substrate_bridge_t* bridge);

/**
 * @brief Get complete substrate effects structure
 *
 * WHAT: Returns full effects structure with all modulation factors
 *
 * WHY: Efficient access to all effects in single call
 *
 * HOW: Returns copy of current effects structure
 *
 * @param bridge Bridge instance
 * @return Complete effects structure
 */
emotion_substrate_effects_t emotion_substrate_get_effects(const emotion_substrate_bridge_t* bridge);

/**
 * @brief Check if emotion processing is impaired
 *
 * WHAT: Returns impairment status flag
 *
 * WHY: Critical indicator of severe metabolic compromise
 *
 * HOW: Returns true if ATP < critical threshold
 *
 * @param bridge Bridge instance
 * @return true if emotion processing impaired, false otherwise
 *
 * IMPAIRMENT CONDITIONS:
 * - ATP < 10% (critical metabolic failure)
 * - Severe emotion dysregulation expected
 * - Emergency mode - basic survival emotions only
 */
bool emotion_substrate_is_impaired(const emotion_substrate_bridge_t* bridge);

/**
 * @brief Get bridge statistics
 *
 * WHAT: Returns statistical summary of substrate-emotion interactions
 *
 * WHY: Monitoring and analysis of metabolic effects on emotion
 *
 * HOW: Returns copy of accumulated statistics
 *
 * @param bridge Bridge instance
 * @return Bridge statistics structure
 */
emotion_substrate_stats_t emotion_substrate_get_stats(const emotion_substrate_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_EMOTION_SUBSTRATE_BRIDGE_H */
