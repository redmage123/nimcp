# COW Brain Cloning - Quick Start Guide

## What You'll Learn

This 5-minute guide will show you how to use Copy-on-Write (COW) brain cloning in the NIMCP web demo to achieve 65-99% memory savings.

## Prerequisites

- NIMCP web demo running (backend + frontend)
- Basic understanding of neural networks

## Step-by-Step Tutorial

### Step 1: Start the Demo (2 minutes)

**Terminal 1 - Backend:**
```bash
cd /home/bbrelin/nimcp/src/bindings/web-demo/backend
python app.py
```

**Terminal 2 - Frontend:**
```bash
cd /home/bbrelin/nimcp/src/bindings/web-demo/frontend
npm start
```

**Browser:** Open http://localhost:3000

### Step 2: Initialize Brain (30 seconds)

1. Click the **"Initialize Brain"** button
2. Wait for confirmation message
3. You should see "Brain initialized successfully"

### Step 3: Train the Brain (1 minute)

1. Click **"Train on Dataset"** button
2. Watch the training progress
3. See loss decrease in real-time
4. Training complete when all examples processed

### Step 4: Navigate to COW Cloning Tab (5 seconds)

1. Click the **"COW Cloning"** tab in navigation
2. You'll see the COW Panel with your brain listed

### Step 5: Create Your First Clone (10 seconds)

1. Find your brain in the hierarchy (iris_classifier)
2. Click the **"Clone (COW)"** button
3. Watch clone appear instantly (< 10ms)
4. Notice the clone is marked with a special badge

### Step 6: View Memory Savings (30 seconds)

Check the **Memory Efficiency** panel at the top:

```
Without COW: 20.00 MB   (2 brains × 10MB each)
With COW:    10.50 MB   (10MB shared + 0.5KB private)
Savings:     47.5%
```

### Step 7: Create More Clones (1 minute)

1. Click **"Clone (COW)"** again on the original brain
2. Create 2-3 more clones
3. Watch memory savings increase:
   - 3 clones = ~71% savings
   - 5 clones = ~80% savings
   - 10 clones = ~86% savings

### Step 8: Inspect Clone Details (30 seconds)

1. Click **"Details"** on any clone
2. View detailed statistics:
   - Shared memory (MB)
   - Private memory (KB)
   - Reference count
   - Memory savings percentage
   - Architecture info

### Step 9: Delete a Clone (10 seconds)

1. Click **"Delete"** on any clone
2. Confirm deletion
3. Watch parent's clone count decrease
4. Memory savings update automatically

## Understanding the Results

### What You Created

```
Original Brain (ID: 0)
├── Clone 1 (ID: 1) - shares 10MB, uses 0.5KB private
├── Clone 2 (ID: 2) - shares 10MB, uses 0.5KB private
└── Clone 3 (ID: 3) - shares 10MB, uses 0.5KB private

Total Memory:
- Without COW: 40 MB (4 × 10MB)
- With COW: 11.5 MB (10MB shared + 4×0.5KB)
- Savings: 71.3%
```

### Key Insights

1. **Clone Speed**: < 10ms vs ~1000ms for full copy
2. **Memory Sharing**: ~99% of network is shared
3. **Scalability**: More clones = higher savings percentage
4. **Independence**: Each clone can perform inference separately

## Common Use Cases

### 1. Parallel Inference
Create multiple clones to process different inputs simultaneously:
```
Original Brain (trained model)
├── Clone 1 (processes user A's request)
├── Clone 2 (processes user B's request)
└── Clone 3 (processes user C's request)
```

### 2. Checkpointing
Create snapshot before training:
```
Trained Brain
└── Checkpoint Clone (snapshot before further training)
```

### 3. A/B Testing
Test different strategies:
```
Base Model
├── Strategy A Clone
└── Strategy B Clone
```

## API Examples

### Create Clone (JavaScript)
```javascript
const response = await fetch('/api/brain/0/clone_cow', {
  method: 'POST'
});
const data = await response.json();
console.log(`Clone ${data.clone_id} created in ${data.clone_time*1000}ms`);
console.log(`Memory savings: ${data.cow_stats.memory_savings_pct}%`);
```

### Get Statistics (JavaScript)
```javascript
const response = await fetch('/api/brain/1/cow_stats');
const data = await response.json();
console.log(`Shared: ${data.cow_stats.shared_bytes / 1024 / 1024} MB`);
console.log(`Private: ${data.cow_stats.private_bytes / 1024} KB`);
```

### List All Brains (Python)
```python
import requests
response = requests.get('http://localhost:5000/api/brains')
brains = response.json()['brains']
for brain in brains:
    print(f"{brain['name']}: {brain['clone_count']} clones")
```

## Testing Your Setup

Run the automated test script:

```bash
cd /home/bbrelin/nimcp/src/bindings/web-demo
python3 test_cow_integration.py
```

Expected output:
```
======================================================================
TEST: Initialize Brain
======================================================================
✓ Brain initialized with ID: 0

======================================================================
TEST: Create COW Clone
======================================================================
✓ Clone created with ID: 1
✓ Parent ID: 0
✓ Clone time: 2.34ms
✓ Is COW clone: True
✓ Shared memory: 10.00 MB
✓ Private memory: 0.50 KB
✓ Memory savings: 99.9%

...

======================================================================
ALL TESTS PASSED!
======================================================================
```

## Troubleshooting

### "Brain not found" error
- **Issue**: Invalid brain ID
- **Fix**: Check `/api/brains` endpoint to see valid IDs

### Clone creation slow
- **Issue**: Python bindings not compiled with COW support
- **Fix**: Rebuild NIMCP with latest version

### Memory savings showing 0%
- **Issue**: NIMCP version doesn't support COW
- **Fix**: Update to NIMCP v2.7.0 or later

### UI not updating
- **Issue**: Backend not running or CORS error
- **Fix**: Check browser console and backend logs

## Next Steps

1. **Read Full Documentation**: See [COW_INTEGRATION_GUIDE.md](COW_INTEGRATION_GUIDE.md)
2. **Explore Python API**: Check `/home/bbrelin/nimcp/docs/PYTHON_BRAIN_COW.md`
3. **Try Examples**: Run `/home/bbrelin/nimcp/examples/python_brain_cow_demo.py`
4. **Integrate into Your App**: Use the API endpoints in your own projects

## Performance Tips

1. **Create clones from trained brains** - Training invalidates COW sharing
2. **Use clones for read-only inference** - Writes trigger copy
3. **Monitor reference counts** - Higher counts = more sharing
4. **Delete unused clones** - Free up references when done

## Key Takeaways

- COW cloning is **instant** (< 10ms)
- Memory savings increase with **more clones** (up to 99%)
- Perfect for **parallel inference** scenarios
- Shares network structure, **not training history**
- Clones are **independent** after creation

## Resources

- Main README: `/home/bbrelin/nimcp/src/bindings/web-demo/README.md`
- Integration Guide: `/home/bbrelin/nimcp/src/bindings/web-demo/COW_INTEGRATION_GUIDE.md`
- Python COW Docs: `/home/bbrelin/nimcp/docs/PYTHON_BRAIN_COW.md`
- Example Code: `/home/bbrelin/nimcp/examples/python_brain_cow_demo.py`

## Support

Questions? Issues?
- GitHub: https://github.com/bbrelin/nimcp
- Docs: http://localhost:5000/docs
- Email: support@nimcp.org

---

**Congratulations!** You now know how to use COW brain cloning in NIMCP. Start saving memory today!
