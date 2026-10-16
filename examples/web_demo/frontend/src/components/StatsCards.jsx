import React from 'react'

function StatsCards({ neuronCount, timestamp, activityLevel, avgWeight }) {
  return (
    <div className="stats-grid">
      <div className="stat-card">
        <div className="label">Neurons</div>
        <div className="value">{neuronCount}</div>
      </div>
      <div className="stat-card">
        <div className="label">Timestamp</div>
        <div className="value">{timestamp}</div>
      </div>
      <div className="stat-card">
        <div className="label">Activity</div>
        <div className="value">{activityLevel.toFixed(3)}</div>
      </div>
      <div className="stat-card">
        <div className="label">Avg Weight</div>
        <div className="value">{avgWeight.toFixed(3)}</div>
      </div>
    </div>
  )
}

export default StatsCards
