interface Props {
  micActive: boolean;
  camActive: boolean;
  onToggleMic: () => void;
  onToggleCam: () => void;
  videoRef: React.RefObject<HTMLVideoElement>;
}

export function MediaControls({ micActive, camActive, onToggleMic, onToggleCam, videoRef }: Props) {
  return (
    <div className="media-controls">
      <button
        className={`chat-media-btn ${micActive ? 'active' : ''}`}
        onClick={onToggleMic}
        title={micActive ? 'Stop microphone' : 'Start microphone'}
      >
        {micActive ? '🔴 Mic' : '🎤 Mic'}
      </button>
      <button
        className={`chat-media-btn ${camActive ? 'active' : ''}`}
        onClick={onToggleCam}
        title={camActive ? 'Stop camera' : 'Start camera'}
      >
        {camActive ? '🔴 Cam' : '📷 Cam'}
      </button>
      {camActive && (
        <video
          ref={videoRef}
          className="media-preview"
          autoPlay
          muted
          playsInline
        />
      )}
      {micActive && (
        <span className="media-recording-indicator">Recording...</span>
      )}
    </div>
  );
}
