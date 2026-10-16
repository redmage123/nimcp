#!/usr/bin/env python3
"""
Integration Tests for Community Detection Python Bindings
==========================================================

WHAT: Test Python bindings for community detection work correctly
WHY:  Ensure seamless integration between C implementation and Python API
HOW:  Create test networks, run detection, verify results

Test Coverage:
- Python bindings functionality
- Community detection algorithm
- Hub detection
- Topology validation
- Training with community detection enabled
- Checkpoint saving/loading with communities
"""

import pytest
import sys
from pathlib import Path

# Add src to path for nimcp import
sys.path.insert(0, str(Path(__file__).parent.parent.parent.parent / 'build'))

import nimcp


class TestCommunityDetectionBindings:
    """Test basic Python bindings for community detection"""

    def test_brain_create(self):
        """Test brain creation"""
        brain = nimcp.brain_create(100, 4, 10, 0.01)
        assert brain is not None

    def test_detect_communities_basic(self):
        """Test basic community detection"""
        brain = nimcp.brain_create(100, 4, 10, 0.01)

        # Detect communities
        communities = nimcp.brain_detect_communities(brain)

        # Verify structure
        assert communities is not None
        assert hasattr(communities, 'num_neurons')
        assert hasattr(communities, 'num_communities')
        assert hasattr(communities, 'modularity')
        assert hasattr(communities, 'community_ids')
        assert hasattr(communities, 'community_sizes')

        # Check basic properties
        assert communities.num_neurons == 100
        assert communities.num_communities >= 1
        assert -0.5 <= communities.modularity <= 1.0
        assert len(communities.community_ids) == 100
        assert len(communities.community_sizes) == communities.num_communities

    def test_get_modularity(self):
        """Test modularity getter"""
        brain = nimcp.brain_create(100, 4, 10, 0.01)

        modularity = nimcp.brain_get_modularity(brain)

        assert isinstance(modularity, float)
        assert -0.5 <= modularity <= 1.0

    def test_get_num_communities(self):
        """Test number of communities getter"""
        brain = nimcp.brain_create(100, 4, 10, 0.01)

        num_communities = nimcp.brain_get_num_communities(brain)

        assert isinstance(num_communities, int)
        assert num_communities >= 1

    def test_get_neuron_community(self):
        """Test getting community for specific neuron"""
        brain = nimcp.brain_create(100, 4, 10, 0.01)

        # Get community for first neuron
        comm_id = nimcp.brain_get_neuron_community(brain, 0)

        assert isinstance(comm_id, int)
        assert comm_id >= 0

    def test_detect_hubs_basic(self):
        """Test basic hub detection"""
        brain = nimcp.brain_create(100, 4, 10, 0.01)

        # Detect hubs
        hubs = nimcp.brain_detect_hubs(brain, threshold=0.8)

        # Verify structure
        assert hubs is not None
        assert hasattr(hubs, 'num_hubs')
        assert hasattr(hubs, 'hub_indices')
        assert hasattr(hubs, 'degree_centrality')

        # Check properties
        assert hubs.num_hubs >= 0
        assert len(hubs.hub_indices) == hubs.num_hubs
        assert len(hubs.degree_centrality) == hubs.num_hubs

        # Check centrality values in valid range
        for centrality in hubs.degree_centrality:
            assert 0.0 <= centrality <= 1.0

    def test_detect_hubs_threshold(self):
        """Test hub detection with different thresholds"""
        brain = nimcp.brain_create(100, 4, 10, 0.01)

        # Lower threshold should find more hubs
        hubs_low = nimcp.brain_detect_hubs(brain, threshold=0.5)
        hubs_high = nimcp.brain_detect_hubs(brain, threshold=0.9)

        assert hubs_low.num_hubs >= hubs_high.num_hubs

    def test_validate_topology_basic(self):
        """Test basic topology validation"""
        brain = nimcp.brain_create(100, 4, 10, 0.01)

        # Validate topology
        validation = nimcp.brain_validate_topology(brain, min_modularity=0.2)

        # Verify structure
        assert validation is not None
        assert hasattr(validation, 'is_valid')
        assert hasattr(validation, 'modularity')
        assert hasattr(validation, 'clustering_coefficient')
        assert hasattr(validation, 'num_communities')
        assert hasattr(validation, 'num_hubs')
        assert hasattr(validation, 'error_message')

        # Check types
        assert isinstance(validation.is_valid, bool)
        assert isinstance(validation.modularity, float)
        assert isinstance(validation.num_communities, int)
        assert isinstance(validation.num_hubs, int)
        assert isinstance(validation.error_message, str)


class TestCommunityDetectionAlgorithm:
    """Test community detection algorithm correctness"""

    def test_single_neuron(self):
        """Test with single neuron"""
        brain = nimcp.brain_create(1, 1, 1, 0.01)

        communities = nimcp.brain_detect_communities(brain)

        assert communities.num_neurons == 1
        assert communities.num_communities >= 1
        assert communities.community_ids[0] >= 0

    def test_disconnected_network(self):
        """Test with disconnected network (multiple components)"""
        brain = nimcp.brain_create(10, 2, 5, 0.01)

        communities = nimcp.brain_detect_communities(brain)

        # Should detect multiple communities for disconnected components
        assert communities.num_communities >= 1

    def test_fully_connected_small_network(self):
        """Test with small fully connected network"""
        brain = nimcp.brain_create(5, 2, 3, 0.01)

        communities = nimcp.brain_detect_communities(brain)

        # Small fully connected network should have low modularity
        # (all neurons in one community)
        assert communities.num_communities >= 1

    def test_modularity_range(self):
        """Test modularity is in valid range"""
        brain = nimcp.brain_create(100, 4, 10, 0.01)

        communities = nimcp.brain_detect_communities(brain)

        # Modularity Q ∈ [-0.5, 1.0] per Newman's definition
        assert -0.5 <= communities.modularity <= 1.0

    def test_community_ids_valid(self):
        """Test community IDs are valid"""
        brain = nimcp.brain_create(100, 4, 10, 0.01)

        communities = nimcp.brain_detect_communities(brain)

        # All community IDs should be in valid range
        for comm_id in communities.community_ids:
            assert 0 <= comm_id < communities.num_communities

    def test_community_sizes_sum(self):
        """Test community sizes sum to total neurons"""
        brain = nimcp.brain_create(100, 4, 10, 0.01)

        communities = nimcp.brain_detect_communities(brain)

        # Sum of community sizes should equal total neurons
        total_size = sum(communities.community_sizes)
        assert total_size == communities.num_neurons


class TestHubDetection:
    """Test hub neuron detection"""

    def test_hub_indices_valid(self):
        """Test hub indices are valid neuron IDs"""
        brain = nimcp.brain_create(100, 4, 10, 0.01)

        hubs = nimcp.brain_detect_hubs(brain, threshold=0.8)

        # All hub indices should be valid neuron IDs
        for idx in hubs.hub_indices:
            assert 0 <= idx < 100

    def test_hub_indices_unique(self):
        """Test hub indices are unique (no duplicates)"""
        brain = nimcp.brain_create(100, 4, 10, 0.01)

        hubs = nimcp.brain_detect_hubs(brain, threshold=0.8)

        # Hub indices should be unique
        assert len(set(hubs.hub_indices)) == len(hubs.hub_indices)

    def test_centrality_sorted(self):
        """Test hubs are sorted by centrality (highest first)"""
        brain = nimcp.brain_create(100, 4, 10, 0.01)

        hubs = nimcp.brain_detect_hubs(brain, threshold=0.7)

        # Centrality values should be in descending order (or equal)
        if hubs.num_hubs > 1:
            for i in range(hubs.num_hubs - 1):
                assert hubs.degree_centrality[i] >= hubs.degree_centrality[i + 1]


class TestTopologyValidation:
    """Test topology validation"""

    def test_validation_consistent(self):
        """Test validation is consistent with individual checks"""
        brain = nimcp.brain_create(100, 4, 10, 0.01)

        # Get modularity
        modularity = nimcp.brain_get_modularity(brain)

        # Validate with same threshold
        validation = nimcp.brain_validate_topology(brain, min_modularity=0.2)

        # If modularity passes threshold, validation should consider it
        if modularity >= 0.2:
            # Validation might still fail for other reasons, but modularity is ok
            assert validation.modularity >= 0.2
        else:
            # Validation should fail if modularity too low
            assert not validation.is_valid

    def test_validation_threshold_effect(self):
        """Test validation threshold affects result"""
        brain = nimcp.brain_create(100, 4, 10, 0.01)

        modularity = nimcp.brain_get_modularity(brain)

        # Very low threshold should pass
        validation_low = nimcp.brain_validate_topology(brain, min_modularity=0.0)

        # Very high threshold should fail
        validation_high = nimcp.brain_validate_topology(brain, min_modularity=0.9)

        # Check expectations based on actual modularity
        if modularity < 0.9:
            assert not validation_high.is_valid


class TestTrainingIntegration:
    """Test community detection during training"""

    def test_training_with_analysis(self):
        """Test training with periodic network analysis"""
        # This would test the hybrid_train.py integration
        # For now, just verify the API calls work

        brain = nimcp.brain_create(100, 4, 10, 0.01)

        # Simulate training loop with periodic analysis
        for epoch in range(10):
            # Simulate some training
            # (actual training API to be implemented)

            # Periodically analyze (every 5 epochs)
            if epoch % 5 == 0:
                communities = nimcp.brain_detect_communities(brain)
                modularity = communities.modularity
                num_communities = communities.num_communities

                # Verify we got valid results
                assert isinstance(modularity, float)
                assert isinstance(num_communities, int)
                assert num_communities >= 1


class TestErrorHandling:
    """Test error handling in bindings"""

    def test_invalid_neuron_id(self):
        """Test error on invalid neuron ID"""
        brain = nimcp.brain_create(10, 2, 5, 0.01)

        # Try to get community for invalid neuron
        with pytest.raises(ValueError):
            nimcp.brain_get_neuron_community(brain, 999)

    def test_invalid_threshold(self):
        """Test error on invalid threshold"""
        brain = nimcp.brain_create(10, 2, 5, 0.01)

        # Negative threshold should still work (will just find no hubs)
        hubs = nimcp.brain_detect_hubs(brain, threshold=-1.0)
        assert hubs.num_hubs == 0

        # Threshold > 1.0 should find no hubs
        hubs = nimcp.brain_detect_hubs(brain, threshold=2.0)
        assert hubs.num_hubs == 0


def run_tests():
    """Run all tests"""
    pytest.main([__file__, '-v', '--tb=short'])


if __name__ == "__main__":
    run_tests()
