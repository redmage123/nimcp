/**
 * @file nimcp_pfc_adapter.h
 * @brief Prefrontal Cortex Adapter for Executive Layer
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: Adapts PFC for Executive layer working memory and control
 * WHY:  PFC mediates working memory, cognitive control, and planning
 * HOW:  Implements gated working memory, rule representation, inhibitory control
 *
 * PFC MODEL:
 * - dlPFC: Working memory maintenance and manipulation
 * - vlPFC: Rule representation and inhibitory control
 * - ACC: Conflict monitoring and effort allocation
 * - OFC: Value-based decision making (simplified)
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_PFC_ADAPTER_H
#define NIMCP_PFC_ADAPTER_H

#include "integration/core/nimcp_layer_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct nimcp_pfc_adapter_struct* nimcp_pfc_adapter_t;

/**
 * @brief Working memory slot
 */
typedef struct {
    float content[64];          /**< Memory content (embedding) */
    uint32_t content_size;
    float activation;           /**< Current activation level */
    float decay_timer;          /**< Time since last refresh */
    bool is_occupied;
} pfc_wm_slot_t;

/**
 * @brief Rule representation
 */
typedef struct {
    uint32_t rule_id;
    float condition[32];        /**< Condition pattern */
    float action[32];           /**< Action pattern */
    float strength;             /**< Rule strength/confidence */
    bool is_active;
} pfc_rule_t;

typedef struct {
    uint32_t wm_slots;                  /**< Working memory capacity */
    uint32_t max_rules;                 /**< Maximum rule representations */
    float gate_threshold;               /**< WM gating threshold */
    float decay_rate;                   /**< WM decay rate */
    float conflict_threshold;           /**< ACC conflict detection */
    float inhibition_strength;          /**< vlPFC inhibitory control */
    float dopamine_sensitivity;         /**< DA modulation of gating */
    bool enable_acc;                    /**< Enable conflict monitoring */
    bool enable_logging;
} nimcp_pfc_config_t;

typedef struct {
    uint32_t occupied_slots;            /**< Currently occupied WM slots */
    float mean_wm_activation;           /**< Mean WM activation */
    float conflict_level;               /**< ACC conflict signal */
    float cognitive_load;               /**< Overall cognitive load */
    float inhibitory_control;           /**< Current inhibitory state */
    uint32_t active_rules;              /**< Currently active rules */
    bool is_active;
} nimcp_pfc_state_t;

typedef struct {
    uint64_t updates_processed;
    uint64_t messages_handled;
    uint64_t wm_stores;
    uint64_t wm_retrievals;
    uint64_t gate_opens;
    uint64_t gate_closes;
    uint64_t conflicts_detected;
    uint64_t inhibitions_applied;
} nimcp_pfc_stats_t;

NIMCP_EXPORT nimcp_pfc_config_t nimcp_pfc_adapter_default_config(void);
NIMCP_EXPORT nimcp_pfc_adapter_t nimcp_pfc_adapter_create(const nimcp_pfc_config_t* config);
NIMCP_EXPORT void nimcp_pfc_adapter_destroy(nimcp_pfc_adapter_t adapter);
NIMCP_EXPORT nimcp_module_interface_t* nimcp_pfc_adapter_adapter_get_interface(nimcp_pfc_adapter_t adapter);

/**
 * @brief Store content in working memory (gated)
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_pfc_adapter_wm_store(
    nimcp_pfc_adapter_t adapter,
    const float* content,
    uint32_t content_size,
    float priority,
    uint32_t* slot_out
);

/**
 * @brief Retrieve content from working memory slot
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_pfc_adapter_wm_retrieve(
    nimcp_pfc_adapter_t adapter,
    uint32_t slot,
    float* content_out,
    uint32_t max_size
);

/**
 * @brief Refresh working memory slot (prevent decay)
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_pfc_adapter_wm_refresh(
    nimcp_pfc_adapter_t adapter,
    uint32_t slot
);

/**
 * @brief Clear working memory slot
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_pfc_adapter_wm_clear(
    nimcp_pfc_adapter_t adapter,
    uint32_t slot
);

/**
 * @brief Apply dopamine modulation to gating
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_pfc_adapter_modulate_gating(
    nimcp_pfc_adapter_t adapter,
    float dopamine_level
);

/**
 * @brief Get current conflict level (ACC)
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_pfc_adapter_get_conflict(
    nimcp_pfc_adapter_t adapter,
    float* conflict_out
);

/**
 * @brief Apply inhibitory control (vlPFC)
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_pfc_adapter_apply_inhibition(
    nimcp_pfc_adapter_t adapter,
    float inhibition_strength
);

NIMCP_EXPORT nimcp_layer_error_t nimcp_pfc_adapter_get_state(nimcp_pfc_adapter_t adapter, nimcp_pfc_state_t* state_out);
NIMCP_EXPORT nimcp_layer_error_t nimcp_pfc_adapter_get_stats(nimcp_pfc_adapter_t adapter, nimcp_pfc_stats_t* stats_out);
NIMCP_EXPORT nimcp_layer_error_t nimcp_pfc_adapter_reset_stats(nimcp_pfc_adapter_t adapter);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PFC_ADAPTER_H */
