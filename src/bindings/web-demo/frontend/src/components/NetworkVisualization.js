/**
 * NetworkVisualization Component
 * ================================
 *
 * WHAT: 3D visualization of NIMCP neural network structure
 * WHY:  Allow humans to understand and explore the network topology
 * HOW:  Three.js rendering of neurons and synaptic connections
 */

import React, { useEffect, useRef, useState } from 'react';
import axios from 'axios';

const API_URL = '/api';

function NetworkVisualization({ brainInitialized }) {
  const canvasRef = useRef(null);
  const [networkData, setNetworkData] = useState(null);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState(null);
  const [viewMode, setViewMode] = useState('3d'); // '3d' or 'connectivity'
  const [showConnections, setShowConnections] = useState(false);
  const [selectedNeuron, setSelectedNeuron] = useState(null);

  // Animation state
  const animationRef = useRef(null);
  const sceneRef = useRef(null);

  useEffect(() => {
    // Only load Three.js and fetch data if brain is initialized
    if (!brainInitialized) {
      return;
    }

    // Load Three.js dynamically
    const script = document.createElement('script');
    script.src = 'https://cdn.jsdelivr.net/npm/three@0.150.0/build/three.min.js';
    script.async = true;
    script.onload = () => {
      console.log('Three.js loaded');
      fetchNetworkData();
    };
    document.body.appendChild(script);

    return () => {
      if (animationRef.current) {
        cancelAnimationFrame(animationRef.current);
      }
      if (document.body.contains(script)) {
        document.body.removeChild(script);
      }
    };
  }, [brainInitialized]);

  const fetchNetworkData = async () => {
    setLoading(true);
    setError(null);

    try {
      const response = await axios.get(`${API_URL}/network/visualize`);
      if (response.data.success) {
        setNetworkData(response.data.network);
        initializeVisualization(response.data.network);
      } else {
        setError(response.data.message || 'Failed to fetch network data');
      }
    } catch (err) {
      setError(`Failed to fetch network: ${err.message}`);
    } finally {
      setLoading(false);
    }
  };

  const initializeVisualization = (data) => {
    if (!window.THREE || !canvasRef.current) return;

    const THREE = window.THREE;

    // Clear previous scene
    if (sceneRef.current) {
      sceneRef.current.clear();
    }

    // Scene setup
    const scene = new THREE.Scene();
    scene.background = new THREE.Color(0x0a0a0a);
    sceneRef.current = scene;

    // Camera
    const camera = new THREE.PerspectiveCamera(
      75,
      canvasRef.current.clientWidth / canvasRef.current.clientHeight,
      0.1,
      1000
    );
    camera.position.z = 150;
    camera.position.y = 50;
    camera.lookAt(0, 0, 0);

    // Renderer
    const renderer = new THREE.WebGLRenderer({
      canvas: canvasRef.current,
      antialias: true,
      alpha: true
    });
    renderer.setSize(canvasRef.current.clientWidth, canvasRef.current.clientHeight);
    renderer.setPixelRatio(window.devicePixelRatio);

    // Lighting
    const ambientLight = new THREE.AmbientLight(0xffffff, 0.6);
    scene.add(ambientLight);

    const pointLight = new THREE.PointLight(0xffffff, 0.8);
    pointLight.position.set(50, 50, 50);
    scene.add(pointLight);

    // Create neurons
    const neuronGeometry = new THREE.SphereGeometry(0.5, 16, 16);
    const excitatoryMaterial = new THREE.MeshPhongMaterial({
      color: 0x00ff88,
      emissive: 0x00ff88,
      emissiveIntensity: 0.2,
      shininess: 100
    });
    const inhibitoryMaterial = new THREE.MeshPhongMaterial({
      color: 0xff4444,
      emissive: 0xff4444,
      emissiveIntensity: 0.2,
      shininess: 100
    });

    const neurons = [];
    data.neurons.forEach((neuron) => {
      const material = neuron.type === 'excitatory' ? excitatoryMaterial : inhibitoryMaterial;
      const mesh = new THREE.Mesh(neuronGeometry, material);

      mesh.position.set(neuron.x, neuron.y, neuron.z);
      mesh.userData = { id: neuron.id, type: neuron.type, activity: neuron.activity };

      scene.add(mesh);
      neurons.push(mesh);
    });

    // Create connections (if enabled)
    if (showConnections && data.connections) {
      const lineMaterial = new THREE.LineBasicMaterial({
        color: 0x4488ff,
        transparent: true,
        opacity: 0.1
      });

      data.connections.forEach((conn) => {
        const sourceNeuron = neurons.find(n => n.userData.id === conn.source);
        const targetNeuron = neurons.find(n => n.userData.id === conn.target);

        if (sourceNeuron && targetNeuron) {
          const points = [];
          points.push(sourceNeuron.position);
          points.push(targetNeuron.position);

          const geometry = new THREE.BufferGeometry().setFromPoints(points);
          const line = new THREE.Line(geometry, lineMaterial);
          scene.add(line);
        }
      });
    }

    // Add grid helper
    const gridHelper = new THREE.GridHelper(200, 20, 0x444444, 0x222222);
    scene.add(gridHelper);

    // Mouse interaction
    const raycaster = new THREE.Raycaster();
    const mouse = new THREE.Vector2();

    const onMouseMove = (event) => {
      const rect = canvasRef.current.getBoundingClientRect();
      mouse.x = ((event.clientX - rect.left) / rect.width) * 2 - 1;
      mouse.y = -((event.clientY - rect.top) / rect.height) * 2 + 1;

      raycaster.setFromCamera(mouse, camera);
      const intersects = raycaster.intersectObjects(neurons);

      // Reset all neurons
      neurons.forEach(n => {
        n.scale.set(1, 1, 1);
      });

      if (intersects.length > 0) {
        const neuron = intersects[0].object;
        neuron.scale.set(2, 2, 2);
        canvasRef.current.style.cursor = 'pointer';
      } else {
        canvasRef.current.style.cursor = 'default';
      }
    };

    const onClick = (event) => {
      const rect = canvasRef.current.getBoundingClientRect();
      mouse.x = ((event.clientX - rect.left) / rect.width) * 2 - 1;
      mouse.y = -((event.clientY - rect.top) / rect.height) * 2 + 1;

      raycaster.setFromCamera(mouse, camera);
      const intersects = raycaster.intersectObjects(neurons);

      if (intersects.length > 0) {
        const neuron = intersects[0].object;
        setSelectedNeuron(neuron.userData);
      } else {
        setSelectedNeuron(null);
      }
    };

    canvasRef.current.addEventListener('mousemove', onMouseMove);
    canvasRef.current.addEventListener('click', onClick);

    // Animation loop
    let rotation = 0;
    const animate = () => {
      animationRef.current = requestAnimationFrame(animate);

      // Auto-rotate camera
      rotation += 0.002;
      camera.position.x = Math.sin(rotation) * 150;
      camera.position.z = Math.cos(rotation) * 150;
      camera.lookAt(0, 0, 0);

      // Pulse neurons based on activity
      neurons.forEach((neuron, i) => {
        const pulse = Math.sin(Date.now() * 0.001 + i) * 0.2 + 1;
        const activity = neuron.userData.activity || 0.5;
        neuron.material.emissiveIntensity = 0.1 + activity * pulse * 0.5;
      });

      renderer.render(scene, camera);
    };

    animate();

    // Handle window resize
    const handleResize = () => {
      if (!canvasRef.current) return;

      camera.aspect = canvasRef.current.clientWidth / canvasRef.current.clientHeight;
      camera.updateProjectionMatrix();
      renderer.setSize(canvasRef.current.clientWidth, canvasRef.current.clientHeight);
    };

    window.addEventListener('resize', handleResize);

    return () => {
      window.removeEventListener('resize', handleResize);
      canvasRef.current?.removeEventListener('mousemove', onMouseMove);
      canvasRef.current?.removeEventListener('click', onClick);
    };
  };

  const renderStats = () => {
    if (!networkData) return null;

    return (
      <div className="network-stats">
        <div className="stat-item">
          <span className="label">Total Neurons:</span>
          <span className="value">{networkData.total_neurons.toLocaleString()}</span>
        </div>
        <div className="stat-item">
          <span className="label">Displayed:</span>
          <span className="value">{networkData.neurons.length}</span>
        </div>
        <div className="stat-item">
          <span className="label">Excitatory:</span>
          <span className="value" style={{ color: '#00ff88' }}>
            {networkData.neurons.filter(n => n.type === 'excitatory').length}
          </span>
        </div>
        <div className="stat-item">
          <span className="label">Inhibitory:</span>
          <span className="value" style={{ color: '#ff4444' }}>
            {networkData.neurons.filter(n => n.type === 'inhibitory').length}
          </span>
        </div>
        {networkData.connections && (
          <div className="stat-item">
            <span className="label">Connections:</span>
            <span className="value">{networkData.connections.length.toLocaleString()}</span>
          </div>
        )}
      </div>
    );
  };

  const renderNeuronInfo = () => {
    if (!selectedNeuron) return null;

    return (
      <div className="neuron-info">
        <h4>Neuron #{selectedNeuron.id}</h4>
        <div className="info-item">
          <span className="label">Type:</span>
          <span className={`value ${selectedNeuron.type}`}>
            {selectedNeuron.type}
          </span>
        </div>
        <div className="info-item">
          <span className="label">Activity:</span>
          <span className="value">{(selectedNeuron.activity * 100).toFixed(1)}%</span>
        </div>
        <button
          className="btn btn-small"
          onClick={() => setSelectedNeuron(null)}
        >
          Close
        </button>
      </div>
    );
  };

  return (
    <div className="network-visualization">
      <div className="visualization-header">
        <h2>🌐 Neural Network Visualization</h2>

        <div className="visualization-controls">
          <button
            className="btn btn-small"
            onClick={fetchNetworkData}
            disabled={loading || !brainInitialized}
          >
            {loading ? 'Loading...' : '🔄 Refresh'}
          </button>

          <label className="control-label">
            <input
              type="checkbox"
              checked={showConnections}
              onChange={(e) => {
                setShowConnections(e.target.checked);
                if (networkData) initializeVisualization(networkData);
              }}
              disabled={!brainInitialized}
            />
            Show Connections
          </label>
        </div>
      </div>

      {!brainInitialized && (
        <div className="info-message" style={{
          background: 'rgba(102, 126, 234, 0.1)',
          border: '1px solid var(--primary)',
          borderRadius: '8px',
          padding: '2rem',
          margin: '2rem 0',
          textAlign: 'center'
        }}>
          <h3>Brain Not Initialized</h3>
          <p>Please go to the <strong>Interactive Demo</strong> tab and initialize a brain first.</p>
          <p>Once initialized, return here to visualize the neural network structure.</p>
        </div>
      )}

      {error && brainInitialized && (
        <div className="error-message">
          <span>⚠️ {error}</span>
        </div>
      )}

      <div className="visualization-container">
        <canvas
          ref={canvasRef}
          className="network-canvas"
          style={{
            width: '100%',
            height: '600px',
            borderRadius: '8px',
            background: '#0a0a0a'
          }}
        />

        {renderStats()}
        {renderNeuronInfo()}

        {loading && (
          <div className="visualization-loading">
            <div className="spinner"></div>
            <p>Loading network data...</p>
          </div>
        )}
      </div>

      <div className="visualization-legend">
        <h4>Legend</h4>
        <div className="legend-items">
          <div className="legend-item">
            <span className="color-box" style={{ background: '#00ff88' }}></span>
            <span>Excitatory Neurons</span>
          </div>
          <div className="legend-item">
            <span className="color-box" style={{ background: '#ff4444' }}></span>
            <span>Inhibitory Neurons</span>
          </div>
          {showConnections && (
            <div className="legend-item">
              <span className="color-box" style={{ background: '#4488ff' }}></span>
              <span>Synaptic Connections</span>
            </div>
          )}
        </div>
        <p className="legend-note">
          Click on neurons to see details. Network auto-rotates for better viewing.
        </p>
      </div>
    </div>
  );
}

export default NetworkVisualization;
