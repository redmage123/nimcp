import { useRef, useEffect, useMemo } from 'react';
import type { BrainProbe } from '../../types';

interface Props {
  probe: BrainProbe | null;
}

interface Node {
  x: number;
  y: number;
  baseRadius: number;
  glow: number;       // 0-1: current glow intensity
  glowTarget: number; // 0-1: target glow (lerps toward this)
  firing: number;     // 0-1: firing flash (decays)
}

interface Edge {
  from: number;
  to: number;
}

// Seeded pseudo-random for consistent layout
function seededRandom(seed: number) {
  let s = seed;
  return () => {
    s = (s * 16807 + 0) % 2147483647;
    return s / 2147483647;
  };
}

function generateNetwork(nodeCount: number, edgeCount: number) {
  const rng = seededRandom(42);
  const nodes: Node[] = [];
  const edges: Edge[] = [];

  // Place nodes with some padding from edges
  const pad = 0.08;
  for (let i = 0; i < nodeCount; i++) {
    nodes.push({
      x: pad + rng() * (1 - 2 * pad),
      y: pad + rng() * (1 - 2 * pad),
      baseRadius: 3 + rng() * 3,
      glow: 0,
      glowTarget: 0,
      firing: 0,
    });
  }

  // Connect nearby nodes preferentially
  const candidates: { i: number; j: number; dist: number }[] = [];
  for (let i = 0; i < nodeCount; i++) {
    for (let j = i + 1; j < nodeCount; j++) {
      const dx = nodes[i].x - nodes[j].x;
      const dy = nodes[i].y - nodes[j].y;
      candidates.push({ i, j, dist: Math.sqrt(dx * dx + dy * dy) });
    }
  }
  candidates.sort((a, b) => a.dist - b.dist);

  // Take top edgeCount closest pairs
  const count = Math.min(edgeCount, candidates.length);
  for (let k = 0; k < count; k++) {
    edges.push({ from: candidates[k].i, to: candidates[k].j });
  }

  return { nodes, edges };
}

export function NeuralActivityCanvas({ probe }: Props) {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const animRef = useRef<number>(0);
  const networkRef = useRef(generateNetwork(40, 60));
  const prevProbeRef = useRef<BrainProbe | null>(null);

  // Compute activity ratio
  const activityRatio = useMemo(() => {
    if (!probe || probe.num_synapses === 0) return 0;
    return probe.num_active_synapses / probe.num_synapses;
  }, [probe?.num_active_synapses, probe?.num_synapses]);

  // When probe changes, update glow targets and trigger firing
  useEffect(() => {
    const net = networkRef.current;
    const activeCount = Math.floor(activityRatio * net.nodes.length);

    // Update glow targets
    for (let i = 0; i < net.nodes.length; i++) {
      net.nodes[i].glowTarget = i < activeCount ? 0.6 + Math.random() * 0.4 : 0.05;
    }

    // Trigger firing on new probe data
    if (prevProbeRef.current && probe) {
      const changed = prevProbeRef.current.num_active_synapses !== probe.num_active_synapses
        || prevProbeRef.current.total_inferences !== probe.total_inferences;
      if (changed) {
        // Fire a random subset of active nodes
        const fireCount = Math.max(3, Math.floor(activeCount * 0.4));
        for (let i = 0; i < fireCount; i++) {
          const idx = Math.floor(Math.random() * net.nodes.length);
          net.nodes[idx].firing = 1.0;
        }
      }
    }
    prevProbeRef.current = probe;
  }, [probe, activityRatio]);

  // Animation loop
  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    function draw() {
      const c = canvasRef.current!;
      const ctx2 = c.getContext('2d')!;
      const w = c.offsetWidth;
      const h = c.offsetHeight;
      const dpr = window.devicePixelRatio || 1;
      c.width = w * dpr;
      c.height = h * dpr;
      ctx2.setTransform(dpr, 0, 0, dpr, 0, 0);

      // Clear with dark bg
      ctx2.fillStyle = 'rgba(18, 18, 31, 0.95)';
      ctx2.fillRect(0, 0, w, h);

      const net = networkRef.current;

      // Draw edges
      for (const edge of net.edges) {
        const a = net.nodes[edge.from];
        const b = net.nodes[edge.to];
        const avgGlow = (a.glow + b.glow) / 2;
        ctx2.beginPath();
        ctx2.moveTo(a.x * w, a.y * h);
        ctx2.lineTo(b.x * w, b.y * h);
        ctx2.strokeStyle = `rgba(102, 126, 234, ${0.08 + avgGlow * 0.25})`;
        ctx2.lineWidth = 0.5 + avgGlow;
        ctx2.stroke();
      }

      // Draw nodes
      for (const node of net.nodes) {
        // Lerp glow toward target
        node.glow += (node.glowTarget - node.glow) * 0.08;
        // Decay firing
        node.firing *= 0.92;
        if (node.firing < 0.01) node.firing = 0;

        const cx = node.x * w;
        const cy = node.y * h;
        const r = node.baseRadius + node.glow * 3 + node.firing * 6;

        // Outer glow
        if (node.glow > 0.1 || node.firing > 0.05) {
          const grad = ctx2.createRadialGradient(cx, cy, r * 0.5, cx, cy, r * 3);
          const alpha = Math.min(0.4, node.glow * 0.3 + node.firing * 0.5);
          grad.addColorStop(0, `rgba(102, 126, 234, ${alpha})`);
          grad.addColorStop(1, 'rgba(102, 126, 234, 0)');
          ctx2.beginPath();
          ctx2.arc(cx, cy, r * 3, 0, Math.PI * 2);
          ctx2.fillStyle = grad;
          ctx2.fill();
        }

        // Firing flash (bright white-ish)
        if (node.firing > 0.05) {
          ctx2.beginPath();
          ctx2.arc(cx, cy, r + node.firing * 8, 0, Math.PI * 2);
          ctx2.fillStyle = `rgba(255, 255, 255, ${node.firing * 0.3})`;
          ctx2.fill();
        }

        // Node core
        ctx2.beginPath();
        ctx2.arc(cx, cy, r, 0, Math.PI * 2);
        const brightness = 80 + node.glow * 175;
        const green = 100 + node.glow * 80;
        ctx2.fillStyle = `rgba(${brightness}, ${green}, 234, ${0.3 + node.glow * 0.7})`;
        ctx2.fill();
      }

      animRef.current = requestAnimationFrame(draw);
    }

    draw();
    return () => cancelAnimationFrame(animRef.current);
  }, []);

  return (
    <div className="neural-canvas-panel">
      <div className="panel-title">Neural Activity</div>
      <canvas ref={canvasRef} style={{ width: '100%', height: 240 }} />
    </div>
  );
}
