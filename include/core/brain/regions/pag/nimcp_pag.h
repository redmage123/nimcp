/**
 * @file nimcp_pag.h
 * @brief Periaqueductal Gray (PAG) - Survival Behavior Control Center
 *
 * WHAT: Neural substrate for defensive behaviors, pain modulation,
 *       vocalization, autonomic control, and emotional expression
 * WHY:  Critical for survival responses and integration of threat/pain signals
 * HOW:  Implements columnar organization with distinct behavioral outputs
 *       and descending pain modulation pathways
 *
 * BIOLOGICAL BASIS:
 * - Surrounds the cerebral aqueduct in the midbrain
 * - Receives inputs from prefrontal cortex, hypothalamus, amygdala, spinal cord
 * - Projects to brainstem motor nuclei, rostral ventromedial medulla (RVM)
 * - Controls defensive behaviors: fight, flight, freeze, fawn
 * - Major component of descending pain inhibition system
 *
 * COLUMNAR ORGANIZATION:
 * - Dorsolateral (dlPAG): Active coping (fight, flight), tachycardia
 * - Lateral (lPAG): Active coping, vocalization
 * - Dorsomedial (dmPAG): Passive coping precursor, defensive attention
 * - Ventrolateral (vlPAG): Passive coping (freeze, fawn), bradycardia, analgesia
 *
 * PAIN MODULATION:
 * - Opioid pathway: Endogenous endorphins/enkephalins
 * - Non-opioid pathway: Cannabinoid, serotonergic, noradrenergic
 * - Descending inhibition via RVM to spinal dorsal horn
 *
 * FULL INTEGRATION WITH:
 * - Security module (BBB registration), Immune system
 * - KG wiring system (full node registration, state updates, queries)
 * - Bio-async system (message types with subscription masks)
 * - Brain initialization, SNN/STDP/plasticity modules
 * - Hypothalamus (bidirectional drive integration)
 * - Omnidirectional module, Cognitive/Training layers
 * - NIMCP utilities and math utilities, Threading (mutex)
 * - Quantum algorithms (QMC, QMCTS), Perception layer
 * - Symbolic logic system, Swarm/Dragonfly/Portia
 * - Logging system, Thalamic layer, Neural substrate layer
 *
 * @version 1.0
 * @date 2026-01-13
 */

#ifndef NIMCP_PAG_H
#define NIMCP_PAG_H

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
struct nimcp_amygdala;
struct nimcp_prefrontal;
struct nimcp_brainstem;
struct nimcp_spinal_interface;
struct nimcp_rvm;  /* Rostral ventromedial medulla */

/*=============================================================================
 * PAG COLUMNAR ORGANIZATION
 *===========================================================================*/

/**
 * @brief PAG columns with distinct functional roles
 *
 * BIOLOGICAL BASIS:
 * - dlPAG: Flight/fight, increased heart rate, non-opioid analgesia
 * - lPAG: Vocalization, active defense, affective responses
 * - dmPAG: Defensive attention, scanning, threat assessment
 * - vlPAG: Freeze/fawn, decreased heart rate, opioid analgesia
 */
typedef enum {
    PAG_COLUMN_DORSOLATERAL = 0,  /**< Active coping: fight/flight */
    PAG_COLUMN_LATERAL,           /**< Vocalization, active defense */
    PAG_COLUMN_DORSOMEDIAL,       /**< Defensive attention, scanning */
    PAG_COLUMN_VENTROLATERAL,     /**< Passive coping: freeze/fawn */
    PAG_COLUMN_COUNT
} pag_column_t;

/**
 * @brief PAG rostro-caudal levels
 */
typedef enum {
    PAG_LEVEL_ROSTRAL = 0,        /**< Rostral PAG (r/caudal PAG) */
    PAG_LEVEL_INTERMEDIATE,       /**< Intermediate level */
    PAG_LEVEL_CAUDAL,             /**< Caudal PAG */
    PAG_LEVEL_COUNT
} pag_level_t;

/*=============================================================================
 * DEFENSIVE BEHAVIOR TYPES
 *===========================================================================*/

/**
 * @brief Defensive response types (4F model)
 */
typedef enum {
    PAG_DEFENSE_FIGHT = 0,        /**< Active confrontation */
    PAG_DEFENSE_FLIGHT,           /**< Active escape */
    PAG_DEFENSE_FREEZE,           /**< Passive immobility */
    PAG_DEFENSE_FAWN,             /**< Passive submission/appeasement */
    PAG_DEFENSE_COUNT
} pag_defense_type_t;

/**
 * @brief Coping strategy
 */
typedef enum {
    PAG_COPING_ACTIVE = 0,        /**< Fight or flight (dlPAG/lPAG) */
    PAG_COPING_PASSIVE,           /**< Freeze or fawn (vlPAG) */
    PAG_COPING_MIXED              /**< Transitional state */
} pag_coping_strategy_t;

/**
 * @brief Threat proximity levels
 */
typedef enum {
    PAG_THREAT_DISTAL = 0,        /**< Threat detected but distant */
    PAG_THREAT_PROXIMAL,          /**< Threat approaching */
    PAG_THREAT_IMMINENT,          /**< Threat immediate */
    PAG_THREAT_CONTACT,           /**< Physical contact with threat */
    PAG_THREAT_NONE               /**< No threat detected */
} pag_threat_level_t;

/*=============================================================================
 * PAIN MODULATION SYSTEM
 *===========================================================================*/

/**
 * @brief Pain modulation pathway types
 */
typedef enum {
    PAG_PAIN_PATHWAY_OPIOID = 0,     /**< Endogenous opioid-mediated */
    PAG_PAIN_PATHWAY_NON_OPIOID,     /**< Non-opioid (stress-induced) */
    PAG_PAIN_PATHWAY_CANNABINOID,    /**< Endocannabinoid-mediated */
    PAG_PAIN_PATHWAY_SEROTONERGIC,   /**< 5-HT descending inhibition */
    PAG_PAIN_PATHWAY_NORADRENERGIC,  /**< Norepinephrine-mediated */
    PAG_PAIN_PATHWAY_COUNT
} pag_pain_pathway_t;

/**
 * @brief Analgesia state
 */
typedef struct {
    float analgesia_level;          /**< Overall analgesia [0, 1] */
    float pathway_activity[PAG_PAIN_PATHWAY_COUNT];  /**< Per-pathway activity */
    float descending_inhibition;    /**< Signal to spinal cord */
    float opioid_tone;              /**< Endogenous opioid level */
    float stress_induced_factor;    /**< SIA (stress-induced analgesia) */
    bool opioid_tolerance;          /**< Tolerance developed */
    uint64_t onset_timestamp_us;    /**< When analgesia started */
    uint64_t duration_us;           /**< Expected duration */
} pag_analgesia_state_t;

/**
 * @brief Pain input signal
 */
typedef struct {
    float intensity;                /**< Pain intensity [0, 1] */
    float unpleasantness;           /**< Affective component [0, 1] */
    uint32_t location_code;         /**< Body location encoding */
    bool nociceptive;               /**< Pure nociceptive signal */
    bool neuropathic;               /**< Neuropathic component */
    uint64_t timestamp_us;          /**< When received */
} pag_pain_input_t;

/*=============================================================================
 * VOCALIZATION CONTROL
 *===========================================================================*/

/**
 * @brief Vocalization types controlled by PAG
 */
typedef enum {
    PAG_VOCAL_NONE = 0,
    PAG_VOCAL_ALARM,                /**< Warning/alarm calls */
    PAG_VOCAL_AGGRESSION,           /**< Aggressive vocalization */
    PAG_VOCAL_SUBMISSION,           /**< Submissive vocalization */
    PAG_VOCAL_DISTRESS,             /**< Pain/distress vocalization */
    PAG_VOCAL_PLEASURE,             /**< Affiliative vocalization */
    PAG_VOCAL_STARTLE,              /**< Startle response */
    PAG_VOCAL_COUNT
} pag_vocal_type_t;

/**
 * @brief Vocalization output state
 */
typedef struct {
    pag_vocal_type_t type;          /**< Current vocalization type */
    float intensity;                /**< Vocalization intensity [0, 1] */
    float urgency;                  /**< Urgency/priority [0, 1] */
    bool active;                    /**< Currently vocalizing */
    uint64_t onset_us;              /**< When started */
} pag_vocal_state_t;

/*=============================================================================
 * AUTONOMIC CONTROL
 *===========================================================================*/

/**
 * @brief Autonomic output state
 */
typedef struct {
    /* Cardiovascular */
    float heart_rate_modulation;    /**< HR change [-1=brady, +1=tachy] */
    float blood_pressure_modulation; /**< BP change [-1, +1] */
    float vasoconstriction;         /**< Peripheral vasoconstriction [0, 1] */

    /* Respiratory */
    float respiratory_rate_mod;     /**< RR change [-1, +1] */
    float respiratory_depth_mod;    /**< Tidal volume change */
    bool apnea_triggered;           /**< Breath-holding (freeze) */

    /* Other autonomic */
    float pupil_dilation;           /**< Mydriasis [0, 1] */
    float sweating;                 /**< Sudomotor response [0, 1] */
    float piloerection;             /**< Hair standing [0, 1] */
    float bladder_sphincter_tone;   /**< Sphincter control [0, 1] */

    /* Muscle tone */
    float muscle_tone;              /**< Global muscle tone [0, 1] */
    bool tonic_immobility;          /**< Complete freeze state */
} pag_autonomic_state_t;

/*=============================================================================
 * EMOTIONAL EXPRESSION
 *===========================================================================*/

/**
 * @brief Emotional states mediated by PAG
 */
typedef enum {
    PAG_EMOTION_FEAR = 0,           /**< Fear/terror */
    PAG_EMOTION_RAGE,               /**< Anger/rage */
    PAG_EMOTION_PAIN,               /**< Pain affect */
    PAG_EMOTION_PANIC,              /**< Panic/separation distress */
    PAG_EMOTION_MATERNAL,           /**< Maternal/nurturing */
    PAG_EMOTION_REPRODUCTIVE,       /**< Sexual/reproductive */
    PAG_EMOTION_COUNT
} pag_emotion_type_t;

/**
 * @brief Emotional state output
 */
typedef struct {
    float emotion_levels[PAG_EMOTION_COUNT];  /**< Per-emotion activation */
    pag_emotion_type_t dominant_emotion;      /**< Currently dominant */
    float emotional_intensity;                /**< Overall intensity [0, 1] */
    float valence;                            /**< Positive/negative [-1, +1] */
    float arousal;                            /**< Activation level [0, 1] */
} pag_emotional_state_t;

/*=============================================================================
 * BIO-ASYNC MESSAGE TYPES
 *===========================================================================*/

/**
 * @brief PAG bio-async message types
 */
typedef enum {
    PAG_BIO_MSG_THREAT_DETECTED = 0,    /**< Threat signal received */
    PAG_BIO_MSG_DEFENSE_ACTIVATED,      /**< Defensive behavior started */
    PAG_BIO_MSG_DEFENSE_TERMINATED,     /**< Defensive behavior ended */
    PAG_BIO_MSG_PAIN_RECEIVED,          /**< Pain signal processed */
    PAG_BIO_MSG_ANALGESIA_ONSET,        /**< Analgesia activated */
    PAG_BIO_MSG_VOCALIZATION,           /**< Vocalization command */
    PAG_BIO_MSG_AUTONOMIC_CHANGE,       /**< Autonomic state update */
    PAG_BIO_MSG_EMOTION_UPDATE,         /**< Emotional state broadcast */
    PAG_BIO_MSG_COPING_SWITCH,          /**< Active/passive coping change */
    PAG_BIO_MSG_COLUMN_ACTIVATION,      /**< Column activity change */
    PAG_BIO_MSG_STATE_REQUEST,          /**< Request for PAG state */
    PAG_BIO_MSG_HYPOTHALAMUS_SIGNAL,    /**< Signal to/from hypothalamus */
    PAG_BIO_MSG_COUNT
} pag_bio_msg_type_t;

/**
 * @brief Subscription masks for bio-async
 */
#define PAG_BIO_SUB_THREAT          (1U << PAG_BIO_MSG_THREAT_DETECTED)
#define PAG_BIO_SUB_DEFENSE         (1U << PAG_BIO_MSG_DEFENSE_ACTIVATED)
#define PAG_BIO_SUB_DEFENSE_END     (1U << PAG_BIO_MSG_DEFENSE_TERMINATED)
#define PAG_BIO_SUB_PAIN            (1U << PAG_BIO_MSG_PAIN_RECEIVED)
#define PAG_BIO_SUB_ANALGESIA       (1U << PAG_BIO_MSG_ANALGESIA_ONSET)
#define PAG_BIO_SUB_VOCAL           (1U << PAG_BIO_MSG_VOCALIZATION)
#define PAG_BIO_SUB_AUTONOMIC       (1U << PAG_BIO_MSG_AUTONOMIC_CHANGE)
#define PAG_BIO_SUB_EMOTION         (1U << PAG_BIO_MSG_EMOTION_UPDATE)
#define PAG_BIO_SUB_COPING          (1U << PAG_BIO_MSG_COPING_SWITCH)
#define PAG_BIO_SUB_COLUMN          (1U << PAG_BIO_MSG_COLUMN_ACTIVATION)
#define PAG_BIO_SUB_HYPOTHALAMUS    (1U << PAG_BIO_MSG_HYPOTHALAMUS_SIGNAL)
#define PAG_BIO_SUB_ALL             (0xFFFFFFFFU)

/*=============================================================================
 * KG WIRING INTEGRATION
 *===========================================================================*/

/**
 * @brief KG node types for PAG
 */
typedef enum {
    PAG_KG_NODE_REGION = 0,         /**< PAG region node */
    PAG_KG_NODE_COLUMN,             /**< Column subdivision node */
    PAG_KG_NODE_DEFENSE_STATE,      /**< Defensive behavior node */
    PAG_KG_NODE_PAIN_STATE,         /**< Pain modulation node */
    PAG_KG_NODE_AUTONOMIC,          /**< Autonomic output node */
    PAG_KG_NODE_EMOTION,            /**< Emotional state node */
    PAG_KG_NODE_CONNECTION          /**< Connection/edge node */
} pag_kg_node_type_t;

/**
 * @brief KG wiring state for PAG
 */
typedef struct {
    uint64_t region_node_id;                    /**< Main PAG node in KG */
    uint64_t column_node_ids[PAG_COLUMN_COUNT]; /**< Per-column nodes */
    uint64_t defense_node_ids[PAG_DEFENSE_COUNT]; /**< Defense state nodes */
    uint64_t pain_pathway_node_ids[PAG_PAIN_PATHWAY_COUNT]; /**< Pain pathway nodes */
    uint64_t emotion_node_ids[PAG_EMOTION_COUNT]; /**< Emotion nodes */
    uint32_t edge_count;                        /**< Number of KG edges */
    bool registered;                            /**< Registration complete */
    uint64_t admin_token;                       /**< Security token for KG ops */
} pag_kg_state_t;

/*=============================================================================
 * DEFENSIVE BEHAVIOR STATE
 *===========================================================================*/

/**
 * @brief Complete defensive behavior state
 */
typedef struct {
    /* Current threat assessment */
    pag_threat_level_t threat_level;
    float threat_intensity;             /**< Overall threat [0, 1] */
    float threat_direction;             /**< Direction to threat (radians) */
    float threat_distance;              /**< Distance estimate */

    /* Response selection */
    pag_defense_type_t active_defense;  /**< Current defensive response */
    pag_coping_strategy_t coping;       /**< Active vs passive coping */
    float response_probability[PAG_DEFENSE_COUNT];  /**< Response likelihoods */

    /* Response intensity */
    float defense_intensity;            /**< Response intensity [0, 1] */
    float motor_output;                 /**< Motor command strength */
    float inhibition_level;             /**< Cortical inhibition */

    /* Timing */
    uint64_t threat_onset_us;           /**< When threat detected */
    uint64_t response_latency_us;       /**< Response delay */
    uint64_t duration_us;               /**< Time in defensive state */

    /* Flags */
    bool escape_route_available;        /**< Can flee */
    bool defense_active;                /**< Currently defending */
    bool habituated;                    /**< Threat habituated */
} pag_defense_state_t;

/*=============================================================================
 * COLUMN STATE
 *===========================================================================*/

/**
 * @brief Single column state
 */
typedef struct {
    pag_column_t column;                /**< Column identifier */
    float activity;                     /**< Current activity [0, 1] */
    float modulation;                   /**< External modulation factor */
    float baseline;                     /**< Baseline activity */
    float inhibition;                   /**< Inhibitory input */
    float excitation;                   /**< Excitatory input */
    bool active;                        /**< Above threshold */
} pag_column_state_t;

/*=============================================================================
 * STATISTICS AND METRICS
 *===========================================================================*/

/**
 * @brief PAG operational statistics
 */
typedef struct {
    /* Threat/defense statistics */
    uint64_t threats_detected;
    uint64_t defense_activations[PAG_DEFENSE_COUNT];
    uint64_t coping_switches;           /**< Active<->passive transitions */
    float avg_response_latency_us;

    /* Pain modulation statistics */
    uint64_t pain_signals_processed;
    uint64_t analgesia_episodes;
    float total_analgesia_time_us;
    float avg_analgesia_level;

    /* Vocalization statistics */
    uint64_t vocalizations[PAG_VOCAL_COUNT];

    /* Autonomic statistics */
    float avg_heart_rate_modulation;
    float avg_respiratory_modulation;

    /* Integration statistics */
    uint64_t bio_msgs_sent;
    uint64_t bio_msgs_received;
    uint64_t kg_updates;
    uint64_t immune_alerts;
    uint64_t hypothalamus_exchanges;

    /* Column activity */
    float avg_column_activity[PAG_COLUMN_COUNT];
} pag_stats_t;

/*=============================================================================
 * CONFIGURATION
 *===========================================================================*/

/**
 * @brief PAG configuration
 */
typedef struct {
    /* Defensive behavior parameters */
    float threat_threshold;             /**< Threshold for defense activation */
    float defense_decay_rate;           /**< Decay of defensive response */
    float coping_switch_threshold;      /**< Threshold for active<->passive */
    float flight_vs_fight_bias;         /**< Preference [-1=fight, +1=flight] */
    float freeze_duration_base_ms;      /**< Base freeze duration */

    /* Pain modulation parameters */
    float analgesia_gain;               /**< Analgesia response gain */
    float opioid_sensitivity;           /**< Opioid pathway sensitivity */
    float non_opioid_sensitivity;       /**< Non-opioid pathway sensitivity */
    float descending_inhibition_gain;   /**< Descending pathway gain */
    float stress_analgesia_threshold;   /**< SIA activation threshold */

    /* Autonomic parameters */
    float autonomic_gain;               /**< Autonomic response gain */
    float cardiovascular_coupling;      /**< CV response coupling */
    float respiratory_coupling;         /**< Respiratory response coupling */

    /* Vocalization parameters */
    float vocal_threshold;              /**< Vocalization activation threshold */
    float vocal_intensity_gain;         /**< Intensity scaling */

    /* Column parameters */
    float column_competition_strength;  /**< Inter-column competition */
    float column_decay_rate;            /**< Activity decay rate */

    /* Integration settings */
    bool enable_bio_async;              /**< Enable bio-async messaging */
    bool enable_kg_wiring;              /**< Enable KG registration */
    bool enable_immune;                 /**< Enable immune monitoring */
    bool enable_security;               /**< Enable security checks */
    bool enable_logging;                /**< Enable detailed logging */
    bool enable_quantum;                /**< Enable QMC optimization */
    bool enable_hypothalamus_link;      /**< Enable hypothalamus connection */

    /* Resource limits */
    uint32_t max_threat_history;        /**< Threat history buffer size */
    uint32_t update_interval_ms;        /**< State update interval */

    /* Platform tier */
    platform_tier_t platform_tier;
} pag_config_t;

/*=============================================================================
 * MAIN PAG STATE STRUCTURE
 *===========================================================================*/

/**
 * @brief Complete PAG system state
 */
typedef struct nimcp_pag {
    /* Configuration */
    pag_config_t config;

    /* Column states */
    pag_column_state_t columns[PAG_COLUMN_COUNT];

    /* Defensive behavior */
    pag_defense_state_t defense;

    /* Pain modulation */
    pag_analgesia_state_t analgesia;
    pag_pain_input_t current_pain;
    bool pain_active;

    /* Vocalization */
    pag_vocal_state_t vocal;

    /* Autonomic output */
    pag_autonomic_state_t autonomic;

    /* Emotional state */
    pag_emotional_state_t emotion;

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
    struct nimcp_amygdala* amygdala;
    struct nimcp_prefrontal* prefrontal;
    struct nimcp_brainstem* brainstem;
    struct nimcp_spinal_interface* spinal;
    struct nimcp_rvm* rvm;

    /* KG wiring state */
    pag_kg_state_t kg_state;

    /* Statistics */
    pag_stats_t stats;

    /* Threading */
    nimcp_mutex_t* mutex;

    /* Logging */
    nimcp_logger_t* logger;

    /* State flags */
    bool initialized;
    bool connected;
    uint64_t last_update_us;
} nimcp_pag_t;

/*=============================================================================
 * LIFECYCLE API
 *===========================================================================*/

/**
 * @brief Get default PAG configuration
 * @param config Output configuration structure
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pag_default_config(pag_config_t* config);

/**
 * @brief Create PAG instance
 * @param config Configuration (NULL for defaults)
 * @return New PAG instance, or NULL on failure
 */
NIMCP_EXPORT nimcp_pag_t* pag_create(const pag_config_t* config);

/**
 * @brief Destroy PAG instance
 * @param pag PAG instance to destroy
 */
NIMCP_EXPORT void pag_destroy(nimcp_pag_t* pag);

/**
 * @brief Initialize PAG (post-creation setup)
 * @param pag PAG instance
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pag_init(nimcp_pag_t* pag);

/**
 * @brief Reset PAG to initial state
 * @param pag PAG instance
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pag_reset(nimcp_pag_t* pag);

/*=============================================================================
 * THREAT AND DEFENSIVE BEHAVIOR API
 *===========================================================================*/

/**
 * @brief Process threat signal
 * @param pag PAG instance
 * @param threat_level Threat proximity level
 * @param intensity Threat intensity [0, 1]
 * @param direction Direction to threat (radians)
 * @param distance Distance estimate
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pag_process_threat(
    nimcp_pag_t* pag,
    pag_threat_level_t threat_level,
    float intensity,
    float direction,
    float distance);

/**
 * @brief Get current defensive response
 * @param pag PAG instance
 * @param defense Output defense state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pag_get_defense_state(
    const nimcp_pag_t* pag,
    pag_defense_state_t* defense);

/**
 * @brief Force specific defensive response (for testing/override)
 * @param pag PAG instance
 * @param defense_type Defensive response to activate
 * @param intensity Response intensity [0, 1]
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pag_set_defense_response(
    nimcp_pag_t* pag,
    pag_defense_type_t defense_type,
    float intensity);

/**
 * @brief Check if escape route is available
 * @param pag PAG instance
 * @param available Output escape availability
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pag_check_escape_route(
    nimcp_pag_t* pag,
    bool* available);

/**
 * @brief Set escape route availability
 * @param pag PAG instance
 * @param available Escape route available
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pag_set_escape_route(
    nimcp_pag_t* pag,
    bool available);

/**
 * @brief Clear threat (return to baseline)
 * @param pag PAG instance
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pag_clear_threat(nimcp_pag_t* pag);

/**
 * @brief Get coping strategy
 * @param pag PAG instance
 * @return Current coping strategy
 */
NIMCP_EXPORT pag_coping_strategy_t pag_get_coping_strategy(
    const nimcp_pag_t* pag);

/*=============================================================================
 * PAIN MODULATION API
 *===========================================================================*/

/**
 * @brief Process pain input
 * @param pag PAG instance
 * @param pain Pain input signal
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pag_process_pain(
    nimcp_pag_t* pag,
    const pag_pain_input_t* pain);

/**
 * @brief Get current analgesia state
 * @param pag PAG instance
 * @param analgesia Output analgesia state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pag_get_analgesia_state(
    const nimcp_pag_t* pag,
    pag_analgesia_state_t* analgesia);

/**
 * @brief Activate specific pain pathway
 * @param pag PAG instance
 * @param pathway Pain pathway to activate
 * @param activation Activation level [0, 1]
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pag_activate_pain_pathway(
    nimcp_pag_t* pag,
    pag_pain_pathway_t pathway,
    float activation);

/**
 * @brief Get descending inhibition signal (to spinal cord)
 * @param pag PAG instance
 * @return Descending inhibition level [0, 1]
 */
NIMCP_EXPORT float pag_get_descending_inhibition(
    const nimcp_pag_t* pag);

/**
 * @brief Trigger stress-induced analgesia
 * @param pag PAG instance
 * @param stress_level Stress intensity [0, 1]
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pag_trigger_stress_analgesia(
    nimcp_pag_t* pag,
    float stress_level);

/**
 * @brief Check for opioid tolerance
 * @param pag PAG instance
 * @return true if tolerance developed
 */
NIMCP_EXPORT bool pag_has_opioid_tolerance(
    const nimcp_pag_t* pag);

/*=============================================================================
 * VOCALIZATION API
 *===========================================================================*/

/**
 * @brief Trigger vocalization
 * @param pag PAG instance
 * @param type Vocalization type
 * @param intensity Vocalization intensity [0, 1]
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pag_trigger_vocalization(
    nimcp_pag_t* pag,
    pag_vocal_type_t type,
    float intensity);

/**
 * @brief Get current vocalization state
 * @param pag PAG instance
 * @param vocal Output vocalization state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pag_get_vocalization_state(
    const nimcp_pag_t* pag,
    pag_vocal_state_t* vocal);

/**
 * @brief Stop vocalization
 * @param pag PAG instance
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pag_stop_vocalization(nimcp_pag_t* pag);

/*=============================================================================
 * AUTONOMIC CONTROL API
 *===========================================================================*/

/**
 * @brief Get autonomic output state
 * @param pag PAG instance
 * @param autonomic Output autonomic state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pag_get_autonomic_state(
    const nimcp_pag_t* pag,
    pag_autonomic_state_t* autonomic);

/**
 * @brief Get cardiovascular modulation
 * @param pag PAG instance
 * @param heart_rate_mod Output heart rate modulation [-1, +1]
 * @param bp_mod Output blood pressure modulation [-1, +1]
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pag_get_cardiovascular_output(
    const nimcp_pag_t* pag,
    float* heart_rate_mod,
    float* bp_mod);

/**
 * @brief Get respiratory modulation
 * @param pag PAG instance
 * @param rate_mod Output respiratory rate modulation
 * @param depth_mod Output respiratory depth modulation
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pag_get_respiratory_output(
    const nimcp_pag_t* pag,
    float* rate_mod,
    float* depth_mod);

/**
 * @brief Check for tonic immobility (complete freeze)
 * @param pag PAG instance
 * @return true if in tonic immobility
 */
NIMCP_EXPORT bool pag_is_tonic_immobility(const nimcp_pag_t* pag);

/*=============================================================================
 * EMOTIONAL STATE API
 *===========================================================================*/

/**
 * @brief Get emotional state
 * @param pag PAG instance
 * @param emotion Output emotional state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pag_get_emotional_state(
    const nimcp_pag_t* pag,
    pag_emotional_state_t* emotion);

/**
 * @brief Set emotional input (from amygdala, hypothalamus)
 * @param pag PAG instance
 * @param emotion_type Emotion to modulate
 * @param intensity Emotion intensity [0, 1]
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pag_set_emotion_input(
    nimcp_pag_t* pag,
    pag_emotion_type_t emotion_type,
    float intensity);

/**
 * @brief Get dominant emotion
 * @param pag PAG instance
 * @return Currently dominant emotion
 */
NIMCP_EXPORT pag_emotion_type_t pag_get_dominant_emotion(
    const nimcp_pag_t* pag);

/*=============================================================================
 * COLUMN CONTROL API
 *===========================================================================*/

/**
 * @brief Get column activity
 * @param pag PAG instance
 * @param column Column to query
 * @return Activity level [0, 1]
 */
NIMCP_EXPORT float pag_get_column_activity(
    const nimcp_pag_t* pag,
    pag_column_t column);

/**
 * @brief Set column modulation (external input)
 * @param pag PAG instance
 * @param column Column to modulate
 * @param modulation Modulation factor [0, 2]
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pag_set_column_modulation(
    nimcp_pag_t* pag,
    pag_column_t column,
    float modulation);

/**
 * @brief Get column state
 * @param pag PAG instance
 * @param column Column to query
 * @param state Output column state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pag_get_column_state(
    const nimcp_pag_t* pag,
    pag_column_t column,
    pag_column_state_t* state);

/**
 * @brief Get most active column
 * @param pag PAG instance
 * @return Most active column
 */
NIMCP_EXPORT pag_column_t pag_get_dominant_column(const nimcp_pag_t* pag);

/*=============================================================================
 * INTEGRATION API - KG WIRING
 *===========================================================================*/

/**
 * @brief Register PAG with Knowledge Graph
 * @param pag PAG instance
 * @param kg Knowledge graph instance
 * @param admin_token Security admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pag_kg_register(
    nimcp_pag_t* pag,
    struct nimcp_brain_kg* kg,
    uint64_t admin_token);

/**
 * @brief Unregister PAG from Knowledge Graph
 * @param pag PAG instance
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pag_kg_unregister(nimcp_pag_t* pag);

/**
 * @brief Update KG with current state
 * @param pag PAG instance
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pag_kg_update_state(nimcp_pag_t* pag);

/**
 * @brief Query KG for related information
 * @param pag PAG instance
 * @param query Query string
 * @param result Output result buffer
 * @param result_size Result buffer size
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pag_kg_query(
    nimcp_pag_t* pag,
    const char* query,
    void* result,
    size_t result_size);

/*=============================================================================
 * INTEGRATION API - BIO-ASYNC
 *===========================================================================*/

/**
 * @brief Connect to bio-async router
 * @param pag PAG instance
 * @param router Bio-async router
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pag_bio_async_connect(
    nimcp_pag_t* pag,
    struct nimcp_bio_router* router);

/**
 * @brief Disconnect from bio-async router
 * @param pag PAG instance
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pag_bio_async_disconnect(nimcp_pag_t* pag);

/**
 * @brief Broadcast PAG message
 * @param pag PAG instance
 * @param msg_type Message type
 * @param payload Message payload
 * @param payload_size Payload size
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pag_bio_async_broadcast(
    nimcp_pag_t* pag,
    pag_bio_msg_type_t msg_type,
    const void* payload,
    size_t payload_size);

/**
 * @brief Subscribe to messages
 * @param pag PAG instance
 * @param subscription_mask Bitmask of message types to subscribe
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pag_bio_async_subscribe(
    nimcp_pag_t* pag,
    uint32_t subscription_mask);

/*=============================================================================
 * INTEGRATION API - SECURITY (BBB)
 *===========================================================================*/

/**
 * @brief Connect to security context (BBB registration)
 * @param pag PAG instance
 * @param security Security context
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pag_security_connect(
    nimcp_pag_t* pag,
    struct nimcp_security_context* security);

/**
 * @brief Register with Blood-Brain Barrier
 * @param pag PAG instance
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pag_bbb_register(nimcp_pag_t* pag);

/*=============================================================================
 * INTEGRATION API - IMMUNE SYSTEM
 *===========================================================================*/

/**
 * @brief Connect to immune system
 * @param pag PAG instance
 * @param immune Immune system instance
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pag_immune_connect(
    nimcp_pag_t* pag,
    struct nimcp_immune_system* immune);

/**
 * @brief Report immune alert
 * @param pag PAG instance
 * @param alert_type Alert type identifier
 * @param severity Severity level [0, 1]
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pag_immune_alert(
    nimcp_pag_t* pag,
    uint32_t alert_type,
    float severity);

/*=============================================================================
 * INTEGRATION API - SNN/PLASTICITY
 *===========================================================================*/

/**
 * @brief Connect to SNN/plasticity
 * @param pag PAG instance
 * @param snn SNN network
 * @param plasticity Plasticity engine
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pag_snn_connect(
    nimcp_pag_t* pag,
    struct nimcp_snn_network* snn,
    struct nimcp_plasticity_engine* plasticity);

/**
 * @brief Update plasticity based on defensive learning
 * @param pag PAG instance
 * @param reward Reward/punishment signal [-1, +1]
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pag_update_plasticity(
    nimcp_pag_t* pag,
    float reward);

/*=============================================================================
 * INTEGRATION API - HYPOTHALAMUS (BIDIRECTIONAL)
 *===========================================================================*/

/**
 * @brief Connect to hypothalamus (bidirectional drive integration)
 * @param pag PAG instance
 * @param hypo Hypothalamus instance
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pag_hypothalamus_connect(
    nimcp_pag_t* pag,
    struct nimcp_hypothalamus* hypo);

/**
 * @brief Receive drive signal from hypothalamus
 * @param pag PAG instance
 * @param drive_type Hypothalamic drive type
 * @param drive_level Drive intensity [0, 1]
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pag_receive_drive_signal(
    nimcp_pag_t* pag,
    uint32_t drive_type,
    float drive_level);

/**
 * @brief Send defensive state to hypothalamus
 * @param pag PAG instance
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pag_send_to_hypothalamus(nimcp_pag_t* pag);

/*=============================================================================
 * INTEGRATION API - OTHER BRAIN REGIONS
 *===========================================================================*/

/**
 * @brief Connect to thalamus
 * @param pag PAG instance
 * @param thalamus Thalamus instance
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pag_thalamus_connect(
    nimcp_pag_t* pag,
    struct nimcp_thalamus* thalamus);

/**
 * @brief Connect to amygdala
 * @param pag PAG instance
 * @param amygdala Amygdala instance
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pag_amygdala_connect(
    nimcp_pag_t* pag,
    struct nimcp_amygdala* amygdala);

/**
 * @brief Connect to prefrontal cortex
 * @param pag PAG instance
 * @param prefrontal Prefrontal cortex instance
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pag_prefrontal_connect(
    nimcp_pag_t* pag,
    struct nimcp_prefrontal* prefrontal);

/**
 * @brief Connect to brainstem
 * @param pag PAG instance
 * @param brainstem Brainstem instance
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pag_brainstem_connect(
    nimcp_pag_t* pag,
    struct nimcp_brainstem* brainstem);

/**
 * @brief Connect to rostral ventromedial medulla (RVM)
 * @param pag PAG instance
 * @param rvm RVM instance (for descending pain modulation)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pag_rvm_connect(
    nimcp_pag_t* pag,
    struct nimcp_rvm* rvm);

/*=============================================================================
 * INTEGRATION API - COGNITIVE/TRAINING
 *===========================================================================*/

/**
 * @brief Connect to cognitive hub
 * @param pag PAG instance
 * @param hub Cognitive hub instance
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pag_cognitive_connect(
    nimcp_pag_t* pag,
    struct nimcp_cognitive_hub* hub);

/**
 * @brief Connect to training system
 * @param pag PAG instance
 * @param training Training context
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pag_training_connect(
    nimcp_pag_t* pag,
    struct nimcp_training_context* training);

/**
 * @brief Connect to perception system
 * @param pag PAG instance
 * @param perception Perception system
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pag_perception_connect(
    nimcp_pag_t* pag,
    struct nimcp_perception_system* perception);

/**
 * @brief Connect to symbolic logic engine
 * @param pag PAG instance
 * @param symbolic Symbolic engine
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pag_symbolic_connect(
    nimcp_pag_t* pag,
    struct nimcp_symbolic_engine* symbolic);

/*=============================================================================
 * INTEGRATION API - SWARM/DRAGONFLY/PORTIA
 *===========================================================================*/

/**
 * @brief Connect to swarm system
 * @param pag PAG instance
 * @param swarm Swarm context
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pag_swarm_connect(
    nimcp_pag_t* pag,
    struct nimcp_swarm_context* swarm);

/**
 * @brief Connect to dragonfly system
 * @param pag PAG instance
 * @param dragonfly Dragonfly context
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pag_dragonfly_connect(
    nimcp_pag_t* pag,
    struct nimcp_dragonfly_context* dragonfly);

/**
 * @brief Connect to portia system
 * @param pag PAG instance
 * @param portia Portia context
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pag_portia_connect(
    nimcp_pag_t* pag,
    struct nimcp_portia_context* portia);

/*=============================================================================
 * INTEGRATION API - QUANTUM (QMC/QMCTS)
 *===========================================================================*/

/**
 * @brief Connect to QMC system
 * @param pag PAG instance
 * @param qmc QMC context
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pag_qmc_connect(
    nimcp_pag_t* pag,
    struct nimcp_qmc_context* qmc);

/**
 * @brief Connect to omnidirectional predictor
 * @param pag PAG instance
 * @param omni Omnidirectional predictor
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pag_omni_connect(
    nimcp_pag_t* pag,
    struct nimcp_omni_predictor* omni);

/**
 * @brief Optimize defense selection using QMC
 * @param pag PAG instance
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pag_qmc_optimize_defense(nimcp_pag_t* pag);

/**
 * @brief Use QMCTS for threat response planning
 * @param pag PAG instance
 * @param num_iterations Number of search iterations
 * @param best_response Output best defensive response
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pag_qmcts_threat_response(
    nimcp_pag_t* pag,
    uint32_t num_iterations,
    pag_defense_type_t* best_response);

/*=============================================================================
 * UPDATE AND STATE API
 *===========================================================================*/

/**
 * @brief Update PAG state (call each timestep)
 * @param pag PAG instance
 * @param dt Time delta in seconds
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pag_update(nimcp_pag_t* pag, float dt);

/**
 * @brief Get current statistics
 * @param pag PAG instance
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pag_get_stats(
    const nimcp_pag_t* pag,
    pag_stats_t* stats);

/*=============================================================================
 * UTILITY FUNCTIONS
 *===========================================================================*/

/**
 * @brief Get column name string
 * @param column Column type
 * @return Human-readable column name
 */
NIMCP_EXPORT const char* pag_column_string(pag_column_t column);

/**
 * @brief Get defense type name string
 * @param defense Defense type
 * @return Human-readable defense name
 */
NIMCP_EXPORT const char* pag_defense_string(pag_defense_type_t defense);

/**
 * @brief Get threat level name string
 * @param level Threat level
 * @return Human-readable threat level name
 */
NIMCP_EXPORT const char* pag_threat_string(pag_threat_level_t level);

/**
 * @brief Get emotion type name string
 * @param emotion Emotion type
 * @return Human-readable emotion name
 */
NIMCP_EXPORT const char* pag_emotion_string(pag_emotion_type_t emotion);

/**
 * @brief Get vocalization type name string
 * @param vocal Vocalization type
 * @return Human-readable vocalization name
 */
NIMCP_EXPORT const char* pag_vocal_string(pag_vocal_type_t vocal);

/**
 * @brief Get pain pathway name string
 * @param pathway Pain pathway type
 * @return Human-readable pathway name
 */
NIMCP_EXPORT const char* pag_pain_pathway_string(pag_pain_pathway_t pathway);

/**
 * @brief Get coping strategy name string
 * @param coping Coping strategy
 * @return Human-readable coping strategy name
 */
NIMCP_EXPORT const char* pag_coping_string(pag_coping_strategy_t coping);

/**
 * @brief Get PAG mutex for external synchronization
 * @param pag PAG instance
 * @return Mutex pointer, or NULL if not available
 */
NIMCP_EXPORT nimcp_mutex_t* pag_get_mutex(nimcp_pag_t* pag);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PAG_H */
