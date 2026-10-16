/**
 * @file test_embedding_thread_safety.cpp
 * @brief Thread safety tests for embedding layer
 *
 * WHAT: Verify embedding create uses per-instance RNG (no global state)
 * WHY:  Bug H20 replaced global static RNG with per-instance PRNG
 * HOW:  Create multiple embeddings concurrently, verify no crash and
 *       that different instances get different weights
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <cmath>
#include <cstring>

#include "generation/nimcp_embedding.h"

class EmbeddingThreadSafetyTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(EmbeddingThreadSafetyTest, CreateDestroy_Lifecycle) {
    embedding_config_t cfg = embedding_default_config(100, 32);
    embedding_layer_t* emb = embedding_create(&cfg);
    ASSERT_NE(emb, nullptr);
    EXPECT_EQ(embedding_get_vocab_size(emb), 100u);
    EXPECT_EQ(embedding_get_dim(emb), 32u);
    embedding_destroy(emb);
}

TEST_F(EmbeddingThreadSafetyTest, NullDestroy_Safe) {
    embedding_destroy(nullptr);
}

TEST_F(EmbeddingThreadSafetyTest, PerInstanceRNG_DifferentWeights) {
    // Two embeddings created with same config should get different weights
    // because per-instance RNG uses pointer address for seeding
    embedding_config_t cfg = embedding_default_config(50, 16);
    embedding_layer_t* emb1 = embedding_create(&cfg);
    embedding_layer_t* emb2 = embedding_create(&cfg);
    ASSERT_NE(emb1, nullptr);
    ASSERT_NE(emb2, nullptr);

    // Look up the same token from both; weights should differ
    float vec1[16], vec2[16];
    EXPECT_EQ(embedding_lookup(emb1, 0, vec1), 0);
    EXPECT_EQ(embedding_lookup(emb2, 0, vec2), 0);

    // It is statistically near-impossible for 16 floats from different seeds
    // to be identical
    bool all_same = true;
    for (int i = 0; i < 16; i++) {
        if (std::fabs(vec1[i] - vec2[i]) > 1e-10f) {
            all_same = false;
            break;
        }
    }
    EXPECT_FALSE(all_same) << "Two embeddings should have different random weights";

    embedding_destroy(emb1);
    embedding_destroy(emb2);
}

TEST_F(EmbeddingThreadSafetyTest, ConcurrentCreate_NoCrash) {
    const int num_threads = 4;
    std::vector<embedding_layer_t*> results(num_threads, nullptr);
    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([&results, t]() {
            embedding_config_t cfg = embedding_default_config(50, 16);
            results[t] = embedding_create(&cfg);
        });
    }

    for (auto& th : threads) th.join();

    for (int t = 0; t < num_threads; t++) {
        ASSERT_NE(results[t], nullptr) << "Thread " << t << " failed to create embedding";
        EXPECT_EQ(embedding_get_vocab_size(results[t]), 50u);
        embedding_destroy(results[t]);
    }
}

TEST_F(EmbeddingThreadSafetyTest, ConcurrentLookup_NoCrash) {
    embedding_config_t cfg = embedding_default_config(100, 32);
    embedding_layer_t* emb = embedding_create(&cfg);
    ASSERT_NE(emb, nullptr);

    const int num_threads = 4;
    const int iterations = 200;
    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([emb, t, iterations]() {
            float output[32];
            for (int i = 0; i < iterations; i++) {
                uint32_t token_id = (uint32_t)((t * 25 + i) % 100);
                embedding_lookup(emb, token_id, output);
            }
        });
    }

    for (auto& th : threads) th.join();
    embedding_destroy(emb);
}
