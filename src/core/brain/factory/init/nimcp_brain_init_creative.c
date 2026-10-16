//=============================================================================
// nimcp_brain_init_creative.c - Creative System Initialization
//=============================================================================
/**
 * @file nimcp_brain_init_creative.c
 * @brief Creative System Initialization Implementation
 *
 * WHAT: Initialization functions for creative/artistic cognitive system
 * WHY:  Enable artistic appreciation and generation capabilities
 * HOW:  Creates creative orchestrator and connects all integration bridges
 *
 * EXTRACTED FROM: Brain factory initialization
 * DATE: 2025-01-30
 *
 * @version Phase Creative: Creative System Integration
 * @author NIMCP Development Team
 */

//=============================================================================
// Includes
//=============================================================================

#include "core/brain/factory/init/nimcp_brain_init_creative.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"

#define LOG_MODULE "BRAIN_INIT_CREATIVE"

//=============================================================================
#include <stddef.h>  /* for NULL */
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(brain_init_creative, MESH_ADAPTER_CATEGORY_SYSTEM)


// Compatibility macro for set_error (converts to LOG_ERROR)
#ifndef set_error
#define set_error(msg) LOG_ERROR(LOG_MODULE, "%s", msg)
#endif

// Creative system includes
#include "cognitive/creative/nimcp_creative.h"
#include "cognitive/creative/nimcp_creative_orchestrator.h"
// Appreciation system
#include "cognitive/creative/appreciation/nimcp_aesthetic_evaluation.h"
#include "cognitive/creative/appreciation/nimcp_style_perception.h"
#include "cognitive/creative/appreciation/nimcp_creative_emotion_bridge.h"
#include "cognitive/creative/appreciation/nimcp_creative_memory_bridge.h"
// Inspiration system
#include "cognitive/creative/inspiration/nimcp_style_representation.h"
#include "cognitive/creative/inspiration/nimcp_influence_blending.h"
#include "cognitive/creative/inspiration/nimcp_creative_pattern_extractor.h"
#include "cognitive/creative/inspiration/nimcp_creative_knowledge_bridge.h"
// Generation system
#include "cognitive/creative/generation/nimcp_text_generation.h"
#include "cognitive/creative/generation/nimcp_music_generation.h"
#include "cognitive/creative/generation/nimcp_visual_generation.h"
#include "cognitive/creative/generation/nimcp_video_generation.h"
#include "cognitive/creative/generation/nimcp_multimodal_director.h"
// Creative bridges
#include "cognitive/creative/bridges/nimcp_creative_bridge.h"
#include "cognitive/creative/bridges/nimcp_creative_neural_bridge.h"
#include "cognitive/creative/bridges/nimcp_creative_ethics_bridge.h"
#include "cognitive/creative/bridges/nimcp_creative_training_bridge.h"
#include "utils/exception/nimcp_exception_macros.h"

// Perception cortex modules
#include "perception/nimcp_visual_cortex.h"
#include "perception/nimcp_audio_cortex.h"
#include "perception/nimcp_speech_cortex.h"
#include "core/cortical_columns/nimcp_cortical_column.h"

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * @brief Connect creative to bio-async messaging
 */
static bool connect_creative_to_bio_async(brain_t brain) {
    if (!brain || !brain->creative_orchestrator) return true; /* Non-fatal if not available */

    /* Register with bio-async if enabled */
    if (brain->bio_async_enabled && brain->bio_async_ctx) {
        /*
         * TODO: Register creative message handlers
         * bio_router_register_module(router, BIO_MODULE_CREATIVE, brain->creative_orchestrator);
         */
    }

    return true;
}

//=============================================================================
// Public API Implementation
//=============================================================================

bool nimcp_brain_factory_init_creative_subsystem(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                "nimcp_brain_factory_init_creative_subsystem: brain is NULL");
        return false;
    }

    /* Check if already initialized */
    if (brain->creative_orchestrator) {
        return true;  /* Already initialized */
    }

    /* Check if creative is enabled in config */
    if (!brain->config.enable_creative_system) {
        brain->creative_enabled = false;
        return true;  /* Not enabled, not an error */
    }

    /* Check for lazy init */
    if (brain->config.lazy_creative_init || brain->config.lazy_init_mode) {
        brain->creative_lazy_init = true;
        brain->creative_enabled = true;
        LOG_INFO(LOG_MODULE, "Creative system lazy init deferred");
        return true;
    }

    brain_init_creative_heartbeat("init_creative_orchestrator", 0.1f);

    /* Create creative orchestrator with default configuration */
    creative_config_t creative_cfg;
    creative_config_init_defaults(&creative_cfg);

    /* Configure based on brain settings */
    creative_cfg.enable_text_generation = true;
    creative_cfg.enable_music_generation = true;
    creative_cfg.enable_visual_generation = true;
    creative_cfg.enable_video_generation = true;
    creative_cfg.enable_appreciation = true;
    creative_cfg.integrate_with_ethics = brain->config.enable_ethics;

    /* Create orchestrator */
    brain->creative_orchestrator = creative_orchestrator_create(&creative_cfg);
    if (!brain->creative_orchestrator) {
        LOG_ERROR(LOG_MODULE, "Failed to create creative orchestrator");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_creative_subsystem: brain->creative_orchestrator is NULL");
        return false;
    }

    brain_init_creative_heartbeat("init_appreciation", 0.2f);

    /* Initialize appreciation subsystem */
    if (!nimcp_brain_factory_init_creative_appreciation(brain)) {
        LOG_WARN(LOG_MODULE, "Appreciation subsystem initialization failed");
        /* Continue - non-fatal */
    }

    brain_init_creative_heartbeat("init_inspiration", 0.3f);

    /* Initialize inspiration subsystem */
    if (!nimcp_brain_factory_init_creative_inspiration(brain)) {
        LOG_WARN(LOG_MODULE, "Inspiration subsystem initialization failed");
        /* Continue - non-fatal */
    }

    brain_init_creative_heartbeat("init_generation", 0.4f);

    /* Initialize generation subsystem */
    if (!nimcp_brain_factory_init_creative_generation(brain)) {
        LOG_WARN(LOG_MODULE, "Generation subsystem initialization failed");
        /* Continue - non-fatal */
    }

    brain_init_creative_heartbeat("init_bridges", 0.6f);

    /* Initialize bridges */
    if (!nimcp_brain_factory_init_creative_bridge(brain)) {
        LOG_WARN(LOG_MODULE, "Creative bridge initialization failed");
    }

    if (!nimcp_brain_factory_init_creative_neural_bridge(brain)) {
        LOG_WARN(LOG_MODULE, "Neural bridge initialization failed");
    }

    if (!nimcp_brain_factory_init_creative_ethics_bridge(brain)) {
        LOG_WARN(LOG_MODULE, "Ethics bridge initialization failed");
    }

    if (!nimcp_brain_factory_init_creative_training_bridge(brain)) {
        LOG_WARN(LOG_MODULE, "Training bridge initialization failed");
    }

    brain_init_creative_heartbeat("connect_integrations", 0.8f);

    /* Connect to other brain subsystems */
    if (!nimcp_brain_factory_connect_creative_to_emotion(brain)) {
        LOG_WARN(LOG_MODULE, "Emotion connection failed");
    }

    if (!nimcp_brain_factory_connect_creative_to_memory(brain)) {
        LOG_WARN(LOG_MODULE, "Memory connection failed");
    }

    if (!nimcp_brain_factory_connect_creative_to_ethics(brain)) {
        LOG_WARN(LOG_MODULE, "Ethics connection failed");
    }

    if (!nimcp_brain_factory_connect_creative_to_gpu(brain)) {
        LOG_WARN(LOG_MODULE, "GPU connection failed");
    }

    if (!nimcp_brain_factory_connect_creative_to_knowledge(brain)) {
        LOG_WARN(LOG_MODULE, "Knowledge connection failed");
    }

    if (!connect_creative_to_bio_async(brain)) {
        LOG_WARN(LOG_MODULE, "Bio-async connection failed");
    }

    /* Connect to perception cortex modules */
    if (!nimcp_brain_factory_connect_creative_to_visual_cortex(brain)) {
        LOG_WARN(LOG_MODULE, "Visual cortex connection failed");
    }

    if (!nimcp_brain_factory_connect_creative_to_audio_cortex(brain)) {
        LOG_WARN(LOG_MODULE, "Audio cortex connection failed");
    }

    if (!nimcp_brain_factory_connect_creative_to_speech_cortex(brain)) {
        LOG_WARN(LOG_MODULE, "Speech cortex connection failed");
    }

    if (!nimcp_brain_factory_connect_creative_to_cortical_columns(brain)) {
        LOG_WARN(LOG_MODULE, "Cortical columns connection failed");
    }

    brain->creative_enabled = true;
    brain->last_creative_update_us = 0;

    brain_init_creative_heartbeat("init_creative_complete", 1.0f);

    LOG_INFO(LOG_MODULE, "Creative system initialized successfully");
    return true;
}

bool nimcp_brain_factory_init_creative_appreciation(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_creative_appreciation: brain is NULL");
        return false;
    }

    /* Create aesthetic evaluator */
    aesthetic_evaluator_config_t eval_cfg;
    aesthetic_evaluator_config_defaults(&eval_cfg);
    brain->aesthetic_evaluator = aesthetic_evaluator_create(&eval_cfg);
    if (!brain->aesthetic_evaluator) {
        LOG_WARN(LOG_MODULE, "Failed to create aesthetic evaluator");
    }

    /* Create style perception */
    style_perception_config_t perc_cfg;
    style_perception_config_defaults(&perc_cfg);
    brain->style_perception = style_perception_create(&perc_cfg);
    if (!brain->style_perception) {
        LOG_WARN(LOG_MODULE, "Failed to create style perception");
    }

    /* Create emotion bridge */
    creative_emotion_bridge_config_t emo_cfg;
    creative_emotion_bridge_config_defaults(&emo_cfg);
    brain->creative_emotion_bridge = creative_emotion_bridge_create(&emo_cfg);
    if (!brain->creative_emotion_bridge) {
        LOG_WARN(LOG_MODULE, "Failed to create emotion bridge");
    }

    /* Create memory bridge */
    creative_memory_bridge_config_t mem_cfg;
    creative_memory_bridge_config_defaults(&mem_cfg);
    brain->creative_memory_bridge = creative_memory_bridge_create(&mem_cfg);
    if (!brain->creative_memory_bridge) {
        LOG_WARN(LOG_MODULE, "Failed to create memory bridge");
    }

    return true;
}

bool nimcp_brain_factory_init_creative_inspiration(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_creative_inspiration: brain is NULL");
        return false;
    }

    /* Create style representation */
    style_representer_config_t style_cfg;
    style_representer_config_defaults(&style_cfg);
    brain->style_representation = style_representer_create(&style_cfg);
    if (!brain->style_representation) {
        LOG_WARN(LOG_MODULE, "Failed to create style representation");
    }

    /* Create influence blender */
    influence_blender_config_t blend_cfg;
    influence_blender_config_defaults(&blend_cfg);
    brain->influence_blender = influence_blender_create(&blend_cfg, brain->style_representation);
    if (!brain->influence_blender) {
        LOG_WARN(LOG_MODULE, "Failed to create influence blender");
    }

    /* Create pattern extractor */
    creative_pattern_extractor_config_t pattern_cfg;
    creative_pattern_extractor_config_defaults(&pattern_cfg);
    brain->creative_pattern_extractor = creative_pattern_extractor_create(&pattern_cfg);
    if (!brain->creative_pattern_extractor) {
        LOG_WARN(LOG_MODULE, "Failed to create pattern extractor");
    }

    /* Create knowledge bridge */
    creative_knowledge_bridge_config_t kg_cfg;
    creative_knowledge_bridge_config_defaults(&kg_cfg);
    brain->creative_knowledge_bridge = creative_knowledge_bridge_create(&kg_cfg);
    if (!brain->creative_knowledge_bridge) {
        LOG_WARN(LOG_MODULE, "Failed to create knowledge bridge");
    }

    return true;
}

bool nimcp_brain_factory_init_creative_generation(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_creative_generation: brain is NULL");
        return false;
    }

    /* Create text generator */
    text_generator_config_t text_cfg;
    text_generator_config_defaults(&text_cfg);
    brain->text_generator = text_generator_create(&text_cfg);
    if (!brain->text_generator) {
        LOG_WARN(LOG_MODULE, "Failed to create text generator");
    }

    /* Create music generator */
    music_generator_config_t music_cfg;
    music_generator_config_defaults(&music_cfg);
    brain->music_generator = music_generator_create(&music_cfg);
    if (!brain->music_generator) {
        LOG_WARN(LOG_MODULE, "Failed to create music generator");
    }

    /* Create visual generator */
    visual_generator_config_t visual_cfg;
    visual_generator_config_defaults(&visual_cfg);
    brain->visual_generator = visual_generator_create(&visual_cfg);
    if (!brain->visual_generator) {
        LOG_WARN(LOG_MODULE, "Failed to create visual generator");
    }

    /* Create video generator */
    video_generator_config_t video_cfg;
    video_generator_config_defaults(&video_cfg);
    brain->video_generator = video_generator_create(&video_cfg);
    if (!brain->video_generator) {
        LOG_WARN(LOG_MODULE, "Failed to create video generator");
    }

    /* Create multimodal director */
    multimodal_director_config_t dir_cfg;
    multimodal_director_config_defaults(&dir_cfg);
    brain->multimodal_director = multimodal_director_create(&dir_cfg);
    if (!brain->multimodal_director) {
        LOG_WARN(LOG_MODULE, "Failed to create multimodal director");
    }

    return true;
}

bool nimcp_brain_factory_init_creative_neural_bridge(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_creative_neural_bridge: brain is NULL");
        return false;
    }

    creative_neural_bridge_config_t cfg;
    creative_neural_bridge_config_defaults(&cfg);

    /* Configure based on available backends */
    cfg.enable_diffusion = true;
    cfg.enable_gan = true;
    cfg.enable_api = true;  /* API fallback */
    cfg.enable_fallback = true;

    brain->creative_neural_bridge = creative_neural_bridge_create(&cfg);
    if (!brain->creative_neural_bridge) {
        LOG_WARN(LOG_MODULE, "Failed to create neural bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_creative_neural_bridge: brain->creative_neural_bridge is NULL");
        return false;
    }

    return true;
}

bool nimcp_brain_factory_init_creative_ethics_bridge(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_creative_ethics_bridge: brain is NULL");
        return false;
    }

    creative_ethics_bridge_config_t cfg;
    creative_ethics_bridge_config_defaults(&cfg);

    /* Enable all safety checks by default */
    cfg.concerns[ETHICS_CONCERN_COPYRIGHT].detect = true;
    cfg.concerns[ETHICS_CONCERN_NSFW].detect = true;
    cfg.concerns[ETHICS_CONCERN_VIOLENCE].detect = true;
    cfg.concerns[ETHICS_CONCERN_HATE].detect = true;
    cfg.concerns[ETHICS_CONCERN_DECEPTION].detect = true;
    cfg.concerns[ETHICS_CONCERN_BIAS].detect = true;
    cfg.concerns[ETHICS_CONCERN_PRIVACY].detect = true;

    brain->creative_ethics_bridge = creative_ethics_bridge_create(&cfg);
    if (!brain->creative_ethics_bridge) {
        LOG_WARN(LOG_MODULE, "Failed to create ethics bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_creative_ethics_bridge: brain->creative_ethics_bridge is NULL");
        return false;
    }

    /* Connect to brain ethics engine if available */
    if (brain->ethics) {
        creative_ethics_set_engine(brain->creative_ethics_bridge, brain->ethics);
    }

    return true;
}

bool nimcp_brain_factory_init_creative_training_bridge(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_creative_training_bridge: brain is NULL");
        return false;
    }

    creative_training_bridge_config_t cfg;
    creative_training_bridge_config_defaults(&cfg);

    brain->creative_training_bridge = creative_training_bridge_create(&cfg);
    if (!brain->creative_training_bridge) {
        LOG_WARN(LOG_MODULE, "Failed to create training bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_creative_training_bridge: brain->creative_training_bridge is NULL");
        return false;
    }

    return true;
}

bool nimcp_brain_factory_init_creative_bridge(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_creative_bridge: brain is NULL");
        return false;
    }

    creative_bridge_config_t cfg;
    creative_bridge_config_defaults(&cfg);

    /* Configure validation pipeline */
    cfg.short_circuit_on_deny = true;
    cfg.collect_all_warnings = true;

    /* Enable all validation stages */
    for (int i = 0; i < VALIDATION_STAGE_COUNT; i++) {
        cfg.stages[i].enabled = true;
    }

    brain->creative_bridge = creative_bridge_create(&cfg);
    if (!brain->creative_bridge) {
        LOG_WARN(LOG_MODULE, "Failed to create creative bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_creative_bridge: brain->creative_bridge is NULL");
        return false;
    }

    return true;
}

//=============================================================================
// Integration Functions
//=============================================================================

bool nimcp_brain_factory_connect_creative_to_emotion(brain_t brain) {
    if (!brain || !brain->creative_emotion_bridge) return true;

    /* Connect to emotional system if available */
    if (brain->emotional_system) {
        creative_emotion_set_emotion_system(
            brain->creative_emotion_bridge,
            brain->emotional_system
        );
    }

    return true;
}

bool nimcp_brain_factory_connect_creative_to_memory(brain_t brain) {
    if (!brain || !brain->creative_memory_bridge) return true;

    /* Connect to memory systems if available */
    if (brain->semantic_memory) {
        creative_memory_set_semantic_system(
            brain->creative_memory_bridge,
            brain->semantic_memory
        );
    }

    if (brain->autobio) {
        creative_memory_set_hippocampus(
            brain->creative_memory_bridge,
            brain->autobio
        );
    }

    return true;
}

bool nimcp_brain_factory_connect_creative_to_ethics(brain_t brain) {
    if (!brain || !brain->creative_ethics_bridge) return true;

    /* Connect to ethics engine if available */
    if (brain->ethics) {
        creative_ethics_set_engine(brain->creative_ethics_bridge, brain->ethics);
    }

    /* Connect to creative bridge for validation */
    if (brain->creative_bridge) {
        creative_bridge_set_ethics_engine(brain->creative_bridge, brain->ethics);
    }

    return true;
}

bool nimcp_brain_factory_connect_creative_to_gpu(brain_t brain) {
    if (!brain || !brain->creative_neural_bridge) return true;

    /* Connect to GPU context if available */
    if (brain->gpu_enabled && brain->gpu_ctx) {
        /* Neural bridge will use GPU for diffusion/GAN inference */
        /* TODO: neural_bridge_set_gpu_context(brain->creative_neural_bridge, brain->gpu_ctx); */
    }

    return true;
}

bool nimcp_brain_factory_connect_creative_to_knowledge(brain_t brain) {
    if (!brain || !brain->creative_knowledge_bridge) return true;

    /* Connect to internal KG if available */
    if (brain->internal_kg_enabled && brain->internal_kg) {
        creative_knowledge_set_brain_kg(
            brain->creative_knowledge_bridge,
            brain->internal_kg
        );
    }

    return true;
}

bool nimcp_brain_factory_connect_creative_to_training(brain_t brain) {
    if (!brain || !brain->creative_training_bridge) return true;

    /* Connect to brain training context if available */
    if (brain->enable_training_integration && brain->training_ctx) {
        /* TODO: creative_training_bridge_set_training_ctx(
            brain->creative_training_bridge,
            brain->training_ctx
        ); */
    }

    return true;
}

bool nimcp_brain_factory_connect_creative_to_immune(brain_t brain) {
    if (!brain) { NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_connect_creative_to_immune: brain is NULL"); return false; }

    /* Connect to brain immune system if available */
    if (brain->immune_enabled && brain->immune_system) {
        /* TODO: Register creative validation failures as immune events */
    }

    return true;
}

bool nimcp_brain_factory_connect_creative_to_visual_cortex(brain_t brain) {
    if (!brain) { NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_connect_creative_to_visual_cortex: brain is NULL"); return false; }

    /* Connect to visual cortex if available */
    if (brain->visual_cortex) {
        /* Connect visual generator to visual cortex for style guidance */
        if (brain->visual_generator) {
            visual_generator_set_visual_cortex(brain->visual_generator,
                                                brain->visual_cortex);
            LOG_DEBUG(LOG_MODULE, "Visual generator connected to visual cortex");
        }

        /* Connect aesthetic evaluator for visual quality assessment */
        if (brain->aesthetic_evaluator) {
            aesthetic_evaluator_set_visual_cortex(brain->aesthetic_evaluator,
                                                   brain->visual_cortex);
            LOG_DEBUG(LOG_MODULE, "Aesthetic evaluator connected to visual cortex");
        }

        /* Connect style perception for visual style recognition */
        if (brain->style_perception) {
            style_perception_set_visual_cortex(brain->style_perception,
                                                brain->visual_cortex);
            LOG_DEBUG(LOG_MODULE, "Style perception connected to visual cortex");
        }

        LOG_INFO(LOG_MODULE, "Creative system connected to visual cortex");
    }

    return true;
}

bool nimcp_brain_factory_connect_creative_to_audio_cortex(brain_t brain) {
    if (!brain) { NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_connect_creative_to_audio_cortex: brain is NULL"); return false; }

    /* Connect to audio cortex if available */
    if (brain->audio_cortex) {
        /* Connect music generator to audio cortex for harmonic analysis */
        if (brain->music_generator) {
            music_generator_set_audio_cortex(brain->music_generator,
                                              brain->audio_cortex);
            LOG_DEBUG(LOG_MODULE, "Music generator connected to audio cortex");
        }

        /* Connect aesthetic evaluator for audio quality assessment */
        if (brain->aesthetic_evaluator) {
            aesthetic_evaluator_set_audio_cortex(brain->aesthetic_evaluator,
                                                  brain->audio_cortex);
            LOG_DEBUG(LOG_MODULE, "Aesthetic evaluator connected to audio cortex");
        }

        /* Connect style perception for musical style recognition */
        if (brain->style_perception) {
            style_perception_set_audio_cortex(brain->style_perception,
                                               brain->audio_cortex);
            LOG_DEBUG(LOG_MODULE, "Style perception connected to audio cortex");
        }

        LOG_INFO(LOG_MODULE, "Creative system connected to audio cortex");
    }

    return true;
}

bool nimcp_brain_factory_connect_creative_to_speech_cortex(brain_t brain) {
    if (!brain) { NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_connect_creative_to_speech_cortex: brain is NULL"); return false; }

    /* Connect to speech cortex if available */
    if (brain->speech_cortex) {
        /* Connect text generator for prosody and phonological features */
        if (brain->text_generator) {
            text_generator_set_speech_cortex(brain->text_generator,
                                              brain->speech_cortex);
            LOG_DEBUG(LOG_MODULE, "Text generator connected to speech cortex");
        }

        /* Connect aesthetic evaluator for euphony/prosody assessment */
        if (brain->aesthetic_evaluator) {
            aesthetic_evaluator_set_speech_cortex(brain->aesthetic_evaluator,
                                                   brain->speech_cortex);
            LOG_DEBUG(LOG_MODULE, "Aesthetic evaluator connected to speech cortex");
        }

        /* Connect style perception for linguistic style recognition */
        if (brain->style_perception) {
            style_perception_set_speech_cortex(brain->style_perception,
                                                brain->speech_cortex);
            LOG_DEBUG(LOG_MODULE, "Style perception connected to speech cortex");
        }

        LOG_INFO(LOG_MODULE, "Creative system connected to speech cortex");
    }

    return true;
}

bool nimcp_brain_factory_connect_creative_to_cortical_columns(brain_t brain) {
    if (!brain) { NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_connect_creative_to_cortical_columns: brain is NULL"); return false; }

    /* Connect to cortical columns if available */
    if (brain->cortical_column_pool) {
        /* Connect pattern extractor for hierarchical feature extraction */
        if (brain->creative_pattern_extractor) {
            creative_pattern_extractor_set_cortical_columns(
                brain->creative_pattern_extractor,
                brain->cortical_column_pool);
            LOG_DEBUG(LOG_MODULE, "Pattern extractor connected to cortical columns");
        }

        /* Connect style representation for multi-scale style vectors */
        if (brain->style_representation) {
            style_representer_set_cortical_columns(brain->style_representation,
                                                    brain->cortical_column_pool);
            LOG_DEBUG(LOG_MODULE, "Style representation connected to cortical columns");
        }

        /* Connect visual generator for predictive coding feedback */
        if (brain->visual_generator) {
            visual_generator_set_cortical_columns(brain->visual_generator,
                                                   brain->cortical_column_pool);
            LOG_DEBUG(LOG_MODULE, "Visual generator connected to cortical columns");
        }

        LOG_INFO(LOG_MODULE, "Creative system connected to cortical columns");
    }

    return true;
}

//=============================================================================
// Cleanup Functions
//=============================================================================

void nimcp_brain_factory_destroy_creative_subsystem(brain_t brain) {
    if (!brain) { NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_destroy_creative_subsystem: brain is NULL"); return; }

    /* Destroy in reverse initialization order */

    /* Destroy bridges */
    if (brain->creative_training_bridge) {
        creative_training_bridge_destroy(brain->creative_training_bridge);
        brain->creative_training_bridge = NULL;
    }

    if (brain->creative_ethics_bridge) {
        creative_ethics_bridge_destroy(brain->creative_ethics_bridge);
        brain->creative_ethics_bridge = NULL;
    }

    if (brain->creative_neural_bridge) {
        creative_neural_bridge_destroy(brain->creative_neural_bridge);
        brain->creative_neural_bridge = NULL;
    }

    if (brain->creative_bridge) {
        creative_bridge_destroy(brain->creative_bridge);
        brain->creative_bridge = NULL;
    }

    /* Destroy generation subsystem */
    if (brain->multimodal_director) {
        multimodal_director_destroy(brain->multimodal_director);
        brain->multimodal_director = NULL;
    }

    if (brain->video_generator) {
        video_generator_destroy(brain->video_generator);
        brain->video_generator = NULL;
    }

    if (brain->visual_generator) {
        visual_generator_destroy(brain->visual_generator);
        brain->visual_generator = NULL;
    }

    if (brain->music_generator) {
        music_generator_destroy(brain->music_generator);
        brain->music_generator = NULL;
    }

    if (brain->text_generator) {
        text_generator_destroy(brain->text_generator);
        brain->text_generator = NULL;
    }

    /* Destroy inspiration subsystem */
    if (brain->creative_knowledge_bridge) {
        creative_knowledge_bridge_destroy(brain->creative_knowledge_bridge);
        brain->creative_knowledge_bridge = NULL;
    }

    if (brain->creative_pattern_extractor) {
        creative_pattern_extractor_destroy(brain->creative_pattern_extractor);
        brain->creative_pattern_extractor = NULL;
    }

    if (brain->influence_blender) {
        influence_blender_destroy(brain->influence_blender);
        brain->influence_blender = NULL;
    }

    if (brain->style_representation) {
        style_representer_destroy(brain->style_representation);
        brain->style_representation = NULL;
    }

    /* Destroy appreciation subsystem */
    if (brain->creative_memory_bridge) {
        creative_memory_bridge_destroy(brain->creative_memory_bridge);
        brain->creative_memory_bridge = NULL;
    }

    if (brain->creative_emotion_bridge) {
        creative_emotion_bridge_destroy(brain->creative_emotion_bridge);
        brain->creative_emotion_bridge = NULL;
    }

    if (brain->style_perception) {
        style_perception_destroy(brain->style_perception);
        brain->style_perception = NULL;
    }

    if (brain->aesthetic_evaluator) {
        aesthetic_evaluator_destroy(brain->aesthetic_evaluator);
        brain->aesthetic_evaluator = NULL;
    }

    /* Destroy orchestrator */
    if (brain->creative_orchestrator) {
        creative_orchestrator_destroy(brain->creative_orchestrator);
        brain->creative_orchestrator = NULL;
    }

    brain->creative_enabled = false;
    brain->creative_lazy_init = false;

    LOG_INFO(LOG_MODULE, "Creative system destroyed");
}
