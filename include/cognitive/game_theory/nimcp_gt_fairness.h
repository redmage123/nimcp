//=============================================================================
// nimcp_gt_fairness.h - Enhanced Fairness Metrics for Resource Allocation
//=============================================================================
/**
 * @file nimcp_gt_fairness.h
 * @brief Enhanced fairness metrics for game-theoretic resource allocation
 *
 * WHAT: Multiple fairness measures and allocation property verification
 * WHY:  Evaluate and ensure fair division of resources in multi-agent systems
 * HOW:  Jain's index, Gini coefficient, envy-freeness, proportionality, MMS
 *
 * BIOLOGICAL INSPIRATION:
 * - Fair resource sharing in social insects (ant colonies, bee hives)
 * - Equitable neural activation distribution across brain regions
 * - Homeostatic mechanisms ensuring balanced resource allocation
 * - Immune system's proportional response to threats
 *
 * KEY CONCEPTS:
 * - Fairness Indices: Quantitative measures of distribution equality
 * - Envy-Freeness: No agent prefers another's allocation
 * - Proportionality: Each agent gets at least 1/n of total value
 * - Maximin Share: Guaranteed minimum under worst-case partitioning
 *
 * @author NIMCP Development Team
 * @date 2024-12-27
 * @version 1.0.0
 */

#ifndef NIMCP_GT_FAIRNESS_H
#define NIMCP_GT_FAIRNESS_H

#include "cognitive/game_theory/nimcp_game_theory.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Error Codes (Using Game Theory Range: 25000-25999)
//=============================================================================

#define NIMCP_GT_FAIRNESS_ERROR_BASE            25100
#define NIMCP_GT_ERROR_FAIRNESS_NULL_POINTER    (NIMCP_GT_FAIRNESS_ERROR_BASE + 1)
#define NIMCP_GT_ERROR_FAIRNESS_INVALID_PARAM   (NIMCP_GT_FAIRNESS_ERROR_BASE + 2)
#define NIMCP_GT_ERROR_FAIRNESS_NO_MEMORY       (NIMCP_GT_FAIRNESS_ERROR_BASE + 3)
#define NIMCP_GT_ERROR_FAIRNESS_EMPTY_INPUT     (NIMCP_GT_FAIRNESS_ERROR_BASE + 4)
#define NIMCP_GT_ERROR_FAIRNESS_INVALID_EPSILON (NIMCP_GT_FAIRNESS_ERROR_BASE + 5)
#define NIMCP_GT_ERROR_FAIRNESS_INVALID_ALLOC   (NIMCP_GT_FAIRNESS_ERROR_BASE + 6)
#define NIMCP_GT_ERROR_FAIRNESS_NO_IMPROVEMENT  (NIMCP_GT_FAIRNESS_ERROR_BASE + 7)

//=============================================================================
// Bio-Async Module IDs (Using Game Theory Range: 0x1500-0x150F)
//=============================================================================

#define BIO_MODULE_GT_FAIRNESS                  (BIO_MODULE_GAME_THEORY_BASE + 10)

//=============================================================================
// Tier-Scaled Constants
//=============================================================================

/** Maximum number of items for MMS computation (tier-scaled due to O(2^n)) */
#if NIMCP_BUILD_TIER == PLATFORM_TIER_FULL_VALUE
    #define NIMCP_FAIRNESS_MAX_MMS_ITEMS 20
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_MEDIUM
    #define NIMCP_FAIRNESS_MAX_MMS_ITEMS 15
#elif NIMCP_BUILD_TIER == PLATFORM_TIER_CONSTRAINED
    #define NIMCP_FAIRNESS_MAX_MMS_ITEMS 10
#else
    #define NIMCP_FAIRNESS_MAX_MMS_ITEMS 8
#endif

/** Maximum pairs to track for envy analysis */
#define NIMCP_FAIRNESS_MAX_ENVY_PAIRS 128

/** Default Atkinson epsilon (inequality aversion parameter) */
#define NIMCP_FAIRNESS_DEFAULT_ATKINSON_EPSILON 0.5f

//=============================================================================
// Fairness Measure Enumerations
//=============================================================================

/**
 * @brief Fairness index types
 *
 * WHAT: Different quantitative measures of distribution fairness
 * WHY:  Each captures different aspects of equality/inequality
 */
typedef enum {
    NIMCP_FAIRNESS_JAIN,              /**< Jain's fairness index [0,1] */
    NIMCP_FAIRNESS_GINI,              /**< Gini coefficient [0,1], lower=more equal */
    NIMCP_FAIRNESS_THEIL,             /**< Theil index (entropy-based) */
    NIMCP_FAIRNESS_ATKINSON,          /**< Atkinson index (inequality aversion) */
    NIMCP_FAIRNESS_COEFFICIENT_VARIATION, /**< Coefficient of variation */
    NIMCP_FAIRNESS_COUNT
} nimcp_fairness_measure_t;

/**
 * @brief Allocation property types
 *
 * WHAT: Qualitative properties of fair division
 * WHY:  Different allocation scenarios require different fairness guarantees
 */
typedef enum {
    NIMCP_ALLOC_ENVY_FREE,            /**< No player envies another's bundle */
    NIMCP_ALLOC_EF1,                  /**< Envy-free up to one item */
    NIMCP_ALLOC_EFX,                  /**< Envy-free up to any item */
    NIMCP_ALLOC_PROPORTIONAL,         /**< Each gets >= 1/n of own total value */
    NIMCP_ALLOC_MAXIMIN,              /**< Maximin share guarantee */
    NIMCP_ALLOC_PARETO_OPTIMAL,       /**< No Pareto improvement possible */
    NIMCP_ALLOC_PROPERTY_COUNT
} nimcp_allocation_property_t;

//=============================================================================
// Core Structures
//=============================================================================

/**
 * @brief Fairness computation configuration
 */
typedef struct {
    float atkinson_epsilon;           /**< Inequality aversion [0,1), higher=more averse */
    bool compute_all_measures;        /**< Compute all indices at once */
    bool track_envy_pairs;            /**< Track which pairs have envy */
    uint32_t max_mms_items;           /**< Max items for MMS (limits exponential) */
    float tolerance;                  /**< Numerical tolerance for comparisons */
} nimcp_fairness_config_t;

/**
 * @brief Item allocation to players
 *
 * WHAT: Represents assignment of items to agents
 * WHY:  Required for envy-freeness and proportionality checks
 */
typedef struct {
    uint32_t num_players;             /**< Number of players/agents */
    uint32_t num_items;               /**< Number of items to allocate */
    uint32_t* assignment;             /**< Item i assigned to assignment[i] */
    float** valuations;               /**< valuations[player][item] = value */
    float* bundle_values;             /**< Cached: each player's bundle value */
    bool values_cached;               /**< Are bundle values up to date? */
} nimcp_allocation_t;

/**
 * @brief Envy pair information
 *
 * WHAT: Records which player envies which
 * WHY:  Identify specific fairness violations to fix
 */
typedef struct {
    nimcp_player_id_t envying_player; /**< Player who envies */
    nimcp_player_id_t envied_player;  /**< Player being envied */
    float envy_amount;                /**< How much more envier values envied's bundle */
    uint32_t blocking_item;           /**< Item that if removed makes EF1 (0xFFFFFFFF if none) */
} nimcp_envy_pair_t;

/**
 * @brief Comprehensive fairness result
 */
typedef struct {
    // Fairness indices
    float jain_index;                 /**< Jain's index [0,1], 1=perfectly fair */
    float gini_coefficient;           /**< Gini [0,1], 0=perfectly equal */
    float theil_index;                /**< Theil index, 0=perfectly equal */
    float atkinson_index;             /**< Atkinson [0,1], 0=perfectly equal */
    float coefficient_of_variation;   /**< CV = stddev/mean */

    // Allocation properties (bit flags)
    bool is_envy_free;                /**< True if no envy exists */
    bool is_ef1;                      /**< True if envy-free up to one item */
    bool is_efx;                      /**< True if envy-free up to any item */
    bool is_proportional;             /**< True if each gets >= 1/n value */
    bool has_mms_guarantee;           /**< True if MMS satisfied for all */
    bool is_pareto_optimal;           /**< True if Pareto optimal */

    // Envy analysis
    uint32_t num_envy_pairs;          /**< Number of envy relationships */
    float max_envy;                   /**< Maximum envy amount */
    float total_envy;                 /**< Sum of all envy amounts */

    // Proportionality analysis
    float min_proportional_share;     /**< Minimum share any player got */
    nimcp_player_id_t worst_off_player; /**< Player with lowest share */

    // MMS analysis
    float* maximin_shares;            /**< MMS value per player (allocated) */
    float min_mms_ratio;              /**< Lowest (value/MMS) ratio */
} nimcp_fairness_result_t;

//=============================================================================
// Configuration Functions
//=============================================================================

/**
 * @brief Get default fairness configuration
 *
 * @return Default configuration with sensible defaults
 */
nimcp_fairness_config_t nimcp_fairness_default_config(void);

//=============================================================================
// Basic Fairness Index Functions
//=============================================================================

/**
 * @brief Compute Jain's fairness index
 *
 * WHAT: Measure how evenly resources are distributed
 * WHY:  Standard fairness metric from networking, [0,1] bounded
 * HOW:  J = (sum x_i)^2 / (n * sum x_i^2)
 *
 * PROPERTIES:
 * - Returns 1.0 for perfectly equal distribution
 * - Returns 1/n for maximally unfair (one gets all)
 * - Bounded in [1/n, 1]
 *
 * @param values Array of allocation values
 * @param num_values Number of values
 * @return Jain's index [1/n, 1], or -1.0f on error
 */
float nimcp_fairness_jain_index(const float* values, uint32_t num_values);

/**
 * @brief Compute Gini coefficient
 *
 * WHAT: Measure inequality in distribution
 * WHY:  Standard economics metric, captures relative inequality
 * HOW:  G = sum|x_i - x_j| / (2 * n * sum x_i)
 *
 * PROPERTIES:
 * - Returns 0.0 for perfectly equal distribution
 * - Returns 1.0 for maximally unequal (one gets all)
 * - Lorenz curve based interpretation
 *
 * @param values Array of allocation values
 * @param num_values Number of values
 * @return Gini coefficient [0, 1], or -1.0f on error
 */
float nimcp_fairness_gini_coefficient(const float* values, uint32_t num_values);

/**
 * @brief Compute Theil index (entropy-based inequality)
 *
 * WHAT: Information-theoretic inequality measure
 * WHY:  Decomposes into within-group and between-group inequality
 * HOW:  T = (1/n) * sum (x_i/mu) * ln(x_i/mu)
 *
 * PROPERTIES:
 * - Returns 0.0 for perfectly equal distribution
 * - Unbounded above (unlike Gini)
 * - Additive decomposability
 *
 * @param values Array of allocation values (must be positive)
 * @param num_values Number of values
 * @return Theil index [0, inf), or -1.0f on error
 */
float nimcp_fairness_theil_index(const float* values, uint32_t num_values);

/**
 * @brief Compute Atkinson index
 *
 * WHAT: Normative inequality measure with sensitivity parameter
 * WHY:  Allows weighting how much we care about inequality
 * HOW:  A = 1 - (1/mu) * (mean of x_i^(1-e))^(1/(1-e))
 *
 * PROPERTIES:
 * - Returns 0.0 for perfectly equal distribution
 * - Returns 1.0 for maximally unequal
 * - epsilon controls inequality aversion (higher = more averse)
 *
 * @param values Array of allocation values (must be positive)
 * @param num_values Number of values
 * @param epsilon Inequality aversion parameter [0, 1)
 * @return Atkinson index [0, 1], or -1.0f on error
 */
float nimcp_fairness_atkinson_index(const float* values, uint32_t num_values,
                                     float epsilon);

/**
 * @brief Compute coefficient of variation
 *
 * WHAT: Standard deviation relative to mean
 * WHY:  Scale-independent measure of dispersion
 * HOW:  CV = stddev / mean
 *
 * @param values Array of allocation values
 * @param num_values Number of values
 * @return CV >= 0, or -1.0f on error
 */
float nimcp_fairness_coefficient_variation(const float* values, uint32_t num_values);

//=============================================================================
// Allocation Property Verification
//=============================================================================

/**
 * @brief Check if allocation is envy-free
 *
 * WHAT: Verify no player prefers another's bundle
 * WHY:  Fundamental fairness criterion in fair division
 * HOW:  For all i,j: value_i(bundle_i) >= value_i(bundle_j)
 *
 * @param valuations 2D array [num_players][num_items] of valuations
 * @param assignment Array mapping item -> player
 * @param num_players Number of players
 * @param num_items Number of items
 * @return true if envy-free
 */
bool nimcp_fairness_is_envy_free(const float* const* valuations,
                                  const uint32_t* assignment,
                                  uint32_t num_players,
                                  uint32_t num_items);

/**
 * @brief Check if allocation is EF1 (envy-free up to one item)
 *
 * WHAT: Verify envy can be eliminated by removing one item
 * WHY:  Achievable for indivisible goods unlike full envy-freeness
 * HOW:  For all i,j: exists item g in j's bundle s.t.
 *       value_i(bundle_i) >= value_i(bundle_j - {g})
 *
 * @param valuations 2D array [num_players][num_items] of valuations
 * @param assignment Array mapping item -> player
 * @param num_players Number of players
 * @param num_items Number of items
 * @return true if EF1
 */
bool nimcp_fairness_is_ef1(const float* const* valuations,
                            const uint32_t* assignment,
                            uint32_t num_players,
                            uint32_t num_items);

/**
 * @brief Check if allocation is EFX (envy-free up to any item)
 *
 * WHAT: Verify envy can be eliminated by removing any envied item
 * WHY:  Stronger than EF1, existence for 3+ players still open
 * HOW:  For all i,j: for all items g in j's bundle with v_i(g) > 0:
 *       value_i(bundle_i) >= value_i(bundle_j - {g})
 *
 * @param valuations 2D array [num_players][num_items] of valuations
 * @param assignment Array mapping item -> player
 * @param num_players Number of players
 * @param num_items Number of items
 * @return true if EFX
 */
bool nimcp_fairness_is_efx(const float* const* valuations,
                            const uint32_t* assignment,
                            uint32_t num_players,
                            uint32_t num_items);

/**
 * @brief Check if allocation is proportional
 *
 * WHAT: Verify each player gets >= 1/n of their total value
 * WHY:  Basic fairness guarantee for divisible goods
 * HOW:  For all i: value_i(bundle_i) >= (1/n) * value_i(all items)
 *
 * @param valuations 2D array [num_players][num_items] of valuations
 * @param assignment Array mapping item -> player
 * @param num_players Number of players
 * @param num_items Number of items
 * @return true if proportional
 */
bool nimcp_fairness_is_proportional(const float* const* valuations,
                                     const uint32_t* assignment,
                                     uint32_t num_players,
                                     uint32_t num_items);

//=============================================================================
// Maximin Share Functions
//=============================================================================

/**
 * @brief Compute maximin share for a player
 *
 * WHAT: Find guaranteed value under worst-case n-way partition
 * WHY:  Lower bound on what player should accept as fair
 * HOW:  MMS_i = max over partitions P: min over bundles B in P: value_i(B)
 *
 * COMPLEXITY: O(n^m) where m = num_items - exponential, use with care
 *
 * @param valuations 2D array [num_players][num_items] of valuations
 * @param player Player index to compute MMS for
 * @param num_players Number of players
 * @param num_items Number of items
 * @return MMS value for player, or -1.0f on error
 */
float nimcp_fairness_maximin_share(const float* const* valuations,
                                    uint32_t player,
                                    uint32_t num_players,
                                    uint32_t num_items);

/**
 * @brief Check if allocation provides MMS guarantee
 *
 * WHAT: Verify each player gets at least their MMS
 * WHY:  MMS is considered a fair share guarantee
 * HOW:  For all i: value_i(bundle_i) >= MMS_i
 *
 * @param valuations 2D array [num_players][num_items] of valuations
 * @param assignment Array mapping item -> player
 * @param num_players Number of players
 * @param num_items Number of items
 * @return true if MMS guaranteed for all
 */
bool nimcp_fairness_has_mms_guarantee(const float* const* valuations,
                                       const uint32_t* assignment,
                                       uint32_t num_players,
                                       uint32_t num_items);

//=============================================================================
// Comprehensive Analysis Functions
//=============================================================================

/**
 * @brief Compute all fairness measures at once
 *
 * WHAT: Compute all fairness indices for a value distribution
 * WHY:  Efficiency when multiple measures needed
 * HOW:  Single pass computation where possible
 *
 * @param values Array of allocation values
 * @param num_values Number of values
 * @param config Configuration (NULL for defaults)
 * @param result Output result structure
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_fairness_compute_all(const float* values,
                                          uint32_t num_values,
                                          const nimcp_fairness_config_t* config,
                                          nimcp_fairness_result_t* result);

/**
 * @brief Analyze allocation comprehensively
 *
 * WHAT: Check all allocation properties and compute metrics
 * WHY:  Complete fairness analysis in one call
 * HOW:  Runs all verification functions and computes indices
 *
 * @param allocation Allocation structure
 * @param config Configuration (NULL for defaults)
 * @param result Output result structure
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_fairness_analyze_allocation(const nimcp_allocation_t* allocation,
                                                 const nimcp_fairness_config_t* config,
                                                 nimcp_fairness_result_t* result);

/**
 * @brief Find all envious pairs in allocation
 *
 * WHAT: Identify which players envy which
 * WHY:  Understand specific fairness violations
 * HOW:  Check all pairs for envy relationship
 *
 * @param valuations 2D array [num_players][num_items] of valuations
 * @param assignment Array mapping item -> player
 * @param num_players Number of players
 * @param num_items Number of items
 * @param pairs Output array of envy pairs
 * @param max_pairs Maximum pairs to return
 * @param num_found Output: actual number found
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_fairness_find_envious_pairs(const float* const* valuations,
                                                 const uint32_t* assignment,
                                                 uint32_t num_players,
                                                 uint32_t num_items,
                                                 nimcp_envy_pair_t* pairs,
                                                 uint32_t max_pairs,
                                                 uint32_t* num_found);

//=============================================================================
// Allocation Improvement Functions
//=============================================================================

/**
 * @brief Find Pareto improvement if one exists
 *
 * WHAT: Find reallocation that makes someone better without hurting others
 * WHY:  Move toward Pareto optimal allocation
 * HOW:  Search for beneficial item swaps
 *
 * @param allocation Current allocation (modified in place if improved)
 * @param valuations 2D array [num_players][num_items] of valuations
 * @param improved_allocation Output allocation (can be same as input)
 * @return NIMCP_SUCCESS if improved, NIMCP_GT_ERROR_FAIRNESS_NO_IMPROVEMENT if already optimal
 */
nimcp_error_t nimcp_fairness_pareto_improve(const nimcp_allocation_t* allocation,
                                             const float* const* valuations,
                                             nimcp_allocation_t* improved_allocation);

/**
 * @brief Attempt to reduce envy through swaps
 *
 * WHAT: Try to reduce total envy by swapping items
 * WHY:  Move toward more envy-free allocation
 * HOW:  Greedy search for envy-reducing swaps
 *
 * @param allocation Current allocation
 * @param valuations 2D array [num_players][num_items] of valuations
 * @param improved_allocation Output allocation
 * @return NIMCP_SUCCESS if improved, NIMCP_GT_ERROR_FAIRNESS_NO_IMPROVEMENT otherwise
 */
nimcp_error_t nimcp_fairness_reduce_envy(const nimcp_allocation_t* allocation,
                                          const float* const* valuations,
                                          nimcp_allocation_t* improved_allocation);

//=============================================================================
// Allocation Structure Functions
//=============================================================================

/**
 * @brief Create an allocation structure
 *
 * @param num_players Number of players
 * @param num_items Number of items
 * @return Allocated structure or NULL on failure
 */
nimcp_allocation_t* nimcp_allocation_create(uint32_t num_players, uint32_t num_items);

/**
 * @brief Destroy an allocation structure
 *
 * @param allocation Allocation to destroy
 */
void nimcp_allocation_destroy(nimcp_allocation_t* allocation);

/**
 * @brief Set item valuations for a player
 *
 * @param allocation Allocation structure
 * @param player Player index
 * @param valuations Array of num_items valuations
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_allocation_set_valuations(nimcp_allocation_t* allocation,
                                               uint32_t player,
                                               const float* valuations);

/**
 * @brief Set item assignment
 *
 * @param allocation Allocation structure
 * @param item Item index
 * @param player Player to assign item to
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_allocation_assign_item(nimcp_allocation_t* allocation,
                                            uint32_t item,
                                            uint32_t player);

/**
 * @brief Compute bundle values (cache them)
 *
 * @param allocation Allocation structure
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_allocation_compute_bundle_values(nimcp_allocation_t* allocation);

/**
 * @brief Copy an allocation structure
 *
 * @param src Source allocation
 * @param dst Destination allocation (must be pre-allocated with same dimensions)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_allocation_copy(const nimcp_allocation_t* src,
                                     nimcp_allocation_t* dst);

//=============================================================================
// Result Structure Functions
//=============================================================================

/**
 * @brief Initialize fairness result structure
 *
 * @param result Result to initialize
 * @param num_players Number of players (for MMS array)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_fairness_result_init(nimcp_fairness_result_t* result,
                                          uint32_t num_players);

/**
 * @brief Cleanup fairness result structure
 *
 * @param result Result to cleanup
 */
void nimcp_fairness_result_cleanup(nimcp_fairness_result_t* result);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get fairness measure name
 *
 * @param measure Fairness measure type
 * @return Human-readable name
 */
const char* nimcp_fairness_measure_name(nimcp_fairness_measure_t measure);

/**
 * @brief Get allocation property name
 *
 * @param property Allocation property type
 * @return Human-readable name
 */
const char* nimcp_allocation_property_name(nimcp_allocation_property_t property);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_GT_FAIRNESS_H
