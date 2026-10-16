import React from 'react'
import {
  Chart as ChartJS,
  CategoryScale,
  LinearScale,
  PointElement,
  LineElement,
  Title,
  Tooltip,
  Legend,
  Filler
} from 'chart.js'
import { Line } from 'react-chartjs-2'

ChartJS.register(
  CategoryScale,
  LinearScale,
  PointElement,
  LineElement,
  Title,
  Tooltip,
  Legend,
  Filler
)

function MetricsCharts({ metricsData }) {
  const chartOptions = {
    responsive: true,
    maintainAspectRatio: false,
    plugins: {
      legend: { display: false }
    },
    scales: {
      x: { display: false },
      y: { beginAtZero: true }
    },
    animation: { duration: 0 }
  }

  const activityChartData = {
    labels: metricsData.timestamps,
    datasets: [{
      label: 'Network Activity',
      data: metricsData.activity,
      borderColor: '#667eea',
      backgroundColor: 'rgba(102, 126, 234, 0.1)',
      borderWidth: 2,
      fill: true,
      tension: 0.4
    }]
  }

  const weightChartData = {
    labels: metricsData.timestamps,
    datasets: [{
      label: 'Average Weight',
      data: metricsData.weight,
      borderColor: '#48bb78',
      backgroundColor: 'rgba(72, 187, 120, 0.1)',
      borderWidth: 2,
      fill: true,
      tension: 0.4
    }]
  }

  return (
    <div className="charts-grid">
      <div className="chart-panel">
        <h3>📈 Network Activity</h3>
        <div className="chart-container">
          <Line data={activityChartData} options={chartOptions} />
        </div>
      </div>

      <div className="chart-panel">
        <h3>⚖️ Average Weight</h3>
        <div className="chart-container">
          <Line data={weightChartData} options={chartOptions} />
        </div>
      </div>

      <div className="chart-panel">
        <h3>⚡ Spike Events</h3>
        <div className="chart-container">
          <Line data={weightChartData} options={chartOptions} />
        </div>
      </div>
    </div>
  )
}

export default MetricsCharts
