# NIMCP Language Bindings

This directory contains language bindings for NIMCP. All bindings use **ONLY** the unified `nimcp.h` public API and do not depend on internal headers.

## Architecture

```
nimcp.h (Public API)
    ↓
nimcp.c (Wrapper Implementation)
    ↓
Internal APIs (brain, neuralnet, ethics, knowledge, etc.)
```

**Key Design Principles:**
- Single entry point: `nimcp.h`
- Consistent naming: All public symbols use `nimcp_` prefix
- Opaque handles: Internal structures are hidden
- Memory safety: Clear ownership boundaries
- Language-specific idioms: Each binding follows its language's conventions

## Available Bindings

| Language | Location | Technology | Status |
|----------|----------|------------|--------|
| **Python** | `python/` | CPython C API | ✅ Complete |
| **Node.js** | `nodejs/` | N-API | ✅ Complete |
| **Rust** | `rust/` | FFI | ✅ Complete |
| **Go** | `go/` | CGo | ✅ Complete |
| **Java** | `java/` | JNI | ✅ Complete |
| **C#** | `csharp/` | P/Invoke | ✅ Complete |
| **Perl** | `perl/` | XS | ✅ Complete |
| **Ruby** | `ruby/` | FFI | ✅ Complete |

## Building

### Prerequisites
1. Build the core library first:
   ```bash
   cd build
   cmake ..
   make nimcp
   ```

### Python
```bash
cd src/bindings/python
python setup.py install
```

### Node.js
```bash
cd src/bindings/nodejs
npm install
node-gyp rebuild
```

### Rust
```bash
cd src/bindings/rust
cargo build --release
```

### Go
```bash
cd src/bindings/go
go build
```

### Java
```bash
cd src/bindings/java
javac NIMCP.java
# Implement JNI bridge (nimcp_jni.c) - TODO
```

### C#
```bash
cd src/bindings/csharp
csc /target:library NIMCP.cs
```

### Perl
```bash
cd src/bindings/perl
perl Makefile.PL
make
make install
```

### Ruby
```bash
cd src/bindings/ruby
gem build nimcp.gemspec
gem install nimcp-2.6.1.gem
```

## Usage Examples

### Python
```python
import nimcp

nimcp.init()

# Create a brain
brain = nimcp.Brain(
    name="classifier",
    size=nimcp.BRAIN_SMALL,
    task=nimcp.TASK_CLASSIFICATION,
    num_inputs=5,
    num_outputs=3
)

# Learn from examples
brain.learn([1.0, 2.0, 3.0, 4.0, 5.0], "class_a", 0.95)

# Make predictions
label, confidence = brain.predict([1.5, 2.5, 3.5, 4.5, 5.5])
print(f"Predicted: {label} with confidence {confidence}")
```

### Node.js
```javascript
const nimcp = require('./build/Release/nimcp');

const network = new nimcp.NeuralNetwork({
    num_inputs: 10,
    num_outputs: 5,
    num_hidden: 20,
    learning_rate: 0.01
});

const outputs = network.forward([1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0]);
console.log('Outputs:', outputs);
```

### Rust
```rust
use nimcp::{init, Brain, BrainSize, BrainTask};

fn main() -> Result<(), String> {
    init()?;

    let mut brain = Brain::new(
        "classifier",
        BrainSize::Small,
        BrainTask::Classification,
        5,
        3
    )?;

    brain.learn(&[1.0, 2.0, 3.0, 4.0, 5.0], "class_a", 0.95)?;

    let (label, confidence) = brain.predict(&[1.5, 2.5, 3.5, 4.5, 5.5])?;
    println!("Predicted: {} with confidence {}", label, confidence);

    Ok(())
}
```

### Go
```go
package main

import (
    "fmt"
    "nimcp"
)

func main() {
    nimcp.Init()

    brain, err := nimcp.NewBrain(
        "classifier",
        nimcp.BrainSmall,
        nimcp.TaskClassification,
        5, 3,
    )
    if err != nil {
        panic(err)
    }
    defer brain.Destroy()

    brain.Learn([]float32{1.0, 2.0, 3.0, 4.0, 5.0}, "class_a", 0.95)

    label, confidence, err := brain.Predict([]float32{1.5, 2.5, 3.5, 4.5, 5.5})
    if err != nil {
        panic(err)
    }

    fmt.Printf("Predicted: %s with confidence %.2f\n", label, confidence)
}
```

### Java
```java
import com.nimcp.NIMCP;
import com.nimcp.NIMCP.Brain;

public class Example {
    public static void main(String[] args) throws NIMCP.NIMCPException {
        NIMCP.init();

        Brain brain = new Brain(
            "classifier",
            Brain.Size.SMALL,
            Brain.Task.CLASSIFICATION,
            5, 3
        );

        brain.learn(new float[]{1.0f, 2.0f, 3.0f, 4.0f, 5.0f}, "class_a", 0.95f);

        Brain.Prediction result = brain.predict(new float[]{1.5f, 2.5f, 3.5f, 4.5f, 5.5f});
        System.out.println("Predicted: " + result.label + " with confidence " + result.confidence);
    }
}
```

### C#
```csharp
using NIMCP;

class Program
{
    static void Main()
    {
        Library.Init();

        using (var brain = new Brain("classifier", BrainSize.Small, BrainTask.Classification, 5, 3))
        {
            brain.Learn(new float[] {1.0f, 2.0f, 3.0f, 4.0f, 5.0f}, "class_a", 0.95f);

            var (label, confidence) = brain.Predict(new float[] {1.5f, 2.5f, 3.5f, 4.5f, 5.5f});
            Console.WriteLine($"Predicted: {label} with confidence {confidence}");
        }
    }
}
```

### Perl
```perl
use NIMCP;

NIMCP::init();

my $brain = NIMCP::Brain->new(
    name => 'classifier',
    size => NIMCP::BRAIN_SMALL,
    task => NIMCP::TASK_CLASSIFICATION,
    num_inputs => 5,
    num_outputs => 3
);

$brain->learn([1.0, 2.0, 3.0, 4.0, 5.0], 'class_a', 0.95);

my ($label, $confidence) = $brain->predict([1.5, 2.5, 3.5, 4.5, 5.5]);
print "Predicted: $label with confidence $confidence\n";
```

### Ruby
```ruby
require 'nimcp'

NIMCP.init

brain = NIMCP::Brain.new(
  name: 'classifier',
  size: :small,
  task: :classification,
  num_inputs: 5,
  num_outputs: 3
)

brain.learn([1.0, 2.0, 3.0, 4.0, 5.0], 'class_a', 0.95)

result = brain.predict([1.5, 2.5, 3.5, 4.5, 5.5])
puts "Predicted: #{result[:label]} with confidence #{result[:confidence]}"

brain.destroy
NIMCP.shutdown
```

## Memory Management

### Important Guidelines

**Bindings Own Their Wrappers:**
- Use language-specific allocators (`malloc`, `new`, etc.) for wrapper structs
- Do NOT use `nimcp_malloc`/`nimcp_free` in binding code
- Example:
  ```c
  // Node.js binding - CORRECT
  BrainWrap* wrap = malloc(sizeof(BrainWrap));  // ← Use malloc()
  wrap->brain = nimcp_brain_create(...);         // ← NIMCP owns internal handle
  ```

**NIMCP Owns Internal Objects:**
- All `nimcp_*_create()` calls allocate using `nimcp_malloc` internally
- All `nimcp_*_destroy()` calls free using `nimcp_free` internally
- Bindings should call public API functions, never manage NIMCP memory directly

**Lifecycle:**
1. Binding creates wrapper → Uses language allocator
2. Wrapper calls `nimcp_*_create()` → NIMCP allocates internal object
3. Wrapper calls `nimcp_*_destroy()` → NIMCP frees internal object
4. Binding destroys wrapper → Uses language deallocator

## API Coverage

All bindings currently expose:

### Brain API
- `nimcp_brain_create()`
- `nimcp_brain_destroy()`
- `nimcp_brain_learn_example()`
- `nimcp_brain_predict()`
- `nimcp_brain_save()`
- `nimcp_brain_load()`

### Network API
- `nimcp_network_create()`
- `nimcp_network_destroy()`
- `nimcp_network_forward()`

### Utility Functions
- `nimcp_init()`
- `nimcp_shutdown()`
- `nimcp_version()`
- `nimcp_get_error()`

## Future Additions

To be added in future versions:
- Ethics API bindings (`nimcp_ethics_*`)
- Knowledge API bindings (`nimcp_knowledge_*`)
- Additional language bindings (Swift, Kotlin, Julia)

## Testing

Each binding should include basic tests:
- Library initialization
- Object creation/destruction
- Basic operations
- Error handling

## Contributing

When adding new bindings:
1. Follow the architecture: Use ONLY `nimcp.h`
2. Implement proper memory management
3. Follow language conventions
4. Add usage examples
5. Add basic tests

## License

Same as main NIMCP project.
