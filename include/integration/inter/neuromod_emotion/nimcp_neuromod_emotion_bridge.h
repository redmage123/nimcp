/**
 * @file nimcp_neuromod_emotion_bridge.h
 * @brief Neuromodulatory-Emotion Inter-Layer Bridge
 * @version 1.0.0
 * @date 2026-01-12
 *
 * WHAT: Bridges neuromodulatory nuclei to emotional processing systems (amygdala, insula)
 * WHY:  Emotions are fundamentally driven by neuromodulatory state (NE=arousal, DA=valence, 5-HT=regulation)
 * HOW:  LC/NE controls arousal, VTA/DA controls valence, 5-HT enables regulation, Habenula signals aversion
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * LOCUS COERULEUS - EMOTIONAL AROUSAL:
 * The LC-NE system is the primary controller of emotional arousal:
 * - NE release: Amplifies emotional responses (both positive and negative)
 * - Stress response: High NE = anxiety, hypervigilance, panic
 * - Amygdala modulation: NE potentiates amygdala fear responses
 * - Memory consolidation: NE enhances emotional memory formation
 *
 * VTA - EMOTIONAL VALENCE:
 * Dopamine from VTA controls the hedonic/valence dimension:
 * - Reward/pleasure: DA release = positive emotional valence
 * - Motivation: DA drives approach behaviors and positive anticipation
 * - Anhedonia: Low DA = loss of pleasure, depression
 * - Wanting vs Liking: DA mediates "wanting" (motivation) more than "liking" (pleasure)
 *
 * RAPHE - EMOTIONAL REGULATION:
 * Serotonin from Raphe enables emotional control:
 * - Mood stability: 5-HT maintains baseline emotional equilibrium
 * - Impulse control: 5-HT inhibits impulsive emotional reactions
 * - Anxiety modulation: 5-HT reduces anxiety (SSRIs work here)
 * - Emotional flexibility: 5-HT enables adaptive emotional responses
 *
 * HABENULA - AVERSION PROCESSING:
 * The habenula signals disappointment and aversion:
 * - Negative prediction errors: Active when expected reward fails
 * - Learned helplessness: Chronic activation leads to depression
 * - VTA/Raphe inhibition: Reduces DA/5-HT, increasing negative affect
 *
 * KEY PATHWAYS:
 * =============
 * Bottom-Up (Neuromodulatory -> Emotion):
 * - NE levels -> emotional arousal/intensity
 * - DA levels -> emotional valence (positive/negative)
 * - 5-HT levels -> emotional regulation capacity
 * - Habenula activity -> aversion/disappointment signals
 *
 * Top-Down (Emotion -> Neuromodulatory):
 * - Fear/threat -> LC activation (arousal)
 * - Reward anticipation -> VTA activation
 * - Emotional conflict -> 5-HT demand
 * - Disappointment -> Habenula activation
 *
 * ARCHITECTURE:
 * ```
 * +===========================================================================+
 * |               NEUROMODULATORY-EMOTION INTER-LAYER BRIDGE                  |
 * +===========================================================================+
 * |                                                                           |
 * |   NEUROMODULATORY LAYER              EMOTION LAYER                        |
 * |   +-------------------+              +-------------------+                |
 * |   | LC (NE)           |------------->| Arousal System    |                |
 * |   | - Stress response |              | - Intensity       |                |
 * |   | - Vigilance       |              | - Fight/Flight    |                |
 * |   +-------------------+              +-------------------+                |
 * |                                                                           |
 * |   +-------------------+              +-------------------+                |
 * |   | VTA (DA)          |------------->| Valence System    |                |
 * |   | - Reward          |              | - Pleasure/Pain   |                |
 * |   | - Motivation      |              | - Approach/Avoid  |                |
 * |   +-------------------+              +-------------------+                |
 * |                                                                           |
 * |   +-------------------+              +-------------------+                |
 * |   | Raphe (5-HT)      |------------->| Regulation System |                |
 * |   | - Mood stability  |              | - Impulse control |                |
 * |   | - Anxiety control |              | - Flexibility     |                |
 * |   +-------------------+              +-------------------+                |
 * |                                                                           |
 * |   +-------------------+              +-------------------+                |
 * |   | Habenula          |------------->| Aversion System   |                |
 * |   | - Disappointment  |              | - Negative affect |                |
 * |   | - Learned helpless|              | - Withdrawal      |                |
 * |   +-------------------+              +-------------------+                |
 * |                                                                           |
 * |                        TOP-DOWN FEEDBACK                                  |
 * |   +-------------------+              +-------------------+                |
 * |   | LC Trigger        |<-------------| Fear/Threat       |                |
 * |   | VTA Activation    |<-------------| Reward Anticipate |                |
 * |   | 5-HT Demand       |<-------------| Conflict/Stress   |                |
 * |   | Hab Activation    |<-------------| Disappointment    |                |
 * |   +-------------------+              +-------------------+                |
 * |                                                                           |
 * +===========================================================================+
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_NEUROMOD_EMOTION_BRIDGE_H
#define NIMCP_NEUROMOD_EMOTION_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define NEUROMOD_EMOTION_BRIDGE_MAGIC   0x4E454D42  /* "NEMB" */

/* Bridge message types */
#define NEURO_EMO_MSG_AROUSAL           0x0070
#define NEURO_EMO_MSG_VALENCE           0x0071
#define NEURO_EMO_MSG_REGULATION        0x0072
#define NEURO_EMO_MSG_AVERSION          0x0073
#define NEURO_EMO_MSG_FEAR_TRIGGER      0x0074
#define NEURO_EMO_MSG_REWARD_ANTICIPATE 0x0075
#define NEURO_EMO_MSG_CONFLICT_STRESS   0x0076
#define NEURO_EMO_MSG_DISAPPOINTMENT    0x0077

/* Biological constants */
#define NE_AROUSAL_COUPLING             0.7f    /* NE-to-arousal coupling strength */
#define DA_VALENCE_COUPLING             0.6f    /* DA-to-valence coupling strength */
#define HT_REGULATION_COUPLING          0.5f    /* 5-HT-to-regulation coupling */
#define HAB_AVERSION_COUPLING           0.6f    /* Habenula-to-aversion coupling */
#define AMYGDALA_NE_POTENTIATION        0.4f    /* NE potentiation of amygdala */

/* Emotional state thresholds */
#define AROUSAL_ANXIETY_THRESHOLD       0.7f    /* Arousal level triggering anxiety */
#define VALENCE_ANHEDONIA_THRESHOLD     0.2f    /* Low valence indicating anhedonia */
#define REGULATION_DYSREGULATION_THRESH 0.3f    /* Low regulation indicating dysregulation */

/* ============================================================================
 * Types
 * ============================================================================ */

typedef struct neuromod_emotion_bridge_struct neuromod_emotion_bridge_t;

/**
 * @brief Emotional state representation
 */
typedef enum {
    EMOTION_STATE_NEUTRAL = 0,
    EMOTION_STATE_POSITIVE_LOW,      /* Content, calm satisfaction */
    EMOTION_STATE_POSITIVE_HIGH,     /* Excited, euphoric */
    EMOTION_STATE_NEGATIVE_LOW,      /* Sad, melancholic */
    EMOTION_STATE_NEGATIVE_HIGH,     /* Angry, fearful, panicked */
    EMOTION_STATE_ANXIOUS,           /* High arousal, negative valence */
    EMOTION_STATE_ANHEDONIC,         /* Low arousal, low valence */
    EMOTION_STATE_DYSREGULATED       /* Poor emotional control */
} emotional_state_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* NE-Arousal coupling */
    float ne_arousal_coupling;           /**< NE-to-arousal coupling strength [0-1] */
    float ne_amygdala_potentiation;      /**< NE potentiation of amygdala [0-1] */
    float arousal_anxiety_threshold;     /**< Arousal threshold for anxiety [0-1] */

    /* DA-Valence coupling */
    float da_valence_coupling;           /**< DA-to-valence coupling [0-1] */
    float da_motivation_coupling;        /**< DA-to-motivation coupling [0-1] */
    float valence_anhedonia_threshold;   /**< Valence threshold for anhedonia [0-1] */

    /* 5-HT-Regulation coupling */
    float ht_regulation_coupling;        /**< 5-HT-to-regulation coupling [0-1] */
    float ht_anxiety_reduction;          /**< 5-HT anxiety reduction strength [0-1] */
    float regulation_dysreg_threshold;   /**< Threshold for dysregulation [0-1] */

    /* Habenula-Aversion coupling */
    float hab_aversion_coupling;         /**< Habenula-to-aversion coupling [0-1] */
    float hab_learned_helplessness;      /**< Habenula contribution to helplessness [0-1] */

    /* Top-down feedback */
    float fear_lc_trigger_gain;          /**< Fear-to-LC coupling [0-1] */
    float reward_vta_trigger_gain;       /**< Reward anticipation-to-VTA coupling [0-1] */
    float conflict_ht_demand_gain;       /**< Conflict-to-5-HT demand [0-1] */
    float disappoint_hab_trigger_gain;   /**< Disappointment-to-Habenula coupling [0-1] */

    /* Timing */
    uint32_t update_interval_ms;         /**< Update interval (default: 10ms) */

    /* Features */
    bool enable_arousal_modulation;      /**< Enable NE arousal effects */
    bool enable_valence_modulation;      /**< Enable DA valence effects */
    bool enable_regulation_modulation;   /**< Enable 5-HT regulation effects */
    bool enable_aversion_modulation;     /**< Enable habenula aversion effects */
    bool enable_state_classification;    /**< Enable emotional state classification */
    bool enable_logging;                 /**< Enable event logging */
} neuromod_emotion_config_t;

/**
 * @brief Current bridge state
 */
typedef struct {
    /* Core emotional dimensions */
    float arousal_level;                 /**< Current arousal/intensity [0-1] */
    float valence_level;                 /**< Current valence [-1=negative, +1=positive] */
    float regulation_capacity;           /**< Current regulation capacity [0-1] */
    float aversion_level;                /**< Current aversion/disappointment [0-1] */

    /* Derived states */
    float anxiety_level;                 /**< Computed anxiety (high arousal + negative) [0-1] */
    float motivation_level;              /**< Computed motivation (DA-driven) [0-1] */
    emotional_state_t current_state;     /**< Classified emotional state */

    /* Neuromodulator levels (cached) */
    float ne_level;                      /**< Current NE level [0-1] */
    float da_level;                      /**< Current DA level [0-1] */
    float ht_level;                      /**< Current 5-HT level [0-1] */
    float hab_level;                     /**< Current habenula activity [0-1] */

    /* Top-down signals */
    float fear_signal;                   /**< Recent fear/threat detection [0-1] */
    float reward_anticipation;           /**< Recent reward anticipation [0-1] */
    float conflict_signal;               /**< Recent conflict/stress [0-1] */
    float disappointment_signal;         /**< Recent disappointment [0-1] */

    /* Metrics */
    float emotional_stability;           /**< Overall emotional stability [0-1] */
    float bridge_coherence;              /**< Bottom-up/top-down coherence [0-1] */
    uint64_t last_update_us;             /**< Last update timestamp */
} neuromod_emotion_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    /* Bottom-up events */
    uint32_t arousal_modulations;        /**< Arousal modulation events */
    uint32_t valence_modulations;        /**< Valence modulation events */
    uint32_t regulation_modulations;     /**< Regulation modulation events */
    uint32_t aversion_events;            /**< Aversion signal events */

    /* State classifications */
    uint32_t anxiety_episodes;           /**< Times anxiety threshold crossed */
    uint32_t anhedonia_episodes;         /**< Times anhedonia detected */
    uint32_t dysregulation_episodes;     /**< Times dysregulation detected */

    /* Top-down events */
    uint32_t fear_triggers;              /**< Fear-to-LC trigger events */
    uint32_t reward_triggers;            /**< Reward-to-VTA trigger events */
    uint32_t conflict_signals;           /**< Conflict-to-5-HT signals */
    uint32_t disappointment_signals;     /**< Disappointment-to-Hab signals */

    /* Aggregates */
    float avg_arousal;                   /**< Average arousal level */
    float avg_valence;                   /**< Average valence level */
    float avg_stability;                 /**< Average emotional stability */

    uint64_t total_updates;              /**< Total update cycles */
    uint64_t bottom_up_messages;         /**< Total bottom-up messages */
    uint64_t top_down_messages;          /**< Total top-down messages */
} neuromod_emotion_stats_t;

/* ============================================================================
 * API Functions
 * ============================================================================ */

/* Lifecycle */
neuromod_emotion_config_t neuromod_emotion_default_config(void);
neuromod_emotion_bridge_t* neuromod_emotion_create(const neuromod_emotion_config_t* config);
void neuromod_emotion_destroy(neuromod_emotion_bridge_t* bridge);

/* Bottom-up modulation (neuromod -> emotion) */
int neuromod_emotion_apply_ne_arousal(neuromod_emotion_bridge_t* bridge, float ne_level, float* arousal_out);
int neuromod_emotion_apply_da_valence(neuromod_emotion_bridge_t* bridge, float da_level, float* valence_out);
int neuromod_emotion_apply_ht_regulation(neuromod_emotion_bridge_t* bridge, float ht_level, float* regulation_out);
int neuromod_emotion_apply_hab_aversion(neuromod_emotion_bridge_t* bridge, float hab_level, float* aversion_out);

/* Top-down feedback (emotion -> neuromod) */
int neuromod_emotion_report_fear(neuromod_emotion_bridge_t* bridge, float fear_intensity, float* lc_trigger_out);
int neuromod_emotion_report_reward_anticipation(neuromod_emotion_bridge_t* bridge, float anticipation, float* vta_trigger_out);
int neuromod_emotion_report_conflict(neuromod_emotion_bridge_t* bridge, float conflict_level, float* ht_demand_out);
int neuromod_emotion_report_disappointment(neuromod_emotion_bridge_t* bridge, float disappointment, float* hab_trigger_out);

/* Unified modulation */
int neuromod_emotion_compute_modulation(neuromod_emotion_bridge_t* bridge,
                                        float ne_level, float da_level,
                                        float ht_level, float hab_level,
                                        neuromod_emotion_state_t* state_out);

/* State classification */
emotional_state_t neuromod_emotion_classify_state(const neuromod_emotion_bridge_t* bridge);
const char* neuromod_emotion_state_name(emotional_state_t state);

/* Update and state */
int neuromod_emotion_update(neuromod_emotion_bridge_t* bridge, float delta_ms);
int neuromod_emotion_get_state(const neuromod_emotion_bridge_t* bridge, neuromod_emotion_state_t* state_out);
int neuromod_emotion_get_stats(const neuromod_emotion_bridge_t* bridge, neuromod_emotion_stats_t* stats_out);
int neuromod_emotion_reset_stats(neuromod_emotion_bridge_t* bridge);

/* Diagnostics */
bool neuromod_emotion_is_connected(const neuromod_emotion_bridge_t* bridge);
float neuromod_emotion_get_stability(const neuromod_emotion_bridge_t* bridge);
float neuromod_emotion_get_coherence(const neuromod_emotion_bridge_t* bridge);
void neuromod_emotion_print_summary(const neuromod_emotion_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_NEUROMOD_EMOTION_BRIDGE_H */
