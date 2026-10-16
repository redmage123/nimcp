/**
 * @file nimcp_parietal_linguistics_statistics_bridge.c
 * @brief Statistics & ML Integration Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-31
 */

#include "cognitive/parietal/linguistics/bridges/nimcp_parietal_linguistics_statistics_bridge.h"
#include "constants/nimcp_buffer_constants.h"
#include "cognitive/parietal/linguistics/nimcp_parietal_linguistics_mesh.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <time.h>
#include <stdio.h>
#include <stdarg.h>
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "constants/nimcp_learning_constants.h"

/* ============================================================================
 * CONSTANTS
 * ============================================================================ */

#define LOG_MODULE_LING_STATS "LING_STATS"
#define STATS_BRIDGE_MAGIC 0x53544154  /* "STAT" */
#define LOG_ZERO (-1e10f)

/* ============================================================================
 * INTERNAL STRUCTURES
 * ============================================================================ */

/**
 * @brief HMM internal state
 */
typedef struct {
    /* Transition matrix: P(state_t | state_{t-1}) */
    float transition[NUM_HMM_STATE_COUNT][NUM_HMM_STATE_COUNT];

    /* Emission matrix: P(observation | state) */
    float emission[NUM_HMM_STATE_COUNT][NUM_OBS_COUNT];

    /* Initial state distribution */
    float initial[NUM_HMM_STATE_COUNT];

    /* Temporary buffers for Viterbi/Forward */
    float alpha[NUM_HMM_STATE_COUNT];
    float alpha_prev[NUM_HMM_STATE_COUNT];
    int backpointer[256][NUM_HMM_STATE_COUNT];  /* Max sequence length 256 */

    bool initialized;
} num_hmm_model_t;

/**
 * @brief Phoneme distribution for information theory
 */
typedef struct {
    float* feature_dist;                /**< Feature distribution */
    uint32_t num_features;
    uint32_t num_phonemes;
} phoneme_model_t;

/**
 * @brief Internal statistics bridge state
 */
struct ling_stats_bridge {
    uint32_t magic;                     /**< Validation magic */

    /* Configuration */
    ling_stats_bridge_config_t config;

    /* HMM model */
    num_hmm_model_t hmm;

    /* Reference frame priors */
    float frame_priors[LING_STATS_MAX_REF_FRAMES];

    /* Phoneme model for information theory */
    phoneme_model_t phoneme_model;

    /* Mesh integration */
    linguistics_mesh_t* mesh;
    bool mesh_registered;
    linguistics_belief_t current_belief;
    float current_precision;

    /* Statistics */
    ling_stats_bridge_stats_t stats;

    /* Thread safety */
    void* mutex;

    /* Timing */
    uint64_t creation_time_ms;
};

/* ============================================================================
 * THREAD-LOCAL ERROR
 * ============================================================================ */

static _Thread_local char s_last_error[NIMCP_ERROR_BUFFER_SIZE] = {0};

static void set_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(s_last_error, sizeof(s_last_error), fmt, args);
    va_end(args);
}

/* ============================================================================
 * INTERNAL HELPERS
 * ============================================================================ */

static bool is_valid_bridge(const ling_stats_bridge_t* bridge) {
    return bridge && bridge->magic == STATS_BRIDGE_MAGIC;
}

static uint64_t get_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

static uint64_t get_time_ms(void) {
    return get_time_us() / 1000ULL;
}

static float log_add(float log_a, float log_b) {
    /* log(a + b) = log(a) + log(1 + exp(log(b) - log(a))) */
    if (log_a < LOG_ZERO) return log_b;
    if (log_b < LOG_ZERO) return log_a;
    if (log_a > log_b) {
        return log_a + log1pf(expf(log_b - log_a));
    } else {
        return log_b + log1pf(expf(log_a - log_b));
    }
}

static float safe_log(float x) {
    if (x <= 0.0f) return LOG_ZERO;
    return logf(x);
}

/**
 * @brief Initialize default HMM parameters
 */
static void init_default_hmm(num_hmm_model_t* hmm) {
    memset(hmm, 0, sizeof(*hmm));

    /* Initialize with small values (smoothing) */
    float smoothing = 1e-6f;
    for (int i = 0; i < NUM_HMM_STATE_COUNT; i++) {
        for (int j = 0; j < NUM_HMM_STATE_COUNT; j++) {
            hmm->transition[i][j] = smoothing;
        }
        for (int j = 0; j < NUM_OBS_COUNT; j++) {
            hmm->emission[i][j] = smoothing;
        }
        hmm->initial[i] = smoothing;
    }

    /* Set meaningful transitions */
    /* START -> UNITS, TEENS, TENS, HUNDRED, THOUSAND, ... */
    hmm->initial[NUM_HMM_STATE_START] = 1.0f;

    hmm->transition[NUM_HMM_STATE_START][NUM_HMM_STATE_UNITS] = 0.3f;
    hmm->transition[NUM_HMM_STATE_START][NUM_HMM_STATE_TEENS] = 0.1f;
    hmm->transition[NUM_HMM_STATE_START][NUM_HMM_STATE_TENS] = 0.2f;
    hmm->transition[NUM_HMM_STATE_START][NUM_HMM_STATE_HUNDRED] = 0.2f;
    hmm->transition[NUM_HMM_STATE_START][NUM_HMM_STATE_THOUSAND] = 0.1f;
    hmm->transition[NUM_HMM_STATE_START][NUM_HMM_STATE_MILLION] = 0.05f;
    hmm->transition[NUM_HMM_STATE_START][NUM_HMM_STATE_BILLION] = 0.05f;

    /* TENS -> UNITS (twenty-one), END */
    hmm->transition[NUM_HMM_STATE_TENS][NUM_HMM_STATE_TENS_UNITS] = 0.4f;
    hmm->transition[NUM_HMM_STATE_TENS][NUM_HMM_STATE_END] = 0.3f;
    hmm->transition[NUM_HMM_STATE_TENS][NUM_HMM_STATE_HUNDRED] = 0.15f;
    hmm->transition[NUM_HMM_STATE_TENS][NUM_HMM_STATE_THOUSAND] = 0.15f;

    /* TENS_UNITS -> END, HUNDRED, THOUSAND */
    hmm->transition[NUM_HMM_STATE_TENS_UNITS][NUM_HMM_STATE_END] = 0.6f;
    hmm->transition[NUM_HMM_STATE_TENS_UNITS][NUM_HMM_STATE_HUNDRED] = 0.2f;
    hmm->transition[NUM_HMM_STATE_TENS_UNITS][NUM_HMM_STATE_THOUSAND] = 0.2f;

    /* HUNDRED -> UNITS, TENS, AND, END */
    hmm->transition[NUM_HMM_STATE_HUNDRED][NUM_HMM_STATE_HUNDRED_UNITS] = 0.3f;
    hmm->transition[NUM_HMM_STATE_HUNDRED][NUM_HMM_STATE_TENS] = 0.3f;
    hmm->transition[NUM_HMM_STATE_HUNDRED][NUM_HMM_STATE_END] = 0.2f;
    hmm->transition[NUM_HMM_STATE_HUNDRED][NUM_HMM_STATE_THOUSAND] = 0.2f;

    /* UNITS -> END, ORDINAL */
    hmm->transition[NUM_HMM_STATE_UNITS][NUM_HMM_STATE_END] = 0.7f;
    hmm->transition[NUM_HMM_STATE_UNITS][NUM_HMM_STATE_ORDINAL] = 0.15f;
    hmm->transition[NUM_HMM_STATE_UNITS][NUM_HMM_STATE_HUNDRED] = 0.15f;

    /* TEENS -> END, ORDINAL */
    hmm->transition[NUM_HMM_STATE_TEENS][NUM_HMM_STATE_END] = 0.7f;
    hmm->transition[NUM_HMM_STATE_TEENS][NUM_HMM_STATE_ORDINAL] = 0.3f;

    /* ORDINAL -> END */
    hmm->transition[NUM_HMM_STATE_ORDINAL][NUM_HMM_STATE_END] = 1.0f;

    /* Set emissions */
    /* UNITS emit 1-9 */
    for (int i = NUM_OBS_ONE; i <= NUM_OBS_NINE; i++) {
        hmm->emission[NUM_HMM_STATE_UNITS][i] = 0.11f;
    }

    /* TEENS emit 10-19 */
    for (int i = NUM_OBS_TEN; i <= NUM_OBS_NINETEEN; i++) {
        hmm->emission[NUM_HMM_STATE_TEENS][i] = 0.1f;
    }

    /* TENS emit twenty, thirty, ... ninety */
    hmm->emission[NUM_HMM_STATE_TENS][NUM_OBS_TWENTY] = 0.14f;
    hmm->emission[NUM_HMM_STATE_TENS][NUM_OBS_THIRTY] = 0.14f;
    hmm->emission[NUM_HMM_STATE_TENS][NUM_OBS_FORTY] = 0.14f;
    hmm->emission[NUM_HMM_STATE_TENS][NUM_OBS_FIFTY] = 0.14f;
    hmm->emission[NUM_HMM_STATE_TENS][NUM_OBS_SIXTY] = 0.11f;
    hmm->emission[NUM_HMM_STATE_TENS][NUM_OBS_SEVENTY] = 0.11f;
    hmm->emission[NUM_HMM_STATE_TENS][NUM_OBS_EIGHTY] = 0.11f;
    hmm->emission[NUM_HMM_STATE_TENS][NUM_OBS_NINETY] = 0.11f;

    /* TENS_UNITS emit 1-9 */
    for (int i = NUM_OBS_ONE; i <= NUM_OBS_NINE; i++) {
        hmm->emission[NUM_HMM_STATE_TENS_UNITS][i] = 0.11f;
    }

    /* HUNDRED emits "hundred" */
    hmm->emission[NUM_HMM_STATE_HUNDRED][NUM_OBS_HUNDRED] = 0.95f;

    /* THOUSAND emits "thousand" */
    hmm->emission[NUM_HMM_STATE_THOUSAND][NUM_OBS_THOUSAND] = 0.95f;

    /* MILLION emits "million" */
    hmm->emission[NUM_HMM_STATE_MILLION][NUM_OBS_MILLION] = 0.95f;

    /* ORDINAL emits ordinal suffix */
    hmm->emission[NUM_HMM_STATE_ORDINAL][NUM_OBS_ORDINAL_SUFFIX] = 0.95f;

    /* Normalize */
    for (int i = 0; i < NUM_HMM_STATE_COUNT; i++) {
        float sum_t = 0.0f, sum_e = 0.0f;
        for (int j = 0; j < NUM_HMM_STATE_COUNT; j++) sum_t += hmm->transition[i][j];
        for (int j = 0; j < NUM_OBS_COUNT; j++) sum_e += hmm->emission[i][j];

        if (sum_t > 0.0f) {
            for (int j = 0; j < NUM_HMM_STATE_COUNT; j++)
                hmm->transition[i][j] /= sum_t;
        }
        if (sum_e > 0.0f) {
            for (int j = 0; j < NUM_OBS_COUNT; j++)
                hmm->emission[i][j] /= sum_e;
        }
    }

    hmm->initialized = true;
}

/**
 * @brief Initialize phoneme model for information theory
 */
static void init_phoneme_model(phoneme_model_t* model) {
    /* Initialize with 44 English phonemes, 8 features each */
    model->num_phonemes = 44;
    model->num_features = 8;

    /* Allocate feature distributions */
    model->feature_dist = (float*)nimcp_calloc(model->num_phonemes * model->num_features,
                                          sizeof(float));
    if (!model->feature_dist) return;

    /* Initialize with uniform + noise */
    for (uint32_t p = 0; p < model->num_phonemes; p++) {
        for (uint32_t f = 0; f < model->num_features; f++) {
            /* Each phoneme has a characteristic feature pattern */
            model->feature_dist[p * model->num_features + f] =
                0.1f + 0.1f * ((float)(p + f) / (model->num_phonemes + model->num_features));
        }
    }
}

/* ============================================================================
 * LIFECYCLE API IMPLEMENTATION
 * ============================================================================ */

ling_stats_bridge_config_t ling_stats_bridge_default_config(void) {
    ling_stats_bridge_config_t config = {
        .hmm_smoothing = 1e-6f,
        .hmm_max_iterations = 100,
        .hmm_convergence_threshold = 1e-4f,
        .prior_egocentric = 0.4f,
        .prior_allocentric = 0.3f,
        .prior_intrinsic = 0.2f,
        .prior_relative = 0.1f,
        .entropy_bins = 32,
        .use_kl_divergence = true,
        .enable_mesh = true,
        .mesh_learning_rate = NIMCP_LEARNING_RATE_COARSE,
        .enable_bbb = true,
        .enable_health = true,
        .enable_logging = true
    };
    return config;
}

ling_stats_bridge_t* ling_stats_bridge_create(
    const ling_stats_bridge_config_t* config
) {
    ling_stats_bridge_t* bridge = (ling_stats_bridge_t*)nimcp_calloc(1, sizeof(*bridge));
    if (!bridge) {
        set_error("Failed to allocate statistics bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "ling_stats_bridge_create: bridge is NULL");
        return NULL;
    }

    bridge->magic = STATS_BRIDGE_MAGIC;
    bridge->config = config ? *config : ling_stats_bridge_default_config();
    bridge->current_precision = LING_STATS_DEFAULT_PRECISION;
    bridge->creation_time_ms = get_time_ms();

    /* Initialize HMM */
    init_default_hmm(&bridge->hmm);

    /* Initialize reference frame priors */
    bridge->frame_priors[REF_FRAME_EGOCENTRIC] = bridge->config.prior_egocentric;
    bridge->frame_priors[REF_FRAME_ALLOCENTRIC] = bridge->config.prior_allocentric;
    bridge->frame_priors[REF_FRAME_INTRINSIC] = bridge->config.prior_intrinsic;
    bridge->frame_priors[REF_FRAME_RELATIVE] = bridge->config.prior_relative;

    /* Initialize phoneme model */
    init_phoneme_model(&bridge->phoneme_model);

    /* Initialize belief */
    bridge->current_belief.certainty = 0.5f;
    bridge->current_belief.precision = bridge->current_precision;

    return bridge;
}

void ling_stats_bridge_destroy(ling_stats_bridge_t* bridge) {
    if (!is_valid_bridge(bridge)) {
        return;
    }

    if (bridge->phoneme_model.feature_dist) {
        nimcp_free(bridge->phoneme_model.feature_dist);
    }

    bridge->magic = 0;
    nimcp_free(bridge);
    bridge = NULL;
}

int ling_stats_bridge_register_mesh(
    ling_stats_bridge_t* bridge,
    linguistics_mesh_t* mesh
) {
    if (!is_valid_bridge(bridge) || !mesh) {
        return LING_STATS_ERR_NULL;
    }

    linguistics_mesh_handler_t handler;
    int ret = ling_stats_get_mesh_handler(bridge, &handler);
    if (ret != LING_STATS_ERR_OK) {
        return ret;
    }

    ret = linguistics_mesh_register_participant(
        mesh,
        BIO_MODULE_LING_STATS_BRIDGE,
        "statistics_bridge",
        handler
    );
    if (ret != 0) {
        set_error("Failed to register with mesh: %d", ret);
        return LING_STATS_ERR_MESH_REGISTER;
    }

    bridge->mesh = mesh;
    bridge->mesh_registered = true;

    return LING_STATS_ERR_OK;
}

/* ============================================================================
 * HMM API IMPLEMENTATION
 * ============================================================================ */

int ling_stats_hmm_init_default(ling_stats_bridge_t* bridge) {
    if (!is_valid_bridge(bridge)) {
        return LING_STATS_ERR_NULL;
    }

    init_default_hmm(&bridge->hmm);
    return LING_STATS_ERR_OK;
}

int ling_stats_hmm_viterbi_decode(
    ling_stats_bridge_t* bridge,
    const num_observation_t* observations,
    uint32_t num_obs,
    num_hmm_decode_result_t* result
) {
    if (!is_valid_bridge(bridge) || !observations || !result) {
        return LING_STATS_ERR_NULL;
    }
    if (!bridge->hmm.initialized) {
        set_error("HMM not initialized");
        return LING_STATS_ERR_HMM_FAIL;
    }
    if (num_obs == 0 || num_obs > 256) {
        set_error("Invalid observation count: %u", num_obs);
        return LING_STATS_ERR_INVALID_STATE;
    }

    uint64_t start_us = get_time_us();
    num_hmm_model_t* hmm = &bridge->hmm;

    /* Viterbi algorithm in log space */
    float viterbi[NUM_HMM_STATE_COUNT];
    float viterbi_prev[NUM_HMM_STATE_COUNT];

    /* Initialize: viterbi[s] = log(initial[s]) + log(emission[s][obs[0]]) */
    for (int s = 0; s < NUM_HMM_STATE_COUNT; s++) {
        viterbi_prev[s] = safe_log(hmm->initial[s]) +
                          safe_log(hmm->emission[s][observations[0]]);
        hmm->backpointer[0][s] = NUM_HMM_STATE_START;
    }

    /* Recursion */
    for (uint32_t t = 1; t < num_obs; t++) {
        for (int s = 0; s < NUM_HMM_STATE_COUNT; s++) {
            float best_score = LOG_ZERO;
            int best_prev = 0;

            for (int prev = 0; prev < NUM_HMM_STATE_COUNT; prev++) {
                float score = viterbi_prev[prev] + safe_log(hmm->transition[prev][s]);
                if (score > best_score) {
                    best_score = score;
                    best_prev = prev;
                }
            }

            viterbi[s] = best_score + safe_log(hmm->emission[s][observations[t]]);
            hmm->backpointer[t][s] = best_prev;
        }

        memcpy(viterbi_prev, viterbi, sizeof(viterbi));
    }

    /* Find best final state */
    float best_final_score = LOG_ZERO;
    int best_final_state = NUM_HMM_STATE_END;
    for (int s = 0; s < NUM_HMM_STATE_COUNT; s++) {
        if (viterbi_prev[s] > best_final_score) {
            best_final_score = viterbi_prev[s];
            best_final_state = s;
        }
    }

    /* Backtrace */
    result->state_sequence = (num_hmm_state_t*)nimcp_calloc(num_obs, sizeof(num_hmm_state_t));
    if (!result->state_sequence) {
        return LING_STATS_ERR_HMM_FAIL;
    }

    result->state_sequence[num_obs - 1] = (num_hmm_state_t)best_final_state;
    for (int t = num_obs - 2; t >= 0; t--) {
        result->state_sequence[t] = (num_hmm_state_t)hmm->backpointer[t + 1][result->state_sequence[t + 1]];
    }

    result->sequence_length = num_obs;
    result->log_probability = best_final_score;
    result->confidence = 1.0f / (1.0f + expf(-best_final_score / num_obs));

    /* Update stats */
    bridge->stats.hmm_decodes++;
    if (isfinite(result->confidence)) {
        bridge->stats.avg_hmm_confidence = bridge->stats.avg_hmm_confidence * 0.99f +
                                            result->confidence * 0.01f;
    }
    bridge->stats.avg_latency_us = bridge->stats.avg_latency_us * 0.99f +
                                    (get_time_us() - start_us) * 0.01f;

    /* Update precision based on confidence */
    bridge->current_precision = LING_STATS_DEFAULT_PRECISION * result->confidence;

    return LING_STATS_ERR_OK;
}

int ling_stats_hmm_forward(
    ling_stats_bridge_t* bridge,
    const num_observation_t* observations,
    uint32_t num_obs,
    float* log_probability
) {
    if (!is_valid_bridge(bridge) || !observations || !log_probability) {
        return LING_STATS_ERR_NULL;
    }
    if (!bridge->hmm.initialized || num_obs == 0) {
        return LING_STATS_ERR_HMM_FAIL;
    }

    num_hmm_model_t* hmm = &bridge->hmm;

    /* Initialize alpha */
    for (int s = 0; s < NUM_HMM_STATE_COUNT; s++) {
        hmm->alpha_prev[s] = safe_log(hmm->initial[s]) +
                             safe_log(hmm->emission[s][observations[0]]);
    }

    /* Recursion */
    for (uint32_t t = 1; t < num_obs; t++) {
        for (int s = 0; s < NUM_HMM_STATE_COUNT; s++) {
            float log_sum = LOG_ZERO;
            for (int prev = 0; prev < NUM_HMM_STATE_COUNT; prev++) {
                log_sum = log_add(log_sum,
                                  hmm->alpha_prev[prev] + safe_log(hmm->transition[prev][s]));
            }
            hmm->alpha[s] = log_sum + safe_log(hmm->emission[s][observations[t]]);
        }
        memcpy(hmm->alpha_prev, hmm->alpha, sizeof(hmm->alpha));
    }

    /* Sum over final states */
    float log_prob = LOG_ZERO;
    for (int s = 0; s < NUM_HMM_STATE_COUNT; s++) {
        log_prob = log_add(log_prob, hmm->alpha_prev[s]);
    }

    *log_probability = log_prob;
    bridge->stats.hmm_forward_passes++;

    return LING_STATS_ERR_OK;
}

int ling_stats_hmm_predict_next(
    ling_stats_bridge_t* bridge,
    num_hmm_state_t current_state,
    float* predictions,
    uint32_t num_predictions
) {
    if (!is_valid_bridge(bridge) || !predictions) {
        return LING_STATS_ERR_NULL;
    }
    if (current_state < 0 || current_state >= NUM_HMM_STATE_COUNT) {
        return LING_STATS_ERR_INVALID_STATE;
    }
    if (num_predictions < NUM_OBS_COUNT) {
        return LING_STATS_ERR_INVALID_STATE;
    }

    num_hmm_model_t* hmm = &bridge->hmm;

    /* P(obs) = Σ_s' P(s' | s) × P(obs | s') */
    memset(predictions, 0, num_predictions * sizeof(float));

    for (int next_state = 0; next_state < NUM_HMM_STATE_COUNT; next_state++) {
        float trans_prob = hmm->transition[current_state][next_state];
        for (int obs = 0; obs < NUM_OBS_COUNT; obs++) {
            predictions[obs] += trans_prob * hmm->emission[next_state][obs];
        }
    }

    return LING_STATS_ERR_OK;
}

int ling_stats_parse_number_word(const char* word, num_observation_t* obs) {
    if (!word || !obs) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ling_stats_parse_number_word: required parameter is NULL (word, obs)");
        return -1;
    }

    static const struct {
        const char* word;
        num_observation_t obs;
    } mappings[] = {
        {"zero", NUM_OBS_ZERO}, {"one", NUM_OBS_ONE}, {"two", NUM_OBS_TWO},
        {"three", NUM_OBS_THREE}, {"four", NUM_OBS_FOUR}, {"five", NUM_OBS_FIVE},
        {"six", NUM_OBS_SIX}, {"seven", NUM_OBS_SEVEN}, {"eight", NUM_OBS_EIGHT},
        {"nine", NUM_OBS_NINE}, {"ten", NUM_OBS_TEN}, {"eleven", NUM_OBS_ELEVEN},
        {"twelve", NUM_OBS_TWELVE}, {"thirteen", NUM_OBS_THIRTEEN},
        {"fourteen", NUM_OBS_FOURTEEN}, {"fifteen", NUM_OBS_FIFTEEN},
        {"sixteen", NUM_OBS_SIXTEEN}, {"seventeen", NUM_OBS_SEVENTEEN},
        {"eighteen", NUM_OBS_EIGHTEEN}, {"nineteen", NUM_OBS_NINETEEN},
        {"twenty", NUM_OBS_TWENTY}, {"thirty", NUM_OBS_THIRTY},
        {"forty", NUM_OBS_FORTY}, {"fifty", NUM_OBS_FIFTY},
        {"sixty", NUM_OBS_SIXTY}, {"seventy", NUM_OBS_SEVENTY},
        {"eighty", NUM_OBS_EIGHTY}, {"ninety", NUM_OBS_NINETY},
        {"hundred", NUM_OBS_HUNDRED}, {"thousand", NUM_OBS_THOUSAND},
        {"million", NUM_OBS_MILLION}, {"billion", NUM_OBS_BILLION},
        {"and", NUM_OBS_AND}, {"-", NUM_OBS_HYPHEN},
        {NULL, NUM_OBS_COUNT}
    };

    for (int i = 0; mappings[i].word != NULL; i++) {
        if (strcasecmp(word, mappings[i].word) == 0) {
            *obs = mappings[i].obs;
            return 0;
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "ling_stats_parse_number_word: validation failed");
    return -1;
}

const char* ling_stats_observation_name(num_observation_t obs) {
    static const char* names[] = {
        "zero", "one", "two", "three", "four", "five",
        "six", "seven", "eight", "nine", "ten",
        "eleven", "twelve", "thirteen", "fourteen", "fifteen",
        "sixteen", "seventeen", "eighteen", "nineteen",
        "twenty", "thirty", "forty", "fifty",
        "sixty", "seventy", "eighty", "ninety",
        "hundred", "thousand", "million", "billion",
        "and", "hyphen", "ordinal_suffix"
    };

    if (obs >= 0 && obs < NUM_OBS_COUNT) {
        return names[obs];
    }
    return "unknown";
}

/* ============================================================================
 * BAYESIAN REFERENCE FRAME API IMPLEMENTATION
 * ============================================================================ */

int ling_stats_bayes_select_frame(
    ling_stats_bridge_t* bridge,
    spatial_preposition_t spatial_word,
    const float* context_cues,
    uint32_t num_cues,
    ref_frame_selection_result_t* result
) {
    if (!is_valid_bridge(bridge) || !result) {
        return LING_STATS_ERR_NULL;
    }

    uint64_t start_us = get_time_us();
    memset(result, 0, sizeof(*result));

    /* Define likelihood based on spatial word */
    /* Different prepositions prefer different reference frames */
    float likelihoods[LING_STATS_MAX_REF_FRAMES];

    /* Default likelihoods */
    likelihoods[REF_FRAME_EGOCENTRIC] = 0.25f;
    likelihoods[REF_FRAME_ALLOCENTRIC] = 0.25f;
    likelihoods[REF_FRAME_INTRINSIC] = 0.25f;
    likelihoods[REF_FRAME_RELATIVE] = 0.25f;

    /* Adjust based on preposition */
    switch (spatial_word) {
        case SPATIAL_PREP_LEFT:
        case SPATIAL_PREP_RIGHT:
        case SPATIAL_PREP_FRONT:
        case SPATIAL_PREP_BEHIND:
            /* Directional prepositions prefer egocentric or relative */
            likelihoods[REF_FRAME_EGOCENTRIC] = 0.4f;
            likelihoods[REF_FRAME_RELATIVE] = 0.35f;
            likelihoods[REF_FRAME_ALLOCENTRIC] = 0.15f;
            likelihoods[REF_FRAME_INTRINSIC] = 0.1f;
            break;

        case SPATIAL_PREP_NEAR:
        case SPATIAL_PREP_FAR:
            /* Distance prepositions can use any frame */
            likelihoods[REF_FRAME_EGOCENTRIC] = 0.3f;
            likelihoods[REF_FRAME_ALLOCENTRIC] = 0.3f;
            likelihoods[REF_FRAME_INTRINSIC] = 0.2f;
            likelihoods[REF_FRAME_RELATIVE] = 0.2f;
            break;

        case SPATIAL_PREP_ABOVE:
        case SPATIAL_PREP_BELOW:
            /* Vertical prepositions prefer allocentric (gravity-defined) */
            likelihoods[REF_FRAME_ALLOCENTRIC] = 0.5f;
            likelihoods[REF_FRAME_INTRINSIC] = 0.3f;
            likelihoods[REF_FRAME_EGOCENTRIC] = 0.1f;
            likelihoods[REF_FRAME_RELATIVE] = 0.1f;
            break;

        case SPATIAL_PREP_IN:
        case SPATIAL_PREP_ON:
            /* Containment/support prefer intrinsic */
            likelihoods[REF_FRAME_INTRINSIC] = 0.6f;
            likelihoods[REF_FRAME_ALLOCENTRIC] = 0.2f;
            likelihoods[REF_FRAME_EGOCENTRIC] = 0.1f;
            likelihoods[REF_FRAME_RELATIVE] = 0.1f;
            break;

        default:
            break;
    }

    /* Modify likelihoods based on context cues */
    if (context_cues && num_cues > 0) {
        /* Cue 0: speaker/listener alignment (high = same perspective) */
        if (num_cues > 0 && context_cues[0] > 0.7f) {
            likelihoods[REF_FRAME_RELATIVE] *= 1.5f;
        }
        /* Cue 1: object has intrinsic front (high = has front) */
        if (num_cues > 1 && context_cues[1] > 0.5f) {
            likelihoods[REF_FRAME_INTRINSIC] *= 1.5f;
        }
        /* Cue 2: indoor/outdoor (high = outdoor, favors allocentric) */
        if (num_cues > 2 && context_cues[2] > 0.7f) {
            likelihoods[REF_FRAME_ALLOCENTRIC] *= 1.3f;
        }
    }

    /* Compute posteriors: P(frame|evidence) ∝ P(evidence|frame) × P(frame) */
    float evidence = 0.0f;  /* Normalization constant */
    result->num_hypotheses = 4;

    for (uint32_t i = 0; i < result->num_hypotheses; i++) {
        result->hypotheses[i].frame = (reference_frame_t)i;
        result->hypotheses[i].prior = bridge->frame_priors[i];
        result->hypotheses[i].likelihood = likelihoods[i];
        result->hypotheses[i].posterior = likelihoods[i] * bridge->frame_priors[i];
        evidence += result->hypotheses[i].posterior;
    }

    /* Normalize posteriors */
    float max_posterior = 0.0f;
    reference_frame_t best_frame = REF_FRAME_EGOCENTRIC;

    for (uint32_t i = 0; i < result->num_hypotheses; i++) {
        if (evidence > 0.0f) {
            result->hypotheses[i].posterior /= evidence;
        }
        if (result->hypotheses[i].posterior > max_posterior) {
            max_posterior = result->hypotheses[i].posterior;
            best_frame = result->hypotheses[i].frame;
        }
    }

    result->selected_frame = best_frame;
    result->confidence = max_posterior;

    /* Precision based on how decisive the selection is */
    float entropy = 0.0f;
    for (uint32_t i = 0; i < result->num_hypotheses; i++) {
        float p = result->hypotheses[i].posterior;
        if (p > 0.0f) {
            entropy -= p * log2f(p);
        }
    }
    /* Max entropy = log2(4) = 2 bits */
    float normalized_entropy = entropy / 2.0f;
    result->precision = LING_STATS_DEFAULT_PRECISION * (1.0f - normalized_entropy);

    /* Update stats */
    bridge->stats.bayesian_inferences++;
    if (isfinite(result->confidence)) {
        bridge->stats.avg_bayes_confidence = bridge->stats.avg_bayes_confidence * 0.99f +
                                              result->confidence * 0.01f;
    }
    bridge->stats.avg_latency_us = bridge->stats.avg_latency_us * 0.99f +
                                    (get_time_us() - start_us) * 0.01f;

    bridge->current_precision = result->precision;

    return LING_STATS_ERR_OK;
}

int ling_stats_bayes_update_prior(
    ling_stats_bridge_t* bridge,
    reference_frame_t frame,
    float evidence_strength
) {
    if (!is_valid_bridge(bridge)) {
        return LING_STATS_ERR_NULL;
    }
    if (frame < 0 || frame >= LING_STATS_MAX_REF_FRAMES) {
        return LING_STATS_ERR_INVALID_STATE;
    }

    /* Bayesian update: prior' = prior × likelihood / evidence */
    /* Simplified: nudge prior toward evidence */
    float lr = NIMCP_LEARNING_RATE_COARSE;  /* Learning rate */
    bridge->frame_priors[frame] += lr * evidence_strength * (1.0f - bridge->frame_priors[frame]);

    /* Renormalize */
    float sum = 0.0f;
    for (int i = 0; i < LING_STATS_MAX_REF_FRAMES; i++) {
        sum += bridge->frame_priors[i];
    }
    if (sum > 0.0f) {
        for (int i = 0; i < LING_STATS_MAX_REF_FRAMES; i++) {
            bridge->frame_priors[i] /= sum;
        }
    }

    return LING_STATS_ERR_OK;
}

int ling_stats_bayes_get_priors(
    const ling_stats_bridge_t* bridge,
    float* priors,
    uint32_t num_frames
) {
    if (!is_valid_bridge(bridge) || !priors) {
        return LING_STATS_ERR_NULL;
    }

    uint32_t n = (num_frames < LING_STATS_MAX_REF_FRAMES) ? num_frames : LING_STATS_MAX_REF_FRAMES;
    memcpy(priors, bridge->frame_priors, n * sizeof(float));

    return LING_STATS_ERR_OK;
}

/* ============================================================================
 * INFORMATION THEORY API IMPLEMENTATION
 * ============================================================================ */

int ling_stats_phonological_similarity(
    ling_stats_bridge_t* bridge,
    uint32_t phoneme_a,
    uint32_t phoneme_b,
    phonological_similarity_t* result
) {
    if (!is_valid_bridge(bridge) || !result) {
        return LING_STATS_ERR_NULL;
    }

    memset(result, 0, sizeof(*result));

    phoneme_model_t* model = &bridge->phoneme_model;
    if (!model->feature_dist) {
        return LING_STATS_ERR_INFO_FAIL;
    }

    if (phoneme_a >= model->num_phonemes || phoneme_b >= model->num_phonemes) {
        return LING_STATS_ERR_INVALID_STATE;
    }

    uint64_t start_us = get_time_us();

    /* Get feature distributions for each phoneme */
    float* dist_a = &model->feature_dist[phoneme_a * model->num_features];
    float* dist_b = &model->feature_dist[phoneme_b * model->num_features];

    /* Compute individual entropies */
    float h_a = 0.0f, h_b = 0.0f;
    for (uint32_t f = 0; f < model->num_features; f++) {
        if (dist_a[f] > 0.0f) h_a -= dist_a[f] * log2f(dist_a[f]);
        if (dist_b[f] > 0.0f) h_b -= dist_b[f] * log2f(dist_b[f]);
    }

    /* Compute joint entropy (assuming independence for simplicity) */
    float h_ab = h_a + h_b;

    /* Mutual information: I(A;B) = H(A) + H(B) - H(A,B) */
    float mi = h_a + h_b - h_ab;
    if (mi < 0.0f) mi = 0.0f;

    /* Normalized MI */
    float max_h = (h_a > h_b) ? h_a : h_b;
    float nmi = (max_h > 0.0f) ? (mi / max_h) : 0.0f;

    /* Similarity = 1 - normalized distance */
    /* Using feature overlap as a proxy */
    float overlap = 0.0f;
    for (uint32_t f = 0; f < model->num_features; f++) {
        float min_val = (dist_a[f] < dist_b[f]) ? dist_a[f] : dist_b[f];
        overlap += min_val;
    }
    float similarity = overlap;

    /* Confusability based on similarity */
    float confusability = similarity * 0.5f;  /* Scaled to reasonable range */

    result->entropy_a = h_a;
    result->entropy_b = h_b;
    result->joint_entropy = h_ab;
    result->mutual_information = mi;
    result->normalized_mi = nmi;
    result->similarity = similarity;
    result->confusability = confusability;

    bridge->stats.similarity_computations++;
    bridge->stats.avg_latency_us = bridge->stats.avg_latency_us * 0.99f +
                                    (get_time_us() - start_us) * 0.01f;

    return LING_STATS_ERR_OK;
}

int ling_stats_entropy(
    ling_stats_bridge_t* bridge,
    const float* distribution,
    uint32_t size,
    float* entropy
) {
    if (!is_valid_bridge(bridge) || !distribution || !entropy) {
        return LING_STATS_ERR_NULL;
    }

    float h = 0.0f;
    for (uint32_t i = 0; i < size; i++) {
        if (distribution[i] > 0.0f) {
            h -= distribution[i] * log2f(distribution[i]);
        }
    }

    *entropy = h;
    return LING_STATS_ERR_OK;
}

int ling_stats_kl_divergence(
    ling_stats_bridge_t* bridge,
    const float* p,
    const float* q,
    uint32_t size,
    float* kl_div
) {
    if (!is_valid_bridge(bridge) || !p || !q || !kl_div) {
        return LING_STATS_ERR_NULL;
    }

    float kl = 0.0f;
    for (uint32_t i = 0; i < size; i++) {
        if (p[i] > 0.0f && q[i] > 0.0f) {
            kl += p[i] * log2f(p[i] / q[i]);
        } else if (p[i] > 0.0f && q[i] <= 0.0f) {
            /* Undefined - return large value */
            *kl_div = FLT_MAX;
            return LING_STATS_ERR_OK;
        }
    }

    *kl_div = kl;
    return LING_STATS_ERR_OK;
}

/* ============================================================================
 * MESH HANDLER INTERFACE IMPLEMENTATION
 * ============================================================================ */

int ling_stats_mesh_process(
    void* ctx,
    const linguistics_request_t* request,
    linguistics_belief_t* belief
) {
    ling_stats_bridge_t* bridge = (ling_stats_bridge_t*)ctx;
    if (!is_valid_bridge(bridge) || !request || !belief) {
        return LING_STATS_ERR_NULL;
    }

    memset(belief, 0, sizeof(*belief));

    /* Process based on request type */
    if (request->type == LING_REQUEST_PARSE_NUMBER) {
        /* HMM-based processing */
        belief->certainty = 0.7f;  /* Default for number processing */
        belief->precision = bridge->current_precision;
    } else if (request->type == LING_REQUEST_PARSE_SPATIAL) {
        /* Bayesian reference frame processing */
        ref_frame_selection_result_t result;
        int ret = ling_stats_bayes_select_frame(bridge, SPATIAL_PREP_NEAR, NULL, 0, &result);
        if (ret == LING_STATS_ERR_OK) {
            belief->certainty = result.confidence;
            belief->precision = result.precision;
        }
    } else {
        belief->certainty = 0.5f;
        belief->precision = LING_STATS_DEFAULT_PRECISION;
    }

    belief->belief_vector[0] = belief->certainty;
    belief->vector_dim = 1;

    bridge->stats.mesh_contributions++;

    return LING_STATS_ERR_OK;
}

int ling_stats_mesh_update(
    void* ctx,
    const linguistics_belief_t* neighbors,
    uint32_t count,
    linguistics_belief_t* updated
) {
    ling_stats_bridge_t* bridge = (ling_stats_bridge_t*)ctx;
    if (!is_valid_bridge(bridge) || !neighbors || !updated) {
        return LING_STATS_ERR_NULL;
    }

    *updated = bridge->current_belief;

    float lr = bridge->config.mesh_learning_rate;
    for (uint32_t i = 0; i < count; i++) {
        float error = neighbors[i].certainty - updated->certainty;
        float weight = neighbors[i].precision * lr;
        updated->certainty += weight * error;

        for (uint32_t d = 0; d < updated->vector_dim && d < neighbors[i].vector_dim; d++) {
            float vec_error = neighbors[i].belief_vector[d] - updated->belief_vector[d];
            updated->belief_vector[d] += weight * vec_error;
        }
    }

    if (updated->certainty < 0.0f) updated->certainty = 0.0f;
    if (updated->certainty > 1.0f) updated->certainty = 1.0f;

    bridge->current_belief = *updated;
    bridge->stats.mesh_updates++;

    return LING_STATS_ERR_OK;
}

float ling_stats_mesh_get_precision(void* ctx) {
    ling_stats_bridge_t* bridge = (ling_stats_bridge_t*)ctx;
    if (!is_valid_bridge(bridge)) {
        return 0.01f;
    }
    return bridge->current_precision;
}

int ling_stats_get_mesh_handler(
    ling_stats_bridge_t* bridge,
    linguistics_mesh_handler_t* handler
) {
    if (!is_valid_bridge(bridge) || !handler) {
        return LING_STATS_ERR_NULL;
    }

    handler->process = ling_stats_mesh_process;
    handler->update = ling_stats_mesh_update;
    handler->get_precision = ling_stats_mesh_get_precision;
    handler->ctx = bridge;

    return LING_STATS_ERR_OK;
}

/* ============================================================================
 * STATISTICS API IMPLEMENTATION
 * ============================================================================ */

int ling_stats_bridge_get_stats(
    const ling_stats_bridge_t* bridge,
    ling_stats_bridge_stats_t* stats
) {
    if (!is_valid_bridge(bridge) || !stats) {
        return LING_STATS_ERR_NULL;
    }

    *stats = bridge->stats;
    return LING_STATS_ERR_OK;
}

void ling_stats_bridge_reset_stats(ling_stats_bridge_t* bridge) {
    if (!is_valid_bridge(bridge)) {
        return;
    }
    memset(&bridge->stats, 0, sizeof(bridge->stats));
}

const char* ling_stats_bridge_get_last_error(void) {
    return s_last_error;
}
