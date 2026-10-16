#include <gtest/gtest.h>
// Headers have their own extern "C" guards
#include "middleware/buffering/nimcp_circular_buffer.h"

class CircularBufferTest : public ::testing::Test {
protected:
    circular_buffer_t* buffer;
    
    void SetUp() override {
        buffer = circular_buffer_create(sizeof(float), 10, OVERFLOW_OVERWRITE);
        ASSERT_NE(buffer, nullptr);
    }
    
    void TearDown() override {
        circular_buffer_destroy(buffer);
    }
};

TEST_F(CircularBufferTest, CreateAndDestroy) {
    EXPECT_NE(buffer, nullptr);
    EXPECT_EQ(circular_buffer_capacity(buffer), 10);
    EXPECT_EQ(circular_buffer_size(buffer), 0);
}

TEST_F(CircularBufferTest, NullHandling) {
    EXPECT_EQ(circular_buffer_size(nullptr), 0);
    EXPECT_EQ(circular_buffer_capacity(nullptr), 0);
    EXPECT_TRUE(circular_buffer_is_empty(nullptr));
    circular_buffer_destroy(nullptr);  // Should not crash
}

TEST_F(CircularBufferTest, PushAndPop) {
    float value_in = 42.0f;
    float value_out = 0.0f;
    
    EXPECT_TRUE(circular_buffer_push(buffer, &value_in));
    EXPECT_EQ(circular_buffer_size(buffer), 1);
    EXPECT_FALSE(circular_buffer_is_empty(buffer));
    
    EXPECT_TRUE(circular_buffer_pop(buffer, &value_out));
    EXPECT_FLOAT_EQ(value_out, value_in);
    EXPECT_EQ(circular_buffer_size(buffer), 0);
}

TEST_F(CircularBufferTest, FillAndEmpty) {
    for (int i = 0; i < 10; i++) {
        float val = (float)i;
        EXPECT_TRUE(circular_buffer_push(buffer, &val));
    }
    
    EXPECT_TRUE(circular_buffer_is_full(buffer));
    EXPECT_EQ(circular_buffer_size(buffer), 10);
    
    for (int i = 0; i < 10; i++) {
        float val;
        EXPECT_TRUE(circular_buffer_pop(buffer, &val));
        EXPECT_FLOAT_EQ(val, (float)i);
    }
    
    EXPECT_TRUE(circular_buffer_is_empty(buffer));
}

TEST_F(CircularBufferTest, OverflowOverwrite) {
    for (int i = 0; i < 15; i++) {
        float val = (float)i;
        EXPECT_TRUE(circular_buffer_push(buffer, &val));
    }
    
    EXPECT_EQ(circular_buffer_size(buffer), 10);
    
    float first;
    EXPECT_TRUE(circular_buffer_pop(buffer, &first));
    EXPECT_FLOAT_EQ(first, 5.0f);  // First 5 were overwritten
}

TEST_F(CircularBufferTest, OverflowError) {
    circular_buffer_t* err_buf = circular_buffer_create(sizeof(float), 5, OVERFLOW_ERROR);
    
    for (int i = 0; i < 5; i++) {
        float val = (float)i;
        EXPECT_TRUE(circular_buffer_push(err_buf, &val));
    }
    
    float val = 999.0f;
    EXPECT_FALSE(circular_buffer_push(err_buf, &val));  // Should fail
    
    circular_buffer_destroy(err_buf);
}

TEST_F(CircularBufferTest, PeekOperations) {
    for (int i = 0; i < 5; i++) {
        float val = (float)i;
        circular_buffer_push(buffer, &val);
    }
    
    float peeked;
    EXPECT_TRUE(circular_buffer_peek(buffer, 0, &peeked));
    EXPECT_FLOAT_EQ(peeked, 0.0f);
    
    EXPECT_TRUE(circular_buffer_peek(buffer, 4, &peeked));
    EXPECT_FLOAT_EQ(peeked, 4.0f);
    
    EXPECT_FALSE(circular_buffer_peek(buffer, 10, &peeked));  // Out of range
    
    EXPECT_EQ(circular_buffer_size(buffer), 5);  // Size unchanged
}

TEST_F(CircularBufferTest, BatchPush) {
    float values[5] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    
    size_t pushed = circular_buffer_push_batch(buffer, values, 5);
    EXPECT_EQ(pushed, 5);
    EXPECT_EQ(circular_buffer_size(buffer), 5);
}

TEST_F(CircularBufferTest, BatchPop) {
    float in_values[5] = {10.0f, 20.0f, 30.0f, 40.0f, 50.0f};
    float out_values[5] = {0};
    
    circular_buffer_push_batch(buffer, in_values, 5);
    
    size_t popped = circular_buffer_pop_batch(buffer, out_values, 5);
    EXPECT_EQ(popped, 5);
    
    for (int i = 0; i < 5; i++) {
        EXPECT_FLOAT_EQ(out_values[i], in_values[i]);
    }
}

TEST_F(CircularBufferTest, Clear) {
    for (int i = 0; i < 5; i++) {
        float val = (float)i;
        circular_buffer_push(buffer, &val);
    }
    
    EXPECT_EQ(circular_buffer_size(buffer), 5);
    
    circular_buffer_clear(buffer);
    
    EXPECT_EQ(circular_buffer_size(buffer), 0);
    EXPECT_TRUE(circular_buffer_is_empty(buffer));
}

TEST_F(CircularBufferTest, Utilization) {
    EXPECT_FLOAT_EQ(circular_buffer_utilization(buffer), 0.0f);
    
    for (int i = 0; i < 5; i++) {
        float val = (float)i;
        circular_buffer_push(buffer, &val);
    }
    
    EXPECT_FLOAT_EQ(circular_buffer_utilization(buffer), 50.0f);
    
    for (int i = 0; i < 5; i++) {
        float val = (float)i;
        circular_buffer_push(buffer, &val);
    }
    
    EXPECT_FLOAT_EQ(circular_buffer_utilization(buffer), 100.0f);
}

TEST_F(CircularBufferTest, Statistics) {
    circular_buffer_stats_t stats;
    
    for (int i = 0; i < 15; i++) {
        float val = (float)i;
        circular_buffer_push(buffer, &val);
    }
    
    circular_buffer_get_stats(buffer, &stats);
    
    EXPECT_EQ(stats.total_writes, 15);
    EXPECT_GT(stats.overflows, 0);  // Should have overflows
}

TEST_F(CircularBufferTest, ResetStats) {
    for (int i = 0; i < 5; i++) {
        float val = (float)i;
        circular_buffer_push(buffer, &val);
    }
    
    circular_buffer_reset_stats(buffer);
    
    circular_buffer_stats_t stats;
    circular_buffer_get_stats(buffer, &stats);
    
    EXPECT_EQ(stats.total_writes, 0);
    EXPECT_EQ(stats.total_reads, 0);
}

TEST_F(CircularBufferTest, WrapAround) {
    for (int i = 0; i < 20; i++) {
        float val = (float)i;
        circular_buffer_push(buffer, &val);
    }
    
    float values[10];
    size_t popped = circular_buffer_pop_batch(buffer, values, 10);
    
    EXPECT_EQ(popped, 10);
    for (int i = 0; i < 10; i++) {
        EXPECT_FLOAT_EQ(values[i], (float)(i + 10));  // Last 10 values
    }
}

TEST_F(CircularBufferTest, EmptyPop) {
    float val;
    EXPECT_FALSE(circular_buffer_pop(buffer, &val));
    
    circular_buffer_stats_t stats;
    circular_buffer_get_stats(buffer, &stats);
    EXPECT_GT(stats.underflows, 0);
}

TEST_F(CircularBufferTest, LargeElements) {
    struct LargeStruct {
        double data[100];
    };
    
    circular_buffer_t* large_buf = circular_buffer_create(
        sizeof(LargeStruct), 5, OVERFLOW_OVERWRITE
    );
    
    ASSERT_NE(large_buf, nullptr);
    
    LargeStruct in, out;
    for (int i = 0; i < 100; i++) {
        in.data[i] = i * 3.14159;
    }
    
    EXPECT_TRUE(circular_buffer_push(large_buf, &in));
    EXPECT_TRUE(circular_buffer_pop(large_buf, &out));
    
    for (int i = 0; i < 100; i++) {
        EXPECT_DOUBLE_EQ(out.data[i], in.data[i]);
    }
    
    circular_buffer_destroy(large_buf);
}

TEST_F(CircularBufferTest, ZeroCapacity) {
    circular_buffer_t* zero_buf = circular_buffer_create(sizeof(float), 0, OVERFLOW_OVERWRITE);
    EXPECT_EQ(zero_buf, nullptr);
}

TEST_F(CircularBufferTest, ZeroElementSize) {
    circular_buffer_t* zero_buf = circular_buffer_create(0, 10, OVERFLOW_OVERWRITE);
    EXPECT_EQ(zero_buf, nullptr);
}

TEST_F(CircularBufferTest, MultipleDataTypes) {
    circular_buffer_t* int_buf = circular_buffer_create(sizeof(int), 5, OVERFLOW_OVERWRITE);
    
    for (int i = 0; i < 5; i++) {
        EXPECT_TRUE(circular_buffer_push(int_buf, &i));
    }
    
    for (int i = 0; i < 5; i++) {
        int val;
        EXPECT_TRUE(circular_buffer_pop(int_buf, &val));
        EXPECT_EQ(val, i);
    }
    
    circular_buffer_destroy(int_buf);
}

TEST_F(CircularBufferTest, PeekAfterWrap) {
    for (int i = 0; i < 15; i++) {
        float val = (float)i;
        circular_buffer_push(buffer, &val);
    }
    
    float peeked;
    EXPECT_TRUE(circular_buffer_peek(buffer, 0, &peeked));
    EXPECT_FLOAT_EQ(peeked, 5.0f);  // After wrapping, oldest is 5
}

TEST_F(CircularBufferTest, BatchOperationPartial) {
    float values[20];
    for (int i = 0; i < 20; i++) values[i] = (float)i;
    
    circular_buffer_t* err_buf = circular_buffer_create(sizeof(float), 10, OVERFLOW_ERROR);
    
    size_t pushed = circular_buffer_push_batch(err_buf, values, 20);
    EXPECT_EQ(pushed, 10);  // Only first 10 should succeed
    
    circular_buffer_destroy(err_buf);
}

TEST_F(CircularBufferTest, ConcurrentPatterns) {
    // Test alternating push/pop pattern
    for (int cycle = 0; cycle < 100; cycle++) {
        float val_in = (float)cycle;
        float val_out;
        
        circular_buffer_push(buffer, &val_in);
        circular_buffer_pop(buffer, &val_out);
        
        EXPECT_FLOAT_EQ(val_out, val_in);
    }
    
    EXPECT_TRUE(circular_buffer_is_empty(buffer));
}

TEST_F(CircularBufferTest, StressTest) {
    const int ITERATIONS = 10000;
    
    for (int i = 0; i < ITERATIONS; i++) {
        float val = (float)i;
        circular_buffer_push(buffer, &val);
        
        if (i % 3 == 0) {
            float dummy;
            circular_buffer_pop(buffer, &dummy);
        }
    }
    
    EXPECT_GT(circular_buffer_size(buffer), 0);
    EXPECT_LE(circular_buffer_size(buffer), 10);
}

TEST_F(CircularBufferTest, UtilizationTracking) {
    circular_buffer_stats_t stats;
    
    for (int i = 0; i < 5; i++) {
        float val = (float)i;
        circular_buffer_push(buffer, &val);
    }
    
    circular_buffer_get_stats(buffer, &stats);
    EXPECT_GT(stats.peak_usage, 0);
    EXPECT_GT(stats.avg_usage, 0.0f);
}

TEST_F(CircularBufferTest, NullPointerSafety) {
    float val = 1.0f;
    EXPECT_FALSE(circular_buffer_push(nullptr, &val));
    EXPECT_FALSE(circular_buffer_push(buffer, nullptr));
    EXPECT_FALSE(circular_buffer_pop(nullptr, &val));
    EXPECT_FALSE(circular_buffer_pop(buffer, nullptr));
    EXPECT_FALSE(circular_buffer_peek(nullptr, 0, &val));
    EXPECT_FALSE(circular_buffer_peek(buffer, 0, nullptr));
}

TEST_F(CircularBufferTest, BatchNullSafety) {
    float values[5] = {1, 2, 3, 4, 5};
    EXPECT_EQ(circular_buffer_push_batch(nullptr, values, 5), 0);
    EXPECT_EQ(circular_buffer_push_batch(buffer, nullptr, 5), 0);
    EXPECT_EQ(circular_buffer_pop_batch(nullptr, values, 5), 0);
    EXPECT_EQ(circular_buffer_pop_batch(buffer, nullptr, 5), 0);
}

TEST_F(CircularBufferTest, ZeroCountBatch) {
    float values[5] = {1, 2, 3, 4, 5};
    EXPECT_EQ(circular_buffer_push_batch(buffer, values, 0), 0);
    EXPECT_EQ(circular_buffer_pop_batch(buffer, values, 0), 0);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
