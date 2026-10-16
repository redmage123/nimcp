/**
 * @file nimcp_genius_types.h
 * @brief Genius profile type definitions and enumerations
 *
 * WHAT: Defines genius profile types, bio-async message IDs, and error codes
 * WHY:  Provides type-safe enumeration of cognitive excellence profiles
 * HOW:  Based on neuroscience research into domain-specific cognitive abilities
 *
 * @version 1.0.0
 * @date 2026-02-03
 */

#ifndef NIMCP_GENIUS_TYPES_H
#define NIMCP_GENIUS_TYPES_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * MODULE IDENTIFICATION
 * ============================================================================ */

/**
 * @brief Bio-async module ID for genius profiles
 *
 * Placed in cognitive module range (0x1F00) for proper routing
 */
#define BIO_MODULE_GENIUS_PROFILES      0x1F00

/**
 * @brief Module name for logging and KG wiring
 */
#define GENIUS_PROFILES_MODULE_NAME     "genius_profiles"

/**
 * @brief Module version
 */
#define GENIUS_PROFILES_VERSION_MAJOR   1
#define GENIUS_PROFILES_VERSION_MINOR   0
#define GENIUS_PROFILES_VERSION_PATCH   0

/* ============================================================================
 * BIO-ASYNC MESSAGE TYPES (0x1F00 - 0x1FFF)
 * ============================================================================ */

/**
 * @brief Genius profile bio-async message types
 *
 * Message range: 0x1F00 - 0x1FFF (256 messages)
 * Categories:
 *   - 0x1F00-0x1F0F: Profile lifecycle
 *   - 0x1F10-0x1F1F: Profile queries
 *   - 0x1F20-0x1F2F: Modulation/adaptation
 *   - 0x1F30-0x1F3F: Eidetic memory
 *   - 0x1F40-0x1F4F: Training integration
 *   - 0x1F50-0x1F5F: Immune integration
 *   - 0x1F60-0x1F6F: Mesh coordination
 *   - 0x1F70-0x1F7F: SNN/plasticity
 */
typedef enum {
    /* Profile lifecycle (0x1F00-0x1F0F) */
    BIO_MSG_GENIUS_PROFILE_ACTIVATE         = 0x1F00,  /**< Activate a genius profile */
    BIO_MSG_GENIUS_PROFILE_DEACTIVATE       = 0x1F01,  /**< Deactivate profile */
    BIO_MSG_GENIUS_PROFILE_SWITCH           = 0x1F02,  /**< Switch between profiles */
    BIO_MSG_GENIUS_PROFILE_BLEND            = 0x1F03,  /**< Blend multiple profiles (polymath) */
    BIO_MSG_GENIUS_PROFILE_RESET            = 0x1F04,  /**< Reset to baseline */
    BIO_MSG_GENIUS_PROFILE_CREATED          = 0x1F05,  /**< Profile instance created */
    BIO_MSG_GENIUS_PROFILE_DESTROYED        = 0x1F06,  /**< Profile instance destroyed */

    /* Profile queries (0x1F10-0x1F1F) */
    BIO_MSG_GENIUS_QUERY_STATE              = 0x1F10,  /**< Query current profile state */
    BIO_MSG_GENIUS_QUERY_TRAITS             = 0x1F11,  /**< Query trait values */
    BIO_MSG_GENIUS_QUERY_REGIONS            = 0x1F12,  /**< Query region configurations */
    BIO_MSG_GENIUS_QUERY_CONNECTIVITY       = 0x1F13,  /**< Query connectivity strengths */
    BIO_MSG_GENIUS_QUERY_EIDETIC            = 0x1F14,  /**< Query eidetic memory state */
    BIO_MSG_GENIUS_STATE_RESPONSE           = 0x1F15,  /**< Response to state query */
    BIO_MSG_GENIUS_TRAITS_RESPONSE          = 0x1F16,  /**< Response to traits query */

    /* Modulation/adaptation (0x1F20-0x1F2F) */
    BIO_MSG_GENIUS_MODULATE_STRENGTH        = 0x1F20,  /**< Modulate profile strength */
    BIO_MSG_GENIUS_ADAPT_CONTEXT            = 0x1F21,  /**< Adapt to task context */
    BIO_MSG_GENIUS_FATIGUE_UPDATE           = 0x1F22,  /**< Cognitive fatigue update */
    BIO_MSG_GENIUS_RECOVERY_NEEDED          = 0x1F23,  /**< Recovery period needed */
    BIO_MSG_GENIUS_FLOW_STATE_ENTER         = 0x1F24,  /**< Entering flow state */
    BIO_MSG_GENIUS_FLOW_STATE_EXIT          = 0x1F25,  /**< Exiting flow state */

    /* Eidetic memory (0x1F30-0x1F3F) */
    BIO_MSG_GENIUS_EIDETIC_ENCODE           = 0x1F30,  /**< Eidetic encoding request */
    BIO_MSG_GENIUS_EIDETIC_RETRIEVE         = 0x1F31,  /**< Eidetic retrieval request */
    BIO_MSG_GENIUS_EIDETIC_REFRESH          = 0x1F32,  /**< Refresh eidetic buffer */
    BIO_MSG_GENIUS_EIDETIC_DECAY            = 0x1F33,  /**< Eidetic decay notification */
    BIO_MSG_GENIUS_EIDETIC_CONSOLIDATE      = 0x1F34,  /**< Consolidate to long-term */

    /* Training integration (0x1F40-0x1F4F) */
    BIO_MSG_GENIUS_TRAINING_START           = 0x1F40,  /**< Start profile training */
    BIO_MSG_GENIUS_TRAINING_STEP            = 0x1F41,  /**< Training step complete */
    BIO_MSG_GENIUS_TRAINING_LOSS            = 0x1F42,  /**< Loss computed */
    BIO_MSG_GENIUS_TRAINING_GRADIENT        = 0x1F43,  /**< Gradient computed */
    BIO_MSG_GENIUS_TRAINING_COMPLETE        = 0x1F44,  /**< Training complete */

    /* Immune integration (0x1F50-0x1F5F) */
    BIO_MSG_GENIUS_IMMUNE_MODULATION        = 0x1F50,  /**< Immune modulation applied */
    BIO_MSG_GENIUS_CYTOKINE_EFFECT          = 0x1F51,  /**< Cytokine affecting profile */
    BIO_MSG_GENIUS_INFLAMMATION_RESPONSE    = 0x1F52,  /**< Inflammation response */
    BIO_MSG_GENIUS_HEALTH_STATUS            = 0x1F53,  /**< Health agent status */
    BIO_MSG_GENIUS_BBB_ALERT                = 0x1F54,  /**< BBB threat detected */

    /* Mesh coordination (0x1F60-0x1F6F) */
    BIO_MSG_GENIUS_MESH_PROPOSE             = 0x1F60,  /**< Propose profile change */
    BIO_MSG_GENIUS_MESH_ENDORSE             = 0x1F61,  /**< Endorse proposal */
    BIO_MSG_GENIUS_MESH_COMMIT              = 0x1F62,  /**< Commit change */
    BIO_MSG_GENIUS_MESH_CONSENSUS           = 0x1F63,  /**< Consensus reached */
    BIO_MSG_GENIUS_MESH_ROLLBACK            = 0x1F64,  /**< Rollback change */

    /* SNN/plasticity (0x1F70-0x1F7F) */
    BIO_MSG_GENIUS_SNN_CONFIG               = 0x1F70,  /**< SNN configuration update */
    BIO_MSG_GENIUS_STDP_EVENT               = 0x1F71,  /**< STDP learning event */
    BIO_MSG_GENIUS_PLASTICITY_UPDATE        = 0x1F72,  /**< Plasticity rule update */
    BIO_MSG_GENIUS_WEIGHT_CHANGE            = 0x1F73,  /**< Weight change notification */
    BIO_MSG_GENIUS_METAPLASTICITY           = 0x1F74,  /**< Metaplasticity shift */

    /* Quantum optimization (0x1F80-0x1F8F) */
    BIO_MSG_GENIUS_QUANTUM_OPTIMIZE         = 0x1F80,  /**< Quantum optimization request */
    BIO_MSG_GENIUS_QUANTUM_RESULT           = 0x1F81,  /**< Quantum result ready */
    BIO_MSG_GENIUS_QUANTUM_ANNEAL           = 0x1F82,  /**< Annealing step */

    /* Cognitive layer (0x1F90-0x1F9F) */
    BIO_MSG_GENIUS_RCOG_GOAL                = 0x1F90,  /**< RCOG goal submission */
    BIO_MSG_GENIUS_RCOG_RESULT              = 0x1F91,  /**< RCOG result */
    BIO_MSG_GENIUS_CCOG_UPDATE              = 0x1F92,  /**< CCOG state update */

    BIO_MSG_GENIUS_MAX                      = 0x1FFF   /**< Maximum message ID */
} genius_bio_message_t;

/* ============================================================================
 * GENIUS TYPE ENUMERATION
 * ============================================================================ */

/**
 * @brief Genius profile types based on cognitive neuroscience research
 *
 * Each type represents a distinct pattern of cognitive excellence
 * characterized by specific brain region enhancements and connectivity patterns.
 *
 * Exemplars:
 *   - MATHEMATICAL: Gauss, Newton, Ramanujan, Euler, von Neumann
 *   - VISUAL_ARTISTIC: Rembrandt, Van Gogh, Da Vinci, Picasso
 *   - MUSICAL: Mozart, Beethoven, Bach, Chopin
 *   - LITERARY: Shakespeare, Tolstoy, Dostoevsky, Goethe
 *   - SCIENTIFIC: Tesla, Darwin, Curie, Einstein
 *   - ATHLETIC: Jordan, Gretzky, Nureyev, Bolt
 *   - STRATEGIC: Napoleon, Churchill, Alexander, Sun Tzu
 *   - FINANCIAL: Buffett, Soros, Keynes, Simons
 *   - POLYMATH: Da Vinci, Leibniz (combines multiple)
 */
typedef enum {
    GENIUS_TYPE_MATHEMATICAL    = 0,    /**< Enhanced parietal, strong prefrontal-parietal */
    GENIUS_TYPE_VISUAL_ARTISTIC = 1,    /**< Enhanced V4/V8, right hemisphere dominance */
    GENIUS_TYPE_MUSICAL         = 2,    /**< Enlarged planum temporale, cerebellum timing */
    GENIUS_TYPE_LITERARY        = 3,    /**< Enhanced Broca's/Wernicke's, semantic networks */
    GENIUS_TYPE_SCIENTIFIC      = 4,    /**< Eidetic visual cortex, cross-domain association */
    GENIUS_TYPE_ATHLETIC        = 5,    /**< Enhanced motor cortex, fast visual processing */
    GENIUS_TYPE_STRATEGIC       = 6,    /**< Enhanced social cognition, risk assessment */
    GENIUS_TYPE_FINANCIAL       = 7,    /**< Risk assessment, temporal discounting, patterns */
    GENIUS_TYPE_POLYMATH        = 8,    /**< Combines multiple profiles with weighted blending */
    GENIUS_TYPE_COUNT           = 9,    /**< Number of genius types */
    GENIUS_TYPE_INVALID         = -1    /**< Invalid/uninitialized type */
} genius_type_t;

/**
 * @brief Get string name for genius type
 * @param type Genius type
 * @return Static string name, or "UNKNOWN" if invalid
 */
static inline const char* genius_type_name(genius_type_t type) {
    static const char* names[] = {
        "Mathematical",
        "Visual/Artistic",
        "Musical",
        "Literary",
        "Scientific",
        "Athletic",
        "Strategic",
        "Financial",
        "Polymath"
    };
    if (type >= 0 && type < GENIUS_TYPE_COUNT) {
        return names[type];
    }
    return "UNKNOWN";
}

/**
 * @brief Get exemplar names for genius type
 * @param type Genius type
 * @return Static string of exemplar names
 */
static inline const char* genius_type_exemplars(genius_type_t type) {
    static const char* exemplars[] = {
        "Gauss, Newton, Ramanujan, Euler",
        "Rembrandt, Van Gogh, Da Vinci, Picasso",
        "Mozart, Beethoven, Bach, Chopin",
        "Shakespeare, Tolstoy, Dostoevsky",
        "Tesla, Darwin, Curie, Einstein",
        "Jordan, Gretzky, Nureyev, Bolt",
        "Napoleon, Churchill, Alexander",
        "Buffett, Soros, Keynes, Simons",
        "Da Vinci, Leibniz, Goethe"
    };
    if (type >= 0 && type < GENIUS_TYPE_COUNT) {
        return exemplars[type];
    }
    return "Unknown";
}

/* ============================================================================
 * EIDETIC MEMORY MODALITY
 * ============================================================================ */

/**
 * @brief Eidetic memory modality types
 *
 * Different geniuses exhibit different eidetic strengths:
 *   - Tesla: Visual-spatial (could visualize complete machines)
 *   - Mozart: Auditory (replayed Miserere after one hearing)
 *   - von Neumann: Verbal/numerical (photographic text memory)
 *   - Kim Peek: Encyclopedic (factual recall)
 */
typedef enum {
    EIDETIC_MODALITY_VISUAL     = 0,    /**< Visual/photographic memory */
    EIDETIC_MODALITY_AUDITORY   = 1,    /**< Auditory/echoic memory */
    EIDETIC_MODALITY_SPATIAL    = 2,    /**< Spatial/3D memory */
    EIDETIC_MODALITY_VERBAL     = 3,    /**< Verbal/text memory */
    EIDETIC_MODALITY_NUMERICAL  = 4,    /**< Numerical sequence memory */
    EIDETIC_MODALITY_KINESTHETIC= 5,    /**< Motor/procedural memory */
    EIDETIC_MODALITY_COUNT      = 6
} eidetic_modality_t;

/* ============================================================================
 * PROFILE ACTIVATION STATE
 * ============================================================================ */

/**
 * @brief Genius profile activation states
 */
typedef enum {
    GENIUS_STATE_INACTIVE       = 0,    /**< Profile not active */
    GENIUS_STATE_ACTIVATING     = 1,    /**< Profile being activated */
    GENIUS_STATE_ACTIVE         = 2,    /**< Profile fully active */
    GENIUS_STATE_BLENDED        = 3,    /**< Multiple profiles blended */
    GENIUS_STATE_FATIGUED       = 4,    /**< Cognitive fatigue - reduced capacity */
    GENIUS_STATE_RECOVERING     = 5,    /**< Recovery period */
    GENIUS_STATE_FLOW           = 6,    /**< In flow state - peak performance */
    GENIUS_STATE_DEGRADED       = 7,    /**< Degraded by immune response */
    GENIUS_STATE_ERROR          = 8     /**< Error state */
} genius_activation_state_t;

/**
 * @brief Get string name for activation state
 */
static inline const char* genius_state_name(genius_activation_state_t state) {
    static const char* names[] = {
        "Inactive", "Activating", "Active", "Blended",
        "Fatigued", "Recovering", "Flow", "Degraded", "Error"
    };
    if (state >= 0 && state <= GENIUS_STATE_ERROR) {
        return names[state];
    }
    return "Unknown";
}

/* ============================================================================
 * ERROR CODES
 * ============================================================================ */

/**
 * @brief Genius profile specific error codes
 *
 * Base: 0x1F00 (matches module ID)
 */
typedef enum {
    GENIUS_ERROR_SUCCESS                = 0,        /**< Success */
    GENIUS_ERROR_NULL_POINTER           = 0x1F01,   /**< Null pointer argument */
    GENIUS_ERROR_INVALID_TYPE           = 0x1F02,   /**< Invalid genius type */
    GENIUS_ERROR_INVALID_STATE          = 0x1F03,   /**< Invalid activation state */
    GENIUS_ERROR_ALREADY_ACTIVE         = 0x1F04,   /**< Profile already active */
    GENIUS_ERROR_NOT_ACTIVE             = 0x1F05,   /**< Profile not active */
    GENIUS_ERROR_BLEND_FAILED           = 0x1F06,   /**< Profile blending failed */
    GENIUS_ERROR_EIDETIC_OVERFLOW       = 0x1F07,   /**< Eidetic buffer overflow */
    GENIUS_ERROR_EIDETIC_DECAY          = 0x1F08,   /**< Eidetic memory decayed */
    GENIUS_ERROR_IMMUNE_BLOCKED         = 0x1F09,   /**< Blocked by immune system */
    GENIUS_ERROR_BBB_REJECTED           = 0x1F0A,   /**< Rejected by BBB */
    GENIUS_ERROR_MESH_CONSENSUS_FAILED  = 0x1F0B,   /**< Mesh consensus failed */
    GENIUS_ERROR_TRAINING_FAILED        = 0x1F0C,   /**< Training step failed */
    GENIUS_ERROR_SNN_CONFIG_INVALID     = 0x1F0D,   /**< Invalid SNN configuration */
    GENIUS_ERROR_QUANTUM_FAILED         = 0x1F0E,   /**< Quantum optimization failed */
    GENIUS_ERROR_MEMORY_ALLOCATION      = 0x1F0F,   /**< Memory allocation failed */
    GENIUS_ERROR_BIO_ASYNC_FAILED       = 0x1F10,   /**< Bio-async operation failed */
    GENIUS_ERROR_KG_WIRING_FAILED       = 0x1F11,   /**< KG wiring failed */
    GENIUS_ERROR_BRIDGE_DISCONNECTED    = 0x1F12,   /**< Bridge not connected */
    GENIUS_ERROR_FATIGUE_LIMIT          = 0x1F13,   /**< Cognitive fatigue limit reached */
    GENIUS_ERROR_FLOW_INTERRUPTED       = 0x1F14,   /**< Flow state interrupted */
    GENIUS_ERROR_CONTEXT_MISMATCH       = 0x1F15,   /**< Context mismatch for profile */
    GENIUS_ERROR_INVALID_PARAM          = 0x1F16,   /**< Invalid parameter */
    GENIUS_ERROR_MESSAGE_TOO_SMALL      = 0x1F17,   /**< Message size too small */
    GENIUS_ERROR_KG_UNAVAILABLE         = 0x1F18,   /**< Knowledge graph not available */
    GENIUS_ERROR_KG_REGISTRATION_FAILED = 0x1F19,   /**< KG node registration failed */
    GENIUS_ERROR_MESH_PROPOSAL_FAILED   = 0x1F1A,   /**< Mesh transaction proposal failed */
    GENIUS_ERROR_MESH_ENDORSEMENT_FAILED= 0x1F1B,   /**< Mesh endorsement failed */
    GENIUS_ERROR_BRAIN_CREATION_FAILED  = 0x1F1C,   /**< Brain factory creation failed */
    GENIUS_ERROR_FLOW_STATE_CONFLICT    = 0x1F1D,   /**< Operation conflicts with flow state */
    GENIUS_ERROR_FATIGUE_EXCEEDED       = 0x1F1E,   /**< Fatigue level too high for operation */
    GENIUS_ERROR_MAX                    = 0x1FFF    /**< Maximum error code */
} genius_error_t;

/**
 * @brief Get error message for genius error code
 */
static inline const char* genius_error_message(genius_error_t error) {
    switch (error) {
        case GENIUS_ERROR_SUCCESS:              return "Success";
        case GENIUS_ERROR_NULL_POINTER:         return "Null pointer argument";
        case GENIUS_ERROR_INVALID_TYPE:         return "Invalid genius type";
        case GENIUS_ERROR_INVALID_STATE:        return "Invalid activation state";
        case GENIUS_ERROR_ALREADY_ACTIVE:       return "Profile already active";
        case GENIUS_ERROR_NOT_ACTIVE:           return "Profile not active";
        case GENIUS_ERROR_BLEND_FAILED:         return "Profile blending failed";
        case GENIUS_ERROR_EIDETIC_OVERFLOW:     return "Eidetic buffer overflow";
        case GENIUS_ERROR_EIDETIC_DECAY:        return "Eidetic memory decayed";
        case GENIUS_ERROR_IMMUNE_BLOCKED:       return "Blocked by immune system";
        case GENIUS_ERROR_BBB_REJECTED:         return "Rejected by BBB";
        case GENIUS_ERROR_MESH_CONSENSUS_FAILED:return "Mesh consensus failed";
        case GENIUS_ERROR_TRAINING_FAILED:      return "Training step failed";
        case GENIUS_ERROR_SNN_CONFIG_INVALID:   return "Invalid SNN configuration";
        case GENIUS_ERROR_QUANTUM_FAILED:       return "Quantum optimization failed";
        case GENIUS_ERROR_MEMORY_ALLOCATION:    return "Memory allocation failed";
        case GENIUS_ERROR_BIO_ASYNC_FAILED:     return "Bio-async operation failed";
        case GENIUS_ERROR_KG_WIRING_FAILED:     return "KG wiring failed";
        case GENIUS_ERROR_BRIDGE_DISCONNECTED:  return "Bridge not connected";
        case GENIUS_ERROR_FATIGUE_LIMIT:        return "Cognitive fatigue limit reached";
        case GENIUS_ERROR_FLOW_INTERRUPTED:     return "Flow state interrupted";
        case GENIUS_ERROR_CONTEXT_MISMATCH:     return "Context mismatch for profile";
        case GENIUS_ERROR_INVALID_PARAM:        return "Invalid parameter";
        case GENIUS_ERROR_MESSAGE_TOO_SMALL:    return "Message size too small";
        case GENIUS_ERROR_KG_UNAVAILABLE:       return "Knowledge graph not available";
        case GENIUS_ERROR_KG_REGISTRATION_FAILED: return "KG node registration failed";
        case GENIUS_ERROR_MESH_PROPOSAL_FAILED: return "Mesh transaction proposal failed";
        case GENIUS_ERROR_MESH_ENDORSEMENT_FAILED: return "Mesh endorsement failed";
        case GENIUS_ERROR_BRAIN_CREATION_FAILED: return "Brain factory creation failed";
        case GENIUS_ERROR_FLOW_STATE_CONFLICT:  return "Operation conflicts with flow state";
        case GENIUS_ERROR_FATIGUE_EXCEEDED:     return "Fatigue level too high";
        default:                                return "Unknown error";
    }
}

/* ============================================================================
 * EXCEPTION CATEGORY
 * ============================================================================ */

/**
 * @brief Exception category for genius module
 *
 * Used for immune system integration - exceptions are presented as antigens
 */
#define GENIUS_EXCEPTION_CATEGORY       21  /* Cognitive subsystem category */

/**
 * @brief Exception epitope size for immune recognition
 */
#define GENIUS_EPITOPE_SIZE             16

/* ============================================================================
 * CAPACITY LIMITS
 * ============================================================================ */

/**
 * @brief Maximum number of active profiles (for polymath blending)
 */
#define GENIUS_MAX_ACTIVE_PROFILES      4

/**
 * @brief Maximum number of blend weights
 */
#define GENIUS_MAX_BLEND_WEIGHTS        GENIUS_MAX_ACTIVE_PROFILES

/**
 * @brief Maximum eidetic buffer items per modality
 */
#define GENIUS_EIDETIC_BUFFER_SIZE      256

/**
 * @brief Maximum custom parameters per region
 */
#define GENIUS_MAX_REGION_PARAMS        8

/**
 * @brief Maximum KG entity name length
 */
#define GENIUS_KG_NAME_MAX              64

/* ============================================================================
 * NEUROMODULATOR CHANNELS
 * ============================================================================ */

/**
 * @brief Preferred neuromodulator channels for genius profiles
 *
 * Different profiles use different neuromodulators for optimal performance:
 *   - Mathematical: ACh (attention) + DA (reward for insights)
 *   - Artistic: DA (creativity) + 5-HT (mood/color perception)
 *   - Athletic: NE (alertness) + DA (motor reward)
 */
typedef enum {
    GENIUS_CHANNEL_DOPAMINE         = 0,    /**< Reward, motivation, creativity */
    GENIUS_CHANNEL_SEROTONIN        = 1,    /**< Mood, patience, emotional processing */
    GENIUS_CHANNEL_NOREPINEPHRINE   = 2,    /**< Alertness, arousal, stress response */
    GENIUS_CHANNEL_ACETYLCHOLINE    = 3,    /**< Attention, memory encoding, focus */
    GENIUS_CHANNEL_COUNT            = 4
} genius_neuromod_channel_t;

/* ============================================================================
 * FORWARD DECLARATIONS
 * ============================================================================ */

/* Main structures defined in other headers */
typedef struct genius_profile_t genius_profile_t;
typedef struct genius_traits_t genius_traits_t;
typedef struct genius_region_config_t genius_region_config_t;
typedef struct genius_connectivity_t genius_connectivity_t;
typedef struct eidetic_memory_config_t eidetic_memory_config_t;
typedef struct genius_profiles_bridge_t genius_profiles_bridge_t;

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_GENIUS_TYPES_H */
