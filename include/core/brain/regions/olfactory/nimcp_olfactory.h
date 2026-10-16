/**
 * @file nimcp_olfactory.h
 * @brief Olfactory/Piriform Cortex - Smell Processing
 * @version Phase 6: Sensory Processing
 * @date 2026-01-12
 *
 * The olfactory cortex (piriform cortex) processes smell information,
 * uniquely bypassing the thalamus for direct cortical access. It has
 * strong connections to the amygdala (emotional memory) and hippocampus
 * (episodic memory), enabling the powerful emotional associations of smell.
 *
 * Key Features:
 * - Odor receptor pattern processing (combinatorial coding)
 * - Odor identification and discrimination
 * - Olfactory memory (Proustian memory)
 * - Hedonic valence (pleasant/unpleasant)
 * - Cross-modal associations
 * - Sniff-coupled processing
 *
 * Anatomical Components:
 * - Olfactory bulb (glomeruli, mitral cells)
 * - Piriform cortex (primary olfactory cortex)
 * - Orbitofrontal cortex (odor identity, quality)
 * - Amygdala (emotional associations)
 * - Entorhinal cortex (memory encoding)
 */

#ifndef NIMCP_OLFACTORY_H
#define NIMCP_OLFACTORY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

#define OLFACT_MAX_RECEPTORS        400     /* ~400 OR genes in humans */
#define OLFACT_MAX_GLOMERULI        400     /* 1:1 with receptor types */
#define OLFACT_DEFAULT_MITRAL_CELLS 2000    /* Mitral/tufted cells */
#define OLFACT_DEFAULT_PIRIFORM     4000    /* Piriform cortex neurons */
#define OLFACT_MAX_ODORS            1000    /* Recognizable odors */
#define OLFACT_PATTERN_DIM          128     /* Odor pattern dimension */
#define OLFACT_SNIFF_CYCLE_MS       500.0f  /* Typical sniff cycle */
#define OLFACT_ADAPTATION_TAU       2000.0f /* ms - olfactory adaptation */
#define OLFACT_DETECTION_THRESHOLD  0.001f  /* Very low detection */
#define OLFACT_VERSION_MAJOR        1
#define OLFACT_VERSION_MINOR        0
#define OLFACT_VERSION_PATCH        0

/*=============================================================================
 * ENUMERATIONS
 *===========================================================================*/

/**
 * @brief Olfactory system status
 */
typedef enum {
    OLFACT_STATUS_IDLE = 0,
    OLFACT_STATUS_READY,
    OLFACT_STATUS_SNIFFING,
    OLFACT_STATUS_PROCESSING,
    OLFACT_STATUS_IDENTIFYING,
    OLFACT_STATUS_ADAPTING,
    OLFACT_STATUS_ERROR
} olfact_status_t;

/**
 * @brief Olfactory error codes
 */
typedef enum {
    OLFACT_ERROR_NONE = 0,
    OLFACT_ERROR_INVALID_INPUT,
    OLFACT_ERROR_SATURATION,
    OLFACT_ERROR_IDENTIFICATION_FAILED,
    OLFACT_ERROR_MEMORY_FULL,
    OLFACT_ERROR_BRIDGE_ERROR,
    OLFACT_ERROR_INTERNAL
} olfact_error_t;

/**
 * @brief Odor categories
 */
typedef enum {
    ODOR_CAT_UNKNOWN = 0,
    ODOR_CAT_FLORAL,
    ODOR_CAT_FRUITY,
    ODOR_CAT_CITRUS,
    ODOR_CAT_WOODY,
    ODOR_CAT_MINTY,
    ODOR_CAT_SWEET,
    ODOR_CAT_SPICY,
    ODOR_CAT_SAVORY,
    ODOR_CAT_SMOKY,
    ODOR_CAT_CHEMICAL,
    ODOR_CAT_DECAYED,
    ODOR_CAT_PUNGENT,
    ODOR_CAT_COUNT
} odor_category_t;

/**
 * @brief Hedonic valence (pleasantness)
 */
typedef enum {
    HEDONIC_VERY_UNPLEASANT = 0,
    HEDONIC_UNPLEASANT,
    HEDONIC_SLIGHTLY_UNPLEASANT,
    HEDONIC_NEUTRAL,
    HEDONIC_SLIGHTLY_PLEASANT,
    HEDONIC_PLEASANT,
    HEDONIC_VERY_PLEASANT
} hedonic_valence_t;

/**
 * @brief Sniff phase
 */
typedef enum {
    SNIFF_PHASE_BASELINE = 0,
    SNIFF_PHASE_INSPIRATION,
    SNIFF_PHASE_PEAK,
    SNIFF_PHASE_EXPIRATION
} sniff_phase_t;

/*=============================================================================
 * STRUCTURES
 *===========================================================================*/

/**
 * @brief Olfactory receptor activation pattern
 */
typedef struct {
    float receptor_activations[OLFACT_MAX_RECEPTORS];
    uint32_t num_active_receptors;
    float total_activation;
    uint64_t timestamp;
} olfact_receptor_pattern_t;

/**
 * @brief Odor identification result
 */
typedef struct {
    uint32_t odor_id;
    char name[64];
    odor_category_t category;
    hedonic_valence_t valence;
    float intensity;            /* 0.0-1.0 */
    float confidence;           /* Identification confidence */
    float familiarity;          /* How familiar this odor is */
    float* pattern;             /* Neural pattern */
    uint32_t pattern_dim;
} olfact_odor_id_t;

/**
 * @brief Olfactory memory entry
 */
typedef struct {
    uint32_t memory_id;
    olfact_odor_id_t odor;
    float emotional_valence;
    float emotional_arousal;
    uint32_t associated_episode_id;
    float memory_strength;
    uint64_t encoding_time;
    uint64_t last_recall;
    uint32_t recall_count;
    char associated_context[128];
} olfact_memory_t;

/**
 * @brief Sniff cycle state
 */
typedef struct {
    sniff_phase_t phase;
    float cycle_position;       /* 0.0-1.0 within cycle */
    float sniff_strength;       /* Sniff vigor */
    float airflow_rate;
    uint32_t cycle_count;
    uint64_t cycle_start;
} sniff_state_t;

/**
 * @brief Glomerular unit
 */
typedef struct {
    uint32_t glomerulus_id;
    uint32_t receptor_type;     /* Which OR it receives from */
    float activation;
    float lateral_inhibition;
    uint32_t* mitral_cell_ids;
    uint32_t num_mitral_cells;
} olfact_glomerulus_t;

/**
 * @brief Mitral cell
 */
typedef struct {
    uint32_t cell_id;
    uint32_t glomerulus_id;
    float activation;
    float gamma_oscillation;    /* Coupled to sniff cycle */
    float lateral_inhibition;
    float output_to_piriform;
    uint32_t snn_neuron_id;
} olfact_mitral_cell_t;

/*=============================================================================
 * BRIDGE STRUCTURES
 *===========================================================================*/

typedef struct {
    bool initialized;
    void* pr_memory_ctx;
    float resonance_frequency;
    float olfactory_memory_enhancement;
} olfact_prime_resonance_bridge_t;

typedef struct {
    bool initialized;
    void* immune_system;
    float health_score;
    float inflammation_level;
    bool nasal_congestion;
} olfact_immune_bridge_t;

typedef struct {
    bool initialized;
    void* amygdala;
    float emotional_modulation;
    float fear_response;
    float pleasure_response;
} olfact_amygdala_bridge_t;

typedef struct {
    bool initialized;
    void* entorhinal;
    float memory_gate;
    float* context_vector;
    uint32_t context_dim;
} olfact_entorhinal_bridge_t;

typedef struct {
    bool initialized;
    void* orbitofrontal;
    float value_signal;
    float identity_confidence;
} olfact_ofc_bridge_t;

typedef struct {
    bool initialized;
    void* hypothalamus;
    float hunger_modulation;
    float satiety_signal;
    float food_seeking;
} olfact_hypothalamus_bridge_t;

typedef struct {
    bool initialized;
    void* logger;
    uint32_t log_level;
    char log_prefix[32];
} olfact_logging_bridge_t;

typedef struct {
    bool initialized;
    void* snn;
    uint32_t* neuron_ids;        /* Mapped SNN neuron IDs for olfactory bulb */
    uint32_t num_mapped_neurons;
    float snn_activation_gain;
} olfact_snn_bridge_t;

typedef struct {
    bool initialized;
    void* plasticity_coordinator;
    void* stdp_context;
    float learning_rate;
    float olfactory_plasticity_gate;   /* Modulates plasticity for odor learning */
    bool hebbian_enabled;
} olfact_plasticity_bridge_t;

typedef struct {
    bool initialized;
    void* runtime;                     /* Bio-async runtime context */
    uint32_t subscription_mask;        /* Message types to receive */
    uint32_t messages_sent;
    uint32_t messages_received;
} olfact_bio_async_embed_t;

/*=============================================================================
 * CONFIGURATION AND MAIN STRUCTURE
 *===========================================================================*/

/**
 * @brief Olfactory system configuration
 */
typedef struct {
    uint32_t num_mitral_cells;
    uint32_t num_piriform_neurons;
    uint32_t max_stored_odors;
    float adaptation_rate;
    float lateral_inhibition_strength;
    bool enable_sniff_coupling;
    bool enable_emotional_memory;
    bool enable_all_bridges;
} olfact_config_t;

/**
 * @brief Olfactory statistics
 */
typedef struct {
    uint32_t odors_detected;
    uint32_t odors_identified;
    uint32_t memories_stored;
    uint32_t sniff_cycles;
    float avg_detection_confidence;
    float current_adaptation_level;
    uint64_t last_update_time;
} olfact_stats_t;

/**
 * @brief Main olfactory cortex structure
 */
typedef struct {
    olfact_config_t config;
    olfact_status_t status;
    olfact_error_t last_error;

    /* Olfactory bulb */
    olfact_glomerulus_t* glomeruli;
    uint32_t num_glomeruli;
    olfact_mitral_cell_t* mitral_cells;
    uint32_t num_mitral_cells;

    /* Piriform cortex */
    float* piriform_activation;
    uint32_t num_piriform;

    /* Current state */
    olfact_receptor_pattern_t current_pattern;
    olfact_odor_id_t current_odor;
    sniff_state_t sniff_state;
    float adaptation_level;

    /* Memory */
    olfact_memory_t* odor_memories;
    uint32_t num_memories;
    uint32_t max_memories;

    /* Known odors */
    olfact_odor_id_t* known_odors;
    uint32_t num_known_odors;

    /* Bridges */
    olfact_prime_resonance_bridge_t prime_resonance_bridge;
    olfact_immune_bridge_t immune_bridge;
    olfact_amygdala_bridge_t amygdala_bridge;
    olfact_entorhinal_bridge_t entorhinal_bridge;
    olfact_ofc_bridge_t ofc_bridge;
    olfact_hypothalamus_bridge_t hypothalamus_bridge;
    olfact_logging_bridge_t logging_bridge;
    olfact_snn_bridge_t snn_bridge;
    olfact_plasticity_bridge_t plasticity_bridge;
    olfact_bio_async_embed_t bio_async_embed;

    /* Statistics */
    uint32_t updates_processed;
    uint64_t creation_time;
    uint64_t last_update_time;
} nimcp_olfactory_t;

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

olfact_config_t olfact_default_config(void);
nimcp_olfactory_t* olfact_create(const olfact_config_t* config);
void olfact_destroy(nimcp_olfactory_t* olfact);
int olfact_reset(nimcp_olfactory_t* olfact);
int olfact_update(nimcp_olfactory_t* olfact, float dt);

/*=============================================================================
 * ODOR PROCESSING
 *===========================================================================*/

/**
 * @brief Process odor input (receptor activation pattern)
 */
int olfact_process_odor(nimcp_olfactory_t* olfact,
                        const float* receptor_pattern,
                        uint32_t pattern_size,
                        float concentration);

/**
 * @brief Identify current odor
 */
int olfact_identify_odor(nimcp_olfactory_t* olfact,
                         olfact_odor_id_t* result);

/**
 * @brief Get odor category
 */
odor_category_t olfact_classify_odor(nimcp_olfactory_t* olfact);

/**
 * @brief Get hedonic valence
 */
hedonic_valence_t olfact_get_valence(nimcp_olfactory_t* olfact);

/**
 * @brief Get odor intensity
 */
float olfact_get_intensity(nimcp_olfactory_t* olfact);

/**
 * @brief Compare two odor patterns
 */
float olfact_compare_odors(nimcp_olfactory_t* olfact,
                           const float* pattern1,
                           const float* pattern2,
                           uint32_t dim);

/*=============================================================================
 * SNIFF PROCESSING
 *===========================================================================*/

int olfact_start_sniff(nimcp_olfactory_t* olfact, float strength);
int olfact_update_sniff(nimcp_olfactory_t* olfact, float dt);
sniff_phase_t olfact_get_sniff_phase(nimcp_olfactory_t* olfact);
float olfact_get_sniff_modulation(nimcp_olfactory_t* olfact);

/*=============================================================================
 * OLFACTORY MEMORY
 *===========================================================================*/

int olfact_store_memory(nimcp_olfactory_t* olfact,
                        const olfact_odor_id_t* odor,
                        float emotional_valence,
                        float emotional_arousal,
                        const char* context);

int olfact_recall_by_odor(nimcp_olfactory_t* olfact,
                          const float* odor_pattern,
                          uint32_t pattern_dim,
                          olfact_memory_t* memory);

int olfact_recall_by_context(nimcp_olfactory_t* olfact,
                             const char* context,
                             olfact_memory_t* memories,
                             uint32_t max_memories,
                             uint32_t* num_found);

float olfact_get_familiarity(nimcp_olfactory_t* olfact,
                             const float* odor_pattern,
                             uint32_t pattern_dim);

/*=============================================================================
 * ADAPTATION
 *===========================================================================*/

int olfact_apply_adaptation(nimcp_olfactory_t* olfact, float dt);
float olfact_get_adaptation_level(nimcp_olfactory_t* olfact);
int olfact_reset_adaptation(nimcp_olfactory_t* olfact);

/*=============================================================================
 * BRIDGE INITIALIZATION
 *===========================================================================*/

int olfact_init_prime_resonance_bridge(nimcp_olfactory_t* olfact, void* pr_memory);
int olfact_init_immune_bridge(nimcp_olfactory_t* olfact, void* immune);
int olfact_init_amygdala_bridge(nimcp_olfactory_t* olfact, void* amygdala);
int olfact_init_entorhinal_bridge(nimcp_olfactory_t* olfact, void* entorhinal);
int olfact_init_ofc_bridge(nimcp_olfactory_t* olfact, void* ofc);
int olfact_init_hypothalamus_bridge(nimcp_olfactory_t* olfact, void* hypothalamus);
int olfact_init_logging_bridge(nimcp_olfactory_t* olfact, void* logger);
int olfact_init_snn_bridge(nimcp_olfactory_t* olfact, void* snn);
int olfact_init_plasticity_bridge(nimcp_olfactory_t* olfact, void* plasticity, void* stdp);
int olfact_init_bio_async_bridge(nimcp_olfactory_t* olfact, void* runtime);

/*=============================================================================
 * BIDIRECTIONAL FLOW
 *===========================================================================*/

int olfact_process_incoming(nimcp_olfactory_t* olfact);
int olfact_send_outgoing(nimcp_olfactory_t* olfact);
int olfact_bidirectional_update(nimcp_olfactory_t* olfact, float dt);
int olfact_sync_amygdala(nimcp_olfactory_t* olfact);
int olfact_sync_entorhinal(nimcp_olfactory_t* olfact);
int olfact_sync_ofc(nimcp_olfactory_t* olfact);

/*=============================================================================
 * STATUS AND DIAGNOSTICS
 *===========================================================================*/

olfact_status_t olfact_get_status(nimcp_olfactory_t* olfact);
olfact_error_t olfact_get_last_error(nimcp_olfactory_t* olfact);
const char* olfact_error_string(olfact_error_t error);
const char* olfact_status_string(olfact_status_t status);
int olfact_get_stats(nimcp_olfactory_t* olfact, olfact_stats_t* stats);
float olfact_get_health_status(nimcp_olfactory_t* olfact);

/*=============================================================================
 * UTILITY FUNCTIONS
 *===========================================================================*/

const char* olfact_category_name(odor_category_t category);
const char* olfact_valence_name(hedonic_valence_t valence);
const char* olfact_sniff_phase_name(sniff_phase_t phase);

/*=============================================================================
 * SERIALIZATION
 *===========================================================================*/

size_t olfact_get_serialization_size(nimcp_olfactory_t* olfact);
int olfact_serialize(nimcp_olfactory_t* olfact, uint8_t* buffer, size_t size, size_t* written);
nimcp_olfactory_t* olfact_deserialize(const uint8_t* buffer, size_t size, size_t* bytes_read);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_OLFACTORY_H */
