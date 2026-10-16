import { useState } from 'react';

interface Props {
  onSend: (text: string, mode: string) => void;
  disabled: boolean;
}

const MODES = [
  { id: 'chat', label: 'Chat' },
  { id: 'teach', label: 'Teach' },
  { id: 'introspect', label: 'Introspect' },
];

const PLACEHOLDERS: Record<string, string> = {
  chat: 'Say something...',
  teach: "Teach me something (e.g., 'cats: furry animals that purr')",
  introspect: 'Press Enter to ask the brain about itself',
};

export function ChatInput({ onSend, disabled }: Props) {
  const [text, setText] = useState('');
  const [mode, setMode] = useState('chat');

  const handleSubmit = (e: React.FormEvent) => {
    e.preventDefault();
    if (!text.trim() && mode !== 'introspect') return;
    onSend(text, mode);
    setText('');
  };

  return (
    <div>
      <div className="chat-mode-toggle">
        {MODES.map((m) => (
          <button
            key={m.id}
            className={`chat-mode-btn ${mode === m.id ? 'active' : ''}`}
            onClick={() => setMode(m.id)}
          >
            {m.label}
          </button>
        ))}
      </div>
      <form className="chat-input-bar" onSubmit={handleSubmit}>
        <input
          className="chat-input"
          value={text}
          onChange={(e) => setText(e.target.value)}
          placeholder={PLACEHOLDERS[mode] || 'Type a message...'}
          disabled={disabled}
        />
        <button className="btn btn-primary" type="submit" disabled={disabled}>
          Send
        </button>
      </form>
    </div>
  );
}
