#include <catch2/benchmark/catch_chronometer.hpp>
#include <cstddef>
#include <cstdint>
#include <vector>
#define CATCH_CONFIG_ENABLE_BENCHMARKING // Enable Catch2 Benchmarking
#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>

#include "BenchmarkComponents.h"
#include <archon/ecs.h>

// --- Helper data and functions ---
constexpr std::size_t ENTITY_COUNT_FOR_BENCHMARK = 10000;
constexpr std::size_t COMPONENT_DATA_SIZE = 128;

// Create component type aliases using numeric IDs
using ComponentA = benchmark::BenchmarkComponent<1, COMPONENT_DATA_SIZE>;
using ComponentB = benchmark::BenchmarkComponent<2, COMPONENT_DATA_SIZE>;

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
            benchmark::initialize_sequential<1, COMPONENT_DATA_SIZE>(
                static_cast<uint8_t>(i)),
            benchmark::initialize_sequential<2, COMPONENT_DATA_SIZE>(
                static_cast<uint8_t>(i + entity_count)));
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
        data_vec[i] = {benchmark::initialize_sequential<1, COMPONENT_DATA_SIZE>(
                           static_cast<uint8_t>(i)),
                       benchmark::initialize_sequential<2, COMPONENT_DATA_SIZE>(
                           static_cast<uint8_t>(i + entity_count))};
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
            benchmark::initialize_sequential<1, COMPONENT_DATA_SIZE>(
                static_cast<uint8_t>(i)));
        data.comp_b_data.push_back(
            benchmark::initialize_sequential<2, COMPONENT_DATA_SIZE>(
                static_cast<uint8_t>(i + entity_count)));
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
            comp_a_data[i] =
                benchmark::initialize_sequential<1, COMPONENT_DATA_SIZE>(
                    static_cast<uint8_t>(i));
            comp_b_data[i] =
                benchmark::initialize_sequential<2, COMPONENT_DATA_SIZE>(
                    static_cast<uint8_t>(i + entity_count));
        }
    }

    ~RawArrayBenchmark()
    {
        delete[] comp_a_data;
        delete[] comp_b_data;
    }
};

TEST_CASE("ECS Iteration Performance", "[benchmark][ecs]")
{
    uint64_t dummy_accumulator = 0; // To prevent optimization

    // --- Benchmark for iterating two components with the ECS API ---
    BENCHMARK_ADVANCED("ECS Query: Iterate 2 Components")
    (Catch::Benchmark::Chronometer meter)
    {
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
    };

    // --- Benchmark for AoS (Array of Structs) baseline ---
    BENCHMARK_ADVANCED("Baseline: AoS std::vector<struct>")
    (Catch::Benchmark::Chronometer meter)
    {
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
    };

    // --- Benchmark for true SoA (Structure of Arrays) baseline ---
    BENCHMARK_ADVANCED("Baseline: SoA separate std::vectors")
    (Catch::Benchmark::Chronometer meter)
    {
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
    };

    // --- Benchmark for C-style raw arrays baseline ---
    BENCHMARK_ADVANCED("Baseline: Raw C arrays")
    (Catch::Benchmark::Chronometer meter)
    {
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
    };

    // --- Benchmark for iterating one component with the ECS API ---
    BENCHMARK_ADVANCED("ECS Query: Iterate 1 Component")
    (Catch::Benchmark::Chronometer meter)
    {
        ecs::World world;
        ecs::register_component<ComponentA>();
        for (std::size_t i = 0; i < ENTITY_COUNT_FOR_BENCHMARK; ++i) {
            auto e = world.create_entity();
            world.add_components(
                e, benchmark::initialize_sequential<1, COMPONENT_DATA_SIZE>(
                       static_cast<uint8_t>(i)));
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
    };
}

TEST_CASE("ECS Component Type Scaling", "[benchmark][ecs][scaling]")
{
    uint64_t dummy_accumulator = 0;
    constexpr std::size_t ENTITY_COUNT = 5000;
    constexpr std::size_t COMPONENT_SIZE = 128;

    auto measure = [&dummy_accumulator,
                    &COMPONENT_SIZE](Catch::Benchmark::Chronometer &meter,
                                     ecs::World &world) {
        meter.measure([&world, &dummy_accumulator, &COMPONENT_SIZE] {
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
    BENCHMARK_ADVANCED("2 Component Types")(Catch::Benchmark::Chronometer meter)
    {
        ecs::World world;
        benchmark::setup_world_with_component_types<2, COMPONENT_SIZE>(
            world, ENTITY_COUNT);

        measure(meter, world);
    };

    // Test with 4 component types
    BENCHMARK_ADVANCED("4 Component Types")(Catch::Benchmark::Chronometer meter)
    {
        ecs::World world;
        benchmark::setup_world_with_component_types<4, COMPONENT_SIZE>(
            world, ENTITY_COUNT);

        measure(meter, world);
    };

    // Test with 8 component types
    BENCHMARK_ADVANCED("8 Component Types")(Catch::Benchmark::Chronometer meter)
    {
        ecs::World world;
        benchmark::setup_world_with_component_types<8, COMPONENT_SIZE>(
            world, ENTITY_COUNT);

        measure(meter, world);
    };

    // Test with 16 component types
    BENCHMARK_ADVANCED("16 Component Types")
    (Catch::Benchmark::Chronometer meter)
    {
        ecs::World world;
        benchmark::setup_world_with_component_types<16, COMPONENT_SIZE>(
            world, ENTITY_COUNT);

        measure(meter, world);
    };

    // Test with 32 component types (maximum)
    BENCHMARK_ADVANCED("32 Component Types")
    (Catch::Benchmark::Chronometer meter)
    {
        ecs::World world;
        benchmark::setup_world_with_component_types<32, COMPONENT_SIZE>(
            world, ENTITY_COUNT);

        measure(meter, world);
    };
}

TEST_CASE("ECS Setup Performance Comparison", "[benchmark][ecs][setup]")
{
    constexpr std::size_t ENTITY_COUNT = 1000;
    constexpr std::size_t COMPONENT_SIZE = 128;
    constexpr std::size_t COMPONENT_COUNT =
        8; // Smaller count for faster comparison

    // Realistic batch setup (all components added at once)
    BENCHMARK_ADVANCED("Batch Setup: 8 Components")
    (Catch::Benchmark::Chronometer meter)
    {
        meter.measure([] {
            ecs::World world;
            benchmark::setup_world_with_component_types<COMPONENT_COUNT,
                                                        COMPONENT_SIZE>(
                world, ENTITY_COUNT);
            return ENTITY_COUNT; // Just return something to prevent
                                 // optimization
        });
    };

    // Migration-heavy setup (components added one by one)
    BENCHMARK_ADVANCED("Migration Setup: 8 Components")
    (Catch::Benchmark::Chronometer meter)
    {
        meter.measure([] {
            ecs::World world;
            benchmark::setup_world_with_component_types_migrating<
                COMPONENT_COUNT, COMPONENT_SIZE>(world, ENTITY_COUNT);
            return ENTITY_COUNT; // Just return something to prevent
                                 // optimization
        });
    };
}