//=============================================================================
// nimcp_brain_biological.c - Biological Subsystems Module Implementation
//=============================================================================
/**
 * @file nimcp_brain_biological.c
 * @brief Implementation of biological subsystem initialization functions
 *
 * EXTRACTED FROM: src/core/brain/nimcp_brain.c (lines 1347-1831)
 * PURPOSE: Reduce nimcp_brain.c complexity (~1500 lines extracted)
 *
 * This module handles initialization of:
 * - Glial integration with astrocytes
 * - Multimodal sensory integration (visual, audio, speech)
 * - Neuromodulator systems (dopamine, serotonin, etc.)
 * - Pink noise neuromodulation
 * - Spatial neuromodulation with quantum walk diffusion
 *
 * @author NIMCP Development Team
 * @version 2.7.0
 * @date 2025-11-19
 */

#include "core/brain/biological/nimcp_brain_biological.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include <stdio.h>
#include <stdlib.h>

// Bio-async integration
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

// Logging integration
#include "utils/logging/nimcp_logging.h"

// Unified memory integration
#include "utils/memory/nimcp_unified_memory.h"

// Required includes from original nimcp_brain.c
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "plasticity/adaptive/nimcp_adaptive.h"
#include "glial/integration/nimcp_glial_integration.h"
#include "core/integration/nimcp_multimodal_integration.h"
#include "perception/nimcp_visual_cortex.h"
#include "perception/nimcp_audio_cortex.h"
#include "perception/nimcp_speech_cortex.h"
#include "nlp/nimcp_nlp.h"
#include "plasticity/neuromodulators/nimcp_neuromod_pink_noise.h"
#include "plasticity/neuromodulators/nimcp_neuromodulators.h"
#include "plasticity/neuromodulators/nimcp_spatial_neuromod.h"
#include "utils/memory/nimcp_memory_guards.h"  // For nimcp_calloc/nimcp_free
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "BRAIN_BIOLOGICAL"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_dimension_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(brain_biological, MESH_ADAPTER_CATEGORY_COGNITIVE)


// External error function declaration
extern void set_error(const char* format, ...);

//=============================================================================
// Function Implementations
//=============================================================================

/**
 * WHAT: Initialize glial integration subsystem
 * WHY:  Enable astrocytes to support neural computation and neuromodulation
 * HOW:  Create glial integration with reasonable mapping capacity
 *
 * BIOLOGICAL MOTIVATION:
 * - Astrocytes: ~10x more numerous than neurons
 * - Metabolic support: Supply glucose and lactate to active neurons
 * - Neuromodulation: Regulate synaptic transmission via gliotransmitters
 * - Information processing: Bidirectional signaling with neurons
 */
bool init_glial_subsystem(brain_t brain)
{
    LOG_DEBUG("Initializing glial subsystem");

    if (!brain || !brain->network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "init_glial_subsystem: brain or network is NULL");
        LOG_ERROR("Invalid brain or network handle");
        return false;
    }

    // Check if already initialized (prevent double initialization)
    if (brain->glial) {
        LOG_DEBUG("Glial subsystem already initialized");
        return true;  // Already initialized
    }

    // Check if glial integration is enabled
    if (!brain->config.enable_glial) {
        LOG_INFO("Glial integration disabled");
        return true;  // Disabled, not an error
    }

    LOG_INFO("Creating glial integration with 1000 max mappings");

    // Get base network for glial integration
    neural_network_t base = adaptive_network_get_base_network(brain->network);
    if (!base) {
        set_error("Failed to get base network for glial integration");
        LOG_ERROR("Failed to get base network for glial integration");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "init_glial_subsystem: base is NULL");
        return false;
    }

    // Create glial integration with reasonable max_mappings
    brain->glial = glial_integration_create(base, 1000);

    if (!brain->glial) {
        set_error("Failed to create glial integration");
        LOG_ERROR("Failed to create glial integration");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "init_glial_subsystem: brain->glial is NULL");
        return false;
    }

    LOG_INFO("Glial subsystem initialized successfully");
    return true;
}

/**
 * WHAT: Initialize multimodal subsystems
 * WHY:  Enable processing of visual, audio, and speech inputs integrated together
 * HOW:  Create visual, audio, speech cortices and multimodal integration layer
 *
 * BIOLOGICAL MOTIVATION:
 * - Visual cortex (V1): 6-layered structure with orientation selectivity
 * - Auditory cortex (A1): Tonotopic organization from low to high frequencies
 * - Speech areas: Wernicke (comprehension) and Broca (production)
 * - STS (Superior Temporal Sulcus): Integrates visual-audio information
 * - Multisensory integration: Faster perception and better disambiguation
 *
 * FRACTAL TOPOLOGY (Phase 8.5):
 * - Hub neurons: High-degree connector nodes (15% of neurons)
 * - Power-law connectivity: Scale-free network properties
 * - V1 columnar structure: 10 neurons per filter
 * - A1 tonotopic structure: 10 neurons per mel filter
 * - Speech network: 10 neurons per phoneme
 */
bool init_multimodal_subsystems(brain_t brain)
{
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "init_multimodal_subsystems: brain is NULL");
        return false;
    }

    // Check if multi-modal processing is enabled
    if (!brain->config.enable_multimodal_integration) {
        // Even when multimodal is disabled, we still need integrated_feature_buffer
        // as a working buffer for direct-only predictions
        if (!brain->integrated_feature_buffer) {
            brain->integrated_feature_buffer = nimcp_calloc(brain->config.num_inputs, sizeof(float));
            if (!brain->integrated_feature_buffer) {
                set_error("Failed to allocate integrated feature buffer for direct predictions");
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "init_multimodal_subsystems: brain->integrated_feature_buffer is NULL");
                return false;
            }
        }
        return true;
    }

    // If multimodal is enabled, check if already fully initialized
    // We check for visual/audio/speech cortices only if they should be enabled
    bool visual_needed = brain->config.enable_visual_cortex && brain->config.visual_feature_dim > 0;
    bool audio_needed = brain->config.enable_audio_cortex && brain->config.audio_feature_dim > 0;
    bool speech_needed = brain->config.enable_speech_cortex && brain->config.speech_feature_dim > 0;

    bool visual_ready = !visual_needed || brain->visual_cortex;
    bool audio_ready = !audio_needed || brain->audio_cortex;
    bool speech_ready = !speech_needed || brain->speech_cortex;

    // If all needed components are ready and multimodal layer exists, we're done
    if (brain->multimodal && visual_ready && audio_ready && speech_ready) {
        return true;  // Already fully initialized
    }

    // Initialize visual cortex (if enabled)
    if (brain->config.enable_visual_cortex && brain->config.visual_feature_dim > 0) {
        visual_cortex_config_t visual_config = {
            .input_width = 640,        // Default camera resolution
            .input_height = 480,
            .num_v1_filters = 32,      // 32 orientation-selective filters
            .feature_dim = brain->config.visual_feature_dim,
            .enable_attention = true,
            .enable_memory = true,

            // NIMCP 2.7 Phase 8.5: Fractal Topology Integration
            .enable_fractal_topology = brain->config.enable_fractal_topology,
            .hub_ratio = 0.15F,        // 15% hub neurons (biological cortex ratio)
            .power_law_gamma = -2.1F,  // Cortical power-law exponent
            .internal_neurons = 32 * 10 // 10 neurons per filter (V1 columnar structure)
        };

        brain->visual_cortex = visual_cortex_create(&visual_config);
        if (!brain->visual_cortex) {
            set_error("Failed to create visual cortex");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "init_multimodal_subsystems: brain->visual_cortex is NULL");
            return false;
        }

        // Allocate visual feature buffer
        brain->visual_feature_buffer = nimcp_calloc(brain->config.visual_feature_dim, sizeof(float));
        if (!brain->visual_feature_buffer) {
            set_error("Failed to allocate visual feature buffer");
            visual_cortex_destroy(brain->visual_cortex);
            brain->visual_cortex = NULL;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "init_multimodal_subsystems: brain->visual_feature_buffer is NULL");
            return false;
        }
    }

    // Initialize audio cortex (if enabled)
    if (brain->config.enable_audio_cortex && brain->config.audio_feature_dim > 0) {
        audio_cortex_config_t audio_config = {
            .sample_rate = 16000,      // Default 16kHz audio
            .frame_size = 512,         // 32ms frames at 16kHz
            .num_freq_bins = 256,
            .num_mel_filters = 40,     // Standard for speech
            .num_mfcc = brain->config.audio_feature_dim,
            .num_channels = 1,         // Mono by default
            .feature_dim = brain->config.audio_feature_dim,
            .enable_attention = true,
            .enable_memory = true,

            // NIMCP 2.7 Phase 8.5: Fractal Topology Integration
            .enable_fractal_topology = brain->config.enable_fractal_topology,
            .hub_ratio = 0.15F,        // 15% hub neurons (biological A1 ratio)
            .power_law_gamma = -2.1F,  // Tonotopic power-law exponent
            .internal_neurons = 40 * 10 // 10 neurons per mel filter (A1 tonotopic structure)
        };

        brain->audio_cortex = audio_cortex_create(&audio_config);
        if (!brain->audio_cortex) {
            set_error("Failed to create audio cortex");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "init_multimodal_subsystems: brain->audio_cortex is NULL");
            return false;
        }

        // Allocate audio feature buffer
        brain->audio_feature_buffer = nimcp_calloc(brain->config.audio_feature_dim, sizeof(float));
        if (!brain->audio_feature_buffer) {
            set_error("Failed to allocate audio feature buffer");
            audio_cortex_destroy(brain->audio_cortex);
            brain->audio_cortex = NULL;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "init_multimodal_subsystems: brain->audio_feature_buffer is NULL");
            return false;
        }
    }

    // Initialize speech cortex (Phase 8.8)
    if (brain->config.enable_speech_cortex && brain->config.speech_feature_dim > 0) {
        speech_cortex_config_t speech_config = speech_cortex_default_config();

        // Override defaults with brain config
        speech_config.sample_rate = 16000;        // Standard speech rate
        speech_config.frame_size_ms = 20;         // 20ms frames for phoneme analysis
        speech_config.num_phonemes = SPEECH_NUM_PHONEMES; // 44 phonemes (English)
        speech_config.feature_dim = brain->config.speech_feature_dim;
        speech_config.enable_wernicke = true;     // Enable word recognition
        speech_config.enable_prosody = true;      // Enable pitch/stress analysis
        speech_config.enable_memory = true;       // Enable phonological working memory

        // NIMCP 2.7 Phase 8.5: Fractal Topology Integration
        speech_config.enable_fractal_topology = brain->config.enable_fractal_topology;
        speech_config.hub_ratio = 0.15F;          // 15% hub neurons (biological STG ratio)
        speech_config.power_law_gamma = -2.1F;    // Speech network power-law exponent
        speech_config.internal_neurons = SPEECH_NUM_PHONEMES * 10; // 10 neurons per phoneme

        brain->speech_cortex = speech_cortex_create(&speech_config);
        if (!brain->speech_cortex) {
            set_error("Failed to create speech cortex");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "init_multimodal_subsystems: brain->speech_cortex is NULL");
            return false;
        }

        // Allocate speech feature buffer
        brain->speech_feature_buffer = nimcp_calloc(brain->config.speech_feature_dim, sizeof(float));
        if (!brain->speech_feature_buffer) {
            set_error("Failed to allocate speech feature buffer");
            speech_cortex_destroy(brain->speech_cortex);
            brain->speech_cortex = NULL;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "init_multimodal_subsystems: brain->speech_feature_buffer is NULL");
            return false;
        }
    }

    // Initialize multi-modal integration layer
    uint32_t visual_dim = brain->config.enable_visual_cortex ? brain->config.visual_feature_dim : 0;
    uint32_t audio_dim = brain->config.enable_audio_cortex ? brain->config.audio_feature_dim : 0;
    uint32_t speech_dim = brain->config.enable_speech_cortex ? brain->config.speech_feature_dim : 0;
    // Direct dimension: Remaining space after visual, audio, and speech features
    uint32_t direct_dim = 0;
    if (brain->config.num_inputs > (visual_dim + audio_dim + speech_dim)) {
        direct_dim = brain->config.num_inputs - visual_dim - audio_dim - speech_dim;
    }

    if (visual_dim > 0 || audio_dim > 0 || speech_dim > 0 || direct_dim > 0) {
        // Phase 8.8: Speech is now a dedicated modality
        multimodal_config_t mm_config = multimodal_default_config(visual_dim, audio_dim, speech_dim, direct_dim);

        // Output dimension should match network input size
        mm_config.output_dim = brain->config.num_inputs;

        brain->multimodal = multimodal_integration_create(&mm_config);
        if (!brain->multimodal) {
            set_error("Failed to create multimodal integration layer");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "init_multimodal_subsystems: brain->multimodal is NULL");
            return false;
        }

        // Allocate integrated feature buffer
        brain->integrated_feature_buffer = nimcp_calloc(mm_config.output_dim, sizeof(float));
        if (!brain->integrated_feature_buffer) {
            set_error("Failed to allocate integrated feature buffer");
            multimodal_integration_destroy(brain->multimodal);
            brain->multimodal = NULL;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "init_multimodal_subsystems: brain->integrated_feature_buffer is NULL");
            return false;
        }
    }

    // Initialize NLP network (if multimodal or speech is enabled)
    if (brain->config.enable_multimodal_integration || brain->config.enable_speech_cortex) {

        // Configure NLP network with minimal config
        nlp_network_config_t nlp_config = {0};

        // NLP-specific parameters
        nlp_config.vocab_size = 10000;            // 10k token vocabulary
        nlp_config.embedding_dim = NIMCP_MEDIUM_EMBEDDING_DIM;           // 128-dim embeddings
        nlp_config.max_sequence_length = 32;      // 32 token context
        nlp_config.use_attention_synapses = true;
        nlp_config.use_neuromodulated_synapses = true;

        // Configure base network (required for neural_network_create)
        nlp_config.network_config.num_neurons = 256;  // Small NLP network
        nlp_config.network_config.input_size = nlp_config.embedding_dim;
        nlp_config.network_config.output_size = nlp_config.embedding_dim;
        nlp_config.network_config.enable_stdp = true;
        nlp_config.network_config.enable_hebbian = false;
        nlp_config.network_config.enable_oja = false;
        nlp_config.network_config.enable_homeostasis = false;
        nlp_config.network_config.learning_rate = 0.01F;

        // Configure attention (required for multihead_attention_create)
        nlp_config.attention_config.num_heads = brain->config.num_attention_heads > 0 ? brain->config.num_attention_heads : 4;
        nlp_config.attention_config.input_dim = nlp_config.embedding_dim;
        nlp_config.attention_config.output_dim = nlp_config.embedding_dim;
        nlp_config.attention_config.sequence_length = nlp_config.max_sequence_length;
        nlp_config.attention_config.use_thalamic_gate = false;
        nlp_config.attention_config.use_salience_weighting = false;
        nlp_config.attention_config.gate_bias = 0.5F;

        // Configure neuromodulators (required for neuromodulator_system_create)
        nlp_config.neuromod_config.baseline_dopamine = 0.2F;
        nlp_config.neuromod_config.baseline_serotonin = 0.2F;
        nlp_config.neuromod_config.baseline_acetylcholine = 0.2F;
        nlp_config.neuromod_config.baseline_norepinephrine = 0.2F;
        nlp_config.neuromod_config.dopamine_decay = 2.0F;
        nlp_config.neuromod_config.serotonin_decay = 10.0F;
        nlp_config.neuromod_config.acetylcholine_decay = 0.5F;
        nlp_config.neuromod_config.norepinephrine_decay = 3.0F;
        nlp_config.neuromod_config.reward_dopamine_gain = 0.5F;
        nlp_config.neuromod_config.threat_norepinephrine_gain = 0.7F;
        nlp_config.neuromod_config.salience_acetylcholine_gain = 0.6F;
        nlp_config.neuromod_config.punishment_serotonin_gain = 0.4F;
        nlp_config.neuromod_config.enable_volume_transmission = true;
        nlp_config.neuromod_config.diffusion_rate = 0.1F;

        brain->nlp_network = nlp_network_create(&nlp_config);
        if (!brain->nlp_network) {
            set_error("Failed to create NLP network");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: brain->nlp_network is NULL");
            return false;
        }
    }

    return true;
}

/**
 * WHAT: Initialize pink noise neuromodulation subsystem
 * WHY:  Enable 1/f noise-modulated dopamine/serotonin for exploration-exploitation balance
 * HOW:  Create pink noise neuromodulator if config flag is set
 *
 * BIOLOGICAL MOTIVATION:
 * - Dopamine neurons exhibit 1/f noise in firing patterns (Montague et al., 2004)
 * - Serotonin fluctuations follow pink spectrum (Cools et al., 2008)
 * - Multi-timescale correlations enable context-dependent learning
 *
 * INTEGRATION:
 * - Modulates learning rates via dopamine
 * - Scales attention via acetylcholine
 * - Enables exploration via pink noise
 *
 * @param brain Brain instance to initialize
 * @return true on success, false on failure
 *
 * @version 2.7.0 Phase 8.6
 * @author NIMCP Development Team
 * @date 2025-11-08
 */
bool init_pink_noise_subsystem(brain_t brain)
{
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "init_pink_noise_subsystem: brain is NULL");
        return false;
    }

    // Check if already initialized
    if (brain->pink_noise) {
        return true;  // Already initialized
    }

    // Check if pink noise is enabled
    if (!brain->config.enable_pink_noise) {
        return true;  // Not enabled, not an error
    }

    // Create pink noise neuromodulator with default configuration
    neuromod_pink_config_t pink_config = neuromod_pink_default_config();

    // Adjust baselines for brain-level processing
    pink_config.dopamine_baseline = 0.3F;      // Moderate baseline for learning
    pink_config.serotonin_baseline = 0.4F;     // Moderate baseline for stability
    pink_config.acetylcholine_baseline = 0.5F; // Moderate baseline for attention
    pink_config.norepinephrine_baseline = 0.2F;// Lower baseline for arousal

    // Configure noise amplitudes for exploration-exploitation balance
    pink_config.dopamine_noise_amplitude = 0.15F;      // 15% noise for exploration
    pink_config.serotonin_noise_amplitude = 0.08F;     // 8% noise for stability modulation
    pink_config.acetylcholine_noise_amplitude = 0.20F; // 20% noise for dynamic attention
    pink_config.norepinephrine_noise_amplitude = 0.10F;// 10% noise for arousal variation

    brain->pink_noise = neuromod_pink_create(&pink_config);
    if (!brain->pink_noise) {
        set_error("Failed to create pink noise neuromodulator");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "init_pink_noise_subsystem: brain->pink_noise is NULL");
        return false;
    }

    return true;
}

/**
 * WHAT: Initialize full neuromodulator system
 * WHY:  Enable mental health interventions to adjust neurotransmitter levels
 * HOW:  Create neuromodulator system with default configuration
 *
 * BIOLOGICAL MOTIVATION:
 * - Neurotransmitters regulate mood, attention, arousal, and learning
 * - Mental health disorders often involve chemical imbalances
 * - Interventions can modulate levels to restore healthy functioning
 *
 * PERSONALITY MAPPING (Phase 12):
 * - Dopamine (reward, motivation): Driven by Extraversion
 *   Extraverts seek social rewards → higher dopamine baseline
 * - Serotonin (mood stability, impulse control): Inverse of Neuroticism
 *   High neuroticism → low serotonin (anxiety, mood instability)
 * - Acetylcholine (attention, learning): Driven by Openness
 *   High openness → high acetylcholine (intellectual curiosity)
 * - Norepinephrine (arousal, vigilance): Driven by Conscientiousness
 *   High conscientiousness → sustained alertness
 */
bool init_neuromodulator_system(brain_t brain)
{
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "init_neuromodulator_system: brain is NULL");
        return false;
    }

    // Check if already initialized
    if (brain->neuromodulator_system) {
        return true;  // Already initialized
    }

    // Phase 12: Compute personality-modulated neuromodulator baselines
    // Default to moderate levels if no personality
    float dopamine_base = 0.5F;
    float serotonin_base = 0.5F;
    float acetylcholine_base = 0.5F;
    float norepinephrine_base = 0.5F;

    if (brain->personality) {
        // Map personality traits to neurotransmitter baselines
        personality_profile_t* p = brain->personality;

        // Dopamine (reward, motivation): Driven by Extraversion
        // Extraverts seek social rewards → higher dopamine baseline
        dopamine_base = 0.3F + p->traits.extraversion * 0.5F;  // [0.3, 0.8]

        // Serotonin (mood stability, impulse control): Inverse of Neuroticism
        // High neuroticism → low serotonin (anxiety, mood instability)
        serotonin_base = 0.7F - p->traits.neuroticism * 0.4F;  // [0.3, 0.7]

        // Acetylcholine (attention, learning): Driven by Openness
        // High openness → high acetylcholine (intellectual curiosity)
        acetylcholine_base = 0.3F + p->traits.openness * 0.5F;  // [0.3, 0.8]

        // Norepinephrine (arousal, vigilance): Driven by Conscientiousness
        // High conscientiousness → sustained alertness
        norepinephrine_base = 0.4F + p->traits.conscientiousness * 0.4F;  // [0.4, 0.8]
    }

    // Always create neuromodulator system (needed for mental health monitoring)
    // Configuration with personality-modulated baseline levels
    neuromodulator_config_t neuromod_config = {
        // Baseline concentrations (personality-modulated homeostatic set points)
        .baseline_dopamine = dopamine_base,          // Reward sensitivity
        .baseline_serotonin = serotonin_base,        // Mood/impulse control
        .baseline_acetylcholine = acetylcholine_base, // Attention
        .baseline_norepinephrine = norepinephrine_base, // Arousal

        // Decay time constants (seconds)
        .dopamine_decay = 2.0F,         // Fast decay (phasic DA bursts)
        .serotonin_decay = 10.0F,       // Slow decay (tonic 5-HT)
        .acetylcholine_decay = 0.5F,    // Very fast decay (attention bursts)
        .norepinephrine_decay = 3.0F,   // Moderate decay (arousal)

        // Response gains
        .reward_dopamine_gain = 0.5F,
        .threat_norepinephrine_gain = 0.7F,
        .salience_acetylcholine_gain = 0.6F,
        .punishment_serotonin_gain = 0.4F,

        // Volume transmission
        .enable_volume_transmission = true,
        .diffusion_rate = 0.1F
    };

    brain->neuromodulator_system = neuromodulator_system_create(&neuromod_config);
    if (!brain->neuromodulator_system) {
        set_error("Failed to create neuromodulator system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "init_neuromodulator_system: brain->neuromodulator_system is NULL");
        return false;
    }

    return true;
}

/**
 * WHAT: Initialize spatial neuromodulator system with quantum walk diffusion (Phase C2.1)
 * WHY:  Enable spatially-distributed neuromodulation with quantum speedup
 * HOW:  Delegate to factory implementation to avoid code duplication
 *
 * NOTE: This function exists for API compatibility with code that calls
 *       init_spatial_neuromod_system directly (e.g., brain_persistence.c).
 *       The actual implementation is in nimcp_brain_init.c.
 *
 * @param brain Brain instance to initialize
 * @return true on success, false on failure
 */
bool init_spatial_neuromod_system(brain_t brain)
{
    // Delegate to factory implementation (defined in nimcp_brain_init.c)
    extern bool nimcp_brain_factory_init_spatial_neuromod_system(brain_t brain);
    return nimcp_brain_factory_init_spatial_neuromod_system(brain);
}
