/**
 * @file nimcp_trigeminal_oral_bridge.h
 * @brief Trigeminal-Oral Sensory Integration Bridge
 * @version 1.0.0
 * @date 2026-01-12
 *
 * WHAT: Bridge integrating trigeminal somatosensory input with gustatory
 *       processing for complete oral sensation (mouthfeel).
 *
 * WHY: Oral perception is multimodal - taste alone is incomplete:
 *      - Spiciness (capsaicin) activates TRPV1 heat/pain receptors
 *      - Cooling (menthol) activates TRPM8 cold receptors
 *      - Texture affects perceived flavor intensity
 *      - Temperature modulates taste perception (cold suppresses sweet)
 *      - Astringency (tannins) causes oral drying sensation
 *      - Carbonation activates both taste and trigeminal nerves
 *
 * HOW: Combines somatosensory oral signals (touch, temperature, pain) with
 *      gustatory signals to create unified mouthfeel perception, handles
 *      chemesthetic sensations (spicy, cooling), and computes texture-flavor
 *      interactions.
 *
 * BIOLOGICAL BASIS:
 * =================
 * - Trigeminal nerve (CN V) innervates oral cavity
 * - TRPV1 receptors: capsaicin, heat, acid (spicy sensation)
 * - TRPM8 receptors: menthol, cold (cooling sensation)
 * - Mechanoreceptors: texture, pressure, proprioception
 * - Insular cortex integrates trigeminal + gustatory signals
 * - Orbitofrontal cortex computes unified flavor/mouthfeel
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_TRIGEMINAL_ORAL_BRIDGE_H
#define NIMCP_TRIGEMINAL_ORAL_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "core/brain/regions/somatosensory/nimcp_somatosensory.h"
#include "core/brain/regions/gustatory/nimcp_gustatory.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define TRIGEMINAL_MAX_ORAL_REGIONS     16
#define TRIGEMINAL_TEXTURE_DIM          8
#define TRIGEMINAL_MOUTHFEEL_DIM        12
#define TRIGEMINAL_CHEMESTHESIS_TYPES   6

/** Capsaicin heat units for spiciness scaling */
#define TRIGEMINAL_SCOVILLE_SCALE       1000000.0f

/** Temperature thresholds (Celsius) */
#define TRIGEMINAL_COLD_THRESHOLD       15.0f
#define TRIGEMINAL_COOL_THRESHOLD       25.0f
#define TRIGEMINAL_WARM_THRESHOLD       37.0f
#define TRIGEMINAL_HOT_THRESHOLD        45.0f
#define TRIGEMINAL_PAIN_THRESHOLD       52.0f

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Oral region for trigeminal innervation
 */
typedef enum {
    ORAL_REGION_TONGUE_TIP = 0,     /**< Tip of tongue */
    ORAL_REGION_TONGUE_BODY,        /**< Body of tongue */
    ORAL_REGION_TONGUE_BACK,        /**< Back of tongue */
    ORAL_REGION_TONGUE_SIDES,       /**< Lateral tongue */
    ORAL_REGION_PALATE_HARD,        /**< Hard palate */
    ORAL_REGION_PALATE_SOFT,        /**< Soft palate */
    ORAL_REGION_GUMS_UPPER,         /**< Upper gums */
    ORAL_REGION_GUMS_LOWER,         /**< Lower gums */
    ORAL_REGION_INNER_CHEEK,        /**< Buccal mucosa */
    ORAL_REGION_LIPS,               /**< Lips */
    ORAL_REGION_THROAT,             /**< Pharynx */
    ORAL_REGION_COUNT
} oral_region_t;

/**
 * @brief Chemesthetic sensation type (trigeminal chemical sensing)
 */
typedef enum {
    CHEMESTHESIS_NONE = 0,          /**< No chemesthetic sensation */
    CHEMESTHESIS_SPICY_HEAT,        /**< Capsaicin/piperine (TRPV1) */
    CHEMESTHESIS_COOLING,           /**< Menthol/eucalyptol (TRPM8) */
    CHEMESTHESIS_TINGLING,          /**< Szechuan peppercorn (TRPA1) */
    CHEMESTHESIS_ASTRINGENT,        /**< Tannins (drying) */
    CHEMESTHESIS_CARBONATION,       /**< CO2 fizz */
    CHEMESTHESIS_IRRITANT,          /**< Mustard/horseradish (TRPA1) */
    CHEMESTHESIS_COUNT
} chemesthesis_type_t;

/**
 * @brief Food texture category
 */
typedef enum {
    TEXTURE_SMOOTH = 0,             /**< Smooth (yogurt, pudding) */
    TEXTURE_ROUGH,                  /**< Rough (crackers) */
    TEXTURE_CRUNCHY,                /**< Crunchy (chips, celery) */
    TEXTURE_CHEWY,                  /**< Chewy (caramel, steak) */
    TEXTURE_CRISPY,                 /**< Crispy (fried foods) */
    TEXTURE_CREAMY,                 /**< Creamy (ice cream) */
    TEXTURE_GRAINY,                 /**< Grainy (whole grain) */
    TEXTURE_FIBROUS,                /**< Fibrous (celery, meat) */
    TEXTURE_GELATINOUS,             /**< Gelatinous (jello) */
    TEXTURE_LIQUID,                 /**< Liquid */
    TEXTURE_COUNT
} texture_category_t;

/**
 * @brief Mouthfeel quality
 */
typedef enum {
    MOUTHFEEL_NEUTRAL = 0,          /**< Neutral/nothing special */
    MOUTHFEEL_CREAMY,               /**< Creamy/smooth */
    MOUTHFEEL_OILY,                 /**< Oily/fatty */
    MOUTHFEEL_DRY,                  /**< Dry */
    MOUTHFEEL_ASTRINGENT,           /**< Astringent/puckering */
    MOUTHFEEL_BURNING,              /**< Burning (from capsaicin) */
    MOUTHFEEL_COOLING,              /**< Cooling (from menthol) */
    MOUTHFEEL_COUNT
} mouthfeel_quality_t;

/**
 * @brief Temperature perception category
 */
typedef enum {
    TEMP_PERCEPTION_COLD = 0,       /**< Cold (<20C) */
    TEMP_PERCEPTION_COOL,           /**< Cool (20-30C) */
    TEMP_PERCEPTION_NEUTRAL,        /**< Neutral (30-40C) */
    TEMP_PERCEPTION_WARM,           /**< Warm (40-50C) */
    TEMP_PERCEPTION_HOT,            /**< Hot (>50C) */
    TEMP_PERCEPTION_COUNT
} temp_perception_t;

/**
 * @brief Bridge status
 */
typedef enum {
    TRIGEMINAL_STATUS_IDLE = 0,
    TRIGEMINAL_STATUS_PROCESSING,
    TRIGEMINAL_STATUS_INTEGRATING,
    TRIGEMINAL_STATUS_ERROR
} trigeminal_status_t;

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief Oral somatosensory input
 */
typedef struct {
    oral_region_t region;           /**< Oral region */

    /* Mechanical */
    float pressure;                 /**< Contact pressure [0, 1] */
    float texture_roughness;        /**< Surface roughness [0, 1] */
    float hardness;                 /**< Food hardness [0, 1] */
    float viscosity;                /**< Liquid viscosity [0, 1] */

    /* Thermal */
    float temperature_c;            /**< Temperature in Celsius */
    temp_perception_t temp_category; /**< Perceived temperature */

    /* Pain/irritation */
    float pain_level;               /**< Pain intensity [0, 1] */
    bool is_noxious;                /**< Potentially harmful */

    /* Proprioceptive */
    float jaw_position;             /**< Jaw opening [0, 1] */
    float bite_force;               /**< Bite force [0, 1] */

    uint64_t timestamp;             /**< Sample timestamp */
} oral_soma_input_t;

/**
 * @brief Chemesthetic sensation
 */
typedef struct {
    chemesthesis_type_t type;       /**< Sensation type */
    float intensity;                /**< Intensity [0, 1] */
    float onset_rate;               /**< How fast it builds */
    float duration_s;               /**< Expected duration */
    float spread;                   /**< Spatial spread in mouth */

    /* For spiciness */
    float scoville_equiv;           /**< Equivalent Scoville units */

    /* For cooling */
    float cooling_intensity;        /**< Cooling strength */

    /* Localization */
    oral_region_t primary_region;   /**< Where strongest */
    uint32_t affected_regions;      /**< Bitmask of regions */
} chemesthesis_t;

/**
 * @brief Texture perception
 */
typedef struct {
    texture_category_t primary;     /**< Primary texture */
    texture_category_t secondary;   /**< Secondary texture */

    float* texture_profile;         /**< Texture feature vector */
    uint32_t profile_dim;           /**< Profile dimensionality */

    /* Specific attributes */
    float crunchiness;              /**< Crunch level [0, 1] */
    float chewiness;                /**< Chew resistance [0, 1] */
    float smoothness;               /**< Smoothness [0, 1] */
    float graininess;               /**< Grain size [0, 1] */
    float moisture;                 /**< Moisture content [0, 1] */

    /* Temporal */
    float breakdown_rate;           /**< How fast it breaks down */
    bool changes_with_chewing;      /**< Texture evolves */
} texture_perception_t;

/**
 * @brief Unified mouthfeel perception
 */
typedef struct {
    /* Overall qualities */
    mouthfeel_quality_t primary_quality;
    float* mouthfeel_profile;       /**< Mouthfeel feature vector */
    uint32_t profile_dim;           /**< Profile dimensionality */

    /* Components */
    texture_perception_t texture;   /**< Texture component */
    chemesthesis_t chemesthesis;    /**< Chemesthetic component */
    temp_perception_t temperature;  /**< Temperature perception */

    /* Integration with taste */
    float taste_enhancement;        /**< How much texture enhances taste */
    float flavor_release;           /**< Flavor release from chewing */

    /* Hedonic */
    float pleasantness;             /**< Overall pleasantness [-1, 1] */
    float familiarity;              /**< How familiar [0, 1] */

    /* Temporal dynamics */
    float onset_time_ms;            /**< Time to perceive */
    float peak_time_ms;             /**< Time to peak sensation */
    float linger_time_ms;           /**< Aftertaste/afterfeel duration */

    uint64_t timestamp;             /**< Perception timestamp */
} mouthfeel_t;

/**
 * @brief Temperature-taste interaction
 */
typedef struct {
    temp_perception_t temperature;  /**< Temperature category */
    float temp_celsius;             /**< Actual temperature */

    /* Modulation factors for each taste */
    float sweet_modulation;         /**< Sweet enhancement/suppression */
    float salty_modulation;         /**< Salty modulation */
    float sour_modulation;          /**< Sour modulation */
    float bitter_modulation;        /**< Bitter modulation */
    float umami_modulation;         /**< Umami modulation */

    /* Overall effect */
    float intensity_modulation;     /**< Overall taste intensity change */
    float threshold_shift;          /**< Detection threshold shift */
} temp_taste_interaction_t;

/**
 * @brief Spiciness perception
 */
typedef struct {
    float heat_level;               /**< Subjective heat [0, 1] */
    float scoville_estimate;        /**< Estimated Scoville units */

    /* Temporal profile */
    float onset_delay_ms;           /**< Time before feeling heat */
    float peak_intensity;           /**< Peak heat level */
    float peak_time_ms;             /**< Time to peak */
    float decay_rate;               /**< How fast it fades */

    /* Physiological response */
    float salivation_increase;      /**< Increased salivation */
    float sweating_response;        /**< Sweating trigger */
    float pain_component;           /**< Pain contribution */

    /* Tolerance */
    float tolerance_factor;         /**< Individual tolerance [0, 1] */
    bool causes_endorphin_release;  /**< Triggers pleasure response */

    /* Affected regions */
    uint32_t affected_regions;      /**< Bitmask of oral regions */
} spiciness_perception_t;

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Feature enables */
    bool enable_chemesthesis;       /**< Enable spicy/cooling detection */
    bool enable_texture;            /**< Enable texture processing */
    bool enable_temp_taste;         /**< Enable temperature-taste interaction */
    bool enable_mouthfeel;          /**< Enable unified mouthfeel */

    /* Sensitivity */
    float spice_sensitivity;        /**< Spice sensitivity [0, 1] */
    float cold_sensitivity;         /**< Cold sensitivity [0, 1] */
    float texture_sensitivity;      /**< Texture sensitivity [0, 1] */

    /* Tolerance (for spiciness) */
    float spice_tolerance;          /**< Spice tolerance level [0, 1] */

    /* Temporal */
    uint32_t integration_window_ms; /**< Sensory integration window */
    float adaptation_rate;          /**< Sensory adaptation rate */

    /* Logging */
    bool enable_logging;            /**< Enable debug logging */
} trigeminal_oral_config_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t oral_inputs_processed;
    uint64_t chemesthesis_detected;
    uint64_t textures_analyzed;
    uint64_t mouthfeels_computed;
    uint64_t temp_taste_interactions;

    float avg_spice_level;
    float avg_texture_pleasantness;
    float avg_mouthfeel_rating;

    uint64_t spice_tolerance_updates;
} trigeminal_oral_stats_t;

/* ============================================================================
 * Handle
 * ============================================================================ */

typedef struct trigeminal_oral_bridge_struct trigeminal_oral_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Initialize default configuration
 */
int trigeminal_oral_default_config(trigeminal_oral_config_t* config);

/**
 * @brief Create trigeminal-oral bridge
 */
trigeminal_oral_bridge_t* trigeminal_oral_bridge_create(const trigeminal_oral_config_t* config);

/**
 * @brief Destroy bridge
 */
void trigeminal_oral_bridge_destroy(trigeminal_oral_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect to somatosensory and gustatory modules
 */
int trigeminal_oral_connect(trigeminal_oral_bridge_t* bridge,
                            nimcp_somatosensory_t* soma,
                            nimcp_gustatory_t* gust);

/**
 * @brief Disconnect from modules
 */
int trigeminal_oral_disconnect(trigeminal_oral_bridge_t* bridge);

/**
 * @brief Check connection status
 */
bool trigeminal_oral_is_connected(const trigeminal_oral_bridge_t* bridge);

/**
 * @brief Get bridge status
 */
trigeminal_status_t trigeminal_oral_get_status(const trigeminal_oral_bridge_t* bridge);

/* ============================================================================
 * Oral Input Processing
 * ============================================================================ */

/**
 * @brief Process oral somatosensory input
 */
int trigeminal_oral_process_input(trigeminal_oral_bridge_t* bridge,
                                  const oral_soma_input_t* input);

/**
 * @brief Process multiple oral regions simultaneously
 */
int trigeminal_oral_process_multi(trigeminal_oral_bridge_t* bridge,
                                  const oral_soma_input_t* inputs,
                                  uint32_t num_inputs);

/**
 * @brief Update bridge state (call periodically)
 */
int trigeminal_oral_update(trigeminal_oral_bridge_t* bridge, float dt);

/* ============================================================================
 * Chemesthesis API (Spicy/Cooling/Tingling)
 * ============================================================================ */

/**
 * @brief Detect chemesthetic sensation from chemical exposure
 * @param compound_type Type of compound (capsaicin, menthol, etc.)
 * @param concentration Concentration [0, 1]
 */
int trigeminal_oral_detect_chemesthesis(trigeminal_oral_bridge_t* bridge,
                                        chemesthesis_type_t compound_type,
                                        float concentration,
                                        oral_region_t region,
                                        chemesthesis_t* result);

/**
 * @brief Get current spiciness perception
 */
int trigeminal_oral_get_spiciness(trigeminal_oral_bridge_t* bridge,
                                  spiciness_perception_t* spiciness);

/**
 * @brief Update spice tolerance based on exposure
 */
int trigeminal_oral_update_spice_tolerance(trigeminal_oral_bridge_t* bridge,
                                           float exposure_intensity,
                                           float duration_s);

/**
 * @brief Check if cooling sensation is active
 */
bool trigeminal_oral_is_cooling_active(const trigeminal_oral_bridge_t* bridge);

/**
 * @brief Get cooling intensity
 */
float trigeminal_oral_get_cooling_intensity(const trigeminal_oral_bridge_t* bridge);

/* ============================================================================
 * Texture API
 * ============================================================================ */

/**
 * @brief Analyze food texture from mechanical input
 */
int trigeminal_oral_analyze_texture(trigeminal_oral_bridge_t* bridge,
                                    const oral_soma_input_t* input,
                                    texture_perception_t* texture);

/**
 * @brief Classify texture category
 */
int trigeminal_oral_classify_texture(trigeminal_oral_bridge_t* bridge,
                                     const float* texture_features,
                                     uint32_t num_features,
                                     texture_category_t* category,
                                     float* confidence);

/**
 * @brief Get current texture perception
 */
int trigeminal_oral_get_texture(trigeminal_oral_bridge_t* bridge,
                                texture_perception_t* texture);

/* ============================================================================
 * Temperature-Taste Interaction API
 * ============================================================================ */

/**
 * @brief Compute temperature effect on taste perception
 */
int trigeminal_oral_compute_temp_taste(trigeminal_oral_bridge_t* bridge,
                                       float temperature_c,
                                       temp_taste_interaction_t* interaction);

/**
 * @brief Apply temperature modulation to taste values
 */
int trigeminal_oral_modulate_taste(trigeminal_oral_bridge_t* bridge,
                                   float temperature_c,
                                   float* sweet, float* salty,
                                   float* sour, float* bitter,
                                   float* umami);

/**
 * @brief Get optimal serving temperature for taste profile
 */
int trigeminal_oral_optimal_temperature(trigeminal_oral_bridge_t* bridge,
                                        const taste_perception_t* taste,
                                        float* optimal_temp_c);

/* ============================================================================
 * Mouthfeel Integration API
 * ============================================================================ */

/**
 * @brief Compute unified mouthfeel from all inputs
 */
int trigeminal_oral_compute_mouthfeel(trigeminal_oral_bridge_t* bridge,
                                      const oral_soma_input_t* soma_input,
                                      const taste_perception_t* taste_input,
                                      mouthfeel_t* mouthfeel);

/**
 * @brief Get current mouthfeel state
 */
int trigeminal_oral_get_mouthfeel(trigeminal_oral_bridge_t* bridge,
                                  mouthfeel_t* mouthfeel);

/**
 * @brief Predict mouthfeel pleasantness
 */
int trigeminal_oral_predict_pleasantness(trigeminal_oral_bridge_t* bridge,
                                         const mouthfeel_t* mouthfeel,
                                         float* pleasantness);

/**
 * @brief Compute flavor-texture synergy
 */
int trigeminal_oral_flavor_texture_synergy(trigeminal_oral_bridge_t* bridge,
                                           const taste_perception_t* taste,
                                           const texture_perception_t* texture,
                                           float* synergy_score);

/* ============================================================================
 * Mastication (Chewing) API
 * ============================================================================ */

/**
 * @brief Start tracking mastication cycle
 */
int trigeminal_oral_start_mastication(trigeminal_oral_bridge_t* bridge,
                                      float initial_hardness);

/**
 * @brief Update mastication state
 */
int trigeminal_oral_update_mastication(trigeminal_oral_bridge_t* bridge,
                                       float bite_force,
                                       float jaw_position);

/**
 * @brief Get chew count and breakdown progress
 */
int trigeminal_oral_get_mastication_state(trigeminal_oral_bridge_t* bridge,
                                          uint32_t* chew_count,
                                          float* breakdown_progress,
                                          bool* ready_to_swallow);

/**
 * @brief End mastication (swallow)
 */
int trigeminal_oral_end_mastication(trigeminal_oral_bridge_t* bridge);

/* ============================================================================
 * Statistics API
 * ============================================================================ */

/**
 * @brief Get bridge statistics
 */
int trigeminal_oral_get_stats(const trigeminal_oral_bridge_t* bridge,
                              trigeminal_oral_stats_t* stats);

/**
 * @brief Reset statistics
 */
int trigeminal_oral_reset_stats(trigeminal_oral_bridge_t* bridge);

/**
 * @brief Print summary
 */
void trigeminal_oral_print_summary(const trigeminal_oral_bridge_t* bridge);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get name for oral region
 */
const char* trigeminal_oral_region_name(oral_region_t region);

/**
 * @brief Get name for chemesthesis type
 */
const char* trigeminal_chemesthesis_name(chemesthesis_type_t type);

/**
 * @brief Get name for texture category
 */
const char* trigeminal_texture_name(texture_category_t texture);

/**
 * @brief Get name for mouthfeel quality
 */
const char* trigeminal_mouthfeel_name(mouthfeel_quality_t quality);

/**
 * @brief Convert Scoville to normalized heat [0, 1]
 */
float trigeminal_scoville_to_normalized(float scoville);

/**
 * @brief Convert normalized heat to Scoville
 */
float trigeminal_normalized_to_scoville(float normalized);

/* ============================================================================
 * Cleanup
 * ============================================================================ */

/**
 * @brief Free texture perception resources
 */
void trigeminal_texture_free(texture_perception_t* texture);

/**
 * @brief Free mouthfeel resources
 */
void trigeminal_mouthfeel_free(mouthfeel_t* mouthfeel);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_TRIGEMINAL_ORAL_BRIDGE_H */
