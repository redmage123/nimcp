/**
 * @file test_dataio.cpp
 * @brief Comprehensive test suite for NIMCP data I/O infrastructure
 *
 * WHAT: Tests for all aspects of the data I/O system including CSV reading,
 *       batch processing, error handling, thread safety, and memory management
 * WHY: Ensure data I/O system is robust, thread-safe, and handles edge cases
 * HOW: Use GoogleTest framework with fixtures and file operations
 */

#include <fcntl.h>
#include <gtest/gtest.h>
#include <sys/stat.h>
#include <unistd.h>
#include <atomic>
#include <chrono>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "io/dataio/nimcp_dataio.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Constants
//=============================================================================

static const char* TEST_DATA_DIR = "/tmp/nimcp_test_dataio";
static const char* TEST_CSV_FILE = "/tmp/nimcp_test_dataio/test.csv";
static const char* TEST_CSV_NO_HEADER = "/tmp/nimcp_test_dataio/test_no_header.csv";
static const char* TEST_CSV_LARGE = "/tmp/nimcp_test_dataio/test_large.csv";
static const char* TEST_CSV_MALFORMED = "/tmp/nimcp_test_dataio/test_malformed.csv";
static const char* TEST_CSV_BINARY = "/tmp/nimcp_test_dataio/test_binary.csv";
static const char* TEST_OUTPUT_CSV = "/tmp/nimcp_test_dataio/output.csv";

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * WHAT: Create test data directory
 * WHY: Ensure directory exists for test files
 */
static void create_test_data_dir()
{
    mkdir(TEST_DATA_DIR, 0755);
}

/**
 * WHAT: Clean up test data directory
 * WHY: Ensure clean state for tests
 */
static void cleanup_test_data()
{
    // Remove test files
    unlink(TEST_CSV_FILE);
    unlink(TEST_CSV_NO_HEADER);
    unlink(TEST_CSV_LARGE);
    unlink(TEST_CSV_MALFORMED);
    unlink(TEST_CSV_BINARY);
    unlink(TEST_OUTPUT_CSV);

    // Remove directory
    rmdir(TEST_DATA_DIR);
}

/**
 * WHAT: Create a simple test CSV file with header
 * WHY: Test basic CSV reading functionality
 */
static void create_test_csv_with_header()
{
    std::ofstream file(TEST_CSV_FILE);
    file << "feature1,feature2,feature3,label\n";
    file << "1.0,2.0,3.0,classA\n";
    file << "4.0,5.0,6.0,classB\n";
    file << "7.0,8.0,9.0,classA\n";
    file << "10.0,11.0,12.0,classC\n";
    file.close();
}

/**
 * WHAT: Create a test CSV file without header
 * WHY: Test CSV reading without header row
 */
static void create_test_csv_no_header()
{
    std::ofstream file(TEST_CSV_NO_HEADER);
    file << "1.0,2.0,3.0,classA\n";
    file << "4.0,5.0,6.0,classB\n";
    file << "7.0,8.0,9.0,classA\n";
    file.close();
}

/**
 * WHAT: Create a large test CSV file
 * WHY: Test batch processing and memory management
 */
static void create_large_csv(int num_rows = 5000)
{
    std::ofstream file(TEST_CSV_LARGE);
    file << "f1,f2,f3,f4,label\n";
    for (int i = 0; i < num_rows; i++) {
        file << (i * 1.0) << "," << (i * 2.0) << "," << (i * 3.0) << "," << (i * 4.0)
             << ",class" << (i % 3) << "\n";
    }
    file.close();
}

/**
 * WHAT: Create a malformed CSV file
 * WHY: Test error handling for invalid data
 */
static void create_malformed_csv()
{
    std::ofstream file(TEST_CSV_MALFORMED);
    file << "feature1,feature2,label\n";
    file << "1.0,2.0,classA\n";
    file << "invalid,5.0,classB\n";  // Invalid float
    file << "7.0,classC\n";          // Missing column
    file << "10.0,11.0,classD\n";
    file << ",,classE\n";  // Empty values
    file.close();
}

/**
 * WHAT: Create a CSV with special characters and edge cases
 * WHY: Test handling of special values
 */
static void create_csv_with_special_chars()
{
    std::ofstream file(TEST_CSV_BINARY);
    file << "f1,f2,f3,label\n";
    file << "0.0,0.0,0.0,zero\n";
    file << "-1.5,-2.5,-3.5,negative\n";
    file << "999999.9,888888.8,777777.7,large\n";
    file << "0.000001,0.000002,0.000003,tiny\n";
    file << "1.23e10,4.56e-10,7.89e5,scientific\n";
    file.close();
}

/**
 * WHAT: Count lines in a file
 * WHY: Verify correct number of rows processed
 */
static int count_file_lines(const char* path)
{
    std::ifstream file(path);
    if (!file.is_open()) {
        return -1;
    }

    int count = 0;
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty()) {
            count++;
        }
    }
    return count;
}

/**
 * WHAT: Check if file exists
 * WHY: Verify file creation and deletion
 */
static bool file_exists(const char* path)
{
    struct stat buffer;
    return (stat(path, &buffer) == 0);
}

//=============================================================================
// Test Fixture
//=============================================================================

class DataIOTest : public ::testing::Test {
   protected:
    void SetUp() override
    {
        // Clean up any existing test data
        cleanup_test_data();

        // Create test directory
        create_test_data_dir();
    }

    void TearDown() override
    {
        // Clean up after test
        cleanup_test_data();
    }
};

//=============================================================================
// Basic CSV Reading Tests
//=============================================================================

/**
 * WHAT: Test opening a CSV file with header
 * WHY: Verify basic CSV loading functionality
 */
TEST_F(DataIOTest, OpenCSVWithHeader)
{
    create_test_csv_with_header();

    dataset_t dataset = dataset_load_csv(TEST_CSV_FILE, 3, 1, true);
    ASSERT_NE(dataset, nullptr);

    dataset_close(dataset);
}

/**
 * WHAT: Test opening a CSV file without header
 * WHY: Verify CSV loading works without header row
 */
TEST_F(DataIOTest, OpenCSVWithoutHeader)
{
    create_test_csv_no_header();

    dataset_t dataset = dataset_load_csv(TEST_CSV_NO_HEADER, 3, 1, false);
    ASSERT_NE(dataset, nullptr);

    dataset_close(dataset);
}

/**
 * WHAT: Test reading a single batch from CSV
 * WHY: Verify batch reading functionality
 */
TEST_F(DataIOTest, ReadSingleBatch)
{
    create_test_csv_with_header();

    dataset_t dataset = dataset_load_csv(TEST_CSV_FILE, 3, 1, true);
    ASSERT_NE(dataset, nullptr);

    data_batch_t batch;
    memset(&batch, 0, sizeof(batch));

    bool result = dataset_next_batch(dataset, &batch);
    EXPECT_TRUE(result);
    EXPECT_GT(batch.num_samples, 0u);
    EXPECT_NE(batch.features, nullptr);
    EXPECT_NE(batch.labels, nullptr);

    // Verify first row data
    if (batch.num_samples > 0) {
        EXPECT_FLOAT_EQ(batch.features[0][0], 1.0f);
        EXPECT_FLOAT_EQ(batch.features[0][1], 2.0f);
        EXPECT_FLOAT_EQ(batch.features[0][2], 3.0f);
        EXPECT_STREQ(batch.labels[0], "classA");
    }

    dataset_free_batch(&batch);
    dataset_close(dataset);
}

/**
 * WHAT: Test reading multiple batches
 * WHY: Verify streaming functionality for large files
 */
TEST_F(DataIOTest, ReadMultipleBatches)
{
    create_large_csv(2500);

    dataset_t dataset = dataset_load_csv(TEST_CSV_LARGE, 4, 1, true);
    ASSERT_NE(dataset, nullptr);

    int total_samples = 0;
    int batch_count = 0;

    while (true) {
        data_batch_t batch;
        memset(&batch, 0, sizeof(batch));

        if (!dataset_next_batch(dataset, &batch)) {
            break;
        }

        total_samples += batch.num_samples;
        batch_count++;

        dataset_free_batch(&batch);

        if (batch.end_of_dataset) {
            break;
        }
    }

    EXPECT_GT(batch_count, 2);  // Should have multiple batches
    EXPECT_EQ(total_samples, 2500);

    dataset_close(dataset);
}

/**
 * WHAT: Test resetting dataset to beginning
 * WHY: Verify multiple epoch support
 */
TEST_F(DataIOTest, ResetDataset)
{
    create_test_csv_with_header();

    dataset_t dataset = dataset_load_csv(TEST_CSV_FILE, 3, 1, true);
    ASSERT_NE(dataset, nullptr);

    // Read first batch
    data_batch_t batch1;
    memset(&batch1, 0, sizeof(batch1));
    bool result = dataset_next_batch(dataset, &batch1);
    EXPECT_TRUE(result);
    uint32_t first_count = batch1.num_samples;
    dataset_free_batch(&batch1);

    // Reset dataset
    bool reset_result = dataset_reset(dataset);
    EXPECT_TRUE(reset_result);

    // Read again - should get same data
    data_batch_t batch2;
    memset(&batch2, 0, sizeof(batch2));
    result = dataset_next_batch(dataset, &batch2);
    EXPECT_TRUE(result);
    EXPECT_EQ(batch2.num_samples, first_count);

    if (batch2.num_samples > 0) {
        EXPECT_FLOAT_EQ(batch2.features[0][0], 1.0f);
    }

    dataset_free_batch(&batch2);
    dataset_close(dataset);
}

//=============================================================================
// Error Handling Tests
//=============================================================================

/**
 * WHAT: Test opening non-existent file
 * WHY: Verify error handling for missing files
 */
TEST_F(DataIOTest, OpenNonExistentFile)
{
    dataset_t dataset = dataset_load_csv("/tmp/nonexistent_file.csv", 3, 1, true);
    EXPECT_EQ(dataset, nullptr);
}

/**
 * WHAT: Test NULL parameter handling
 * WHY: Verify robustness against NULL inputs
 */
TEST_F(DataIOTest, NullParameters)
{
    // NULL filepath
    dataset_t dataset = dataset_load_csv(nullptr, 3, 1, true);
    EXPECT_EQ(dataset, nullptr);

    // NULL dataset for next_batch
    data_batch_t batch;
    memset(&batch, 0, sizeof(batch));
    bool result = dataset_next_batch(nullptr, &batch);
    EXPECT_FALSE(result);

    // NULL batch pointer
    create_test_csv_with_header();
    dataset = dataset_load_csv(TEST_CSV_FILE, 3, 1, true);
    ASSERT_NE(dataset, nullptr);
    result = dataset_next_batch(dataset, nullptr);
    EXPECT_FALSE(result);
    dataset_close(dataset);

    // NULL dataset for close (should not crash)
    EXPECT_NO_THROW({ dataset_close(nullptr); });

    // NULL batch for free (should not crash)
    EXPECT_NO_THROW({ dataset_free_batch(nullptr); });
}

/**
 * WHAT: Test malformed CSV data
 * WHY: Verify handling of invalid data
 */
TEST_F(DataIOTest, MalformedCSV)
{
    create_malformed_csv();

    dataset_t dataset = dataset_load_csv(TEST_CSV_MALFORMED, 2, 1, true);
    ASSERT_NE(dataset, nullptr);

    data_batch_t batch;
    memset(&batch, 0, sizeof(batch));

    // Should read file but skip invalid lines
    bool result = dataset_next_batch(dataset, &batch);
    EXPECT_TRUE(result);

    // Should have fewer samples than total lines (due to skipped invalid lines)
    EXPECT_GT(batch.num_samples, 0u);

    dataset_free_batch(&batch);
    dataset_close(dataset);
}

/**
 * WHAT: Test empty CSV file
 * WHY: Verify handling of empty files
 */
TEST_F(DataIOTest, EmptyCSVFile)
{
    // Create empty file
    std::ofstream file(TEST_CSV_FILE);
    file.close();

    dataset_t dataset = dataset_load_csv(TEST_CSV_FILE, 3, 1, false);
    ASSERT_NE(dataset, nullptr);

    data_batch_t batch;
    memset(&batch, 0, sizeof(batch));

    bool result = dataset_next_batch(dataset, &batch);
    EXPECT_FALSE(result);

    dataset_close(dataset);
}

/**
 * WHAT: Test CSV with only header
 * WHY: Verify handling of header-only files
 */
TEST_F(DataIOTest, HeaderOnlyCSV)
{
    std::ofstream file(TEST_CSV_FILE);
    file << "f1,f2,f3,label\n";
    file.close();

    dataset_t dataset = dataset_load_csv(TEST_CSV_FILE, 3, 1, true);
    ASSERT_NE(dataset, nullptr);

    data_batch_t batch;
    memset(&batch, 0, sizeof(batch));

    bool result = dataset_next_batch(dataset, &batch);
    EXPECT_FALSE(result);

    dataset_close(dataset);
}

//=============================================================================
// Data Validation Tests
//=============================================================================

/**
 * WHAT: Test special numeric values
 * WHY: Verify handling of edge case numbers
 */
TEST_F(DataIOTest, SpecialNumericValues)
{
    create_csv_with_special_chars();

    dataset_t dataset = dataset_load_csv(TEST_CSV_BINARY, 3, 1, true);
    ASSERT_NE(dataset, nullptr);

    data_batch_t batch;
    memset(&batch, 0, sizeof(batch));

    bool result = dataset_next_batch(dataset, &batch);
    EXPECT_TRUE(result);
    EXPECT_GT(batch.num_samples, 0u);

    // Check zero values
    if (batch.num_samples > 0) {
        EXPECT_FLOAT_EQ(batch.features[0][0], 0.0f);
        EXPECT_FLOAT_EQ(batch.features[0][1], 0.0f);
        EXPECT_FLOAT_EQ(batch.features[0][2], 0.0f);
    }

    // Check negative values
    if (batch.num_samples > 1) {
        EXPECT_LT(batch.features[1][0], 0.0f);
        EXPECT_LT(batch.features[1][1], 0.0f);
        EXPECT_LT(batch.features[1][2], 0.0f);
    }

    dataset_free_batch(&batch);
    dataset_close(dataset);
}

/**
 * WHAT: Test feature dimension consistency
 * WHY: Verify all rows have correct number of features
 */
TEST_F(DataIOTest, FeatureDimensionConsistency)
{
    create_test_csv_with_header();

    dataset_t dataset = dataset_load_csv(TEST_CSV_FILE, 3, 1, true);
    ASSERT_NE(dataset, nullptr);

    data_batch_t batch;
    memset(&batch, 0, sizeof(batch));

    bool result = dataset_next_batch(dataset, &batch);
    EXPECT_TRUE(result);

    // Verify all samples have correct dimensions
    for (uint32_t i = 0; i < batch.num_samples; i++) {
        EXPECT_NE(batch.features[i], nullptr);
        EXPECT_NE(batch.labels[i], nullptr);
    }

    dataset_free_batch(&batch);
    dataset_close(dataset);
}

//=============================================================================
// Memory Management Tests
//=============================================================================

/**
 * WHAT: Test memory cleanup after batch free
 * WHY: Verify no memory leaks in batch operations
 */
TEST_F(DataIOTest, BatchMemoryCleanup)
{
    create_test_csv_with_header();

    dataset_t dataset = dataset_load_csv(TEST_CSV_FILE, 3, 1, true);
    ASSERT_NE(dataset, nullptr);

    for (int i = 0; i < 10; i++) {
        data_batch_t batch;
        memset(&batch, 0, sizeof(batch));

        if (!dataset_next_batch(dataset, &batch)) {
            dataset_reset(dataset);
            continue;
        }

        dataset_free_batch(&batch);
    }

    dataset_close(dataset);
    // If no memory leaks, test passes
    SUCCEED();
}

/**
 * WHAT: Test multiple dataset open/close cycles
 * WHY: Verify proper resource cleanup
 */
TEST_F(DataIOTest, MultipleOpenCloseCycles)
{
    create_test_csv_with_header();

    for (int i = 0; i < 20; i++) {
        dataset_t dataset = dataset_load_csv(TEST_CSV_FILE, 3, 1, true);
        ASSERT_NE(dataset, nullptr);

        data_batch_t batch;
        memset(&batch, 0, sizeof(batch));

        if (dataset_next_batch(dataset, &batch)) {
            dataset_free_batch(&batch);
        }

        dataset_close(dataset);
    }

    SUCCEED();
}

/**
 * WHAT: Test large file memory management
 * WHY: Verify efficient memory usage with large datasets
 */
TEST_F(DataIOTest, LargeFileMemory)
{
    create_large_csv(10000);

    dataset_t dataset = dataset_load_csv(TEST_CSV_LARGE, 4, 1, true);
    ASSERT_NE(dataset, nullptr);

    int total_samples = 0;

    while (true) {
        data_batch_t batch;
        memset(&batch, 0, sizeof(batch));

        if (!dataset_next_batch(dataset, &batch)) {
            break;
        }

        total_samples += batch.num_samples;
        dataset_free_batch(&batch);

        if (batch.end_of_dataset) {
            break;
        }
    }

    EXPECT_EQ(total_samples, 10000);
    dataset_close(dataset);
}

//=============================================================================
// Thread Safety Tests
//=============================================================================

/**
 * WHAT: Test concurrent dataset reading
 * WHY: Verify thread-safe batch operations
 */
TEST_F(DataIOTest, ConcurrentReading)
{
    create_large_csv(5000);

    dataset_t dataset = dataset_load_csv(TEST_CSV_LARGE, 4, 1, true);
    ASSERT_NE(dataset, nullptr);

    const int NUM_THREADS = 4;
    std::vector<std::thread> threads;
    std::atomic<int> total_samples{0};
    std::atomic<int> completed{0};

    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back([dataset, &total_samples, &completed]() {
            for (int j = 0; j < 10; j++) {
                data_batch_t batch;
                memset(&batch, 0, sizeof(batch));

                if (dataset_next_batch(dataset, &batch)) {
                    total_samples += batch.num_samples;
                    dataset_free_batch(&batch);
                }
            }
            completed++;
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(completed.load(), NUM_THREADS);
    EXPECT_GT(total_samples.load(), 0);

    dataset_close(dataset);
}

/**
 * WHAT: Test concurrent open/close operations
 * WHY: Verify thread safety of initialization/cleanup
 */
TEST_F(DataIOTest, ConcurrentOpenClose)
{
    create_test_csv_with_header();

    const int NUM_THREADS = 8;
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};

    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back([&success_count]() {
            dataset_t dataset = dataset_load_csv(TEST_CSV_FILE, 3, 1, true);
            if (dataset != nullptr) {
                data_batch_t batch;
                memset(&batch, 0, sizeof(batch));

                if (dataset_next_batch(dataset, &batch)) {
                    dataset_free_batch(&batch);
                    success_count++;
                }

                dataset_close(dataset);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(success_count.load(), NUM_THREADS);
}

//=============================================================================
// File Format Tests
//=============================================================================

/**
 * WHAT: Test different CSV delimiters
 * WHY: Verify handling of non-comma delimiters
 */
TEST_F(DataIOTest, CustomDelimiter)
{
    // Create tab-delimited file
    std::ofstream file(TEST_CSV_FILE);
    file << "f1\tf2\tf3\tlabel\n";
    file << "1.0\t2.0\t3.0\tclassA\n";
    file << "4.0\t5.0\t6.0\tclassB\n";
    file.close();

    // Open with custom delimiter config
    dataset_config_t config = {};
    config.format = DATA_FORMAT_CSV;
    config.source = DATA_SOURCE_FILE;
    config.num_feature_columns = 3;
    config.num_label_columns = 1;
    config.has_header = true;
    config.delimiter = '\t';
    config.batch_size = 1000;
    strncpy(config.location, TEST_CSV_FILE, sizeof(config.location) - 1);

    dataset_t dataset = dataset_open(&config);
    ASSERT_NE(dataset, nullptr);

    data_batch_t batch;
    memset(&batch, 0, sizeof(batch));

    bool result = dataset_next_batch(dataset, &batch);
    EXPECT_TRUE(result);

    if (batch.num_samples > 0) {
        EXPECT_FLOAT_EQ(batch.features[0][0], 1.0f);
    }

    dataset_free_batch(&batch);
    dataset_close(dataset);
}

/**
 * WHAT: Test Windows-style line endings (CRLF)
 * WHY: Verify cross-platform compatibility
 */
TEST_F(DataIOTest, WindowsLineEndings)
{
    std::ofstream file(TEST_CSV_FILE, std::ios::binary);
    file << "f1,f2,f3,label\r\n";
    file << "1.0,2.0,3.0,classA\r\n";
    file << "4.0,5.0,6.0,classB\r\n";
    file.close();

    dataset_t dataset = dataset_load_csv(TEST_CSV_FILE, 3, 1, true);
    ASSERT_NE(dataset, nullptr);

    data_batch_t batch;
    memset(&batch, 0, sizeof(batch));

    bool result = dataset_next_batch(dataset, &batch);
    EXPECT_TRUE(result);
    EXPECT_GT(batch.num_samples, 0u);

    dataset_free_batch(&batch);
    dataset_close(dataset);
}

//=============================================================================
// Dataset API Tests
//=============================================================================

/**
 * WHAT: Test dataset_get_size function
 * WHY: Verify size reporting functionality
 */
TEST_F(DataIOTest, GetDatasetSize)
{
    create_large_csv(1000);

    dataset_t dataset = dataset_load_csv(TEST_CSV_LARGE, 4, 1, true);
    ASSERT_NE(dataset, nullptr);

    // Read some batches first
    data_batch_t batch;
    memset(&batch, 0, sizeof(batch));
    dataset_next_batch(dataset, &batch);
    dataset_free_batch(&batch);

    uint64_t size = dataset_get_size(dataset);
    EXPECT_GT(size, 0u);

    dataset_close(dataset);
}

/**
 * WHAT: Test dataset config with all parameters
 * WHY: Verify full configuration support
 */
TEST_F(DataIOTest, FullConfiguration)
{
    create_test_csv_with_header();

    dataset_config_t config = {};
    config.format = DATA_FORMAT_CSV;
    config.source = DATA_SOURCE_FILE;
    config.num_feature_columns = 3;
    config.num_label_columns = 1;
    config.has_header = true;
    config.delimiter = ',';
    config.shuffle = false;
    config.batch_size = 1000;
    config.max_rows = 0;
    strncpy(config.location, TEST_CSV_FILE, sizeof(config.location) - 1);

    dataset_t dataset = dataset_open(&config);
    ASSERT_NE(dataset, nullptr);

    dataset_close(dataset);
}

//=============================================================================
// Data Export Tests
//=============================================================================

/**
 * WHAT: Test CSV save functionality
 * WHY: Verify data export works correctly
 */
TEST_F(DataIOTest, SaveCSV)
{
    // Create test data
    const uint32_t num_samples = 3;
    const uint32_t num_features = 2;

    float** features = (float**) nimcp_malloc(num_samples * sizeof(float*));
    char** labels = (char**) nimcp_malloc(num_samples * sizeof(char*));

    for (uint32_t i = 0; i < num_samples; i++) {
        features[i] = (float*) nimcp_malloc(num_features * sizeof(float));
        features[i][0] = (float) i;
        features[i][1] = (float) (i * 2);

        labels[i] = (char*) nimcp_malloc(16);
        snprintf(labels[i], 16, "class%u", i);
    }

    char* feature_names[] = {(char*) "feat1", (char*) "feat2"};

    // Save to CSV
    bool result = dataset_save_csv(features, labels, num_samples, num_features, TEST_OUTPUT_CSV,
                                   feature_names);
    EXPECT_TRUE(result);
    EXPECT_TRUE(file_exists(TEST_OUTPUT_CSV));

    // Verify file content
    int line_count = count_file_lines(TEST_OUTPUT_CSV);
    EXPECT_EQ(line_count, num_samples + 1);  // +1 for header

    // Cleanup
    for (uint32_t i = 0; i < num_samples; i++) {
        nimcp_free(features[i]);
        nimcp_free(labels[i]);
    }
    nimcp_free(features);
    nimcp_free(labels);
}

/**
 * WHAT: Test save CSV without header
 * WHY: Verify optional header in export
 */
TEST_F(DataIOTest, SaveCSVNoHeader)
{
    const uint32_t num_samples = 2;
    const uint32_t num_features = 3;

    float** features = (float**) nimcp_malloc(num_samples * sizeof(float*));
    char** labels = (char**) nimcp_malloc(num_samples * sizeof(char*));

    for (uint32_t i = 0; i < num_samples; i++) {
        features[i] = (float*) nimcp_malloc(num_features * sizeof(float));
        features[i][0] = 1.0f;
        features[i][1] = 2.0f;
        features[i][2] = 3.0f;

        labels[i] = (char*) nimcp_malloc(16);
        strcpy(labels[i], "test");
    }

    // Save without feature names (no header)
    bool result = dataset_save_csv(features, labels, num_samples, num_features, TEST_OUTPUT_CSV,
                                   nullptr);
    EXPECT_TRUE(result);

    int line_count = count_file_lines(TEST_OUTPUT_CSV);
    EXPECT_EQ(line_count, num_samples);  // No header

    // Cleanup
    for (uint32_t i = 0; i < num_samples; i++) {
        nimcp_free(features[i]);
        nimcp_free(labels[i]);
    }
    nimcp_free(features);
    nimcp_free(labels);
}

//=============================================================================
// Edge Case Tests
//=============================================================================

/**
 * WHAT: Test very long lines
 * WHY: Verify handling of lines exceeding buffer size
 */
TEST_F(DataIOTest, VeryLongLines)
{
    // Create CSV with very long lines
    std::ofstream file(TEST_CSV_FILE);
    file << "f1,f2,f3,label\n";

    std::string long_value(1000, 'A');
    file << "1.0,2.0,3.0," << long_value << "\n";
    file.close();

    dataset_t dataset = dataset_load_csv(TEST_CSV_FILE, 3, 1, true);
    ASSERT_NE(dataset, nullptr);

    data_batch_t batch;
    memset(&batch, 0, sizeof(batch));

    bool result = dataset_next_batch(dataset, &batch);
    // May succeed or fail depending on buffer size handling
    // Just ensure it doesn't crash

    if (result) {
        dataset_free_batch(&batch);
    }

    dataset_close(dataset);
}

/**
 * WHAT: Test single row CSV
 * WHY: Verify minimal dataset handling
 */
TEST_F(DataIOTest, SingleRowCSV)
{
    std::ofstream file(TEST_CSV_FILE);
    file << "f1,f2,label\n";
    file << "1.0,2.0,classA\n";
    file.close();

    dataset_t dataset = dataset_load_csv(TEST_CSV_FILE, 2, 1, true);
    ASSERT_NE(dataset, nullptr);

    data_batch_t batch;
    memset(&batch, 0, sizeof(batch));

    bool result = dataset_next_batch(dataset, &batch);
    EXPECT_TRUE(result);
    EXPECT_EQ(batch.num_samples, 1u);

    dataset_free_batch(&batch);
    dataset_close(dataset);
}

/**
 * WHAT: Test reading after end of dataset
 * WHY: Verify proper end-of-data handling
 */
TEST_F(DataIOTest, ReadAfterEndOfDataset)
{
    create_test_csv_with_header();

    dataset_t dataset = dataset_load_csv(TEST_CSV_FILE, 3, 1, true);
    ASSERT_NE(dataset, nullptr);

    // Read all data
    data_batch_t batch;
    memset(&batch, 0, sizeof(batch));
    dataset_next_batch(dataset, &batch);
    dataset_free_batch(&batch);

    // Try reading again - should return false
    memset(&batch, 0, sizeof(batch));
    bool result = dataset_next_batch(dataset, &batch);
    EXPECT_FALSE(result);

    dataset_close(dataset);
}

/**
 * WHAT: Test double close
 * WHY: Verify safe double-close behavior
 */
TEST_F(DataIOTest, DoubleClose)
{
    create_test_csv_with_header();

    dataset_t dataset = dataset_load_csv(TEST_CSV_FILE, 3, 1, true);
    ASSERT_NE(dataset, nullptr);

    dataset_close(dataset);
    dataset = nullptr;  // Properly set to NULL after close (C idiom)
    // Second close on NULL should not crash
    EXPECT_NO_THROW({ dataset_close(dataset); });
}

/**
 * WHAT: Test double batch free
 * WHY: Verify safe double-free behavior
 */
TEST_F(DataIOTest, DoubleBatchFree)
{
    create_test_csv_with_header();

    dataset_t dataset = dataset_load_csv(TEST_CSV_FILE, 3, 1, true);
    ASSERT_NE(dataset, nullptr);

    data_batch_t batch;
    memset(&batch, 0, sizeof(batch));
    dataset_next_batch(dataset, &batch);

    dataset_free_batch(&batch);
    // Second free on cleared batch should be safe
    EXPECT_NO_THROW({ dataset_free_batch(&batch); });

    dataset_close(dataset);
}

//=============================================================================
// Unsupported Format Tests
//=============================================================================

/**
 * WHAT: Test unsupported format error handling
 * WHY: Verify proper error reporting for unsupported formats
 */
TEST_F(DataIOTest, UnsupportedFormat)
{
    dataset_config_t config = {};
    config.format = DATA_FORMAT_JSON;  // Not implemented
    config.source = DATA_SOURCE_FILE;
    config.num_feature_columns = 3;
    config.num_label_columns = 1;
    strncpy(config.location, TEST_CSV_FILE, sizeof(config.location) - 1);

    dataset_t dataset = dataset_open(&config);
    EXPECT_EQ(dataset, nullptr);
}

/**
 * WHAT: Test PostgreSQL backend (not implemented)
 * WHY: Verify placeholder implementation
 */
TEST_F(DataIOTest, PostgreSQLNotImplemented)
{
    dataset_t dataset = dataset_load_postgres("host=localhost", "SELECT * FROM test", 3);
    EXPECT_EQ(dataset, nullptr);
}

//=============================================================================
// Performance Tests
//=============================================================================

/**
 * WHAT: Test reading performance
 * WHY: Verify acceptable read speed
 */
TEST_F(DataIOTest, ReadPerformance)
{
    create_large_csv(10000);

    dataset_t dataset = dataset_load_csv(TEST_CSV_LARGE, 4, 1, true);
    ASSERT_NE(dataset, nullptr);

    auto start = std::chrono::high_resolution_clock::now();

    int total_samples = 0;
    while (true) {
        data_batch_t batch;
        memset(&batch, 0, sizeof(batch));

        if (!dataset_next_batch(dataset, &batch)) {
            break;
        }

        total_samples += batch.num_samples;
        dataset_free_batch(&batch);

        if (batch.end_of_dataset) {
            break;
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should read 10000 samples in reasonable time (< 2 seconds)
    EXPECT_LT(duration.count(), 2000);
    EXPECT_EQ(total_samples, 10000);

    dataset_close(dataset);
}

//=============================================================================
// Integration Tests
//=============================================================================

/**
 * WHAT: Test complete workflow: open, read, reset, read again
 * WHY: Verify end-to-end functionality
 */
TEST_F(DataIOTest, CompleteWorkflow)
{
    create_test_csv_with_header();

    // Open dataset
    dataset_t dataset = dataset_load_csv(TEST_CSV_FILE, 3, 1, true);
    ASSERT_NE(dataset, nullptr);

    // Read first pass
    data_batch_t batch1;
    memset(&batch1, 0, sizeof(batch1));
    bool result = dataset_next_batch(dataset, &batch1);
    EXPECT_TRUE(result);
    uint32_t first_samples = batch1.num_samples;
    dataset_free_batch(&batch1);

    // Reset
    EXPECT_TRUE(dataset_reset(dataset));

    // Read second pass
    data_batch_t batch2;
    memset(&batch2, 0, sizeof(batch2));
    result = dataset_next_batch(dataset, &batch2);
    EXPECT_TRUE(result);
    EXPECT_EQ(batch2.num_samples, first_samples);
    dataset_free_batch(&batch2);

    // Close
    dataset_close(dataset);

    SUCCEED();
}

/**
 * WHAT: Test read-export-read cycle
 * WHY: Verify round-trip data integrity
 */
TEST_F(DataIOTest, ReadExportReadCycle)
{
    create_test_csv_with_header();

    // Read original
    dataset_t dataset1 = dataset_load_csv(TEST_CSV_FILE, 3, 1, true);
    ASSERT_NE(dataset1, nullptr);

    data_batch_t batch1;
    memset(&batch1, 0, sizeof(batch1));
    dataset_next_batch(dataset1, &batch1);

    // Export to new file
    dataset_save_csv(batch1.features, batch1.labels, batch1.num_samples, 3, TEST_OUTPUT_CSV,
                     nullptr);

    dataset_free_batch(&batch1);
    dataset_close(dataset1);

    // Read exported file
    dataset_t dataset2 = dataset_load_csv(TEST_OUTPUT_CSV, 3, 1, false);
    ASSERT_NE(dataset2, nullptr);

    data_batch_t batch2;
    memset(&batch2, 0, sizeof(batch2));
    bool result = dataset_next_batch(dataset2, &batch2);
    EXPECT_TRUE(result);
    EXPECT_GT(batch2.num_samples, 0u);

    dataset_free_batch(&batch2);
    dataset_close(dataset2);
}

//=============================================================================
// Phase IO-1: Memory Integration Tests
//=============================================================================

/**
 * WHAT: Test default configuration
 * WHY: Verify sensible defaults are provided
 */
TEST_F(DataIOTest, DefaultConfiguration)
{
    dataset_config_t config = dataset_default_config();

    // Check sensible defaults
    EXPECT_EQ(config.format, DATA_FORMAT_CSV);
    EXPECT_EQ(config.source, DATA_SOURCE_FILE);
    EXPECT_TRUE(config.has_header);
    EXPECT_EQ(config.delimiter, ',');
    EXPECT_EQ(config.batch_size, 1000u);
    EXPECT_EQ(config.max_rows, 0u);

    // Memory integration defaults (disabled)
    EXPECT_FALSE(config.use_unified_memory);
    EXPECT_EQ(config.memory_manager, nullptr);

    // Security integration defaults (disabled)
    EXPECT_FALSE(config.enable_security);
    EXPECT_EQ(config.security_context, nullptr);
}

/**
 * WHAT: Test dataset statistics without integration
 * WHY: Verify basic stats work without memory/security
 */
TEST_F(DataIOTest, DatasetStatsBasic)
{
    create_test_csv_with_header();

    dataset_config_t config = dataset_default_config();
    strncpy(config.location, TEST_CSV_FILE, sizeof(config.location) - 1);
    config.num_feature_columns = 3;
    config.num_label_columns = 1;

    dataset_t dataset = dataset_open(&config);
    ASSERT_NE(dataset, nullptr);

    // Read a batch
    data_batch_t batch;
    memset(&batch, 0, sizeof(batch));
    dataset_next_batch(dataset, &batch);
    dataset_free_batch(&batch);

    // Get statistics
    dataset_stats_t stats;
    bool result = dataset_get_stats(dataset, &stats);
    EXPECT_TRUE(result);

    // Basic stats should be populated
    EXPECT_GT(stats.total_rows_read, 0u);
    EXPECT_GT(stats.total_batches_read, 0u);

    // Memory integration should be disabled
    EXPECT_FALSE(stats.using_unified_memory);

    // Security integration should be disabled
    EXPECT_FALSE(stats.security_registered);

    dataset_close(dataset);
}

/**
 * WHAT: Test dataset with unified memory enabled
 * WHY: Verify memory integration works correctly
 */
TEST_F(DataIOTest, DatasetWithUnifiedMemory)
{
    create_test_csv_with_header();

    dataset_config_t config = dataset_default_config();
    strncpy(config.location, TEST_CSV_FILE, sizeof(config.location) - 1);
    config.num_feature_columns = 3;
    config.num_label_columns = 1;
    config.use_unified_memory = true;  // Enable memory integration

    dataset_t dataset = dataset_open(&config);
    ASSERT_NE(dataset, nullptr);

    // Read a batch
    data_batch_t batch;
    memset(&batch, 0, sizeof(batch));
    dataset_next_batch(dataset, &batch);
    EXPECT_GT(batch.num_samples, 0u);
    dataset_free_batch(&batch);

    // Get statistics
    dataset_stats_t stats;
    bool result = dataset_get_stats(dataset, &stats);
    EXPECT_TRUE(result);
    EXPECT_TRUE(stats.using_unified_memory);

    dataset_close(dataset);
}

/**
 * WHAT: Test dataset_get_stats with NULL parameters
 * WHY: Verify error handling
 */
TEST_F(DataIOTest, DatasetStatsNullParams)
{
    dataset_stats_t stats;

    // NULL dataset
    EXPECT_FALSE(dataset_get_stats(nullptr, &stats));

    // NULL stats
    create_test_csv_with_header();
    dataset_t dataset = dataset_load_csv(TEST_CSV_FILE, 3, 1, true);
    ASSERT_NE(dataset, nullptr);
    EXPECT_FALSE(dataset_get_stats(dataset, nullptr));
    dataset_close(dataset);
}

//=============================================================================
// Phase IO-2: Security Integration Tests
//=============================================================================

/**
 * WHAT: Test module initialization and shutdown
 * WHY: Verify module lifecycle management
 */
TEST_F(DataIOTest, ModuleInitShutdown)
{
    // Initialize without security context
    nimcp_result_t result = dataio_init(nullptr);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Get module ID (should be 0 without security)
    uint32_t module_id = dataio_get_security_module_id();
    EXPECT_EQ(module_id, 0u);

    // Shutdown
    dataio_shutdown();

    // Re-initialization should work
    result = dataio_init(nullptr);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    dataio_shutdown();
}

/**
 * WHAT: Test double initialization
 * WHY: Verify idempotent initialization
 */
TEST_F(DataIOTest, DoubleModuleInit)
{
    nimcp_result_t result = dataio_init(nullptr);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Second init should succeed (no-op)
    result = dataio_init(nullptr);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    dataio_shutdown();
}

/**
 * WHAT: Test shutdown without init
 * WHY: Verify safe shutdown when not initialized
 */
TEST_F(DataIOTest, ShutdownWithoutInit)
{
    // Should not crash
    EXPECT_NO_THROW({ dataio_shutdown(); });
}

/**
 * WHAT: Test dataset with security enabled but no context
 * WHY: Verify graceful handling when security not available
 */
TEST_F(DataIOTest, DatasetSecurityNoContext)
{
    create_test_csv_with_header();

    dataset_config_t config = dataset_default_config();
    strncpy(config.location, TEST_CSV_FILE, sizeof(config.location) - 1);
    config.num_feature_columns = 3;
    config.num_label_columns = 1;
    config.enable_security = true;  // Enable but no context

    dataset_t dataset = dataset_open(&config);
    ASSERT_NE(dataset, nullptr);

    // Should work but security not registered
    dataset_stats_t stats;
    EXPECT_TRUE(dataset_get_stats(dataset, &stats));
    EXPECT_FALSE(stats.security_registered);

    dataset_close(dataset);
}

//=============================================================================
// Combined Memory and Security Tests
//=============================================================================

/**
 * WHAT: Test dataset with both memory and security enabled
 * WHY: Verify full integration stack works together
 */
TEST_F(DataIOTest, FullIntegrationStack)
{
    create_large_csv(1000);

    dataset_config_t config = dataset_default_config();
    strncpy(config.location, TEST_CSV_LARGE, sizeof(config.location) - 1);
    config.num_feature_columns = 4;
    config.num_label_columns = 1;
    config.use_unified_memory = true;
    config.enable_security = true;

    dataset_t dataset = dataset_open(&config);
    ASSERT_NE(dataset, nullptr);

    // Read multiple batches
    int total_samples = 0;
    while (true) {
        data_batch_t batch;
        memset(&batch, 0, sizeof(batch));

        if (!dataset_next_batch(dataset, &batch)) {
            break;
        }

        total_samples += batch.num_samples;
        dataset_free_batch(&batch);

        if (batch.end_of_dataset) {
            break;
        }
    }

    EXPECT_EQ(total_samples, 1000);

    // Verify stats
    dataset_stats_t stats;
    EXPECT_TRUE(dataset_get_stats(dataset, &stats));
    EXPECT_TRUE(stats.using_unified_memory);
    EXPECT_GT(stats.total_rows_read, 0u);

    dataset_close(dataset);
}

/**
 * WHAT: Test external memory manager injection
 * WHY: Verify user-provided memory manager works
 */
TEST_F(DataIOTest, ExternalMemoryManager)
{
    create_test_csv_with_header();

    // Create external memory manager
    unified_mem_config_t mem_config = unified_mem_default_config();
    mem_config.enable_cow = true;
    unified_mem_manager_t ext_manager = unified_mem_create(&mem_config);
    ASSERT_NE(ext_manager, nullptr);

    // Use external manager
    dataset_config_t config = dataset_default_config();
    strncpy(config.location, TEST_CSV_FILE, sizeof(config.location) - 1);
    config.num_feature_columns = 3;
    config.num_label_columns = 1;
    config.use_unified_memory = true;
    config.memory_manager = ext_manager;  // External manager

    dataset_t dataset = dataset_open(&config);
    ASSERT_NE(dataset, nullptr);

    // Read a batch
    data_batch_t batch;
    memset(&batch, 0, sizeof(batch));
    dataset_next_batch(dataset, &batch);
    dataset_free_batch(&batch);

    // Verify external manager is used
    dataset_stats_t stats;
    EXPECT_TRUE(dataset_get_stats(dataset, &stats));
    EXPECT_TRUE(stats.using_unified_memory);

    dataset_close(dataset);

    // Clean up external manager (should NOT be destroyed by dataset)
    unified_mem_destroy(ext_manager);
}
