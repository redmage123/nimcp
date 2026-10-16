/**
 * @file nimcp_cognitive_bio_async_bridge.h
 * @brief Bio-Async Integration Bridge for Cognitive Systems
 * @version 1.0.0
 * @date 2026-01-13
 *
 * WHAT: Unified bridge connecting all cognitive modules with bio-async system
 * WHY:  Bio-async provides coordination, futures, and neuromodulator-based signaling
 *       for cognitive processes like attention, emotion, memory, and reasoning
 * HOW:  Full bridge pattern with message handlers, module registration, and state sync
 *
 * BIOLOGICAL BASIS:
 * Cognitive processes require dynamic coordination across brain systems:
 * - Dopamine signals goal achievement and reward prediction
 * - Norepinephrine signals urgency, priority, and arousal
 * - Acetylcholine modulates attention and memory encoding
 * - Serotonin reflects mood state and patience
 * - Phase coupling synchronizes distributed cognitive operations
 * - Glial waves coordinate global state transitions
 *
 * ARCHITECTURE:
 * ```
 * +------------------------+                    +----------------------+
 * | COGNITIVE MODULES      |                    |     BIO-ASYNC        |
 * |                        |                    |                      |
 * | - Attention            |<-- neuromod    --->| - Message Router     |
 * | - Emotion              |    channels        | - Future Manager     |
 * | - Working Memory       |                    | - Phase Sync         |
 * | - Reasoning            |<-- phase       --->| - Glial Waves        |
 * | - Goal System          |    coupling        | - Oscillators        |
 * | - Salience             |                    |                      |
 * | - Ethics               |                    |                      |
 * | - Curiosity            |                    |                      |
 * | - Introspection        |                    |                      |
 * +------------------------+                    +----------------------+
 *            |                                           |
 *            +---------------- BRIDGE -------------------+
 *                       (bidirectional flow)
 * ```
 *
 * MODULE ID ALLOCATION:
 * - 0x2000: Cognitive Bridge Root
 * - 0x2001: Attention Module
 * - 0x2002: Emotion Module
 * - 0x2003: Working Memory Module
 * - 0x2004: Reasoning Module
 * - 0x2005: Goal System Module
 * - 0x2006: Salience Module
 * - 0x2007: Ethics Module
 * - 0x2008: Curiosity Module
 * - 0x2009: Introspection Module
 * - 0x200A: Theory of Mind Module
 * - 0x200B: Empathy Module
 * - 0x200C: Knowledge Module
 * - 0x200D: Consolidation Module
 * - 0x200E: Decision Module
 * - 0x200F: Executive Control Module
 */

#ifndef NIMCP_COGNITIVE_BIO_ASYNC_BRIDGE_H
#define NIMCP_COGNITIVE_BIO_ASYNC_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * FORWARD DECLARATIONS
 *===========================================================================*/

struct nimcp_bio_async;
struct nimcp_bio_future;
struct nimcp_phase_sync;
struct nimcp_glial_wave;
struct brain_struct;

/* Cognitive module forward declarations */
struct attention_ctx;
struct emotional_system;
struct working_memory;
struct reasoning_ctx;
struct goal_system;
struct salience_evaluator;
struct ethics_engine;
struct curiosity_engine;
struct introspection_ctx;
struct theory_of_mind;
struct empathy_network;
struct knowledge_graph;

/*=============================================================================
 * ERROR CODES
 *===========================================================================*/

/** Base error code for cognitive bio-async bridge */
#define COG_BIO_ERROR_BASE                  20000

/** Cognitive bio-async specific error codes */
#define COG_BIO_ERROR_NULL_POINTER          (COG_BIO_ERROR_BASE + 1)
#define COG_BIO_ERROR_NOT_INITIALIZED       (COG_BIO_ERROR_BASE + 2)
#define COG_BIO_ERROR_ALREADY_INITIALIZED   (COG_BIO_ERROR_BASE + 3)
#define COG_BIO_ERROR_DISCONNECTED          (COG_BIO_ERROR_BASE + 4)
#define COG_BIO_ERROR_MODULE_NOT_FOUND      (COG_BIO_ERROR_BASE + 5)
#define COG_BIO_ERROR_MODULE_ALREADY_REG    (COG_BIO_ERROR_BASE + 6)
#define COG_BIO_ERROR_HANDLER_FULL          (COG_BIO_ERROR_BASE + 7)
#define COG_BIO_ERROR_INVALID_MESSAGE       (COG_BIO_ERROR_BASE + 8)
#define COG_BIO_ERROR_INVALID_CONFIG        (COG_BIO_ERROR_BASE + 9)
#define COG_BIO_ERROR_TIMEOUT               (COG_BIO_ERROR_BASE + 10)
#define COG_BIO_ERROR_PHASE_INCOHERENT      (COG_BIO_ERROR_BASE + 11)
#define COG_BIO_ERROR_BROADCAST_FAILED      (COG_BIO_ERROR_BASE + 12)
#define COG_BIO_ERROR_STATE_INVALID         (COG_BIO_ERROR_BASE + 13)
#define COG_BIO_ERROR_RESOURCE_EXHAUSTED    (COG_BIO_ERROR_BASE + 14)

/** Success code */
#define COG_BIO_OK                          0

/*=============================================================================
 * MODULE ID CONSTANTS
 *===========================================================================*/

/** Module ID for cognitive bridge root */
#define COG_BIO_MODULE_ROOT                 0x2000

/** Module ID for attention */
#define COG_BIO_MODULE_ATTENTION            0x2001

/** Module ID for emotion */
#define COG_BIO_MODULE_EMOTION              0x2002

/** Module ID for working memory */
#define COG_BIO_MODULE_WORKING_MEMORY       0x2003

/** Module ID for reasoning */
#define COG_BIO_MODULE_REASONING            0x2004

/** Module ID for goal system */
#define COG_BIO_MODULE_GOAL                 0x2005

/** Module ID for salience */
#define COG_BIO_MODULE_SALIENCE             0x2006

/** Module ID for ethics */
#define COG_BIO_MODULE_ETHICS               0x2007

/** Module ID for curiosity */
#define COG_BIO_MODULE_CURIOSITY            0x2008

/** Module ID for introspection */
#define COG_BIO_MODULE_INTROSPECTION        0x2009

/** Module ID for theory of mind */
#define COG_BIO_MODULE_THEORY_OF_MIND       0x200A

/** Module ID for empathy */
#define COG_BIO_MODULE_EMPATHY              0x200B

/** Module ID for knowledge */
#define COG_BIO_MODULE_KNOWLEDGE            0x200C

/** Module ID for consolidation */
#define COG_BIO_MODULE_CONSOLIDATION        0x200D

/** Module ID for decision */
#define COG_BIO_MODULE_DECISION             0x200E

/** Module ID for executive control */
#define COG_BIO_MODULE_EXECUTIVE            0x200F

/** Maximum number of cognitive modules */
#define COG_BIO_MAX_MODULES                 16

/*=============================================================================
 * DEFAULT VALUES
 *===========================================================================*/

/** Default dopamine release on goal achievement */
#define COG_BIO_DEFAULT_DOPAMINE_RELEASE        0.4f

/** Default norepinephrine threshold for urgency */
#define COG_BIO_DEFAULT_NE_URGENCY_THRESHOLD    0.6f

/** Default coherence threshold for phase sync */
#define COG_BIO_DEFAULT_COHERENCE_THRESHOLD     0.75f

/** Default message queue size */
#define COG_BIO_DEFAULT_QUEUE_SIZE              512

/** Maximum message handlers per module */
#define COG_BIO_MAX_HANDLERS                    64

/*=============================================================================
 * MESSAGE TYPE ENUMS
 *===========================================================================*/

/**
 * @brief Cognitive module types for registration
 */
typedef enum {
    COG_MODULE_TYPE_ATTENTION = 0,
    COG_MODULE_TYPE_EMOTION,
    COG_MODULE_TYPE_WORKING_MEMORY,
    COG_MODULE_TYPE_REASONING,
    COG_MODULE_TYPE_GOAL,
    COG_MODULE_TYPE_SALIENCE,
    COG_MODULE_TYPE_ETHICS,
    COG_MODULE_TYPE_CURIOSITY,
    COG_MODULE_TYPE_INTROSPECTION,
    COG_MODULE_TYPE_THEORY_OF_MIND,
    COG_MODULE_TYPE_EMPATHY,
    COG_MODULE_TYPE_KNOWLEDGE,
    COG_MODULE_TYPE_CONSOLIDATION,
    COG_MODULE_TYPE_DECISION,
    COG_MODULE_TYPE_EXECUTIVE,
    COG_MODULE_TYPE_COUNT
} cog_module_type_t;

/**
 * @brief Bio-async message types for cognitive coordination
 *
 * Message types are organized by cognitive domain with unique IDs:
 * - 0x2001xx: Attention messages
 * - 0x2002xx: Emotion messages
 * - 0x2003xx: Working Memory messages
 * - 0x2004xx: Reasoning messages
 * - 0x2005xx: Goal System messages
 * - 0x2006xx: Salience messages
 * - 0x2007xx: Ethics messages
 * - 0x2008xx: Curiosity messages
 * - 0x2009xx: Introspection messages
 * - 0x200Axx: Theory of Mind messages
 * - 0x200Bxx: Empathy messages
 * - 0x200Cxx: Knowledge messages
 * - 0x200Dxx: Consolidation messages
 * - 0x200Exx: Decision messages
 * - 0x200Fxx: Executive Control messages
 * - 0x2000xx: Bridge/Global messages
 */
typedef enum {
    /*--------------------------------------------------
     * Bridge/Global Messages (0x2000xx)
     *------------------------------------------------*/
    COG_MSG_BRIDGE_STARTED          = 0x200001,  /**< Bridge initialization complete */
    COG_MSG_BRIDGE_STOPPED          = 0x200002,  /**< Bridge shutdown */
    COG_MSG_MODULE_REGISTERED       = 0x200003,  /**< Module registered with bridge */
    COG_MSG_MODULE_UNREGISTERED     = 0x200004,  /**< Module unregistered */
    COG_MSG_STATE_SYNC_REQUEST      = 0x200005,  /**< Request global state sync */
    COG_MSG_STATE_SYNC_COMPLETE     = 0x200006,  /**< State sync completed */
    COG_MSG_BROADCAST_ALL           = 0x200007,  /**< Broadcast to all modules */

    /*--------------------------------------------------
     * Attention Messages (0x2001xx)
     *------------------------------------------------*/
    COG_MSG_ATTENTION_SHIFT         = 0x200101,  /**< Attention focus shifted */
    COG_MSG_ATTENTION_CAPTURE       = 0x200102,  /**< Exogenous attention capture */
    COG_MSG_ATTENTION_RELEASE       = 0x200103,  /**< Attention released from target */
    COG_MSG_ATTENTION_SPLIT         = 0x200104,  /**< Attention divided */
    COG_MSG_ATTENTION_INHIBIT       = 0x200105,  /**< Inhibition of return */
    COG_MSG_ATTENTION_SPOTLIGHT     = 0x200106,  /**< Spotlight narrowed/widened */
    COG_MSG_ATTENTION_OVERLOAD      = 0x200107,  /**< Attention capacity exceeded */
    COG_MSG_ATTENTION_BLINK         = 0x200108,  /**< Attentional blink event */

    /*--------------------------------------------------
     * Emotion Messages (0x2002xx)
     *------------------------------------------------*/
    COG_MSG_EMOTION_UPDATE          = 0x200201,  /**< Emotional state changed */
    COG_MSG_EMOTION_PEAK            = 0x200202,  /**< Emotion reached peak */
    COG_MSG_EMOTION_DECAY           = 0x200203,  /**< Emotion decaying */
    COG_MSG_EMOTION_BLEND           = 0x200204,  /**< Multiple emotions blended */
    COG_MSG_EMOTION_REGULATION      = 0x200205,  /**< Emotion regulation triggered */
    COG_MSG_EMOTION_CONTAGION       = 0x200206,  /**< Emotional contagion detected */
    COG_MSG_EMOTION_SUPPRESSION     = 0x200207,  /**< Emotion suppressed */
    COG_MSG_MOOD_SHIFT              = 0x200208,  /**< Background mood changed */
    COG_MSG_VALENCE_CHANGE          = 0x200209,  /**< Valence (pos/neg) changed */
    COG_MSG_AROUSAL_CHANGE          = 0x20020A,  /**< Arousal level changed */

    /*--------------------------------------------------
     * Working Memory Messages (0x2003xx)
     *------------------------------------------------*/
    COG_MSG_WM_ITEM_ADDED           = 0x200301,  /**< Item added to working memory */
    COG_MSG_WM_ITEM_REMOVED         = 0x200302,  /**< Item removed from WM */
    COG_MSG_WM_ITEM_REFRESHED       = 0x200303,  /**< Item refreshed (decay reset) */
    COG_MSG_WM_ITEM_DECAYED         = 0x200304,  /**< Item decayed from WM */
    COG_MSG_WM_CAPACITY_WARNING     = 0x200305,  /**< Approaching capacity limit */
    COG_MSG_WM_OVERFLOW             = 0x200306,  /**< Capacity exceeded */
    COG_MSG_WM_CHUNK_FORMED         = 0x200307,  /**< Items chunked together */
    COG_MSG_WM_REHEARSAL_START      = 0x200308,  /**< Rehearsal loop started */
    COG_MSG_WM_TRANSFER_LTM         = 0x200309,  /**< Transfer to long-term memory */

    /*--------------------------------------------------
     * Reasoning Messages (0x2004xx)
     *------------------------------------------------*/
    COG_MSG_REASONING_START         = 0x200401,  /**< Reasoning process initiated */
    COG_MSG_REASONING_STEP          = 0x200402,  /**< Reasoning step completed */
    COG_MSG_REASONING_COMPLETE      = 0x200403,  /**< Reasoning concluded */
    COG_MSG_REASONING_BLOCKED       = 0x200404,  /**< Reasoning blocked/stuck */
    COG_MSG_INFERENCE_MADE          = 0x200405,  /**< New inference produced */
    COG_MSG_CONTRADICTION_FOUND     = 0x200406,  /**< Logical contradiction */
    COG_MSG_ANALOGY_FORMED          = 0x200407,  /**< Analogy discovered */
    COG_MSG_HYPOTHESIS_GENERATED    = 0x200408,  /**< Hypothesis created */
    COG_MSG_HYPOTHESIS_TESTED       = 0x200409,  /**< Hypothesis evaluated */
    COG_MSG_CAUSAL_LINK_FOUND       = 0x20040A,  /**< Causal relationship found */

    /*--------------------------------------------------
     * Goal System Messages (0x2005xx)
     *------------------------------------------------*/
    COG_MSG_GOAL_SET                = 0x200501,  /**< New goal activated */
    COG_MSG_GOAL_ACHIEVED           = 0x200502,  /**< Goal completed */
    COG_MSG_GOAL_ABANDONED          = 0x200503,  /**< Goal abandoned */
    COG_MSG_GOAL_BLOCKED            = 0x200504,  /**< Goal progress blocked */
    COG_MSG_GOAL_CONFLICT           = 0x200505,  /**< Goal conflict detected */
    COG_MSG_GOAL_PRIORITY_CHANGE    = 0x200506,  /**< Goal priority adjusted */
    COG_MSG_SUBGOAL_CREATED         = 0x200507,  /**< Subgoal decomposed */
    COG_MSG_GOAL_PROGRESS           = 0x200508,  /**< Progress toward goal */
    COG_MSG_GOAL_TIMEOUT            = 0x200509,  /**< Goal deadline approached */
    COG_MSG_GOAL_STACK_PUSH         = 0x20050A,  /**< Goal pushed to stack */
    COG_MSG_GOAL_STACK_POP          = 0x20050B,  /**< Goal popped from stack */

    /*--------------------------------------------------
     * Salience Messages (0x2006xx)
     *------------------------------------------------*/
    COG_MSG_SALIENCE_SPIKE          = 0x200601,  /**< High salience detected */
    COG_MSG_NOVELTY_DETECTED        = 0x200602,  /**< Novel stimulus found */
    COG_MSG_SURPRISE_EVENT          = 0x200603,  /**< Expectation violated */
    COG_MSG_URGENCY_SIGNAL          = 0x200604,  /**< Urgent response needed */
    COG_MSG_THREAT_DETECTED         = 0x200605,  /**< Potential threat */
    COG_MSG_OPPORTUNITY_DETECTED    = 0x200606,  /**< Potential opportunity */
    COG_MSG_SALIENCE_DECAY          = 0x200607,  /**< Salience decaying */
    COG_MSG_HABITUATION             = 0x200608,  /**< Stimulus habituated */

    /*--------------------------------------------------
     * Ethics Messages (0x2007xx)
     *------------------------------------------------*/
    COG_MSG_ETHICS_EVALUATION       = 0x200701,  /**< Ethical evaluation started */
    COG_MSG_ETHICS_VERDICT          = 0x200702,  /**< Ethical verdict rendered */
    COG_MSG_ETHICS_VIOLATION        = 0x200703,  /**< Ethics violation detected */
    COG_MSG_ETHICS_DILEMMA          = 0x200704,  /**< Ethical dilemma identified */
    COG_MSG_GOLDEN_RULE_CHECK       = 0x200705,  /**< Golden rule evaluation */
    COG_MSG_HARM_ASSESSMENT         = 0x200706,  /**< Potential harm assessed */
    COG_MSG_FAIRNESS_EVALUATION     = 0x200707,  /**< Fairness check performed */
    COG_MSG_CONSENT_CHECK           = 0x200708,  /**< Consent verification */

    /*--------------------------------------------------
     * Curiosity Messages (0x2008xx)
     *------------------------------------------------*/
    COG_MSG_CURIOSITY_SPIKE         = 0x200801,  /**< Curiosity activated */
    COG_MSG_KNOWLEDGE_GAP           = 0x200802,  /**< Knowledge gap detected */
    COG_MSG_EXPLORATION_START       = 0x200803,  /**< Exploration initiated */
    COG_MSG_EXPLORATION_COMPLETE    = 0x200804,  /**< Exploration finished */
    COG_MSG_LEARNING_OPPORTUNITY    = 0x200805,  /**< Learning chance found */
    COG_MSG_INFORMATION_GAINED      = 0x200806,  /**< New information acquired */
    COG_MSG_UNCERTAINTY_REDUCED     = 0x200807,  /**< Uncertainty decreased */
    COG_MSG_CURIOSITY_SATISFIED     = 0x200808,  /**< Curiosity satiated */

    /*--------------------------------------------------
     * Introspection Messages (0x2009xx)
     *------------------------------------------------*/
    COG_MSG_SELF_REFLECTION         = 0x200901,  /**< Self-reflection triggered */
    COG_MSG_META_AWARENESS          = 0x200902,  /**< Meta-cognitive awareness */
    COG_MSG_CONFIDENCE_ASSESSMENT   = 0x200903,  /**< Confidence level assessed */
    COG_MSG_UNCERTAINTY_DETECTED    = 0x200904,  /**< High uncertainty found */
    COG_MSG_MENTAL_STATE_QUERY      = 0x200905,  /**< Mental state queried */
    COG_MSG_CAPABILITY_ASSESSMENT   = 0x200906,  /**< Capability self-check */
    COG_MSG_LIMITATION_RECOGNIZED   = 0x200907,  /**< Limitation identified */
    COG_MSG_EXPLANATION_GENERATED   = 0x200908,  /**< Self-explanation created */

    /*--------------------------------------------------
     * Theory of Mind Messages (0x200Axx)
     *------------------------------------------------*/
    COG_MSG_TOM_BELIEF_UPDATE       = 0x200A01,  /**< Belief about other updated */
    COG_MSG_TOM_GOAL_INFERENCE      = 0x200A02,  /**< Other's goal inferred */
    COG_MSG_TOM_EMOTION_INFERENCE   = 0x200A03,  /**< Other's emotion inferred */
    COG_MSG_TOM_INTENT_INFERENCE    = 0x200A04,  /**< Other's intent inferred */
    COG_MSG_TOM_PERSPECTIVE_TAKE    = 0x200A05,  /**< Perspective taking event */
    COG_MSG_TOM_PREDICTION_MADE     = 0x200A06,  /**< Behavior prediction */
    COG_MSG_TOM_FALSE_BELIEF        = 0x200A07,  /**< False belief recognized */
    COG_MSG_TOM_DECEPTION_DETECTED  = 0x200A08,  /**< Deception suspected */

    /*--------------------------------------------------
     * Empathy Messages (0x200Bxx)
     *------------------------------------------------*/
    COG_MSG_EMPATHY_ACTIVATED       = 0x200B01,  /**< Empathetic response started */
    COG_MSG_EMOTIONAL_RESONANCE     = 0x200B02,  /**< Emotional resonance felt */
    COG_MSG_COMPASSION_TRIGGERED    = 0x200B03,  /**< Compassion activated */
    COG_MSG_DISTRESS_DETECTED       = 0x200B04,  /**< Other's distress detected */
    COG_MSG_SUPPORT_OFFERED         = 0x200B05,  /**< Support response generated */
    COG_MSG_BOUNDARY_MAINTAINED     = 0x200B06,  /**< Self-other boundary kept */
    COG_MSG_EMPATHY_FATIGUE         = 0x200B07,  /**< Empathy fatigue detected */

    /*--------------------------------------------------
     * Knowledge Messages (0x200Cxx)
     *------------------------------------------------*/
    COG_MSG_KNOWLEDGE_ADDED         = 0x200C01,  /**< Knowledge added */
    COG_MSG_KNOWLEDGE_RETRIEVED     = 0x200C02,  /**< Knowledge retrieved */
    COG_MSG_KNOWLEDGE_UPDATED       = 0x200C03,  /**< Knowledge updated */
    COG_MSG_KNOWLEDGE_CONFLICT      = 0x200C04,  /**< Knowledge conflict found */
    COG_MSG_SCHEMA_ACTIVATED        = 0x200C05,  /**< Schema activated */
    COG_MSG_CONCEPT_FORMED          = 0x200C06,  /**< New concept formed */
    COG_MSG_ASSOCIATION_MADE        = 0x200C07,  /**< Association created */
    COG_MSG_FORGETTING_EVENT        = 0x200C08,  /**< Knowledge forgotten */

    /*--------------------------------------------------
     * Consolidation Messages (0x200Dxx)
     *------------------------------------------------*/
    COG_MSG_CONSOLIDATION_START     = 0x200D01,  /**< Consolidation initiated */
    COG_MSG_CONSOLIDATION_COMPLETE  = 0x200D02,  /**< Consolidation finished */
    COG_MSG_REPLAY_EVENT            = 0x200D03,  /**< Memory replay event */
    COG_MSG_SLEEP_CONSOLIDATION     = 0x200D04,  /**< Sleep consolidation phase */
    COG_MSG_SYNAPTIC_DOWNSCALE      = 0x200D05,  /**< Synaptic downscaling */
    COG_MSG_MEMORY_INTEGRATION      = 0x200D06,  /**< Memories integrated */
    COG_MSG_SEMANTIC_EXTRACTION     = 0x200D07,  /**< Semantic gist extracted */

    /*--------------------------------------------------
     * Decision Messages (0x200Exx)
     *------------------------------------------------*/
    COG_MSG_DECISION_START          = 0x200E01,  /**< Decision process started */
    COG_MSG_DECISION_MADE           = 0x200E02,  /**< Decision finalized */
    COG_MSG_OPTION_EVALUATED        = 0x200E03,  /**< Option evaluated */
    COG_MSG_COMMITMENT_MADE         = 0x200E04,  /**< Committed to choice */
    COG_MSG_CHOICE_CONFLICT         = 0x200E05,  /**< Choice conflict detected */
    COG_MSG_REGRET_ANTICIPATED      = 0x200E06,  /**< Anticipated regret */
    COG_MSG_DECISION_DEFERRED       = 0x200E07,  /**< Decision postponed */
    COG_MSG_ACTION_SELECTED         = 0x200E08,  /**< Action selected */

    /*--------------------------------------------------
     * Executive Control Messages (0x200Fxx)
     *------------------------------------------------*/
    COG_MSG_TASK_SWITCH             = 0x200F01,  /**< Task switching event */
    COG_MSG_INHIBITION_ACTIVATED    = 0x200F02,  /**< Response inhibition */
    COG_MSG_COGNITIVE_LOAD_HIGH     = 0x200F03,  /**< High cognitive load */
    COG_MSG_RESOURCE_ALLOCATION     = 0x200F04,  /**< Resource reallocation */
    COG_MSG_CONFLICT_MONITORING     = 0x200F05,  /**< Conflict detected */
    COG_MSG_ERROR_MONITORING        = 0x200F06,  /**< Error detected */
    COG_MSG_STRATEGY_SHIFT          = 0x200F07,  /**< Strategy changed */
    COG_MSG_FATIGUE_WARNING         = 0x200F08,  /**< Cognitive fatigue warning */
    COG_MSG_EXECUTIVE_OVERRIDE      = 0x200F09   /**< Executive override applied */

} cog_bio_message_type_t;

/**
 * @brief Cognitive state transition types
 */
typedef enum {
    COG_TRANSITION_NONE = 0,
    COG_TRANSITION_IDLE_TO_ACTIVE,       /**< Activation from idle */
    COG_TRANSITION_ACTIVE_TO_FOCUSED,    /**< Deep focus entered */
    COG_TRANSITION_FOCUSED_TO_ACTIVE,    /**< Focus released */
    COG_TRANSITION_ACTIVE_TO_IDLE,       /**< Return to idle */
    COG_TRANSITION_TO_SLEEP,             /**< Sleep mode entered */
    COG_TRANSITION_FROM_SLEEP,           /**< Wake from sleep */
    COG_TRANSITION_EMERGENCY,            /**< Emergency state change */
    COG_TRANSITION_OVERLOAD,             /**< Cognitive overload */
    COG_TRANSITION_RECOVERY              /**< Recovery from overload */
} cog_state_transition_t;

/**
 * @brief Cognitive processing priority levels
 */
typedef enum {
    COG_PRIORITY_BACKGROUND = 0,         /**< Low priority background */
    COG_PRIORITY_NORMAL = 1,             /**< Normal priority */
    COG_PRIORITY_ELEVATED = 2,           /**< Elevated importance */
    COG_PRIORITY_HIGH = 3,               /**< High priority */
    COG_PRIORITY_URGENT = 4,             /**< Urgent processing */
    COG_PRIORITY_CRITICAL = 5            /**< Critical/emergency */
} cog_priority_t;

/*=============================================================================
 * EFFECTS STRUCTURES
 *===========================================================================*/

/**
 * @brief Effects flowing from cognitive modules to bio-async
 *
 * WHAT: Signals generated by cognitive processes for system-wide coordination
 * WHY:  Other brain modules need to know about cognitive state changes
 */
typedef struct {
    /* Dopamine channel - reward/achievement */
    float dopamine_release;              /**< Goal achievement signal [0.0-1.0] */
    uint32_t goals_achieved;             /**< Number of goals achieved */
    float prediction_accuracy;           /**< Recent prediction accuracy */

    /* Norepinephrine channel - urgency/arousal */
    float norepinephrine_level;          /**< Urgency level [0.0-1.0] */
    bool urgency_escalation;             /**< Priority escalation flag */
    cog_priority_t current_priority;     /**< Current processing priority */

    /* Acetylcholine channel - attention/encoding */
    float acetylcholine_level;           /**< Attention level [0.0-1.0] */
    uint32_t attention_target_id;        /**< Current attention target */
    float encoding_strength;             /**< Memory encoding strength */

    /* Serotonin channel - mood/patience */
    float serotonin_level;               /**< Mood/patience level [0.0-1.0] */
    float current_valence;               /**< Emotional valence [-1.0 to 1.0] */
    float current_arousal;               /**< Arousal level [0.0-1.0] */

    /* Cognitive load */
    float working_memory_load;           /**< WM utilization [0.0-1.0] */
    float cognitive_load;                /**< Overall cognitive load [0.0-1.0] */
    bool capacity_warning;               /**< Near capacity flag */

    /* Phase coupling requests */
    bool request_phase_sync;             /**< Request synchronization */
    uint32_t sync_module_count;          /**< Modules to synchronize */
    float desired_coherence;             /**< Target coherence level */

    /* Glial wave triggers */
    bool trigger_glial_wave;             /**< Trigger global wave */
    cog_state_transition_t transition;   /**< State transition type */
} cog_to_bio_async_effects_t;

/**
 * @brief Effects flowing from bio-async to cognitive modules
 *
 * WHAT: Signals from bio-async that modulate cognitive processing
 * WHY:  Cognitive modules should adapt to system-wide state
 */
typedef struct {
    /* Global state */
    float global_arousal;                /**< System arousal [0.0-1.0] */
    float global_valence;                /**< System valence [-1.0 to 1.0] */
    bool system_overload;                /**< System overload flag */
    bool system_idle;                    /**< System idle flag */

    /* Phase synchronization */
    bool phase_sync_achieved;            /**< Phase sync success */
    float current_coherence;             /**< Current coherence [0.0-1.0] */
    uint32_t synchronized_modules;       /**< Number of synced modules */

    /* Oscillation bands */
    uint8_t dominant_band;               /**< Dominant oscillation band */
    float band_power;                    /**< Power in dominant band */
    bool theta_active;                   /**< Theta rhythm active */
    bool gamma_active;                   /**< Gamma rhythm active */

    /* Glial wave state */
    bool glial_wave_active;              /**< Wave propagating */
    float wave_intensity;                /**< Current wave intensity */
    uint32_t wave_source_module;         /**< Module that triggered wave */

    /* Resource state */
    float available_capacity;            /**< Available processing [0.0-1.0] */
    bool throttling_active;              /**< Resource throttling on */
    float metabolic_state;               /**< Metabolic resources [0.0-1.0] */

    /* Timing */
    uint64_t current_tick_ms;            /**< Current system time */
    float circadian_factor;              /**< Circadian modulation */
    bool sleep_pressure_high;            /**< Need for consolidation */
} bio_async_to_cog_effects_t;

/*=============================================================================
 * CONFIGURATION
 *===========================================================================*/

/**
 * @brief Cognitive bio-async bridge configuration
 */
typedef struct {
    /* Neuromodulator sensitivity */
    float dopamine_sensitivity;          /**< Sensitivity to DA signals */
    float norepinephrine_sensitivity;    /**< Sensitivity to NE signals */
    float acetylcholine_sensitivity;     /**< Sensitivity to ACh signals */
    float serotonin_sensitivity;         /**< Sensitivity to 5-HT signals */

    /* Phase coupling */
    float coherence_threshold;           /**< Phase lock threshold */
    float coupling_strength;             /**< Coupling strength */
    uint8_t default_oscillation_band;    /**< Default band (0-4) */

    /* Message routing */
    bool enable_message_logging;         /**< Log all messages */
    uint32_t message_queue_size;         /**< Queue size per module */
    uint32_t max_handlers_per_type;      /**< Handlers per message type */

    /* Coordination */
    float attention_update_rate_hz;      /**< Attention update frequency */
    float emotion_update_rate_hz;        /**< Emotion update frequency */
    float goal_update_rate_hz;           /**< Goal system update freq */

    /* Glial waves */
    float glial_wave_threshold;          /**< Wave trigger threshold */
    float glial_wave_decay_rate;         /**< Wave decay rate */

    /* Cognitive load management */
    float load_warning_threshold;        /**< Warning threshold */
    float load_critical_threshold;       /**< Critical threshold */
    bool enable_adaptive_throttling;     /**< Auto-throttle on overload */
} cognitive_bio_bridge_config_t;

/*=============================================================================
 * MAIN STRUCTURE
 *===========================================================================*/

/**
 * @brief Cognitive bio-async bridge opaque handle
 */
typedef struct cognitive_bio_bridge cognitive_bio_bridge_t;

/**
 * @brief Registered cognitive module entry
 */
typedef struct {
    cog_module_type_t type;              /**< Module type */
    uint32_t module_id;                  /**< Module ID */
    void* module_ptr;                    /**< Module pointer */
    bool active;                         /**< Module active flag */
    uint64_t last_update_ms;             /**< Last update timestamp */
    uint64_t messages_sent;              /**< Messages sent count */
    uint64_t messages_received;          /**< Messages received count */
} cog_module_registration_t;

/*=============================================================================
 * CALLBACK TYPES
 *===========================================================================*/

/**
 * @brief Message handler callback type
 *
 * @param type Message type
 * @param payload Message payload
 * @param payload_size Payload size in bytes
 * @param user_data User-provided context
 */
typedef void (*cog_bio_message_handler_t)(
    cog_bio_message_type_t type,
    const void* payload,
    size_t payload_size,
    void* user_data
);

/**
 * @brief State change callback type
 *
 * @param transition State transition that occurred
 * @param source_module Module that initiated transition
 * @param user_data User-provided context
 */
typedef void (*cog_state_change_handler_t)(
    cog_state_transition_t transition,
    uint32_t source_module,
    void* user_data
);

/*=============================================================================
 * STATISTICS
 *===========================================================================*/

/**
 * @brief Bridge statistics
 */
typedef struct {
    /* Message counts */
    uint64_t total_messages_sent;
    uint64_t total_messages_received;
    uint64_t messages_dropped;
    uint64_t broadcast_count;

    /* Future tracking */
    uint64_t futures_created;
    uint64_t futures_completed;
    uint64_t futures_timed_out;

    /* Phase sync */
    uint64_t phase_syncs_requested;
    uint64_t phase_syncs_achieved;
    float avg_coherence_achieved;

    /* Glial waves */
    uint64_t glial_waves_initiated;
    uint64_t glial_waves_completed;

    /* Module activity */
    uint32_t active_modules;
    uint32_t registered_handlers;

    /* Performance */
    uint64_t total_update_time_us;
    uint64_t max_update_time_us;
    float avg_update_time_us;

    /* Neuromodulator averages */
    float avg_dopamine;
    float avg_norepinephrine;
    float avg_acetylcholine;
    float avg_serotonin;
} cognitive_bio_bridge_stats_t;

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

/**
 * @brief Get default bridge configuration
 * @return Default configuration values
 */
cognitive_bio_bridge_config_t cognitive_bio_bridge_default_config(void);

/**
 * @brief Create cognitive bio-async bridge
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
cognitive_bio_bridge_t* cognitive_bio_bridge_create(
    const cognitive_bio_bridge_config_t* config
);

/**
 * @brief Create bridge with default configuration
 * @return Bridge handle or NULL on error
 */
cognitive_bio_bridge_t* cognitive_bio_bridge_create_default(void);

/**
 * @brief Destroy cognitive bio-async bridge
 * @param bridge Bridge handle (NULL safe)
 */
void cognitive_bio_bridge_destroy(cognitive_bio_bridge_t* bridge);

/*=============================================================================
 * CONNECTION FUNCTIONS
 *===========================================================================*/

/**
 * @brief Connect bridge to bio-async system
 * @param bridge Bridge handle
 * @param bio_async Bio-async system handle
 * @return 0 on success, error code on failure
 */
int cognitive_bio_bridge_connect(
    cognitive_bio_bridge_t* bridge,
    struct nimcp_bio_async* bio_async
);

/**
 * @brief Connect bridge to brain
 * @param bridge Bridge handle
 * @param brain Brain handle
 * @return 0 on success, error code on failure
 */
int cognitive_bio_bridge_connect_brain(
    cognitive_bio_bridge_t* bridge,
    struct brain_struct* brain
);

/**
 * @brief Disconnect from bio-async system
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int cognitive_bio_bridge_disconnect(cognitive_bio_bridge_t* bridge);

/**
 * @brief Check if bridge is connected
 * @param bridge Bridge handle
 * @return true if fully connected
 */
bool cognitive_bio_bridge_is_connected(const cognitive_bio_bridge_t* bridge);

/*=============================================================================
 * MODULE REGISTRATION
 *===========================================================================*/

/**
 * @brief Register cognitive module with bridge
 * @param bridge Bridge handle
 * @param type Module type
 * @param module_ptr Pointer to module instance
 * @return 0 on success, error code on failure
 */
int cognitive_bio_bridge_register_module(
    cognitive_bio_bridge_t* bridge,
    cog_module_type_t type,
    void* module_ptr
);

/**
 * @brief Unregister cognitive module
 * @param bridge Bridge handle
 * @param type Module type to unregister
 * @return 0 on success, error code on failure
 */
int cognitive_bio_bridge_unregister_module(
    cognitive_bio_bridge_t* bridge,
    cog_module_type_t type
);

/**
 * @brief Get registered module
 * @param bridge Bridge handle
 * @param type Module type
 * @return Module registration or NULL if not found
 */
const cog_module_registration_t* cognitive_bio_bridge_get_module(
    const cognitive_bio_bridge_t* bridge,
    cog_module_type_t type
);

/**
 * @brief Check if module is registered
 * @param bridge Bridge handle
 * @param type Module type
 * @return true if registered
 */
bool cognitive_bio_bridge_module_registered(
    const cognitive_bio_bridge_t* bridge,
    cog_module_type_t type
);

/**
 * @brief Get count of registered modules
 * @param bridge Bridge handle
 * @return Number of registered modules
 */
uint32_t cognitive_bio_bridge_module_count(const cognitive_bio_bridge_t* bridge);

/*=============================================================================
 * UPDATE FUNCTION
 *===========================================================================*/

/**
 * @brief Update bridge state (call each frame/tick)
 * @param bridge Bridge handle
 * @param delta_time_ms Time since last update in milliseconds
 * @return 0 on success, error code on failure
 */
int cognitive_bio_bridge_update(
    cognitive_bio_bridge_t* bridge,
    float delta_time_ms
);

/*=============================================================================
 * MESSAGING FUNCTIONS
 *===========================================================================*/

/**
 * @brief Send message through bio-async
 * @param bridge Bridge handle
 * @param message_type Message type
 * @param payload Message payload
 * @param payload_size Payload size in bytes
 * @return 0 on success, error code on failure
 */
int cognitive_bio_bridge_send_message(
    cognitive_bio_bridge_t* bridge,
    cog_bio_message_type_t message_type,
    const void* payload,
    size_t payload_size
);

/**
 * @brief Send message to specific module
 * @param bridge Bridge handle
 * @param target_module Target module type
 * @param message_type Message type
 * @param payload Message payload
 * @param payload_size Payload size
 * @return 0 on success, error code on failure
 */
int cognitive_bio_bridge_send_to_module(
    cognitive_bio_bridge_t* bridge,
    cog_module_type_t target_module,
    cog_bio_message_type_t message_type,
    const void* payload,
    size_t payload_size
);

/**
 * @brief Register message handler
 * @param bridge Bridge handle
 * @param message_type Message type to handle
 * @param handler Handler callback
 * @param user_data User context
 * @return 0 on success, error code on failure
 */
int cognitive_bio_bridge_register_handler(
    cognitive_bio_bridge_t* bridge,
    cog_bio_message_type_t message_type,
    cog_bio_message_handler_t handler,
    void* user_data
);

/**
 * @brief Unregister message handler
 * @param bridge Bridge handle
 * @param message_type Message type
 * @param handler Handler to remove
 * @return 0 on success, error code on failure
 */
int cognitive_bio_bridge_unregister_handler(
    cognitive_bio_bridge_t* bridge,
    cog_bio_message_type_t message_type,
    cog_bio_message_handler_t handler
);

/*=============================================================================
 * BROADCAST FUNCTIONS
 *===========================================================================*/

/**
 * @brief Broadcast attention shift to all modules
 * @param bridge Bridge handle
 * @param target_id New attention target ID
 * @param intensity Attention intensity [0.0-1.0]
 * @param source_module Module initiating shift
 * @return 0 on success, error code on failure
 */
int cognitive_bio_bridge_broadcast_attention_shift(
    cognitive_bio_bridge_t* bridge,
    uint32_t target_id,
    float intensity,
    cog_module_type_t source_module
);

/**
 * @brief Broadcast emotion update to all modules
 * @param bridge Bridge handle
 * @param valence Emotional valence [-1.0 to 1.0]
 * @param arousal Arousal level [0.0-1.0]
 * @param dominant_emotion Dominant emotion ID
 * @return 0 on success, error code on failure
 */
int cognitive_bio_bridge_broadcast_emotion_update(
    cognitive_bio_bridge_t* bridge,
    float valence,
    float arousal,
    uint32_t dominant_emotion
);

/**
 * @brief Broadcast goal change to all modules
 * @param bridge Bridge handle
 * @param goal_id Goal identifier
 * @param goal_status Goal status (0=set, 1=achieved, 2=abandoned, 3=blocked)
 * @param priority Goal priority level
 * @return 0 on success, error code on failure
 */
int cognitive_bio_bridge_broadcast_goal_change(
    cognitive_bio_bridge_t* bridge,
    uint32_t goal_id,
    uint8_t goal_status,
    cog_priority_t priority
);

/**
 * @brief Broadcast salience spike to all modules
 * @param bridge Bridge handle
 * @param stimulus_id Stimulus identifier
 * @param salience_value Salience level [0.0-1.0]
 * @param is_threat True if potential threat
 * @return 0 on success, error code on failure
 */
int cognitive_bio_bridge_broadcast_salience_spike(
    cognitive_bio_bridge_t* bridge,
    uint32_t stimulus_id,
    float salience_value,
    bool is_threat
);

/**
 * @brief Broadcast cognitive load warning
 * @param bridge Bridge handle
 * @param current_load Current load level [0.0-1.0]
 * @param critical True if critical level
 * @return 0 on success, error code on failure
 */
int cognitive_bio_bridge_broadcast_load_warning(
    cognitive_bio_bridge_t* bridge,
    float current_load,
    bool critical
);

/*=============================================================================
 * STATE COORDINATION
 *===========================================================================*/

/**
 * @brief Request state synchronization across modules
 * @param bridge Bridge handle
 * @param modules Array of module types to sync
 * @param count Number of modules
 * @param coherence_threshold Required coherence [0.0-1.0]
 * @param timeout_ms Timeout in milliseconds
 * @return 0 on success, error code on failure/timeout
 */
int cognitive_bio_bridge_request_state_sync(
    cognitive_bio_bridge_t* bridge,
    const cog_module_type_t* modules,
    size_t count,
    float coherence_threshold,
    uint32_t timeout_ms
);

/**
 * @brief Register state change callback
 * @param bridge Bridge handle
 * @param handler State change handler
 * @param user_data User context
 * @return 0 on success, error code on failure
 */
int cognitive_bio_bridge_on_state_change(
    cognitive_bio_bridge_t* bridge,
    cog_state_change_handler_t handler,
    void* user_data
);

/**
 * @brief Trigger cognitive state transition
 * @param bridge Bridge handle
 * @param transition Transition type
 * @return 0 on success, error code on failure
 */
int cognitive_bio_bridge_trigger_transition(
    cognitive_bio_bridge_t* bridge,
    cog_state_transition_t transition
);

/*=============================================================================
 * NEUROMODULATOR CONTROL
 *===========================================================================*/

/**
 * @brief Release dopamine (goal achievement signal)
 * @param bridge Bridge handle
 * @param amount Release amount [0.0-1.0]
 * @param goal_id Associated goal ID
 * @return 0 on success, error code on failure
 */
int cognitive_bio_bridge_release_dopamine(
    cognitive_bio_bridge_t* bridge,
    float amount,
    uint32_t goal_id
);

/**
 * @brief Signal urgency via norepinephrine
 * @param bridge Bridge handle
 * @param urgency Urgency level [0.0-1.0]
 * @param source_module Module signaling urgency
 * @return 0 on success, error code on failure
 */
int cognitive_bio_bridge_signal_urgency(
    cognitive_bio_bridge_t* bridge,
    float urgency,
    cog_module_type_t source_module
);

/**
 * @brief Modulate attention via acetylcholine
 * @param bridge Bridge handle
 * @param attention Attention level [0.0-1.0]
 * @param target_id Attention target
 * @return 0 on success, error code on failure
 */
int cognitive_bio_bridge_modulate_attention(
    cognitive_bio_bridge_t* bridge,
    float attention,
    uint32_t target_id
);

/**
 * @brief Set mood/patience via serotonin
 * @param bridge Bridge handle
 * @param level Serotonin level [0.0-1.0]
 * @return 0 on success, error code on failure
 */
int cognitive_bio_bridge_set_mood_level(
    cognitive_bio_bridge_t* bridge,
    float level
);

/*=============================================================================
 * PHASE COUPLING
 *===========================================================================*/

/**
 * @brief Create phase synchronization for modules
 * @param bridge Bridge handle
 * @param modules Array of module types to sync
 * @param count Number of modules
 * @param band Oscillation band (0-4)
 * @param sync Output phase sync handle
 * @return 0 on success, error code on failure
 */
int cognitive_bio_bridge_create_phase_sync(
    cognitive_bio_bridge_t* bridge,
    const cog_module_type_t* modules,
    size_t count,
    uint8_t band,
    struct nimcp_phase_sync** sync
);

/**
 * @brief Wait for phase coherence
 * @param bridge Bridge handle
 * @param sync Phase sync handle
 * @param coherence_threshold Required coherence
 * @param timeout_ms Timeout
 * @return 0 on success, error code on timeout/failure
 */
int cognitive_bio_bridge_wait_coherent(
    cognitive_bio_bridge_t* bridge,
    struct nimcp_phase_sync* sync,
    float coherence_threshold,
    uint32_t timeout_ms
);

/*=============================================================================
 * GLIAL WAVES
 *===========================================================================*/

/**
 * @brief Initiate glial wave for state transition
 * @param bridge Bridge handle
 * @param transition State transition type
 * @param wave Output wave handle
 * @return 0 on success, error code on failure
 */
int cognitive_bio_bridge_initiate_glial_wave(
    cognitive_bio_bridge_t* bridge,
    cog_state_transition_t transition,
    struct nimcp_glial_wave** wave
);

/*=============================================================================
 * EFFECTS ACCESS
 *===========================================================================*/

/**
 * @brief Get outgoing effects (cognitive to bio-async)
 * @param bridge Bridge handle
 * @param effects Output effects structure
 * @return 0 on success, error code on failure
 */
int cognitive_bio_bridge_get_outgoing_effects(
    const cognitive_bio_bridge_t* bridge,
    cog_to_bio_async_effects_t* effects
);

/**
 * @brief Get incoming effects (bio-async to cognitive)
 * @param bridge Bridge handle
 * @param effects Output effects structure
 * @return 0 on success, error code on failure
 */
int cognitive_bio_bridge_get_incoming_effects(
    const cognitive_bio_bridge_t* bridge,
    bio_async_to_cog_effects_t* effects
);

/*=============================================================================
 * STATISTICS
 *===========================================================================*/

/**
 * @brief Get bridge statistics
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, error code on failure
 */
int cognitive_bio_bridge_get_stats(
    const cognitive_bio_bridge_t* bridge,
    cognitive_bio_bridge_stats_t* stats
);

/**
 * @brief Reset bridge statistics
 * @param bridge Bridge handle
 */
void cognitive_bio_bridge_reset_stats(cognitive_bio_bridge_t* bridge);

/*=============================================================================
 * UTILITY FUNCTIONS
 *===========================================================================*/

/**
 * @brief Get module type name as string
 * @param type Module type
 * @return Module type name
 */
const char* cognitive_bio_module_type_name(cog_module_type_t type);

/**
 * @brief Get message type name as string
 * @param type Message type
 * @return Message type name
 */
const char* cognitive_bio_message_type_name(cog_bio_message_type_t type);

/**
 * @brief Get state transition name as string
 * @param transition Transition type
 * @return Transition name
 */
const char* cognitive_bio_transition_name(cog_state_transition_t transition);

/**
 * @brief Get priority level name as string
 * @param priority Priority level
 * @return Priority name
 */
const char* cognitive_bio_priority_name(cog_priority_t priority);

/**
 * @brief Get module ID for module type
 * @param type Module type
 * @return Module ID constant
 */
uint32_t cognitive_bio_module_id(cog_module_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_COGNITIVE_BIO_ASYNC_BRIDGE_H */
