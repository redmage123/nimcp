//=============================================================================
// nimcp_white_matter_tracts.h - White Matter Tract System
//=============================================================================
/**
 * @file nimcp_white_matter_tracts.h
 * @brief White matter tract modeling for inter-region signal conduction
 *
 * WHAT: Models major white matter tracts with myelination-dependent conduction
 * WHY:  Real brains route signals through myelinated fiber bundles with
 *       velocity-dependent delays. Demyelination (e.g., MS) degrades cognition.
 *       Tract integrity modulates inter-region communication bandwidth.
 * HOW:  Per-tract state with conduction velocity, myelination level, integrity,
 *       and signal delay. Pink noise jitter on velocity for biological realism.
 *
 * BIOLOGICAL BASIS:
 * - Corpus callosum: 200M+ axons connecting hemispheres (interhemispheric transfer)
 * - Arcuate fasciculus: Broca <-> Wernicke (language production/comprehension)
 * - Uncinate fasciculus: Temporal <-> Frontal (emotion-cognition integration)
 * - Cingulum: Cingulate bundle (attention, memory, emotional regulation)
 * - IFOF: Inferior fronto-occipital fasciculus (visual-semantic processing)
 * - Corticospinal tract: Motor cortex -> spinal cord (voluntary movement)
 * - Spinothalamic tract: Spinal cord -> thalamus (pain, temperature)
 * - Optic radiation: LGN -> V1 (visual information relay)
 *
 * CONDUCTION VELOCITY:
 * - Unmyelinated C fibers: 0.5-2 m/s
 * - Thinly myelinated A-delta: 5-30 m/s
 * - Heavily myelinated A-alpha: 80-120 m/s
 * - Myelination level directly modulates velocity via saltatory conduction
 *
 * INTEGRATION POINTS:
 * - Hemispheric brain: Corpus callosum for interhemispheric transfer
 * - Language system: Arcuate fasciculus for Broca-Wernicke loop
 * - Emotional system: Uncinate fasciculus for emotion-cognition coupling
 * - Visual cortex: Optic radiation for V1 input
 * - Thalamus: Spinothalamic for sensory relay
 * - Motor system: Corticospinal for motor output
 * - Sleep system: Myelination repair during sleep
 * - Immune system: Demyelination from neuroinflammation
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 * @date 2026-03-05
 */

#ifndef NIMCP_WHITE_MATTER_TRACTS_H
#define NIMCP_WHITE_MATTER_TRACTS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Magic number for struct validation */
#define WMT_MAGIC 0x574D5431  /* "WMT1" */

/** Default tract lengths in meters (approximate neuroanatomical values) */
#define WMT_LENGTH_CORPUS_CALLOSUM     0.10f   /* ~10 cm */
#define WMT_LENGTH_ARCUATE_FASCICULUS  0.08f   /* ~8 cm */
#define WMT_LENGTH_UNCINATE_FASCICULUS 0.05f   /* ~5 cm */
#define WMT_LENGTH_CINGULUM            0.12f   /* ~12 cm */
#define WMT_LENGTH_IFOF                0.15f   /* ~15 cm */
#define WMT_LENGTH_CORTICOSPINAL       0.45f   /* ~45 cm (cortex to brainstem) */
#define WMT_LENGTH_SPINOTHALAMIC       0.40f   /* ~40 cm (spinal to thalamus) */
#define WMT_LENGTH_OPTIC_RADIATION     0.07f   /* ~7 cm (LGN to V1) */

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief White matter tract types
 *
 * WHAT: Enumeration of major white matter fiber bundles
 * WHY:  Each tract has distinct anatomical properties and functions
 */
typedef enum {
    WMT_CORPUS_CALLOSUM,       /**< Interhemispheric commissure */
    WMT_ARCUATE_FASCICULUS,    /**< Broca <-> Wernicke (language) */
    WMT_UNCINATE_FASCICULUS,   /**< Temporal <-> Frontal (emotion-cognition) */
    WMT_CINGULUM,              /**< Cingulate bundle (attention, memory) */
    WMT_IFOF,                  /**< Inferior fronto-occipital (visual-semantic) */
    WMT_CORTICOSPINAL,         /**< Motor cortex -> spinal cord */
    WMT_SPINOTHALAMIC,         /**< Spinal cord -> thalamus (pain, temperature) */
    WMT_OPTIC_RADIATION,       /**< LGN -> V1 (visual) */
    WMT_COUNT                  /**< Total number of tract types */
} white_matter_tract_t;

//=============================================================================
// Data Structures
//=============================================================================

/**
 * @brief Per-tract state
 *
 * WHAT: Runtime state for a single white matter tract
 * WHY:  Tracks conduction properties that change with myelination and damage
 */
typedef struct {
    white_matter_tract_t type;          /**< Tract identity */
    float conduction_velocity_ms;       /**< Current conduction velocity (m/s), range 1-120 */
    float myelination_level;            /**< Myelination degree [0.0-1.0] */
    float integrity;                    /**< Structural integrity [0.0-1.0] */
    float signal_delay_ms;              /**< Computed delay = (length / velocity) * 1000 */
    float tract_length_m;               /**< Anatomical length in meters */
    uint32_t source_region;             /**< Source brain region ID */
    uint32_t target_region;             /**< Target brain region ID */
    float bandwidth;                    /**< Signal bandwidth capacity [0.0-1.0] */
    bool bidirectional;                 /**< Whether tract supports bidirectional signaling */
} tract_state_t;

/**
 * @brief White matter system configuration
 *
 * WHAT: Configuration parameters for the white matter tract system
 * WHY:  Allows tuning of biological parameters for different simulation fidelity
 */
typedef struct {
    float base_myelination;             /**< Initial myelination level [0.0-1.0], default 0.7 */
    float base_integrity;               /**< Initial integrity [0.0-1.0], default 1.0 */
    float velocity_noise_amplitude;     /**< Pink noise amplitude for velocity jitter, default 0.02 */
    float velocity_noise_alpha;         /**< Pink noise spectral exponent, default 1.0 */
    float demyelination_rate;           /**< Rate of myelination decay per second, default 0.001 */
    float remyelination_rate;           /**< Rate of myelination repair per second, default 0.005 */
    float min_conduction_velocity;      /**< Minimum velocity (m/s), default 1.0 */
    float max_conduction_velocity;      /**< Maximum velocity (m/s), default 120.0 */
    float fractal_dimension;            /**< Fractal dimension of tract branching, default 1.3 */
    bool enable_velocity_jitter;        /**< Enable pink noise on conduction velocity */
    bool enable_integrity_decay;        /**< Enable passive integrity decay */
    uint32_t pink_noise_seed;           /**< RNG seed for pink noise (0 = time-based) */
} wmt_config_t;

/**
 * @brief White matter system statistics
 *
 * WHAT: Runtime statistics for monitoring tract health
 */
typedef struct {
    float mean_myelination;             /**< Average myelination across all tracts */
    float mean_integrity;               /**< Average integrity across all tracts */
    float mean_conduction_velocity;     /**< Average conduction velocity (m/s) */
    float mean_signal_delay_ms;         /**< Average signal delay (ms) */
    uint32_t demyelinated_tract_count;  /**< Number of tracts with myelination < 0.3 */
    uint32_t degraded_tract_count;      /**< Number of tracts with integrity < 0.5 */
    uint64_t total_signals_routed;      /**< Cumulative signals routed through tracts */
    uint64_t total_updates;             /**< Total update cycles */
} wmt_stats_t;

/**
 * @brief Opaque white matter tract system handle
 *
 * WHAT: Manages all white matter tracts and their dynamics
 * WHY:  Encapsulates internal state (mutex, pink noise generator, timing)
 */
typedef struct wmt_system wmt_system_t;

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Get default white matter configuration
 *
 * WHAT: Returns a configuration with biologically plausible defaults
 * WHY:  Provides sensible starting point for most simulations
 *
 * @return Default configuration struct (by value)
 */
wmt_config_t wmt_default_config(void);

/**
 * @brief Create a white matter tract system
 *
 * WHAT: Allocates and initializes the tract system with all 8 tracts
 * WHY:  Entry point for white matter simulation
 * HOW:  Allocates system, initializes tracts with anatomical defaults,
 *       creates pink noise generator for velocity jitter
 *
 * @param config Configuration (NULL uses defaults)
 * @return System handle, or NULL on allocation failure
 */
wmt_system_t* wmt_create(const wmt_config_t* config);

/**
 * @brief Destroy a white matter tract system
 *
 * WHAT: Frees all resources associated with the system
 * WHY:  Proper resource cleanup
 *
 * @param system System to destroy (NULL-safe)
 */
void wmt_destroy(wmt_system_t* system);

//=============================================================================
// Update API
//=============================================================================

/**
 * @brief Update all tracts for one simulation timestep
 *
 * WHAT: Advances tract dynamics (myelination decay, velocity jitter, delay recomputation)
 * WHY:  Tracts are dynamic structures that change over time
 * HOW:  Applies pink noise to velocity, optional integrity decay, recomputes delays
 *
 * @param system The tract system
 * @param dt_s Timestep in seconds
 * @return 0 on success, -1 on error
 */
int wmt_update(wmt_system_t* system, float dt_s);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Get signal delay for a specific tract
 *
 * @param system The tract system
 * @param tract Tract type to query
 * @return Delay in milliseconds, or -1.0f on error
 */
float wmt_get_tract_delay(const wmt_system_t* system, white_matter_tract_t tract);

/**
 * @brief Get integrity level for a specific tract
 *
 * @param system The tract system
 * @param tract Tract type to query
 * @return Integrity [0.0-1.0], or -1.0f on error
 */
float wmt_get_tract_integrity(const wmt_system_t* system, white_matter_tract_t tract);

/**
 * @brief Get myelination level for a specific tract
 *
 * @param system The tract system
 * @param tract Tract type to query
 * @return Myelination level [0.0-1.0], or -1.0f on error
 */
float wmt_get_tract_myelination(const wmt_system_t* system, white_matter_tract_t tract);

/**
 * @brief Get conduction velocity for a specific tract
 *
 * @param system The tract system
 * @param tract Tract type to query
 * @return Velocity in m/s, or -1.0f on error
 */
float wmt_get_tract_velocity(const wmt_system_t* system, white_matter_tract_t tract);

/**
 * @brief Get a copy of the full tract state
 *
 * @param system The tract system
 * @param tract Tract type to query
 * @param out_state Output state (caller-allocated)
 * @return 0 on success, -1 on error
 */
int wmt_get_tract_state(const wmt_system_t* system, white_matter_tract_t tract,
                        tract_state_t* out_state);

/**
 * @brief Get system-wide statistics
 *
 * @param system The tract system
 * @param out_stats Output statistics (caller-allocated)
 * @return 0 on success, -1 on error
 */
int wmt_get_stats(const wmt_system_t* system, wmt_stats_t* out_stats);

//=============================================================================
// Modulation API
//=============================================================================

/**
 * @brief Modulate myelination level for a specific tract
 *
 * WHAT: Adjusts myelination (e.g., from sleep repair or immune damage)
 * WHY:  Myelination directly affects conduction velocity
 * HOW:  Applies delta clamped to [0.0, 1.0], recomputes velocity and delay
 *
 * @param system The tract system
 * @param tract Tract type to modulate
 * @param delta_myelination Change in myelination (positive = repair, negative = damage)
 * @return 0 on success, -1 on error
 */
int wmt_modulate_myelination(wmt_system_t* system, white_matter_tract_t tract,
                             float delta_myelination);

/**
 * @brief Modulate integrity for a specific tract
 *
 * @param system The tract system
 * @param tract Tract type to modulate
 * @param delta_integrity Change in integrity (positive = repair, negative = damage)
 * @return 0 on success, -1 on error
 */
int wmt_modulate_integrity(wmt_system_t* system, white_matter_tract_t tract,
                           float delta_integrity);

//=============================================================================
// Signal Routing API
//=============================================================================

/**
 * @brief Route a signal through a white matter tract
 *
 * WHAT: Applies conduction delay and bandwidth attenuation to a signal
 * WHY:  Signals traveling through tracts experience velocity-dependent delay
 *       and integrity-dependent attenuation
 * HOW:  Computes effective signal strength = amplitude * integrity * bandwidth
 *       Returns the tract's current signal delay
 *
 * @param system The tract system
 * @param tract Tract type to route through
 * @param signal_amplitude Input signal amplitude
 * @param out_attenuated_amplitude Output: signal after attenuation (caller-allocated)
 * @param out_delay_ms Output: delay in milliseconds (caller-allocated)
 * @return 0 on success, -1 on error
 */
int wmt_route_signal(wmt_system_t* system, white_matter_tract_t tract,
                     float signal_amplitude, float* out_attenuated_amplitude,
                     float* out_delay_ms);

//=============================================================================
// Utility
//=============================================================================

/**
 * @brief Get human-readable name for a tract type
 *
 * @param tract Tract type
 * @return Static string name, or "UNKNOWN" for invalid types
 */
const char* wmt_tract_name(white_matter_tract_t tract);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_WHITE_MATTER_TRACTS_H */
