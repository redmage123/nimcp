/**
 * @file nimcp_basal_ganglia_amygdala_bridge.h
 * @brief Basal ganglia-amygdala emotional modulation bridge
 *
 * WHAT: Integration bridge for emotional influence on action selection
 * WHY:  Fear/threat signals from amygdala modulate BG action selection
 * HOW:  Amygdala fear → BG avoidance bias, anxiety → cautious behavior
 *
 * BIOLOGICAL BASIS:
 * - Amygdala projects to striatum (especially ventral striatum/NAc)
 * - Fear responses bias action selection toward avoidance/freezing
 * - Anxiety increases STN activity (hyperdirect pathway) → behavioral inhibition
 * - Threat detection can override goal-directed behavior with defensive actions
 * - Dopamine from VTA/SNc interacts with amygdala fear signals
 *
 * INTEGRATION PATHWAYS:
 * 1. Amygdala CeA → Striatum: Emotional valence affects action values
 * 2. Amygdala BA → STN: Threat triggers hyperdirect pathway activation
 * 3. BG → Amygdala: Action outcomes feed back to update threat assessment
 *
 * @author NIMCP Development Team
 * @date 2025-12-30
 */

#ifndef NIMCP_BASAL_GANGLIA_AMYGDALA_BRIDGE_H
#define NIMCP_BASAL_GANGLIA_AMYGDALA_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include "core/brain/subcortical/nimcp_basal_ganglia.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "core/brain/subcortical/nimcp_amygdala.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define BGA_MAX_THREAT_ACTIONS    16    /**< Actions marked as threatening */
#define BGA_MAX_SAFE_ACTIONS      16    /**< Actions marked as safe */
#define BGA_DEFAULT_FEAR_WEIGHT   0.5f  /**< Default fear influence weight */
#define BGA_DEFAULT_ANXIETY_WEIGHT 0.3f /**< Default anxiety influence weight */
#define BGA_THREAT_BIAS_MAX       1.0f  /**< Maximum threat-induced bias */
#define BGA_FREEZE_THRESHOLD      0.8f  /**< Fear level for freezing */

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Emotional influence type
 */
typedef enum {
    BGA_INFLUENCE_NONE = 0,       /**< No emotional influence */
    BGA_INFLUENCE_AVOIDANCE,      /**< Bias toward avoidance actions */
    BGA_INFLUENCE_APPROACH,       /**< Bias toward approach (reward) */
    BGA_INFLUENCE_FREEZE,         /**< Suppress all actions (freeze) */
    BGA_INFLUENCE_FLIGHT,         /**< Bias toward escape actions */
    BGA_INFLUENCE_FIGHT           /**< Bias toward aggressive actions */
} bga_influence_type_t;

/**
 * @brief Action emotion tag
 */
typedef enum {
    BGA_TAG_NEUTRAL = 0,          /**< Neutral action */
    BGA_TAG_THREATENING,          /**< Action associated with threat */
    BGA_TAG_SAFE,                 /**< Action associated with safety */
    BGA_TAG_ESCAPE,               /**< Escape/avoidance action */
    BGA_TAG_DEFENSIVE             /**< Defensive action */
} bga_action_tag_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Action emotional modulation
 */
typedef struct {
    uint32_t action_id;           /**< Action identifier */
    bga_action_tag_t tag;         /**< Emotional tag */
    float fear_bias;              /**< Fear-induced bias [-1 to 1] */
    float value_modulation;       /**< Value adjustment factor */
    bool is_active;               /**< Modulation is active */
} bga_action_modulation_t;

/**
 * @brief Emotional state snapshot
 */
typedef struct {
    float fear_level;             /**< Current fear [0-1] */
    float anxiety_level;          /**< Current anxiety [0-1] */
    amyg_threat_level_t threat;   /**< Current threat level */
    amyg_valence_t valence;       /**< Emotional valence */
    bga_influence_type_t influence; /**< Resulting influence type */
} bga_emotional_state_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    float fear_weight;            /**< Fear influence on action selection */
    float anxiety_weight;         /**< Anxiety influence on STN */
    float threat_stn_gain;        /**< Threat → STN activation gain */
    float avoidance_bias;         /**< Bias strength for avoidance */
    float freeze_threshold;       /**< Fear threshold for freezing */
    float habituation_rate;       /**< Rate of threat habituation */
    bool enable_freeze_response;  /**< Enable freezing behavior */
    bool enable_flight_bias;      /**< Enable flight/escape bias */
    bool enable_feedback;         /**< Enable BG→amygdala feedback */
} bga_bridge_config_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_modulations;   /**< Total action modulations */
    uint64_t fear_triggered;      /**< Fear-triggered modifications */
    uint64_t freeze_events;       /**< Freeze response activations */
    uint64_t avoidance_biases;    /**< Avoidance bias applications */
    uint64_t feedback_signals;    /**< BG→amygdala feedback signals */
    float avg_fear_influence;     /**< Average fear influence strength */
    float avg_threat_level;       /**< Average threat level */
} bga_bridge_stats_t;

/**
 * @brief BG-Amygdala bridge instance
 */
typedef struct bga_bridge {
    bridge_base_t base;           /**< MUST be first: base bridge infrastructure */

    /* Connected components */
    basal_ganglia_t* bg;          /**< Connected basal ganglia */
    amygdala_t* amygdala;         /**< Connected amygdala */

    /* Action modulations */
    bga_action_modulation_t* modulations; /**< Per-action modulations */
    uint32_t num_actions;         /**< Number of actions */

    /* Emotional state */
    bga_emotional_state_t state;  /**< Current emotional state */

    /* Threat-tagged actions */
    uint32_t threat_actions[BGA_MAX_THREAT_ACTIONS];
    uint32_t num_threat_actions;
    uint32_t safe_actions[BGA_MAX_SAFE_ACTIONS];
    uint32_t num_safe_actions;

    /* STN modulation */
    float stn_boost;              /**< Anxiety-induced STN boost */

    /* Configuration */
    bga_bridge_config_t config;   /**< Configuration */

    /* Statistics */
    bga_bridge_stats_t stats;     /**< Runtime statistics *//**< Mutex for thread safety */
} bga_bridge_t;

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

/**
 * @brief Get default configuration
 * @param config Configuration to initialize
 */
void bga_bridge_default_config(bga_bridge_config_t* config);

/**
 * @brief Create BG-amygdala bridge
 * @param config Configuration (NULL for defaults)
 * @return Bridge instance or NULL on failure
 */
bga_bridge_t* bga_bridge_create(const bga_bridge_config_t* config);

/**
 * @brief Destroy bridge
 * @param bridge Bridge to destroy
 */
void bga_bridge_destroy(bga_bridge_t* bridge);

/**
 * @brief Reset bridge state
 * @param bridge Bridge to reset
 * @return 0 on success, -1 on error
 */
int bga_bridge_reset(bga_bridge_t* bridge);

/* ============================================================================
 * Connection Functions
 * ============================================================================ */

/**
 * @brief Connect basal ganglia to bridge
 * @param bridge Bridge instance
 * @param bg Basal ganglia to connect
 * @return 0 on success, -1 on error
 */
int bga_bridge_connect_bg(bga_bridge_t* bridge, basal_ganglia_t* bg);

/**
 * @brief Connect amygdala to bridge
 * @param bridge Bridge instance
 * @param amygdala Amygdala to connect
 * @return 0 on success, -1 on error
 */
int bga_bridge_connect_amygdala(bga_bridge_t* bridge, amygdala_t* amygdala);

/**
 * @brief Check if fully connected
 * @param bridge Bridge instance
 * @return true if both BG and amygdala connected
 */
bool bga_bridge_is_connected(const bga_bridge_t* bridge);

/* ============================================================================
 * Action Tagging Functions
 * ============================================================================ */

/**
 * @brief Tag action as threatening
 * @param bridge Bridge instance
 * @param action_id Action to tag
 * @return 0 on success, -1 on error
 */
int bga_bridge_tag_threat_action(bga_bridge_t* bridge, uint32_t action_id);

/**
 * @brief Tag action as safe
 * @param bridge Bridge instance
 * @param action_id Action to tag
 * @return 0 on success, -1 on error
 */
int bga_bridge_tag_safe_action(bga_bridge_t* bridge, uint32_t action_id);

/**
 * @brief Tag action as escape/avoidance
 * @param bridge Bridge instance
 * @param action_id Action to tag
 * @return 0 on success, -1 on error
 */
int bga_bridge_tag_escape_action(bga_bridge_t* bridge, uint32_t action_id);

/**
 * @brief Get action tag
 * @param bridge Bridge instance
 * @param action_id Action to query
 * @return Action tag
 */
bga_action_tag_t bga_bridge_get_action_tag(
    const bga_bridge_t* bridge,
    uint32_t action_id
);

/* ============================================================================
 * Modulation Functions
 * ============================================================================ */

/**
 * @brief Update emotional modulation from amygdala
 *
 * Reads amygdala state and computes:
 * 1. Fear influence on action values
 * 2. STN boost from anxiety
 * 3. Freeze/flight bias if thresholds exceeded
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int bga_bridge_update_modulation(bga_bridge_t* bridge);

/**
 * @brief Apply emotional modulation to action values
 *
 * Modifies BG action values based on current emotional state:
 * - Threatening actions get value reduction when fearful
 * - Safe actions get value boost when anxious
 * - Escape actions get value boost when in flight mode
 *
 * @param bridge Bridge instance
 * @param action_values Action values to modify (in-place)
 * @param num_actions Number of actions
 * @return 0 on success, -1 on error
 */
int bga_bridge_apply_modulation(
    bga_bridge_t* bridge,
    float* action_values,
    uint32_t num_actions
);

/**
 * @brief Get modulation for specific action
 * @param bridge Bridge instance
 * @param action_id Action to query
 * @return Modulation factor (< 1 = suppress, > 1 = boost)
 */
float bga_bridge_get_action_modulation(
    const bga_bridge_t* bridge,
    uint32_t action_id
);

/**
 * @brief Check if freeze response active
 * @param bridge Bridge instance
 * @return true if freeze response is active
 */
bool bga_bridge_is_frozen(const bga_bridge_t* bridge);

/**
 * @brief Get current STN boost from anxiety
 * @param bridge Bridge instance
 * @return STN boost [0-1]
 */
float bga_bridge_get_stn_boost(const bga_bridge_t* bridge);

/* ============================================================================
 * Feedback Functions
 * ============================================================================ */

/**
 * @brief Send action outcome to amygdala
 *
 * When BG action completes, inform amygdala:
 * - Successful escape → reduce threat
 * - Failed avoidance → increase fear
 * - Safe action success → habituation
 *
 * @param bridge Bridge instance
 * @param action_id Completed action
 * @param outcome Outcome value [-1 to 1]
 * @param was_threat Whether action was threat-related
 * @return 0 on success, -1 on error
 */
int bga_bridge_send_outcome(
    bga_bridge_t* bridge,
    uint32_t action_id,
    float outcome,
    bool was_threat
);

/* ============================================================================
 * State Query Functions
 * ============================================================================ */

/**
 * @brief Get current emotional state
 * @param bridge Bridge instance
 * @param state Output: emotional state
 * @return 0 on success, -1 on error
 */
int bga_bridge_get_state(
    const bga_bridge_t* bridge,
    bga_emotional_state_t* state
);

/**
 * @brief Get current influence type
 * @param bridge Bridge instance
 * @return Current influence type
 */
bga_influence_type_t bga_bridge_get_influence(const bga_bridge_t* bridge);

/* ============================================================================
 * Statistics Functions
 * ============================================================================ */

/**
 * @brief Get bridge statistics
 * @param bridge Bridge instance
 * @param stats Output: statistics
 * @return 0 on success, -1 on error
 */
int bga_bridge_get_stats(
    const bga_bridge_t* bridge,
    bga_bridge_stats_t* stats
);

/**
 * @brief Reset statistics
 * @param bridge Bridge instance
 */
void bga_bridge_reset_stats(bga_bridge_t* bridge);

/**
 * @brief Get influence type name
 * @param type Influence type
 * @return Type name string
 */
const char* bga_influence_type_name(bga_influence_type_t type);

/**
 * @brief Get action tag name
 * @param tag Action tag
 * @return Tag name string
 */
const char* bga_action_tag_name(bga_action_tag_t tag);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BASAL_GANGLIA_AMYGDALA_BRIDGE_H */
