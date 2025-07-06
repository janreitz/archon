# Archon ECS

A minimal archetype-based Entity-Component-System library written in C++20:

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
    // Components must be registered before use:
    ecs::register_component<Position>();
    ecs::register_component<Velocity>();
    
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
- **Archetype**: Entities with the same component signature are grouped into Archetypes, which store components in Structure of Arrays (SoA) format. This provides cache-efficient access patterns during queries.
- **World**: Container that manages all entities, components, and their relationships
- **Query**: Template-based system for iterating over components with compile-time type safety

### Query System

The query system supports:
- **Template-based queries**: `ecs::Query<Position, Velocity>()`
- **Optional entity parameter**: Functions can optionally take `ecs::EntityId` as last parameter
- **Filtering**: `.with<Component>()` and `.without<Component>()` methods
- **Const-correctness**: Const world references require const component parameters

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

### Type-Trait Based Component Operations
Archon automatically selects optimal operations to transition between archetypes based on component type traits. It uses `std::memcpy` for `std::is_trivially_copyable_v<T>`, moves when it's safe (`std::is_nothrow_move_constructible_v<T>`) and only calls copy constructors as fallback.


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
