/**
 * @file nimcp_complement_system.h
 * @brief Complement System - Innate Immune Amplification and Direct Elimination
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Complement system implementing opsonization, membrane attack complex (MAC),
 *       and amplification cascades for rapid threat response.
 * WHY:  Models biological complement system's role in innate immunity - rapid
 *       threat tagging, amplification, and direct elimination without prior exposure.
 * HOW:  Three activation pathways (classical, alternative, lectin) converge on
 *       C3 convertase, leading to C5 convertase and MAC formation. Opsonization
 *       tags threats for enhanced phagocytosis. Anaphylatoxins drive inflammation.
 *
 * BIOLOGICAL MODEL:
 * ```
 * BIOLOGICAL COMPONENT          NIMCP IMPLEMENTATION
 * ─────────────────────────────────────────────────────────────────
 * Classical pathway           → Antibody-antigen complex triggers
 * Alternative pathway         → Spontaneous C3 hydrolysis, amplification loop
 * Lectin pathway              → Pattern recognition (mannose-binding lectin)
 * C3 convertase               → C3b production, opsonization cascade
 * C5 convertase               → Terminal pathway initiation
 * C3b opsonization            → Tag threats for enhanced recognition
 * C3a/C5a anaphylatoxins      → Inflammation signaling via cytokines
 * MAC (C5b-9)                 → Direct threat elimination (pore formation)
 * Amplification loop          → Exponential C3b generation
 * Regulatory proteins         → Prevent excessive activation (DAF, CD59)
 * ```
 *
 * ACTIVATION PATHWAYS:
 * ```
 * CLASSICAL:         ALTERNATIVE:        LECTIN:
 *   Antibody-Ag  →     Spontaneous C3    Mannose pattern
 *      ↓                hydrolysis           ↓
 *   C1q-C1r-C1s          ↓               MBL-MASP
 *      ↓                C3(H2O)             ↓
 *   C4b2a               ↓                C4b2a
 *   (C3 convertase)  C3bBb             (C3 convertase)
 *      ↓           (C3 convertase)         ↓
 *      └──────────────┬──────────────────┘
 *                     ↓
 *                 C3b (opsonization)
 *                     ↓
 *              C3bBb3b / C4b2a3b
 *              (C5 convertase)
 *                     ↓
 *                  C5b-9 (MAC)
 *                     ↓
 *              Threat elimination
 * ```
 *
 * AMPLIFICATION CASCADE:
 * ```
 *  Initial C3b  →  [Amplification Loop]  →  Exponential C3b
 *     ↓               C3bBb convertase         ↓
 *  1 molecule      generates more C3b      1000s of C3b
 *     ↓               feedback loop            ↓
 *  Local effect                           Systemic effect
 * ```
 *
 * INTEGRATION WITH BRAIN IMMUNE:
 * - Classical pathway: Triggered by brain antibodies (IgG/IgM)
 * - Alternative pathway: Spontaneous activation on unrecognized surfaces
 * - Lectin pathway: Pattern recognition (similar to BBB threat signatures)
 * - Opsonization: Enhances B cell and T cell antigen recognition
 * - MAC: Direct threat elimination (analogous to BFT node termination)
 * - Anaphylatoxins: Release cytokines (IL-1, IL-6) to drive inflammation
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

#ifndef NIMCP_COMPLEMENT_SYSTEM_H
#define NIMCP_COMPLEMENT_SYSTEM_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Forward declaration */
typedef struct brain_immune_system brain_immune_system_t;

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define COMPLEMENT_MAX_ACTIVE_C3B       512   /**< Max active C3b molecules */
#define COMPLEMENT_MAX_ACTIVE_C5B       256   /**< Max active C5b molecules */
#define COMPLEMENT_MAX_MACS             128   /**< Max active MACs */
#define COMPLEMENT_MAX_ANAPHYLATOXINS   256   /**< Max anaphylatoxin signals */
#define COMPLEMENT_AMPLIFICATION_FACTOR 10.0f /**< C3b amplification factor */
#define COMPLEMENT_MODULE_NAME          "complement_system"

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Complement activation pathways
 *
 * BIOLOGICAL BASIS:
 * - Classical: Antibody-mediated, requires prior immune response
 * - Alternative: Spontaneous, innate immunity, no prior exposure needed
 * - Lectin: Pattern recognition, innate immunity via MBL
 */
typedef enum {
    COMPLEMENT_PATHWAY_CLASSICAL = 0,  /**< Antibody-antigen triggered */
    COMPLEMENT_PATHWAY_ALTERNATIVE,    /**< Spontaneous C3 hydrolysis */
    COMPLEMENT_PATHWAY_LECTIN,         /**< Pattern recognition (MBL) */
    COMPLEMENT_PATHWAY_COUNT
} complement_pathway_t;

/**
 * @brief Complement component types
 * @note Prefixed with COMPL_ to avoid collision with microglia complement enums
 */
typedef enum {
    COMPL_C1 = 0,          /**< Classical pathway initiator */
    COMPL_C2,              /**< Classical/lectin component */
    COMPL_C3,              /**< Central component (opsonization) */
    COMPL_C4,              /**< Classical/lectin component */
    COMPL_C5,              /**< Terminal pathway initiator */
    COMPL_C6,              /**< MAC component */
    COMPL_C7,              /**< MAC component */
    COMPL_C8,              /**< MAC component */
    COMPL_C9,              /**< MAC polymerization */
    COMPL_FACTOR_B,        /**< Alternative pathway */
    COMPL_FACTOR_D,        /**< Alternative pathway */
    COMPL_MBL,             /**< Lectin pathway initiator */
    COMPL_COMPONENT_COUNT
} complement_component_t;

/**
 * @brief Anaphylatoxin types (inflammatory mediators)
 */
typedef enum {
    ANAPHYLATOXIN_C3A = 0, /**< Moderate inflammation, mast cell activation */
    ANAPHYLATOXIN_C4A,     /**< Weak inflammation */
    ANAPHYLATOXIN_C5A,     /**< Strong inflammation, chemotaxis */
    ANAPHYLATOXIN_COUNT
} anaphylatoxin_type_t;

/**
 * @brief MAC formation state
 */
typedef enum {
    MAC_STATE_INACTIVE = 0,  /**< Not formed */
    MAC_STATE_FORMING,       /**< C5b-7 complex forming */
    MAC_STATE_ASSEMBLING,    /**< C8 inserted, C9 polymerizing */
    MAC_STATE_COMPLETE,      /**< Pore formed, membrane disrupted */
    MAC_STATE_COUNT
} mac_state_t;

/* ============================================================================
 * Core Structures
 * ============================================================================ */

/**
 * @brief C3b molecule - central opsonization component
 *
 * C3b coats threat surfaces, marking them for destruction.
 * Biological half-life: 90 seconds (in vivo), we simulate decay.
 */
typedef struct {
    uint32_t id;                   /**< Unique C3b ID */
    uint32_t target_antigen_id;    /**< Target antigen being opsonized */
    complement_pathway_t pathway;  /**< Pathway that generated this C3b */

    uint64_t generation_time;      /**< When generated */
    float concentration;           /**< Local concentration (0-1) */
    bool bound_to_target;          /**< Bound to threat surface */

    uint32_t convertase_count;     /**< Convertases formed by this C3b */
    bool part_of_amplification;    /**< Part of amplification loop */
} complement_c3b_t;

/**
 * @brief C5b molecule - terminal pathway initiator
 *
 * C5b initiates MAC formation by recruiting C6-C9.
 */
typedef struct {
    uint32_t id;                   /**< Unique C5b ID */
    uint32_t target_antigen_id;    /**< Target antigen */
    uint32_t parent_c3b_id;        /**< C3b that led to this C5b */

    uint64_t generation_time;      /**< When generated */
    uint32_t mac_id;               /**< MAC formed by this C5b (0 if none) */
    bool recruited_c6;             /**< C6 recruited */
} complement_c5b_t;

/**
 * @brief Membrane Attack Complex (MAC) - C5b-9
 *
 * MAC forms pore in threat membrane, causing direct lysis.
 * Biological analog: Pore formation disrupts membrane integrity.
 */
typedef struct {
    uint32_t id;                   /**< Unique MAC ID */
    uint32_t target_antigen_id;    /**< Target being eliminated */
    mac_state_t state;             /**< Assembly state */

    uint32_t initiating_c5b_id;    /**< C5b that initiated */
    uint8_t c9_polymers;           /**< Number of C9 molecules (1-18) */

    uint64_t formation_time;       /**< When formation started */
    uint64_t completion_time;      /**< When pore completed */
    float lytic_effectiveness;     /**< Elimination effectiveness (0-1) */
    bool target_eliminated;        /**< Target destroyed */
} complement_mac_t;

/**
 * @brief Anaphylatoxin signal - inflammatory mediator
 *
 * C3a, C4a, C5a release triggers inflammation and recruits immune cells.
 */
typedef struct {
    uint32_t id;                   /**< Unique anaphylatoxin ID */
    anaphylatoxin_type_t type;     /**< Anaphylatoxin type */
    uint32_t source_antigen_id;    /**< Antigen that triggered release */

    float concentration;           /**< Signal strength (0-1) */
    uint64_t release_time;         /**< When released */
    uint32_t cytokine_id;          /**< Mapped brain cytokine ID */
    bool delivered;                /**< Signal delivered to immune system */
} complement_anaphylatoxin_t;

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Complement system configuration
 */
typedef struct {
    /* Pathway enables */
    bool enable_classical_pathway;    /**< Enable antibody-triggered pathway */
    bool enable_alternative_pathway;  /**< Enable spontaneous activation */
    bool enable_lectin_pathway;       /**< Enable pattern recognition */

    /* Activation thresholds */
    float classical_threshold;        /**< Antibody concentration threshold */
    float alternative_threshold;      /**< Spontaneous activation threshold */
    float lectin_threshold;           /**< Pattern match threshold */

    /* Amplification parameters */
    float amplification_factor;       /**< C3b amplification multiplier */
    float max_amplification;          /**< Max amplification level */
    bool enable_amplification_loop;   /**< Enable positive feedback */

    /* MAC parameters */
    uint32_t min_c9_for_lysis;        /**< Min C9 polymers for effective pore */
    float mac_formation_delay_ms;     /**< Delay for MAC assembly */
    float mac_effectiveness;          /**< Base MAC lytic effectiveness */

    /* Opsonization parameters */
    float opsonization_enhancement;   /**< Recognition enhancement factor */
    float c3b_decay_rate;             /**< C3b decay per second */

    /* Anaphylatoxin parameters */
    bool enable_anaphylatoxins;       /**< Enable inflammatory signals */
    float c3a_potency;                /**< C3a inflammatory strength */
    float c5a_potency;                /**< C5a inflammatory strength */

    /* Regulation parameters */
    bool enable_self_regulation;      /**< Prevent excessive activation */
    float regulation_threshold;       /**< Max activity before inhibition */

    /* Integration */
    bool enable_logging;              /**< Enable logging */
} complement_config_t;

/**
 * @brief Complement system statistics
 */
typedef struct {
    /* Pathway activations */
    uint64_t classical_activations;
    uint64_t alternative_activations;
    uint64_t lectin_activations;

    /* Component generation */
    uint64_t c3b_generated;
    uint64_t c5b_generated;
    uint64_t macs_formed;
    uint64_t macs_completed;

    /* Effects */
    uint64_t targets_opsonized;
    uint64_t targets_eliminated_by_mac;
    uint64_t anaphylatoxins_released;

    /* Amplification */
    float current_amplification_level;
    float max_amplification_reached;
    uint64_t amplification_cycles;

    /* Regulation */
    uint64_t regulation_activations;

    /* Activity levels */
    uint32_t active_c3b;
    uint32_t active_c5b;
    uint32_t active_macs;
} complement_stats_t;

/* ============================================================================
 * Main System Structure
 * ============================================================================ */

/**
 * @brief Complement system state
 */
typedef struct {
    complement_config_t config;            /**< Configuration */

    /* Integration */
    brain_immune_system_t* immune_system;  /**< Brain immune system */

    /* Component pools */
    complement_c3b_t* c3b_pool;
    size_t c3b_count;
    size_t c3b_capacity;
    uint32_t next_c3b_id;

    complement_c5b_t* c5b_pool;
    size_t c5b_count;
    size_t c5b_capacity;
    uint32_t next_c5b_id;

    complement_mac_t* mac_pool;
    size_t mac_count;
    size_t mac_capacity;
    uint32_t next_mac_id;

    complement_anaphylatoxin_t* anaphylatoxin_pool;
    size_t anaphylatoxin_count;
    size_t anaphylatoxin_capacity;
    uint32_t next_anaphylatoxin_id;

    /* Statistics */
    complement_stats_t stats;

    /* Thread safety */
    void* mutex;                           /**< Platform mutex */

    /* State */
    bool running;                          /**< System is active */
    uint64_t start_time;                   /**< System start time */
    float current_c3_level;                /**< Current C3 concentration */
    float current_c5_level;                /**< Current C5 concentration */
} complement_system_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with balanced parameters
 * HOW:  Return struct with biologically-inspired defaults
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int complement_default_config(complement_config_t* config);

/**
 * @brief Create complement system
 *
 * WHAT: Initialize complement system with configuration
 * WHY:  Set up innate immune amplification infrastructure
 * HOW:  Allocate pools, connect to brain immune system
 *
 * @param config Configuration (NULL for defaults)
 * @param immune_system Brain immune system to integrate with
 * @return New complement system or NULL on failure
 */
complement_system_t* complement_create(
    const complement_config_t* config,
    brain_immune_system_t* immune_system
);

/**
 * @brief Destroy complement system
 *
 * WHAT: Clean up complement system resources
 * WHY:  Proper resource deallocation
 * HOW:  Free pools, disconnect from immune system
 *
 * @param system System to destroy
 */
void complement_destroy(complement_system_t* system);

/* ============================================================================
 * Activation API
 * ============================================================================ */

/**
 * @brief Activate complement cascade via specified pathway
 *
 * WHAT: Initiate complement activation for target antigen
 * WHY:  Begin opsonization and MAC formation process
 * HOW:  Trigger pathway-specific C3 convertase formation
 *
 * @param system Complement system
 * @param pathway Activation pathway to use
 * @param target_id Target antigen ID
 * @return 0 on success, -1 on error
 */
int complement_activate(
    complement_system_t* system,
    complement_pathway_t pathway,
    uint32_t target_id
);

/**
 * @brief Activate classical pathway via antibody-antigen complex
 *
 * WHAT: Classical pathway activation (antibody-dependent)
 * WHY:  Amplify adaptive immune response
 * HOW:  C1q binds antibody, activates C1r/C1s, forms C3 convertase
 *
 * @param system Complement system
 * @param antibody_id Antibody bound to antigen
 * @param target_id Target antigen ID
 * @return 0 on success, -1 on error
 */
int complement_activate_classical(
    complement_system_t* system,
    uint32_t antibody_id,
    uint32_t target_id
);

/**
 * @brief Activate alternative pathway via spontaneous hydrolysis
 *
 * WHAT: Alternative pathway activation (antibody-independent)
 * WHY:  Innate immunity, rapid response to novel threats
 * HOW:  Spontaneous C3 hydrolysis, factor B/D form C3 convertase
 *
 * @param system Complement system
 * @param target_id Target antigen ID
 * @return 0 on success, -1 on error
 */
int complement_activate_alternative(
    complement_system_t* system,
    uint32_t target_id
);

/**
 * @brief Activate lectin pathway via pattern recognition
 *
 * WHAT: Lectin pathway activation (mannose-binding lectin)
 * WHY:  Innate immunity, pathogen-associated molecular patterns
 * HOW:  MBL binds mannose, activates MASP, forms C3 convertase
 *
 * @param system Complement system
 * @param target_id Target antigen ID
 * @param pattern Pattern signature
 * @param pattern_len Pattern length
 * @return 0 on success, -1 on error
 */
int complement_activate_lectin(
    complement_system_t* system,
    uint32_t target_id,
    const uint8_t* pattern,
    size_t pattern_len
);

/* ============================================================================
 * Opsonization API
 * ============================================================================ */

/**
 * @brief Opsonize target with C3b coating
 *
 * WHAT: Tag target with C3b for enhanced recognition
 * WHY:  Mark threats for phagocytosis and amplification
 * HOW:  Generate C3b molecules, bind to target surface
 *
 * @param system Complement system
 * @param target_id Target antigen ID
 * @return Number of C3b molecules deposited, or -1 on error
 */
int complement_opsonize(
    complement_system_t* system,
    uint32_t target_id
);

/**
 * @brief Get C3b opsonization level for target
 *
 * WHAT: Measure C3b coating density on target
 * WHY:  Determine opsonization effectiveness
 * HOW:  Count C3b molecules bound to target
 *
 * @param system Complement system
 * @param target_id Target antigen ID
 * @return C3b level (0-1 normalized), or -1 on error
 */
float complement_get_c3b_level(
    complement_system_t* system,
    uint32_t target_id
);

/* ============================================================================
 * MAC Formation API
 * ============================================================================ */

/**
 * @brief Form membrane attack complex (MAC) on target
 *
 * WHAT: Assemble C5b-9 pore complex for direct elimination
 * WHY:  Direct lysis without phagocytosis
 * HOW:  C5b nucleates C6-C9 assembly, forming transmembrane pore
 *
 * @param system Complement system
 * @param target_id Target antigen ID
 * @return MAC ID on success, 0 on error
 */
uint32_t complement_form_mac(
    complement_system_t* system,
    uint32_t target_id
);

/**
 * @brief Check if MAC formation is complete
 *
 * WHAT: Verify MAC pore assembly status
 * WHY:  Determine if target elimination is effective
 * HOW:  Check C9 polymerization and membrane insertion
 *
 * @param system Complement system
 * @param mac_id MAC ID to check
 * @return true if complete and lytic, false otherwise
 */
bool complement_is_mac_complete(
    complement_system_t* system,
    uint32_t mac_id
);

/* ============================================================================
 * Amplification API
 * ============================================================================ */

/**
 * @brief Trigger amplification cascade
 *
 * WHAT: Activate positive feedback loop for C3b generation
 * WHY:  Exponential response to threats (1 → 1000s of C3b)
 * HOW:  C3b forms additional C3 convertases, each generates more C3b
 *
 * @param system Complement system
 * @param factor Amplification intensity factor (typically 1.0)
 * @return Number of C3b generated, or -1 on error
 */
int complement_cascade_amplify(
    complement_system_t* system,
    float factor
);

/**
 * @brief Get current amplification level
 *
 * WHAT: Measure current cascade amplification
 * WHY:  Monitor feedback loop intensity
 * HOW:  Calculate ratio of amplified C3b to initial C3b
 *
 * @param system Complement system
 * @return Amplification level (1.0 = no amplification), or -1 on error
 */
float complement_get_amplification_level(
    complement_system_t* system
);

/* ============================================================================
 * Anaphylatoxin API
 * ============================================================================ */

/**
 * @brief Release anaphylatoxin inflammatory signal
 *
 * WHAT: Generate inflammatory mediator (C3a/C5a)
 * WHY:  Recruit immune cells, trigger inflammation
 * HOW:  Cleave C3/C5, release small fragment, map to cytokine
 *
 * @param system Complement system
 * @param type Anaphylatoxin type (C3a/C5a)
 * @param target_id Source antigen ID
 * @return Anaphylatoxin ID on success, 0 on error
 */
uint32_t complement_release_anaphylatoxin(
    complement_system_t* system,
    anaphylatoxin_type_t type,
    uint32_t target_id
);

/* ============================================================================
 * Update and Query API
 * ============================================================================ */

/**
 * @brief Update complement system state
 *
 * WHAT: Process complement activity, decay, MAC formation
 * WHY:  Advance complement dynamics over time
 * HOW:  Decay C3b, progress MAC assembly, deliver anaphylatoxins
 *
 * @param system Complement system
 * @param delta_ms Time since last update
 * @return 0 on success, -1 on error
 */
int complement_update(
    complement_system_t* system,
    uint64_t delta_ms
);

/**
 * @brief Get complement system statistics
 *
 * @param system Complement system
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int complement_get_stats(
    complement_system_t* system,
    complement_stats_t* stats
);

/**
 * @brief Check if target is opsonized
 *
 * @param system Complement system
 * @param target_id Target antigen ID
 * @return true if opsonized with C3b
 */
bool complement_is_opsonized(
    complement_system_t* system,
    uint32_t target_id
);

/**
 * @brief Get number of active MACs on target
 *
 * @param system Complement system
 * @param target_id Target antigen ID
 * @return Number of active MACs
 */
uint32_t complement_get_mac_count(
    complement_system_t* system,
    uint32_t target_id
);

/* ============================================================================
 * String Conversion Utilities
 * ============================================================================ */

const char* complement_pathway_to_string(complement_pathway_t pathway);
const char* complement_anaphylatoxin_to_string(anaphylatoxin_type_t type);
const char* complement_mac_state_to_string(mac_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_COMPLEMENT_SYSTEM_H */
