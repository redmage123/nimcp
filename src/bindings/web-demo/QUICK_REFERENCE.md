# NIMCP Web Demo - Quick Reference Card

## 🚀 Start the Demo

```bash
cd /home/bbrelin/nimcp/src/bindings/web-demo
./start-demo.sh
```

Then open: **http://localhost:3005**

## 🛑 Stop the Demo

```bash
cd /home/bbrelin/nimcp/src/bindings/web-demo
./stop-demo.sh
```

## 📍 Service URLs

| Service | URL |
|---------|-----|
| Frontend | http://localhost:3005 |
| Backend API | http://localhost:5000 |
| API Docs | http://localhost:5000/docs |

## 🧪 Quick API Tests

```bash
# Status
curl http://localhost:5000/api/status

# Initialize
curl -X POST http://localhost:5000/api/init

# Train
curl -X POST http://localhost:5000/api/train \
  -H "Content-Type: application/json" \
  -d '{"features": [5.1, 3.5, 1.4, 0.2], "label": "setosa"}'

# Predict
curl -X POST http://localhost:5000/api/predict \
  -H "Content-Type: application/json" \
  -d '{"features": [5.0, 3.6, 1.4, 0.2]}'
```

## 🔧 Manual Start

**Backend**:
```bash
cd /home/bbrelin/nimcp/src/bindings/web-demo/backend
./venv/bin/python app.py
```

**Frontend** (new terminal):
```bash
cd /home/bbrelin/nimcp/src/bindings/web-demo/frontend
PORT=3005 npm start
```

## 📚 Documentation Files

- `README.md` - Full documentation
- `STARTUP.md` - Detailed startup guide
- `QUICK_START.md` - Quick start tutorial
- `DEPLOYMENT_SUMMARY.md` - Technical deployment details
- `backend/README.md` - Backend documentation
- `frontend/README.md` - Frontend documentation

## 🐛 Common Issues

### Backend won't start
```bash
# Reinstall dependencies
cd backend
./venv/bin/pip install -r requirements.txt

# Reinstall NIMCP bindings
cd ../../python
../web-demo/backend/venv/bin/pip install -e .
```

### Frontend won't compile
```bash
# Clear and reinstall
cd frontend
rm -rf node_modules package-lock.json
npm install
```

### Port conflicts
```bash
# Use different ports
# Backend: Edit app.py, change port in app.run()
# Frontend: PORT=3006 npm start
```

## 📦 Prerequisites

- ✅ Python 3.7+
- ✅ Node.js 14+
- ✅ NIMCP library built (`/home/bbrelin/nimcp/bin/libnimcp.so`)

## 🎯 Demo Features

1. **Initialize Brain** - Create NIMCP instance
2. **Quick Train** - Train on Iris dataset (15 examples)
3. **Manual Training** - Train on individual examples
4. **Predictions** - Classify iris flowers
5. **Real-time Charts** - Loss, confidence, distribution
6. **Metrics** - Training/prediction stats

## 📊 Test Data

**Setosa** (small flowers):
- [5.1, 3.5, 1.4, 0.2]
- [4.9, 3.0, 1.4, 0.2]

**Versicolor** (medium flowers):
- [7.0, 3.2, 4.7, 1.4]
- [6.4, 3.2, 4.5, 1.5]

**Virginica** (large flowers):
- [6.3, 3.3, 6.0, 2.5]
- [5.8, 2.7, 5.1, 1.9]

## ⚡ Performance

- Training: ~0.5ms per example
- Prediction: ~0.1ms per inference
- API latency: ~5-10ms round-trip

## 💡 Tips

- Train on at least 5-10 examples per class for good accuracy
- Use "Quick Train" to train on all examples at once
- Watch the loss chart - it should decrease over time
- Confidence > 0.3 usually indicates good prediction
- Try examples near class boundaries to test robustness

---

**Version**: 2.7.0 | **Last Updated**: Nov 9, 2025
