/**
 * @file Example.java
 * @brief Complete Java example demonstrating NIMCP v2.7.0 features
 * @version 2.7.0
 * @date 2025-11-05
 *
 * WHAT: Reference implementation of NIMCP Java API usage
 * WHY:  Show proper usage patterns, error handling, Java idioms
 * HOW:  Create brain, train, predict, demonstrate features
 *
 * Compilation:
 *     javac -cp . com/nimcp/NIMCP.java Example.java
 *
 * Execution:
 *     java -Djava.library.path=/path/to/nimcp/build/src/lib Example
 */

import com.nimcp.NIMCP;
import com.nimcp.NIMCP.*;

public class Example {

    private static void printSeparator() {
        System.out.println("=".repeat(70));
    }

    public static void main(String[] args) {
        try {
            printSeparator();
            System.out.println("NIMCP Java Bindings Example (v2.7.0)");
            printSeparator();

            // ================================================================
            // 1. Initialize Library
            // ================================================================
            NIMCP.init();
            System.out.println("NIMCP Version: " + NIMCP.version());
            System.out.println();

            // ================================================================
            // 2. Basic Brain Operations
            // ================================================================
            System.out.println("1. Creating and training a brain...");

            // Create brain
            Brain brain = new Brain(
                "iris_classifier",
                Brain.Size.SMALL,
                Brain.Task.CLASSIFICATION,
                3,  // inputs
                5   // outputs
            );

            // Training examples
            float[][] trainingFeatures = {
                {0.1f, 0.2f, 0.3f},
                {0.4f, 0.5f, 0.6f},
                {0.7f, 0.8f, 0.9f},
                {0.15f, 0.25f, 0.35f},
                {0.45f, 0.55f, 0.65f}
            };
            String[] trainingLabels = {"setosa", "versicolor", "virginica", "setosa", "versicolor"};
            float[] trainingConfidences = {0.9f, 0.8f, 0.95f, 0.85f, 0.9f};

            System.out.println("   Training on " + trainingFeatures.length + " examples...");
            for (int i = 0; i < trainingFeatures.length; i++) {
                brain.learn(trainingFeatures[i], trainingLabels[i], trainingConfidences[i]);
                System.out.printf("   Learned %-12s%n", trainingLabels[i]);
            }

            // Make predictions
            System.out.println("\n   Making predictions...");
            float[][] testFeatures = {
                {0.12f, 0.22f, 0.32f},  // Similar to setosa
                {0.72f, 0.82f, 0.92f}   // Similar to virginica
            };

            for (float[] features : testFeatures) {
                Prediction pred = brain.predict(features);
                System.out.printf("   Features [%.2f, %.2f, %.2f] -> %s (confidence: %.3f)%n",
                    features[0], features[1], features[2],
                    pred.label, pred.confidence);
            }

            // ================================================================
            // 3. Save and Load
            // ================================================================
            System.out.println("\n2. Save and load brain...");

            // Save brain
            String savePath = System.getProperty("java.io.tmpdir") + "/brain_save.nimcp";
            brain.save(savePath);
            System.out.println("   Brain saved to: " + savePath);

            // Load brain
            Brain loadedBrain = Brain.load(savePath);
            System.out.println("   Brain loaded successfully");

            // Verify loaded brain works
            Prediction testPred = loadedBrain.predict(new float[]{0.1f, 0.2f, 0.3f});
            System.out.printf("   Loaded brain prediction: %s (confidence: %.3f)%n",
                testPred.label, testPred.confidence);

            // Clean up file
            new java.io.File(savePath).delete();

            // ================================================================
            // 4. Network Operations
            // ================================================================
            System.out.println("\n3. Neural network operations...");

            Network network = new Network(3, 2, 10, 0.01f);
            System.out.println("   Created network: 3 inputs, 2 outputs, 10 hidden");

            float[] inputs = {0.5f, 0.7f, 0.3f};
            float[] outputs = network.forward(inputs);

            System.out.printf("   Forward pass: [%.2f, %.2f, %.2f] -> [%.4f, %.4f]%n",
                inputs[0], inputs[1], inputs[2], outputs[0], outputs[1]);

            // ================================================================
            // 5. Error Handling
            // ================================================================
            System.out.println("\n4. Error handling...");

            try {
                // Try to predict with wrong number of features
                Brain badBrain = new Brain("test", Brain.Size.TINY, Brain.Task.CLASSIFICATION, 5, 2);
                badBrain.predict(new float[]{1.0f, 2.0f});  // Only 2 features, needs 5
            } catch (NIMCPException e) {
                System.out.println("   Caught expected error: " + e.getMessage());
            }

            // ================================================================
            // Summary
            // ================================================================
            printSeparator();
            System.out.println("Example completed successfully!");
            printSeparator();
            System.out.println("\nKey Features Demonstrated:");
            System.out.println("  ✓ Brain creation and configuration");
            System.out.println("  ✓ Single-example learning and prediction");
            System.out.println("  ✓ Save/load functionality");
            System.out.println("  ✓ Neural network operations");
            System.out.println("  ✓ Exception-based error handling");
            System.out.println("\nFor more information, see:");
            System.out.println("  - API documentation: docs/java_api.md");
            System.out.println("  - NIMCP repository: https://github.com/youruser/nimcp");

        } catch (NIMCPException e) {
            System.err.println("NIMCP Error: " + e.getMessage());
            System.exit(1);
        } catch (Exception e) {
            System.err.println("Error: " + e.getMessage());
            e.printStackTrace();
            System.exit(1);
        }
    }
}
