/**
 * @file nimcp_gustatory.h
 * @brief Gustatory Cortex - Taste Processing
 * @version Phase 6: Sensory Processing
 * @date 2026-01-12
 *
 * The gustatory cortex (insular cortex and frontal operculum) processes
 * taste information from the tongue and mouth. It integrates five basic
 * tastes with texture, temperature, and olfactory information to create
 * flavor perception.
 *
 * Key Features:
 * - Five basic tastes: sweet, salty, sour, bitter, umami
 * - Taste intensity and quality coding
 * - Food reward processing
 * - Disgust response
 * - Integration with olfactory for flavor
 * - Hedonic valuation
 *
 * Anatomical Components:
 * - Taste buds (papillae on tongue)
 * - Nucleus tractus solitarius (NTS)
 * - Ventral posteromedial nucleus (VPM) of thalamus
 * - Insular cortex (primary gustatory cortex)
 * - Orbitofrontal cortex (flavor identity)
 */

#ifndef NIMCP_GUSTATORY_H
#define NIMCP_GUSTATORY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

#define GUST_NUM_BASIC_TASTES       5       /* Sweet, salty, sour, bitter, umami */
#define GUST_MAX_TASTE_RECEPTORS    10000   /* Taste receptor cells */
#define GUST_DEFAULT_INSULA_NEURONS 2000    /* Primary gustatory cortex */
#define GUST_DEFAULT_OFC_NEURONS    1000    /* Secondary gustatory in OFC */
#define GUST_TASTE_PATTERN_DIM      64
#define GUST_ADAPTATION_TAU         5000.0f /* ms - taste adaptation */
#define GUST_BITTER_THRESHOLD       0.0001f /* Very low bitter threshold */
#define GUST_SWEET_THRESHOLD        0.01f
#define GUST_VERSION_MAJOR          1
#define GUST_VERSION_MINOR          0
#define GUST_VERSION_PATCH          0

/*=============================================================================
 * ENUMERATIONS
 *===========================================================================*/

/**
 * @brief Gustatory system status
 */
typedef enum {
    GUST_STATUS_IDLE = 0,
    GUST_STATUS_READY,
    GUST_STATUS_TASTING,
    GUST_STATUS_PROCESSING,
    GUST_STATUS_INTEGRATING,
    GUST_STATUS_ERROR
} gust_status_t;

/**
 * @brief Gustatory error codes
 */
typedef enum {
    GUST_ERROR_NONE = 0,
    GUST_ERROR_INVALID_INPUT,
    GUST_ERROR_SATURATION,
    GUST_ERROR_PROCESSING_FAILED,
    GUST_ERROR_BRIDGE_ERROR,
    GUST_ERROR_INTERNAL
} gust_error_t;

/**
 * @brief Basic taste types
 */
typedef enum {
    TASTE_SWEET = 0,
    TASTE_SALTY,
    TASTE_SOUR,
    TASTE_BITTER,
    TASTE_UMAMI,
    TASTE_COUNT
} basic_taste_t;

/**
 * @brief Tongue region (papillae distribution)
 */
typedef enum {
    TONGUE_TIP = 0,
    TONGUE_FRONT_SIDES,
    TONGUE_BACK_SIDES,
    TONGUE_BACK,
    TONGUE_CENTER,
    TONGUE_REGION_COUNT
} tongue_region_t;

/**
 * @brief Food category for reward processing
 */
typedef enum {
    FOOD_CAT_UNKNOWN = 0,
    FOOD_CAT_CARBOHYDRATE,
    FOOD_CAT_PROTEIN,
    FOOD_CAT_FAT,
    FOOD_CAT_FRUIT,
    FOOD_CAT_VEGETABLE,
    FOOD_CAT_DAIRY,
    FOOD_CAT_TOXIC,
    FOOD_CAT_SPOILED
} food_category_t;

/**
 * @brief Disgust level
 */
typedef enum {
    DISGUST_NONE = 0,
    DISGUST_MILD,
    DISGUST_MODERATE,
    DISGUST_STRONG,
    DISGUST_EXTREME       /* Triggers gag reflex */
} disgust_level_t;

/**
 * @brief Hedonic valence for taste
 */
typedef enum {
    TASTE_HEDONIC_AVERSIVE = 0,
    TASTE_HEDONIC_UNPLEASANT,
    TASTE_HEDONIC_NEUTRAL,
    TASTE_HEDONIC_PLEASANT,
    TASTE_HEDONIC_HIGHLY_PLEASANT
} taste_hedonic_t;

/*=============================================================================
 * STRUCTURES
 *===========================================================================*/

/**
 * @brief Taste stimulus
 */
typedef struct {
    float sweet;            /* 0.0-1.0 intensity */
    float salty;
    float sour;
    float bitter;
    float umami;
    float temperature;      /* Celsius */
    float texture;          /* Smoothness 0.0-1.0 */
    float fat_content;      /* Richness */
    float* olfactory_component;  /* Smell contribution */
    uint32_t olfactory_dim;
} taste_stimulus_t;

/**
 * @brief Taste perception result
 */
typedef struct {
    /* Basic tastes */
    float perceived_sweet;
    float perceived_salty;
    float perceived_sour;
    float perceived_bitter;
    float perceived_umami;

    /* Overall perception */
    float overall_intensity;
    taste_hedonic_t hedonic_value;
    float palatability;         /* 0.0-1.0 */

    /* Flavor (taste + smell) */
    float flavor_complexity;
    float* flavor_profile;
    uint32_t flavor_dim;

    /* Food evaluation */
    food_category_t food_category;
    float nutritive_value_estimate;
    disgust_level_t disgust;

    /* Confidence */
    float identification_confidence;
    uint64_t timestamp;
} taste_perception_t;

/**
 * @brief Taste receptor cell
 */
typedef struct {
    uint32_t receptor_id;
    tongue_region_t region;
    basic_taste_t primary_taste;
    float sensitivity[TASTE_COUNT];
    float activation;
    float adaptation_level;
    uint32_t snn_neuron_id;
} taste_receptor_t;

/**
 * @brief Food reward signal
 */
typedef struct {
    float reward_magnitude;
    float novelty;
    float satiety_modulation;   /* Decreases with satiety */
    float learned_preference;
    float nutritional_value;
    bool is_toxic_warning;
} food_reward_t;

/*=============================================================================
 * BRIDGE STRUCTURES
 *===========================================================================*/

typedef struct {
    bool initialized;
    void* pr_memory_ctx;
    float resonance_frequency;
    float taste_memory_enhancement;
} gust_prime_resonance_bridge_t;

typedef struct {
    bool initialized;
    void* immune_system;
    float health_score;
    bool taste_alteration;      /* Dysgeusia from illness */
} gust_immune_bridge_t;

typedef struct {
    bool initialized;
    void* hypothalamus;
    float hunger_level;
    float satiety_level;
    float metabolic_state;
    float reward_modulation;
} gust_hypothalamus_bridge_t;

typedef struct {
    bool initialized;
    void* amygdala;
    float disgust_response;
    float fear_of_food;
    float learned_aversion;
} gust_amygdala_bridge_t;

typedef struct {
    bool initialized;
    void* olfactory;
    float* current_odor;
    uint32_t odor_dim;
    float flavor_integration;
} gust_olfactory_bridge_t;

typedef struct {
    bool initialized;
    void* insula;
    float interoceptive_state;
    float visceral_response;
} gust_insula_bridge_t;

typedef struct {
    bool initialized;
    void* orbitofrontal;
    float value_signal;
    float expected_reward;
    float reward_prediction_error;
} gust_ofc_bridge_t;

typedef struct {
    bool initialized;
    void* logger;
    uint32_t log_level;
    char log_prefix[32];
} gust_logging_bridge_t;

typedef struct {
    bool initialized;
    void* snn;
    uint32_t* neuron_ids;        /* Mapped SNN neuron IDs for taste cortex */
    uint32_t num_mapped_neurons;
    float snn_activation_gain;
} gust_snn_bridge_t;

typedef struct {
    bool initialized;
    void* plasticity_coordinator;
    void* stdp_context;
    float learning_rate;
    float gustatory_plasticity_gate;   /* Modulates plasticity for taste learning */
    bool hebbian_enabled;
} gust_plasticity_bridge_t;

typedef struct {
    bool initialized;
    void* runtime;                     /* Bio-async runtime context */
    uint32_t subscription_mask;        /* Message types to receive */
    uint32_t messages_sent;
    uint32_t messages_received;
} gust_bio_async_embed_t;

/*=============================================================================
 * CONFIGURATION AND MAIN STRUCTURE
 *===========================================================================*/

/**
 * @brief Gustatory system configuration
 */
typedef struct {
    uint32_t num_insula_neurons;
    uint32_t num_ofc_neurons;
    uint32_t max_receptors;
    float adaptation_rate;
    float bitter_sensitivity;   /* Typically high for safety */
    float sweet_sensitivity;
    bool enable_flavor_integration;
    bool enable_reward_learning;
    bool enable_all_bridges;
} gust_config_t;

/**
 * @brief Gustatory statistics
 */
typedef struct {
    uint32_t tastes_processed;
    uint32_t rewards_generated;
    uint32_t disgust_events;
    float avg_palatability;
    float current_adaptation[TASTE_COUNT];
    uint64_t last_update_time;
} gust_stats_t;

/**
 * @brief Main gustatory cortex structure
 */
typedef struct {
    gust_config_t config;
    gust_status_t status;
    gust_error_t last_error;

    /* Receptors */
    taste_receptor_t* receptors;
    uint32_t num_receptors;

    /* Cortical neurons */
    float* insula_activation;
    uint32_t num_insula;
    float* ofc_activation;
    uint32_t num_ofc;

    /* Current state */
    taste_stimulus_t current_stimulus;
    taste_perception_t current_perception;
    food_reward_t current_reward;

    /* Adaptation per taste */
    float adaptation_level[TASTE_COUNT];

    /* Learned preferences */
    float learned_preferences[TASTE_COUNT];

    /* Bridges */
    gust_prime_resonance_bridge_t prime_resonance_bridge;
    gust_immune_bridge_t immune_bridge;
    gust_hypothalamus_bridge_t hypothalamus_bridge;
    gust_amygdala_bridge_t amygdala_bridge;
    gust_olfactory_bridge_t olfactory_bridge;
    gust_insula_bridge_t insula_bridge;
    gust_ofc_bridge_t ofc_bridge;
    gust_logging_bridge_t logging_bridge;
    gust_snn_bridge_t snn_bridge;
    gust_plasticity_bridge_t plasticity_bridge;
    gust_bio_async_embed_t bio_async_embed;

    /* Statistics */
    uint32_t updates_processed;
    uint64_t creation_time;
    uint64_t last_update_time;
} nimcp_gustatory_t;

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

gust_config_t gust_default_config(void);
nimcp_gustatory_t* gust_create(const gust_config_t* config);
void gust_destroy(nimcp_gustatory_t* gust);
int gust_reset(nimcp_gustatory_t* gust);
int gust_update(nimcp_gustatory_t* gust, float dt);

/*=============================================================================
 * TASTE PROCESSING
 *===========================================================================*/

int gust_process_taste(nimcp_gustatory_t* gust, const taste_stimulus_t* stimulus);
int gust_get_perception(nimcp_gustatory_t* gust, taste_perception_t* perception);
float gust_get_taste_intensity(nimcp_gustatory_t* gust, basic_taste_t taste);
taste_hedonic_t gust_get_hedonic_value(nimcp_gustatory_t* gust);
float gust_get_palatability(nimcp_gustatory_t* gust);
int gust_identify_food(nimcp_gustatory_t* gust, food_category_t* category, float* confidence);

/*=============================================================================
 * FLAVOR INTEGRATION
 *===========================================================================*/

int gust_integrate_flavor(nimcp_gustatory_t* gust, const float* olfactory, uint32_t olf_dim, float* flavor_out, uint32_t* flavor_dim);
float gust_compute_flavor_complexity(nimcp_gustatory_t* gust);

/*=============================================================================
 * FOOD REWARD
 *===========================================================================*/

int gust_compute_reward(nimcp_gustatory_t* gust, food_reward_t* reward);
int gust_apply_satiety_modulation(nimcp_gustatory_t* gust, float satiety_level);
int gust_learn_preference(nimcp_gustatory_t* gust, basic_taste_t taste, float preference_change);

/*=============================================================================
 * DISGUST RESPONSE
 *===========================================================================*/

disgust_level_t gust_evaluate_disgust(nimcp_gustatory_t* gust);
bool gust_is_toxic_warning(nimcp_gustatory_t* gust);
int gust_trigger_disgust_response(nimcp_gustatory_t* gust, disgust_level_t level);

/*=============================================================================
 * ADAPTATION
 *===========================================================================*/

int gust_apply_adaptation(nimcp_gustatory_t* gust, float dt);
float gust_get_adaptation(nimcp_gustatory_t* gust, basic_taste_t taste);
int gust_reset_adaptation(nimcp_gustatory_t* gust);

/*=============================================================================
 * BRIDGE INITIALIZATION
 *===========================================================================*/

int gust_init_prime_resonance_bridge(nimcp_gustatory_t* gust, void* pr_memory);
int gust_init_immune_bridge(nimcp_gustatory_t* gust, void* immune);
int gust_init_hypothalamus_bridge(nimcp_gustatory_t* gust, void* hypothalamus);
int gust_init_amygdala_bridge(nimcp_gustatory_t* gust, void* amygdala);
int gust_init_olfactory_bridge(nimcp_gustatory_t* gust, void* olfactory);
int gust_init_insula_bridge(nimcp_gustatory_t* gust, void* insula);
int gust_init_ofc_bridge(nimcp_gustatory_t* gust, void* ofc);
int gust_init_logging_bridge(nimcp_gustatory_t* gust, void* logger);
int gust_init_snn_bridge(nimcp_gustatory_t* gust, void* snn);
int gust_init_plasticity_bridge(nimcp_gustatory_t* gust, void* plasticity, void* stdp);
int gust_init_bio_async_bridge(nimcp_gustatory_t* gust, void* runtime);

/*=============================================================================
 * BIDIRECTIONAL FLOW
 *===========================================================================*/

int gust_process_incoming(nimcp_gustatory_t* gust);
int gust_send_outgoing(nimcp_gustatory_t* gust);
int gust_bidirectional_update(nimcp_gustatory_t* gust, float dt);
int gust_sync_hypothalamus(nimcp_gustatory_t* gust);
int gust_sync_olfactory(nimcp_gustatory_t* gust);
int gust_sync_ofc(nimcp_gustatory_t* gust);

/*=============================================================================
 * STATUS AND DIAGNOSTICS
 *===========================================================================*/

gust_status_t gust_get_status(nimcp_gustatory_t* gust);
gust_error_t gust_get_last_error(nimcp_gustatory_t* gust);
const char* gust_error_string(gust_error_t error);
const char* gust_status_string(gust_status_t status);
int gust_get_stats(nimcp_gustatory_t* gust, gust_stats_t* stats);
float gust_get_health_status(nimcp_gustatory_t* gust);

/*=============================================================================
 * UTILITY FUNCTIONS
 *===========================================================================*/

const char* gust_taste_name(basic_taste_t taste);
const char* gust_food_category_name(food_category_t category);
const char* gust_disgust_name(disgust_level_t level);
const char* gust_hedonic_name(taste_hedonic_t hedonic);

/*=============================================================================
 * SERIALIZATION
 *===========================================================================*/

size_t gust_get_serialization_size(nimcp_gustatory_t* gust);
int gust_serialize(nimcp_gustatory_t* gust, uint8_t* buffer, size_t size, size_t* written);
nimcp_gustatory_t* gust_deserialize(const uint8_t* buffer, size_t size, size_t* bytes_read);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_GUSTATORY_H */
