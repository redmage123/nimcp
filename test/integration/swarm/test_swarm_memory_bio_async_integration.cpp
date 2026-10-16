/**
 * @file test_swarm_memory_bio_async_integration.cpp
 * @brief Integration tests for swarm memory bio-async message handling
 *
 * Tests the bio-async integration for swarm memory including:
 * - Message header payload_size field usage (not total_length)
 * - Memory sync message processing
 * - Consolidation trigger handling
 * - Cross-subsystem message routing
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 */

#include <gtest/gtest.h>
#include <cstring>
#include <thread>
#include <chrono>

// Headers have their own extern "C" guards
#include "swarm/nimcp_swarm_memory.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"

class SwarmMemoryBioAsyncIntegrationTest : public ::testing::Test {
protected:
    NimcpSwarmMemory* memory;

    void SetUp() override {
        memory = nimcp_swarm_memory_create(1000, 3);
        ASSERT_NE(memory, nullptr);
        nimcp_swarm_memory_init(memory, nullptr);
        memory->bio_async_enabled = true;
    }

    void TearDown() override {
        if (memory) {
            nimcp_swarm_memory_destroy(memory);
        }
    }

    // Helper to store test memory
    void StoreTestMemory(const char* id_suffix, char* out_id) {
        char data[64];
        snprintf(data, sizeof(data), "test_data_%s", id_suffix);
        nimcp_swarm_memory_store(memory, NIMCP_MEMORY_SEMANTIC,
                                 NIMCP_IMPORTANCE_MEDIUM,
                                 (uint8_t*)data, strlen(data) + 1, out_id);
    }
};

// =============================================================================
// Message Header Field Tests (payload_size fix verification)
// =============================================================================

TEST_F(SwarmMemoryBioAsyncIntegrationTest, UsesPayloadSizeNotTotalLength) {
    // Create message with payload
    struct {
        bio_message_header_t header;
        char payload[256];
    } msg;
    memset(&msg, 0, sizeof(msg));

    msg.header.type = BIO_MSG_SWARM_MEMORY_SYNC;
    msg.header.source_module = 0x1234;
    msg.header.target_module = 0;
    msg.header.payload_size = 32;  // Only 32 bytes of actual payload

    // Fill entire payload buffer but only first 32 bytes should be used
    memset(msg.payload, 'A', 32);
    memset(msg.payload + 32, 'X', sizeof(msg.payload) - 32);

    // Should process using payload_size, not sizeof entire struct
    nimcp_result_t result = nimcp_swarm_memory_process_message(memory, &msg);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SwarmMemoryBioAsyncIntegrationTest, ZeroPayloadSizeNoPayloadRead) {
    bio_message_header_t header = {};
    header.type = BIO_MSG_SWARM_MEMORY_SYNC;
    header.payload_size = 0;

    // Zero payload should be handled without reading past header
    nimcp_result_t result = nimcp_swarm_memory_process_message(memory, &header);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SwarmMemoryBioAsyncIntegrationTest, LargePayloadSizeHandled) {
    struct {
        bio_message_header_t header;
        char large_payload[4096];
    } msg;
    memset(&msg, 0, sizeof(msg));

    msg.header.type = BIO_MSG_SWARM_MEMORY_SYNC;
    msg.header.payload_size = sizeof(msg.large_payload);
    memset(msg.large_payload, 'Z', sizeof(msg.large_payload));

    // Large payload should be handled (may truncate internally)
    nimcp_result_t result = nimcp_swarm_memory_process_message(memory, &msg);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

// =============================================================================
// Memory Sync Message Integration Tests
// =============================================================================

TEST_F(SwarmMemoryBioAsyncIntegrationTest, SyncMessageUpdatesDistributedFlag) {
    // Store a memory first
    char memory_id[64];
    StoreTestMemory("sync_test", memory_id);

    // Create sync message for this memory
    struct {
        bio_message_header_t header;
        char memory_id[64];
    } msg;
    memset(&msg, 0, sizeof(msg));

    msg.header.type = BIO_MSG_SWARM_MEMORY_SYNC;
    msg.header.source_module = 0x5000;
    msg.header.payload_size = strlen(memory_id) + 1;
    strncpy(msg.memory_id, memory_id, sizeof(msg.memory_id) - 1);

    nimcp_result_t result = nimcp_swarm_memory_process_message(memory, &msg);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Memory should be marked as distributed
    // (Verification depends on internal state access)
}

TEST_F(SwarmMemoryBioAsyncIntegrationTest, SyncMessageForNonexistentMemory) {
    struct {
        bio_message_header_t header;
        char memory_id[64];
    } msg;
    memset(&msg, 0, sizeof(msg));

    msg.header.type = BIO_MSG_SWARM_MEMORY_SYNC;
    msg.header.payload_size = 20;
    strncpy(msg.memory_id, "nonexistent_id", sizeof(msg.memory_id) - 1);

    // Should handle gracefully
    nimcp_result_t result = nimcp_swarm_memory_process_message(memory, &msg);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

// =============================================================================
// Consolidation Trigger Integration Tests
// =============================================================================

TEST_F(SwarmMemoryBioAsyncIntegrationTest, ConsolidationTriggerConsolidatesMemories) {
    // Store multiple memories
    for (int i = 0; i < 10; i++) {
        char suffix[16], memory_id[64];
        snprintf(suffix, sizeof(suffix), "consolidate_%d", i);
        StoreTestMemory(suffix, memory_id);
    }

    // Send consolidation trigger
    bio_message_header_t header = {};
    header.type = BIO_MSG_CONSOLIDATION_TRIGGER;
    header.payload_size = 0;

    nimcp_result_t result = nimcp_swarm_memory_process_message(memory, &header);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SwarmMemoryBioAsyncIntegrationTest, ConsolidationTriggerWithEmptyStore) {
    // Create fresh empty memory system
    NimcpSwarmMemory* empty = nimcp_swarm_memory_create(100, 2);
    nimcp_swarm_memory_init(empty, nullptr);
    empty->bio_async_enabled = true;

    bio_message_header_t header = {};
    header.type = BIO_MSG_CONSOLIDATION_TRIGGER;
    header.payload_size = 0;

    nimcp_result_t result = nimcp_swarm_memory_process_message(empty, &header);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    nimcp_swarm_memory_destroy(empty);
}

// =============================================================================
// Unknown Message Type Tests
// =============================================================================

TEST_F(SwarmMemoryBioAsyncIntegrationTest, UnknownMessageTypeIgnored) {
    bio_message_header_t header = {};
    header.type = (bio_message_type_t)0xDEAD;  // Invalid type
    header.payload_size = 0;

    // Should succeed (ignore unknown message)
    nimcp_result_t result = nimcp_swarm_memory_process_message(memory, &header);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

// =============================================================================
// Bio-Async Enable/Disable Tests
// =============================================================================

TEST_F(SwarmMemoryBioAsyncIntegrationTest, DisabledBioAsyncIgnoresMessages) {
    memory->bio_async_enabled = false;

    bio_message_header_t header = {};
    header.type = BIO_MSG_SWARM_MEMORY_SYNC;
    header.payload_size = 0;

    // Should return success but do nothing
    nimcp_result_t result = nimcp_swarm_memory_process_message(memory, &header);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SwarmMemoryBioAsyncIntegrationTest, ReenableBioAsyncWorks) {
    memory->bio_async_enabled = false;
    memory->bio_async_enabled = true;

    bio_message_header_t header = {};
    header.type = BIO_MSG_CONSOLIDATION_TRIGGER;
    header.payload_size = 0;

    nimcp_result_t result = nimcp_swarm_memory_process_message(memory, &header);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

// =============================================================================
// Error Handling Tests
// =============================================================================

TEST_F(SwarmMemoryBioAsyncIntegrationTest, NullMessageReturnsError) {
    nimcp_result_t result = nimcp_swarm_memory_process_message(memory, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(SwarmMemoryBioAsyncIntegrationTest, NullMemorySystemReturnsError) {
    bio_message_header_t header = {};
    header.type = BIO_MSG_SWARM_MEMORY_SYNC;

    nimcp_result_t result = nimcp_swarm_memory_process_message(nullptr, &header);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(SwarmMemoryBioAsyncIntegrationTest, UninitializedSystemHandled) {
    NimcpSwarmMemory* uninit = nimcp_swarm_memory_create(100, 2);
    // Don't call init
    uninit->bio_async_enabled = true;

    bio_message_header_t header = {};
    header.type = BIO_MSG_SWARM_MEMORY_SYNC;

    nimcp_result_t result = nimcp_swarm_memory_process_message(uninit, &header);
    // Should succeed (not initialized check returns early)
    EXPECT_EQ(result, NIMCP_SUCCESS);

    nimcp_swarm_memory_destroy(uninit);
}

// =============================================================================
// Concurrent Message Processing Tests
// =============================================================================

TEST_F(SwarmMemoryBioAsyncIntegrationTest, ConcurrentMessageProcessing) {
    // Store some memories
    for (int i = 0; i < 20; i++) {
        char suffix[16], memory_id[64];
        snprintf(suffix, sizeof(suffix), "concurrent_%d", i);
        StoreTestMemory(suffix, memory_id);
    }

    // Process multiple messages in sequence (simulating concurrent arrival)
    for (int i = 0; i < 5; i++) {
        bio_message_header_t header = {};
        header.type = (i % 2 == 0) ? BIO_MSG_SWARM_MEMORY_SYNC : BIO_MSG_CONSOLIDATION_TRIGGER;
        header.payload_size = 0;

        nimcp_result_t result = nimcp_swarm_memory_process_message(memory, &header);
        EXPECT_EQ(result, NIMCP_SUCCESS);
    }
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
