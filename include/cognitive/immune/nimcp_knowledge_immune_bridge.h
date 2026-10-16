/**
 * @file nimcp_knowledge_immune_bridge.h
 * @brief Knowledge Base-Immune System Integration Bridge
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Bidirectional integration between brain immune system and knowledge base
 * WHY:  Biological evidence shows neuroinflammation impairs semantic memory and knowledge retrieval.
 *       Essential for realistic cognitive modeling under immune stress.
 * HOW:  Cytokines slow retrieval speed and impair encoding, while health knowledge affects
 *       immune-related decision making. Chronic inflammation causes cognitive decline.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * IMMUNE → KNOWLEDGE PATHWAYS:
 * ---------------------------
 * 1. Pro-inflammatory Cytokines (IL-1β, IL-6, TNF-α):
 *    - Impair hippocampal long-term potentiation (LTP)
 *    - Reduce synaptic plasticity → slower semantic access
 *    - Increase retrieval latency for declarative knowledge
 *    - Impair new fact encoding and consolidation
 *    - Reference: Yirmiya & Goshen (2011) "Immune modulation of learning and memory"
 *
 * 2. IL-6 and Semantic Processing:
 *    - Elevated IL-6 correlates with semantic memory deficits
 *    - Slows category fluency tasks
 *    - Reduces association strength between concepts
 *    - Reference: Marsland et al. (2006) "IL-6 and verbal memory"
 *
 * 3. Chronic Inflammation and Cognitive Decline:
 *    - Sustained inflammation → progressive knowledge impairment
 *    - Reduced retrieval accuracy over time
 *    - Difficulty forming new semantic associations
 *    - Reference: Perry et al. (2010) "Systemic inflammation and cognitive function"
 *
 * 4. Sickness Behavior:
 *    - Cytokines induce reduced cognitive engagement
 *    - Lower curiosity and exploration (less new learning)
 *    - Fatigue reduces knowledge acquisition motivation
 *    - Reference: Dantzer (2001) "Cytokine-induced sickness behavior"
 *
 * 5. Anti-inflammatory Cytokines (IL-10):
 *    - Restore synaptic plasticity
 *    - Normalize retrieval speed
 *    - Enable new learning during recovery
 *    - Reference: Rizzo et al. (2018) "IL-10 and cognitive function"
 *
 * KNOWLEDGE → IMMUNE PATHWAYS:
 * ---------------------------
 * 1. Health Knowledge Affects Immune Decisions:
 *    - Knowledge about pathogens modulates immune response
 *    - Understanding of stress management reduces inflammation
 *    - Awareness of health practices influences immune function
 *    - Reference: Vedhara et al. (2007) "Health beliefs and immunity"
 *
 * 2. Semantic Processing During Illness:
 *    - Brain prioritizes illness-relevant knowledge during infection
 *    - Faster retrieval of health/medical concepts when sick
 *    - Selective attention to disease-related information
 *    - Reference: Schaller & Park (2011) "Behavioral immune system"
 *
 * 3. Learning About Threats:
 *    - Acquiring knowledge about dangers triggers immune priming
 *    - Reading about diseases increases immune vigilance
 *    - Semantic understanding of risk modulates immune preparedness
 *    - Reference: Schaller (2006) "Parasites, behavioral defenses"
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                    KNOWLEDGE-IMMUNE BRIDGE                                 ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  IMMUNE → KNOWLEDGE PATHWAYS                        │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │  CYTOKINES   │                                                 │  ║
 * ║   │   │ ──────────── │                                                 │  ║
 * ║   │   │ IL-1β → +40% │  ───────┐                                       │  ║
 * ║   │   │ IL-6  → +30% │         │                                       │  ║
 * ║   │   │ TNF-α → +50% │         ├──→ Retrieval Latency                 │  ║
 * ║   │   │              │         │    (Slower access)                    │  ║
 * ║   │   └──────────────┘         │                                       │  ║
 * ║   │                            ▼                                       │  ║
 * ║   │   ┌─────────────────────────────────┐                             │  ║
 * ║   │   │     KNOWLEDGE SYSTEM            │                             │  ║
 * ║   │   │  - Retrieval speed modulation   │                             │  ║
 * ║   │   │  - Encoding impairment          │                             │  ║
 * ║   │   │  - Association weakening        │                             │  ║
 * ║   │   │  - Learning rate reduction      │                             │  ║
 * ║   │   └─────────────────────────────────┘                             │  ║
 * ║   │                            ▲                                       │  ║
 * ║   │   ┌──────────────┐         │                                       │  ║
 * ║   │   │   IL-10      │         │                                       │  ║
 * ║   │   │ Anti-inflam  │  ───────┘                                       │  ║
 * ║   │   │   Restore    │     Recovery, Normal Speed                      │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  KNOWLEDGE → IMMUNE PATHWAYS                        │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │  HEALTH      │ ──→ Immune-Relevant Decisions                   │  ║
 * ║   │   │  KNOWLEDGE   │ ──→ Risk Assessment                             │  ║
 * ║   │   │              │ ──→ Protective Behaviors                        │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │  THREAT      │ ──→ Immune Priming                              │  ║
 * ║   │   │  LEARNING    │ ──→ Vigilance Increase                          │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe via mutex
 * - nimcp_malloc/nimcp_free memory management
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_KNOWLEDGE_IMMUNE_BRIDGE_H
#define NIMCP_KNOWLEDGE_IMMUNE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "cognitive/immune/nimcp_brain_immune.h"
#include "cognitive/knowledge/nimcp_knowledge.h"

/* Bio-async integration */
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Cytokine impact on retrieval latency (multipliers) */
#define CYTOKINE_IL1_RETRIEVAL_IMPACT      1.4f   /**< IL-1β → +40% latency */
#define CYTOKINE_IL6_RETRIEVAL_IMPACT      1.3f   /**< IL-6 → +30% latency */
#define CYTOKINE_TNF_RETRIEVAL_IMPACT      1.5f   /**< TNF-α → +50% latency */
#define CYTOKINE_IFN_GAMMA_RETRIEVAL_IMPACT 1.2f  /**< IFN-γ → +20% latency */
#define CYTOKINE_IL10_RETRIEVAL_BENEFIT    0.9f   /**< IL-10 → -10% latency (recovery) */

/* Inflammation impact on encoding (confidence reduction) */
#define INFLAMMATION_ENCODING_PENALTY_LOCAL    0.90f  /**< Local: -10% confidence */
#define INFLAMMATION_ENCODING_PENALTY_REGIONAL 0.75f  /**< Regional: -25% confidence */
#define INFLAMMATION_ENCODING_PENALTY_SYSTEMIC 0.50f  /**< Systemic: -50% confidence */
#define INFLAMMATION_ENCODING_PENALTY_STORM    0.20f  /**< Storm: -80% confidence */

/* Sickness behavior learning impact */
#define SICKNESS_LEARNING_THRESHOLD        0.5f   /**< Sickness level to impair learning */
#define SICKNESS_CURIOSITY_SUPPRESSION     0.7f   /**< Reduces curiosity-driven learning */

/* Chronic inflammation duration (seconds) */
#define CHRONIC_INFLAMMATION_THRESHOLD     (86400.0f * 7)  /**< 7 days = chronic */

/* Health knowledge relevance boost during illness */
#define HEALTH_KNOWLEDGE_BOOST_MULTIPLIER  1.5f   /**< 50% faster health fact retrieval when sick */

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Cytokine effects on knowledge retrieval
 *
 * How cytokines modulate semantic memory access speed
 */
typedef struct {
    /* Pro-inflammatory effects on retrieval latency */
    float il1_latency_multiplier;      /**< IL-1β-induced slowdown */
    float il6_latency_multiplier;      /**< IL-6-induced slowdown */
    float tnf_latency_multiplier;      /**< TNF-α-induced slowdown */
    float ifn_gamma_latency_multiplier; /**< IFN-γ-induced slowdown */

    /* Anti-inflammatory recovery */
    float il10_latency_benefit;        /**< IL-10 recovery speedup */

    /* Aggregate effects */
    float total_latency_multiplier;    /**< Combined retrieval slowdown [1.0 = baseline] */
    float retrieval_impairment;        /**< Overall impairment [0-1] */
    float encoding_impairment;         /**< New fact learning impairment [0-1] */
} cytokine_knowledge_effects_t;

/**
 * @brief Inflammation effects on knowledge processing
 *
 * How chronic inflammation affects semantic memory
 */
typedef struct {
    /* Inflammation state */
    brain_inflammation_level_t current_level;
    float inflammation_duration_sec;   /**< How long inflamed */
    bool is_chronic;                   /**< >= 7 days */

    /* Knowledge impacts */
    float retrieval_slowdown;          /**< Retrieval latency increase [0-1] */
    float encoding_penalty;            /**< New learning confidence penalty [0-1] */
    float association_weakening;       /**< Concept connection strength reduction [0-1] */
    float cognitive_decline;           /**< Progressive impairment [0-1] */

    /* Sickness behavior effects */
    float sickness_level;              /**< Sickness behavior intensity [0-1] */
    float curiosity_suppression;       /**< Reduced exploration [0-1] */
    float learning_motivation;         /**< Motivation to acquire new knowledge [0-1] */
} inflammation_knowledge_state_t;

/**
 * @brief Knowledge-based immune modulation
 *
 * How health knowledge affects immune decisions
 */
typedef struct {
    /* Health knowledge state */
    uint32_t health_concepts_known;    /**< Number of health/medical concepts */
    float health_knowledge_depth;      /**< Understanding depth [0-1] */
    float threat_awareness;            /**< Awareness of pathogens/risks [0-1] */

    /* Immune decision influence */
    bool enable_knowledge_priming;     /**< Knowledge-based immune vigilance */
    float immune_preparedness_boost;   /**< Preparedness from knowledge [0-1] */
    float risk_assessment_accuracy;    /**< Improved threat evaluation [0-1] */

    /* Behavioral immune system */
    float disgust_sensitivity;         /**< Disgust response to contaminants */
    float avoidance_behavior;          /**< Protective avoidance [0-1] */
} knowledge_immune_modulation_t;

/**
 * @brief Selective knowledge prioritization during illness
 *
 * Brain prioritizes illness-relevant knowledge when sick
 */
typedef struct {
    /* Prioritization state */
    bool is_sick;                      /**< Currently experiencing sickness behavior */
    float health_relevance_boost;      /**< Retrieval boost for health concepts */
    float non_health_suppression;      /**< Retrieval penalty for non-health concepts */

    /* Selective attention */
    knowledge_domain_t prioritized_domains[4]; /**< Domains getting attention boost */
    uint32_t num_prioritized_domains;
    float domain_boost_multipliers[4]; /**< Boost multipliers per domain */
} illness_knowledge_priority_t;

/**
 * @brief Complete knowledge-immune bridge state
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    /* System handles */
    brain_immune_system_t* immune_system;
    knowledge_system_t knowledge_system;

    /* Current state */
    cytokine_knowledge_effects_t cytokine_effects;
    inflammation_knowledge_state_t inflammation_state;
    knowledge_immune_modulation_t knowledge_modulation;
    illness_knowledge_priority_t illness_priority;

    /* Integration flags */
    bool enable_cytokine_retrieval_modulation;
    bool enable_inflammation_encoding_impairment;
    bool enable_knowledge_immune_priming;
    bool enable_illness_knowledge_priority;
    bool enable_sickness_learning_impairment;

    /* Performance tracking */
    float baseline_retrieval_latency_ms;  /**< Baseline retrieval time */
    float current_retrieval_latency_ms;   /**< Current retrieval time */
    uint32_t retrieval_count;             /**< Total retrievals */
    uint32_t encoding_count;              /**< Total encodings */
    uint32_t impaired_retrievals;         /**< Retrievals slowed by inflammation */
    uint32_t failed_encodings;            /**< Encodings failed due to inflammation */

    /* Statistics */
    uint64_t total_updates;
    uint32_t cytokine_modulations;
    uint32_t inflammation_impairments;
    uint32_t knowledge_priming_events;
    uint32_t illness_prioritizations;
    } knowledge_immune_bridge_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Feature enables */
    bool enable_cytokine_retrieval_modulation;
    bool enable_inflammation_encoding_impairment;
    bool enable_knowledge_immune_priming;
    bool enable_illness_knowledge_priority;
    bool enable_sickness_learning_impairment;

    /* Sensitivity tuning */
    float cytokine_sensitivity;        /**< Cytokine effect multiplier [0.5-2.0] */
    float inflammation_sensitivity;    /**< Inflammation effect multiplier [0.5-2.0] */
    float knowledge_priming_sensitivity; /**< Knowledge-based priming multiplier [0.5-2.0] */

    /* Thresholds */
    float sickness_learning_threshold; /**< Sickness level to impair learning [0.3-0.7] */
    float chronic_inflammation_days;   /**< Days for chronic inflammation [5-10] */

    /* Baseline performance */
    float baseline_retrieval_latency_ms; /**< Normal retrieval time [10-100ms] */
} knowledge_immune_config_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with biological defaults
 * HOW:  Return struct with evidence-based parameters
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int knowledge_immune_default_config(knowledge_immune_config_t* config);

/**
 * @brief Create knowledge-immune bridge
 *
 * WHAT: Initialize bidirectional knowledge-immune integration
 * WHY:  Enable realistic inflammation-based cognitive impairment
 * HOW:  Allocate structure, link subsystems
 *
 * @param config Configuration (NULL for defaults)
 * @param immune_system Brain immune system
 * @param knowledge_system Knowledge base system
 * @return New bridge or NULL on failure
 */
knowledge_immune_bridge_t* knowledge_immune_bridge_create(
    const knowledge_immune_config_t* config,
    brain_immune_system_t* immune_system,
    knowledge_system_t knowledge_system
);

/**
 * @brief Destroy knowledge-immune bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Free structure (doesn't destroy linked systems)
 *
 * @param bridge Bridge to destroy
 */
void knowledge_immune_bridge_destroy(knowledge_immune_bridge_t* bridge);

/* ============================================================================
 * Immune → Knowledge API
 * ============================================================================ */

/**
 * @brief Apply cytokine effects to knowledge retrieval
 *
 * WHAT: Modulate retrieval latency based on cytokine levels
 * WHY:  Pro-inflammatory cytokines slow semantic memory access
 * HOW:  Query immune cytokines, adjust retrieval speed multipliers
 *
 * @param bridge Knowledge-immune bridge
 * @return 0 on success
 */
int knowledge_immune_apply_cytokine_effects(knowledge_immune_bridge_t* bridge);

/**
 * @brief Apply inflammation effects to knowledge encoding
 *
 * WHAT: Impair new fact learning based on inflammation level
 * WHY:  Inflammation reduces synaptic plasticity and encoding
 * HOW:  Reduce confidence of newly learned facts based on inflammation
 *
 * @param bridge Knowledge-immune bridge
 * @return 0 on success
 */
int knowledge_immune_apply_inflammation_encoding(knowledge_immune_bridge_t* bridge);

/**
 * @brief Compute retrieval latency multiplier
 *
 * WHAT: Calculate how much slower retrieval is due to inflammation
 * WHY:  Cytokines increase retrieval time
 * HOW:  Combine cytokine multipliers, return aggregate slowdown
 *
 * @param bridge Knowledge-immune bridge
 * @return Latency multiplier [1.0 = baseline, >1.0 = slower]
 */
float knowledge_immune_get_retrieval_latency_multiplier(
    const knowledge_immune_bridge_t* bridge
);

/**
 * @brief Compute encoding confidence penalty
 *
 * WHAT: Calculate reduction in new fact confidence
 * WHY:  Inflammation impairs new learning
 * HOW:  Map inflammation level to confidence penalty [0-1]
 *
 * @param bridge Knowledge-immune bridge
 * @return Confidence multiplier [0-1, lower = more impairment]
 */
float knowledge_immune_get_encoding_penalty(
    const knowledge_immune_bridge_t* bridge
);

/**
 * @brief Apply sickness behavior to learning
 *
 * WHAT: Reduce learning motivation during sickness
 * WHY:  Cytokines induce fatigue and reduced cognitive engagement
 * HOW:  Check sickness level, suppress curiosity and learning rate
 *
 * @param bridge Knowledge-immune bridge
 * @return 0 on success
 */
int knowledge_immune_apply_sickness_learning_impairment(
    knowledge_immune_bridge_t* bridge
);

/* ============================================================================
 * Knowledge → Immune API
 * ============================================================================ */

/**
 * @brief Prime immune system from health knowledge
 *
 * WHAT: Increase immune vigilance based on health knowledge
 * WHY:  Awareness of threats enhances immune preparedness
 * HOW:  Query health domain knowledge, boost immune sensitivity
 *
 * @param bridge Knowledge-immune bridge
 * @return 0 on success
 */
int knowledge_immune_prime_from_health_knowledge(knowledge_immune_bridge_t* bridge);

/**
 * @brief Assess threat from knowledge
 *
 * WHAT: Use knowledge to evaluate immune threat severity
 * WHY:  Semantic understanding improves risk assessment
 * HOW:  Retrieve relevant health concepts, inform immune response
 *
 * @param bridge Knowledge-immune bridge
 * @param threat_description Textual threat description
 * @param assessed_severity Output: assessed severity [0-10]
 * @return 0 on success
 */
int knowledge_immune_assess_threat(
    knowledge_immune_bridge_t* bridge,
    const char* threat_description,
    float* assessed_severity
);

/**
 * @brief Trigger immune priming from threat learning
 *
 * WHAT: Activate immune when learning about pathogens/risks
 * WHY:  Reading about diseases increases immune vigilance
 * HOW:  Detect health threat concepts, trigger immune preparedness
 *
 * @param bridge Knowledge-immune bridge
 * @param learned_concept Recently learned concept
 * @return 0 on success
 */
int knowledge_immune_trigger_from_threat_learning(
    knowledge_immune_bridge_t* bridge,
    const char* learned_concept
);

/* ============================================================================
 * Illness-Based Prioritization API
 * ============================================================================ */

/**
 * @brief Prioritize health knowledge during illness
 *
 * WHAT: Boost retrieval speed for health/medical concepts when sick
 * WHY:  Brain prioritizes illness-relevant information during infection
 * HOW:  Check sickness behavior, boost health domain retrieval
 *
 * @param bridge Knowledge-immune bridge
 * @return 0 on success
 */
int knowledge_immune_prioritize_health_knowledge(knowledge_immune_bridge_t* bridge);

/**
 * @brief Get domain-specific retrieval multiplier
 *
 * WHAT: Get retrieval speed multiplier for specific domain
 * WHY:  Different domains prioritized based on illness state
 * HOW:  Check illness priority, return domain-specific multiplier
 *
 * @param bridge Knowledge-immune bridge
 * @param domain Knowledge domain
 * @return Multiplier [<1.0 = faster, 1.0 = baseline, >1.0 = slower]
 */
float knowledge_immune_get_domain_retrieval_multiplier(
    const knowledge_immune_bridge_t* bridge,
    knowledge_domain_t domain
);

/* ============================================================================
 * Bidirectional Update API
 * ============================================================================ */

/**
 * @brief Update knowledge-immune bridge (both directions)
 *
 * WHAT: Process all knowledge-immune interactions
 * WHY:  Advance coupled state machine
 * HOW:  Apply cytokine effects, encode impairment, prime immune from knowledge
 *
 * @param bridge Knowledge-immune bridge
 * @param delta_ms Time since last update
 * @return 0 on success
 */
int knowledge_immune_bridge_update(
    knowledge_immune_bridge_t* bridge,
    uint64_t delta_ms
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current cytokine knowledge effects
 *
 * @param bridge Knowledge-immune bridge
 * @param effects Output effects structure
 * @return 0 on success
 */
int knowledge_immune_get_cytokine_effects(
    const knowledge_immune_bridge_t* bridge,
    cytokine_knowledge_effects_t* effects
);

/**
 * @brief Get current inflammation knowledge state
 *
 * @param bridge Knowledge-immune bridge
 * @param state Output state structure
 * @return 0 on success
 */
int knowledge_immune_get_inflammation_state(
    const knowledge_immune_bridge_t* bridge,
    inflammation_knowledge_state_t* state
);

/**
 * @brief Check if experiencing cognitive impairment
 *
 * WHAT: Determine if inflammation causing significant impairment
 * WHY:  Cognitive impairment is clinically relevant state
 * HOW:  Check retrieval slowdown and encoding penalty thresholds
 *
 * @param bridge Knowledge-immune bridge
 * @return true if experiencing significant cognitive impairment
 */
bool knowledge_immune_is_cognitively_impaired(
    const knowledge_immune_bridge_t* bridge
);

/**
 * @brief Get retrieval latency increase percentage
 *
 * @param bridge Knowledge-immune bridge
 * @return Percentage increase in retrieval latency (0-100+)
 */
float knowledge_immune_get_retrieval_latency_increase_pct(
    const knowledge_immune_bridge_t* bridge
);

/**
 * @brief Get encoding success rate
 *
 * @param bridge Knowledge-immune bridge
 * @return Encoding success rate [0-1]
 */
float knowledge_immune_get_encoding_success_rate(
    const knowledge_immune_bridge_t* bridge
);


/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

/**
 * @brief Connect bridge to bio-async router
 *
 * WHAT: Register bridge as bio-async module
 * WHY:  Enable inter-module messaging for distributed immune signals
 * HOW:  Register with bio_router using BIO_MODULE_IMMUNE_KNOWLEDGE
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int knowledge_immune_connect_bio_async(knowledge_immune_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * WHAT: Unregister bridge from bio-async
 * WHY:  Clean shutdown of messaging
 * HOW:  Unregister from bio_router
 *
 * @param bridge Bridge instance
 * @return 0 on success
 */
int knowledge_immune_disconnect_bio_async(knowledge_immune_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Bridge instance
 * @return true if connected
 */
bool knowledge_immune_is_bio_async_connected(const knowledge_immune_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_KNOWLEDGE_IMMUNE_BRIDGE_H */
