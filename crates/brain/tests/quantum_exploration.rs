//! Integration test — quantum walk exploration over a ZLadder graph.
//!
//! This test demonstrates that `nimcp-quantum`'s discrete-time quantum
//! walker can be driven over a graph derived from `nimcp-memory`'s
//! `ZLadder`, using `nimcp-mathutils::cosine_similarity` to build the
//! adjacency. It is a brain-adjacent scenario — memory nodes with
//! feature vectors, connected by semantic similarity, explored by a
//! quantum walk. No brain internals are touched; this is a pure
//! integration test between three crates.
//!
//! The four test cases check:
//!
//! 1. Amplitudes stay normalized across 20 steps.
//! 2. Probability spreads across the connected subgraph.
//! 3. Different start nodes produce different final distributions.
//! 4. The quantum-walk top-k overlaps partially — but not fully — with
//!    the cosine-similarity top-k, showing the walk is its own signal.

use nimcp_mathutils::cosine_similarity;
use nimcp_memory::{MemoryNode, Tier, ZLadder, ZLadderConfig};
use nimcp_quantum::{QuantumWalker, probabilities};

/// Build an undirected adjacency list: `adj[i]` contains `j` (j != i)
/// iff `cosine_similarity(features[i], features[j]) > threshold`.
fn build_graph_by_cosine(features: &[Vec<f32>], threshold: f32) -> Vec<Vec<usize>> {
    let n = features.len();
    let mut adj = vec![Vec::new(); n];
    for i in 0..n {
        for j in 0..n {
            if i == j {
                continue;
            }
            if cosine_similarity(&features[i], &features[j]) > threshold {
                adj[i].push(j);
            }
        }
    }
    adj
}

/// Eight hand-crafted 8-dim feature vectors arranged so the
/// cosine-similarity graph at threshold 0.5 is connected but sparse:
/// each node has two dominant axes shared with its neighbours.
fn eight_feature_nodes() -> Vec<Vec<f32>> {
    vec![
        vec![1.0, 0.8, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0],
        vec![0.8, 1.0, 0.6, 0.0, 0.0, 0.0, 0.0, 0.0],
        vec![0.0, 0.6, 1.0, 0.8, 0.0, 0.0, 0.0, 0.0],
        vec![0.0, 0.0, 0.8, 1.0, 0.6, 0.0, 0.0, 0.0],
        vec![0.0, 0.0, 0.0, 0.6, 1.0, 0.8, 0.0, 0.0],
        vec![0.0, 0.0, 0.0, 0.0, 0.8, 1.0, 0.6, 0.0],
        vec![0.0, 0.0, 0.0, 0.0, 0.0, 0.6, 1.0, 0.8],
        vec![0.8, 0.0, 0.0, 0.0, 0.0, 0.0, 0.8, 1.0],
    ]
}

/// Populate a fresh `ZLadder` with `features`, one node per vector,
/// all inserted into `Z0`. Node IDs are `0..features.len()`.
fn ladder_with(features: &[Vec<f32>]) -> ZLadder {
    let mut l = ZLadder::new(ZLadderConfig::default()).expect("default config");
    for (i, f) in features.iter().enumerate() {
        let mut n = MemoryNode::new(i as u64, f.clone(), 0);
        n.tier = Tier::Z0;
        l.insert(n).expect("insert");
    }
    l
}

#[test]
fn quantum_walk_over_memory_graph_preserves_normalization() {
    let feats = eight_feature_nodes();
    let _ladder = ladder_with(&feats);
    let adj = build_graph_by_cosine(&feats, 0.5);

    let mut walker = QuantumWalker::new(0, adj, 1234).expect("walker");
    for step in 1..=20 {
        walker.step();
        let probs = probabilities(&walker.amps);
        let sum: f32 = probs.iter().sum();
        assert!(
            (sum - 1.0).abs() < 1e-3,
            "normalization drifted at step {step}: sum = {sum}"
        );
        // Each probability must also be non-negative and finite.
        for (i, p) in probs.iter().enumerate() {
            assert!(p.is_finite(), "non-finite probability at node {i}");
            assert!(*p >= -1e-6, "negative probability at node {i}: {p}");
        }
    }
}

#[test]
fn quantum_walk_spreads_over_connected_subgraph() {
    let feats = eight_feature_nodes();
    let adj = build_graph_by_cosine(&feats, 0.5);

    // Sanity: every node should have at least one neighbour.
    for (i, row) in adj.iter().enumerate() {
        assert!(!row.is_empty(), "node {i} is isolated in cosine graph");
    }

    let mut walker = QuantumWalker::new(0, adj, 2024).expect("walker");
    walker.step_n(20);
    let probs = probabilities(&walker.amps);

    let spread_count = probs.iter().filter(|&&p| p > 0.01).count();
    assert!(
        spread_count >= 4,
        "walk failed to spread: only {spread_count} nodes have p > 0.01 (probs = {probs:?})"
    );
}

#[test]
fn different_start_node_gives_different_distribution() {
    let feats = eight_feature_nodes();
    let adj = build_graph_by_cosine(&feats, 0.5);

    // Start nodes 0 and 1 sit on opposite sides of the graph's
    // bipartite structure — after 20 (even) steps, walker `a` lives on
    // even-index nodes while walker `b` lives on odd-index nodes.
    let mut a = QuantumWalker::new(0, adj.clone(), 42).expect("walker a");
    let mut b = QuantumWalker::new(1, adj, 42).expect("walker b");
    a.step_n(20);
    b.step_n(20);

    let pa = probabilities(&a.amps);
    let pb = probabilities(&b.amps);

    // Total-variation distance between the two distributions — must be
    // non-trivial if the walk is sensitive to initial conditions.
    let tv: f32 = pa
        .iter()
        .zip(pb.iter())
        .map(|(x, y)| (x - y).abs())
        .sum::<f32>()
        * 0.5;
    assert!(
        tv > 0.1,
        "distributions from different start nodes are too similar: TV = {tv} (pa={pa:?}, pb={pb:?})"
    );
}

#[test]
fn walk_signal_overlaps_but_differs_from_cosine_query() {
    let feats = eight_feature_nodes();
    let ladder = ladder_with(&feats);
    let adj = build_graph_by_cosine(&feats, 0.5);

    let start = 0;
    let k = 4;

    // Cosine top-k against the start node's own feature vector. The
    // first hit is always `start` itself (similarity 1.0).
    let cosine_hits = ladder.query_all_tiers(&feats[start], k);
    assert_eq!(cosine_hits.len(), k);
    let cosine_top: std::collections::BTreeSet<u64> =
        cosine_hits.iter().map(|h| h.node_id).collect();

    // Quantum walk top-k: run 20 steps, take the k highest-probability
    // node indices.
    let mut walker = QuantumWalker::new(start, adj, 5150).expect("walker");
    walker.step_n(20);
    let probs = probabilities(&walker.amps);
    let mut indexed: Vec<(usize, f32)> = probs.iter().copied().enumerate().collect();
    indexed.sort_by(|(_, a), (_, b)| b.partial_cmp(a).unwrap_or(std::cmp::Ordering::Equal));
    let walk_top: std::collections::BTreeSet<u64> =
        indexed.iter().take(k).map(|(i, _)| *i as u64).collect();

    let overlap = cosine_top.intersection(&walk_top).count();
    assert!(
        overlap >= 1,
        "walk top-k and cosine top-k share nothing: walk={walk_top:?}, cosine={cosine_top:?}"
    );
    assert!(
        overlap < k,
        "walk top-k and cosine top-k are identical: \
         walk is just reimplementing cosine (walk={walk_top:?}, cosine={cosine_top:?})"
    );
}
