/**
 * @file nimcp_synesthesia.h
 * @brief Superhuman cross-modal perception enhancement module
 *
 * WHAT: Provides synesthesia-like cross-modal sensory associations
 * WHY:  Enable enhanced perception through automatic sensory binding
 * HOW:  Learned cross-modal mappings, concurrent sensory activation
 *
 * ARCHITECTURE:
 * - Grapheme-color associations for enhanced symbol processing
 * - Sound-shape mappings for audio-visual binding (bouba/kiki effect)
 * - Taste-touch associations for embodied perception
 * - Bidirectional sensory cascades
 * - Learned association strength tuning
 *
 * BIOLOGICAL BASIS:
 * - Models synesthesia as enhanced cross-modal connectivity
 * - Increased binding between sensory cortices
 * - Preserved feedforward/feedback sensory pathways
 * - Developmental synesthesia patterns
 * - Chromesthesia, grapheme-color, lexical-gustatory variants
 *
 * @version Phase T12: Superhuman Enhancement Modules
 * @date 2026-01-13
 */

#ifndef NIMCP_SYNESTHESIA_H
#define NIMCP_SYNESTHESIA_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/*=============================================================================
 * ERROR CODES
 *===========================================================================*/

/**
 * @brief Synesthesia module error codes
 */
typedef enum {
    SYNESTHESIA_ERROR_NONE = 0,
    SYNESTHESIA_ERROR_INVALID_INPUT,
    SYNESTHESIA_ERROR_MAPPING_FAILED,
    SYNESTHESIA_ERROR_MODALITY_NOT_FOUND,
    SYNESTHESIA_ERROR_ASSOCIATION_NOT_FOUND,
    SYNESTHESIA_ERROR_CAPACITY_EXCEEDED,
    SYNESTHESIA_ERROR_CASCADE_OVERFLOW,
    SYNESTHESIA_ERROR_TRAINING_FAILED,
    SYNESTHESIA_ERROR_INHIBITION_ACTIVE,
    SYNESTHESIA_ERROR_NOT_INITIALIZED,
    SYNESTHESIA_ERROR_INTERNAL
} synesthesia_error_t;

/*=============================================================================
 * ENUMERATIONS
 *===========================================================================*/

/**
 * @brief Processing status
 */
typedef enum {
    SYNESTHESIA_STATUS_IDLE = 0,
    SYNESTHESIA_STATUS_MAPPING,
    SYNESTHESIA_STATUS_CASCADING,
    SYNESTHESIA_STATUS_TRAINING,
    SYNESTHESIA_STATUS_INHIBITED,
    SYNESTHESIA_STATUS_READY,
    SYNESTHESIA_STATUS_ERROR
} synesthesia_status_t;

/**
 * @brief Sensory modalities for cross-modal binding
 */
typedef enum {
    MODALITY_VISUAL = 0,
    MODALITY_AUDITORY,
    MODALITY_TACTILE,
    MODALITY_GUSTATORY,
    MODALITY_OLFACTORY,
    MODALITY_PROPRIOCEPTIVE,
    MODALITY_VESTIBULAR,
    MODALITY_NOCICEPTIVE,
    MODALITY_COUNT
} sensory_modality_t;

/**
 * @brief Visual sub-modalities
 */
typedef enum {
    VISUAL_COLOR = 0,
    VISUAL_SHAPE,
    VISUAL_MOTION,
    VISUAL_TEXTURE,
    VISUAL_LUMINANCE,
    VISUAL_SPATIAL,
    VISUAL_GRAPHEME,
    VISUAL_SUB_COUNT
} visual_submodality_t;

/**
 * @brief Auditory sub-modalities
 */
typedef enum {
    AUDITORY_PITCH = 0,
    AUDITORY_TIMBRE,
    AUDITORY_RHYTHM,
    AUDITORY_LOUDNESS,
    AUDITORY_SPATIAL,
    AUDITORY_PHONEME,
    AUDITORY_SUB_COUNT
} auditory_submodality_t;

/**
 * @brief Synesthesia types (common variants)
 */
typedef enum {
    SYNESTHESIA_GRAPHEME_COLOR = 0,      /**< Letters/numbers -> colors */
    SYNESTHESIA_CHROMESTHESIA,           /**< Sounds -> colors */
    SYNESTHESIA_SPATIAL_SEQUENCE,        /**< Numbers -> spatial positions */
    SYNESTHESIA_LEXICAL_GUSTATORY,       /**< Words -> tastes */
    SYNESTHESIA_ORDINAL_LINGUISTIC,      /**< Ordered sequences -> personalities */
    SYNESTHESIA_MISOPHONIA,              /**< Sounds -> emotions */
    SYNESTHESIA_MIRROR_TOUCH,            /**< Observed touch -> felt touch */
    SYNESTHESIA_AUDITORY_TACTILE,        /**< Sounds -> tactile sensations */
    SYNESTHESIA_TYPE_COUNT
} synesthesia_type_t;

/**
 * @brief Association strength levels
 */
typedef enum {
    ASSOC_STRENGTH_NONE = 0,
    ASSOC_STRENGTH_WEAK,
    ASSOC_STRENGTH_MODERATE,
    ASSOC_STRENGTH_STRONG,
    ASSOC_STRENGTH_AUTOMATIC
} association_strength_t;

/**
 * @brief Cascade propagation modes
 */
typedef enum {
    CASCADE_NONE = 0,
    CASCADE_UNIDIRECTIONAL,
    CASCADE_BIDIRECTIONAL,
    CASCADE_BROADCAST
} cascade_mode_t;

/*=============================================================================
 * CONFIGURATION
 *===========================================================================*/

/**
 * @brief Default configuration values
 */
#define SYNESTHESIA_DEFAULT_MAX_ASSOCIATIONS        10000
#define SYNESTHESIA_DEFAULT_MAX_CASCADE_DEPTH       5
#define SYNESTHESIA_DEFAULT_FEATURE_DIM             64
#define SYNESTHESIA_DEFAULT_COLOR_CHANNELS          3
#define SYNESTHESIA_DEFAULT_ACTIVATION_THRESHOLD    0.3f
#define SYNESTHESIA_DEFAULT_CASCADE_DECAY           0.7f
#define SYNESTHESIA_DEFAULT_LEARNING_RATE           0.05f

/**
 * @brief Synesthesia module configuration
 */
typedef struct {
    /* Capacity limits */
    uint32_t max_associations;           /**< Maximum cross-modal associations */
    uint32_t max_cascade_depth;          /**< Maximum cascade propagation depth */

    /* Feature dimensions */
    uint32_t feature_dim;                /**< Generic feature vector dimension */
    uint32_t color_channels;             /**< Color representation channels (RGB) */

    /* Activation parameters */
    float activation_threshold;          /**< Minimum activation for triggering */
    float cascade_decay;                 /**< Decay per cascade step */
    float concurrent_boost;              /**< Boost for concurrent activations */

    /* Learning parameters */
    float learning_rate;                 /**< Association learning rate */
    float consistency_threshold;         /**< Threshold for consistent associations */
    bool enable_hebbian_learning;        /**< Enable Hebbian association learning */

    /* Enabled synesthesia types */
    bool enable_grapheme_color;          /**< Enable grapheme-color */
    bool enable_chromesthesia;           /**< Enable sound-color */
    bool enable_spatial_sequence;        /**< Enable number-space */
    bool enable_lexical_gustatory;       /**< Enable word-taste */
    bool enable_auditory_tactile;        /**< Enable sound-touch */
    bool enable_mirror_touch;            /**< Enable observed-felt touch */

    /* Processing options */
    bool enable_bidirectional;           /**< Allow reverse mappings */
    bool enable_cascade;                 /**< Enable cascade propagation */
    bool enable_inhibition;              /**< Enable selective inhibition */
    uint32_t max_concurrent_cascades;    /**< Max parallel cascades */
} synesthesia_config_t;

/*=============================================================================
 * DATA STRUCTURES
 *===========================================================================*/

/**
 * @brief RGB color representation
 */
typedef struct {
    float r;                             /**< Red channel [0, 1] */
    float g;                             /**< Green channel [0, 1] */
    float b;                             /**< Blue channel [0, 1] */
    float alpha;                         /**< Transparency [0, 1] */
    float saturation;                    /**< Color saturation [0, 1] */
    float luminance;                     /**< Perceived luminance [0, 1] */
} synesthesia_color_t;

/**
 * @brief Shape descriptor for sound-shape associations
 */
typedef struct {
    float roundness;                     /**< Round vs angular [0, 1] */
    float sharpness;                     /**< Smooth vs sharp [0, 1] */
    float size;                          /**< Perceived size [0, 1] */
    float complexity;                    /**< Shape complexity [0, 1] */
    float symmetry;                      /**< Symmetry degree [0, 1] */
    uint32_t num_vertices;               /**< Approximate vertex count */
    float* contour;                      /**< Shape contour points */
    uint32_t contour_size;
} synesthesia_shape_t;

/**
 * @brief Sound descriptor
 */
typedef struct {
    float pitch;                         /**< Pitch in Hz (normalized) */
    float loudness;                      /**< Loudness [0, 1] */
    float* timbre;                       /**< Timbre features */
    uint32_t timbre_dim;
    float duration_ms;                   /**< Duration in milliseconds */
    float attack;                        /**< Attack time [0, 1] */
    float decay;                         /**< Decay time [0, 1] */
} synesthesia_sound_t;

/**
 * @brief Tactile sensation descriptor
 */
typedef struct {
    float pressure;                      /**< Pressure intensity [0, 1] */
    float temperature;                   /**< Temperature [-1, 1] */
    float texture_roughness;             /**< Texture roughness [0, 1] */
    float vibration;                     /**< Vibration intensity [0, 1] */
    float location_x;                    /**< Body location X */
    float location_y;                    /**< Body location Y */
    float spread;                        /**< Sensation spread [0, 1] */
} synesthesia_tactile_t;

/**
 * @brief Taste descriptor
 */
typedef struct {
    float sweet;                         /**< Sweetness [0, 1] */
    float sour;                          /**< Sourness [0, 1] */
    float salty;                         /**< Saltiness [0, 1] */
    float bitter;                        /**< Bitterness [0, 1] */
    float umami;                         /**< Umami [0, 1] */
    float intensity;                     /**< Overall intensity [0, 1] */
    float pleasantness;                  /**< Pleasantness [-1, 1] */
} synesthesia_taste_t;

/**
 * @brief Grapheme (letter/number) representation
 */
typedef struct {
    uint32_t codepoint;                  /**< Unicode codepoint */
    char utf8[8];                        /**< UTF-8 representation */
    float* visual_features;              /**< Visual shape features */
    uint32_t feature_count;
    bool is_digit;                       /**< True if numeric digit */
    bool is_letter;                      /**< True if alphabetic */
} grapheme_t;

/**
 * @brief Cross-modal association entry
 */
typedef struct {
    uint64_t association_id;             /**< Unique identifier */
    synesthesia_type_t type;             /**< Association type */

    /* Inducer (trigger) */
    sensory_modality_t inducer_modality; /**< Triggering modality */
    float* inducer_features;             /**< Inducer feature vector */
    uint32_t inducer_feature_count;

    /* Concurrent (induced sensation) */
    sensory_modality_t concurrent_modality; /**< Induced modality */
    float* concurrent_features;          /**< Concurrent feature vector */
    uint32_t concurrent_feature_count;

    /* Association properties */
    float strength;                      /**< Association strength [0, 1] */
    float consistency;                   /**< How consistent across triggers */
    float automaticity;                  /**< How automatic (involuntary) */
    uint32_t activation_count;           /**< Times activated */

    /* Bidirectional link */
    uint64_t reverse_association_id;     /**< ID of reverse mapping (if exists) */
} cross_modal_association_t;

/**
 * @brief Grapheme-color mapping
 */
typedef struct {
    grapheme_t grapheme;                 /**< The grapheme */
    synesthesia_color_t color;           /**< Associated color */
    float strength;                      /**< Association strength */
    float consistency;                   /**< Cross-context consistency */
} grapheme_color_mapping_t;

/**
 * @brief Sound-shape mapping
 */
typedef struct {
    synesthesia_sound_t sound;           /**< Sound features */
    synesthesia_shape_t shape;           /**< Associated shape */
    float strength;                      /**< Association strength */
    float bouba_kiki_score;              /**< Bouba-kiki effect strength */
} sound_shape_mapping_t;

/**
 * @brief Cascade propagation result
 */
typedef struct {
    sensory_modality_t* activated_modalities; /**< Modalities activated */
    uint32_t modality_count;
    float* activation_strengths;         /**< Strength at each modality */
    uint32_t cascade_depth;              /**< Depth reached */
    uint64_t* triggered_associations;    /**< Association IDs triggered */
    uint32_t triggered_count;
} cascade_result_t;

/**
 * @brief Synesthetic experience result
 */
typedef struct {
    synesthesia_type_t type;             /**< Experience type */

    /* Inducer info */
    sensory_modality_t inducer_modality;
    float inducer_intensity;

    /* Concurrent experiences */
    synesthesia_color_t* colors;         /**< Induced colors */
    uint32_t color_count;
    synesthesia_shape_t* shapes;         /**< Induced shapes */
    uint32_t shape_count;
    synesthesia_tactile_t* tactile;      /**< Induced tactile sensations */
    uint32_t tactile_count;
    synesthesia_taste_t* tastes;         /**< Induced tastes */
    uint32_t taste_count;

    /* Quality metrics */
    float overall_intensity;             /**< Overall experience intensity */
    float vividness;                     /**< Concurrent vividness */
    float involuntariness;               /**< How automatic/involuntary */
} synesthetic_experience_t;

/*=============================================================================
 * STATISTICS
 *===========================================================================*/

/**
 * @brief Module statistics
 */
typedef struct {
    /* Association counts */
    uint64_t total_associations;         /**< Total associations created */
    uint64_t active_associations;        /**< Currently active */
    uint64_t grapheme_color_count;       /**< Grapheme-color mappings */
    uint64_t sound_shape_count;          /**< Sound-shape mappings */
    uint64_t lexical_gustatory_count;    /**< Word-taste mappings */

    /* Activation counts */
    uint64_t total_activations;          /**< Total synesthetic activations */
    uint64_t cascade_activations;        /**< Cascade-triggered activations */
    uint64_t inhibited_activations;      /**< Inhibited activations */

    /* Quality metrics */
    float avg_association_strength;      /**< Average association strength */
    float avg_consistency;               /**< Average consistency */
    float avg_cascade_depth;             /**< Average cascade depth */

    /* Learning metrics */
    uint64_t associations_learned;       /**< Hebbian-learned associations */
    uint64_t associations_strengthened;  /**< Strengthened through use */
    uint64_t associations_weakened;      /**< Weakened through non-use */

    /* Performance */
    float avg_mapping_time_us;           /**< Average mapping time */
    float avg_cascade_time_us;           /**< Average cascade time */

    /* Resource usage */
    size_t memory_used_bytes;            /**< Total memory usage */
} synesthesia_stats_t;

/*=============================================================================
 * CALLBACK TYPES
 *===========================================================================*/

/**
 * @brief Callback for synesthetic experience events
 */
typedef void (*synesthesia_experience_callback_t)(
    const synesthetic_experience_t* experience,
    void* user_data
);

/**
 * @brief Callback for cascade propagation events
 */
typedef void (*synesthesia_cascade_callback_t)(
    const cascade_result_t* cascade,
    void* user_data
);

/**
 * @brief Callback for association learning events
 */
typedef void (*synesthesia_learning_callback_t)(
    uint64_t association_id,
    float old_strength,
    float new_strength,
    void* user_data
);

/*=============================================================================
 * OPAQUE TYPE
 *===========================================================================*/

/**
 * @brief Opaque synesthesia module handle
 */
typedef struct synesthesia_module synesthesia_module_t;

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

/**
 * @brief Get default configuration
 *
 * WHAT: Returns sensible default configuration
 * WHY:  Provide starting point for customization
 * HOW:  Initialize all fields with synesthesia-optimized values
 *
 * @return Default configuration structure
 */
synesthesia_config_t synesthesia_default_config(void);

/**
 * @brief Create synesthesia module
 *
 * WHAT: Allocate and initialize the module
 * WHY:  Enable cross-modal perception enhancement
 * HOW:  Create association stores, initialize mapping networks
 *
 * @param config Configuration (NULL for defaults)
 * @return New module instance, or NULL on failure
 */
synesthesia_module_t* synesthesia_create(const synesthesia_config_t* config);

/**
 * @brief Destroy synesthesia module
 *
 * WHAT: Free all resources
 * WHY:  Prevent memory leaks
 * HOW:  Destroy association stores and mapping networks
 *
 * @param module Module to destroy
 */
void synesthesia_destroy(synesthesia_module_t* module);

/**
 * @brief Reset module state
 *
 * WHAT: Clear all associations and reset state
 * WHY:  Allow fresh start without reallocation
 * HOW:  Clear association stores
 *
 * @param module Module instance
 * @return true on success
 */
bool synesthesia_reset(synesthesia_module_t* module);

/*=============================================================================
 * GRAPHEME-COLOR SYNESTHESIA
 *===========================================================================*/

/**
 * @brief Add grapheme-color association
 *
 * WHAT: Create mapping from grapheme to color
 * WHY:  Core grapheme-color synesthesia
 * HOW:  Store grapheme-color pair with strength
 *
 * @param module Module instance
 * @param grapheme Grapheme to map
 * @param color Associated color
 * @param strength Association strength [0, 1]
 * @return true on success
 */
bool synesthesia_add_grapheme_color(
    synesthesia_module_t* module,
    const grapheme_t* grapheme,
    const synesthesia_color_t* color,
    float strength
);

/**
 * @brief Get color for grapheme
 *
 * WHAT: Retrieve synesthetic color for grapheme
 * WHY:  Trigger grapheme-color experience
 * HOW:  Lookup grapheme in mapping store
 *
 * @param module Module instance
 * @param grapheme Query grapheme
 * @param color Output color
 * @return true if mapping exists
 */
bool synesthesia_get_grapheme_color(
    synesthesia_module_t* module,
    const grapheme_t* grapheme,
    synesthesia_color_t* color
);

/**
 * @brief Get color for character
 *
 * WHAT: Convenience function for single character
 * WHY:  Simple character-to-color lookup
 * HOW:  Convert char to grapheme and lookup
 *
 * @param module Module instance
 * @param character ASCII character
 * @param color Output color
 * @return true if mapping exists
 */
bool synesthesia_get_char_color(
    synesthesia_module_t* module,
    char character,
    synesthesia_color_t* color
);

/**
 * @brief Get colors for string
 *
 * WHAT: Get synesthetic color sequence for text
 * WHY:  Colorize text synesthetically
 * HOW:  Map each character to its color
 *
 * @param module Module instance
 * @param text Input string
 * @param colors Output color array (must be pre-allocated)
 * @param max_colors Maximum colors to output
 * @param color_count Output: actual color count
 * @return true on success
 */
bool synesthesia_colorize_text(
    synesthesia_module_t* module,
    const char* text,
    synesthesia_color_t* colors,
    uint32_t max_colors,
    uint32_t* color_count
);

/*=============================================================================
 * SOUND-SHAPE SYNESTHESIA (BOUBA/KIKI)
 *===========================================================================*/

/**
 * @brief Add sound-shape association
 *
 * WHAT: Create mapping from sound to shape
 * WHY:  Sound-shape binding (bouba/kiki effect)
 * HOW:  Store sound-shape pair with strength
 *
 * @param module Module instance
 * @param sound Sound features
 * @param shape Associated shape
 * @param strength Association strength [0, 1]
 * @return true on success
 */
bool synesthesia_add_sound_shape(
    synesthesia_module_t* module,
    const synesthesia_sound_t* sound,
    const synesthesia_shape_t* shape,
    float strength
);

/**
 * @brief Get shape for sound
 *
 * WHAT: Retrieve synesthetic shape for sound
 * WHY:  Trigger sound-shape experience
 * HOW:  Match sound features to stored mappings
 *
 * @param module Module instance
 * @param sound Query sound
 * @param shape Output shape
 * @return true if mapping found
 */
bool synesthesia_get_sound_shape(
    synesthesia_module_t* module,
    const synesthesia_sound_t* sound,
    synesthesia_shape_t* shape
);

/**
 * @brief Compute bouba-kiki score
 *
 * WHAT: Calculate bouba/kiki classification
 * WHY:  Quantify sound-shape correspondence
 * HOW:  Score sound features against bouba/kiki prototypes
 *
 * @param module Module instance
 * @param sound Sound to classify
 * @return Score [-1, 1] where -1=bouba, +1=kiki
 */
float synesthesia_bouba_kiki_score(
    synesthesia_module_t* module,
    const synesthesia_sound_t* sound
);

/**
 * @brief Generate shape from sound
 *
 * WHAT: Create shape based on sound features
 * WHY:  Automatic sound-to-shape generation
 * HOW:  Map sound parameters to shape parameters
 *
 * @param module Module instance
 * @param sound Input sound
 * @param shape Output generated shape
 * @return true on success
 */
bool synesthesia_generate_shape_from_sound(
    synesthesia_module_t* module,
    const synesthesia_sound_t* sound,
    synesthesia_shape_t* shape
);

/*=============================================================================
 * TASTE-TOUCH SYNESTHESIA
 *===========================================================================*/

/**
 * @brief Add taste-touch association
 *
 * WHAT: Create mapping from taste to tactile sensation
 * WHY:  Enable taste-touch synesthesia
 * HOW:  Store taste-tactile pair with strength
 *
 * @param module Module instance
 * @param taste Taste features
 * @param tactile Associated tactile sensation
 * @param strength Association strength [0, 1]
 * @return true on success
 */
bool synesthesia_add_taste_touch(
    synesthesia_module_t* module,
    const synesthesia_taste_t* taste,
    const synesthesia_tactile_t* tactile,
    float strength
);

/**
 * @brief Get tactile sensation for taste
 *
 * WHAT: Retrieve synesthetic tactile for taste
 * WHY:  Trigger taste-touch experience
 * HOW:  Match taste features to stored mappings
 *
 * @param module Module instance
 * @param taste Query taste
 * @param tactile Output tactile sensation
 * @return true if mapping found
 */
bool synesthesia_get_taste_touch(
    synesthesia_module_t* module,
    const synesthesia_taste_t* taste,
    synesthesia_tactile_t* tactile
);

/*=============================================================================
 * GENERIC CROSS-MODAL ASSOCIATIONS
 *===========================================================================*/

/**
 * @brief Create generic cross-modal association
 *
 * WHAT: Create association between any two modalities
 * WHY:  Support arbitrary cross-modal bindings
 * HOW:  Store inducer-concurrent feature pair
 *
 * @param module Module instance
 * @param inducer_modality Triggering modality
 * @param inducer_features Inducer feature vector
 * @param inducer_count Inducer feature count
 * @param concurrent_modality Induced modality
 * @param concurrent_features Concurrent feature vector
 * @param concurrent_count Concurrent feature count
 * @param strength Association strength [0, 1]
 * @return Association ID on success, 0 on failure
 */
uint64_t synesthesia_create_association(
    synesthesia_module_t* module,
    sensory_modality_t inducer_modality,
    const float* inducer_features,
    uint32_t inducer_count,
    sensory_modality_t concurrent_modality,
    const float* concurrent_features,
    uint32_t concurrent_count,
    float strength
);

/**
 * @brief Trigger cross-modal experience
 *
 * WHAT: Activate synesthetic experience from inducer
 * WHY:  Core synesthetic triggering
 * HOW:  Find matching associations and generate concurrent
 *
 * @param module Module instance
 * @param inducer_modality Triggering modality
 * @param inducer_features Inducer feature vector
 * @param inducer_count Inducer feature count
 * @param intensity Triggering intensity [0, 1]
 * @param experience Output experience result
 * @return true on success
 */
bool synesthesia_trigger_experience(
    synesthesia_module_t* module,
    sensory_modality_t inducer_modality,
    const float* inducer_features,
    uint32_t inducer_count,
    float intensity,
    synesthetic_experience_t* experience
);

/**
 * @brief Get association by ID
 *
 * WHAT: Retrieve specific association
 * WHY:  Inspect association details
 * HOW:  ID-based lookup
 *
 * @param module Module instance
 * @param association_id Association ID
 * @param association Output association
 * @return true if found
 */
bool synesthesia_get_association(
    const synesthesia_module_t* module,
    uint64_t association_id,
    cross_modal_association_t* association
);

/**
 * @brief Update association strength
 *
 * WHAT: Modify association strength
 * WHY:  Allow strength tuning
 * HOW:  Update strength field
 *
 * @param module Module instance
 * @param association_id Association ID
 * @param new_strength New strength value [0, 1]
 * @return true on success
 */
bool synesthesia_update_strength(
    synesthesia_module_t* module,
    uint64_t association_id,
    float new_strength
);

/**
 * @brief Remove association
 *
 * WHAT: Delete cross-modal association
 * WHY:  Allow association removal
 * HOW:  Remove from storage
 *
 * @param module Module instance
 * @param association_id Association to remove
 * @return true on success
 */
bool synesthesia_remove_association(
    synesthesia_module_t* module,
    uint64_t association_id
);

/*=============================================================================
 * CASCADE PROPAGATION
 *===========================================================================*/

/**
 * @brief Trigger cascade propagation
 *
 * WHAT: Propagate activation through association network
 * WHY:  Model cascading synesthetic experiences
 * HOW:  Activate associations recursively with decay
 *
 * @param module Module instance
 * @param start_modality Starting modality
 * @param start_features Starting feature vector
 * @param feature_count Feature count
 * @param mode Cascade propagation mode
 * @param result Output cascade result
 * @return true on success
 */
bool synesthesia_cascade(
    synesthesia_module_t* module,
    sensory_modality_t start_modality,
    const float* start_features,
    uint32_t feature_count,
    cascade_mode_t mode,
    cascade_result_t* result
);

/**
 * @brief Inhibit synesthesia temporarily
 *
 * WHAT: Suppress synesthetic experiences
 * WHY:  Allow controlled inhibition
 * HOW:  Set inhibition flag
 *
 * @param module Module instance
 * @param inhibit true to inhibit, false to enable
 * @return true on success
 */
bool synesthesia_set_inhibition(
    synesthesia_module_t* module,
    bool inhibit
);

/**
 * @brief Check if synesthesia is inhibited
 *
 * @param module Module instance
 * @return true if currently inhibited
 */
bool synesthesia_is_inhibited(const synesthesia_module_t* module);

/*=============================================================================
 * LEARNING AND TRAINING
 *===========================================================================*/

/**
 * @brief Train association through co-occurrence
 *
 * WHAT: Strengthen association through Hebbian learning
 * WHY:  Learn new synesthetic associations
 * HOW:  Increase strength when both modalities co-activate
 *
 * @param module Module instance
 * @param inducer_modality Inducer modality
 * @param inducer_features Inducer features
 * @param inducer_count Inducer feature count
 * @param concurrent_modality Concurrent modality
 * @param concurrent_features Concurrent features
 * @param concurrent_count Concurrent feature count
 * @return Association ID (new or existing)
 */
uint64_t synesthesia_train_cooccurrence(
    synesthesia_module_t* module,
    sensory_modality_t inducer_modality,
    const float* inducer_features,
    uint32_t inducer_count,
    sensory_modality_t concurrent_modality,
    const float* concurrent_features,
    uint32_t concurrent_count
);

/**
 * @brief Decay unused associations
 *
 * WHAT: Weaken associations not recently activated
 * WHY:  Model forgetting/weakening
 * HOW:  Reduce strength of inactive associations
 *
 * @param module Module instance
 * @param decay_rate Decay rate [0, 1]
 * @param min_activations Minimum activations to avoid decay
 * @return Number of associations decayed
 */
uint32_t synesthesia_decay_unused(
    synesthesia_module_t* module,
    float decay_rate,
    uint32_t min_activations
);

/**
 * @brief Initialize standard grapheme-color palette
 *
 * WHAT: Create typical grapheme-color associations
 * WHY:  Initialize with common synesthetic patterns
 * HOW:  Use research-based color distributions
 *
 * @param module Module instance
 * @return Number of associations created
 */
uint32_t synesthesia_init_grapheme_colors(synesthesia_module_t* module);

/**
 * @brief Initialize standard bouba-kiki mappings
 *
 * WHAT: Create prototype sound-shape associations
 * WHY:  Initialize with universal bouba-kiki patterns
 * HOW:  Use research-based sound-shape correspondences
 *
 * @param module Module instance
 * @return Number of associations created
 */
uint32_t synesthesia_init_bouba_kiki(synesthesia_module_t* module);

/*=============================================================================
 * CALLBACKS
 *===========================================================================*/

/**
 * @brief Set experience callback
 *
 * @param module Module instance
 * @param callback Callback function
 * @param user_data User context
 * @return true on success
 */
bool synesthesia_set_experience_callback(
    synesthesia_module_t* module,
    synesthesia_experience_callback_t callback,
    void* user_data
);

/**
 * @brief Set cascade callback
 *
 * @param module Module instance
 * @param callback Callback function
 * @param user_data User context
 * @return true on success
 */
bool synesthesia_set_cascade_callback(
    synesthesia_module_t* module,
    synesthesia_cascade_callback_t callback,
    void* user_data
);

/**
 * @brief Set learning callback
 *
 * @param module Module instance
 * @param callback Callback function
 * @param user_data User context
 * @return true on success
 */
bool synesthesia_set_learning_callback(
    synesthesia_module_t* module,
    synesthesia_learning_callback_t callback,
    void* user_data
);

/*=============================================================================
 * STATUS AND DIAGNOSTICS
 *===========================================================================*/

/**
 * @brief Get current status
 *
 * @param module Module instance
 * @return Current status
 */
synesthesia_status_t synesthesia_get_status(const synesthesia_module_t* module);

/**
 * @brief Get last error code
 *
 * @param module Module instance
 * @return Last error code
 */
synesthesia_error_t synesthesia_get_last_error(const synesthesia_module_t* module);

/**
 * @brief Get error description string
 *
 * @param error Error code
 * @return Human-readable description
 */
const char* synesthesia_error_string(synesthesia_error_t error);

/**
 * @brief Get status description string
 *
 * @param status Status code
 * @return Human-readable description
 */
const char* synesthesia_status_string(synesthesia_status_t status);

/**
 * @brief Get module statistics
 *
 * @param module Module instance
 * @param stats Output statistics structure
 * @return true on success
 */
bool synesthesia_get_stats(const synesthesia_module_t* module, synesthesia_stats_t* stats);

/**
 * @brief Get module configuration
 *
 * @param module Module instance
 * @param config Output configuration structure
 * @return true on success
 */
bool synesthesia_get_config(const synesthesia_module_t* module, synesthesia_config_t* config);

/*=============================================================================
 * UTILITY FUNCTIONS
 *===========================================================================*/

/**
 * @brief Free synesthetic experience result
 *
 * @param experience Experience to free
 */
void synesthesia_free_experience(synesthetic_experience_t* experience);

/**
 * @brief Free cascade result
 *
 * @param cascade Cascade result to free
 */
void synesthesia_free_cascade(cascade_result_t* cascade);

/**
 * @brief Free shape resources
 *
 * @param shape Shape to free
 */
void synesthesia_free_shape(synesthesia_shape_t* shape);

/**
 * @brief Convert HSV to RGB color
 *
 * @param h Hue [0, 360]
 * @param s Saturation [0, 1]
 * @param v Value [0, 1]
 * @param color Output RGB color
 */
void synesthesia_hsv_to_rgb(float h, float s, float v, synesthesia_color_t* color);

/**
 * @brief Convert RGB to HSV color
 *
 * @param color Input RGB color
 * @param h Output hue [0, 360]
 * @param s Output saturation [0, 1]
 * @param v Output value [0, 1]
 */
void synesthesia_rgb_to_hsv(const synesthesia_color_t* color, float* h, float* s, float* v);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SYNESTHESIA_H */
