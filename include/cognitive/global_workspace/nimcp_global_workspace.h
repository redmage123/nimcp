/**
 * @file nimcp_global_workspace.h
 * @brief Global Workspace Theory implementation for conscious access
 *
 * WHAT: Central broadcast architecture for information integration across modules
 * WHY:  Enable flexible information sharing, conscious access, and unified cognition
 * HOW:  Limited-capacity broadcast buffer + competition mechanism + subscriber notification
 *
 * DESIGN RATIONALE:
 * Cognitive modules in NIMCP (working memory, executive, theory of mind, etc.)
 * currently operate in relative isolation. The Global Workspace provides a
 * "blackboard" where any module can post information that becomes available
 * to ALL other modules. This creates:
 * - Flexible information routing (not hard-coded pipelines)
 * - Natural attention bottleneck (competition for limited workspace)
 * - Conscious access (workspace content = conscious content)
 * - Emergent integration (modules automatically coordinate via broadcast)
 *
 * DESIGN PATTERNS:
 * - Publish-Subscribe: Modules subscribe to broadcasts
 * - Producer-Consumer: Modules produce content, others consume
 * - Singleton: One global workspace per brain
 * - Observer: Subscribers notified of broadcasts
 * - Strategy: Pluggable competition strategies
 *
 * BIOLOGICAL INSPIRATION:
 * Based on Bernard Baars' Global Workspace Theory (1988) and
 * Stanislas Dehaene's neuronal workspace model (2001, 2011):
 *
 * 1. CONSCIOUS ACCESS = GLOBAL BROADCAST
 *    - Prefrontal-parietal network acts as "workspace"
 *    - Information in workspace is globally available
 *    - Non-conscious processing = local, modular
 *    - Conscious processing = global, integrated
 *
 * 2. COMPETITION FOR ACCESS
 *    - Limited capacity (~1 "chunk" at a time)
 *    - Winner-take-all competition based on salience
 *    - "Ignition" = crossing threshold for broadcast
 *
 * 3. BROADCAST TO SUBSCRIBERS
 *    - Once in workspace, information is broadcast
 *    - All modules receive broadcast (parallel access)
 *    - Enables flexible coordination
 *
 * BRAIN AREAS:
 * - Prefrontal Cortex: Executive control, maintains workspace content
 * - Parietal Cortex: Attention control, competition mechanism
 * - Anterior Cingulate: Conflict monitoring, competition resolution
 * - Thalamus: Gating, enables/disables workspace access
 *
 * COGNITIVE PHENOMENA EXPLAINED:
 * - Attentional Blink: Workspace locked during consolidation
 * - Change Blindness: Non-workspace changes go unnoticed
 * - Inattentional Blindness: No competition → no workspace access
 * - Limited Multitasking: Only one item in workspace at a time
 * - Flexible Control: Workspace content guides behavior
 *
 * EXAMPLE:
 * @code
 *   // Create global workspace
 *   global_workspace_config_t config = global_workspace_default_config();
 *   config.capacity_dim = 256;
 *   config.ignition_threshold = 0.6;
 *   global_workspace_t* workspace = global_workspace_create(&config);
 *
 *   // Modules subscribe to broadcasts
 *   global_workspace_subscribe(workspace, MODULE_WORKING_MEMORY);
 *   global_workspace_subscribe(workspace, MODULE_EXECUTIVE);
 *   global_workspace_subscribe(workspace, MODULE_THEORY_OF_MIND);
 *
 *   // Module competes for workspace access
 *   float content[256];
 *   // ... fill content with important information ...
 *   bool won = global_workspace_compete(workspace, MODULE_WORKING_MEMORY,
 *                                        content, 256, 0.85);
 *   if (won) {
 *       // Content is now globally broadcast!
 *       // All subscribers can access it
 *   }
 *
 *   // Other modules read broadcast
 *   float received[256];
 *   uint32_t dim;
 *   cognitive_module_t source;
 *   if (global_workspace_read_broadcast(workspace, received, 256, &dim, &source)) {
 *       // Use broadcast content for decision-making
 *       if (source == MODULE_WORKING_MEMORY) {
 *           // Working memory content now available
 *       }
 *   }
 *
 *   global_workspace_destroy(workspace);
 * @endcode
 *
 * PHASE: 11 (Cognitive Pipeline Improvements)
 * DEPENDENCIES: None (standalone, but integrates with all cognitive modules)
 * TRAINING_IMPACT: Minimal (workspace dynamics, competition weights can be learned)
 *
 * @author NIMCP Development Team - Part J
 * @date 2025-11-11
 * @version 2.8.0 Phase 11 (Part J1)
 */

#ifndef NIMCP_GLOBAL_WORKSPACE_H
#define NIMCP_GLOBAL_WORKSPACE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "utils/platform/nimcp_tier_optimization.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Configuration Constants (NIMCP Coding Standards - Named Constants)
//=============================================================================

/**
 * @brief Maximum workspace content dimensionality
 *
 * WHAT: Upper limit on content vector size
 * WHY:  Prevent unbounded memory allocation
 * VALUE: 1024 floats = 4KB per broadcast (reasonable for most representations)
 */
#define GLOBAL_WORKSPACE_MAX_DIM 1024

/**
 * @brief Default workspace content dimensionality
 *
 * WHAT: Standard size for workspace content
 * WHY:  Matches typical feature vector sizes in NIMCP
 * VALUE: 256 floats = 1KB (same as working memory items)
 * BIOLOGICAL: Roughly corresponds to ~7 chunks × 36 features per chunk
 */
#define GLOBAL_WORKSPACE_DEFAULT_DIM 256

/**
 * @brief Maximum number of competing modules
 *
 * WHAT: How many modules can compete for workspace simultaneously
 * WHY:  Prevent unbounded competition pool
 * VALUE: 32 modules (more than enough for NIMCP's ~20 cognitive modules)
 */
#define GLOBAL_WORKSPACE_MAX_COMPETITORS 32

/**
 * @brief Maximum number of subscriber modules
 *
 * WHAT: How many modules can listen to broadcasts
 * WHY:  Prevent unbounded subscriber list
 * VALUE: 32 subscribers (all cognitive modules can subscribe)
 */
#define GLOBAL_WORKSPACE_MAX_SUBSCRIBERS 32

/**
 * @brief Default ignition threshold
 *
 * WHAT: Minimum signal strength required for workspace access
 * WHY:  Prevents weak signals from reaching consciousness
 * VALUE: 0.6 (60% strength) - balances access vs selectivity
 * BIOLOGICAL: Corresponds to "ignition" in neuronal workspace theory
 */
#define GLOBAL_WORKSPACE_DEFAULT_IGNITION_THRESHOLD 0.6f

/**
 * @brief Minimum ignition threshold
 *
 * WHAT: Lower bound for ignition threshold
 * WHY:  Too low = everything gets in (no selectivity)
 * VALUE: 0.3 (30%) - ensures some filtering
 */
#define GLOBAL_WORKSPACE_MIN_IGNITION_THRESHOLD 0.3f

/**
 * @brief Maximum ignition threshold
 *
 * WHAT: Upper bound for ignition threshold
 * WHY:  Too high = nothing gets in (workspace unused)
 * VALUE: 2.0 (200%) - allows room for inflammation modulation
 *        Storm level multiplies by 1.8x, so with baseline 0.6 we get 1.08
 *        Setting to 2.0 provides headroom for extreme modulation
 */
#define GLOBAL_WORKSPACE_MAX_IGNITION_THRESHOLD 2.0f

/**
 * @brief Broadcast refractory period in milliseconds
 *
 * WHAT: Minimum time between broadcasts
 * WHY:  Prevent rapid switching (consolidation time)
 * VALUE: 50ms - matches attentional blink timescale
 * BIOLOGICAL: Workspace needs time to consolidate content
 */
#define GLOBAL_WORKSPACE_REFRACTORY_PERIOD_MS 50

/**
 * @brief Broadcast history depth
 *
 * WHAT: How many recent broadcasts to remember
 * WHY:  Track temporal dynamics, enable recency analysis
 * VALUE: Tier-based (FULL=32, MEDIUM=16, CONSTRAINED=8, MINIMAL=4)
 */
#define GLOBAL_WORKSPACE_HISTORY_DEPTH NIMCP_HISTORY_SIZE_SMALL

/**
 * @brief Competition decay time constant (ms)
 *
 * WHAT: How fast competitor signals decay
 * WHY:  Stale competition signals should fade
 * VALUE: 200ms - typical working memory decay
 */
#define GLOBAL_WORKSPACE_COMPETITION_DECAY_TAU_MS 200.0f

//=============================================================================
// Core Types
//=============================================================================

/**
 * @brief Cognitive module identifiers
 *
 * WHAT: Enum of all cognitive modules that can access workspace
 * WHY:  Type-safe module identification
 * HOW:  Each module gets unique ID
 *
 * EXTENSIBILITY: Add new modules as NIMCP grows
 */
typedef enum {
    MODULE_NONE = 0,               /**< Invalid/uninitialized */
    MODULE_PERCEPTION,             /**< Visual/audio/multimodal input */
    MODULE_WORKING_MEMORY,         /**< Active buffer (7±2 items) */
    MODULE_EXECUTIVE,              /**< Task control, planning, inhibition */
    MODULE_THEORY_OF_MIND,         /**< Belief/desire/intention modeling */
    MODULE_ETHICS,                 /**< Empathy, moral reasoning */
    MODULE_EPISODIC_MEMORY,        /**< Autobiographical experiences */
    MODULE_SEMANTIC_MEMORY,        /**< Facts, concepts, knowledge */
    MODULE_LANGUAGE,               /**< NLP, speech, generation */
    MODULE_EMOTION,                /**< Emotional states and regulation */
    MODULE_SALIENCE,               /**< Novelty, surprise, urgency */
    MODULE_MOTOR,                  /**< Action planning and execution */
    MODULE_ATTENTION,              /**< Attention control and filtering */
    MODULE_METACOGNITION,          /**< Confidence, error awareness */
    MODULE_CURIOSITY,              /**< Exploration, novelty-seeking */
    MODULE_INTROSPECTION,          /**< Self-awareness, reflection */
    MODULE_PREDICTIVE,             /**< Free energy, prediction errors */
    MODULE_CONSOLIDATION,          /**< Memory consolidation */
    MODULE_WELLBEING,              /**< Distress monitoring */
    MODULE_MENTAL_HEALTH,          /**< Disorder detection */
    MODULE_GOAL_MOTIVATION,        /**< Goal hierarchy, rewards */
    MODULE_COGNITIVE_CONTROL,      /**< Error/conflict monitoring */
    MODULE_INNER_DIALOGUE,         /**< Inner dialogue engine (multi-perspective deliberation) */
    MODULE_CUSTOM_START = 100      /**< User-defined modules start here */
} cognitive_module_t;

/**
 * @brief Global workspace instance (opaque pointer)
 *
 * WHAT: Opaque handle to global workspace
 * WHY:  Encapsulation - hide implementation details
 * PATTERN: Opaque pointer (Pimpl idiom)
 * THREAD-SAFETY: One workspace per brain, brain operations are single-threaded
 */
#ifndef GLOBAL_WORKSPACE_T_DEFINED
#define GLOBAL_WORKSPACE_T_DEFINED
typedef struct global_workspace_struct* global_workspace_t;
#endif

/**
 * @brief Competition strategy for workspace access
 *
 * WHAT: How to resolve competition when multiple modules compete
 * WHY:  Different strategies for different scenarios
 * HOW:  Strategy pattern - pluggable competition resolution
 */
typedef enum {
    /**
     * WHAT: Winner-take-all (strongest signal wins)
     * WHY:  Simple, biologically plausible, fast
     * WHEN: Default - most cognitively realistic
     * ALGORITHM: argmax(signal_strengths)
     */
    COMPETITION_WINNER_TAKE_ALL,

    /**
     * WHAT: Weighted fusion (blend multiple strong signals)
     * WHY:  Allow mixed representations
     * WHEN: When integration of multiple sources is desired
     * ALGORITHM: weighted_sum(contents * strengths)
     * NOTE: May dilute individual signals
     */
    COMPETITION_WEIGHTED_FUSION,

    /**
     * WHAT: Priority-based (highest priority module wins)
     * WHY:  Some modules are more important (e.g., safety)
     * WHEN: Hierarchical module importance
     * ALGORITHM: argmax(priorities), then strength as tiebreaker
     */
    COMPETITION_PRIORITY_BASED,

    /**
     * WHAT: Round-robin (take turns)
     * WHY:  Ensure all modules get occasional access
     * WHEN: Fairness is important, prevent starvation
     * ALGORITHM: winner = next_in_sequence(last_winner)
     * NOTE: Less biologically realistic
     */
    COMPETITION_ROUND_ROBIN

} competition_strategy_t;

/**
 * @brief Global workspace configuration
 *
 * WHAT: Hyperparameters for workspace behavior
 * WHY:  Flexible configuration for different scenarios
 * HOW:  Initialize with global_workspace_default_config(), modify as needed
 */
typedef struct {
    // Content dimensions
    uint32_t capacity_dim;              /**< Workspace content dimensionality */

    // Competition parameters
    competition_strategy_t strategy;    /**< Competition resolution strategy */
    float ignition_threshold;           /**< Minimum strength for broadcast */
    uint32_t refractory_period_ms;      /**< Minimum time between broadcasts */
    float competition_decay_tau_ms;     /**< Decay time for stale competition */

    // Module priorities (for COMPETITION_PRIORITY_BASED)
    float module_priorities[MODULE_CUSTOM_START]; /**< Priority per module */

    // History tracking
    uint32_t history_depth;             /**< How many broadcasts to remember */
    bool enable_history;                /**< Track broadcast history? */

    // Statistics
    bool enable_statistics;             /**< Track competition stats? */

} global_workspace_config_t;

/**
 * @brief Workspace broadcast state
 *
 * WHAT: Current content being broadcast
 * WHY:  Subscribers need to know what's being broadcast and who sent it
 * HOW:  Populated by competition winner, read by subscribers
 *
 * LIFETIME: Updated on each successful competition
 */
typedef struct {
    // Content
    float* content;                     /**< Broadcast content vector */
    uint32_t content_dim;               /**< Content dimensionality */

    // Source information
    cognitive_module_t source_module;   /**< Which module won competition */
    float source_strength;              /**< Strength of winning signal */

    // Timing
    uint64_t broadcast_timestamp_ms;    /**< When broadcast occurred */
    uint32_t broadcast_id;              /**< Sequential broadcast ID */

    // Competition context
    uint32_t num_competitors;           /**< How many competed this round */
    float runner_up_strength;           /**< Second-place signal strength */

    // Validity
    bool is_valid;                      /**< Is there currently a broadcast? */

    // Phase 1.5: Copy-on-Write support for efficient broadcast sharing
    uint32_t* _cow_refcount;            /**< Shared reference count (NULL = owned) */
    bool _cow_is_shallow;               /**< True if content pointer is shared */

} workspace_broadcast_t;

/**
 * @brief Competitor entry (internal to competition pool)
 *
 * WHAT: One module's submission for workspace access
 * WHY:  Track all competing signals for resolution
 * HOW:  Populated by global_workspace_compete()
 */
typedef struct {
    cognitive_module_t module;          /**< Competing module */
    float* content;                     /**< Content to broadcast if wins */
    uint32_t content_dim;               /**< Content dimensionality */
    float strength;                     /**< Signal strength (0-1) */
    uint64_t timestamp_ms;              /**< When submitted */
    bool is_active;                     /**< Currently competing? */
} competitor_entry_t;

/**
 * @brief Workspace statistics
 *
 * WHAT: Performance and usage metrics
 * WHY:  Monitor workspace dynamics, debug competition
 * HOW:  Accumulated counters, updated on each operation
 */
typedef struct {
    // Competition metrics
    uint64_t total_competitions;        /**< Total competition events */
    uint64_t total_broadcasts;          /**< Successful broadcasts */
    uint64_t rejected_submissions;      /**< Below ignition threshold */

    // Per-module statistics
    uint64_t broadcasts_per_module[MODULE_CUSTOM_START]; /**< Wins per module */
    uint64_t competitions_per_module[MODULE_CUSTOM_START]; /**< Submissions per module */

    // Timing
    uint64_t avg_competition_latency_us; /**< Average competition time (microseconds) */
    uint64_t max_competition_latency_us; /**< Max competition time */

    // Current state
    uint32_t current_subscribers;       /**< Number of active subscribers */
    uint32_t current_competitors;       /**< Number in competition pool */

    // Threshold violations
    uint64_t refractory_violations;     /**< Attempts during refractory period */

} workspace_statistics_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create global workspace with default configuration
 *
 * WHAT: Allocate and initialize workspace with sensible defaults
 * WHY:  Convenient creation for common use cases
 * HOW:  Calls global_workspace_create_custom() with defaults
 *
 * @return Workspace handle or NULL on failure
 *
 * COMPLEXITY: O(1)
 * MALLOC: Yes (workspace structure + content buffer)
 * THREAD-SAFE: Yes (initialization only)
 * FAILURE: Returns NULL if allocation fails
 *
 * USAGE:
 * @code
 *   global_workspace_t* ws = global_workspace_create();
 *   if (!ws) { fprintf(stderr, "Failed to create workspace\n"); }
 * @endcode
 */
global_workspace_t* global_workspace_create(void);

/**
 * @brief Create global workspace with custom configuration
 *
 * WHAT: Allocate and initialize workspace with specified parameters
 * WHY:  Flexible creation for specialized scenarios
 * HOW:  Allocate workspace struct + content buffers + history
 *
 * @param config Configuration parameters (NULL = use defaults)
 * @return Workspace handle or NULL on failure
 *
 * COMPLEXITY: O(D × H) where D=capacity_dim, H=history_depth
 * MALLOC: Yes (workspace + content[capacity_dim] + history[history_depth])
 * THREAD-SAFE: Yes (initialization only)
 * FAILURE: Returns NULL if:
 *   - Allocation fails
 *   - capacity_dim > GLOBAL_WORKSPACE_MAX_DIM
 *   - ignition_threshold out of range
 *
 * VALIDATION: Config parameters are validated, clamped to valid ranges
 *
 * USAGE:
 * @code
 *   global_workspace_config_t cfg = global_workspace_default_config();
 *   cfg.capacity_dim = 512;
 *   cfg.ignition_threshold = 0.7;
 *   cfg.strategy = COMPETITION_WINNER_TAKE_ALL;
 *   global_workspace_t* ws = global_workspace_create_custom(&cfg);
 * @endcode
 */
global_workspace_t* global_workspace_create_custom(const global_workspace_config_t* config);

/**
 * @brief Destroy global workspace
 *
 * WHAT: Free all allocated memory
 * WHY:  Clean resource management
 * HOW:  Free content buffers, history, competitor pool, workspace struct
 *
 * @param workspace Workspace to destroy (can be NULL)
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: No (caller must ensure exclusive access)
 * IDEMPOTENT: Yes (safe to call with NULL)
 *
 * USAGE:
 * @code
 *   global_workspace_destroy(workspace);
 *   workspace = NULL;  // Good practice
 * @endcode
 */
void global_workspace_destroy(global_workspace_t* workspace);

//=============================================================================
// Core Operations
//=============================================================================

/**
 * @brief Compete for global workspace access
 *
 * WHAT: Module submits content for potential broadcast
 * WHY:  Information must compete for limited workspace capacity
 * HOW:  Add to competition pool, resolve based on strategy, broadcast winner
 *
 * ALGORITHM:
 * 1. Validate inputs (content, strength)
 * 2. Check refractory period (too soon since last broadcast?)
 * 3. Add to competition pool (or update if already competing)
 * 4. Apply competition decay to stale entries
 * 5. Resolve competition based on strategy:
 *    - WINNER_TAKE_ALL: Select strongest signal above threshold
 *    - WEIGHTED_FUSION: Blend all signals above threshold
 *    - PRIORITY_BASED: Select highest priority, then strongest
 *    - ROUND_ROBIN: Select next in sequence
 * 6. If winner exceeds ignition threshold:
 *    - Broadcast content to all subscribers
 *    - Update workspace state
 *    - Record in history
 *    - Return true to winner
 * 7. Else:
 *    - No broadcast (all below threshold)
 *    - Return false to all competitors
 *
 * @param workspace Global workspace handle
 * @param module Which module is competing
 * @param content Content to broadcast if wins (must persist until next competition)
 * @param content_dim Content dimensionality (must match workspace capacity_dim)
 * @param strength Signal strength (0.0-1.0, higher = more likely to win)
 * @return true if this module won and content was broadcast, false otherwise
 *
 * COMPLEXITY: O(N) where N = number of current competitors (typically <10)
 * THREAD-SAFE: No (workspace is single-threaded per brain)
 * FAILURE: Returns false if:
 *   - workspace is NULL
 *   - content is NULL
 *   - content_dim != workspace capacity_dim
 *   - strength < 0 or strength > 1
 *   - During refractory period
 *   - Signal below ignition threshold
 *
 * REFRACTORY PERIOD: If called too soon after last broadcast, submission is
 *                     rejected (strength doesn't matter). This prevents rapid
 *                     switching and allows consolidation.
 *
 * IGNITION: Signal must exceed ignition_threshold to trigger broadcast.
 *           Below threshold = "unconscious" processing (local to module).
 *
 * COMPETITION POOL: Competitors remain in pool until:
 *   - They win (removed after broadcast)
 *   - They decay below threshold (timeout)
 *   - They are replaced by newer submission from same module
 *
 * USAGE:
 * @code
 *   // Working memory has important item
 *   float wm_content[256];
 *   working_memory_get_item(wm, most_salient_idx, wm_content, 256);
 *   float salience = working_memory_get_salience(wm, most_salient_idx);
 *
 *   // Compete for workspace access
 *   bool won = global_workspace_compete(workspace, MODULE_WORKING_MEMORY,
 *                                        wm_content, 256, salience);
 *   if (won) {
 *       printf("Working memory content is now globally broadcast!\n");
 *       // All other modules can now access this content
 *   } else {
 *       printf("Lost competition or below threshold\n");
 *   }
 * @endcode
 */
bool global_workspace_compete(
    global_workspace_t* workspace,
    cognitive_module_t module,
    const float* content,
    uint32_t content_dim,
    float strength
);

/**
 * @brief Submit content to competition pool without immediate resolution
 *
 * WHAT: Add competitor to pool for later evaluation
 * WHY:  Allows batch submissions before resolution
 * HOW:  Adds to pool, does NOT resolve or broadcast
 *
 * Use this when you want multiple modules to submit before resolving.
 * Call global_workspace_resolve() when ready to evaluate and broadcast.
 *
 * @param workspace Global workspace handle
 * @param module Which module is submitting
 * @param content Content to potentially broadcast (must persist until resolve)
 * @param content_dim Content dimensionality
 * @param strength Signal strength (0.0-1.0)
 * @return true if successfully added to pool, false on error
 */
bool global_workspace_submit(
    global_workspace_t* workspace,
    cognitive_module_t module,
    const float* content,
    uint32_t content_dim,
    float strength
);

/**
 * @brief Resolve current competition pool and broadcast winner
 *
 * WHAT: Evaluate all competitors in pool and broadcast strongest
 * WHY:  Explicit control over when resolution happens
 * HOW:  Runs competition strategy, broadcasts winner, clears pool
 *
 * Use after one or more submit() calls to resolve the competition.
 * Returns which module (if any) won and was broadcast.
 *
 * @param workspace Global workspace handle
 * @param winning_module Output: which module won (MODULE_NONE if none)
 * @return true if a winner was broadcast, false if no winner or blocked by refractory
 */
bool global_workspace_resolve(
    global_workspace_t* workspace,
    cognitive_module_t* winning_module
);

/**
 * @brief Read current workspace broadcast
 *
 * WHAT: Subscriber reads current globally-available content
 * WHY:  Modules need to access broadcast information
 * HOW:  Copy current broadcast content to caller's buffer
 *
 * @param workspace Global workspace handle
 * @param content Output buffer (caller allocates, at least max_dim floats)
 * @param max_dim Maximum floats to read (buffer size)
 * @param actual_dim Output: actual dimensionality read
 * @param source Output: which module produced this broadcast (can be NULL)
 * @return true if broadcast available and read successfully, false otherwise
 *
 * COMPLEXITY: O(D) where D = content dimensionality (memory copy)
 * THREAD-SAFE: Yes (read-only access to broadcast)
 * FAILURE: Returns false if:
 *   - workspace is NULL
 *   - content buffer is NULL
 *   - No current broadcast (workspace empty)
 *   - max_dim < actual broadcast dimension
 *
 * SYNCHRONIZATION: Broadcast remains stable between competition events.
 *                  Safe to read at any time (won't change mid-read).
 *
 * TYPICAL USAGE: Subscribers call this periodically or when notified.
 *
 * USAGE:
 * @code
 *   float broadcast_content[256];
 *   uint32_t dim;
 *   cognitive_module_t source;
 *
 *   if (global_workspace_read_broadcast(workspace, broadcast_content, 256,
 *                                        &dim, &source)) {
 *       printf("Received broadcast from %s\n", module_name(source));
 *       // Use content for module's processing
 *       if (source == MODULE_WORKING_MEMORY) {
 *           executive_plan_using_wm_content(exec, broadcast_content, dim);
 *       }
 *   } else {
 *       // No current broadcast
 *   }
 * @endcode
 */
bool global_workspace_read_broadcast(
    global_workspace_t* workspace,
    float* content,
    uint32_t max_dim,
    uint32_t* actual_dim,
    cognitive_module_t* source
);

/**
 * @brief Subscribe module to workspace broadcasts
 *
 * WHAT: Register module as listener for all broadcasts
 * WHY:  Modules must opt-in to receive broadcasts
 * HOW:  Add to subscriber list, receive all future broadcasts
 *
 * SUBSCRIPTION MODEL: Push-based (could be pull-based instead)
 *   - Subscribers are notified when broadcast occurs (not implemented yet)
 *   - Or subscribers poll via global_workspace_read_broadcast()
 *
 * IMPLEMENTATION NOTE: Currently uses pull model (subscribers poll).
 *                      Future: Could add callback support for push model.
 *
 * @param workspace Global workspace handle
 * @param module Which module wants to subscribe
 * @return true if subscription successful, false otherwise
 *
 * COMPLEXITY: O(1) - add to subscriber list
 * THREAD-SAFE: No (workspace is single-threaded)
 * FAILURE: Returns false if:
 *   - workspace is NULL
 *   - module already subscribed
 *   - Subscriber list full (>= MAX_SUBSCRIBERS)
 *
 * IDEMPOTENT: Calling multiple times with same module has no effect (no duplicates)
 *
 * USAGE:
 * @code
 *   // During initialization, modules subscribe
 *   global_workspace_subscribe(workspace, MODULE_WORKING_MEMORY);
 *   global_workspace_subscribe(workspace, MODULE_EXECUTIVE);
 *   global_workspace_subscribe(workspace, MODULE_THEORY_OF_MIND);
 *   // Now these modules can read broadcasts
 * @endcode
 */
bool global_workspace_subscribe(
    global_workspace_t* workspace,
    cognitive_module_t module
);

/**
 * @brief Unsubscribe module from workspace broadcasts
 *
 * WHAT: Remove module from subscriber list
 * WHY:  Module no longer needs/wants broadcasts
 * HOW:  Remove from subscriber list
 *
 * @param workspace Global workspace handle
 * @param module Which module wants to unsubscribe
 * @return true if unsubscription successful, false otherwise
 *
 * COMPLEXITY: O(N) where N = number of subscribers (typically <20)
 * THREAD-SAFE: No
 * FAILURE: Returns false if:
 *   - workspace is NULL
 *   - module not currently subscribed
 *
 * USAGE:
 * @code
 *   // Module no longer needs broadcasts
 *   global_workspace_unsubscribe(workspace, MODULE_CURIOSITY);
 * @endcode
 */
bool global_workspace_unsubscribe(
    global_workspace_t* workspace,
    cognitive_module_t module
);

//=============================================================================
// Query and Inspection Functions
//=============================================================================

/**
 * @brief Check if workspace currently has broadcast
 *
 * WHAT: Is there content in workspace?
 * WHY:  Quick check before attempting to read
 * HOW:  Check broadcast validity flag
 *
 * @param workspace Global workspace handle
 * @return true if broadcast available, false if workspace empty
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 *
 * USAGE:
 * @code
 *   if (global_workspace_has_broadcast(workspace)) {
 *       // Safe to read broadcast
 *       global_workspace_read_broadcast(...);
 *   }
 * @endcode
 */
bool global_workspace_has_broadcast(global_workspace_t* workspace);

/**
 * @brief Get current broadcast source module
 *
 * WHAT: Which module is currently broadcasting?
 * WHY:  Know source without reading full content
 * HOW:  Return source module from current broadcast
 *
 * @param workspace Global workspace handle
 * @return Source module, or MODULE_NONE if no broadcast
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
cognitive_module_t global_workspace_get_broadcast_source(
    const global_workspace_t* workspace
);

/**
 * @brief Get current broadcast strength
 *
 * WHAT: How strong was the winning signal?
 * WHY:  Assess confidence in broadcast
 * HOW:  Return strength from current broadcast
 *
 * @param workspace Global workspace handle
 * @return Strength (0-1), or 0.0 if no broadcast
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
float global_workspace_get_broadcast_strength(
    const global_workspace_t* workspace
);

/**
 * @brief Get number of current subscribers
 *
 * @param workspace Global workspace handle
 * @return Number of subscribed modules
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
uint32_t global_workspace_get_subscriber_count(global_workspace_t* workspace);

/**
 * @brief Get number of current competitors
 *
 * @param workspace Global workspace handle
 * @return Number of modules in competition pool
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
uint32_t global_workspace_get_competitor_count(global_workspace_t* workspace);

/**
 * @brief Check if module is currently competing
 *
 * @param workspace Global workspace handle
 * @param module Module to check
 * @return true if module has active competition entry
 *
 * COMPLEXITY: O(N) where N = competitors
 * THREAD-SAFE: Yes
 */
bool global_workspace_is_competing(
    const global_workspace_t* workspace,
    cognitive_module_t module
);

//=============================================================================
// History and Temporal Queries
//=============================================================================

/**
 * @brief Get broadcast history
 *
 * WHAT: Retrieve recent broadcast sequence
 * WHY:  Analyze temporal dynamics, recency effects
 * HOW:  Copy from circular history buffer
 *
 * @param workspace Global workspace handle
 * @param history Output array (caller allocates, at least max_history size)
 * @param max_history Maximum history entries to retrieve
 * @param actual_count Output: actual entries returned
 * @return true if history available
 *
 * COMPLEXITY: O(H) where H = history depth
 * THREAD-SAFE: Yes (read-only)
 * ORDER: Most recent first (history[0] = most recent broadcast)
 *
 * USAGE:
 * @code
 *   workspace_broadcast_t history[10];
 *   uint32_t count;
 *   if (global_workspace_get_history(workspace, history, 10, &count)) {
 *       printf("Last %u broadcasts:\n", count);
 *       for (uint32_t i = 0; i < count; i++) {
 *           printf("  %u: Module %d at time %lu\n",
 *                  i, history[i].source_module, history[i].broadcast_timestamp_ms);
 *       }
 *   }
 * @endcode
 */
bool global_workspace_get_history(
    const global_workspace_t* workspace,
    workspace_broadcast_t* history,
    uint32_t max_history,
    uint32_t* actual_count
);

/**
 * @brief Get time since last broadcast
 *
 * WHAT: How long since last workspace update?
 * WHY:  Detect stale workspace, check refractory period
 * HOW:  current_time - last_broadcast_time
 *
 * @param workspace Global workspace handle
 * @param current_time_ms Current time in milliseconds
 * @return Milliseconds since last broadcast, or UINT64_MAX if never broadcast
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
uint64_t global_workspace_time_since_broadcast(
    const global_workspace_t* workspace,
    uint64_t current_time_ms
);

//=============================================================================
// Statistics and Monitoring
//=============================================================================

/**
 * @brief Get workspace statistics
 *
 * WHAT: Retrieve performance and usage metrics
 * WHY:  Monitor workspace dynamics, debug competition, optimize thresholds
 * HOW:  Copy internal statistics structure
 *
 * @param workspace Global workspace handle
 * @param stats Output statistics structure
 * @return true if statistics available (enabled in config)
 *
 * COMPLEXITY: O(M) where M = number of modules (copy per-module stats)
 * THREAD-SAFE: Yes (read-only snapshot)
 *
 * USAGE:
 * @code
 *   workspace_statistics_t stats;
 *   if (global_workspace_get_statistics(workspace, &stats)) {
 *       printf("Total broadcasts: %lu\n", stats.total_broadcasts);
 *       printf("Working memory wins: %lu\n",
 *              stats.broadcasts_per_module[MODULE_WORKING_MEMORY]);
 *       printf("Avg competition time: %lu us\n",
 *              stats.avg_competition_latency_us);
 *   }
 * @endcode
 */
bool global_workspace_get_statistics(
    const global_workspace_t* workspace,
    workspace_statistics_t* stats
);

/**
 * @brief Reset workspace statistics
 *
 * WHAT: Clear all counters and metrics
 * WHY:  Start fresh measurement period
 * HOW:  Zero all statistics fields
 *
 * @param workspace Global workspace handle
 *
 * COMPLEXITY: O(M) where M = number of modules
 * THREAD-SAFE: No (modifies state)
 */
void global_workspace_reset_statistics(global_workspace_t* workspace);

/**
 * @brief Print workspace state for debugging
 *
 * WHAT: Human-readable dump of workspace state
 * WHY:  Debugging, monitoring, development
 * HOW:  Print broadcast, competitors, subscribers to stderr
 *
 * @param workspace Global workspace handle
 * @param verbose If true, print detailed info including content values
 *
 * COMPLEXITY: O(N + S) where N=competitors, S=subscribers
 * THREAD-SAFE: Yes (read-only)
 * OUTPUT: stderr
 *
 * EXAMPLE OUTPUT:
 * @code
 *   === Global Workspace State ===
 *   Broadcast: MODULE_WORKING_MEMORY (strength=0.85, dim=256)
 *   Competitors (3): MODULE_EXECUTIVE(0.72), MODULE_SALIENCE(0.68), MODULE_EMOTION(0.55)
 *   Subscribers (5): MODULE_WORKING_MEMORY, MODULE_EXECUTIVE, ...
 *   Ignition threshold: 0.60
 *   Total broadcasts: 1247
 * @endcode
 */
void global_workspace_print_state(
    const global_workspace_t* workspace,
    bool verbose
);

//=============================================================================
// Configuration and Tuning
//=============================================================================

/**
 * @brief Get default configuration
 *
 * WHAT: Sensible default parameters for typical use
 * WHY:  Convenient starting point, documented defaults
 * HOW:  Return struct with NIMCP standard values
 *
 * @return Default configuration
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (pure function)
 *
 * DEFAULTS:
 * - capacity_dim = 256 (matches working memory)
 * - strategy = WINNER_TAKE_ALL (most realistic)
 * - ignition_threshold = 0.6 (balanced)
 * - refractory_period_ms = 50 (attentional blink timescale)
 * - enable_statistics = true
 * - enable_history = true
 *
 * USAGE:
 * @code
 *   global_workspace_config_t cfg = global_workspace_default_config();
 *   // Modify as needed
 *   cfg.ignition_threshold = 0.7;  // Stricter threshold
 *   global_workspace_t* ws = global_workspace_create_custom(&cfg);
 * @endcode
 */
global_workspace_config_t global_workspace_default_config(void);

/**
 * @brief Update ignition threshold dynamically
 *
 * WHAT: Change threshold without recreating workspace
 * WHY:  Adaptive threshold based on performance
 * HOW:  Validate and update threshold value
 *
 * ADAPTIVE THRESHOLD:
 * - Too many broadcasts (workspace thrashing) → increase threshold
 * - Too few broadcasts (workspace underused) → decrease threshold
 * - Optimal: ~10-20 broadcasts per second
 *
 * @param workspace Global workspace handle
 * @param new_threshold New ignition threshold (0.0-1.0)
 * @return true if updated successfully
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: No (modifies state)
 * VALIDATION: Threshold clamped to [MIN_IGNITION, MAX_IGNITION]
 *
 * USAGE:
 * @code
 *   // Too many broadcasts - make it harder to get in
 *   if (broadcast_rate > 20) {
 *       float current = global_workspace_get_ignition_threshold(workspace);
 *       global_workspace_set_ignition_threshold(workspace, current + 0.05);
 *   }
 * @endcode
 */
bool global_workspace_set_ignition_threshold(
    global_workspace_t* workspace,
    float new_threshold
);

/**
 * @brief Get current ignition threshold
 *
 * @param workspace Global workspace handle
 * @return Current ignition threshold
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
float global_workspace_get_ignition_threshold(global_workspace_t* workspace);

/**
 * @brief Set module priority (for PRIORITY_BASED strategy)
 *
 * WHAT: Assign priority value to module
 * WHY:  Some modules are more important (e.g., safety, pain)
 * HOW:  Store priority, used in competition resolution
 *
 * PRIORITY VALUES:
 * - 1.0 = highest priority (safety-critical)
 * - 0.5 = normal priority (default)
 * - 0.1 = low priority (background)
 *
 * @param workspace Global workspace handle
 * @param module Which module to set priority for
 * @param priority Priority value (0.0-1.0)
 * @return true if set successfully
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: No
 * VALIDATION: Priority clamped to [0.0, 1.0]
 *
 * USAGE:
 * @code
 *   // Safety-related modules get highest priority
 *   global_workspace_set_module_priority(workspace, MODULE_WELLBEING, 1.0);
 *   global_workspace_set_module_priority(workspace, MODULE_EMOTION, 0.9);
 *   // Background modules get lower priority
 *   global_workspace_set_module_priority(workspace, MODULE_CONSOLIDATION, 0.2);
 * @endcode
 */
bool global_workspace_set_module_priority(
    global_workspace_t* workspace,
    cognitive_module_t module,
    float priority
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Convert module enum to string name
 *
 * WHAT: Human-readable module name
 * WHY:  Debugging, logging, UI display
 * HOW:  Lookup table
 *
 * @param module Module enum value
 * @return String name (e.g., "WORKING_MEMORY"), or "UNKNOWN" if invalid
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (const char*)
 * LIFETIME: Returned string is static (don't free)
 */
const char* cognitive_module_to_string(cognitive_module_t module);

/**
 * @brief Convert competition strategy enum to string
 *
 * @param strategy Competition strategy enum
 * @return String name (e.g., "WINNER_TAKE_ALL")
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
const char* competition_strategy_to_string(competition_strategy_t strategy);

/**
 * @brief Validate workspace configuration
 *
 * WHAT: Check if configuration is valid
 * WHY:  Catch invalid configs before creation
 * HOW:  Range checks on all parameters
 *
 * @param config Configuration to validate
 * @param error_msg Output buffer for error message (can be NULL)
 * @param error_msg_len Size of error message buffer
 * @return true if valid, false if invalid (error_msg contains reason)
 *
 * CHECKS:
 * - capacity_dim <= MAX_DIM
 * - ignition_threshold in valid range
 * - refractory_period >= 0
 * - history_depth <= MAX_HISTORY
 *
 * USAGE:
 * @code
 *   char error[256];
 *   if (!global_workspace_validate_config(&cfg, error, sizeof(error))) {
 *       fprintf(stderr, "Invalid config: %s\n", error);
 *       return -1;
 *   }
 * @endcode
 */
bool global_workspace_validate_config(
    const global_workspace_config_t* config,
    char* error_msg,
    size_t error_msg_len
);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_GLOBAL_WORKSPACE_H
