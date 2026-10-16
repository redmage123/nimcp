/**
 * @file test_p2pnode.cpp
 * @brief Test suite for P2P node functionality
 * @details Tests node creation, startup, shutdown, and peer connections
 */

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include "test_helpers.h"
#include "utils/nimcp_test_base.h"

class P2PNodeTest : public NimcpTestBase {
   protected:
    node_config_t test_config;
    p2p_node_t test_node;

    void SetUp() override
    {
        NimcpTestBase::SetUp();
        test_config.listen_port = 48917;
        test_config.max_peers = 10;
        //        test_config.keepalive_interval = 1000;
        test_node = nullptr;
    }

    void TearDown() override
    {
        if (test_node) {
            p2p_node_destroy(test_node);
            test_node = nullptr;
        }
        NimcpTestBase::TearDown();
    }

    /**
     * @brief Checks if a network port is available for use
     * @param port Port number to check
     * @return true if port is available, false otherwise
     */
    bool is_port_available(int port)
    {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0)
            return false;

        // Set SO_REUSEADDR to avoid TIME_WAIT issues
        int reuse = 1;
        setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);

        int result = bind(sock, (struct sockaddr*) &addr, sizeof(addr));
        close(sock);

        return result == 0;
    }
};

TEST_F(P2PNodeTest, CreateNode)
{
    ASSERT_TRUE(is_port_available(test_config.listen_port));

    test_node = p2p_node_create(&test_config);
    ASSERT_NE(test_node, nullptr);
    ASSERT_EQ(p2p_node_get_status(test_node), NODE_STATUS_INIT);
}

TEST_F(P2PNodeTest, CreateNodeNullConfig)
{
    test_node = p2p_node_create(nullptr);
    ASSERT_EQ(test_node, nullptr);
}

TEST_F(P2PNodeTest, CreateNodeZeroPeers)
{
    test_config.max_peers = 0;
    test_node = p2p_node_create(&test_config);
    ASSERT_EQ(test_node, nullptr);
}

TEST_F(P2PNodeTest, StartNode)
{
    ASSERT_TRUE(is_port_available(test_config.listen_port));

    test_node = p2p_node_create(&test_config);
    ASSERT_NE(test_node, nullptr);

    ASSERT_TRUE(p2p_node_start(test_node));
    ASSERT_EQ(p2p_node_get_status(test_node), NODE_STATUS_RUNNING);
}

TEST_F(P2PNodeTest, StartNodeTwice)
{
    ASSERT_TRUE(is_port_available(test_config.listen_port));

    test_node = p2p_node_create(&test_config);
    ASSERT_NE(test_node, nullptr);

    ASSERT_TRUE(p2p_node_start(test_node));
    ASSERT_FALSE(p2p_node_start(test_node));
}

TEST_F(P2PNodeTest, StopNode)
{
    test_node = p2p_node_create(&test_config);
    ASSERT_NE(test_node, nullptr);

    ASSERT_TRUE(p2p_node_start(test_node));
    ASSERT_TRUE(p2p_node_stop(test_node));
    ASSERT_EQ(p2p_node_get_status(test_node), NODE_STATUS_SHUTDOWN);
}

TEST_F(P2PNodeTest, StopNodeNotRunning)
{
    test_node = p2p_node_create(&test_config);
    ASSERT_NE(test_node, nullptr);

    ASSERT_FALSE(p2p_node_stop(test_node));
}

class P2PNodeIntegrationTest : public NimcpTestBase {
   protected:
    p2p_node_t node1;
    p2p_node_t node2;
    node_config_t config1;
    node_config_t config2;

    void SetUp() override
    {
        NimcpTestBase::SetUp();
        // FIX: Use higher port numbers to avoid conflicts with other tests
        config1.listen_port = 18001;
        config1.max_peers = 10;
        //        config1.keepalive_interval = 1000;

        config2.listen_port = 18002;
        config2.max_peers = 10;
        //      config2.keepalive_interval = 1000;

        ASSERT_TRUE(is_port_available(config1.listen_port));
        ASSERT_TRUE(is_port_available(config2.listen_port));

        node1 = p2p_node_create(&config1);
        node2 = p2p_node_create(&config2);
    }

    void TearDown() override
    {
        // Note: p2p_node_destroy already calls p2p_node_stop internally
        if (node1) {
            p2p_node_destroy(node1);
            node1 = nullptr;
        }
        if (node2) {
            p2p_node_destroy(node2);
            node2 = nullptr;
        }
        // Give the OS time to release the ports
        usleep(50000);
        NimcpTestBase::TearDown();
    }

    bool is_port_available(int port)
    {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0)
            return false;

        // Set SO_REUSEADDR to avoid TIME_WAIT issues
        int reuse = 1;
        setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);

        int result = bind(sock, (struct sockaddr*) &addr, sizeof(addr));
        close(sock);

        return result == 0;
    }
};

TEST_F(P2PNodeIntegrationTest, TwoNodeConnection)
{
    ASSERT_NE(node1, nullptr);
    ASSERT_NE(node2, nullptr);

    ASSERT_TRUE(p2p_node_start(node1));
    ASSERT_TRUE(p2p_node_start(node2));

    usleep(100000);  // Allow time for nodes to start

    ASSERT_TRUE(p2p_node_connect_peer(node1, "127.0.0.1", 18002));

    usleep(100000);  // Allow time for connection

    ASSERT_EQ(p2p_node_get_status(node1), NODE_STATUS_RUNNING);
    ASSERT_EQ(p2p_node_get_status(node2), NODE_STATUS_RUNNING);
}

TEST_F(P2PNodeIntegrationTest, NodeReconnection)
{
    ASSERT_NE(node1, nullptr);
    ASSERT_NE(node2, nullptr);

    ASSERT_TRUE(p2p_node_start(node1));
    ASSERT_TRUE(p2p_node_start(node2));

    usleep(100000);

    ASSERT_TRUE(p2p_node_connect_peer(node1, "127.0.0.1", 18002));
    usleep(100000);

    ASSERT_TRUE(p2p_node_disconnect_peer(node1, "127.0.0.1", 18002));
    usleep(100000);

    ASSERT_TRUE(p2p_node_connect_peer(node1, "127.0.0.1", 18002));
    usleep(100000);
}

TEST_F(P2PNodeIntegrationTest, RapidConnectionDisconnection)
{
    ASSERT_NE(node1, nullptr);
    ASSERT_NE(node2, nullptr);

    ASSERT_TRUE(p2p_node_start(node1));
    ASSERT_TRUE(p2p_node_start(node2));

    for (int i = 0; i < 10; i++) {
        ASSERT_TRUE(p2p_node_connect_peer(node1, "127.0.0.1", 18002));
        usleep(10000);
        ASSERT_TRUE(p2p_node_disconnect_peer(node1, "127.0.0.1", 18002));
        usleep(10000);
    }
}
