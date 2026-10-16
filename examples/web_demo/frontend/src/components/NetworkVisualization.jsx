import React, { useEffect, useRef } from 'react'
import { Network } from 'vis-network/standalone'

function NetworkVisualization({ topology, activeNeurons, onNodeSelect, selectedNeuron, showConnections }) {
  const containerRef = useRef(null)
  const networkRef = useRef(null)
  const edgesRef = useRef(null)

  useEffect(() => {
    if (!containerRef.current) return

    // Filter edges based on showConnections setting
    let filteredEdges = []
    if (showConnections === 'all') {
      filteredEdges = topology.edges || []
    } else if (showConnections === 'selected' && selectedNeuron !== null) {
      filteredEdges = (topology.edges || []).filter(edge =>
        edge.from === selectedNeuron || edge.to === selectedNeuron
      )
    }

    // Store edges for later updates
    edgesRef.current = filteredEdges

    const options = {
      nodes: {
        shape: 'dot',
        size: 20,
        font: { size: 12, color: '#333' },
        borderWidth: 2,
        borderWidthSelected: 4
      },
      edges: {
        width: 2,
        color: { color: '#cccccc', highlight: '#667eea' },
        arrows: { to: { enabled: true, scaleFactor: 0.5 } },
        smooth: { type: 'continuous' },
        selectionWidth: 3
      },
      physics: {
        enabled: true,
        stabilization: { enabled: true, iterations: 100 },
        barnesHut: {
          gravitationalConstant: -2000,
          centralGravity: 0.3,
          springLength: 150,
          springConstant: 0.04,
          damping: 0.09
        }
      },
      interaction: {
        hover: true,
        selectConnectedEdges: false
      }
    }

    const data = {
      nodes: topology.nodes.map(node => ({
        id: node.id,
        label: node.id.toString(),
        color: getNodeColor(node.activity || 0),
        title: `Neuron ${node.id}\nActivity: ${(node.activity || 0).toFixed(3)}`,
        value: 10 + (node.activity || 0) * 20
      })),
      edges: filteredEdges
    }

    if (!networkRef.current) {
      networkRef.current = new Network(containerRef.current, data, options)

      networkRef.current.on('select', (params) => {
        if (params.nodes.length > 0) {
          onNodeSelect(params.nodes[0])
        }
      })
    } else {
      networkRef.current.setData(data)
    }
  }, [topology, onNodeSelect, showConnections, selectedNeuron])

  // Highlight active neurons and connections
  useEffect(() => {
    if (!networkRef.current) return

    console.log('🎨 Updating visualization - Active neurons:', activeNeurons)

    const nodes = networkRef.current.body.data.nodes
    const edges = networkRef.current.body.data.edges

    // Create a Set for fast lookup
    const activeSet = new Set(activeNeurons)

    // Update all nodes based on whether they're active
    topology.nodes.forEach(node => {
      const isActive = activeSet.has(node.id)

      nodes.update({
        id: node.id,
        borderWidth: isActive ? 6 : 2,
        color: isActive ? {
          border: '#10b981',
          background: '#6ee7b7',
          highlight: { border: '#059669', background: '#34d399' }
        } : getNodeColor(node.activity || 0.05),
        shadow: isActive ? { enabled: true, color: '#10b981', size: 10 } : false,
        title: `Neuron ${node.id}\nActivity: ${(node.activity || 0).toFixed(3)}\nState: ${(node.state || 0).toFixed(3)}`
      })
    })

    // Reset all edges to default color
    if (edgesRef.current && edgesRef.current.length > 0) {
      edgesRef.current.forEach(edge => {
        edges.update({
          id: edge.id,
          color: { color: '#cccccc', opacity: 0.3 },
          width: 2
        })
      })
    }

    // Highlight active connections (connections between active neurons)
    if (activeNeurons.length > 0 && edgesRef.current) {

      edgesRef.current.forEach(edge => {
        const fromActive = activeSet.has(edge.from)
        const toActive = activeSet.has(edge.to)

        if (fromActive && toActive) {
          // Both neurons active - highlight connection strongly
          edges.update({
            id: edge.id,
            color: { color: '#f59e0b', opacity: 1 },
            width: 4,
            shadow: { enabled: true, color: '#f59e0b', size: 8 }
          })
        } else if (fromActive || toActive) {
          // One neuron active - show partial activation
          edges.update({
            id: edge.id,
            color: { color: '#fbbf24', opacity: 0.6 },
            width: 3
          })
        }
      })
    }
  }, [activeNeurons, topology])

  const getNodeColor = (activity) => {
    if (activity > 0.7) {
      return {
        border: '#f56565',
        background: '#feb2b2',
        highlight: { border: '#e53e3e', background: '#fc8181' }
      }
    } else if (activity > 0.3) {
      return {
        border: '#ed8936',
        background: '#fbd38d',
        highlight: { border: '#dd6b20', background: '#f6ad55' }
      }
    } else {
      return {
        border: '#667eea',
        background: '#b3bcf5',
        highlight: { border: '#5568d3', background: '#9fa8e4' }
      }
    }
  }

  return (
    <div className="panel">
      <h2>Neural Network Topology</h2>
      <div ref={containerRef} className="network-viz" />
    </div>
  )
}

export default NetworkVisualization
