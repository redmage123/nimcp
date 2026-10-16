/**
 * @file example.cpp
 * @brief Complete C++ example demonstrating NIMCP v2.7.0 features
 * @version 2.7.0
 * @date 2025-11-05
 *
 * WHAT: Reference implementation of NIMCP C++ API usage
 * WHY:  Show proper usage patterns, error handling, modern C++ idioms
 * HOW:  Create brain, train, predict, use v2.7 enhancements
 *
 * Compilation:
 *     g++ -std=c++17 -I../include -I../../include \
 *         -L../../build/src/lib example.cpp \
 *         -lnimcp_core -o nimcp_example
 *
 * Usage:
 *     ./nimcp_example
 */

#include "nimcp.hpp"
#include <iostream>
#include <iomanip>
#include <filesystem>

using namespace nimcp;
namespace fs = std::filesystem;

void printSeparator() {
    std::cout << std::string(70, '=') << '\n';
}

int main() {
    try {
        printSeparator();
        std::cout << "NIMCP C++ Bindings Example (v2.7.0)\n";
        printSeparator();

        // Initialize library (RAII - automatic cleanup)
        Library lib;
        std::cout << "NIMCP Version: " << Library::version() << "\n\n";

        // =====================================================================
        // 1. Basic Brain Operations
        // =====================================================================
        std::cout << "1. Creating and training a brain...\n";

        // Create brain with move semantics
        Brain brain("iris_classifier", BrainSize::Small, TaskType::Classification, 3, 5);

        // Training examples
        struct Example {
            std::vector<float> features;
            std::string label;
            float confidence;
        };

        std::vector<Example> examples = {
            {{0.1f, 0.2f, 0.3f}, "setosa", 0.9f},
            {{0.4f, 0.5f, 0.6f}, "versicolor", 0.8f},
            {{0.7f, 0.8f, 0.9f}, "virginica", 0.95f},
            {{0.15f, 0.25f, 0.35f}, "setosa", 0.85f},
            {{0.45f, 0.55f, 0.65f}, "versicolor", 0.9f},
        };

        std::cout << "   Training on " << examples.size() << " examples...\n";
        for (const auto& ex : examples) {
            float loss = brain.learn(ex.features, ex.label, ex.confidence);
            std::cout << "   Learned " << std::setw(12) << ex.label
                      << ": loss = " << std::fixed << std::setprecision(4) << loss << '\n';
        }

        // Make predictions
        std::cout << "\n   Making predictions...\n";
        std::vector<std::vector<float>> testFeatures = {
            {0.12f, 0.22f, 0.32f},  // Similar to setosa
            {0.72f, 0.82f, 0.92f},  // Similar to virginica
        };

        for (const auto& features : testFeatures) {
            auto [label, confidence] = brain.predict(features);
            std::cout << "   Features [";
            for (size_t i = 0; i < features.size(); ++i) {
                if (i > 0) std::cout << ", ";
                std::cout << features[i];
            }
            std::cout << "] -> " << label
                      << " (confidence: " << std::setprecision(3) << confidence << ")\n";
        }

        // =====================================================================
        // 2. Batch Processing (v2.7.0)
        // =====================================================================
        std::cout << "\n2. Batch prediction (v2.7.0)...\n";

        std::vector<std::vector<float>> batchFeatures = {
            {0.1f, 0.2f, 0.3f},
            {0.4f, 0.5f, 0.6f},
            {0.7f, 0.8f, 0.9f},
            {0.13f, 0.23f, 0.33f},
        };

        auto predictions = brain.predictBatch(batchFeatures);
        std::cout << "   Batch prediction results:\n";
        for (size_t i = 0; i < predictions.size(); ++i) {
            std::cout << "   [" << i << "] " << predictions[i].label
                      << " (confidence: " << std::fixed << std::setprecision(3)
                      << predictions[i].confidence << ")\n";
        }

        // =====================================================================
        // 3. Checkpointing (v2.7.0)
        // =====================================================================
        std::cout << "\n3. Checkpointing (v2.7.0)...\n";

        // Create checkpoint directory
        fs::path checkpointDir = fs::temp_directory_path() / "nimcp_checkpoints";
        fs::create_directories(checkpointDir);
        std::cout << "   Checkpoint directory: " << checkpointDir << '\n';

        // Enable checkpointing
        CheckpointConfig config(checkpointDir.string());
        config.maxCheckpoints = 5;
        brain.enableCheckpointing(config);
        std::cout << "   Checkpointing enabled\n";

        // Create manual checkpoint
        brain.checkpoint("after_initial_training");
        std::cout << "   Created checkpoint: after_initial_training\n";

        // Train more
        for (int i = 0; i < 3; ++i) {
            float loss = brain.learn({0.2f + i*0.1f, 0.3f + i*0.1f, 0.4f + i*0.1f},
                                    "setosa", 0.9f);
        }

        // Another checkpoint
        brain.checkpoint("after_additional_training");
        std::cout << "   Created checkpoint: after_additional_training\n";

        // =====================================================================
        // 4. SIMD Operations (v2.7.0)
        // =====================================================================
        std::cout << "\n4. SIMD operations (v2.7.0)...\n";

        std::vector<float> vecA = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
        std::vector<float> vecB = {2.0f, 3.0f, 4.0f, 5.0f, 6.0f};

        float dotProduct = simd::dotProduct(vecA, vecB);
        std::cout << "   Dot product of [1.0, 2.0, 3.0, 4.0, 5.0]\n";
        std::cout << "              and [2.0, 3.0, 4.0, 5.0, 6.0]\n";
        std::cout << "              = " << std::fixed << std::setprecision(2)
                  << dotProduct << '\n';

        // =====================================================================
        // 5. Save and Load with Move Semantics
        // =====================================================================
        std::cout << "\n5. Save and load brain...\n";

        // Save brain
        fs::path savePath = fs::temp_directory_path() / "brain_save.nimcp";
        brain.save(savePath.string());
        std::cout << "   Brain saved to: " << savePath << '\n';

        // Load brain (demonstrates factory method and move semantics)
        Brain loadedBrain = Brain::load(savePath.string());
        std::cout << "   Brain loaded successfully\n";

        // Verify loaded brain works
        auto [label, conf] = loadedBrain.predict({0.1f, 0.2f, 0.3f});
        std::cout << "   Loaded brain prediction: " << label
                  << " (confidence: " << std::setprecision(3) << conf << ")\n";

        // Clean up
        fs::remove(savePath);

        // =====================================================================
        // 6. Error Handling with Exceptions
        // =====================================================================
        std::cout << "\n6. Error handling...\n";

        try {
            // Try to predict with wrong number of features
            Brain badBrain("test", BrainSize::Tiny, TaskType::Classification, 5, 2);
            badBrain.predict({1.0f, 2.0f});  // Only 2 features, needs 5
        } catch (const OperationError& e) {
            std::cout << "   Caught expected error: " << e.what() << '\n';
        }

        // =====================================================================
        // Summary
        // =====================================================================
        printSeparator();
        std::cout << "Example completed successfully!\n";
        printSeparator();
        std::cout << "\nKey Features Demonstrated:\n";
        std::cout << "  ✓ RAII resource management\n";
        std::cout << "  ✓ Move semantics (no copying)\n";
        std::cout << "  ✓ Exception-based error handling\n";
        std::cout << "  ✓ Structured bindings for return values\n";
        std::cout << "  ✓ Batch prediction (v2.7.0)\n";
        std::cout << "  ✓ Checkpointing system (v2.7.0)\n";
        std::cout << "  ✓ SIMD operations (v2.7.0)\n";
        std::cout << "  ✓ Save/load functionality\n";
        std::cout << "\nFor more information, see:\n";
        std::cout << "  - API documentation: docs/cpp_api.md\n";
        std::cout << "  - NIMCP repository: https://github.com/youruser/nimcp\n";

    } catch (const Exception& e) {
        std::cerr << "NIMCP Error: " << e.what() << '\n';
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }

    return 0;
}
