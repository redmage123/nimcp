/**
 * @file nimcp_trained_immunity.h
 * @brief Trained Immunity (Innate Memory) System for Brain Immune
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Epigenetic-like reprogramming of innate immune cells for enhanced non-specific responses
 * WHY:  Biological evidence shows monocytes/macrophages develop memory-like properties after
 *       specific stimuli (BCG, beta-glucan), providing faster broad-spectrum protection.
 *       Critical for realistic immune modeling.
 * HOW:  Metabolic reprogramming (glycolysis shift), enhanced PRR sensitivity, epigenetic
 *       modifications, and long-lasting innate memory (weeks to months).
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * TRAINED IMMUNITY MECHANISMS:
 * ---------------------------
 * 1. Epigenetic Reprogramming:
 *    - Histone modifications (H3K4me3, H3K27ac) at inflammatory gene promoters
 *    - Open chromatin state at immune response genes
 *    - Persistent transcriptional readiness
 *    - Duration: Weeks to months in humans, days to weeks in mice
 *    - Reference: Netea et al. (2011) "Trained immunity: A program of innate immune
 *      memory in health and disease" Science 352(6284)
 *    - Reference: Saeed et al. (2014) "Epigenetic programming of monocyte-to-macrophage
 *      differentiation and trained innate immunity" Science 345(6204)
 *
 * 2. Metabolic Reprogramming:
 *    - Shift from oxidative phosphorylation to aerobic glycolysis (Warburg effect)
 *    - mTOR pathway activation
 *    - HIF-1α stabilization
 *    - Increased glucose consumption
 *    - Enhanced rapid ATP production for immune response
 *    - Reference: Cheng et al. (2014) "mTOR- and HIF-1α-mediated aerobic glycolysis
 *      as metabolic basis for trained immunity" Science 345(6204)
 *
 * 3. Enhanced Pattern Recognition Receptor (PRR) Sensitivity:
 *    - Upregulated TLR expression
 *    - Enhanced NOD-like receptor sensitivity
 *    - Faster recognition of PAMPs and DAMPs
 *    - Broader reactivity to unrelated pathogens
 *    - Reference: Quintin et al. (2012) "Candida albicans infection affords protection
 *      against reinfection via functional reprogramming of monocytes" Cell Host Microbe
 *
 * 4. Cross-Protection Effects:
 *    - BCG vaccine → protection against unrelated infections
 *    - Beta-glucan training → enhanced response to bacterial/viral threats
 *    - Non-specific heterologous immunity
 *    - Reference: Arts et al. (2018) "BCG vaccination protects against experimental
 *      viral infection in humans through the induction of cytokines associated with
 *      trained immunity" Cell Host Microbe 23(1)
 *
 * TRAINING STIMULI:
 * ----------------
 * 1. BCG (Bacillus Calmette-Guérin) vaccine:
 *    - Strongest and longest-lasting training effect
 *    - Protection against respiratory infections
 *    - Reduces all-cause mortality in children
 *
 * 2. Beta-glucan (fungal cell wall component):
 *    - Enhanced cytokine production (TNF-α, IL-6, IL-1β)
 *    - Protection against bacterial sepsis
 *    - Transient but potent effect
 *
 * 3. Low-dose LPS (endotoxin tolerance paradox):
 *    - Initial suppression followed by enhanced responsiveness
 *    - Trained state after resolution
 *
 * 4. Oxidized LDL (oxLDL):
 *    - Relevant for atherosclerosis
 *    - Chronic low-grade inflammation training
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                    TRAINED IMMUNITY SYSTEM                                 ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                    TRAINING STIMULI                                 │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌─────────┐  ┌──────────┐  ┌─────────┐  ┌─────────┐            │  ║
 * ║   │   │   BCG   │  │Beta-Glucan│  │ LPS-Low│  │ oxLDL   │            │  ║
 * ║   │   │(Vaccine)│  │ (Fungal) │  │  Dose  │  │(Chronic)│            │  ║
 * ║   │   └────┬────┘  └────┬─────┘  └────┬────┘  └────┬────┘            │  ║
 * ║   │        │            │             │            │                  │  ║
 * ║   │        └────────────┴─────────────┴────────────┘                  │  ║
 * ║   │                            │                                       │  ║
 * ║   │                            ▼                                       │  ║
 * ║   │              ┌──────────────────────────┐                          │  ║
 * ║   │              │  EPIGENETIC CHANGES      │                          │  ║
 * ║   │              │  - H3K4me3 ↑             │                          │  ║
 * ║   │              │  - H3K27ac ↑             │                          │  ║
 * ║   │              │  - Open chromatin        │                          │  ║
 * ║   │              └──────────┬───────────────┘                          │  ║
 * ║   │                         │                                          │  ║
 * ║   │                         ▼                                          │  ║
 * ║   │              ┌──────────────────────────┐                          │  ║
 * ║   │              │  METABOLIC SHIFT         │                          │  ║
 * ║   │              │  - Glycolysis ↑↑         │                          │  ║
 * ║   │              │  - mTOR activation       │                          │  ║
 * ║   │              │  - HIF-1α ↑              │                          │  ║
 * ║   │              └──────────┬───────────────┘                          │  ║
 * ║   │                         │                                          │  ║
 * ║   │                         ▼                                          │  ║
 * ║   │              ┌──────────────────────────┐                          │  ║
 * ║   │              │  ENHANCED PRR SENSITIVITY│                          │  ║
 * ║   │              │  - TLR upregulation      │                          │  ║
 * ║   │              │  - NOD-like ↑            │                          │  ║
 * ║   │              │  - Faster recognition    │                          │  ║
 * ║   │              └──────────┬───────────────┘                          │  ║
 * ║   │                         │                                          │  ║
 * ║   │                         ▼                                          │  ║
 * ║   │              ┌──────────────────────────┐                          │  ║
 * ║   │              │  ENHANCED RESPONSES      │                          │  ║
 * ║   │              │  - Faster activation     │                          │  ║
 * ║   │              │  - Greater magnitude     │                          │  ║
 * ║   │              │  - Broader spectrum      │                          │  ║
 * ║   │              │  - Cross-protection      │                          │  ║
 * ║   │              └──────────────────────────┘                          │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                    TEMPORAL DYNAMICS                                │  ║
 * ║   │                                                                     │  ║
 * ║   │   Enhancement                                                       │  ║
 * ║   │   Factor                                                            │  ║
 * ║   │      │                                                              │  ║
 * ║   │   3.0├──────┐                                                       │  ║
 * ║   │      │      │╲                                                      │  ║
 * ║   │   2.5│      │ ╲                                                     │  ║
 * ║   │      │      │  ╲___                                                 │  ║
 * ║   │   2.0│      │      ───___                                           │  ║
 * ║   │      │      │           ────____                                    │  ║
 * ║   │   1.5│      │                  ─────____                            │  ║
 * ║   │      │      │                          ────____                     │  ║
 * ║   │   1.0├──────┴──────────────────────────────────────────────        │  ║
 * ║   │      └──────┬───────┬───────┬───────┬───────┬───────┬──────> Time  │  ║
 * ║   │          Training  Days   Weeks   Months  Decay   Baseline         │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * DESIGN PATTERNS:
 * - Strategy: Different training stimuli
 * - State: Metabolic state tracking
 * - Observer: Integration with brain immune system
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

#ifndef NIMCP_TRAINED_IMMUNITY_H
#define NIMCP_TRAINED_IMMUNITY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "cognitive/immune/nimcp_brain_immune.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define TRAINED_IMMUNITY_MAX_HISTORY      32     /**< Max training history entries */
#define TRAINED_IMMUNITY_DECAY_INTERVAL   86400000  /**< Decay check interval (24h in ms) */

/* Enhancement factors by stimulus type */
#define TRAINED_BCG_ENHANCEMENT_PEAK      3.0f   /**< BCG peak enhancement (3x) */
#define TRAINED_BETA_GLUCAN_ENHANCEMENT   2.5f   /**< Beta-glucan enhancement */
#define TRAINED_LPS_ENHANCEMENT           2.0f   /**< Low-dose LPS enhancement */
#define TRAINED_OXLDL_ENHANCEMENT         1.8f   /**< oxLDL enhancement */

/* Duration constants (milliseconds) */
#define TRAINED_BCG_DURATION              7776000000ULL   /**< BCG: ~90 days */
#define TRAINED_BETA_GLUCAN_DURATION      604800000ULL    /**< Beta-glucan: ~7 days */
#define TRAINED_LPS_DURATION              1209600000ULL   /**< LPS: ~14 days */
#define TRAINED_OXLDL_DURATION            2592000000ULL   /**< oxLDL: ~30 days */

/* Half-life for decay (milliseconds) */
#define TRAINED_BCG_HALF_LIFE             2592000000ULL   /**< BCG: 30 days */
#define TRAINED_BETA_GLUCAN_HALF_LIFE     172800000ULL    /**< Beta-glucan: 2 days */
#define TRAINED_LPS_HALF_LIFE             345600000ULL    /**< LPS: 4 days */
#define TRAINED_OXLDL_HALF_LIFE           864000000ULL    /**< oxLDL: 10 days */

/* PRR sensitivity enhancement */
#define TRAINED_PRR_BASE_SENSITIVITY      1.0f    /**< Baseline PRR sensitivity */
#define TRAINED_PRR_MAX_SENSITIVITY       2.5f    /**< Max PRR enhancement */

/* Metabolic state thresholds */
#define TRAINED_GLYCOLYSIS_THRESHOLD      0.6f    /**< Threshold for glycolytic state */
#define TRAINED_MTOR_ACTIVATION_THRESHOLD 0.5f    /**< mTOR activation threshold */

/* Cross-protection thresholds */
#define TRAINED_CROSS_PROTECTION_THRESHOLD 0.7f  /**< Min training level for cross-protection */

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Training stimulus types
 *
 * BIOLOGICAL BASIS:
 * Different stimuli produce different training patterns and durations.
 */
typedef enum {
    TRAINED_STIMULUS_NONE = 0,        /**< No training stimulus */
    TRAINED_STIMULUS_BCG,             /**< BCG vaccine (strongest, longest) */
    TRAINED_STIMULUS_BETA_GLUCAN,     /**< Fungal beta-glucan (potent, transient) */
    TRAINED_STIMULUS_LPS_LOW_DOSE,    /**< Low-dose LPS (endotoxin tolerance) */
    TRAINED_STIMULUS_OXIDIZED_LDL,    /**< Oxidized LDL (chronic training) */
    TRAINED_STIMULUS_COUNT
} training_stimulus_t;

/**
 * @brief Metabolic state of innate immune cells
 *
 * BIOLOGICAL BASIS:
 * Trained immunity is associated with metabolic reprogramming to glycolysis.
 */
typedef enum {
    METABOLIC_STATE_OXIDATIVE = 0,    /**< Oxidative phosphorylation (resting) */
    METABOLIC_STATE_MIXED,            /**< Mixed metabolism */
    METABOLIC_STATE_GLYCOLYTIC        /**< Aerobic glycolysis (trained) */
} metabolic_state_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Training history entry
 *
 * Records a single training event for decay calculation.
 */
typedef struct {
    training_stimulus_t stimulus_type;   /**< Type of training stimulus */
    float intensity;                     /**< Training intensity (0-1) */
    uint64_t training_time;              /**< When training occurred (ms) */
    float current_enhancement;           /**< Current enhancement factor */
    bool active;                         /**< Whether still active */
} training_history_entry_t;

/**
 * @brief Epigenetic modifications (simulated)
 *
 * Models histone modifications and chromatin state.
 */
typedef struct {
    float h3k4me3_level;                 /**< H3K4me3 (active promoter mark) */
    float h3k27ac_level;                 /**< H3K27ac (active enhancer mark) */
    float chromatin_openness;            /**< Chromatin accessibility (0-1) */
    float transcriptional_readiness;     /**< Readiness to transcribe (0-1) */
} epigenetic_state_t;

/**
 * @brief Metabolic reprogramming state
 *
 * Tracks metabolic shift to glycolysis.
 */
typedef struct {
    metabolic_state_t state;             /**< Current metabolic state */
    float glycolysis_rate;               /**< Glycolytic flux (0-1) */
    float oxidative_phosphorylation;     /**< OXPHOS rate (0-1) */
    float mtor_activation;               /**< mTOR pathway activation (0-1) */
    float hif1a_level;                   /**< HIF-1α stabilization (0-1) */
    float glucose_consumption;           /**< Glucose uptake rate (0-1) */
    float atp_production_rate;           /**< ATP production capacity (0-1) */
} metabolic_reprogramming_t;

/**
 * @brief Pattern recognition receptor sensitivity
 *
 * Enhanced PRR expression and sensitivity.
 */
typedef struct {
    float tlr_expression;                /**< TLR expression level (1-2.5x) */
    float nod_sensitivity;               /**< NOD-like receptor sensitivity */
    float recognition_speed;             /**< Recognition time reduction (0-1) */
    float pamp_sensitivity;              /**< PAMP detection sensitivity */
    float damp_sensitivity;              /**< DAMP detection sensitivity */
} prr_sensitivity_t;

/**
 * @brief Trained immunity configuration
 */
typedef struct {
    /* Training parameters */
    float max_training_intensity;        /**< Max training intensity */
    float decay_rate_multiplier;         /**< Decay rate adjustment */
    bool enable_cross_protection;        /**< Enable cross-protection */

    /* Thresholds */
    float min_training_threshold;        /**< Min intensity for training */
    float prr_sensitivity_multiplier;    /**< PRR enhancement multiplier */

    /* Integration */
    bool enable_bio_async;               /**< Enable bio-async messaging */
    bool enable_logging;                 /**< Enable logging */
} trained_immunity_config_t;

/**
 * @brief Trained immunity system state
 */
typedef struct {
    trained_immunity_config_t config;    /**< Configuration */

    /* Connected immune system */
    brain_immune_system_t* immune_system; /**< Brain immune system */

    /* Training state */
    training_history_entry_t training_history[TRAINED_IMMUNITY_MAX_HISTORY];
    size_t training_count;               /**< Number of training events */
    uint64_t last_training_time;         /**< Most recent training */

    /* Epigenetic state */
    epigenetic_state_t epigenetic;       /**< Epigenetic modifications */

    /* Metabolic state */
    metabolic_reprogramming_t metabolic; /**< Metabolic reprogramming */

    /* PRR state */
    prr_sensitivity_t prr;               /**< PRR sensitivity */

    /* Computed enhancement */
    float current_enhancement_factor;    /**< Overall enhancement (1.0-3.0) */
    float prr_sensitivity_factor;        /**< PRR sensitivity (1.0-2.5) */

    /* Cross-protection */
    bool cross_protection_active;        /**< Whether cross-protection enabled */
    float cross_protection_level;        /**< Cross-protection strength (0-1) */

    /* Temporal tracking */
    uint64_t last_decay_check;           /**< Last decay update (ms) */
    uint64_t system_start_time;          /**< System start time */

    /* Statistics */
    uint64_t total_training_events;      /**< Total training count */
    uint64_t cross_protection_activations; /**< Times cross-protection triggered */

    /* Thread safety */
    void* mutex;                         /**< Platform mutex */
} trained_immunity_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with biological defaults
 * HOW:  Return struct with biologically-inspired parameters
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int trained_immunity_default_config(trained_immunity_config_t* config);

/**
 * @brief Create trained immunity system
 *
 * WHAT: Initialize trained immunity tracking
 * WHY:  Enable innate immune memory
 * HOW:  Allocate state, initialize metabolic/epigenetic baselines
 *
 * @param config Configuration (NULL for defaults)
 * @param immune_system Brain immune system to integrate with
 * @return New trained immunity system or NULL on failure
 */
trained_immunity_t* trained_immunity_create(
    const trained_immunity_config_t* config,
    brain_immune_system_t* immune_system
);

/**
 * @brief Destroy trained immunity system
 *
 * WHAT: Clean up trained immunity resources
 * WHY:  Proper resource deallocation
 * HOW:  Free state, release mutex
 *
 * @param system System to destroy
 */
void trained_immunity_destroy(trained_immunity_t* system);

/* ============================================================================
 * Training API
 * ============================================================================ */

/**
 * @brief Apply training stimulus
 *
 * WHAT: Train innate immune cells with stimulus
 * WHY:  Induce epigenetic and metabolic reprogramming
 * HOW:  Record training event, update epigenetic/metabolic state
 *
 * @param system Trained immunity system
 * @param stimulus_type Type of training stimulus
 * @param intensity Stimulus intensity (0-1)
 * @return 0 on success, -1 on error
 */
int trained_immunity_train(
    trained_immunity_t* system,
    training_stimulus_t stimulus_type,
    float intensity
);

/**
 * @brief Get current enhancement factor
 *
 * WHAT: Get overall immune response enhancement
 * WHY:  Determine magnitude of trained immunity effect
 * HOW:  Combine all active training effects
 *
 * @param system Trained immunity system
 * @return Enhancement factor (1.0 = baseline, 3.0 = max)
 */
float trained_immunity_get_enhancement_factor(const trained_immunity_t* system);

/**
 * @brief Get PRR sensitivity factor
 *
 * WHAT: Get pattern recognition receptor sensitivity
 * WHY:  Determine faster threat recognition
 * HOW:  Return computed PRR sensitivity multiplier
 *
 * @param system Trained immunity system
 * @return PRR sensitivity (1.0 = baseline, 2.5 = max)
 */
float trained_immunity_get_prr_sensitivity(const trained_immunity_t* system);

/**
 * @brief Apply time-based decay
 *
 * WHAT: Gradually reduce trained immunity over time
 * WHY:  Biologically realistic decay of training effects
 * HOW:  Exponential decay based on half-life
 *
 * @param system Trained immunity system
 * @param current_time Current timestamp (ms)
 * @return 0 on success
 */
int trained_immunity_decay(
    trained_immunity_t* system,
    uint64_t current_time
);

/**
 * @brief Check cross-protection for antigen
 *
 * WHAT: Determine if trained immunity provides cross-protection
 * WHY:  Heterologous protection from unrelated threats
 * HOW:  Check training level and cross-protection threshold
 *
 * @param system Trained immunity system
 * @param antigen Antigen to check
 * @return true if cross-protection active for this antigen
 */
bool trained_immunity_check_cross_protection(
    const trained_immunity_t* system,
    const brain_antigen_t* antigen
);

/**
 * @brief Get current metabolic state
 *
 * WHAT: Get metabolic reprogramming state
 * WHY:  Monitor glycolytic shift
 * HOW:  Return current metabolic state enum
 *
 * @param system Trained immunity system
 * @return Current metabolic state
 */
metabolic_state_t trained_immunity_get_metabolic_state(
    const trained_immunity_t* system
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get epigenetic state
 *
 * @param system Trained immunity system
 * @param state Output epigenetic state
 * @return 0 on success
 */
int trained_immunity_get_epigenetic_state(
    const trained_immunity_t* system,
    epigenetic_state_t* state
);

/**
 * @brief Get metabolic reprogramming state
 *
 * @param system Trained immunity system
 * @param state Output metabolic state
 * @return 0 on success
 */
int trained_immunity_get_metabolic_reprogramming(
    const trained_immunity_t* system,
    metabolic_reprogramming_t* state
);

/**
 * @brief Get PRR sensitivity state
 *
 * @param system Trained immunity system
 * @param state Output PRR state
 * @return 0 on success
 */
int trained_immunity_get_prr_state(
    const trained_immunity_t* system,
    prr_sensitivity_t* state
);

/**
 * @brief Check if training is active
 *
 * @param system Trained immunity system
 * @return true if any training effects are active
 */
bool trained_immunity_is_active(const trained_immunity_t* system);

/**
 * @brief Get training history count
 *
 * @param system Trained immunity system
 * @return Number of training events in history
 */
size_t trained_immunity_get_history_count(const trained_immunity_t* system);

/**
 * @brief Get time since last training
 *
 * @param system Trained immunity system
 * @param current_time Current timestamp (ms)
 * @return Milliseconds since last training
 */
uint64_t trained_immunity_time_since_training(
    const trained_immunity_t* system,
    uint64_t current_time
);

/* ============================================================================
 * String Conversion Utilities
 * ============================================================================ */

const char* trained_immunity_stimulus_to_string(training_stimulus_t stimulus);
const char* trained_immunity_metabolic_state_to_string(metabolic_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_TRAINED_IMMUNITY_H */
