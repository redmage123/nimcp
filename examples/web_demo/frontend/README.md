# NIMCP React Web Demo

A modern React-based frontend for the NIMCP Neural Network visualization with real-time updates via WebSockets.

## Features

### Enhanced Network Visualization
- **Toggleable Connections**: Choose to show no connections, only selected neuron connections, or all connections
- **Click to Inspect**: Click any neuron to see detailed information including state, activity, weight statistics, and incoming connections
- **Real-time Updates**: Live visualization of active neurons during simulation

### Interactive Controls
- Start/Stop/Reset simulation
- Add synaptic connections with custom weights
- Apply plasticity rules (STDP, Oja, Homeostasis)
- Prune weak synapses

### Real-time Analytics
- Network activity chart
- Average weight chart
- Spike events chart
- Live activity log

## Quick Start

### Prerequisites
```bash
# Install Node.js (v16 or higher)
# On Ubuntu:
sudo apt install nodejs npm

# Or using nvm:
curl -o- https://raw.githubusercontent.com/nvm-sh/nvm/v0.39.0/install.sh | bash
nvm install 18
```

### Install Dependencies
```bash
cd /home/bbrelin/nimcp/examples/web_demo/frontend
npm install
```

### Run the App

#### Terminal 1 - Start Flask Backend
```bash
cd /home/bbrelin/nimcp/examples/web_demo
pip3 install flask-cors  # If not already installed
python3 app.py
```

The backend will run on http://localhost:5000

#### Terminal 2 - Start React Frontend
```bash
cd /home/bbrelin/nimcp/examples/web_demo/frontend
npm run dev
```

The frontend will run on http://localhost:3000 and proxy API/WebSocket requests to the backend.

### Access the Application
Open your browser and navigate to: **http://localhost:3000**

## Usage

### View Neuron Details
1. Click on any neuron in the network visualization
2. Detailed information will appear in the left sidebar including:
   - Current state
   - Activity level
   - Weight statistics (norm, mean, std)
   - Number of incoming connections

### Toggle Connection Display
1. Use the "Show Connections" dropdown above the network visualization
2. Options:
   - **None**: Hide all connections for cleaner view
   - **Selected Neuron Only**: Show only connections involving the currently selected neuron
   - **All Connections**: Display all synaptic connections in the network

### Start Simulation
1. Click "▶️ Start Simulation"
2. Watch neurons light up as they fire
3. Observe real-time chart updates

### Add Connection
1. Enter "From Neuron ID" (e.g., 0)
2. Enter "To Neuron ID" (e.g., 10)
3. Set "Weight" (e.g., 0.5)
4. Click "➕ Add Synapse"

### Apply Plasticity
1. Select plasticity rule: STDP, Oja's Rule, or Homeostasis
2. Enter target neuron ID
3. Click "⚡ Apply Plasticity"

## Architecture

### Frontend (React + Vite)
- **React 18**: Modern React with hooks
- **Socket.IO Client**: WebSocket communication
- **Chart.js**: Real-time charting with react-chartjs-2
- **vis-network**: Neural network topology visualization
- **Vite**: Fast development server with HMR

### Backend (Flask)
- **Flask**: REST API for network control
- **Flask-SocketIO**: WebSocket server for real-time updates
- **Flask-CORS**: Cross-origin resource sharing support
- **NIMCP Python Bindings**: Neural network engine

## Component Structure

```
src/
├── App.jsx                       # Main app component with state management
├── App.css                       # Global styles
├── index.jsx                     # React entry point
└── components/
    ├── NetworkVisualization.jsx  # vis-network graph with connection toggling
    ├── ControlPanel.jsx          # Simulation and network controls
    ├── MetricsCharts.jsx         # Real-time Chart.js charts
    ├── StatsCards.jsx            # Statistics display cards
    ├── NeuronDetails.jsx         # Detailed neuron information panel
    └── ActivityLog.jsx           # Activity log display
```

## Development

### Hot Module Replacement
Vite provides instant HMR - changes to React components will update immediately without full page reload.

### Build for Production
```bash
npm run build
```

The production build will be in the `dist/` directory.

### Preview Production Build
```bash
npm run preview
```

## API Endpoints

All API requests are proxied through Vite to the Flask backend:

- `GET /api/network/info` - Network information
- `GET /api/network/topology` - Network nodes and edges
- `GET /api/neuron/:id` - Detailed neuron information
- `POST /api/simulation/start` - Start simulation
- `POST /api/simulation/stop` - Stop simulation
- `POST /api/simulation/reset` - Reset network
- `POST /api/connection/add` - Add synaptic connection
- `POST /api/plasticity/apply` - Apply plasticity rule
- `POST /api/network/prune` - Prune weak synapses

## WebSocket Events

### Server → Client
- `simulation_update`: { timestamp, active_neurons }
- `metrics_update`: { activity, weight, timestamp }

### Client → Server
- `connect`: Connection established

## Troubleshooting

### Port Already in Use
```bash
# Kill process on port 3000
lsof -ti:3000 | xargs kill -9

# Or change port in vite.config.js
```

### CORS Errors
Ensure Flask backend has `flask-cors` installed and CORS is enabled in `app.py`.

### WebSocket Connection Failed
1. Verify Flask backend is running on port 5000
2. Check browser console for connection errors
3. Ensure firewall allows WebSocket connections

### Dependencies Not Installing
```bash
# Clear npm cache
npm cache clean --force

# Remove node_modules and reinstall
rm -rf node_modules package-lock.json
npm install
```

## Performance

- **React**: Virtual DOM for efficient updates
- **Vite**: Lightning-fast HMR and build times
- **Chart.js**: Hardware-accelerated canvas rendering
- **vis-network**: WebGL-accelerated network visualization

## Future Enhancements

- [ ] Dark mode toggle
- [ ] Network export (PNG, SVG)
- [ ] Metrics export (CSV, JSON)
- [ ] Save/load network configurations
- [ ] Multi-network comparison view
- [ ] Custom neuron activation functions
- [ ] Advanced topology layouts (circular, hierarchical)

## License

Part of the NIMCP (Neural Inference for Massive Concurrent Processing) project.
