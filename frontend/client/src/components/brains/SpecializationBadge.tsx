interface Props {
  level: number;
  label: string;
  small?: boolean;
}

const LEVEL_COLORS: Record<number, { bg: string; text: string }> = {
  0: { bg: 'rgba(107,114,128,0.2)', text: '#9ca3af' },  // Pending — gray
  1: { bg: 'rgba(59,130,246,0.2)', text: '#60a5fa' },    // Infant — blue
  2: { bg: 'rgba(245,158,11,0.2)', text: '#fbbf24' },    // Developing — amber
  3: { bg: 'rgba(34,197,94,0.2)', text: '#4ade80' },     // Mature — green
  4: { bg: 'rgba(234,179,8,0.25)', text: '#facc15' },    // Expert — gold
};

export function SpecializationBadge({ level, label, small }: Props) {
  const colors = LEVEL_COLORS[level] ?? LEVEL_COLORS[0];
  const isPending = level === 0;

  return (
    <span
      style={{
        display: 'inline-flex',
        alignItems: 'center',
        gap: 4,
        padding: small ? '1px 6px' : '2px 8px',
        borderRadius: 999,
        fontSize: small ? 10 : 11,
        fontWeight: 600,
        background: colors.bg,
        color: colors.text,
        lineHeight: 1.4,
        animation: isPending ? 'pulse 2s ease-in-out infinite' : undefined,
      }}
    >
      {label}
    </span>
  );
}
