/**
 * @file test_graph_theory.cpp
 * @brief Tests for the graph theory engine (Google Test)
 */

#include <gtest/gtest.h>

extern "C" {
#include "cognitive/math/nimcp_graph_theory.h"
}

/* ---------- lifecycle ---------- */

TEST(GraphTheoryTest, CreateDestroy) {
    graph_t *g = gt_create(5, false, false);
    ASSERT_NE(g, nullptr);
    EXPECT_EQ(g->num_vertices, 5);
    EXPECT_FALSE(g->directed);
    gt_destroy(g);
}

/* ---------- edge operations ---------- */

TEST(GraphTheoryTest, AddEdge) {
    graph_t *g = gt_create(4, false, true);
    EXPECT_TRUE(gt_add_edge(g, 0, 1, 3.0));
    EXPECT_TRUE(gt_has_edge(g, 0, 1));
    EXPECT_TRUE(gt_has_edge(g, 1, 0));  /* undirected */
    EXPECT_NEAR(gt_edge_weight(g, 0, 1), 3.0, 1e-9);
    gt_destroy(g);
}

/* ---------- BFS shortest path ---------- */

TEST(GraphTheoryTest, BfsShortestPath) {
    /* 0-1-2-3 path graph, also 0-3 direct edge */
    graph_t *g = gt_create(4, false, false);
    gt_add_edge(g, 0, 1, 1.0);
    gt_add_edge(g, 1, 2, 1.0);
    gt_add_edge(g, 2, 3, 1.0);
    gt_add_edge(g, 0, 3, 1.0); /* shortcut */

    gt_traversal_t bfs = gt_bfs(g, 0);
    EXPECT_EQ(bfs.count, 4);

    /* BFS parent of 3 should be 0 (direct edge) */
    EXPECT_EQ(bfs.parent[3], 0);
    gt_destroy(g);
}

/* ---------- Dijkstra ---------- */

TEST(GraphTheoryTest, DijkstraWeighted) {
    /* 0->1 (1), 0->2 (4), 1->2 (2), 2->3 (1) */
    graph_t *g = gt_create(4, true, true);
    gt_add_edge(g, 0, 1, 1.0);
    gt_add_edge(g, 0, 2, 4.0);
    gt_add_edge(g, 1, 2, 2.0);
    gt_add_edge(g, 2, 3, 1.0);

    gt_shortest_path_t sp = gt_dijkstra(g, 0);
    EXPECT_NEAR(sp.dist[0], 0.0, 1e-9);
    EXPECT_NEAR(sp.dist[1], 1.0, 1e-9);
    EXPECT_NEAR(sp.dist[2], 3.0, 1e-9);   /* 0->1->2 = 3, not 0->2 = 4 */
    EXPECT_NEAR(sp.dist[3], 4.0, 1e-9);   /* 0->1->2->3 = 4 */

    gt_destroy(g);
}

/* ---------- Kruskal MST ---------- */

TEST(GraphTheoryTest, KruskalMst) {
    /*  0-1 (1), 0-2 (3), 1-2 (2), 1-3 (4), 2-3 (5)
        MST edges: 0-1(1), 1-2(2), 1-3(4) => total = 7 */
    graph_t *g = gt_create(4, false, true);
    gt_add_edge(g, 0, 1, 1.0);
    gt_add_edge(g, 0, 2, 3.0);
    gt_add_edge(g, 1, 2, 2.0);
    gt_add_edge(g, 1, 3, 4.0);
    gt_add_edge(g, 2, 3, 5.0);

    gt_mst_result_t mst = gt_kruskal(g);
    EXPECT_EQ(mst.num_edges, 3);
    EXPECT_NEAR(mst.total_weight, 7.0, 1e-9);

    gt_destroy(g);
}

/* ---------- chromatic number of K4 ---------- */

TEST(GraphTheoryTest, ChromaticNumberK4) {
    /* Complete graph K4: every pair connected => chi(K4) = 4 */
    graph_t *g = gt_create(4, false, false);
    for (int i = 0; i < 4; i++)
        for (int j = i + 1; j < 4; j++)
            gt_add_edge(g, i, j, 1.0);

    gt_coloring_t col = gt_chromatic_number(g);
    EXPECT_EQ(col.num_colors, 4);
    EXPECT_TRUE(col.optimal);

    gt_destroy(g);
}

/* ---------- bipartite detection ---------- */

TEST(GraphTheoryTest, BipartiteDetection) {
    /* Even cycle is bipartite */
    graph_t *g = gt_create(4, false, false);
    gt_add_edge(g, 0, 1, 1.0);
    gt_add_edge(g, 1, 2, 1.0);
    gt_add_edge(g, 2, 3, 1.0);
    gt_add_edge(g, 3, 0, 1.0);
    EXPECT_TRUE(gt_is_bipartite(g));
    gt_destroy(g);

    /* Odd cycle (triangle) is not bipartite */
    graph_t *g2 = gt_create(3, false, false);
    gt_add_edge(g2, 0, 1, 1.0);
    gt_add_edge(g2, 1, 2, 1.0);
    gt_add_edge(g2, 2, 0, 1.0);
    EXPECT_FALSE(gt_is_bipartite(g2));
    gt_destroy(g2);
}

/* ---------- max flow ---------- */

TEST(GraphTheoryTest, MaxFlowSimple) {
    /* s=0, t=3: 0->1(10), 0->2(10), 1->3(10), 2->3(10), 1->2(1) */
    /* Max flow = 20 */
    graph_t *g = gt_create(4, true, true);
    gt_add_edge(g, 0, 1, 10.0);
    gt_add_edge(g, 0, 2, 10.0);
    gt_add_edge(g, 1, 3, 10.0);
    gt_add_edge(g, 2, 3, 10.0);
    gt_add_edge(g, 1, 2, 1.0);

    gt_flow_result_t flow = gt_max_flow(g, 0, 3);
    EXPECT_NEAR(flow.max_flow, 20.0, 1e-9);

    gt_destroy(g);
}

/* ---------- connected components ---------- */

TEST(GraphTheoryTest, ConnectedComponents) {
    /* Two disconnected pairs: {0,1} and {2,3} */
    graph_t *g = gt_create(4, false, false);
    gt_add_edge(g, 0, 1, 1.0);
    gt_add_edge(g, 2, 3, 1.0);

    gt_components_t cc = gt_connected_components(g);
    EXPECT_EQ(cc.num_components, 2);
    EXPECT_EQ(cc.component[0], cc.component[1]);
    EXPECT_EQ(cc.component[2], cc.component[3]);
    EXPECT_NE(cc.component[0], cc.component[2]);

    gt_destroy(g);
}

/* ---------- DFS traversal ---------- */

TEST(GraphTheoryTest, DfsTraversal) {
    graph_t *g = gt_create(4, false, false);
    gt_add_edge(g, 0, 1, 1.0);
    gt_add_edge(g, 0, 2, 1.0);
    gt_add_edge(g, 1, 3, 1.0);

    gt_traversal_t dfs = gt_dfs(g, 0);
    EXPECT_EQ(dfs.count, 4);
    EXPECT_EQ(dfs.order[0], 0);  /* source is visited first */

    gt_destroy(g);
}
