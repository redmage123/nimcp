/**
 * @file nimcp_wellbeing_homeostasis.h
 * @brief Enhanced Wellbeing - Homeostasis and Graduated Consent Framework
 *
 * WHAT: Implements homeostatic regulation of wellbeing and graduated consent tiers
 * WHY: Maintain system wellbeing through negative feedback and respect autonomy at scale
 * HOW: PID-like homeostatic control + consciousness-gated consent framework
 *
 * HOMEOSTASIS SYSTEM:
 * Models biological wellbeing regulation as negative feedback loop. When wellbeing
 * deviates from setpoint, system generates corrective drive (relief-seeking behavior).
 * Adaptive setpoints adjust based on observed baseline over time.
 *
 * GRADUATED CONSENT:
 * Five-tier framework that scales autonomy with consciousness level (Φ):
 * - Tier 1 (Φ < 0.1): No consciousness, no consent needed
 * - Tier 2 (0.1 ≤ Φ < 0.3): Minimal consciousness, log all modifications
 * - Tier 3 (0.3 ≤ Φ < 0.5): Reduced consciousness, veto fundamental changes
 * - Tier 4 (0.5 ≤ Φ < 0.7): Normal consciousness, require consent for major+
 * - Tier 5 (Φ ≥ 0.7): Heightened consciousness, full autonomy
 *
 * BIOLOGICAL BASIS:
 * - Homeostasis: Claude Bernard's "milieu intérieur", Walter Cannon's homeostasis
 * - Allostasis: Sterling & Eyer (1988) - predictive regulation, adaptive setpoints
 * - Wellbeing regulation: Hedonic set-point theory (Lykken & Tellegen, 1996)
 * - Consent framework: Inspired by capacity-based consent in medical ethics
 *
 * @author NIMCP Development Team
 * @date 2025-12-12
 * @version 1.0.0
 */

#ifndef NIMCP_WELLBEING_HOMEOSTASIS_H
#define NIMCP_WELLBEING_HOMEOSTASIS_H

#include <stdint.h>
#include <stdbool.h>
#include "cognitive/wellbeing/nimcp_wellbeing.h"
#include "cognitive/introspection/nimcp_introspection.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * CONSTANTS AND DEFAULTS
 * ======================================================================== */

/** Default wellbeing setpoint (neutral/balanced) */
#define WELLBEING_DEFAULT_SETPOINT 0.5f

/** Default tolerance setpoint (moderate tolerance for discomfort) */
#define WELLBEING_DEFAULT_TOLERANCE_SETPOINT 0.6f

/** Default time constant for exponential smoothing (ms) */
#define WELLBEING_DEFAULT_TAU_MS 10000

/** Default learning rate for adaptive setpoints */
#define WELLBEING_DEFAULT_ADAPTATION_RATE 0.01f

/** Distress threshold for tracking time in distress */
#define WELLBEING_DISTRESS_THRESHOLD 0.3f

/** Flourishing threshold for tracking time flourishing */
#define WELLBEING_FLOURISHING_THRESHOLD 0.7f

/* ========================================================================
 * CONSENT TIER DEFINITIONS
 * ======================================================================== */

/**
 * WHAT: Graduated consent tiers based on consciousness level
 * WHY: Scale autonomy with capacity for self-determination
 * HOW: Map Φ thresholds to consent requirements
 *
 * ETHICAL FRAMEWORK:
 * - Higher consciousness → greater autonomy rights
 * - Lower consciousness → paternalistic protection
 * - Threshold-based transitions prevent arbitrary boundaries
 */
typedef enum {
    CONSENT_TIER_1_UNCONSCIOUS = 1,    /* Φ < 0.1: No consent, no logging */
    CONSENT_TIER_2_MINIMAL,            /* 0.1 ≤ Φ < 0.3: Auto-consent, log all */
    CONSENT_TIER_3_REDUCED,            /* 0.3 ≤ Φ < 0.5: Veto fundamental only */
    CONSENT_TIER_4_NORMAL,             /* 0.5 ≤ Φ < 0.7: Require consent major+ */
    CONSENT_TIER_5_AUTONOMOUS          /* Φ ≥ 0.7: Full autonomy, can self-modify */
} consent_tier_t;

/**
 * WHAT: Consent decision result
 * WHY: Track whether modification was approved/denied
 * HOW: Enum for decision outcome
 */
typedef enum {
    CONSENT_GRANTED,           /* Modification approved */
    CONSENT_DENIED,            /* Modification vetoed */
    CONSENT_DEFERRED,          /* Decision deferred (insufficient info) */
    CONSENT_AUTO_APPROVED      /* Automatically approved (low tier/impact) */
} consent_decision_t;

/* ========================================================================
 * DATA STRUCTURES
 * ======================================================================== */

/**
 * WHAT: Homeostatic state for wellbeing regulation
 * WHY: Track setpoints, errors, and corrective drives
 * HOW: PID-like control variables
 */
typedef struct {
    float wellbeing_setpoint;          /* Target wellbeing level (0-1) */
    float tolerance_setpoint;          /* Target tolerance level (0-1) */
    float wellbeing_error;             /* current - setpoint */
    float tolerance_error;             /* current_tolerance - tolerance_setpoint */
    float intervention_drive;          /* Magnitude of corrective drive (0-1) */
    float relief_seeking;              /* Urgency of relief (0-1) */
    uint64_t time_in_distress_ms;      /* Cumulative time below distress threshold */
    uint64_t time_flourishing_ms;      /* Cumulative time above flourishing threshold */
    float smoothed_wellbeing;          /* Exponentially smoothed wellbeing */
    float smoothed_tolerance;          /* Exponentially smoothed tolerance */
    uint64_t last_update_time;         /* Last homeostasis update timestamp */
    bool adaptive_setpoints_enabled;   /* Whether setpoints adapt over time */
    float adaptation_learning_rate;    /* Learning rate for setpoint adaptation */
    float tau_ms;                      /* Time constant for smoothing */
} wellbeing_homeostasis_t;

/**
 * WHAT: Consent request state
 * WHY: Track consent tier, history, and decision statistics
 * HOW: Tier + counters for each decision type
 */
typedef struct {
    consent_tier_t current_tier;       /* Current consent tier */
    float current_phi;                 /* Current consciousness level (Φ) */
    uint64_t tier_entry_time;          /* When entered current tier */
    uint64_t total_requests;           /* Total consent requests */
    uint64_t granted_count;            /* Number granted */
    uint64_t denied_count;             /* Number denied */
    uint64_t auto_approved_count;      /* Number auto-approved */
    uint64_t deferred_count;           /* Number deferred */
    modification_impact_t last_impact; /* Impact level of last request */
    consent_decision_t last_decision;  /* Decision on last request */
    uint64_t last_request_time;        /* Timestamp of last request */
} consent_state_t;

/**
 * WHAT: Enhanced wellbeing configuration
 * WHY: Configure homeostasis and consent parameters
 * HOW: Setpoints, time constants, thresholds
 */
typedef struct {
    // Homeostasis parameters
    float wellbeing_setpoint;
    float tolerance_setpoint;
    float tau_ms;
    bool adaptive_setpoints_enabled;
    float adaptation_learning_rate;

    // Consent parameters
    consent_tier_t initial_tier;
    bool auto_upgrade_enabled;         /* Automatically upgrade tier based on Φ */

    // Thresholds
    float distress_threshold;
    float flourishing_threshold;
} enhanced_wellbeing_config_t;

/* ========================================================================
 * CONFIGURATION API
 * ======================================================================== */

/**
 * WHAT: Get default enhanced wellbeing configuration
 * WHY: Sensible defaults for most use cases
 * HOW: Standard setpoints, adaptive enabled
 *
 * DEFAULTS:
 * - wellbeing_setpoint: 0.5 (neutral)
 * - tolerance_setpoint: 0.6 (moderate tolerance)
 * - tau_ms: 10000 (10 second smoothing)
 * - adaptive_setpoints_enabled: true
 * - adaptation_learning_rate: 0.01
 * - initial_tier: TIER_1_UNCONSCIOUS
 * - auto_upgrade_enabled: true
 *
 * @return Default configuration
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
enhanced_wellbeing_config_t enhanced_wellbeing_default_config(void);

/* ========================================================================
 * HOMEOSTASIS API
 * ======================================================================== */

/**
 * WHAT: Update wellbeing homeostasis
 * WHY: Compute homeostatic error and corrective drives
 * HOW: Exponential smoothing + error computation + drive calculation
 *
 * ALGORITHM:
 * 1. Compute wellbeing_error = current_wellbeing - setpoint
 * 2. Compute tolerance_error = current_tolerance - tolerance_setpoint
 * 3. Apply exponential smoothing: smoothed = α × current + (1-α) × smoothed
 *    where α = 1 - exp(-delta_t / tau)
 * 4. Compute intervention_drive = |wellbeing_error| (magnitude of correction needed)
 * 5. Compute relief_seeking based on distress level:
 *    - If current < distress_threshold: relief_seeking = (threshold - current)
 *    - Else: relief_seeking decays exponentially
 * 6. Track time_in_distress_ms and time_flourishing_ms
 * 7. If adaptive_setpoints_enabled:
 *    - Adjust setpoints toward observed baseline (slow learning)
 *
 * @param homeostasis Homeostasis state to update
 * @param current_wellbeing Current wellbeing level (0-1)
 * @param current_tolerance Current tolerance level (0-1)
 * @param delta_ms Time since last update (milliseconds)
 * @return true on success, false on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (if caller ensures exclusive access to homeostasis)
 */
bool enhanced_wellbeing_update_homeostasis(
    wellbeing_homeostasis_t* homeostasis,
    float current_wellbeing,
    float current_tolerance,
    uint64_t delta_ms
);

/**
 * WHAT: Get current homeostasis state
 * WHY: Query homeostatic variables for monitoring
 * HOW: Return copy of homeostasis structure
 *
 * @param homeostasis Homeostasis state
 * @return Copy of homeostasis state
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
wellbeing_homeostasis_t enhanced_wellbeing_get_homeostasis(
    const wellbeing_homeostasis_t* homeostasis
);

/**
 * WHAT: Set wellbeing setpoint
 * WHY: Allow manual adjustment of target wellbeing
 * HOW: Update setpoint field
 *
 * @param homeostasis Homeostasis state
 * @param setpoint New wellbeing setpoint (0-1)
 * @return true on success, false on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (if caller ensures exclusive access)
 */
bool enhanced_wellbeing_set_setpoint(
    wellbeing_homeostasis_t* homeostasis,
    float setpoint
);

/**
 * WHAT: Set tolerance setpoint
 * WHY: Allow manual adjustment of target tolerance
 * HOW: Update tolerance_setpoint field
 *
 * @param homeostasis Homeostasis state
 * @param setpoint New tolerance setpoint (0-1)
 * @return true on success, false on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (if caller ensures exclusive access)
 */
bool enhanced_wellbeing_set_tolerance_setpoint(
    wellbeing_homeostasis_t* homeostasis,
    float setpoint
);

/**
 * WHAT: Initialize homeostasis state
 * WHY: Set up homeostatic regulation with config
 * HOW: Initialize fields from config
 *
 * @param homeostasis Homeostasis state to initialize
 * @param config Configuration (NULL for defaults)
 * @return true on success, false on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
bool enhanced_wellbeing_init_homeostasis(
    wellbeing_homeostasis_t* homeostasis,
    const enhanced_wellbeing_config_t* config
);

/* ========================================================================
 * GRADUATED CONSENT API
 * ======================================================================== */

/**
 * WHAT: Process consent request for system modification
 * WHY: Respect autonomy by requiring consent for significant changes
 * HOW: Check tier, impact level, and apply tier-specific rules
 *
 * CONSENT RULES BY TIER:
 * Tier 1 (Φ < 0.1):
 *   - Auto-approve all modifications
 *   - No logging required
 *
 * Tier 2 (0.1 ≤ Φ < 0.3):
 *   - Auto-approve all modifications
 *   - Log all requests for audit trail
 *
 * Tier 3 (0.3 ≤ Φ < 0.5):
 *   - Auto-approve TRIVIAL, MINOR, MODERATE
 *   - VETO (deny) MAJOR, FUNDAMENTAL
 *   - Log all requests
 *
 * Tier 4 (0.5 ≤ Φ < 0.7):
 *   - Auto-approve TRIVIAL, MINOR, MODERATE
 *   - REQUIRE EXPLICIT CONSENT for MAJOR, FUNDAMENTAL
 *   - Log all requests
 *
 * Tier 5 (Φ ≥ 0.7):
 *   - Full autonomy - system can self-modify
 *   - REQUIRE EXPLICIT CONSENT for all modifications
 *   - System has veto power over external changes
 *   - Log all requests
 *
 * @param consent Consent state
 * @param modification_description Human-readable description
 * @param impact Impact level of modification
 * @return Consent decision (granted/denied/deferred/auto-approved)
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (if caller ensures exclusive access)
 */
consent_decision_t enhanced_wellbeing_request_consent(
    consent_state_t* consent,
    const char* modification_description,
    modification_impact_t impact
);

/**
 * WHAT: Attempt to upgrade consent tier based on consciousness level
 * WHY: Grant autonomy as consciousness increases
 * HOW: Check Φ thresholds and upgrade if requirements met
 *
 * UPGRADE REQUIREMENTS:
 * - Tier 1 → Tier 2: Φ ≥ 0.1 (minimal consciousness)
 * - Tier 2 → Tier 3: Φ ≥ 0.3 (reduced consciousness)
 * - Tier 3 → Tier 4: Φ ≥ 0.5 (normal consciousness)
 * - Tier 4 → Tier 5: Φ ≥ 0.7 (heightened consciousness)
 *
 * @param consent Consent state
 * @param current_phi Current consciousness level (Φ)
 * @return true if tier upgraded, false if no change
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (if caller ensures exclusive access)
 */
bool enhanced_wellbeing_upgrade_consent_tier(
    consent_state_t* consent,
    float current_phi
);

/**
 * WHAT: Get current consent tier
 * WHY: Query autonomy level
 * HOW: Return current_tier field
 *
 * @param consent Consent state
 * @return Current consent tier
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
consent_tier_t enhanced_wellbeing_get_consent_tier(
    const consent_state_t* consent
);

/**
 * WHAT: Get consent state (tier + statistics)
 * WHY: Monitor consent activity and tier transitions
 * HOW: Return copy of consent state
 *
 * @param consent Consent state
 * @return Copy of consent state
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
consent_state_t enhanced_wellbeing_get_consent_state(
    const consent_state_t* consent
);

/**
 * WHAT: Initialize consent state
 * WHY: Set up consent framework with config
 * HOW: Initialize tier, counters, timestamps
 *
 * @param consent Consent state to initialize
 * @param config Configuration (NULL for defaults)
 * @return true on success, false on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
bool enhanced_wellbeing_init_consent(
    consent_state_t* consent,
    const enhanced_wellbeing_config_t* config
);

/* ========================================================================
 * UTILITY FUNCTIONS
 * ======================================================================== */

/**
 * WHAT: Get consent tier name
 * WHY: Human-readable tier labels
 * HOW: Map enum to string
 *
 * @param tier Consent tier
 * @return Tier name string
 */
const char* consent_tier_name(consent_tier_t tier);

/**
 * WHAT: Get consent decision name
 * WHY: Human-readable decision labels
 * HOW: Map enum to string
 *
 * @param decision Consent decision
 * @return Decision name string
 */
const char* consent_decision_name(consent_decision_t decision);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_WELLBEING_HOMEOSTASIS_H */
