/**
 * @file test_replication.cpp
 * @brief Tests for brain replication functionality
 *
 * WHAT: Verify brain replication works correctly
 * WHY: Production Artemis needs distributed brain synchronization
 * HOW: Unit tests for replication API with filesystem backend
 */

#include "test_helpers.h"

#include "core/brain/nimcp_brain.h"
#include "networking/replication/nimcp_replication.h"

#include <gtest/gtest.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

//=============================================================================
// Test Fixtures and Helpers
//=============================================================================

/**
 * WHAT: Test fixture for replication tests
 * WHY: Set up/tear down shared directory and cluster
 */
class ReplicationTest : public ::testing::Test {
   protected:
    const char* test_dir = "/tmp/nimcp_replication_test";
    replication_cluster_t cluster;
    brain_t brain;
    bool brain_registered;  // Track if brain was registered with cluster

    void SetUp() override
    {
        // Create test directory
        mkdir(test_dir, 0755);

        // Create test brain
        brain = brain_create("test_brain", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 13, 3);
        ASSERT_NE(brain, nullptr);

        cluster = nullptr;
        brain_registered = false;
    }

    void TearDown() override
    {
        // Clean up cluster (this will destroy registered brains)
        if (cluster) {
            replication_destroy_cluster(cluster);
        }

        // Clean up brain only if it wasn't registered
        // (registered brains are destroyed by the cluster)
        if (brain && !brain_registered) {
            brain_destroy(brain);
        }

        // Clean up test directory
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "rm -rf %s", test_dir);
        system(cmd);
    }
};

//=============================================================================
// Cluster Creation Tests
//=============================================================================

/**
 * WHAT: Test filesystem cluster creation
 * WHY: Verify basic cluster initialization works
 */
TEST_F(ReplicationTest, CreateFilesystemCluster)
{
    // Create cluster
    cluster = replication_create_filesystem_cluster(test_dir, "node_1");

    ASSERT_NE(cluster, nullptr);

    // Verify cluster directories created
    char brains_dir[512];
    snprintf(brains_dir, sizeof(brains_dir), "%s/brains", test_dir);

    struct stat st;
    ASSERT_EQ(stat(brains_dir, &st), 0);
    ASSERT_TRUE(S_ISDIR(st.st_mode));
}

/**
 * WHAT: Test cluster creation with NULL parameters
 * WHY: Verify proper error handling
 */
TEST_F(ReplicationTest, CreateClusterNullParams)
{
    cluster = replication_create_filesystem_cluster(nullptr,  // NULL directory
                                                    "node_1");

    ASSERT_EQ(cluster, nullptr);
}

/**
 * WHAT: Test cluster creation with invalid directory
 * WHY: Verify error handling for filesystem issues
 */
TEST_F(ReplicationTest, CreateClusterInvalidDirectory)
{
    cluster =
        replication_create_filesystem_cluster("/nonexistent/path/that/does/not/exist", "node_1");

    // Should fail gracefully
    ASSERT_EQ(cluster, nullptr);
}

//=============================================================================
// Brain Registration Tests
//=============================================================================

/**
 * WHAT: Test brain registration with cluster
 * WHY: Verify brains can be registered for replication
 */
TEST_F(ReplicationTest, RegisterBrain)
{
    cluster = replication_create_filesystem_cluster(test_dir, "node_1");
    ASSERT_NE(cluster, nullptr);

    // Register brain
    bool result = replication_register_brain(cluster, brain, "test_brain");
    ASSERT_TRUE(result);
    brain_registered = true;  // Mark as registered to prevent double-free
}

/**
 * WHAT: Test brain registration with NULL cluster
 * WHY: Verify parameter validation
 */
TEST_F(ReplicationTest, RegisterBrainNullCluster)
{
    bool result = replication_register_brain(nullptr, brain, "test_brain");
    ASSERT_FALSE(result);
}

/**
 * WHAT: Test brain registration with NULL brain
 * WHY: Verify parameter validation
 */
TEST_F(ReplicationTest, RegisterBrainNullBrain)
{
    cluster = replication_create_filesystem_cluster(test_dir, "node_1");
    ASSERT_NE(cluster, nullptr);

    bool result = replication_register_brain(cluster, nullptr, "test_brain");
    ASSERT_FALSE(result);
}

/**
 * WHAT: Test brain registration with NULL name
 * WHY: Verify parameter validation
 */
TEST_F(ReplicationTest, RegisterBrainNullName)
{
    cluster = replication_create_filesystem_cluster(test_dir, "node_1");
    ASSERT_NE(cluster, nullptr);

    bool result = replication_register_brain(cluster, brain, nullptr);
    ASSERT_FALSE(result);
}

//=============================================================================
// Brain Synchronization Tests
//=============================================================================

/**
 * WHAT: Test pushing brain state to cluster
 * WHY: Verify brain serialization and storage works
 */
TEST_F(ReplicationTest, SyncPush)
{
    cluster = replication_create_filesystem_cluster(test_dir, "node_1");
    ASSERT_NE(cluster, nullptr);

    replication_register_brain(cluster, brain, "test_brain");
    brain_registered = true;  // Mark as registered to prevent double-free

    // Train brain a bit
    float features[13] = {0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5};
    brain_learn_example(brain, features, 13, "allow", 0.9);

    // Push to cluster
    bool result = replication_sync_push(cluster, "test_brain");
    ASSERT_TRUE(result);

    // Verify file was created
    char brain_path[512];
    snprintf(brain_path, sizeof(brain_path), "%s/brains/test_brain.nimcp", test_dir);

    struct stat st;
    ASSERT_EQ(stat(brain_path, &st), 0);
    ASSERT_TRUE(st.st_size > 0);
}

/**
 * WHAT: Test pulling brain state from cluster
 * WHY: Verify brain deserialization and loading works
 */
TEST_F(ReplicationTest, SyncPull)
{
    // Create and push brain from first cluster
    cluster = replication_create_filesystem_cluster(test_dir, "node_1");
    ASSERT_NE(cluster, nullptr);

    replication_register_brain(cluster, brain, "test_brain");
    brain_registered = true;  // Mark as registered to prevent double-free

    float features[13] = {0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5};
    brain_learn_example(brain, features, 13, "allow", 0.9);

    bool pushed = replication_sync_push(cluster, "test_brain");
    ASSERT_TRUE(pushed);

    // Create second cluster (same shared dir)
    replication_cluster_t cluster2 = replication_create_filesystem_cluster(test_dir, "node_2");
    ASSERT_NE(cluster2, nullptr);

    // Pull from cluster WITHOUT pre-registering a brain
    // This will create and register a new brain from the pulled data
    bool pulled = replication_sync_pull(cluster2, "test_brain");
    ASSERT_TRUE(pulled);

    // Clean up cluster (this will destroy the pulled brain automatically)
    replication_destroy_cluster(cluster2);
}

/**
 * WHAT: Test sync push with NULL cluster
 * WHY: Verify parameter validation
 */
TEST_F(ReplicationTest, SyncPushNullCluster)
{
    bool result = replication_sync_push(nullptr, "test_brain");
    ASSERT_FALSE(result);
}

/**
 * WHAT: Test sync pull with NULL cluster
 * WHY: Verify parameter validation
 */
TEST_F(ReplicationTest, SyncPullNullCluster)
{
    bool result = replication_sync_pull(nullptr, "test_brain");
    ASSERT_FALSE(result);
}

/**
 * WHAT: Test sync push with unregistered brain
 * WHY: Verify error handling
 */
TEST_F(ReplicationTest, SyncPushUnregisteredBrain)
{
    cluster = replication_create_filesystem_cluster(test_dir, "node_1");
    ASSERT_NE(cluster, nullptr);

    // Don't register brain
    bool result = replication_sync_push(cluster, "nonexistent_brain");
    ASSERT_FALSE(result);
}

//=============================================================================
// Auto-Sync Tests
//=============================================================================

/**
 * WHAT: Test enabling auto-sync
 * WHY: Verify auto-sync configuration works
 */
TEST_F(ReplicationTest, SetAutosync)
{
    cluster = replication_create_filesystem_cluster(test_dir, "node_1");
    ASSERT_NE(cluster, nullptr);

    replication_register_brain(cluster, brain, "test_brain");
    brain_registered = true;  // Mark as registered to prevent double-free

    // Enable auto-sync
    bool result = replication_set_autosync(cluster, "test_brain", true);
    ASSERT_TRUE(result);

    // Disable auto-sync
    result = replication_set_autosync(cluster, "test_brain", false);
    ASSERT_TRUE(result);
}

/**
 * WHAT: Test auto-sync with NULL cluster
 * WHY: Verify parameter validation
 */
TEST_F(ReplicationTest, SetAutosyncNullCluster)
{
    bool result = replication_set_autosync(nullptr, "test_brain", true);
    ASSERT_FALSE(result);
}

//=============================================================================
// Cluster Status Tests
//=============================================================================

/**
 * WHAT: Test getting cluster status
 * WHY: Verify node discovery works
 */
TEST_F(ReplicationTest, GetClusterStatus)
{
    cluster = replication_create_filesystem_cluster(test_dir, "node_1");
    ASSERT_NE(cluster, nullptr);

    // Wait a bit for heartbeat to register
    usleep(100000);  // 100ms

    // Get cluster status
    cluster_node_t nodes[10];
    uint32_t num_nodes = replication_get_cluster_status(cluster, nodes, 10);

    // Should find at least our own node
    ASSERT_GE(num_nodes, 1);

    // Verify our node is in the list
    bool found_self = false;
    for (uint32_t i = 0; i < num_nodes; i++) {
        if (strcmp(nodes[i].node_id, "node_1") == 0) {
            found_self = true;
            break;
        }
    }
    ASSERT_TRUE(found_self);
}

/**
 * WHAT: Test cluster health check
 * WHY: Verify health monitoring works
 */
TEST_F(ReplicationTest, IsHealthy)
{
    cluster = replication_create_filesystem_cluster(test_dir, "node_1");
    ASSERT_NE(cluster, nullptr);

    // Cluster should be healthy immediately after creation
    // (initial heartbeat is sent synchronously during cluster creation)
    bool healthy = replication_is_healthy(cluster);
    ASSERT_TRUE(healthy);
}

/**
 * WHAT: Test health check with NULL cluster
 * WHY: Verify parameter validation
 */
TEST_F(ReplicationTest, IsHealthyNullCluster)
{
    bool healthy = replication_is_healthy(nullptr);
    ASSERT_FALSE(healthy);
}

//=============================================================================
// Brain Unregistration Tests
//=============================================================================

/**
 * WHAT: Test unregistering brain from cluster
 * WHY: Verify cleanup works correctly
 */
TEST_F(ReplicationTest, UnregisterBrain)
{
    cluster = replication_create_filesystem_cluster(test_dir, "node_1");
    ASSERT_NE(cluster, nullptr);

    replication_register_brain(cluster, brain, "test_brain");
    brain_registered = true;  // Mark as registered

    // Unregister (this will destroy the brain object)
    bool result = replication_unregister_brain(cluster, "test_brain");
    ASSERT_TRUE(result);

    // Brain was destroyed by unregister, so set to NULL to prevent double-free in TearDown
    brain = nullptr;

    // Should not be able to sync after unregistering
    result = replication_sync_push(cluster, "test_brain");
    ASSERT_FALSE(result);
}

/**
 * WHAT: Test unregister with NULL cluster
 * WHY: Verify parameter validation
 */
TEST_F(ReplicationTest, UnregisterBrainNullCluster)
{
    bool result = replication_unregister_brain(nullptr, "test_brain");
    ASSERT_FALSE(result);
}

//=============================================================================
// Multi-Node Tests
//=============================================================================

/**
 * WHAT: Test multiple nodes sharing same cluster
 * WHY: Verify actual distributed replication works
 */
TEST_F(ReplicationTest, MultiNodeReplication)
{
    // Create first node
    cluster = replication_create_filesystem_cluster(test_dir, "node_1");
    ASSERT_NE(cluster, nullptr);

    replication_register_brain(cluster, brain, "shared_brain");
    brain_registered = true;  // Mark as registered to prevent double-free

    // Train brain
    float features[13] = {0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5};
    brain_learn_example(brain, features, 13, "allow", 0.9);

    // Push to cluster
    replication_sync_push(cluster, "shared_brain");

    // Create second node and pull brain from cluster
    replication_cluster_t cluster2 = replication_create_filesystem_cluster(test_dir, "node_2");
    ASSERT_NE(cluster2, nullptr);

    // Pull from cluster (this will create and register the brain)
    bool pulled = replication_sync_pull(cluster2, "shared_brain");
    ASSERT_TRUE(pulled);

    // Wait for heartbeats
    usleep(200000);  // 200ms

    // Check cluster status from node 1
    cluster_node_t nodes[10];
    uint32_t num_nodes = replication_get_cluster_status(cluster, nodes, 10);

    // Should see both nodes
    ASSERT_GE(num_nodes, 2);

    // Clean up (cluster2 will destroy the pulled brain automatically)
    replication_destroy_cluster(cluster2);
}

// Note: main() is defined in test_module.cpp - all test files share one main()
