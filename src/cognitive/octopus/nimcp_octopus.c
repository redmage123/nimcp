/**
 * @file nimcp_octopus.c
 * @brief Octopus cognitive module — distributed peripheral cognition.
 *
 * See include/cognitive/octopus/nimcp_octopus.h for API + rationale.
 */
#include "cognitive/octopus/nimcp_octopus.h"

/* NIMCP infra: memory, logging, exceptions, math helpers. No raw
 * malloc/free or pthreads — all allocation flows through nimcp_memory. */
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/math/nimcp_math_helpers.h"

/* Phase 3a integrations: pink noise (biologically-plausible exploration),
 * fractal DFA (arm health metric), Shannon entropy (arm diversity). */
#include "plasticity/noise/nimcp_pink_noise.h"
#include "cognitive/memory/core/nimcp_fractal.h"

/* Phase 4h: per-arm LNN cells for continuous-time temporal integration. */
#include "lnn/nimcp_lnn_network.h"
/* Phase 4j: gradient flow on arm LNNs. */
#include "lnn/nimcp_lnn_training.h"
/* Phase 4l: natural gradient on arm latents. */
#include "physics/geometry/nimcp_information_geometry.h"
/* Phase 4m: JEPA latent-space predictive coding. */
#include "cognitive/jepa/nimcp_jepa_predictor.h"
#include "cognitive/jepa/nimcp_jepa_latent.h"
#include "utils/tensor/nimcp_tensor.h"

#include <math.h>
#include <string.h>

/*============================================================================
 * Private struct
 *==========================================================================*/

struct octopus_system_s {
    uint32_t       n_arms;
    octopus_arm_t* arms;
    octopus_stats_t stats;

    /* Latest aggregated latent — kept so repeated reads don't re-aggregate. */
    float          last_aggregated[OCTOPUS_ARM_DIM];
    float          last_coherence;

    /* Integration hooks. */
    octopus_ethics_hook_t ethics_hook;   void* ethics_user;
    octopus_swarm_hook_t  swarm_hook;    void* swarm_user;
    octopus_world_hook_t  world_hook;    void* world_user;
    octopus_fep_hook_t    fep_hook;      void* fep_user;
    octopus_bio_hook_t    bio_hook;      void* bio_user;
    octopus_immune_hook_t immune_hook;   void* immune_user;

    /* Thresholds. */
    float    swarm_delegation_threshold; /**< arm.confidence > this → delegate */
    float    low_coherence_threshold;    /**< coherence < this → broadcast     */

    /* Phase 3a: shared pink-noise generator used to inject biologically
     * plausible 1/f stochastic exploration into arm latents. Shared across
     * arms (single temporal noise source) since biological neuromod noise
     * is a whole-system signal, not per-arm. Lazy-created on first use;
     * if the noise module is unavailable, arms just skip the bias. */
    pink_noise_generator_t pink_gen;

    /* DFA cadence: recompute fractal_dfa on each arm's history at most
     * once per N explorations, since DFA is O(N * num_scales). */
    uint32_t dfa_update_every;
    uint32_t dfa_update_counter;

    /* Phase 4h: per-arm LNN cells. Each arm has its own small NCP network
     * that replaces the 0.7*slice + 0.3*history_mean transform with proper
     * continuous-time dynamics + learnable τ. Kept as void* to avoid
     * leaking lnn_network_t through the public header. NULL-terminated
     * or sized by n_arms — absence (NULL entries) means that arm falls
     * back to the legacy transform. Shared 64-dim input/output tensors
     * are reused across arms within a single explore() call. */
    void**  arm_lnns;       /* lnn_network_t*[n_arms] */
    void*   lnn_input;      /* nimcp_tensor_t* (1D, 64 floats) */
    void*   lnn_output;     /* nimcp_tensor_t* (1D, 64 floats) */
    float   lnn_dt_ms;      /* integration step in ms */

    /* Phase 4j: gradient flow on arm LNNs. Per-arm trainers + a shared
     * [1, OCTOPUS_ARM_DIM] input/target tensor pair for single-step
     * training. Two slice ring-buffer slots per arm (prev and cur) so
     * octopus_train_step can form (prev → cur) predictive-coding pairs
     * without requiring the caller to thread current-slice state through. */
    void**  arm_trainers;     /* lnn_training_ctx_t*[n_arms] */
    void*   lnn_train_input;  /* nimcp_tensor_t* (2D, [1, OCTOPUS_ARM_DIM]) */
    void*   lnn_train_target; /* nimcp_tensor_t* (2D, [1, OCTOPUS_ARM_DIM]) */
    float (*arm_prev_slices)[OCTOPUS_ARM_DIM]; /* [n_arms][64] */
    float (*arm_cur_slices)[OCTOPUS_ARM_DIM];  /* [n_arms][64] */
    bool*   arm_has_prev;     /* bool[n_arms] — prev slot populated */
    bool*   arm_has_cur;      /* bool[n_arms] — cur slot populated */

    /* Phase 4l: per-arm natural gradient preconditioner on arm latents.
     * Fisher is updated lazily inside nimcp_natural_grad_step from the
     * gradient signal we pass (the latent itself). This is latent-space
     * regularization under an information-geometric metric; it is NOT
     * weight-space NG replacing Adam (which would require bypassing
     * lnn_training_step's internal optimizer — deferred). */
    void**  arm_nat_grads;    /* nimcp_natural_gradient_t[n_arms] */

    /* Phase 4m: JEPA latent-space predictive coding. Shared predictor
     * learns arm-embedding transitions: context = arm latent at tick N-1,
     * target = arm latent at tick N. Trains in embedding space, so even
     * though arms still Adam-train on raw-slice MSE (Phase 4j), the JEPA
     * predictor learns the semantic/latent dynamics on top.
     *
     * Two slice-buffer analogs for LATENTS (not raw slices): arm_prev_
     * latents holds last tick's arm->latent; arm_cur_latents holds this
     * tick's. Shift runs BEFORE _arm_process_slice each tick — same
     * pattern as arm_prev_slices / arm_cur_slices in Phase 4j. */
    void*   jepa_predictor;   /* jepa_predictor_t* */
    void*   jepa_ctx_latent;  /* jepa_latent_t* reused buffer */
    void*   jepa_tgt_latent;  /* jepa_latent_t* reused buffer */
    float (*arm_prev_latents)[OCTOPUS_ARM_DIM]; /* [n_arms][64] */
    float (*arm_cur_latents)[OCTOPUS_ARM_DIM];  /* [n_arms][64] */
    bool*   arm_has_prev_latent;  /* [n_arms] */
    bool*   arm_has_cur_latent;   /* [n_arms] */
};

/*============================================================================
 * Small helpers — all scalar math routes through utils/math/nimcp_math_helpers
 * (nimcp_clampf, nimcp_clamp01, nimcp_safe_expf, etc.). Do not re-define.
 *==========================================================================*/

/* Tanh-activated per-element transform on an input slice → latent.
 * This is the arm's local "processing" — a cheap nonlinearity plus
 * a contribution from the arm's recent history. Full ML weights aren't
 * the point; this is cheap distributed-reflex compute, biology-analogous.
 *
 * Phase 3a: injects a single pink-noise sample into the pre-activation
 * (before tanh) so arms explore in a 1/f correlated way. The sample is
 * shared across dims of one arm for one step (simulates a single neuro-
 * modulator affecting the whole arm), then regenerated per exploration.
 * Shannon entropy of the softmaxed latent is computed at the end as a
 * principled info-theoretic diversity measure. */
static void _arm_process_slice(octopus_arm_t* arm, const float* slice,
                                pink_noise_generator_t pink_gen,
                                octopus_stats_t* stats,
                                /* Phase 4h: optional per-arm LNN network
                                 * + shared input/output tensors + dt.
                                 * NULL lnn → fall back to legacy transform. */
                                lnn_network_t* lnn,
                                nimcp_tensor_t* lnn_input,
                                nimcp_tensor_t* lnn_output,
                                float lnn_dt_ms) {
    /* Store new input in history ring. */
    memcpy(arm->recent_inputs[arm->history_head], slice,
           sizeof(float) * OCTOPUS_ARM_DIM);
    arm->history_head = (arm->history_head + 1) % OCTOPUS_ARM_HISTORY;

    /* Pink-noise bias — one scalar sample for this arm this step. */
    float pn_bias = 0.0f;
    if (pink_gen && pink_noise_generate_sample(pink_gen, &pn_bias)) {
        if (stats) stats->n_pink_noise_injections++;
    }

    /* Phase 4h path: run the arm's LNN cell over the slice+bias. LNN owns
     * its own hidden state across calls (continuous-time dynamics with
     * learnable τ), so the arm "remembers" without us computing a history
     * mean. Pink-noise bias is added to the input pre-forward to keep the
     * biological exploration signal. On any step-failure or unavailable
     * LNN, fall through to the legacy 0.7/0.3 transform. */
    bool lnn_path_ok = false;
    if (lnn && lnn_input && lnn_output) {
        float* in_data = (float*)nimcp_tensor_data(lnn_input);
        if (in_data) {
            for (uint32_t d = 0; d < OCTOPUS_ARM_DIM; d++) {
                in_data[d] = slice[d] + pn_bias;
            }
            if (lnn_network_forward_step(lnn, lnn_input,
                                         lnn_output, lnn_dt_ms) == 0) {
                const float* out_data =
                    (const float*)nimcp_tensor_data_const(lnn_output);
                if (out_data) {
                    for (uint32_t d = 0; d < OCTOPUS_ARM_DIM; d++) {
                        /* LNN outputs can be unbounded; tanh-squash so the
                         * downstream confidence/variance math stays well-
                         * scaled like the legacy path. */
                        arm->latent[d] = tanhf(out_data[d]);
                    }
                    if (stats) stats->n_lnn_steps++;
                    lnn_path_ok = true;
                }
            }
        }
    }
    if (!lnn_path_ok) {
        /* Legacy transform: 70% current slice, 30% smoothed history mean,
         * + pink-noise bias. Still tanh-squashed into [-1, 1]. */
        float history_mean[OCTOPUS_ARM_DIM] = {0};
        for (uint32_t h = 0; h < OCTOPUS_ARM_HISTORY; h++) {
            for (uint32_t d = 0; d < OCTOPUS_ARM_DIM; d++) {
                history_mean[d] += arm->recent_inputs[h][d];
            }
        }
        for (uint32_t d = 0; d < OCTOPUS_ARM_DIM; d++) {
            history_mean[d] /= (float)OCTOPUS_ARM_HISTORY;
            float v = 0.7f * slice[d] + 0.3f * history_mean[d] + pn_bias;
            arm->latent[d] = tanhf(v);
        }
    }

    /* Confidence = L1 norm of latent, normalized. Arms whose latent is
     * all zeros or tiny have low confidence — literally "no opinion". */
    float l1 = 0.0f;
    for (uint32_t d = 0; d < OCTOPUS_ARM_DIM; d++) l1 += fabsf(arm->latent[d]);
    arm->confidence = nimcp_clampf(l1 / (float)OCTOPUS_ARM_DIM, 0.0f, 1.0f);

    /* Latent variance — how much this arm's latent deviates from the
     * mean of the full latent. High variance = arm is in a distinctive
     * local regime (exploration signal). */
    float mean = 0.0f;
    for (uint32_t d = 0; d < OCTOPUS_ARM_DIM; d++) mean += arm->latent[d];
    mean /= (float)OCTOPUS_ARM_DIM;
    float var = 0.0f;
    for (uint32_t d = 0; d < OCTOPUS_ARM_DIM; d++) {
        float diff = arm->latent[d] - mean;
        var += diff * diff;
    }
    arm->latent_variance = var / (float)OCTOPUS_ARM_DIM;

    /* Broadcast state — confidence × (1 + variance), squashed to [0,1].
     * An arm that's both confident AND distinctive broadcasts loudly. */
    arm->broadcast_state = nimcp_clampf(arm->confidence *
                                    (1.0f + arm->latent_variance), 0.0f, 1.0f);

    /* Chemosensory-tactile fusion: the first 32 elements of the slice
     * double as chemical fingerprint. Copy them directly so downstream
     * modules can read arm->chemo_id as a semantic-tactile sense. */
    for (uint32_t d = 0; d < 32; d++) arm->chemo_id[d] = slice[d];

    /* Phase 3a: Shannon entropy over the arm's softmaxed latent. Gives
     * an info-theoretic "certainty" measure complementing latent_variance.
     * Values in [0, log2(N)] — normalized to [0, 1] for downstream use. */
    float probs[OCTOPUS_ARM_DIM];
    float max_v = arm->latent[0];
    for (uint32_t d = 1; d < OCTOPUS_ARM_DIM; d++) {
        if (arm->latent[d] > max_v) max_v = arm->latent[d];
    }
    float sum_exp = 0.0f;
    for (uint32_t d = 0; d < OCTOPUS_ARM_DIM; d++) {
        /* Subtract max for numerical stability. */
        probs[d] = expf(arm->latent[d] - max_v);
        sum_exp += probs[d];
    }
    if (sum_exp > 1e-8f) {
        for (uint32_t d = 0; d < OCTOPUS_ARM_DIM; d++) probs[d] /= sum_exp;
        float H = nimcp_entropy(probs, OCTOPUS_ARM_DIM);
        /* Normalize by log(N) = ln(64) ≈ 4.158 so arm->shannon_entropy is in [0,1]. */
        arm->shannon_entropy = nimcp_clamp01(H / 4.158883f);
    } else {
        arm->shannon_entropy = 0.0f;
    }
}

/*============================================================================
 * Lifecycle
 *==========================================================================*/

octopus_system_t* octopus_create(uint32_t n_arms) {
    if (n_arms == 0) n_arms = OCTOPUS_DEFAULT_N_ARMS;
    if (n_arms < 2)  n_arms = 2;
    if (n_arms > 16) n_arms = 16;

    octopus_system_t* ctx = (octopus_system_t*)nimcp_calloc(
        1, sizeof(octopus_system_t));
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
                              "octopus_create: ctx alloc failed");
        return NULL;
    }
    ctx->n_arms = n_arms;
    ctx->arms = (octopus_arm_t*)nimcp_calloc(n_arms, sizeof(octopus_arm_t));
    if (!ctx->arms) {
        nimcp_free(ctx);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
                              "octopus_create: arms alloc failed");
        return NULL;
    }
    for (uint32_t i = 0; i < n_arms; i++) {
        ctx->arms[i].id = i;
    }
    ctx->stats.n_arms = n_arms;
    ctx->swarm_delegation_threshold = 0.75f;
    ctx->low_coherence_threshold    = 0.30f;
    ctx->dfa_update_every = 20;  /* once per 20 explorations — cheap cadence */

    /* Create the shared pink-noise generator. 1/f spectrum (alpha=1),
     * small amplitude so it biases exploration without overwhelming the
     * deterministic signal. sample_rate ≥ 2 × max_frequency (Nyquist) —
     * previously set sample_rate=max_frequency=100 which silently failed
     * validation. Using 250 Hz keeps a comfortable margin. Non-fatal if
     * creation still fails. */
    pink_noise_config_t pink_cfg = {0};
    pink_cfg.alpha          = 1.0f;
    pink_cfg.amplitude      = 0.05f;
    pink_cfg.min_frequency  = 0.1f;
    pink_cfg.max_frequency  = 100.0f;
    pink_cfg.sample_rate    = 250.0f;  /* > 2 × max_frequency per Nyquist */
    pink_cfg.method         = PINK_NOISE_VOSS;  /* fast streaming method */
    pink_cfg.seed           = 0;  /* time-based */
    ctx->pink_gen = pink_noise_create(&pink_cfg);
    if (!ctx->pink_gen) {
        NIMCP_LOGGING_WARN("octopus_create: pink noise generator unavailable; "
                           "arms will run without 1/f exploration bias");
    }

    /* Phase 4h: per-arm LNN cells. One small NCP network per arm gives
     * each arm independent continuous-time dynamics with learnable τ.
     * NCP shape 64→16→16→64: small enough that 16 arms × NCP ~= 64KB,
     * big enough to exhibit real LTC dynamics. Seed per arm differs so
     * arms don't start identical — combined with pink noise, this
     * guarantees arm diversity at init. Graceful failure: if LNN setup
     * fails for any reason, arm_lnns stays NULL and _arm_process_slice
     * falls back to the legacy 0.7/0.3 transform. */
    ctx->lnn_dt_ms = 1.0f;
    ctx->arm_lnns = (void**)nimcp_calloc(n_arms, sizeof(void*));
    if (ctx->arm_lnns) {
        uint32_t created = 0;
        for (uint32_t i = 0; i < n_arms; i++) {
            lnn_network_t* net = lnn_network_create_ncp(
                OCTOPUS_ARM_DIM, 16, 16, OCTOPUS_ARM_DIM);
            if (!net) continue;
            if (lnn_network_init_weights(net, (uint64_t)(0xA17u + i)) != 0) {
                lnn_network_destroy(net);
                continue;
            }
            ctx->arm_lnns[i] = net;
            created++;
        }
        if (created > 0) {
            uint32_t dims[1] = { OCTOPUS_ARM_DIM };
            ctx->lnn_input  = nimcp_tensor_create(dims, 1, NIMCP_DTYPE_F32);
            ctx->lnn_output = nimcp_tensor_create(dims, 1, NIMCP_DTYPE_F32);
            if (!ctx->lnn_input || !ctx->lnn_output) {
                /* Tensor alloc failed — tear down LNNs, fall back. */
                if (ctx->lnn_input)  { nimcp_tensor_destroy(ctx->lnn_input);  ctx->lnn_input  = NULL; }
                if (ctx->lnn_output) { nimcp_tensor_destroy(ctx->lnn_output); ctx->lnn_output = NULL; }
                for (uint32_t i = 0; i < n_arms; i++) {
                    if (ctx->arm_lnns[i]) {
                        lnn_network_destroy((lnn_network_t*)ctx->arm_lnns[i]);
                        ctx->arm_lnns[i] = NULL;
                    }
                }
            } else {
                ctx->stats.lnn_enabled = true;
            }
        }
    }

    /* Phase 4j: gradient flow. Allocate per-arm trainers + slice buffers.
     * Only attempted when LNN is live; all allocations are non-fatal on
     * failure — absent structures simply mean training APIs are no-ops. */
    if (ctx->stats.lnn_enabled) {
        ctx->arm_prev_slices = (float (*)[OCTOPUS_ARM_DIM])nimcp_calloc(
            n_arms, sizeof(float) * OCTOPUS_ARM_DIM);
        ctx->arm_cur_slices  = (float (*)[OCTOPUS_ARM_DIM])nimcp_calloc(
            n_arms, sizeof(float) * OCTOPUS_ARM_DIM);
        ctx->arm_has_prev    = (bool*)nimcp_calloc(n_arms, sizeof(bool));
        ctx->arm_has_cur     = (bool*)nimcp_calloc(n_arms, sizeof(bool));
        ctx->arm_trainers    = (void**)nimcp_calloc(n_arms, sizeof(void*));

        if (ctx->arm_trainers) {
            lnn_training_config_t tcfg;
            lnn_training_config_default(&tcfg);
            tcfg.optimizer_type = NIMCP_OPTIMIZER_ADAM;
            tcfg.learning_rate  = 1e-3f;
            tcfg.loss_type      = NIMCP_LOSS_MSE;
            tcfg.lnn_train_mode = LNN_TRAIN_ADJOINT;
            tcfg.gradient_clip_norm = 1.0f;
            tcfg.diversity_loss_weight = 0.0f; /* per-arm specialization */

            uint32_t t_created = 0;
            for (uint32_t i = 0; i < n_arms; i++) {
                if (!ctx->arm_lnns[i]) continue;
                lnn_training_ctx_t* trn = lnn_training_create(
                    (lnn_network_t*)ctx->arm_lnns[i], &tcfg);
                if (trn) {
                    ctx->arm_trainers[i] = trn;
                    t_created++;
                }
            }
            if (t_created > 0) {
                /* Training tensors are (seq_len=1, n_features=OCTOPUS_ARM_DIM)
                 * 2-D so lnn_training_step's [seq_len, n_inputs] contract
                 * is satisfied with a single time step. */
                uint32_t t_dims[2] = { 1u, OCTOPUS_ARM_DIM };
                ctx->lnn_train_input  = nimcp_tensor_create(t_dims, 2, NIMCP_DTYPE_F32);
                ctx->lnn_train_target = nimcp_tensor_create(t_dims, 2, NIMCP_DTYPE_F32);
            }
        }
    }

    /* Phase 4l: per-arm natural gradient preconditioners on latents.
     * Allocated regardless of LNN status since NG operates on latent
     * vectors, not weights. Failure is non-fatal — absent preconditioner
     * just means octopus_ng_regularize() is a no-op. */
    ctx->arm_nat_grads = (void**)nimcp_calloc(n_arms, sizeof(void*));
    if (ctx->arm_nat_grads) {
        nimcp_natural_grad_config_t ng_cfg = nimcp_natural_grad_default_config();
        ng_cfg.learning_rate     = 1e-3f;
        ng_cfg.momentum          = 0.9f;
        ng_cfg.gradient_clip     = 1.0f;
        ng_cfg.use_preconditioner = true;
        uint32_t ng_created = 0;
        for (uint32_t i = 0; i < n_arms; i++) {
            nimcp_natural_gradient_t ng = nimcp_natural_grad_create(
                &ng_cfg, OCTOPUS_ARM_DIM);
            if (ng) {
                ctx->arm_nat_grads[i] = (void*)ng;
                ng_created++;
            }
        }
        if (ng_created == 0) {
            nimcp_free(ctx->arm_nat_grads);
            ctx->arm_nat_grads = NULL;
        }
    }

    /* Phase 4m: JEPA latent-space predictor + per-arm latent shift buffers.
     * Shared predictor (not per-arm) since the transition dynamics arms
     * learn are the same semantic family. Light MLP: 64→64 with a 64-dim
     * hidden layer. Non-fatal on failure — absence keeps jepa_enabled
     * false forever. */
    {
        jepa_predictor_config_t jcfg = {0};
        (void)jepa_predictor_default_config(&jcfg);
        jcfg.input_dim        = OCTOPUS_ARM_DIM;
        jcfg.output_dim       = OCTOPUS_ARM_DIM;
        jcfg.hidden_dim       = OCTOPUS_ARM_DIM;
        jcfg.num_layers       = 1;
        jcfg.learning_rate    = 1e-3f;
        jcfg.weight_decay     = 1e-5f;
        jcfg.dropout_rate     = 0.0f;
        jcfg.enable_layer_norm = true;
        jcfg.enable_fep       = false;

        jepa_predictor_t* jp = jepa_predictor_create(&jcfg);
        if (jp) {
            (void)jepa_predictor_set_training(jp, true);
            ctx->jepa_predictor   = (void*)jp;
            ctx->jepa_ctx_latent  = (void*)jepa_latent_create_dim(OCTOPUS_ARM_DIM);
            ctx->jepa_tgt_latent  = (void*)jepa_latent_create_dim(OCTOPUS_ARM_DIM);
            ctx->arm_prev_latents = (float (*)[OCTOPUS_ARM_DIM])nimcp_calloc(
                n_arms, sizeof(float) * OCTOPUS_ARM_DIM);
            ctx->arm_cur_latents  = (float (*)[OCTOPUS_ARM_DIM])nimcp_calloc(
                n_arms, sizeof(float) * OCTOPUS_ARM_DIM);
            ctx->arm_has_prev_latent = (bool*)nimcp_calloc(n_arms, sizeof(bool));
            ctx->arm_has_cur_latent  = (bool*)nimcp_calloc(n_arms, sizeof(bool));
            /* Any mid-sequence alloc failure → tear the whole thing down so
             * octopus_set_jepa_enabled can't promise a broken JEPA. */
            if (!ctx->jepa_ctx_latent || !ctx->jepa_tgt_latent ||
                !ctx->arm_prev_latents || !ctx->arm_cur_latents ||
                !ctx->arm_has_prev_latent || !ctx->arm_has_cur_latent) {
                if (ctx->jepa_ctx_latent) {
                    jepa_latent_destroy((jepa_latent_t*)ctx->jepa_ctx_latent);
                    ctx->jepa_ctx_latent = NULL;
                }
                if (ctx->jepa_tgt_latent) {
                    jepa_latent_destroy((jepa_latent_t*)ctx->jepa_tgt_latent);
                    ctx->jepa_tgt_latent = NULL;
                }
                if (ctx->arm_prev_latents) { nimcp_free(ctx->arm_prev_latents); ctx->arm_prev_latents = NULL; }
                if (ctx->arm_cur_latents)  { nimcp_free(ctx->arm_cur_latents);  ctx->arm_cur_latents  = NULL; }
                if (ctx->arm_has_prev_latent) { nimcp_free(ctx->arm_has_prev_latent); ctx->arm_has_prev_latent = NULL; }
                if (ctx->arm_has_cur_latent)  { nimcp_free(ctx->arm_has_cur_latent);  ctx->arm_has_cur_latent  = NULL; }
                jepa_predictor_destroy(jp);
                ctx->jepa_predictor = NULL;
            }
        }
    }

    NIMCP_LOGGING_INFO("octopus_create: %u arms initialized "
                       "(pink-noise=%s, DFA cadence=%u, LNN=%s, "
                       "trainers=%s, NG=%s, JEPA=%s)",
                       n_arms, ctx->pink_gen ? "live" : "off",
                       ctx->dfa_update_every,
                       ctx->stats.lnn_enabled ? "live" : "off",
                       (ctx->arm_trainers && ctx->lnn_train_input &&
                        ctx->lnn_train_target) ? "live" : "off",
                       ctx->arm_nat_grads ? "live" : "off",
                       ctx->jepa_predictor ? "live" : "off");
    NIMCP_LOGGING_DEBUG("octopus_create: thresholds swarm=%.2f low_coh=%.2f "
                        "lnn_dt_ms=%.3f",
                        ctx->swarm_delegation_threshold,
                        ctx->low_coherence_threshold,
                        ctx->lnn_dt_ms);
    return ctx;
}

void octopus_destroy(octopus_system_t* ctx) {
    if (!ctx) return;
    NIMCP_LOGGING_INFO("octopus_destroy: %u arms, %u explorations, "
                       "%u integrations, %u lnn_steps, %u jepa_steps, "
                       "%u ng_steps, %u ethics_vetoes, %u delegations",
                       ctx->n_arms,
                       ctx->stats.n_explorations,
                       ctx->stats.n_integrations,
                       ctx->stats.n_lnn_steps,
                       ctx->stats.n_jepa_steps,
                       ctx->stats.n_ng_steps,
                       ctx->stats.n_ethics_vetoes,
                       ctx->stats.n_swarm_delegations);
    if (ctx->pink_gen) {
        pink_noise_destroy(ctx->pink_gen);
        ctx->pink_gen = NULL;
    }
    /* Phase 4j trainers FIRST (reverse-create order). Current
     * lnn_training_destroy / lnn_gradient_ctx_destroy don't dereference
     * their held network pointer, so UAF is not possible today — but
     * reverse-order destroy is the robust pattern if that ever changes. */
    if (ctx->arm_trainers) {
        for (uint32_t i = 0; i < ctx->n_arms; i++) {
            if (ctx->arm_trainers[i]) {
                lnn_training_destroy((lnn_training_ctx_t*)ctx->arm_trainers[i]);
                ctx->arm_trainers[i] = NULL;
            }
        }
        nimcp_free(ctx->arm_trainers);
        ctx->arm_trainers = NULL;
    }
    if (ctx->lnn_train_input)  { nimcp_tensor_destroy(ctx->lnn_train_input);  ctx->lnn_train_input  = NULL; }
    if (ctx->lnn_train_target) { nimcp_tensor_destroy(ctx->lnn_train_target); ctx->lnn_train_target = NULL; }

    /* Phase 4h: LNN cells + shared tensors (after trainers that referenced them). */
    if (ctx->arm_lnns) {
        for (uint32_t i = 0; i < ctx->n_arms; i++) {
            if (ctx->arm_lnns[i]) {
                lnn_network_destroy((lnn_network_t*)ctx->arm_lnns[i]);
                ctx->arm_lnns[i] = NULL;
            }
        }
        nimcp_free(ctx->arm_lnns);
        ctx->arm_lnns = NULL;
    }
    if (ctx->lnn_input)  { nimcp_tensor_destroy(ctx->lnn_input);  ctx->lnn_input  = NULL; }
    if (ctx->lnn_output) { nimcp_tensor_destroy(ctx->lnn_output); ctx->lnn_output = NULL; }
    if (ctx->arm_prev_slices) { nimcp_free(ctx->arm_prev_slices); ctx->arm_prev_slices = NULL; }
    if (ctx->arm_cur_slices)  { nimcp_free(ctx->arm_cur_slices);  ctx->arm_cur_slices  = NULL; }
    if (ctx->arm_has_prev)    { nimcp_free(ctx->arm_has_prev);    ctx->arm_has_prev    = NULL; }
    if (ctx->arm_has_cur)     { nimcp_free(ctx->arm_has_cur);     ctx->arm_has_cur     = NULL; }

    /* Phase 4l: natural gradient preconditioners. */
    if (ctx->arm_nat_grads) {
        for (uint32_t i = 0; i < ctx->n_arms; i++) {
            if (ctx->arm_nat_grads[i]) {
                nimcp_natural_grad_destroy(
                    (nimcp_natural_gradient_t)ctx->arm_nat_grads[i]);
                ctx->arm_nat_grads[i] = NULL;
            }
        }
        nimcp_free(ctx->arm_nat_grads);
        ctx->arm_nat_grads = NULL;
    }

    /* Phase 4m: JEPA predictor + latents + latent buffers. */
    if (ctx->jepa_predictor) {
        jepa_predictor_destroy((jepa_predictor_t*)ctx->jepa_predictor);
        ctx->jepa_predictor = NULL;
    }
    if (ctx->jepa_ctx_latent) {
        jepa_latent_destroy((jepa_latent_t*)ctx->jepa_ctx_latent);
        ctx->jepa_ctx_latent = NULL;
    }
    if (ctx->jepa_tgt_latent) {
        jepa_latent_destroy((jepa_latent_t*)ctx->jepa_tgt_latent);
        ctx->jepa_tgt_latent = NULL;
    }
    if (ctx->arm_prev_latents) { nimcp_free(ctx->arm_prev_latents); ctx->arm_prev_latents = NULL; }
    if (ctx->arm_cur_latents)  { nimcp_free(ctx->arm_cur_latents);  ctx->arm_cur_latents  = NULL; }
    if (ctx->arm_has_prev_latent) { nimcp_free(ctx->arm_has_prev_latent); ctx->arm_has_prev_latent = NULL; }
    if (ctx->arm_has_cur_latent)  { nimcp_free(ctx->arm_has_cur_latent);  ctx->arm_has_cur_latent  = NULL; }

    nimcp_free(ctx->arms);
    nimcp_free(ctx);
}

/*============================================================================
 * Core operations
 *==========================================================================*/

int octopus_explore(octopus_system_t* ctx,
                     const float* input, uint32_t input_len) {
    if (!ctx || !input || input_len == 0) return -1;
    NIMCP_LOGGING_TRACE("octopus_explore: ctx=%p input_len=%u n_arms=%u "
                        "(pre: n_explorations=%u)",
                        (void*)ctx, input_len, ctx->n_arms,
                        ctx->stats.n_explorations);
    if (ctx->immune_hook) ctx->immune_hook(ctx->immune_user);

    float slice[OCTOPUS_ARM_DIM];
    for (uint32_t a = 0; a < ctx->n_arms; a++) {
        /* Per-arm slicing: strided windows with wraparound. This is
         * deliberately simple — each arm sees a different "part" of the
         * input and builds its own local opinion. */
        uint32_t start = (a * (OCTOPUS_ARM_DIM / 2)) % input_len;
        for (uint32_t d = 0; d < OCTOPUS_ARM_DIM; d++) {
            slice[d] = input[(start + d) % input_len];
        }
        /* Phase 4m: shift latent buffers BEFORE the arm overwrites arm->latent.
         * On each tick: cur_latent[a] (last tick's latent) becomes
         * prev_latent[a]; after _arm_process_slice runs the new arm->latent
         * is captured into cur_latent[a]. Train step consumes the (prev→cur)
         * pair the same way Phase 4j consumes (prev_slice → cur_slice). */
        if (ctx->arm_cur_latents && ctx->arm_prev_latents &&
            ctx->arm_has_cur_latent && ctx->arm_has_prev_latent) {
            if (ctx->arm_has_cur_latent[a]) {
                memcpy(ctx->arm_prev_latents[a], ctx->arm_cur_latents[a],
                       sizeof(float) * OCTOPUS_ARM_DIM);
                ctx->arm_has_prev_latent[a] = true;
            }
        }

        lnn_network_t* arm_lnn = (ctx->arm_lnns && ctx->stats.lnn_enabled)
            ? (lnn_network_t*)ctx->arm_lnns[a] : NULL;
        _arm_process_slice(&ctx->arms[a], slice, ctx->pink_gen, &ctx->stats,
                           arm_lnn,
                           (nimcp_tensor_t*)ctx->lnn_input,
                           (nimcp_tensor_t*)ctx->lnn_output,
                           ctx->lnn_dt_ms);
        ctx->arms[a].vetoed = false;

        /* Phase 4m: capture the fresh arm->latent into cur_latent[a]. */
        if (ctx->arm_cur_latents && ctx->arm_has_cur_latent) {
            memcpy(ctx->arm_cur_latents[a], ctx->arms[a].latent,
                   sizeof(float) * OCTOPUS_ARM_DIM);
            ctx->arm_has_cur_latent[a] = true;
        }

        /* Phase 4j: maintain two slice slots per arm (prev and cur) so
         * octopus_train_step can form (prev → cur) predictive-coding pairs
         * without requiring caller state. On each explore, shift the old
         * `cur` slice into `prev` (if present) and capture the new slice
         * into `cur`. This is cheap (128 bytes per arm per step). */
        if (ctx->arm_cur_slices && ctx->arm_prev_slices &&
            ctx->arm_has_cur && ctx->arm_has_prev) {
            if (ctx->arm_has_cur[a]) {
                memcpy(ctx->arm_prev_slices[a], ctx->arm_cur_slices[a],
                       sizeof(float) * OCTOPUS_ARM_DIM);
                ctx->arm_has_prev[a] = true;
            }
            memcpy(ctx->arm_cur_slices[a], slice,
                   sizeof(float) * OCTOPUS_ARM_DIM);
            ctx->arm_has_cur[a] = true;
        }

        /* Ethics gate — arm decisions are checked per-arm (biology: each
         * arm has local reflex-level "don't touch that" reactions). */
        if (ctx->ethics_hook) {
            if (!ctx->ethics_hook(&ctx->arms[a], ctx->ethics_user)) {
                ctx->arms[a].vetoed = true;
                ctx->stats.n_ethics_vetoes++;
                NIMCP_LOGGING_DEBUG("octopus_explore: arm %u vetoed by ethics "
                                    "(confidence=%.3f)", a, ctx->arms[a].confidence);
                if (ctx->bio_hook) {
                    ctx->bio_hook("octopus_arm_vetoed",
                                  (float)a, ctx->bio_user);
                }
            }
        }

        /* Swarm delegation — highly confident arm findings get pushed to
         * an edge agent. Lower-confidence arm opinions stay local. */
        if (ctx->swarm_hook &&
            !ctx->arms[a].vetoed &&
            ctx->arms[a].confidence > ctx->swarm_delegation_threshold) {
            ctx->swarm_hook(&ctx->arms[a], ctx->swarm_user);
            ctx->stats.n_swarm_delegations++;
            NIMCP_LOGGING_DEBUG("octopus_explore: arm %u delegated "
                                "(confidence=%.3f > threshold=%.3f)",
                                a, ctx->arms[a].confidence,
                                ctx->swarm_delegation_threshold);
        }
        NIMCP_LOGGING_TRACE("octopus_explore: arm=%u conf=%.3f var=%.3f "
                            "ent=%.3f bc=%.3f vetoed=%d",
                            a, ctx->arms[a].confidence,
                            ctx->arms[a].latent_variance,
                            ctx->arms[a].shannon_entropy,
                            ctx->arms[a].broadcast_state,
                            (int)ctx->arms[a].vetoed);
    }

    ctx->stats.n_explorations++;

    /* Phase 3a: periodically run DFA on each arm's history. DFA expects a
     * 1-D temporal series; we use L2 norm per history slot → OCTOPUS_ARM_
     * HISTORY samples (64 as of Phase 3b, which matches FRACTAL_MIN_SAMPLES
     * so the call actually produces a real exponent instead of failing
     * silently as it did in Phase 3a). */
    ctx->dfa_update_counter++;
    if (ctx->dfa_update_counter >= ctx->dfa_update_every) {
        ctx->dfa_update_counter = 0;
        for (uint32_t a = 0; a < ctx->n_arms; a++) {
            octopus_arm_t* arm = &ctx->arms[a];
            /* L2 norm per history time-slot = one sample per time step.
             * This is the *temporal* signal DFA needs — not a flatten
             * of multi-dim data, which would measure the wrong thing. */
            float signal[OCTOPUS_ARM_HISTORY];
            for (uint32_t h = 0; h < OCTOPUS_ARM_HISTORY; h++) {
                float sq = 0.0f;
                for (uint32_t d = 0; d < OCTOPUS_ARM_DIM; d++) {
                    sq += arm->recent_inputs[h][d] * arm->recent_inputs[h][d];
                }
                signal[h] = sqrtf(sq);
            }
            fractal_result_t res = {0};
            if (fractal_dfa(signal, OCTOPUS_ARM_HISTORY, NULL, &res) == 0) {
                arm->dfa_exponent = res.dfa_exponent;
                ctx->stats.n_dfa_computations++;
                NIMCP_LOGGING_TRACE("octopus_explore: arm %u DFA=%.3f",
                                    a, arm->dfa_exponent);
            }
            /* Else: call fails because OCTOPUS_ARM_HISTORY < FRACTAL_MIN_SAMPLES;
             * dfa_exponent keeps its previous value (0.0 on fresh arm). */
        }
    }
    return 0;
}

int octopus_integrate(octopus_system_t* ctx,
                       float* out, float* coherence) {
    if (!ctx || !out) return -1;

    /* Confidence-weighted mean of non-vetoed arms. */
    float weight_sum = 0.0f;
    float agg[OCTOPUS_ARM_DIM] = {0};
    uint32_t contributing = 0;
    float conf_sum = 0.0f;
    float var_sum = 0.0f;
    for (uint32_t a = 0; a < ctx->n_arms; a++) {
        if (ctx->arms[a].vetoed) continue;
        float w = ctx->arms[a].confidence;
        for (uint32_t d = 0; d < OCTOPUS_ARM_DIM; d++) {
            agg[d] += w * ctx->arms[a].latent[d];
        }
        weight_sum += w;
        conf_sum += ctx->arms[a].confidence;
        var_sum += ctx->arms[a].latent_variance;
        contributing++;
    }
    if (contributing == 0 || weight_sum < 1e-8f) {
        /* All arms vetoed or silent — zero output, low coherence. */
        NIMCP_LOGGING_DEBUG("octopus_integrate: no contributing arms "
                            "(contributing=%u, weight_sum=%.6f)",
                            contributing, weight_sum);
        memset(out, 0, sizeof(float) * OCTOPUS_ARM_DIM);
        if (coherence) *coherence = 0.0f;
        ctx->last_coherence = 0.0f;
        return -1;
    }
    for (uint32_t d = 0; d < OCTOPUS_ARM_DIM; d++) {
        agg[d] /= weight_sum;
        out[d] = agg[d];
    }
    memcpy(ctx->last_aggregated, out, sizeof(ctx->last_aggregated));

    /* Central coherence: 1 - mean-pairwise divergence of arm latents.
     * Cheap proxy: 1 - variance of element-wise means across arms. */
    float per_elem_var = 0.0f;
    for (uint32_t d = 0; d < OCTOPUS_ARM_DIM; d++) {
        float mean = 0.0f; uint32_t n = 0;
        for (uint32_t a = 0; a < ctx->n_arms; a++) {
            if (ctx->arms[a].vetoed) continue;
            mean += ctx->arms[a].latent[d]; n++;
        }
        if (n == 0) continue;
        mean /= (float)n;
        float v = 0.0f;
        for (uint32_t a = 0; a < ctx->n_arms; a++) {
            if (ctx->arms[a].vetoed) continue;
            float diff = ctx->arms[a].latent[d] - mean;
            v += diff * diff;
        }
        v /= (float)n;
        per_elem_var += v;
    }
    per_elem_var /= (float)OCTOPUS_ARM_DIM;
    float coh = nimcp_clampf(1.0f - per_elem_var, 0.0f, 1.0f);
    ctx->last_coherence = coh;
    ctx->stats.central_coherence = coh;  /* publish before hooks so hooks can read it */
    if (coherence) *coherence = coh;

    /* Fire downstream hooks. */
    if (ctx->world_hook) {
        ctx->world_hook(out, OCTOPUS_ARM_DIM, ctx->world_user);
        ctx->stats.n_world_model_updates++;
    }
    if (ctx->fep_hook) {
        ctx->fep_hook(coh, ctx->fep_user);
    }
    if (ctx->bio_hook && coh < ctx->low_coherence_threshold) {
        ctx->bio_hook("octopus_low_coherence", coh, ctx->bio_user);
    }

    /* Update running stats. central_coherence is published earlier (before
     * hooks fire) so that hook implementations can read it during this same
     * integrate call — don't re-assign here. */
    ctx->stats.n_integrations++;
    ctx->stats.avg_arm_confidence = conf_sum / (float)contributing;
    ctx->stats.avg_arm_variance   = var_sum  / (float)contributing;

    /* Phase 3a aggregations. */
    float ent_sum = 0.0f, dfa_sum = 0.0f;
    uint32_t dfa_contrib = 0;
    for (uint32_t a = 0; a < ctx->n_arms; a++) {
        if (ctx->arms[a].vetoed) continue;
        ent_sum += ctx->arms[a].shannon_entropy;
        if (ctx->arms[a].dfa_exponent > 0.0f) {
            dfa_sum += ctx->arms[a].dfa_exponent;
            dfa_contrib++;
        }
    }
    ctx->stats.avg_arm_entropy = ent_sum / (float)contributing;
    ctx->stats.avg_arm_dfa     = dfa_contrib > 0
                               ? (dfa_sum / (float)dfa_contrib) : 0.0f;

    NIMCP_LOGGING_DEBUG("octopus_integrate: coherence=%.3f contributing=%u "
                        "avg_conf=%.3f avg_var=%.3f avg_ent=%.3f",
                        coh, contributing,
                        ctx->stats.avg_arm_confidence,
                        ctx->stats.avg_arm_variance,
                        ctx->stats.avg_arm_entropy);
    return 0;
}

void octopus_get_stats(const octopus_system_t* ctx, octopus_stats_t* out) {
    if (!ctx || !out) return;
    *out = ctx->stats;
}

/*============================================================================
 * Hook setters
 *==========================================================================*/

#define HOOK_SETTER(name, ftype)                                               \
    void octopus_set_##name##_hook(octopus_system_t* ctx,                      \
                                    ftype fn, void* user) {                    \
        if (!ctx) return;                                                      \
        ctx->name##_hook = fn;                                                 \
        ctx->name##_user = user;                                               \
    }

HOOK_SETTER(ethics, octopus_ethics_hook_t)
HOOK_SETTER(swarm,  octopus_swarm_hook_t)
HOOK_SETTER(world,  octopus_world_hook_t)
HOOK_SETTER(fep,    octopus_fep_hook_t)
HOOK_SETTER(bio,    octopus_bio_hook_t)
HOOK_SETTER(immune, octopus_immune_hook_t)

#undef HOOK_SETTER

/*============================================================================
 * Read-only accessors
 *==========================================================================*/

uint32_t octopus_get_n_arms(const octopus_system_t* ctx) {
    return ctx ? ctx->n_arms : 0;
}

const octopus_arm_t* octopus_get_arm(const octopus_system_t* ctx,
                                      uint32_t arm_id) {
    if (!ctx || arm_id >= ctx->n_arms) return NULL;
    return &ctx->arms[arm_id];
}

float octopus_get_broadcast_state(const octopus_system_t* ctx,
                                   uint32_t arm_id) {
    if (!ctx || arm_id >= ctx->n_arms) return 0.0f;
    return ctx->arms[arm_id].broadcast_state;
}

/*============================================================================
 * Phase 4j: gradient flow on arm LNNs.
 *==========================================================================*/

/* Forward decl: Phase 4m JEPA step fires additively from octopus_train_step. */
static uint32_t _octopus_jepa_train_all_arms(octopus_system_t* ctx,
                                              float* out_loss);

void octopus_set_training_enabled(octopus_system_t* ctx, bool enabled) {
    if (!ctx) return;
    bool prev = ctx->stats.lnn_training_enabled;
    ctx->stats.lnn_training_enabled = enabled;
    if (prev != enabled) {
        NIMCP_LOGGING_INFO("octopus: LNN training %s",
                           enabled ? "ENABLED" : "DISABLED");
    }
}

int octopus_train_step(octopus_system_t* ctx, float* loss_out) {
    if (!ctx) return -1;
    if (!ctx->stats.lnn_training_enabled) return -1;
    /* All six allocations must have succeeded for predictive-coding pairs
     * to be well-defined; partial-alloc states (one nimcp_calloc failed
     * while siblings succeeded) would otherwise segfault on access below. */
    if (!ctx->arm_trainers || !ctx->lnn_train_input || !ctx->lnn_train_target ||
        !ctx->arm_prev_slices || !ctx->arm_cur_slices ||
        !ctx->arm_has_prev || !ctx->arm_has_cur) {
        return -1;
    }

    float* in_data  = (float*)nimcp_tensor_data(
        (nimcp_tensor_t*)ctx->lnn_train_input);
    float* tgt_data = (float*)nimcp_tensor_data(
        (nimcp_tensor_t*)ctx->lnn_train_target);
    if (!in_data || !tgt_data) return -1;

    float total_loss   = 0.0f;
    uint32_t trained   = 0;
    for (uint32_t a = 0; a < ctx->n_arms; a++) {
        lnn_training_ctx_t* trn = (lnn_training_ctx_t*)ctx->arm_trainers[a];
        if (!trn) continue;
        if (!ctx->arm_has_prev[a] || !ctx->arm_has_cur[a]) continue;

        /* Pack (prev → cur) into the shared training tensors. */
        memcpy(in_data,  ctx->arm_prev_slices[a], sizeof(float) * OCTOPUS_ARM_DIM);
        memcpy(tgt_data, ctx->arm_cur_slices[a],  sizeof(float) * OCTOPUS_ARM_DIM);

        float step_loss = 0.0f;
        int rc = lnn_training_step(trn,
                                    (nimcp_tensor_t*)ctx->lnn_train_input,
                                    (nimcp_tensor_t*)ctx->lnn_train_target,
                                    &step_loss);
        if (rc == 0) {
            total_loss += step_loss;
            trained++;
        }
    }
    /* Phase 4m: additive JEPA training on the same cadence. The Adam MSE
     * training above is raw-space; JEPA is embedding-space. Both run, no
     * conflict (they touch different parameters). JEPA no-ops when
     * jepa_enabled is false. */
    (void)_octopus_jepa_train_all_arms(ctx, NULL);

    if (trained > 0) {
        ctx->stats.n_lnn_train_steps += trained;
        ctx->stats.last_lnn_loss = total_loss / (float)trained;
        NIMCP_LOGGING_DEBUG("octopus_train_step: trained %u/%u arms, "
                            "mean_loss=%.4f total=%u",
                            trained, ctx->n_arms,
                            ctx->stats.last_lnn_loss,
                            ctx->stats.n_lnn_train_steps);
        if (loss_out) *loss_out = ctx->stats.last_lnn_loss;
        return 0;
    }
    NIMCP_LOGGING_TRACE("octopus_train_step: no arms trained (no prev/cur pairs yet)");
    if (loss_out) *loss_out = 0.0f;
    return -1;
}

/*============================================================================
 * Phase 4m: JEPA latent-space predictive coding.
 *==========================================================================*/

void octopus_set_jepa_enabled(octopus_system_t* ctx, bool enabled) {
    if (!ctx) return;
    /* Refuse to promise capability we can't deliver (statue-lesson pattern). */
    if (enabled &&
        (!ctx->jepa_predictor || !ctx->jepa_ctx_latent ||
         !ctx->jepa_tgt_latent || !ctx->arm_prev_latents ||
         !ctx->arm_cur_latents || !ctx->arm_has_prev_latent ||
         !ctx->arm_has_cur_latent)) {
        NIMCP_LOGGING_WARN("octopus_set_jepa_enabled(true) refused: "
                           "JEPA infrastructure not allocated");
        return;
    }
    bool prev = ctx->stats.jepa_enabled;
    ctx->stats.jepa_enabled = enabled;
    if (prev != enabled) {
        NIMCP_LOGGING_INFO("octopus: JEPA latent-space training %s",
                           enabled ? "ENABLED" : "DISABLED");
    }
}

/* Called from octopus_train_step when jepa_enabled. Trains the shared JEPA
 * predictor on (prev_arm_latent → cur_arm_latent) pairs per arm.
 * Returns number of arms trained + mean loss via *out_loss. */
static uint32_t _octopus_jepa_train_all_arms(octopus_system_t* ctx,
                                              float* out_loss) {
    if (!ctx || !ctx->stats.jepa_enabled || !ctx->jepa_predictor) {
        if (out_loss) *out_loss = 0.0f;
        return 0;
    }

    jepa_predictor_t* jp = (jepa_predictor_t*)ctx->jepa_predictor;
    jepa_latent_t*    ctx_lat = (jepa_latent_t*)ctx->jepa_ctx_latent;
    jepa_latent_t*    tgt_lat = (jepa_latent_t*)ctx->jepa_tgt_latent;

    float total_loss = 0.0f;
    uint32_t trained = 0;
    for (uint32_t a = 0; a < ctx->n_arms; a++) {
        if (ctx->arms[a].vetoed) continue;
        if (!ctx->arm_has_prev_latent[a] || !ctx->arm_has_cur_latent[a]) continue;

        if (jepa_latent_set_embedding(ctx_lat,
                ctx->arm_prev_latents[a], OCTOPUS_ARM_DIM) != 0) continue;
        if (jepa_latent_set_embedding(tgt_lat,
                ctx->arm_cur_latents[a],  OCTOPUS_ARM_DIM) != 0) continue;

        float step_loss = 0.0f;
        if (jepa_predictor_train_step(jp, ctx_lat, tgt_lat, &step_loss) == 0) {
            total_loss += step_loss;
            trained++;
        }
    }
    if (trained > 0) {
        ctx->stats.n_jepa_steps += trained;
        ctx->stats.last_jepa_loss = total_loss / (float)trained;
        NIMCP_LOGGING_DEBUG("octopus_jepa_train: trained %u/%u arms, "
                            "mean_loss=%.4f total=%u",
                            trained, ctx->n_arms,
                            ctx->stats.last_jepa_loss,
                            ctx->stats.n_jepa_steps);
        if (out_loss) *out_loss = ctx->stats.last_jepa_loss;
    } else {
        NIMCP_LOGGING_TRACE("octopus_jepa_train: no arms eligible (no prev/cur latents yet)");
        if (out_loss) *out_loss = 0.0f;
    }
    return trained;
}

/*============================================================================
 * Phase 4l: natural-gradient latent regularization.
 *==========================================================================*/

void octopus_set_ng_enabled(octopus_system_t* ctx, bool enabled) {
    if (!ctx) return;
    /* Only flip the public flag if the NG infrastructure is actually live —
     * prevents a true flag from promising capability we can't deliver. */
    if (enabled && !ctx->arm_nat_grads) {
        NIMCP_LOGGING_WARN("octopus_set_ng_enabled(true) refused: "
                           "NG infrastructure not allocated");
        return;
    }
    bool prev = ctx->stats.ng_enabled;
    ctx->stats.ng_enabled = enabled;
    if (prev != enabled) {
        NIMCP_LOGGING_INFO("octopus: NG regularization %s",
                           enabled ? "ENABLED" : "DISABLED");
    }
}

int octopus_ng_regularize(octopus_system_t* ctx) {
    if (!ctx) return -1;
    if (!ctx->stats.ng_enabled) return -1;
    if (!ctx->arm_nat_grads) return -1;

    uint32_t applied = 0;
    for (uint32_t a = 0; a < ctx->n_arms; a++) {
        if (ctx->arms[a].vetoed) continue;
        nimcp_natural_gradient_t ng =
            (nimcp_natural_gradient_t)ctx->arm_nat_grads[a];
        if (!ng) continue;

        /* Gradient signal = the latent itself — pulls latent toward origin.
         * The Fisher metric makes this pull anisotropic along directions
         * the arm's latent has historically varied, rather than uniform
         * shrinkage. One ghost "gradient = param" step is cheap but still
         * exercises the Fisher EMA update inside natural_grad_step. */
        float grad_copy[OCTOPUS_ARM_DIM];
        memcpy(grad_copy, ctx->arms[a].latent, sizeof(grad_copy));

        if (nimcp_natural_grad_step(ng, ctx->arms[a].latent,
                                     grad_copy, OCTOPUS_ARM_DIM)
            == INFO_GEOM_OK) {
            applied++;
        }
    }
    if (applied > 0) {
        ctx->stats.n_ng_steps += applied;
        NIMCP_LOGGING_DEBUG("octopus_ng_regularize: applied to %u arms "
                            "(total=%u)", applied, ctx->stats.n_ng_steps);
        return 0;
    }
    return -1;
}

void octopus_set_bridge_stats(octopus_system_t* ctx,
                               uint64_t engram_encodings,
                               uint64_t kg_nodes_added,
                               uint64_t stress_broadcasts,
                               uint64_t fear_broadcasts,
                               uint64_t amygdala_steps,
                               float    last_cortisol,
                               float    last_fear,
                               uint64_t wm_updates,
                               uint64_t fear_conditionings,
                               uint64_t vision_samples,
                               uint64_t audio_samples,
                               uint64_t somato_samples,
                               uint64_t snn_samples,
                               uint64_t neuromod_samples,
                               uint64_t peer_samples) {
    if (!ctx) return;
    ctx->stats.bridge_engram_encodings   = engram_encodings;
    ctx->stats.bridge_kg_nodes_added     = kg_nodes_added;
    ctx->stats.bridge_stress_broadcasts  = stress_broadcasts;
    ctx->stats.bridge_fear_broadcasts    = fear_broadcasts;
    ctx->stats.bridge_amygdala_steps     = amygdala_steps;
    ctx->stats.bridge_last_cortisol      = last_cortisol;
    ctx->stats.bridge_last_fear          = last_fear;
    ctx->stats.bridge_wm_updates         = wm_updates;
    ctx->stats.bridge_fear_conditionings = fear_conditionings;
    ctx->stats.bridge_vision_samples     = vision_samples;
    ctx->stats.bridge_audio_samples      = audio_samples;
    ctx->stats.bridge_somato_samples     = somato_samples;
    ctx->stats.bridge_snn_samples        = snn_samples;
    ctx->stats.bridge_neuromod_samples   = neuromod_samples;
    ctx->stats.bridge_peer_samples       = peer_samples;
}
