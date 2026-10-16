/**
 * COW (Copy-on-Write) Brain Management Panel
 *
 * WHAT: Interactive panel for managing brain clones with COW memory sharing
 * WHY:  Demonstrate NIMCP's efficient memory sharing capabilities
 * HOW:  Display brain hierarchy, clone relationships, and memory statistics
 */

import React, { useState, useEffect } from 'react';
import axios from 'axios';
import './COWPanel.css';

const API_URL = '/api';

function COWPanel({ brainInitialized, primaryBrainId }) {
  const [brains, setBrains] = useState([]);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState(null);
  const [selectedBrain, setSelectedBrain] = useState(null);
  const [refreshInterval, setRefreshInterval] = useState(null);

  // Fetch all brains
  const fetchBrains = async () => {
    try {
      const response = await axios.get(`${API_URL}/brains`);
      if (response.data.success) {
        setBrains(response.data.brains);
      }
    } catch (err) {
      console.error('Failed to fetch brains:', err);
      setError('Failed to fetch brain list');
    }
  };

  // Auto-refresh brain list every 2 seconds
  useEffect(() => {
    if (brainInitialized) {
      fetchBrains();
      const interval = setInterval(fetchBrains, 2000);
      setRefreshInterval(interval);
      return () => clearInterval(interval);
    }
  }, [brainInitialized]);

  // Create COW clone
  const createClone = async (brainId) => {
    setLoading(true);
    setError(null);
    try {
      const response = await axios.post(`${API_URL}/brain/${brainId}/clone_cow`);
      if (response.data.success) {
        await fetchBrains();
        return response.data;
      }
    } catch (err) {
      setError(`Failed to create clone: ${err.message}`);
    } finally {
      setLoading(false);
    }
  };

  // Delete brain/clone
  const deleteBrain = async (brainId) => {
    if (!window.confirm('Are you sure you want to delete this brain?')) {
      return;
    }

    setLoading(true);
    setError(null);
    try {
      const response = await axios.delete(`${API_URL}/brain/${brainId}/delete`);
      if (response.data.success) {
        await fetchBrains();
        if (selectedBrain === brainId) {
          setSelectedBrain(null);
        }
      }
    } catch (err) {
      setError(`Failed to delete brain: ${err.response?.data?.error || err.message}`);
    } finally {
      setLoading(false);
    }
  };

  // View brain details
  const viewBrainDetails = async (brainId) => {
    try {
      const response = await axios.get(`${API_URL}/brain/${brainId}/cow_stats`);
      if (response.data.success) {
        setSelectedBrain({
          id: brainId,
          ...response.data
        });
      }
    } catch (err) {
      setError(`Failed to fetch brain details: ${err.message}`);
    }
  };

  // Calculate total memory usage
  const calculateTotalMemory = () => {
    let totalWithoutCOW = 0;
    let totalWithCOW = 0;
    let sharedMemory = 0;

    brains.forEach(brain => {
      const fullMemory = brain.memory_bytes || 0;
      const shared = brain.cow_shared_bytes || 0;
      const private_ = brain.cow_private_bytes || 0;

      totalWithoutCOW += fullMemory;
      totalWithCOW += private_;

      if (brain.is_cow_clone && shared > sharedMemory) {
        sharedMemory = shared;
      }
    });

    totalWithCOW += sharedMemory;

    return {
      withoutCOW: totalWithoutCOW,
      withCOW: totalWithCOW,
      savings: totalWithoutCOW > 0 ? ((totalWithoutCOW - totalWithCOW) / totalWithoutCOW * 100) : 0
    };
  };

  // Format bytes to human-readable
  const formatBytes = (bytes) => {
    if (bytes === 0) return '0 B';
    const k = 1024;
    const sizes = ['B', 'KB', 'MB', 'GB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return Math.round((bytes / Math.pow(k, i)) * 100) / 100 + ' ' + sizes[i];
  };

  // Build brain hierarchy tree
  const buildHierarchy = () => {
    const roots = brains.filter(b => !b.parent_id);
    return roots.map(root => ({
      ...root,
      children: brains.filter(b => b.parent_id === root.id)
    }));
  };

  const memoryStats = calculateTotalMemory();
  const hierarchy = buildHierarchy();

  if (!brainInitialized) {
    return (
      <div className="cow-panel">
        <h2>COW Brain Cloning</h2>
        <p className="info-text">Initialize a brain to start using COW cloning</p>
      </div>
    );
  }

  return (
    <div className="cow-panel">
      <div className="cow-header">
        <h2>COW Brain Management</h2>
        <button className="btn-refresh" onClick={fetchBrains} disabled={loading}>
          Refresh
        </button>
      </div>

      {error && (
        <div className="error-message">
          {error}
          <button onClick={() => setError(null)}>×</button>
        </div>
      )}

      {/* Memory Savings Summary */}
      <div className="memory-summary">
        <h3>Memory Efficiency</h3>
        <div className="memory-stats">
          <div className="stat-card">
            <div className="stat-label">Without COW</div>
            <div className="stat-value">{formatBytes(memoryStats.withoutCOW)}</div>
          </div>
          <div className="stat-card">
            <div className="stat-label">With COW</div>
            <div className="stat-value">{formatBytes(memoryStats.withCOW)}</div>
          </div>
          <div className="stat-card highlight">
            <div className="stat-label">Savings</div>
            <div className="stat-value">{memoryStats.savings.toFixed(1)}%</div>
          </div>
        </div>
      </div>

      {/* Brain Hierarchy */}
      <div className="brain-hierarchy">
        <h3>Brain Instances ({brains.length})</h3>
        <div className="brain-tree">
          {hierarchy.map(root => (
            <div key={root.id} className="brain-node root">
              <div className={`brain-card ${selectedBrain?.id === root.id ? 'selected' : ''}`}>
                <div className="brain-info">
                  <div className="brain-name">
                    <span className="brain-icon">🧠</span>
                    {root.name}
                  </div>
                  <div className="brain-meta">
                    ID: {root.id} | Memory: {formatBytes(root.memory_bytes)}
                  </div>
                  {root.clone_count > 0 && (
                    <div className="clone-count">
                      {root.clone_count} clone{root.clone_count > 1 ? 's' : ''}
                    </div>
                  )}
                </div>
                <div className="brain-actions">
                  <button
                    className="btn-clone"
                    onClick={() => createClone(root.id)}
                    disabled={loading}
                  >
                    Clone (COW)
                  </button>
                  <button
                    className="btn-details"
                    onClick={() => viewBrainDetails(root.id)}
                  >
                    Details
                  </button>
                </div>
              </div>

              {/* Render clones */}
              {root.children && root.children.length > 0 && (
                <div className="brain-children">
                  {root.children.map(clone => (
                    <div key={clone.id} className="brain-node clone">
                      <div className={`brain-card ${selectedBrain?.id === clone.id ? 'selected' : ''}`}>
                        <div className="brain-info">
                          <div className="brain-name">
                            <span className="brain-icon">📋</span>
                            {clone.name}
                          </div>
                          <div className="brain-meta">
                            ID: {clone.id} | Shared: {formatBytes(clone.cow_shared_bytes)} |
                            Private: {formatBytes(clone.cow_private_bytes)}
                          </div>
                          <div className="clone-badge">
                            COW Clone
                          </div>
                        </div>
                        <div className="brain-actions">
                          <button
                            className="btn-details"
                            onClick={() => viewBrainDetails(clone.id)}
                          >
                            Details
                          </button>
                          <button
                            className="btn-delete"
                            onClick={() => deleteBrain(clone.id)}
                            disabled={loading}
                          >
                            Delete
                          </button>
                        </div>
                      </div>
                    </div>
                  ))}
                </div>
              )}
            </div>
          ))}
        </div>
      </div>

      {/* Brain Details Panel */}
      {selectedBrain && (
        <div className="brain-details">
          <div className="details-header">
            <h3>Brain Details - {selectedBrain.metadata?.name}</h3>
            <button onClick={() => setSelectedBrain(null)}>×</button>
          </div>
          <div className="details-content">
            <div className="detail-section">
              <h4>COW Statistics</h4>
              <table className="stats-table">
                <tbody>
                  <tr>
                    <td>Is COW Clone:</td>
                    <td>{selectedBrain.cow_stats?.is_cow_clone ? 'Yes' : 'No'}</td>
                  </tr>
                  <tr>
                    <td>Shared Memory:</td>
                    <td>{formatBytes(selectedBrain.cow_stats?.shared_bytes || 0)}</td>
                  </tr>
                  <tr>
                    <td>Private Memory:</td>
                    <td>{formatBytes(selectedBrain.cow_stats?.private_bytes || 0)}</td>
                  </tr>
                  <tr>
                    <td>Total Memory:</td>
                    <td>{formatBytes(selectedBrain.cow_stats?.total_bytes || 0)}</td>
                  </tr>
                  <tr>
                    <td>Reference Count:</td>
                    <td>{selectedBrain.cow_stats?.ref_count || 0}</td>
                  </tr>
                  <tr>
                    <td>Memory Savings:</td>
                    <td className="highlight">
                      {(selectedBrain.cow_stats?.memory_savings_pct || 0).toFixed(1)}%
                    </td>
                  </tr>
                </tbody>
              </table>
            </div>

            <div className="detail-section">
              <h4>Architecture</h4>
              <table className="stats-table">
                <tbody>
                  <tr>
                    <td>Neurons:</td>
                    <td>{(selectedBrain.architecture?.num_neurons || 0).toLocaleString()}</td>
                  </tr>
                  <tr>
                    <td>Synapses:</td>
                    <td>{(selectedBrain.architecture?.num_synapses || 0).toLocaleString()}</td>
                  </tr>
                  <tr>
                    <td>Inputs:</td>
                    <td>{selectedBrain.architecture?.num_inputs || 0}</td>
                  </tr>
                  <tr>
                    <td>Outputs:</td>
                    <td>{selectedBrain.architecture?.num_outputs || 0}</td>
                  </tr>
                </tbody>
              </table>
            </div>

            <div className="detail-section">
              <h4>Metadata</h4>
              <table className="stats-table">
                <tbody>
                  <tr>
                    <td>Created:</td>
                    <td>{new Date(selectedBrain.metadata?.created_at).toLocaleString()}</td>
                  </tr>
                  {selectedBrain.metadata?.parent_id !== null && (
                    <tr>
                      <td>Parent ID:</td>
                      <td>{selectedBrain.metadata?.parent_id}</td>
                    </tr>
                  )}
                  <tr>
                    <td>Clone Count:</td>
                    <td>{selectedBrain.metadata?.clone_count || 0}</td>
                  </tr>
                </tbody>
              </table>
            </div>
          </div>
        </div>
      )}
    </div>
  );
}

export default COWPanel;
