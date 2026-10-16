#!/usr/bin/env python3
"""
Test COW Integration for NIMCP Web Demo

This script tests the COW cloning endpoints to verify integration.
Run this with the Flask backend running on http://localhost:5000
"""

import requests
import time
import sys

API_URL = 'http://localhost:5000/api'

def print_test(name):
    """Print test section header"""
    print(f"\n{'='*70}")
    print(f"TEST: {name}")
    print('='*70)

def print_success(message):
    """Print success message"""
    print(f"✓ {message}")

def print_error(message):
    """Print error message"""
    print(f"✗ {message}")
    sys.exit(1)

def test_initialize_brain():
    """Test brain initialization"""
    print_test("Initialize Brain")

    response = requests.post(f'{API_URL}/init')
    data = response.json()

    if not data.get('success'):
        print_error(f"Failed to initialize brain: {data.get('error')}")

    brain_id = data.get('brain_id')
    if brain_id is None:
        print_error("No brain_id returned")

    print_success(f"Brain initialized with ID: {brain_id}")
    return brain_id

def test_create_cow_clone(brain_id):
    """Test COW clone creation"""
    print_test("Create COW Clone")

    response = requests.post(f'{API_URL}/brain/{brain_id}/clone_cow')
    data = response.json()

    if not data.get('success'):
        print_error(f"Failed to create clone: {data.get('error')}")

    clone_id = data.get('clone_id')
    parent_id = data.get('parent_id')
    clone_time = data.get('clone_time', 0)
    cow_stats = data.get('cow_stats', {})

    print_success(f"Clone created with ID: {clone_id}")
    print_success(f"Parent ID: {parent_id}")
    print_success(f"Clone time: {clone_time*1000:.2f}ms")
    print_success(f"Is COW clone: {cow_stats.get('is_cow_clone')}")
    print_success(f"Shared memory: {cow_stats.get('shared_bytes', 0) / 1024 / 1024:.2f} MB")
    print_success(f"Private memory: {cow_stats.get('private_bytes', 0) / 1024:.2f} KB")
    print_success(f"Memory savings: {cow_stats.get('memory_savings_pct', 0):.1f}%")

    return clone_id

def test_get_cow_stats(brain_id):
    """Test getting COW statistics"""
    print_test("Get COW Statistics")

    response = requests.get(f'{API_URL}/brain/{brain_id}/cow_stats')
    data = response.json()

    if not data.get('success'):
        print_error(f"Failed to get stats: {data.get('error')}")

    cow_stats = data.get('cow_stats', {})
    metadata = data.get('metadata', {})
    architecture = data.get('architecture', {})

    print_success(f"Brain ID: {brain_id}")
    print_success(f"Is COW clone: {cow_stats.get('is_cow_clone')}")
    print_success(f"Shared bytes: {cow_stats.get('shared_bytes', 0):,}")
    print_success(f"Private bytes: {cow_stats.get('private_bytes', 0):,}")
    print_success(f"Total bytes: {cow_stats.get('total_bytes', 0):,}")
    print_success(f"Reference count: {cow_stats.get('ref_count', 0)}")
    print_success(f"Neurons: {architecture.get('num_neurons', 0):,}")
    print_success(f"Synapses: {architecture.get('num_synapses', 0):,}")

def test_list_brains():
    """Test listing all brains"""
    print_test("List All Brains")

    response = requests.get(f'{API_URL}/brains')
    data = response.json()

    if not data.get('success'):
        print_error(f"Failed to list brains: {data.get('error')}")

    brains = data.get('brains', [])
    total_count = data.get('total_count', 0)

    print_success(f"Total brains: {total_count}")

    for brain in brains:
        print_success(f"  Brain {brain['id']}: {brain['name']}")
        print_success(f"    Parent: {brain.get('parent_id', 'None')}")
        print_success(f"    Is COW: {brain.get('is_cow_clone', False)}")
        print_success(f"    Clones: {brain.get('clone_count', 0)}")
        print_success(f"    Memory: {brain.get('memory_bytes', 0) / 1024 / 1024:.2f} MB")

def test_create_multiple_clones(brain_id, count=3):
    """Test creating multiple clones"""
    print_test(f"Create {count} Clones")

    clone_ids = []
    for i in range(count):
        response = requests.post(f'{API_URL}/brain/{brain_id}/clone_cow')
        data = response.json()

        if not data.get('success'):
            print_error(f"Failed to create clone {i+1}: {data.get('error')}")

        clone_id = data.get('clone_id')
        clone_ids.append(clone_id)
        print_success(f"Clone {i+1} created with ID: {clone_id}")

    return clone_ids

def test_memory_savings(brain_id, clone_ids):
    """Test memory savings calculation"""
    print_test("Memory Savings Analysis")

    # Get original brain stats
    response = requests.get(f'{API_URL}/brain/{brain_id}/cow_stats')
    original_stats = response.json().get('cow_stats', {})
    original_memory = original_stats.get('total_bytes', 0)

    # Calculate total memory without COW
    total_without_cow = original_memory * (len(clone_ids) + 1)

    # Calculate total memory with COW
    total_with_cow = original_memory  # Original brain
    for clone_id in clone_ids:
        response = requests.get(f'{API_URL}/brain/{clone_id}/cow_stats')
        clone_stats = response.json().get('cow_stats', {})
        total_with_cow += clone_stats.get('private_bytes', 0)

    savings = ((total_without_cow - total_with_cow) / total_without_cow * 100) if total_without_cow > 0 else 0

    print_success(f"Total brains: {len(clone_ids) + 1}")
    print_success(f"Without COW: {total_without_cow / 1024 / 1024:.2f} MB")
    print_success(f"With COW: {total_with_cow / 1024 / 1024:.2f} MB")
    print_success(f"Total savings: {savings:.1f}%")

def test_delete_clone(clone_id):
    """Test deleting a clone"""
    print_test("Delete Clone")

    response = requests.delete(f'{API_URL}/brain/{clone_id}/delete')
    data = response.json()

    if not data.get('success'):
        print_error(f"Failed to delete clone: {data.get('error')}")

    print_success(f"Clone {clone_id} deleted successfully")

def test_delete_parent_with_clones(brain_id):
    """Test that deleting parent with clones fails"""
    print_test("Delete Parent with Clones (Should Fail)")

    response = requests.delete(f'{API_URL}/brain/{brain_id}/delete')
    data = response.json()

    if data.get('success'):
        print_error("Parent brain was deleted (should have failed)")

    print_success("Correctly prevented deletion of parent with active clones")
    print_success(f"Error message: {data.get('error')}")

def main():
    """Run all tests"""
    print("="*70)
    print("NIMCP Web Demo - COW Integration Tests")
    print("="*70)
    print("\nMake sure Flask backend is running on http://localhost:5000")
    print("Press Ctrl+C to cancel...")
    time.sleep(2)

    try:
        # Test 1: Initialize brain
        brain_id = test_initialize_brain()

        # Test 2: Create single clone
        clone_id_1 = test_create_cow_clone(brain_id)

        # Test 3: Get COW stats for clone
        test_get_cow_stats(clone_id_1)

        # Test 4: Get COW stats for original
        test_get_cow_stats(brain_id)

        # Test 5: List all brains
        test_list_brains()

        # Test 6: Create multiple clones
        clone_ids = test_create_multiple_clones(brain_id, count=3)

        # Test 7: List brains again
        test_list_brains()

        # Test 8: Memory savings analysis
        test_memory_savings(brain_id, [clone_id_1] + clone_ids)

        # Test 9: Try to delete parent (should fail)
        test_delete_parent_with_clones(brain_id)

        # Test 10: Delete a clone
        test_delete_clone(clone_ids[0])

        # Test 11: List brains after deletion
        test_list_brains()

        # Final summary
        print("\n" + "="*70)
        print("ALL TESTS PASSED!")
        print("="*70)
        print("\nCOW Integration is working correctly!")
        print("You can now use the web interface at http://localhost:3000")

    except requests.exceptions.ConnectionError:
        print_error("Could not connect to backend. Make sure Flask is running on http://localhost:5000")
    except KeyboardInterrupt:
        print("\n\nTests cancelled by user")
        sys.exit(0)
    except Exception as e:
        print_error(f"Unexpected error: {str(e)}")

if __name__ == '__main__':
    main()
