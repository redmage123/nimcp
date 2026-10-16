/**
 * @file nimcp.hpp
 * @brief Modern C++17 wrapper for NIMCP v2.7.0
 * @version 2.7.0
 * @date 2025-11-05
 *
 * WHAT: Object-oriented C++ interface to NIMCP unified API
 * WHY:  Provide idiomatic C++ with RAII, exceptions, type safety
 * HOW:  Wrapper classes around C API with smart pointers and move semantics
 *
 * FEATURES:
 * - RAII resource management (automatic cleanup)
 * - Exception-based error handling
 * - Move semantics (no copies, efficient transfers)
 * - Type-safe enum classes
 * - STL containers (vector, string)
 * - v2.7.0 enhancements: batch, async, checkpointing, SIMD
 *
 * CODING STANDARDS:
 * - WHAT/WHY/HOW documentation
 * - const-correctness
 * - noexcept specifications where appropriate
 * - Rule of Five for resource-managing classes
 * - Modern C++17 features (structured bindings, std::optional)
 *
 * EXAMPLE USAGE:
 * ```cpp
 * #include <nimcp.hpp>
 * using namespace nimcp;
 *
 * // Create and train brain
 * Brain brain("classifier", BrainSize::Small, TaskType::Classification, 10, 5);
 * float loss = brain.learn({0.1f, 0.2f, 0.3f}, "class_a", 0.9f);
 * auto [label, confidence] = brain.predict({0.15f, 0.25f, 0.35f});
 *
 * // Batch processing
 * auto predictions = brain.predictBatch({{0.1f, 0.2f}, {0.3f, 0.4f}});
 *
 * // Checkpointing
 * brain.enableCheckpointing("/tmp/checkpoints");
 * brain.checkpoint("manual_save");
 * ```
 */

#ifndef NIMCP_HPP
#define NIMCP_HPP

#include <string>
#include <vector>
#include <memory>
#include <stdexcept>
#include <utility>
#include <optional>

// Include C API
extern "C" {
#include "nimcp.h"
}

namespace nimcp {

//=============================================================================
// Exceptions
//=============================================================================

/**
 * WHAT: Base exception class for all NIMCP errors
 * WHY:  Enable C++-style error handling with try/catch
 * HOW:  Inherit from std::runtime_error, store NIMCP error message
 */
class Exception : public std::runtime_error {
public:
    explicit Exception(const std::string& message)
        : std::runtime_error(message) {}
};

/**
 * WHAT: Exception for initialization errors
 * WHY:  Distinguish initialization failures from runtime errors
 * HOW:  Separate exception type
 */
class InitializationError : public Exception {
public:
    explicit InitializationError(const std::string& message)
        : Exception("Initialization error: " + message) {}
};

/**
 * WHAT: Exception for prediction/learning errors
 * WHY:  Distinguish operational errors
 * HOW:  Separate exception type
 */
class OperationError : public Exception {
public:
    explicit OperationError(const std::string& message)
        : Exception("Operation error: " + message) {}
};

//=============================================================================
// Enumerations
//=============================================================================

/**
 * WHAT: Brain size enumeration
 * WHY:  Type-safe brain size specification
 * HOW:  Enum class wrapping C constants
 */
enum class BrainSize {
    Tiny   = NIMCP_BRAIN_TINY,
    Small  = NIMCP_BRAIN_SMALL,
    Medium = NIMCP_BRAIN_MEDIUM,
    Large  = NIMCP_BRAIN_LARGE
};

/**
 * WHAT: Task type enumeration
 * WHY:  Type-safe task specification
 * HOW:  Enum class wrapping C constants
 */
enum class TaskType {
    Classification  = NIMCP_TASK_CLASSIFICATION,
    Regression      = NIMCP_TASK_REGRESSION,
    PatternMatching = NIMCP_TASK_PATTERN_MATCHING,
    Sequence        = NIMCP_TASK_SEQUENCE,
    Association     = NIMCP_TASK_ASSOCIATION
};

//=============================================================================
// Library Initialization
//=============================================================================

/**
 * WHAT: RAII wrapper for NIMCP library initialization
 * WHY:  Automatic init/shutdown with scope-based lifetime
 * HOW:  Constructor calls nimcp_init(), destructor calls nimcp_shutdown()
 *
 * USAGE:
 * ```cpp
 * int main() {
 *     nimcp::Library lib;  // Initializes NIMCP
 *     // Use NIMCP...
 * }  // Automatic shutdown
 * ```
 */
class Library {
public:
    /**
     * WHAT: Initialize NIMCP library
     * WHY:  Required before using any NIMCP functions
     * HOW:  Call nimcp_init(), throw on error
     */
    Library() {
        if (nimcp_init() != NIMCP_OK) {
            throw InitializationError("Failed to initialize NIMCP");
        }
    }

    /**
     * WHAT: Shutdown NIMCP library
     * WHY:  Clean up resources
     * HOW:  Call nimcp_shutdown()
     */
    ~Library() noexcept {
        nimcp_shutdown();
    }

    // Non-copyable, non-movable (singleton pattern)
    Library(const Library&) = delete;
    Library& operator=(const Library&) = delete;
    Library(Library&&) = delete;
    Library& operator=(Library&&) = delete;

    /**
     * WHAT: Get NIMCP version string
     * WHY:  Check library version
     * HOW:  Call nimcp_version()
     */
    static std::string version() {
        return nimcp_version();
    }
};

//=============================================================================
// Prediction Result
//=============================================================================

/**
 * WHAT: Structured prediction result
 * WHY:  Type-safe return value for predictions
 * HOW:  Struct with label and confidence
 */
struct Prediction {
    std::string label;
    float confidence;

    Prediction(std::string lbl, float conf)
        : label(std::move(lbl)), confidence(conf) {}
};

//=============================================================================
// Checkpoint Configuration
//=============================================================================

/**
 * WHAT: Checkpointing configuration
 * WHY:  Type-safe checkpoint settings
 * HOW:  Struct wrapping nimcp_checkpoint_config_t
 */
struct CheckpointConfig {
    std::string checkpointDir;
    uint32_t autoIntervalMinutes = 0;  // 0 = manual only
    uint32_t maxCheckpoints = 5;
    bool enabled = true;

    CheckpointConfig(std::string dir) : checkpointDir(std::move(dir)) {}
};

//=============================================================================
// Brain Class
//=============================================================================

/**
 * WHAT: C++ wrapper for NIMCP brain
 * WHY:  RAII, type safety, modern C++ interface
 * HOW:  Unique ownership of nimcp_brain_t with custom deleter
 *
 * FEATURES:
 * - Automatic resource cleanup
 * - Move semantics (no copies)
 * - Exception-based error handling
 * - STL containers for data
 * - v2.7.0 enhancements
 */
class Brain {
private:
    /**
     * WHAT: Custom deleter for nimcp_brain_t
     * WHY:  Enable unique_ptr with C API cleanup function
     * HOW:  Functor calling nimcp_brain_destroy
     */
    struct BrainDeleter {
        void operator()(nimcp_brain_t brain) const noexcept {
            if (brain) {
                nimcp_brain_destroy(brain);
            }
        }
    };

    std::unique_ptr<std::remove_pointer<nimcp_brain_t>::type, BrainDeleter> brain_;

    /**
     * WHAT: Throw exception with NIMCP error message
     * WHY:  Convert C error codes to C++ exceptions
     * HOW:  Get nimcp_get_error(), throw OperationError
     */
    [[noreturn]] static void throwError(const std::string& context) {
        std::string error = nimcp_get_error();
        throw OperationError(context + ": " + error);
    }

public:
    /**
     * WHAT: Create new brain
     * WHY:  Initialize cognitive learning system
     * HOW:  Call nimcp_brain_create, wrap in unique_ptr
     *
     * @param name Brain identifier (for logging)
     * @param size Brain size (Tiny/Small/Medium/Large)
     * @param task Task type (Classification/Regression/etc)
     * @param numInputs Number of input features
     * @param numOutputs Number of output classes/values
     * @throws InitializationError if creation fails
     */
    Brain(const std::string& name, BrainSize size, TaskType task,
          uint32_t numInputs, uint32_t numOutputs) {

        nimcp_brain_t brain = nimcp_brain_create(
            name.c_str(),
            static_cast<nimcp_brain_size_t>(size),
            static_cast<nimcp_brain_task_t>(task),
            numInputs,
            numOutputs
        );

        if (!brain) {
            throw InitializationError("Failed to create brain: " +
                                     std::string(nimcp_get_error()));
        }

        brain_.reset(brain);
    }

    /**
     * WHAT: Load brain from file (factory method)
     * WHY:  Restore previously saved brain
     * HOW:  Call nimcp_brain_load, wrap in Brain object
     *
     * @param filepath Path to saved brain file
     * @return Loaded Brain object
     * @throws OperationError if load fails
     */
    static Brain load(const std::string& filepath) {
        nimcp_brain_t brain = nimcp_brain_load(filepath.c_str());

        if (!brain) {
            throw OperationError("Failed to load brain from " + filepath + ": " +
                               std::string(nimcp_get_error()));
        }

        Brain result;
        result.brain_.reset(brain);
        return result;
    }

    // Rule of Five: Move-only semantics

    Brain() = default;  // For load()

    ~Brain() = default;  // unique_ptr handles cleanup

    Brain(const Brain&) = delete;  // No copying
    Brain& operator=(const Brain&) = delete;

    Brain(Brain&&) noexcept = default;  // Move constructor
    Brain& operator=(Brain&&) noexcept = default;  // Move assignment

    /**
     * WHAT: Train brain on single example
     * WHY:  Supervised learning
     * HOW:  Convert vector to array, call nimcp_brain_learn_example
     *
     * @param features Input feature vector
     * @param label Target label
     * @param confidence Training confidence (0-1)
     * @return Loss value
     * @throws OperationError if learning fails
     */
    float learn(const std::vector<float>& features,
                const std::string& label,
                float confidence = 1.0f) {

        float loss = nimcp_brain_learn_example(
            brain_.get(),
            features.data(),
            static_cast<uint32_t>(features.size()),
            label.c_str(),
            confidence
        );

        if (loss < 0.0f) {
            throwError("Failed to learn example");
        }

        return loss;
    }

    /**
     * WHAT: Make prediction on single example
     * WHY:  Inference after training
     * HOW:  Call nimcp_brain_predict, return structured result
     *
     * @param features Input feature vector
     * @return Prediction with label and confidence
     * @throws OperationError if prediction fails
     */
    Prediction predict(const std::vector<float>& features) {
        char label[256];
        float confidence;

        nimcp_status_t status = nimcp_brain_predict(
            brain_.get(),
            features.data(),
            static_cast<uint32_t>(features.size()),
            label,
            &confidence
        );

        if (status != NIMCP_OK) {
            throwError("Failed to predict");
        }

        return Prediction(label, confidence);
    }

    /**
     * WHAT: Batch prediction (v2.7.0)
     * WHY:  Efficient processing of multiple samples
     * HOW:  Convert vector of vectors to C arrays, call nimcp_brain_predict_batch
     *
     * @param featuresBatch Vector of feature vectors
     * @return Vector of predictions
     * @throws OperationError if batch prediction fails
     */
    std::vector<Prediction> predictBatch(const std::vector<std::vector<float>>& featuresBatch) {
        if (featuresBatch.empty()) {
            throw std::invalid_argument("Batch cannot be empty");
        }

        const size_t batchSize = featuresBatch.size();
        const size_t numFeatures = featuresBatch[0].size();

        // Allocate arrays
        std::vector<const float*> featurePtrs(batchSize);
        for (size_t i = 0; i < batchSize; ++i) {
            if (featuresBatch[i].size() != numFeatures) {
                throw std::invalid_argument("All feature vectors must have same length");
            }
            featurePtrs[i] = featuresBatch[i].data();
        }

        std::vector<char*> labels(batchSize);
        for (size_t i = 0; i < batchSize; ++i) {
            labels[i] = new char[256];
        }

        std::vector<float> confidences(batchSize);

        // Call batch predict
        nimcp_status_t status = nimcp_brain_predict_batch(
            brain_.get(),
            featurePtrs.data(),
            static_cast<uint32_t>(numFeatures),
            static_cast<uint32_t>(batchSize),
            labels.data(),
            confidences.data()
        );

        // Build results
        std::vector<Prediction> results;
        results.reserve(batchSize);

        for (size_t i = 0; i < batchSize; ++i) {
            results.emplace_back(labels[i], confidences[i]);
            delete[] labels[i];
        }

        if (status != NIMCP_OK) {
            throwError("Failed to predict batch");
        }

        return results;
    }

    /**
     * WHAT: Enable checkpointing (v2.7.0)
     * WHY:  Automatic state saving
     * HOW:  Configure and call nimcp_brain_enable_checkpointing
     *
     * @param config Checkpoint configuration
     * @throws OperationError if enable fails
     */
    void enableCheckpointing(const CheckpointConfig& config) {
        nimcp_checkpoint_config_t cConfig = nimcp_checkpoint_default_config();
        cConfig.checkpoint_dir = config.checkpointDir.c_str();
        cConfig.auto_checkpoint_interval_minutes = config.autoIntervalMinutes;
        cConfig.max_checkpoints_to_keep = config.maxCheckpoints;
        cConfig.enabled = config.enabled;

        nimcp_status_t status = nimcp_brain_enable_checkpointing(brain_.get(), &cConfig);

        if (status != NIMCP_OK) {
            throwError("Failed to enable checkpointing");
        }
    }

    /**
     * WHAT: Create manual checkpoint (v2.7.0)
     * WHY:  Save state at specific points
     * HOW:  Call nimcp_brain_checkpoint
     *
     * @param name Optional checkpoint name
     * @throws OperationError if checkpoint fails
     */
    void checkpoint(const std::optional<std::string>& name = std::nullopt) {
        const char* namePtr = name ? name->c_str() : nullptr;

        nimcp_status_t status = nimcp_brain_checkpoint(brain_.get(), namePtr);

        if (status != NIMCP_OK) {
            throwError("Failed to create checkpoint");
        }
    }

    /**
     * WHAT: Save brain to file
     * WHY:  Persist trained model
     * HOW:  Call nimcp_brain_save
     *
     * @param filepath Output file path
     * @throws OperationError if save fails
     */
    void save(const std::string& filepath) {
        nimcp_status_t status = nimcp_brain_save(brain_.get(), filepath.c_str());

        if (status != NIMCP_OK) {
            throwError("Failed to save brain to " + filepath);
        }
    }

    /**
     * WHAT: Get raw C handle
     * WHY:  Interop with C API if needed
     * HOW:  Return raw pointer
     *
     * @return Raw nimcp_brain_t handle
     */
    nimcp_brain_t handle() const noexcept {
        return brain_.get();
    }
};

//=============================================================================
// SIMD Operations (v2.7.0)
//=============================================================================

/**
 * WHAT: Namespace for SIMD operations
 * WHY:  Group high-performance vector operations
 * HOW:  Static functions wrapping C SIMD API
 */
namespace simd {

/**
 * WHAT: SIMD dot product
 * WHY:  High-performance vector multiplication
 * HOW:  Call nimcp_simd_dot_product
 *
 * @param a First vector
 * @param b Second vector
 * @return Dot product
 * @throws std::invalid_argument if vectors have different lengths
 */
inline float dotProduct(const std::vector<float>& a, const std::vector<float>& b) {
    if (a.size() != b.size()) {
        throw std::invalid_argument("Vectors must have same length for dot product");
    }

    return nimcp_simd_dot_product(a.data(), b.data(), static_cast<uint32_t>(a.size()));
}

}  // namespace simd

}  // namespace nimcp

#endif  // NIMCP_HPP
