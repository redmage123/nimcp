/**
 * @file nimcp_cortical_layers.h
 * @brief Cortical layer organization and canonical microcircuit implementation
 *
 * WHAT: Six-layer cortical architecture with layer-specific processing
 * WHY:  Biological cortex organizes into laminar structures with distinct
 *       computational roles (Douglas & Martin 1991, Thomson & Bannister 2003)
 * HOW:  Implements layer-specific dynamics, inter-layer connectivity patterns,
 *       and canonical microcircuit processing
 *
 * BIOLOGICAL MODEL:
 * Layer I    (5%):  Apical dendrites, top-down modulation
 * Layer II/III (40%): Cortico-cortical projections, lateral integration
 * Layer IV   (15%): Thalamic input, feedforward processing
 * Layer V    (20%): Subcortical output, decision signals
 * Layer VI   (20%): Corticothalamic feedback, predictive coding
 *
 * CANONICAL MICROCIRCUIT (Douglas & Martin 1991):
 * - Thalamus → Layer IV (feedforward input)
 * - Layer IV → Layer II/III → Layer V (excitatory chain)
 * - Layer V → Layer VI (output pathway)
 * - Layer VI → Layer IV (modulatory feedback)
 * - Layer I modulates Layer II/III (top-down attention)
 * - Inhibitory interneurons in each layer (normalization)
 *
 * MATHEMATICAL ALGORITHMS:
 * - Layer IV: Divisive normalization r = c^n / (σ^n + Σc_j^n)
 * - Layer II/III: Recurrent dynamics τ·dx/dt = -x + f(Wx + I)
 * - Layer V: Izhikevich bursting dynamics
 * - Layer VI: Predictive coding e = actual - predicted
 *
 * REFERENCES:
 * - Douglas & Martin (1991) "A functional microcircuit for cat visual cortex"
 * - Thomson & Bannister (2003) "Interlaminar connections in the neocortex"
 * - Bastos et al. (2012) "Canonical microcircuits for predictive coding"
 *
 * @author NIMCP Development Team
 * @date 2025-11-25
 */

#ifndef NIMCP_CORTICAL_LAYERS_H
#define NIMCP_CORTICAL_LAYERS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Layer Type Enumeration
//=============================================================================

/**
 * WHAT: Cortical layer identifiers (5-layer condensed model)
 * WHY:  Distinguish cortical layers with distinct biological roles
 *       Uses combined II/III layer (common in computational literature)
 * HOW:  Enum mapping to layers I, II/III, IV, V, VI
 * NOTE: Prefixed with CC_ to avoid conflicts with brain_regions.h
 *       (which uses full 6-layer model)
 */
typedef enum {
    CC_LAYER_I = 0,         /**< Apical dendrites, feedback modulation */
    CC_LAYER_II_III = 1,    /**< Lateral association, feature binding */
    CC_LAYER_IV = 2,        /**< Thalamic input, feature detection */
    CC_LAYER_V = 3,         /**< Subcortical output, action commands */
    CC_LAYER_VI = 4,        /**< Corticothalamic feedback, prediction */
    CC_LAYER_COUNT = 5      /**< Total number of layers (condensed model) */
} cc_cortical_layer_t;

// Compatibility aliases for older code using CORTICAL_LAYER_* naming
// Note: These use CC_ prefix to avoid conflicts with brain_regions.h
#define CORTICAL_LAYER_I       CC_LAYER_I
#define CORTICAL_LAYER_II_III  CC_LAYER_II_III
#define CORTICAL_LAYER_IV      CC_LAYER_IV
#define CORTICAL_LAYER_V       CC_LAYER_V
#define CORTICAL_LAYER_VI      CC_LAYER_VI
#define CORTICAL_LAYER_COUNT   CC_LAYER_COUNT

//=============================================================================
// Configuration Structures
//=============================================================================

/**
 * WHAT: Layer-specific configuration parameters
 * WHY:  Each layer has different neuronal density, connectivity, and E/I ratio
 * HOW:  Stores biological parameters for each layer type
 *
 * INVARIANTS:
 * - 0.0 < thickness_ratio <= 1.0
 * - neuron_density > 0
 * - 0.0 < excitatory_ratio <= 1.0
 * - 0.0 <= default_connectivity <= 1.0
 */
typedef struct cortical_layer_config {
    cc_cortical_layer_t layer;          /**< Layer identifier */
    float thickness_ratio;           /**< Fraction of total cortical depth */
    uint32_t neuron_density;         /**< Neurons per unit volume */
    float excitatory_ratio;          /**< Fraction excitatory neurons */
    float default_connectivity;      /**< Base connection probability */
} cortical_layer_config_t;

/**
 * WHAT: Laminar activity profile snapshot
 * WHY:  Track activation patterns across cortical depth
 * HOW:  Arrays indexed by layer containing activation levels
 *
 * USAGE: Use for visualization, analysis, and feedback control
 */
typedef struct laminar_profile {
    float layer_activations[CC_LAYER_COUNT];  /**< Current activation */
    float layer_inputs[CC_LAYER_COUNT];       /**< Input currents */
    float layer_outputs[CC_LAYER_COUNT];      /**< Output signals */
    uint64_t timestamp;                             /**< Profile capture time */
} laminar_profile_t;

/**
 * WHAT: Laminar structure statistics
 * WHY:  Monitor layer health and processing efficiency
 * HOW:  Aggregated metrics across layers
 */
typedef struct laminar_stats {
    float mean_activation[CC_LAYER_COUNT];    /**< Average activation */
    float variance_activation[CC_LAYER_COUNT]; /**< Activation variance */
    float total_feedforward_flow;                    /**< FF information flow */
    float total_feedback_flow;                       /**< FB information flow */
    float prediction_error;                          /**< Layer VI error signal */
    uint64_t update_count;                           /**< Total updates */
} laminar_stats_t;

//=============================================================================
// Opaque Structure (Implementation hidden)
//=============================================================================

/**
 * WHAT: Laminar structure handle (opaque)
 * WHY:  Encapsulation - hide internal implementation details
 * HOW:  Forward declaration, definition in .c file
 */
typedef struct laminar_structure laminar_structure_t;

//=============================================================================
// Layer Configuration API
//=============================================================================

/**
 * WHAT: Get default configuration for a cortical layer
 * WHY:  Provide biologically realistic starting parameters
 * HOW:  Return preset configuration based on layer type
 *
 * LAYER DEFAULTS (from Thomson & Bannister 2003):
 * - Layer I:    5% thickness, low density, 60% excitatory, 10% connectivity
 * - Layer II/III: 40% thickness, high density, 80% excitatory, 30% connectivity
 * - Layer IV:   15% thickness, highest density, 85% excitatory, 40% connectivity
 * - Layer V:    20% thickness, medium density, 75% excitatory, 25% connectivity
 * - Layer VI:   20% thickness, medium density, 70% excitatory, 20% connectivity
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (read-only)
 *
 * @param layer Layer type to get configuration for
 * @return Default configuration structure
 */
cortical_layer_config_t cortical_layer_get_default_config(cc_cortical_layer_t layer);

/**
 * WHAT: Set custom configuration for a layer
 * WHY:  Allow parameter tuning for specific applications
 * HOW:  Validates and copies configuration parameters
 *
 * VALIDATION:
 * - Checks parameter ranges
 * - Logs warnings for unusual values
 * - Clamps to valid ranges if needed
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: No (caller must synchronize)
 *
 * @param config Configuration structure to set (must be non-NULL)
 * @param layer Layer to apply configuration to
 */
void cortical_layer_set_config(cortical_layer_config_t* config, cc_cortical_layer_t layer);

/**
 * WHAT: Get layer name as string
 * WHY:  Debugging, logging, and UI display
 * HOW:  Static string lookup
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (read-only)
 *
 * @param layer Layer type
 * @return Layer name (e.g., "Layer I", "Layer II/III")
 */
const char* cortical_layer_get_name(cc_cortical_layer_t layer);

/**
 * WHAT: Get layer description
 * WHY:  Educational and documentation purposes
 * HOW:  Static string lookup
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (read-only)
 *
 * @param layer Layer type
 * @return Description of layer's biological role
 */
const char* cortical_layer_get_description(cc_cortical_layer_t layer);

//=============================================================================
// Laminar Structure Lifecycle
//=============================================================================

/**
 * WHAT: Create laminar structure with 6 cortical layers
 * WHY:  Initialize complete cortical column architecture
 * HOW:  Allocates memory, initializes layer dynamics, sets up connectivity
 *
 * ALLOCATION:
 * - Uses nimcp_malloc for thread-safe allocation
 * - Initializes mutex for concurrent access
 * - Sets up internal state arrays
 *
 * COMPLEXITY: O(L + C) where L = layers, C = connections
 * THREAD-SAFE: Yes (returns isolated instance)
 *
 * @param configs Array of 5 layer configurations (indexed by cc_cortical_layer_t)
 *                If NULL, uses default configurations
 * @return Pointer to laminar structure, or NULL on failure
 */
laminar_structure_t* laminar_structure_create(
    const cortical_layer_config_t configs[CC_LAYER_COUNT]
);

/**
 * WHAT: Destroy laminar structure and free resources
 * WHY:  Clean up memory when structure no longer needed
 * HOW:  Releases all internal arrays, destroys mutex, frees handle
 *
 * SAFETY:
 * - Guard clause: NULL pointer safe (no-op)
 * - Destroys mutex before freeing
 * - Uses nimcp_free for tracked deallocation
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: No (caller must ensure no concurrent access)
 *
 * @param ls Laminar structure to destroy (NULL safe)
 */
void laminar_structure_destroy(laminar_structure_t* ls);

//=============================================================================
// Layer Input Processing
//=============================================================================

/**
 * WHAT: Process input to a specific cortical layer
 * WHY:  External stimuli target specific layers (e.g., thalamus → Layer IV)
 * HOW:  Accumulates input into layer's buffer for next update cycle
 *
 * USAGE:
 * - Thalamic input → Layer IV
 * - Cortical feedback → Layer I
 * - Lateral input → Layer II/III
 *
 * COMPLEXITY: O(N) where N = input size
 * THREAD-SAFE: Yes (mutex-protected)
 *
 * @param ls Laminar structure (must be non-NULL)
 * @param target Target layer to receive input
 * @param input Input vector (must be non-NULL)
 * @param size Number of elements in input vector
 */
void laminar_process_input(
    laminar_structure_t* ls,
    cc_cortical_layer_t target,
    const float* input,
    uint32_t size
);

//=============================================================================
// Inter-Layer Processing Pipelines
//=============================================================================

/**
 * WHAT: Process feedforward information flow (IV → II/III → V)
 * WHY:  Implements bottom-up processing from sensory input to motor output
 * HOW:  Sequential layer updates following canonical microcircuit
 *
 * ALGORITHM:
 * 1. Layer IV: Divisive normalization of thalamic input
 * 2. Layer IV → Layer II/III: Excitatory projection
 * 3. Layer II/III: Recurrent lateral integration
 * 4. Layer II/III → Layer V: Excitatory projection
 * 5. Layer V: Threshold and burst detection
 *
 * COMPLEXITY: O(N) where N = neurons per layer
 * THREAD-SAFE: Yes (mutex-protected)
 *
 * @param ls Laminar structure (must be non-NULL)
 */
void laminar_process_feedforward(laminar_structure_t* ls);

/**
 * WHAT: Process feedback information flow (VI → IV, I → II/III)
 * WHY:  Implements top-down predictions and attentional modulation
 * HOW:  Backward projections from higher to lower layers
 *
 * ALGORITHM:
 * 1. Layer VI: Compute prediction for Layer IV
 * 2. Layer VI → Layer IV: Modulatory feedback (gain control)
 * 3. Layer I: Top-down attentional signal
 * 4. Layer I → Layer II/III: Multiplicative modulation
 * 5. Compute prediction error in Layer VI
 *
 * COMPLEXITY: O(N) where N = neurons per layer
 * THREAD-SAFE: Yes (mutex-protected)
 *
 * @param ls Laminar structure (must be non-NULL)
 */
void laminar_process_feedback(laminar_structure_t* ls);

/**
 * WHAT: Process lateral connections within Layer II/III
 * WHY:  Implement recurrent dynamics for feature binding and integration
 * HOW:  Updates recurrent network dynamics within Layer II/III
 *
 * ALGORITHM:
 * τ·dx/dt = -x + f(Wx + I)
 * where:
 * - τ = time constant (10ms typical)
 * - x = layer activation state
 * - W = lateral weight matrix
 * - I = external input
 * - f = sigmoid activation function
 *
 * COMPLEXITY: O(N²) for fully connected, O(N) for sparse
 * THREAD-SAFE: Yes (mutex-protected)
 *
 * @param ls Laminar structure (must be non-NULL)
 */
void laminar_process_lateral(laminar_structure_t* ls);

//=============================================================================
// Layer Output and State Access
//=============================================================================

/**
 * WHAT: Get output from a specific cortical layer
 * WHY:  Read layer activation for downstream processing or analysis
 * HOW:  Copies current layer state to output buffer
 *
 * USAGE:
 * - Layer V output → motor commands
 * - Layer II/III output → higher cortical areas
 * - Layer VI output → thalamus
 *
 * COMPLEXITY: O(N) where N = output size
 * THREAD-SAFE: Yes (mutex-protected read)
 *
 * @param ls Laminar structure (must be non-NULL)
 * @param layer Layer to read from
 * @param output Output buffer (must be pre-allocated, non-NULL)
 * @param size Number of elements to copy
 */
void laminar_get_output(
    laminar_structure_t* ls,
    cc_cortical_layer_t layer,
    float* output,
    uint32_t size
);

/**
 * WHAT: Get mean activation level of a layer
 * WHY:  Quick scalar metric for layer activity
 * HOW:  Returns average of all neurons in layer
 *
 * COMPLEXITY: O(1) (pre-computed)
 * THREAD-SAFE: Yes (mutex-protected read)
 *
 * @param ls Laminar structure (must be non-NULL)
 * @param layer Layer to query
 * @return Mean activation level (0.0 to 1.0 typically)
 */
float laminar_get_layer_activation(
    laminar_structure_t* ls,
    cc_cortical_layer_t layer
);

/**
 * WHAT: Get number of neurons in a specific layer
 * WHY:  Allow external modules to query layer size for connectivity generation
 * HOW:  Returns neuron_count from layer_state_t
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (returns immutable value after initialization)
 *
 * @param ls Laminar structure (must be non-NULL)
 * @param layer Layer to query
 * @return Number of neurons in the layer (0 if invalid)
 */
uint32_t laminar_get_layer_neuron_count(
    const laminar_structure_t* ls,
    cc_cortical_layer_t layer
);

//=============================================================================
// Inter-Layer Connectivity Configuration
//=============================================================================

/**
 * WHAT: Configure feedforward connection strength between layers
 * WHY:  Adjust canonical microcircuit weights for specific tasks
 * HOW:  Sets synaptic strength multiplier for layer-to-layer projection
 *
 * TYPICAL VALUES:
 * - Layer IV → Layer II/III: 1.0 (strong excitatory)
 * - Layer II/III → Layer V: 0.8 (moderate excitatory)
 * - Layer V → Layer VI: 0.6 (weak excitatory)
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (mutex-protected)
 *
 * @param ls Laminar structure (must be non-NULL)
 * @param from Source layer
 * @param to Target layer
 * @param strength Connection strength (0.0 to 1.0+, typically)
 */
void laminar_connect_feedforward(
    laminar_structure_t* ls,
    cc_cortical_layer_t from,
    cc_cortical_layer_t to,
    float strength
);

/**
 * WHAT: Configure feedback connection strength between layers
 * WHY:  Adjust predictive coding and attentional modulation strength
 * HOW:  Sets synaptic strength for backward projections
 *
 * TYPICAL VALUES:
 * - Layer VI → Layer IV: 0.5 (modulatory)
 * - Layer I → Layer II/III: 0.4 (attentional gain)
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (mutex-protected)
 *
 * @param ls Laminar structure (must be non-NULL)
 * @param from Source layer (higher)
 * @param to Target layer (lower)
 * @param strength Connection strength (0.0 to 1.0, typically)
 */
void laminar_connect_feedback(
    laminar_structure_t* ls,
    cc_cortical_layer_t from,
    cc_cortical_layer_t to,
    float strength
);

//=============================================================================
// Canonical Microcircuit
//=============================================================================

/**
 * WHAT: Apply canonical cortical microcircuit connectivity pattern
 * WHY:  Set up standard mammalian cortex wiring diagram automatically
 * HOW:  Configures all inter-layer connections per Douglas & Martin 1991
 *
 * CONNECTIVITY PATTERN:
 * - Thalamus → Layer IV (external input, not set here)
 * - Layer IV → Layer II/III (strength: 1.0)
 * - Layer II/III → Layer V (strength: 0.8)
 * - Layer II/III → Layer II/III (recurrent, strength: 0.6)
 * - Layer V → Layer VI (strength: 0.7)
 * - Layer VI → Layer IV (feedback, strength: 0.5)
 * - Layer I → Layer II/III (feedback, strength: 0.4)
 * - Inhibitory connections in all layers (automatic)
 *
 * COMPLEXITY: O(L²) where L = number of layers (constant = 5)
 * THREAD-SAFE: Yes (mutex-protected)
 *
 * @param ls Laminar structure (must be non-NULL)
 */
void laminar_apply_canonical_circuit(laminar_structure_t* ls);

//=============================================================================
// Statistics and Analysis
//=============================================================================

/**
 * WHAT: Get current laminar activity profile
 * WHY:  Snapshot of layer states for analysis and visualization
 * HOW:  Copies current activations, inputs, outputs to profile structure
 *
 * COMPLEXITY: O(L) where L = number of layers
 * THREAD-SAFE: Yes (mutex-protected read)
 *
 * @param ls Laminar structure (must be non-NULL)
 * @param profile Output profile structure (must be non-NULL)
 */
void laminar_get_profile(
    laminar_structure_t* ls,
    laminar_profile_t* profile
);

/**
 * WHAT: Get laminar structure statistics
 * WHY:  Monitor processing efficiency and layer health
 * HOW:  Computes mean, variance, and flow metrics across layers
 *
 * METRICS:
 * - Mean activation per layer (average firing rate)
 * - Variance per layer (response variability)
 * - Feedforward flow (total IV → V information)
 * - Feedback flow (total VI → IV + I → II/III information)
 * - Prediction error (VI error signal)
 *
 * COMPLEXITY: O(N·L) where N = neurons/layer, L = layers
 * THREAD-SAFE: Yes (mutex-protected)
 *
 * @param ls Laminar structure (must be non-NULL)
 * @param stats Output statistics structure (must be non-NULL)
 */
void laminar_get_stats(
    laminar_structure_t* ls,
    laminar_stats_t* stats
);

#ifdef __cplusplus
}
#endif

#endif  // NIMCP_CORTICAL_LAYERS_H
