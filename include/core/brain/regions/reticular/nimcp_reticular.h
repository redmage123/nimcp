/**
 * @file nimcp_reticular.h
 * @brief Reticular Formation - Arousal, Consciousness, and Vital Functions
 *
 * WHAT: Neural substrate for arousal control, consciousness regulation,
 *       motor tone modulation, autonomic regulation, and sensory gating
 * WHY:  Critical for alertness, sleep-wake cycles, vital reflexes,
 *       and the integration of consciousness with bodily functions
 * HOW:  Implements ascending reticular activating system (ARAS),
 *       descending motor control, autonomic centers, and pain modulation
 *
 * BIOLOGICAL BASIS:
 * - Extends from medulla through pons to midbrain
 * - ARAS projects to thalamus, hypothalamus, and cortex for arousal
 * - Reticulospinal tracts modulate motor tone and posture
 * - Contains cardiovascular and respiratory centers
 * - Integrates with PAG for pain modulation
 *
 * NUCLEI IMPLEMENTED:
 * - Raphe Nuclei: Serotonin release, mood, sleep
 * - Locus Coeruleus: Norepinephrine, vigilance, attention
 * - Pontine Reticular Formation: REM sleep, motor control
 * - Medullary Reticular Formation: Vital reflexes, autonomic
 * - Pedunculopontine Nucleus: Acetylcholine, arousal, locomotion
 * - Gigantocellular Nucleus: Motor tone, posture
 * - Parvocellular Nucleus: Respiratory rhythm
 * - Lateral Tegmental Nucleus: Norepinephrine projections
 *
 * FULL INTEGRATION WITH:
 * - Security module, Immune system, KG wiring, Bio-async
 * - SNN/STDP/Plasticity, Hypothalamus, Omnidirectional
 * - Cognitive/Training layers, Perception, Symbolic logic
 * - Swarm, Dragonfly, Portia, Logging, Thalamic, Neural substrate
 * - Quantum algorithms (QMC, QMCTS), Math utilities, Threading
 * - PAG (defensive behavior), Brain initialization
 *
 * @version 1.0
 * @date 2026-01-13
 */

#ifndef NIMCP_RETICULAR_H
#define NIMCP_RETICULAR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Platform and utilities */
#include "utils/platform/nimcp_platform_tier.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/validation/nimcp_common.h"

/* Forward declarations for integration */
struct nimcp_brain_kg;
struct nimcp_bio_router;
struct nimcp_immune_system;
struct nimcp_security_context;
struct nimcp_snn_network;
struct nimcp_plasticity_engine;
struct nimcp_hypothalamus;
struct nimcp_thalamus;
struct nimcp_cognitive_hub;
struct nimcp_training_context;
struct nimcp_perception_system;
struct nimcp_symbolic_engine;
struct nimcp_swarm_context;
struct nimcp_dragonfly_context;
struct nimcp_portia_context;
struct nimcp_qmc_context;
struct nimcp_omni_predictor;
struct nimcp_pag;
struct nimcp_neural_substrate;
struct nimcp_raphe_system;
struct nimcp_locus_coeruleus;

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

#define RETICULAR_MAX_NUCLEI              16
#define RETICULAR_MAX_PROJECTIONS         32
#define RETICULAR_MAX_REFLEX_PATTERNS     24
#define RETICULAR_MAX_AUTONOMIC_ZONES     8
#define RETICULAR_MAX_NAME_LEN            64
#define RETICULAR_HISTORY_SIZE            128

/* Default physiological parameters */
#define RETICULAR_DEFAULT_AROUSAL         0.5f    /**< Baseline arousal */
#define RETICULAR_DEFAULT_TONIC_RATE      5.0f    /**< Hz baseline firing */
#define RETICULAR_ALERT_THRESHOLD         0.7f    /**< Arousal for alert state */
#define RETICULAR_SLEEP_THRESHOLD         0.3f    /**< Arousal for sleep onset */
#define RETICULAR_MODULE_ID               0x5100

/*=============================================================================
 * RETICULAR NUCLEI TYPES
 *===========================================================================*/

/**
 * @brief Reticular formation nuclei with distinct functional roles
 *
 * BIOLOGICAL BASIS:
 * - Raphe: Serotonin, mood, sleep-wake
 * - Locus Coeruleus: Norepinephrine, vigilance
 * - Pontine: REM, motor, arousal relay
 * - Medullary: Vital centers, reflexes
 */
typedef enum {
    /* Serotonergic nuclei */
    RETICULAR_NUCLEUS_RAPHE_DORSAL = 0,     /**< Dorsal raphe - 5-HT, mood */
    RETICULAR_NUCLEUS_RAPHE_MEDIAN,          /**< Median raphe - 5-HT, anxiety */
    RETICULAR_NUCLEUS_RAPHE_MAGNUS,          /**< Raphe magnus - pain modulation */
    RETICULAR_NUCLEUS_RAPHE_OBSCURUS,        /**< Raphe obscurus - autonomic */

    /* Noradrenergic nuclei */
    RETICULAR_NUCLEUS_LOCUS_COERULEUS,       /**< LC - NE, vigilance, attention */
    RETICULAR_NUCLEUS_LATERAL_TEGMENTAL,     /**< A5/A7 - NE projections */

    /* Cholinergic nuclei */
    RETICULAR_NUCLEUS_PEDUNCULOPONTINE,      /**< PPN - ACh, arousal, locomotion */
    RETICULAR_NUCLEUS_LATERODORSAL_TEGMENTAL,/**< LDT - ACh, REM, cortical activation */

    /* Pontine reticular formation */
    RETICULAR_NUCLEUS_PONTINE_ORAL,          /**< Pontis oralis - REM atonia */
    RETICULAR_NUCLEUS_PONTINE_CAUDAL,        /**< Pontis caudalis - startle */

    /* Medullary reticular formation */
    RETICULAR_NUCLEUS_GIGANTOCELLULAR,       /**< Motor tone, posture */
    RETICULAR_NUCLEUS_PARVOCELLULAR,         /**< Respiratory rhythm */
    RETICULAR_NUCLEUS_PARAMEDIAN,            /**< Eye movements, gaze */
    RETICULAR_NUCLEUS_VENTRAL_MEDULLARY,     /**< Cardiovascular control */

    /* Dopaminergic (ventral tegmental) */
    RETICULAR_NUCLEUS_VTA,                   /**< VTA - DA, reward, motivation */

    RETICULAR_NUCLEUS_COUNT
} reticular_nucleus_t;

/*=============================================================================
 * AROUSAL STATES
 *===========================================================================*/

/**
 * @brief Arousal state classification
 *
 * BIOLOGICAL BASIS: Maps to EEG patterns and behavioral states
 */
typedef enum {
    RETICULAR_AROUSAL_DEEP_SLEEP = 0,   /**< Stage 3-4 NREM, delta waves */
    RETICULAR_AROUSAL_LIGHT_SLEEP,       /**< Stage 1-2 NREM, theta/spindles */
    RETICULAR_AROUSAL_REM_SLEEP,         /**< REM, desynchronized EEG, atonia */
    RETICULAR_AROUSAL_DROWSY,            /**< Pre-sleep, alpha dropout */
    RETICULAR_AROUSAL_RELAXED,           /**< Quiet wakefulness, alpha */
    RETICULAR_AROUSAL_ALERT,             /**< Active attention, beta */
    RETICULAR_AROUSAL_HYPERVIGILANT,     /**< High arousal, stress, gamma */
    RETICULAR_AROUSAL_COUNT
} reticular_arousal_state_t;

/*=============================================================================
 * NEUROMODULATOR OUTPUTS
 *===========================================================================*/

/**
 * @brief Neuromodulator types released by reticular formation
 */
typedef enum {
    RETICULAR_MODULATOR_SEROTONIN = 0,   /**< 5-HT from raphe nuclei */
    RETICULAR_MODULATOR_NOREPINEPHRINE,  /**< NE from locus coeruleus */
    RETICULAR_MODULATOR_ACETYLCHOLINE,   /**< ACh from PPN/LDT */
    RETICULAR_MODULATOR_DOPAMINE,        /**< DA from VTA */
    RETICULAR_MODULATOR_HISTAMINE,       /**< HA from TMN (associated) */
    RETICULAR_MODULATOR_OREXIN,          /**< Orexin from hypothalamus input */
    RETICULAR_MODULATOR_GABA,            /**< Inhibitory output */
    RETICULAR_MODULATOR_GLUTAMATE,       /**< Excitatory output */
    RETICULAR_MODULATOR_COUNT
} reticular_modulator_t;

/**
 * @brief Neuromodulator state
 */
typedef struct {
    reticular_modulator_t type;
    float concentration;        /**< Current level [0, 1] normalized */
    float release_rate;         /**< Release rate per update */
    float decay_rate;           /**< Decay time constant */
    float baseline;             /**< Baseline level */
    float target_effect;        /**< Effect on target regions */
    uint64_t last_update_us;
} reticular_modulator_state_t;

/*=============================================================================
 * AUTONOMIC CONTROL
 *===========================================================================*/

/**
 * @brief Autonomic function types
 */
typedef enum {
    RETICULAR_AUTONOMIC_CARDIOVASCULAR = 0, /**< Heart rate, blood pressure */
    RETICULAR_AUTONOMIC_RESPIRATORY,        /**< Breathing rate, depth */
    RETICULAR_AUTONOMIC_VASOMOTOR,          /**< Vascular tone */
    RETICULAR_AUTONOMIC_DIGESTIVE,          /**< GI motility */
    RETICULAR_AUTONOMIC_COUNT
} reticular_autonomic_t;

/**
 * @brief Autonomic state
 */
typedef struct {
    reticular_autonomic_t type;
    float sympathetic_tone;     /**< Sympathetic activity [0, 1] */
    float parasympathetic_tone; /**< Parasympathetic activity [0, 1] */
    float balance;              /**< Sympathovagal balance [-1, 1] */
    float setpoint;             /**< Target value */
    float current_value;        /**< Measured value */
} reticular_autonomic_state_t;

/*=============================================================================
 * REFLEX CONTROL
 *===========================================================================*/

/**
 * @brief Reflex types mediated by reticular formation
 */
typedef enum {
    RETICULAR_REFLEX_SWALLOWING = 0,    /**< Deglutition */
    RETICULAR_REFLEX_COUGHING,          /**< Airway protection */
    RETICULAR_REFLEX_VOMITING,          /**< Emesis */
    RETICULAR_REFLEX_SNEEZING,          /**< Nasal irritation */
    RETICULAR_REFLEX_GAGGING,           /**< Pharyngeal protection */
    RETICULAR_REFLEX_YAWNING,           /**< Arousal modulation */
    RETICULAR_REFLEX_STARTLE,           /**< Acoustic startle */
    RETICULAR_REFLEX_RIGHTING,          /**< Postural correction */
    RETICULAR_REFLEX_COUNT
} reticular_reflex_t;

/**
 * @brief Reflex state
 */
typedef struct {
    reticular_reflex_t type;
    float threshold;            /**< Activation threshold */
    float gain;                 /**< Response magnitude */
    float current_activation;   /**< Current trigger level */
    bool active;                /**< Reflex currently triggered */
    uint32_t trigger_count;     /**< Times triggered */
    uint64_t last_triggered_us;
} reticular_reflex_state_t;

/*=============================================================================
 * MOTOR CONTROL
 *===========================================================================*/

/**
 * @brief Motor tone regulation state
 */
typedef struct {
    float postural_tone;        /**< Antigravity muscle tone [0, 1] */
    float limb_tone;            /**< Limb muscle tone [0, 1] */
    float atonia_level;         /**< REM atonia depth [0, 1] */
    float locomotor_drive;      /**< Mesencephalic locomotor region */
    float startle_readiness;    /**< Startle response readiness */
    bool rem_atonia_active;     /**< REM sleep muscle inhibition */
} reticular_motor_state_t;

/*=============================================================================
 * PAIN MODULATION
 *===========================================================================*/

/**
 * @brief Descending pain modulation state
 */
typedef struct {
    float gate_control;         /**< Spinal gate closure [0, 1] */
    float endogenous_analgesia; /**< Opioid-mediated analgesia [0, 1] */
    float serotonin_analgesia;  /**< 5-HT descending inhibition */
    float noradrenergic_mod;    /**< NE descending modulation */
    float stress_analgesia;     /**< Stress-induced analgesia */
    float pain_threshold;       /**< Current pain threshold */
} reticular_pain_state_t;

/*=============================================================================
 * SENSORY GATING
 *===========================================================================*/

/**
 * @brief Thalamic relay modulation for sensory gating
 */
typedef struct {
    float thalamic_gate;        /**< Overall thalamic filtering [0, 1] */
    float visual_gate;          /**< LGN gating */
    float auditory_gate;        /**< MGN gating */
    float somatosensory_gate;   /**< VPL/VPM gating */
    float attention_bias;       /**< Attention-driven gating */
    float habituation_level;    /**< Stimulus habituation */
} reticular_sensory_gate_t;

/*=============================================================================
 * NUCLEUS STATE
 *===========================================================================*/

/**
 * @brief Individual nucleus state
 */
typedef struct {
    reticular_nucleus_t type;
    char name[RETICULAR_MAX_NAME_LEN];

    /* Activity state */
    float activity;             /**< Current activity [0, 1] */
    float firing_rate;          /**< Firing rate (Hz) */
    float baseline_rate;        /**< Baseline firing rate */

    /* Inputs */
    float excitatory_input;
    float inhibitory_input;

    /* Outputs */
    reticular_modulator_t primary_modulator;
    float modulator_output;     /**< Amount released */

    /* Projections */
    uint32_t projection_count;
    bool enabled;
} reticular_nucleus_state_t;

/*=============================================================================
 * BIO-ASYNC MESSAGE TYPES
 *===========================================================================*/

/**
 * @brief Reticular bio-async message types
 */
typedef enum {
    RETICULAR_BIO_MSG_AROUSAL_CHANGE = 0,   /**< Arousal state transition */
    RETICULAR_BIO_MSG_SLEEP_STAGE,           /**< Sleep stage change */
    RETICULAR_BIO_MSG_NEUROMODULATOR,        /**< Neuromodulator release */
    RETICULAR_BIO_MSG_AUTONOMIC,             /**< Autonomic state change */
    RETICULAR_BIO_MSG_REFLEX_TRIGGER,        /**< Reflex triggered */
    RETICULAR_BIO_MSG_MOTOR_TONE,            /**< Motor tone change */
    RETICULAR_BIO_MSG_PAIN_MODULATION,       /**< Pain gate change */
    RETICULAR_BIO_MSG_SENSORY_GATE,          /**< Sensory gating update */
    RETICULAR_BIO_MSG_ATTENTION_ALERT,       /**< Attention signal */
    RETICULAR_BIO_MSG_STATE_REQUEST,         /**< State query */
    RETICULAR_BIO_MSG_COUNT
} reticular_bio_msg_type_t;

/**
 * @brief Subscription masks for bio-async
 */
#define RETICULAR_BIO_SUB_AROUSAL       (1U << RETICULAR_BIO_MSG_AROUSAL_CHANGE)
#define RETICULAR_BIO_SUB_SLEEP         (1U << RETICULAR_BIO_MSG_SLEEP_STAGE)
#define RETICULAR_BIO_SUB_MODULATOR     (1U << RETICULAR_BIO_MSG_NEUROMODULATOR)
#define RETICULAR_BIO_SUB_AUTONOMIC     (1U << RETICULAR_BIO_MSG_AUTONOMIC)
#define RETICULAR_BIO_SUB_REFLEX        (1U << RETICULAR_BIO_MSG_REFLEX_TRIGGER)
#define RETICULAR_BIO_SUB_MOTOR         (1U << RETICULAR_BIO_MSG_MOTOR_TONE)
#define RETICULAR_BIO_SUB_PAIN          (1U << RETICULAR_BIO_MSG_PAIN_MODULATION)
#define RETICULAR_BIO_SUB_SENSORY       (1U << RETICULAR_BIO_MSG_SENSORY_GATE)
#define RETICULAR_BIO_SUB_ATTENTION     (1U << RETICULAR_BIO_MSG_ATTENTION_ALERT)
#define RETICULAR_BIO_SUB_ALL           (0xFFFFFFFFU)

/*=============================================================================
 * KG WIRING INTEGRATION
 *===========================================================================*/

/**
 * @brief KG node types for reticular formation
 */
typedef enum {
    RETICULAR_KG_NODE_REGION = 0,       /**< Main region node */
    RETICULAR_KG_NODE_NUCLEUS,          /**< Individual nucleus node */
    RETICULAR_KG_NODE_AROUSAL_STATE,    /**< Arousal state node */
    RETICULAR_KG_NODE_MODULATOR,        /**< Neuromodulator node */
    RETICULAR_KG_NODE_AUTONOMIC,        /**< Autonomic function node */
    RETICULAR_KG_NODE_CONNECTION        /**< Connection/edge node */
} reticular_kg_node_type_t;

/**
 * @brief KG wiring state
 */
typedef struct {
    uint64_t region_node_id;
    uint64_t nucleus_node_ids[RETICULAR_NUCLEUS_COUNT];
    uint64_t arousal_node_ids[RETICULAR_AROUSAL_COUNT];
    uint64_t modulator_node_ids[RETICULAR_MODULATOR_COUNT];
    uint32_t edge_count;
    bool registered;
    uint64_t admin_token;
} reticular_kg_state_t;

/*=============================================================================
 * STATISTICS
 *===========================================================================*/

/**
 * @brief Reticular operational statistics
 */
typedef struct {
    /* State transitions */
    uint64_t arousal_transitions;
    uint64_t sleep_cycles;
    uint64_t wake_episodes;

    /* Activity */
    uint64_t reflexes_triggered;
    uint64_t attention_alerts;
    uint64_t autonomic_adjustments;

    /* Neuromodulation */
    float total_serotonin_released;
    float total_norepinephrine_released;
    float total_acetylcholine_released;
    float total_dopamine_released;

    /* Performance */
    float avg_arousal_level;
    float time_in_sleep_us;
    float time_in_wake_us;

    /* Integration */
    uint64_t bio_msgs_sent;
    uint64_t bio_msgs_received;
    uint64_t kg_updates;
    uint64_t immune_alerts;
} reticular_stats_t;

/*=============================================================================
 * CONFIGURATION
 *===========================================================================*/

/**
 * @brief Reticular formation configuration
 */
typedef struct {
    /* Arousal parameters */
    float arousal_gain;             /**< Arousal response sensitivity */
    float arousal_decay;            /**< Arousal decay rate */
    float sleep_threshold;          /**< Threshold for sleep onset */
    float wake_threshold;           /**< Threshold for wake onset */
    float hypervigilant_threshold;  /**< Threshold for hypervigilance */

    /* Neuromodulator parameters */
    float serotonin_baseline;
    float norepinephrine_baseline;
    float acetylcholine_baseline;
    float dopamine_baseline;

    /* Autonomic parameters */
    float cardiovascular_gain;
    float respiratory_gain;
    float sympathetic_baseline;
    float parasympathetic_baseline;

    /* Motor control */
    float postural_tone_baseline;
    float atonia_threshold;         /**< Arousal for REM atonia */

    /* Pain modulation */
    float pain_gate_baseline;
    float analgesia_gain;

    /* Sensory gating */
    float thalamic_gate_baseline;
    float habituation_rate;

    /* Reflex parameters */
    float reflex_threshold_base;
    float startle_habituation;

    /* Integration settings */
    bool enable_bio_async;
    bool enable_kg_wiring;
    bool enable_immune;
    bool enable_security;
    bool enable_logging;
    bool enable_quantum;

    /* Resource limits */
    uint32_t max_history_size;
    uint32_t update_interval_ms;

    /* Platform tier */
    platform_tier_t platform_tier;
} reticular_config_t;

/*=============================================================================
 * MAIN RETICULAR STATE STRUCTURE
 *===========================================================================*/

/**
 * @brief Complete reticular formation system state
 */
typedef struct nimcp_reticular {
    /* Configuration */
    reticular_config_t config;

    /* Arousal state */
    reticular_arousal_state_t arousal_state;
    float arousal_level;            /**< Continuous arousal [0, 1] */
    float arousal_momentum;         /**< Rate of arousal change */
    uint64_t arousal_state_start_us;

    /* Nuclei states */
    reticular_nucleus_state_t nuclei[RETICULAR_NUCLEUS_COUNT];

    /* Neuromodulator states */
    reticular_modulator_state_t modulators[RETICULAR_MODULATOR_COUNT];

    /* Autonomic states */
    reticular_autonomic_state_t autonomic[RETICULAR_AUTONOMIC_COUNT];

    /* Reflex states */
    reticular_reflex_state_t reflexes[RETICULAR_REFLEX_COUNT];

    /* Motor state */
    reticular_motor_state_t motor;

    /* Pain modulation state */
    reticular_pain_state_t pain;

    /* Sensory gating state */
    reticular_sensory_gate_t sensory_gate;

    /* Circadian input from hypothalamus */
    float circadian_drive;          /**< SCN input [0, 1] */
    float homeostatic_sleep_pressure; /**< Process S */

    /* Integration handles */
    struct nimcp_brain_kg* kg;
    struct nimcp_bio_router* bio_router;
    struct nimcp_immune_system* immune;
    struct nimcp_security_context* security;
    struct nimcp_snn_network* snn;
    struct nimcp_plasticity_engine* plasticity;
    struct nimcp_hypothalamus* hypothalamus;
    struct nimcp_thalamus* thalamus;
    struct nimcp_cognitive_hub* cognitive_hub;
    struct nimcp_training_context* training;
    struct nimcp_perception_system* perception;
    struct nimcp_symbolic_engine* symbolic;
    struct nimcp_swarm_context* swarm;
    struct nimcp_dragonfly_context* dragonfly;
    struct nimcp_portia_context* portia;
    struct nimcp_qmc_context* qmc;
    struct nimcp_omni_predictor* omni;
    struct nimcp_pag* pag;
    struct nimcp_neural_substrate* substrate;
    struct nimcp_raphe_system* raphe;
    struct nimcp_locus_coeruleus* locus_coeruleus;

    /* KG wiring state */
    reticular_kg_state_t kg_state;

    /* Statistics */
    reticular_stats_t stats;

    /* Threading */
    nimcp_mutex_t* mutex;

    /* Logging */
    nimcp_logger_t* logger;

    /* State flags */
    bool initialized;
    bool connected;
    uint64_t last_update_us;
    uint64_t simulation_time_us;
} nimcp_reticular_t;

/*=============================================================================
 * LIFECYCLE API
 *===========================================================================*/

/**
 * @brief Get default reticular configuration
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int reticular_default_config(reticular_config_t* config);

/**
 * @brief Create reticular formation instance
 * @param config Configuration (NULL for defaults)
 * @return New instance, or NULL on failure
 */
NIMCP_EXPORT nimcp_reticular_t* reticular_create(const reticular_config_t* config);

/**
 * @brief Destroy reticular formation instance
 * @param reticular Instance to destroy
 */
NIMCP_EXPORT void reticular_destroy(nimcp_reticular_t* reticular);

/**
 * @brief Initialize reticular formation (post-creation setup)
 * @param reticular Instance
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int reticular_init(nimcp_reticular_t* reticular);

/**
 * @brief Reset reticular formation to initial state
 * @param reticular Instance
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int reticular_reset(nimcp_reticular_t* reticular);

/*=============================================================================
 * AROUSAL CONTROL API
 *===========================================================================*/

/**
 * @brief Update arousal state
 * @param reticular Instance
 * @param dt Time step in seconds
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int reticular_update_arousal(nimcp_reticular_t* reticular, float dt);

/**
 * @brief Get current arousal level
 * @param reticular Instance
 * @return Arousal level [0, 1]
 */
NIMCP_EXPORT float reticular_get_arousal(const nimcp_reticular_t* reticular);

/**
 * @brief Get current arousal state
 * @param reticular Instance
 * @return Arousal state enum
 */
NIMCP_EXPORT reticular_arousal_state_t reticular_get_arousal_state(
    const nimcp_reticular_t* reticular);

/**
 * @brief Apply arousal stimulus
 * @param reticular Instance
 * @param stimulus Stimulus strength [-1, 1]
 * @param source Source identifier
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int reticular_apply_arousal_stimulus(
    nimcp_reticular_t* reticular,
    float stimulus,
    const char* source);

/**
 * @brief Transition to sleep state
 * @param reticular Instance
 * @param target_state Target sleep state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int reticular_initiate_sleep(
    nimcp_reticular_t* reticular,
    reticular_arousal_state_t target_state);

/**
 * @brief Wake from sleep
 * @param reticular Instance
 * @param urgency Wake urgency [0, 1]
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int reticular_wake(nimcp_reticular_t* reticular, float urgency);

/*=============================================================================
 * NEUROMODULATOR API
 *===========================================================================*/

/**
 * @brief Get neuromodulator concentration
 * @param reticular Instance
 * @param modulator Modulator type
 * @return Concentration [0, 1]
 */
NIMCP_EXPORT float reticular_get_modulator(
    const nimcp_reticular_t* reticular,
    reticular_modulator_t modulator);

/**
 * @brief Set neuromodulator release rate
 * @param reticular Instance
 * @param modulator Modulator type
 * @param rate Release rate
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int reticular_set_modulator_release(
    nimcp_reticular_t* reticular,
    reticular_modulator_t modulator,
    float rate);

/**
 * @brief Get all neuromodulator states
 * @param reticular Instance
 * @param states Output array (size RETICULAR_MODULATOR_COUNT)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int reticular_get_all_modulators(
    const nimcp_reticular_t* reticular,
    reticular_modulator_state_t* states);

/**
 * @brief Compute neuromodulator effects on arousal
 * @param reticular Instance
 * @param arousal_delta Output arousal modification
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int reticular_compute_modulator_effects(
    nimcp_reticular_t* reticular,
    float* arousal_delta);

/*=============================================================================
 * NUCLEUS CONTROL API
 *===========================================================================*/

/**
 * @brief Get nucleus activity
 * @param reticular Instance
 * @param nucleus Nucleus type
 * @return Activity level [0, 1]
 */
NIMCP_EXPORT float reticular_get_nucleus_activity(
    const nimcp_reticular_t* reticular,
    reticular_nucleus_t nucleus);

/**
 * @brief Stimulate nucleus
 * @param reticular Instance
 * @param nucleus Nucleus type
 * @param excitation Excitation level
 * @param inhibition Inhibition level
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int reticular_stimulate_nucleus(
    nimcp_reticular_t* reticular,
    reticular_nucleus_t nucleus,
    float excitation,
    float inhibition);

/**
 * @brief Get nucleus state
 * @param reticular Instance
 * @param nucleus Nucleus type
 * @param state Output state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int reticular_get_nucleus_state(
    const nimcp_reticular_t* reticular,
    reticular_nucleus_t nucleus,
    reticular_nucleus_state_t* state);

/*=============================================================================
 * AUTONOMIC CONTROL API
 *===========================================================================*/

/**
 * @brief Update autonomic functions
 * @param reticular Instance
 * @param dt Time step
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int reticular_update_autonomic(
    nimcp_reticular_t* reticular,
    float dt);

/**
 * @brief Get autonomic balance
 * @param reticular Instance
 * @param function Autonomic function type
 * @return Balance [-1 parasympathetic, +1 sympathetic]
 */
NIMCP_EXPORT float reticular_get_autonomic_balance(
    const nimcp_reticular_t* reticular,
    reticular_autonomic_t function);

/**
 * @brief Set autonomic setpoint
 * @param reticular Instance
 * @param function Autonomic function type
 * @param setpoint Target value
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int reticular_set_autonomic_setpoint(
    nimcp_reticular_t* reticular,
    reticular_autonomic_t function,
    float setpoint);

/**
 * @brief Apply sympathetic drive
 * @param reticular Instance
 * @param intensity Drive intensity [0, 1]
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int reticular_apply_sympathetic_drive(
    nimcp_reticular_t* reticular,
    float intensity);

/**
 * @brief Apply parasympathetic drive
 * @param reticular Instance
 * @param intensity Drive intensity [0, 1]
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int reticular_apply_parasympathetic_drive(
    nimcp_reticular_t* reticular,
    float intensity);

/*=============================================================================
 * REFLEX CONTROL API
 *===========================================================================*/

/**
 * @brief Trigger reflex
 * @param reticular Instance
 * @param reflex Reflex type
 * @param stimulus Trigger stimulus strength
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int reticular_trigger_reflex(
    nimcp_reticular_t* reticular,
    reticular_reflex_t reflex,
    float stimulus);

/**
 * @brief Check if reflex is active
 * @param reticular Instance
 * @param reflex Reflex type
 * @return true if active
 */
NIMCP_EXPORT bool reticular_is_reflex_active(
    const nimcp_reticular_t* reticular,
    reticular_reflex_t reflex);

/**
 * @brief Set reflex threshold
 * @param reticular Instance
 * @param reflex Reflex type
 * @param threshold New threshold
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int reticular_set_reflex_threshold(
    nimcp_reticular_t* reticular,
    reticular_reflex_t reflex,
    float threshold);

/**
 * @brief Get reflex state
 * @param reticular Instance
 * @param reflex Reflex type
 * @param state Output state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int reticular_get_reflex_state(
    const nimcp_reticular_t* reticular,
    reticular_reflex_t reflex,
    reticular_reflex_state_t* state);

/*=============================================================================
 * MOTOR CONTROL API
 *===========================================================================*/

/**
 * @brief Update motor tone
 * @param reticular Instance
 * @param dt Time step
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int reticular_update_motor_tone(
    nimcp_reticular_t* reticular,
    float dt);

/**
 * @brief Get postural tone
 * @param reticular Instance
 * @return Postural tone [0, 1]
 */
NIMCP_EXPORT float reticular_get_postural_tone(
    const nimcp_reticular_t* reticular);

/**
 * @brief Set REM atonia
 * @param reticular Instance
 * @param active Enable atonia
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int reticular_set_rem_atonia(
    nimcp_reticular_t* reticular,
    bool active);

/**
 * @brief Get locomotor drive
 * @param reticular Instance
 * @return Locomotor drive [0, 1]
 */
NIMCP_EXPORT float reticular_get_locomotor_drive(
    const nimcp_reticular_t* reticular);

/**
 * @brief Set locomotor drive
 * @param reticular Instance
 * @param drive Drive level [0, 1]
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int reticular_set_locomotor_drive(
    nimcp_reticular_t* reticular,
    float drive);

/*=============================================================================
 * PAIN MODULATION API
 *===========================================================================*/

/**
 * @brief Update pain modulation
 * @param reticular Instance
 * @param dt Time step
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int reticular_update_pain_modulation(
    nimcp_reticular_t* reticular,
    float dt);

/**
 * @brief Get pain gate state
 * @param reticular Instance
 * @return Gate closure [0 open, 1 closed]
 */
NIMCP_EXPORT float reticular_get_pain_gate(const nimcp_reticular_t* reticular);

/**
 * @brief Apply descending pain inhibition
 * @param reticular Instance
 * @param inhibition Inhibition strength [0, 1]
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int reticular_apply_pain_inhibition(
    nimcp_reticular_t* reticular,
    float inhibition);

/**
 * @brief Get pain threshold
 * @param reticular Instance
 * @return Current pain threshold
 */
NIMCP_EXPORT float reticular_get_pain_threshold(
    const nimcp_reticular_t* reticular);

/**
 * @brief Activate stress-induced analgesia
 * @param reticular Instance
 * @param stress_level Stress level [0, 1]
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int reticular_activate_stress_analgesia(
    nimcp_reticular_t* reticular,
    float stress_level);

/*=============================================================================
 * SENSORY GATING API
 *===========================================================================*/

/**
 * @brief Update sensory gating
 * @param reticular Instance
 * @param dt Time step
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int reticular_update_sensory_gating(
    nimcp_reticular_t* reticular,
    float dt);

/**
 * @brief Get thalamic gate state
 * @param reticular Instance
 * @return Gate openness [0 closed, 1 open]
 */
NIMCP_EXPORT float reticular_get_thalamic_gate(
    const nimcp_reticular_t* reticular);

/**
 * @brief Set attention bias for gating
 * @param reticular Instance
 * @param bias Attention bias [0, 1]
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int reticular_set_attention_bias(
    nimcp_reticular_t* reticular,
    float bias);

/**
 * @brief Apply habituation
 * @param reticular Instance
 * @param stimulus Habituating stimulus
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int reticular_apply_habituation(
    nimcp_reticular_t* reticular,
    float stimulus);

/**
 * @brief Get modality-specific gate
 * @param reticular Instance
 * @param modality 0=visual, 1=auditory, 2=somatosensory
 * @return Gate openness [0, 1]
 */
NIMCP_EXPORT float reticular_get_modality_gate(
    const nimcp_reticular_t* reticular,
    int modality);

/*=============================================================================
 * CIRCADIAN INTEGRATION API
 *===========================================================================*/

/**
 * @brief Set circadian drive from hypothalamus
 * @param reticular Instance
 * @param circadian_phase Phase [0, 24] hours
 * @param circadian_amplitude Amplitude [0, 1]
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int reticular_set_circadian_input(
    nimcp_reticular_t* reticular,
    float circadian_phase,
    float circadian_amplitude);

/**
 * @brief Update homeostatic sleep pressure
 * @param reticular Instance
 * @param wake_duration Time awake in hours
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int reticular_update_sleep_pressure(
    nimcp_reticular_t* reticular,
    float wake_duration);

/**
 * @brief Get sleep propensity
 * @param reticular Instance
 * @return Sleep propensity [0, 1]
 */
NIMCP_EXPORT float reticular_get_sleep_propensity(
    const nimcp_reticular_t* reticular);

/*=============================================================================
 * INTEGRATION API - KG WIRING
 *===========================================================================*/

/**
 * @brief Register with Knowledge Graph
 * @param reticular Instance
 * @param kg Knowledge graph handle
 * @param admin_token Security token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int reticular_kg_register(
    nimcp_reticular_t* reticular,
    struct nimcp_brain_kg* kg,
    uint64_t admin_token);

/**
 * @brief Unregister from Knowledge Graph
 * @param reticular Instance
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int reticular_kg_unregister(nimcp_reticular_t* reticular);

/**
 * @brief Update KG with current state
 * @param reticular Instance
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int reticular_kg_update_state(nimcp_reticular_t* reticular);

/**
 * @brief Query KG for related information
 * @param reticular Instance
 * @param query Query string
 * @param result Result buffer
 * @param result_size Buffer size
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int reticular_kg_query(
    nimcp_reticular_t* reticular,
    const char* query,
    void* result,
    size_t result_size);

/*=============================================================================
 * INTEGRATION API - BIO-ASYNC
 *===========================================================================*/

/**
 * @brief Connect to bio-async router
 * @param reticular Instance
 * @param router Router handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int reticular_bio_async_connect(
    nimcp_reticular_t* reticular,
    struct nimcp_bio_router* router);

/**
 * @brief Disconnect from bio-async router
 * @param reticular Instance
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int reticular_bio_async_disconnect(nimcp_reticular_t* reticular);

/**
 * @brief Broadcast reticular message
 * @param reticular Instance
 * @param msg_type Message type
 * @param payload Message payload
 * @param payload_size Payload size
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int reticular_bio_async_broadcast(
    nimcp_reticular_t* reticular,
    reticular_bio_msg_type_t msg_type,
    const void* payload,
    size_t payload_size);

/**
 * @brief Subscribe to messages
 * @param reticular Instance
 * @param subscription_mask Message types to subscribe
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int reticular_bio_async_subscribe(
    nimcp_reticular_t* reticular,
    uint32_t subscription_mask);

/*=============================================================================
 * INTEGRATION API - OTHER SYSTEMS
 *===========================================================================*/

/**
 * @brief Connect to immune system
 * @param reticular Instance
 * @param immune Immune system handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int reticular_immune_connect(
    nimcp_reticular_t* reticular,
    struct nimcp_immune_system* immune);

/**
 * @brief Connect to security context
 * @param reticular Instance
 * @param security Security context handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int reticular_security_connect(
    nimcp_reticular_t* reticular,
    struct nimcp_security_context* security);

/**
 * @brief Connect to SNN/plasticity
 * @param reticular Instance
 * @param snn SNN network handle
 * @param plasticity Plasticity engine handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int reticular_snn_connect(
    nimcp_reticular_t* reticular,
    struct nimcp_snn_network* snn,
    struct nimcp_plasticity_engine* plasticity);

/**
 * @brief Connect to hypothalamus (circadian integration)
 * @param reticular Instance
 * @param hypo Hypothalamus handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int reticular_hypothalamus_connect(
    nimcp_reticular_t* reticular,
    struct nimcp_hypothalamus* hypo);

/**
 * @brief Connect to thalamus (sensory relay)
 * @param reticular Instance
 * @param thalamus Thalamus handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int reticular_thalamus_connect(
    nimcp_reticular_t* reticular,
    struct nimcp_thalamus* thalamus);

/**
 * @brief Connect to cognitive hub
 * @param reticular Instance
 * @param hub Cognitive hub handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int reticular_cognitive_connect(
    nimcp_reticular_t* reticular,
    struct nimcp_cognitive_hub* hub);

/**
 * @brief Connect to training system
 * @param reticular Instance
 * @param training Training context handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int reticular_training_connect(
    nimcp_reticular_t* reticular,
    struct nimcp_training_context* training);

/**
 * @brief Connect to perception system
 * @param reticular Instance
 * @param perception Perception system handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int reticular_perception_connect(
    nimcp_reticular_t* reticular,
    struct nimcp_perception_system* perception);

/**
 * @brief Connect to symbolic logic engine
 * @param reticular Instance
 * @param symbolic Symbolic engine handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int reticular_symbolic_connect(
    nimcp_reticular_t* reticular,
    struct nimcp_symbolic_engine* symbolic);

/**
 * @brief Connect to swarm system
 * @param reticular Instance
 * @param swarm Swarm context handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int reticular_swarm_connect(
    nimcp_reticular_t* reticular,
    struct nimcp_swarm_context* swarm);

/**
 * @brief Connect to dragonfly system
 * @param reticular Instance
 * @param dragonfly Dragonfly context handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int reticular_dragonfly_connect(
    nimcp_reticular_t* reticular,
    struct nimcp_dragonfly_context* dragonfly);

/**
 * @brief Connect to portia system
 * @param reticular Instance
 * @param portia Portia context handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int reticular_portia_connect(
    nimcp_reticular_t* reticular,
    struct nimcp_portia_context* portia);

/**
 * @brief Connect to QMC system
 * @param reticular Instance
 * @param qmc QMC context handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int reticular_qmc_connect(
    nimcp_reticular_t* reticular,
    struct nimcp_qmc_context* qmc);

/**
 * @brief Connect to omnidirectional predictor
 * @param reticular Instance
 * @param omni Omni predictor handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int reticular_omni_connect(
    nimcp_reticular_t* reticular,
    struct nimcp_omni_predictor* omni);

/**
 * @brief Connect to PAG (defensive behavior)
 * @param reticular Instance
 * @param pag PAG handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int reticular_pag_connect(
    nimcp_reticular_t* reticular,
    struct nimcp_pag* pag);

/**
 * @brief Connect to neural substrate
 * @param reticular Instance
 * @param substrate Neural substrate handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int reticular_substrate_connect(
    nimcp_reticular_t* reticular,
    struct nimcp_neural_substrate* substrate);

/*=============================================================================
 * UPDATE AND STATE API
 *===========================================================================*/

/**
 * @brief Main update function
 * @param reticular Instance
 * @param dt Time step in seconds
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int reticular_update(nimcp_reticular_t* reticular, float dt);

/**
 * @brief Get current statistics
 * @param reticular Instance
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int reticular_get_stats(
    const nimcp_reticular_t* reticular,
    reticular_stats_t* stats);

/**
 * @brief Reset statistics
 * @param reticular Instance
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int reticular_reset_stats(nimcp_reticular_t* reticular);

/*=============================================================================
 * QUANTUM OPTIMIZATION API
 *===========================================================================*/

/**
 * @brief Optimize arousal dynamics using QMC
 * @param reticular Instance
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int reticular_qmc_optimize_arousal(nimcp_reticular_t* reticular);

/**
 * @brief Use QMCTS for sleep-wake prediction
 * @param reticular Instance
 * @param num_iterations Search iterations
 * @param predicted_state Output predicted state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int reticular_qmcts_predict_state(
    nimcp_reticular_t* reticular,
    uint32_t num_iterations,
    reticular_arousal_state_t* predicted_state);

/*=============================================================================
 * UTILITY API
 *===========================================================================*/

/**
 * @brief Get arousal state name
 * @param state Arousal state
 * @return Human-readable name
 */
NIMCP_EXPORT const char* reticular_arousal_state_string(
    reticular_arousal_state_t state);

/**
 * @brief Get nucleus name
 * @param nucleus Nucleus type
 * @return Human-readable name
 */
NIMCP_EXPORT const char* reticular_nucleus_string(reticular_nucleus_t nucleus);

/**
 * @brief Get modulator name
 * @param modulator Modulator type
 * @return Human-readable name
 */
NIMCP_EXPORT const char* reticular_modulator_string(
    reticular_modulator_t modulator);

/**
 * @brief Get reflex name
 * @param reflex Reflex type
 * @return Human-readable name
 */
NIMCP_EXPORT const char* reticular_reflex_string(reticular_reflex_t reflex);

/**
 * @brief Get autonomic function name
 * @param function Autonomic function type
 * @return Human-readable name
 */
NIMCP_EXPORT const char* reticular_autonomic_string(
    reticular_autonomic_t function);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_RETICULAR_H */
