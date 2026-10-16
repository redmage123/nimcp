/**
 * @file nimcp_entorhinal_hypothalamus_bridge.h
 * @brief Entorhinal-Hypothalamus Bidirectional Bridge
 * @version Phase 5: Memory Circuit
 * @date 2025-01-12
 *
 * WHAT: Bridge connecting entorhinal cortex to hypothalamus for homeostatic
 *       and motivational integration with spatial memory.
 *
 * WHY:  The hypothalamus modulates memory encoding based on:
 *       - Motivational state (hunger, thirst, safety needs)
 *       - Homeostatic drives (temperature, energy balance)
 *       - Reward prediction and value assignment
 *       - Circadian rhythms affecting memory consolidation
 *
 * HOW:  Bidirectional data flow:
 *       - Hypothalamus -> Entorhinal: motivation signals, reward predictions
 *       - Entorhinal -> Hypothalamus: spatial context, memory salience
 *
 * BIOLOGICAL BASIS:
 * - Lateral hypothalamus: feeding, reward, motivation
 * - Medial hypothalamus: satiety, defensive behaviors
 * - Suprachiasmatic nucleus: circadian modulation
 * - Mammillary bodies: memory circuit (Papez circuit)
 */

#ifndef NIMCP_ENTORHINAL_HYPOTHALAMUS_BRIDGE_H
#define NIMCP_ENTORHINAL_HYPOTHALAMUS_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/*=============================================================================
 * FORWARD DECLARATIONS
 *===========================================================================*/

typedef struct nimcp_entorhinal nimcp_entorhinal_t;
typedef struct hypothalamus_adapter hypothalamus_adapter_t;
typedef struct nimcp_brain_handle* nimcp_brain_t;

/*=============================================================================
 * HYPOTHALAMIC NUCLEI ENUMERATION
 *===========================================================================*/

/**
 * @brief Hypothalamic nuclei involved in memory modulation
 */
typedef enum {
    HYPOTHAL_NUCLEUS_LATERAL = 0,       /* Reward, motivation, feeding */
    HYPOTHAL_NUCLEUS_VENTROMEDIAL,      /* Satiety, defensive behavior */
    HYPOTHAL_NUCLEUS_DORSOMEDIAL,       /* Circadian rhythm, stress */
    HYPOTHAL_NUCLEUS_PARAVENTRICULAR,   /* Stress response, HPA axis */
    HYPOTHAL_NUCLEUS_SUPRACHIASMATIC,   /* Master circadian clock */
    HYPOTHAL_NUCLEUS_ARCUATE,           /* Energy homeostasis */
    HYPOTHAL_NUCLEUS_MAMMILLARY,        /* Memory (Papez circuit) */
    HYPOTHAL_NUCLEUS_POSTERIOR,         /* Arousal, wakefulness */
    HYPOTHAL_NUCLEUS_COUNT
} hypothalamic_nucleus_t;

/*=============================================================================
 * MOTIVATIONAL STATE STRUCTURE
 *===========================================================================*/

/**
 * @brief Current motivational state from hypothalamus
 */
typedef struct {
    /* Primary drives */
    float hunger_drive;             /* [0,1] hunger level */
    float thirst_drive;             /* [0,1] thirst level */
    float safety_drive;             /* [0,1] safety/threat avoidance */
    float exploration_drive;        /* [0,1] novelty seeking */
    float social_drive;             /* [0,1] social interaction need */

    /* Reward and value */
    float reward_prediction;        /* Expected reward value */
    float reward_prediction_error;  /* RPE for learning */
    float value_signal;             /* Integrated value estimate */

    /* Arousal and circadian */
    float arousal_level;            /* [0,1] wakefulness */
    float circadian_phase;          /* [0, 2*PI] time of day */
    float sleep_pressure;           /* [0,1] need for sleep */

    /* Stress and homeostasis */
    float stress_level;             /* [0,1] HPA axis activity */
    float temperature_deviation;    /* Deviation from setpoint */
    float energy_balance;           /* Positive=surplus, negative=deficit */

    /* Modulation outputs */
    float memory_encoding_boost;    /* Boost to encoding strength */
    float consolidation_gate;       /* Gate for memory consolidation */
    float attention_bias;           /* Bias toward survival-relevant stimuli */
} hypothalamic_motivational_state_t;

/*=============================================================================
 * SPATIAL-MOTIVATIONAL BINDING
 *===========================================================================*/

/**
 * @brief Binds spatial location to motivational significance
 */
typedef struct {
    float position[3];              /* Spatial location */
    float value;                    /* Motivational value at location */
    float uncertainty;              /* Value uncertainty */
    uint32_t visit_count;           /* Number of visits */
    float last_reward;              /* Last reward received here */
    float avg_reward;               /* Average reward at location */
    uint64_t last_visit_time_ms;    /* Time of last visit */
} spatial_value_binding_t;

/**
 * @brief Value map over space
 */
typedef struct {
    spatial_value_binding_t* bindings;
    uint32_t num_bindings;
    uint32_t max_bindings;
    float spatial_resolution;       /* Grid resolution in meters */
    float decay_rate;               /* Value decay over time */
    float learning_rate;            /* Value learning rate */
} spatial_value_map_t;

/*=============================================================================
 * BRIDGE CONFIGURATION
 *===========================================================================*/

/**
 * @brief Hypothalamus bridge configuration
 */
typedef struct {
    /* Connection settings */
    bool enable_motivation_modulation;
    bool enable_reward_learning;
    bool enable_circadian_modulation;
    bool enable_stress_modulation;
    bool enable_value_mapping;

    /* Modulation strengths */
    float motivation_encoding_weight;   /* How much motivation affects encoding */
    float reward_plasticity_weight;     /* Reward modulation of plasticity */
    float circadian_consolidation_weight; /* Circadian effect on consolidation */
    float stress_memory_weight;         /* Stress effect on memory (inverted U) */

    /* Value map settings */
    uint32_t max_value_bindings;
    float value_map_resolution;
    float value_decay_rate;
    float value_learning_rate;

    /* Update rates */
    float motivation_update_rate_hz;
    float value_map_update_rate_hz;

    /* Thresholds */
    float high_motivation_threshold;
    float stress_impairment_threshold;
    float consolidation_circadian_peak;  /* Circadian phase for peak consolidation */
} entorhinal_hypothalamus_config_t;

/*=============================================================================
 * BRIDGE STATE STRUCTURE
 *===========================================================================*/

/**
 * @brief Full hypothalamus bridge state
 */
typedef struct {
    /* Configuration */
    entorhinal_hypothalamus_config_t config;

    /* Connected systems */
    nimcp_entorhinal_t* entorhinal;
    hypothalamus_adapter_t* hypothalamus;

    /* Current motivational state */
    hypothalamic_motivational_state_t motivation;

    /* Spatial value map */
    spatial_value_map_t* value_map;

    /* Nucleus activity levels */
    float nucleus_activity[HYPOTHAL_NUCLEUS_COUNT];

    /* Modulation outputs */
    float encoding_modulation;      /* Current encoding strength modifier */
    float retrieval_modulation;     /* Current retrieval strength modifier */
    float plasticity_modulation;    /* Current plasticity rate modifier */
    float consolidation_modulation; /* Current consolidation rate modifier */

    /* State tracking */
    bool connected;
    uint64_t last_update_ms;
    uint64_t updates_processed;

    /* Statistics */
    float mean_motivation_signal;
    float mean_reward_prediction;
    uint64_t value_map_updates;
    float mean_encoding_boost;
} entorhinal_hypothalamus_bridge_state_t;

/*=============================================================================
 * LIFECYCLE API
 *===========================================================================*/

/**
 * @brief Get default configuration
 */
entorhinal_hypothalamus_config_t entorhinal_hypothalamus_default_config(void);

/**
 * @brief Create hypothalamus bridge
 */
entorhinal_hypothalamus_bridge_state_t* entorhinal_hypothalamus_bridge_create(
    const entorhinal_hypothalamus_config_t* config);

/**
 * @brief Destroy hypothalamus bridge
 */
void entorhinal_hypothalamus_bridge_destroy(
    entorhinal_hypothalamus_bridge_state_t* bridge);

/**
 * @brief Connect bridge to entorhinal and hypothalamus
 */
int entorhinal_hypothalamus_bridge_connect(
    entorhinal_hypothalamus_bridge_state_t* bridge,
    nimcp_entorhinal_t* entorhinal,
    hypothalamus_adapter_t* hypothalamus);

/**
 * @brief Disconnect bridge
 */
int entorhinal_hypothalamus_bridge_disconnect(
    entorhinal_hypothalamus_bridge_state_t* bridge);

/**
 * @brief Reset bridge state
 */
int entorhinal_hypothalamus_bridge_reset(
    entorhinal_hypothalamus_bridge_state_t* bridge);

/*=============================================================================
 * BIDIRECTIONAL DATA FLOW API
 *===========================================================================*/

/**
 * @brief Update bridge (full bidirectional cycle)
 */
int entorhinal_hypothalamus_bridge_update(
    entorhinal_hypothalamus_bridge_state_t* bridge,
    float dt);

/**
 * @brief Receive motivational state from hypothalamus
 */
int entorhinal_hypothalamus_receive_motivation(
    entorhinal_hypothalamus_bridge_state_t* bridge,
    const hypothalamic_motivational_state_t* motivation);

/**
 * @brief Send spatial context to hypothalamus
 */
int entorhinal_hypothalamus_send_spatial_context(
    entorhinal_hypothalamus_bridge_state_t* bridge,
    const float* position, uint32_t dim,
    float memory_salience);

/**
 * @brief Process reward signal for value learning
 */
int entorhinal_hypothalamus_process_reward(
    entorhinal_hypothalamus_bridge_state_t* bridge,
    float reward,
    const float* position, uint32_t dim);

/*=============================================================================
 * MODULATION API
 *===========================================================================*/

/**
 * @brief Get current encoding modulation factor
 */
float entorhinal_hypothalamus_get_encoding_modulation(
    const entorhinal_hypothalamus_bridge_state_t* bridge);

/**
 * @brief Get current retrieval modulation factor
 */
float entorhinal_hypothalamus_get_retrieval_modulation(
    const entorhinal_hypothalamus_bridge_state_t* bridge);

/**
 * @brief Get current plasticity modulation factor
 */
float entorhinal_hypothalamus_get_plasticity_modulation(
    const entorhinal_hypothalamus_bridge_state_t* bridge);

/**
 * @brief Get consolidation gate value
 */
float entorhinal_hypothalamus_get_consolidation_gate(
    const entorhinal_hypothalamus_bridge_state_t* bridge);

/**
 * @brief Apply motivational modulation to encoding
 */
int entorhinal_hypothalamus_modulate_encoding(
    entorhinal_hypothalamus_bridge_state_t* bridge,
    float* encoding_strength);

/*=============================================================================
 * VALUE MAP API
 *===========================================================================*/

/**
 * @brief Get value at spatial location
 */
float entorhinal_hypothalamus_get_spatial_value(
    const entorhinal_hypothalamus_bridge_state_t* bridge,
    const float* position, uint32_t dim);

/**
 * @brief Update value at spatial location
 */
int entorhinal_hypothalamus_update_spatial_value(
    entorhinal_hypothalamus_bridge_state_t* bridge,
    const float* position, uint32_t dim,
    float reward);

/**
 * @brief Get gradient of value map at location
 */
int entorhinal_hypothalamus_get_value_gradient(
    const entorhinal_hypothalamus_bridge_state_t* bridge,
    const float* position, uint32_t dim,
    float* gradient_out);

/**
 * @brief Decay all values in map
 */
int entorhinal_hypothalamus_decay_value_map(
    entorhinal_hypothalamus_bridge_state_t* bridge,
    float dt);

/*=============================================================================
 * CIRCADIAN API
 *===========================================================================*/

/**
 * @brief Get circadian modulation of consolidation
 */
float entorhinal_hypothalamus_get_circadian_consolidation(
    const entorhinal_hypothalamus_bridge_state_t* bridge);

/**
 * @brief Check if in optimal consolidation window
 */
bool entorhinal_hypothalamus_in_consolidation_window(
    const entorhinal_hypothalamus_bridge_state_t* bridge);

/*=============================================================================
 * DIAGNOSTICS API
 *===========================================================================*/

/**
 * @brief Get bridge statistics
 */
int entorhinal_hypothalamus_bridge_get_stats(
    const entorhinal_hypothalamus_bridge_state_t* bridge,
    uint64_t* updates_processed,
    float* mean_motivation,
    float* mean_encoding_boost);

/**
 * @brief Log bridge diagnostics
 */
int entorhinal_hypothalamus_bridge_log_diagnostics(
    const entorhinal_hypothalamus_bridge_state_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_ENTORHINAL_HYPOTHALAMUS_BRIDGE_H */
