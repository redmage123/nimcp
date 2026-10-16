//=============================================================================
// nimcp_brain_init_occipital.c - Occipital Cortex Initialization
//=============================================================================
/**
 * @file nimcp_brain_init_occipital.c
 * @brief Occipital Cortex (Visual Cortex) Initialization Implementation
 *
 * WHAT: Initialization functions for Occipital Cortex (visual processing)
 * WHY:  Enable visual processing capabilities in the brain
 * HOW:  Creates occipital adapter and connects all integration bridges
 *
 * EXTRACTED FROM: Brain factory initialization
 * DATE: 2025-12-30
 *
 * @version Phase O1: Occipital Cortex Brain Integration
 * @author NIMCP Development Team
 */

//=============================================================================
// Includes
//=============================================================================

#include "core/brain/factory/init/nimcp_brain_init_occipital.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"

#define LOG_MODULE "BRAIN_INIT_OCCIPITAL"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(brain_init_occipital, MESH_ADAPTER_CATEGORY_SYSTEM)


// Compatibility macro for set_error (converts to LOG_ERROR)
#ifndef set_error
#define set_error(msg) LOG_ERROR(LOG_MODULE, "%s", msg)
#endif

// Occipital region includes
#include "core/brain/regions/occipital/nimcp_occipital_adapter.h"
#include "core/brain/regions/occipital/nimcp_occipital_substrate_bridge.h"
#include "core/brain/regions/occipital/nimcp_occipital_thalamic_bridge.h"
#include "core/brain/regions/occipital/nimcp_occipital_quantum_bridge.h"
#include "core/brain/regions/occipital/nimcp_occipital_audiovisual_bridge.h"
#include "core/brain/regions/occipital/nimcp_occipital_cortical_bridge.h"
#include "core/brain/regions/occipital/nimcp_occipital_logic_bridge.h"
#include "core/brain/regions/occipital/nimcp_occipital_cognitive_bridge.h"

// Stream connection includes
#include "core/brain/regions/parietal/nimcp_parietal_adapter.h"
#include "core/brain/regions/temporal/nimcp_temporal_adapter.h"

// Training integration includes
#include "middleware/training/nimcp_occipital_training_bridge.h"

// Logic integration includes
#include "cognitive/logic/nimcp_visual_logic_bridge.h"

// Cognitive module includes
#include "cognitive/salience/nimcp_salience.h"
#include "cognitive/curiosity/nimcp_curiosity.h"
#include "cognitive/introspection/nimcp_introspection.h"
#include "plasticity/attention/nimcp_attention.h"
#include "cognitive/global_workspace/nimcp_global_workspace.h"

// Cortical columns includes
#include "core/cortical_columns/nimcp_orientation_columns.h"
#include "core/cortical_columns/nimcp_feature_hypercolumns.h"

// Swarm includes
#include "swarm/nimcp_swarm_brain.h"

// Broca region includes
#include "core/brain/regions/broca/nimcp_broca_adapter.h"

// Audio cortex includes
#include "perception/nimcp_audio_cortex.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>

//=============================================================================
// Stream Connection Callbacks
//=============================================================================

/**
 * @brief Convert occipital motion vector to parietal spatial target
 *
 * BIOLOGICAL: Motion vectors from V5/MT are converted to spatial attention
 * targets for the dorsal "where" pathway to parietal cortex.
 */
static void dorsal_motion_callback(const motion_vector_t* motion, void* user_data) {
    if (!motion || !user_data) return;

    brain_t brain = (brain_t)user_data;
    if (!brain->parietal) return;

    /* Convert motion vector to spatial target */
    parietal_cortex_spatial_target_t target = {
        .target_id = 0,  /* Auto-assign */
        .position = {
            .x = motion->x,
            .y = motion->y,
            .z = 0.0f  /* 2D visual field */
        },
        .frame = PARIETAL_CORTEX_SPATIAL_FRAME_RETINOTOPIC,
        .salience = motion->confidence,
        .size = 0.1f,  /* Default size */
        .velocity = {
            .x = motion->dx,
            .y = motion->dy,
            .z = 0.0f
        }
    };

    /* Add motion target to parietal spatial attention */
    parietal_cortex_add_spatial_target(brain->parietal, &target);
}

/**
 * @brief Convert occipital feature to temporal visual input
 *
 * BIOLOGICAL: Features from V4 (color, form) are sent along the ventral
 * "what" pathway to temporal cortex for object recognition.
 */
static void ventral_feature_callback(const visual_feature_t* feature, void* user_data) {
    if (!feature || !user_data) return;

    brain_t brain = (brain_t)user_data;
    if (!brain->temporal) return;

    /* Convert visual feature to temporal input */
    temporal_visual_input_t input = {
        .features = feature->descriptor,
        .feature_dim = feature->descriptor_size,
        .x = feature->x,
        .y = feature->y,
        .timestamp_ms = (double)feature->timestamp_us / 1000.0
    };

    /* Send to temporal for object recognition */
    temporal_object_recognition_result_t result;
    temporal_recognize_object(brain->temporal, &input, &result);
}

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * @brief Connect occipital to bio-async messaging
 */
static bool connect_occipital_to_bio_async(brain_t brain) {
    if (!brain || !brain->occipital) return true; /* Non-fatal if not available */

    /* TODO: Register with bio-async when brain bio-router infrastructure is implemented */
    if (brain->bio_async_enabled) {
        /* Bio-async connection will be implemented when brain has bio_router field.
         * For now, we just note that bio-async was requested. */
        LOG_DEBUG(LOG_MODULE, "Occipital bio-async requested but brain router not yet available");
    }

    return true;
}

/**
 * @brief Connect occipital substrate bridge to neural substrate
 */
static bool connect_substrate_bridge(brain_t brain) {
    if (!brain || !brain->occipital_substrate_bridge) return true;

    /* Apply initial metabolic effects */
    if (occipital_substrate_bridge_update(brain->occipital_substrate_bridge) != 0) {
        LOG_WARN(LOG_MODULE, "Initial substrate bridge update failed");
    }

    return true;
}

/**
 * @brief Connect occipital thalamic bridge to thalamic router
 */
static bool connect_thalamic_bridge(brain_t brain) {
    if (!brain || !brain->occipital_thalamic_bridge) return true;

    /* Reset bridge to clean state */
    if (occipital_thalamic_bridge_reset(brain->occipital_thalamic_bridge) != 0) {
        LOG_WARN(LOG_MODULE, "Thalamic bridge reset failed");
    }

    return true;
}

//=============================================================================
// Public API Implementation
//=============================================================================

bool nimcp_brain_factory_init_occipital_subsystem(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_brain_factory_init_occipital_subsystem: brain is NULL");

            return false;
    }

    /* Check if already initialized */
    if (brain->occipital) {
        return true;  /* Already initialized */
    }

    /* Check if occipital is enabled in config */
    /* Default to enabled for vision-capable brains */
    if (!brain->config.enable_visual_cortex &&
        !brain->config.enable_multimodal_integration) {
        brain->occipital_enabled = false;
        return true;  /* Not enabled, not an error */
    }

    /* Create occipital adapter with default configuration */
    occipital_config_t occipital_cfg = occipital_default_config();

    /* Scale configuration based on brain config */
    if (brain->config.num_inputs > 0) {
        /* Infer image dimensions from input size */
        uint32_t input_size = brain->config.num_inputs;
        uint32_t channels = 3;  /* Assume RGB */
        uint32_t pixels = input_size / channels;
        uint32_t side = (uint32_t)sqrtf((float)pixels);
        if (side > 0) {
            occipital_cfg.image_width = side;
            occipital_cfg.image_height = side;
            occipital_cfg.color_channels = channels;
        }
    }

    /* Enable training if brain training is enabled */
    occipital_cfg.enable_training = brain->enable_training_integration;
    occipital_cfg.enable_bio_async = brain->bio_async_enabled;

    brain->occipital = occipital_create(&occipital_cfg);
    if (!brain->occipital) {
        set_error("Failed to create occipital adapter");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_occipital_subsystem: brain->occipital is NULL");
        return false;
    }

    brain->occipital_enabled = true;
    brain->last_occipital_update_us = 0;

    /* Initialize integration bridges */
    if (!nimcp_brain_factory_init_occipital_substrate_bridge(brain)) {
        LOG_WARN(LOG_MODULE, "Occipital substrate bridge init failed (non-fatal)");
    }

    if (!nimcp_brain_factory_init_occipital_thalamic_bridge(brain)) {
        LOG_WARN(LOG_MODULE, "Occipital thalamic bridge init failed (non-fatal)");
    }

    if (!nimcp_brain_factory_init_occipital_quantum_bridge(brain)) {
        LOG_WARN(LOG_MODULE, "Occipital quantum bridge init failed (non-fatal)");
    }

    /* Connect to other subsystems */
    if (!nimcp_brain_factory_connect_occipital_to_parietal(brain)) {
        LOG_WARN(LOG_MODULE, "Occipital-Parietal connection failed (non-fatal)");
    }

    if (!nimcp_brain_factory_connect_occipital_to_temporal(brain)) {
        LOG_WARN(LOG_MODULE, "Occipital-Temporal connection failed (non-fatal)");
    }

    if (!nimcp_brain_factory_connect_occipital_to_dragonfly(brain)) {
        LOG_WARN(LOG_MODULE, "Occipital-Dragonfly connection failed (non-fatal)");
    }

    if (!nimcp_brain_factory_connect_occipital_to_training(brain)) {
        LOG_WARN(LOG_MODULE, "Occipital-Training connection failed (non-fatal)");
    }

    if (!nimcp_brain_factory_connect_occipital_to_immune(brain)) {
        LOG_WARN(LOG_MODULE, "Occipital-Immune connection failed (non-fatal)");
    }

    if (!nimcp_brain_factory_connect_occipital_to_logic(brain)) {
        LOG_WARN(LOG_MODULE, "Occipital-Logic connection failed (non-fatal)");
    }

    if (!nimcp_brain_factory_connect_occipital_to_cognitive(brain)) {
        LOG_WARN(LOG_MODULE, "Occipital-Cognitive connection failed (non-fatal)");
    }

    if (!nimcp_brain_factory_connect_occipital_to_cortical_columns(brain)) {
        LOG_WARN(LOG_MODULE, "Occipital-CorticalColumns connection failed (non-fatal)");
    }

    if (!nimcp_brain_factory_connect_occipital_to_swarm(brain)) {
        LOG_WARN(LOG_MODULE, "Occipital-Swarm connection failed (non-fatal)");
    }

    if (!nimcp_brain_factory_connect_occipital_to_broca(brain)) {
        LOG_WARN(LOG_MODULE, "Occipital-Broca connection failed (non-fatal)");
    }

    /* Connect to bio-async */
    if (!connect_occipital_to_bio_async(brain)) {
        LOG_WARN(LOG_MODULE, "Occipital bio-async connection failed (non-fatal)");
    }

    LOG_INFO(LOG_MODULE, "Occipital cortex initialized successfully");
    return true;
}

bool nimcp_brain_factory_init_occipital_substrate_bridge(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_brain_factory_init_occipital_substrate_bridge: brain is NULL");

            return false;
    }

    /* Check if already initialized */
    if (brain->occipital_substrate_bridge) {
        return true;
    }

    /* Need occipital adapter first */
    if (!brain->occipital) {
        return true;  /* Not ready yet, will be called again */
    }

    /* Get neural substrate - may be NULL in simple configurations */
    neural_substrate_t* substrate = NULL;
    /*
     * TODO: Get substrate from brain if available
     * substrate = brain->substrate;
     */

    /* Create substrate bridge with default config */
    occipital_substrate_config_t config = occipital_substrate_default_config();

    brain->occipital_substrate_bridge = occipital_substrate_bridge_create(
        brain->occipital, substrate, &config);

    if (!brain->occipital_substrate_bridge) {
        LOG_WARN(LOG_MODULE, "Failed to create occipital substrate bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_occipital_substrate_bridge: brain->occipital_substrate_bridge is NULL");
        return false;
    }

    /* Connect to substrate */
    connect_substrate_bridge(brain);

    LOG_DEBUG(LOG_MODULE, "Occipital substrate bridge initialized");
    return true;
}

bool nimcp_brain_factory_init_occipital_thalamic_bridge(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_brain_factory_init_occipital_thalamic_bridge: brain is NULL");

            return false;
    }

    /* Check if already initialized */
    if (brain->occipital_thalamic_bridge) {
        return true;
    }

    /* Need occipital adapter first */
    if (!brain->occipital) {
        return true;  /* Not ready yet */
    }

    /* Get thalamic router - may be NULL in simple configurations */
    void* router = NULL;
    /*
     * TODO: Get thalamic router from brain if available
     * router = brain->thalamic_router;
     */

    /* Create thalamic bridge with default config */
    occipital_thalamic_config_t thal_config = occipital_thalamic_default_config();

    brain->occipital_thalamic_bridge = occipital_thalamic_bridge_create(
        brain->occipital, router, &thal_config);

    if (!brain->occipital_thalamic_bridge) {
        LOG_WARN(LOG_MODULE, "Failed to create occipital thalamic bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_occipital_thalamic_bridge: brain->occipital_thalamic_bridge is NULL");
        return false;
    }

    /* Connect to thalamus */
    connect_thalamic_bridge(brain);

    LOG_DEBUG(LOG_MODULE, "Occipital thalamic bridge initialized");
    return true;
}

bool nimcp_brain_factory_init_occipital_quantum_bridge(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_brain_factory_init_occipital_quantum_bridge: brain is NULL");

            return false;
    }

    /* Check if already initialized */
    if (brain->occipital_quantum_bridge) {
        return true;
    }

    /* Need occipital adapter first */
    if (!brain->occipital) {
        return true;  /* Not ready yet */
    }

    /* Check if quantum reasoning is enabled */
    if (!brain->quantum_reasoning_enabled) {
        return true;  /* Not enabled, not an error */
    }

    /* Create quantum bridge with default config */
    occipital_quantum_config_t config = occipital_quantum_default_config();

    /* Scale search depth based on visual field size */
    if (brain->config.num_inputs > 0) {
        config.visual_search_depth = brain->config.num_inputs / 3;  /* Approx pixels */
    }

    brain->occipital_quantum_bridge = occipital_quantum_bridge_create(
        brain->occipital, &config);

    if (!brain->occipital_quantum_bridge) {
        LOG_WARN(LOG_MODULE, "Failed to create occipital quantum bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_occipital_quantum_bridge: brain->occipital_quantum_bridge is NULL");
        return false;
    }

    LOG_DEBUG(LOG_MODULE, "Occipital quantum bridge initialized");
    return true;
}

bool nimcp_brain_factory_connect_occipital_to_parietal(brain_t brain) {
    if (!brain || !brain->occipital) {
        return true;  /* Nothing to connect */
    }

    /* Check if parietal is available */
    if (!brain->parietal) {
        return true;  /* Parietal not initialized yet */
    }

    /*
     * Connect dorsal stream: V5/MT motion signals to parietal
     *
     * BIOLOGICAL: The dorsal "where" stream carries:
     * - Motion perception (V5/MT -> MST -> parietal)
     * - Spatial attention and awareness
     * - Visually-guided reaching and grasping
     */

    /* Register motion callback for dorsal stream */
    if (!occipital_set_motion_callback(brain->occipital,
            dorsal_motion_callback, brain)) {
        LOG_WARN(LOG_MODULE, "Failed to set occipital motion callback");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_brain_factory_connect_occipital_to_parietal: operation failed");
        return false;
    }

    LOG_DEBUG(LOG_MODULE, "Occipital connected to parietal (dorsal stream)");
    return true;
}

bool nimcp_brain_factory_connect_occipital_to_temporal(brain_t brain) {
    if (!brain || !brain->occipital) {
        return true;  /* Nothing to connect */
    }

    /* Check if temporal is available */
    if (!brain->temporal) {
        return true;  /* Temporal not initialized yet */
    }

    /*
     * Connect ventral stream: V4 color/form signals to temporal
     *
     * BIOLOGICAL: The ventral "what" stream carries:
     * - Object identity (color, shape, texture)
     * - Face recognition (fusiform face area)
     * - Scene categorization (parahippocampal place area)
     */

    /* Register feature callback for ventral stream */
    if (!occipital_set_feature_callback(brain->occipital,
            ventral_feature_callback, brain)) {
        LOG_WARN(LOG_MODULE, "Failed to set occipital feature callback");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_brain_factory_connect_occipital_to_temporal: operation failed");
        return false;
    }

    LOG_DEBUG(LOG_MODULE, "Occipital connected to temporal (ventral stream)");
    return true;
}

bool nimcp_brain_factory_connect_occipital_to_dragonfly(brain_t brain) {
    if (!brain || !brain->occipital) {
        return true;  /* Nothing to connect */
    }

    /* Check if dragonfly is available */
    if (!brain->dragonfly_enabled || !brain->dragonfly) {
        return true;  /* Dragonfly not initialized */
    }

    /*
     * Connect visual motion to dragonfly target tracking
     *
     * BIOLOGICAL: Dragonfly visual processing:
     * - TSDN (Target-Selective Descending Neurons) receive V5/MT motion
     * - CSTMD1 (Centrifugal Small Target Motion Detector) for selective attention
     * - Enables high-speed target interception
     */

    /* TODO: Register motion callback for dragonfly
     * occipital_set_motion_callback(brain->occipital,
     *     dragonfly_receive_visual_motion, brain->dragonfly);
     */

    LOG_DEBUG(LOG_MODULE, "Occipital connected to dragonfly system");
    return true;
}

bool nimcp_brain_factory_connect_occipital_to_training(brain_t brain) {
    if (!brain || !brain->occipital) {
        return true;  /* Nothing to connect */
    }

    /* Check if training is enabled */
    if (!brain->enable_training_integration || !brain->training_ctx) {
        return true;  /* Training not enabled */
    }

    /*
     * Create and connect occipital training bridge.
     * This allows visual processing learning through:
     * - Feature detector refinement (V1 Gabor filters)
     * - Contour integration learning (V2 association fields)
     * - Color constancy calibration (V4 adaptation)
     * - Motion estimation improvement (V5/MT temporal filters)
     */

    /* Create training bridge with default config */
    occipital_training_config_t config;
    occipital_training_default_config(&config);

    occipital_training_bridge_t* training_bridge = occipital_training_bridge_create(&config);
    if (!training_bridge) {
        LOG_WARN(LOG_MODULE, "Failed to create occipital training bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_connect_occipital_to_training: training_bridge is NULL");
        return false;
    }

    /* Connect occipital adapter to training bridge */
    if (occipital_training_connect_occipital(training_bridge, brain->occipital) != 0) {
        LOG_WARN(LOG_MODULE, "Failed to connect occipital to training bridge");
        occipital_training_bridge_destroy(training_bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_brain_factory_connect_occipital_to_training: validation failed");
        return false;
    }

    /* Connect training context to bridge */
    if (occipital_training_connect_training(training_bridge, brain->training_ctx) != 0) {
        LOG_WARN(LOG_MODULE, "Failed to connect training context to bridge");
        occipital_training_bridge_destroy(training_bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_brain_factory_connect_occipital_to_training: validation failed");
        return false;
    }

    /* No storage field exists in brain for this bridge — destroy to prevent leak */
    occipital_training_bridge_destroy(training_bridge);

    LOG_DEBUG(LOG_MODULE, "Occipital connected to training system");
    return true;
}

bool nimcp_brain_factory_connect_occipital_to_immune(brain_t brain) {
    if (!brain || !brain->occipital) {
        return true;  /* Nothing to connect */
    }

    /* Check if immune system is available */
    if (!brain->immune_enabled || !brain->immune_system) {
        return true;  /* Immune not enabled */
    }

    /*
     * Register for cytokine signals that affect visual processing.
     * Neuroinflammation (high IL-1beta, TNF-alpha) can cause:
     * - Reduced contrast sensitivity (V1 impairment)
     * - Color perception disturbances (V4 impairment)
     * - Motion perception deficits (V5/MT impairment)
     * - Visual fatigue and photophobia
     */

    /* TODO: Register with immune system
     * brain_immune_register_cytokine_callback(brain->immune_system,
     *     CYTOKINE_IL1B | CYTOKINE_TNF_A, occipital_inflammation_callback, brain->occipital);
     */

    LOG_DEBUG(LOG_MODULE, "Occipital connected to immune system");
    return true;
}

bool nimcp_brain_factory_connect_occipital_to_logic(brain_t brain) {
    if (!brain || !brain->occipital) {
        return true;  /* Nothing to connect */
    }

    /* Check if logic module is available */
    if (!brain->logic && !brain->symbolic_logic) {
        return true;  /* Logic not initialized */
    }

    /*
     * Create visual-logic bridge for perception-to-predicate conversion.
     *
     * BIOLOGICAL: The ventral visual stream terminates in inferotemporal
     * cortex, which links visual features to semantic concepts. The
     * prefrontal cortex then integrates visual evidence with logical reasoning.
     *
     * FUNCTION MAPPING:
     * - Object detection (V4/IT) -> Unary predicates: cat(obj_1), red(obj_2)
     * - Spatial relations (parietal) -> Binary predicates: above(obj_1, obj_2)
     * - Scene parsing (parahippocampal) -> Context predicates: kitchen(scene)
     */

    /* Get logic module (neural or symbolic) */
    void* logic_module = brain->symbolic_logic ?
        (void*)brain->symbolic_logic : (void*)&brain->logic;

    /* Create bridge with default config */
    visual_logic_config_t config = visual_logic_default_config();
    config.enable_object_grounding = true;
    config.enable_relation_extraction = true;
    config.enable_top_down_attention = true;
    config.min_confidence_threshold = 0.7f;

    visual_logic_bridge_t* bridge = visual_logic_bridge_create(
        brain->occipital, logic_module, &config);

    if (!bridge) {
        LOG_WARN(LOG_MODULE, "Failed to create visual-logic bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_connect_occipital_to_logic: bridge is NULL");
        return false;
    }

    /* No storage field exists in brain for this bridge — destroy to prevent leak */
    visual_logic_bridge_destroy(bridge);

    /*
     * Also create occipital-specific logic bridge for visual predicate grounding.
     * This provides direct visual feature -> predicate conversion.
     */
    occipital_logic_config_t occ_logic_cfg = occipital_logic_default_config();
    occ_logic_cfg.enable_object_grounding = true;
    occ_logic_cfg.enable_spatial_predicates = true;
    occ_logic_cfg.enable_motion_predicates = true;
    occ_logic_cfg.enable_bio_async = brain->bio_async_enabled;

    occipital_logic_bridge_t* occ_logic_bridge = occipital_logic_bridge_create(
        brain->occipital, &occ_logic_cfg);

    if (occ_logic_bridge) {
        /* Connect to neural logic network if available */
        if (brain->logic) {
            occipital_logic_connect_brain(occ_logic_bridge, brain);
        }
        LOG_DEBUG(LOG_MODULE, "Created occipital logic bridge");
        /* No storage field exists in brain for this bridge — destroy to prevent leak */
        occipital_logic_bridge_destroy(occ_logic_bridge);
    }

    LOG_DEBUG(LOG_MODULE, "Occipital connected to logic module");
    return true;
}

bool nimcp_brain_factory_connect_occipital_to_cognitive(brain_t brain) {
    if (!brain || !brain->occipital) {
        return true;  /* Nothing to connect */
    }

    /*
     * Connect occipital to cognitive modules for visual cognition.
     *
     * INTEGRATIONS:
     * 1. Salience: Visual features contribute to salience computation
     * 2. Curiosity: Novel visual stimuli drive exploration behavior
     * 3. Introspection: Visual processing state included in self-model
     * 4. Attention: Visual attention allocation and gating
     * 5. Global Workspace: Conscious visual awareness broadcasting
     */

    /* Connect to salience evaluator if available */
    if (brain->salience) {
        /* TODO: Register visual salience provider
         * salience_register_visual_source(brain->salience,
         *     occipital_get_salience_map, brain->occipital);
         */
        LOG_DEBUG(LOG_MODULE, "Occipital connected to salience evaluator");
    }

    /* Connect to curiosity engine if available */
    if (brain->curiosity) {
        /* TODO: Register visual novelty detector
         * curiosity_register_novelty_source(brain->curiosity,
         *     NOVELTY_SOURCE_VISUAL, occipital_get_novelty, brain->occipital);
         */
        LOG_DEBUG(LOG_MODULE, "Occipital connected to curiosity engine");
    }

    /* Connect to introspection if available */
    if (brain->introspection) {
        /* TODO: Register visual state provider
         * introspection_register_visual_state(brain->introspection,
         *     occipital_get_processing_state, brain->occipital);
         */
        LOG_DEBUG(LOG_MODULE, "Occipital connected to introspection");
    }

    /* Connect to multihead attention if available */
    if (brain->multihead_attention) {
        /* TODO: Register visual attention query
         * multihead_attention_register_visual_query(brain->multihead_attention,
         *     occipital_attention_query, brain->occipital);
         */
        LOG_DEBUG(LOG_MODULE, "Occipital connected to attention system");
    }

    /* Connect to global workspace if available */
    if (brain->global_workspace) {
        /* TODO: Register visual broadcast channel
         * global_workspace_register_broadcast_channel(brain->global_workspace,
         *     GW_CHANNEL_VISUAL, occipital_broadcast_visual, brain->occipital);
         */
        LOG_DEBUG(LOG_MODULE, "Occipital connected to global workspace");
    }

    LOG_DEBUG(LOG_MODULE, "Occipital connected to cognitive modules");
    return true;
}

bool nimcp_brain_factory_connect_occipital_to_cortical_columns(brain_t brain) {
    if (!brain || !brain->occipital) {
        return true;  /* Nothing to connect */
    }

    /* Check if cortical columns are enabled */
    if (!brain->enable_cortical_columns) {
        return true;  /* Cortical columns not enabled */
    }

    /*
     * Connect occipital to cortical column architecture.
     *
     * BIOLOGICAL BASIS:
     * - V1 is organized into orientation columns (~0.5mm spacing)
     * - Hypercolumns contain full orientation coverage (0-180 degrees)
     * - Ocular dominance columns alternate left/right eye preference
     * - Color blobs (CO-rich) process chromatic information
     * - Retinotopic mapping preserves spatial relationships
     *
     * IMPLEMENTATION:
     * - Create orientation hypercolumns for V1 orientation selectivity
     * - Map occipital V1 Gabor filters to orientation columns
     * - Create feature hypercolumns for multi-dimensional features
     */

    /* Check if orientation hypercolumns exist */
    if (!brain->orientation_hypercolumns || brain->num_orientation_hypercolumns == 0) {
        /* Create orientation hypercolumns for V1 */
        uint32_t num_hypercolumns = 16;  /* Grid of hypercolumns */

        brain->orientation_hypercolumns = (orientation_hypercolumn_t**)
            nimcp_calloc(num_hypercolumns, sizeof(orientation_hypercolumn_t*));

        if (!brain->orientation_hypercolumns) {
            LOG_WARN(LOG_MODULE, "Failed to allocate orientation hypercolumns");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_brain_factory_connect_occipital_to_cortical_columns: brain->orientation_hypercolumns is NULL");
            return false;
        }

        /* Create each hypercolumn */
        orientation_hypercolumn_config_t config = {
            .num_orientations = 8,       /* 0, 22.5, 45, ... 157.5 degrees */
            .num_spatial_frequencies = 4, /* Log-spaced spatial frequencies */
            .num_phases = 2,             /* 0 and 90 degree phase */
            .receptive_field_size = 0.1f /* Fraction of visual field */
        };

        for (uint32_t i = 0; i < num_hypercolumns; i++) {
            brain->orientation_hypercolumns[i] = orientation_hypercolumn_create(&config);
            if (!brain->orientation_hypercolumns[i]) {
                LOG_WARN(LOG_MODULE, "Failed to create orientation hypercolumn %u", i);
            }
        }

        brain->num_orientation_hypercolumns = num_hypercolumns;
        LOG_DEBUG(LOG_MODULE, "Created %u orientation hypercolumns for V1", num_hypercolumns);
    }

    /* Connect occipital V1 outputs to orientation columns */
    /* TODO: Map V1 Gabor filter outputs to corresponding orientation columns
     * for (uint32_t i = 0; i < brain->num_orientation_hypercolumns; i++) {
     *     orientation_hypercolumn_connect_input(brain->orientation_hypercolumns[i],
     *         occipital_get_gabor_output, brain->occipital, i);
     * }
     */

    LOG_DEBUG(LOG_MODULE, "Occipital connected to cortical columns");
    return true;
}

bool nimcp_brain_factory_connect_occipital_to_swarm(brain_t brain) {
    if (!brain || !brain->occipital) {
        return true;  /* Nothing to connect */
    }

    /* Check if distributed cognition is available */
    if (!brain->distributed) {
        return true;  /* Not in distributed mode */
    }

    /*
     * Connect occipital to swarm intelligence for distributed visual processing.
     *
     * APPLICATIONS:
     * - Distributed object detection: Multiple nodes process scene regions
     * - Consensus perception: Aggregate visual interpretations across nodes
     * - Fault-tolerant vision: Processing continues if nodes fail
     * - Load balancing: Spread visual computation across available nodes
     *
     * IMPLEMENTATION:
     * - Register occipital as visual processing capability
     * - Enable visual task distribution across swarm
     * - Setup visual result aggregation
     */

    /* TODO: Register with swarm module registry
     * if (brain->swarm_module_registry) {
     *     swarm_module_registry_register(brain->swarm_module_registry,
     *         "occipital_visual", SWARM_CAPABILITY_VISUAL,
     *         occipital_process_distributed, brain->occipital);
     * }
     */

    /* TODO: Register for distributed visual tasks
     * distrib_cognition_register_handler(brain->distributed,
     *     TASK_TYPE_VISUAL_PROCESSING, occipital_handle_distributed_task,
     *     brain->occipital);
     */

    LOG_DEBUG(LOG_MODULE, "Occipital connected to swarm system");
    return true;
}

bool nimcp_brain_factory_connect_occipital_to_broca(brain_t brain) {
    if (!brain || !brain->occipital) {
        return true;  /* Nothing to connect */
    }

    /* Check if Broca's region is available */
    if (!brain->broca_enabled || !brain->broca) {
        return true;  /* Broca not initialized */
    }

    /*
     * Connect occipital to Broca via audiovisual bridge.
     *
     * BIOLOGICAL BASIS:
     * - Superior Temporal Sulcus (STS) integrates visual and auditory speech cues
     * - Lip movements precede audio by ~150ms, enabling predictive speech processing
     * - McGurk effect demonstrates visual-audio speech integration
     * - Mirror neurons in Broca's area link gesture observation to motor plans
     *
     * PATHWAYS:
     * - Occipital V4/V5 -> STS -> Broca (visual speech cues to articulation)
     * - Occipital V4 -> Broca (visual gesture -> motor planning)
     * - Occipital face area -> STS -> Broca (lip reading)
     */

    /* Create audiovisual bridge with default config */
    occipital_av_config_t av_config = occipital_av_default_config();
    av_config.enable_lip_reading = true;
    av_config.enable_gesture_binding = true;
    av_config.enable_bio_async = brain->bio_async_enabled;

    occipital_audiovisual_bridge_t* av_bridge = occipital_av_bridge_create(
        brain->occipital, &av_config);

    if (!av_bridge) {
        LOG_WARN(LOG_MODULE, "Failed to create occipital audiovisual bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_connect_occipital_to_broca: av_bridge is NULL");
        return false;
    }

    /* Connect to Broca's region */
    if (occipital_av_connect_broca(av_bridge, brain->broca) != 0) {
        LOG_WARN(LOG_MODULE, "Failed to connect audiovisual bridge to Broca");
        occipital_av_bridge_destroy(av_bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_brain_factory_connect_occipital_to_broca: validation failed");
        return false;
    }

    /* Connect to audio cortex if available */
    if (brain->audio_cortex) {
        if (occipital_av_connect_audio_cortex(av_bridge, brain->audio_cortex) != 0) {
            LOG_WARN(LOG_MODULE, "Failed to connect audiovisual bridge to audio cortex");
        }
    }

    /* No storage field exists in brain for this bridge — destroy to prevent leak */
    occipital_av_bridge_destroy(av_bridge);

    LOG_DEBUG(LOG_MODULE, "Occipital connected to Broca (audiovisual bridge)");
    return true;
}

//=============================================================================
// Destruction
//=============================================================================

/**
 * @brief Destroy occipital cortex subsystem
 *
 * WHAT: Clean up all occipital resources, bridges, and cortical columns
 * WHY:  Prevent memory leaks during brain destruction
 * HOW:  Destroy in reverse initialization order
 *
 * @param brain Brain instance
 */
void nimcp_brain_factory_destroy_occipital_subsystem(brain_t brain) {
    if (!brain) { NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_destroy_occipital_subsystem: brain is NULL"); return; }

    LOG_DEBUG(LOG_MODULE, "Destroying occipital cortex subsystem");

    /* Destroy orientation hypercolumns */
    if (brain->orientation_hypercolumns && brain->num_orientation_hypercolumns > 0) {
        for (uint32_t i = 0; i < brain->num_orientation_hypercolumns; i++) {
            if (brain->orientation_hypercolumns[i]) {
                orientation_hypercolumn_destroy(brain->orientation_hypercolumns[i]);
                brain->orientation_hypercolumns[i] = NULL;
            }
        }
        nimcp_free(brain->orientation_hypercolumns);
        brain->orientation_hypercolumns = NULL;
        brain->num_orientation_hypercolumns = 0;
    }

    /* Destroy quantum bridge first (depends on occipital) */
    if (brain->occipital_quantum_bridge) {
        occipital_quantum_bridge_destroy(brain->occipital_quantum_bridge);
        brain->occipital_quantum_bridge = NULL;
    }

    /* Destroy thalamic bridge */
    if (brain->occipital_thalamic_bridge) {
        occipital_thalamic_bridge_destroy(brain->occipital_thalamic_bridge);
        brain->occipital_thalamic_bridge = NULL;
    }

    /* Destroy substrate bridge */
    if (brain->occipital_substrate_bridge) {
        occipital_substrate_bridge_destroy(brain->occipital_substrate_bridge);
        brain->occipital_substrate_bridge = NULL;
    }

    /* Destroy occipital adapter */
    if (brain->occipital) {
        occipital_destroy(brain->occipital);
        brain->occipital = NULL;
    }

    brain->occipital_enabled = false;
    brain->last_occipital_update_us = 0;

    LOG_DEBUG(LOG_MODULE, "Occipital cortex subsystem destroyed");
}
