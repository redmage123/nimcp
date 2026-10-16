/**
 * @file nimcp_cognitive_transcript.h
 * @brief Cognitive Transcript — captures all cognitive stage outputs from brain_decide()
 *
 * The transcript provides a structured record of what each cognitive module
 * contributed during a single decision cycle. This enables:
 * - Response composition: The Response Composer uses transcript entries to
 *   build multi-sentence, contextually rich responses
 * - Introspection: The brain can examine its own cognitive process
 * - Debugging: Detailed visibility into which modules fired and what they produced
 *
 * BIOLOGICAL RATIONALE:
 * In biological brains, consciousness arises from the integration of parallel
 * processing streams (visual, auditory, semantic, emotional, motor planning).
 * The transcript captures these parallel streams before they're collapsed into
 * a single decision, preserving the rich cognitive context.
 */

#ifndef NIMCP_COGNITIVE_TRANSCRIPT_H
#define NIMCP_COGNITIVE_TRANSCRIPT_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Maximum entries in a single transcript */
#define NIMCP_TRANSCRIPT_MAX_ENTRIES    32

/** Maximum length of a transcript entry summary */
#define NIMCP_TRANSCRIPT_SUMMARY_LEN   256

/** Maximum key-value pairs per entry */
#define NIMCP_TRANSCRIPT_MAX_VALUES    8

/**
 * @brief Cognitive module identifiers
 *
 * Each module that can contribute to a decision has its own ID.
 * Order roughly follows the processing pipeline in brain_decide().
 */
typedef enum {
    TRANSCRIPT_MODULE_NONE = 0,

    /* Early stages */
    TRANSCRIPT_MODULE_WELLBEING,         /**< Wellbeing/distress monitoring */
    TRANSCRIPT_MODULE_ENGRAM,            /**< Engram recall (memory retrieval) */
    TRANSCRIPT_MODULE_CURIOSITY,         /**< Curiosity drive evaluation */
    TRANSCRIPT_MODULE_SLEEP,             /**< Sleep state / consolidation */

    /* Core processing */
    TRANSCRIPT_MODULE_PREDICTIVE,        /**< Predictive processing (prediction + error) */
    TRANSCRIPT_MODULE_HEMISPHERIC,       /**< Hemispheric processing + corpus callosum */

    /* Memory */
    TRANSCRIPT_MODULE_WORKING_MEMORY,    /**< Working memory integration */
    TRANSCRIPT_MODULE_SEMANTIC_MEMORY,   /**< Semantic memory query */
    TRANSCRIPT_MODULE_EPISODIC_MEMORY,   /**< Episodic / engram consolidation */

    /* Higher cognition */
    TRANSCRIPT_MODULE_REASONING,         /**< Reasoning engine (causal/abductive/convergent) */
    TRANSCRIPT_MODULE_INNER_DIALOGUE,    /**< Inner dialogue (multi-perspective deliberation) */
    TRANSCRIPT_MODULE_IMAGINATION,       /**< Imagination engine (prospective simulation) */
    TRANSCRIPT_MODULE_RECURSIVE_COG,     /**< Recursive cognition (goal decomposition) */
    TRANSCRIPT_MODULE_EXECUTIVE,         /**< Executive controller */

    /* Global integration */
    TRANSCRIPT_MODULE_GLOBAL_WORKSPACE,  /**< Global workspace competition */
    TRANSCRIPT_MODULE_EMOTION,           /**< Emotional tagging */
    TRANSCRIPT_MODULE_THEORY_OF_MIND,    /**< Theory of mind */
    TRANSCRIPT_MODULE_MIRROR_NEURON,     /**< Mirror neuron integration */

    /* Evaluation */
    TRANSCRIPT_MODULE_ETHICS,            /**< Ethics engine (Golden Rule) */
    TRANSCRIPT_MODULE_EPISTEMIC,         /**< Epistemic filtering (skepticism) */

    /* Cognitive subsystems (C5) */
    TRANSCRIPT_MODULE_FEP_PARIETAL,      /**< FEP parietal active inference */
    TRANSCRIPT_MODULE_PRED_HIERARCHY,    /**< Predictive hierarchy */
    TRANSCRIPT_MODULE_VAE,               /**< VAE anomaly detection */
    TRANSCRIPT_MODULE_JEPA,              /**< JEPA latent prediction */

    /* Language */
    TRANSCRIPT_MODULE_GROUNDED_LANG,     /**< Grounded language comprehension/production */
    TRANSCRIPT_MODULE_KNOWLEDGE,         /**< Knowledge graph query */
    TRANSCRIPT_MODULE_CREATIVE,          /**< Creative orchestrator */
    TRANSCRIPT_MODULE_INTUITION,         /**< Intuition integration */

    TRANSCRIPT_MODULE_COUNT              /**< Sentinel — number of module types */
} transcript_module_t;

/**
 * @brief Single transcript entry from one cognitive module
 *
 * Each entry records what a module contributed, how confident it was,
 * and a human-readable summary suitable for response composition.
 */
typedef struct transcript_entry {
    transcript_module_t module;          /**< Which module produced this */
    bool contributed;                    /**< Did this module actually fire? */

    float salience;                      /**< Relevance to overall response [0,1] */
    float confidence;                    /**< Module-specific confidence [0,1] */

    char summary[NIMCP_TRANSCRIPT_SUMMARY_LEN]; /**< Human-readable summary text */

    /** Named numeric values (module-specific key metrics) */
    float values[NIMCP_TRANSCRIPT_MAX_VALUES];
    const char* value_labels[NIMCP_TRANSCRIPT_MAX_VALUES];
    uint32_t num_values;

    uint64_t latency_us;                 /**< Processing time for this module */
} transcript_entry_t;

/**
 * @brief Full cognitive transcript for one brain_decide() call
 *
 * Captures outputs from all cognitive stages, with quick-access flags
 * for the Response Composer to determine response strategy.
 */
typedef struct cognitive_transcript {
    transcript_entry_t entries[NIMCP_TRANSCRIPT_MAX_ENTRIES];
    uint32_t num_entries;

    /** Aggregate metrics */
    uint64_t total_latency_us;           /**< Total cognitive processing time */
    float overall_coherence;             /**< How well modules agree [0,1] */
    float cognitive_load;                /**< How hard the brain worked [0,1] */

    /** Dominant signal (highest-salience entry) */
    transcript_module_t dominant_module;
    float dominant_salience;

    /** Response composition hints — set by transcript_finalize() */
    bool has_emotional_content;          /**< Emotion module fired with high valence/arousal */
    bool has_ethical_concern;            /**< Ethics module flagged a concern */
    bool has_creative_insight;           /**< Creative module produced something novel */
    bool has_knowledge_retrieval;        /**< Knowledge module found relevant facts */
    bool has_uncertainty;                /**< Epistemic module flagged low confidence */
    bool has_reasoning_chain;            /**< Reasoning module produced a chain */
    bool has_memory_recall;              /**< Episodic/semantic memory contributed */
    bool has_prediction;                 /**< Predictive modules generated forecasts */
    bool has_imagination;                /**< Imagination module ran simulation */
    bool has_inner_conflict;             /**< Inner dialogue had low agreement */
} cognitive_transcript_t;


/* ======================================================================== */
/* API                                                                      */
/* ======================================================================== */

/**
 * @brief Create a new empty transcript
 * @return Heap-allocated transcript, or NULL on failure. Caller must free.
 */
cognitive_transcript_t* transcript_create(void);

/**
 * @brief Free a transcript
 * @param t Transcript to free (NULL-safe)
 */
void transcript_free(cognitive_transcript_t* t);

/**
 * @brief Add an entry to the transcript
 *
 * @param t        Transcript
 * @param module   Module that produced this entry
 * @param salience Relevance score [0,1]
 * @param confidence Module confidence [0,1]
 * @param summary  Human-readable summary (copied, max NIMCP_TRANSCRIPT_SUMMARY_LEN-1)
 * @return Pointer to the added entry (for adding values), or NULL if full
 */
transcript_entry_t* transcript_add(cognitive_transcript_t* t,
                                   transcript_module_t module,
                                   float salience,
                                   float confidence,
                                   const char* summary);

/**
 * @brief Add a named numeric value to a transcript entry
 *
 * @param entry  Entry to add value to
 * @param label  Value label (pointer must remain valid — use string literals)
 * @param value  Numeric value
 */
void transcript_entry_add_value(transcript_entry_t* entry,
                                const char* label,
                                float value);

/**
 * @brief Finalize transcript after all stages complete
 *
 * Computes aggregate metrics, identifies dominant module,
 * and sets response composition hint flags.
 *
 * @param t Transcript to finalize
 */
void transcript_finalize(cognitive_transcript_t* t);

/**
 * @brief Get the module name as a string
 * @param module Module identifier
 * @return Static string name
 */
const char* transcript_module_name(transcript_module_t module);

/**
 * @brief Find entry by module type
 * @param t      Transcript to search
 * @param module Module to find
 * @return Pointer to entry, or NULL if not found
 */
const transcript_entry_t* transcript_find(const cognitive_transcript_t* t,
                                          transcript_module_t module);

/**
 * @brief Get entries sorted by salience (descending)
 *
 * Fills indices array with entry indices sorted by salience.
 *
 * @param t           Transcript
 * @param indices     Output array (must be at least num_entries large)
 * @param max_indices Maximum indices to return
 * @return Number of indices written
 */
uint32_t transcript_get_by_salience(const cognitive_transcript_t* t,
                                    uint32_t* indices,
                                    uint32_t max_indices);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_COGNITIVE_TRANSCRIPT_H */
