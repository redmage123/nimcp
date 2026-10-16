import React from 'react'

function ActivityLog({ logs }) {
  return (
    <div className="panel">
      <h2>Activity Log</h2>
      <div className="log-panel">
        {logs.map((log, index) => (
          <div key={index} className="log-entry">
            <span className="timestamp">[{log.timestamp}]</span>
            <strong>{log.category}:</strong> {log.message}
          </div>
        ))}
      </div>
    </div>
  )
}

export default ActivityLog
