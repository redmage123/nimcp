# NIMCP Web Demo - Startup Guide

## Quick Start (Recommended)

The easiest way to start the demo is using the automated startup script:

```bash
cd /home/bbrelin/nimcp/src/bindings/web-demo
./start-demo.sh
```

To stop the demo:

```bash
./stop-demo.sh
```

## What the Startup Script Does

1. **Checks Prerequisites**
   - Verifies NIMCP library is built (`libnimcp.so`)
   - Creates Python virtual environment if needed
   - Installs Python dependencies (Flask, flask-cors, numpy)
   - Installs NIMCP Python bindings
   - Installs Node.js dependencies if needed

2. **Starts Backend**
   - Launches Flask server on `http://localhost:5000`
   - Waits for backend to be ready

3. **Starts Frontend**
   - Launches React development server on `http://localhost:3005`
   - Opens in default browser (if available)

## Manual Startup

If you prefer to start components manually or need to troubleshoot:

### 1. Prerequisites

Ensure you have:
- Python 3.7+ installed
- Node.js 14+ and npm installed
- NIMCP library built (in `/home/bbrelin/nimcp/bin/libnimcp.so`)

### 2. Backend Setup

```bash
cd /home/bbrelin/nimcp/src/bindings/web-demo/backend

# Create virtual environment (first time only)
python3 -m venv venv

# Activate virtual environment
source venv/bin/activate  # On Linux/Mac
# OR
venv\Scripts\activate  # On Windows

# Install dependencies (first time only)
pip install -r requirements.txt

# Install NIMCP Python bindings (first time only)
cd ../../python
pip install -e .
cd ../web-demo/backend

# Start backend server
python app.py
```

Backend will be available at: `http://localhost:5000`

### 3. Frontend Setup

Open a new terminal:

```bash
cd /home/bbrelin/nimcp/src/bindings/web-demo/frontend

# Install dependencies (first time only)
npm install

# Start development server
PORT=3005 npm start
```

Frontend will be available at: `http://localhost:3005`

## Accessing the Demo

Once both servers are running:

1. Open your browser to `http://localhost:3005`
2. Click "Initialize Brain" to create a NIMCP brain instance
3. Use "Quick Train" to train on the Iris dataset
4. Enter flower measurements to make predictions
5. Watch real-time charts showing training progress and metrics

## Testing the API

You can test the backend API directly:

```bash
# Check status
curl http://localhost:5000/api/status

# Initialize brain
curl -X POST http://localhost:5000/api/init

# Train on an example
curl -X POST http://localhost:5000/api/train \
  -H "Content-Type: application/json" \
  -d '{"features": [5.1, 3.5, 1.4, 0.2], "label": "setosa"}'

# Make a prediction
curl -X POST http://localhost:5000/api/predict \
  -H "Content-Type: application/json" \
  -d '{"features": [5.0, 3.6, 1.4, 0.2]}'
```

## Troubleshooting

### "NIMCP library not found"

Build the NIMCP library first:

```bash
cd /home/bbrelin/nimcp/build
cmake ..
make
```

Verify the library exists:

```bash
ls -la /home/bbrelin/nimcp/bin/libnimcp.so
```

### "ModuleNotFoundError: No module named 'nimcp'"

Install the NIMCP Python bindings:

```bash
cd /home/bbrelin/nimcp/src/bindings/python
/home/bbrelin/nimcp/src/bindings/web-demo/backend/venv/bin/pip install -e .
```

### "Port already in use"

If port 5000 or 3005 is already in use, you can change them:

**Backend (port 5000):**
Edit `backend/app.py` and change the port in the last line:
```python
app.run(host='0.0.0.0', port=5001, debug=True)  # Change 5000 to 5001
```

**Frontend (port 3005):**
Use the PORT environment variable:
```bash
PORT=3006 npm start  # Use port 3006 instead
```

### Backend crashes on startup

Check the error message. Common issues:

1. **Missing dependencies**: Run `pip install -r requirements.txt` in the backend venv
2. **Wrong Python bindings parameters**: This has been fixed in the current version
3. **Missing numpy**: Run `pip install numpy` in the backend venv

### Frontend compilation errors

1. Delete `node_modules` and reinstall:
   ```bash
   cd frontend
   rm -rf node_modules package-lock.json
   npm install
   ```

2. Clear npm cache if issues persist:
   ```bash
   npm cache clean --force
   npm install
   ```

### CORS errors in browser console

Make sure:
1. Backend is running and accessible
2. Flask-CORS is installed: `pip install flask-cors`
3. Backend is using the correct port (check browser console for the exact URL being called)

## Demo Features

### Training Panel
- **Manual Training**: Enter individual examples
- **Quick Train**: Train on entire Iris dataset (15 examples)
- **Real-time Loss**: See training loss decrease as the brain learns

### Prediction Panel
- **Input Features**: Sepal length, sepal width, petal length, petal width
- **Predictions**: Get flower species classification
- **Confidence Scores**: See prediction confidence (0-1)

### Metrics Dashboard
- **Training Loss Chart**: Loss over time
- **Prediction Confidence Chart**: Confidence trends
- **Class Distribution**: Bar chart of predictions by class
- **Recent Predictions Table**: Last 10 predictions with timing
- **Statistics**: Total trained, predictions, accuracy, timing

## Architecture

```
┌─────────────────┐         ┌──────────────────┐
│   React App     │  HTTP   │   Flask API      │
│  (Port 3005)    │ ◄─────► │   (Port 5000)    │
│                 │  JSON   │                  │
│ ┌─────────────┐ │         │ ┌──────────────┐ │
│ │Training     │ │         │ │NIMCP Brain   │ │
│ │Panel        │ │         │ │Python Module │ │
│ └─────────────┘ │         │ └──────────────┘ │
│ ┌─────────────┐ │         │                  │
│ │Prediction   │ │         │ libnimcp.so      │
│ │Panel        │ │         │                  │
│ └─────────────┘ │         └──────────────────┘
│ ┌─────────────┐ │
│ │Metrics      │ │
│ │Dashboard    │ │
│ └─────────────┘ │
└─────────────────┘
```

## Additional Resources

- **Full Documentation**: See `README.md`
- **Quick Start Guide**: See `QUICK_START.md`
- **API Documentation**: Visit `http://localhost:5000/docs` when backend is running
- **Backend Details**: See `backend/README.md`
- **Frontend Details**: See `frontend/README.md`

## Support

If you encounter issues not covered here:

1. Check the terminal output for error messages
2. Verify all prerequisites are installed
3. Ensure NIMCP library is built and accessible
4. Check that no other services are using ports 5000 and 3005
5. Review the troubleshooting section above

## Version Information

- **Demo Version**: 2.7.0
- **NIMCP Version**: 2.7.0
- **Python**: 3.7+
- **Node.js**: 14+
- **React**: 18.2+
- **Flask**: 3.0+

Last Updated: November 9, 2025
