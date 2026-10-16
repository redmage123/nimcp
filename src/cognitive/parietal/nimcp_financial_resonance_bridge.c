//=============================================================================
// nimcp_financial_resonance_bridge.c - Financial Resonance Pattern Bridge
//=============================================================================
/**
 * @file nimcp_financial_resonance_bridge.c
 * @brief Implementation of resonance-based pattern matching and similarity
 *        detection for financial market data
 *
 * @author NIMCP Development Team
 * @date 2026-01-29
 */

#include "cognitive/parietal/nimcp_financial_resonance_bridge.h"
#include "constants/nimcp_buffer_constants.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/error/nimcp_error_codes.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Constants
//=============================================================================


#define TWO_PI (2.0 * M_PI)
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_threshold_constants.h"
#include "constants/nimcp_math_constants.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(fin_resonance, MESH_ADAPTER_CATEGORY_COGNITIVE)


/** Send heartbeat from instance level */
static inline void fin_resonance_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_fin_resonance_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_fin_resonance_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_fin_resonance_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

//=============================================================================
// Immune/BBB Integration (Phase 9: Security Integration)
//=============================================================================

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

//=============================================================================
// KG Wiring Integration
//=============================================================================

struct kg_wiring;
typedef struct kg_wiring kg_wiring_t;

/* KG message type defines */
#define KG_MSG_FIN_RESONANCE_REQUEST    "FIN_RESONANCE_REQUEST"
#define KG_MSG_FIN_RESONANCE_RESPONSE   "FIN_RESONANCE_RESPONSE"
#define KG_MSG_FIN_RESONANCE_ERROR      "FIN_RESONANCE_ERROR"
#define KG_MSG_FIN_RESONANCE_UPDATE     "FIN_RESONANCE_UPDATE"

//=============================================================================
// Thread-Local Error Storage
//=============================================================================

static _Thread_local char fin_resonance_last_error[NIMCP_ERROR_BUFFER_SIZE] = {0};

static void set_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(fin_resonance_last_error, sizeof(fin_resonance_last_error), fmt, args);
    va_end(args);
}

//=============================================================================
// Logging Macros (Placeholders)
//=============================================================================

#define FIN_RESONANCE_LOG_DEBUG(bridge, fmt, ...) /* placeholder */
#define FIN_RESONANCE_LOG_INFO(bridge, fmt, ...)  /* placeholder */
#define FIN_RESONANCE_LOG_WARN(bridge, fmt, ...)  /* placeholder */
#define FIN_RESONANCE_LOG_ERROR(bridge, fmt, ...) /* placeholder */

//=============================================================================
// Internal Structure
//=============================================================================

struct financial_resonance_bridge {
    fin_resonance_config_t config;
    fin_resonance_state_t state;
    fin_resonance_bridge_stats_t stats;

    /* Modulation state */
    float inflammation;
    float fatigue;

    /* Subsystem pointers */
    brain_immune_system_t* immune;
    bbb_system_t bbb;
    nimcp_health_agent_t* health_agent;
    kg_wiring_t* kg_wiring;
    void* logger;
    void* security;
    void* bio_router;
    void* cycle;

    /* Validation flags */
    bool enable_immune_validation;
    bool enable_bbb_validation;

    /* Pattern storage */
    fin_resonance_pattern_t* patterns;
    uint32_t pattern_count;
    uint32_t pattern_capacity;

    /* RNG state for trace IDs */
    uint64_t rng_state;
};

//=============================================================================
// Helper: Wrap phase to [0, 2*pi)
//=============================================================================

static inline float wrap_phase(float phase) {
    while (phase < 0.0f) phase += (float)NIMCP_TWO_PI_F;
    while (phase >= (float)NIMCP_TWO_PI_F) phase -= (float)NIMCP_TWO_PI_F;
    return phase;
}

//=============================================================================
// Helper: Phase difference (circular)
//=============================================================================

static inline float phase_diff(float p1, float p2) {
    float d = p1 - p2;
    while (d > M_PI) d -= (float)NIMCP_TWO_PI_F;
    while (d < -M_PI) d += (float)NIMCP_TWO_PI_F;
    return fabsf(d);
}

//=============================================================================
// Helper: Generate trace ID
//=============================================================================

static uint64_t gen_trace_id(financial_resonance_bridge_t* bridge) {
    if (!bridge) return 0;
    bridge->rng_state = bridge->rng_state * 6364136223846793005ULL + 1442695040888963407ULL;
    return bridge->rng_state;
}

//=============================================================================
// Helper: Hash combine
//=============================================================================

static uint64_t hash_combine(uint64_t h1, uint64_t h2) {
    return h1 ^ (h2 + 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2));
}

//=============================================================================
// Helper: Float to hash bits
//=============================================================================

static uint64_t float_hash(float f) {
    union { float f; uint32_t u; } conv;
    conv.f = f;
    return (uint64_t)conv.u * 2654435761ULL;
}

//=============================================================================
// KG Publish Helper
//=============================================================================

static int resonance_kg_publish(financial_resonance_bridge_t* bridge,
                                 const char* msg_type,
                                 const void* payload,
                                 size_t size) {
    if (bridge && bridge->kg_wiring) {
        /* kg_wiring_publish would be called here */
        (void)msg_type;
        (void)payload;
        (void)size;
        bridge->stats.kg_messages_sent++;
        return 0;
    }
    return 0;
}

//=============================================================================
// Immune/BBB Validation Helper
//=============================================================================

static int resonance_validate_subsystems(financial_resonance_bridge_t* bridge,
                                          const char* operation) {
    if (!bridge) return FIN_RESONANCE_ERR_NULL;

    if (bridge->enable_bbb_validation && bridge->bbb) {
        int rc = bbb_validate_data(bridge->bbb, NULL, 0, operation);
        if (rc != 0) {
            set_error("BBB validation failed for %s", operation);
            bridge->stats.bbb_validations++;
            NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_BBB_VALIDATION,
                "financial_resonance: BBB validation failed for %s", operation);
            return FIN_RESONANCE_ERR_VALIDATION;
        }
        bridge->stats.bbb_validations++;
    }

    if (bridge->enable_immune_validation && bridge->immune) {
        int rc = brain_immune_validate_operation(bridge->immune, operation, 5);
        if (rc != 0) {
            set_error("Immune validation failed for %s", operation);
            bridge->stats.immune_checks++;
            NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_BBB_VALIDATION,
                "financial_resonance: immune validation failed for %s", operation);
            return FIN_RESONANCE_ERR_VALIDATION;
        }
        bridge->stats.immune_checks++;
    }

    return FIN_RESONANCE_ERR_OK;
}

//=============================================================================
// Antigen Presentation Helper
//=============================================================================

static void resonance_present_antigen(financial_resonance_bridge_t* bridge,
                                       const char* anomaly,
                                       uint32_t severity) {
    if (bridge && bridge->immune) {
        uint8_t sig[64] = {0};
        snprintf((char*)sig, sizeof(sig), "fin_resonance:%s", anomaly);
        uint32_t antigen_id = 0;
        brain_immune_present_antigen(bridge->immune, 0, sig, strlen((char*)sig),
                                      severity, 0, &antigen_id);
    }
}

//=============================================================================
// Lifecycle: Default Config
//=============================================================================

fin_resonance_config_t financial_resonance_bridge_default_config(void) {
    fin_resonance_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));

    /* Encoding */
    cfg.default_encoding = FIN_RESONANCE_ENCODING_HYBRID;
    cfg.num_oscillators = FIN_RESONANCE_MAX_OSCILLATORS;
    cfg.phase_resolution = 0.01f;
    cfg.hash_similarity_threshold = 0.5f;

    /* Similarity weights (sum to 1.0) */
    cfg.jaccard_weight = 0.25f;
    cfg.phase_weight = 0.25f;
    cfg.quaternion_weight = 0.25f;
    cfg.kuramoto_weight = 0.25f;

    /* Kuramoto */
    cfg.kuramoto_coupling_strength = 0.5f;
    cfg.critical_sync_threshold = 0.7f;

    /* Storage */
    cfg.max_patterns = FIN_RESONANCE_MAX_PATTERNS;
    cfg.pattern_consolidation_threshold = 0.3f;

    /* Modulation */
    cfg.inflammation_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    cfg.fatigue_sensitivity = NIMCP_SENSITIVITY_DEFAULT;

    /* Validation */
    cfg.enable_immune_validation = true;
    cfg.enable_bbb_validation = true;

    /* Logging */
    cfg.enable_trace_logging = false;

    return cfg;
}

//=============================================================================
// Lifecycle: Create
//=============================================================================

financial_resonance_bridge_t* financial_resonance_bridge_create(
    const fin_resonance_config_t* config)
{
    fin_resonance_heartbeat("financial_resonance_bridge_create", 0.0f);

    financial_resonance_bridge_t* bridge =
        (financial_resonance_bridge_t*)nimcp_malloc(sizeof(financial_resonance_bridge_t));
    if (!bridge) {
        set_error("Failed to allocate financial_resonance_bridge_t");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NO_MEMORY,
            "Failed to allocate financial_resonance_bridge_t");
        return NULL;
    }
    memset(bridge, 0, sizeof(*bridge));

    /* Copy config or use defaults */
    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = financial_resonance_bridge_default_config();
    }

    /* Initialize state */
    bridge->state = FIN_RESONANCE_STATE_IDLE;
    bridge->inflammation = 0.0f;
    bridge->fatigue = 0.0f;

    /* Copy validation flags from config */
    bridge->enable_immune_validation = bridge->config.enable_immune_validation;
    bridge->enable_bbb_validation = bridge->config.enable_bbb_validation;

    /* Allocate pattern storage */
    bridge->pattern_capacity = bridge->config.max_patterns;
    if (bridge->pattern_capacity > FIN_RESONANCE_MAX_PATTERNS) {
        bridge->pattern_capacity = FIN_RESONANCE_MAX_PATTERNS;
    }
    bridge->patterns = (fin_resonance_pattern_t*)nimcp_malloc(
        bridge->pattern_capacity * sizeof(fin_resonance_pattern_t));
    if (!bridge->patterns) {
        set_error("Failed to allocate pattern storage");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NO_MEMORY,
            "Failed to allocate pattern storage");
        nimcp_free(bridge);
        bridge = NULL;
        return NULL;
    }
    memset(bridge->patterns, 0, bridge->pattern_capacity * sizeof(fin_resonance_pattern_t));
    bridge->pattern_count = 0;

    /* Initialize RNG for trace IDs */
    bridge->rng_state = 0xDEADBEEF42ULL;

    fin_resonance_heartbeat("financial_resonance_bridge_create", 1.0f);
    return bridge;
}

//=============================================================================
// Lifecycle: Destroy
//=============================================================================

void financial_resonance_bridge_destroy(financial_resonance_bridge_t* bridge) {
    if (!bridge) return;

    fin_resonance_heartbeat("financial_resonance_bridge_destroy", 0.0f);

    if (bridge->patterns) {
        nimcp_free(bridge->patterns);
        bridge->patterns = NULL;
    }

    nimcp_free(bridge);
    bridge = NULL;

    fin_resonance_heartbeat("financial_resonance_bridge_destroy", 1.0f);
}

//=============================================================================
// Lifecycle: Get State
//=============================================================================

fin_resonance_state_t financial_resonance_bridge_get_state(
    const financial_resonance_bridge_t* bridge)
{
    if (!bridge) return FIN_RESONANCE_STATE_UNINITIALIZED;
    return bridge->state;
}

//=============================================================================
// Lifecycle: Reset
//=============================================================================

int financial_resonance_bridge_reset(financial_resonance_bridge_t* bridge) {
    if (!bridge) {
        set_error("bridge is NULL in reset");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_resonance_bridge_reset: bridge is NULL");
        return FIN_RESONANCE_ERR_NULL;
    }

    fin_resonance_heartbeat_instance(bridge->health_agent, "reset", 0.0f);

    bridge->state = FIN_RESONANCE_STATE_IDLE;
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    bridge->inflammation = 0.0f;
    bridge->fatigue = 0.0f;

    /* Clear patterns */
    if (bridge->patterns) {
        memset(bridge->patterns, 0,
               bridge->pattern_capacity * sizeof(fin_resonance_pattern_t));
    }
    bridge->pattern_count = 0;

    fin_resonance_heartbeat_instance(bridge->health_agent, "reset", 1.0f);
    return FIN_RESONANCE_ERR_OK;
}

//=============================================================================
// Subsystem Setters (Macro Pattern)
//=============================================================================

#define FIN_RESONANCE_SETTER(name, field, type) \
    int financial_resonance_bridge_set_##name(financial_resonance_bridge_t* bridge, void* ptr) { \
        if (!bridge) { \
            set_error("bridge is NULL in set_" #name); \
            NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER, \
                "financial_resonance_bridge_set_" #name ": bridge is NULL"); \
            return FIN_RESONANCE_ERR_NULL; \
        } \
        bridge->field = (type)ptr; \
        return FIN_RESONANCE_ERR_OK; \
    }

FIN_RESONANCE_SETTER(immune,        immune,        brain_immune_system_t*)
FIN_RESONANCE_SETTER(bbb,           bbb,           bbb_system_t)
FIN_RESONANCE_SETTER(health_agent,  health_agent,  nimcp_health_agent_t*)
FIN_RESONANCE_SETTER(kg_wiring,     kg_wiring,     kg_wiring_t*)
FIN_RESONANCE_SETTER(logger,        logger,        void*)
FIN_RESONANCE_SETTER(security,      security,      void*)
FIN_RESONANCE_SETTER(bio_router,    bio_router,    void*)
FIN_RESONANCE_SETTER(cycle,         cycle,         void*)

int financial_resonance_bridge_enable_immune_validation(
    financial_resonance_bridge_t* bridge, bool enable)
{
    if (!bridge) {
        set_error("bridge is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_resonance_bridge_enable_immune_validation: bridge is NULL");
        return FIN_RESONANCE_ERR_NULL;
    }
    bridge->enable_immune_validation = enable;
    return FIN_RESONANCE_ERR_OK;
}

int financial_resonance_bridge_enable_bbb_validation(
    financial_resonance_bridge_t* bridge, bool enable)
{
    if (!bridge) {
        set_error("bridge is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_resonance_bridge_enable_bbb_validation: bridge is NULL");
        return FIN_RESONANCE_ERR_NULL;
    }
    bridge->enable_bbb_validation = enable;
    return FIN_RESONANCE_ERR_OK;
}

//=============================================================================
// Core API: Encode Market State
//=============================================================================

int financial_resonance_bridge_encode_market(
    financial_resonance_bridge_t* bridge,
    const fin_market_state_t* state,
    fin_resonance_query_t* out_query)
{
    if (!bridge) {
        set_error("bridge is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_resonance_bridge_encode_market: bridge is NULL");
        return FIN_RESONANCE_ERR_NULL;
    }
    if (!state || !out_query) {
        set_error("NULL state or output");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_resonance_bridge_encode_market: NULL parameter");
        return FIN_RESONANCE_ERR_NULL;
    }

    /* Security validation */
    int val_rc = resonance_validate_subsystems(bridge, "encode_market");
    if (val_rc != FIN_RESONANCE_ERR_OK) return val_rc;

    fin_resonance_heartbeat_instance(bridge->health_agent, "encode_market", 0.0f);
    bridge->state = FIN_RESONANCE_STATE_ENCODING;

    memset(out_query, 0, sizeof(*out_query));

    /* Apply health modulation */
    float health_mod = 1.0f
        - bridge->inflammation * bridge->config.inflammation_sensitivity * 0.2f
        - bridge->fatigue * bridge->config.fatigue_sensitivity * 0.15f;
    if (health_mod < 0.1f) health_mod = 0.1f;

    /* Compute signature hash from market state components */
    uint64_t h = 0;
    h = hash_combine(h, float_hash(state->price));
    h = hash_combine(h, float_hash(state->volume));
    h = hash_combine(h, float_hash(state->volatility));
    h = hash_combine(h, float_hash(state->momentum));
    h = hash_combine(h, float_hash(state->rsi));
    h = hash_combine(h, float_hash(state->macd));
    h = hash_combine(h, (uint64_t)state->regime);

    /* Add symbol contribution */
    for (size_t i = 0; i < strlen(state->symbol) && i < 32; i++) {
        h = hash_combine(h, (uint64_t)state->symbol[i]);
    }

    out_query->signature_hash = h;

    /* Compute global phase from momentum + RSI */
    float norm_rsi = state->rsi / 100.0f;
    float norm_momentum = nimcp_clampf(state->momentum / 0.1f, -1.0f, 1.0f);
    float global_phase = (float)M_PI * (norm_rsi + (norm_momentum + 1.0f) / 2.0f);
    out_query->phase = wrap_phase(global_phase);

    /* Compute oscillator phases for multi-scale representation */
    uint32_t num_osc = bridge->config.num_oscillators;
    if (num_osc > FIN_RESONANCE_MAX_OSCILLATORS) {
        num_osc = FIN_RESONANCE_MAX_OSCILLATORS;
    }

    for (uint32_t i = 0; i < num_osc; i++) {
        float freq_mult = 1.0f + (float)i * 0.5f;  /* Increasing frequencies */
        float osc_input = 0.0f;

        switch (i % 4) {
            case 0: osc_input = state->price > 0.0f ? logf(state->price) : 0.0f; break;
            case 1: osc_input = state->volatility * 10.0f; break;
            case 2: osc_input = norm_rsi * 2.0f - 1.0f; break;
            case 3: osc_input = state->macd; break;
        }

        float osc_phase = wrap_phase(osc_input * freq_mult * health_mod);
        out_query->oscillator_phases[i] = osc_phase;
    }
    out_query->num_oscillators = num_osc;

    /* Check for anomalous encoding (all oscillators near-zero) */
    float total_phase_mag = 0.0f;
    for (uint32_t i = 0; i < num_osc; i++) {
        total_phase_mag += fabsf(out_query->oscillator_phases[i]);
    }
    if (total_phase_mag < 0.001f && num_osc > 0) {
        resonance_present_antigen(bridge, "encoding_anomaly", 2);
    }

    bridge->stats.encodings++;
    bridge->state = FIN_RESONANCE_STATE_IDLE;

    /* KG notification */
    resonance_kg_publish(bridge, KG_MSG_FIN_RESONANCE_UPDATE, out_query, sizeof(*out_query));

    fin_resonance_heartbeat_instance(bridge->health_agent, "encode_market", 1.0f);
    bridge->stats.health_heartbeats++;

    return FIN_RESONANCE_ERR_OK;
}

//=============================================================================
// Core API: Batch Encoding
//=============================================================================

int financial_resonance_bridge_encode_batch(
    financial_resonance_bridge_t* bridge,
    const fin_market_state_t* states,
    uint32_t count,
    fin_resonance_query_t* out_queries)
{
    if (!bridge) {
        set_error("bridge is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_resonance_bridge_encode_batch: bridge is NULL");
        return FIN_RESONANCE_ERR_NULL;
    }
    if (!states || !out_queries || count == 0) {
        set_error("NULL parameter or zero count");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_resonance_bridge_encode_batch: NULL parameter");
        return FIN_RESONANCE_ERR_NULL;
    }

    fin_resonance_heartbeat_instance(bridge->health_agent, "encode_batch", 0.0f);

    for (uint32_t i = 0; i < count; i++) {
        int rc = financial_resonance_bridge_encode_market(bridge, &states[i], &out_queries[i]);
        if (rc != FIN_RESONANCE_ERR_OK) {
            return rc;
        }

        /* Progress heartbeat for large batches */
        if (i % 10 == 0 && count > 10) {
            float progress = (float)i / (float)count;
            fin_resonance_heartbeat_instance(bridge->health_agent, "encode_batch", progress);
        }
    }

    fin_resonance_heartbeat_instance(bridge->health_agent, "encode_batch", 1.0f);
    return FIN_RESONANCE_ERR_OK;
}

//=============================================================================
// Core API: Compute Similarity
//=============================================================================

int financial_resonance_bridge_compute_similarity(
    financial_resonance_bridge_t* bridge,
    const fin_resonance_query_t* query1,
    const fin_resonance_query_t* query2,
    fin_resonance_result_t* out_result)
{
    if (!bridge) {
        set_error("bridge is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_resonance_bridge_compute_similarity: bridge is NULL");
        return FIN_RESONANCE_ERR_NULL;
    }
    if (!query1 || !query2 || !out_result) {
        set_error("NULL parameter");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_resonance_bridge_compute_similarity: NULL parameter");
        return FIN_RESONANCE_ERR_NULL;
    }

    memset(out_result, 0, sizeof(*out_result));
    out_result->trace_id = bridge->config.enable_trace_logging ? gen_trace_id(bridge) : 0;

    /* 1. Jaccard similarity on signature hash bits */
    uint64_t h1 = query1->signature_hash;
    uint64_t h2 = query2->signature_hash;
    uint64_t intersection = __builtin_popcountll(h1 & h2);
    uint64_t union_bits = __builtin_popcountll(h1 | h2);
    out_result->jaccard_score = (union_bits > 0)
        ? (float)intersection / (float)union_bits
        : 0.0f;

    /* 2. Phase coherence score */
    float phase_diff_global = phase_diff(query1->phase, query2->phase);
    out_result->phase_score = 1.0f - phase_diff_global / (float)M_PI;
    out_result->phase_score = nimcp_clampf(out_result->phase_score, 0.0f, 1.0f);

    /* 3. Oscillator phase similarity (mean cosine similarity) */
    uint32_t min_osc = (query1->num_oscillators < query2->num_oscillators)
        ? query1->num_oscillators : query2->num_oscillators;
    float phase_cos_sum = 0.0f;
    for (uint32_t i = 0; i < min_osc; i++) {
        float d = phase_diff(query1->oscillator_phases[i], query2->oscillator_phases[i]);
        phase_cos_sum += cosf(d);  /* cos(0) = 1 for identical phases */
    }
    float osc_score = (min_osc > 0) ? (phase_cos_sum / (float)min_osc + 1.0f) / 2.0f : 0.5f;

    /* 4. Quaternion distance approximation (using oscillator phases as rotation angles) */
    float q1[4] = {1.0f, 0.0f, 0.0f, 0.0f};
    float q2[4] = {1.0f, 0.0f, 0.0f, 0.0f};

    /* Build quaternion from first 3 oscillator phases as rotation angles */
    if (query1->num_oscillators >= 3) {
        float half_phi = query1->oscillator_phases[0] / 2.0f;
        float half_theta = query1->oscillator_phases[1] / 2.0f;
        float half_psi = query1->oscillator_phases[2] / 2.0f;

        q1[0] = cosf(half_phi) * cosf(half_theta) * cosf(half_psi);
        q1[1] = sinf(half_phi) * cosf(half_theta) * cosf(half_psi);
        q1[2] = cosf(half_phi) * sinf(half_theta) * cosf(half_psi);
        q1[3] = cosf(half_phi) * cosf(half_theta) * sinf(half_psi);
    }
    if (query2->num_oscillators >= 3) {
        float half_phi = query2->oscillator_phases[0] / 2.0f;
        float half_theta = query2->oscillator_phases[1] / 2.0f;
        float half_psi = query2->oscillator_phases[2] / 2.0f;

        q2[0] = cosf(half_phi) * cosf(half_theta) * cosf(half_psi);
        q2[1] = sinf(half_phi) * cosf(half_theta) * cosf(half_psi);
        q2[2] = cosf(half_phi) * sinf(half_theta) * cosf(half_psi);
        q2[3] = cosf(half_phi) * cosf(half_theta) * sinf(half_psi);
    }

    /* Quaternion dot product (similarity) */
    float q_dot = q1[0]*q2[0] + q1[1]*q2[1] + q1[2]*q2[2] + q1[3]*q2[3];
    out_result->quaternion_score = fabsf(q_dot);  /* |dot| in [0,1] */

    /* 5. Kuramoto score using oscillator phases */
    float sin_sum = 0.0f, cos_sum = 0.0f;
    for (uint32_t i = 0; i < min_osc; i++) {
        float avg_phase = (query1->oscillator_phases[i] + query2->oscillator_phases[i]) / 2.0f;
        sin_sum += sinf(avg_phase);
        cos_sum += cosf(avg_phase);
    }
    float order_param = (min_osc > 0)
        ? sqrtf(sin_sum * sin_sum + cos_sum * cos_sum) / (float)min_osc
        : 0.0f;
    out_result->kuramoto_score = order_param;

    /* Combined weighted score */
    out_result->combined_score =
        bridge->config.jaccard_weight * out_result->jaccard_score +
        bridge->config.phase_weight * out_result->phase_score * osc_score +
        bridge->config.quaternion_weight * out_result->quaternion_score +
        bridge->config.kuramoto_weight * out_result->kuramoto_score;

    out_result->combined_score = nimcp_clampf(out_result->combined_score, 0.0f, 1.0f);

    /* Update running average */
    bridge->stats.similarity_queries++;
    float n = (float)bridge->stats.similarity_queries;
    bridge->stats.avg_combined_score =
        bridge->stats.avg_combined_score * ((n - 1.0f) / (fabsf(n) > 1e-7f ? n : 1e-7f)) +
        out_result->combined_score / (fabsf(n) > 1e-7f ? n : 1e-7f);

    return FIN_RESONANCE_ERR_OK;
}

//=============================================================================
// Core API: Find Similar Patterns
//=============================================================================

int financial_resonance_bridge_find_similar(
    financial_resonance_bridge_t* bridge,
    const fin_resonance_query_t* query,
    fin_resonance_pattern_t* out_results,
    uint32_t max_results,
    uint32_t* out_count)
{
    if (!bridge) {
        set_error("bridge is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_resonance_bridge_find_similar: bridge is NULL");
        return FIN_RESONANCE_ERR_NULL;
    }
    if (!query || !out_results || !out_count) {
        set_error("NULL parameter");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_resonance_bridge_find_similar: NULL parameter");
        return FIN_RESONANCE_ERR_NULL;
    }

    /* Security validation */
    int val_rc = resonance_validate_subsystems(bridge, "find_similar");
    if (val_rc != FIN_RESONANCE_ERR_OK) return val_rc;

    fin_resonance_heartbeat_instance(bridge->health_agent, "find_similar", 0.0f);
    bridge->state = FIN_RESONANCE_STATE_SEARCHING;

    *out_count = 0;

    if (bridge->pattern_count == 0) {
        bridge->state = FIN_RESONANCE_STATE_IDLE;
        fin_resonance_heartbeat_instance(bridge->health_agent, "find_similar", 1.0f);
        return FIN_RESONANCE_ERR_OK;  /* No patterns to search */
    }

    /* Compute similarity scores for all patterns */
    typedef struct {
        uint32_t idx;
        float score;
    } score_entry_t;

    /* Stack allocation for scoring (limited by max patterns) */
    score_entry_t* scores = (score_entry_t*)nimcp_malloc(
        bridge->pattern_count * sizeof(score_entry_t));
    if (!scores) {
        set_error("Failed to allocate score array");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NO_MEMORY,
            "financial_resonance_bridge_find_similar: allocation failed");
        bridge->state = FIN_RESONANCE_STATE_ERROR;
        return FIN_RESONANCE_ERR_SIMILARITY;
    }

    for (uint32_t i = 0; i < bridge->pattern_count; i++) {
        fin_resonance_result_t result;
        financial_resonance_bridge_compute_similarity(
            bridge, query, &bridge->patterns[i].query, &result);
        scores[i].idx = i;
        scores[i].score = result.combined_score;
    }

    /* Simple selection sort for top-k (descending) */
    uint32_t k = (max_results < bridge->pattern_count) ? max_results : bridge->pattern_count;
    for (uint32_t i = 0; i < k; i++) {
        uint32_t best = i;
        for (uint32_t j = i + 1; j < bridge->pattern_count; j++) {
            if (scores[j].score > scores[best].score) {
                best = j;
            }
        }
        if (best != i) {
            score_entry_t tmp = scores[i];
            scores[i] = scores[best];
            scores[best] = tmp;
        }
    }

    /* Copy top-k patterns to output */
    for (uint32_t i = 0; i < k; i++) {
        /* Apply hash pre-filter threshold */
        if (scores[i].score >= bridge->config.hash_similarity_threshold) {
            uint32_t idx = scores[i].idx;
            out_results[*out_count] = bridge->patterns[idx];
            bridge->patterns[idx].retrieval_count++;
            (*out_count)++;
        }
    }

    nimcp_free(scores);
    scores = NULL;

    bridge->stats.patterns_retrieved += *out_count;
    bridge->state = FIN_RESONANCE_STATE_IDLE;

    fin_resonance_heartbeat_instance(bridge->health_agent, "find_similar", 1.0f);
    bridge->stats.health_heartbeats++;

    return FIN_RESONANCE_ERR_OK;
}

//=============================================================================
// Core API: Kuramoto Coherence
//=============================================================================

int financial_resonance_bridge_kuramoto_coherence(
    financial_resonance_bridge_t* bridge,
    const fin_kuramoto_input_t* input,
    fin_kuramoto_output_t* out_coherence)
{
    if (!bridge) {
        set_error("bridge is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_resonance_bridge_kuramoto_coherence: bridge is NULL");
        return FIN_RESONANCE_ERR_NULL;
    }
    if (!input || !out_coherence) {
        set_error("NULL parameter");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_resonance_bridge_kuramoto_coherence: NULL parameter");
        return FIN_RESONANCE_ERR_NULL;
    }
    if (input->num_assets == 0 || input->num_assets > FIN_RESONANCE_MAX_ASSETS) {
        set_error("Invalid asset count: %u", input->num_assets);
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_INVALID_PARAM,
            "financial_resonance_bridge_kuramoto_coherence: invalid asset count");
        return FIN_RESONANCE_ERR_INVALID_PARAM;
    }

    /* Security validation */
    int val_rc = resonance_validate_subsystems(bridge, "kuramoto_coherence");
    if (val_rc != FIN_RESONANCE_ERR_OK) return val_rc;

    fin_resonance_heartbeat_instance(bridge->health_agent, "kuramoto_coherence", 0.0f);
    bridge->state = FIN_RESONANCE_STATE_COMPUTING;

    memset(out_coherence, 0, sizeof(*out_coherence));

    uint32_t n = input->num_assets;
    float K = bridge->config.kuramoto_coupling_strength;

    /* Compute Kuramoto order parameter: r * exp(i*psi) = (1/N) * sum(exp(i*theta_j)) */
    float sin_sum = 0.0f;
    float cos_sum = 0.0f;

    for (uint32_t i = 0; i < n; i++) {
        sin_sum += sinf(input->phases[i]);
        cos_sum += cosf(input->phases[i]);
    }

    sin_sum /= (float)n;
    cos_sum /= (float)n;

    float order_param = sqrtf(sin_sum * sin_sum + cos_sum * cos_sum);
    float mean_phase = atan2f(sin_sum, cos_sum);

    out_coherence->order_parameter = order_param;
    out_coherence->mean_phase = mean_phase;

    /* Compute phase variance */
    float phase_var = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        float d = phase_diff(input->phases[i], mean_phase);
        phase_var += d * d;
    }
    out_coherence->phase_variance = phase_var / (float)n;

    /* Count synchronized vs desynchronized assets */
    float sync_threshold = (float)M_PI / 4.0f;  /* 45 degrees */
    uint32_t synced = 0;
    uint32_t desynced = 0;

    for (uint32_t i = 0; i < n; i++) {
        float d = phase_diff(input->phases[i], mean_phase);
        if (d < sync_threshold) {
            synced++;
        } else if (d > (float)M_PI - sync_threshold) {
            desynced++;
        }
    }

    out_coherence->num_synced = synced;
    out_coherence->num_desynced = desynced;
    out_coherence->critical_sync = (order_param >= bridge->config.critical_sync_threshold);

    /* Apply health modulation to threshold check */
    float health_mod = 1.0f
        - bridge->inflammation * bridge->config.inflammation_sensitivity * 0.1f
        - bridge->fatigue * bridge->config.fatigue_sensitivity * 0.1f;
    if (health_mod < 0.5f) health_mod = 0.5f;

    /* Check for market-wide synchronization anomaly */
    if (out_coherence->critical_sync && order_param > 0.9f) {
        resonance_present_antigen(bridge, "extreme_sync", 4);
    }

    bridge->stats.coherence_calcs++;
    bridge->state = FIN_RESONANCE_STATE_IDLE;

    /* KG notification for critical sync */
    if (out_coherence->critical_sync) {
        resonance_kg_publish(bridge, KG_MSG_FIN_RESONANCE_UPDATE,
                             out_coherence, sizeof(*out_coherence));
    }

    fin_resonance_heartbeat_instance(bridge->health_agent, "kuramoto_coherence", 1.0f);
    bridge->stats.health_heartbeats++;

    return FIN_RESONANCE_ERR_OK;
}

//=============================================================================
// Core API: Quick Coherence
//=============================================================================

int financial_resonance_bridge_quick_coherence(
    financial_resonance_bridge_t* bridge,
    const float* phases,
    uint32_t count,
    float* out_order_param)
{
    if (!bridge) {
        set_error("bridge is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_resonance_bridge_quick_coherence: bridge is NULL");
        return FIN_RESONANCE_ERR_NULL;
    }
    if (!phases || !out_order_param || count == 0) {
        set_error("NULL parameter or zero count");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_resonance_bridge_quick_coherence: NULL parameter");
        return FIN_RESONANCE_ERR_NULL;
    }

    float sin_sum = 0.0f;
    float cos_sum = 0.0f;

    for (uint32_t i = 0; i < count; i++) {
        sin_sum += sinf(phases[i]);
        cos_sum += cosf(phases[i]);
    }

    sin_sum /= (float)count;
    cos_sum /= (float)count;

    *out_order_param = sqrtf(sin_sum * sin_sum + cos_sum * cos_sum);

    bridge->stats.coherence_calcs++;
    return FIN_RESONANCE_ERR_OK;
}

//=============================================================================
// Pattern Storage: Store
//=============================================================================

int financial_resonance_bridge_store_pattern(
    financial_resonance_bridge_t* bridge,
    const fin_resonance_query_t* query,
    float outcome,
    float importance,
    const char* label)
{
    if (!bridge) {
        set_error("bridge is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_resonance_bridge_store_pattern: bridge is NULL");
        return FIN_RESONANCE_ERR_NULL;
    }
    if (!query) {
        set_error("query is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_resonance_bridge_store_pattern: query is NULL");
        return FIN_RESONANCE_ERR_NULL;
    }

    /* Security validation */
    int val_rc = resonance_validate_subsystems(bridge, "store_pattern");
    if (val_rc != FIN_RESONANCE_ERR_OK) return val_rc;

    fin_resonance_heartbeat_instance(bridge->health_agent, "store_pattern", 0.0f);

    /* Find insertion point */
    uint32_t insert_idx = bridge->pattern_count;

    if (bridge->pattern_count >= bridge->pattern_capacity) {
        /* Find least important pattern to replace */
        float min_importance = bridge->patterns[0].importance;
        uint32_t min_idx = 0;

        for (uint32_t i = 1; i < bridge->pattern_count; i++) {
            if (bridge->patterns[i].importance < min_importance) {
                min_importance = bridge->patterns[i].importance;
                min_idx = i;
            }
        }

        if (importance > min_importance) {
            insert_idx = min_idx;
        } else {
            /* Pattern not important enough to store */
            fin_resonance_heartbeat_instance(bridge->health_agent, "store_pattern", 1.0f);
            return FIN_RESONANCE_ERR_OK;
        }
    }

    /* Store the pattern */
    fin_resonance_pattern_t* pat = &bridge->patterns[insert_idx];
    memset(pat, 0, sizeof(*pat));
    pat->query = *query;
    pat->outcome = outcome;
    pat->importance = importance;
    pat->creation_time_us = 0;  /* Could use system time */
    pat->retrieval_count = 0;
    pat->regime = FIN_REGIME_UNKNOWN;

    if (label) {
        strncpy(pat->label, label, sizeof(pat->label) - 1);
        pat->label[sizeof(pat->label) - 1] = '\0';
    }

    if (insert_idx == bridge->pattern_count && bridge->pattern_count < bridge->pattern_capacity) {
        bridge->pattern_count++;
    }

    bridge->stats.patterns_stored = bridge->pattern_count;

    fin_resonance_heartbeat_instance(bridge->health_agent, "store_pattern", 1.0f);
    return FIN_RESONANCE_ERR_OK;
}

//=============================================================================
// Pattern Storage: Consolidate
//=============================================================================

int financial_resonance_bridge_consolidate(financial_resonance_bridge_t* bridge) {
    if (!bridge) {
        set_error("bridge is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_resonance_bridge_consolidate: bridge is NULL");
        return FIN_RESONANCE_ERR_NULL;
    }

    fin_resonance_heartbeat_instance(bridge->health_agent, "consolidate", 0.0f);

    float threshold = bridge->config.pattern_consolidation_threshold;
    uint32_t write_idx = 0;

    for (uint32_t i = 0; i < bridge->pattern_count; i++) {
        float effective_importance = bridge->patterns[i].importance;

        /* Boost importance for frequently retrieved patterns */
        if (bridge->patterns[i].retrieval_count > 0) {
            effective_importance += 0.1f * (float)bridge->patterns[i].retrieval_count;
        }

        if (effective_importance >= threshold) {
            /* Keep pattern */
            if (write_idx != i) {
                bridge->patterns[write_idx] = bridge->patterns[i];
            }
            /* Reset retrieval count after consolidation */
            bridge->patterns[write_idx].retrieval_count = 0;
            write_idx++;
        }
    }

    bridge->pattern_count = write_idx;
    bridge->stats.patterns_stored = bridge->pattern_count;

    fin_resonance_heartbeat_instance(bridge->health_agent, "consolidate", 1.0f);
    return FIN_RESONANCE_ERR_OK;
}

//=============================================================================
// Pattern Storage: Get Count
//=============================================================================

uint32_t financial_resonance_bridge_get_pattern_count(
    const financial_resonance_bridge_t* bridge)
{
    if (!bridge) return 0;
    return bridge->pattern_count;
}

//=============================================================================
// Health & Modulation
//=============================================================================

int financial_resonance_bridge_check_health(financial_resonance_bridge_t* bridge) {
    if (!bridge) {
        set_error("bridge is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_resonance_bridge_check_health: bridge is NULL");
        return FIN_RESONANCE_ERR_NULL;
    }

    fin_resonance_heartbeat_instance(bridge->health_agent, "check_health", 0.0f);

    /* Check for degraded conditions */
    float health_penalty = bridge->inflammation * bridge->config.inflammation_sensitivity
                         + bridge->fatigue * bridge->config.fatigue_sensitivity;

    if (health_penalty > 1.5f) {
        if (bridge->state != FIN_RESONANCE_STATE_DEGRADED &&
            bridge->state != FIN_RESONANCE_STATE_ERROR) {
            bridge->state = FIN_RESONANCE_STATE_DEGRADED;
        }
    } else if (bridge->state == FIN_RESONANCE_STATE_DEGRADED && health_penalty < 0.5f) {
        /* Recover from degraded state */
        bridge->state = FIN_RESONANCE_STATE_IDLE;
    }

    fin_resonance_heartbeat_instance(bridge->health_agent, "check_health", 1.0f);
    bridge->stats.health_heartbeats++;

    return FIN_RESONANCE_ERR_OK;
}

int financial_resonance_bridge_heartbeat(
    financial_resonance_bridge_t* bridge,
    const char* operation,
    float progress)
{
    if (!bridge) return FIN_RESONANCE_ERR_NULL;

    /* Forward to global health agent */
    fin_resonance_heartbeat(operation ? operation : "heartbeat", progress);

    /* Forward to instance health agent */
    if (bridge->health_agent) {
        nimcp_health_agent_heartbeat_ex(bridge->health_agent, operation, progress);
    }

    bridge->stats.health_heartbeats++;
    return FIN_RESONANCE_ERR_OK;
}

int financial_resonance_bridge_set_inflammation(
    financial_resonance_bridge_t* bridge, float level)
{
    if (!bridge) {
        set_error("bridge is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_resonance_bridge_set_inflammation: bridge is NULL");
        return FIN_RESONANCE_ERR_NULL;
    }
    bridge->inflammation = nimcp_clampf(level, 0.0f, 1.0f);
    return FIN_RESONANCE_ERR_OK;
}

int financial_resonance_bridge_set_fatigue(
    financial_resonance_bridge_t* bridge, float level)
{
    if (!bridge) {
        set_error("bridge is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_resonance_bridge_set_fatigue: bridge is NULL");
        return FIN_RESONANCE_ERR_NULL;
    }
    bridge->fatigue = nimcp_clampf(level, 0.0f, 1.0f);
    return FIN_RESONANCE_ERR_OK;
}

int financial_resonance_bridge_get_stats(
    const financial_resonance_bridge_t* bridge,
    fin_resonance_bridge_stats_t* out_stats)
{
    if (!bridge) {
        set_error("bridge is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_resonance_bridge_get_stats: bridge is NULL");
        return FIN_RESONANCE_ERR_NULL;
    }
    if (!out_stats) {
        set_error("out_stats is NULL");
        NIMCP_THROW_IMMUNE_RECOVER(NIMCP_ERROR_NULL_POINTER,
            "financial_resonance_bridge_get_stats: out_stats is NULL");
        return FIN_RESONANCE_ERR_NULL;
    }

    *out_stats = bridge->stats;
    return FIN_RESONANCE_ERR_OK;
}

void financial_resonance_bridge_reset_stats(financial_resonance_bridge_t* bridge) {
    if (!bridge) return;
    uint32_t preserved_patterns = bridge->stats.patterns_stored;
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    bridge->stats.patterns_stored = preserved_patterns;
}

const char* financial_resonance_bridge_get_last_error(void) {
    return fin_resonance_last_error;
}
