/**
 * @file nimcp_mesh_receptive_fields.c
 * @brief Pre-Defined Receptive Fields for Brain Module Categories
 */

#include "mesh/nimcp_mesh_receptive_fields.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdbool.h>

/* ============================================================================
 * Helper Macros for Defining Fields
 * ============================================================================ */

/**
 * @brief Define a receptive field with a single primary pattern
 */
#define DEFINE_FIELD(name, dim1, val1, thresh, sharp) \
    const mesh_receptive_field_t MESH_RF_##name = { \
        .preferred = { \
            { .vector = { [(dim1)] = (val1) }, .magnitude = 1.0f } \
        }, \
        .pattern_count = 1, \
        .threshold = (thresh), \
        .sharpness = (sharp), \
        .learned_bias = 0.0f, \
        .neuromod_gain = 1.0f \
    }

/**
 * @brief Define a receptive field with two primary patterns
 */
#define DEFINE_FIELD2(name, dim1, val1, dim2, val2, thresh, sharp) \
    const mesh_receptive_field_t MESH_RF_##name = { \
        .preferred = { \
            { .vector = { [(dim1)] = (val1), [(dim2)] = (val2) }, .magnitude = 1.0f } \
        }, \
        .pattern_count = 1, \
        .threshold = (thresh), \
        .sharpness = (sharp), \
        .learned_bias = 0.0f, \
        .neuromod_gain = 1.0f \
    }

/* ============================================================================
 * Memory System Receptive Fields
 * ============================================================================ */

DEFINE_FIELD2(HIPPOCAMPUS, MESH_DIM_COGNITIVE_PLANNING, 0.8f,
              MESH_DIM_ATTENTION, 0.6f, 0.4f, 0.7f);
DEFINE_FIELD2(EPISODIC_MEMORY, MESH_DIM_COGNITIVE_PLANNING, 0.7f,
              MESH_DIM_ATTENTION, 0.5f, 0.4f, 0.6f);
DEFINE_FIELD(SEMANTIC_MEMORY, MESH_DIM_COGNITIVE_REASONING, 0.7f, 0.4f, 0.6f);
DEFINE_FIELD2(WORKING_MEMORY, MESH_DIM_ATTENTION, 0.8f,
              MESH_DIM_EXECUTIVE, 0.6f, 0.3f, 0.8f);
DEFINE_FIELD2(PROCEDURAL_MEMORY, MESH_DIM_MOTOR_SEQUENCE, 0.7f,
              MESH_DIM_LEARNING_SIGNAL, 0.5f, 0.4f, 0.7f);

/* ============================================================================
 * Limbic System Receptive Fields
 * ============================================================================ */

DEFINE_FIELD2(AMYGDALA, MESH_DIM_SECURITY_THREAT, 0.9f,
              MESH_DIM_NEUROMOD_NOREPINEPHRINE, 0.7f, 0.2f, 0.9f);
DEFINE_FIELD2(HYPOTHALAMUS, MESH_DIM_NEUROMOD_DOPAMINE, 0.7f,
              MESH_DIM_SYSTEM_HEALTH, 0.6f, 0.3f, 0.7f);
DEFINE_FIELD2(NUCLEUS_ACCUMBENS, MESH_DIM_NEUROMOD_DOPAMINE, 0.9f,
              MESH_DIM_LEARNING_SIGNAL, 0.6f, 0.3f, 0.8f);
DEFINE_FIELD2(CINGULATE, MESH_DIM_PREDICTION_ERROR, 0.8f,
              MESH_DIM_ATTENTION, 0.6f, 0.3f, 0.7f);

/* ============================================================================
 * Cortex Receptive Fields
 * ============================================================================ */

DEFINE_FIELD2(PFC_LEFT, MESH_DIM_COGNITIVE_REASONING, 0.8f,
              MESH_DIM_EXECUTIVE, 0.7f, 0.3f, 0.8f);
DEFINE_FIELD2(PFC_RIGHT, MESH_DIM_COGNITIVE_PLANNING, 0.8f,
              MESH_DIM_ATTENTION, 0.6f, 0.3f, 0.7f);
DEFINE_FIELD2(DORSOLATERAL_PFC, MESH_DIM_EXECUTIVE, 0.9f,
              MESH_DIM_COGNITIVE_REASONING, 0.7f, 0.3f, 0.8f);
DEFINE_FIELD2(ORBITOFRONTAL, MESH_DIM_NEUROMOD_DOPAMINE, 0.7f,
              MESH_DIM_PREDICTION_ERROR, 0.6f, 0.3f, 0.7f);
DEFINE_FIELD2(ANTERIOR_CINGULATE, MESH_DIM_PREDICTION_ERROR, 0.9f,
              MESH_DIM_EXECUTIVE, 0.6f, 0.3f, 0.8f);

/* ============================================================================
 * Motor System Receptive Fields
 * ============================================================================ */

DEFINE_FIELD2(MOTOR_CORTEX, MESH_DIM_MOTOR_COMMAND, 0.9f,
              MESH_DIM_MOTOR_SEQUENCE, 0.7f, 0.3f, 0.9f);
DEFINE_FIELD2(PREMOTOR, MESH_DIM_MOTOR_SEQUENCE, 0.8f,
              MESH_DIM_COGNITIVE_PLANNING, 0.6f, 0.3f, 0.8f);
DEFINE_FIELD2(SUPPLEMENTARY_MOTOR, MESH_DIM_MOTOR_SEQUENCE, 0.8f,
              MESH_DIM_MOTOR_COMMAND, 0.5f, 0.3f, 0.7f);
DEFINE_FIELD2(CEREBELLUM, MESH_DIM_MOTOR_FEEDBACK, 0.9f,
              MESH_DIM_PREDICTION_ERROR, 0.7f, 0.2f, 0.9f);
DEFINE_FIELD2(BASAL_GANGLIA, MESH_DIM_MOTOR_COMMAND, 0.8f,
              MESH_DIM_NEUROMOD_DOPAMINE, 0.8f, 0.3f, 0.8f);

/* ============================================================================
 * Sensory Receptive Fields
 * ============================================================================ */

DEFINE_FIELD(VISUAL_CORTEX, MESH_DIM_PERCEPTION_VISUAL, 0.9f, 0.3f, 0.9f);
DEFINE_FIELD(AUDITORY_CORTEX, MESH_DIM_PERCEPTION_AUDITORY, 0.9f, 0.3f, 0.9f);
DEFINE_FIELD(SOMATOSENSORY, MESH_DIM_PERCEPTION_SOMATOSENSORY, 0.9f, 0.3f, 0.9f);
DEFINE_FIELD2(THALAMUS, MESH_DIM_PERCEPTION_MULTIMODAL, 0.7f,
              MESH_DIM_ATTENTION, 0.6f, 0.2f, 0.6f);
DEFINE_FIELD2(SUPERIOR_COLLICULUS, MESH_DIM_PERCEPTION_VISUAL, 0.7f,
              MESH_DIM_MOTOR_COMMAND, 0.5f, 0.3f, 0.7f);

/* ============================================================================
 * Cognitive Receptive Fields
 * ============================================================================ */

DEFINE_FIELD2(FEP_ORCHESTRATOR, MESH_DIM_PREDICTION_ERROR, 0.9f,
              MESH_DIM_EXECUTIVE, 0.7f, 0.2f, 0.8f);
DEFINE_FIELD(ATTENTION_MANAGER, MESH_DIM_ATTENTION, 0.9f, 0.2f, 0.9f);
DEFINE_FIELD(REASONING_ENGINE, MESH_DIM_COGNITIVE_REASONING, 0.9f, 0.3f, 0.8f);
DEFINE_FIELD(PLANNING_MODULE, MESH_DIM_COGNITIVE_PLANNING, 0.9f, 0.3f, 0.8f);
DEFINE_FIELD2(DECISION_MAKING, MESH_DIM_EXECUTIVE, 0.8f,
              MESH_DIM_COGNITIVE_REASONING, 0.6f, 0.3f, 0.7f);

/* ============================================================================
 * Security Receptive Fields
 * ============================================================================ */

DEFINE_FIELD(BBB, MESH_DIM_SECURITY_BBB, 0.9f, 0.2f, 0.9f);
DEFINE_FIELD(IMMUNE_SYSTEM, MESH_DIM_SECURITY_IMMUNE, 0.9f, 0.2f, 0.9f);
DEFINE_FIELD(THREAT_DETECTOR, MESH_DIM_SECURITY_THREAT, 0.9f, 0.2f, 0.9f);

/* ============================================================================
 * Plasticity Receptive Fields
 * ============================================================================ */

DEFINE_FIELD(STDP, MESH_DIM_PLASTICITY_STDP, 0.9f, 0.3f, 0.8f);
DEFINE_FIELD(LTP, MESH_DIM_PLASTICITY_LTP, 0.9f, 0.3f, 0.8f);
DEFINE_FIELD(HOMEOSTATIC, MESH_DIM_PLASTICITY_HOMEOSTATIC, 0.8f, 0.3f, 0.7f);
DEFINE_FIELD2(METAPLASTICITY, MESH_DIM_PLASTICITY_STDP, 0.7f,
              MESH_DIM_PLASTICITY_LTP, 0.6f, 0.3f, 0.6f);

/* ============================================================================
 * Glial Receptive Fields
 * ============================================================================ */

DEFINE_FIELD(ASTROCYTE, MESH_DIM_SYSTEM_HEALTH, 0.7f, 0.3f, 0.6f);
DEFINE_FIELD2(OLIGODENDROCYTE, MESH_DIM_NEURAL_ACTIVATION, 0.6f,
              MESH_DIM_SYSTEM_HEALTH, 0.5f, 0.4f, 0.5f);

/* ============================================================================
 * Swarm Receptive Fields
 * ============================================================================ */

DEFINE_FIELD(GOSSIP_BELIEFS, MESH_DIM_PREDICTION_ERROR, 0.7f, 0.3f, 0.6f);
DEFINE_FIELD(SWARM_CONSENSUS, MESH_DIM_EXECUTIVE, 0.7f, 0.3f, 0.6f);
DEFINE_FIELD2(COLLECTIVE_WORKSPACE, MESH_DIM_ATTENTION, 0.7f,
              MESH_DIM_EXECUTIVE, 0.5f, 0.3f, 0.6f);

/* ============================================================================
 * GPU Receptive Fields
 * ============================================================================ */

DEFINE_FIELD(GPU_RECOVERY, MESH_DIM_SYSTEM_ERROR, 0.9f, 0.2f, 0.9f);
DEFINE_FIELD(GPU_BATCH, MESH_DIM_NEURAL_ACTIVATION, 0.7f, 0.3f, 0.7f);
DEFINE_FIELD(MULTI_GPU, MESH_DIM_NEURAL_ACTIVATION, 0.7f, 0.3f, 0.7f);

/* ============================================================================
 * Field Name Lookup Table
 * ============================================================================ */

typedef struct {
    const char* name;
    const mesh_receptive_field_t* field;
} field_entry_t;

static const field_entry_t g_field_table[] = {
    /* Memory */
    {"hippocampus", &MESH_RF_HIPPOCAMPUS},
    {"episodic_memory", &MESH_RF_EPISODIC_MEMORY},
    {"semantic_memory", &MESH_RF_SEMANTIC_MEMORY},
    {"working_memory", &MESH_RF_WORKING_MEMORY},
    {"procedural_memory", &MESH_RF_PROCEDURAL_MEMORY},
    /* Limbic */
    {"amygdala", &MESH_RF_AMYGDALA},
    {"hypothalamus", &MESH_RF_HYPOTHALAMUS},
    {"nucleus_accumbens", &MESH_RF_NUCLEUS_ACCUMBENS},
    {"cingulate", &MESH_RF_CINGULATE},
    /* Cortex */
    {"pfc_left", &MESH_RF_PFC_LEFT},
    {"pfc_right", &MESH_RF_PFC_RIGHT},
    {"dorsolateral_pfc", &MESH_RF_DORSOLATERAL_PFC},
    {"orbitofrontal", &MESH_RF_ORBITOFRONTAL},
    {"anterior_cingulate", &MESH_RF_ANTERIOR_CINGULATE},
    /* Motor */
    {"motor_cortex", &MESH_RF_MOTOR_CORTEX},
    {"premotor", &MESH_RF_PREMOTOR},
    {"supplementary_motor", &MESH_RF_SUPPLEMENTARY_MOTOR},
    {"cerebellum", &MESH_RF_CEREBELLUM},
    {"basal_ganglia", &MESH_RF_BASAL_GANGLIA},
    /* Sensory */
    {"visual_cortex", &MESH_RF_VISUAL_CORTEX},
    {"auditory_cortex", &MESH_RF_AUDITORY_CORTEX},
    {"somatosensory", &MESH_RF_SOMATOSENSORY},
    {"thalamus", &MESH_RF_THALAMUS},
    {"superior_colliculus", &MESH_RF_SUPERIOR_COLLICULUS},
    /* Cognitive */
    {"fep_orchestrator", &MESH_RF_FEP_ORCHESTRATOR},
    {"attention_manager", &MESH_RF_ATTENTION_MANAGER},
    {"reasoning_engine", &MESH_RF_REASONING_ENGINE},
    {"planning_module", &MESH_RF_PLANNING_MODULE},
    {"decision_making", &MESH_RF_DECISION_MAKING},
    /* Security */
    {"bbb", &MESH_RF_BBB},
    {"immune_system", &MESH_RF_IMMUNE_SYSTEM},
    {"threat_detector", &MESH_RF_THREAT_DETECTOR},
    /* Plasticity */
    {"stdp", &MESH_RF_STDP},
    {"ltp", &MESH_RF_LTP},
    {"homeostatic", &MESH_RF_HOMEOSTATIC},
    {"metaplasticity", &MESH_RF_METAPLASTICITY},
    /* Glial */
    {"astrocyte", &MESH_RF_ASTROCYTE},
    {"oligodendrocyte", &MESH_RF_OLIGODENDROCYTE},
    /* Swarm */
    {"gossip_beliefs", &MESH_RF_GOSSIP_BELIEFS},
    {"swarm_consensus", &MESH_RF_SWARM_CONSENSUS},
    {"collective_workspace", &MESH_RF_COLLECTIVE_WORKSPACE},
    /* GPU */
    {"gpu_recovery", &MESH_RF_GPU_RECOVERY},
    {"gpu_batch", &MESH_RF_GPU_BATCH},
    {"multi_gpu", &MESH_RF_MULTI_GPU},
    /* Sentinel */
    {NULL, NULL}
};

#define FIELD_TABLE_SIZE (sizeof(g_field_table) / sizeof(g_field_table[0]) - 1)

/* ============================================================================
 * Library State
 * ============================================================================ */

static bool g_initialized = false;

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

nimcp_error_t mesh_receptive_fields_init(void) {
    if (g_initialized) {
        return NIMCP_SUCCESS;  /* Already initialized */
    }

    LOG_DEBUG("Initializing receptive fields library (%zu fields)", FIELD_TABLE_SIZE);
    g_initialized = true;

    return NIMCP_SUCCESS;
}

void mesh_receptive_fields_cleanup(void) {
    g_initialized = false;
    LOG_DEBUG("Receptive fields library cleaned up");
}

const mesh_receptive_field_t* mesh_receptive_field_get_by_name(const char* name) {
    if (!name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_receptive_field_get_by_name: name is NULL");
        return NULL;
    }

    for (size_t i = 0; g_field_table[i].name != NULL; i++) {
        if (strcmp(g_field_table[i].name, name) == 0) {
            return g_field_table[i].field;
        }
    }

    /* Not found is normal lookup result, not an error */
    return NULL;
}

nimcp_error_t mesh_receptive_fields_get_by_category(
    mesh_adapter_category_t category,
    const mesh_receptive_field_t** fields_out,
    size_t max_fields,
    size_t* count_out
) {
    if (!fields_out || !count_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_receptive_fields: NULL pointer parameter");
        return NIMCP_ERROR_NULL_POINTER;
    }

    *count_out = 0;

    /* Map category to relevant fields */
    const mesh_receptive_field_t* category_fields[16];
    size_t field_count = 0;

    switch (category) {
    case MESH_ADAPTER_CATEGORY_MEMORY:
        category_fields[field_count++] = &MESH_RF_HIPPOCAMPUS;
        category_fields[field_count++] = &MESH_RF_EPISODIC_MEMORY;
        category_fields[field_count++] = &MESH_RF_WORKING_MEMORY;
        break;

    case MESH_ADAPTER_CATEGORY_SUBCORTICAL:
        category_fields[field_count++] = &MESH_RF_AMYGDALA;
        category_fields[field_count++] = &MESH_RF_HYPOTHALAMUS;
        category_fields[field_count++] = &MESH_RF_NUCLEUS_ACCUMBENS;
        category_fields[field_count++] = &MESH_RF_THALAMUS;
        category_fields[field_count++] = &MESH_RF_BASAL_GANGLIA;
        break;

    case MESH_ADAPTER_CATEGORY_COGNITIVE:
        category_fields[field_count++] = &MESH_RF_FEP_ORCHESTRATOR;
        category_fields[field_count++] = &MESH_RF_ATTENTION_MANAGER;
        category_fields[field_count++] = &MESH_RF_REASONING_ENGINE;
        break;

    case MESH_ADAPTER_CATEGORY_MOTOR:
        category_fields[field_count++] = &MESH_RF_MOTOR_CORTEX;
        category_fields[field_count++] = &MESH_RF_CEREBELLUM;
        category_fields[field_count++] = &MESH_RF_BASAL_GANGLIA;
        break;

    case MESH_ADAPTER_CATEGORY_PERCEPTION:
        category_fields[field_count++] = &MESH_RF_VISUAL_CORTEX;
        category_fields[field_count++] = &MESH_RF_AUDITORY_CORTEX;
        category_fields[field_count++] = &MESH_RF_THALAMUS;
        break;

    case MESH_ADAPTER_CATEGORY_SECURITY:
        category_fields[field_count++] = &MESH_RF_BBB;
        category_fields[field_count++] = &MESH_RF_IMMUNE_SYSTEM;
        category_fields[field_count++] = &MESH_RF_THREAT_DETECTOR;
        break;

    case MESH_ADAPTER_CATEGORY_GPU:
        category_fields[field_count++] = &MESH_RF_GPU_RECOVERY;
        category_fields[field_count++] = &MESH_RF_GPU_BATCH;
        break;

    case MESH_ADAPTER_CATEGORY_PLASTICITY:
        category_fields[field_count++] = &MESH_RF_STDP;
        category_fields[field_count++] = &MESH_RF_LTP;
        category_fields[field_count++] = &MESH_RF_HOMEOSTATIC;
        break;

    case MESH_ADAPTER_CATEGORY_GLIAL:
        category_fields[field_count++] = &MESH_RF_ASTROCYTE;
        category_fields[field_count++] = &MESH_RF_OLIGODENDROCYTE;
        break;

    case MESH_ADAPTER_CATEGORY_SWARM:
        category_fields[field_count++] = &MESH_RF_GOSSIP_BELIEFS;
        category_fields[field_count++] = &MESH_RF_SWARM_CONSENSUS;
        break;

    default:
        return NIMCP_SUCCESS;  /* Empty result for unknown category */
    }

    /* Copy to output */
    size_t copy_count = (field_count < max_fields) ? field_count : max_fields;
    for (size_t i = 0; i < copy_count; i++) {
        fields_out[i] = category_fields[i];
    }
    *count_out = copy_count;

    return NIMCP_SUCCESS;
}

nimcp_error_t mesh_receptive_field_create_range(
    size_t dim_start,
    size_t dim_end,
    float value,
    mesh_receptive_field_t* field_out
) {
    if (!field_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mesh_receptive_fields: NULL pointer parameter");
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (dim_start >= MESH_PATTERN_DIM || dim_end > MESH_PATTERN_DIM) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mesh_receptive_fields: invalid parameter");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    memset(field_out, 0, sizeof(*field_out));

    /* Set pattern values for the range */
    for (size_t i = dim_start; i < dim_end; i++) {
        field_out->preferred[0].vector[i] = value;
    }
    field_out->preferred[0].magnitude = 1.0f;
    field_out->pattern_count = 1;
    field_out->threshold = 0.3f;
    field_out->sharpness = 0.7f;
    field_out->learned_bias = 0.0f;
    field_out->neuromod_gain = 1.0f;

    return NIMCP_SUCCESS;
}
