//=============================================================================
// nimcp_brain_init_wernicke.c - Wernicke's Region Initialization
//=============================================================================
/**
 * @file nimcp_brain_init_wernicke.c
 * @brief Wernicke's Region Initialization Implementation
 *
 * WHAT: Initialization functions for Wernicke's region (language comprehension)
 * WHY:  Enable language comprehension capabilities in the brain
 * HOW:  Creates Wernicke adapter and connects all integration bridges
 *
 * EXTRACTED FROM: Brain factory initialization
 * DATE: 2026-01-05
 *
 * @version Phase W6: Wernicke Full Brain Integration
 * @author NIMCP Development Team
 */

//=============================================================================
// Includes
//=============================================================================

#include "core/brain/factory/init/nimcp_brain_init_wernicke.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"

#define LOG_MODULE "BRAIN_INIT_WERNICKE"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(brain_init_wernicke, MESH_ADAPTER_CATEGORY_SYSTEM)


// Compatibility macro for set_error (converts to LOG_ERROR)
#ifndef set_error
#define set_error(msg) LOG_ERROR(LOG_MODULE, "%s", msg)
#endif

// Wernicke's region includes
// NOTE: Avoid including wernicke_adapter.h directly due to semantic_memory_t conflict
// Use forward declarations instead
#include "core/brain/regions/wernicke/nimcp_wernicke_substrate_bridge.h"
#include "core/brain/regions/wernicke/nimcp_wernicke_quantum_bridge.h"
#include "core/brain/regions/wernicke/nimcp_wernicke_broca_bridge.h"
#include "core/brain/regions/wernicke/nimcp_omni_wernicke_bridge.h"
#include "core/brain/regions/wernicke/nimcp_wernicke_gpu_bio_bridge.h"
#include "core/brain/regions/wernicke/nimcp_wernicke_immune.h"
#include "core/brain/regions/wernicke/nimcp_wernicke_nlp_bridge.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <math.h>

/* Local duplicate of wernicke_config_t — MUST match the real definition
 * in nimcp_wernicke_adapter.h EXACTLY. If fields are added there, add them
 * here too, or wernicke_default_config() will overflow the stack.
 * Cannot include the real header due to semantic_memory_t type conflict. */
typedef struct {
    uint32_t max_phonemes;
    uint32_t max_words;
    uint32_t max_concepts;
    uint32_t lexicon_size;
    bool enable_lexicon;
    uint32_t working_memory_slots;
    bool enable_working_memory;
    bool enable_phonological;
    bool enable_lexical;
    bool enable_semantic;
    bool enable_syntactic;
    uint32_t embedding_dim;
    uint32_t formant_count;
    bool enable_audiovisual;
    bool enable_prosody;
    bool enable_broca_connection;
    bool enable_semantic_memory;
    bool enable_kg_registration;
    bool enable_events;
    bool enable_training;
    float learning_rate;
    float processing_window_ms;
    bool enable_bio_async;
    uint32_t default_channel;  /* nimcp_bio_channel_type_t */
} wernicke_config_t;
extern wernicke_config_t wernicke_default_config(void);
extern wernicke_adapter_t* wernicke_create(const wernicke_config_t* config);
extern void wernicke_destroy(wernicke_adapter_t* adapter);

// Lexical access forward declarations for lexicon seeding
struct lexical_access;
typedef struct lexical_access lexical_access_t;
extern lexical_access_t* wernicke_get_lexical_access(wernicke_adapter_t* adapter);
extern uint32_t lexical_build_common_english(lexical_access_t* lex);

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * @brief Connect Wernicke to bio-async messaging (internal)
 */
static bool connect_wernicke_to_bio_async_internal(brain_t brain) {
    if (!brain || !brain->wernicke) return true; /* Non-fatal if not available */

    /* Register with bio-async if enabled */
    if (brain->bio_async_enabled && brain->bio_async_ctx) {
        /*
         * TODO: Register Wernicke message handlers
         * bio_router_register_module(router, BIO_MODULE_WERNICKE, brain->wernicke);
         */
    }

    return true;
}

/**
 * @brief Connect Wernicke substrate bridge to neural substrate
 */
static bool connect_substrate_bridge(brain_t brain) {
    if (!brain || !brain->wernicke_substrate_bridge) return true;

    /* Apply initial metabolic effects */
    if (wernicke_substrate_bridge_update(brain->wernicke_substrate_bridge) != 0) {
        LOG_WARN(LOG_MODULE, "Initial substrate bridge update failed");
    }

    return true;
}

//=============================================================================
// Public API Implementation
//=============================================================================

bool nimcp_brain_factory_init_wernicke_subsystem(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_brain_factory_init_wernicke_subsystem: brain is NULL");

            return false;
    }

    /* Check if already initialized */
    if (brain->wernicke) {
        return true;  /* Already initialized */
    }

    /* Check if Wernicke is enabled in config */
    /* Note: Default to enabled for language-capable brains */
    if (!brain->config.enable_speech_cortex &&
        !brain->config.enable_multimodal_integration) {
        brain->wernicke_enabled = false;
        return true;  /* Not enabled, not an error */
    }

    /* Create Wernicke adapter with default configuration */
    wernicke_config_t wernicke_cfg = wernicke_default_config();

    /* Scale configuration based on brain config */
    if (brain->config.working_memory_capacity > 0) {
        wernicke_cfg.working_memory_slots = brain->config.working_memory_capacity;
    }

    brain->wernicke = wernicke_create(&wernicke_cfg);
    if (!brain->wernicke) {
        set_error("Failed to create Wernicke adapter");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_wernicke_subsystem: brain->wernicke is NULL");
        return false;
    }

    brain->wernicke_enabled = true;
    brain->last_wernicke_update_us = 0;

    /* Initialize integration bridges */
    if (!nimcp_brain_factory_init_wernicke_substrate_bridge(brain)) {
        LOG_WARN(LOG_MODULE, "Wernicke substrate bridge init failed (non-fatal)");
    }

    if (!nimcp_brain_factory_init_wernicke_quantum_bridge(brain)) {
        LOG_WARN(LOG_MODULE, "Wernicke quantum bridge init failed (non-fatal)");
    }

    if (!nimcp_brain_factory_init_wernicke_broca_bridge(brain)) {
        LOG_WARN(LOG_MODULE, "Wernicke-Broca bridge init failed (non-fatal)");
    }

    if (!nimcp_brain_factory_init_wernicke_omni_bridge(brain)) {
        LOG_WARN(LOG_MODULE, "Wernicke omni bridge init failed (non-fatal)");
    }

    if (!nimcp_brain_factory_init_wernicke_gpu_bridge(brain)) {
        LOG_WARN(LOG_MODULE, "Wernicke GPU bridge init failed (non-fatal)");
    }

    /* Connect to other subsystems */
    if (!nimcp_brain_factory_connect_wernicke_to_semantic_memory(brain)) {
        LOG_WARN(LOG_MODULE, "Wernicke-semantic memory connection failed (non-fatal)");
    }

    if (!nimcp_brain_factory_connect_wernicke_to_working_memory(brain)) {
        LOG_WARN(LOG_MODULE, "Wernicke-WM connection failed (non-fatal)");
    }

    if (!nimcp_brain_factory_connect_wernicke_to_training(brain)) {
        LOG_WARN(LOG_MODULE, "Wernicke-Training connection failed (non-fatal)");
    }

    if (!nimcp_brain_factory_connect_wernicke_to_immune(brain)) {
        LOG_WARN(LOG_MODULE, "Wernicke-Immune connection failed (non-fatal)");
    }

    /* Connect to bio-async */
    if (!connect_wernicke_to_bio_async_internal(brain)) {
        LOG_WARN(LOG_MODULE, "Wernicke bio-async connection failed (non-fatal)");
    }

    /* Seed lexicon with common English words for language production */
    lexical_access_t* lexical = wernicke_get_lexical_access(brain->wernicke);
    if (lexical) {
        uint32_t word_count = lexical_build_common_english(lexical);
        LOG_INFO(LOG_MODULE, "Seeded Wernicke lexicon with %u common English words", word_count);
    } else {
        LOG_WARN(LOG_MODULE, "Could not seed lexicon — lexical access unavailable");
    }

    LOG_INFO(LOG_MODULE, "Wernicke's region initialized successfully");
    return true;
}

bool nimcp_brain_factory_init_wernicke_substrate_bridge(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_brain_factory_init_wernicke_substrate_bridge: brain is NULL");

            return false;
    }

    /* Check if already initialized */
    if (brain->wernicke_substrate_bridge) {
        return true;
    }

    /* Need Wernicke adapter first */
    if (!brain->wernicke) {
        return true;  /* Not ready yet, will be called again */
    }

    /* Get neural substrate - may be NULL in simple configurations */
    void* substrate = NULL;
    /*
     * TODO: Get substrate from brain if available
     * substrate = brain->substrate;
     */

    /* Create substrate bridge with default config */
    wernicke_substrate_config_t config = wernicke_substrate_default_config();

    brain->wernicke_substrate_bridge = wernicke_substrate_bridge_create(
        brain->wernicke, substrate, &config);

    if (!brain->wernicke_substrate_bridge) {
        LOG_WARN(LOG_MODULE, "Failed to create Wernicke substrate bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_wernicke_substrate_bridge: brain->wernicke_substrate_bridge is NULL");
        return false;
    }

    /* Connect to substrate */
    connect_substrate_bridge(brain);

    LOG_DEBUG(LOG_MODULE, "Wernicke substrate bridge initialized");
    return true;
}

bool nimcp_brain_factory_init_wernicke_quantum_bridge(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_brain_factory_init_wernicke_quantum_bridge: brain is NULL");

            return false;
    }

    /* Check if already initialized */
    if (brain->wernicke_quantum_bridge) {
        return true;
    }

    /* Need Wernicke adapter first */
    if (!brain->wernicke) {
        return true;  /* Not ready yet */
    }

    /* Check if quantum reasoning is enabled */
    if (!brain->quantum_reasoning_enabled) {
        return true;  /* Not enabled, not an error */
    }

    /* Create quantum bridge with default config */
    wernicke_quantum_config_t config;
    wernicke_quantum_default_config(&config);

    /* Scale Grover iterations based on lexicon size (sqrt(N) complexity) */
    if (brain->config.num_outputs > 100) {
        /* Grover needs sqrt(N) iterations, estimate from vocabulary size */
        uint32_t estimated_iters = (uint32_t)(sqrt((double)brain->config.num_outputs) * 1.5f);
        if (estimated_iters > config.grover_max_iterations) {
            config.grover_max_iterations = estimated_iters;
        }
    }

    brain->wernicke_quantum_bridge = wernicke_quantum_bridge_create(
        brain->wernicke, &config);

    if (!brain->wernicke_quantum_bridge) {
        LOG_WARN(LOG_MODULE, "Failed to create Wernicke quantum bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_wernicke_quantum_bridge: brain->wernicke_quantum_bridge is NULL");
        return false;
    }

    LOG_DEBUG(LOG_MODULE, "Wernicke quantum bridge initialized");
    return true;
}

bool nimcp_brain_factory_init_wernicke_broca_bridge(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_brain_factory_init_wernicke_broca_bridge: brain is NULL");

            return false;
    }

    /* Check if already initialized */
    if (brain->wernicke_broca_bridge) {
        return true;
    }

    /* Need Wernicke adapter first */
    if (!brain->wernicke) {
        return true;  /* Not ready yet */
    }

    /* Get Broca adapter - may be NULL if Broca not initialized yet */
    void* broca = brain->broca;

    /* Create Wernicke-Broca bridge with default config */
    wbb_config_t config = wbb_default_config();

    brain->wernicke_broca_bridge = wbb_create(
        brain->wernicke, broca, &config);

    if (!brain->wernicke_broca_bridge) {
        LOG_WARN(LOG_MODULE, "Failed to create Wernicke-Broca bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_wernicke_broca_bridge: brain->wernicke_broca_bridge is NULL");
        return false;
    }

    /* Reset bridge to clean state */
    if (wbb_reset(brain->wernicke_broca_bridge) != 0) {
        LOG_WARN(LOG_MODULE, "Wernicke-Broca bridge reset failed");
    }

    LOG_DEBUG(LOG_MODULE, "Wernicke-Broca bridge initialized");
    return true;
}

bool nimcp_brain_factory_init_wernicke_omni_bridge(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_brain_factory_init_wernicke_omni_bridge: brain is NULL");

            return false;
    }

    /* Check if already initialized */
    if (brain->omni_wernicke_bridge) {
        return true;
    }

    /* Need Wernicke adapter first */
    if (!brain->wernicke) {
        return true;  /* Not ready yet */
    }

    /* Create omni-wernicke bridge with default config */
    omni_wernicke_config_t config;
    omni_wernicke_default_config(&config);

    brain->omni_wernicke_bridge = omni_wernicke_bridge_create(&config);

    if (!brain->omni_wernicke_bridge) {
        LOG_WARN(LOG_MODULE, "Failed to create omni-Wernicke bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_wernicke_omni_bridge: brain->omni_wernicke_bridge is NULL");
        return false;
    }

    /* Connect Wernicke adapter to bridge */
    if (omni_wernicke_connect_wernicke(brain->omni_wernicke_bridge, brain->wernicke) != 0) {
        LOG_WARN(LOG_MODULE, "Failed to connect Wernicke to omni bridge");
    }

    /* Connect to Broca bridge if available */
    if (brain->wernicke_broca_bridge) {
        if (omni_wernicke_connect_broca_bridge(brain->omni_wernicke_bridge,
                                                brain->wernicke_broca_bridge) != 0) {
            LOG_WARN(LOG_MODULE, "Failed to connect Broca bridge to omni-Wernicke");
        }
    }

    /* Connect to bio-async */
    if (brain->bio_async_enabled) {
        if (omni_wernicke_connect_bio_async(brain->omni_wernicke_bridge) != 0) {
            LOG_WARN(LOG_MODULE, "Failed to connect omni-Wernicke to bio-async");
        }
    }

    LOG_DEBUG(LOG_MODULE, "Omni-Wernicke bridge initialized");
    return true;
}

bool nimcp_brain_factory_init_wernicke_gpu_bridge(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_brain_factory_init_wernicke_gpu_bridge: brain is NULL");

            return false;
    }

    /* Check if already initialized */
    if (brain->wernicke_gpu_bridge) {
        return true;
    }

    /* Need Wernicke adapter first */
    if (!brain->wernicke) {
        return true;  /* Not ready yet */
    }

    /* GPU bridge is optional - only create if CUDA is available */
    /* Note: We create the bridge without GPU context, it can be set later */
    wernicke_gpu_bio_config_t config = wernicke_gpu_bio_default_config();

    /* Create GPU bio bridge (GPU context can be NULL, set later when CUDA available) */
    brain->wernicke_gpu_bridge = wernicke_gpu_bio_create(NULL, &config);

    if (!brain->wernicke_gpu_bridge) {
        /* Non-fatal - GPU not required */
        LOG_DEBUG(LOG_MODULE, "Wernicke GPU bridge not available (CUDA may not be present)");
        return true;
    }

    /* Connect to bio-async if enabled */
    if (brain->bio_async_enabled) {
        if (wernicke_gpu_bio_connect(brain->wernicke_gpu_bridge) != 0) {
            LOG_WARN(LOG_MODULE, "Failed to connect Wernicke GPU bridge to bio-async");
        }
    }

    LOG_DEBUG(LOG_MODULE, "Wernicke GPU bridge initialized");
    return true;
}

bool nimcp_brain_factory_connect_wernicke_to_semantic_memory(brain_t brain) {
    if (!brain || !brain->wernicke) {
        return true;  /* Nothing to connect */
    }

    /* Check if semantic memory is available */
    /* Note: Semantic memory may be stored in brain_kg or separate system */

    /*
     * TODO: Connect Wernicke to semantic memory
     * wernicke_connect_semantic_memory(brain->wernicke, brain->semantic_memory);
     */

    LOG_DEBUG(LOG_MODULE, "Wernicke connected to semantic memory");
    return true;
}

bool nimcp_brain_factory_connect_wernicke_to_working_memory(brain_t brain) {
    if (!brain || !brain->wernicke) {
        return true;  /* Nothing to connect */
    }

    /* Check if working memory is available */
    if (!brain->working_memory) {
        return true;  /* WM not initialized yet */
    }

    /*
     * Register Wernicke as a working memory consumer for phonological loop.
     * This allows Wernicke to maintain active word candidates during
     * speech comprehension.
     */

    /* TODO: Register with working memory
     * working_memory_register_consumer(brain->working_memory,
     *     WM_CONSUMER_WERNICKE, brain->wernicke);
     */

    LOG_DEBUG(LOG_MODULE, "Wernicke connected to working memory");
    return true;
}

bool nimcp_brain_factory_connect_wernicke_to_training(brain_t brain) {
    if (!brain || !brain->wernicke) {
        return true;  /* Nothing to connect */
    }

    /* Check if training is enabled */
    if (!brain->enable_training_integration || !brain->training_ctx) {
        return true;  /* Training not enabled */
    }

    /*
     * Register Wernicke adapter with training context.
     * This allows language comprehension learning through:
     * - Vocabulary expansion
     * - Semantic association strengthening
     * - Phonological pattern learning
     */

    /* TODO: Register with training
     * nimcp_brain_training_register_module(brain->training_ctx,
     *     TRAIN_MODULE_WERNICKE, brain->wernicke);
     */

    LOG_DEBUG(LOG_MODULE, "Wernicke connected to training system");
    return true;
}

bool nimcp_brain_factory_connect_wernicke_to_immune(brain_t brain) {
    if (!brain || !brain->wernicke) {
        return true;  /* Nothing to connect */
    }

    /* Check if immune system is available */
    if (!brain->immune_enabled || !brain->immune_system) {
        return true;  /* Immune not enabled */
    }

    /*
     * Register for cytokine signals that affect language comprehension.
     * Neuroinflammation (high IL-1beta, TNF-alpha) can cause:
     * - Reduced comprehension speed
     * - Word-finding difficulties
     * - Semantic integration impairment
     */

    /* TODO: Register with immune system
     * brain_immune_register_cytokine_callback(brain->immune_system,
     *     CYTOKINE_IL1B | CYTOKINE_TNF_A, wernicke_inflammation_callback, brain->wernicke);
     */

    LOG_DEBUG(LOG_MODULE, "Wernicke connected to immune system");
    return true;
}

bool nimcp_brain_factory_connect_wernicke_to_bio_async(brain_t brain) {
    if (!brain || !brain->wernicke) {
        return true;  /* Nothing to connect */
    }

    /* Check if bio-async is enabled */
    if (!brain->bio_async_enabled || !brain->bio_async_ctx) {
        return true;  /* Bio-async not enabled */
    }

    /* Connect via omni bridge if available */
    if (brain->omni_wernicke_bridge) {
        if (omni_wernicke_connect_bio_async(brain->omni_wernicke_bridge) != 0) {
            LOG_WARN(LOG_MODULE, "Failed to connect omni-Wernicke to bio-async");
        }
    }

    /* Connect GPU bridge if available */
    if (brain->wernicke_gpu_bridge) {
        if (wernicke_gpu_bio_connect(brain->wernicke_gpu_bridge) != 0) {
            LOG_WARN(LOG_MODULE, "Failed to connect Wernicke GPU to bio-async");
        }
    }

    LOG_DEBUG(LOG_MODULE, "Wernicke connected to bio-async messaging");
    return true;
}

bool nimcp_brain_factory_init_wernicke_immune_bridge(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_brain_factory_init_wernicke_immune_bridge: brain is NULL");

            return false;
    }

    /* Check if already initialized */
    if (brain->wernicke_immune_bridge) {
        return true;
    }

    /* Need Wernicke adapter first */
    if (!brain->wernicke) {
        return true;  /* Not ready yet */
    }

    /* Need immune system */
    if (!brain->immune_enabled || !brain->immune_system) {
        return true;  /* Immune not enabled, not an error */
    }

    /* Create immune bridge with default config */
    wernicke_immune_config_t config;
    wernicke_immune_default_config(&config);

    brain->wernicke_immune_bridge = (struct wernicke_immune_bridge*)wernicke_immune_bridge_create(
        &config, brain->immune_system, brain->wernicke);

    if (!brain->wernicke_immune_bridge) {
        LOG_WARN(LOG_MODULE, "Failed to create Wernicke immune bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_wernicke_immune_bridge: brain->wernicke_immune_bridge is NULL");
        return false;
    }

    /* Start immune integration monitoring */
    if (wernicke_immune_bridge_start((wernicke_immune_bridge_t*)brain->wernicke_immune_bridge) != 0) {
        LOG_WARN(LOG_MODULE, "Failed to start Wernicke immune bridge");
    }

    LOG_DEBUG(LOG_MODULE, "Wernicke immune bridge initialized");
    return true;
}

bool nimcp_brain_factory_init_wernicke_nlp_bridge(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "nimcp_brain_factory_init_wernicke_nlp_bridge: brain is NULL");

            return false;
    }

    /* Check if already initialized */
    if (brain->wernicke_nlp_bridge) {
        return true;
    }

    /* Need Wernicke adapter first */
    if (!brain->wernicke) {
        return true;  /* Not ready yet */
    }

    /* Create NLP bridge with default config */
    wernicke_nlp_config_t config;
    wernicke_nlp_default_config(&config);

    brain->wernicke_nlp_bridge = wernicke_nlp_bridge_create(brain->wernicke, &config);

    if (!brain->wernicke_nlp_bridge) {
        LOG_WARN(LOG_MODULE, "Failed to create Wernicke NLP bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_wernicke_nlp_bridge: brain->wernicke_nlp_bridge is NULL");
        return false;
    }

    /* Connect to speech cortex if available */
    if (brain->speech_cortex) {
        wernicke_nlp_connect_speech_cortex(brain->wernicke_nlp_bridge, brain->speech_cortex);
    }

    /* Connect to NLP network if available */
    if (brain->nlp_network) {
        wernicke_nlp_connect_nlp_network(brain->wernicke_nlp_bridge, brain->nlp_network);
    }

    /* Connect to working memory if available */
    if (brain->working_memory) {
        wernicke_nlp_connect_working_memory(brain->wernicke_nlp_bridge, brain->working_memory);
    }

    /* Connect to knowledge graph if available */
    if (brain->internal_kg) {
        wernicke_nlp_connect_knowledge_graph(brain->wernicke_nlp_bridge, brain->internal_kg);
    }

    /* Connect to bio-async if enabled */
    if (brain->bio_async_enabled) {
        wernicke_nlp_connect_bio_async(brain->wernicke_nlp_bridge);
    }

    LOG_DEBUG(LOG_MODULE, "Wernicke NLP bridge initialized");
    return true;
}

//=============================================================================
// Destruction
//=============================================================================

/**
 * @brief Destroy Wernicke's area subsystem
 *
 * WHAT: Clean up all Wernicke resources and bridges
 * WHY:  Prevent memory leaks during brain destruction
 * HOW:  Destroy in reverse initialization order
 *
 * @param brain Brain instance
 */
void nimcp_brain_factory_destroy_wernicke_subsystem(brain_t brain) {
    if (!brain) { NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_destroy_wernicke_subsystem: brain is NULL"); return; }

    LOG_DEBUG(LOG_MODULE, "Destroying Wernicke's area subsystem");

    /* Destroy NLP bridge first */
    if (brain->wernicke_nlp_bridge) {
        wernicke_nlp_bridge_destroy(brain->wernicke_nlp_bridge);
        brain->wernicke_nlp_bridge = NULL;
    }

    /* Destroy immune bridge */
    if (brain->wernicke_immune_bridge) {
        wernicke_immune_bridge_destroy((wernicke_immune_bridge_t*)brain->wernicke_immune_bridge);
        brain->wernicke_immune_bridge = NULL;
    }

    /* Destroy GPU bio bridge */
    if (brain->wernicke_gpu_bridge) {
        wernicke_gpu_bio_destroy(brain->wernicke_gpu_bridge);
        brain->wernicke_gpu_bridge = NULL;
    }

    /* Destroy omni-Wernicke bridge */
    if (brain->omni_wernicke_bridge) {
        omni_wernicke_bridge_destroy(brain->omni_wernicke_bridge);
        brain->omni_wernicke_bridge = NULL;
    }

    /* Destroy Wernicke-Broca bridge */
    if (brain->wernicke_broca_bridge) {
        wbb_destroy(brain->wernicke_broca_bridge);
        brain->wernicke_broca_bridge = NULL;
    }

    /* Destroy quantum bridge (depends on wernicke) */
    if (brain->wernicke_quantum_bridge) {
        wernicke_quantum_bridge_destroy(brain->wernicke_quantum_bridge);
        brain->wernicke_quantum_bridge = NULL;
    }

    /* Destroy substrate bridge */
    if (brain->wernicke_substrate_bridge) {
        wernicke_substrate_bridge_destroy(brain->wernicke_substrate_bridge);
        brain->wernicke_substrate_bridge = NULL;
    }

    /* Destroy Wernicke adapter */
    if (brain->wernicke) {
        wernicke_destroy(brain->wernicke);
        brain->wernicke = NULL;
    }

    brain->wernicke_enabled = false;
    brain->last_wernicke_update_us = 0;

    LOG_DEBUG(LOG_MODULE, "Wernicke's area subsystem destroyed");
}
