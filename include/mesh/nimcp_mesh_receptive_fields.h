/**
 * @file nimcp_mesh_receptive_fields.h
 * @brief Pre-Defined Receptive Fields for Brain Module Categories
 *
 * WHAT: Pre-defined pattern receptive fields for all major NIMCP module categories
 * WHY:  Enable brain-like self-selection without manual pattern configuration
 * HOW:  Static field definitions based on biological tuning curves
 *
 * @author NIMCP Development Team
 * @date 2025-01-31
 * @version 1.0.0
 */

#ifndef NIMCP_MESH_RECEPTIVE_FIELDS_H
#define NIMCP_MESH_RECEPTIVE_FIELDS_H

#include "mesh/nimcp_mesh_pattern_routing.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/error/nimcp_error_codes.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Pattern Dimension Indices
 * ============================================================================ */

/**
 * @brief Pattern dimension indices for standard brain signals
 *
 * 64-dimensional pattern space organized by category
 */
enum mesh_pattern_dim_index {
    /* Neural activity (0-7) */
    MESH_DIM_NEURAL_ACTIVATION = 0,
    MESH_DIM_SPIKE_RATE = 1,
    MESH_DIM_LEARNING_SIGNAL = 2,
    MESH_DIM_PREDICTION_ERROR = 3,
    MESH_DIM_FIRING_PATTERN = 4,

    /* Plasticity (8-15) */
    MESH_DIM_PLASTICITY_STDP = 8,
    MESH_DIM_PLASTICITY_LTP = 9,
    MESH_DIM_PLASTICITY_LTD = 10,
    MESH_DIM_PLASTICITY_HOMEOSTATIC = 11,

    /* Neuromodulation (16-23) */
    MESH_DIM_NEUROMOD_DOPAMINE = 16,
    MESH_DIM_NEUROMOD_SEROTONIN = 17,
    MESH_DIM_NEUROMOD_NOREPINEPHRINE = 18,
    MESH_DIM_NEUROMOD_ACETYLCHOLINE = 19,

    /* Perception (24-31) */
    MESH_DIM_PERCEPTION_VISUAL = 24,
    MESH_DIM_PERCEPTION_AUDITORY = 25,
    MESH_DIM_PERCEPTION_SOMATOSENSORY = 26,
    MESH_DIM_PERCEPTION_MULTIMODAL = 27,

    /* Cognitive (32-39) */
    MESH_DIM_COGNITIVE_REASONING = 32,
    MESH_DIM_COGNITIVE_PLANNING = 33,
    MESH_DIM_ATTENTION = 34,
    MESH_DIM_EXECUTIVE = 35,

    /* Motor (40-47) */
    MESH_DIM_MOTOR_COMMAND = 40,
    MESH_DIM_MOTOR_SEQUENCE = 41,
    MESH_DIM_MOTOR_FEEDBACK = 42,

    /* Security (48-55) */
    MESH_DIM_SECURITY_THREAT = 48,
    MESH_DIM_SECURITY_IMMUNE = 49,
    MESH_DIM_SECURITY_BBB = 50,

    /* System (56-63) */
    MESH_DIM_SYSTEM_HEALTH = 56,
    MESH_DIM_SYSTEM_ERROR = 57,
    MESH_DIM_SYSTEM_LIFECYCLE = 58,
};

/* ============================================================================
 * Pre-Defined Receptive Field Declarations
 * ============================================================================ */

/* Memory System */
extern const mesh_receptive_field_t MESH_RF_HIPPOCAMPUS;
extern const mesh_receptive_field_t MESH_RF_EPISODIC_MEMORY;
extern const mesh_receptive_field_t MESH_RF_SEMANTIC_MEMORY;
extern const mesh_receptive_field_t MESH_RF_WORKING_MEMORY;
extern const mesh_receptive_field_t MESH_RF_PROCEDURAL_MEMORY;

/* Limbic System */
extern const mesh_receptive_field_t MESH_RF_AMYGDALA;
extern const mesh_receptive_field_t MESH_RF_HYPOTHALAMUS;
extern const mesh_receptive_field_t MESH_RF_NUCLEUS_ACCUMBENS;
extern const mesh_receptive_field_t MESH_RF_CINGULATE;

/* Cortex */
extern const mesh_receptive_field_t MESH_RF_PFC_LEFT;
extern const mesh_receptive_field_t MESH_RF_PFC_RIGHT;
extern const mesh_receptive_field_t MESH_RF_DORSOLATERAL_PFC;
extern const mesh_receptive_field_t MESH_RF_ORBITOFRONTAL;
extern const mesh_receptive_field_t MESH_RF_ANTERIOR_CINGULATE;

/* Motor */
extern const mesh_receptive_field_t MESH_RF_MOTOR_CORTEX;
extern const mesh_receptive_field_t MESH_RF_PREMOTOR;
extern const mesh_receptive_field_t MESH_RF_SUPPLEMENTARY_MOTOR;
extern const mesh_receptive_field_t MESH_RF_CEREBELLUM;
extern const mesh_receptive_field_t MESH_RF_BASAL_GANGLIA;

/* Sensory */
extern const mesh_receptive_field_t MESH_RF_VISUAL_CORTEX;
extern const mesh_receptive_field_t MESH_RF_AUDITORY_CORTEX;
extern const mesh_receptive_field_t MESH_RF_SOMATOSENSORY;
extern const mesh_receptive_field_t MESH_RF_THALAMUS;
extern const mesh_receptive_field_t MESH_RF_SUPERIOR_COLLICULUS;

/* Cognitive */
extern const mesh_receptive_field_t MESH_RF_FEP_ORCHESTRATOR;
extern const mesh_receptive_field_t MESH_RF_ATTENTION_MANAGER;
extern const mesh_receptive_field_t MESH_RF_REASONING_ENGINE;
extern const mesh_receptive_field_t MESH_RF_PLANNING_MODULE;
extern const mesh_receptive_field_t MESH_RF_DECISION_MAKING;

/* Security */
extern const mesh_receptive_field_t MESH_RF_BBB;
extern const mesh_receptive_field_t MESH_RF_IMMUNE_SYSTEM;
extern const mesh_receptive_field_t MESH_RF_THREAT_DETECTOR;

/* Plasticity */
extern const mesh_receptive_field_t MESH_RF_STDP;
extern const mesh_receptive_field_t MESH_RF_LTP;
extern const mesh_receptive_field_t MESH_RF_HOMEOSTATIC;
extern const mesh_receptive_field_t MESH_RF_METAPLASTICITY;

/* Glial */
extern const mesh_receptive_field_t MESH_RF_ASTROCYTE;
extern const mesh_receptive_field_t MESH_RF_OLIGODENDROCYTE;

/* Swarm */
extern const mesh_receptive_field_t MESH_RF_GOSSIP_BELIEFS;
extern const mesh_receptive_field_t MESH_RF_SWARM_CONSENSUS;
extern const mesh_receptive_field_t MESH_RF_COLLECTIVE_WORKSPACE;

/* GPU */
extern const mesh_receptive_field_t MESH_RF_GPU_RECOVERY;
extern const mesh_receptive_field_t MESH_RF_GPU_BATCH;
extern const mesh_receptive_field_t MESH_RF_MULTI_GPU;

/* ============================================================================
 * Receptive Field Library API
 * ============================================================================ */

/**
 * @brief Initialize the receptive fields library
 *
 * Must be called before using any MESH_RF_* fields.
 * Called automatically by mesh_bootstrap_create().
 *
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_receptive_fields_init(void);

/**
 * @brief Cleanup receptive field library
 */
void mesh_receptive_fields_cleanup(void);

/**
 * @brief Get a receptive field by name
 *
 * @param name Field name (e.g., "hippocampus", "amygdala")
 * @return Field pointer or NULL if not found
 */
const mesh_receptive_field_t* mesh_receptive_field_get_by_name(const char* name);

/**
 * @brief Get all fields for a category
 *
 * @param category Module category
 * @param fields_out Output array for fields
 * @param max_fields Maximum number of fields
 * @param count_out Output count
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_receptive_fields_get_by_category(
    mesh_adapter_category_t category,
    const mesh_receptive_field_t** fields_out,
    size_t max_fields,
    size_t* count_out
);

/**
 * @brief Create a receptive field for a dimension range
 *
 * @param dim_start Start dimension
 * @param dim_end End dimension (exclusive)
 * @param value Pattern value for these dimensions
 * @param field_out Output field
 * @return NIMCP_SUCCESS on success
 */
nimcp_error_t mesh_receptive_field_create_range(
    size_t dim_start,
    size_t dim_end,
    float value,
    mesh_receptive_field_t* field_out
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MESH_RECEPTIVE_FIELDS_H */
