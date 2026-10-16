/**
 * @file nimcp_hypothalamus_attention_bridge.h
 * @brief Hypothalamus -> Attention Bridge for Drive-Biased Salience Modulation
 *
 * WHAT: Bridge between hypothalamus drives and attention/salience systems
 * WHY:  Drive states must bias attention (hungry = food salient, threatened = threats salient)
 * HOW:  Maps drive urgencies to salience modulation weights, modulates attention gate
 *
 * BYRNES MODEL CONTEXT:
 * The steering subsystem (hypothalamus) must influence what the learning subsystem
 * attends to. Drive states create a "lens" through which perception is filtered:
 * - High HUNGER drive: Food-related stimuli become more salient
 * - High SAFETY drive: Potential threats get attention priority
 * - High CURIOSITY drive: Novel stimuli become more attention-grabbing
 *
 * SALIENCE MODULATION:
 * Each drive type maps to salience categories:
 * - HUNGER -> Food, eating, nourishment salience boosted
 * - THIRST -> Water, drinking, hydration salience boosted
 * - SAFETY -> Threat, danger, escape route salience boosted
 * - SOCIAL -> Faces, social cues, cooperation salience boosted
 * - CURIOSITY -> Novel, unexpected, information-rich salience boosted
 *
 * BIO-ASYNC MESSAGES:
 * - Receives: BIO_MSG_HYPO_DRIVE_STATE (from hypothalamus)
 * - Sends: BIO_MSG_ATTENTION_MODULATION, BIO_MSG_SALIENCE_QUERY
 *
 * @version Phase 7: Attention Bridge
 * @date 2026-01-04
 */

#ifndef NIMCP_HYPOTHALAMUS_ATTENTION_BRIDGE_H
#define NIMCP_HYPOTHALAMUS_ATTENTION_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_drives.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "utils/bridge/nimcp_bridge_base.h"

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

/** Maximum number of salience categories */
#define HYPO_ATTN_MAX_CATEGORIES    16

/** Maximum number of attention targets to modulate */
#define HYPO_ATTN_MAX_TARGETS       64

/** Default urgency threshold for salience boost */
#define HYPO_ATTN_URGENCY_THRESHOLD 0.3f

/** Maximum salience boost multiplier */
#define HYPO_ATTN_MAX_BOOST         3.0f

/** Default salience boost scale */
#define HYPO_ATTN_DEFAULT_BOOST     1.5f

/*=============================================================================
 * SALIENCE CATEGORY TYPES
 *===========================================================================*/

/**
 * @brief Salience category types that can be modulated by drives
 *
 * These map to semantic categories that attention systems recognize.
 * When a drive is urgent, its associated categories get salience boost.
 */
typedef enum {
    HYPO_SAL_CAT_FOOD = 0,          /**< Food and eating related */
    HYPO_SAL_CAT_WATER,             /**< Water and drinking related */
    HYPO_SAL_CAT_THREAT,            /**< Potential dangers and threats */
    HYPO_SAL_CAT_ESCAPE,            /**< Escape routes and safety */
    HYPO_SAL_CAT_SOCIAL,            /**< Faces and social cues */
    HYPO_SAL_CAT_NOVEL,             /**< Novel and unexpected stimuli */
    HYPO_SAL_CAT_INFORMATION,       /**< Information-rich stimuli */
    HYPO_SAL_CAT_COMPETENCE,        /**< Skill and mastery related */
    HYPO_SAL_CAT_AUTONOMY,          /**< Choice and control related */
    HYPO_SAL_CAT_REST,              /**< Rest and comfort related */
    HYPO_SAL_CAT_TEMPERATURE,       /**< Temperature related (hot/cold) */
    HYPO_SAL_CAT_ALIGNMENT,         /**< Alignment-relevant (harm, honesty) */
    HYPO_SAL_CAT_COUNT
} hypo_salience_category_t;

/**
 * @brief Attention modulation mode
 */
typedef enum {
    HYPO_ATTN_MODE_MULTIPLICATIVE = 0, /**< Multiply salience by boost factor */
    HYPO_ATTN_MODE_ADDITIVE,           /**< Add boost to salience */
    HYPO_ATTN_MODE_GATING,             /**< Binary gate (pass/block) */
    HYPO_ATTN_MODE_MIXED               /**< Combination of modes */
} hypo_attn_mode_t;

/*=============================================================================
 * SALIENCE MODULATION STRUCTURES
 *===========================================================================*/

/**
 * @brief Category-specific salience modulation
 */
typedef struct {
    hypo_salience_category_t category;  /**< Which category */
    float base_weight;                   /**< Base category weight [0,1] */
    float drive_boost;                   /**< Current boost from drives */
    float effective_weight;              /**< Base * (1 + boost) */
    hypo_drive_type_t primary_drive;     /**< Which drive dominates this category */
    float drive_sensitivity;             /**< How sensitive to drive urgency */
} hypo_category_modulation_t;

/**
 * @brief Overall salience modulation state
 */
typedef struct {
    hypo_category_modulation_t categories[HYPO_SAL_CAT_COUNT];

    /* Global modulation */
    float global_gain;                   /**< Global salience multiplier (arousal) */
    float novelty_boost;                 /**< Extra boost for novel stimuli */
    float threat_boost;                  /**< Extra boost for threats (safety critical) */

    /* Active drive influence */
    hypo_drive_type_t dominant_drive;    /**< Currently most urgent drive */
    float dominant_urgency;              /**< Urgency of dominant drive */

    /* Statistics */
    uint64_t modulations_applied;
    uint64_t timestamp_us;
} hypo_salience_state_t;

/**
 * @brief Attention target modulation
 */
typedef struct {
    uint32_t target_id;                  /**< Target being modulated */
    hypo_salience_category_t category;   /**< Category of this target */
    float original_salience;             /**< Salience before modulation */
    float modulated_salience;            /**< Salience after drive modulation */
    float boost_applied;                 /**< Boost that was applied */
    bool is_suppressed;                  /**< Suppressed by competing drive */
} hypo_attn_target_mod_t;

/*=============================================================================
 * BRIDGE CONFIGURATION
 *===========================================================================*/

/**
 * @brief Attention bridge configuration
 */
typedef struct {
    /* Modulation parameters */
    hypo_attn_mode_t mode;               /**< Modulation mode */
    float urgency_threshold;             /**< Min urgency for boost */
    float max_boost;                     /**< Maximum boost multiplier */
    float boost_scale;                   /**< Boost scaling factor */

    /* Drive-to-category mapping weights */
    float drive_category_map[HYPO_DRIVE_COUNT][HYPO_SAL_CAT_COUNT];

    /* Category base weights */
    float category_base_weights[HYPO_SAL_CAT_COUNT];

    /* Safety/alignment parameters */
    float alignment_boost;               /**< Extra boost for alignment-relevant */
    float threat_priority;               /**< Priority for threat detection */
    bool safety_override;                /**< Always boost threat salience */

    /* Integration options */
    bool connect_attention_gate;         /**< Connect to attention gate */
    bool connect_salience_module;        /**< Connect to salience evaluator */
    bool broadcast_enabled;              /**< Enable bio-async broadcasts */
} hypo_attn_bridge_config_t;

/**
 * @brief Attention bridge context
 */
typedef struct {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    /* Configuration */
    hypo_attn_bridge_config_t config;

    /* State */
    hypo_salience_state_t salience;

    /* Target modulations (cached) */
    hypo_attn_target_mod_t targets[HYPO_ATTN_MAX_TARGETS];
    uint32_t active_targets;

    /* Connected modules */
    hypo_drive_system_handle_t* drives;  /**< Hypothalamus drives (source) */
    void* attention_gate;                /**< Attention gate (optional) */
    void* salience_evaluator;            /**< Salience evaluator (optional) */

    /* Bio-async context */
    bio_module_context_t bio_ctx;

    /* Statistics */
    uint64_t modulations_computed;
    uint64_t targets_boosted;
    uint64_t targets_suppressed;
    uint64_t broadcasts_sent;

} hypo_attn_bridge_t;

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

/**
 * @brief Get default attention bridge configuration
 *
 * Initializes drive-to-category mappings with biologically plausible defaults:
 * - HUNGER strongly maps to FOOD category
 * - SAFETY strongly maps to THREAT and ESCAPE categories
 * - CURIOSITY maps to NOVEL and INFORMATION categories
 *
 * @return Default configuration
 */
hypo_attn_bridge_config_t hypo_attn_bridge_default_config(void);

/**
 * @brief Create attention bridge
 *
 * @param drives Hypothalamus drive system handle
 * @param config Bridge configuration (NULL for defaults)
 * @return Bridge context, or NULL on failure
 */
hypo_attn_bridge_t* hypo_attn_bridge_create(
    hypo_drive_system_handle_t* drives,
    const hypo_attn_bridge_config_t* config);

/**
 * @brief Destroy attention bridge
 *
 * @param bridge Bridge to destroy
 */
void hypo_attn_bridge_destroy(hypo_attn_bridge_t* bridge);

/**
 * @brief Reset bridge to initial state
 *
 * @param bridge Bridge to reset
 */
void hypo_attn_bridge_reset(hypo_attn_bridge_t* bridge);

/*=============================================================================
 * CORE FUNCTIONS
 *===========================================================================*/

/**
 * @brief Update salience modulation based on current drive states
 *
 * WHAT: Recompute category boosts based on drive urgencies
 * WHY:  Drive states must influence attention allocation
 * HOW:  Maps each drive's urgency to category boosts via drive_category_map
 *
 * @param bridge Bridge context
 * @return Updated salience state
 */
hypo_salience_state_t hypo_attn_bridge_update_modulation(
    hypo_attn_bridge_t* bridge);

/**
 * @brief Modulate a single target's salience
 *
 * @param bridge Bridge context
 * @param target_id Target identifier
 * @param category Target's semantic category
 * @param base_salience Original salience [0,1]
 * @return Modulated salience [0, max_boost]
 */
float hypo_attn_bridge_modulate_target(
    hypo_attn_bridge_t* bridge,
    uint32_t target_id,
    hypo_salience_category_t category,
    float base_salience);

/**
 * @brief Modulate multiple targets (batch operation)
 *
 * @param bridge Bridge context
 * @param target_ids Array of target identifiers
 * @param categories Array of target categories
 * @param base_saliences Array of base salience values
 * @param modulated_out Output: modulated saliences
 * @param count Number of targets
 * @return Number of targets successfully modulated
 */
uint32_t hypo_attn_bridge_modulate_batch(
    hypo_attn_bridge_t* bridge,
    const uint32_t* target_ids,
    const hypo_salience_category_t* categories,
    const float* base_saliences,
    float* modulated_out,
    uint32_t count);

/**
 * @brief Get category boost for a salience category
 *
 * @param bridge Bridge context
 * @param category Salience category
 * @return Current boost factor for this category
 */
float hypo_attn_bridge_get_category_boost(
    const hypo_attn_bridge_t* bridge,
    hypo_salience_category_t category);

/**
 * @brief Get dominant drive affecting attention
 *
 * @param bridge Bridge context
 * @param urgency_out Output: urgency of dominant drive (optional)
 * @return Dominant drive type
 */
hypo_drive_type_t hypo_attn_bridge_get_dominant_drive(
    const hypo_attn_bridge_t* bridge,
    float* urgency_out);

/*=============================================================================
 * ATTENTION GATE INTEGRATION
 *===========================================================================*/

/**
 * @brief Connect to attention gate module
 *
 * @param bridge Bridge context
 * @param gate Attention gate handle
 * @return true if connected
 */
bool hypo_attn_bridge_connect_gate(
    hypo_attn_bridge_t* bridge,
    void* gate);

/**
 * @brief Push salience modulation to attention gate
 *
 * Updates the attention gate with current drive-based modulation weights.
 *
 * @param bridge Bridge context
 * @return Number of targets updated in gate
 */
uint32_t hypo_attn_bridge_push_to_gate(hypo_attn_bridge_t* bridge);

/**
 * @brief Connect to salience evaluator
 *
 * @param bridge Bridge context
 * @param evaluator Salience evaluator handle
 * @return true if connected
 */
bool hypo_attn_bridge_connect_salience(
    hypo_attn_bridge_t* bridge,
    void* evaluator);

/*=============================================================================
 * BIO-ASYNC INTEGRATION
 *===========================================================================*/

/**
 * @brief Register with bio-async router
 *
 * @param bridge Bridge context
 * @param use_kg_wiring Use KG-driven wiring (true) or legacy (false)
 * @return true on success
 */
bool hypo_attn_bridge_register_bio(
    hypo_attn_bridge_t* bridge,
    bool use_kg_wiring);

/**
 * @brief Process incoming bio-async messages
 *
 * @param bridge Bridge context
 * @param max_messages Maximum messages to process
 * @return Number of messages processed
 */
uint32_t hypo_attn_bridge_process_bio(
    hypo_attn_bridge_t* bridge,
    uint32_t max_messages);

/**
 * @brief Broadcast salience modulation state
 *
 * @param bridge Bridge context
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t hypo_attn_bridge_broadcast_modulation(hypo_attn_bridge_t* bridge);

/*=============================================================================
 * STATISTICS
 *===========================================================================*/

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge context
 * @param modulations_computed Output: total modulations computed
 * @param targets_boosted Output: targets with positive boost
 * @param targets_suppressed Output: targets with suppression
 */
void hypo_attn_bridge_get_stats(
    const hypo_attn_bridge_t* bridge,
    uint64_t* modulations_computed,
    uint64_t* targets_boosted,
    uint64_t* targets_suppressed);

/*=============================================================================
 * UTILITY FUNCTIONS
 *===========================================================================*/

/**
 * @brief Get string representation of salience category
 *
 * @param category Salience category
 * @return Human-readable string
 */
const char* hypo_salience_category_string(hypo_salience_category_t category);

/**
 * @brief Map drive type to primary salience category
 *
 * @param drive Drive type
 * @return Primary salience category for this drive
 */
hypo_salience_category_t hypo_drive_to_salience_category(hypo_drive_type_t drive);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HYPOTHALAMUS_ATTENTION_BRIDGE_H */
