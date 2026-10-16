/**
 * @file nimcp_audio_cortex.c
 * @brief Implementation of biologically-inspired auditory processing
 *
 * WHAT: Cochlear processing with FFT-based frequency analysis
 * WHY:  Enable auditory perception and memory in NIMCP
 * HOW:  Mel-scale filterbank + MFCC + tensor-accelerated operations
 *
 * Phase TENSOR-2: Tensor library integration for accelerated operations
 *
 * @author NIMCP Development Team
 * @date 2025
 * @version 2.7.0 - Added tensor library integration
 */

#include "perception/nimcp_audio_cortex.h"
#include "utils/tensor/nimcp_tensor.h"  /* Tensor library for vectorized operations */
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "security/nimcp_bbb_helpers.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"  /* KG reader for self-awareness */

#include "utils/memory/nimcp_unified_memory.h"
#include <stddef.h>  /* for NULL */
#include "perception/nimcp_visual_cortex.h"  // For receptor_expression_t
#include "utils/memory/nimcp_memory.h"
#include "utils/memory/nimcp_memory_pool.h"  // Memory pool for O(1) allocations
#include "utils/memory/nimcp_page_cow.h"     // Copy-on-Write for shallow copies
#include "utils/validation/nimcp_validate.h"
#include "utils/logging/nimcp_logging.h"
#include "plasticity/neuromodulators/nimcp_neuromodulators.h"  // Neuromodulator integration
#include "plasticity/nimcp_second_messengers.h"  // Second messenger cascade system
#include "core/brain/nimcp_brain.h"  // Brain reference
#include "core/neuralnet/nimcp_neuralnet.h"  // Neural network for internal A1 connections
#include "core/topology/nimcp_fractal_topology.h"  // Scale-free topology generation
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_math_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(audio_cortex)

/*=============================================================================
 * LOGGING MODULE IDENTIFIER
 *===========================================================================*/

#define AUDIO_LOG_MODULE "AUDIO_CORTEX"


//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Phasic/tonic and receptor types imported from nimcp_visual_cortex.h
 * (Removed local definitions to use shared types from visual cortex header)
 */
// typedef phasic_tonic_state_t and receptor_expression_t removed - using shared definitions

/**
 * @brief Audio cortex internal structure
 */
struct audio_cortex {
    // Configuration
    audio_cortex_config_t config;

    // Cochlear processing
    float* fft_window;           ///< Hamming window
    float* fft_real;             ///< FFT real part buffer
    float* fft_imag;             ///< FFT imaginary part buffer
    float* mel_filterbank;       ///< Mel-scale filter weights

    // Temporal processing
    float* prev_frame;           ///< Previous frame for onset detection
    float prev_energy;           ///< Previous frame energy

    // Memory system
    auditory_memory_t** memories;
    uint32_t num_memories;
    uint32_t memory_capacity;

    // Neuromodulation
    brain_t brain;  /**< Brain reference for ACh + 5-HT modulation */

    // Phasic/tonic neuromodulation
    phasic_tonic_state_t ach_state;     /**< Acetylcholine phasic/tonic state */
    phasic_tonic_state_t ne_state;      /**< Norepinephrine phasic/tonic state */
    receptor_expression_t receptors;    /**< Receptor subtype expression */

    // NIMCP 2.7 Phase 8.5: Internal recurrent network with fractal topology
    neural_network_t internal_network;  /**< Recurrent network for tonotopic connections */
    bool has_internal_network;          /**< Whether internal network was created */

    // === Memory Pool for O(1) Memory Allocation ===
    // WHAT: Pre-allocated pool for auditory_memory_t structures
    // WHY:  Memory storage is hot path; avoid malloc overhead
    // HOW:  Pool with block_size = sizeof(auditory_memory_t)
    memory_pool_t memory_pool;            /**< Pool for auditory memory entries */
    nimcp_mutex_t* memory_pool_mutex;     /**< Mutex for memory pool thread safety */

    // === Copy-on-Write Support ===
    // WHAT: Enable shallow copy of cortex with lazy duplication
    // WHY:  Fast cloning for parallel processing or checkpointing
    // HOW:  Reference counting on shared data, copy on modification
    uint32_t* _cow_refcount;              /**< Reference count for CoW (NULL if owned) */
    bool _cow_is_shallow;                 /**< True if this is a shallow copy */

    // === Bio-Async Communication ===
    bio_module_context_t bio_ctx;         /**< Bio-async module context */
    bool bio_async_enabled;               /**< Whether bio-async is enabled */

    // === Second Messenger Cascade System ===
    second_messenger_system_t* second_messengers; /**< Second messenger cascade system */
    bool second_messengers_enabled;       /**< Whether second messengers are enabled */

    // === Training Interface (CNN-Cortex Integration) ===
    bool training_mode;                           /**< Whether in training mode */
    float* cached_mel_features;                   /**< Cached mel features for training */
    uint32_t cached_mel_size;                     /**< Size of cached mel features */
    float* cached_mfcc_features;                  /**< Cached MFCC features for training */
    uint32_t cached_mfcc_size;                    /**< Size of cached MFCC features */
    float last_quality;                           /**< Last computed audio quality */
    float last_speech_salience;                   /**< Last computed speech salience */
    uint64_t last_process_timestamp;              /**< Timestamp of last processing */

    // Statistics
    audio_cortex_stats_t stats;
};

//=============================================================================
// Forward Declarations
//=============================================================================

static void update_neuromodulator_states(audio_cortex_t* cortex, float dt);

//=============================================================================
// Neuromodulation - Phasic/Tonic Dynamics
//=============================================================================

/**
 * @brief Update phasic/tonic neuromodulator state with decay
 *
 * WHAT: Update phasic component to decay toward tonic baseline
 * WHY:  Phasic bursts are transient, tonic is sustained
 * HOW:  Exponential decay: phasic(t+dt) = phasic(t) * exp(-decay_rate * dt) + tonic
 *
 * COMPLEXITY: O(1)
 *
 * @param state Phasic/tonic state to update
 * @param dt Time step in seconds
 */
static void update_phasic_tonic(phasic_tonic_state_t* state, float dt)
{
    if (!state) return;

    // Exponential decay toward tonic baseline
    float burst_decay_tau = phasic_tonic_get_burst_decay_tau(state);
    float decay_rate = 1.0F / burst_decay_tau;  // Convert tau to rate
    float decay_factor = expf(-decay_rate * dt);
    float phasic = phasic_tonic_get_phasic_burst(state);
    float tonic = phasic_tonic_get_tonic_level(state);
    float updated_phasic = phasic * decay_factor + tonic * (1.0F - decay_factor);
    if (isfinite(updated_phasic)) {
        phasic_tonic_set_phasic_burst(state, updated_phasic);
    }
}

/**
 * @brief Trigger phasic burst of neuromodulator
 *
 * WHAT: Add phasic burst component
 * WHY:  Salient events trigger rapid release
 * HOW:  Add to phasic level, clamped to [0, 1]
 *
 * COMPLEXITY: O(1)
 *
 * @param state Phasic/tonic state
 * @param burst_amount Amount to add [0-1]
 */
static void trigger_phasic_burst(phasic_tonic_state_t* state, float burst_amount)
{
    if (!state) return;

    float phasic = phasic_tonic_get_phasic_burst(state) + burst_amount;
    if (phasic > 1.0F) phasic = 1.0F;
    phasic_tonic_set_phasic_burst(state, phasic);
}

/**
 * @brief Compute effective ACh level from phasic/tonic + receptors
 *
 * WHAT: Calculate ACh modulation strength
 * WHY:  Combines temporal dynamics (phasic/tonic) with receptor density
 * HOW:  effective = (phasic + tonic) / 2 * receptor_density
 *
 * COMPLEXITY: O(1)
 *
 * @param ach_state ACh phasic/tonic state
 * @param receptors Receptor expression
 * @return Effective ACh modulation [0-1]
 */
static float get_effective_ach(const phasic_tonic_state_t* ach_state,
                                const receptor_expression_t* receptors)
{
    if (!ach_state || !receptors) return 0.0F;

    // Average of phasic and tonic
    float ach_level = (phasic_tonic_get_phasic_burst(ach_state) + phasic_tonic_get_tonic_level(ach_state)) * 0.5F;

    // M4 receptors primarily sharpen frequency tuning
    float m4_effect = ach_level * receptors->m2_density;

    // M2 receptors provide presynaptic feedback
    float m2_effect = ach_level * receptors->m2_density * 0.5F;

    return m4_effect + m2_effect;
}

/**
 * @brief Compute effective NE level from phasic/tonic + receptors
 *
 * WHAT: Calculate NE modulation strength
 * WHY:  Combines temporal dynamics with receptor density
 * HOW:  effective = (phasic + tonic) / 2 * receptor_density
 *
 * COMPLEXITY: O(1)
 *
 * @param ne_state NE phasic/tonic state
 * @param receptors Receptor expression
 * @return Effective NE modulation [0-1]
 */
static float get_effective_ne(const phasic_tonic_state_t* ne_state,
                               const receptor_expression_t* receptors)
{
    if (!ne_state || !receptors) return 0.0F;

    // Average of phasic and tonic
    float ne_level = (phasic_tonic_get_phasic_burst(ne_state) + phasic_tonic_get_tonic_level(ne_state)) * 0.5F;

    // α1 receptors enhance onset detection
    float alpha1_effect = ne_level * receptors->alpha1_density;

    // β2 receptors modulate temporal sensitivity
    float beta2_effect = ne_level * receptors->beta2_density * 0.7F;

    return alpha1_effect + beta2_effect;
}

/**
 * @brief Apply cocktail party effect - ACh sharpens frequency tuning
 *
 * WHAT: Enhance frequency selectivity via lateral inhibition
 * WHY:  ACh enables selective attention to specific frequencies
 * HOW:  Sharpen mel filterbank response using center-surround contrast
 *
 * BIOLOGY:
 * - High ACh → narrow tuning curves (focus on target frequency)
 * - Low ACh → broad tuning curves (poor discrimination)
 * - M4 receptors in auditory cortex sharpen frequency selectivity
 *
 * ALGORITHM:
 * 1. Compute local contrast: enhanced[i] = mel[i] - avg(neighbors)
 * 2. Scale by ACh: sharpened[i] = mel[i] + ach_strength * contrast[i]
 * 3. Normalize to maintain energy
 *
 * COMPLEXITY: O(n) where n = num_mel_filters
 *
 * @param mel_features Mel-scale features to sharpen
 * @param num_filters Number of mel filters
 * @param ach_strength ACh modulation strength [0-1]
 */
static void apply_cocktail_party_effect(float* mel_features, uint32_t num_filters,
                                         float ach_strength)
{
    if (!mel_features || num_filters < 3) return;

    // Allocate temporary buffer for contrast computation
    float* contrast = (float*)nimcp_calloc(num_filters, sizeof(float));
    if (!contrast) return;

    // Compute center-surround contrast
    for (uint32_t i = 0; i < num_filters; i++) {
        float center = mel_features[i];
        float surround = 0.0F;
        int neighbor_count = 0;

        // Average of neighbors (1 to each side)
        if (i > 0) {
            surround += mel_features[i - 1];
            neighbor_count++;
        }
        if (i < num_filters - 1) {
            surround += mel_features[i + 1];
            neighbor_count++;
        }

        if (neighbor_count > 0) {
            surround /= neighbor_count;
            contrast[i] = center - surround;
        }
    }

    // Apply sharpening scaled by ACh strength
    // ach_strength in [0, 1] → sharpening_factor in [0, 4]
    // Stronger sharpening for more pronounced variance increase
    float sharpening_factor = ach_strength * 4.0F;

    for (uint32_t i = 0; i < num_filters; i++) {
        mel_features[i] += sharpening_factor * contrast[i];
        // Clamp to non-negative (log-scale can go negative)
        if (mel_features[i] < -10.0F) mel_features[i] = -10.0F;
    }

    nimcp_free(contrast);
}

/**
 * @brief Enhance onset detection sensitivity with NE modulation
 *
 * WHAT: Modulate onset detection threshold
 * WHY:  NE enhances temporal sensitivity and alertness
 * HOW:  Lower threshold for onset detection by NE amount
 *
 * BIOLOGY:
 * - High NE → sensitive to transients (alert state)
 * - Low NE → less sensitive (relaxed state)
 * - α1 receptors in auditory cortex enhance onset responses
 *
 * ALGORITHM:
 * threshold_multiplier = 1.0 - (ne_strength * 0.5)
 * Effective range: [1.0, 0.5] (up to 50% reduction)
 *
 * COMPLEXITY: O(1)
 *
 * @param base_threshold Base onset threshold
 * @param ne_strength NE modulation strength [0-1]
 * @return Modulated threshold
 */
static float modulate_onset_threshold(float base_threshold, float ne_strength)
{
    // NE reduces threshold (increases sensitivity)
    // ne_strength in [0, 1] → reduction in [0%, 50%]
    float threshold_multiplier = 1.0F - (ne_strength * 0.5F);
    return base_threshold * threshold_multiplier;
}

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Compute Hamming window
 */
static void compute_hamming_window(float* window, uint32_t size)
{
    if (!nimcp_validate_pointer(window, "window")) return;

    for (uint32_t i = 0; i < size; i++) {
        window[i] = 0.54F - 0.46F * cosf(2.0F * M_PI * i / (size - 1));
    }
}

/**
 * @brief Convert frequency to mel scale
 */
static float hz_to_mel(float hz)
{
    return 2595.0F * log10f(1.0F + hz / 700.0F);
}

/**
 * @brief Convert mel to frequency
 */
static float mel_to_hz(float mel)
{
    return 700.0F * (powf(10.0F, mel / 2595.0F) - 1.0F);
}

/**
 * @brief Initialize mel filterbank
 */
static bool init_mel_filterbank(audio_cortex_t* cortex)
{
    if (!nimcp_validate_pointer(cortex, "cortex")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_mel_filterbank: nimcp_validate_pointer is NULL");
        return false;
    }

    uint32_t num_filters = cortex->config.num_mel_filters;
    uint32_t num_bins = cortex->config.num_freq_bins;
    float sample_rate = (float)cortex->config.sample_rate;

    // Allocate filterbank matrix
    cortex->mel_filterbank = (float*)nimcp_calloc(
        num_filters * num_bins, sizeof(float)
    );
    if (!nimcp_validate_pointer(cortex->mel_filterbank, "mel_filterbank")) {
        LOG_ERROR(AUDIO_LOG_MODULE, "Failed to allocate mel filterbank");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "init_mel_filterbank: nimcp_validate_pointer is NULL");
        return false;
    }

    // Compute mel-scale boundaries
    float mel_min = hz_to_mel(0.0F);
    float mel_max = hz_to_mel(sample_rate / 2.0F);
    float mel_step = (mel_max - mel_min) / (num_filters + 1);

    // Create triangular filters
    for (uint32_t m = 0; m < num_filters; m++) {
        float f_left = mel_to_hz(mel_min + m * mel_step);
        float f_center = mel_to_hz(mel_min + (m + 1) * mel_step);
        float f_right = mel_to_hz(mel_min + (m + 2) * mel_step);

        // Convert frequencies to bin indices
        uint32_t bin_left = (uint32_t)(f_left * num_bins / (sample_rate / 2.0F));
        uint32_t bin_center = (uint32_t)(f_center * num_bins / (sample_rate / 2.0F));
        uint32_t bin_right = (uint32_t)(f_right * num_bins / (sample_rate / 2.0F));

        // Rising slope
        for (uint32_t k = bin_left; k < bin_center && k < num_bins; k++) {
            float weight = (float)(k - bin_left) / (bin_center - bin_left);
            cortex->mel_filterbank[m * num_bins + k] = weight;
        }

        // Falling slope
        for (uint32_t k = bin_center; k < bin_right && k < num_bins; k++) {
            float weight = (float)(bin_right - k) / (bin_right - bin_center);
            cortex->mel_filterbank[m * num_bins + k] = weight;
        }
    }

    return true;
}

/**
 * @brief Simple FFT implementation (Cooley-Tukey radix-2)
 */
static void fft(float* real, float* imag, uint32_t n, bool inverse)
{
    if (n <= 1) return;

    // Guard: Validate n is power of 2 for radix-2 FFT
    if ((n & (n - 1)) != 0) {
        LOG_ERROR(AUDIO_LOG_MODULE, "FFT size must be power of 2, got %u", n);
        return;
    }

    // Bit-reversal permutation
    uint32_t j = 0;
    for (uint32_t i = 0; i < n - 1; i++) {
        if (i < j) {
            float temp = real[i];
            real[i] = real[j];
            real[j] = temp;
            temp = imag[i];
            imag[i] = imag[j];
            imag[j] = temp;
        }
        uint32_t k = n / 2;
        while (k <= j) {
            j -= k;
            k /= 2;
        }
        j += k;
    }

    // Cooley-Tukey FFT
    float direction = inverse ? 1.0F : -1.0F;
    for (uint32_t s = 1; s <= (uint32_t)log2f((float)n); s++) {
        uint32_t m = 1 << s;
        uint32_t m2 = m / 2;
        float w_real = cosf(direction * 2.0F * M_PI / m);
        float w_imag = sinf(direction * 2.0F * M_PI / m);

        for (uint32_t k = 0; k < n; k += m) {
            float wr = 1.0F;
            float wi = 0.0F;

            for (uint32_t j = 0; j < m2; j++) {
                uint32_t t = k + j;
                uint32_t u = k + j + m2;

                float tr = real[u] * wr - imag[u] * wi;
                float ti = real[u] * wi + imag[u] * wr;

                real[u] = real[t] - tr;
                imag[u] = imag[t] - ti;
                real[t] = real[t] + tr;
                imag[t] = imag[t] + ti;

                float temp = wr;
                wr = temp * w_real - wi * w_imag;
                wi = temp * w_imag + wi * w_real;
            }
        }
    }

    // Normalize for inverse transform
    if (inverse) {
        for (uint32_t i = 0; i < n; i++) {
            real[i] /= n;
            imag[i] /= n;
        }
    }
}

/**
 * @brief Compute DCT for MFCC
 */
static void dct(const float* input, float* output, uint32_t n, uint32_t num_coeff)
{
    if (!nimcp_validate_pointer(input, "input") || !nimcp_validate_pointer(output, "output")) return;

    for (uint32_t k = 0; k < num_coeff; k++) {
        float sum = 0.0F;
        for (uint32_t n_idx = 0; n_idx < n; n_idx++) {
            sum += input[n_idx] * cosf(M_PI * k * (n_idx + 0.5F) / n);
        }
        output[k] = sum;
    }
}

//=============================================================================
// Core API Implementation
//=============================================================================

audio_cortex_t* audio_cortex_create(const audio_cortex_config_t* config)
{
    if (!nimcp_validate_pointer(config, "config")) {
        NIMCP_THROW_BRAIN(NIMCP_ERROR_NULL_POINTER, 0, "audio_cortex",
            "NULL config provided to audio_cortex_create");
        return NULL;
    }

    // Validate configuration
    if (config->sample_rate < AUDIO_MIN_SAMPLE_RATE ||
        config->sample_rate > AUDIO_MAX_SAMPLE_RATE) {
        NIMCP_THROW_BRAIN(NIMCP_ERROR_INVALID_PARAM, 0, "audio_cortex",
            "Invalid sample rate: %u (valid range: %u-%u)",
            config->sample_rate, AUDIO_MIN_SAMPLE_RATE, AUDIO_MAX_SAMPLE_RATE);
        LOG_ERROR(AUDIO_LOG_MODULE, "Invalid sample rate: %u", config->sample_rate);
        return NULL;
    }

    if (config->num_channels == 0 || config->num_channels > AUDIO_MAX_CHANNELS) {
        NIMCP_THROW_BRAIN(NIMCP_ERROR_INVALID_PARAM, 0, "audio_cortex",
            "Invalid number of channels: %u (max: %u)", config->num_channels, AUDIO_MAX_CHANNELS);
        LOG_ERROR(AUDIO_LOG_MODULE, "Invalid number of channels: %u", config->num_channels);
        return NULL;
    }

    // Allocate cortex structure
    audio_cortex_t* cortex = (audio_cortex_t*)nimcp_calloc(1, sizeof(audio_cortex_t));
    if (!cortex) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(audio_cortex_t),
            "Failed to allocate audio cortex structure");
        LOG_ERROR(AUDIO_LOG_MODULE, "Failed to allocate audio cortex");
        return NULL;
    }

    // Copy configuration
    memcpy(&cortex->config, config, sizeof(audio_cortex_config_t));

    // Initialize neuromodulation
    cortex->brain = NULL;

    // Initialize phasic/tonic neuromodulator states using proper API
    // ACh: Fast phasic bursts for attention (decay ~500ms)
    cortex->ach_state = phasic_tonic_state_create();
    phasic_tonic_set_phasic_burst(&cortex->ach_state, 0.0F);
    phasic_tonic_set_tonic_level(&cortex->ach_state, 0.3F);  // Baseline attention

    // NE: Moderate phasic bursts for arousal (decay ~1s)
    cortex->ne_state = phasic_tonic_state_create();
    phasic_tonic_set_phasic_burst(&cortex->ne_state, 0.0F);
    phasic_tonic_set_tonic_level(&cortex->ne_state, 0.3F);  // Baseline arousal

    // Initialize receptor expression (auditory cortex typical values)
    // ACh receptors: Using M2 density for frequency selectivity
    cortex->receptors.m1_density = 0.4F;
    cortex->receptors.m2_density = 0.7F;  // Strong tuning sharpening

    // NE receptors: Balanced α1/β2 for onset detection
    cortex->receptors.alpha1_density = 0.6F;  // Onset enhancement
    cortex->receptors.beta2_density = 0.5F;   // Temporal plasticity

    // Allocate FFT buffers
    cortex->fft_real = (float*)nimcp_calloc(config->frame_size, sizeof(float));
    cortex->fft_imag = (float*)nimcp_calloc(config->frame_size, sizeof(float));
    cortex->fft_window = (float*)nimcp_calloc(config->frame_size, sizeof(float));

    if (!nimcp_validate_pointer(cortex->fft_real, "fft_real") ||
        !nimcp_validate_pointer(cortex->fft_imag, "fft_imag") ||
        !nimcp_validate_pointer(cortex->fft_window, "fft_window")) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, config->frame_size * sizeof(float) * 3,
            "Failed to allocate FFT buffers for frame size %u", config->frame_size);
        LOG_ERROR(AUDIO_LOG_MODULE, "Failed to allocate FFT buffers");
        audio_cortex_destroy(cortex);
        return NULL;
    }

    // Initialize Hamming window
    compute_hamming_window(cortex->fft_window, config->frame_size);

    // Initialize mel filterbank
    if (!init_mel_filterbank(cortex)) {
        audio_cortex_destroy(cortex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "audio_cortex_create: init_mel_filterbank is NULL");
        return NULL;
    }

    // Allocate temporal processing buffers
    cortex->prev_frame = (float*)nimcp_calloc(config->frame_size, sizeof(float));
    if (!nimcp_validate_pointer(cortex->prev_frame, "prev_frame")) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, config->frame_size * sizeof(float),
            "Failed to allocate temporal processing buffer for frame size %u", config->frame_size);
        LOG_ERROR(AUDIO_LOG_MODULE, "Failed to allocate temporal processing buffer");
        audio_cortex_destroy(cortex);
        return NULL;
    }

    // Initialize memory system
    if (config->enable_memory) {
        cortex->memory_capacity = AUDIO_MAX_MEMORIES;
        cortex->memories = (auditory_memory_t**)nimcp_calloc(
            cortex->memory_capacity, sizeof(auditory_memory_t*)
        );
        if (!nimcp_validate_pointer(cortex->memories, "memories")) {
            NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, cortex->memory_capacity * sizeof(auditory_memory_t*),
                "Failed to allocate auditory memory array (capacity=%zu)", cortex->memory_capacity);
            LOG_ERROR(AUDIO_LOG_MODULE, "Failed to allocate auditory memory array");
            audio_cortex_destroy(cortex);
            return NULL;
        }

        // === Initialize Memory Pool for Auditory Memories ===
        // WHAT: Create O(1) allocation pool for auditory_memory_t
        // WHY:  audio_cortex_store_memory() is hot path; avoid malloc overhead
        // HOW:  Pre-allocate for max capacity
        memory_pool_config_t pool_config = memory_pool_default_config(
            sizeof(auditory_memory_t),
            cortex->memory_capacity
        );
        pool_config.alignment = 16;  // 16-byte alignment for cache efficiency
        pool_config.enable_tracking = true;
        cortex->memory_pool = memory_pool_create(&pool_config);
        if (!cortex->memory_pool) {
            LOG_WARN(AUDIO_LOG_MODULE, "Failed to create auditory memory pool, will use malloc fallback");
            // Non-fatal: continue without pool (falls back to malloc)
        }
    }

    // Initialize memory pool mutex for thread safety
    cortex->memory_pool_mutex = nimcp_malloc(sizeof(nimcp_mutex_t));
    if (cortex->memory_pool_mutex) {
        nimcp_mutex_init(cortex->memory_pool_mutex, NULL);
    } else {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(nimcp_mutex_t),
            "Failed to allocate memory pool mutex for audio cortex");
        LOG_ERROR(AUDIO_LOG_MODULE, "Failed to allocate memory pool mutex");
        audio_cortex_destroy(cortex);
        return NULL;
    }

    // === Initialize Copy-on-Write Fields ===
    cortex->_cow_refcount = NULL;
    cortex->_cow_is_shallow = false;

    // NIMCP 2.7 Phase 8.5: Fractal Topology Integration (Future Enhancement)
    // TODO: Add internal recurrent network with scale-free topology
    //
    // WHAT: Internal spiking network for recurrent A1 processing
    // WHY:  Enable temporal integration, tonotopic refinement, and auditory scene analysis
    // HOW:  Create neural_network with config->internal_neurons neurons,
    //       apply topology_generate_scale_free() with configured parameters
    //
    // BIOLOGICAL MOTIVATION:
    // While A1's tonotopic organization (FFT → mel filters) is innate, recurrent
    // connections within A1 and feedback from higher auditory areas exhibit
    // scale-free properties. This internal network models:
    // - Lateral inhibition for frequency competition
    // - Temporal binding for coincidence detection
    // - Feedback for auditory attention and prediction
    // - Streaming and segregation of sound sources
    //
    cortex->internal_network = NULL;
    cortex->has_internal_network = false;

    if (config->enable_fractal_topology && config->internal_neurons > 0) {
        // Create internal recurrent network
        network_config_t net_config = {
            .num_neurons = config->internal_neurons,
            .ei_ratio = 0.8F,  // 80% excitatory (typical cortex)
            .learning_rate = 0.001F,
            .hebbian_rate = 0.01F,
            .stdp_window = 20.0F,
            .homeostatic_rate = 0.001F,
            .target_activity = 0.1F,
            .adaptation_rate = 0.01F,
            .refractory_period = 2.0F,
            .min_weight = 0.0F,
            .max_weight = 1.0F,
            .update_interval = 1,
            .enable_stdp = true,
            .enable_homeostasis = true,
            .neuron_model = NEURON_MODEL_LIF,
            .model_params = NULL,
            .integration_method = ODE_EULER
        };

        cortex->internal_network = neural_network_create(&net_config);

        if (cortex->internal_network) {
            // Generate scale-free topology with tonotopic spatial constraint
            scale_free_config_t topo_config = {
                .power_law_gamma = config->power_law_gamma,
                .hub_ratio = config->hub_ratio,
                .min_degree = 2,
                .max_degree = config->internal_neurons / 10,
                .spatial_constraint = 0.7F,  // Strong tonotopic organization
                .bidirectional = false
            };

            topology_stats_t stats;
            if (topology_generate_scale_free(cortex->internal_network, &topo_config, &stats)) {
                cortex->has_internal_network = true;
                LOG_INFO(AUDIO_LOG_MODULE, "A1 internal network: %u neurons, %u synapses, %.2f avg degree",
                         stats.num_neurons, stats.num_synapses, stats.avg_degree);
            } else {
                LOG_WARN(AUDIO_LOG_MODULE, "Failed to generate A1 topology, using network without topology");
                cortex->has_internal_network = true;  // Network exists, just without topology
            }
        } else {
            LOG_WARN(AUDIO_LOG_MODULE, "Failed to create A1 internal network");
        }
    }

    // === Bio-Async Registration ===
    cortex->bio_ctx = NULL;
    cortex->bio_async_enabled = false;

    if (config->enable_bio_async && bio_router_is_initialized()) {
        bio_module_info_t bio_info = {
            .module_id = BIO_MODULE_AUDIO_CORTEX,
            .module_name = "audio_cortex",
            .inbox_capacity = 64,
            .user_data = cortex
        };

        cortex->bio_ctx = bio_router_register_module(&bio_info);
        if (cortex->bio_ctx) {
            cortex->bio_async_enabled = true;
            LOG_INFO(AUDIO_LOG_MODULE, "Bio-async registered for audio cortex (module_id=%d)",
                     BIO_MODULE_AUDIO_CORTEX);
        } else {
            LOG_WARN(AUDIO_LOG_MODULE, "Failed to register bio-async for audio cortex");
        }
    }

    // === Second Messenger Cascade System ===
    cortex->second_messengers = NULL;
    cortex->second_messengers_enabled = false;

    if (config->enable_second_messengers) {
        // WHAT: Initialize second messenger system for frequency neurons
        // WHY:  Enable neuromodulator-driven cascades for audio processing modulation
        // HOW:  Create system with num_mel_filters neurons (one per frequency band)
        uint32_t num_neurons = config->num_mel_filters;

        second_messenger_config_t sm_config = second_messenger_default_config();
        sm_config.enable_bio_async = config->enable_bio_async;
        sm_config.enable_security = true;

        cortex->second_messengers = second_messenger_create(num_neurons, &sm_config);
        if (cortex->second_messengers) {
            cortex->second_messengers_enabled = true;
            LOG_INFO(AUDIO_LOG_MODULE, "Second messenger system initialized for %u frequency neurons",
                     num_neurons);

            // Note: Security integration enabled via sm_config.enable_security
            // BBB registration is handled internally by the second messenger system
        } else {
            LOG_WARN(AUDIO_LOG_MODULE, "Failed to create second messenger system");
        }
    }

    // === Training Interface Initialization ===
    cortex->training_mode = false;
    cortex->cached_mel_features = NULL;
    cortex->cached_mel_size = 0;
    cortex->cached_mfcc_features = NULL;
    cortex->cached_mfcc_size = 0;
    cortex->last_quality = 0.0F;
    cortex->last_speech_salience = 0.0F;
    cortex->last_process_timestamp = 0;

    return cortex;
}

void audio_cortex_destroy(audio_cortex_t* cortex)
{
    if (!cortex) return;

    // === Second Messenger System Cleanup ===
    if (cortex->second_messengers) {
        second_messenger_destroy(cortex->second_messengers);
        cortex->second_messengers = NULL;
        cortex->second_messengers_enabled = false;
        LOG_DEBUG(AUDIO_LOG_MODULE, "Second messenger system destroyed");
    }

    // === Bio-Async Unregistration ===
    if (cortex->bio_async_enabled && cortex->bio_ctx) {
        bio_router_unregister_module(cortex->bio_ctx);
        cortex->bio_ctx = NULL;
        cortex->bio_async_enabled = false;
        LOG_DEBUG(AUDIO_LOG_MODULE, "Bio-async unregistered for audio cortex");
    }

    // === Handle Copy-on-Write Reference Counting ===
    // If this is a shallow copy, decrement refcount
    if (cortex->_cow_refcount) {
        // Load current count and attempt to decrement atomically
        uint32_t old_count = __atomic_load_n(cortex->_cow_refcount, __ATOMIC_SEQ_CST);
        do {
            if (old_count == 0) {
                // Already freed by another thread - just free our handle
                nimcp_free(cortex);
                return;
            }
        } while (!__atomic_compare_exchange_n(cortex->_cow_refcount, &old_count, old_count - 1,
                                              false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST));

        if (old_count > 1) {
            // Other references still exist - just free our handle
            nimcp_free(cortex);
            return;
        }
        // We were the last reference (old_count == 1) - free the refcount and continue cleanup
        nimcp_free(cortex->_cow_refcount);
    }

    // Free FFT buffers
    nimcp_free(cortex->fft_real);
    nimcp_free(cortex->fft_imag);
    nimcp_free(cortex->fft_window);
    nimcp_free(cortex->mel_filterbank);

    // Free temporal buffers
    nimcp_free(cortex->prev_frame);

    // Free memories (with mutex protection for pool operations)
    if (cortex->memories) {
        for (uint32_t i = 0; i < cortex->num_memories; i++) {
            if (cortex->memories[i]) {
                nimcp_free(cortex->memories[i]->features);
                // Thread-safe check and release using mutex
                if (cortex->memory_pool_mutex) {
                    nimcp_mutex_lock(cortex->memory_pool_mutex);
                }
                if (cortex->memory_pool && memory_pool_owns(cortex->memory_pool, cortex->memories[i])) {
                    memory_pool_release(cortex->memory_pool, cortex->memories[i]);
                } else {
                    nimcp_free(cortex->memories[i]);
                }
                if (cortex->memory_pool_mutex) {
                    nimcp_mutex_unlock(cortex->memory_pool_mutex);
                }
            }
        }
        nimcp_free(cortex->memories);
    }

    // === Destroy Memory Pool ===
    if (cortex->memory_pool) {
        memory_pool_destroy(cortex->memory_pool);
        cortex->memory_pool = NULL;
    }

    // === Destroy Memory Pool Mutex ===
    if (cortex->memory_pool_mutex) {
        nimcp_mutex_destroy(cortex->memory_pool_mutex);

    }

    // NIMCP 2.7 Phase 8.5: Destroy internal recurrent network
    if (cortex->internal_network) {
        neural_network_destroy(cortex->internal_network);
    }

    // === Training Cache Cleanup ===
    if (cortex->cached_mel_features) {
        nimcp_free(cortex->cached_mel_features);
        cortex->cached_mel_features = NULL;
    }
    if (cortex->cached_mfcc_features) {
        nimcp_free(cortex->cached_mfcc_features);
        cortex->cached_mfcc_features = NULL;
    }

    nimcp_free(cortex);
}

bool audio_cortex_process(
    audio_cortex_t* cortex,
    const float* audio_data,
    uint32_t num_samples,
    uint8_t num_channels,
    float* features)
{
    if (!nimcp_validate_pointer(cortex, "cortex") ||
        !nimcp_validate_pointer(audio_data, "audio_data") ||
        !nimcp_validate_pointer(features, "features")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "audio_cortex_process: nimcp_validate_pointer is NULL");
        return false;
    }

    // BBB: Validate external audio input (SECURITY CRITICAL)
    // This is external sensory data that could be adversarial/corrupted
    if (!bbb_check_pointer(audio_data, "audio_cortex_process")) {
        bbb_audit_log(BBB_AUDIT_WARNING, AUDIO_LOG_MODULE, "invalid_audio_ptr",
                      "NULL audio pointer rejected");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "audio_cortex_process: bbb_check_pointer is NULL");
        return false;
    }

    // BBB: Validate audio parameters are within safe bounds
    const uint32_t MAX_SAMPLES = 48000 * 60;  // Max 1 minute of audio at 48kHz
    const uint8_t MAX_CHANNELS = 8;           // 7.1 surround max
    if (!bbb_validate_range_u(num_samples, 1, MAX_SAMPLES, "audio_cortex_process") ||
        !bbb_validate_range_u(num_channels, 1, MAX_CHANNELS, "audio_cortex_process")) {
        bbb_audit_log(BBB_AUDIT_WARNING, AUDIO_LOG_MODULE, "invalid_audio_params",
                      "samples=%u channels=%u rejected", num_samples, num_channels);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "audio_cortex_process: bbb_validate_range_u is NULL");
        return false;
    }

    // BBB: Validate buffer size doesn't overflow
    uint64_t expected_size = (uint64_t)num_samples * num_channels * sizeof(float);
    if (expected_size > (uint64_t)UINT32_MAX) {
        bbb_audit_log(BBB_AUDIT_WARNING, AUDIO_LOG_MODULE, "size_overflow",
                      "Audio buffer size %llu exceeds maximum", (unsigned long long)expected_size);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "audio_cortex_process: validation failed");
        return false;
    }

    // BBB: Check for NaN/Inf in first few samples (adversarial detection)
    for (uint32_t i = 0; i < (num_samples < 16 ? num_samples : 16); i++) {
        if (!isfinite(audio_data[i])) {
            bbb_audit_log(BBB_AUDIT_WARNING, AUDIO_LOG_MODULE, "invalid_audio_data",
                          "NaN/Inf detected at sample %u", i);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "audio_cortex_process: isfinite is NULL");
            return false;
        }
    }

    if (num_samples != cortex->config.frame_size) {
        LOG_ERROR(AUDIO_LOG_MODULE, "Invalid frame size: %u (expected %u)", num_samples, cortex->config.frame_size);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "audio_cortex_process: validation failed");
        return false;
    }

    if (num_channels != cortex->config.num_channels) {
        LOG_ERROR(AUDIO_LOG_MODULE, "Invalid number of channels: %u (expected %u)", num_channels, cortex->config.num_channels);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "audio_cortex_process: validation failed");
        return false;
    }

    // Update neuromodulator states (assuming ~30ms frame @ 16kHz = 512 samples)
    float frame_duration = (float)num_samples / (float)cortex->config.sample_rate;
    update_neuromodulator_states(cortex, frame_duration);

    // === Update Second Messenger Cascade Dynamics ===
    // WHAT: Update cascade kinetics for all frequency neurons
    // WHY:  Cascades evolve over time (seconds to minutes timescale)
    // HOW:  Call update with frame duration converted to milliseconds
    if (cortex->second_messengers_enabled && cortex->second_messengers) {
        float dt_ms = frame_duration * 1000.0F;
        uint64_t timestamp_ms = (uint64_t)(time(NULL) * 1000);
        second_messenger_update(cortex->second_messengers, dt_ms, timestamp_ms);
    }

    // Convert to mono if stereo
    float* mono_audio = (float*)audio_data;
    float* temp_mono = NULL;

    if (num_channels == 2) {
        temp_mono = (float*)nimcp_calloc(num_samples, sizeof(float));
        if (!nimcp_validate_pointer(temp_mono, "temp_mono")) {
            LOG_ERROR(AUDIO_LOG_MODULE, "Failed to allocate mono conversion buffer");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "audio_cortex_process: nimcp_validate_pointer is NULL");
            return false;
        }

        for (uint32_t i = 0; i < num_samples; i++) {
            temp_mono[i] = (audio_data[i * 2] + audio_data[i * 2 + 1]) * 0.5F;
        }
        mono_audio = temp_mono;
    }

    // Compute power spectrum
    float* spectrum = (float*)nimcp_calloc(cortex->config.num_freq_bins, sizeof(float));
    if (!nimcp_validate_pointer(spectrum, "spectrum")) {
        LOG_ERROR(AUDIO_LOG_MODULE, "Failed to allocate spectrum buffer");
        nimcp_free(temp_mono);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "audio_cortex_process: nimcp_validate_pointer is NULL");
        return false;
    }

    bool success = audio_cortex_compute_spectrum(cortex, mono_audio, num_samples, spectrum);

    if (success) {
        // Compute mel features
        float* mel_features = (float*)nimcp_calloc(cortex->config.num_mel_filters, sizeof(float));
        if (mel_features) {
            success = audio_cortex_compute_mel_features(
                cortex, spectrum, cortex->config.num_freq_bins, mel_features
            );

            if (success) {
                // Compute MFCC
                success = audio_cortex_compute_mfcc(
                    cortex, mel_features, cortex->config.num_mel_filters, features
                );
            }

            nimcp_free(mel_features);
        } else {
            success = false;
        }
    }

    nimcp_free(spectrum);
    nimcp_free(temp_mono);

    if (success) {
        cortex->stats.frames_processed++;
    }

    return success;
}

bool audio_cortex_get_stats(
    const audio_cortex_t* cortex,
    audio_cortex_stats_t* stats)
{
    if (!nimcp_validate_pointer(cortex, "cortex") ||
        !nimcp_validate_pointer(stats, "stats")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "audio_cortex_get_stats: nimcp_validate_pointer is NULL");
        return false;
    }

    memcpy(stats, &cortex->stats, sizeof(audio_cortex_stats_t));
    return true;
}

//=============================================================================
// Cochlear Processing Implementation
//=============================================================================

bool audio_cortex_compute_spectrum(
    audio_cortex_t* cortex,
    const float* audio_data,
    uint32_t num_samples,
    float* spectrum)
{
    if (!nimcp_validate_pointer(cortex, "cortex") ||
        !nimcp_validate_pointer(audio_data, "audio_data") ||
        !nimcp_validate_pointer(spectrum, "spectrum")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "audio_cortex_compute_spectrum: nimcp_validate_pointer is NULL");
        return false;
    }

    if (num_samples != cortex->config.frame_size) {
        LOG_ERROR(AUDIO_LOG_MODULE, "Invalid frame size: %u (expected %u)", num_samples, cortex->config.frame_size);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "audio_cortex_compute_spectrum: validation failed");
        return false;
    }

    // Apply window and prepare for FFT
    for (uint32_t i = 0; i < num_samples; i++) {
        cortex->fft_real[i] = audio_data[i] * cortex->fft_window[i];
        cortex->fft_imag[i] = 0.0F;
    }

    // Compute FFT
    fft(cortex->fft_real, cortex->fft_imag, num_samples, false);

    // Compute power spectrum (magnitude squared)
    uint32_t num_bins = cortex->config.num_freq_bins;
    for (uint32_t i = 0; i < num_bins; i++) {
        float re = cortex->fft_real[i];
        float im = cortex->fft_imag[i];
        spectrum[i] = re * re + im * im;
    }

    // Apply neuromodulator filter (ACh + 5-HT modulation)
    // TODO: Implement get_audio_filter() to get neuromodulator filtering
    float audio_filter = 1.0F;  // Default: no filtering
    if (audio_filter != 1.0F) {
        for (uint32_t i = 0; i < num_bins; i++) {
            spectrum[i] *= audio_filter;
        }
    }

    return true;
}

bool audio_cortex_compute_mel_features(
    audio_cortex_t* cortex,
    const float* spectrum,
    uint32_t num_bins,
    float* mel_features)
{
    if (!nimcp_validate_pointer(cortex, "cortex") ||
        !nimcp_validate_pointer(spectrum, "spectrum") ||
        !nimcp_validate_pointer(mel_features, "mel_features")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "audio_cortex_compute_mel_features: nimcp_validate_pointer is NULL");
        return false;
    }

    if (num_bins != cortex->config.num_freq_bins) {
        LOG_ERROR(AUDIO_LOG_MODULE, "Invalid number of bins: %u (expected %u)", num_bins, cortex->config.num_freq_bins);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "audio_cortex_compute_mel_features: validation failed");
        return false;
    }

    // Apply mel filterbank
    uint32_t num_filters = cortex->config.num_mel_filters;
    for (uint32_t m = 0; m < num_filters; m++) {
        float sum = 0.0F;
        for (uint32_t k = 0; k < num_bins; k++) {
            sum += spectrum[k] * cortex->mel_filterbank[m * num_bins + k];
        }

        // Store linear energy (tests expect positive values)
        // Add small epsilon to avoid zero
        mel_features[m] = sum + 1e-10F;

        // === Apply Second Messenger Cascade Modulation ===
        // WHAT: Modulate frequency band gain by kinase activities
        // WHY:  PKA/PKC/CaMKII affect neuronal excitability and tuning
        // HOW:  Query cascade state and apply as multiplicative gain
        if (cortex->second_messengers_enabled && cortex->second_messengers) {
            second_messenger_state_t state;
            nimcp_result_t result = second_messenger_get_state(
                cortex->second_messengers,
                m,  // neuron_id = frequency bin index
                &state
            );

            if (result == NIMCP_SUCCESS) {
                // Integrate kinase activities
                float pka = state.camp.pka_activity;
                float pkc = state.ip3_dag.pkc_activity;
                float camkii = state.calcium.camkii_activity;

                // BIOLOGY:
                // - PKA enhances frequency tuning (D1 dopamine pathway)
                // - PKC enhances selectivity (ACh M4, 5-HT2A pathways)
                // - CaMKII enhances temporal precision
                //
                // Modulation range: [0.5, 1.5] where 1.0 = baseline
                float cascade_gain = 1.0F + (pka * 0.3F) + (pkc * 0.4F) + (camkii * 0.2F) - 0.45F;
                cascade_gain = fmaxf(0.5F, fminf(cascade_gain, 1.5F));

                mel_features[m] *= cascade_gain;
            }
        }
    }

    // Apply cocktail party effect - ACh sharpens frequency tuning
    float ach_strength = get_effective_ach(&cortex->ach_state, &cortex->receptors);
    if (ach_strength > 0.01F) {  // Only apply if significant ACh
        apply_cocktail_party_effect(mel_features, num_filters, ach_strength);
    }

    return true;
}

bool audio_cortex_compute_mfcc(
    audio_cortex_t* cortex,
    const float* mel_features,
    uint32_t num_mel,
    float* mfcc)
{
    if (!nimcp_validate_pointer(cortex, "cortex") ||
        !nimcp_validate_pointer(mel_features, "mel_features") ||
        !nimcp_validate_pointer(mfcc, "mfcc")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "audio_cortex_compute_mfcc: nimcp_validate_pointer is NULL");
        return false;
    }

    if (num_mel != cortex->config.num_mel_filters) {
        LOG_ERROR(AUDIO_LOG_MODULE, "Invalid number of mel filters: %u (expected %u)", num_mel, cortex->config.num_mel_filters);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "audio_cortex_compute_mfcc: validation failed");
        return false;
    }

    // Apply DCT to mel features
    dct(mel_features, mfcc, num_mel, cortex->config.num_mfcc);

    return true;
}

//=============================================================================
// Attention Mechanisms Implementation
//=============================================================================

audio_attention_map_t* audio_attention_map_create(
    uint32_t num_freq,
    uint32_t num_time)
{
    if (num_freq == 0 || num_time == 0) {
        LOG_ERROR(AUDIO_LOG_MODULE, "Invalid audio attention map dimensions: %u x %u", num_freq, num_time);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "audio_attention_map_create: num_freq is zero");
        return NULL;
    }

    audio_attention_map_t* map = (audio_attention_map_t*)nimcp_calloc(
        1, sizeof(audio_attention_map_t)
    );
    if (!nimcp_validate_pointer(map, "map")) {
        LOG_ERROR(AUDIO_LOG_MODULE, "Failed to allocate audio attention map");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "audio_attention_map_create: nimcp_validate_pointer is NULL");
        return NULL;
    }

    map->num_freq = num_freq;
    map->num_time = num_time;
    map->values = (float*)nimcp_calloc(num_freq * num_time, sizeof(float));

    if (!nimcp_validate_pointer(map->values, "map->values")) {
        LOG_ERROR(AUDIO_LOG_MODULE, "Failed to allocate audio attention map values");
        nimcp_free(map);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "audio_attention_map_create: nimcp_validate_pointer is NULL");
        return NULL;
    }

    return map;
}

void audio_attention_map_destroy(audio_attention_map_t* map)
{
    if (!map) return;

    nimcp_free(map->values);
    nimcp_free(map);
}

bool audio_cortex_compute_attention(
    audio_cortex_t* cortex,
    const float* audio_data,
    uint32_t num_samples,
    audio_attention_map_t* attn_map)
{
    if (!nimcp_validate_pointer(cortex, "cortex") ||
        !nimcp_validate_pointer(audio_data, "audio_data") ||
        !nimcp_validate_pointer(attn_map, "attn_map")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "audio_cortex_compute_attention: nimcp_validate_pointer is NULL");
        return false;
    }

    // Compute spectrum - handle variable length audio by using first frame
    float* spectrum = (float*)nimcp_calloc(cortex->config.num_freq_bins, sizeof(float));
    if (!nimcp_validate_pointer(spectrum, "spectrum")) {
        LOG_ERROR(AUDIO_LOG_MODULE, "Failed to allocate spectrum buffer for attention");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "audio_cortex_compute_attention: nimcp_validate_pointer is NULL");
        return false;
    }

    // Process audio in chunks of frame_size
    uint32_t frame_size = cortex->config.frame_size;
    uint32_t samples_to_process = (num_samples < frame_size) ? num_samples : frame_size;

    bool success = audio_cortex_compute_spectrum(cortex, audio_data, samples_to_process, spectrum);

    if (success) {
        // Normalize spectrum to attention weights
        float max_val = 0.0F;
        for (uint32_t i = 0; i < cortex->config.num_freq_bins; i++) {
            if (spectrum[i] > max_val) max_val = spectrum[i];
        }

        if (max_val > 0.0F) {
            for (uint32_t i = 0; i < cortex->config.num_freq_bins && i < attn_map->num_freq; i++) {
                // Simple attention: high energy frequencies get high attention
                attn_map->values[i] = spectrum[i] / max_val;
            }
        }
    }

    nimcp_free(spectrum);
    return success;
}

//=============================================================================
// Auditory Memory Implementation
//=============================================================================

bool audio_cortex_store_memory(
    audio_cortex_t* cortex,
    const float* features,
    float salience)
{
    if (!nimcp_validate_pointer(cortex, "cortex") ||
        !nimcp_validate_pointer(features, "features")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "audio_cortex_store_memory: nimcp_validate_pointer is NULL");
        return false;
    }

    if (!cortex->config.enable_memory) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "audio_cortex_store_memory: cortex->config is NULL");
        return false;
    }

    // Check capacity
    if (cortex->num_memories >= cortex->memory_capacity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "audio_cortex_store_memory: capacity exceeded");
        return false;  // Memory full
    }

    // === Allocate Memory Entry from Pool (O(1)) or Fallback to calloc ===
    auditory_memory_t* memory = NULL;
    if (cortex->memory_pool) {
        if (cortex->memory_pool_mutex) {
            nimcp_mutex_lock(cortex->memory_pool_mutex);
        }
        memory = (auditory_memory_t*)memory_pool_acquire(cortex->memory_pool);
        if (cortex->memory_pool_mutex) {
            nimcp_mutex_unlock(cortex->memory_pool_mutex);
        }
        if (memory) {
            memset(memory, 0, sizeof(auditory_memory_t));  // Pool doesn't zero memory
        }
    }
    if (!memory) {
        // Pool exhausted or doesn't exist - fallback to calloc
        memory = (auditory_memory_t*)nimcp_calloc(1, sizeof(auditory_memory_t));
    }
    if (!nimcp_validate_pointer(memory, "memory")) {
        LOG_ERROR(AUDIO_LOG_MODULE, "Failed to allocate auditory memory entry");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "audio_cortex_store_memory: nimcp_validate_pointer is NULL");
        return false;
    }

    memory->features = (float*)nimcp_calloc(cortex->config.feature_dim, sizeof(float));
    if (!nimcp_validate_pointer(memory->features, "memory->features")) {
        LOG_ERROR(AUDIO_LOG_MODULE, "Failed to allocate auditory memory features");
        // Thread-safe check and release using mutex
        if (cortex->memory_pool_mutex) {
            nimcp_mutex_lock(cortex->memory_pool_mutex);
        }
        if (cortex->memory_pool && memory_pool_owns(cortex->memory_pool, memory)) {
            memory_pool_release(cortex->memory_pool, memory);
        } else {
            nimcp_free(memory);
        }
        if (cortex->memory_pool_mutex) {
            nimcp_mutex_unlock(cortex->memory_pool_mutex);
        }
        return false;
    }

    memcpy(memory->features, features, cortex->config.feature_dim * sizeof(float));
    memory->salience = salience;
    memory->timestamp = (uint64_t)time(NULL);
    memory->context[0] = '\0';

    cortex->memories[cortex->num_memories++] = memory;
    cortex->stats.memories_stored = cortex->num_memories;

    return true;
}

bool audio_cortex_recall_memory(
    audio_cortex_t* cortex,
    const float* query_features,
    int top_k,
    auditory_memory_t*** memories,
    int* num_recalled)
{
    if (!nimcp_validate_pointer(cortex, "cortex") ||
        !nimcp_validate_pointer(query_features, "query_features") ||
        !nimcp_validate_pointer(memories, "memories") ||
        !nimcp_validate_pointer(num_recalled, "num_recalled")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "audio_cortex_recall_memory: nimcp_validate_pointer is NULL");
        return false;
    }

    *memories = NULL;
    *num_recalled = 0;

    if (!cortex->config.enable_memory || cortex->num_memories == 0) {
        return true;  // No memories to recall
    }

    // Compute similarities
    typedef struct {
        auditory_memory_t* memory;
        float similarity;
    } memory_score_t;

    memory_score_t* scores = (memory_score_t*)nimcp_calloc(
        cortex->num_memories, sizeof(memory_score_t)
    );
    if (!nimcp_validate_pointer(scores, "scores")) {
        LOG_ERROR(AUDIO_LOG_MODULE, "Failed to allocate memory score buffer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "audio_cortex_recall_memory: nimcp_validate_pointer is NULL");
        return false;
    }

    for (uint32_t i = 0; i < cortex->num_memories; i++) {
        float dot_product = 0.0F;
        for (uint32_t j = 0; j < cortex->config.feature_dim; j++) {
            dot_product += query_features[j] * cortex->memories[i]->features[j];
        }
        scores[i].memory = cortex->memories[i];
        scores[i].similarity = dot_product;
    }

    // Sort by similarity (simple bubble sort for small arrays)
    for (uint32_t i = 0; i < cortex->num_memories - 1; i++) {
        for (uint32_t j = 0; j < cortex->num_memories - i - 1; j++) {
            if (scores[j].similarity < scores[j + 1].similarity) {
                memory_score_t temp = scores[j];
                scores[j] = scores[j + 1];
                scores[j + 1] = temp;
            }
        }
    }

    // Return top-k
    int k = (top_k < (int)cortex->num_memories) ? top_k : (int)cortex->num_memories;
    *memories = (auditory_memory_t**)nimcp_calloc(k, sizeof(auditory_memory_t*));
    if (!nimcp_validate_pointer(*memories, "result_memories")) {
        LOG_ERROR(AUDIO_LOG_MODULE, "Failed to allocate memory results array");
        nimcp_free(scores);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "audio_cortex_recall_memory: nimcp_validate_pointer is NULL");
        return false;
    }

    for (int i = 0; i < k; i++) {
        (*memories)[i] = scores[i].memory;
    }
    *num_recalled = k;

    nimcp_free(scores);
    return true;
}

//=============================================================================
// Brain Integration Helpers Implementation
//=============================================================================

float audio_cortex_compute_novelty(
    audio_cortex_t* cortex,
    const float* features)
{
    if (!nimcp_validate_pointer(cortex, "cortex") ||
        !nimcp_validate_pointer(features, "features")) {
        return 0.0F;
    }

    if (!cortex->config.enable_memory || cortex->num_memories == 0) {
        return 1.0F;  // Everything is novel with no memories
    }

    // Normalize query features
    /* Use tensor library for query norm computation */
    float query_norm = 0.0F;
    {
        uint32_t dims[] = {cortex->config.feature_dim};
        nimcp_tensor_t* query_tensor = nimcp_tensor_from_data(features, dims, 1, NIMCP_DTYPE_F32, false);
        if (query_tensor) {
            query_norm = (float)nimcp_tensor_norm_p(query_tensor, 2.0);
            nimcp_tensor_destroy(query_tensor);
        } else {
            /* Fallback to scalar */
            for (uint32_t j = 0; j < cortex->config.feature_dim; j++) {
                query_norm += features[j] * features[j];
            }
            query_norm = sqrtf(query_norm);
        }
    }
    if (query_norm < 1e-6F) {
        return 1.0F;  // Zero features are maximally novel
    }

    // Find maximum cosine similarity to any stored memory
    float max_similarity = 0.0F;
    for (uint32_t i = 0; i < cortex->num_memories; i++) {
        // Compute normalized dot product (cosine similarity)
        float dot_product = 0.0F;
        float memory_norm = 0.0F;
        for (uint32_t j = 0; j < cortex->config.feature_dim; j++) {
            dot_product += features[j] * cortex->memories[i]->features[j];
            memory_norm += cortex->memories[i]->features[j] * cortex->memories[i]->features[j];
        }
        memory_norm = sqrtf(memory_norm);

        if (memory_norm > 1e-6F) {
            float cosine_sim = dot_product / (query_norm * memory_norm);
            if (cosine_sim > max_similarity) {
                max_similarity = cosine_sim;
            }
        }
    }

    // Novelty = 1 - max_similarity (cosine similarity ranges from -1 to 1)
    // Clamp similarity to [0, 1] range first
    if (max_similarity < 0.0F) max_similarity = 0.0F;
    if (max_similarity > 1.0F) max_similarity = 1.0F;

    float novelty = 1.0F - max_similarity;
    return novelty;
}

bool audio_cortex_get_attention_peak(
    const audio_attention_map_t* attn_map,
    uint32_t* max_freq,
    uint32_t* max_time,
    float* max_value)
{
    if (!nimcp_validate_pointer(attn_map, "attn_map") ||
        !nimcp_validate_pointer(max_freq, "max_freq") ||
        !nimcp_validate_pointer(max_time, "max_time") ||
        !nimcp_validate_pointer(max_value, "max_value")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "audio_cortex_get_attention_peak: nimcp_validate_pointer is NULL");
        return false;
    }

    if (!nimcp_validate_pointer(attn_map->values, "attn_map->values")) {
        LOG_ERROR(AUDIO_LOG_MODULE, "Audio attention map has no values");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "audio_cortex_get_attention_peak: nimcp_validate_pointer is NULL");
        return false;
    }

    *max_value = -INFINITY;
    *max_freq = 0;
    *max_time = 0;

    for (uint32_t t = 0; t < attn_map->num_time; t++) {
        for (uint32_t f = 0; f < attn_map->num_freq; f++) {
            float value = attn_map->values[t * attn_map->num_freq + f];
            if (value > *max_value) {
                *max_value = value;
                *max_freq = f;
                *max_time = t;
            }
        }
    }

    return true;
}

bool audio_cortex_consolidate_memory(
    audio_cortex_t* cortex,
    const float* features,
    float salience,
    const char* context)
{
    if (!nimcp_validate_pointer(cortex, "cortex") ||
        !nimcp_validate_pointer(features, "features")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "audio_cortex_consolidate_memory: nimcp_validate_pointer is NULL");
        return false;
    }

    // Store memory
    if (!audio_cortex_store_memory(cortex, features, salience)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "audio_cortex_consolidate_memory: audio_cortex_store_memory is NULL");
        return false;
    }

    // Add context if provided
    if (context && cortex->num_memories > 0) {
        auditory_memory_t* last_memory = cortex->memories[cortex->num_memories - 1];
        strncpy(last_memory->context, context, sizeof(last_memory->context) - 1);
        last_memory->context[sizeof(last_memory->context) - 1] = '\0';
    }

    // TODO: Future hippocampus integration
    // TODO: Future knowledge graph integration

    return true;
}

//=============================================================================
// Temporal Processing Implementation
//=============================================================================

bool audio_cortex_detect_temporal_events(
    audio_cortex_t* cortex,
    const float* audio_data,
    uint32_t num_samples,
    bool* onset_detected,
    bool* offset_detected)
{
    if (!nimcp_validate_pointer(cortex, "cortex") ||
        !nimcp_validate_pointer(audio_data, "audio_data") ||
        !nimcp_validate_pointer(onset_detected, "onset_detected") ||
        !nimcp_validate_pointer(offset_detected, "offset_detected")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "audio_cortex_detect_temporal_events: nimcp_validate_pointer is NULL");
        return false;
    }

    *onset_detected = false;
    *offset_detected = false;

    // Compute current frame energy
    float energy = 0.0F;
    for (uint32_t i = 0; i < num_samples; i++) {
        energy += audio_data[i] * audio_data[i];
    }
    energy /= num_samples;

    // Get NE modulation strength
    float ne_strength = get_effective_ne(&cortex->ne_state, &cortex->receptors);

    // Base onset threshold (2x energy increase)
    float base_onset_threshold = 2.0F;

    // Modulate threshold with NE (higher NE = more sensitive)
    float onset_threshold = modulate_onset_threshold(base_onset_threshold, ne_strength);

    // Onset: sudden increase in energy
    if (energy > cortex->prev_energy * onset_threshold && cortex->prev_energy > 1e-6F) {
        *onset_detected = true;

        // Trigger NE phasic burst on onset detection (positive feedback)
        trigger_phasic_burst(&cortex->ne_state, 0.2F);
    }

    // Offset: sudden decrease in energy
    float base_offset_threshold = 0.5F;
    float offset_threshold = modulate_onset_threshold(base_offset_threshold, ne_strength * 0.7F);

    if (energy < cortex->prev_energy * offset_threshold && cortex->prev_energy > 1e-6F) {
        *offset_detected = true;
    }

    // Update state
    cortex->prev_energy = energy;
    memcpy(cortex->prev_frame, audio_data, num_samples * sizeof(float));

    return true;
}

bool audio_cortex_compute_envelope(
    audio_cortex_t* cortex,
    const float* audio_data,
    uint32_t num_samples,
    float* envelope)
{
    if (!nimcp_validate_pointer(cortex, "cortex") ||
        !nimcp_validate_pointer(audio_data, "audio_data") ||
        !nimcp_validate_pointer(envelope, "envelope")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "audio_cortex_compute_envelope: nimcp_validate_pointer is NULL");
        return false;
    }

    // Simple envelope: rectify and smooth
    float alpha = 0.1F;  // Smoothing factor
    float smoothed = 0.0F;

    for (uint32_t i = 0; i < num_samples; i++) {
        float rectified = fabsf(audio_data[i]);
        smoothed = alpha * rectified + (1.0F - alpha) * smoothed;
        envelope[i] = smoothed;
    }

    return true;
}

/**
 * @brief Update neuromodulator phasic/tonic states with decay
 *
 * WHAT: Apply temporal decay to phasic components
 * WHY:  Phasic bursts are transient and decay to tonic baseline
 * HOW:  Exponential decay with time-based updates
 *
 * @param cortex Audio cortex instance
 * @param dt Time step in seconds
 */
static void update_neuromodulator_states(audio_cortex_t* cortex, float dt)
{
    if (!cortex) return;

    // Sync tonic levels with brain's global neuromodulator system FIRST (before decay)
    // This allows sudden global changes to trigger phasic bursts
    if (cortex->brain) {
        neuromodulator_system_t neuromod = brain_get_neuromodulator_system(cortex->brain);
        if (neuromod) {
            // Read global levels
            float global_ach = neuromodulator_get_level(neuromod, NEUROMOD_ACETYLCHOLINE);
            float global_ne = neuromodulator_get_level(neuromod, NEUROMOD_NOREPINEPHRINE);

            // Detect sudden changes (trigger phasic burst)
            float ach_tonic = phasic_tonic_get_tonic_level(&cortex->ach_state);
            float ne_tonic = phasic_tonic_get_tonic_level(&cortex->ne_state);
            float ach_change = global_ach - ach_tonic;
            float ne_change = global_ne - ne_tonic;

            if (fabsf(ach_change) > 0.3F) {
                // Sudden change - trigger phasic burst
                trigger_phasic_burst(&cortex->ach_state, fabsf(ach_change));
            }
            if (fabsf(ne_change) > 0.3F) {
                trigger_phasic_burst(&cortex->ne_state, fabsf(ne_change));
            }

            // Slowly sync tonic to global (time constant ~5s)
            float sync_rate = 0.2F;  // 20% per second
            float new_ach_tonic = ach_tonic + ach_change * sync_rate * dt;
            float new_ne_tonic = ne_tonic + ne_change * sync_rate * dt;

            // Clamp to valid range and set
            new_ach_tonic = fminf(fmaxf(new_ach_tonic, 0.0F), 1.0F);
            new_ne_tonic = fminf(fmaxf(new_ne_tonic, 0.0F), 1.0F);
            phasic_tonic_set_tonic_level(&cortex->ach_state, new_ach_tonic);
            phasic_tonic_set_tonic_level(&cortex->ne_state, new_ne_tonic);
        }
    }

    // Update ACh and NE phasic/tonic states (decay phasic toward tonic)
    update_phasic_tonic(&cortex->ach_state, dt);
    update_phasic_tonic(&cortex->ne_state, dt);
}

/**
 * @brief Associate brain with audio cortex for neuromodulation
 *
 * WHAT: Set brain reference for ACh + NE modulation of auditory processing
 * WHY:  Enable neurochemical modulation (attention, arousal, temporal sensitivity)
 * HOW:  Store brain pointer for neurotransmitter reading
 *
 * BIOLOGY:
 * - Acetylcholine enhances auditory frequency selectivity (cocktail party effect)
 * - Norepinephrine enhances onset detection and temporal sensitivity
 *
 * @param cortex Audio cortex instance
 * @param brain Brain instance (or NULL to clear)
 */
void audio_cortex_set_brain(audio_cortex_t* cortex, brain_t brain)
{
    if (!cortex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "audio_cortex_set_brain: cortex is NULL");
        return;
    }
    cortex->brain = brain;

    // Initialize tonic levels from brain if available
    if (brain) {
        neuromodulator_system_t neuromod = brain_get_neuromodulator_system(brain);
        if (neuromod) {
            phasic_tonic_set_tonic_level(&cortex->ach_state, neuromodulator_get_level(neuromod, NEUROMOD_ACETYLCHOLINE));
            phasic_tonic_set_tonic_level(&cortex->ne_state, neuromodulator_get_level(neuromod, NEUROMOD_NOREPINEPHRINE));
        }
    }
}

//=============================================================================
// Bidirectional Feedback Functions (Phase 10.11.3)
//=============================================================================

/**
 * @brief Get speech salience from audio features
 *
 * WHAT: Query how speech-like current audio is
 * WHY:  Speech cortex can prioritize speech processing
 * HOW:  Return energy concentration in speech frequencies (300-3400 Hz)
 *
 * BIOLOGY: Superior temporal gyrus (STG) receives speech-tuned signals from A1
 *
 * COMPLEXITY: O(n) where n = num_features
 *
 * @param cortex Audio cortex instance
 * @param features Audio feature vector from recent processing
 * @param num_features Number of features
 * @return Speech salience [0, 1] (0=noise, 1=clear speech)
 */
float audio_cortex_get_speech_salience(audio_cortex_t* cortex,
                                        const float* features,
                                        uint32_t num_features)
{
    // Guard: Validate inputs
    if (!cortex || !features) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "audio_cortex_get_speech_salience: required parameter is NULL");
        return 0.0F;
    }
    if (num_features == 0) {
        return 0.0F;
    }

    // WHAT: Compute speech likelihood based on formant structure
    // WHY:  Speech has characteristic spectral peaks (formants) unlike noise
    // HOW:  Detect spectral peaks and structure typical of speech
    //
    // SPEECH CHARACTERISTICS:
    // - Distinct spectral peaks (formants) with energy concentrated in bands
    // - Moderate variance (structured but not random)
    // - Speech formants create non-uniform distribution

    // Compute statistics
    float mean = 0.0F;
    float total_energy = 0.0F;

    for (uint32_t i = 0; i < num_features; i++) {
        mean += features[i];
        total_energy += features[i] * features[i];
    }
    mean /= num_features;

    if (total_energy < 1e-6F) {
        return 0.0F;  // Silence
    }

    // Find peaks (speech has distinct formant peaks)
    uint32_t num_peaks = 0;
    float peak_energy = 0.0F;

    for (uint32_t i = 1; i < num_features - 1; i++) {
        // Is this a local maximum?
        if (features[i] > features[i-1] && features[i] > features[i+1]) {
            // Strong peak (above mean)?
            if (features[i] > mean * 1.5F) {
                num_peaks++;
                peak_energy += features[i];
            }
        }
    }

    // Speech typically has 2-4 formant peaks
    float peak_score = 0.0F;
    if (num_peaks >= 2 && num_peaks <= 6) {
        peak_score = 1.0F;  // Ideal formant count
    } else if (num_peaks == 1 || num_peaks == 7) {
        peak_score = 0.5F;  // Borderline
    }

    // Energy concentration in peaks (speech has concentrated energy)
    float concentration_score = 0.0F;
    if (total_energy > 0.0F) {
        concentration_score = peak_energy / total_energy;
        concentration_score = fminf(concentration_score * 1.5F, 1.0F);
    }

    // Compute variance (speech has moderate variance, noise is very high)
    float variance = 0.0F;
    for (uint32_t i = 0; i < num_features; i++) {
        float diff = features[i] - mean;
        variance += diff * diff;
    }
    variance /= num_features;

    // Coefficient of variation (CV = std/mean)
    float std_dev = sqrtf(variance);
    float cv = std_dev / (mean + 1e-6F);

    // Speech: moderate CV (0.5 - 1.5), Noise: high CV (> 2)
    float cv_score = 0.0F;
    if (cv > 0.3F && cv < 2.0F) {
        cv_score = 1.0F;
    } else if (cv >= 2.0F && cv < 3.0F) {
        cv_score = 0.3F;  // Noisy
    }

    // Combine indicators
    // Peak structure is most important for speech
    float salience = (peak_score * 0.5F + concentration_score * 0.3F + cv_score * 0.2F);

    // Clamp to [0, 1]
    return fminf(fmaxf(salience, 0.0F), 1.0F);
}

/**
 * @brief Activate speech processing mode
 *
 * WHAT: Signal that speech detected, optimize for phoneme extraction
 * WHY:  Speech detection triggers specialized processing
 * HOW:  Prime frequency bands and temporal resolution for speech
 *
 * COMPLEXITY: O(1)
 *
 * @param cortex Audio cortex instance
 */
void audio_cortex_activate_speech_mode(audio_cortex_t* cortex)
{
    // Guard: Validate cortex
    if (!cortex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "audio_cortex_activate_speech_mode: cortex is NULL");
        return;
    }

    // WHAT: Set flag for speech-optimized processing
    // WHY:  Speech requires different processing than music/noise
    // HOW:  Future enhancement would adjust:
    //       - FFT window size (shorter for speech: 20-30ms)
    //       - Frequency resolution (finer in 300-3400 Hz band)
    //       - Temporal resolution (faster for consonants)
    //
    // NOTE: In full implementation, this would:
    // 1. Adjust mel filterbank to emphasize speech frequencies
    // 2. Increase temporal resolution (smaller hop size)
    // 3. Boost ACh modulation for enhanced frequency selectivity
    //
    // For now, this is a placeholder that logs the activation.
    // The existing neuromodulator system already provides some speech optimization
    // via ACh enhancement of frequency selectivity.
}

//=============================================================================
// Bio-Async Communication Implementation
//=============================================================================

/**
 * @brief Get bio-async module context
 */
bio_module_context_t audio_cortex_get_bio_context(audio_cortex_t* cortex)
{
    if (!cortex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "audio_cortex_get_bio_context: cortex is NULL");
        return NULL;
    }
    if (!cortex->bio_async_enabled) {
        return NULL;
    }
    return cortex->bio_ctx;
}

/**
 * @brief Process pending bio-async messages
 *
 * Uses bio_router_process_inbox() which calls registered handlers.
 */
uint32_t audio_cortex_process_bio_messages(audio_cortex_t* cortex, uint32_t max_messages)
{
    if (!cortex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "audio_cortex_process_bio_messages: cortex is NULL");
        return 0;
    }
    if (!cortex->bio_async_enabled || !cortex->bio_ctx) {
        return 0;
    }

    // Process inbox using the router's handler-based system
    uint32_t processed = bio_router_process_inbox(cortex->bio_ctx, max_messages);

    if (processed > 0) {
        LOG_DEBUG(AUDIO_LOG_MODULE, "Processed %u bio-async messages", processed);
    }

    return processed;
}

/**
 * @brief Broadcast audio feature detection via bio-async
 */
nimcp_error_t audio_cortex_broadcast_input(
    audio_cortex_t* cortex,
    const float* features,
    uint32_t num_features,
    float salience)
{
    if (!cortex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "audio_cortex_broadcast_input: cortex is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (!cortex->bio_async_enabled || !cortex->bio_ctx) {
        return NIMCP_ERROR_NOT_INITIALIZED;
    }

    if (!features) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "audio_cortex_broadcast_input: features is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (num_features == 0) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    // Create audio feature detected message
    bio_msg_audio_feature_detected_t msg;
    bio_msg_init_header(&msg.header, BIO_MSG_AUDIO_FEATURE_DETECTED,
                        BIO_MODULE_AUDIO_CORTEX, 0,  // 0 = broadcast
                        sizeof(msg) - sizeof(bio_message_header_t));

    msg.header.channel = BIO_CHANNEL_ACETYLCHOLINE;  // Auditory processing involves ACh
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;

    // Fill in audio feature data
    msg.feature_id = 0;  // Generic audio input
    msg.frequency_hz = 1000.0F;  // Placeholder frequency
    msg.amplitude = salience;
    msg.onset_time_ms = 0.0F;
    msg.duration_ms = 20.0F;  // Typical frame duration
    msg.channel = 0;  // Mono/center

    // Broadcast to all interested modules
    nimcp_error_t err = bio_router_broadcast(cortex->bio_ctx, &msg, sizeof(msg));
    if (err != NIMCP_SUCCESS) {
        LOG_WARN(AUDIO_LOG_MODULE, "Failed to broadcast audio input: %d", err);
        return err;
    }

    LOG_DEBUG(AUDIO_LOG_MODULE, "Broadcast audio input: %u features, salience=%.2f",
              num_features, salience);

    return NIMCP_SUCCESS;
}

/**
 * @brief Broadcast speech detected notification
 */
nimcp_error_t audio_cortex_broadcast_speech_detected(
    audio_cortex_t* cortex,
    float speech_salience)
{
    if (!cortex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "audio_cortex_broadcast_speech_detected: cortex is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (!cortex->bio_async_enabled || !cortex->bio_ctx) {
        return NIMCP_ERROR_NOT_INITIALIZED;
    }

    // Create speech onset detected message using the proper type
    bio_msg_audio_feature_detected_t msg;
    bio_msg_init_header(&msg.header, BIO_MSG_SPEECH_ONSET_DETECTED,
                        BIO_MODULE_AUDIO_CORTEX, BIO_MODULE_SPEECH_CORTEX,
                        sizeof(msg) - sizeof(bio_message_header_t));

    msg.header.channel = BIO_CHANNEL_ACETYLCHOLINE;
    msg.header.flags = BIO_MSG_FLAG_URGENT;  // Speech is high priority

    msg.feature_id = 1;  // Speech onset feature
    msg.frequency_hz = 1650.0F;  // Center of speech band
    msg.amplitude = speech_salience;
    msg.onset_time_ms = 0.0F;
    msg.duration_ms = 0.0F;  // Unknown duration at onset
    msg.channel = 0;

    // Send to speech cortex
    nimcp_error_t err = bio_router_send(cortex->bio_ctx, &msg, sizeof(msg), 0);
    if (err != NIMCP_SUCCESS) {
        LOG_WARN(AUDIO_LOG_MODULE, "Failed to send speech detected: %d", err);
        return err;
    }

    LOG_DEBUG(AUDIO_LOG_MODULE, "Sent speech detected: salience=%.2f", speech_salience);

    return NIMCP_SUCCESS;
}

//=============================================================================
// Second Messenger Cascade Integration Implementation
//=============================================================================

/**
 * @brief Activate receptor cascade in audio cortex neuron
 *
 * WHAT: Trigger second messenger cascade via receptor activation
 * WHY:  Neuromodulators activate GPCRs -> cascades modulate audio processing
 * HOW:  Route to appropriate G-protein pathway (Gs/Gi/Gq)
 *
 * BIOLOGY:
 * - Dopamine D1 (Gs) -> cAMP -> PKA: Modulates frequency tuning
 * - Acetylcholine M4 (Gq) -> IP3/DAG -> PKC: Enhances frequency selectivity
 * - Serotonin 5-HT2A (Gq) -> IP3/DAG -> PKC: Gates auditory sensitivity
 *
 * COMPLEXITY: O(1)
 *
 * @param cortex Audio cortex instance
 * @param neuron_id Neuron ID (maps to frequency bin)
 * @param receptor Receptor type to activate
 * @param occupancy Receptor occupancy [0, 1]
 * @param timestamp_ms Current timestamp
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t audio_cortex_trigger_receptor(
    audio_cortex_t* cortex,
    uint32_t neuron_id,
    uint32_t receptor,
    float occupancy,
    uint64_t timestamp_ms)
{
    // Guard: Validate inputs
    if (!cortex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "audio_cortex_trigger_receptor: cortex is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (!cortex->second_messengers_enabled || !cortex->second_messengers) {
        return NIMCP_ERROR_NOT_INITIALIZED;
    }

    // Validate neuron_id is within range
    if (neuron_id >= cortex->config.num_mel_filters) {
        LOG_ERROR(AUDIO_LOG_MODULE, "Invalid neuron_id %u (max %u)",
                  neuron_id, cortex->config.num_mel_filters - 1);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    // Validate occupancy range
    if (occupancy < 0.0F || occupancy > 1.0F) {
        LOG_ERROR(AUDIO_LOG_MODULE, "Invalid receptor occupancy %.2f (must be [0, 1])",
                  occupancy);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    // WHAT: Map receptor type to G-protein coupling and activate cascade
    // WHY:  Different receptors trigger different intracellular pathways
    // HOW:  Use receptor_type_t enum to determine Gs/Gi/Gq coupling
    gpcr_coupling_t coupling = second_messenger_receptor_coupling((receptor_type_t)receptor);

    nimcp_result_t result = NIMCP_SUCCESS;

    switch (coupling) {
        case GPCR_GS_COUPLED:
            // D1 dopamine, beta-adrenergic -> cAMP -> PKA
            result = second_messenger_activate_gs(
                cortex->second_messengers,
                neuron_id,
                occupancy,
                timestamp_ms
            );
            break;

        case GPCR_GI_COUPLED:
            // D2 dopamine, alpha2-adrenergic -> inhibit cAMP
            result = second_messenger_activate_gi(
                cortex->second_messengers,
                neuron_id,
                occupancy,
                timestamp_ms
            );
            break;

        case GPCR_GQ_COUPLED:
            // 5-HT2A serotonin, M4 ACh, mGluR -> IP3/DAG -> PKC
            result = second_messenger_activate_gq(
                cortex->second_messengers,
                neuron_id,
                occupancy,
                timestamp_ms
            );
            break;

        default:
            LOG_WARN(AUDIO_LOG_MODULE, "Unknown receptor coupling type %d", coupling);
            return NIMCP_ERROR_INVALID_PARAM;
    }

    if (result != NIMCP_SUCCESS) {
        LOG_ERROR(AUDIO_LOG_MODULE, "Failed to activate cascade for neuron %u: %d",
                  neuron_id, result);
        return result;
    }

    LOG_DEBUG(AUDIO_LOG_MODULE, "Activated receptor %u on neuron %u (occupancy=%.2f)",
              receptor, neuron_id, occupancy);

    return NIMCP_SUCCESS;
}

/**
 * @brief Get second messenger cascade state for neuron
 *
 * WHAT: Query cascade state for specific frequency neuron
 * WHY:  Monitor kinase activities and modulation levels
 * HOW:  Return copy of cascade state from second messenger system
 *
 * COMPLEXITY: O(1)
 *
 * @param cortex Audio cortex instance
 * @param neuron_id Neuron ID (frequency bin index)
 * @param state Output state buffer (must be second_messenger_state_t*)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t audio_cortex_get_second_messenger_state(
    audio_cortex_t* cortex,
    uint32_t neuron_id,
    void* state)
{
    // Guard: Validate inputs
    if (!cortex || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "audio_cortex_get_second_messenger_state: required parameter is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (!cortex->second_messengers_enabled || !cortex->second_messengers) {
        return NIMCP_ERROR_NOT_INITIALIZED;
    }

    // Validate neuron_id is within range
    if (neuron_id >= cortex->config.num_mel_filters) {
        LOG_ERROR(AUDIO_LOG_MODULE, "Invalid neuron_id %u (max %u)",
                  neuron_id, cortex->config.num_mel_filters - 1);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    // WHAT: Query cascade state from second messenger system
    // WHY:  Expose cascade state for monitoring and debugging
    // HOW:  Call second_messenger_get_state() and copy to output
    second_messenger_state_t* output_state = (second_messenger_state_t*)state;

    nimcp_result_t result = second_messenger_get_state(
        cortex->second_messengers,
        neuron_id,
        output_state
    );

    if (result != NIMCP_SUCCESS) {
        LOG_ERROR(AUDIO_LOG_MODULE, "Failed to get cascade state for neuron %u: %d",
                  neuron_id, result);
        return result;
    }

    return NIMCP_SUCCESS;
}

//=============================================================================
// Training Interface Implementation (CNN-Cortex Integration)
//=============================================================================

/**
 * @brief Get training state from audio cortex
 *
 * WHAT: Retrieve cached mel/MFCC features and quality metrics
 * WHY:  CNN trainer needs access to cortex activations for gradient feedback
 * HOW:  Copy cached outputs to provided state structure
 *
 * BIOLOGY: Top-down feedback in auditory system modulates A1 via attention
 * and prediction error signals (predictive coding).
 *
 * @param cortex Audio cortex instance
 * @param state Output training state structure
 * @return 0 on success, negative on error
 */
int audio_cortex_get_training_state(
    audio_cortex_t* cortex,
    audio_training_state_t* state)
{
    /* Guard: Validate inputs */
    if (!cortex || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "audio_cortex_get_training_state: required parameter is NULL");
        return -1;
    }

    /* Initialize state */
    memset(state, 0, sizeof(audio_training_state_t));

    /* Copy cached mel features if available */
    if (cortex->cached_mel_features && cortex->cached_mel_size > 0) {
        state->mel_features = cortex->cached_mel_features;
        state->num_mel_filters = cortex->cached_mel_size;
    }

    /* Copy cached MFCC features if available */
    if (cortex->cached_mfcc_features && cortex->cached_mfcc_size > 0) {
        state->mfcc_features = cortex->cached_mfcc_features;
        state->num_mfcc = cortex->cached_mfcc_size;
    }

    /* Copy metrics */
    state->quality = cortex->last_quality;
    state->speech_salience = cortex->last_speech_salience;
    state->timestamp_ms = cortex->last_process_timestamp;
    state->valid = (cortex->cached_mel_features != NULL);

    LOG_DEBUG(AUDIO_LOG_MODULE, "Training state retrieved: mel=%u, mfcc=%u, quality=%.2f",
              state->num_mel_filters, state->num_mfcc, state->quality);

    return 0;
}

/**
 * @brief Apply gradient feedback to audio cortex for STDP modulation
 *
 * WHAT: Convert gradients from CNN to STDP signals for cortex plasticity
 * WHY:  Enable top-down learning modulation in auditory system
 * HOW:  Modulate internal network using scaled gradient signal
 *
 * BIOLOGY: Top-down prediction errors in predictive coding framework
 * modulate synaptic plasticity in auditory cortex.
 *
 * @param cortex Audio cortex instance
 * @param gradients Gradient vector from CNN backprop
 * @param gradient_size Size of gradient vector
 * @param scale Scale factor for gradient signal
 * @return 0 on success, negative on error
 */
int audio_cortex_apply_gradient_feedback(
    audio_cortex_t* cortex,
    const float* gradients,
    uint32_t gradient_size,
    float scale)
{
    /* Guard: Validate inputs */
    if (!cortex || !gradients) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "audio_cortex_apply_gradient_feedback: required parameter is NULL");
        return -1;
    }
    if (gradient_size == 0) {
        return -1;
    }

    /* Guard: Training mode must be enabled */
    if (!cortex->training_mode) {
        LOG_WARN(AUDIO_LOG_MODULE, "Gradient feedback rejected: training mode not enabled");
        return -2;
    }

    /* Guard: Scale must be reasonable */
    if (scale < 0.0F || scale > 10.0F) {
        LOG_WARN(AUDIO_LOG_MODULE, "Gradient scale out of range: %.2f", scale);
        return -3;
    }

    /* Compute gradient magnitude for STDP modulation strength */
    float grad_magnitude = 0.0F;
    for (uint32_t i = 0; i < gradient_size; i++) {
        grad_magnitude += gradients[i] * gradients[i];
    }
    grad_magnitude = sqrtf(grad_magnitude) * scale;

    /* Clamp modulation strength to reasonable range */
    if (grad_magnitude > 1.0F) {
        grad_magnitude = 1.0F;
    }

    /* Apply to internal recurrent network if available */
    if (cortex->has_internal_network && cortex->internal_network) {
        /* STDP modulation: Use gradient magnitude as acetylcholine-like signal
         * High gradients = high prediction error = enhanced attention */

        /* Apply neuromodulator boost to internal network */
        neuromodulator_system_t neuromod = brain_get_neuromodulator_system(cortex->brain);
        if (neuromod) {
            /* Boost ACh based on gradient (attention signal) */
            float current_ach = neuromodulator_get_level(neuromod, NEUROMOD_ACETYLCHOLINE);
            float boosted_ach = current_ach + grad_magnitude * 0.4F;
            if (boosted_ach > 1.0F) boosted_ach = 1.0F;

            neuromodulator_set_level(neuromod, NEUROMOD_ACETYLCHOLINE, boosted_ach);

            LOG_DEBUG(AUDIO_LOG_MODULE, "Applied gradient feedback: mag=%.4f, ACh: %.2f -> %.2f",
                      grad_magnitude, current_ach, boosted_ach);
        }
    }

    /* Broadcast gradient feedback via bio-async if enabled */
    if (cortex->bio_async_enabled && cortex->bio_ctx) {
        bio_msg_audio_feature_detected_t msg;
        bio_msg_init_header(&msg.header, BIO_MSG_AUDIO_FEATURE_DETECTED,
                            BIO_MODULE_AUDIO_CORTEX, 0,
                            sizeof(msg) - sizeof(bio_message_header_t));

        msg.header.channel = BIO_CHANNEL_ACETYLCHOLINE;  /* Attention signal */
        msg.header.flags = BIO_MSG_FLAG_BROADCAST;
        msg.feature_id = 0xFFFF;  /* Special ID for gradient feedback */
        msg.amplitude = grad_magnitude;
        msg.frequency_hz = 0.0F;  /* N/A for gradient feedback */
        msg.onset_time_ms = 0.0F;
        msg.duration_ms = 0.0F;
        msg.channel = 0;

        bio_router_broadcast(cortex->bio_ctx, &msg, sizeof(msg));
    }

    return 0;
}

/**
 * @brief Extract audio features as a tensor
 *
 * WHAT: Process audio through A1 and return features as nimcp_tensor_t
 * WHY:  CNN trainer expects tensor format for batch processing
 * HOW:  Call audio_cortex_process, wrap output in tensor
 *
 * @param cortex Audio cortex instance
 * @param audio Input audio samples
 * @param num_samples Number of audio samples
 * @param num_channels Number of audio channels
 * @param features Output tensor pointer (caller must destroy)
 * @return 0 on success, negative on error
 */
int audio_cortex_extract_features_tensor(
    audio_cortex_t* cortex,
    const float* audio,
    uint32_t num_samples,
    uint8_t num_channels,
    struct nimcp_tensor** features)
{
    /* Guard: Validate inputs */
    if (!cortex || !audio || !features) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "audio_cortex_extract_features_tensor: required parameter is NULL");
        return -1;
    }
    if (num_samples == 0) {
        return -1;
    }

    /* Guard: Validate channels */
    if (num_channels == 0 || num_channels > 2) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "audio_cortex_extract_features_tensor: num_channels is zero");
        return -1;
    }

    /* Calculate feature dimension */
    uint32_t feature_dim = cortex->config.num_mel_filters + cortex->config.num_mfcc;

    /* Allocate temporary feature buffer */
    float* feature_buffer = (float*)nimcp_calloc(feature_dim, sizeof(float));
    if (!feature_buffer) {
        LOG_ERROR(AUDIO_LOG_MODULE, "Failed to allocate feature buffer");
        return -2;
    }

    /* Process audio through audio cortex */
    bool success = audio_cortex_process(cortex, audio, num_samples, num_channels, feature_buffer);
    if (!success) {
        nimcp_free(feature_buffer);
        return -3;
    }

    /* Cache outputs if in training mode */
    if (cortex->training_mode) {
        /* Compute quality as signal energy normalized */
        float energy = 0.0F;
        for (uint32_t i = 0; i < num_samples; i++) {
            energy += audio[i] * audio[i];
        }
        energy = sqrtf(energy / (float)num_samples);

        /* Quality: reasonable energy level (not too quiet, not clipping) */
        cortex->last_quality = fminf(1.0F, energy * 10.0F);
        if (energy > 0.9F) {
            cortex->last_quality = 0.5F;  /* Likely clipping */
        }

        /* Speech salience: estimate from feature characteristics */
        float mean = 0.0F;
        for (uint32_t i = 0; i < feature_dim; i++) {
            mean += feature_buffer[i];
        }
        mean /= (float)feature_dim;

        float variance = 0.0F;
        for (uint32_t i = 0; i < feature_dim; i++) {
            float diff = feature_buffer[i] - mean;
            variance += diff * diff;
        }
        variance /= (float)feature_dim;

        /* Speech has characteristic spectral patterns (high variance in MFCCs) */
        cortex->last_speech_salience = fminf(1.0F, sqrtf(variance) * 3.0F);

        /* Get current timestamp */
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        cortex->last_process_timestamp = (uint64_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
    }

    /* Create 1D tensor with feature dimension */
    uint32_t dims[1] = { feature_dim };
    nimcp_tensor_t* tensor = nimcp_tensor_create(dims, 1, NIMCP_DTYPE_F32);
    if (!tensor) {
        nimcp_free(feature_buffer);
        LOG_ERROR(AUDIO_LOG_MODULE, "Failed to create feature tensor");
        return -4;
    }

    /* Copy features to tensor */
    float* tensor_data = (float*)nimcp_tensor_data(tensor);
    memcpy(tensor_data, feature_buffer, feature_dim * sizeof(float));

    nimcp_free(feature_buffer);

    *features = (struct nimcp_tensor*)tensor;

    LOG_DEBUG(AUDIO_LOG_MODULE, "Extracted %u features as tensor", feature_dim);

    return 0;
}

/**
 * @brief Get audio cortex output feature dimension
 *
 * WHAT: Return the output feature vector size
 * WHY:  CNN trainer needs to know input dimensions for layer sizing
 * HOW:  Return mel_filters + mfcc count
 *
 * @param cortex Audio cortex instance
 * @return Feature dimension, or 0 on error
 */
uint32_t audio_cortex_get_feature_dim(const audio_cortex_t* cortex)
{
    if (!cortex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "audio_cortex_get_feature_dim: cortex is NULL");
        return 0;
    }
    return cortex->config.num_mel_filters + cortex->config.num_mfcc;
}

/**
 * @brief Enable or disable training mode
 *
 * WHAT: Toggle training mode for activation caching
 * WHY:  Training requires cached outputs for gradient computation
 * HOW:  Set flag that enables caching during audio_cortex_process
 *
 * @param cortex Audio cortex instance
 * @param enable True to enable training mode
 * @return 0 on success, negative on error
 */
int audio_cortex_set_training_mode(audio_cortex_t* cortex, bool enable)
{
    if (!cortex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cortex is NULL");

        return -1;
    }

    bool was_enabled = cortex->training_mode;
    cortex->training_mode = enable;

    if (enable && !was_enabled) {
        LOG_INFO(AUDIO_LOG_MODULE, "Training mode ENABLED - activation caching active");
    } else if (!enable && was_enabled) {
        /* Clear cached outputs when disabling */
        if (cortex->cached_mel_features) {
            nimcp_free(cortex->cached_mel_features);
            cortex->cached_mel_features = NULL;
            cortex->cached_mel_size = 0;
        }
        if (cortex->cached_mfcc_features) {
            nimcp_free(cortex->cached_mfcc_features);
            cortex->cached_mfcc_features = NULL;
            cortex->cached_mfcc_size = 0;
        }
        LOG_INFO(AUDIO_LOG_MODULE, "Training mode DISABLED - caches cleared");
    }

    return 0;
}

/**
 * @brief Check if training mode is enabled
 *
 * WHAT: Query training mode state
 * WHY:  Bridge needs to know if cortex is ready for gradient feedback
 * HOW:  Return training_mode flag
 *
 * @param cortex Audio cortex instance
 * @return True if training mode is enabled
 */
bool audio_cortex_is_training_mode(const audio_cortex_t* cortex)
{
    if (!cortex) {
        return false;
    }
    return cortex->training_mode;
}

//=============================================================================
// Self-Awareness (KG Reader Integration)
//=============================================================================

/**
 * @brief Query self-knowledge from knowledge graph
 *
 * WHAT: Allows audio cortex to introspect its own capabilities and connections
 * WHY:  Enables self-awareness - "what am I and what can I do?"
 * HOW:  Query KG for Audio_Cortex entity and its relations
 *
 * @param kg Knowledge graph reader (must be loaded)
 * @return 1 if self-knowledge found, 0 otherwise
 */
int audio_cortex_query_self_knowledge(kg_reader_t* kg)
{
    if (!kg) {
        return 0;
    }

    /* Query self-identity from KG */
    const kg_entity_t* self = kg_reader_get_entity(kg, "Audio_Cortex");
    if (self) {
        /* Module now has access to its documented capabilities */
        LOG_DEBUG(AUDIO_LOG_MODULE, "Self-knowledge: entity_type=%s, observations=%u",
                  self->entity_type, self->num_observations);
    }

    /* Query what this module connects to (outputs) */
    kg_relation_list_t* outputs = kg_reader_get_relations_from(kg, "Audio_Cortex");
    if (outputs) {
        LOG_DEBUG(AUDIO_LOG_MODULE, "Self-knowledge: %u downstream targets",
                  outputs->count);
        kg_relation_list_destroy(outputs);
    }

    /* Query what connects to this module (inputs) */
    kg_relation_list_t* inputs = kg_reader_get_relations_to(kg, "Audio_Cortex");
    if (inputs) {
        LOG_DEBUG(AUDIO_LOG_MODULE, "Self-knowledge: %u upstream sources",
                  inputs->count);
        kg_relation_list_destroy(inputs);
    }

    return self ? 1 : 0;
}
