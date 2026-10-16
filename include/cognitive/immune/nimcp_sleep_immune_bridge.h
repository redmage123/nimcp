/**
 * @file nimcp_sleep_immune_bridge.h
 * @brief Sleep-Immune System Integration Bridge
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Bidirectional integration between brain immune system and sleep-wake cycle
 * WHY:  Biological evidence shows strong immune-sleep coupling (cytokines induce sleep,
 *       sleep consolidates immune memory). Essential for realistic brain modeling.
 * HOW:  Cytokines modulate sleep pressure and quality, sleep stages enhance immune
 *       function, sleep deprivation suppresses immunity.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * IMMUNE → SLEEP PATHWAYS:
 * -------------------------
 * 1. Pro-inflammatory Cytokines (IL-1β, TNF-α):
 *    - Promote NREM sleep (slow-wave sleep)
 *    - Increase sleep pressure (adenosine-like effect)
 *    - Induce "sickness behavior" sleep during infection
 *    - Reference: Opp (2009) "Cytokines and sleep"
 *
 * 2. IL-10 (Anti-inflammatory):
 *    - Promotes restorative sleep quality
 *    - Reduces sleep fragmentation
 *    - Facilitates recovery during sleep
 *    - Reference: Imeri & Opp (2009) "How and why the immune system makes us sleep"
 *
 * 3. Inflammation Disrupts Sleep:
 *    - Chronic inflammation → sleep fragmentation
 *    - Cytokine storm → excessive sleep (sickness behavior)
 *    - Systemic inflammation → poor sleep quality
 *    - Reference: Motivala (2011) "Sleep and inflammation"
 *
 * SLEEP → IMMUNE PATHWAYS:
 * -------------------------
 * 1. Deep NREM Sleep:
 *    - Enhances T cell activity and proliferation
 *    - Promotes antibody production
 *    - Strengthens immunological memory
 *    - Reference: Besedovsky et al. (2012) "Sleep and immune function"
 *
 * 2. REM Sleep:
 *    - Consolidates immune memory (like episodic memory)
 *    - Reorganizes immune response patterns
 *    - Optimizes threat recognition
 *    - Reference: Preston et al. (2019) "Interplay of hippocampus and sleep"
 *
 * 3. Sleep Deprivation:
 *    - Suppresses T cell function
 *    - Reduces antibody production
 *    - Impairs immune memory formation
 *    - Increases inflammation (pro-inflammatory shift)
 *    - Reference: Irwin (2015) "Why sleep is important for health"
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                    SLEEP-IMMUNE BRIDGE                                     ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  IMMUNE → SLEEP PATHWAYS                            │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │  CYTOKINES   │                                                 │  ║
 * ║   │   │ ──────────── │                                                 │  ║
 * ║   │   │ IL-1β → +0.3 │  ───────┐                                       │  ║
 * ║   │   │ TNF-α → +0.4 │         │                                       │  ║
 * ║   │   │ IL-6  → +0.2 │         ├──→ Increase Sleep Pressure            │  ║
 * ║   │   │              │         │    (Adenosine-like effect)            │  ║
 * ║   │   └──────────────┘         │                                       │  ║
 * ║   │                            ▼                                       │  ║
 * ║   │   ┌─────────────────────────────────┐                             │  ║
 * ║   │   │     SLEEP SYSTEM                │                             │  ║
 * ║   │   │  - Sleep pressure modulation    │                             │  ║
 * ║   │   │  - Sleep quality changes        │                             │  ║
 * ║   │   │  - Stage transitions            │                             │  ║
 * ║   │   │  - Fragmentation (inflammation) │                             │  ║
 * ║   │   └─────────────────────────────────┘                             │  ║
 * ║   │                            ▲                                       │  ║
 * ║   │   ┌──────────────┐         │                                       │  ║
 * ║   │   │   IL-10      │         │                                       │  ║
 * ║   │   │ Anti-inflam  │  ───────┘                                       │  ║
 * ║   │   │   Restorative│     Better Sleep Quality                        │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  SLEEP → IMMUNE PATHWAYS                            │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │  DEEP NREM   │ ──→ Enhance T Cell Activity                     │  ║
 * ║   │   │  (SWS)       │ ──→ Boost Antibody Production                   │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │  REM SLEEP   │ ──→ Consolidate Immune Memory                   │  ║
 * ║   │   │              │ ──→ Optimize Threat Recognition                 │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │ SLEEP DEPRI- │ ──→ Suppress T Cell Function                    │  ║
 * ║   │   │   VATION     │ ──→ Reduce Antibody Production                  │  ║
 * ║   │   │              │ ──→ Increase Inflammation                       │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe via mutex
 * - nimcp_malloc/nimcp_free memory management
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_SLEEP_IMMUNE_BRIDGE_H
#define NIMCP_SLEEP_IMMUNE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "cognitive/immune/nimcp_brain_immune.h"
#include "cognitive/nimcp_sleep_wake.h"

/* Bio-async integration */
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Cytokine sleep pressure impact factors */
#define CYTOKINE_IL1_SLEEP_PRESSURE      0.3f    /**< IL-1β → increase sleep pressure */
#define CYTOKINE_TNF_SLEEP_PRESSURE      0.4f    /**< TNF-α → strong sleep induction */
#define CYTOKINE_IL6_SLEEP_PRESSURE      0.2f    /**< IL-6 → mild sleep pressure */
#define CYTOKINE_IL10_SLEEP_QUALITY      0.3f    /**< IL-10 → restorative sleep */

/* Inflammation sleep disruption */
#define INFLAMMATION_FRAGMENTATION_THRESHOLD  0.6f   /**< Inflammation level for sleep fragmentation */
#define INFLAMMATION_STORM_SLEEP_MULTIPLIER   2.0f   /**< Cytokine storm increases sleep need */

/* Sleep deprivation immune impact */
#define SLEEP_DEPRIVATION_HOURS          24.0f   /**< Hours awake for deprivation threshold */
#define SLEEP_DEPRIVATION_IMMUNE_PENALTY 0.5f    /**< Immune function reduction (0.5 = 50%) */

/* Deep sleep immune enhancement */
#define DEEP_SLEEP_T_CELL_BOOST          0.3f    /**< T cell activity increase during SWS */
#define DEEP_SLEEP_ANTIBODY_BOOST        0.25f   /**< Antibody production increase */
#define REM_SLEEP_MEMORY_BOOST           0.4f    /**< Immune memory consolidation in REM */

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Cytokine sleep effects
 *
 * Represents how cytokine levels modulate sleep pressure and quality
 */
typedef struct {
    /* Pro-inflammatory sleep effects */
    float il1_sleep_pressure;         /**< IL-1β induced sleep pressure */
    float tnf_sleep_pressure;         /**< TNF-α induced sleep pressure */
    float il6_sleep_pressure;         /**< IL-6 induced sleep pressure */

    /* Anti-inflammatory sleep effects */
    float il10_sleep_quality;         /**< IL-10 improved sleep quality */

    /* Aggregate effects */
    float total_sleep_pressure_bonus; /**< Combined sleep pressure increase */
    float sleep_quality_modifier;     /**< Sleep quality [0-1], 1=optimal */
    float sickness_sleep_drive;       /**< Sickness behavior sleep drive [0-1] */
    float fragmentation_level;        /**< Sleep fragmentation from inflammation [0-1] */
} cytokine_sleep_effects_t;

/**
 * @brief Inflammation sleep disruption state
 *
 * How inflammation affects sleep architecture and quality
 */
typedef struct {
    /* Inflammation state */
    brain_inflammation_level_t current_level;
    float inflammation_duration_sec;   /**< How long inflamed */
    bool is_chronic;                   /**< Chronic inflammation */

    /* Sleep disruption */
    float fragmentation_severity;      /**< Sleep fragmentation [0-1] */
    float quality_impairment;          /**< Sleep quality reduction [0-1] */
    float rem_suppression;             /**< REM sleep reduction [0-1] */
    float deep_sleep_reduction;        /**< SWS reduction [0-1] */

    /* Sickness sleep */
    float sickness_sleep_multiplier;   /**< Increased sleep need during infection */
} inflammation_sleep_state_t;

/**
 * @brief Sleep stage immune modulation
 *
 * How different sleep stages affect immune function
 */
typedef struct {
    /* Current sleep state */
    sleep_state_t current_state;
    uint64_t state_duration_ms;        /**< Time in current state */

    /* Immune enhancement during sleep */
    float t_cell_activity_multiplier;  /**< T cell enhancement [1.0-1.5] */
    float antibody_production_boost;   /**< Antibody production boost [0-0.3] */
    float memory_consolidation_rate;   /**< Memory consolidation [0-1] */

    /* Deep sleep effects */
    bool in_deep_sleep;                /**< Currently in deep NREM */
    uint64_t total_deep_sleep_ms;      /**< Cumulative deep sleep this cycle */

    /* REM effects */
    bool in_rem_sleep;                 /**< Currently in REM */
    uint64_t total_rem_ms;             /**< Cumulative REM this cycle */
} sleep_immune_modulation_t;

/**
 * @brief Sleep deprivation immune suppression
 *
 * How lack of sleep impairs immune function
 */
typedef struct {
    /* Sleep debt */
    uint64_t time_awake_ms;            /**< Time since last sleep */
    float sleep_debt_hours;            /**< Accumulated sleep debt */
    bool is_sleep_deprived;            /**< >= 24 hours awake */

    /* Immune suppression */
    float t_cell_suppression;          /**< T cell impairment [0-1] */
    float antibody_suppression;        /**< Antibody production impairment [0-1] */
    float memory_formation_impairment; /**< Memory formation reduction [0-1] */

    /* Inflammatory effects */
    float pro_inflammatory_shift;      /**< Increased inflammation [0-1] */
    float immune_dysregulation;        /**< Overall immune dysfunction [0-1] */
} sleep_deprivation_state_t;

/**
 * @brief Complete sleep-immune bridge state
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    /* System handles */
    brain_immune_system_t* immune_system;
    sleep_system_t sleep_system;

    /* Current state */
    cytokine_sleep_effects_t cytokine_effects;
    inflammation_sleep_state_t inflammation_state;
    sleep_immune_modulation_t sleep_modulation;
    sleep_deprivation_state_t deprivation_state;

    /* Integration flags */
    bool enable_cytokine_sleep_modulation;
    bool enable_inflammation_sleep_disruption;
    bool enable_sleep_immune_enhancement;
    bool enable_sleep_deprivation_tracking;
    bool enable_rem_memory_consolidation;

    /* Statistics */
    uint64_t total_updates;
    uint32_t cytokine_modulations;
    uint32_t sleep_enhanced_immune_events;
    uint32_t deprivation_suppression_events;
    uint32_t memory_consolidations;
    } sleep_immune_bridge_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Feature enables */
    bool enable_cytokine_sleep_modulation;
    bool enable_inflammation_sleep_disruption;
    bool enable_sleep_immune_enhancement;
    bool enable_sleep_deprivation_tracking;
    bool enable_rem_memory_consolidation;

    /* Sensitivity tuning */
    float cytokine_sensitivity;        /**< Cytokine effect multiplier [0.5-2.0] */
    float inflammation_sensitivity;    /**< Inflammation effect multiplier [0.5-2.0] */
    float sleep_enhancement_factor;    /**< Sleep immune boost multiplier [0.5-2.0] */

    /* Thresholds */
    float sleep_deprivation_hours;     /**< Hours awake for deprivation [18-30] */
    float inflammation_fragmentation_threshold; /**< Inflammation for sleep disruption [0.4-0.8] */
} sleep_immune_config_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with biological defaults
 * HOW:  Return struct with evidence-based parameters
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int sleep_immune_default_config(sleep_immune_config_t* config);

/**
 * @brief Create sleep-immune bridge
 *
 * WHAT: Initialize bidirectional sleep-immune integration
 * WHY:  Enable realistic sleep-immune coupling
 * HOW:  Allocate structure, link subsystems
 *
 * @param config Configuration (NULL for defaults)
 * @param immune_system Brain immune system
 * @param sleep_system Sleep-wake system
 * @return New bridge or NULL on failure
 */
sleep_immune_bridge_t* sleep_immune_bridge_create(
    const sleep_immune_config_t* config,
    brain_immune_system_t* immune_system,
    sleep_system_t sleep_system
);

/**
 * @brief Destroy sleep-immune bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Free structure (doesn't destroy linked systems)
 *
 * @param bridge Bridge to destroy
 */
void sleep_immune_bridge_destroy(sleep_immune_bridge_t* bridge);

/* ============================================================================
 * Immune → Sleep API
 * ============================================================================ */

/**
 * @brief Apply cytokine effects to sleep system
 *
 * WHAT: Modulate sleep pressure and quality based on cytokine levels
 * WHY:  Pro-inflammatory cytokines induce sleep, IL-10 improves quality
 * HOW:  Query immune system cytokines, adjust sleep pressure and quality
 *
 * @param bridge Sleep-immune bridge
 * @return 0 on success
 */
int sleep_immune_apply_cytokine_effects(sleep_immune_bridge_t* bridge);

/**
 * @brief Apply inflammation to sleep architecture
 *
 * WHAT: Disrupt sleep quality and fragmentation from inflammation
 * WHY:  Chronic inflammation causes sleep fragmentation and poor quality
 * HOW:  Check inflammation level/duration, reduce sleep quality, fragment stages
 *
 * @param bridge Sleep-immune bridge
 * @return 0 on success
 */
int sleep_immune_apply_inflammation_effects(sleep_immune_bridge_t* bridge);

/**
 * @brief Compute sleep pressure bonus from cytokines
 *
 * WHAT: Calculate sleep pressure increase from immune state
 * WHY:  Cytokines increase sleep need (sickness behavior)
 * HOW:  Map cytokine levels to sleep pressure increase [0-1]
 *
 * @param bridge Sleep-immune bridge
 * @return Sleep pressure bonus [0-1]
 */
float sleep_immune_compute_cytokine_sleep_pressure(const sleep_immune_bridge_t* bridge);

/**
 * @brief Check if inflammation is disrupting sleep
 *
 * WHAT: Determine if inflammation level causes sleep fragmentation
 * WHY:  High inflammation disrupts sleep architecture
 * HOW:  Check if inflammation exceeds fragmentation threshold
 *
 * @param bridge Sleep-immune bridge
 * @return true if sleep is fragmented by inflammation
 */
bool sleep_immune_is_sleep_fragmented(const sleep_immune_bridge_t* bridge);

/* ============================================================================
 * Sleep → Immune API
 * ============================================================================ */

/**
 * @brief Enhance immune function during deep sleep
 *
 * WHAT: Boost T cell activity and antibody production during deep NREM
 * WHY:  Deep sleep strengthens immune function
 * HOW:  Query sleep state, if deep NREM, boost immune cell activity
 *
 * @param bridge Sleep-immune bridge
 * @return 0 on success
 */
int sleep_immune_enhance_during_deep_sleep(sleep_immune_bridge_t* bridge);

/**
 * @brief Consolidate immune memory during REM sleep
 *
 * WHAT: Strengthen immune memory cells during REM sleep
 * WHY:  REM sleep consolidates immune memory like episodic memory
 * HOW:  Query sleep state, if REM, enhance memory B cell consolidation
 *
 * @param bridge Sleep-immune bridge
 * @return 0 on success
 */
int sleep_immune_consolidate_memory_during_rem(sleep_immune_bridge_t* bridge);

/**
 * @brief Suppress immune function from sleep deprivation
 *
 * WHAT: Reduce T cell activity and antibody production when sleep deprived
 * WHY:  Sleep deprivation impairs immune function
 * HOW:  Track time awake, apply suppression if > threshold
 *
 * @param bridge Sleep-immune bridge
 * @return 0 on success
 */
int sleep_immune_suppress_from_deprivation(sleep_immune_bridge_t* bridge);

/**
 * @brief Increase inflammation from chronic sleep loss
 *
 * WHAT: Trigger pro-inflammatory response from sustained sleep deprivation
 * WHY:  Chronic sleep loss increases inflammatory markers
 * HOW:  Check sleep debt, release pro-inflammatory cytokines if chronic
 *
 * @param bridge Sleep-immune bridge
 * @return 0 on success
 */
int sleep_immune_inflame_from_chronic_loss(sleep_immune_bridge_t* bridge);

/* ============================================================================
 * Bidirectional Update API
 * ============================================================================ */

/**
 * @brief Update sleep-immune bridge (both directions)
 *
 * WHAT: Process all sleep-immune interactions
 * WHY:  Advance coupled state machine
 * HOW:  Apply cytokine effects on sleep, enhance immune during sleep stages
 *
 * @param bridge Sleep-immune bridge
 * @param delta_ms Time since last update
 * @return 0 on success
 */
int sleep_immune_bridge_update(
    sleep_immune_bridge_t* bridge,
    uint64_t delta_ms
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current cytokine sleep effects
 *
 * @param bridge Sleep-immune bridge
 * @param effects Output effects structure
 * @return 0 on success
 */
int sleep_immune_get_cytokine_effects(
    const sleep_immune_bridge_t* bridge,
    cytokine_sleep_effects_t* effects
);

/**
 * @brief Get current inflammation sleep state
 *
 * @param bridge Sleep-immune bridge
 * @param state Output state structure
 * @return 0 on success
 */
int sleep_immune_get_inflammation_state(
    const sleep_immune_bridge_t* bridge,
    inflammation_sleep_state_t* state
);

/**
 * @brief Get current sleep immune modulation
 *
 * @param bridge Sleep-immune bridge
 * @param modulation Output modulation structure
 * @return 0 on success
 */
int sleep_immune_get_sleep_modulation(
    const sleep_immune_bridge_t* bridge,
    sleep_immune_modulation_t* modulation
);

/**
 * @brief Get current sleep deprivation state
 *
 * @param bridge Sleep-immune bridge
 * @param state Output state structure
 * @return 0 on success
 */
int sleep_immune_get_deprivation_state(
    const sleep_immune_bridge_t* bridge,
    sleep_deprivation_state_t* state
);

/**
 * @brief Check if experiencing sickness sleep
 *
 * WHAT: Determine if cytokines inducing sickness sleep behavior
 * WHY:  Sickness sleep is distinct sleep drive from infection
 * HOW:  Check cytokine levels and sickness sleep drive score
 *
 * @param bridge Sleep-immune bridge
 * @return true if experiencing sickness sleep drive
 */
bool sleep_immune_is_sickness_sleep(const sleep_immune_bridge_t* bridge);

/**
 * @brief Get sleep quality impairment from inflammation
 *
 * @param bridge Sleep-immune bridge
 * @return Quality impairment [0-1], 0=no impairment
 */
float sleep_immune_get_quality_impairment(const sleep_immune_bridge_t* bridge);

/**
 * @brief Check if sleep deprived
 *
 * @param bridge Sleep-immune bridge
 * @return true if sleep deprived (>= threshold hours awake)
 */
bool sleep_immune_is_sleep_deprived(const sleep_immune_bridge_t* bridge);

/**
 * @brief Get immune suppression level from sleep deprivation
 *
 * @param bridge Sleep-immune bridge
 * @return Suppression level [0-1], 0=no suppression
 */
float sleep_immune_get_suppression_level(const sleep_immune_bridge_t* bridge);


/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

/**
 * @brief Connect bridge to bio-async router
 *
 * WHAT: Register bridge as bio-async module
 * WHY:  Enable inter-module messaging for distributed immune signals
 * HOW:  Register with bio_router using BIO_MODULE_IMMUNE_SLEEP
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int sleep_immune_connect_bio_async(sleep_immune_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * WHAT: Unregister bridge from bio-async
 * WHY:  Clean shutdown of messaging
 * HOW:  Unregister from bio_router
 *
 * @param bridge Bridge instance
 * @return 0 on success
 */
int sleep_immune_disconnect_bio_async(sleep_immune_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Bridge instance
 * @return true if connected
 */
bool sleep_immune_is_bio_async_connected(const sleep_immune_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SLEEP_IMMUNE_BRIDGE_H */
