import React, { useState, useEffect } from 'react'
import './CheckpointManager.css'

const CheckpointManager = ({ onLog }) => {
  const [checkpoints, setCheckpoints] = useState([])
  const [saving, setSaving] = useState(false)
  const [restoring, setRestoring] = useState(false)

  // Load checkpoints from localStorage on mount
  useEffect(() => {
    loadCheckpoints()
  }, [])

  const loadCheckpoints = () => {
    try {
      const stored = localStorage.getItem('nimcp_checkpoints')
      if (stored) {
        const parsed = JSON.parse(stored)
        setCheckpoints(parsed)
      }
    } catch (error) {
      console.error('Error loading checkpoints:', error)
      onLog?.('Error', 'Failed to load checkpoints from storage')
    }
  }

  const saveCheckpoints = (newCheckpoints) => {
    try {
      localStorage.setItem('nimcp_checkpoints', JSON.stringify(newCheckpoints))
      setCheckpoints(newCheckpoints)
    } catch (error) {
      console.error('Error saving checkpoints:', error)
      onLog?.('Error', 'Failed to save checkpoints to storage')
    }
  }

  const handleSaveCheckpoint = async () => {
    setSaving(true)
    try {
      const response = await fetch('/api/checkpoint/save', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ compress: true })
      })

      const result = await response.json()

      if (result.success) {
        const checkpoint = result.checkpoint

        // Add friendly name and save
        const newCheckpoint = {
          ...checkpoint,
          id: Date.now(),
          name: `Checkpoint ${new Date().toLocaleString()}`,
          created_display: new Date(checkpoint.created_at * 1000).toLocaleString()
        }

        const updated = [...checkpoints, newCheckpoint]
        saveCheckpoints(updated)

        onLog?.('Checkpoint', `Saved (${formatBytes(checkpoint.size_bytes)})`)
      } else {
        onLog?.('Error', result.error || 'Failed to save checkpoint')
      }
    } catch (error) {
      console.error('Error saving checkpoint:', error)
      onLog?.('Error', 'Failed to save checkpoint')
    } finally {
      setSaving(false)
    }
  }

  const handleRestoreCheckpoint = async (checkpoint) => {
    if (!confirm(`Restore checkpoint from ${checkpoint.created_display}?\nThis will replace your current network state.`)) {
      return
    }

    setRestoring(true)
    try {
      const response = await fetch('/api/checkpoint/restore', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          data: checkpoint.data,
          timestamp: checkpoint.timestamp
        })
      })

      const result = await response.json()

      if (result.success) {
        onLog?.('Checkpoint', 'Restored successfully')
        // Force page reload to refresh topology
        window.location.reload()
      } else {
        onLog?.('Error', result.error || 'Failed to restore checkpoint')
      }
    } catch (error) {
      console.error('Error restoring checkpoint:', error)
      onLog?.('Error', 'Failed to restore checkpoint')
    } finally {
      setRestoring(false)
    }
  }

  const handleDeleteCheckpoint = (id) => {
    if (!confirm('Delete this checkpoint?')) {
      return
    }

    const updated = checkpoints.filter(cp => cp.id !== id)
    saveCheckpoints(updated)
    onLog?.('Checkpoint', 'Deleted')
  }

  const handleRenameCheckpoint = (id) => {
    const checkpoint = checkpoints.find(cp => cp.id === id)
    const newName = prompt('Enter new name:', checkpoint.name)

    if (newName && newName !== checkpoint.name) {
      const updated = checkpoints.map(cp =>
        cp.id === id ? { ...cp, name: newName } : cp
      )
      saveCheckpoints(updated)
    }
  }

  const handleExportCheckpoint = (checkpoint) => {
    try {
      const dataStr = JSON.stringify(checkpoint, null, 2)
      const blob = new Blob([dataStr], { type: 'application/json' })
      const url = URL.createObjectURL(blob)
      const link = document.createElement('a')
      link.href = url
      link.download = `nimcp-checkpoint-${checkpoint.id}.json`
      document.body.appendChild(link)
      link.click()
      document.body.removeChild(link)
      URL.revokeObjectURL(url)
      onLog?.('Checkpoint', 'Exported to file')
    } catch (error) {
      onLog?.('Error', 'Failed to export checkpoint')
    }
  }

  const handleImportCheckpoint = () => {
    const input = document.createElement('input')
    input.type = 'file'
    input.accept = '.json'
    input.onchange = async (e) => {
      try {
        const file = e.target.files[0]
        const text = await file.text()
        const checkpoint = JSON.parse(text)

        // Validate checkpoint structure
        if (!checkpoint.data || !checkpoint.timestamp) {
          throw new Error('Invalid checkpoint format')
        }

        const updated = [...checkpoints, checkpoint]
        saveCheckpoints(updated)
        onLog?.('Checkpoint', 'Imported successfully')
      } catch (error) {
        onLog?.('Error', 'Failed to import checkpoint: ' + error.message)
      }
    }
    input.click()
  }

  const formatBytes = (bytes) => {
    if (bytes < 1024) return bytes + ' B'
    if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + ' KB'
    return (bytes / (1024 * 1024)).toFixed(1) + ' MB'
  }

  return (
    <div className="checkpoint-manager">
      <div className="checkpoint-header">
        <h3>💾 Checkpoints</h3>
        <p className="checkpoint-description">
          Save and restore network states. Checkpoints are stored in your browser.
        </p>
      </div>

      <div className="checkpoint-actions">
        <button
          className="btn-primary"
          onClick={handleSaveCheckpoint}
          disabled={saving}
        >
          {saving ? '⏳ Saving...' : '💾 Save Checkpoint'}
        </button>
        <button
          className="btn-secondary"
          onClick={handleImportCheckpoint}
        >
          📥 Import
        </button>
      </div>

      <div className="checkpoint-list">
        {checkpoints.length === 0 ? (
          <div className="empty-state">
            <p>No checkpoints saved</p>
            <p className="hint">Click "Save Checkpoint" to create one</p>
          </div>
        ) : (
          checkpoints.map(checkpoint => (
            <div key={checkpoint.id} className="checkpoint-item">
              <div className="checkpoint-info">
                <div className="checkpoint-name">{checkpoint.name}</div>
                <div className="checkpoint-meta">
                  <span>⏱️ T={checkpoint.timestamp}</span>
                  <span>📊 {formatBytes(checkpoint.size_bytes)}</span>
                  {checkpoint.compressed && <span>🗜️ Compressed</span>}
                </div>
              </div>
              <div className="checkpoint-buttons">
                <button
                  className="btn-restore"
                  onClick={() => handleRestoreCheckpoint(checkpoint)}
                  disabled={restoring}
                  title="Restore this checkpoint"
                >
                  ↩️
                </button>
                <button
                  className="btn-icon"
                  onClick={() => handleRenameCheckpoint(checkpoint.id)}
                  title="Rename"
                >
                  ✏️
                </button>
                <button
                  className="btn-icon"
                  onClick={() => handleExportCheckpoint(checkpoint)}
                  title="Export to file"
                >
                  📤
                </button>
                <button
                  className="btn-delete"
                  onClick={() => handleDeleteCheckpoint(checkpoint.id)}
                  title="Delete"
                >
                  🗑️
                </button>
              </div>
            </div>
          ))
        )}
      </div>

      {checkpoints.length > 0 && (
        <div className="checkpoint-footer">
          <p className="checkpoint-count">
            {checkpoints.length} checkpoint{checkpoints.length !== 1 ? 's' : ''} stored
          </p>
        </div>
      )}
    </div>
  )
}

export default CheckpointManager
