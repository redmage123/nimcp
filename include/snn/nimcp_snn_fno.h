/**
 * @file nimcp_snn_fno.h
 * @brief Fourier Neural Operator for SNN Population Dynamics
 *
 * WHAT: Learns population-level state transitions as a neural operator
 * WHY:  Per-neuron LIF stepping is O(N) per timestep. FNO learns the
 *       collective dynamics, enabling larger effective timesteps and
 *       capturing emergent population phenomena.
 * HOW:  Train FNO on (state_t, I_syn) → (state_{t+dt}, spikes) pairs
 *       collected from LIF ground truth. Switch to FNO for inference
 *       when validation MSE falls below threshold.
 *
 * COEXISTENCE:
 *   Training mode:  Run LIF normally, record state pairs → train FNO
 *   Inference mode:  FNO predicts next state, skips per-neuron LIF
 *   Validation mode: Run both, compare, fine-tune FNO online
 */

#ifndef NIMCP_SNN_FNO_H
#define NIMCP_SNN_FNO_H

#include "snn/nimcp_snn_types.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * Configuration
 * ========================================================================= */

typedef struct snn_fno_config_s {
    uint32_t n_modes;               /**< Fourier modes (0 = auto: pop_size/4) */
    uint32_t hidden_channels;       /**< FNO width (default: 16) */
    uint32_t n_blocks;              /**< Number of FNO blocks (default: 3) */
    uint32_t training_buffer_size;  /**< State pairs to buffer (default: 256) */
    float learning_rate;            /**< FNO learning rate (default: 0.001) */
    float replacement_threshold;    /**< MSE below which FNO replaces LIF (default: 0.01) */
    bool per_population;            /**< One FNO per population (default: true) */
} snn_fno_config_t;

/* =========================================================================
 * Training Data
 * ========================================================================= */

typedef struct snn_fno_state_pair_s {
    float* state_in;                /**< Membrane voltages at time t [n] */
    float* synaptic_input;          /**< External currents at time t [n] */
    float* state_out;               /**< Membrane voltages at time t+dt [n] */
    float* spike_out;               /**< Spike indicators at time t+dt [n] */
} snn_fno_state_pair_t;

/* =========================================================================
 * Per-Population FNO
 * ========================================================================= */

typedef struct snn_fno_population_s {
    uint32_t pop_id;
    uint32_t n_neurons;
    uint32_t state_dim;             /**< = n_neurons (membrane V) */
    uint32_t input_dim;             /**< = n_neurons (synaptic I) */

    /* FNO layers (simplified: dense lifting → spectral blocks → dense projection)
     * Uses flat arrays instead of fno_spectral_conv_t to avoid circular dependency */
    uint32_t n_blocks;
    uint32_t hidden_channels;
    uint32_t n_modes;
    uint32_t fft_size;

    /* Lifting: [hidden_ch, state_dim + input_dim] */
    float* W_lift;
    float* b_lift;
    float* grad_W_lift;
    float* grad_b_lift;

    /* Spectral weights per block: [n_blocks][hidden_ch * hidden_ch * n_modes * 2] */
    float** block_W_real;
    float** block_W_imag;
    float** block_grad_W_real;
    float** block_grad_W_imag;
    float** block_bypass;
    float** block_grad_bypass;

    /* Projection: [state_dim + n_neurons, hidden_ch] → V_next + spikes */
    float* W_proj;
    float* b_proj;
    float* grad_W_proj;
    float* grad_b_proj;

    /* Training ring buffer */
    snn_fno_state_pair_t* buffer;
    uint32_t buffer_size;
    uint32_t buffer_count;
    uint32_t buffer_write_idx;

    /* Metrics */
    float train_mse;
    float validation_mse;
    bool ready_for_inference;
    uint64_t train_steps;
    uint64_t inference_steps;
} snn_fno_population_t;

/* =========================================================================
 * API
 * ========================================================================= */

/** Set default config */
void snn_fno_config_default(snn_fno_config_t* config);

/** Create per-population FNO */
snn_fno_population_t* snn_fno_population_create(
    uint32_t pop_id, uint32_t n_neurons,
    const snn_fno_config_t* config);

/** Destroy per-population FNO */
void snn_fno_population_destroy(snn_fno_population_t* fno);

/** Record a (state_before, I_syn, state_after, spikes) pair for training */
int snn_fno_record_pair(snn_fno_population_t* fno,
    const float* v_before, const float* i_syn,
    const float* v_after, const float* spikes, uint32_t n);

/** Train FNO on accumulated pairs */
int snn_fno_train(snn_fno_population_t* fno, uint32_t n_epochs);

/** Predict next state using FNO */
int snn_fno_predict(snn_fno_population_t* fno,
    const float* v_current, const float* i_syn, uint32_t n,
    float* v_next, float* spikes);

/** Validate FNO against LIF ground truth, returns MSE */
float snn_fno_validate(snn_fno_population_t* fno,
    const float* v_next_lif, const float* spikes_lif, uint32_t n);

/** Get training metrics */
float snn_fno_get_train_mse(const snn_fno_population_t* fno);
float snn_fno_get_validation_mse(const snn_fno_population_t* fno);
bool snn_fno_is_ready(const snn_fno_population_t* fno);
uint32_t snn_fno_param_count(const snn_fno_population_t* fno);

/* Network-level FNO management */

/** Initialize FNO for all populations in the network */
int snn_network_init_fno(snn_network_t* network, const snn_fno_config_t* config);

/** Run one network step using FNO (replaces LIF when ready) */
int snn_network_step_fno(snn_network_t* network, float dt);

/** Destroy all population FNOs */
void snn_network_destroy_fno(snn_network_t* network);

/** Snapshot per-population membrane V into the network's v_prev scratch.
 *  Called at the top of snn_network_step BEFORE any integration so the
 *  matching record_post_step at the bottom can pair (V_before, V_after). */
void snn_fno_snapshot_v_before(snn_network_t* network);

/** Record (V_before, I_syn, V_after, spikes) into each population's FNO
 *  training buffer. Called at the bottom of snn_network_step AFTER all
 *  integration and stats updates have settled. No-op when recording is
 *  disabled or the FNO array isn't wired. */
void snn_fno_record_post_step(snn_network_t* network);

/** Toggle FNO training-pair recording on/off at runtime.
 *  Default after snn_network_init_fno is OFF — opt-in here once the
 *  GPU-sync path is acceptable. Refused if init never allocated the
 *  scratch buffer. */
void snn_set_fno_recording_enabled(snn_network_t* network, bool enabled);

/** Read current recording state. */
bool snn_get_fno_recording_enabled(const snn_network_t* network);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SNN_FNO_H */
