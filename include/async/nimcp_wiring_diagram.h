/**
 * @file nimcp_wiring_diagram.h
 * @brief KG-Based Runtime Module Wiring Diagram System
 * @version 1.0.0
 * @date 2025-12-31
 *
 * WHAT: Dynamic module wiring system using Knowledge Graph for runtime assembly
 * WHY:  Enable self-assembling module topology without hardcoded dependencies
 * HOW:  Load modular JSONL wiring diagrams, merge by profile, sync to brain_kg
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * SELF-ORGANIZING NEURAL NETWORKS:
 * ---------------------------------
 * The brain's wiring is not hardcoded but emerges through:
 * - Genetic blueprints (baseline connectivity)
 * - Activity-dependent refinement (use it or lose it)
 * - Neuromodulator-gated plasticity (context-sensitive wiring)
 * - Homeostatic regulation (stability through adaptation)
 *
 * This system provides similar flexibility:
 * - JSONL blueprints define baseline module connectivity
 * - Hardware profiles adapt wiring to available resources
 * - Custom diagrams allow context-specific overrides
 * - Auto-persistence maintains learned configurations
 *
 * MODULAR SUB-DIAGRAMS:
 * ---------------------
 * Like cortical columns that specialize yet integrate, wiring is organized:
 * - Subsystems: ethics, perception, cognition, memory, emotion, immune, plasticity
 * - Platforms: FULL, MEDIUM, CONSTRAINED, MINIMAL (like phylogenetic scaling)
 * - Hardware: GPU variants, neuromorphic cores (like species-specific adaptations)
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════════╗
 * ║                         WIRING DIAGRAM SYSTEM                                  ║
 * ╠═══════════════════════════════════════════════════════════════════════════════╣
 * ║                                                                                ║
 * ║  ┌─────────────────┐   ┌─────────────────┐   ┌─────────────────┐              ║
 * ║  │  master.jsonl   │   │ subsystems/X.jsonl│   │ platforms/X.jsonl│             ║
 * ║  │  (base wiring)  │   │  (per component)  │   │  (per tier)      │             ║
 * ║  └────────┬────────┘   └────────┬─────────┘   └────────┬─────────┘             ║
 * ║           │                     │                       │                       ║
 * ║           └─────────────────────┼───────────────────────┘                       ║
 * ║                                 ▼                                               ║
 * ║  ┌─────────────────────────────────────────────────────────────────────────┐   ║
 * ║  │                        WIRING COMPOSITOR                                  │   ║
 * ║  │  - Merges applicable diagrams based on hardware profile                  │   ║
 * ║  │  - Later diagrams override earlier (custom > hardware > platform > base) │   ║
 * ║  │  - Validates merged result for consistency                               │   ║
 * ║  └────────────────────────────────────┬────────────────────────────────────┘   ║
 * ║                                       ▼                                         ║
 * ║  ┌─────────────────────────────────────────────────────────────────────────┐   ║
 * ║  │                    RUNTIME WIRING CACHE                                   │   ║
 * ║  │  - Per-module config: depends_on, sends_to, handles_messages             │   ║
 * ║  │  - Hardware requirements: GPU type, neuromorphic, memory                 │   ║
 * ║  │  - Tier constraints: min platform tier for module                        │   ║
 * ║  └────────────────────────────────────┬────────────────────────────────────┘   ║
 * ║                                       ▼                                         ║
 * ║  ┌──────────────────┐         ┌──────────────────┐         ┌───────────────┐   ║
 * ║  │  brain_kg sync   │ ──────► │ Orchestrator     │ ──────► │ Module Init   │   ║
 * ║  │  (HANDLES_MESSAGE│         │ (invoke handlers)│         │ (auto-wire)   │   ║
 * ║  │   edges)         │         │                  │         │               │   ║
 * ║  └──────────────────┘         └──────────────────┘         └───────────────┘   ║
 * ╚═══════════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * LOADING PRIORITY (later overrides earlier):
 * 1. master.jsonl           - Base module wiring
 * 2. subsystems/X.jsonl     - Subsystem-specific (ethics, perception, etc.)
 * 3. platforms/<tier>.jsonl - Platform tier specific
 * 4. hardware/<hw>.jsonl    - Hardware-specific (cuda, rocm, loihi, etc.)
 * 5. custom/X.jsonl         - User overrides (highest priority)
 *
 * JSONL FORMAT:
 * - Entities: {"type":"entity","name":"Module_Name","entityType":"CognitiveModule",...}
 * - Relations: {"type":"relation","from":"A","to":"B","relationType":"SENDS_TO",...}
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe via mutex
 * - nimcp_malloc/nimcp_free memory management
 * - Auto-persist on changes (default behavior)
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_WIRING_DIAGRAM_H
#define NIMCP_WIRING_DIAGRAM_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Dependencies */
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"
#include "utils/platform/nimcp_platform_tier.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define WIRING_MAX_MODULES              512     /**< Max modules in diagram */
#define WIRING_MAX_DEPENDENCIES         64      /**< Max dependencies per module */
#define WIRING_MAX_HANDLERS             128     /**< Max message handlers per module */
#define WIRING_MAX_CUSTOM_DIAGRAMS      32      /**< Max custom diagram files */
#define WIRING_MAX_PATH_LENGTH          256     /**< Max path string length */
#define WIRING_MODULE_NAME              "wiring_diagram"

/** Default wiring directory relative to project root */
#define WIRING_DEFAULT_PATH             ".aim/wiring"

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct wiring_diagram wiring_diagram_t;
typedef struct brain_kg brain_kg_t;  /* Forward declare from nimcp_brain_kg.h */

/* ============================================================================
 * Hardware Capability Flags
 * ============================================================================ */

/**
 * @brief Hardware capability flags for GPU and accelerator detection
 *
 * WHAT: Bitmask for available compute accelerators
 * WHY:  Enable hardware-specific wiring configurations
 * HOW:  Detected at runtime, used to select hardware/X.jsonl diagrams
 */
typedef enum {
    WIRING_HW_NONE           = 0,           /**< No accelerators */

    /* GPU Compute */
    WIRING_HW_CUDA           = (1 << 0),    /**< NVIDIA CUDA */
    WIRING_HW_ROCM           = (1 << 1),    /**< AMD ROCm/HIP */
    WIRING_HW_ONEAPI         = (1 << 2),    /**< Intel oneAPI/SYCL */
    WIRING_HW_METAL          = (1 << 3),    /**< Apple Metal */
    WIRING_HW_OPENCL         = (1 << 4),    /**< Generic OpenCL */

    /* Neuromorphic */
    WIRING_HW_LOIHI          = (1 << 5),    /**< Intel Loihi neuromorphic */
    WIRING_HW_SPINNAKER      = (1 << 6),    /**< SpiNNaker neuromorphic */
    WIRING_HW_AKIDA          = (1 << 7),    /**< BrainChip Akida neuromorphic */
    WIRING_HW_BRAINSCALES    = (1 << 8),    /**< BrainScaleS neuromorphic */

    /* Specialized */
    WIRING_HW_TPU            = (1 << 9),    /**< Google TPU */
    WIRING_HW_NPU            = (1 << 10),   /**< Generic Neural Processing Unit */

    /* Combined flags */
    WIRING_HW_ANY_GPU        = (WIRING_HW_CUDA | WIRING_HW_ROCM |
                                WIRING_HW_ONEAPI | WIRING_HW_METAL |
                                WIRING_HW_OPENCL),
    WIRING_HW_ANY_NEUROMORPHIC = (WIRING_HW_LOIHI | WIRING_HW_SPINNAKER |
                                   WIRING_HW_AKIDA | WIRING_HW_BRAINSCALES)
} wiring_hardware_flags_t;

/* ============================================================================
 * Subsystem Identifiers
 * ============================================================================ */

/**
 * @brief Subsystem identifier for modular wiring
 *
 * WHAT: Classification of modules by functional subsystem
 * WHY:  Enable subsystem-specific wiring diagrams
 * HOW:  Each module belongs to one subsystem, loaded from subsystems/X.jsonl
 */
typedef enum {
    WIRING_SUBSYSTEM_CORE = 0,       /**< Core infrastructure (brain, synapse) */
    WIRING_SUBSYSTEM_ETHICS,         /**< Ethics and safety modules */
    WIRING_SUBSYSTEM_PERCEPTION,     /**< Perception (visual, audio, etc.) */
    WIRING_SUBSYSTEM_COGNITION,      /**< Higher cognition (reasoning, etc.) */
    WIRING_SUBSYSTEM_MEMORY,         /**< Memory systems (working, episodic) */
    WIRING_SUBSYSTEM_EMOTION,        /**< Emotional processing */
    WIRING_SUBSYSTEM_IMMUNE,         /**< Brain immune system */
    WIRING_SUBSYSTEM_PLASTICITY,     /**< Learning and plasticity */
    WIRING_SUBSYSTEM_RECURSIVE,      /**< Recursive cognition */
    WIRING_SUBSYSTEM_SOCIAL,         /**< Social cognition (ToM, empathy) */
    WIRING_SUBSYSTEM_COUNT           /**< Number of subsystems */
} wiring_subsystem_t;

/* ============================================================================
 * Relation Types
 * ============================================================================ */

/**
 * @brief Wiring relation types for module connectivity
 *
 * WHAT: Types of relationships between modules
 * WHY:  Define different kinds of connections (dependency, messaging, etc.)
 * HOW:  Stored as edges in JSONL, synced to brain_kg
 */
typedef enum {
    WIRING_RELATION_DEPENDS_ON = 0,      /**< Module A depends on B (startup order) */
    WIRING_RELATION_SENDS_TO,            /**< Module A sends messages to B */
    WIRING_RELATION_RECEIVES_FROM,       /**< Module A receives from B */
    WIRING_RELATION_HANDLES_MESSAGE,     /**< Module handles specific message type */
    WIRING_RELATION_BELONGS_TO,          /**< Module belongs to subsystem */
    WIRING_RELATION_REQUIRES_HW,         /**< Module requires specific hardware */
    WIRING_RELATION_AVAILABLE_ON_TIER,   /**< Module available on platform tier */
    WIRING_RELATION_COUNT
} wiring_relation_type_t;

/* ============================================================================
 * Hardware Profile
 * ============================================================================ */

/**
 * @brief Runtime hardware profile for wiring selection
 *
 * WHAT: Complete hardware profile for a runtime environment
 * WHY:  Select appropriate wiring diagrams based on available resources
 * HOW:  Detected at startup, used by compositor to merge diagrams
 */
typedef struct {
    platform_tier_t tier;                /**< Platform tier (FULL/MEDIUM/etc.) */
    wiring_hardware_flags_t hw_flags;    /**< Available hardware accelerators */
    size_t memory_mb;                    /**< Available memory in MB */
    uint32_t cpu_cores;                  /**< Available CPU cores */
    uint32_t gpu_compute_units;          /**< GPU compute units (if available) */
    uint32_t neuromorphic_cores;         /**< Neuromorphic cores (if available) */
} wiring_hardware_profile_t;

/* ============================================================================
 * Module Configuration
 * ============================================================================ */

/**
 * @brief Per-module wiring configuration
 *
 * WHAT: Complete wiring info for a single module
 * WHY:  Enable modules to discover their connections at runtime
 * HOW:  Populated from merged JSONL diagrams, queried by modules
 */
typedef struct {
    /* Identity */
    char module_name[64];                /**< Module name (matches KG entity) */
    bio_module_id_t module_id;           /**< Bio-async module ID */
    wiring_subsystem_t subsystem;        /**< Subsystem membership */

    /* Dependencies (startup ordering) */
    bio_module_id_t* depends_on;         /**< Modules this depends on */
    uint32_t depends_on_count;           /**< Number of dependencies */
    uint32_t depends_on_capacity;        /**< Array capacity */

    /* Message routing */
    bio_module_id_t* sends_to;           /**< Modules to send messages to */
    uint32_t sends_to_count;             /**< Number of destinations */
    uint32_t sends_to_capacity;          /**< Array capacity */

    bio_module_id_t* receives_from;      /**< Modules to receive from */
    uint32_t receives_from_count;        /**< Number of sources */
    uint32_t receives_from_capacity;     /**< Array capacity */

    /* Message handlers */
    bio_message_type_t* handles_messages; /**< Message types handled */
    uint32_t handles_message_count;       /**< Number of handlers */
    uint32_t handles_message_capacity;    /**< Array capacity */

    /* Constraints */
    platform_tier_t min_tier;            /**< Minimum platform tier required */
    wiring_hardware_flags_t required_hw; /**< Required hardware flags */

    /* State */
    bool enabled;                        /**< Module is enabled */
    bool discovered;                     /**< Wiring has been discovered */
    uint64_t discovery_time_ms;          /**< When wiring was discovered */
} wiring_module_config_t;

/* ============================================================================
 * Handler Callback
 * ============================================================================ */

/**
 * @brief Callback type for handler registration
 *
 * WHAT: Callback invoked by orchestrator when wiring is discovered
 * WHY:  Enable modules to auto-register handlers for discovered message types
 * HOW:  Called with list of message types module should handle
 *
 * @param ctx Bio-async module context
 * @param message_types Array of message types to handle
 * @param message_count Number of message types
 * @param user_data User data passed during callback registration
 * @return 0 on success, -1 on error
 */
typedef int (*wiring_handler_callback_t)(
    bio_module_context_t ctx,
    const bio_message_type_t* message_types,
    uint32_t message_count,
    void* user_data
);

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Create wiring diagram manager
 *
 * WHAT: Initialize wiring diagram system
 * WHY:  Central manager for loading and querying wiring configurations
 * HOW:  Allocate structures, set base path, prepare for loading
 *
 * @param base_path Base path to wiring directory (NULL for default .aim/wiring)
 * @return New wiring diagram manager or NULL on failure
 */
wiring_diagram_t* wiring_diagram_create(const char* base_path);

/**
 * @brief Destroy wiring diagram manager
 *
 * WHAT: Clean up wiring diagram resources
 * WHY:  Proper resource deallocation
 * HOW:  Free all module configs, close files, free manager
 *
 * @param wd Wiring diagram to destroy (NULL safe)
 */
void wiring_diagram_destroy(wiring_diagram_t* wd);

/* ============================================================================
 * Loading API
 * ============================================================================ */

/**
 * @brief Load master wiring diagram
 *
 * WHAT: Load base module wiring from master.jsonl
 * WHY:  Establish baseline connectivity for all modules
 * HOW:  Parse JSONL, populate module configs
 *
 * @param wd Wiring diagram manager
 * @return 0 on success, -1 on error
 */
int wiring_diagram_load_master(wiring_diagram_t* wd);

/**
 * @brief Load subsystem-specific wiring
 *
 * WHAT: Load wiring for specific subsystem from subsystems/<name>.jsonl
 * WHY:  Add subsystem-specific connectivity
 * HOW:  Merge subsystem entries with existing configs
 *
 * @param wd Wiring diagram manager
 * @param subsystem Subsystem to load
 * @return 0 on success, -1 on error (missing file returns 0)
 */
int wiring_diagram_load_subsystem(wiring_diagram_t* wd, wiring_subsystem_t subsystem);

/**
 * @brief Load all subsystem wiring diagrams
 *
 * WHAT: Load all subsystem X.jsonl files
 * WHY:  Convenience function to load all subsystems at once
 * HOW:  Iterate WIRING_SUBSYSTEM_* and load each
 *
 * @param wd Wiring diagram manager
 * @return 0 on success, -1 on error
 */
int wiring_diagram_load_all_subsystems(wiring_diagram_t* wd);

/**
 * @brief Load platform tier-specific wiring
 *
 * WHAT: Load wiring for specific platform tier from platforms/<tier>.jsonl
 * WHY:  Apply tier-specific overrides (enable/disable modules)
 * HOW:  Merge tier entries with existing configs
 *
 * @param wd Wiring diagram manager
 * @param tier Platform tier to load
 * @return 0 on success, -1 on error (missing file returns 0)
 */
int wiring_diagram_load_platform(wiring_diagram_t* wd, platform_tier_t tier);

/**
 * @brief Load hardware-specific wiring
 *
 * WHAT: Load wiring for specific hardware from hardware/<hw>.jsonl
 * WHY:  Apply hardware-specific configurations
 * HOW:  Load based on hardware flags (may load multiple files)
 *
 * @param wd Wiring diagram manager
 * @param hw Hardware flags to load configurations for
 * @return 0 on success, -1 on error
 */
int wiring_diagram_load_hardware(wiring_diagram_t* wd, wiring_hardware_flags_t hw);

/**
 * @brief Load custom wiring diagram
 *
 * WHAT: Load user-provided custom wiring from custom/<name>.jsonl
 * WHY:  Allow user overrides and extensions
 * HOW:  Merge custom entries (override existing, add new)
 *
 * @param wd Wiring diagram manager
 * @param filename Custom diagram filename (without path)
 * @return 0 on success, -1 on error
 */
int wiring_diagram_load_custom(wiring_diagram_t* wd, const char* filename);

/**
 * @brief Load all custom wiring diagrams
 *
 * WHAT: Load all custom X.jsonl files
 * WHY:  Apply all user customizations
 * HOW:  Iterate custom/ directory and load each
 *
 * @param wd Wiring diagram manager
 * @return 0 on success, -1 on error
 */
int wiring_diagram_load_all_custom(wiring_diagram_t* wd);

/**
 * @brief Load composite wiring for hardware profile
 *
 * WHAT: Load and merge all applicable diagrams for given profile
 * WHY:  One-shot loading with correct priority ordering
 * HOW:  Load master → subsystems → platform → hardware → custom
 *
 * @param wd Wiring diagram manager
 * @param profile Hardware profile to load for
 * @return 0 on success, -1 on error
 */
int wiring_diagram_load_for_profile(
    wiring_diagram_t* wd,
    const wiring_hardware_profile_t* profile
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get module wiring configuration
 *
 * WHAT: Query wiring configuration for a specific module
 * WHY:  Enable modules to discover their connectivity at runtime
 * HOW:  Lookup module by name in loaded configurations
 *
 * @param wd Wiring diagram manager
 * @param module_name Module name to query
 * @param config Output configuration (caller allocated)
 * @return 0 on success, -1 if not found
 */
int wiring_diagram_get_module_config(
    const wiring_diagram_t* wd,
    const char* module_name,
    wiring_module_config_t* config
);

/**
 * @brief Get module wiring configuration by ID
 *
 * WHAT: Query wiring configuration by bio-async module ID
 * WHY:  Lookup by ID is often more convenient than by name
 * HOW:  Lookup module by ID in loaded configurations
 *
 * @param wd Wiring diagram manager
 * @param module_id Module ID to query
 * @param config Output configuration (caller allocated)
 * @return 0 on success, -1 if not found
 */
int wiring_diagram_get_module_config_by_id(
    const wiring_diagram_t* wd,
    bio_module_id_t module_id,
    wiring_module_config_t* config
);

/**
 * @brief Check if module is available for current profile
 *
 * WHAT: Test if module should be enabled for given hardware profile
 * WHY:  Determine if module meets tier/hardware requirements
 * HOW:  Check min_tier and required_hw against profile
 *
 * @param wd Wiring diagram manager
 * @param module_name Module name to check
 * @param profile Hardware profile to check against
 * @return true if available, false otherwise
 */
bool wiring_diagram_module_available(
    const wiring_diagram_t* wd,
    const char* module_name,
    const wiring_hardware_profile_t* profile
);

/**
 * @brief Get all modules in a subsystem
 *
 * WHAT: Query all modules belonging to a subsystem
 * WHY:  Enable subsystem-level operations
 * HOW:  Filter modules by subsystem field
 *
 * @param wd Wiring diagram manager
 * @param subsystem Subsystem to query
 * @param module_names Output array of module names (caller allocated)
 * @param max_modules Array capacity
 * @return Number of modules found
 */
uint32_t wiring_diagram_get_subsystem_modules(
    const wiring_diagram_t* wd,
    wiring_subsystem_t subsystem,
    const char** module_names,
    uint32_t max_modules
);

/**
 * @brief Get all loaded modules
 *
 * WHAT: Enumerate all modules in the wiring diagram
 * WHY:  System introspection
 * HOW:  Return array of all module names
 *
 * @param wd Wiring diagram manager
 * @param module_names Output array of module names (caller allocated)
 * @param max_modules Array capacity
 * @return Number of modules found
 */
uint32_t wiring_diagram_get_all_modules(
    const wiring_diagram_t* wd,
    const char** module_names,
    uint32_t max_modules
);

/**
 * @brief Get total module count
 *
 * WHAT: Return the number of modules loaded in the wiring diagram
 * WHY:  Quick check for self-assembly availability
 * HOW:  Return module_count from wiring diagram
 *
 * @param wd Wiring diagram manager
 * @return Number of modules, 0 if empty or NULL
 */
uint32_t wiring_diagram_get_module_count(const wiring_diagram_t* wd);

/* ============================================================================
 * Dynamic Update API
 * ============================================================================ */

/**
 * @brief Add or update module in wiring diagram
 *
 * WHAT: Add new module or update existing module configuration
 * WHY:  Enable dynamic wiring modifications at runtime
 * HOW:  Insert/update in module map, auto-persist if enabled
 *
 * @param wd Wiring diagram manager
 * @param module_name Module name
 * @param config Module configuration
 * @return 0 on success, -1 on error
 */
int wiring_diagram_add_module(
    wiring_diagram_t* wd,
    const char* module_name,
    const wiring_module_config_t* config
);

/**
 * @brief Add relation between modules
 *
 * WHAT: Add a wiring relation between two modules
 * WHY:  Enable dynamic connectivity changes
 * HOW:  Update source module's sends_to/depends_on arrays
 *
 * @param wd Wiring diagram manager
 * @param from Source module name
 * @param to Destination module name
 * @param relation_type Type of relation
 * @return 0 on success, -1 on error
 */
int wiring_diagram_add_relation(
    wiring_diagram_t* wd,
    const char* from,
    const char* to,
    wiring_relation_type_t relation_type
);

/**
 * @brief Add message handler to module
 *
 * WHAT: Register that a module handles a message type
 * WHY:  Enable dynamic handler registration
 * HOW:  Add to module's handles_messages array
 *
 * @param wd Wiring diagram manager
 * @param module_name Module name
 * @param message_type Message type to handle
 * @return 0 on success, -1 on error
 */
int wiring_diagram_add_handler(
    wiring_diagram_t* wd,
    const char* module_name,
    bio_message_type_t message_type
);

/**
 * @brief Remove module from wiring diagram
 *
 * WHAT: Remove a module and its relations
 * WHY:  Enable module removal at runtime
 * HOW:  Remove from map, clean up relations, auto-persist
 *
 * @param wd Wiring diagram manager
 * @param module_name Module name to remove
 * @return 0 on success, -1 if not found
 */
int wiring_diagram_remove_module(wiring_diagram_t* wd, const char* module_name);

/**
 * @brief Enable or disable module
 *
 * WHAT: Toggle module enabled state
 * WHY:  Selective activation without removing wiring
 * HOW:  Set module's enabled flag, auto-persist
 *
 * @param wd Wiring diagram manager
 * @param module_name Module name
 * @param enabled Enabled state
 * @return 0 on success, -1 if not found
 */
int wiring_diagram_set_enabled(
    wiring_diagram_t* wd,
    const char* module_name,
    bool enabled
);

/* ============================================================================
 * Persistence API
 * ============================================================================ */

/**
 * @brief Persist wiring diagram to disk
 *
 * WHAT: Save current wiring state to JSONL files
 * WHY:  Maintain configuration across restarts
 * HOW:  Write to master.jsonl (or subsystem files based on module membership)
 *
 * NOTE: Auto-persist is enabled by default. This forces immediate save.
 *
 * @param wd Wiring diagram manager
 * @return 0 on success, -1 on error
 */
int wiring_diagram_persist(wiring_diagram_t* wd);

/**
 * @brief Persist specific subsystem to disk
 *
 * WHAT: Save only specified subsystem's wiring
 * WHY:  Selective persistence for partial updates
 * HOW:  Write to subsystems/<name>.jsonl
 *
 * @param wd Wiring diagram manager
 * @param subsystem Subsystem to persist
 * @return 0 on success, -1 on error
 */
int wiring_diagram_persist_subsystem(wiring_diagram_t* wd, wiring_subsystem_t subsystem);

/**
 * @brief Set auto-persist behavior
 *
 * WHAT: Enable or disable automatic persistence on changes
 * WHY:  Control when wiring is saved to disk
 * HOW:  Set internal flag (default: enabled)
 *
 * @param wd Wiring diagram manager
 * @param enabled Auto-persist enabled
 */
void wiring_diagram_set_auto_persist(wiring_diagram_t* wd, bool enabled);

/**
 * @brief Check if auto-persist is enabled
 *
 * @param wd Wiring diagram manager
 * @return true if auto-persist is enabled
 */
bool wiring_diagram_get_auto_persist(const wiring_diagram_t* wd);

/* ============================================================================
 * Brain KG Integration API
 * ============================================================================ */

/**
 * @brief Sync wiring diagram to brain_kg
 *
 * WHAT: Create/update brain_kg nodes and edges from wiring diagram
 * WHY:  Enable runtime topology queries via brain_kg
 * HOW:  Create module nodes, HANDLES_MESSAGE edges, DEPENDS_ON edges
 *
 * @param wd Wiring diagram manager
 * @param kg Brain knowledge graph
 * @return 0 on success, -1 on error
 */
int wiring_diagram_sync_to_brain_kg(wiring_diagram_t* wd, brain_kg_t* kg);

/**
 * @brief Sync changes from brain_kg back to wiring diagram
 *
 * WHAT: Update wiring diagram from brain_kg changes
 * WHY:  Enable bidirectional sync for runtime modifications
 * HOW:  Query brain_kg for HANDLES_MESSAGE, update configs
 *
 * @param wd Wiring diagram manager
 * @param kg Brain knowledge graph
 * @return 0 on success, -1 on error
 */
int wiring_diagram_sync_from_brain_kg(wiring_diagram_t* wd, const brain_kg_t* kg);

/* ============================================================================
 * Validation and Dependency Resolution API (Phase 9)
 * ============================================================================ */

/**
 * @brief Validation result structure
 */
typedef struct {
    bool valid;                          /**< Overall validation result */
    uint32_t error_count;                /**< Number of errors found */
    uint32_t warning_count;              /**< Number of warnings found */
    char errors[16][256];                /**< Error messages (max 16) */
    char warnings[16][256];              /**< Warning messages (max 16) */
    bool has_circular_deps;              /**< Circular dependencies detected */
    bool has_missing_deps;               /**< Missing dependencies detected */
    bool has_invalid_handlers;           /**< Invalid handler configs detected */
} wiring_validation_result_t;

/**
 * @brief Validate wiring diagram configuration
 *
 * WHAT: Check wiring diagram for consistency and correctness
 * WHY:  Catch configuration errors before runtime
 * HOW:  Validate module configs, check dependencies, verify handlers
 *
 * Checks performed:
 * - All dependencies reference existing modules
 * - No circular dependencies (via check_circular_deps)
 * - All handler message types are valid
 * - Module IDs are unique
 * - Required modules are enabled
 *
 * @param wd Wiring diagram to validate
 * @param result Validation result (caller allocated)
 * @return 0 if valid, -1 if invalid (check result for details)
 */
int wiring_diagram_validate(const wiring_diagram_t* wd, wiring_validation_result_t* result);

/**
 * @brief Check for circular dependencies in wiring diagram
 *
 * WHAT: Detect circular dependency chains
 * WHY:  Circular deps cause infinite loops in startup ordering
 * HOW:  Depth-first search with cycle detection
 *
 * @param wd Wiring diagram to check
 * @param cycle_modules Output array for modules in cycle (optional, can be NULL)
 * @param max_cycle_modules Capacity of cycle_modules array
 * @param cycle_count Output number of modules in cycle (optional)
 * @return 0 if no cycles, -1 if cycles detected
 */
int wiring_diagram_check_circular_deps(
    const wiring_diagram_t* wd,
    bio_module_id_t* cycle_modules,
    uint32_t max_cycle_modules,
    uint32_t* cycle_count
);

/**
 * @brief Get module startup order (topological sort)
 *
 * WHAT: Compute correct startup order respecting dependencies
 * WHY:  Modules must start after their dependencies
 * HOW:  Kahn's algorithm for topological sorting
 *
 * @param wd Wiring diagram
 * @param order_out Output array for startup order (caller allocated)
 * @param max_modules Capacity of order_out array
 * @return Number of modules in order, -1 on error (circular deps)
 */
int wiring_diagram_get_startup_order(
    const wiring_diagram_t* wd,
    bio_module_id_t* order_out,
    uint32_t max_modules
);

/**
 * @brief Get dependency chain for a module
 *
 * WHAT: Get all transitive dependencies for a module
 * WHY:  Know full dependency tree for module isolation
 * HOW:  BFS/DFS traversal of DEPENDS_ON relations
 *
 * @param wd Wiring diagram
 * @param module_id Module to get dependencies for
 * @param deps_out Output array for dependencies (caller allocated)
 * @param max_deps Capacity of deps_out array
 * @return Number of dependencies, -1 on error
 */
int wiring_diagram_get_dependency_chain(
    const wiring_diagram_t* wd,
    bio_module_id_t module_id,
    bio_module_id_t* deps_out,
    uint32_t max_deps
);

/**
 * @brief Get modules that depend on a given module
 *
 * WHAT: Get reverse dependencies (dependents)
 * WHY:  Know what breaks if a module is disabled
 * HOW:  Traverse all modules checking DEPENDS_ON
 *
 * @param wd Wiring diagram
 * @param module_id Module to find dependents for
 * @param dependents_out Output array for dependents (caller allocated)
 * @param max_dependents Capacity of dependents_out array
 * @return Number of dependents, -1 on error
 */
int wiring_diagram_get_dependents(
    const wiring_diagram_t* wd,
    bio_module_id_t module_id,
    bio_module_id_t* dependents_out,
    uint32_t max_dependents
);

/**
 * @brief Check if module can be safely disabled
 *
 * WHAT: Verify no critical dependencies would break
 * WHY:  Prevent disabling modules that others need
 * HOW:  Check dependents and their criticality
 *
 * @param wd Wiring diagram
 * @param module_id Module to check
 * @return 1 if safe to disable, 0 if not safe, -1 on error
 */
int wiring_diagram_can_disable_module(
    const wiring_diagram_t* wd,
    bio_module_id_t module_id
);

/* ============================================================================
 * Hardware Detection API
 * ============================================================================ */

/**
 * @brief Detect hardware profile for current system
 *
 * WHAT: Auto-detect platform tier and available hardware
 * WHY:  Enable automatic wiring configuration selection
 * HOW:  Query system resources, GPU APIs, check for neuromorphic
 *
 * @param profile Output hardware profile (caller allocated)
 * @return 0 on success, -1 on error
 */
int wiring_detect_hardware_profile(wiring_hardware_profile_t* profile);

/**
 * @brief Get default hardware profile
 *
 * WHAT: Get safe default profile for unknown hardware
 * WHY:  Fallback when detection fails
 * HOW:  Return conservative CPU-only profile
 *
 * @param profile Output hardware profile (caller allocated)
 */
void wiring_get_default_profile(wiring_hardware_profile_t* profile);

/* ============================================================================
 * Utility API
 * ============================================================================ */

/**
 * @brief Convert subsystem enum to string
 *
 * @param subsystem Subsystem enum
 * @return Human-readable string
 */
const char* wiring_subsystem_to_string(wiring_subsystem_t subsystem);

/**
 * @brief Convert relation type to string
 *
 * @param relation Relation type enum
 * @return Human-readable string
 */
const char* wiring_relation_to_string(wiring_relation_type_t relation);

/**
 * @brief Convert hardware flags to string
 *
 * @param hw Hardware flags
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Pointer to buffer
 */
const char* wiring_hardware_flags_to_string(
    wiring_hardware_flags_t hw,
    char* buffer,
    size_t buffer_size
);

/**
 * @brief Initialize module config structure
 *
 * WHAT: Zero-initialize a module config with proper defaults
 * WHY:  Ensure clean initialization before use
 * HOW:  memset to zero, set defaults
 *
 * @param config Module config to initialize
 */
void wiring_module_config_init(wiring_module_config_t* config);

/**
 * @brief Clean up module config arrays
 *
 * WHAT: Free dynamically allocated arrays in module config
 * WHY:  Proper memory cleanup
 * HOW:  Free depends_on, sends_to, receives_from, handles_messages arrays
 *
 * @param config Module config to clean up
 */
void wiring_module_config_cleanup(wiring_module_config_t* config);

/**
 * @brief Get wiring diagram statistics
 *
 * WHAT: Get counts and status of loaded wiring
 * WHY:  System introspection and debugging
 * HOW:  Count modules, relations, enabled/disabled
 *
 * @param wd Wiring diagram manager
 * @param total_modules Output total module count
 * @param enabled_modules Output enabled module count
 * @param total_relations Output total relation count
 * @return 0 on success, -1 on error
 */
int wiring_diagram_get_stats(
    const wiring_diagram_t* wd,
    uint32_t* total_modules,
    uint32_t* enabled_modules,
    uint32_t* total_relations
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_WIRING_DIAGRAM_H */
