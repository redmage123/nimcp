//=============================================================================
// nimcp_brain_multimodal.c - Multimodal Processing and Integration
//=============================================================================
/**
 * @file nimcp_brain_multimodal.c
 * @brief Multimodal sensory processing and cognitive integration
 *
 * This module contains multimodal processing extracted from nimcp_brain.c:
 * - extract_sensory_features() - Visual/audio/speech feature extraction
 * - apply_attention_to_features() - Multihead attention for selective processing
 * - process_brain_regions() - Hierarchical brain regions processing
 * - integrate_multimodal_features() - Cross-modal integration
 * - process_neural_network() - Network processing with glial/oscillations
 * - apply_cognitive_processing() - Introspection/ethics/salience/curiosity
 * - consolidation_strengthen() - Memory consolidation
 * - format_output() - Decision formatting and explanation generation
 * - brain_process_multimodal() - Complete multimodal pipeline
 *
 * @version 1.0.0
 * @date 2025-12-08
 */

#include "core/brain/nimcp_brain_multimodal.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/nimcp_brain_lazy_init.h"
#include "core/brain/nimcp_brain_bio_async.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "BRAIN_MULTIMODAL"
#include "utils/time/nimcp_time.h"
#include "core/integration/nimcp_multimodal_integration.h"
#include "perception/nimcp_visual_cortex.h"
#include "perception/nimcp_audio_cortex.h"
#include "perception/nimcp_speech_cortex.h"
#include "language/nimcp_grounded_language.h"
#include "plasticity/attention/nimcp_attention.h"
#include "plasticity/adaptive/nimcp_adaptive.h"
#include "cognitive/introspection/nimcp_introspection.h"
#include "cognitive/ethics/nimcp_ethics.h"
#include "cognitive/salience/nimcp_salience.h"
#include "cognitive/curiosity/nimcp_curiosity.h"
#include "cognitive/consolidation/nimcp_consolidation.h"
#include "cognitive/nimcp_working_memory.h"
#include "cognitive/nimcp_emotional_tagging.h"
#include "cognitive/nimcp_explanations.h"
#include "cognitive/global_workspace/nimcp_global_workspace.h"
#include "core/brain_regions/nimcp_brain_regions.h"
#include "core/neuron_types/nimcp_neural_logic.h"
#include "information/nimcp_cross_modal.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

#define LOG_MODULE "BRAIN_MULTIMODAL"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_buffer_constants.h"
/* cochlea_process via extern using void* — including perception/nimcp_cochlea.h
 * here pulls in a medulla_t / thalamus_t typedef that collides with other
 * headers included transitively via nimcp_brain.c. Pointer-typed parameters
 * are ABI-compatible with the real cochlea_t* / cochlea_output_t* so this
 * declaration links cleanly. */
extern nimcp_error_t cochlea_process(
    void* cochlea, const float* audio_in,
    uint32_t num_samples, void* output);

BRIDGE_BOILERPLATE_MESH_ONLY(brain_multimodal, MESH_ADAPTER_CATEGORY_COGNITIVE)


//=============================================================================
// External declarations
//=============================================================================

// Error handling (shared across all brain modules)
extern void set_error(const char* format, ...);
extern void brain_clear_error(void);

//=============================================================================
// Static helper functions
//=============================================================================

/**
 * @brief STAGE 1: Extract sensory features from raw inputs
 *
 * RESPONSIBILITY: Process visual/audio/speech cortex inputs
 */
static bool extract_sensory_features(
    brain_t brain,
    const brain_multimodal_input_t* input,
    float** visual_features,
    uint32_t* visual_dim,
    float** audio_features,
    uint32_t* audio_dim,
    bool* audio_success,
    float** speech_features,
    uint32_t* speech_dim,
    float** direct_features,
    uint32_t* direct_dim,
    bool has_visual,
    bool has_audio,
    bool has_direct)
{
    // Initialize outputs
    *visual_features = NULL;
    *visual_dim = 0;
    *audio_features = NULL;
    *audio_dim = 0;
    *audio_success = false;
    *speech_features = NULL;
    *speech_dim = 0;
    *direct_features = (float*)input->direct_data;
    *direct_dim = input->direct_dim;

    // Process visual input through V1 visual cortex
    if (has_visual && brain->visual_cortex && brain->visual_feature_buffer) {
        bool visual_success = visual_cortex_process(
            brain->visual_cortex,
            input->visual_data,
            input->visual_width,
            input->visual_height,
            input->visual_channels,
            brain->visual_feature_buffer
        );

        if (visual_success) {
            *visual_features = brain->visual_feature_buffer;
            *visual_dim = brain->config.visual_feature_dim;
        }
    }

    // Process audio input through A1 auditory cortex
    if (has_audio && brain->audio_cortex && brain->audio_feature_buffer) {
        *audio_success = audio_cortex_process(
            brain->audio_cortex,
            input->audio_data,
            input->audio_samples,
            input->audio_channels,
            brain->audio_feature_buffer
        );

        if (*audio_success) {
            *audio_features = brain->audio_feature_buffer;
            *audio_dim = brain->config.audio_feature_dim;
        }
    }

    // Process speech from audio (hierarchical: A1 -> STG/Wernicke)
    if (has_audio && *audio_success && brain->speech_cortex && brain->speech_feature_buffer) {
        bool speech_success = speech_cortex_process(
            brain->speech_cortex,
            input->audio_data,
            input->audio_samples,
            brain->speech_feature_buffer
        );

        if (speech_success) {
            *speech_features = brain->speech_feature_buffer;
            *speech_dim = brain->config.speech_feature_dim;
        }
    }

    return true;  // At least one modality is present (validated by caller)
}

/**
 * @brief STAGE 2.5: Apply multihead attention to integrated features
 *
 * WHAT: Selective feature processing using cortical column-inspired attention
 * WHY:  Focus on salient features, reduce computation, improve accuracy
 * HOW:  Apply multihead attention if enabled, use guard clauses for clean code
 *
 * DESIGN PATTERN: Strategy Pattern (attention is pluggable strategy)
 * BIOLOGICAL ANALOGY: Thalamic gating + cortical column parallel processing
 * PERFORMANCE: 2-5x speedup through selective processing
 */
bool apply_attention_to_features(brain_t brain)
{
    // GUARD CLAUSE: Brain must exist
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "apply_attention_to_features: brain is NULL");

            return false;
    }

    // GUARD CLAUSE: Skip if attention not enabled or not available
    if (!brain->multihead_attention || !brain->config.enable_multihead_attention) {
        return true;  // Not an error - just skip attention
    }

    // GUARD CLAUSE: Need integrated feature buffer
    if (!brain->integrated_feature_buffer) {
        return true;  // Skip if no buffer (shouldn't happen)
    }

    // Calculate integrated feature dimension (no nesting)
    uint32_t integrated_dim = brain->config.num_inputs;
    if (brain->config.enable_multimodal_integration) {
        uint32_t multimodal_dim = brain->config.visual_feature_dim +
                                   brain->config.audio_feature_dim +
                                   brain->config.speech_feature_dim;
        integrated_dim = (multimodal_dim > 0) ? multimodal_dim : integrated_dim;
    }

    // Prepare salience scores (NULL = let attention compute its own weights)
    float* salience_scores = NULL;

    // Apply attention (in-place transformation)
    bool success = multihead_attention_forward(
        brain->multihead_attention,
        brain->integrated_feature_buffer,
        1,  // Sequence length = 1 (single timestep)
        salience_scores,
        brain->integrated_feature_buffer  // In-place
    );

    // Non-fatal error handling
    if (!success && brain->config.enable_explanations) {
        LOG_MODULE_WARN(LOG_MODULE, "Attention failed, continuing without selective processing");
    }

    return true;  // Always return true (non-fatal even if attention fails)
}

/**
 * WHAT: Process features through hierarchical brain regions
 * WHY:  Enable specialized cortical processing with realistic layer dynamics
 * HOW:  Feed features to brain regions, collect output from multiple regions
 *
 * BIOLOGICAL MOTIVATION:
 * - Sensory inputs flow through specialized cortical regions (V1, A1)
 * - Each region processes with 6-layer architecture
 * - Output from multiple regions converges for decision-making
 */
bool process_brain_regions(brain_t brain)
{
    // GUARD CLAUSE: Brain must exist
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "process_brain_regions: brain is NULL");

            return false;
    }

    // GUARD CLAUSE: Skip if brain_regions not enabled or not available
    if (!brain->brain_regions || !brain->config.enable_brain_regions) {
        return true;  // Not an error - just skip brain regions
    }

    // GUARD CLAUSE: Need integrated feature buffer
    if (!brain->integrated_feature_buffer) {
        return true;  // Skip if no buffer
    }

    // Calculate integrated feature dimension
    uint32_t integrated_dim = brain->config.num_inputs;
    if (brain->config.enable_multimodal_integration) {
        uint32_t multimodal_dim = brain->config.visual_feature_dim +
                                   brain->config.audio_feature_dim +
                                   brain->config.speech_feature_dim;
        integrated_dim = (multimodal_dim > 0) ? multimodal_dim : integrated_dim;
    }

    // WHAT: Feed input to each brain region
    uint64_t timestamp = nimcp_time_get_ms();

    for (uint32_t i = 0; i < brain->brain_regions->num_regions; i++) {
        brain_region_t* region = brain->brain_regions->regions[i];
        if (!region) continue;

        // Feed features to this region's input layer (Layer 4)
        brain_region_process_input(
            region,
            brain->integrated_feature_buffer,
            integrated_dim,
            timestamp
        );

        // Step the region forward one time step for processing
        brain_region_step(region, 1);
    }

    // WHAT: Collect output from all brain regions
    // Temporary buffer for collecting region outputs
    float* region_output = (float*)nimcp_calloc(integrated_dim, sizeof(float));
    if (!region_output) {
        return true;  // Non-fatal: continue without region processing
    }

    // Clear feature buffer to accumulate region outputs
    memset(brain->integrated_feature_buffer, 0, integrated_dim * sizeof(float));

    uint32_t regions_processed = 0;
    for (uint32_t i = 0; i < brain->brain_regions->num_regions; i++) {
        brain_region_t* region = brain->brain_regions->regions[i];
        if (!region) continue;

        // Get output from this region's Layer 5 (output layer)
        uint32_t output_count = brain_region_get_output(
            region,
            region_output,
            integrated_dim
        );

        if (output_count > 0) {
            // Add this region's output to accumulated features
            for (uint32_t j = 0; j < output_count && j < integrated_dim; j++) {
                brain->integrated_feature_buffer[j] += region_output[j];
            }
            regions_processed++;
        }
    }

    // Average the accumulated outputs
    if (regions_processed > 0) {
        for (uint32_t j = 0; j < integrated_dim; j++) {
            brain->integrated_feature_buffer[j] /= (float)regions_processed;
        }
    }

    nimcp_free(region_output);

    return true;  // Always return true (non-fatal even if brain regions processing fails)
}

/**
 * @brief STAGE 2: Integrate multi-modal features using attention
 *
 * RESPONSIBILITY: Fuse visual/audio/speech/direct features into unified representation
 */
static bool integrate_multimodal_features(
    brain_t brain,
    float* visual_features,
    uint32_t visual_dim,
    float* audio_features,
    uint32_t audio_dim,
    float* speech_features,
    uint32_t speech_dim,
    float* direct_features,
    uint32_t direct_dim,
    uint64_t timestamp_ms,
    brain_multimodal_output_t* output)
{
    // Check if we have visual or audio inputs that require multimodal integration
    bool needs_multimodal = (visual_dim > 0) || (audio_dim > 0) || (speech_dim > 0);

    // If we only have direct features and multimodal is disabled
    if (!needs_multimodal && direct_dim > 0) {
        // Direct-only mode: No multimodal integration needed
        output->visual_attention = 0.0F;
        output->audio_attention = 0.0F;
        output->speech_attention = 0.0F;
        output->direct_attention = 1.0F;

        // If we have integrated_feature_buffer, copy direct features to it
        if (brain->integrated_feature_buffer && direct_features && direct_dim > 0) {
            uint32_t copy_size = (direct_dim < brain->config.num_inputs) ?
                                direct_dim : brain->config.num_inputs;
            memcpy(brain->integrated_feature_buffer, direct_features, copy_size * sizeof(float));
            // Zero out any remaining elements
            if (copy_size < brain->config.num_inputs) {
                memset(brain->integrated_feature_buffer + copy_size, 0,
                      (brain->config.num_inputs - copy_size) * sizeof(float));
            }
        }

        return true;
    }

    // For visual/audio/speech inputs, we need the multimodal system
    if (!brain->multimodal || !brain->integrated_feature_buffer) {
        set_error("Multimodal integration required for visual/audio/speech but not enabled");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "integrate_multimodal_features: required parameter is NULL (brain->multimodal, brain->integrated_feature_buffer)");
        return false;
    }

    multimodal_input_t mm_input = {
        .visual_features = visual_features,
        .visual_dim = visual_dim,
        .audio_features = audio_features,
        .audio_dim = audio_dim,
        .speech_features = speech_features,
        .speech_dim = speech_dim,
        .direct_features = direct_features,
        .direct_dim = direct_dim,
        .timestamp = timestamp_ms
    };

    bool integrate_success = multimodal_integrate(
        brain->multimodal,
        &mm_input,
        brain->integrated_feature_buffer
    );

    if (!integrate_success) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,


                "if: integrate_success is NULL");


            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "integrate_multimodal_features: integrate_success is NULL");
            return false;
    }

    // Get attention weights for transparency
    multimodal_get_attention(
        brain->multimodal,
        &output->visual_attention,
        &output->audio_attention,
        &output->speech_attention,
        &output->direct_attention
    );

    return true;
}

/**
 * @brief STAGE 3: Process integrated features through neural network
 *
 * RESPONSIBILITY: Forward pass through spiking network with learning
 */
static uint32_t process_neural_network(
    brain_t brain,
    const float* input_buffer,
    uint32_t input_size,
    uint64_t timestamp_ms,
    float** network_output,
    uint32_t network_output_size,
    brain_multimodal_output_t* output)
{
    // Validate input buffer
    if (!input_buffer || input_size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "process_neural_network: invalid parameters");

            return 0;
    }

    // Allocate network output buffer
    *network_output = nimcp_calloc(network_output_size, sizeof(float));
    if (!*network_output) {
        return 0;
    }

    // Forward pass through adaptive network
    uint32_t spikes_generated = adaptive_network_forward(
        brain->network,
        input_buffer,
        input_size,
        *network_output,
        network_output_size,
        timestamp_ms
    );

    // Copy network output to user's output buffer
    if (output->output_vector && output->output_dim > 0) {
        uint32_t copy_size = (output->output_dim < network_output_size) ?
                             output->output_dim : network_output_size;
        memcpy(output->output_vector, *network_output, copy_size * sizeof(float));

        // Apply task-specific output transform (e.g., softmax for classification)
        if (brain->strategy && brain->strategy->transform_output) {
            brain->strategy->transform_output(output->output_vector, output->output_dim);
        }
    }

    return spikes_generated;
}

/**
 * @brief STAGE 4: Apply cognitive assessments to network output
 *
 * RESPONSIBILITY: Compute confidence, ethics, salience, novelty, curiosity
 */
static bool apply_cognitive_processing(
    brain_t brain,
    const float* input_buffer,
    uint32_t input_size,
    const float* network_output,
    uint32_t network_output_size,
    uint32_t spikes_generated,
    uint64_t timestamp_ms,
    brain_multimodal_output_t* output)
{
    // Introspection: Assess confidence and uncertainty
    if (brain->introspection && input_buffer) {
        brain_uncertainty_t uncertainty = brain_get_uncertainty(
            brain->introspection,
            input_buffer,
            input_size
        );

        output->introspection_uncertainty = uncertainty.total;
        output->confidence = 1.0F - output->introspection_uncertainty;
    } else {
        // Fallback: Compute confidence from output variance and spike counts
        float output_variance = 0.0F;
        float output_mean = 0.0F;
        for (uint32_t i = 0; i < network_output_size; i++) {
            output_mean += network_output[i];
        }
        output_mean /= network_output_size;

        for (uint32_t i = 0; i < network_output_size; i++) {
            float diff = network_output[i] - output_mean;
            output_variance += diff * diff;
        }
        output_variance /= network_output_size;

        output->confidence = fminf(1.0F, (float)spikes_generated / (brain->config.num_inputs * 2.0F));
        output->confidence *= (1.0F - fminf(1.0F, output_variance));
        output->introspection_uncertainty = 1.0F - output->confidence;
    }

    // Ethics: Validate output (check for NaN/inf/extreme values)
    BRAIN_ENSURE_ETHICS(brain);
    if (brain->ethics) {
        output->ethical_approved = true;
        for (uint32_t i = 0; i < network_output_size; i++) {
            if (isnan(network_output[i]) || isinf(network_output[i]) ||
                fabsf(network_output[i]) > 1000.0F) {
                output->ethical_approved = false;
                break;
            }
        }
    } else {
        output->ethical_approved = true;
    }

    // Salience: Evaluate input importance (novelty, surprise, urgency)
    if (brain->salience && input_buffer) {
        brain_salience_t salience = brain_evaluate_salience_temporal(
            brain->salience,
            input_buffer,
            input_size,
            timestamp_ms
        );

        output->salience_score = salience.salience;
        output->novelty_score = salience.novelty;
    } else {
        // Fallback: Max output activation as salience
        float max_activation = 0.0F;
        for (uint32_t i = 0; i < network_output_size; i++) {
            if (network_output[i] > max_activation) {
                max_activation = network_output[i];
            }
        }
        output->salience_score = fminf(1.0F, max_activation);
    }

    // Curiosity: Learn from novel experiences
    if (brain->curiosity) {
        // Fallback if salience didn't compute novelty
        if (!brain->salience) {
            float expected_spikes = brain->config.num_inputs * 0.5F;
            float spike_diff = fabsf((float)spikes_generated - expected_spikes);
            output->novelty_score = fminf(1.0F, spike_diff / expected_spikes);
        }
    } else {
        // Fallback novelty if no cognitive modules
        if (!brain->salience) {
            output->novelty_score = 0.3F;
        }
    }

    // Constants for logic processing
    const float LOGIC_ETHICS_PENALTY = 0.5F;

    if (brain->symbolic_logic) {
        // Validate output structure
        if (!output) {
            set_error("NULL output in symbolic logic processing");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "apply_cognitive_processing: output is NULL");
            return false;
        }

        // Initialize logical consistency state
        output->logical_consistency = true;
        output->reasoning_confidence = output->confidence;

        // Generate reasoning explanation string
        const int written = snprintf(
            output->logical_reasoning,
            sizeof(output->logical_reasoning),
            "Neural confidence: %.2f, Salience: %.2f, Ethical: %s",
            output->confidence,
            output->salience_score,
            output->ethical_approved ? "YES" : "NO"
        );

        // Check for buffer overflow
        if (written < 0 || (size_t)written >= sizeof(output->logical_reasoning)) {
            set_error("Logic reasoning buffer overflow");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "apply_cognitive_processing: capacity exceeded");
            return false;
        }

        // Detect logical inconsistency from ethical violations
        if (!output->ethical_approved) {
            output->logical_consistency = false;
            output->reasoning_confidence *= LOGIC_ETHICS_PENALTY;
        }
    } else {
        output->logical_consistency = output->ethical_approved;
        output->reasoning_confidence = output->confidence;
        snprintf(output->logical_reasoning, sizeof(output->logical_reasoning),
                "Logic engine not enabled");
    }

    // Global Workspace: Output broadcast state if workspace is broadcasting
    BRAIN_ENSURE_GLOBAL_WORKSPACE(brain);
    if (brain->global_workspace) {
        if (global_workspace_has_broadcast(brain->global_workspace)) {
            output->has_workspace_broadcast = true;
            output->workspace_source_module = (uint8_t)global_workspace_get_broadcast_source(brain->global_workspace);
            output->workspace_broadcast_strength = global_workspace_get_broadcast_strength(brain->global_workspace);
            output->workspace_num_competitors = global_workspace_get_competitor_count(brain->global_workspace);
        }
    }

    // Executive Function / Working Memory: Output WM state if items present
    BRAIN_ENSURE_WORKING_MEMORY(brain);
    if (brain->working_memory) {
        output->working_memory_items = working_memory_get_count(brain->working_memory);
        output->working_memory_utilization = working_memory_get_utilization(brain->working_memory);

        // If WM has items, describe most salient one
        if (output->working_memory_items > 0) {
            snprintf(output->top_wm_item_description, sizeof(output->top_wm_item_description),
                    "%u items active, %.1f%% capacity",
                    output->working_memory_items,
                    output->working_memory_utilization * 100.0F);
        }
    }

    // Curiosity: Output exploration drive when novelty is high
    if (brain->curiosity && output->novelty_score > 0.5F) {
        output->curiosity_drive = curiosity_get_drive(brain->curiosity);
        output->exploration_triggered = (output->curiosity_drive > 0.6F);

        if (output->exploration_triggered) {
            snprintf(output->curiosity_reason, sizeof(output->curiosity_reason),
                    "High novelty (%.2f) triggered exploration drive (%.2f)",
                    output->novelty_score, output->curiosity_drive);
        }
    }

    return output->ethical_approved;
}

/**
 * WHAT: Strengthen recently activated synapses during consolidation
 * WHY:  Tag-and-capture model - synaptic tagging during encoding, consolidation during sleep
 * HOW:  Identify tagged synapses (high recent activity), strengthen based on salience/novelty
 */
static bool consolidation_strengthen(
    brain_t brain,
    float novelty_score,
    float salience_score,
    float emotional_valence)
{
    // GUARD: Validate inputs
    if (!brain) {

            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "consolidation_strengthen: brain is NULL");

            return false;

        }
    if (!brain->network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "consolidation_strengthen: brain->network is NULL");
        return false;
    }

    // GUARD: Skip if consolidation disabled
    if (!brain->consolidation) return true;

    // Compute consolidation strength
    float importance = (novelty_score * 0.4F) + (salience_score * 0.4F) +
                      (fabsf(emotional_valence) * 0.2F);

    // GUARD: Skip weak consolidation
    if (importance < 0.3F) return true;

    // Record in long-term memory buffer
    if (brain->longterm_memory && brain->longterm_count < brain->longterm_capacity) {
        uint32_t idx = brain->longterm_count;
        brain->longterm_memory[idx].salience = importance;
        brain->longterm_memory[idx].timestamp_ms = nimcp_time_get_ms();

        // Copy network output as memory trace
        if (brain->integrated_feature_buffer && brain->config.num_inputs > 0) {
            brain->longterm_memory[idx].num_features = brain->config.num_inputs;
            brain->longterm_memory[idx].features = nimcp_malloc(
                brain->config.num_inputs * sizeof(float)
            );

            if (brain->longterm_memory[idx].features) {
                memcpy(
                    brain->longterm_memory[idx].features,
                    brain->integrated_feature_buffer,
                    brain->config.num_inputs * sizeof(float)
                );
                brain->longterm_count++;
            }
        }
    }

    // Trigger background consolidation if needed
    if (importance > 0.8F && brain->consolidation) {
        brain_trigger_consolidation(brain->consolidation);
    }

    return true;
}

/**
 * @brief STAGE 5: Format output with decision label and explanation
 *
 * RESPONSIBILITY: Extract decision, generate comprehensive explanation
 */
static bool format_output(
    brain_t brain,
    const float* network_output,
    uint32_t network_output_size,
    uint32_t spikes_generated,
    bool has_visual,
    bool has_audio,
    float* speech_features,
    uint32_t speech_dim,
    brain_multimodal_output_t* output)
{
    // Consolidation: Strengthen important memories
    BRAIN_ENSURE_CONSOLIDATION(brain);
    if (brain->consolidation && (output->novelty_score > 0.7F || output->salience_score > 0.7F)) {
        float emotional_valence = 0.0F;
        consolidation_strengthen(brain, output->novelty_score, output->salience_score, emotional_valence);
    }

    // Ethical filtering: Block harmful outputs
    if (!output->ethical_approved) {
        snprintf(output->explanation, sizeof(output->explanation),
                 "Output blocked: Failed ethical validation (NaN/Inf/extreme values detected)");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "format_output: output->ethical_approved is NULL");
        return false;
    }

    // Find decision label based on max output activation
    uint32_t max_idx = 0;
    float max_val = -INFINITY;
    for (uint32_t i = 0; i < network_output_size; i++) {
        if (network_output[i] > max_val) {
            max_val = network_output[i];
            max_idx = i;
        }
    }

    // Generate decision label
    if (brain->output_labels && max_idx < brain->num_output_labels && brain->output_labels[max_idx]) {
        strncpy(output->decision_label, brain->output_labels[max_idx],
                sizeof(output->decision_label) - 1);
    } else {
        output->decision_label[0] = '\0';
    }

    // Generate comprehensive explanation with all 4 modalities
    char modality_str[NIMCP_ERROR_BUFFER_SIZE] = {0};
    bool has_speech = (speech_features != NULL && speech_dim > 0);

    // Build modality attention string
    char* pos = modality_str;
    int remaining = sizeof(modality_str);
    bool first = true;

    if (has_visual || output->visual_attention > 0.01F) {
        int written = snprintf(pos, remaining, "%svisual=%.0f%%", first ? "" : " ",
                              output->visual_attention * 100.0F);
        pos += written;
        remaining -= written;
        first = false;
    }
    if (has_audio || output->audio_attention > 0.01F) {
        int written = snprintf(pos, remaining, "%saudio=%.0f%%", first ? "" : " ",
                              output->audio_attention * 100.0F);
        pos += written;
        remaining -= written;
        first = false;
    }
    if (has_speech || output->speech_attention > 0.01F) {
        int written = snprintf(pos, remaining, "%sspeech=%.0f%%", first ? "" : " ",
                              output->speech_attention * 100.0F);
        pos += written;
        remaining -= written;
        first = false;
    }
    if (output->direct_attention > 0.01F) {
        int written = snprintf(pos, remaining, "%sdirect=%.0f%%", first ? "" : " ",
                              output->direct_attention * 100.0F);
        (void)written;  // Suppress unused warning
    }

    snprintf(output->explanation, sizeof(output->explanation),
             "%s | %u spikes | conf=%.0f%% salience=%.0f%% novelty=%.0f%%",
             modality_str,
             spikes_generated,
             output->confidence * 100.0F,
             output->salience_score * 100.0F,
             output->novelty_score * 100.0F);

    return true;
}

//=============================================================================
// Public API
//=============================================================================

/**
 * WHAT: Process multi-modal input through unified cognitive architecture
 * WHY:  Enable coordinated multi-modal perception and cognition
 * HOW:  Sensory -> Integration -> Neural -> Cognitive -> Output
 */
bool brain_process_multimodal(
    brain_t brain,
    const brain_multimodal_input_t* input,
    brain_multimodal_output_t* output)
{
    // =========================================================================
    // VALIDATION STAGE
    // =========================================================================

    // Guard clause: Validate inputs
    if (!brain || !input || !output) {
        set_error("Invalid parameters: brain, input, or output is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_process_multimodal: required parameter is NULL (brain, input, output)");
        return false;
    }

    // Check that at least one modality is present
    bool has_visual = (input->visual_data != NULL && input->visual_width > 0 && input->visual_height > 0);
    bool has_audio = (input->audio_data != NULL && input->audio_samples > 0);
    bool has_direct = (input->direct_data != NULL && input->direct_dim > 0);

    if (!has_visual && !has_audio && !has_direct) {
        set_error("No input modality provided");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_process_multimodal: required parameter is NULL (has_visual, has_audio, has_direct)");
        return false;
    }

    // Check brain is configured for multi-modal processing
    bool needs_multimodal = has_visual || has_audio;
    if (needs_multimodal && !brain->config.enable_multimodal_integration) {
        set_error("Brain not configured for multimodal processing");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_process_multimodal: brain->config is NULL");
        return false;
    }

    // Initialize multimodal output structure
    memset(output->decision_label, 0, sizeof(output->decision_label));
    memset(output->explanation, 0, sizeof(output->explanation));
    output->confidence = 0.0F;
    output->introspection_uncertainty = 0.0F;
    output->salience_score = 0.0F;
    output->ethical_approved = true;
    output->novelty_score = 0.0F;
    output->visual_attention = 0.0F;
    output->audio_attention = 0.0F;
    output->speech_attention = 0.0F;
    output->direct_attention = 0.0F;

    // Initialize language output fields
    output->language_response = NULL;
    output->language_response_length = 0;
    output->language_confidence = 0.0F;

    // Initialize logical reasoning fields
    output->logical_consistency = true;
    output->reasoning_confidence = 0.0F;
    memset(output->logical_reasoning, 0, sizeof(output->logical_reasoning));

    // Initialize cognitive module output fields
    output->has_workspace_broadcast = false;
    output->workspace_source_module = 0;
    output->workspace_broadcast_strength = 0.0F;
    output->workspace_num_competitors = 0;
    output->working_memory_items = 0;
    output->working_memory_utilization = 0.0F;
    memset(output->top_wm_item_description, 0, sizeof(output->top_wm_item_description));
    output->has_mental_state_inference = false;
    memset(output->inferred_belief, 0, sizeof(output->inferred_belief));
    memset(output->inferred_intention, 0, sizeof(output->inferred_intention));
    output->tom_confidence = 0.0F;
    output->curiosity_drive = 0.0F;
    output->exploration_triggered = false;
    memset(output->curiosity_reason, 0, sizeof(output->curiosity_reason));
    output->has_prediction = false;
    output->prediction_error = 0.0F;
    output->prediction_confidence = 0.0F;
    output->has_knowledge_retrieval = false;
    output->num_facts_retrieved = 0;
    memset(output->retrieved_concept, 0, sizeof(output->retrieved_concept));
    output->has_nlp_interpretation = false;
    memset(output->nlp_intent, 0, sizeof(output->nlp_intent));
    memset(output->nlp_sentiment, 0, sizeof(output->nlp_sentiment));
    output->nlp_comprehension_score = 0.0F;

    // Working Memory Temporal Decay
    BRAIN_ENSURE_WORKING_MEMORY(brain);
    if (brain->working_memory) {
        working_memory_decay(brain->working_memory, input->timestamp_ms);
    }

    // =========================================================================
    // PIPELINE: Five-Stage Processing
    // =========================================================================

    // Stage variables
    float* visual_features = NULL;
    uint32_t visual_dim = 0;
    float* audio_features = NULL;
    uint32_t audio_dim = 0;
    bool audio_success = false;
    float* speech_features = NULL;
    uint32_t speech_dim = 0;
    float* direct_features = NULL;
    uint32_t direct_dim = 0;
    float* network_output = NULL;
    uint32_t network_output_size = brain->config.num_outputs;
    uint32_t spikes_generated = 0;

    // -------------------------------------------------------------------------
    // STAGE 1: Extract sensory features from raw inputs
    // -------------------------------------------------------------------------
    if (!extract_sensory_features(
            brain, input,
            &visual_features, &visual_dim,
            &audio_features, &audio_dim, &audio_success,
            &speech_features, &speech_dim,
            &direct_features, &direct_dim,
            has_visual, has_audio, has_direct)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_process_multimodal: operation failed");
        return false;
    }

    // -------------------------------------------------------------------------
    // STAGE 1.5: Wernicke comprehension → grounded_language ingestion
    // -------------------------------------------------------------------------
    // When raw audio is present and both wernicke and grounded_lang exist,
    // run audio through wernicke_comprehend and route the recognized words
    // back into grounded_language as auditory grounding events. This closes
    // the audio→language loop: every recognized word grows total_groundings
    // with attention=word.confidence, which is what turns the lexicon from
    // a static seed into a vocabulary that responds to actual audio.
    if (brain->wernicke && brain->grounded_lang && audio_success &&
        input->audio_data && input->audio_samples > 0 &&
        audio_features && audio_dim > 0) {
        // Sample rate isn't carried on multimodal_input_t; speech-band 16 kHz
        // is the established NIMCP default (matches audio_cortex + Speech
        // Commands fixtures used in training).
        const uint32_t kAudioSampleRateHz = 16000u;
        extern int gl_drive_audio_comprehension(grounded_language_t*, void*,
                                                 const float*, uint32_t,
                                                 uint32_t,
                                                 const float*, uint32_t);
        (void)gl_drive_audio_comprehension(
            (grounded_language_t*)brain->grounded_lang,
            brain->wernicke,
            input->audio_data, input->audio_samples, kAudioSampleRateHz,
            audio_features, audio_dim);
    }

    // -------------------------------------------------------------------------
    // STAGE 2: Integrate multi-modal features using attention
    // -------------------------------------------------------------------------
    if (!integrate_multimodal_features(
            brain,
            visual_features, visual_dim,
            audio_features, audio_dim,
            speech_features, speech_dim,
            direct_features, direct_dim,
            input->timestamp_ms,
            output)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_process_multimodal: operation failed");
        return false;
    }

    // -------------------------------------------------------------------------
    // STAGE 2.4: Track cross-modal information flow
    // -------------------------------------------------------------------------
    if (brain->enable_cross_modal_monitoring && brain->cross_modal_graph) {
        uint32_t num_active_modalities = 0;
        const float* modality_features[3] = {NULL, NULL, NULL};
        uint32_t modality_dims[3] = {0, 0, 0};
        const char* modality_names[3] = {"visual", "audio", "speech"};

        if (visual_dim > 0 && visual_features) {
            modality_features[num_active_modalities] = visual_features;
            modality_dims[num_active_modalities] = visual_dim;
            num_active_modalities++;
        }
        if (audio_dim > 0 && audio_features) {
            modality_features[num_active_modalities] = audio_features;
            modality_dims[num_active_modalities] = audio_dim;
            num_active_modalities++;
        }
        if (speech_dim > 0 && speech_features) {
            modality_features[num_active_modalities] = speech_features;
            modality_dims[num_active_modalities] = speech_dim;
            num_active_modalities++;
        }

        // Analyze multi-modal integration if we have 2+ modalities
        if (num_active_modalities >= 2) {
            brain->last_cross_modal_metrics = cross_modal_analyze_integration(
                modality_features,
                modality_dims,
                num_active_modalities,
                brain->cross_modal_sample_count,
                modality_names,
                &brain->shannon_config
            );

            // Analyze pairwise channels and detect bottlenecks
            for (uint32_t src = 0; src < num_active_modalities; src++) {
                for (uint32_t dst = src + 1; dst < num_active_modalities; dst++) {
                    cross_modal_channel_t channel = cross_modal_analyze_channel(
                        modality_names[src],
                        modality_names[dst],
                        modality_features[src],
                        modality_dims[src],
                        modality_features[dst],
                        modality_dims[dst],
                        brain->cross_modal_sample_count,
                        &brain->shannon_config
                    );

                    cross_modal_update_routing_graph(brain->cross_modal_graph, src, dst, &channel);
                }
            }

            // Detect bottlenecks
            cross_modal_channel_t bottlenecks[10];
            uint32_t num_bottlenecks = 0;
            cross_modal_detect_bottlenecks(
                brain->cross_modal_graph,
                brain->cross_modal_bottleneck_threshold,
                bottlenecks,
                10,
                &num_bottlenecks
            );
        }
    }

    // -------------------------------------------------------------------------
    // STAGE 2.5: Apply multihead attention for selective feature processing
    // -------------------------------------------------------------------------
    if (!apply_attention_to_features(brain)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "brain_process_multimodal: apply_attention_to_features is NULL");
        return false;
    }

    // -------------------------------------------------------------------------
    // STAGE 2.6: Process through hierarchical brain regions
    // -------------------------------------------------------------------------
    if (!process_brain_regions(brain)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "unknown: process_brain_regions is NULL");
        return false;
    }

    // -------------------------------------------------------------------------
    // STAGE 3: Process through neural network with learning
    // -------------------------------------------------------------------------
    const float* network_input = brain->integrated_feature_buffer ?
                                  brain->integrated_feature_buffer :
                                  direct_features;

    if (!network_input) {
        set_error("No input data available for neural network processing");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: network_input is NULL");
        return false;
    }

    uint32_t network_input_size = brain->config.num_inputs;

    spikes_generated = process_neural_network(
        brain,
        network_input,
        network_input_size,
        input->timestamp_ms,
        &network_output,
        network_output_size,
        output
    );

    if (!network_output) {
        set_error("Neural network forward pass returned NULL output");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: network_output is NULL");
        return false;
    }

    // -------------------------------------------------------------------------
    // STAGE 3.5: Execute neural logic network (validate constraints)
    // -------------------------------------------------------------------------
    if (brain->logic) {
        uint64_t logic_delta_t = 100;
        neural_logic_update(
            brain->logic,
            input->timestamp_ms * 1000,
            logic_delta_t
        );
    }

    // -------------------------------------------------------------------------
    // STAGE 4: Apply cognitive assessments (confidence, ethics, salience)
    // -------------------------------------------------------------------------
    if (!apply_cognitive_processing(
            brain,
            network_input,
            network_input_size,
            network_output,
            network_output_size,
            spikes_generated,
            input->timestamp_ms,
            output)) {
        nimcp_free(network_output);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "unknown: operation failed");
        return false;
    }

    // =========================================================================
    // Store network output in working memory with emotional tagging
    // =========================================================================
    BRAIN_ENSURE_WORKING_MEMORY(brain);
    if (brain->working_memory && output->salience_score > 0.1F) {
        emotional_tag_t emotion = emotional_tag_from_cognitive_state(
            output->confidence,
            output->introspection_uncertainty,
            output->novelty_score,
            output->ethical_approved,
            input->timestamp_ms
        );

        working_memory_add_with_emotion(
            brain->working_memory,
            network_output,
            network_output_size,
            output->salience_score,
            &emotion
        );
    }

    // -------------------------------------------------------------------------
    // STAGE 5: Format output with decision label and explanation
    // -------------------------------------------------------------------------
    bool success = format_output(
        brain,
        network_output,
        network_output_size,
        spikes_generated,
        has_visual,
        has_audio,
        speech_features,
        speech_dim,
        output
    );

    // -------------------------------------------------------------------------
    // STAGE 6: Natural Explanations
    // -------------------------------------------------------------------------
    if (success && brain->explanation_gen && brain->config.enable_natural_explanations) {
        natural_explanation_t nat_exp;
        if (explanation_generate_from_multimodal(brain->explanation_gen, brain, output, &nat_exp)) {
            char original_exp[NIMCP_ERROR_BUFFER_SIZE];
            strncpy(original_exp, output->explanation, sizeof(original_exp) - 1);
            original_exp[sizeof(original_exp) - 1] = '\0';

            snprintf(output->explanation, sizeof(output->explanation),
                    "%s | WHAT: %s | WHY: %s",
                    original_exp, nat_exp.what, nat_exp.why);
        }
    }

    // =========================================================================
    // EMOTIONAL INTELLIGENCE PROCESSING
    // =========================================================================
    if (success && brain->empathetic_response_engine && input->language_text != NULL) {
        extern bool emotion_recognize_text_simple(const char* text,
                                                  char* emotion_name, size_t emotion_name_len,
                                                  float* confidence, float* valence, float* arousal);

        char detected_emotion[32] = {0};
        float emotion_confidence = 0.0F;
        float emotion_valence = 0.0F;
        float emotion_arousal = 0.0F;

        bool emotion_detected = emotion_recognize_text_simple(
            input->language_text,
            detected_emotion, sizeof(detected_emotion),
            &emotion_confidence, &emotion_valence, &emotion_arousal
        );

        if (emotion_detected) {
            output->has_emotion_detected = true;
            strncpy(output->detected_emotion, detected_emotion, sizeof(output->detected_emotion) - 1);
            output->emotion_confidence = emotion_confidence;
            output->emotion_valence = emotion_valence;
            output->emotion_arousal = emotion_arousal;
            output->emotion_intensity = fabsf(emotion_valence);
            output->emotion_is_negative = (emotion_valence < -0.3F);

            // Generate empathetic response if negative emotion detected
            if (output->emotion_is_negative && emotion_confidence > 0.5F) {
                typedef struct {
                    int emotion_type;
                    int intensity;
                    float valence;
                    float arousal;
                    char text_input[NIMCP_ERROR_BUFFER_LARGE];
                    uint32_t crisis_flags;
                    float crisis_confidence;
                } emotional_state_t;

                typedef struct {
                    char response_text[NIMCP_LOG_BUFFER_SIZE];
                    int primary_strategy;
                    int secondary_strategy;
                    float empathy_score;
                    float safety_score;
                    bool ethics_approved;
                    bool requires_human_escalation;
                    char escalation_reason[NIMCP_ERROR_BUFFER_SIZE];
                } empathetic_response_t;

                extern bool empathetic_response_generate(void* engine,
                                                        const emotional_state_t* state,
                                                        empathetic_response_t* response);

                emotional_state_t state = {0};
                state.emotion_type = 2;
                state.intensity = (output->emotion_intensity > 0.7F) ? 3 : 2;
                state.valence = emotion_valence;
                state.arousal = emotion_arousal;
                strncpy(state.text_input, input->language_text, sizeof(state.text_input) - 1);
                state.crisis_flags = 0;
                state.crisis_confidence = 0.0F;

                empathetic_response_t response = {0};
                if (empathetic_response_generate(brain->empathetic_response_engine, &state, &response)) {
                    output->has_empathetic_response = true;
                    strncpy(output->empathetic_response, response.response_text,
                           sizeof(output->empathetic_response) - 1);
                    output->empathy_score = response.empathy_score;
                    output->requires_human_escalation = response.requires_human_escalation;
                    if (response.requires_human_escalation) {
                        strncpy(output->escalation_reason, response.escalation_reason,
                               sizeof(output->escalation_reason) - 1);
                    }
                }
            }
        }
    }

    // =========================================================================
    // BIO-ROUTER MESSAGE PROCESSING
    // =========================================================================
    if (brain->bio_async_enabled) {
        uint32_t messages_processed = brain_bio_async_process_messages(brain, 10);

        static uint32_t update_counter = 0;
        if (++update_counter >= 100) {
            brain_bio_async_update(brain);
            update_counter = 0;
        }

        (void)messages_processed;
    }

    // Cleanup
    nimcp_free(network_output);

    return success;
}

/**
 * @brief Convenience wrapper — see header for rationale.
 *
 * Null-safe on features and output (required). `text` may be NULL; when
 * present, it's passed through as language_text with its byte length. The
 * direct_data pointer aliases caller memory (no copy); the input struct is
 * stack-allocated and valid only for the duration of this call.
 */
bool brain_process_features_with_text(brain_t brain,
                                      const float* features,
                                      uint32_t num_features,
                                      const char* text,
                                      brain_multimodal_output_t* output)
{
    if (!brain || !features || num_features == 0 || !output) {
        return false;
    }

    brain_multimodal_input_t input;
    memset(&input, 0, sizeof(input));
    input.direct_data  = features;
    input.direct_dim   = num_features;
    if (text) {
        input.language_text   = text;
        input.language_length = (uint32_t)strlen(text);
    }
    input.timestamp_ms = 0;

    memset(output, 0, sizeof(*output));
    return brain_process_multimodal(brain, &input, output);
}

bool brain_feed_audio(brain_t brain,
                      const float* audio,
                      uint32_t num_samples)
{
    if (!brain || !audio || num_samples == 0) return false;
    if (!brain->cochlea || !brain->cochlea_output_buffer) return false;

    nimcp_error_t err = cochlea_process(
        (void*)brain->cochlea, audio, num_samples,
        (void*)brain->cochlea_output_buffer);
    return err == NIMCP_SUCCESS;
}
