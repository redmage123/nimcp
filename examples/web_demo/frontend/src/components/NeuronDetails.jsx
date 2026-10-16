import React, { useEffect, useState } from 'react'

function NeuronDetails({ neuronId }) {
  const [details, setDetails] = useState(null)
  const [loading, setLoading] = useState(false)

  useEffect(() => {
    if (neuronId === null) {
      setDetails(null)
      return
    }

    const fetchDetails = async () => {
      setLoading(true)
      try {
        const response = await fetch(`/api/neuron/${neuronId}`)
        const data = await response.json()
        setDetails(data)
      } catch (error) {
        console.error('Error fetching neuron details:', error)
      } finally {
        setLoading(false)
      }
    }

    fetchDetails()
  }, [neuronId])

  if (neuronId === null) {
    return (
      <div className="neuron-details">
        <h4>Neuron Details</h4>
        <p className="hint">Click on a neuron to view details</p>
      </div>
    )
  }

  if (loading) {
    return (
      <div className="neuron-details">
        <h4>Neuron Details</h4>
        <p>Loading...</p>
      </div>
    )
  }

  if (!details) {
    return null
  }

  return (
    <div className="neuron-details">
      <h4>Neuron #{details.id}</h4>
      <div className="detail-row">
        <span>State:</span>
        <span>{details.state?.toFixed(4) || '0.0000'}</span>
      </div>
      <div className="detail-row">
        <span>Activity:</span>
        <span>{details.activity?.toFixed(4) || '0.0000'}</span>
      </div>
      <div className="detail-row">
        <span>Weight Norm:</span>
        <span>{details.weight_norm?.toFixed(4) || '0.0000'}</span>
      </div>
      <div className="detail-row">
        <span>Weight Mean:</span>
        <span>{details.weight_mean?.toFixed(4) || '0.0000'}</span>
      </div>
      <div className="detail-row">
        <span>Weight Std:</span>
        <span>{details.weight_std?.toFixed(4) || '0.0000'}</span>
      </div>
      <div className="detail-row">
        <span>Incoming Connections:</span>
        <span>{details.incoming_count || 0}</span>
      </div>
      {details.warning && (
        <div className="warning-text">
          ⚠️ {details.warning}
        </div>
      )}
    </div>
  )
}

export default NeuronDetails
