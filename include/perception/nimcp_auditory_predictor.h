/**
 * @file nimcp_auditory_predictor.h
 * @brief Wernicke predictor: a recurrent spiking network (ALIF neurons) that
 *        listens to the auditory nerve continuously and learns, with e-prop,
 *        to predict the next 40 ms of sound.
 *
 * WHAT: C port of athena_core/athena/bio/wernicke.py (WernickePredictor),
 *       the configuration measured best on the LibriSpeech cochlea stream
 *       (512 neurons, readout decay; skill vs copy .13 after 15 min of
 *       speech, .19 after 5 h; 2026-11-22/23).
 * WHY:  the brain's cochlea produced auditory-nerve spikes that nothing
 *       learned from. This is the consumer: every frame it hears is first
 *       predicted, then learned from (prequential, so its running skill is
 *       an honest held-out measure).
 * HOW:  one frame = per-channel auditory-nerve spike counts over 40 ms,
 *       presented for `sub` steps of 40/sub ms. Learning is local in space
 *       and time: eligibility traces x per-neuron error through fixed random
 *       feedback (no backpropagation). See the Python module docstring for
 *       the equations.
 *
 * Not thread-safe: the caller serialises access.
 */
#ifndef NIMCP_AUDITORY_PREDICTOR_H
#define NIMCP_AUDITORY_PREDICTOR_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t n_rec;          /**< recurrent neurons (512) */
    uint32_t sub;            /**< simulation steps per 40 ms frame (8: dt 5 ms) */
    float tau_m;             /**< membrane ms (20) */
    float tau_out;           /**< readout ms (20) */
    float tau_a[2];          /**< adaptation ms (200, 2000) */
    float frac_adaptive;     /**< share of neurons with adaptation (0.5) */
    float adapt_rise;        /**< threshold rise at target rate, x thr (0.5) */
    float thr;               /**< threshold (1) */
    float gamma;             /**< pseudo-derivative scale (0.3) */
    float target_rate;       /**< Hz (10) */
    float reg;               /**< firing-rate regulariser (0.1) */
    float lr;                /**< input + recurrent learning rate (1e-3) */
    float lr_out;            /**< readout learning rate (0.005) */
    float out_decay;         /**< readout decay per frame (1e-5) */
    float in_gain;           /**< input weight scale (4) */
    float ip_lr;             /**< intrinsic plasticity rate (0.1) */
    float ffi_tau;           /**< feedforward-inhibition window ms (2000; 0 = off) */
    float input_scale;       /**< spike count -> drive: x = count / input_scale (320) */
    float stats_window_frames; /**< EMA window of the running skill (9000 = 6 min) */
    uint64_t seed;
} auditory_predictor_config_t;

typedef struct {
    uint64_t frames;         /**< frames heard */
    uint64_t scored;         /**< frames whose prediction was scored */
    double hours;            /**< speech heard, hours */
    float rate_hz;           /**< mean firing rate over the last frame */
    float skill_recent;      /**< 1 - MSE/MSE(copy), EMA over stats_window_frames */
    float skill_total;       /**< 1 - MSE/MSE(copy) since creation (or load) */
    float mse_recent;
    float copy_mse_recent;
    uint64_t nonfinite_resets; /**< frames whose error was non-finite: listening state
                                    was dropped and restarted (not persisted). Rising
                                    every frame means the weights themselves diverged. */
} auditory_predictor_stats_t;

typedef struct auditory_predictor auditory_predictor_t;

auditory_predictor_config_t auditory_predictor_config_default(void);

/** dim: auditory-nerve channels. NULL on bad config or allocation failure. */
auditory_predictor_t* auditory_predictor_create(uint32_t dim,
                                                const auditory_predictor_config_t* cfg);
void auditory_predictor_destroy(auditory_predictor_t* p);

/** Hear one frame of spike counts (dim values): learn from the error of
 *  the last prediction, then predict the next frame. 0 on success. */
int auditory_predictor_step_counts(auditory_predictor_t* p, const uint16_t* counts);

/** Same, with drive already in [0, 1] (x = count / input_scale). */
int auditory_predictor_step(auditory_predictor_t* p, const float* x);

/** The current prediction of the next frame's drive (dim values), or NULL
 *  before the first frame. */
const float* auditory_predictor_prediction(const auditory_predictor_t* p);

uint32_t auditory_predictor_dim(const auditory_predictor_t* p);
uint32_t auditory_predictor_n_rec(const auditory_predictor_t* p);

/** Error of the last scored prediction (prediction - actual change), dim
 *  values; NULL before the second frame. */
const float* auditory_predictor_last_error(const auditory_predictor_t* p);

/** Recurrent layer's filtered spike trains (n_rec values, the readout's
 *  presynaptic activity): what the network "hears" right now. */
const float* auditory_predictor_hidden(const auditory_predictor_t* p);

/** Mean squared error of the last scored frame, and of predicting no change. */
void auditory_predictor_last_mse(const auditory_predictor_t* p, float* mse, float* copy_mse);
void auditory_predictor_get_stats(const auditory_predictor_t* p, auditory_predictor_stats_t* out);

/** Weights, excitability, listening state and statistics (eligibility
 *  traces restart at zero). Written to path.tmp and renamed. 0 on success. */
int auditory_predictor_save(const auditory_predictor_t* p, const char* path);
/** NULL if the file is missing or not a predictor file. */
auditory_predictor_t* auditory_predictor_load(const char* path);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_AUDITORY_PREDICTOR_H */
