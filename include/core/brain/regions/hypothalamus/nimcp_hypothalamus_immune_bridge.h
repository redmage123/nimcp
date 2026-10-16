/**
 * @file nimcp_hypothalamus_immune_bridge.h
 * @brief Hypothalamus-Immune System Bidirectional Bridge
 *
 * WHAT: Bidirectional integration between hypothalamus drives and brain immune system
 * WHY:  Implements neuroimmune crosstalk for sickness behavior and HPA axis regulation
 * HOW:  Cytokines modulate drive setpoints; HPA axis modulates immune intensity
 *
 * BIOLOGICAL BASIS:
 * The hypothalamus and immune system have extensive bidirectional communication:
 *
 * IMMUNE → HYPOTHALAMUS:
 * - IL-1β, IL-6 → Fever (temperature setpoint ↑)
 * - TNF-α → Anorexia (hunger drive ↓)
 * - Pro-inflammatory cytokines → Fatigue (fatigue drive ↑)
 * - Cytokine storm → Sickness behavior (social drive ↓, curiosity ↓)
 *
 * HYPOTHALAMUS → IMMUNE:
 * - PVN → CRH → ACTH → Cortisol → Immune suppression
 * - SCN circadian phase → Immune cell trafficking rhythms
 * - Stress response → Acute immune boost, chronic immune suppression
 * - Sleep/wake state → Immune function modulation
 *
 * BYRNES' ALIGNMENT INSIGHT:
 * Sickness behavior is a natural "safety mode" that prioritizes survival over
 * exploration. The alignment system can use similar mechanisms to enter safe
 * modes when detecting potential threats or anomalies.
 *
 * @version Phase 14: Brain Immune Integration
 * @date 2026-01-04
 */

#ifndef NIMCP_HYPOTHALAMUS_IMMUNE_BRIDGE_H
#define NIMCP_HYPOTHALAMUS_IMMUNE_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Include headers BEFORE extern "C" - they have their own C linkage guards
 * This prevents CUDA template headers from being included inside extern "C" */
#include "nimcp_hypothalamus_drives.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "glial/microglia/nimcp_microglia.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declaration of orchestrator type (avoid header conflict) */
typedef struct hypo_orchestrator_struct* hypo_orchestrator_t;

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

/** @brief Module ID for bio-async registration */
#define HYPO_IMMUNE_BRIDGE_MODULE_ID  0x1170

/** @brief Maximum cytokine types tracked */
#define HYPO_IMMUNE_MAX_CYTOKINES  6

/** @brief Maximum HPA axis stages */
#define HYPO_IMMUNE_HPA_STAGES     4

/** @brief Sleep-immune coupling factor */
#define HYPO_IMMUNE_SLEEP_FACTOR   0.12f

/** @brief Acute phase response duration (microseconds) */
#define HYPO_IMMUNE_ACUTE_PHASE_DURATION_US  (6 * 3600 * 1000000ULL)  /* 6 hours */

/** @brief Sickness behavior onset threshold (inflammation) */
#define HYPO_IMMUNE_SICKNESS_ONSET_THRESHOLD  0.25f

/** @brief Fever setpoint increase per unit IL-1β (°C equivalent) */
#define HYPO_IMMUNE_FEVER_FACTOR   0.05f

/** @brief Anorexia factor per unit TNF-α */
#define HYPO_IMMUNE_ANOREXIA_FACTOR  0.15f

/** @brief Fatigue increase per unit pro-inflammatory cytokines */
#define HYPO_IMMUNE_FATIGUE_FACTOR   0.10f

/** @brief Social withdrawal factor during sickness */
#define HYPO_IMMUNE_SOCIAL_WITHDRAW_FACTOR  0.20f

/** @brief Cortisol immune suppression coefficient */
#define HYPO_IMMUNE_CORTISOL_SUPPRESSION  0.25f

/*=============================================================================
 * ENUMERATIONS
 *===========================================================================*/

/**
 * @brief Sickness behavior severity levels
 *
 * BIOLOGICAL BASIS: Graded response to infection severity
 */
typedef enum {
    SICKNESS_NONE = 0,          /**< No sickness behavior */
    SICKNESS_MILD,              /**< Slight fatigue, reduced appetite */
    SICKNESS_MODERATE,          /**< Fever, anorexia, social withdrawal */
    SICKNESS_SEVERE,            /**< Profound fatigue, complete withdrawal */
    SICKNESS_CRITICAL           /**< Cytokine storm, emergency response */
} hypo_sickness_level_t;

/**
 * @brief HPA axis activation state
 *
 * BIOLOGICAL BASIS: Hypothalamic-Pituitary-Adrenal axis stress response
 */
typedef enum {
    HPA_BASELINE = 0,           /**< Normal cortisol levels */
    HPA_ACUTE_STRESS,           /**< Acute stress, elevated cortisol */
    HPA_PROLONGED_STRESS,       /**< Prolonged stress, sustained elevation */
    HPA_CHRONIC_STRESS,         /**< Chronic stress, dysregulated axis */
    HPA_RECOVERY                /**< Recovery phase, returning to baseline */
} hypo_hpa_state_t;

/**
 * @brief Circadian immune phase
 *
 * BIOLOGICAL BASIS: SCN modulates immune cell trafficking and cytokine release
 */
typedef enum {
    CIRCADIAN_IMMUNE_DAY = 0,   /**< Daytime: enhanced adaptive immunity */
    CIRCADIAN_IMMUNE_EVENING,   /**< Evening: transition phase */
    CIRCADIAN_IMMUNE_NIGHT,     /**< Night: enhanced innate immunity */
    CIRCADIAN_IMMUNE_MORNING    /**< Morning: peak inflammation potential */
} hypo_circadian_immune_t;

/**
 * @brief Sleep-immune interaction state
 *
 * BIOLOGICAL BASIS: Sleep profoundly affects immune function:
 * - Sleep deprivation increases pro-inflammatory cytokines
 * - Deep sleep enhances immune cell production
 * - Infection increases sleep drive (sickness behavior)
 */
typedef enum {
    SLEEP_IMMUNE_NORMAL = 0,    /**< Normal sleep-immune balance */
    SLEEP_IMMUNE_DEPRIVED,      /**< Sleep deprivation, elevated inflammation */
    SLEEP_IMMUNE_RESTORATIVE,   /**< Deep sleep, enhanced immune function */
    SLEEP_IMMUNE_SICKNESS       /**< Sickness-induced hypersomnia */
} hypo_sleep_immune_t;

/*=============================================================================
 * DATA STRUCTURES
 *===========================================================================*/

/**
 * @brief Cytokine levels affecting hypothalamus
 */
typedef struct {
    float il1_beta;             /**< IL-1β level [0, 1] - fever, fatigue */
    float il6;                  /**< IL-6 level [0, 1] - fever, acute phase */
    float tnf_alpha;            /**< TNF-α level [0, 1] - anorexia, fatigue */
    float il10;                 /**< IL-10 level [0, 1] - anti-inflammatory */
    float ifn_gamma;            /**< IFN-γ level [0, 1] - antiviral response */
    float tgf_beta;             /**< TGF-β level [0, 1] - resolution */

    uint64_t last_update_us;    /**< Last update timestamp */
} hypo_cytokine_state_t;

/**
 * @brief HPA axis state and output
 */
typedef struct {
    hypo_hpa_state_t state;     /**< Current HPA state */

    /* Hormone levels (normalized 0-1) */
    float crh_level;            /**< CRH from PVN */
    float acth_level;           /**< ACTH from pituitary */
    float cortisol_level;       /**< Cortisol from adrenal */

    /* Stress metrics */
    float acute_stress;         /**< Acute stress level */
    float chronic_stress;       /**< Chronic stress accumulation */

    /* Timing */
    uint64_t stress_onset_us;   /**< When stress started */
    uint64_t last_update_us;    /**< Last update */
} hypo_hpa_axis_t;

/**
 * @brief Sickness behavior state
 */
typedef struct {
    hypo_sickness_level_t level;  /**< Current sickness level */

    /* Behavioral changes */
    float fever_magnitude;      /**< Temperature setpoint increase */
    float anorexia_magnitude;   /**< Appetite suppression */
    float fatigue_magnitude;    /**< Fatigue increase */
    float social_withdrawal;    /**< Social drive suppression */
    float curiosity_reduction;  /**< Exploration reduction */

    /* Inflammation metrics */
    float inflammation_level;   /**< Overall inflammation [0, 1] */
    bool cytokine_storm;        /**< Emergency immune state */

    /* Timing */
    uint64_t onset_us;          /**< When sickness started */
    uint64_t peak_us;           /**< Peak sickness time */
} hypo_sickness_state_t;

/**
 * @brief Drive setpoint modulation from immune signals
 */
typedef struct {
    hypo_drive_type_t drive;    /**< Which drive is modulated */
    float baseline_setpoint;    /**< Original setpoint */
    float modulated_setpoint;   /**< Current modulated setpoint */
    float modulation_factor;    /**< How much cytokines affect this drive */
    bool is_modulated;          /**< Currently under immune modulation */
} hypo_immune_modulation_t;

/**
 * @brief Simplified cytokine tracking for API compatibility
 *
 * BIOLOGICAL BASIS: Key cytokines that mediate hypothalamus-immune crosstalk
 */
typedef struct {
    float il1_beta;             /**< IL-1β level [0, 1] - Primary fever inducer */
    float il6;                  /**< IL-6 level [0, 1] - Acute phase response */
    float tnf_alpha;            /**< TNF-α level [0, 1] - Inflammation amplifier */
    float il10;                 /**< IL-10 level [0, 1] - Anti-inflammatory */
    float ifn_gamma;            /**< IFN-γ level [0, 1] - Immunomodulatory */
    float cortisol;             /**< Cortisol level [0, 1] - HPA feedback */
} hypo_immune_cytokines_t;

/**
 * @brief Unified immune state for queries
 *
 * Provides complete snapshot of hypothalamus-immune integration state
 */
typedef struct {
    hypo_immune_cytokines_t cytokines;  /**< Current cytokine levels */
    float inflammation_level;           /**< Overall inflammation [0, 1] */
    float fever_signal;                 /**< Fever magnitude [0, 1] */
    float sickness_behavior;            /**< Sickness behavior intensity [0, 1] */
    float immune_activation;            /**< Overall immune activation [0, 1] */
    bool acute_phase_response;          /**< Acute phase response active */

    /* Additional state */
    hypo_sickness_level_t sickness_level;  /**< Sickness severity level */
    hypo_hpa_state_t hpa_state;            /**< HPA axis state */
    hypo_circadian_immune_t circadian;     /**< Circadian immune phase */
    hypo_sleep_immune_t sleep_immune;      /**< Sleep-immune interaction */

    /* Timing */
    uint64_t last_update_us;            /**< Last state update timestamp */
} hypo_immune_state_t;

/**
 * @brief Hypothalamus-Immune bridge configuration
 */
typedef struct {
    /* Cytokine sensitivity */
    float fever_sensitivity;    /**< Sensitivity to fever-inducing cytokines */
    float anorexia_sensitivity; /**< Sensitivity to appetite-suppressing signals */
    float fatigue_sensitivity;  /**< Sensitivity to fatigue-inducing signals */

    /* HPA axis parameters */
    float cortisol_baseline;    /**< Baseline cortisol level */
    float stress_threshold;     /**< Threshold for stress response */
    float recovery_rate;        /**< Rate of HPA recovery */

    /* Circadian modulation */
    bool circadian_enabled;     /**< Enable circadian immune modulation */
    float circadian_amplitude;  /**< Amplitude of circadian effects */

    /* Sickness behavior thresholds */
    float mild_threshold;       /**< Threshold for mild sickness */
    float moderate_threshold;   /**< Threshold for moderate sickness */
    float severe_threshold;     /**< Threshold for severe sickness */
    float storm_threshold;      /**< Threshold for cytokine storm */

    /* Alignment integration */
    bool use_as_safety_mode;    /**< Use sickness behavior for safety mode */
    float safety_trigger;       /**< Alignment threat level to trigger */

    /* Extended configuration (bidirectional coupling) */
    float cortisol_immune_suppression;  /**< How much cortisol suppresses immune [0, 1] */
    float cytokine_stress_sensitivity;  /**< HPA sensitivity to cytokines [0, 1] */
    float fever_threshold;              /**< IL-1/IL-6 level to trigger fever */
    float sickness_behavior_threshold;  /**< When to induce sickness behavior */
    bool enable_bidirectional;          /**< Enable full bidirectional coupling */
    bool enable_bio_async;              /**< Enable bio-async messaging */

    /* Sleep-immune parameters */
    float sleep_immune_coupling;        /**< Sleep-immune interaction strength */
    bool enable_sleep_modulation;       /**< Enable sleep-based immune modulation */
} hypo_immune_config_t;

/**
 * @brief Hypothalamus-Immune bridge handle (opaque)
 */
typedef struct hypo_immune_bridge hypo_immune_bridge_t;

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

/**
 * @brief Create hypothalamus-immune bridge
 *
 * @param drives Drive system handle (pointer)
 * @param immune Brain immune system handle (may be NULL)
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
hypo_immune_bridge_t* hypo_immune_bridge_create(
    hypo_drive_system_handle_t* drives,
    brain_immune_system_t* immune,
    const hypo_immune_config_t* config);

/**
 * @brief Destroy hypothalamus-immune bridge
 *
 * @param bridge Bridge handle
 */
void hypo_immune_bridge_destroy(hypo_immune_bridge_t* bridge);

/**
 * @brief Get default configuration
 *
 * @param config Output configuration
 */
void hypo_immune_bridge_default_config(hypo_immune_config_t* config);

/*=============================================================================
 * CONNECTION & UPDATE FUNCTIONS
 *===========================================================================*/

/**
 * @brief Connect bridge to orchestrator and immune system
 *
 * WHAT: Establishes bidirectional connection between hypothalamus and immune
 * WHY:  Enables coordinated neuroimmune responses
 * HOW:  Registers callbacks, sets up bio-async channels
 *
 * @param bridge Bridge handle
 * @param orch Hypothalamus orchestrator handle
 * @param immune Brain immune system handle
 * @return 0 on success, -1 on error
 */
int hypo_immune_connect(
    hypo_immune_bridge_t* bridge,
    hypo_orchestrator_t orch,
    brain_immune_system_t* immune);

/**
 * @brief Unified update function with delta time
 *
 * WHAT: Advances all bridge state by delta time
 * WHY:  Single entry point for time-based updates
 * HOW:  Updates HPA dynamics, circadian phase, sickness behavior
 *
 * @param bridge Bridge handle
 * @param delta_ms Time elapsed since last update (milliseconds)
 * @return 0 on success, -1 on error
 */
int hypo_immune_update(hypo_immune_bridge_t* bridge, uint64_t delta_ms);

/**
 * @brief Receive simplified cytokines from external source
 *
 * WHAT: Simplified API for receiving cytokine updates
 * WHY:  Easier integration with external immune modules
 * HOW:  Converts simplified structure to internal format
 *
 * @param bridge Bridge handle
 * @param cytokines Simplified cytokine state
 * @return 0 on success, -1 on error
 */
int hypo_immune_receive_cytokines(
    hypo_immune_bridge_t* bridge,
    const hypo_immune_cytokines_t* cytokines);

/**
 * @brief Send cortisol to immune system
 *
 * WHAT: Sends current cortisol level to brain immune system
 * WHY:  Cortisol modulates immune response (glucocorticoid effects)
 * HOW:  Updates immune system's cortisol-mediated suppression
 *
 * @param bridge Bridge handle
 * @param cortisol Cortisol level [0, 1]
 * @return 0 on success, -1 on error
 */
int hypo_immune_send_cortisol(hypo_immune_bridge_t* bridge, float cortisol);

/*=============================================================================
 * UNIFIED STATE QUERIES
 *===========================================================================*/

/**
 * @brief Get complete unified immune state
 *
 * WHAT: Returns comprehensive snapshot of hypothalamus-immune state
 * WHY:  Single query for all relevant metrics
 * HOW:  Aggregates cytokines, sickness, HPA, circadian, sleep-immune
 *
 * @param bridge Bridge handle
 * @param state Output unified state
 * @return 0 on success, -1 on error
 */
int hypo_immune_get_state(
    const hypo_immune_bridge_t* bridge,
    hypo_immune_state_t* state);

/**
 * @brief Get current inflammation level
 *
 * WHAT: Quick query for overall inflammation
 * WHY:  Common query needed by many modules
 * HOW:  Returns cached inflammation calculation
 *
 * @param bridge Bridge handle
 * @return Inflammation level [0, 1], or 0 on error
 */
float hypo_immune_get_inflammation(const hypo_immune_bridge_t* bridge);

/**
 * @brief Get fever signal strength
 *
 * WHAT: Returns hypothalamic fever signal
 * WHY:  Needed by thermoregulation and other modules
 * HOW:  Computed from IL-1β and IL-6 levels
 *
 * @param bridge Bridge handle
 * @return Fever signal [0, 1], or 0 on error
 */
float hypo_immune_get_fever_signal(const hypo_immune_bridge_t* bridge);

/**
 * @brief Check if sickness behavior is active
 *
 * WHAT: Boolean check for sickness behavior state
 * WHY:  Quick check for behavior modulation decisions
 * HOW:  Returns true if sickness level >= SICKNESS_MILD
 *
 * @param bridge Bridge handle
 * @return true if sickness behavior is active
 */
bool hypo_immune_is_sickness_behavior(const hypo_immune_bridge_t* bridge);

/*=============================================================================
 * MODULATION & CONTROL
 *===========================================================================*/

/**
 * @brief Modulate immune response via cortisol
 *
 * WHAT: Applies suppression factor to immune response
 * WHY:  Implements HPA-mediated immune modulation
 * HOW:  Sends suppression signal via bio-async or direct call
 *
 * @param bridge Bridge handle
 * @param suppression Suppression level [0, 1] (0=none, 1=max)
 * @return 0 on success, -1 on error
 */
int hypo_immune_modulate_immune_response(
    hypo_immune_bridge_t* bridge,
    float suppression);

/**
 * @brief Trigger acute phase response
 *
 * WHAT: Initiates systemic acute phase response
 * WHY:  Coordinated response to significant infection/injury
 * HOW:  Elevates pro-inflammatory cytokines, activates sickness behavior
 *
 * BIOLOGICAL BASIS:
 * The acute phase response includes:
 * - Fever induction
 * - Anorexia
 * - Fatigue and hypersomnia
 * - Social withdrawal
 * - Elevated acute phase proteins (via liver)
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int hypo_immune_trigger_acute_phase(hypo_immune_bridge_t* bridge);

/**
 * @brief End acute phase response
 *
 * WHAT: Terminates acute phase response
 * WHY:  Return to normal homeostatic state
 * HOW:  Elevates anti-inflammatory cytokines, reduces sickness behavior
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int hypo_immune_end_acute_phase(hypo_immune_bridge_t* bridge);

/*=============================================================================
 * SLEEP-IMMUNE INTERACTION
 *===========================================================================*/

/**
 * @brief Update sleep-immune interaction state
 *
 * WHAT: Updates sleep-immune coupling based on sleep state
 * WHY:  Sleep profoundly affects immune function and vice versa
 * HOW:  Modulates cytokine levels based on sleep quality
 *
 * @param bridge Bridge handle
 * @param sleep_quality Sleep quality [0, 1] (0=deprived, 1=restorative)
 * @param is_sleeping Whether currently sleeping
 * @return 0 on success, -1 on error
 */
int hypo_immune_update_sleep(
    hypo_immune_bridge_t* bridge,
    float sleep_quality,
    bool is_sleeping);

/**
 * @brief Get sleep-immune state
 *
 * @param bridge Bridge handle
 * @return Current sleep-immune interaction state
 */
hypo_sleep_immune_t hypo_immune_get_sleep_state(
    const hypo_immune_bridge_t* bridge);

/**
 * @brief Get sickness-induced sleep drive
 *
 * WHAT: Returns increased sleep drive due to sickness
 * WHY:  Sickness behavior includes hypersomnia
 * HOW:  Computed from pro-inflammatory cytokine levels
 *
 * @param bridge Bridge handle
 * @return Additional sleep drive [0, 1], or 0 on error
 */
float hypo_immune_get_sickness_sleep_drive(const hypo_immune_bridge_t* bridge);

/*=============================================================================
 * CYTOKINE → HYPOTHALAMUS (Immune → Drive Modulation)
 *===========================================================================*/

/**
 * @brief Update cytokine levels from immune system
 *
 * @param bridge Bridge handle
 * @param cytokines Current cytokine levels
 * @return 0 on success, -1 on error
 */
int hypo_immune_bridge_update_cytokines(
    hypo_immune_bridge_t* bridge,
    const hypo_cytokine_state_t* cytokines);

/**
 * @brief Process cytokine effects on drive setpoints
 *
 * Implements:
 * - IL-1β, IL-6 → Temperature setpoint ↑ (fever)
 * - TNF-α → Hunger setpoint ↓ (anorexia)
 * - Pro-inflammatory → Fatigue setpoint ↑
 * - High inflammation → Social, curiosity ↓
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int hypo_immune_bridge_apply_cytokine_effects(hypo_immune_bridge_t* bridge);

/**
 * @brief Get current fever magnitude
 *
 * @param bridge Bridge handle
 * @return Fever magnitude (0 = no fever, 1 = max fever)
 */
float hypo_immune_bridge_get_fever(const hypo_immune_bridge_t* bridge);

/**
 * @brief Get sickness behavior state
 *
 * @param bridge Bridge handle
 * @param state Output sickness state
 * @return 0 on success, -1 on error
 */
int hypo_immune_bridge_get_sickness_state(
    const hypo_immune_bridge_t* bridge,
    hypo_sickness_state_t* state);

/*=============================================================================
 * HYPOTHALAMUS → IMMUNE (HPA Axis → Immune Suppression)
 *===========================================================================*/

/**
 * @brief Update HPA axis state
 *
 * @param bridge Bridge handle
 * @param stress_input Current stress level [0, 1]
 * @return 0 on success, -1 on error
 */
int hypo_immune_bridge_update_hpa(
    hypo_immune_bridge_t* bridge,
    float stress_input);

/**
 * @brief Get cortisol-mediated immune suppression factor
 *
 * @param bridge Bridge handle
 * @return Suppression factor (0 = no suppression, 1 = max suppression)
 */
float hypo_immune_bridge_get_immune_suppression(
    const hypo_immune_bridge_t* bridge);

/**
 * @brief Get HPA axis state
 *
 * @param bridge Bridge handle
 * @param hpa Output HPA state
 * @return 0 on success, -1 on error
 */
int hypo_immune_bridge_get_hpa_state(
    const hypo_immune_bridge_t* bridge,
    hypo_hpa_axis_t* hpa);

/**
 * @brief Apply cortisol effects to immune system
 *
 * Sends suppression signals to brain immune system
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int hypo_immune_bridge_apply_cortisol_effects(hypo_immune_bridge_t* bridge);

/*=============================================================================
 * CIRCADIAN → IMMUNE MODULATION
 *===========================================================================*/

/**
 * @brief Update circadian immune phase
 *
 * @param bridge Bridge handle
 * @param scn_phase SCN circadian phase [0, 2π]
 * @return 0 on success, -1 on error
 */
int hypo_immune_bridge_update_circadian(
    hypo_immune_bridge_t* bridge,
    float scn_phase);

/**
 * @brief Get circadian immune phase
 *
 * @param bridge Bridge handle
 * @return Current circadian immune phase
 */
hypo_circadian_immune_t hypo_immune_bridge_get_circadian_phase(
    const hypo_immune_bridge_t* bridge);

/**
 * @brief Get circadian immune modulation factor
 *
 * @param bridge Bridge handle
 * @return Modulation factor affecting immune cell activity
 */
float hypo_immune_bridge_get_circadian_modulation(
    const hypo_immune_bridge_t* bridge);

/*=============================================================================
 * SICKNESS BEHAVIOR
 *===========================================================================*/

/**
 * @brief Compute overall sickness behavior level
 *
 * @param bridge Bridge handle
 * @return Sickness level enum
 */
hypo_sickness_level_t hypo_immune_bridge_compute_sickness_level(
    hypo_immune_bridge_t* bridge);

/**
 * @brief Apply sickness behavior to all drives
 *
 * Modulates drive setpoints based on sickness level:
 * - Hunger ↓ (anorexia)
 * - Fatigue ↑ (rest-seeking)
 * - Temperature setpoint ↑ (fever)
 * - Social ↓ (withdrawal)
 * - Curiosity ↓ (reduced exploration)
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int hypo_immune_bridge_apply_sickness_behavior(hypo_immune_bridge_t* bridge);

/**
 * @brief Check for cytokine storm condition
 *
 * @param bridge Bridge handle
 * @return true if cytokine storm detected
 */
bool hypo_immune_bridge_is_cytokine_storm(const hypo_immune_bridge_t* bridge);

/**
 * @brief Enter alignment safety mode using sickness behavior
 *
 * Uses sickness behavior mechanisms to enter a safe, conservative mode
 * when alignment threats are detected.
 *
 * @param bridge Bridge handle
 * @param threat_level Alignment threat level [0, 1]
 * @return 0 on success, -1 on error
 */
int hypo_immune_bridge_enter_safety_mode(
    hypo_immune_bridge_t* bridge,
    float threat_level);

/**
 * @brief Exit alignment safety mode
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int hypo_immune_bridge_exit_safety_mode(hypo_immune_bridge_t* bridge);

/*=============================================================================
 * BIO-ASYNC INTEGRATION
 *===========================================================================*/

/**
 * @brief Register with bio-async router
 *
 * @param bridge Bridge handle
 * @param use_kg_wiring Use KG wiring for routing
 * @return true on success, false on error
 */
bool hypo_immune_bridge_register_bio(
    hypo_immune_bridge_t* bridge,
    bool use_kg_wiring);

/**
 * @brief Unregister from bio-async router
 *
 * @param bridge Bridge handle
 */
void hypo_immune_bridge_unregister_bio(hypo_immune_bridge_t* bridge);

/**
 * @brief Broadcast fever state change
 *
 * @param bridge Bridge handle
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t hypo_immune_bridge_broadcast_fever(hypo_immune_bridge_t* bridge);

/**
 * @brief Broadcast sickness behavior state
 *
 * @param bridge Bridge handle
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t hypo_immune_bridge_broadcast_sickness(hypo_immune_bridge_t* bridge);

/**
 * @brief Broadcast HPA/cortisol state for immune modulation
 *
 * @param bridge Bridge handle
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t hypo_immune_bridge_broadcast_cortisol(hypo_immune_bridge_t* bridge);

/*=============================================================================
 * STATISTICS & MONITORING
 *===========================================================================*/

/**
 * @brief Bridge statistics
 */
typedef struct {
    /* Cytokine processing */
    uint64_t cytokine_updates;
    uint64_t fever_episodes;
    uint64_t anorexia_episodes;

    /* HPA axis */
    uint64_t stress_activations;
    uint64_t recovery_cycles;
    float peak_cortisol;

    /* Sickness behavior */
    uint64_t sickness_episodes;
    uint64_t cytokine_storms;
    uint64_t safety_mode_entries;

    /* Bio-async */
    uint64_t messages_sent;
    uint64_t messages_received;
} hypo_immune_bridge_stats_t;

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int hypo_immune_bridge_get_stats(
    const hypo_immune_bridge_t* bridge,
    hypo_immune_bridge_stats_t* stats);

/**
 * @brief Reset bridge statistics
 *
 * @param bridge Bridge handle
 */
void hypo_immune_bridge_reset_stats(hypo_immune_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HYPOTHALAMUS_IMMUNE_BRIDGE_H */
