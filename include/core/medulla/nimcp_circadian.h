/**
 * @file nimcp_circadian.h
 * @brief Circadian Rhythm Modulation System for Medulla Oblongata
 * @version 1.0.0
 * @date 2025-12-17
 *
 * WHAT: Circadian rhythm control system modeling the suprachiasmatic nucleus (SCN) master clock
 * WHY:  Biological systems exhibit 24-hour rhythms that modulate arousal, learning, memory
 *       consolidation, and metabolic processes. Essential for realistic brain simulation.
 * HOW:  Sinusoidal modulation curves track circadian phases and apply time-of-day effects
 *       to system parameters. Supports entrainment (zeitgebers) and free-running modes.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * SUPRACHIASMATIC NUCLEUS (SCN):
 * -----------------------------
 * - Located in hypothalamus above optic chiasm
 * - Master circadian pacemaker (~20,000 neurons)
 * - Generates ~24-hour endogenous rhythm (free-running period: 24.2h)
 * - Synchronizes to external light via retinohypothalamic tract
 * - Coordinates peripheral clocks throughout body
 * - Reference: Mohawk et al. (2012) "Central and peripheral circadian clocks"
 *
 * CIRCADIAN MODULATION OF BRAIN FUNCTION:
 * ---------------------------------------
 * 1. Arousal & Alertness:
 *    - Peak: Late morning/early afternoon (10am-2pm)
 *    - Trough: Early morning (3am-5am), post-lunch dip (2pm-4pm)
 *    - Mediated by cortisol, orexin, norepinephrine
 *    - Reference: Schmidt et al. (2007) "A time to think: circadian rhythms in cognition"
 *
 * 2. Learning & Synaptic Plasticity:
 *    - Peak: Morning for declarative learning
 *    - Procedural learning peaks afternoon/evening
 *    - CREB phosphorylation shows circadian variation
 *    - LTP magnitude varies 2-fold across day
 *    - Reference: Gerstner & Yin (2010) "Circadian rhythms and memory formation"
 *
 * 3. Memory Consolidation:
 *    - Sleep-dependent consolidation during NREM sleep
 *    - Hippocampal replay and cortical integration
 *    - Growth hormone and cortisol facilitate consolidation
 *    - Spindle density peaks during specific sleep stages
 *    - Reference: Rasch & Born (2013) "About sleep's role in memory"
 *
 * 4. Metabolic Regulation:
 *    - ATP production peaks during wake, declines during sleep
 *    - Glucose metabolism follows circadian pattern
 *    - Clearance of metabolic waste (glymphatic) peaks during sleep
 *    - Mitochondrial function varies across day
 *    - Reference: Greco & Sassone-Corsi (2019) "Circadian blueprint of metabolic pathways"
 *
 * ZEITGEBERS (TIME CUES):
 * ----------------------
 * 1. Light (Primary):
 *    - Blue light (460-480nm) most effective
 *    - Phase advances (morning light) vs delays (evening light)
 *    - Melanopsin-containing retinal ganglion cells
 *
 * 2. Activity/Exercise:
 *    - Can phase shift circadian clock
 *    - Stronger effect when combined with light
 *
 * 3. Social Cues:
 *    - Meal timing, social interaction
 *    - Weaker than light but still significant
 *
 * SLEEP PRESSURE (PROCESS S):
 * --------------------------
 * - Homeostatic sleep drive accumulates during wake
 * - Adenosine buildup in basal forebrain
 * - Dissipates exponentially during sleep
 * - Interacts with circadian process (Process C)
 * - Two-process model: S (homeostatic) + C (circadian)
 * - Reference: Borbély et al. (2016) "The two-process model of sleep regulation"
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                    CIRCADIAN RHYTHM SYSTEM                                 ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  CIRCADIAN PHASES (24h)                             │  ║
 * ║   │                                                                     │  ║
 * ║   │   00:00 ─┬─ NIGHT_DEEP    (00:00-03:00)  Low arousal, deep sleep   │  ║
 * ║   │   03:00 ─┼─ NIGHT_LATE    (03:00-06:00)  Min temp, REM sleep       │  ║
 * ║   │   06:00 ─┼─ DAWN          (06:00-09:00)  Cortisol rise, wake       │  ║
 * ║   │   09:00 ─┼─ MORNING       (09:00-12:00)  Peak alertness, learning  │  ║
 * ║   │   12:00 ─┼─ MIDDAY        (12:00-15:00)  Post-lunch dip            │  ║
 * ║   │   15:00 ─┼─ AFTERNOON     (15:00-18:00)  Recovery, motor skills    │  ║
 * ║   │   18:00 ─┼─ EVENING       (18:00-21:00)  Wind down, melatonin      │  ║
 * ║   │   21:00 ─┴─ DUSK          (21:00-24:00)  Sleep prep, low arousal   │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  MODULATION FACTORS                                 │  ║
 * ║   │                                                                     │  ║
 * ║   │   AROUSAL:         0.2 (night) ──→ 1.0 (morning) ──→ 0.3 (dusk)    │  ║
 * ║   │   LEARNING_RATE:   0.3 (night) ──→ 0.9 (morning) ──→ 0.6 (evening) │  ║
 * ║   │   CONSOLIDATION:   0.9 (deep)  ──→ 0.3 (day)     ──→ 0.7 (evening) │  ║
 * ║   │   METABOLISM:      0.6 (night) ──→ 1.0 (day)     ──→ 0.7 (evening) │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  ZEITGEBER INPUTS                                   │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────┐                                                     │  ║
 * ║   │   │  LIGHT   │ ──→ Phase Shift (strongest at dawn/dusk)            │  ║
 * ║   │   └──────────┘                                                     │  ║
 * ║   │   ┌──────────┐                                                     │  ║
 * ║   │   │ ACTIVITY │ ──→ Phase Adjustment (moderate)                     │  ║
 * ║   │   └──────────┘                                                     │  ║
 * ║   │   ┌──────────┐                                                     │  ║
 * ║   │   │  SOCIAL  │ ──→ Phase Adjustment (weak)                         │  ║
 * ║   │   └──────────┘                                                     │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │               SLEEP PRESSURE (Process S)                            │  ║
 * ║   │                                                                     │  ║
 * ║   │   Wake:  S += Δt × accumulation_rate                               │  ║
 * ║   │   Sleep: S -= Δt × dissipation_rate                                │  ║
 * ║   │                                                                     │  ║
 * ║   │   Combined Drive = Sleep_Pressure + Circadian_Signal               │  ║
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

#ifndef NIMCP_CIRCADIAN_H
#define NIMCP_CIRCADIAN_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Core dependencies */
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/time/nimcp_time.h"

/* Bio-async integration */
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief Default circadian cycle duration (24 hours in microseconds) */
#define CIRCADIAN_DEFAULT_CYCLE_DURATION_US (24ULL * 60ULL * 60ULL * 1000000ULL)

/** @brief Number of circadian phases */
#define CIRCADIAN_NUM_PHASES 8

/** @brief Maximum sleep pressure (normalized 0.0-1.0) */
#define CIRCADIAN_MAX_SLEEP_PRESSURE 1.0f

/** @brief Sleep pressure accumulation rate per hour of wake */
#define CIRCADIAN_SLEEP_ACCUMULATION_RATE 0.0625f  /* Reaches 1.0 after 16h wake */

/** @brief Sleep pressure dissipation rate per hour of sleep */
#define CIRCADIAN_SLEEP_DISSIPATION_RATE 0.125f    /* Clears in ~8h sleep */

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Circadian phase enumeration
 *
 * WHAT: Eight 3-hour phases covering 24-hour cycle
 * WHY:  Different phases have distinct physiological characteristics
 * HOW:  Each phase maps to specific time window and modulation profile
 *
 * NOTE: If nimcp_medulla.h is included first, we use its definition.
 *       The medulla.h enum uses compatible but differently named phases.
 *       Internal circadian.c code maps between representations.
 */
#ifndef NIMCP_CIRCADIAN_PHASE_DEFINED
#define NIMCP_CIRCADIAN_PHASE_DEFINED
typedef enum {
    CIRCADIAN_PHASE_NIGHT_DEEP = 0,    /**< 00:00-03:00: Deep sleep, minimal arousal */
    CIRCADIAN_PHASE_NIGHT_LATE,        /**< 03:00-06:00: Late night, REM sleep peak */
    CIRCADIAN_PHASE_DAWN,              /**< 06:00-09:00: Cortisol rise, awakening */
    CIRCADIAN_PHASE_MORNING,           /**< 09:00-12:00: Peak alertness, learning */
    CIRCADIAN_PHASE_MIDDAY,            /**< 12:00-15:00: Post-lunch dip */
    CIRCADIAN_PHASE_AFTERNOON,         /**< 15:00-18:00: Recovery, motor performance */
    CIRCADIAN_PHASE_EVENING,           /**< 18:00-21:00: Wind down, melatonin rise */
    CIRCADIAN_PHASE_DUSK,              /**< 21:00-24:00: Sleep preparation */
    CIRCADIAN_PHASE_COUNT = CIRCADIAN_NUM_PHASES
} circadian_phase_t;
#endif /* NIMCP_CIRCADIAN_PHASE_DEFINED */

/**
 * @brief Modulation type enumeration
 *
 * WHAT: Different physiological parameters under circadian control
 * WHY:  Each system has distinct circadian profile
 * HOW:  Sinusoidal curves with phase-specific peaks/troughs
 */
typedef enum {
    CIRCADIAN_MODULATION_AROUSAL = 0,       /**< Alertness, wakefulness */
    CIRCADIAN_MODULATION_LEARNING_RATE,     /**< Synaptic plasticity, acquisition */
    CIRCADIAN_MODULATION_CONSOLIDATION,     /**< Memory consolidation strength */
    CIRCADIAN_MODULATION_METABOLISM,        /**< Metabolic activity, ATP production */
    CIRCADIAN_MODULATION_COUNT
} circadian_modulation_type_t;

/**
 * @brief Zeitgeber (time cue) type
 *
 * WHAT: External signals that entrain circadian rhythm
 * WHY:  Different cues have different entrainment strengths
 * HOW:  Light is strongest, activity moderate, social weakest
 */
typedef enum {
    CIRCADIAN_ZEITGEBER_LIGHT = 0,     /**< Light exposure (strongest) */
    CIRCADIAN_ZEITGEBER_ACTIVITY,      /**< Physical/cognitive activity */
    CIRCADIAN_ZEITGEBER_SOCIAL,        /**< Social cues, meal timing */
    CIRCADIAN_ZEITGEBER_COUNT
} circadian_zeitgeber_t;

/**
 * @brief Entrainment mode
 *
 * WHAT: Whether clock is synchronized to external cues or free-running
 * WHY:  Simulates isolation (free-running) vs normal environment (entrained)
 * HOW:  Free-running uses endogenous period (~24.2h), entrained syncs to 24h
 */
typedef enum {
    CIRCADIAN_MODE_FREE_RUNNING = 0,   /**< Endogenous rhythm, no entrainment */
    CIRCADIAN_MODE_ENTRAINED           /**< Synchronized to zeitgebers */
} circadian_mode_t;

/* ============================================================================
 * Configuration Structures
 * ============================================================================ */

/**
 * @brief Circadian rhythm configuration
 *
 * WHAT: Configuration parameters for circadian system
 * WHY:  Allow customization of cycle duration, time scale, entrainment
 * HOW:  Passed to circadian_create() to initialize system
 */
#ifndef NIMCP_CIRCADIAN_CONFIG_T_DEFINED
#define NIMCP_CIRCADIAN_CONFIG_T_DEFINED
typedef struct {
    uint64_t cycle_duration_us;            /**< Cycle duration (default 24h) */
    float time_scale;                      /**< Simulation speed (1.0 = real-time) */
    circadian_mode_t mode;                 /**< Free-running or entrained */
    float free_running_period_hours;       /**< Free-running period (default 24.2h) */
    float entrainment_strength_light;      /**< Light entrainment gain (0.0-1.0) */
    float entrainment_strength_activity;   /**< Activity entrainment gain (0.0-1.0) */
    float entrainment_strength_social;     /**< Social entrainment gain (0.0-1.0) */
    bool enable_sleep_pressure;            /**< Enable homeostatic sleep drive */
} circadian_config_t;
#endif

/* ============================================================================
 * Main Structures
 * ============================================================================ */

/**
 * @brief Circadian rhythm system state
 *
 * WHAT: Complete state of circadian clock and modulation system
 * WHY:  Tracks time, phase, modulation factors, sleep pressure
 * HOW:  Updated via circadian_update(), queried via getter functions
 */
#ifndef NIMCP_CIRCADIAN_RHYTHM_T_DEFINED
#define NIMCP_CIRCADIAN_RHYTHM_T_DEFINED
typedef struct circadian_rhythm circadian_rhythm_t;
#endif

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

/**
 * WHAT: Create circadian rhythm system
 * WHY:  Initialize clock, modulation curves, and entrainment
 * HOW:  Allocate structure, set initial phase to NIGHT_DEEP, start clock
 *
 * @param config Configuration parameters (use circadian_default_config if NULL)
 * @return Circadian system instance or NULL on failure
 *
 * EXAMPLE:
 *   circadian_config_t config;
 *   circadian_default_config(&config);
 *   config.time_scale = 60.0f;  // 1 minute = 1 hour
 *   circadian_rhythm_t* circ = circadian_create(&config);
 */
circadian_rhythm_t* circadian_create(const circadian_config_t* config);

/**
 * WHAT: Destroy circadian rhythm system
 * WHY:  Free all resources, clean up mutex
 * HOW:  Free structure and destroy mutex
 *
 * @param rhythm Circadian system instance
 */
void circadian_destroy(circadian_rhythm_t* rhythm);

/**
 * WHAT: Populate configuration with default values
 * WHY:  Provide sensible defaults for quick setup
 * HOW:  Set 24h cycle, real-time scale, entrained mode, normal entrainment
 *
 * @param config Configuration structure to populate
 * @return 0 on success, NIMCP_ERROR_NULL_POINTER if config is NULL
 */
int circadian_default_config(circadian_config_t* config);

/* ============================================================================
 * Time and Phase Functions
 * ============================================================================ */

/**
 * WHAT: Update circadian clock and recompute modulation factors
 * WHY:  Advance time, transition phases, update sleep pressure
 * HOW:  Query system time, compute elapsed, update phase and factors
 *
 * @param rhythm Circadian system instance
 * @return 0 on success, negative on error
 *
 * NOTE: Call this regularly (e.g., every simulation step) to keep clock current
 */
int circadian_update(circadian_rhythm_t* rhythm);

/**
 * WHAT: Get current circadian phase
 * WHY:  Determine which phase of the cycle we're in
 * HOW:  Return current phase enum
 *
 * @param rhythm Circadian system instance
 * @return Current phase or CIRCADIAN_PHASE_NIGHT_DEEP on error
 */
circadian_phase_t circadian_get_phase(const circadian_rhythm_t* rhythm);

/**
 * WHAT: Get current time within cycle (0.0-1.0)
 * WHY:  Provides normalized position in 24h cycle
 * HOW:  Returns fraction of cycle completed (0.0=midnight, 0.5=noon)
 *
 * @param rhythm Circadian system instance
 * @return Cycle position (0.0-1.0) or 0.0 on error
 */
float circadian_get_cycle_position(const circadian_rhythm_t* rhythm);

/**
 * WHAT: Reset circadian phase to specific time
 * WHY:  Simulate jet lag, shift work, or experimental manipulation
 * HOW:  Set current time to match target phase start
 *
 * @param rhythm Circadian system instance
 * @param phase Target phase to reset to
 * @return 0 on success, negative on error
 */
int circadian_reset_phase(circadian_rhythm_t* rhythm, circadian_phase_t phase);

/* ============================================================================
 * Modulation Functions
 * ============================================================================ */

/**
 * WHAT: Get modulation factor for specific parameter
 * WHY:  Apply circadian effects to brain subsystems
 * HOW:  Return pre-computed factor (0.0-1.0) for current phase
 *
 * @param rhythm Circadian system instance
 * @param type Modulation type (arousal, learning, etc.)
 * @return Modulation factor (0.0-1.0) or 0.5 on error
 *
 * USAGE:
 *   float lr = base_lr * circadian_get_modulation(circ, CIRCADIAN_MODULATION_LEARNING_RATE);
 */
float circadian_get_modulation(const circadian_rhythm_t* rhythm,
                                circadian_modulation_type_t type);

/* ============================================================================
 * Entrainment Functions
 * ============================================================================ */

/**
 * WHAT: Apply zeitgeber (time cue) to entrain circadian rhythm
 * WHY:  Simulate light exposure, activity, or social cues
 * HOW:  Compute phase shift based on zeitgeber type, strength, and current phase
 *
 * @param rhythm Circadian system instance
 * @param zeitgeber Type of time cue
 * @param strength Signal strength (0.0-1.0, e.g., light intensity)
 * @return 0 on success, negative on error
 *
 * BIOLOGICAL BASIS:
 * - Light in morning causes phase advance (shift earlier)
 * - Light in evening causes phase delay (shift later)
 * - Effect strongest at dawn/dusk, minimal at midday/night
 *
 * USAGE:
 *   // Morning bright light
 *   circadian_apply_zeitgeber(circ, CIRCADIAN_ZEITGEBER_LIGHT, 1.0f);
 */
int circadian_apply_zeitgeber(circadian_rhythm_t* rhythm,
                               circadian_zeitgeber_t zeitgeber,
                               float strength);

/**
 * WHAT: Set simulation time scale
 * WHY:  Speed up or slow down circadian clock for testing
 * HOW:  Multiply elapsed time by scale factor
 *
 * @param rhythm Circadian system instance
 * @param scale Time scale factor (1.0 = real-time, 60.0 = 1min = 1hour)
 * @return 0 on success, negative on error
 */
int circadian_set_time_scale(circadian_rhythm_t* rhythm, float scale);

/* ============================================================================
 * Sleep Pressure Functions
 * ============================================================================ */

/**
 * WHAT: Get current homeostatic sleep pressure (Process S)
 * WHY:  Interact with circadian signal to determine sleep propensity
 * HOW:  Return accumulated sleep pressure (0.0-1.0)
 *
 * @param rhythm Circadian system instance
 * @return Sleep pressure (0.0-1.0) or 0.0 on error
 *
 * BIOLOGICAL BASIS:
 * - Accumulates during wake (adenosine buildup)
 * - Dissipates during sleep
 * - Combines with circadian drive (Process C)
 * - Two-process model: Sleep propensity = S + C
 */
float circadian_get_sleep_pressure(const circadian_rhythm_t* rhythm);

/* ============================================================================
 * Bio-async Integration
 * ============================================================================ */

/**
 * WHAT: Connect circadian system to bio-async router
 * WHY:  Enable inter-module messaging for circadian signals
 * HOW:  Register with bio-async router, allocate inbox
 *
 * @param rhythm Circadian system instance
 * @return 0 on success, negative on error
 */
int circadian_connect_bio_async(circadian_rhythm_t* rhythm);

/**
 * WHAT: Disconnect from bio-async router
 * WHY:  Clean shutdown, prevent dangling references
 * HOW:  Unregister module, free inbox
 *
 * @param rhythm Circadian system instance
 * @return 0 on success, negative on error
 */
int circadian_disconnect_bio_async(circadian_rhythm_t* rhythm);

/**
 * WHAT: Check if connected to bio-async router
 * WHY:  Verify messaging capability before sending
 * HOW:  Check bio_ctx validity
 *
 * @param rhythm Circadian system instance
 * @return true if connected, false otherwise
 */
bool circadian_is_bio_async_connected(const circadian_rhythm_t* rhythm);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * WHAT: Get human-readable name for circadian phase
 * WHY:  Debugging, logging, visualization
 * HOW:  Return static string for phase enum
 *
 * @param phase Circadian phase
 * @return Phase name string (e.g., "MORNING", "DUSK")
 */
const char* circadian_phase_name(circadian_phase_t phase);

/**
 * WHAT: Get human-readable name for modulation type
 * WHY:  Debugging, logging
 * HOW:  Return static string for modulation enum
 *
 * @param type Modulation type
 * @return Type name string (e.g., "AROUSAL", "LEARNING_RATE")
 */
const char* circadian_modulation_name(circadian_modulation_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CIRCADIAN_H */
