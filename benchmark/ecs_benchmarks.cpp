#include "BenchmarkComponents.h"
#include "benchmark_macros.h"

#include <archon/ecs.h>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <vector>

// --- Helper data and functions ---
constexpr std::size_t ENTITY_COUNT_FOR_BENCHMARK = 10000;
constexpr std::size_t COMPONENT_DATA_SIZE = 128;

// Create component type aliases using numeric IDs
using ComponentA = benchmark::BenchmarkComponent<1, COMPONENT_DATA_SIZE>;
using ComponentB = benchmark::BenchmarkComponent<2, COMPONENT_DATA_SIZE>;

// Multi-archetype component definitions
using Position = benchmark::BenchmarkComponent<3, 24>;      // 3D position (3 * 8 bytes)
using Velocity = benchmark::BenchmarkComponent<4, 24>;      // 3D velocity (3 * 8 bytes)
using Renderable = benchmark::BenchmarkComponent<5, 64>;    // Render data (mesh, texture info)
using Health = benchmark::BenchmarkComponent<6, 8>;         // Health points
using Mass = benchmark::BenchmarkComponent<7, 8>;           // Physics mass
using Collider = benchmark::BenchmarkComponent<8, 32>;      // Collision data

// Helper to set up a world with entities having two BenchComp components
void setup_world_two_components(ecs::World &world, std::size_t entity_count)
{
    // Ensure components are registered (only needs to happen once, but safe to
    // call multiple times)
    ecs::register_component<ComponentA>();
    ecs::register_component<ComponentB>();
    for (std::size_t i = 0; i < entity_count; ++i) {
        auto e = world.create_entity();
        world.add_components(
            e,
            ComponentA::initialize_sequential(static_cast<uint8_t>(i)),
            ComponentB::initialize_sequential(static_cast<uint8_t>(i + entity_count)));
    }
}

// Helper for AoS (Array of Structs) iteration - current "baseline"
struct AoSBenchmark {
    ComponentA comp_1;
    ComponentB comp_2;
};

std::vector<AoSBenchmark> setup_aos_data(std::size_t entity_count)
{
    std::vector<AoSBenchmark> data_vec;
    data_vec.resize(entity_count);
    for (std::size_t i = 0; i < entity_count; ++i) {
        data_vec[i] = {ComponentA::initialize_sequential(static_cast<uint8_t>(i)),
                       ComponentB::initialize_sequential(static_cast<uint8_t>(i + entity_count))};
    }
    return data_vec;
}

// Helper for true SoA (Structure of Arrays) iteration
struct SoABenchmark {
    std::vector<ComponentA> comp_a_data;
    std::vector<ComponentB> comp_b_data;
};

SoABenchmark setup_soa_data(std::size_t entity_count)
{
    SoABenchmark data;
    data.comp_a_data.reserve(entity_count);
    data.comp_b_data.reserve(entity_count);

    for (std::size_t i = 0; i < entity_count; ++i) {
        data.comp_a_data.push_back(
            ComponentA::initialize_sequential(static_cast<uint8_t>(i)));
        data.comp_b_data.push_back(
            ComponentB::initialize_sequential(static_cast<uint8_t>(i + entity_count)));
    }
    return data;
}

// Helper for C-style raw arrays
struct RawArrayBenchmark {
    ComponentA *comp_a_data;
    ComponentB *comp_b_data;
    std::size_t count;

    RawArrayBenchmark(std::size_t entity_count) : count(entity_count)
    {
        comp_a_data = new ComponentA[entity_count];
        comp_b_data = new ComponentB[entity_count];

        for (std::size_t i = 0; i < entity_count; ++i) {
            comp_a_data[i] = ComponentA::initialize_sequential(static_cast<uint8_t>(i));
            comp_b_data[i] = ComponentB::initialize_sequential(static_cast<uint8_t>(i + entity_count));
        }
    }

    ~RawArrayBenchmark()
    {
        delete[] comp_a_data;
        delete[] comp_b_data;
    }
};

void run_iteration_performance_benchmarks()
{
    std::cout << "\n=== ECS Iteration Performance ===\n" << std::flush;
    uint64_t dummy_accumulator = 0; // To prevent optimization

    // --- Benchmark for iterating two components with the ECS API ---
    BENCHMARK("ECS Query: Iterate 2 Components", {
        ecs::World world;
        setup_world_two_components(world, ENTITY_COUNT_FOR_BENCHMARK);

        meter.measure([&world, &dummy_accumulator] {
            dummy_accumulator = 0;
            ecs::Query<ComponentA, ComponentB>().each(
                world, [&](ComponentA &c1,
                           ComponentB &c2) { // c1 and c2 are references
                    dummy_accumulator +=
                        benchmark::elementwise_addition<COMPONENT_DATA_SIZE>(
                            c1, c2);
                });
            return dummy_accumulator;
        });
    });

    // --- Benchmark for AoS (Array of Structs) baseline ---
    BENCHMARK("Baseline: AoS std::vector<struct>", {
        auto aos_data = setup_aos_data(ENTITY_COUNT_FOR_BENCHMARK);

        meter.measure([&aos_data, &dummy_accumulator] {
            dummy_accumulator = 0;
            for (const auto &item : aos_data) {
                dummy_accumulator +=
                    benchmark::elementwise_addition<COMPONENT_DATA_SIZE>(
                        item.comp_1, item.comp_2);
            }
            return dummy_accumulator;
        });
    });

    // --- Benchmark for true SoA (Structure of Arrays) baseline ---
    BENCHMARK("Baseline: SoA separate std::vectors", {
        auto soa_data = setup_soa_data(ENTITY_COUNT_FOR_BENCHMARK);

        meter.measure([&soa_data, &dummy_accumulator] {
            dummy_accumulator = 0;
            for (std::size_t i = 0; i < ENTITY_COUNT_FOR_BENCHMARK; ++i) {
                dummy_accumulator +=
                    benchmark::elementwise_addition<COMPONENT_DATA_SIZE>(
                        soa_data.comp_a_data[i], soa_data.comp_b_data[i]);
            }
            return dummy_accumulator;
        });
    });

    // --- Benchmark for C-style raw arrays baseline ---
    BENCHMARK("Baseline: Raw C arrays", {
        RawArrayBenchmark raw_data(ENTITY_COUNT_FOR_BENCHMARK);

        meter.measure([&raw_data, &dummy_accumulator] {
            dummy_accumulator = 0;
            for (std::size_t i = 0; i < raw_data.count; ++i) {
                dummy_accumulator +=
                    benchmark::elementwise_addition<COMPONENT_DATA_SIZE>(
                        raw_data.comp_a_data[i], raw_data.comp_b_data[i]);
            }
            return dummy_accumulator;
        });
    });

    // --- Benchmark for iterating one component with the ECS API ---
    BENCHMARK("ECS Query: Iterate 1 Component", {
        ecs::World world;
        ecs::register_component<ComponentA>();
        for (std::size_t i = 0; i < ENTITY_COUNT_FOR_BENCHMARK; ++i) {
            auto e = world.create_entity();
            world.add_components(
                e, ComponentA::initialize_sequential(static_cast<uint8_t>(i)));
        }

        meter.measure([&world, &dummy_accumulator] {
            dummy_accumulator = 0;
            ecs::Query<ComponentA>().each(
                world,
                [&](ComponentA &c1) { // c1 is a reference
                    // Minimal operation: sum first element to simulate access
                    if constexpr (COMPONENT_DATA_SIZE > 0) {
                        dummy_accumulator += c1.data_[0];
                    } else {
                        dummy_accumulator += 0;
                    }
                });
            return dummy_accumulator;
        });
    });
}

void run_component_type_scaling_benchmarks()
{
    std::cout << "\n=== ECS Component Type Scaling ===\n" << std::flush;
    uint64_t dummy_accumulator = 0;
    constexpr std::size_t ENTITY_COUNT = 5000;
    constexpr std::size_t COMPONENT_SIZE = 128;

    auto measure = [&dummy_accumulator](benchmark_detail::Meter &meter,
                                        ecs::World &world) {
        meter.measure([&world, &dummy_accumulator] {
            dummy_accumulator = 0;
            ecs::Query<benchmark::BenchmarkComponent<1, COMPONENT_SIZE>>().each(
                world,
                [&dummy_accumulator](
                    benchmark::BenchmarkComponent<1, COMPONENT_SIZE> &comp) {
                    dummy_accumulator += comp.data_[0];
                });
            return dummy_accumulator;
        });
    };

    // Test with 2 component types
    BENCHMARK("2 Component Types", {
        ecs::World world;
        benchmark::setup_world_with_component_types<2, COMPONENT_SIZE>(
            world, ENTITY_COUNT);

        measure(meter, world);
    });

    // Test with 4 component types
    BENCHMARK("4 Component Types", {
        ecs::World world;
        benchmark::setup_world_with_component_types<4, COMPONENT_SIZE>(
            world, ENTITY_COUNT);

        measure(meter, world);
    });

    // Test with 8 component types
    BENCHMARK("8 Component Types", {
        ecs::World world;
        benchmark::setup_world_with_component_types<8, COMPONENT_SIZE>(
            world, ENTITY_COUNT);

        measure(meter, world);
    });

    // Test with 16 component types
    BENCHMARK("16 Component Types", {
        ecs::World world;
        benchmark::setup_world_with_component_types<16, COMPONENT_SIZE>(
            world, ENTITY_COUNT);

        measure(meter, world);
    });

    // Test with 26 component types. The ECS enforces a hard, process-wide
    // MAX_COMPONENTS = 32 cap, and this binary also registers 6 more
    // distinct component types later (Position/Velocity/Renderable/Health/
    // Mass/Collider in the multi-archetype benchmarks below), so 26 is the
    // largest count reachable here without exceeding that budget.
    BENCHMARK("26 Component Types (process max)", {
        ecs::World world;
        benchmark::setup_world_with_component_types<26, COMPONENT_SIZE>(
            world, ENTITY_COUNT);

        measure(meter, world);
    });
}

void run_setup_performance_benchmarks()
{
    std::cout << "\n=== ECS Setup Performance Comparison ===\n" << std::flush;
    constexpr std::size_t ENTITY_COUNT = 1000;
    constexpr std::size_t COMPONENT_SIZE = 128;
    constexpr std::size_t COMPONENT_COUNT =
        8; // Smaller count for faster comparison

    // Realistic batch setup (all components added at once)
    BENCHMARK("Batch Setup: 8 Components", {
        meter.measure([] {
            ecs::World world;
            benchmark::setup_world_with_component_types<COMPONENT_COUNT,
                                                        COMPONENT_SIZE>(
                world, ENTITY_COUNT);
            return ENTITY_COUNT; // Just return something to prevent
                                 // optimization
        });
    });

    // Migration-heavy setup (components added one by one)
    BENCHMARK("Migration Setup: 8 Components", {
        meter.measure([] {
            ecs::World world;
            benchmark::setup_world_with_component_types_migrating<
                COMPONENT_COUNT, COMPONENT_SIZE>(world, ENTITY_COUNT);
            return ENTITY_COUNT; // Just return something to prevent
                                 // optimization
        });
    });
}

// Multi-archetype setup functions

// Scenario 1: Game Entities (70% Position+Velocity, 20% Position+Velocity+Renderable, 10% Position+Health)
void setup_game_entities_scenario(ecs::World &world, std::size_t total_entities)
{
    ecs::register_component<Position>();
    ecs::register_component<Velocity>();
    ecs::register_component<Renderable>();
    ecs::register_component<Health>();

    std::size_t moving_entities = static_cast<std::size_t>(total_entities * 0.70);
    std::size_t renderable_entities = static_cast<std::size_t>(total_entities * 0.20);
    std::size_t damageable_entities = total_entities - moving_entities - renderable_entities;

    // 70% Moving objects (Position + Velocity)
    for (std::size_t i = 0; i < moving_entities; ++i) {
        auto entity = world.create_entity();
        world.add_components(entity,
            Position::initialize_sequential(i),
            Velocity::initialize_sequential(i + 100)
        );
    }

    // 20% Renderable moving objects (Position + Velocity + Renderable)
    for (std::size_t i = 0; i < renderable_entities; ++i) {
        auto entity = world.create_entity();
        world.add_components(entity,
            Position::initialize_sequential(i + moving_entities),
            Velocity::initialize_sequential(i + moving_entities + 100),
            Renderable::initialize_sequential(i + moving_entities + 200)
        );
    }

    // 10% Damageable objects (Position + Health)
    for (std::size_t i = 0; i < damageable_entities; ++i) {
        auto entity = world.create_entity();
        world.add_components(entity,
            Position::initialize_sequential(i + moving_entities + renderable_entities),
            Health::initialize_sequential(i + moving_entities + renderable_entities + 300)
        );
    }
}

// Scenario 2: Simulation Entities (50% Position+Velocity, 30% Position+Velocity+Mass+Collider, 20% Position+Velocity+Mass+Collider+Health)
void setup_simulation_entities_scenario(ecs::World &world, std::size_t total_entities)
{
    ecs::register_component<Position>();
    ecs::register_component<Velocity>();
    ecs::register_component<Mass>();
    ecs::register_component<Collider>();
    ecs::register_component<Health>();

    std::size_t basic_particles = static_cast<std::size_t>(total_entities * 0.50);
    std::size_t physics_objects = static_cast<std::size_t>(total_entities * 0.30);
    std::size_t interactive_objects = total_entities - basic_particles - physics_objects;

    // 50% Basic particles (Position + Velocity)
    for (std::size_t i = 0; i < basic_particles; ++i) {
        auto entity = world.create_entity();
        world.add_components(entity,
            Position::initialize_sequential(i),
            Velocity::initialize_sequential(i + 100)
        );
    }

    // 30% Physics objects (Position + Velocity + Mass + Collider)
    for (std::size_t i = 0; i < physics_objects; ++i) {
        auto entity = world.create_entity();
        world.add_components(entity,
            Position::initialize_sequential(i + basic_particles),
            Velocity::initialize_sequential(i + basic_particles + 100),
            Mass::initialize_sequential(i + basic_particles + 200),
            Collider::initialize_sequential(i + basic_particles + 300)
        );
    }

    // 20% Interactive objects (Position + Velocity + Mass + Collider + Health)
    for (std::size_t i = 0; i < interactive_objects; ++i) {
        auto entity = world.create_entity();
        world.add_components(entity,
            Position::initialize_sequential(i + basic_particles + physics_objects),
            Velocity::initialize_sequential(i + basic_particles + physics_objects + 100),
            Mass::initialize_sequential(i + basic_particles + physics_objects + 200),
            Collider::initialize_sequential(i + basic_particles + physics_objects + 300),
            Health::initialize_sequential(i + basic_particles + physics_objects + 400)
        );
    }
}

// Scenario 3: Sparse Query Scenario (few entities match complex queries)
void setup_sparse_query_scenario(ecs::World &world, std::size_t total_entities)
{
    ecs::register_component<Position>();
    ecs::register_component<Velocity>();
    ecs::register_component<Mass>();
    ecs::register_component<Health>();

    // 80% entities have only Position
    std::size_t position_only = static_cast<std::size_t>(total_entities * 0.80);
    // 15% entities have Position + Velocity
    std::size_t position_velocity = static_cast<std::size_t>(total_entities * 0.15);
    // 5% entities have Position + Velocity + Mass + Health (target for sparse query)
    std::size_t full_entities = total_entities - position_only - position_velocity;

    // Position-only entities
    for (std::size_t i = 0; i < position_only; ++i) {
        auto entity = world.create_entity();
        world.add_components(entity, Position::initialize_sequential(i));
    }

    // Position + Velocity entities
    for (std::size_t i = 0; i < position_velocity; ++i) {
        auto entity = world.create_entity();
        world.add_components(entity,
            Position::initialize_sequential(i + position_only),
            Velocity::initialize_sequential(i + position_only + 100)
        );
    }

    // Full entities (Position + Velocity + Mass + Health)
    for (std::size_t i = 0; i < full_entities; ++i) {
        auto entity = world.create_entity();
        world.add_components(entity,
            Position::initialize_sequential(i + position_only + position_velocity),
            Velocity::initialize_sequential(i + position_only + position_velocity + 100),
            Mass::initialize_sequential(i + position_only + position_velocity + 200),
            Health::initialize_sequential(i + position_only + position_velocity + 300)
        );
    }
}

void run_multi_archetype_query_benchmarks()
{
    std::cout << "\n=== Multi-Archetype Query Performance ===\n" << std::flush;
    uint64_t dummy_accumulator = 0;
    constexpr std::size_t ENTITY_COUNT = 10000;

    // Game Entities Scenario - Query Position + Velocity (90% entities match)
    BENCHMARK("Game Entities: Position+Velocity Query (90% match)", {
        ecs::World world;
        setup_game_entities_scenario(world, ENTITY_COUNT);

        meter.measure([&world, &dummy_accumulator] {
            dummy_accumulator = 0;
            ecs::Query<Position, Velocity>().each(
                world, [&](Position &pos, Velocity &vel) {
                    dummy_accumulator += pos.data_[0] + vel.data_[0];
                });
            return dummy_accumulator;
        });
    });

    // Game Entities Scenario - Query Position only (100% entities match)
    BENCHMARK("Game Entities: Position Query (100% match)", {
        ecs::World world;
        setup_game_entities_scenario(world, ENTITY_COUNT);

        meter.measure([&world, &dummy_accumulator] {
            dummy_accumulator = 0;
            ecs::Query<Position>().each(
                world, [&](Position &pos) {
                    dummy_accumulator += pos.data_[0];
                });
            return dummy_accumulator;
        });
    });

    // Game Entities Scenario - Query Position + Health (10% entities match)
    BENCHMARK("Game Entities: Position+Health Query (10% match)", {
        ecs::World world;
        setup_game_entities_scenario(world, ENTITY_COUNT);

        meter.measure([&world, &dummy_accumulator] {
            dummy_accumulator = 0;
            ecs::Query<Position, Health>().each(
                world, [&](Position &pos, Health &health) {
                    dummy_accumulator += pos.data_[0] + health.data_[0];
                });
            return dummy_accumulator;
        });
    });

    // Simulation Entities Scenario - Query Position + Velocity (all match)
    BENCHMARK("Simulation: Position+Velocity Query (100% match)", {
        ecs::World world;
        setup_simulation_entities_scenario(world, ENTITY_COUNT);

        meter.measure([&world, &dummy_accumulator] {
            dummy_accumulator = 0;
            ecs::Query<Position, Velocity>().each(
                world, [&](Position &pos, Velocity &vel) {
                    dummy_accumulator += pos.data_[0] + vel.data_[0];
                });
            return dummy_accumulator;
        });
    });

    // Simulation Entities Scenario - Complex query (20% entities match)
    BENCHMARK("Simulation: Position+Velocity+Mass+Health Query (20% match)", {
        ecs::World world;
        setup_simulation_entities_scenario(world, ENTITY_COUNT);

        meter.measure([&world, &dummy_accumulator] {
            dummy_accumulator = 0;
            ecs::Query<Position, Velocity, Mass, Health>().each(
                world, [&](Position &pos, Velocity &vel,
                          Mass &mass, Health &health) {
                    dummy_accumulator += pos.data_[0] + vel.data_[0] + mass.data_[0] + health.data_[0];
                });
            return dummy_accumulator;
        });
    });

    // Sparse Query Scenario - Very selective query (5% entities match)
    BENCHMARK("Sparse: Position+Velocity+Mass+Health Query (5% match)", {
        ecs::World world;
        setup_sparse_query_scenario(world, ENTITY_COUNT);

        meter.measure([&world, &dummy_accumulator] {
            dummy_accumulator = 0;
            ecs::Query<Position, Velocity, Mass, Health>().each(
                world, [&](Position &pos, Velocity &vel,
                          Mass &mass, Health &health) {
                    dummy_accumulator += pos.data_[0] + vel.data_[0] + mass.data_[0] + health.data_[0];
                });
            return dummy_accumulator;
        });
    });

    // Sparse Query Scenario - Broad query (100% entities match)
    BENCHMARK("Sparse: Position Query (100% match)", {
        ecs::World world;
        setup_sparse_query_scenario(world, ENTITY_COUNT);

        meter.measure([&world, &dummy_accumulator] {
            dummy_accumulator = 0;
            ecs::Query<Position>().each(
                world, [&](Position &pos) {
                    dummy_accumulator += pos.data_[0];
                });
            return dummy_accumulator;
        });
    });
}

void run_archetype_vs_single_type_benchmarks()
{
    std::cout << "\n=== Archetype vs Single-Type Performance Comparison ===\n" << std::flush;
    uint64_t dummy_accumulator = 0;
    constexpr std::size_t ENTITY_COUNT = 10000;

    // Single archetype baseline (all entities have same components)
    BENCHMARK("Single Archetype: Position+Velocity Query", {
        ecs::World world;
        setup_world_two_components(world, ENTITY_COUNT);

        meter.measure([&world, &dummy_accumulator] {
            dummy_accumulator = 0;
            ecs::Query<ComponentA, ComponentB>().each(
                world, [&](ComponentA &a, ComponentB &b) {
                    dummy_accumulator += a.data_[0] + b.data_[0];
                });
            return dummy_accumulator;
        });
    });

    // Multi-archetype with same query selectivity
    BENCHMARK("Multi-Archetype: Position+Velocity Query (90% match)", {
        ecs::World world;
        setup_game_entities_scenario(world, ENTITY_COUNT);

        meter.measure([&world, &dummy_accumulator] {
            dummy_accumulator = 0;
            ecs::Query<Position, Velocity>().each(
                world, [&](Position &pos, Velocity &vel) {
                    dummy_accumulator += pos.data_[0] + vel.data_[0];
                });
            return dummy_accumulator;
        });
    });
}

int main()
{
    run_iteration_performance_benchmarks();
    run_component_type_scaling_benchmarks();
    run_setup_performance_benchmarks();
    run_multi_archetype_query_benchmarks();
    run_archetype_vs_single_type_benchmarks();

    return 0;
}
