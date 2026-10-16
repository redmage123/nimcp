//=============================================================================
// nimcp_financial_jepa_bridge.c - Financial JEPA Masking Bridge Implementation
//=============================================================================
/**
 * @file nimcp_financial_jepa_bridge.c
 * @brief Implementation of JEPA-style masked prediction for financial factors
 *
 * @author NIMCP Development Team
 * @date 2026-01-29
 */

#include "cognitive/parietal/nimcp_financial_jepa_bridge.h"
#include "constants/nimcp_buffer_constants.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/error/nimcp_error_codes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_learning_constants.h"
#include "constants/nimcp_threshold_constants.h"
#include "constants/nimcp_dimension_constants.h"
#include "constants/nimcp_math_constants.h"
#include "utils/math/nimcp_math_helpers.h"

/* Health agent: using pre-existing custom implementation */
static nimcp_health_agent_t* g_fin_jepa_health_agent = NULL;


/* Forward declarations for immune/BBB validation stubs */
struct brain_immune_system;
typedef struct brain_immune_system brain_immune_system_t;
extern int brain_immune_validate_operation(brain_immune_system_t* immune,
                                            const char* operation,
                                            uint32_t severity);
extern int brain_immune_present_antigen(brain_immune_system_t* immune,
                                         int source,
                                         const uint8_t* epitope,
                                         size_t epitope_len,
                                         uint32_t severity,
                                         uint32_t source_node,
                                         uint32_t* antigen_id);

struct bbb_system_struct;
typedef struct bbb_system_struct* bbb_system_t;
extern int bbb_validate_data(bbb_system_t bbb, const void* data,
                              size_t size, const char* context);

/* Stub declarations for subsystem integration globals */
static void* g_fin_jepa_bridge_immune = NULL;
static void* g_fin_jepa_bridge_bbb = NULL;

BRIDGE_DEFINE_MESH_REGISTRATION(fin_jepa, MESH_ADAPTER_CATEGORY_COGNITIVE)


// KG Wiring Integration (Change Set 1)
//=============================================================================
struct kg_wiring;
typedef struct kg_wiring kg_wiring_t;

/* KG message type defines */
#define KG_MSG_FIN_JEPA_REQUEST         "FIN_JEPA_REQUEST"
#define KG_MSG_FIN_JEPA_RESPONSE        "FIN_JEPA_RESPONSE"
#define KG_MSG_FIN_JEPA_ERROR           "FIN_JEPA_ERROR"
#define KG_MSG_FIN_JEPA_PREDICTION      "FIN_JEPA_PREDICTION"
#define KG_MSG_FIN_JEPA_CROSS_MODAL     "FIN_JEPA_CROSS_MODAL"

//=============================================================================
// Thread-local Error
//=============================================================================

static _Thread_local char fin_jepa_last_error[NIMCP_ERROR_BUFFER_SIZE] = {0};

static void set_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(fin_jepa_last_error, sizeof(fin_jepa_last_error), fmt, args);
    va_end(args);
}

//=============================================================================
// Heartbeat Helpers
//=============================================================================

static inline void fin_jepa_heartbeat(const char* operation, float progress) {
    if (g_fin_jepa_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_fin_jepa_health_agent, operation, progress);
    }
}

static inline void fin_jepa_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_fin_jepa_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_fin_jepa_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_fin_jepa_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

//=============================================================================
// Internal Structure
//=============================================================================

struct financial_jepa_bridge {
    fin_jepa_config_t config;
    fin_jepa_bridge_stats_t stats;

    /* Modulation state */
    float inflammation;
    float fatigue;

    /* Encoder weights (context encoder) */
    float* encoder_weights;         /* [num_factors * embed_dim] */
    float* encoder_bias;            /* [embed_dim] */

    /* Predictor network weights */
    float* predictor_w1;            /* [embed_dim * hidden_dim] */
    float* predictor_b1;            /* [hidden_dim] */
    float* predictor_w2;            /* [hidden_dim * embed_dim] */
    float* predictor_b2;            /* [embed_dim] */

    /* Target encoder weights (EMA of context encoder) */
    float* target_encoder_weights;  /* [num_factors * embed_dim] */
    float* target_encoder_bias;     /* [embed_dim] */

    /* Decoder weights */
    float* decoder_weights;         /* [embed_dim * num_factors] */
    float* decoder_bias;            /* [num_factors] */

    /* Cross-modal projection matrices */
    float* cross_modal_proj[FIN_MODALITY_COUNT]; /* Projection per modality */
    uint32_t modality_dims[FIN_MODALITY_COUNT];  /* Dimension per modality */

    /* Factor-to-modality mapping */
    fin_factor_modality_t* factor_modalities;   /* [num_factors] */

    /* Working buffers */
    float* visible_embedding;       /* [embed_dim] */
    float* predicted_embedding;     /* [embed_dim] */
    float* hidden_buffer;           /* [hidden_dim] */

    /* Subsystem pointers */
    brain_immune_system_t* immune;
    bbb_system_t bbb;
    kg_wiring_t* kg_wiring;
    nimcp_health_agent_t* health_agent;
    void* logger;

    /* Security validation flags */
    bool enable_bbb_validation;
    bool enable_immune_validation;

    /* RNG state */
    uint64_t rng_state;

    /* Operational state */
    fin_jepa_op_state_t operational_state;
};

//=============================================================================
// RNG Utilities
//=============================================================================

static float jepa_randf(uint64_t* state) {
    *state = *state * 6364136223846793005ULL + 1442695040888963407ULL;
    return (float)((*state >> 33) & 0x7FFFFFFF) / (float)0x7FFFFFFF;
}

static float jepa_randn(uint64_t* state) {
    float u1 = jepa_randf(state) + 1e-10f;
    float u2 = jepa_randf(state);
    return sqrtf(-2.0f * logf(u1)) * cosf(NIMCP_TWO_PI_F * u2);
}

static float relu(float x) {
    return x > 0.0f ? x : 0.0f;
}

static float sigmoid(float x) {
    if (x < -20.0f) return 0.0f;
    if (x > 20.0f) return 1.0f;
    return 1.0f / (1.0f + expf(-x));
}

static void softmax(float* values, uint32_t n) {
    if (n == 0) return;

    float max_val = values[0];
    for (uint32_t i = 1; i < n; i++) {
        if (values[i] > max_val) max_val = values[i];
    }

    float sum = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        values[i] = expf(values[i] - max_val);
        sum += values[i];
    }

    if (sum > 1e-10f) {
        for (uint32_t i = 0; i < n; i++) {
            values[i] /= sum;
        }
    }
}

static float dot_product(const float* a, const float* b, uint32_t n) {
    float sum = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        sum += a[i] * b[i];
    }
    return sum;
}

static float vector_norm(const float* v, uint32_t n) {
    float sum = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        sum += v[i] * v[i];
    }
    return sqrtf(sum + 1e-10f);
}

static void normalize_vector(float* v, uint32_t n) {
    float norm = vector_norm(v, n);
    if (norm > 1e-10f) {
        for (uint32_t i = 0; i < n; i++) {
            v[i] /= norm;
        }
    }
}

//=============================================================================
// Global Validation Helper
//=============================================================================

static int fin_jepa_validate_global(const char* operation) {
    if (g_fin_jepa_bridge_immune) {
        int rc = brain_immune_validate_operation(g_fin_jepa_bridge_immune, operation, 5);
        if (rc != 0) {
            set_error("fin_jepa: immune validation failed for %s", operation);
            return FIN_JEPA_ERR_SUBSYSTEM;
        }
    }
    if (g_fin_jepa_bridge_bbb) {
        int rc = bbb_validate_data(g_fin_jepa_bridge_bbb, NULL, 0, operation);
        if (rc != 0) {
            set_error("fin_jepa: BBB validation failed for %s", operation);
            return FIN_JEPA_ERR_SUBSYSTEM;
        }
    }
    return FIN_JEPA_ERR_OK;
}

//=============================================================================
// Instance-Level Validation Helper
//=============================================================================

static int fin_jepa_validate_instance(financial_jepa_bridge_t* bridge,
                                       const char* operation) {
    if (!bridge) return FIN_JEPA_ERR_NULL;

    if (bridge->enable_bbb_validation && bridge->bbb) {
        int rc = bbb_validate_data(bridge->bbb, NULL, 0, operation);
        if (rc != 0) {
            set_error("BBB validation failed for %s", operation);
            bridge->stats.bbb_validations++;
            NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_BBB_VALIDATION,
                "fin_jepa: BBB validation failed for %s", operation);
            return FIN_JEPA_ERR_VALIDATION;
        }
        bridge->stats.bbb_validations++;
    }

    if (bridge->enable_immune_validation && bridge->immune) {
        int rc = brain_immune_validate_operation(bridge->immune, operation, 5);
        if (rc != 0) {
            set_error("Immune validation failed for %s", operation);
            bridge->stats.immune_checks++;
            NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_BBB_VALIDATION,
                "fin_jepa: immune validation failed for %s", operation);
            return FIN_JEPA_ERR_VALIDATION;
        }
        bridge->stats.immune_checks++;
    }

    return FIN_JEPA_ERR_OK;
}

//=============================================================================
// Antigen Presentation Helper
//=============================================================================

static void fin_jepa_present_antigen(financial_jepa_bridge_t* bridge,
                                      const char* anomaly, uint32_t severity) {
    if (bridge && bridge->immune) {
        uint8_t sig[64] = {0};
        snprintf((char*)sig, sizeof(sig), "fin_jepa:%s", anomaly);
        uint32_t antigen_id = 0;
        brain_immune_present_antigen(bridge->immune, 0, sig, strlen((char*)sig),
                                      severity, 0, &antigen_id);
    }
}

//=============================================================================
// KG Publish Helper
//=============================================================================

static int fin_jepa_kg_publish(financial_jepa_bridge_t* bridge,
                                const char* msg_type,
                                const void* payload, size_t size) {
    if (bridge && bridge->kg_wiring) {
        /* kg_wiring_publish would be called here */
        (void)msg_type; (void)payload; (void)size;
        bridge->stats.kg_messages_sent++;
        return 0;
    }
    return 0;
}

//=============================================================================
// Weight Initialization
//=============================================================================

static void init_xavier_weights(float* weights, uint32_t rows, uint32_t cols, uint64_t* rng) {
    float scale = sqrtf(2.0f / (float)(rows + cols));
    for (uint32_t i = 0; i < rows * cols; i++) {
        weights[i] = jepa_randn(rng) * scale;
    }
}

static void init_zero_bias(float* bias, uint32_t size) {
    memset(bias, 0, size * sizeof(float));
}

//=============================================================================
// Lifecycle API
//=============================================================================

fin_jepa_config_t financial_jepa_bridge_default_config(void) {
    fin_jepa_heartbeat("default_config", 0.0f);

    fin_jepa_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));

    /* Model settings */
    cfg.num_factors = 32;
    cfg.embed_dim = FIN_JEPA_DEFAULT_EMBED_DIM;

    /* Masking settings */
    cfg.mask_strategy = FIN_MASK_RANDOM;
    cfg.mask_ratio = 0.4f;

    /* Predictor settings */
    cfg.predictor_hidden_dim = NIMCP_MEDIUM_HIDDEN_SIZE;
    cfg.predictor_dropout = 0.1f;

    /* EMA settings */
    cfg.target_ema_decay = NIMCP_EMA_DECAY_DEFAULT;

    /* Confidence settings */
    cfg.min_confidence = NIMCP_CONFIDENCE_MIN;
    cfg.confidence_decay = NIMCP_ELIGIBILITY_DECAY_DEFAULT;

    /* Modulation sensitivity */
    cfg.inflammation_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    cfg.fatigue_sensitivity = NIMCP_SENSITIVITY_DEFAULT;

    /* Security */
    cfg.enable_bbb_validation = false;
    cfg.enable_immune_validation = false;

    fin_jepa_heartbeat("default_config", 1.0f);
    return cfg;
}

financial_jepa_bridge_t* financial_jepa_bridge_create(
    const fin_jepa_config_t* config)
{
    fin_jepa_heartbeat("create", 0.0f);

    financial_jepa_bridge_t* bridge =
        (financial_jepa_bridge_t*)nimcp_malloc(sizeof(financial_jepa_bridge_t));
    if (!bridge) {
        set_error("Failed to allocate financial_jepa_bridge_t");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NO_MEMORY,
            "Failed to allocate financial_jepa_bridge_t");
        return NULL;
    }
    memset(bridge, 0, sizeof(*bridge));

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = financial_jepa_bridge_default_config();
    }

    /* Validate and constrain config */
    if (bridge->config.num_factors == 0) {
        bridge->config.num_factors = 32;
    }
    if (bridge->config.num_factors > FIN_JEPA_MAX_FACTORS) {
        bridge->config.num_factors = FIN_JEPA_MAX_FACTORS;
    }
    if (bridge->config.embed_dim == 0) {
        bridge->config.embed_dim = FIN_JEPA_DEFAULT_EMBED_DIM;
    }
    if (bridge->config.embed_dim > FIN_JEPA_MAX_EMBED_DIM) {
        bridge->config.embed_dim = FIN_JEPA_MAX_EMBED_DIM;
    }
    if (bridge->config.predictor_hidden_dim == 0) {
        bridge->config.predictor_hidden_dim = NIMCP_MEDIUM_HIDDEN_SIZE;
    }
    if (bridge->config.mask_ratio < 0.0f || bridge->config.mask_ratio > FIN_JEPA_MAX_MASK_RATIO) {
        bridge->config.mask_ratio = nimcp_clampf(bridge->config.mask_ratio, 0.1f, FIN_JEPA_MAX_MASK_RATIO);
    }

    uint32_t num_factors = bridge->config.num_factors;
    uint32_t embed_dim = bridge->config.embed_dim;
    uint32_t hidden_dim = bridge->config.predictor_hidden_dim;

    /* Initialize RNG */
    bridge->rng_state = 42;

    /* Allocate encoder weights */
    bridge->encoder_weights = (float*)nimcp_calloc(num_factors * embed_dim, sizeof(float));
    bridge->encoder_bias = (float*)nimcp_calloc(embed_dim, sizeof(float));
    if (!bridge->encoder_weights || !bridge->encoder_bias) {
        set_error("Failed to allocate encoder weights");
        financial_jepa_bridge_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "financial_jepa_bridge_create: required parameter is NULL (bridge->encoder_weights, bridge->encoder_bias)");
        return NULL;
    }
    init_xavier_weights(bridge->encoder_weights, num_factors, embed_dim, &bridge->rng_state);
    init_zero_bias(bridge->encoder_bias, embed_dim);

    /* Allocate target encoder weights (copy of encoder initially) */
    bridge->target_encoder_weights = (float*)nimcp_calloc((size_t)num_factors * embed_dim, sizeof(float));
    bridge->target_encoder_bias = (float*)nimcp_calloc(embed_dim, sizeof(float));
    if (!bridge->target_encoder_weights || !bridge->target_encoder_bias) {
        set_error("Failed to allocate target encoder weights");
        financial_jepa_bridge_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "financial_jepa_bridge_create: required parameter is NULL (bridge->target_encoder_weights, bridge->target_encoder_bias)");
        return NULL;
    }
    memcpy(bridge->target_encoder_weights, bridge->encoder_weights,
           num_factors * embed_dim * sizeof(float));
    memcpy(bridge->target_encoder_bias, bridge->encoder_bias,
           embed_dim * sizeof(float));

    /* Allocate predictor network weights */
    bridge->predictor_w1 = (float*)nimcp_calloc(embed_dim * hidden_dim, sizeof(float));
    bridge->predictor_b1 = (float*)nimcp_calloc(hidden_dim, sizeof(float));
    if (!bridge->predictor_b1) return NULL;
    bridge->predictor_w2 = (float*)nimcp_calloc(hidden_dim * embed_dim, sizeof(float));
    if (!bridge->predictor_w2) return NULL;
    bridge->predictor_b2 = (float*)nimcp_calloc(embed_dim, sizeof(float));
    if (!bridge->predictor_b2) return NULL;
    if (!bridge->predictor_w1 || !bridge->predictor_b1 ||
        !bridge->predictor_w2 || !bridge->predictor_b2) {
        set_error("Failed to allocate predictor weights");
        financial_jepa_bridge_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "financial_jepa_bridge_create: operation failed");
        return NULL;
    }
    init_xavier_weights(bridge->predictor_w1, embed_dim, hidden_dim, &bridge->rng_state);
    init_zero_bias(bridge->predictor_b1, hidden_dim);
    init_xavier_weights(bridge->predictor_w2, hidden_dim, embed_dim, &bridge->rng_state);
    init_zero_bias(bridge->predictor_b2, embed_dim);

    /* Allocate decoder weights */
    bridge->decoder_weights = (float*)nimcp_calloc(embed_dim * num_factors, sizeof(float));
    bridge->decoder_bias = (float*)nimcp_calloc(num_factors, sizeof(float));
    if (!bridge->decoder_weights || !bridge->decoder_bias) {
        set_error("Failed to allocate decoder weights");
        financial_jepa_bridge_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "financial_jepa_bridge_create: required parameter is NULL (bridge->decoder_weights, bridge->decoder_bias)");
        return NULL;
    }
    init_xavier_weights(bridge->decoder_weights, embed_dim, num_factors, &bridge->rng_state);
    init_zero_bias(bridge->decoder_bias, num_factors);

    /* Allocate cross-modal projection matrices */
    for (int m = 0; m < FIN_MODALITY_COUNT; m++) {
        /* Each modality gets a projection to shared embed space */
        bridge->modality_dims[m] = embed_dim / 2; /* Default modality embedding dim */
        bridge->cross_modal_proj[m] = (float*)nimcp_calloc(
            bridge->modality_dims[m] * embed_dim, sizeof(float));
        if (!bridge->cross_modal_proj[m]) {
            set_error("Failed to allocate cross-modal projection %d", m);
            financial_jepa_bridge_destroy(bridge);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "financial_jepa_bridge_create: bridge->cross_modal_proj is NULL");
            return NULL;
        }
        init_xavier_weights(bridge->cross_modal_proj[m],
                            bridge->modality_dims[m], embed_dim, &bridge->rng_state);
    }

    /* Allocate factor-to-modality mapping */
    bridge->factor_modalities = (fin_factor_modality_t*)nimcp_calloc(
        num_factors, sizeof(fin_factor_modality_t));
    if (!bridge->factor_modalities) {
        set_error("Failed to allocate factor modalities");
        financial_jepa_bridge_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "financial_jepa_bridge_create: bridge->factor_modalities is NULL");
        return NULL;
    }
    /* Default assignment: distribute factors across modalities */
    for (uint32_t f = 0; f < num_factors; f++) {
        bridge->factor_modalities[f] = (fin_factor_modality_t)(f % FIN_MODALITY_COUNT);
    }

    /* Allocate working buffers */
    bridge->visible_embedding = (float*)nimcp_calloc(embed_dim, sizeof(float));
    bridge->predicted_embedding = (float*)nimcp_calloc(embed_dim, sizeof(float));
    if (!bridge->predicted_embedding) return NULL;
    bridge->hidden_buffer = (float*)nimcp_calloc(hidden_dim, sizeof(float));
    if (!bridge->hidden_buffer) return NULL;
    if (!bridge->visible_embedding || !bridge->predicted_embedding ||
        !bridge->hidden_buffer) {
        set_error("Failed to allocate working buffers");
        financial_jepa_bridge_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "financial_jepa_bridge_create: operation failed");
        return NULL;
    }

    /* Initialize state */
    bridge->operational_state = FIN_JEPA_STATE_IDLE;
    bridge->inflammation = 0.0f;
    bridge->fatigue = 0.0f;

    /* Copy security flags */
    bridge->enable_bbb_validation = bridge->config.enable_bbb_validation;
    bridge->enable_immune_validation = bridge->config.enable_immune_validation;

    fin_jepa_heartbeat("create", 1.0f);
    return bridge;
}

void financial_jepa_bridge_destroy(financial_jepa_bridge_t* bridge) {
    if (!bridge) return;
    fin_jepa_heartbeat("destroy", 0.0f);

    if (bridge->encoder_weights) nimcp_free(bridge->encoder_weights);
    if (bridge->encoder_bias) nimcp_free(bridge->encoder_bias);
    if (bridge->target_encoder_weights) nimcp_free(bridge->target_encoder_weights);
    if (bridge->target_encoder_bias) nimcp_free(bridge->target_encoder_bias);

    if (bridge->predictor_w1) nimcp_free(bridge->predictor_w1);
    if (bridge->predictor_b1) nimcp_free(bridge->predictor_b1);
    if (bridge->predictor_w2) nimcp_free(bridge->predictor_w2);
    if (bridge->predictor_b2) nimcp_free(bridge->predictor_b2);

    if (bridge->decoder_weights) nimcp_free(bridge->decoder_weights);
    if (bridge->decoder_bias) nimcp_free(bridge->decoder_bias);

    for (int m = 0; m < FIN_MODALITY_COUNT; m++) {
        if (bridge->cross_modal_proj[m]) nimcp_free(bridge->cross_modal_proj[m]);
    }

    if (bridge->factor_modalities) nimcp_free(bridge->factor_modalities);

    if (bridge->visible_embedding) nimcp_free(bridge->visible_embedding);
    if (bridge->predicted_embedding) nimcp_free(bridge->predicted_embedding);
    if (bridge->hidden_buffer) nimcp_free(bridge->hidden_buffer);

    nimcp_free(bridge);
    bridge = NULL;
    fin_jepa_heartbeat("destroy", 1.0f);
}

fin_jepa_op_state_t financial_jepa_bridge_get_state(
    const financial_jepa_bridge_t* bridge)
{
    if (!bridge) return FIN_JEPA_STATE_UNINITIALIZED;
    return bridge->operational_state;
}

int financial_jepa_bridge_reset(financial_jepa_bridge_t* bridge) {
    if (!bridge) {
        set_error("NULL bridge in reset");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_jepa_bridge_reset: bridge is NULL");
        return FIN_JEPA_ERR_NULL;
    }

    fin_jepa_heartbeat_instance(bridge->health_agent, "reset", 0.0f);

    /* Reset statistics */
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    /* Reset modulation */
    bridge->inflammation = 0.0f;
    bridge->fatigue = 0.0f;

    /* Reset RNG */
    bridge->rng_state = 42;

    /* Re-initialize weights */
    uint32_t num_factors = bridge->config.num_factors;
    uint32_t embed_dim = bridge->config.embed_dim;
    uint32_t hidden_dim = bridge->config.predictor_hidden_dim;

    init_xavier_weights(bridge->encoder_weights, num_factors, embed_dim, &bridge->rng_state);
    init_zero_bias(bridge->encoder_bias, embed_dim);

    memcpy(bridge->target_encoder_weights, bridge->encoder_weights,
           num_factors * embed_dim * sizeof(float));
    memcpy(bridge->target_encoder_bias, bridge->encoder_bias,
           embed_dim * sizeof(float));

    init_xavier_weights(bridge->predictor_w1, embed_dim, hidden_dim, &bridge->rng_state);
    init_zero_bias(bridge->predictor_b1, hidden_dim);
    init_xavier_weights(bridge->predictor_w2, hidden_dim, embed_dim, &bridge->rng_state);
    init_zero_bias(bridge->predictor_b2, embed_dim);

    init_xavier_weights(bridge->decoder_weights, embed_dim, num_factors, &bridge->rng_state);
    init_zero_bias(bridge->decoder_bias, num_factors);

    bridge->operational_state = FIN_JEPA_STATE_IDLE;

    fin_jepa_heartbeat_instance(bridge->health_agent, "reset", 1.0f);
    return FIN_JEPA_ERR_OK;
}

//=============================================================================
// Subsystem Setters
//=============================================================================

int financial_jepa_bridge_set_immune(financial_jepa_bridge_t* bridge,
                                      void* immune) {
    if (!bridge) {
        set_error("NULL bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_jepa_bridge_set_immune: bridge is NULL");
        return FIN_JEPA_ERR_NULL;
    }
    bridge->immune = (brain_immune_system_t*)immune;
    return FIN_JEPA_ERR_OK;
}

int financial_jepa_bridge_set_bbb(financial_jepa_bridge_t* bridge,
                                   void* bbb) {
    if (!bridge) {
        set_error("NULL bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_jepa_bridge_set_bbb: bridge is NULL");
        return FIN_JEPA_ERR_NULL;
    }
    bridge->bbb = (bbb_system_t)bbb;
    return FIN_JEPA_ERR_OK;
}

int financial_jepa_bridge_enable_bbb_validation(
    financial_jepa_bridge_t* bridge, bool enable) {
    if (!bridge) {
        set_error("NULL bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_jepa_bridge_enable_bbb_validation: bridge is NULL");
        return FIN_JEPA_ERR_NULL;
    }
    bridge->enable_bbb_validation = enable;
    return FIN_JEPA_ERR_OK;
}

int financial_jepa_bridge_enable_immune_validation(
    financial_jepa_bridge_t* bridge, bool enable) {
    if (!bridge) {
        set_error("NULL bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_jepa_bridge_enable_immune_validation: bridge is NULL");
        return FIN_JEPA_ERR_NULL;
    }
    bridge->enable_immune_validation = enable;
    return FIN_JEPA_ERR_OK;
}

int financial_jepa_bridge_set_kg_wiring(financial_jepa_bridge_t* bridge,
                                         void* kg) {
    if (!bridge) {
        set_error("NULL bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_jepa_bridge_set_kg_wiring: bridge is NULL");
        return FIN_JEPA_ERR_NULL;
    }
    bridge->kg_wiring = (kg_wiring_t*)kg;
    return FIN_JEPA_ERR_OK;
}

int financial_jepa_bridge_set_health_agent(financial_jepa_bridge_t* bridge,
                                            void* health_agent) {
    if (!bridge) {
        set_error("NULL bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_jepa_bridge_set_health_agent: bridge is NULL");
        return FIN_JEPA_ERR_NULL;
    }
    bridge->health_agent = (nimcp_health_agent_t*)health_agent;
    return FIN_JEPA_ERR_OK;
}

int financial_jepa_bridge_set_logger(financial_jepa_bridge_t* bridge,
                                      void* logger) {
    if (!bridge) {
        set_error("NULL bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_jepa_bridge_set_logger: bridge is NULL");
        return FIN_JEPA_ERR_NULL;
    }
    bridge->logger = logger;
    return FIN_JEPA_ERR_OK;
}

//=============================================================================
// Encoding Helpers
//=============================================================================

/**
 * @brief Encode visible factors into embedding space
 */
static void encode_visible_factors(financial_jepa_bridge_t* bridge,
                                    const float* visible_factors,
                                    const bool* mask,
                                    uint32_t num_factors,
                                    float* embedding) {
    uint32_t embed_dim = bridge->config.embed_dim;
    uint32_t max_factors = bridge->config.num_factors;

    /* Initialize embedding to bias */
    memcpy(embedding, bridge->encoder_bias, embed_dim * sizeof(float));

    /* Sum weighted contributions from visible (unmasked) factors */
    for (uint32_t f = 0; f < num_factors && f < max_factors; f++) {
        if (!mask[f]) {
            /* Factor is visible - add its contribution */
            float factor_val = visible_factors[f];
            for (uint32_t e = 0; e < embed_dim; e++) {
                embedding[e] += factor_val * bridge->encoder_weights[f * embed_dim + e];
            }
        }
    }

    /* Normalize embedding */
    normalize_vector(embedding, embed_dim);
}

/**
 * @brief Run predictor network to predict masked embeddings
 */
static void predict_masked_embedding(financial_jepa_bridge_t* bridge,
                                      const float* visible_embedding,
                                      float* predicted_embedding) {
    uint32_t embed_dim = bridge->config.embed_dim;
    uint32_t hidden_dim = bridge->config.predictor_hidden_dim;

    /* Layer 1: hidden = ReLU(W1 * visible + b1) */
    for (uint32_t h = 0; h < hidden_dim; h++) {
        float sum = bridge->predictor_b1[h];
        for (uint32_t e = 0; e < embed_dim; e++) {
            sum += visible_embedding[e] * bridge->predictor_w1[e * hidden_dim + h];
        }
        bridge->hidden_buffer[h] = relu(sum);
    }

    /* Layer 2: predicted = W2 * hidden + b2 */
    for (uint32_t e = 0; e < embed_dim; e++) {
        float sum = bridge->predictor_b2[e];
        for (uint32_t h = 0; h < hidden_dim; h++) {
            sum += bridge->hidden_buffer[h] * bridge->predictor_w2[h * embed_dim + e];
        }
        predicted_embedding[e] = sum;
    }

    /* Normalize predicted embedding */
    normalize_vector(predicted_embedding, embed_dim);
}

/**
 * @brief Decode embedding to factor values
 */
static void decode_embedding(financial_jepa_bridge_t* bridge,
                              const float* embedding,
                              float* factor_values,
                              uint32_t num_factors) {
    uint32_t embed_dim = bridge->config.embed_dim;
    uint32_t max_factors = bridge->config.num_factors;

    for (uint32_t f = 0; f < num_factors && f < max_factors; f++) {
        float sum = bridge->decoder_bias[f];
        for (uint32_t e = 0; e < embed_dim; e++) {
            sum += embedding[e] * bridge->decoder_weights[e * max_factors + f];
        }
        factor_values[f] = sum;
    }
}

//=============================================================================
// Core JEPA API: Predict Missing
//=============================================================================

int financial_jepa_bridge_predict_missing(
    financial_jepa_bridge_t* bridge,
    const fin_jepa_input_t* input,
    fin_jepa_output_t* output)
{
    if (!bridge) {
        set_error("NULL bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_jepa_bridge_predict_missing: bridge is NULL");
        return FIN_JEPA_ERR_NULL;
    }
    if (!input) {
        set_error("NULL input");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_jepa_bridge_predict_missing: input is NULL");
        return FIN_JEPA_ERR_NULL;
    }
    if (!output) {
        set_error("NULL output");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_jepa_bridge_predict_missing: output is NULL");
        return FIN_JEPA_ERR_NULL;
    }
    if (!input->visible_factors || !input->mask) {
        set_error("NULL input arrays");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_jepa_bridge_predict_missing: input arrays are NULL");
        return FIN_JEPA_ERR_NULL;
    }
    if (!output->predicted_factors || !output->confidence) {
        set_error("NULL output arrays");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_jepa_bridge_predict_missing: output arrays are NULL");
        return FIN_JEPA_ERR_NULL;
    }

    /* Validate subsystems */
    int val_rc = fin_jepa_validate_instance(bridge, "predict_missing");
    if (val_rc != FIN_JEPA_ERR_OK) return val_rc;

    fin_jepa_heartbeat_instance(bridge->health_agent, "predict_missing", 0.0f);
    bridge->stats.health_heartbeats++;

    bridge->operational_state = FIN_JEPA_STATE_PREDICTING;

    uint32_t num_factors = input->num_factors;
    if (num_factors > bridge->config.num_factors) {
        num_factors = bridge->config.num_factors;
    }

    /* Health modulation affects confidence */
    float health_mod = 1.0f
        - bridge->inflammation * bridge->config.inflammation_sensitivity * 0.3f
        - bridge->fatigue * bridge->config.fatigue_sensitivity * 0.2f;
    if (health_mod < 0.1f) health_mod = 0.1f;

    /* Count masked factors */
    uint32_t num_masked = 0;
    uint32_t num_visible = 0;
    for (uint32_t f = 0; f < num_factors; f++) {
        if (input->mask[f]) {
            num_masked++;
        } else {
            num_visible++;
        }
    }

    if (num_masked == 0) {
        /* Nothing to predict */
        output->num_predicted = 0;
        bridge->operational_state = FIN_JEPA_STATE_IDLE;
        fin_jepa_heartbeat_instance(bridge->health_agent, "predict_missing", 1.0f);
        return FIN_JEPA_ERR_OK;
    }

    if (num_visible == 0) {
        /* Cannot predict without any visible context */
        set_error("No visible factors to encode");
        fin_jepa_present_antigen(bridge, "no_visible_factors", 3);
        bridge->operational_state = FIN_JEPA_STATE_ERROR;
        return FIN_JEPA_ERR_MASK;
    }

    /* Step 1: Encode visible factors */
    bridge->operational_state = FIN_JEPA_STATE_ENCODING;
    encode_visible_factors(bridge, input->visible_factors, input->mask,
                           num_factors, bridge->visible_embedding);

    /* Step 2: Predict embedding for masked factors */
    predict_masked_embedding(bridge, bridge->visible_embedding,
                             bridge->predicted_embedding);

    /* Step 3: Decode predicted embedding to factor values */
    bridge->operational_state = FIN_JEPA_STATE_DECODING;
    float* all_decoded = (float*)nimcp_calloc(num_factors, sizeof(float));
    if (!all_decoded) {
        set_error("Failed to allocate decode buffer");
        bridge->operational_state = FIN_JEPA_STATE_ERROR;
        return FIN_JEPA_ERR_NO_MEMORY;
    }
    decode_embedding(bridge, bridge->predicted_embedding, all_decoded, num_factors);

    /* Extract only the masked (predicted) factors */
    uint32_t pred_idx = 0;
    for (uint32_t f = 0; f < num_factors && pred_idx < num_masked; f++) {
        if (input->mask[f]) {
            output->predicted_factors[pred_idx] = all_decoded[f];

            /* Compute confidence based on:
             * - Number of visible factors (more context = higher confidence)
             * - Health modulation
             * - Embedding similarity (how well-defined is the prediction)
             */
            float context_confidence = (float)num_visible / (float)num_factors;
            float base_confidence = 0.5f + 0.5f * context_confidence;
            output->confidence[pred_idx] = base_confidence * health_mod;
            output->confidence[pred_idx] = nimcp_clampf(output->confidence[pred_idx],
                                                   bridge->config.min_confidence, 1.0f);

            pred_idx++;
        }
    }

    nimcp_free(all_decoded);
    all_decoded = NULL;

    output->num_predicted = pred_idx;
    bridge->stats.predictions++;
    bridge->operational_state = FIN_JEPA_STATE_IDLE;

    /* Publish to KG */
    fin_jepa_kg_publish(bridge, KG_MSG_FIN_JEPA_PREDICTION, output, sizeof(*output));

    fin_jepa_heartbeat_instance(bridge->health_agent, "predict_missing", 1.0f);
    bridge->stats.health_heartbeats++;

    return FIN_JEPA_ERR_OK;
}

//=============================================================================
// Core JEPA API: Cross-Modal Prediction
//=============================================================================

int financial_jepa_bridge_cross_modal_predict(
    financial_jepa_bridge_t* bridge,
    const fin_cross_modal_input_t* input,
    fin_cross_modal_output_t* output)
{
    if (!bridge) {
        set_error("NULL bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_jepa_bridge_cross_modal_predict: bridge is NULL");
        return FIN_JEPA_ERR_NULL;
    }
    if (!input) {
        set_error("NULL input");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_jepa_bridge_cross_modal_predict: input is NULL");
        return FIN_JEPA_ERR_NULL;
    }
    if (!output) {
        set_error("NULL output");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_jepa_bridge_cross_modal_predict: output is NULL");
        return FIN_JEPA_ERR_NULL;
    }
    if (!input->source_factors) {
        set_error("NULL source factors");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_jepa_bridge_cross_modal_predict: source_factors is NULL");
        return FIN_JEPA_ERR_NULL;
    }
    if (!output->predicted_factors || !output->confidence) {
        set_error("NULL output arrays");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_jepa_bridge_cross_modal_predict: output arrays are NULL");
        return FIN_JEPA_ERR_NULL;
    }

    /* Validate modalities */
    if (input->source_modality >= FIN_MODALITY_COUNT ||
        input->target_modality >= FIN_MODALITY_COUNT) {
        set_error("Invalid modality");
        return FIN_JEPA_ERR_INVALID_PARAM;
    }

    /* Validate subsystems */
    int val_rc = fin_jepa_validate_instance(bridge, "cross_modal_predict");
    if (val_rc != FIN_JEPA_ERR_OK) return val_rc;

    fin_jepa_heartbeat_instance(bridge->health_agent, "cross_modal_predict", 0.0f);
    bridge->stats.health_heartbeats++;

    bridge->operational_state = FIN_JEPA_STATE_CROSS_MODAL;

    uint32_t embed_dim = bridge->config.embed_dim;
    uint32_t num_factors = bridge->config.num_factors;

    /* Health modulation */
    float health_mod = 1.0f
        - bridge->inflammation * bridge->config.inflammation_sensitivity * 0.25f
        - bridge->fatigue * bridge->config.fatigue_sensitivity * 0.15f;
    if (health_mod < 0.1f) health_mod = 0.1f;

    /* Step 1: Project source factors into shared embedding space */
    float* source_embedding = (float*)nimcp_calloc(embed_dim, sizeof(float));
    if (!source_embedding) {
        set_error("Failed to allocate source embedding");
        bridge->operational_state = FIN_JEPA_STATE_ERROR;
        return FIN_JEPA_ERR_NO_MEMORY;
    }

    /* Apply source modality projection */
    uint32_t src_dim = bridge->modality_dims[input->source_modality];
    uint32_t num_source = input->num_source;
    if (num_source > src_dim) num_source = src_dim;

    /* Simple projection: embed = proj * source */
    for (uint32_t e = 0; e < embed_dim; e++) {
        float sum = 0.0f;
        for (uint32_t s = 0; s < num_source; s++) {
            sum += input->source_factors[s] *
                   bridge->cross_modal_proj[input->source_modality][s * embed_dim + e];
        }
        source_embedding[e] = sum;
    }
    normalize_vector(source_embedding, embed_dim);

    /* Step 2: Use predictor to map to target embedding */
    predict_masked_embedding(bridge, source_embedding, bridge->predicted_embedding);

    /* Step 3: Decode to target factor values */
    /* Count factors belonging to target modality */
    uint32_t target_count = 0;
    for (uint32_t f = 0; f < num_factors; f++) {
        if (bridge->factor_modalities[f] == input->target_modality) {
            target_count++;
        }
    }

    /* Decode all factors, then extract target modality */
    float* all_decoded = (float*)nimcp_calloc(num_factors, sizeof(float));
    if (!all_decoded) {
        nimcp_free(source_embedding);
        source_embedding = NULL;
        set_error("Failed to allocate decode buffer");
        bridge->operational_state = FIN_JEPA_STATE_ERROR;
        return FIN_JEPA_ERR_NO_MEMORY;
    }
    decode_embedding(bridge, bridge->predicted_embedding, all_decoded, num_factors);

    /* Extract target modality factors */
    uint32_t pred_idx = 0;
    for (uint32_t f = 0; f < num_factors; f++) {
        if (bridge->factor_modalities[f] == input->target_modality) {
            output->predicted_factors[pred_idx] = all_decoded[f];

            /* Cross-modal confidence is generally lower */
            float base_confidence = 0.4f * health_mod;
            output->confidence[pred_idx] = nimcp_clampf(base_confidence,
                                                   bridge->config.min_confidence, 0.8f);
            pred_idx++;
        }
    }

    output->num_predicted = pred_idx;

    /* Compute cross-modal coherence (how well aligned are the embeddings) */
    float coherence = 0.0f;
    for (uint32_t e = 0; e < embed_dim; e++) {
        coherence += source_embedding[e] * bridge->predicted_embedding[e];
    }
    coherence = (coherence + 1.0f) * 0.5f; /* Map [-1, 1] to [0, 1] */
    output->cross_modal_coherence = nimcp_clampf(coherence * health_mod, 0.0f, 1.0f);

    nimcp_free(source_embedding);
    source_embedding = NULL;
    nimcp_free(all_decoded);
    all_decoded = NULL;

    bridge->stats.cross_modal_predictions++;
    bridge->operational_state = FIN_JEPA_STATE_IDLE;

    /* Publish to KG */
    fin_jepa_kg_publish(bridge, KG_MSG_FIN_JEPA_CROSS_MODAL, output, sizeof(*output));

    fin_jepa_heartbeat_instance(bridge->health_agent, "cross_modal_predict", 1.0f);
    bridge->stats.health_heartbeats++;

    return FIN_JEPA_ERR_OK;
}

//=============================================================================
// Embedding API
//=============================================================================

int financial_jepa_bridge_get_embeddings(
    const financial_jepa_bridge_t* bridge,
    fin_jepa_embedding_t* embeddings)
{
    if (!bridge) {
        set_error("NULL bridge");
        return FIN_JEPA_ERR_NULL;
    }
    if (!embeddings) {
        set_error("NULL embeddings");
        return FIN_JEPA_ERR_NULL;
    }

    embeddings->num_factors = bridge->config.num_factors;
    embeddings->embed_dim = bridge->config.embed_dim;

    /* If caller provided buffer, fill it with encoder weights */
    if (embeddings->embeddings) {
        memcpy(embeddings->embeddings, bridge->encoder_weights,
               bridge->config.num_factors * bridge->config.embed_dim * sizeof(float));
    }

    return FIN_JEPA_ERR_OK;
}

int financial_jepa_bridge_embedding_similarity(
    const financial_jepa_bridge_t* bridge,
    uint32_t factor_a,
    uint32_t factor_b,
    float* similarity)
{
    if (!bridge) {
        set_error("NULL bridge");
        return FIN_JEPA_ERR_NULL;
    }
    if (!similarity) {
        set_error("NULL similarity");
        return FIN_JEPA_ERR_NULL;
    }

    uint32_t num_factors = bridge->config.num_factors;
    uint32_t embed_dim = bridge->config.embed_dim;

    if (factor_a >= num_factors || factor_b >= num_factors) {
        set_error("Factor index out of range");
        return FIN_JEPA_ERR_INVALID_PARAM;
    }

    /* Compute cosine similarity between factor embeddings */
    float* emb_a = &bridge->encoder_weights[factor_a * embed_dim];
    float* emb_b = &bridge->encoder_weights[factor_b * embed_dim];

    float dot = dot_product(emb_a, emb_b, embed_dim);
    float norm_a = vector_norm(emb_a, embed_dim);
    float norm_b = vector_norm(emb_b, embed_dim);

    if (norm_a > 1e-10f && norm_b > 1e-10f) {
        *similarity = dot / (norm_a * norm_b);
    } else {
        *similarity = 0.0f;
    }

    return FIN_JEPA_ERR_OK;
}

//=============================================================================
// Mask Generation API
//=============================================================================

int financial_jepa_bridge_generate_mask(
    financial_jepa_bridge_t* bridge,
    uint32_t num_factors,
    bool* mask,
    uint32_t* num_masked)
{
    if (!bridge) {
        set_error("NULL bridge");
        return FIN_JEPA_ERR_NULL;
    }
    if (!mask || !num_masked) {
        set_error("NULL mask or num_masked");
        return FIN_JEPA_ERR_NULL;
    }

    *num_masked = 0;
    float mask_ratio = bridge->config.mask_ratio;
    uint32_t target_masked = (uint32_t)(mask_ratio * (float)num_factors);
    if (target_masked == 0) target_masked = 1;

    memset(mask, 0, num_factors * sizeof(bool));

    switch (bridge->config.mask_strategy) {
        case FIN_MASK_RANDOM: {
            /* Random masking */
            for (uint32_t i = 0; i < target_masked && *num_masked < num_factors; ) {
                uint32_t idx = (uint32_t)(jepa_randf(&bridge->rng_state) * (float)num_factors);
                if (idx >= num_factors) idx = num_factors - 1;
                if (!mask[idx]) {
                    mask[idx] = true;
                    (*num_masked)++;
                    i++;
                }
            }
            break;
        }

        case FIN_MASK_BLOCK: {
            /* Block (contiguous) masking */
            if (target_masked >= num_factors) {
                target_masked = num_factors - 1;
            }
            uint32_t start = (uint32_t)(jepa_randf(&bridge->rng_state) *
                                        (float)(num_factors - target_masked));
            for (uint32_t i = start; i < start + target_masked && i < num_factors; i++) {
                mask[i] = true;
                (*num_masked)++;
            }
            break;
        }

        case FIN_MASK_IMPORTANCE_WEIGHTED: {
            /* Mask less important factors more often (simplified) */
            for (uint32_t f = 0; f < num_factors && *num_masked < target_masked; f++) {
                /* Higher indexed factors assumed less important in this simple impl */
                float importance = 1.0f - (float)f / (float)num_factors;
                float mask_prob = mask_ratio * (1.0f - importance * 0.5f);
                if (jepa_randf(&bridge->rng_state) < mask_prob) {
                    mask[f] = true;
                    (*num_masked)++;
                }
            }
            break;
        }

        default:
            /* Fall back to random */
            for (uint32_t i = 0; i < target_masked && *num_masked < num_factors; ) {
                uint32_t idx = (uint32_t)(jepa_randf(&bridge->rng_state) * (float)num_factors);
                if (idx >= num_factors) idx = num_factors - 1;
                if (!mask[idx]) {
                    mask[idx] = true;
                    (*num_masked)++;
                    i++;
                }
            }
            break;
    }

    return FIN_JEPA_ERR_OK;
}

int financial_jepa_bridge_generate_modality_mask(
    financial_jepa_bridge_t* bridge,
    fin_factor_modality_t modality,
    uint32_t num_factors,
    const fin_factor_modality_t* factor_modalities,
    bool* mask,
    uint32_t* num_masked)
{
    if (!bridge) {
        set_error("NULL bridge");
        return FIN_JEPA_ERR_NULL;
    }
    if (!factor_modalities || !mask || !num_masked) {
        set_error("NULL input");
        return FIN_JEPA_ERR_NULL;
    }

    *num_masked = 0;
    memset(mask, 0, num_factors * sizeof(bool));

    /* Mask all factors belonging to the specified modality */
    for (uint32_t f = 0; f < num_factors; f++) {
        if (factor_modalities[f] == modality) {
            mask[f] = true;
            (*num_masked)++;
        }
    }

    return FIN_JEPA_ERR_OK;
}

//=============================================================================
// Utility Functions
//=============================================================================

fin_jepa_input_t* financial_jepa_input_create(uint32_t num_factors) {
    if (num_factors == 0 || num_factors > FIN_JEPA_MAX_FACTORS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "financial_jepa_input_create: num_factors is zero");
        return NULL;
    }

    fin_jepa_input_t* input = (fin_jepa_input_t*)nimcp_malloc(sizeof(fin_jepa_input_t));
    if (!input) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "financial_jepa_input_create: input is NULL");
        return NULL;
    }

    memset(input, 0, sizeof(*input));
    input->num_factors = num_factors;

    input->visible_factors = (float*)nimcp_calloc(num_factors, sizeof(float));
    input->mask = (bool*)nimcp_calloc(num_factors, sizeof(bool));

    if (!input->visible_factors || !input->mask) {
        financial_jepa_input_destroy(input);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "financial_jepa_input_create: required parameter is NULL (input->visible_factors, input->mask)");
        return NULL;
    }

    return input;
}

void financial_jepa_input_destroy(fin_jepa_input_t* input) {
    if (!input) return;
    if (input->visible_factors) nimcp_free(input->visible_factors);
    if (input->mask) nimcp_free(input->mask);
    nimcp_free(input);
    input = NULL;
}

fin_jepa_output_t* financial_jepa_output_create(uint32_t num_factors) {
    if (num_factors == 0 || num_factors > FIN_JEPA_MAX_FACTORS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "financial_jepa_output_create: num_factors is zero");
        return NULL;
    }

    fin_jepa_output_t* output = (fin_jepa_output_t*)nimcp_malloc(sizeof(fin_jepa_output_t));
    if (!output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "financial_jepa_output_create: output is NULL");
        return NULL;
    }

    memset(output, 0, sizeof(*output));

    output->predicted_factors = (float*)nimcp_calloc(num_factors, sizeof(float));
    output->confidence = (float*)nimcp_calloc(num_factors, sizeof(float));

    if (!output->predicted_factors || !output->confidence) {
        financial_jepa_output_destroy(output);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "financial_jepa_output_create: required parameter is NULL (output->predicted_factors, output->confidence)");
        return NULL;
    }

    return output;
}

void financial_jepa_output_destroy(fin_jepa_output_t* output) {
    if (!output) return;
    if (output->predicted_factors) nimcp_free(output->predicted_factors);
    if (output->confidence) nimcp_free(output->confidence);
    nimcp_free(output);
    output = NULL;
}

fin_cross_modal_output_t* financial_jepa_cross_modal_output_create(uint32_t num_factors) {
    if (num_factors == 0 || num_factors > FIN_JEPA_MAX_FACTORS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "financial_jepa_cross_modal_output_create: num_factors is zero");
        return NULL;
    }

    fin_cross_modal_output_t* output =
        (fin_cross_modal_output_t*)nimcp_malloc(sizeof(fin_cross_modal_output_t));
    if (!output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "financial_jepa_cross_modal_output_create: output is NULL");
        return NULL;
    }

    memset(output, 0, sizeof(*output));

    output->predicted_factors = (float*)nimcp_calloc(num_factors, sizeof(float));
    output->confidence = (float*)nimcp_calloc(num_factors, sizeof(float));

    if (!output->predicted_factors || !output->confidence) {
        financial_jepa_cross_modal_output_destroy(output);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "financial_jepa_cross_modal_output_create: required parameter is NULL (output->predicted_factors, output->confidence)");
        return NULL;
    }

    return output;
}

void financial_jepa_cross_modal_output_destroy(fin_cross_modal_output_t* output) {
    if (!output) return;
    if (output->predicted_factors) nimcp_free(output->predicted_factors);
    if (output->confidence) nimcp_free(output->confidence);
    nimcp_free(output);
    output = NULL;
}

//=============================================================================
// Modulation API
//=============================================================================

int financial_jepa_bridge_set_inflammation(
    financial_jepa_bridge_t* bridge, float level)
{
    if (!bridge) {
        set_error("NULL bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_jepa_bridge_set_inflammation: bridge is NULL");
        return FIN_JEPA_ERR_NULL;
    }
    bridge->inflammation = nimcp_clampf(level, 0.0f, 1.0f);
    return FIN_JEPA_ERR_OK;
}

int financial_jepa_bridge_set_fatigue(
    financial_jepa_bridge_t* bridge, float level)
{
    if (!bridge) {
        set_error("NULL bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_jepa_bridge_set_fatigue: bridge is NULL");
        return FIN_JEPA_ERR_NULL;
    }
    bridge->fatigue = nimcp_clampf(level, 0.0f, 1.0f);
    return FIN_JEPA_ERR_OK;
}

//=============================================================================
// Statistics API
//=============================================================================

int financial_jepa_bridge_get_stats(
    const financial_jepa_bridge_t* bridge,
    fin_jepa_bridge_stats_t* stats)
{
    if (!bridge) {
        set_error("NULL bridge");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_jepa_bridge_get_stats: bridge is NULL");
        return FIN_JEPA_ERR_NULL;
    }
    if (!stats) {
        set_error("NULL stats");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_jepa_bridge_get_stats: stats is NULL");
        return FIN_JEPA_ERR_NULL;
    }
    *stats = bridge->stats;
    return FIN_JEPA_ERR_OK;
}

void financial_jepa_bridge_reset_stats(financial_jepa_bridge_t* bridge) {
    if (!bridge) return;
    memset(&bridge->stats, 0, sizeof(bridge->stats));
}

const char* financial_jepa_bridge_get_last_error(void) {
    return fin_jepa_last_error;
}
