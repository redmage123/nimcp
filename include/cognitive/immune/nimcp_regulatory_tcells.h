/**
 * @file nimcp_regulatory_tcells.h
 * @brief Regulatory T Cells - Cytokine Storm Prevention for Brain Immune System
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Regulatory T cell (Treg) system that suppresses excessive inflammation
 *       and prevents cytokine storms in the brain immune system.
 * WHY:  Biological immune systems require negative feedback to prevent
 *       autoimmune damage and cytokine storms. Without regulation, inflammation
 *       can spiral out of control, damaging healthy tissue.
 * HOW:  Implements Treg surveillance, checkpoint mechanisms (PD-1/PD-L1, CTLA-4),
 *       and suppressive cytokine production (IL-10, TGF-β) to modulate
 *       inflammation levels and prevent immune overactivation.
 *
 * BIOLOGICAL BASIS:
 * ```
 * BIOLOGICAL CONCEPT              NIMCP IMPLEMENTATION
 * ─────────────────────────────────────────────────────────────────
 * Regulatory T cells (Tregs)   → Inflammation monitoring + suppression
 * IL-10 production             → Anti-inflammatory cytokine release
 * TGF-β production             → Long-term suppression, memory suppression
 * PD-1/PD-L1 checkpoint        → T cell exhaustion/inhibition pathway
 * CTLA-4 checkpoint            → B7 competition, activation inhibition
 * Negative feedback loop       → Dynamic suppression factor computation
 * Immune tolerance             → Prevent autoimmune responses
 * Storm prevention             → Emergency Treg activation at critical levels
 * ```
 *
 * REGULATORY MECHANISMS:
 * 1. **Surveillance**: Continuously monitor inflammation levels across regions
 * 2. **Threshold Detection**: Activate when inflammation exceeds safe levels
 * 3. **Cytokine Suppression**: Release IL-10/TGF-β to counter pro-inflammatory
 * 4. **Checkpoint Activation**: PD-1/PD-L1 and CTLA-4 pathways inhibit T/B cells
 * 5. **Negative Feedback**: Proportional suppression based on inflammation level
 * 6. **Storm Prevention**: Emergency activation at INFLAMMATION_STORM threshold
 *
 * SUPPRESSION DYNAMICS:
 * ```
 * Inflammation Level    Treg Response         Suppression Factor
 * ─────────────────────────────────────────────────────────────────
 * NONE                  Inactive              0.0
 * LOCAL                 Monitor only          0.1
 * REGIONAL              Mild suppression      0.3
 * SYSTEMIC              Strong suppression    0.6
 * STORM                 Emergency activation  1.0 (full suppression)
 * ```
 *
 * CHECKPOINT PATHWAYS:
 * - **PD-1/PD-L1**: Programmed Death pathway - induces T cell exhaustion
 *   - Reduces T cell activation and proliferation
 *   - Prevents excessive killer T cell responses
 *   - Duration-based decay model
 *
 * - **CTLA-4**: Cytotoxic T-Lymphocyte-Associated protein 4
 *   - Competes with CD28 for B7 binding on APCs
 *   - Reduces T cell co-stimulation
 *   - Prevents T helper cell hyperactivation
 *
 * INTEGRATION:
 * - Connects to brain_immune_system_t via callbacks
 * - Monitors inflammation sites in real-time
 * - Releases suppressive cytokines via immune system
 * - Modulates T cell and B cell activation thresholds
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

#ifndef NIMCP_REGULATORY_TCELLS_H
#define NIMCP_REGULATORY_TCELLS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Core brain immune system integration */
#include "cognitive/immune/nimcp_brain_immune.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/logging/nimcp_logging.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define TREG_MAX_CHECKPOINTS           64    /**< Max active checkpoints */
#define TREG_MAX_SUPPRESSIVE_CYTOKINES 128   /**< Max suppressive cytokines */
#define TREG_IL10_PRODUCTION_RATE      0.8f  /**< IL-10 production rate */
#define TREG_TGFB_PRODUCTION_RATE      0.6f  /**< TGF-β production rate */
#define TREG_ACTIVATION_THRESHOLD      0.4f  /**< Inflammation threshold for activation */
#define TREG_STORM_THRESHOLD           0.9f  /**< Emergency activation threshold */
#define TREG_CHECKPOINT_DURATION_MS    5000  /**< Default checkpoint duration */
#define TREG_SUPPRESSION_DECAY_RATE    0.1f  /**< Suppression decay per second */

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Regulatory T cell activation state
 *
 * BIOLOGICAL BASIS:
 * Tregs transition from naive → surveillance → active → suppressing
 * based on inflammation levels.
 */
typedef enum {
    TREG_STATE_NAIVE = 0,      /**< Unactivated, no suppression */
    TREG_STATE_SURVEILLANCE,   /**< Monitoring inflammation */
    TREG_STATE_ACTIVE,         /**< Activated, producing cytokines */
    TREG_STATE_SUPPRESSING,    /**< Actively suppressing immune response */
    TREG_STATE_EXHAUSTED       /**< Over-activation, reduced function */
} treg_state_t;

/**
 * @brief Immune checkpoint type
 *
 * BIOLOGICAL BASIS:
 * PD-1/PD-L1 and CTLA-4 are major inhibitory checkpoint pathways.
 */
typedef enum {
    CHECKPOINT_PD1_PDL1 = 0,   /**< Programmed Death pathway */
    CHECKPOINT_CTLA4,          /**< CTLA-4 co-stimulation inhibitor */
    CHECKPOINT_LAG3,           /**< LAG-3 (Lymphocyte Activation Gene 3) */
    CHECKPOINT_TIM3            /**< TIM-3 (T cell Immunoglobulin Mucin 3) */
} treg_checkpoint_type_t;

/**
 * @brief Suppressive cytokine type
 *
 * BIOLOGICAL BASIS:
 * IL-10 and TGF-β are primary Treg suppressive molecules.
 */
typedef enum {
    TREG_CYTOKINE_IL10 = 0,    /**< Interleukin-10 (rapid suppression) */
    TREG_CYTOKINE_TGFB,        /**< TGF-β (long-term suppression) */
    TREG_CYTOKINE_IL35         /**< Interleukin-35 (regulatory, newer) */
} treg_cytokine_type_t;

/* ============================================================================
 * Core Structures
 * ============================================================================ */

/**
 * @brief Immune checkpoint - inhibitory signal
 *
 * WHAT: Represents an active checkpoint pathway inhibiting T/B cells
 * WHY:  Models PD-1/PD-L1 and CTLA-4 pathways that prevent overactivation
 * HOW:  Tracks checkpoint type, strength, duration, and target cells
 */
typedef struct {
    uint32_t id;                       /**< Checkpoint ID */
    treg_checkpoint_type_t type;       /**< Checkpoint pathway type */
    uint32_t target_cell_id;           /**< Target T/B cell ID */
    float inhibition_strength;         /**< Inhibition factor (0-1) */
    uint64_t activation_time;          /**< When checkpoint activated */
    uint64_t duration_ms;              /**< How long it lasts */
    bool active;                       /**< Currently active */
} treg_checkpoint_t;

/**
 * @brief Suppressive cytokine release
 *
 * WHAT: IL-10 or TGF-β release to suppress inflammation
 * WHY:  Models Treg suppressive mechanism
 * HOW:  Tracks cytokine type, concentration, target region
 */
typedef struct {
    uint32_t id;                       /**< Cytokine release ID */
    treg_cytokine_type_t type;         /**< IL-10, TGF-β, etc. */
    float concentration;               /**< Concentration (0-1) */
    uint32_t target_region;            /**< Target region (0=broadcast) */
    uint64_t release_time;             /**< When released */
    brain_cytokine_type_t mapped_type; /**< Mapped to brain immune cytokine */
} treg_suppressive_cytokine_t;

/**
 * @brief Regulatory T cell system configuration
 */
typedef struct {
    /* Activation thresholds */
    float activation_threshold;        /**< Inflammation level to activate (0-1) */
    float storm_threshold;             /**< Emergency activation (0-1) */
    float exhaustion_threshold;        /**< Over-activation threshold */

    /* Suppression parameters */
    float il10_production_rate;        /**< IL-10 production rate */
    float tgfb_production_rate;        /**< TGF-β production rate */
    float suppression_decay_rate;      /**< Suppression decay per second */
    float max_suppression_factor;      /**< Maximum suppression (0-1) */

    /* Checkpoint parameters */
    uint64_t checkpoint_duration_ms;   /**< Default checkpoint duration */
    float pd1_inhibition_strength;     /**< PD-1/PD-L1 strength */
    float ctla4_inhibition_strength;   /**< CTLA-4 strength */

    /* Population limits */
    size_t max_checkpoints;            /**< Max active checkpoints */
    size_t max_cytokines;              /**< Max suppressive cytokines */

    /* Update timing */
    uint64_t update_interval_ms;       /**< How often to update */

    /* Feature flags */
    bool enable_pd1_pathway;           /**< Enable PD-1/PD-L1 */
    bool enable_ctla4_pathway;         /**< Enable CTLA-4 */
    bool enable_il10_production;       /**< Enable IL-10 */
    bool enable_tgfb_production;       /**< Enable TGF-β */
    bool enable_auto_activation;       /**< Auto-activate on threshold */
    bool enable_logging;               /**< Enable logging */
} treg_config_t;

/**
 * @brief Regulatory T cell system statistics
 */
typedef struct {
    /* Activation counts */
    uint64_t activations;              /**< Total Treg activations */
    uint64_t storm_preventions;        /**< Cytokine storms prevented */
    uint64_t checkpoints_activated;    /**< Checkpoints activated */
    uint64_t cytokines_released;       /**< Suppressive cytokines released */

    /* Current state */
    uint32_t active_checkpoints;       /**< Current active checkpoints */
    uint32_t active_cytokines;         /**< Current suppressive cytokines */
    float current_suppression_factor;  /**< Current suppression (0-1) */
    float max_inflammation_observed;   /**< Peak inflammation seen */

    /* Effectiveness */
    float avg_suppression_factor;      /**< Average suppression */
    uint64_t inflammation_reductions;  /**< Successful reductions */
    uint64_t cytokines_damped;         /**< Pro-inflammatory cytokines that
                                          had their concentration reduced
                                          via Treg suppression */
} treg_stats_t;

/**
 * @brief Forward declaration
 */
typedef struct treg_system treg_system_t;

/**
 * @brief Callback for Treg activation
 */
typedef void (*treg_activation_cb_t)(
    treg_system_t* system,
    brain_inflammation_level_t trigger_level,
    void* user_data
);

/**
 * @brief Callback for checkpoint activation
 */
typedef void (*treg_checkpoint_cb_t)(
    treg_system_t* system,
    const treg_checkpoint_t* checkpoint,
    void* user_data
);

/**
 * @brief Callback for suppressive cytokine release
 */
typedef void (*treg_cytokine_cb_t)(
    treg_system_t* system,
    const treg_suppressive_cytokine_t* cytokine,
    void* user_data
);

/**
 * @brief Regulatory T cell system state
 */
struct treg_system {
    treg_config_t config;              /**< Configuration */
    treg_state_t state;                /**< Current Treg state */

    /* Brain immune system integration */
    brain_immune_system_t* immune_system; /**< Connected brain immune */

    /* Checkpoints */
    treg_checkpoint_t* checkpoints;
    size_t checkpoint_count;
    size_t checkpoint_capacity;
    uint32_t next_checkpoint_id;

    /* Suppressive cytokines */
    treg_suppressive_cytokine_t* cytokines;
    size_t cytokine_count;
    size_t cytokine_capacity;
    uint32_t next_cytokine_id;

    /* Suppression state */
    float current_suppression_factor;  /**< Current suppression (0-1) */
    float inflammation_history[10];    /**< Recent inflammation levels */
    size_t history_index;              /**< Circular buffer index */

    /* Callbacks */
    treg_activation_cb_t on_activation;
    treg_checkpoint_cb_t on_checkpoint;
    treg_cytokine_cb_t on_cytokine;
    void* callback_user_data;          /**< Legacy/shared (deprecated) */
    void* activation_user_data;        /**< Per-callback user_data for activation */
    void* checkpoint_user_data;        /**< Per-callback user_data for checkpoint */
    void* cytokine_user_data;          /**< Per-callback user_data for cytokine */

    /* Statistics */
    treg_stats_t stats;

    /* Thread safety */
    nimcp_mutex_t* mutex;

    /* State */
    bool active;                       /**< System is active */
    uint64_t last_update_time;         /**< Last update timestamp */
};

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default Treg configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with biologically-plausible parameters
 * HOW:  Return struct with balanced threshold and production rates
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int treg_default_config(treg_config_t* config);

/**
 * @brief Create regulatory T cell system
 *
 * WHAT: Initialize Treg system for cytokine storm prevention
 * WHY:  Set up negative feedback mechanism for immune regulation
 * HOW:  Allocate pools, initialize state, connect to brain immune
 *
 * @param config Configuration (NULL for defaults)
 * @param immune_system Brain immune system to regulate
 * @return New Treg system or NULL on failure
 */
treg_system_t* treg_create(
    const treg_config_t* config,
    brain_immune_system_t* immune_system
);

/**
 * @brief Destroy regulatory T cell system
 *
 * WHAT: Clean up Treg system resources
 * WHY:  Proper resource deallocation
 * HOW:  Free pools, destroy mutex
 *
 * @param system System to destroy
 */
void treg_destroy(treg_system_t* system);

/* ============================================================================
 * Regulation API
 * ============================================================================ */

/**
 * @brief Update regulatory T cell system
 *
 * WHAT: Check inflammation and activate regulation if needed
 * WHY:  Main regulation loop - monitors and responds to inflammation
 * HOW:  Query brain immune stats, activate if threshold exceeded,
 *       update checkpoints and cytokine decay
 *
 * @param system Treg system
 * @param delta_ms Time since last update
 * @return 0 on success
 */
int treg_update(treg_system_t* system, uint64_t delta_ms);

/**
 * @brief Suppress inflammation
 *
 * WHAT: Actively suppress inflammation at a site
 * WHY:  Direct intervention to reduce immune overactivation
 * HOW:  Release IL-10/TGF-β, activate checkpoints, compute suppression
 *
 * @param system Treg system
 * @param site_id Inflammation site to suppress
 * @return 0 on success
 */
int treg_suppress_inflammation(treg_system_t* system, uint32_t site_id);

/**
 * @brief Get current suppression factor
 *
 * WHAT: Query how much Tregs are suppressing inflammation
 * WHY:  Other modules can adjust based on suppression level
 * HOW:  Return current suppression factor (0-1)
 *
 * @param system Treg system
 * @return Suppression factor (0=none, 1=full suppression)
 */
float treg_get_suppression_factor(const treg_system_t* system);

/* ============================================================================
 * Checkpoint API
 * ============================================================================ */

/**
 * @brief Activate immune checkpoint
 *
 * WHAT: Enable checkpoint pathway to inhibit T/B cell
 * WHY:  Models PD-1/PD-L1 or CTLA-4 inhibition
 * HOW:  Create checkpoint, apply to target cell, track duration
 *
 * @param system Treg system
 * @param type Checkpoint type (PD-1, CTLA-4, etc.)
 * @param target_cell_id Target T or B cell ID
 * @param duration_ms How long checkpoint lasts (0=use default)
 * @param checkpoint_id Output: checkpoint ID
 * @return 0 on success
 */
int treg_checkpoint_activate(
    treg_system_t* system,
    treg_checkpoint_type_t type,
    uint32_t target_cell_id,
    uint64_t duration_ms,
    uint32_t* checkpoint_id
);

/**
 * @brief Release immune checkpoint
 *
 * WHAT: Deactivate checkpoint, restore T/B cell function
 * WHY:  Allow immune response to resume after threat cleared
 * HOW:  Mark checkpoint inactive, remove inhibition
 *
 * @param system Treg system
 * @param checkpoint_id Checkpoint to release
 * @return 0 on success
 */
int treg_checkpoint_release(treg_system_t* system, uint32_t checkpoint_id);

/**
 * @brief Get checkpoint inhibition strength for cell
 *
 * WHAT: Query how much a T/B cell is being inhibited
 * WHY:  Other modules can adjust cell activation based on checkpoint
 * HOW:  Sum all active checkpoints targeting this cell
 *
 * @param system Treg system
 * @param cell_id T or B cell ID
 * @return Total inhibition (0-1, capped at 1.0)
 */
float treg_get_checkpoint_inhibition(
    const treg_system_t* system,
    uint32_t cell_id
);

/* ============================================================================
 * Cytokine API
 * ============================================================================ */

/**
 * @brief Release suppressive cytokine
 *
 * WHAT: Produce IL-10 or TGF-β to suppress inflammation
 * WHY:  Primary Treg suppression mechanism
 * HOW:  Create cytokine, send via brain immune system
 *
 * @param system Treg system
 * @param type Cytokine type (IL-10, TGF-β)
 * @param concentration Concentration (0-1)
 * @param target_region Target region (0=broadcast)
 * @param cytokine_id Output: cytokine ID
 * @return 0 on success
 */
int treg_release_cytokine(
    treg_system_t* system,
    treg_cytokine_type_t type,
    float concentration,
    uint32_t target_region,
    uint32_t* cytokine_id
);

/* ============================================================================
 * Callback Registration
 * ============================================================================ */

/**
 * @brief Set Treg activation callback
 */
int treg_set_activation_callback(
    treg_system_t* system,
    treg_activation_cb_t callback,
    void* user_data
);

/**
 * @brief Set checkpoint activation callback
 */
int treg_set_checkpoint_callback(
    treg_system_t* system,
    treg_checkpoint_cb_t callback,
    void* user_data
);

/**
 * @brief Set suppressive cytokine callback
 */
int treg_set_cytokine_callback(
    treg_system_t* system,
    treg_cytokine_cb_t callback,
    void* user_data
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get Treg system statistics
 *
 * @param system Treg system
 * @param stats Output statistics
 * @return 0 on success
 */
int treg_get_stats(const treg_system_t* system, treg_stats_t* stats);

/**
 * @brief Get current Treg state
 *
 * @param system Treg system
 * @return Current state
 */
treg_state_t treg_get_state(const treg_system_t* system);

/**
 * @brief Check if Treg is active
 *
 * @param system Treg system
 * @return true if actively suppressing
 */
bool treg_is_active(const treg_system_t* system);

/* ============================================================================
 * String Conversion Utilities
 * ============================================================================ */

const char* treg_state_to_string(treg_state_t state);
const char* treg_checkpoint_to_string(treg_checkpoint_type_t type);
const char* treg_cytokine_to_string(treg_cytokine_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_REGULATORY_TCELLS_H */
