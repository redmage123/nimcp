//=============================================================================
// nimcp_bg_neuromodulators.h - Multi-Neuromodulator Basal Ganglia Integration
//=============================================================================
/**
 * @file nimcp_bg_neuromodulators.h
 * @brief Multiple neuromodulator integration for basal ganglia
 *
 * BIOLOGICAL BASIS:
 * Beyond dopamine, the basal ganglia is modulated by:
 * - Serotonin (5-HT): Patience, aversion, impulse control
 * - Acetylcholine (ACh): Attention, motivation, state transitions
 * - Norepinephrine (NE): Arousal, urgency, uncertainty signaling
 * - Adenosine: Fatigue, effort cost modulation
 *
 * INTEGRATION:
 * - Dopamine: Reward/movement (already implemented)
 * - Serotonin: Opposes impulsive choices, temporal discounting
 * - ACh: Tonically active neurons (TANs) in striatum
 * - NE: Locus coeruleus input for urgency/exploration
 *
 * @version 1.0.0
 * @date 2025-12-30
 */

#ifndef NIMCP_BG_NEUROMODULATORS_H
#define NIMCP_BG_NEUROMODULATORS_H

#include "utils/validation/nimcp_common.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * CONSTANTS
 * ============================================================================ */

/** Number of neuromodulator types */
#define BG_NUM_NEUROMODULATORS      5

/** Baseline levels */
#define BG_DOPAMINE_BASELINE        0.5f
#define BG_SEROTONIN_BASELINE       0.5f
#define BG_ACETYLCHOLINE_BASELINE   0.5f
#define BG_NOREPINEPHRINE_BASELINE  0.3f
#define BG_ADENOSINE_BASELINE       0.2f

/** Receptor sensitivity defaults */
#define BG_D1_SENSITIVITY           1.5f
#define BG_D2_SENSITIVITY           1.2f
#define BG_5HT2A_SENSITIVITY        0.8f
#define BG_M4_SENSITIVITY           0.6f
#define BG_ALPHA2_SENSITIVITY       0.5f

/* ============================================================================
 * ENUMERATIONS
 * ============================================================================ */

/**
 * @brief Neuromodulator types
 */
typedef enum {
    BG_NEUROMOD_DOPAMINE,           /**< Reward, movement */
    BG_NEUROMOD_SEROTONIN,          /**< Patience, aversion */
    BG_NEUROMOD_ACETYLCHOLINE,      /**< Attention, state change */
    BG_NEUROMOD_NOREPINEPHRINE,     /**< Urgency, arousal */
    BG_NEUROMOD_ADENOSINE,          /**< Fatigue, effort cost */
    BG_NEUROMOD_COUNT
} bg_neuromod_type_t;

/**
 * @brief Receptor subtypes for dopamine
 */
typedef enum {
    BG_RECEPTOR_D1,                 /**< Direct pathway facilitation */
    BG_RECEPTOR_D2,                 /**< Indirect pathway inhibition */
    BG_RECEPTOR_D3,                 /**< Limbic modulation */
    BG_RECEPTOR_D4,                 /**< Prefrontal modulation */
    BG_RECEPTOR_D5,                 /**< Hippocampal modulation */
    BG_RECEPTOR_DA_COUNT
} bg_da_receptor_t;

/**
 * @brief Receptor subtypes for serotonin
 */
typedef enum {
    BG_RECEPTOR_5HT1A,              /**< Inhibitory, anxiolytic */
    BG_RECEPTOR_5HT2A,              /**< Excitatory, impulsivity */
    BG_RECEPTOR_5HT2C,              /**< Appetite, mood */
    BG_RECEPTOR_5HT_COUNT
} bg_5ht_receptor_t;

/**
 * @brief Motivational state based on neuromodulator balance
 */
typedef enum {
    BG_MOTIVE_STATE_NEUTRAL,        /**< Balanced state */
    BG_MOTIVE_STATE_APPROACH,       /**< High DA, seeking */
    BG_MOTIVE_STATE_AVOID,          /**< High 5HT, aversion */
    BG_MOTIVE_STATE_ALERT,          /**< High NE, vigilant */
    BG_MOTIVE_STATE_FATIGUED,       /**< High adenosine */
    BG_MOTIVE_STATE_FOCUSED,        /**< High ACh, attentive */
    BG_MOTIVE_STATE_COUNT
} bg_motive_state_t;

/* ============================================================================
 * DATA STRUCTURES
 * ============================================================================ */

/**
 * @brief Neuromodulator levels
 */
typedef struct {
    float dopamine;
    float serotonin;
    float acetylcholine;
    float norepinephrine;
    float adenosine;
} bg_neuromod_levels_t;

/**
 * @brief Receptor activation state
 */
typedef struct {
    /* Dopamine receptors */
    float d1_activation;
    float d2_activation;
    float d3_activation;

    /* Serotonin receptors */
    float ht1a_activation;
    float ht2a_activation;
    float ht2c_activation;

    /* ACh receptors */
    float m4_activation;            /**< Muscarinic M4 in striatum */
    float nicotinic_activation;     /**< Nicotinic in VTA */

    /* NE receptors */
    float alpha1_activation;
    float alpha2_activation;
    float beta_activation;

    /* Adenosine receptors */
    float a1_activation;
    float a2a_activation;           /**< A2A in striatum */
} bg_receptor_state_t;

/**
 * @brief Configuration for neuromodulator system
 */
typedef struct {
    /* Baseline levels */
    bg_neuromod_levels_t baseline;

    /* Receptor sensitivities */
    float d1_sensitivity;
    float d2_sensitivity;
    float ht2a_sensitivity;
    float m4_sensitivity;
    float alpha2_sensitivity;
    float a2a_sensitivity;

    /* Dynamics */
    float release_rate;             /**< How fast neuromodulators are released */
    float reuptake_rate;            /**< How fast they're cleared */
    float synthesis_rate;           /**< How fast reserves are replenished */

    /* Cross-modulation */
    bool enable_da_5ht_interaction; /**< DA-5HT opponent processing */
    bool enable_ach_da_interaction; /**< ACh pauses with DA bursts */
    bool enable_adenosine_da_antagonism; /**< A2A-D2 interaction */

    /* State detection */
    float state_threshold;          /**< Threshold for state detection */
} bg_neuromod_config_t;

/**
 * @brief Effects on basal ganglia processing
 */
typedef struct {
    /* Pathway modulation */
    float direct_pathway_gain;      /**< D1-mediated facilitation */
    float indirect_pathway_gain;    /**< D2-mediated inhibition */
    float hyperdirect_gain;         /**< NE-mediated urgency */

    /* Decision parameters */
    float temporal_discount;        /**< 5HT: patience vs impulsivity */
    float effort_cost;              /**< Adenosine: effort sensitivity */
    float exploration_bonus;        /**< NE: exploration vs exploitation */
    float attention_gate;           /**< ACh: attention gating */

    /* Thresholds */
    float action_threshold_mod;     /**< Combined threshold modulation */
    float habit_threshold_mod;      /**< Habit vs goal-directed balance */

    /* Learning rates */
    float ltp_rate_mod;             /**< Potentiation rate modulation */
    float ltd_rate_mod;             /**< Depression rate modulation */
} bg_neuromod_effects_t;

/**
 * @brief Neuromodulator system statistics
 */
typedef struct {
    bg_neuromod_levels_t current_levels;
    bg_receptor_state_t receptor_state;
    bg_motive_state_t motive_state;
    float da_5ht_ratio;             /**< Approach/avoid balance */
    float arousal_level;            /**< NE-mediated arousal */
    float fatigue_level;            /**< Adenosine-mediated fatigue */
    uint32_t state_transitions;     /**< Number of state changes */
} bg_neuromod_stats_t;

/**
 * @brief Main neuromodulator system handle
 */
typedef struct bg_neuromod_system bg_neuromod_system_t;

/* ============================================================================
 * LIFECYCLE API
 * ============================================================================ */

void bg_neuromod_default_config(bg_neuromod_config_t* config);
bg_neuromod_system_t* bg_neuromod_create(const bg_neuromod_config_t* config);
void bg_neuromod_destroy(bg_neuromod_system_t* system);
int bg_neuromod_reset(bg_neuromod_system_t* system);

/* ============================================================================
 * NEUROMODULATOR CONTROL API
 * ============================================================================ */

/**
 * @brief Set neuromodulator level
 */
int bg_neuromod_set_level(bg_neuromod_system_t* system,
                          bg_neuromod_type_t type,
                          float level);

/**
 * @brief Trigger phasic release (burst)
 */
int bg_neuromod_trigger_release(bg_neuromod_system_t* system,
                                 bg_neuromod_type_t type,
                                 float magnitude);

/**
 * @brief Trigger phasic pause (dip)
 */
int bg_neuromod_trigger_pause(bg_neuromod_system_t* system,
                               bg_neuromod_type_t type,
                               float magnitude);

/**
 * @brief Process reward signal (affects DA, 5HT)
 */
int bg_neuromod_process_reward(bg_neuromod_system_t* system,
                                float reward,
                                float prediction);

/**
 * @brief Process aversive signal (affects 5HT, NE)
 */
int bg_neuromod_process_aversion(bg_neuromod_system_t* system,
                                  float aversion_level);

/**
 * @brief Process uncertainty signal (affects NE, ACh)
 */
int bg_neuromod_process_uncertainty(bg_neuromod_system_t* system,
                                     float uncertainty);

/**
 * @brief Process effort/fatigue (affects adenosine)
 */
int bg_neuromod_process_effort(bg_neuromod_system_t* system,
                                float effort_level);

/**
 * @brief Process attention cue (affects ACh)
 */
int bg_neuromod_process_attention_cue(bg_neuromod_system_t* system,
                                       float salience);

/* ============================================================================
 * PROCESSING API
 * ============================================================================ */

/**
 * @brief Step neuromodulator dynamics
 */
int bg_neuromod_step(bg_neuromod_system_t* system, float dt_ms);

/**
 * @brief Compute effects on BG processing
 */
int bg_neuromod_compute_effects(bg_neuromod_system_t* system,
                                 bg_neuromod_effects_t* effects);

/**
 * @brief Get current motivational state
 */
bg_motive_state_t bg_neuromod_get_state(const bg_neuromod_system_t* system);

/* ============================================================================
 * QUERY API
 * ============================================================================ */

float bg_neuromod_get_level(const bg_neuromod_system_t* system,
                            bg_neuromod_type_t type);

int bg_neuromod_get_all_levels(const bg_neuromod_system_t* system,
                                bg_neuromod_levels_t* levels);

int bg_neuromod_get_receptor_state(const bg_neuromod_system_t* system,
                                    bg_receptor_state_t* state);

int bg_neuromod_get_stats(const bg_neuromod_system_t* system,
                          bg_neuromod_stats_t* stats);

/* ============================================================================
 * INTEGRATION API
 * ============================================================================ */

/**
 * @brief Get D1/D2 pathway modulation for striatum
 */
int bg_neuromod_get_striatal_modulation(const bg_neuromod_system_t* system,
                                         float* d1_mod,
                                         float* d2_mod);

/**
 * @brief Get temporal discounting factor
 */
float bg_neuromod_get_temporal_discount(const bg_neuromod_system_t* system);

/**
 * @brief Get effort cost multiplier
 */
float bg_neuromod_get_effort_cost(const bg_neuromod_system_t* system);

/**
 * @brief Get exploration bonus
 */
float bg_neuromod_get_exploration_bonus(const bg_neuromod_system_t* system);

/**
 * @brief Get attention gate value
 */
float bg_neuromod_get_attention_gate(const bg_neuromod_system_t* system);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BG_NEUROMODULATORS_H */
