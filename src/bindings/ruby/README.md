# NIMCP Ruby Bindings

Ruby bindings for NIMCP (Neural Interface Message Communication Protocol) using FFI.

## Installation

### Prerequisites

1. Build the NIMCP core library:
   ```bash
   cd build
   cmake ..
   make nimcp
   ```

2. Install the gem:
   ```bash
   gem build nimcp.gemspec
   gem install nimcp-2.6.1.gem
   ```

Or add to your Gemfile:
```ruby
gem 'nimcp', path: 'path/to/nimcp/src/bindings/ruby'
```

## Usage

### Basic Example

```ruby
require 'nimcp'

# Initialize the library
NIMCP.init

# Check version
puts "NIMCP version: #{NIMCP.version}"

# Create a brain for classification
brain = NIMCP::Brain.new(
  name: 'classifier',
  size: :small,
  task: :classification,
  num_inputs: 5,
  num_outputs: 3
)

# Learn from examples
brain.learn([1.0, 2.0, 3.0, 4.0, 5.0], 'class_a', 0.95)
brain.learn([2.0, 3.0, 4.0, 5.0, 6.0], 'class_b', 0.90)
brain.learn([3.0, 4.0, 5.0, 6.0, 7.0], 'class_c', 0.85)

# Make predictions
result = brain.predict([1.5, 2.5, 3.5, 4.5, 5.5])
puts "Predicted: #{result[:label]} with confidence #{result[:confidence]}"

# Save and load
brain.save('brain.dat')
loaded_brain = NIMCP::Brain.load('brain.dat')

# Cleanup
brain.destroy
NIMCP.shutdown
```

### Neural Network Example

```ruby
require 'nimcp'

NIMCP.init

# Create a neural network
network = NIMCP::Network.new(
  num_inputs: 10,
  num_outputs: 5,
  num_hidden: 20,
  learning_rate: 0.01
)

# Forward pass
inputs = (1..10).map(&:to_f)
outputs = network.forward(inputs)

puts "Outputs: #{outputs.inspect}"

# Cleanup
network.destroy
NIMCP.shutdown
```

### Using Blocks for Automatic Cleanup

```ruby
require 'nimcp'

NIMCP.init

begin
  brain = NIMCP::Brain.new(
    name: 'temp_brain',
    size: :tiny,
    task: :classification,
    num_inputs: 3,
    num_outputs: 2
  )

  # Use the brain...
  brain.learn([1.0, 2.0, 3.0], 'positive', 1.0)
  result = brain.predict([1.5, 2.5, 3.5])

  puts result[:label]
ensure
  brain.destroy if brain
  NIMCP.shutdown
end
```

## API Reference

### Module Methods

- `NIMCP.init` - Initialize the NIMCP library
- `NIMCP.shutdown` - Shutdown the library
- `NIMCP.version` - Get version string

### Brain Class

#### Constructor

```ruby
NIMCP::Brain.new(
  name: String,
  size: Symbol,           # :tiny, :small, :medium, :large
  task: Symbol,           # :classification, :regression, :pattern_matching, :sequence, :association
  num_inputs: Integer,
  num_outputs: Integer
)
```

#### Instance Methods

- `learn(features, label, confidence = 1.0)` - Learn from an example
  - `features`: Array of floats
  - `label`: String
  - `confidence`: Float (0.0-1.0)
  - Returns: self (for chaining)

- `predict(features)` - Make a prediction
  - `features`: Array of floats
  - Returns: Hash with `:label` and `:confidence`

- `save(filepath)` - Save brain to file
  - `filepath`: String
  - Returns: self

- `destroy` - Free resources (called automatically by GC)

#### Class Methods

- `NIMCP::Brain.load(filepath)` - Load brain from file
  - Returns: Brain instance

### Network Class

#### Constructor

```ruby
NIMCP::Network.new(
  num_inputs: Integer,
  num_outputs: Integer,
  num_hidden: Integer = 100,
  learning_rate: Float = 0.01
)
```

#### Instance Methods

- `forward(inputs)` - Forward pass through network
  - `inputs`: Array of floats
  - Returns: Array of floats (outputs)

- `destroy` - Free resources (called automatically by GC)

#### Attributes

- `num_outputs` - Number of output neurons (read-only)

## Brain Sizes

| Size | Neurons | Memory | Inference Time |
|------|---------|--------|----------------|
| `:tiny` | 100 | <1MB | ~0.1ms |
| `:small` | 1K | ~10MB | ~0.5ms |
| `:medium` | 10K | ~50MB | ~5ms |
| `:large` | 100K | ~500MB | ~50ms |

## Brain Tasks

- `:classification` - Multi-class classification
- `:regression` - Continuous value prediction
- `:pattern_matching` - Pattern recognition
- `:sequence` - Temporal sequence learning
- `:association` - Association learning

## Error Handling

All methods raise `NIMCP::Error` on failure:

```ruby
begin
  brain = NIMCP::Brain.new(
    name: 'test',
    size: :small,
    task: :classification,
    num_inputs: 5,
    num_outputs: 3
  )
rescue NIMCP::Error => e
  puts "Error: #{e.message}"
end
```

## Memory Management

The bindings use automatic memory management via Ruby's garbage collector. However, you can manually free resources:

```ruby
brain = NIMCP::Brain.new(...)
# Use brain...
brain.destroy  # Explicitly free resources
```

Resources are automatically freed when objects are garbage collected, but explicit cleanup is recommended for long-running applications.

## Thread Safety

The NIMCP library is not thread-safe. Use proper synchronization when accessing NIMCP objects from multiple threads.

## License

MIT License - Same as main NIMCP project
