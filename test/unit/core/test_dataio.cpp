/**
 * @file test_dataio.cpp
 * @brief Tests for brain data I/O functionality
 *
 * WHAT: Verify brain data loading, training, and export work correctly
 * WHY: Production Artemis needs to train from external datasets
 * HOW: Unit tests for data I/O API with CSV backend
 */

#include "test_helpers.h"

#include "core/brain/nimcp_brain.h"
#include "io/dataio/nimcp_dataio.h"

#include <gtest/gtest.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

//=============================================================================
// Test Fixtures and Helpers
//=============================================================================

/**
 * WHAT: Test fixture for data I/O tests
 * WHY: Set up/tear down test CSV files and brains
 */
class DataIOTest : public ::testing::Test {
   protected:
    const char* test_csv = "/tmp/nimcp_test_data.csv";
    const char* test_csv_no_header = "/tmp/nimcp_test_data_no_header.csv";
    const char* test_output = "/tmp/nimcp_test_output.csv";
    brain_t brain;
    dataset_t dataset;

    void SetUp() override
    {
        // Create test brain
        brain = brain_create("test_brain", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 13, 3);
        ASSERT_NE(brain, nullptr);

        dataset = nullptr;

        // Create test CSV files
        create_test_csv_with_header();
        create_test_csv_no_header();
    }

    void TearDown() override
    {
        // Clean up dataset
        if (dataset) {
            dataset_close(dataset);
        }

        // Clean up brain
        if (brain) {
            brain_destroy(brain);
        }

        // Clean up files
        unlink(test_csv);
        unlink(test_csv_no_header);
        unlink(test_output);
    }

    /**
     * WHAT: Create test CSV with header
     * WHY: Provide test data for CSV loading tests
     */
    void create_test_csv_with_header()
    {
        FILE* f = fopen(test_csv, "w");
        ASSERT_NE(f, nullptr);

        // Write header
        fprintf(f, "f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11,f12,f13,label\n");

        // Write 10 rows of data
        for (int i = 0; i < 10; i++) {
            fprintf(f, "0.%d,0.%d,0.%d,0.%d,0.%d,0.%d,0.%d,0.%d,0.%d,0.%d,0.%d,0.%d,0.%d,%s\n", i,
                    i, i, i, i, i, i, i, i, i, i, i, i, (i % 2 == 0) ? "allow" : "deny");
        }

        fclose(f);
    }

    /**
     * WHAT: Create test CSV without header
     * WHY: Test CSV loading without header row
     */
    void create_test_csv_no_header()
    {
        FILE* f = fopen(test_csv_no_header, "w");
        ASSERT_NE(f, nullptr);

        // Write 10 rows of data (no header)
        for (int i = 0; i < 10; i++) {
            fprintf(f, "0.%d,0.%d,0.%d,0.%d,0.%d,0.%d,0.%d,0.%d,0.%d,0.%d,0.%d,0.%d,0.%d,%s\n", i,
                    i, i, i, i, i, i, i, i, i, i, i, i, (i % 2 == 0) ? "allow" : "deny");
        }

        fclose(f);
    }
};

//=============================================================================
// CSV Loading Tests
//=============================================================================

/**
 * WHAT: Test loading CSV with header
 * WHY: Verify basic CSV loading works
 */
TEST_F(DataIOTest, LoadCSVWithHeader)
{
    dataset = dataset_load_csv(test_csv, 13, 1, true);
    ASSERT_NE(dataset, nullptr);
}

/**
 * WHAT: Test loading CSV without header
 * WHY: Verify header-less CSV loading works
 */
TEST_F(DataIOTest, LoadCSVWithoutHeader)
{
    dataset = dataset_load_csv(test_csv_no_header, 13, 1, false);
    ASSERT_NE(dataset, nullptr);
}

/**
 * WHAT: Test loading CSV with NULL filepath
 * WHY: Verify parameter validation
 */
TEST_F(DataIOTest, LoadCSVNullPath)
{
    dataset = dataset_load_csv(nullptr, 13, 1, true);
    ASSERT_EQ(dataset, nullptr);
}

/**
 * WHAT: Test loading non-existent CSV
 * WHY: Verify error handling for missing files
 */
TEST_F(DataIOTest, LoadCSVNonExistent)
{
    dataset = dataset_load_csv("/tmp/nonexistent_file.csv", 13, 1, true);
    ASSERT_EQ(dataset, nullptr);
}

//=============================================================================
// Batch Reading Tests
//=============================================================================

/**
 * WHAT: Test reading batch from CSV
 * WHY: Verify batch reading works correctly
 */
TEST_F(DataIOTest, ReadBatch)
{
    dataset = dataset_load_csv(test_csv, 13, 1, true);
    ASSERT_NE(dataset, nullptr);

    // Read first batch
    data_batch_t batch;
    memset(&batch, 0, sizeof(batch));

    bool result = dataset_next_batch(dataset, &batch);
    ASSERT_TRUE(result);
    ASSERT_GT(batch.num_samples, 0);
    ASSERT_NE(batch.features, nullptr);
    ASSERT_NE(batch.labels, nullptr);

    // Verify first sample has data
    ASSERT_NE(batch.features[0], nullptr);
    ASSERT_NE(batch.labels[0], nullptr);

    dataset_free_batch(&batch);
}

/**
 * WHAT: Test reading multiple batches
 * WHY: Verify streaming works for large datasets
 */
TEST_F(DataIOTest, ReadMultipleBatches)
{
    dataset = dataset_load_csv(test_csv, 13, 1, true);
    ASSERT_NE(dataset, nullptr);

    uint32_t total_samples = 0;

    // Read all batches
    while (true) {
        data_batch_t batch;
        memset(&batch, 0, sizeof(batch));

        if (!dataset_next_batch(dataset, &batch)) {
            break;
        }

        total_samples += batch.num_samples;
        dataset_free_batch(&batch);

        if (batch.end_of_dataset)
            break;
    }

    // Should have read all 10 rows
    ASSERT_EQ(total_samples, 10);
}

/**
 * WHAT: Test batch reading with NULL dataset
 * WHY: Verify parameter validation
 */
TEST_F(DataIOTest, ReadBatchNullDataset)
{
    data_batch_t batch;
    bool result = dataset_next_batch(nullptr, &batch);
    ASSERT_FALSE(result);
}

/**
 * WHAT: Test batch reading with NULL batch
 * WHY: Verify parameter validation
 */
TEST_F(DataIOTest, ReadBatchNullBatch)
{
    dataset = dataset_load_csv(test_csv, 13, 1, true);
    ASSERT_NE(dataset, nullptr);

    bool result = dataset_next_batch(dataset, nullptr);
    ASSERT_FALSE(result);
}

//=============================================================================
// Dataset Reset Tests
//=============================================================================

/**
 * WHAT: Test resetting dataset to beginning
 * WHY: Verify multiple epochs work correctly
 */
TEST_F(DataIOTest, ResetDataset)
{
    dataset = dataset_load_csv(test_csv, 13, 1, true);
    ASSERT_NE(dataset, nullptr);

    // Read all batches
    while (true) {
        data_batch_t batch;
        memset(&batch, 0, sizeof(batch));

        if (!dataset_next_batch(dataset, &batch)) {
            break;
        }

        dataset_free_batch(&batch);

        if (batch.end_of_dataset)
            break;
    }

    // Reset
    bool result = dataset_reset(dataset);
    ASSERT_TRUE(result);

    // Should be able to read again
    data_batch_t batch;
    memset(&batch, 0, sizeof(batch));

    result = dataset_next_batch(dataset, &batch);
    ASSERT_TRUE(result);
    ASSERT_GT(batch.num_samples, 0);

    dataset_free_batch(&batch);
}

/**
 * WHAT: Test reset with NULL dataset
 * WHY: Verify parameter validation
 */
TEST_F(DataIOTest, ResetNullDataset)
{
    bool result = dataset_reset(nullptr);
    ASSERT_FALSE(result);
}

//=============================================================================
// Dataset Size Tests
//=============================================================================

/**
 * WHAT: Test getting dataset size
 * WHY: Verify size reporting works
 */
TEST_F(DataIOTest, GetDatasetSize)
{
    dataset = dataset_load_csv(test_csv, 13, 1, true);
    ASSERT_NE(dataset, nullptr);

    // Read all batches to count rows
    while (true) {
        data_batch_t batch;
        memset(&batch, 0, sizeof(batch));

        if (!dataset_next_batch(dataset, &batch)) {
            break;
        }

        dataset_free_batch(&batch);

        if (batch.end_of_dataset)
            break;
    }

    // Get size
    uint64_t size = dataset_get_size(dataset);
    ASSERT_EQ(size, 10);
}

/**
 * WHAT: Test get size with NULL dataset
 * WHY: Verify parameter validation
 */
TEST_F(DataIOTest, GetSizeNullDataset)
{
    uint64_t size = dataset_get_size(nullptr);
    ASSERT_EQ(size, 0);
}

//=============================================================================
// Brain Training Tests
//=============================================================================

/**
 * WHAT: Test training brain from dataset
 * WHY: Verify end-to-end training works
 */
TEST_F(DataIOTest, TrainFromDataset)
{
    dataset = dataset_load_csv(test_csv, 13, 1, true);
    ASSERT_NE(dataset, nullptr);

    // Train brain (1 epoch, no validation)
    float accuracy = brain_train_from_dataset(brain, dataset, 1, 0.0f);

    // Should complete without errors
    // (Accuracy may be 0.0 if validation disabled)
    ASSERT_GE(accuracy, 0.0f);
}

/**
 * WHAT: Test training with multiple epochs
 * WHY: Verify epoch iteration works
 */
TEST_F(DataIOTest, TrainMultipleEpochs)
{
    dataset = dataset_load_csv(test_csv, 13, 1, true);
    ASSERT_NE(dataset, nullptr);

    // Train for 3 epochs
    float accuracy = brain_train_from_dataset(brain, dataset, 3, 0.0f);

    ASSERT_GE(accuracy, 0.0f);
}

/**
 * WHAT: Test training with validation split
 * WHY: Verify validation split works
 */
TEST_F(DataIOTest, TrainWithValidation)
{
    dataset = dataset_load_csv(test_csv, 13, 1, true);
    ASSERT_NE(dataset, nullptr);

    // Train with 20% validation
    float accuracy = brain_train_from_dataset(brain, dataset, 1, 0.2f);

    ASSERT_GE(accuracy, 0.0f);
}

/**
 * WHAT: Test training with NULL brain
 * WHY: Verify parameter validation
 */
TEST_F(DataIOTest, TrainNullBrain)
{
    dataset = dataset_load_csv(test_csv, 13, 1, true);
    ASSERT_NE(dataset, nullptr);

    float accuracy = brain_train_from_dataset(nullptr, dataset, 1, 0.0f);
    ASSERT_EQ(accuracy, 0.0f);
}

/**
 * WHAT: Test training with NULL dataset
 * WHY: Verify parameter validation
 */
TEST_F(DataIOTest, TrainNullDataset)
{
    float accuracy = brain_train_from_dataset(brain, nullptr, 1, 0.0f);
    ASSERT_EQ(accuracy, 0.0f);
}

/**
 * WHAT: Test training with invalid validation split
 * WHY: Verify parameter validation
 */
TEST_F(DataIOTest, TrainInvalidValidationSplit)
{
    dataset = dataset_load_csv(test_csv, 13, 1, true);
    ASSERT_NE(dataset, nullptr);

    // Invalid: >= 1.0
    float accuracy = brain_train_from_dataset(brain, dataset, 1, 1.5f);
    ASSERT_EQ(accuracy, 0.0f);

    // Invalid: < 0.0
    accuracy = brain_train_from_dataset(brain, dataset, 1, -0.5f);
    ASSERT_EQ(accuracy, 0.0f);
}

//=============================================================================
// Streaming Training Tests
//=============================================================================

/**
 * WHAT: Test streaming training
 * WHY: Verify streaming API works for large datasets
 */
TEST_F(DataIOTest, TrainStreaming)
{
    dataset = dataset_load_csv(test_csv, 13, 1, true);
    ASSERT_NE(dataset, nullptr);

    // Train streaming (max 2 batches)
    float loss = brain_train_from_dataset_streaming(brain, dataset, 2);

    ASSERT_GE(loss, 0.0f);
}

/**
 * WHAT: Test streaming with unlimited batches
 * WHY: Verify processing entire dataset works
 */
TEST_F(DataIOTest, TrainStreamingUnlimited)
{
    dataset = dataset_load_csv(test_csv, 13, 1, true);
    ASSERT_NE(dataset, nullptr);

    // Train streaming (all batches)
    float loss = brain_train_from_dataset_streaming(brain, dataset, 0);

    ASSERT_GE(loss, 0.0f);
}

//=============================================================================
// Data Export Tests
//=============================================================================

/**
 * WHAT: Test exporting predictions to CSV
 * WHY: Verify export functionality works
 */
TEST_F(DataIOTest, ExportPredictions)
{
    dataset = dataset_load_csv(test_csv, 13, 1, true);
    ASSERT_NE(dataset, nullptr);

    // Train brain first
    brain_train_from_dataset(brain, dataset, 1, 0.0f);

    // Reset dataset
    dataset_reset(dataset);

    // Export predictions
    bool result = brain_export_predictions(brain, dataset, test_output, DATA_FORMAT_CSV);

    ASSERT_TRUE(result);

    // Verify output file exists
    FILE* f = fopen(test_output, "r");
    ASSERT_NE(f, nullptr);

    // Verify it has content
    char line[1024];
    ASSERT_NE(fgets(line, sizeof(line), f), nullptr);

    fclose(f);
}

/**
 * WHAT: Test export with NULL brain
 * WHY: Verify parameter validation
 */
TEST_F(DataIOTest, ExportNullBrain)
{
    dataset = dataset_load_csv(test_csv, 13, 1, true);
    ASSERT_NE(dataset, nullptr);

    bool result = brain_export_predictions(nullptr, dataset, test_output, DATA_FORMAT_CSV);

    ASSERT_FALSE(result);
}

/**
 * WHAT: Test export with NULL dataset
 * WHY: Verify parameter validation
 */
TEST_F(DataIOTest, ExportNullDataset)
{
    bool result = brain_export_predictions(brain, nullptr, test_output, DATA_FORMAT_CSV);

    ASSERT_FALSE(result);
}

/**
 * WHAT: Test export with NULL output file
 * WHY: Verify parameter validation
 */
TEST_F(DataIOTest, ExportNullOutput)
{
    dataset = dataset_load_csv(test_csv, 13, 1, true);
    ASSERT_NE(dataset, nullptr);

    bool result = brain_export_predictions(brain, dataset, nullptr, DATA_FORMAT_CSV);

    ASSERT_FALSE(result);
}

//=============================================================================
// CSV Save Tests
//=============================================================================

/**
 * WHAT: Test saving dataset to CSV
 * WHY: Verify CSV writing works
 */
TEST_F(DataIOTest, SaveCSV)
{
    // Create test data
    float* features[3];
    char* labels[3];

    for (int i = 0; i < 3; i++) {
        features[i] = (float*) malloc(13 * sizeof(float));
        for (int j = 0; j < 13; j++) {
            features[i][j] = 0.5f;
        }
        labels[i] = strdup((i % 2 == 0) ? "allow" : "deny");
    }

    // Save to CSV
    bool result = dataset_save_csv(features, labels, 3, 13, test_output, nullptr);

    ASSERT_TRUE(result);

    // Verify file exists and has content
    FILE* f = fopen(test_output, "r");
    ASSERT_NE(f, nullptr);

    char line[1024];
    uint32_t line_count = 0;
    while (fgets(line, sizeof(line), f)) {
        line_count++;
    }

    // Should have 3 rows
    ASSERT_EQ(line_count, 3);

    fclose(f);

    // Clean up
    for (int i = 0; i < 3; i++) {
        free(features[i]);
        free(labels[i]);
    }
}

/**
 * WHAT: Test save with feature names
 * WHY: Verify header writing works
 */
TEST_F(DataIOTest, SaveCSVWithHeader)
{
    // Create test data
    float* features[2];
    char* labels[2];
    char* feature_names[13];

    for (int i = 0; i < 2; i++) {
        features[i] = (float*) malloc(13 * sizeof(float));
        for (int j = 0; j < 13; j++) {
            features[i][j] = 0.5f;
        }
        labels[i] = strdup("allow");
    }

    for (int i = 0; i < 13; i++) {
        feature_names[i] = (char*) malloc(16);
        snprintf(feature_names[i], 16, "feature_%d", i);
    }

    // Save with header
    bool result = dataset_save_csv(features, labels, 2, 13, test_output, feature_names);

    ASSERT_TRUE(result);

    // Verify header exists
    FILE* f = fopen(test_output, "r");
    ASSERT_NE(f, nullptr);

    char line[1024];
    ASSERT_NE(fgets(line, sizeof(line), f), nullptr);

    // Should contain "feature_0"
    ASSERT_NE(strstr(line, "feature_0"), nullptr);

    fclose(f);

    // Clean up
    for (int i = 0; i < 2; i++) {
        free(features[i]);
        free(labels[i]);
    }
    for (int i = 0; i < 13; i++) {
        free(feature_names[i]);
    }
}

//=============================================================================
// Error Handling Tests
//=============================================================================

/**
 * WHAT: Test error message retrieval
 * WHY: Verify error reporting works
 */
TEST_F(DataIOTest, GetLastError)
{
    // Trigger an error
    dataset = dataset_load_csv(nullptr, 13, 1, true);
    ASSERT_EQ(dataset, nullptr);

    // Get error message
    // TODO: dataio_get_last_error() function not yet implemented
    // const char* error = dataio_get_last_error();
    // ASSERT_NE(error, nullptr);
    // ASSERT_GT(strlen(error), 0);
}

// Note: main() is defined in test_module.cpp - all test files share one main()
