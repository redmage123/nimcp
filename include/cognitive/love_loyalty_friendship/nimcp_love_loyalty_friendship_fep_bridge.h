/**
 * @file nimcp_love_loyalty_friendship_fep_bridge.h
 * @brief Free Energy Principle - Social Bonding Integration Bridge
 * @version 1.0.0
 * @date 2025-12-15
 *
 * WHAT: Bidirectional integration between Free Energy Principle and social bonding system
 * WHY:  Social bonds minimize free energy through predictable relationships;
 *       FEP prediction errors drive attachment dynamics and trust updates.
 * HOW:  FEP uncertainty modulates social exploration; relationship predictability
 *       reduces free energy; attachment violations trigger belief updates.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * SOCIAL BONDING AS FREE ENERGY MINIMIZATION:
 * -------------------------------------------
 * - Secure attachments minimize social prediction errors
 * - Trust = high precision in social predictions
 * - Reference: Fotopoulou & Tsakiris (2017) "Mentalizing homeostasis"
 *
 * FEP → SOCIAL BONDING PATHWAYS:
 * ------------------------------
 * 1. High Prediction Error → Attachment Anxiety:
 *    - Social PE triggers insecure attachment responses
 *    - Unexpected behaviors → trust updates
 *    - High PE → increased vigilance
 *
 * 2. Precision Weights Social Attention:
 *    - High precision relationships → priority
 *    - Low precision → exploratory bonding
 *    - Oxytocin modulates social precision
 *
 * 3. Surprise-Driven Relationship Updates:
 *    - Novel social patterns → relationship revision
 *    - FEP surprise → trust recalibration
 *
 * SOCIAL BONDING → FEP PATHWAYS:
 * -------------------------------
 * 1. Attachment Security Reduces Free Energy:
 *    - Secure bonds → predictable social world
 *    - Low attachment anxiety → low F
 *    - Trust → high precision priors
 *
 * 2. Relationship Closeness Informs Beliefs:
 *    - Close relationships → strong priors
 *    - Distant relationships → weak priors
 *    - Loyalty → commitment to beliefs
 *
 * 3. Social Violations Update Model:
 *    - Betrayal → belief revision
 *    - Loyalty tests → precision updates
 *    - Love intensity → belief strength
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_LOVE_LOYALTY_FRIENDSHIP_FEP_BRIDGE_H
#define NIMCP_LOVE_LOYALTY_FRIENDSHIP_FEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

#include "cognitive/free_energy/nimcp_free_energy.h"
#include "cognitive/nimcp_love_loyalty_friendship.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define SOCIAL_FEP_HIGH_PE_THRESHOLD        4.0f    /**< PE threshold for attachment anxiety */
#define SOCIAL_FEP_TRUST_PRECISION_FACTOR   2.5f    /**< Precision boost from trust */
#define SOCIAL_FEP_LOYALTY_PRIOR_STRENGTH   0.7f    /**< Loyalty commitment prior */
#define SOCIAL_FEP_MAX_RELATIONSHIPS        32      /**< Maximum tracked relationships */

/* ============================================================================
 * Type Definitions
 * ============================================================================ */

typedef struct social_bond_fep_bridge social_bond_fep_bridge_t;

/**
 * @brief Configuration for social bonding-FEP bridge
 */
typedef struct {
    float pe_anxiety_threshold;         /**< PE threshold for attachment anxiety */
    float trust_precision_factor;       /**< Precision boost from trust */
    float loyalty_prior_strength;       /**< Loyalty commitment weight */
    bool enable_pe_attachment;          /**< Enable PE-driven attachment dynamics */
    bool enable_trust_precision;        /**< Enable trust-based precision */
    bool enable_relationship_priors;    /**< Enable relationship-based priors */
    float attachment_sensitivity;       /**< Sensitivity to attachment changes */
    float closeness_belief_strength;    /**< Closeness influence on beliefs */
    bool enable_closeness_beliefs;      /**< Enable closeness-constrained beliefs */
    bool enable_betrayal_updates;       /**< Enable betrayal belief updates */
    float fe_sensitivity;               /**< FEP sensitivity */
    float social_sensitivity;           /**< Social bonding sensitivity */
} social_bond_fep_config_t;

/**
 * @brief FEP effects on social bonding
 */
typedef struct {
    float current_prediction_error;     /**< Current PE magnitude */
    bool attachment_anxiety_triggered;  /**< Anxiety response triggered */
    float relationship_precision[SOCIAL_FEP_MAX_RELATIONSHIPS]; /**< Precision per relationship */
    uint32_t num_relationships_tracked; /**< Count of tracked relationships */
    float current_surprise;             /**< Current surprise level */
    bool trust_update_active;           /**< Trust recalibration active */
} social_bond_fep_effects_t;

/**
 * @brief Social bonding effects on FEP
 */
typedef struct {
    float attachment_security_bias;     /**< Attachment-based FE reduction */
    bool trust_constraining_model;      /**< Trust constraining generative model */
    float closeness_prior_strength;     /**< Closeness-based prior */
    float loyalty_commitment_level;     /**< Loyalty commitment to beliefs */
    float love_intensity_factor;        /**< Love intensity influence */
    bool model_beliefs_updated;         /**< FEP beliefs updated from social bonds */
} fep_social_bond_effects_t;

/**
 * @brief Bridge state
 */
typedef struct {
    float current_prediction_error;     /**< Current PE */
    float current_attachment_security;  /**< Mean attachment security */
    float current_trust_mean;           /**< Mean trust across relationships */
    bool attachment_anxiety_active;     /**< Anxiety response active */
    uint32_t num_close_relationships;   /**< Count of close bonds */
    uint64_t last_anxiety_time;         /**< Last anxiety trigger time */
    uint64_t last_trust_update_time;    /**< Last trust update time */
} social_bond_fep_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t attachment_anxiety_events; /**< Anxiety trigger events */
    uint64_t trust_updates;             /**< Trust update events */
    uint64_t relationship_revisions;    /**< Relationship revision events */
    float avg_prediction_error;         /**< Average PE */
    float avg_attachment_security;      /**< Average attachment security */
    uint64_t precision_applications;    /**< Trust precision applications */
    uint64_t belief_updates;            /**< FEP belief updates */
    float avg_free_energy;              /**< Average free energy */
} social_bond_fep_stats_t;

/**
 * @brief Complete bridge structure
 */
struct social_bond_fep_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    social_bond_fep_config_t config;
    fep_system_t* fep_system;
    social_bond_system_t* social_system;
    social_bond_fep_effects_t fep_effects;
    fep_social_bond_effects_t social_effects;
    social_bond_fep_state_t state;
    social_bond_fep_stats_t stats;
};

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int social_bond_fep_bridge_default_config(social_bond_fep_config_t* config);
social_bond_fep_bridge_t* social_bond_fep_bridge_create(const social_bond_fep_config_t* config);
void social_bond_fep_bridge_destroy(social_bond_fep_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

int social_bond_fep_bridge_connect_fep(social_bond_fep_bridge_t* bridge, fep_system_t* fep);
int social_bond_fep_bridge_connect_social(social_bond_fep_bridge_t* bridge, social_bond_system_t* social);
int social_bond_fep_bridge_disconnect(social_bond_fep_bridge_t* bridge);

/* ============================================================================
 * FEP → Social Bonding API
 * ============================================================================ */

int social_bond_fep_trigger_attachment_anxiety(social_bond_fep_bridge_t* bridge, float pe_magnitude);
int social_bond_fep_modulate_trust_by_precision(social_bond_fep_bridge_t* bridge);
int social_bond_fep_trigger_relationship_revision(social_bond_fep_bridge_t* bridge);

/* ============================================================================
 * Social Bonding → FEP API
 * ============================================================================ */

int social_bond_fep_apply_attachment_priors(social_bond_fep_bridge_t* bridge);
int social_bond_fep_apply_closeness_beliefs(social_bond_fep_bridge_t* bridge);
int social_bond_fep_update_model_from_betrayal(social_bond_fep_bridge_t* bridge);

/* ============================================================================
 * Update and Query API
 * ============================================================================ */

int social_bond_fep_bridge_update(social_bond_fep_bridge_t* bridge, uint64_t delta_ms);
int social_bond_fep_bridge_get_state(social_bond_fep_bridge_t* bridge, social_bond_fep_state_t* state);
int social_bond_fep_bridge_get_stats(social_bond_fep_bridge_t* bridge, social_bond_fep_stats_t* stats);

/* ============================================================================
 * Bio-async Integration API
 * ============================================================================ */

int social_bond_fep_bridge_connect_bio_async(social_bond_fep_bridge_t* bridge);
int social_bond_fep_bridge_disconnect_bio_async(social_bond_fep_bridge_t* bridge);
bool social_bond_fep_bridge_is_bio_async_connected(const social_bond_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LOVE_LOYALTY_FRIENDSHIP_FEP_BRIDGE_H */
