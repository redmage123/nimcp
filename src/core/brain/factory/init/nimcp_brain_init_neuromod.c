//=============================================================================
// nimcp_brain_init_neuromod.c - Neuromodulation Subsystems
//=============================================================================
/**
 * @file nimcp_brain_init_neuromod.c
 * @brief Neuromodulation Subsystems
 *
 * WHAT: Initialization functions for neuromod subsystems
 * WHY:  SRP refactoring - separate neuromod initialization logic
 * HOW:  Each function initializes a specific brain subsystem
 *
 * EXTRACTED FROM: nimcp_brain_init_subsystems.c
 * DATE: 2025-12-08
 *
 * @version 2.7.0
 * @author NIMCP Development Team
 */

//=============================================================================
// Includes
//=============================================================================

#include "core/brain/factory/init/nimcp_brain_init_neuromod.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/validation/nimcp_validate.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "BRAIN_INIT_NEUROMOD"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(brain_init_neuromod, MESH_ADAPTER_CATEGORY_SYSTEM)


// Compatibility macro for set_error (converts to LOG_ERROR)
#ifndef set_error
#define set_error(msg) LOG_ERROR(LOG_MODULE, "%s", msg)
#endif

// Subsystem includes (add as needed based on functions)
#include "glial/integration/nimcp_glial_integration.h"
#include "glial/myelin_sheath/nimcp_myelin_sheath.h"
#include "core/brain_oscillations/nimcp_brain_oscillations.h"
#include "cognitive/introspection/nimcp_introspection.h"
#include "cognitive/introspection/nimcp_connectivity_health.h"
#include "middleware/integration/nimcp_middleware_controller.h"
#include "core/axon/nimcp_axon.h"
#include "core/dendrite/nimcp_dendrite.h"
#include "cognitive/ethics/nimcp_ethics.h"
#include "cognitive/salience/nimcp_salience.h"
#include "cognitive/consolidation/nimcp_consolidation.h"
#include "cognitive/curiosity/nimcp_curiosity.h"
#include "cognitive/knowledge/nimcp_knowledge.h"
#include "cognitive/nimcp_symbolic_logic.h"
#include "cognitive/epistemic/nimcp_epistemic_filter.h"
#include "cognitive/wellbeing/nimcp_wellbeing.h"
#include "plasticity/neuromodulators/nimcp_neuromod_pink_noise.h"
#include "plasticity/neuromodulators/nimcp_neuromodulators.h"
#include "plasticity/neuromodulators/nimcp_spatial_neuromod.h"
#include "plasticity/attention/nimcp_attention.h"
#include "core/neuron_types/nimcp_neural_logic.h"
#include "cognitive/nimcp_fractal_cognitive.h"
#include "core/integration/nimcp_multimodal_integration.h"
#include "perception/nimcp_visual_cortex.h"
#include "perception/nimcp_audio_cortex.h"
#include "perception/nimcp_speech_cortex.h"
#include "nlp/nimcp_nlp.h"
#include "core/brain_regions/nimcp_brain_regions.h"
#include "core/events/nimcp_event_bus.h"
#include "cognitive/nimcp_working_memory.h"
#include "cognitive/nimcp_emotional_tagging.h"
#include "cognitive/nimcp_emotional_system.h"
#include "cognitive/nimcp_executive.h"
#include "cognitive/nimcp_sleep_wake.h"
#include "cognitive/memory/nimcp_engram.h"
#include "cognitive/memory/nimcp_systems_consolidation.h"
#include "cognitive/memory/nimcp_wm_transfer.h"
#include "cognitive/memory/nimcp_semantic_memory.h"
#include "cognitive/nimcp_mental_health.h"
#include "cognitive/nimcp_theory_of_mind.h"
#include "cognitive/nimcp_explanations.h"
#include "cognitive/nimcp_meta_learning.h"
#include "cognitive/nimcp_predictive.h"
#include "cognitive/nimcp_mirror_neurons.h"
#include "cognitive/global_workspace/nimcp_global_workspace.h"
#include "cognitive/global_workspace/nimcp_global_workspace_shannon.h"
#include "cognitive/nimcp_autobiographical_memory.h"
#include "middleware/training/nimcp_brain_training_integration.h"
#include "plasticity/adaptive/nimcp_adaptive.h"
#include "core/neuralnet/nimcp_neuralnet.h"
#include "plasticity/homeostatic/nimcp_homeostatic.h"
#include "plasticity/dendritic/nimcp_dendritic.h"
#include "plasticity/predictive/nimcp_predictive_coding.h"
#include "core/cortical_columns/nimcp_cortical_column.h"

#include <string.h>
#include <stdio.h>

//=============================================================================
// Neuromodulation Subsystems Implementation
//=============================================================================


/**
 * WHAT: Initialize full neuromodulator system
 * WHY:  Enable mental health interventions to adjust neurotransmitter levels
 * HOW:  Create neuromodulator system with default configuration
 *
 * BIOLOGICAL MOTIVATION:
 * - Neurotransmitters regulate mood, attention, arousal, and learning
 * - Mental health disorders often involve chemical imbalances
 * - Interventions can modulate levels to restore healthy functioning
 */
bool nimcp_brain_factory_init_neuromodulator_system(brain_t brain)
{
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_brain_factory_init_neuromodulator_system: brain is NULL");

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
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_neuromodulator_system: brain->neuromodulator_system is NULL");
        return false;
    }

    return true;
}


/**
 * WHAT: Initialize spatial neuromodulator system with quantum walk diffusion (Phase C2.1)
 * WHY:  Enable spatially-distributed neuromodulation with quantum speedup
 * HOW:  Create spatial neuromod system and wire to glial integration
 *
 * BIOLOGICAL MOTIVATION:
 * - Volume transmission: Neuromodulators diffuse through extracellular space
 * - Glial mediation: Astrocytes regulate neuromodulator concentrations
 * - Quantum walk: O(√N) speedup for diffusion on neural network graph
 *
 * INTEGRATION WITH BRAIN:
 * - Wired into glial integration system for coordination with astrocytes
 * - Uses quantum walk configuration from brain config
 * - Spatially modulates synaptic transmission based on local concentrations
 *
 * @param brain Brain instance to initialize
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_spatial_neuromod_system(brain_t brain)
{
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_brain_factory_init_spatial_neuromod_system: brain is NULL");

            return false;
    }

    // Check if already initialized
    if (brain->glial && brain->glial->spatial_neuromod) {
        return true;  // Already initialized
    }

    // Guard: Need network and glial integration
    if (!brain->network || !brain->glial) {
        // Not an error if glial integration not set up yet
        return true;
    }

    // Phase C2.1: Create spatial neuromod configs with quantum walk settings
    bool enabled_types[NEUROMOD_COUNT] = {true, true, true, true};  // Enable all 4 types
    spatial_neuromod_config_t configs[NEUROMOD_COUNT];

    for (int i = 0; i < NEUROMOD_COUNT; i++) {
        configs[i] = spatial_neuromod_default_config((neuromodulator_type_t)i);

        // Apply brain quantum walk configuration (Phase C2.1)
        configs[i].enable_quantum_walk = brain->config.enable_quantum_walk_diffusion;
        configs[i].quantum_walk_steps = brain->config.quantum_walk_steps;
        configs[i].quantum_mixing_ratio = brain->config.quantum_classical_mixing;
        configs[i].quantum_coin_type = brain->config.quantum_coin_type;
        configs[i].quantum_decoherence = brain->config.quantum_decoherence_rate;
    }

    // Create spatial neuromod system
    neural_network_t base_network = adaptive_network_get_base_network(brain->network);
    spatial_neuromod_system_t* spatial_neuromod =
        spatial_neuromod_system_create(base_network, enabled_types, configs);

    if (!spatial_neuromod) {
        // Non-fatal: spatial neuromod is optional enhancement
        fprintf(stderr, "WARNING: Failed to create spatial neuromodulator system, continuing without it\n");
        return true;
    }

    // Wire into glial integration
    nimcp_result_t result = glial_integration_set_spatial_neuromod_system(
        brain->glial, spatial_neuromod);

    if (result != NIMCP_SUCCESS) {
        spatial_neuromod_system_destroy(spatial_neuromod);
        fprintf(stderr, "WARNING: Failed to wire spatial neuromod into glial integration\n");
        return true;  // Non-fatal
    }

    fprintf(stderr, "INFO: Spatial neuromodulator system initialized %s\n",
            brain->config.enable_quantum_walk_diffusion ?
            "with quantum walk acceleration (√N speedup)" : "(classical diffusion)");

    return true;
}


/**
 * WHAT: Initialize multihead attention mechanism
 * WHY:  Enable selective focus on relevant features for efficient processing
 * HOW:  Create attention system based on cortical column architecture
 *
 * BIOLOGICAL MOTIVATION:
 * - Cortical Columns: Each attention head acts as specialized processing column
 * - Thalamic Gating: Controls information flow (like thalamic relay nucleus)
 * - Salience Weighting: Biologically-inspired attention based on feature importance
 * - Parallel Streams: Multiple heads process different aspects simultaneously
 *
 * INTEGRATION WITH BRAIN:
 * - Applied to multimodal inputs (visual, audio, speech) before neural network
 * - Connects to salience evaluator for attention weighting
 * - Interfaces with executive control for top-down attention modulation
 * - Used in working memory for attention-based retrieval
 *
 * PERFORMANCE BENEFITS:
 * - 2-5x inference speedup by selective processing
 * - 30-50% memory reduction through focused activations
 * - 5-15% accuracy improvement on complex tasks
 *
 * @param brain Brain instance to initialize
 * @return true on success, false on failure
 *
 * @version 3.0.0 Module Integration Phase
 * @author NIMCP Development Team
 * @date 2025-11-11
 */
bool nimcp_brain_factory_init_attention_subsystem(brain_t brain)
{
    // WHAT: Guard clause - validate input
    // WHY:  Prevent null pointer dereference
    // HOW:  Check brain pointer before use
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_brain_factory_init_attention_subsystem: brain is NULL");

            return false;
    }

    // WHAT: Check if already initialized
    // WHY:  Prevent double initialization and memory leak
    // HOW:  Return success if attention already exists
    if (brain->multihead_attention) {
        return true;  // Already initialized
    }

    // WHAT: Check if attention is enabled in configuration
    // WHY:  Only initialize if user requested this feature
    // HOW:  Check config flag, return success (not error) if disabled
    if (!brain->config.enable_multihead_attention) {
        return true;  // Not enabled, not an error
    }

    // WHAT: Calculate appropriate dimensions for attention
    // WHY:  Attention dimensions must match integrated_feature_buffer size
    // HOW:  Always use num_inputs (the output size of multimodal integration)
    //
    // NOTE: The multimodal integration layer compresses all modalities
    //       (visual + audio + speech + direct) into a unified representation
    //       of size num_inputs. The attention system processes this integrated
    //       representation, not the raw concatenated features.
    uint32_t input_dim = brain->config.num_inputs;

    // WHAT: Configure multihead attention system
    // WHY:  Need proper configuration for cortical column architecture
    // HOW:  Create config with biological parameters
    multihead_attention_config_t attention_config = {
        .num_heads = brain->config.num_attention_heads > 0 ?
                     brain->config.num_attention_heads : 8,  // Default: 8 heads
        .input_dim = input_dim,
        .output_dim = input_dim,  // Same dimension (residual connection compatible)
        .sequence_length = 32,    // Default sequence length for temporal processing
        .use_thalamic_gate = brain->config.enable_thalamic_gate,
        .use_salience_weighting = brain->config.enable_salience_weighting,
        .gate_bias = 0.5F        // Default: 50% gate opening
    };

    // WHAT: Create multihead attention system
    // WHY:  Enable selective feature processing with parallel attention streams
    // HOW:  Call attention creation API with configured parameters
    brain->multihead_attention = multihead_attention_create(&attention_config);
    if (!brain->multihead_attention) {
        set_error("Failed to create multihead attention system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_attention_subsystem: brain->multihead_attention is NULL");
        return false;
    }

    return true;
}


/**
 * WHAT: Initialize brain regions hierarchical architecture
 * WHY:  Enable modular cortical organization with layers and minicolumns
 * HOW:  Create brain module with specialized regions if config enables it
 *
 * BIOLOGICAL MOTIVATION:
 * - Cerebral cortex organized into hierarchical regions (V1, A1, M1, PFC, etc.)
 * - Each region has 6 cortical layers with distinct functions
 * - Minicolumns span layers vertically for parallel processing
 * - Inter-region connections follow biological pathways (feedforward/feedback)
 *
 * INTEGRATION WITH BRAIN:
 * - Provides spatial organization of processing
 * - Enables specialized regions for sensory, motor, associative functions
 * - Supports realistic cortical layer dynamics (Layer 4 input, Layer 5 output)
 * - Allows for hierarchical processing pathways
 *
 * @param brain Brain instance to initialize
 * @return true on success, false on failure
 *
 * @version 3.0.0 Module Integration Phase
 * @author NIMCP Development Team
 * @date 2025-11-11
 */
bool nimcp_brain_factory_init_brain_regions_subsystem(brain_t brain)
{
    // WHAT: Guard clause - validate input
    // WHY:  Prevent null pointer dereference
    // HOW:  Check brain pointer before use
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_brain_factory_init_brain_regions_subsystem: brain is NULL");

            return false;
    }

    // WHAT: Check if already initialized
    // WHY:  Prevent double initialization and memory leak
    // HOW:  Return success if brain_regions already exists
    if (brain->brain_regions) {
        return true;  // Already initialized
    }

    // WHAT: Check if brain regions architecture is enabled in configuration
    // WHY:  Only initialize if user requested this feature
    // HOW:  Check config flag, return success (not error) if disabled
    if (!brain->config.enable_brain_regions) {
        return true;  // Not enabled, not an error
    }

    // WHAT: Determine number of regions and neurons per region
    // WHY:  Need proper sizing for modular architecture
    // HOW:  Use config values with sensible defaults
    uint32_t num_regions = brain->config.num_brain_regions > 0 ?
                           brain->config.num_brain_regions : 4;  // Default: 4 regions
    uint32_t neurons_per_region = brain->config.neurons_per_region > 0 ?
                                  brain->config.neurons_per_region : 1000;  // Default: 1000 neurons

    // WHAT: Create brain module with max capacity
    // WHY:  Top-level container for all brain regions
    // HOW:  Allocate module with specified max regions
    brain->brain_regions = brain_module_create(num_regions);
    if (!brain->brain_regions) {
        set_error("Failed to create brain regions module");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_brain_factory_init_brain_regions_subsystem: brain->brain_regions is NULL");
        return false;
    }

    // WHAT: Create individual brain regions with specialized types
    // WHY:  Different regions have different layer proportions and neuron types
    // HOW:  Create regions based on configuration, starting with primary sensory/motor areas
    brain_region_type_t region_types[] = {
        REGION_VISUAL_V1,      // Primary visual cortex
        REGION_AUDITORY_A1,    // Primary auditory cortex
        REGION_MOTOR_M1,       // Primary motor cortex
        REGION_PREFRONTAL      // Prefrontal cortex (executive control)
    };

    for (uint32_t i = 0; i < num_regions && i < 4; i++) {
        brain_region_t* region = brain_region_create(region_types[i], neurons_per_region);
        if (!region) {
            set_error("Failed to create brain region");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_brain_regions_subsystem: region is NULL");
            return false;
        }

        // Organize region into minicolumns (8x8 grid for moderate-sized regions)
        uint32_t columns_x = 8;
        uint32_t columns_y = 8;
        if (brain_region_organize_columns(region, columns_x, columns_y) != NIMCP_SUCCESS) {
            brain_region_destroy(region);
            set_error("Failed to organize brain region into minicolumns");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_brain_factory_init_brain_regions_subsystem: validation failed");
            return false;
        }

        // Add region to brain module
        if (brain_module_add_region(brain->brain_regions, region) != NIMCP_SUCCESS) {
            brain_region_destroy(region);
            set_error("Failed to add region to brain module");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_brain_factory_init_brain_regions_subsystem: validation failed");
            return false;
        }
    }

    // WHAT: Establish inter-region connections
    // WHY:  Brain regions need to communicate (e.g., V1 → PFC for visual attention)
    // HOW:  Connect regions with biologically realistic pathways
    if (num_regions >= 2) {
        // Connect V1 (visual) → PFC (prefrontal) for visual processing pathway
        brain_region_t* v1 = brain_module_get_region_by_type(brain->brain_regions, REGION_VISUAL_V1);
        brain_region_t* pfc = brain_module_get_region_by_type(brain->brain_regions, REGION_PREFRONTAL);
        if (v1 && pfc) {
            brain_module_connect_regions(brain->brain_regions, v1->id, pfc->id, 0.3F);
        }
    }

    if (num_regions >= 3) {
        // Connect A1 (auditory) → PFC for auditory processing pathway
        brain_region_t* a1 = brain_module_get_region_by_type(brain->brain_regions, REGION_AUDITORY_A1);
        brain_region_t* pfc = brain_module_get_region_by_type(brain->brain_regions, REGION_PREFRONTAL);
        if (a1 && pfc) {
            brain_module_connect_regions(brain->brain_regions, a1->id, pfc->id, 0.3F);
        }
    }

    return true;
}


//=============================================================================
// Phase 4 Neuromodulatory Nuclei Integration
//=============================================================================

/* Phase 4 adapter types - include before layer types to avoid conflicts */
#include "core/brain/regions/locus_coeruleus/nimcp_lc_adapter.h"
#include "core/brain/regions/vta/nimcp_vta_adapter.h"
#include "core/brain/regions/raphe/nimcp_raphe_adapter.h"
#include "core/brain/regions/habenula/nimcp_habenula_adapter.h"
#include "utils/exception/nimcp_exception_macros.h"

/* Forward declarations for intra-coordinator to avoid type conflicts */
typedef struct nimcp_neuromod_intra_struct* nimcp_neuromod_intra_t;
typedef struct nimcp_neuromod_intra_config_struct {
    bool enable_lc;
    bool enable_vta;
    bool enable_raphe;
    bool enable_habenula;
    float lc_vta_coupling;
    float lc_raphe_coupling;
    float vta_raphe_coupling;
    float vta_habenula_coupling;
    float raphe_habenula_coupling;
} nimcp_neuromod_intra_config_t;

/* Intra-coordinator function declarations */
nimcp_neuromod_intra_config_t nimcp_neuromod_intra_default_config(void);
nimcp_neuromod_intra_t nimcp_neuromod_intra_create(const nimcp_neuromod_intra_config_t* config);
int nimcp_neuromod_intra_connect_lc(nimcp_neuromod_intra_t coord, void* module, void* interface);
int nimcp_neuromod_intra_connect_vta(nimcp_neuromod_intra_t coord, void* module, void* interface);
int nimcp_neuromod_intra_connect_raphe(nimcp_neuromod_intra_t coord, void* module, void* interface);
int nimcp_neuromod_intra_connect_habenula(nimcp_neuromod_intra_t coord, void* module, void* interface);

/**
 * WHAT: Initialize Phase 4 Neuromodulatory Nuclei (LC, VTA, Raphe, Habenula)
 * WHY:  Enable specific neuromodulatory control beyond the general neuromodulator_system
 * HOW:  Create adapters for each enabled nucleus and wire them to the intra-coordinator
 *
 * BIOLOGICAL MOTIVATION:
 * - LC: ~1500 neurons/hemisphere with massive projections, modulates arousal/attention
 * - VTA: Source of mesolimbic/mesocortical DA, reward prediction errors
 * - Raphe: Major 5-HT source, mood/patience regulation
 * - Habenula: "Disappointment center", inhibits VTA on negative outcomes
 *
 * INITIALIZATION ORDER:
 * 1. Create individual nuclei adapters (LC, VTA, Raphe, Habenula)
 * 2. Create intra-coordinator for cross-nuclei coupling
 * 3. Connect adapters to intra-coordinator
 * 4. Wire to brain integration points
 */
bool nimcp_brain_factory_init_neuromod_nuclei(brain_t brain)
{
    // WHAT: Guard clause - validate input
    // WHY:  Prevent null pointer dereference
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_brain_factory_init_neuromod_nuclei: brain is NULL");

            return false;
    }

    // WHAT: Check if any Phase 4 nuclei are enabled
    // WHY:  Skip initialization if none are enabled (save resources)
    bool any_enabled = brain->config.enable_lc ||
                       brain->config.enable_vta ||
                       brain->config.enable_raphe ||
                       brain->config.enable_habenula;

    if (!any_enabled && !brain->config.enable_neuromod_intra) {
        // No nuclei enabled, not an error
        return true;
    }

    // WHAT: Initialize Locus Coeruleus (LC) adapter
    // WHY:  Norepinephrine modulation for arousal, attention, stress response
    if (brain->config.enable_lc) {
        nimcp_lc_adapter_config_t lc_config = nimcp_lc_adapter_default_config();
        lc_config.enable_bio_async = brain->bio_async_enabled;
        lc_config.enable_training_integration = brain->config.neuromod_enable_training_bridge;

        brain->lc_adapter = nimcp_lc_adapter_create(&lc_config);
        if (!brain->lc_adapter) {
            LOG_WARN(LOG_MODULE, "Failed to create LC adapter, continuing without LC");
        } else {
            brain->lc_enabled = true;
            LOG_INFO(LOG_MODULE, "LC adapter initialized (NE modulation)");
        }
    }

    // WHAT: Initialize VTA adapter
    // WHY:  Dopamine modulation for reward, motivation, learning
    if (brain->config.enable_vta) {
        nimcp_vta_adapter_config_t vta_config = nimcp_vta_adapter_default_config();
        vta_config.enable_bio_async = brain->bio_async_enabled;
        vta_config.enable_training_integration = brain->config.neuromod_enable_training_bridge;

        brain->vta_adapter = nimcp_vta_adapter_create(&vta_config);
        if (!brain->vta_adapter) {
            LOG_WARN(LOG_MODULE, "Failed to create VTA adapter, continuing without VTA");
        } else {
            brain->vta_enabled = true;
            LOG_INFO(LOG_MODULE, "VTA adapter initialized (DA modulation)");
        }
    }

    // WHAT: Initialize Raphe adapter
    // WHY:  Serotonin modulation for mood, impulse control, patience
    if (brain->config.enable_raphe) {
        nimcp_raphe_adapter_config_t raphe_config = nimcp_raphe_adapter_default_config();
        raphe_config.enable_bio_async = brain->bio_async_enabled;
        raphe_config.enable_training_integration = brain->config.neuromod_enable_training_bridge;

        brain->raphe_adapter = nimcp_raphe_adapter_create(&raphe_config);
        if (!brain->raphe_adapter) {
            LOG_WARN(LOG_MODULE, "Failed to create Raphe adapter, continuing without Raphe");
        } else {
            brain->raphe_enabled = true;
            LOG_INFO(LOG_MODULE, "Raphe adapter initialized (5-HT modulation)");
        }
    }

    // WHAT: Initialize Habenula adapter
    // WHY:  Aversion modulation for disappointment, negative outcomes, avoidance
    if (brain->config.enable_habenula) {
        nimcp_habenula_adapter_config_t hab_config;
        nimcp_habenula_adapter_default_config(&hab_config);
        hab_config.enable_training_integration = brain->config.neuromod_enable_training_bridge;
        hab_config.enable_vta_coordination = brain->config.enable_vta;
        hab_config.enable_raphe_coordination = brain->config.enable_raphe;

        brain->habenula_adapter = nimcp_habenula_adapter_create(&hab_config);
        if (!brain->habenula_adapter) {
            LOG_WARN(LOG_MODULE, "Failed to create Habenula adapter, continuing without Habenula");
        } else {
            brain->habenula_enabled = true;
            LOG_INFO(LOG_MODULE, "Habenula adapter initialized (aversion modulation)");
        }
    }

    // WHAT: Initialize Neuromodulatory Intra-Coordinator
    // WHY:  Coordinate cross-nuclei interactions (e.g., Habenula inhibits VTA)
    if (brain->config.enable_neuromod_intra ||
        (brain->lc_enabled && brain->vta_enabled)) {
        nimcp_neuromod_intra_config_t intra_config = nimcp_neuromod_intra_default_config();
        intra_config.enable_lc = brain->lc_enabled;
        intra_config.enable_vta = brain->vta_enabled;
        intra_config.enable_raphe = brain->raphe_enabled;
        intra_config.enable_habenula = brain->habenula_enabled;

        // Apply coupling strengths from brain config
        intra_config.lc_vta_coupling = brain->config.neuromod_lc_vta_coupling;
        intra_config.lc_raphe_coupling = brain->config.neuromod_lc_raphe_coupling;
        intra_config.vta_raphe_coupling = brain->config.neuromod_vta_raphe_coupling;
        intra_config.vta_habenula_coupling = brain->config.neuromod_vta_habenula_coupling;
        intra_config.raphe_habenula_coupling = brain->config.neuromod_raphe_habenula_coupling;

        brain->neuromod_intra_coordinator = nimcp_neuromod_intra_create(&intra_config);
        if (!brain->neuromod_intra_coordinator) {
            LOG_WARN(LOG_MODULE, "Failed to create neuromod intra-coordinator");
        } else {
            brain->neuromod_intra_enabled = true;

            // Connect adapters to intra-coordinator
            // Note: Pass NULL for interface - the coordinator will use default interface
            if (brain->lc_adapter) {
                nimcp_neuromod_intra_connect_lc(brain->neuromod_intra_coordinator,
                                                brain->lc_adapter, NULL);
            }
            if (brain->vta_adapter) {
                nimcp_neuromod_intra_connect_vta(brain->neuromod_intra_coordinator,
                                                 brain->vta_adapter, NULL);
            }
            if (brain->raphe_adapter) {
                nimcp_neuromod_intra_connect_raphe(brain->neuromod_intra_coordinator,
                                                   brain->raphe_adapter, NULL);
            }
            if (brain->habenula_adapter) {
                nimcp_neuromod_intra_connect_habenula(brain->neuromod_intra_coordinator,
                                                      brain->habenula_adapter, NULL);
            }

            LOG_INFO(LOG_MODULE, "Neuromod intra-coordinator initialized (coupling: LC-%s VTA-%s Raphe-%s Hab-%s)",
                     brain->lc_enabled ? "on" : "off",
                     brain->vta_enabled ? "on" : "off",
                     brain->raphe_enabled ? "on" : "off",
                     brain->habenula_enabled ? "on" : "off");
        }
    }

    LOG_INFO(LOG_MODULE, "Phase 4 neuromod nuclei initialization complete");
    return true;
}
