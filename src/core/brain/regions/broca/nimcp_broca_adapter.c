/**
 * @file nimcp_broca_adapter.c
 * @brief Implementation of Broca's region brain adapter
 *
 * WHAT: Unified adapter connecting Broca's region sub-modules to the brain system
 * WHY:  Enable seamless integration with cognitive layers, training, and event system
 * HOW:  Orchestrates syntax, phonological, and speech motor processors
 *
 * @version Phase B2: Broca's Region Brain Integration
 * @date 2025-11-23
 */

#include "core/brain/regions/broca/nimcp_broca_adapter.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "core/brain/regions/broca/nimcp_syntax_processor.h"
#include "core/brain/regions/broca/nimcp_phonological.h"
#include "core/brain/regions/broca/nimcp_speech_motor.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/memory/nimcp_memory_pool.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_wiring_helpers.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(broca_adapter, MESH_ADAPTER_CATEGORY_COGNITIVE)


/*=============================================================================
 * LOGGING MODULE IDENTIFIER
 *===========================================================================*/

#define BROCA_LOG_MODULE "BROCA"

/*=============================================================================
 * INTERNAL STRUCTURES
 *===========================================================================*/

/**
 * @brief Lexicon entry for internal storage
 */
typedef struct lexicon_node {
    broca_lexical_entry_t entry;
    struct lexicon_node* next;       /**< Hash collision chain */
} lexicon_node_t;

/**
 * @brief Working memory slot
 */
typedef struct {
    uint32_t word_id;
    float activation;                /**< Decay-based activation */
    double timestamp;                /**< When added */
} wm_slot_t;

/**
 * @brief Internal adapter structure
 */
struct broca_adapter {
    /* Configuration */
    broca_config_t config;

    /* Sub-modules */
    syntax_processor_t* syntax;
    phonological_processor_t* phonological;
    speech_motor_planner_t* motor;

    /* Lexicon (hash table) */
    lexicon_node_t** lexicon;
    uint32_t lexicon_capacity;
    uint32_t lexicon_count;
    /* Batch H concurrency: the lexicon is mutated by GL→Broca mirror
     * (driven by trainer + cascade self-train threads) and read by
     * broca_lookup_word from produce paths running on the RO socket
     * thread pool. Without a mutex the chain walks race the inserts
     * (writer's `adapter->lexicon[idx] = node` becomes visible mid-
     * read, or worse, `node->next` is dereferenced before the writer
     * has linked it). Plain mutex (not rwlock) keeps the API simple
     * and the lookup path is a short chain walk — contention is
     * minimal in practice. NULL means lexicon mutex creation failed
     * during init; the helpers degrade to no-op locking. */
    nimcp_mutex_t* lexicon_mutex;

    /* Working memory */
    wm_slot_t* working_memory;
    uint32_t wm_count;
    uint32_t wm_head;                /**< Next insert position */

    /* Output buffer */
    broca_output_command_t* output_commands;
    uint32_t output_count;
    uint32_t output_head;            /**< Next read position */

    /* Walkthrough-3 fix — state_mutex guards working_memory[] +
     * output_commands[] + their count/head indices.
     *
     * Both ring buffers are written by produce_utterance (cascade
     * stage_lexical / stage_syntactic) and read by external
     * consumers (broca_get_command, broca_get_commands,
     * broca_wm_get_contents). With the cascade running on the
     * RO socket pool concurrently with the main pool's trainer
     * loop, the head/count writes raced.
     *
     * Separate from lexicon_mutex to avoid lock-ordering hazards —
     * lexicon path doesn't touch WM/output and vice versa. NULL on
     * alloc failure degrades to unlocked. */
    nimcp_mutex_t* state_mutex;

    /* Callbacks */
    broca_lexical_callback_t lexical_callback;
    void* lexical_user_data;
    broca_motor_callback_t motor_callback;
    void* motor_user_data;
    broca_event_callback_t event_callback;
    void* event_user_data;

    /* State */
    broca_status_t status;
    broca_error_t last_error;
    double current_time_ms;

    /* Memory pool for hot-path allocations (Phase 1.5) */
    /* Pool for temp motor commands in broca_produce_utterance() */
    memory_pool_t motor_command_pool;

    /* Bio-async communication context */
    bio_module_context_t bio_ctx;
    nimcp_bio_channel_type_t default_channel;

    /* SNN substrate binding (broca_substrate population — 64K neurons,
     * created by nimcp_brain_factory_init_language_pops). pop_id < 0 = unbound.
     * Adapter logic uses (snn, snn_pop_id) to read spike rates / write bias.
     * Lifetime: SNN owns its pops; adapter does NOT free them. */
    snn_network_t* snn;
    int snn_pop_id;

    /* Grounded-language binding (opaque). Future production-path code
     * will use this for fallthrough when the local broca lexicon misses
     * but GL knows the word. Lifetime: GL is owned by brain. */
    void* grounded_lang;

    /* Statistics */
    broca_stats_t stats;
};

/*=============================================================================
 * INTERNAL HELPERS
 *===========================================================================*/

/**
 * @brief Simple string hash function
 */
static uint32_t hash_string(const char* str, uint32_t capacity) {
    uint32_t hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash % capacity;
}

/**
 * @brief Hash word ID
 */
static uint32_t hash_word_id(uint32_t word_id, uint32_t capacity) {
    return word_id % capacity;
}

/**
 * @brief Emit event to callback
 */
static void emit_event(broca_adapter_t* adapter, uint32_t event_type, const void* data) {
    if (adapter->config.enable_events && adapter->event_callback) {
        adapter->event_callback(event_type, data, adapter->event_user_data);
    }
}

/**
 * @brief Set error state
 */
static void set_error(broca_adapter_t* adapter, broca_error_t error) {
    if (!adapter) return;  /* NULL safety */
    adapter->last_error = error;
    if (error != BROCA_ERROR_NONE) {
        adapter->status = BROCA_STATUS_ERROR;
        LOG_ERROR("[%s] Error set: %d", BROCA_LOG_MODULE, error);
    }
}

/*=============================================================================
 * BIO-ASYNC MESSAGE HANDLERS (Forward declarations)
 *===========================================================================*/

static nimcp_error_t handle_lexical_access_request(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data);

static nimcp_error_t handle_syntax_parse_request(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data);

static nimcp_error_t handle_phonological_encode_request(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data);

static nimcp_error_t handle_motor_command_request(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data);

static nimcp_error_t handle_speech_feedback(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data);

static nimcp_error_t handle_utterance_production_request(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data);

/*=============================================================================
 * KG-DRIVEN WIRING CALLBACK
 *===========================================================================*/

/**
 * @brief KG-driven wiring handler callback
 *
 * WHAT: Register message handlers based on discovered wiring from KG
 * WHY:  Enables runtime assembly - module discovers its handlers from KG
 * HOW:  Orchestrator invokes this with message types from HANDLES_MESSAGE relations
 *
 * @param ctx Bio-async module context
 * @param message_types Array of message types to handle (from KG)
 * @param message_count Number of message types
 * @param user_data Broca adapter pointer
 * @return 0 on success, -1 on error
 */
static int broca_wiring_handler_callback(
    bio_module_context_t ctx,
    const bio_message_type_t* message_types,
    uint32_t message_count,
    void* user_data
) {
    if (!ctx || !message_types || message_count == 0) {
        return 0;  /* No handlers to register */
    }

    LOG_INFO("[%s] broca_wiring_handler_callback: registering %u handlers from KG",
             BROCA_LOG_MODULE, message_count);

    for (uint32_t i = 0; i < message_count; i++) {
        switch (message_types[i]) {
            case BIO_MSG_LEXICAL_ACCESS_REQUEST:
                bio_router_register_handler(ctx, message_types[i], handle_lexical_access_request);
                LOG_DEBUG("[%s]   Registered handler for BIO_MSG_LEXICAL_ACCESS_REQUEST", BROCA_LOG_MODULE);
                break;

            case BIO_MSG_SYNTAX_PARSE_REQUEST:
                bio_router_register_handler(ctx, message_types[i], handle_syntax_parse_request);
                LOG_DEBUG("[%s]   Registered handler for BIO_MSG_SYNTAX_PARSE_REQUEST", BROCA_LOG_MODULE);
                break;

            case BIO_MSG_PHONOLOGICAL_ENCODE_REQUEST:
                bio_router_register_handler(ctx, message_types[i], handle_phonological_encode_request);
                LOG_DEBUG("[%s]   Registered handler for BIO_MSG_PHONOLOGICAL_ENCODE_REQUEST", BROCA_LOG_MODULE);
                break;

            case BIO_MSG_MOTOR_COMMAND_REQUEST:
                bio_router_register_handler(ctx, message_types[i], handle_motor_command_request);
                LOG_DEBUG("[%s]   Registered handler for BIO_MSG_MOTOR_COMMAND_REQUEST", BROCA_LOG_MODULE);
                break;

            case BIO_MSG_SPEECH_FEEDBACK:
                bio_router_register_handler(ctx, message_types[i], handle_speech_feedback);
                LOG_DEBUG("[%s]   Registered handler for BIO_MSG_SPEECH_FEEDBACK", BROCA_LOG_MODULE);
                break;

            case BIO_MSG_UTTERANCE_PRODUCTION_REQUEST:
                bio_router_register_handler(ctx, message_types[i], handle_utterance_production_request);
                LOG_DEBUG("[%s]   Registered handler for BIO_MSG_UTTERANCE_PRODUCTION_REQUEST", BROCA_LOG_MODULE);
                break;

            default:
                LOG_DEBUG("[%s]   Unknown message type %u - skipping", BROCA_LOG_MODULE, message_types[i]);
                break;
        }
    }

    return 0;
}

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

broca_config_t broca_default_config(void) {
    broca_config_t config;
    config.max_words = BROCA_DEFAULT_MAX_WORDS;
    config.max_phonemes = BROCA_DEFAULT_MAX_PHONEMES;
    config.max_motor_commands = BROCA_DEFAULT_MAX_COMMANDS;
    config.working_memory_slots = BROCA_DEFAULT_WORKING_MEMORY_SLOTS;
    config.enable_working_memory = true;
    config.lexicon_size = BROCA_DEFAULT_LEXICON_SIZE;
    config.enable_lexicon = true;
    config.enable_coarticulation = true;
    config.enable_prosody = true;
    config.enable_morphology = true;
    config.enable_events = true;
    config.enable_training = false;
    config.learning_rate = 0.01F;
    config.planning_window_ms = BROCA_DEFAULT_PLANNING_WINDOW_MS;
    /* Bio-async: enabled by default, use acetylcholine for fast language processing */
    config.enable_bio_async = true;
    config.default_channel = BIO_CHANNEL_ACETYLCHOLINE;
    return config;
}

broca_adapter_t* broca_create(const broca_config_t* config) {
    /* WHAT: Create unified Broca's region adapter
     * WHY:  Central point for language production
     * HOW:  Initialize all sub-modules and data structures */

    LOG_INFO("[%s] Creating Broca's region adapter", BROCA_LOG_MODULE);

    broca_adapter_t* adapter = (broca_adapter_t*)nimcp_calloc(1, sizeof(broca_adapter_t));
    if (!adapter) {
        LOG_ERROR("[%s] Failed to allocate adapter memory", BROCA_LOG_MODULE);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "broca_create: adapter is NULL");
        return NULL;
    }

    /* Set configuration */
    if (config) {
        adapter->config = *config;
        LOG_DEBUG("[%s] Using provided configuration", BROCA_LOG_MODULE);
    } else {
        adapter->config = broca_default_config();
        LOG_DEBUG("[%s] Using default configuration", BROCA_LOG_MODULE);
    }

    /* Create syntax processor */
    LOG_DEBUG("[%s] Creating syntax processor", BROCA_LOG_MODULE);
    syntax_config_t syntax_cfg = syntax_default_config();
    syntax_cfg.max_units = adapter->config.max_words;
    syntax_cfg.enable_morphology = adapter->config.enable_morphology;
    adapter->syntax = syntax_create(&syntax_cfg);
    if (!adapter->syntax) {
        LOG_ERROR("[%s] Failed to create syntax processor", BROCA_LOG_MODULE);
        broca_destroy(adapter);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "broca_create: adapter->syntax is NULL");
        return NULL;
    }

    /* Create phonological processor */
    LOG_DEBUG("[%s] Creating phonological processor", BROCA_LOG_MODULE);
    phonological_config_t phono_cfg = phonological_default_config();
    phono_cfg.max_phonemes = adapter->config.max_phonemes;
    phono_cfg.enable_prosody = adapter->config.enable_prosody;
    phono_cfg.enable_coarticulation = adapter->config.enable_coarticulation;
    adapter->phonological = phonological_create(&phono_cfg);
    if (!adapter->phonological) {
        LOG_ERROR("[%s] Failed to create phonological processor", BROCA_LOG_MODULE);
        broca_destroy(adapter);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "broca_create: adapter->phonological is NULL");
        return NULL;
    }

    /* Create speech motor planner */
    LOG_DEBUG("[%s] Creating speech motor planner", BROCA_LOG_MODULE);
    speech_motor_config_t motor_cfg = speech_motor_default_config();
    motor_cfg.max_commands = adapter->config.max_motor_commands;
    motor_cfg.enable_coarticulation = adapter->config.enable_coarticulation;
    motor_cfg.planning_window_ms = adapter->config.planning_window_ms;
    adapter->motor = speech_motor_create(&motor_cfg);
    if (!adapter->motor) {
        LOG_ERROR("[%s] Failed to create speech motor planner", BROCA_LOG_MODULE);
        broca_destroy(adapter);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "broca_create: adapter->motor is NULL");
        return NULL;
    }

    /* Initialize lexicon */
    if (adapter->config.enable_lexicon) {
        LOG_DEBUG("[%s] Initializing lexicon (capacity=%u)", BROCA_LOG_MODULE,
                  adapter->config.lexicon_size);
        adapter->lexicon_capacity = adapter->config.lexicon_size;
        adapter->lexicon = (lexicon_node_t**)nimcp_calloc(
            adapter->lexicon_capacity, sizeof(lexicon_node_t*));
        if (!adapter->lexicon) {
            LOG_ERROR("[%s] Failed to allocate lexicon", BROCA_LOG_MODULE);
            broca_destroy(adapter);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "broca_create: adapter->lexicon is NULL");
            return NULL;
        }
        /* Batch H — lexicon writer/reader guard. NULL on alloc failure is
         * non-fatal; the helpers below treat NULL as no-op locking. */
        adapter->lexicon_mutex = nimcp_mutex_create(NULL);
        if (!adapter->lexicon_mutex) {
            LOG_WARN("[%s] lexicon_mutex create failed — running unlocked",
                      BROCA_LOG_MODULE);
        }
    }

    /* Walkthrough-3 — state_mutex for WM + output ring buffers.
     * Created unconditionally (independent of lexicon/wm config flags)
     * since the destroy path always frees it. */
    adapter->state_mutex = nimcp_mutex_create(NULL);
    if (!adapter->state_mutex) {
        LOG_WARN("[%s] state_mutex create failed — running unlocked",
                  BROCA_LOG_MODULE);
    }

    /* Initialize working memory */
    if (adapter->config.enable_working_memory) {
        LOG_DEBUG("[%s] Initializing working memory (slots=%u)", BROCA_LOG_MODULE,
                  adapter->config.working_memory_slots);
        adapter->working_memory = (wm_slot_t*)nimcp_calloc(
            adapter->config.working_memory_slots, sizeof(wm_slot_t));
        if (!adapter->working_memory) {
            LOG_ERROR("[%s] Failed to allocate working memory", BROCA_LOG_MODULE);
            broca_destroy(adapter);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "broca_create: adapter->working_memory is NULL");
            return NULL;
        }
    }

    /* Initialize output buffer */
    LOG_DEBUG("[%s] Initializing output buffer (max_commands=%u)", BROCA_LOG_MODULE,
              adapter->config.max_motor_commands);
    adapter->output_commands = (broca_output_command_t*)nimcp_calloc(
        adapter->config.max_motor_commands, sizeof(broca_output_command_t));
    if (!adapter->output_commands) {
        LOG_ERROR("[%s] Failed to allocate output buffer", BROCA_LOG_MODULE);
        broca_destroy(adapter);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "broca_create: adapter->output_commands is NULL");
        return NULL;
    }

    /* Initialize memory pool for hot-path allocations (Phase 1.5) */
    /* Pool for temp motor commands - 2 blocks for concurrent utterances */
    LOG_DEBUG("[%s] Creating motor command memory pool", BROCA_LOG_MODULE);
    memory_pool_config_t cmd_pool_config = {
        .block_size = adapter->config.max_motor_commands * sizeof(motor_command_t),
        .num_blocks = 2,
        .alignment = 16,  /* SIMD alignment */
        .enable_tracking = false,
        .enable_guard_pages = false
    };
    adapter->motor_command_pool = memory_pool_create(&cmd_pool_config);
    if (!adapter->motor_command_pool) {
        LOG_ERROR("[%s] Failed to create motor command memory pool", BROCA_LOG_MODULE);
        broca_destroy(adapter);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "broca_create: adapter->motor_command_pool is NULL");
        return NULL;
    }

    /* Initialize bio-async communication */
    adapter->bio_ctx = NULL;
    adapter->default_channel = adapter->config.default_channel;

    /* Initialize SNN substrate binding to "unbound" — calloc gives 0, but 0
     * is a valid SNN pop id, so we MUST explicitly set -1 here. */
    adapter->snn = NULL;
    adapter->snn_pop_id = -1;

    if (adapter->config.enable_bio_async && bio_router_is_initialized()) {
        LOG_DEBUG("[%s] Registering with bio-async router", BROCA_LOG_MODULE);

        bio_module_info_t bio_info = {
            .module_id = BIO_MODULE_BROCA,
            .module_name = "broca_region",
            .inbox_capacity = 64,
            .user_data = adapter
        };

        adapter->bio_ctx = bio_router_register_module(&bio_info);
        if (adapter->bio_ctx) {
            /* KG-Driven Wiring: Register callback for orchestrator to invoke
             * When orchestrator starts, it discovers HANDLES_MESSAGE relations
             * from the KG and invokes this callback with the message types */
            nimcp_error_t cb_result = bio_router_register_wiring_callback(
                BIO_MODULE_BROCA,
                (void*)broca_wiring_handler_callback,
                adapter
            );

            if (cb_result != NIMCP_SUCCESS) {
                /* Fallback: Direct registration if orchestrator not available
                 * This ensures backward compatibility with non-KG systems */
                LEGACY_HANDLER_REGISTRATION(
                    bio_router_register_handler(adapter->bio_ctx,
                        BIO_MSG_LEXICAL_ACCESS_REQUEST, handle_lexical_access_request)
                );
                LEGACY_HANDLER_REGISTRATION(
                    bio_router_register_handler(adapter->bio_ctx,
                        BIO_MSG_SYNTAX_PARSE_REQUEST, handle_syntax_parse_request)
                );
                LEGACY_HANDLER_REGISTRATION(
                    bio_router_register_handler(adapter->bio_ctx,
                        BIO_MSG_PHONOLOGICAL_ENCODE_REQUEST, handle_phonological_encode_request)
                );
                LEGACY_HANDLER_REGISTRATION(
                    bio_router_register_handler(adapter->bio_ctx,
                        BIO_MSG_MOTOR_COMMAND_REQUEST, handle_motor_command_request)
                );
                LEGACY_HANDLER_REGISTRATION(
                    bio_router_register_handler(adapter->bio_ctx,
                        BIO_MSG_SPEECH_FEEDBACK, handle_speech_feedback)
                );
                LEGACY_HANDLER_REGISTRATION(
                    bio_router_register_handler(adapter->bio_ctx,
                        BIO_MSG_UTTERANCE_PRODUCTION_REQUEST, handle_utterance_production_request)
                );
                LOG_INFO("[%s] Bio-async enabled (legacy direct registration)", BROCA_LOG_MODULE);
            } else {
                LOG_INFO("[%s] Bio-async enabled (KG-driven wiring callback registered)", BROCA_LOG_MODULE);
            }
        } else {
            LOG_WARNING("[%s] Failed to register with bio-async router", BROCA_LOG_MODULE);
        }
    } else if (adapter->config.enable_bio_async) {
        LOG_DEBUG("[%s] Bio-async enabled but router not initialized", BROCA_LOG_MODULE);
    }

    /* Initialize state */
    adapter->status = BROCA_STATUS_IDLE;
    adapter->last_error = BROCA_ERROR_NONE;
    adapter->current_time_ms = 0.0;

    LOG_INFO("[%s] Broca's region adapter created successfully", BROCA_LOG_MODULE);
    return adapter;
}

void broca_destroy(broca_adapter_t* adapter) {
    if (!adapter) return;

    LOG_INFO("[%s] Destroying Broca's region adapter", BROCA_LOG_MODULE);

    /* Unregister from bio-async router */
    if (adapter->bio_ctx) {
        LOG_DEBUG("[%s] Unregistering from bio-async router", BROCA_LOG_MODULE);
        bio_router_unregister_module(adapter->bio_ctx);
        adapter->bio_ctx = NULL;
    }

    /* Destroy sub-modules */
    if (adapter->syntax) {
        LOG_DEBUG("[%s] Destroying syntax processor", BROCA_LOG_MODULE);
        syntax_destroy(adapter->syntax);
    }
    if (adapter->phonological) {
        LOG_DEBUG("[%s] Destroying phonological processor", BROCA_LOG_MODULE);
        phonological_destroy(adapter->phonological);
    }
    if (adapter->motor) {
        LOG_DEBUG("[%s] Destroying speech motor planner", BROCA_LOG_MODULE);
        speech_motor_destroy(adapter->motor);
    }

    /* Free lexicon */
    if (adapter->lexicon) {
        LOG_DEBUG("[%s] Freeing lexicon", BROCA_LOG_MODULE);
        for (uint32_t i = 0; i < adapter->lexicon_capacity; i++) {
            lexicon_node_t* node = adapter->lexicon[i];
            while (node) {
                lexicon_node_t* next = node->next;
                nimcp_free(node);
                node = next;
            }
        }
        nimcp_free(adapter->lexicon);
    }
    if (adapter->lexicon_mutex) {
        nimcp_mutex_free(adapter->lexicon_mutex);
        adapter->lexicon_mutex = NULL;
    }
    if (adapter->state_mutex) {
        nimcp_mutex_free(adapter->state_mutex);
        adapter->state_mutex = NULL;
    }

    /* Free working memory */
    if (adapter->working_memory) {
        LOG_DEBUG("[%s] Freeing working memory", BROCA_LOG_MODULE);
        nimcp_free(adapter->working_memory);
    }

    /* Free output buffer */
    if (adapter->output_commands) {
        LOG_DEBUG("[%s] Freeing output buffer", BROCA_LOG_MODULE);
        nimcp_free(adapter->output_commands);
    }

    /* Destroy memory pool (Phase 1.5) */
    LOG_DEBUG("[%s] Destroying motor command memory pool", BROCA_LOG_MODULE);
    memory_pool_destroy(adapter->motor_command_pool);

    LOG_DEBUG("[%s] Broca's region adapter destroyed", BROCA_LOG_MODULE);
    nimcp_free(adapter);
}

bool broca_reset(broca_adapter_t* adapter) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "broca_reset: adapter is NULL");
        return false;
    }

    LOG_DEBUG("[%s] Resetting adapter state", BROCA_LOG_MODULE);

    /* Reset sub-modules */
    syntax_reset(adapter->syntax);
    phonological_reset(adapter->phonological);
    speech_motor_reset(adapter->motor);

    /* Clear working memory */
    if (adapter->working_memory) {
        memset(adapter->working_memory, 0,
               adapter->config.working_memory_slots * sizeof(wm_slot_t));
        adapter->wm_count = 0;
        adapter->wm_head = 0;
    }

    /* Clear output buffer */
    adapter->output_count = 0;
    adapter->output_head = 0;

    /* Reset state */
    adapter->status = BROCA_STATUS_IDLE;
    adapter->last_error = BROCA_ERROR_NONE;

    LOG_DEBUG("[%s] Adapter reset complete", BROCA_LOG_MODULE);
    return true;
}

/*=============================================================================
 * LEXICON MANAGEMENT
 *===========================================================================*/

bool broca_add_lexical_entry(broca_adapter_t* adapter,
                              const broca_lexical_entry_t* entry) {
    if (!adapter || !entry || !adapter->lexicon) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "broca_reset: required parameter is NULL (adapter, entry, adapter->lexicon)");
        return false;
    }

    /* Batch H — capacity check, alloc, and link MUST happen under the
     * lexicon_mutex. Capacity read outside the lock would race with a
     * concurrent insert that just bumped the counter. Pre-alloc the
     * node before locking so the lock window stays tight. */
    lexicon_node_t* node = (lexicon_node_t*)nimcp_calloc(1, sizeof(lexicon_node_t));
    if (!node) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "broca_reset: node is NULL");
        return false;
    }
    node->entry = *entry;
    node->next = NULL;

    /* Lock + capacity check + link + count. */
    if (adapter->lexicon_mutex) nimcp_mutex_lock(adapter->lexicon_mutex);
    if (adapter->lexicon_count >= adapter->lexicon_capacity) {
        if (adapter->lexicon_mutex) nimcp_mutex_unlock(adapter->lexicon_mutex);
        nimcp_free(node);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "broca_reset: capacity exceeded");
        return false;
    }
    uint32_t idx;
    if (entry->word_id != 0) {
        idx = hash_word_id(entry->word_id, adapter->lexicon_capacity);
    } else {
        idx = hash_string(entry->word, adapter->lexicon_capacity);
    }
    node->next = adapter->lexicon[idx];
    adapter->lexicon[idx] = node;
    adapter->lexicon_count++;
    if (adapter->lexicon_mutex) nimcp_mutex_unlock(adapter->lexicon_mutex);

    return true;
}

/* Phase 2C: thin shim used by the GL→Broca lexicon mirror. Takes
 * individual fields instead of a struct so the caller (in the language
 * subsystem) doesn't need to include broca's full header. Builds a
 * broca_lexical_entry_t on the stack and forwards to broca_add_lexical_entry.
 * Phonemes are left empty — broca's phonological_processor generates
 * them on-demand from spelling. */
bool broca_add_lexical_entry_v(void* adapter_v,
                                uint32_t word_id,
                                const char* word,
                                uint8_t pos,
                                float frequency)
{
    broca_adapter_t* adapter = (broca_adapter_t*)adapter_v;
    if (!adapter || !word || !word[0]) return false;

    broca_lexical_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    entry.word_id = word_id;
    strncpy(entry.word, word, sizeof(entry.word) - 1);
    entry.word[sizeof(entry.word) - 1] = '\0';
    entry.phoneme_count = 0;
    entry.pos = pos;
    entry.frequency = frequency;

    return broca_add_lexical_entry(adapter, &entry);
}

bool broca_lookup_word(const broca_adapter_t* adapter,
                        uint32_t word_id,
                        const char* word,
                        broca_lexical_entry_t* entry) {
    if (!adapter || !entry) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "broca_reset: required parameter is NULL (adapter, entry)");
        return false;
    }

    /* Try internal lexicon first */
    if (adapter->lexicon) {
        uint32_t idx;
        if (word_id != 0) {
            idx = hash_word_id(word_id, adapter->lexicon_capacity);
        } else if (word) {
            idx = hash_string(word, adapter->lexicon_capacity);
        } else {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "broca_reset: validation failed");
            return false;
        }

        /* Batch H — lexicon read must be guarded against concurrent
         * inserts. The cast to mutable adapter is safe: we're locking
         * for read-walking the chain, not mutating the adapter. The
         * `entry` copy out happens before unlock to keep the borrowed
         * `node->entry` pointer alive. */
        nimcp_mutex_t* mtx = adapter->lexicon_mutex;
        if (mtx) nimcp_mutex_lock(mtx);
        lexicon_node_t* node = adapter->lexicon[idx];
        while (node) {
            if (word_id != 0 && node->entry.word_id == word_id) {
                *entry = node->entry;
                if (mtx) nimcp_mutex_unlock(mtx);
                return true;
            }
            if (word && strcmp(node->entry.word, word) == 0) {
                *entry = node->entry;
                if (mtx) nimcp_mutex_unlock(mtx);
                return true;
            }
            node = node->next;
        }
        if (mtx) nimcp_mutex_unlock(mtx);
    }

    /* Try callback */
    if (adapter->lexical_callback) {
        return adapter->lexical_callback(word_id, word, entry,
                                         adapter->lexical_user_data);
    }

    return false; /* Word not found in lexicon or via callback */
}

bool broca_set_lexical_callback(broca_adapter_t* adapter,
                                 broca_lexical_callback_t callback,
                                 void* user_data) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "broca_reset: adapter is NULL");
        return false;
    }
    adapter->lexical_callback = callback;
    adapter->lexical_user_data = user_data;
    return true;
}

/*=============================================================================
 * PRODUCTION PIPELINE
 *===========================================================================*/

bool broca_begin_utterance(broca_adapter_t* adapter) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "broca_begin_utterance: adapter is NULL");
        return false;
    }

    /* Reset for new utterance */
    syntax_reset(adapter->syntax);
    phonological_reset(adapter->phonological);
    speech_motor_reset(adapter->motor);

    adapter->output_count = 0;
    adapter->output_head = 0;
    adapter->status = BROCA_STATUS_IDLE;
    adapter->last_error = BROCA_ERROR_NONE;

    return true;
}

bool broca_add_word(broca_adapter_t* adapter, const broca_input_word_t* word) {
    if (!adapter || !word) {
        set_error(adapter, BROCA_ERROR_INVALID_INPUT);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "broca_add_word: required parameter is NULL (adapter, word)");
        return false;
    }

    /* Look up word in lexicon */
    broca_lexical_entry_t entry;
    if (!broca_lookup_word(adapter, word->word_id, word->word, &entry)) {
        set_error(adapter, BROCA_ERROR_LEXICON_MISS);
        adapter->stats.lexicon_misses++;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "broca_add_word: broca_lookup_word is NULL");
        return false;
    }

    /* Add to syntax processor */
    adapter->status = BROCA_STATUS_LEXICAL_ACCESS;

    syntactic_unit_t unit;
    memset(&unit, 0, sizeof(syntactic_unit_t));
    unit.word_id = entry.word_id;
    unit.pos = (word->pos != 0) ? word->pos : entry.pos;
    unit.features.number = word->number;
    unit.features.person = word->person;
    unit.features.tense = word->tense;

    if (!syntax_add_unit(adapter->syntax, &unit)) {
        set_error(adapter, BROCA_ERROR_BUFFER_OVERFLOW);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "broca_add_word: syntax_add_unit is NULL");
        return false;
    }

    adapter->stats.words_processed++;
    return true;
}

bool broca_process_utterance(broca_adapter_t* adapter,
                              broca_utterance_result_t* result) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "broca_add_word: adapter is NULL");
        return false;
    }

    /* Initialize result */
    broca_utterance_result_t local_result;
    memset(&local_result, 0, sizeof(broca_utterance_result_t));

    /* Phase 1: Syntactic processing */
    adapter->status = BROCA_STATUS_SYNTACTIC;

    if (!syntax_build_tree(adapter->syntax)) {
        set_error(adapter, BROCA_ERROR_SYNTAX_FAILURE);
        adapter->stats.syntax_errors++;
        if (result) *result = local_result;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "broca_add_word: validation failed");
        return false;
    }

    bool agreement_valid = false;
    syntax_validate_grammar(adapter->syntax, &agreement_valid);
    local_result.syntax_valid = true;
    local_result.agreement_valid = agreement_valid;
    local_result.word_count = syntax_get_unit_count(adapter->syntax);

    emit_event(adapter, 1 /* BROCA_EVENT_SYNTAX_COMPLETE */, NULL);

    /* Phase 2: Phonological processing */
    adapter->status = BROCA_STATUS_PHONOLOGICAL;

    /* Get words from syntax and look up phonemes */
    uint32_t num_units = syntax_get_unit_count(adapter->syntax);
    for (uint32_t i = 0; i < num_units; i++) {
        syntactic_unit_t unit;
        if (syntax_get_unit(adapter->syntax, i, &unit)) {
            broca_lexical_entry_t entry;
            if (broca_lookup_word(adapter, unit.word_id, NULL, &entry)) {
                /* Add phonemes from lexical entry */
                for (uint32_t p = 0; p < entry.phoneme_count; p++) {
                    uint8_t category = PHONEME_CATEGORY_VOWEL;
                    /* Simple vowel detection */
                    uint8_t ph = entry.phonemes[p];
                    if (ph == 'a' || ph == 'e' || ph == 'i' || ph == 'o' || ph == 'u') {
                        category = PHONEME_CATEGORY_VOWEL;
                    } else {
                        category = PHONEME_CATEGORY_CONSONANT;
                    }
                    phonological_add_phoneme_detailed(adapter->phonological,
                                                       entry.phonemes[p],
                                                       category,
                                                       80.0F, 0.7F);
                }
            }
        }
    }

    /* Generate syllables */
    if (!phonological_generate_syllables(adapter->phonological)) {
        set_error(adapter, BROCA_ERROR_PHONOLOGICAL_FAILURE);
        adapter->stats.phonological_errors++;
        if (result) *result = local_result;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "broca_add_word: validation failed");
        return false;
    }

    /* Generate prosody */
    phonological_generate_prosody(adapter->phonological, INTONATION_PATTERN_FALLING);

    local_result.syllable_count = phonological_get_syllable_count(adapter->phonological);
    local_result.phoneme_count = phonological_get_phoneme_count(adapter->phonological);

    emit_event(adapter, 2 /* BROCA_EVENT_PHONEMES_READY */, NULL);

    /* Phase 3: Motor planning */
    adapter->status = BROCA_STATUS_MOTOR_PLANNING;

    /* Get phonemes and plan motor commands */
    uint32_t phoneme_count = phonological_get_phoneme_count(adapter->phonological);
    for (uint32_t i = 0; i < phoneme_count; i++) {
        /* Get phoneme from phonological processor */
        /* Note: Would need to add get_phoneme function or iterate through syllables */
    }

    /* Plan sequence using syllable phonemes */
    uint32_t syllable_count = phonological_get_syllable_count(adapter->phonological);
    float total_duration = 0.0F;

    for (uint32_t s = 0; s < syllable_count; s++) {
        syllable_t syllable;
        if (phonological_get_syllable(adapter->phonological, s, &syllable)) {
            /* Plan onset phonemes */
            for (uint32_t j = 0; j < syllable.onset_count; j++) {
                speech_motor_plan_phoneme(adapter->motor, syllable.onset[j].symbol);
            }
            /* Plan nucleus phonemes */
            for (uint32_t j = 0; j < syllable.nucleus_count; j++) {
                speech_motor_plan_phoneme(adapter->motor, syllable.nucleus[j].symbol);
            }
            /* Plan coda phonemes */
            for (uint32_t j = 0; j < syllable.coda_count; j++) {
                speech_motor_plan_phoneme(adapter->motor, syllable.coda[j].symbol);
            }
            total_duration += syllable.duration_ms;
        }
    }

    local_result.total_duration_ms = total_duration;

    /* Retrieve motor commands to output buffer - Phase 1.5 O(1) pool allocation */
    uint32_t max_commands = adapter->config.max_motor_commands;
    motor_command_t* temp_commands = (motor_command_t*)memory_pool_acquire(
        adapter->motor_command_pool);
    if (!temp_commands) {
        set_error(adapter, BROCA_ERROR_INTERNAL);
        if (result) *result = local_result;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "broca_add_word: validation failed");
        return false;
    }
    memset(temp_commands, 0, max_commands * sizeof(motor_command_t));

    uint32_t cmd_count = max_commands;
    if (speech_motor_get_commands(adapter->motor, temp_commands, &cmd_count)) {
        /* Walkthrough-3 — guard output ring writes against concurrent
         * broca_get_next_command / broca_get_all_commands readers. */
        if (adapter->state_mutex) nimcp_mutex_lock(adapter->state_mutex);
        /* Convert to output format */
        for (uint32_t i = 0; i < cmd_count && adapter->output_count < max_commands; i++) {
            broca_output_command_t* out = &adapter->output_commands[adapter->output_count];
            out->articulator = temp_commands[i].type;
            out->position = temp_commands[i].position;
            out->velocity = temp_commands[i].velocity;
            out->timestamp_ms = temp_commands[i].timestamp;
            out->phoneme = temp_commands[i].phoneme;
            adapter->output_count++;
        }
        if (adapter->state_mutex) nimcp_mutex_unlock(adapter->state_mutex);
    }
    /* Release back to pool (Phase 1.5) */
    memory_pool_release(adapter->motor_command_pool, temp_commands);

    /* Snapshot output_count for the local_result; this is a single
     * read of an atomic-aligned u32 so torn reads aren't a concern,
     * and command_count is advisory diagnostic data. */
    local_result.command_count = adapter->output_count;
    local_result.ready_for_articulation = (adapter->output_count > 0);

    emit_event(adapter, 3 /* BROCA_EVENT_MOTOR_READY */, NULL);

    /* Update statistics */
    adapter->stats.utterances_processed++;
    adapter->stats.successful_productions++;
    adapter->stats.phonemes_generated += local_result.phoneme_count;
    adapter->stats.commands_generated += local_result.command_count;

    adapter->status = BROCA_STATUS_READY;

    if (result) *result = local_result;
    return true;
}

bool broca_get_next_command(broca_adapter_t* adapter,
                             broca_output_command_t* command) {
    if (!adapter || !command) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "broca_add_word: required parameter is NULL (adapter, command)");
        return false;
    }

    /* Walkthrough-3 — guard output ring read + head advance.
     * Callback fires OUTSIDE the lock to avoid re-entry. */
    if (adapter->state_mutex) nimcp_mutex_lock(adapter->state_mutex);
    if (adapter->output_head >= adapter->output_count) {
        if (adapter->state_mutex) nimcp_mutex_unlock(adapter->state_mutex);
        return false;  /* No more commands - normal end condition */
    }
    *command = adapter->output_commands[adapter->output_head];
    adapter->output_head++;
    if (adapter->state_mutex) nimcp_mutex_unlock(adapter->state_mutex);

    /* Invoke motor callback if set */
    if (adapter->motor_callback) {
        adapter->motor_callback(command, adapter->motor_user_data);
    }

    return true;
}

bool broca_get_all_commands(broca_adapter_t* adapter,
                             broca_output_command_t* commands,
                             uint32_t* count) {
    if (!adapter || !commands || !count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (adapter, commands, count)");
        return false;
    }

    if (adapter->state_mutex) nimcp_mutex_lock(adapter->state_mutex);
    uint32_t available = adapter->output_count - adapter->output_head;
    uint32_t to_copy = (*count < available) ? *count : available;

    memcpy(commands, &adapter->output_commands[adapter->output_head],
           to_copy * sizeof(broca_output_command_t));

    adapter->output_head += to_copy;
    *count = to_copy;
    if (adapter->state_mutex) nimcp_mutex_unlock(adapter->state_mutex);

    return true;
}

/*=============================================================================
 * HIGH-LEVEL CONVENIENCE FUNCTIONS
 *===========================================================================*/

bool broca_produce_from_ids(broca_adapter_t* adapter,
                             const uint32_t* word_ids,
                             uint32_t num_words,
                             broca_utterance_result_t* result) {
    if (!adapter || !word_ids || num_words == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (adapter, word_ids)");
        return false;
    }

    if (!broca_begin_utterance(adapter)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "unknown: broca_begin_utterance is NULL");
        return false;
    }

    for (uint32_t i = 0; i < num_words; i++) {
        broca_input_word_t word;
        memset(&word, 0, sizeof(broca_input_word_t));
        word.word_id = word_ids[i];

        if (!broca_add_word(adapter, &word)) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "unknown: broca_add_word is NULL");
            return false;
        }
    }

    return broca_process_utterance(adapter, result);
}

bool broca_produce_from_strings(broca_adapter_t* adapter,
                                 const char* const* words,
                                 uint32_t num_words,
                                 broca_utterance_result_t* result) {
    if (!adapter || !words || num_words == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (adapter, words)");
        return false;
    }

    if (!broca_begin_utterance(adapter)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "unknown: broca_begin_utterance is NULL");
        return false;
    }

    for (uint32_t i = 0; i < num_words; i++) {
        broca_input_word_t word;
        memset(&word, 0, sizeof(broca_input_word_t));
        word.word_id = 0;
        strncpy(word.word, words[i], sizeof(word.word) - 1);

        if (!broca_add_word(adapter, &word)) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "unknown: broca_add_word is NULL");
            return false;
        }
    }

    return broca_process_utterance(adapter, result);
}

/*=============================================================================
 * WORKING MEMORY INTEGRATION
 *===========================================================================*/

bool broca_wm_push(broca_adapter_t* adapter, uint32_t word_id) {
    if (!adapter || !adapter->working_memory) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "broca_wm_push: required parameter is NULL (adapter, adapter->working_memory)");
        return false;
    }

    /* Walkthrough-3 — guard wm_head + wm_count + working_memory[]. */
    if (adapter->state_mutex) nimcp_mutex_lock(adapter->state_mutex);

    if (adapter->wm_count >= adapter->config.working_memory_slots) {
        /* WM full - overwrite oldest */
        adapter->wm_head = (adapter->wm_head + 1) % adapter->config.working_memory_slots;
    } else {
        adapter->wm_count++;
    }

    uint32_t idx = (adapter->wm_head + adapter->wm_count - 1) %
                   adapter->config.working_memory_slots;
    adapter->working_memory[idx].word_id = word_id;
    adapter->working_memory[idx].activation = 1.0F;
    adapter->working_memory[idx].timestamp = adapter->current_time_ms;

    if (adapter->state_mutex) nimcp_mutex_unlock(adapter->state_mutex);
    return true;
}

bool broca_wm_pop(broca_adapter_t* adapter, uint32_t* word_id) {
    if (!adapter || !word_id || !adapter->working_memory) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "broca_wm_pop: required parameter is NULL (adapter, word_id, adapter->working_memory)");
        return false;
    }

    if (adapter->state_mutex) nimcp_mutex_lock(adapter->state_mutex);
    if (adapter->wm_count == 0) {
        if (adapter->state_mutex) nimcp_mutex_unlock(adapter->state_mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "broca_wm_pop: adapter->wm_count is zero");
        return false;
    }

    *word_id = adapter->working_memory[adapter->wm_head].word_id;
    adapter->wm_head = (adapter->wm_head + 1) % adapter->config.working_memory_slots;
    adapter->wm_count--;

    if (adapter->state_mutex) nimcp_mutex_unlock(adapter->state_mutex);
    return true;
}

bool broca_wm_get_contents(const broca_adapter_t* adapter,
                            uint32_t* word_ids,
                            uint32_t* count) {
    if (!adapter || !word_ids || !count || !adapter->working_memory) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "broca_wm_pop: required parameter is NULL (adapter, word_ids, count, adapter->working_memory)");
        return false;
    }

    if (adapter->state_mutex) nimcp_mutex_lock(adapter->state_mutex);
    uint32_t to_copy = (*count < adapter->wm_count) ? *count : adapter->wm_count;

    for (uint32_t i = 0; i < to_copy; i++) {
        uint32_t idx = (adapter->wm_head + i) % adapter->config.working_memory_slots;
        word_ids[i] = adapter->working_memory[idx].word_id;
    }

    *count = to_copy;
    if (adapter->state_mutex) nimcp_mutex_unlock(adapter->state_mutex);
    return true;
}

/*=============================================================================
 * EVENT INTEGRATION
 *===========================================================================*/

bool broca_set_event_callback(broca_adapter_t* adapter,
                               broca_event_callback_t callback,
                               void* user_data) {
    if (!adapter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "broca_wm_pop: adapter is NULL");
        return false;
    }
    adapter->event_callback = callback;
    adapter->event_user_data = user_data;
    return true;
}

/*=============================================================================
 * TRAINING INTERFACE
 *===========================================================================*/

bool broca_train_phonemes(broca_adapter_t* adapter,
                           const uint8_t* target_phonemes,
                           uint32_t num_phonemes,
                           float learning_rate) {
    if (!adapter || !target_phonemes || num_phonemes == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "broca_wm_pop: required parameter is NULL (adapter, target_phonemes)");
        return false;
    }
    if (!adapter->config.enable_training) {
        return false; /* Training not enabled - not an error condition */
    }

    /* Simple training: compare output phonemes to target and adjust */
    /* This is a placeholder for more sophisticated learning */
    adapter->stats.training_iterations++;

    /* Use default learning rate if not specified */
    if (learning_rate <= 0.0F) {
        learning_rate = adapter->config.learning_rate;
    }

    /* Compute simple loss (placeholder) */
    float loss = 0.0F;
    uint32_t actual_count = phonological_get_phoneme_count(adapter->phonological);
    if (actual_count != num_phonemes) {
        loss = fabsf((float)actual_count - (float)num_phonemes) / (float)num_phonemes;
    }

    adapter->stats.training_loss = loss;
    return true;
}

bool broca_train_word(broca_adapter_t* adapter,
                       const char* word,
                       const uint8_t* phonemes,
                       uint32_t num_phonemes) {
    if (!adapter || !word || !phonemes || num_phonemes == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "broca_wm_pop: required parameter is NULL (adapter, word, phonemes)");
        return false;
    }

    /* Create lexical entry */
    broca_lexical_entry_t entry;
    memset(&entry, 0, sizeof(broca_lexical_entry_t));

    /* word_id MUST be 0 here. broca_add_lexical_entry() uses
     * hash_word_id(entry->word_id, capacity) for insertion, while
     * broca_lookup_word() uses hash_string(word, capacity) — different
     * buckets, silent miss. Setting word_id=0 routes both through the
     * same hash_string-based path. Same convention as the GL→Broca
     * mirror in grounded_language.c:4056-4062 (footgun previously fixed
     * there). */
    entry.word_id = 0;
    strncpy(entry.word, word, sizeof(entry.word) - 1);

    uint32_t copy_count = (num_phonemes < 16) ? num_phonemes : 16;
    memcpy(entry.phonemes, phonemes, copy_count);
    entry.phoneme_count = copy_count;
    entry.pos = POS_NOUN;  /* Default */
    entry.frequency = 0.5F;

    return broca_add_lexical_entry(adapter, &entry);
}

/*=============================================================================
 * STATUS AND DIAGNOSTICS
 *===========================================================================*/

broca_status_t broca_get_status(const broca_adapter_t* adapter) {
    if (!adapter) return BROCA_STATUS_ERROR;
    return adapter->status;
}

broca_error_t broca_get_last_error(const broca_adapter_t* adapter) {
    if (!adapter) return BROCA_ERROR_INTERNAL;
    return adapter->last_error;
}

const char* broca_error_string(broca_error_t error) {
    switch (error) {
        case BROCA_ERROR_NONE: return "No error";
        case BROCA_ERROR_INVALID_INPUT: return "Invalid input";
        case BROCA_ERROR_SYNTAX_FAILURE: return "Syntax processing failed";
        case BROCA_ERROR_PHONOLOGICAL_FAILURE: return "Phonological processing failed";
        case BROCA_ERROR_MOTOR_PLANNING_FAILURE: return "Motor planning failed";
        case BROCA_ERROR_WORKING_MEMORY_FULL: return "Working memory full";
        case BROCA_ERROR_LEXICON_MISS: return "Word not found in lexicon";
        case BROCA_ERROR_BUFFER_OVERFLOW: return "Buffer overflow";
        case BROCA_ERROR_INTERNAL: return "Internal error";
        default: return "Unknown error";
    }
}

const char* broca_status_string(broca_status_t status) {
    switch (status) {
        case BROCA_STATUS_IDLE: return "Idle";
        case BROCA_STATUS_LEXICAL_ACCESS: return "Lexical access";
        case BROCA_STATUS_SYNTACTIC: return "Syntactic processing";
        case BROCA_STATUS_PHONOLOGICAL: return "Phonological planning";
        case BROCA_STATUS_MOTOR_PLANNING: return "Motor planning";
        case BROCA_STATUS_READY: return "Ready";
        case BROCA_STATUS_ERROR: return "Error";
        default: return "Unknown";
    }
}

bool broca_get_stats(const broca_adapter_t* adapter, broca_stats_t* stats) {
    if (!adapter || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "broca_get_stats: required parameter is NULL (adapter, stats)");
        return false;
    }
    *stats = adapter->stats;
    return true;
}

bool broca_get_config(const broca_adapter_t* adapter, broca_config_t* config) {
    if (!adapter || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "broca_get_config: required parameter is NULL (adapter, config)");
        return false;
    }
    *config = adapter->config;
    return true;
}

/*=============================================================================
 * SUB-MODULE ACCESS
 *===========================================================================*/

syntax_processor_t* broca_get_syntax_processor(broca_adapter_t* adapter) {
    if (!adapter) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adapter is NULL");

        return NULL;

    }
    return adapter->syntax;
}

phonological_processor_t* broca_get_phonological_processor(broca_adapter_t* adapter) {
    if (!adapter) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adapter is NULL");

        return NULL;

    }
    return adapter->phonological;
}

speech_motor_planner_t* broca_get_speech_motor_planner(broca_adapter_t* adapter) {
    if (!adapter) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adapter is NULL");

        return NULL;

    }
    return adapter->motor;
}

/*=============================================================================
 * BIO-ASYNC COMMUNICATION API
 *===========================================================================*/

bio_module_context_t broca_get_bio_context(broca_adapter_t* adapter) {
    if (!adapter) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adapter is NULL");

        return NULL;

    }
    return adapter->bio_ctx;
}

uint32_t broca_process_bio_messages(broca_adapter_t* adapter, uint32_t max_messages) {
    if (!adapter || !adapter->bio_ctx) return 0;

    uint32_t processed = bio_router_process_inbox(adapter->bio_ctx, max_messages);
    if (processed > 0) {
        LOG_DEBUG("[%s] Processed %u bio-async messages", BROCA_LOG_MODULE, processed);
    }
    return processed;
}

nimcp_bio_future_t broca_request_lexical_access_async(
    broca_adapter_t* adapter,
    uint32_t word_id,
    const char* word) {

    if (!adapter || !adapter->bio_ctx) {
        LOG_WARNING("[%s] Cannot request lexical access: bio-async not available",
                    BROCA_LOG_MODULE);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "broca_process_bio_messages: required parameter is NULL (adapter, adapter->bio_ctx)");
        return NULL;
    }

    LOG_DEBUG("[%s] Requesting lexical access for word_id=%u", BROCA_LOG_MODULE, word_id);

    /* Create lexical access request message */
    bio_msg_lexical_access_request_t msg;
    memset(&msg, 0, sizeof(msg));

    msg.header.type = BIO_MSG_LEXICAL_ACCESS_REQUEST;
    msg.header.source_module = BIO_MODULE_BROCA;
    msg.header.target_module = BIO_MODULE_WERNICKE;  /* Send to Wernicke's area */
    msg.header.payload_size = sizeof(msg);
    msg.header.channel = adapter->default_channel;

    msg.word_id = word_id;
    if (word) {
        strncpy(msg.word, word, sizeof(msg.word) - 1);
    }

    /* Send async and get promise */
    nimcp_bio_promise_t promise = bio_router_send_async(
        adapter->bio_ctx, &msg, sizeof(msg), adapter->default_channel);

    if (!promise) {
        LOG_ERROR("[%s] Failed to send lexical access request", BROCA_LOG_MODULE);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "broca_process_bio_messages: promise is NULL");
        return NULL;
    }

    return nimcp_bio_promise_get_future(promise);
}

nimcp_bio_future_t broca_request_syntax_parse_async(
    broca_adapter_t* adapter,
    const uint32_t* word_ids,
    uint8_t word_count) {

    if (!adapter || !adapter->bio_ctx || !word_ids || word_count == 0) {
        LOG_WARNING("[%s] Cannot request syntax parse: invalid arguments", BROCA_LOG_MODULE);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "broca_process_bio_messages: required parameter is NULL (adapter, adapter->bio_ctx, word_ids)");
        return NULL;
    }

    LOG_DEBUG("[%s] Requesting syntax parse for %u words", BROCA_LOG_MODULE, word_count);

    bio_msg_syntax_parse_request_t msg;
    memset(&msg, 0, sizeof(msg));

    msg.header.type = BIO_MSG_SYNTAX_PARSE_REQUEST;
    msg.header.source_module = BIO_MODULE_BROCA;
    msg.header.target_module = BIO_MODULE_BROCA;  /* Self-processing */
    msg.header.payload_size = sizeof(msg);
    msg.header.channel = adapter->default_channel;

    uint8_t copy_count = (word_count < 16) ? word_count : 16;
    memcpy(msg.word_ids, word_ids, copy_count * sizeof(uint32_t));
    msg.word_count = copy_count;
    msg.parse_mode = 0;  /* Full parse */

    nimcp_bio_promise_t promise = bio_router_send_async(
        adapter->bio_ctx, &msg, sizeof(msg), adapter->default_channel);

    if (!promise) {
        LOG_ERROR("[%s] Failed to send syntax parse request", BROCA_LOG_MODULE);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "broca_process_bio_messages: promise is NULL");
        return NULL;
    }

    return nimcp_bio_promise_get_future(promise);
}

nimcp_bio_future_t broca_request_motor_command_async(
    broca_adapter_t* adapter,
    uint8_t phoneme,
    float duration_ms,
    float pitch_hz) {

    if (!adapter || !adapter->bio_ctx) {
        LOG_WARNING("[%s] Cannot request motor command: bio-async not available",
                    BROCA_LOG_MODULE);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "broca_process_bio_messages: required parameter is NULL (adapter, adapter->bio_ctx)");
        return NULL;
    }

    LOG_DEBUG("[%s] Requesting motor command for phoneme=%u", BROCA_LOG_MODULE, phoneme);

    bio_msg_motor_command_request_t msg;
    memset(&msg, 0, sizeof(msg));

    msg.header.type = BIO_MSG_MOTOR_COMMAND_REQUEST;
    msg.header.source_module = BIO_MODULE_BROCA;
    msg.header.target_module = BIO_MODULE_SPEECH_CORTEX;  /* Send to speech motor cortex */
    msg.header.payload_size = sizeof(msg);
    msg.header.channel = adapter->default_channel;

    msg.phoneme = phoneme;
    msg.duration_ms = duration_ms;
    msg.pitch_hz = pitch_hz;
    msg.intensity = 0.7F;  /* Default intensity */

    nimcp_bio_promise_t promise = bio_router_send_async(
        adapter->bio_ctx, &msg, sizeof(msg), adapter->default_channel);

    if (!promise) {
        LOG_ERROR("[%s] Failed to send motor command request", BROCA_LOG_MODULE);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "broca_process_bio_messages: promise is NULL");
        return NULL;
    }

    return nimcp_bio_promise_get_future(promise);
}

nimcp_error_t broca_broadcast_utterance_complete(
    broca_adapter_t* adapter,
    const broca_utterance_result_t* result) {

    if (!adapter || !result) {
        return NIMCP_BIO_ERROR_NOT_INITIALIZED;
    }

    if (!adapter->bio_ctx) {
        LOG_DEBUG("[%s] Cannot broadcast: bio-async not available", BROCA_LOG_MODULE);
        return NIMCP_SUCCESS;  /* Not an error if bio-async disabled */
    }

    LOG_INFO("[%s] Broadcasting utterance complete (words=%u, phonemes=%u)",
             BROCA_LOG_MODULE, result->word_count, result->phoneme_count);

    /* Create utterance complete message - reusing syntax_parse_result for now */
    bio_msg_syntax_parse_result_t msg;
    memset(&msg, 0, sizeof(msg));

    msg.header.type = BIO_MSG_UTTERANCE_PRODUCTION_COMPLETE;
    msg.header.source_module = BIO_MODULE_BROCA;
    msg.header.target_module = 0;  /* Broadcast */
    msg.header.payload_size = sizeof(msg);
    msg.header.channel = adapter->default_channel;
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;

    msg.valid = result->syntax_valid;
    msg.constituent_count = (uint8_t)result->word_count;
    msg.complexity = result->total_duration_ms;

    return bio_router_broadcast(adapter->bio_ctx, &msg, sizeof(msg));
}

nimcp_error_t broca_handle_speech_feedback(
    broca_adapter_t* adapter,
    uint8_t phoneme_id,
    float confidence,
    float timing_error) {

    if (!adapter) return NIMCP_BIO_ERROR_NOT_INITIALIZED;

    LOG_DEBUG("[%s] Received speech feedback: phoneme=%u, conf=%.2f, timing_err=%.2fms",
              BROCA_LOG_MODULE, phoneme_id, confidence, timing_error);

    /* Process feedback for self-monitoring and error correction */
    if (confidence < 0.5F || fabsf(timing_error) > 50.0F) {
        LOG_WARNING("[%s] Speech feedback indicates potential error: phoneme=%u",
                    BROCA_LOG_MODULE, phoneme_id);
        /* Could trigger re-planning or adjustment here */
    }

    return NIMCP_SUCCESS;
}

/*=============================================================================
 * BIO-ASYNC MESSAGE HANDLERS (Implementation)
 *===========================================================================*/

static nimcp_error_t handle_lexical_access_request(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data) {

    broca_adapter_t* adapter = (broca_adapter_t*)user_data;
    const bio_msg_lexical_access_request_t* req = (const bio_msg_lexical_access_request_t*)msg;

    if (!adapter || !req || msg_size < sizeof(bio_msg_lexical_access_request_t)) {
        LOG_ERROR("[%s] Invalid lexical access request", BROCA_LOG_MODULE);
        return NIMCP_BIO_ERROR_NOT_INITIALIZED;
    }

    LOG_DEBUG("[%s] Handling lexical access request: word_id=%u, word='%s'",
              BROCA_LOG_MODULE, req->word_id, req->word);

    /* Look up word in lexicon */
    broca_lexical_entry_t entry;
    bool found = broca_lookup_word(adapter, req->word_id, req->word, &entry);

    /* Build response */
    bio_msg_lexical_access_response_t response;
    memset(&response, 0, sizeof(response));

    response.header.type = BIO_MSG_LEXICAL_ACCESS_RESPONSE;
    response.header.source_module = BIO_MODULE_BROCA;
    response.header.target_module = req->header.source_module;
    response.header.payload_size = sizeof(response);
    response.header.channel = req->header.channel;

    response.word_id = entry.word_id;
    response.found = found;
    if (found) {
        memcpy(response.phonemes, entry.phonemes, sizeof(response.phonemes));
        response.phoneme_count = entry.phoneme_count;
        response.pos = entry.pos;
        response.frequency = entry.frequency;
        response.activation = 1.0F;
    }

    /* Complete promise with response */
    if (response_promise) {
        nimcp_bio_promise_complete(response_promise, &response);
    }

    return NIMCP_SUCCESS;
}

static nimcp_error_t handle_syntax_parse_request(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data) {

    broca_adapter_t* adapter = (broca_adapter_t*)user_data;
    const bio_msg_syntax_parse_request_t* req = (const bio_msg_syntax_parse_request_t*)msg;

    if (!adapter || !req || msg_size < sizeof(bio_msg_syntax_parse_request_t)) {
        LOG_ERROR("[%s] Invalid syntax parse request", BROCA_LOG_MODULE);
        return NIMCP_BIO_ERROR_NOT_INITIALIZED;
    }

    LOG_DEBUG("[%s] Handling syntax parse request: word_count=%u", BROCA_LOG_MODULE, req->word_count);

    /* Perform syntax parsing */
    bool parse_valid = true;
    float complexity = 0.0F;

    /* Reset and add words */
    syntax_reset(adapter->syntax);
    for (uint8_t i = 0; i < req->word_count; i++) {
        syntactic_unit_t unit;
        memset(&unit, 0, sizeof(unit));
        unit.word_id = req->word_ids[i];
        if (!syntax_add_unit(adapter->syntax, &unit)) {
            parse_valid = false;
            break;
        }
    }

    if (parse_valid) {
        parse_valid = syntax_build_tree(adapter->syntax);
        complexity = (float)req->word_count * 1.5F;  /* Simple complexity estimate */
    }

    /* Build response */
    bio_msg_syntax_parse_result_t response;
    memset(&response, 0, sizeof(response));

    response.header.type = BIO_MSG_SYNTAX_PARSE_RESULT;
    response.header.source_module = BIO_MODULE_BROCA;
    response.header.target_module = req->header.source_module;
    response.header.payload_size = sizeof(response);
    response.header.channel = req->header.channel;

    response.valid = parse_valid;
    response.constituent_count = req->word_count;
    response.complexity = complexity;

    if (response_promise) {
        nimcp_bio_promise_complete(response_promise, &response);
    }

    return NIMCP_SUCCESS;
}

static nimcp_error_t handle_phonological_encode_request(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data) {

    broca_adapter_t* adapter = (broca_adapter_t*)user_data;
    const bio_msg_phonological_encode_request_t* req =
        (const bio_msg_phonological_encode_request_t*)msg;

    if (!adapter || !req || msg_size < sizeof(bio_msg_phonological_encode_request_t)) {
        LOG_ERROR("[%s] Invalid phonological encode request", BROCA_LOG_MODULE);
        return NIMCP_BIO_ERROR_NOT_INITIALIZED;
    }

    LOG_DEBUG("[%s] Handling phonological encode request: phoneme_count=%u",
              BROCA_LOG_MODULE, req->phoneme_count);

    /* Reset and add phonemes */
    phonological_reset(adapter->phonological);
    for (uint8_t i = 0; i < req->phoneme_count && i < 32; i++) {
        phonological_add_phoneme_detailed(adapter->phonological,
            req->phonemes[i], PHONEME_CATEGORY_CONSONANT, 80.0F, 0.7F);
    }

    /* Generate syllables */
    bool success = phonological_generate_syllables(adapter->phonological);

    /* Build response */
    bio_msg_phonological_encode_result_t response;
    memset(&response, 0, sizeof(response));

    response.header.type = BIO_MSG_PHONOLOGICAL_ENCODE_RESULT;
    response.header.source_module = BIO_MODULE_BROCA;
    response.header.target_module = req->header.source_module;
    response.header.payload_size = sizeof(response);
    response.header.channel = req->header.channel;

    response.success = success;
    response.syllable_count = phonological_get_syllable_count(adapter->phonological);

    if (response_promise) {
        nimcp_bio_promise_complete(response_promise, &response);
    }

    return NIMCP_SUCCESS;
}

static nimcp_error_t handle_motor_command_request(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data) {

    broca_adapter_t* adapter = (broca_adapter_t*)user_data;
    const bio_msg_motor_command_request_t* req = (const bio_msg_motor_command_request_t*)msg;

    if (!adapter || !req || msg_size < sizeof(bio_msg_motor_command_request_t)) {
        LOG_ERROR("[%s] Invalid motor command request", BROCA_LOG_MODULE);
        return NIMCP_BIO_ERROR_NOT_INITIALIZED;
    }

    LOG_DEBUG("[%s] Handling motor command request: phoneme=%u", BROCA_LOG_MODULE, req->phoneme);

    /* Generate motor command for the phoneme */
    speech_motor_plan_phoneme(adapter->motor, req->phoneme);

    /* Build response with articulator positions */
    bio_msg_motor_command_result_t response;
    memset(&response, 0, sizeof(response));

    response.header.type = BIO_MSG_MOTOR_COMMAND_RESULT;
    response.header.source_module = BIO_MODULE_BROCA;
    response.header.target_module = req->header.source_module;
    response.header.payload_size = sizeof(response);
    response.header.channel = req->header.channel;

    /* Fill in articulator positions based on phoneme type */
    /* This is a simplified model - real values would come from speech motor planner */
    response.lip_aperture = 0.5F;
    response.tongue_height = 0.5F;
    response.tongue_advance = 0.5F;
    response.jaw_opening = 0.3F;
    response.velum_opening = 0.0F;
    response.larynx_tension = 0.5F;
    response.timestamp_ms = adapter->current_time_ms;

    if (response_promise) {
        nimcp_bio_promise_complete(response_promise, &response);
    }

    return NIMCP_SUCCESS;
}

static nimcp_error_t handle_speech_feedback(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data) {

    broca_adapter_t* adapter = (broca_adapter_t*)user_data;
    const bio_msg_phoneme_recognized_t* feedback = (const bio_msg_phoneme_recognized_t*)msg;

    if (!adapter || !feedback || msg_size < sizeof(bio_msg_phoneme_recognized_t)) {
        LOG_ERROR("[%s] Invalid speech feedback message", BROCA_LOG_MODULE);
        return NIMCP_BIO_ERROR_NOT_INITIALIZED;
    }

    LOG_DEBUG("[%s] Received speech feedback: phoneme='%s', confidence=%.2f",
              BROCA_LOG_MODULE, feedback->phoneme_symbol, feedback->confidence);

    /* Process feedback for error monitoring */
    broca_handle_speech_feedback(adapter, feedback->phoneme_id,
                                  feedback->confidence, 0.0F);

    /* No response needed for feedback */
    (void)response_promise;

    return NIMCP_SUCCESS;
}

static nimcp_error_t handle_utterance_production_request(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data) {

    broca_adapter_t* adapter = (broca_adapter_t*)user_data;
    const bio_msg_syntax_parse_request_t* req = (const bio_msg_syntax_parse_request_t*)msg;

    if (!adapter || !req || msg_size < sizeof(bio_msg_syntax_parse_request_t)) {
        LOG_ERROR("[%s] Invalid utterance production request", BROCA_LOG_MODULE);
        return NIMCP_BIO_ERROR_NOT_INITIALIZED;
    }

    LOG_INFO("[%s] Handling utterance production request: word_count=%u",
             BROCA_LOG_MODULE, req->word_count);

    /* Process complete utterance */
    broca_utterance_result_t result;
    bool success = broca_produce_from_ids(adapter, req->word_ids, req->word_count, &result);

    /* Build response */
    bio_msg_syntax_parse_result_t response;
    memset(&response, 0, sizeof(response));

    response.header.type = BIO_MSG_UTTERANCE_PRODUCTION_COMPLETE;
    response.header.source_module = BIO_MODULE_BROCA;
    response.header.target_module = req->header.source_module;
    response.header.payload_size = sizeof(response);
    response.header.channel = req->header.channel;

    response.valid = success && result.syntax_valid;
    response.constituent_count = (uint8_t)result.word_count;
    response.complexity = result.total_duration_ms;

    if (response_promise) {
        nimcp_bio_promise_complete(response_promise, &response);
    }

    /* Also broadcast completion */
    if (success) {
        broca_broadcast_utterance_complete(adapter, &result);
    }

    return NIMCP_SUCCESS;
}

/*=============================================================================
 * SNN SUBSTRATE BINDING (broca_substrate population)
 *===========================================================================*/

bool broca_attach_snn_pop(broca_adapter_t* adapter,
                          snn_network_t* snn,
                          int pop_id) {
    if (!adapter) {
        LOG_WARN("[%s] broca_attach_snn_pop: adapter is NULL", BROCA_LOG_MODULE);
        return false;
    }
    /* Negative pop id or NULL snn = unbind (idempotent reset path). Both
     * legs MUST clear together so callers never see (snn != NULL, id < 0). */
    if (!snn || pop_id < 0) {
        adapter->snn = NULL;
        adapter->snn_pop_id = -1;
        LOG_DEBUG("[%s] SNN substrate binding cleared", BROCA_LOG_MODULE);
        return true;
    }
    adapter->snn = snn;
    adapter->snn_pop_id = pop_id;
    LOG_INFO("[%s] attached to SNN pop id=%d (broca_substrate)",
             BROCA_LOG_MODULE, pop_id);
    return true;
}

int broca_get_snn_pop_id(const broca_adapter_t* adapter) {
    if (!adapter) return -1;
    return adapter->snn_pop_id;
}

snn_network_t* broca_get_snn_network(const broca_adapter_t* adapter) {
    if (!adapter) return NULL;
    return adapter->snn;
}

bool broca_attach_grounded_language(broca_adapter_t* adapter, void* gl) {
    if (!adapter) return false;
    adapter->grounded_lang = gl;       /* NULL = unbind */
    return true;
}

void* broca_get_grounded_language(const broca_adapter_t* adapter) {
    if (!adapter) return NULL;
    return adapter->grounded_lang;
}
