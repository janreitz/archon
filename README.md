# Archon ECS

A high-performance, archetype-based Entity-Component-System library written in C++20.

## Features

- **Simple API**: Minimal, unopinionated interface that doesn't impose constraints on your architecture
- **Ergonomic component storage**: Store any type as a component
- **Flexible queries**: Iterate over components using any callable (lambdas, functions, functors)
- **Archetype-based storage**: Efficient memory layout using Structure of Arrays (SoA)
- **Template-based queries**: Compile-time type safety with zero-cost abstractions

## Basic Usage

```cpp
#include <archon/ecs.h>

// Define your components
struct Position {
    float x, y, z;
};

struct Velocity {
    float vx, vy, vz;
};

int main() {
    // Create world and register components
    ecs::World world;
    ecs::ComponentRegistry::instance().register_component<Position>();
    ecs::ComponentRegistry::instance().register_component<Velocity>();
    
    // Create entities and add components
    for (int i = 0; i < 1000; ++i) {
        auto entity = world.create_entity();
        world.add_components(entity, 
            Position{static_cast<float>(i), 0.0f, 0.0f},
            Velocity{1.0f, 0.0f, 0.0f}
        );
    }
    
    // Process entities with queries
    ecs::Query<Position, Velocity>().each(world, [](Position& pos, Velocity& vel) {
        pos.x += vel.vx;
        pos.y += vel.vy;
        pos.z += vel.vz;
    });
    
    return 0;
}
```

## Architecture

### Core Concepts

- **Entity**: A unique identifier (32-bit integer)
- **Component**: Plain data structures that hold state
- **Archetype**: Groups entities with the same component signature for efficient storage
- **World**: Container that manages all entities, components, and their relationships
- **Query**: Iterate over queried components of all entities that satisfy the query

### Memory Layout

Archon uses archetype-based storage where entities with the same component signature are stored together in contiguous memory. This provides:

- **Cache efficiency**: Components are stored in Structure of Arrays (SoA) format
- **Memory locality**: Related data is stored close together in memory

### Query System

The query system works with any callable - lambdas, functions, or functors. It automatically detects if your callable wants the entity ID as an additional parameter:

```cpp
// Lambda without entity
ecs::Query<Position, Velocity>().each(world, [](Position& pos, Velocity& vel) {
    pos.x += vel.vx;
});

// Function without entity
void update_position(Position& pos, Velocity& vel) {
    pos.x += vel.vx;
}
ecs::Query<Position, Velocity>().each(world, update_position);

// Lambda with entity
ecs::Query<Position>().each(world, [](Position& pos, ecs::EntityId entity) {
    // Process with entity ID
});

// Query with filters - components used for filtering but not iteration
struct PlayerTag {};
struct EnemyTag {};

// Only iterate Position/Velocity on entities that have PlayerTag
ecs::Query<Position, Velocity>()
    .with<PlayerTag>()
    .each(world, [](Position& pos, Velocity& vel) {
        // Only processes player entities
        pos.x += vel.vx;
    });

// Iterate all Position components except those on enemy entities
ecs::Query<Position>()
    .without<EnemyTag>()
    .each(world, [](Position& pos) {
        // Process all non-enemy positions
    });
```

## Building and Testing

### Build Options

```bash
# Build with tests
cmake -B build -DBUILD_TESTS=ON
cmake --build build

# Build with benchmarks
cmake -B build -DBUILD_BENCHMARKS=ON
cmake --build build

# Build with Tracy profiling
cmake -B build -DENABLE_TRACY=ON -DENABLE_PROFILING_FLAGS=ON
cmake --build build
```

### Running Tests

```bash
# Run all tests
cd build && ctest

# Run tests directly
./build/archon_unit_tests
```

### Running Benchmarks

```bash
# Run comprehensive benchmarks (uses catch2)
./build/archon_benchmarks

# Run profile benchmark (for external profilers)
./build/archon_profile_benchmark

# Run overhead analysis
./build/archon_overhead_analysis
```

### Integration

#### CMake FetchContent

```cmake
include(FetchContent)

FetchContent_Declare(
    archon
    GIT_REPOSITORY https://github.com/your-username/archon.git
    GIT_TAG        main  # or specific version tag
)

FetchContent_MakeAvailable(archon)
target_link_libraries(your_target PRIVATE archon)
```

#### CMake Submodule

```cmake
# If added as a git submodule
add_subdirectory(third_party/archon)
target_link_libraries(your_target PRIVATE archon)
```

#### CMake Package

```cmake
# If installed system-wide
find_package(archon REQUIRED)
target_link_libraries(your_target PRIVATE archon::archon)
```
