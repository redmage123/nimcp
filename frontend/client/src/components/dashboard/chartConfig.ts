import {
  Chart as ChartJS,
  CategoryScale,
  LinearScale,
  PointElement,
  LineElement,
  BarElement,
  ArcElement,
  Title,
  Tooltip,
  Legend,
  Filler,
} from 'chart.js';

ChartJS.register(
  CategoryScale, LinearScale, PointElement, LineElement,
  BarElement, ArcElement, Title, Tooltip, Legend, Filler
);

export function chartOptions(yLabel: string, isBar: boolean) {
  return {
    responsive: true,
    maintainAspectRatio: false,
    animation: { duration: 200 },
    plugins: {
      legend: { display: false },
    },
    scales: {
      x: {
        ticks: { color: '#9ca3af', maxTicksLimit: 10 },
        grid: { color: 'rgba(255,255,255,0.06)' },
      },
      y: {
        title: { display: true, text: yLabel, color: '#9ca3af' },
        ticks: { color: '#9ca3af' },
        grid: { color: 'rgba(255,255,255,0.06)' },
        beginAtZero: isBar,
      },
    },
  } as const;
}

/** Generate per-point radius array: all 0 except last point = 4 */
export function lastPointRadius(count: number): number[] {
  const radii = new Array(count).fill(0);
  if (count > 0) radii[count - 1] = 4;
  return radii;
}

/** Generate per-point color array: all transparent except last = accentColor */
export function lastPointColor(count: number, accentColor: string): string[] {
  const colors = new Array(count).fill('transparent');
  if (count > 0) colors[count - 1] = accentColor;
  return colors;
}
