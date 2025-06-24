#include "BenchmarkComponents.h"
#include <archon/ecs.h>
#include <chrono>
#include <iostream>

constexpr std::size_t ENTITY_COUNT = 10000;
constexpr std::size_t COMPONENT_DATA_SIZE = 128;
constexpr std::size_t ITERATIONS = 10; // Much fewer for profiling

using ComponentA = benchmark::BenchmarkComponent<1, COMPONENT_DATA_SIZE>;
using ComponentB = benchmark::BenchmarkComponent<2, COMPONENT_DATA_SIZE>;

void setup_world(ecs::World &world)
{
    ecs::ComponentRegistry::instance().register_component<ComponentA>();
    ecs::ComponentRegistry::instance().register_component<ComponentB>();

    for (std::size_t i = 0; i < ENTITY_COUNT; ++i) {
        auto e = world.create_entity();
        world.add_components(
            e,
            benchmark::initialize_sequential<1, COMPONENT_DATA_SIZE>(
                static_cast<uint8_t>(i)),
            benchmark::initialize_sequential<2, COMPONENT_DATA_SIZE>(
                static_cast<uint8_t>(i + ENTITY_COUNT)));
    }
}

void benchmark_ecs_two_components()
{
    std::cout << "Benchmarking ECS 2-component query...\n";

    ecs::World world;
    setup_world(world);

    volatile uint64_t dummy_accumulator = 0;

    auto start = std::chrono::high_resolution_clock::now();

    for (std::size_t iter = 0; iter < ITERATIONS; ++iter) {
        dummy_accumulator = 0;
        ecs::Query<ComponentA, ComponentB>().each(world, [&](ComponentA &c1,
                                                             ComponentB &c2) {
            dummy_accumulator +=
                benchmark::elementwise_addition<COMPONENT_DATA_SIZE>(c1, c2);
        });
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "ECS 2-component: " << duration.count() / ITERATIONS
              << "μs per iteration\n";
    std::cout << "Dummy result: " << dummy_accumulator << "\n";
}

void benchmark_baseline_soa()
{
    std::cout << "Benchmarking SoA baseline...\n";

    std::vector<ComponentA> comp_a_data;
    std::vector<ComponentB> comp_b_data;
    comp_a_data.reserve(ENTITY_COUNT);
    comp_b_data.reserve(ENTITY_COUNT);

    for (std::size_t i = 0; i < ENTITY_COUNT; ++i) {
        comp_a_data.push_back(
            benchmark::initialize_sequential<1, COMPONENT_DATA_SIZE>(
                static_cast<uint8_t>(i)));
        comp_b_data.push_back(
            benchmark::initialize_sequential<2, COMPONENT_DATA_SIZE>(
                static_cast<uint8_t>(i + ENTITY_COUNT)));
    }

    volatile uint64_t dummy_accumulator = 0;

    auto start = std::chrono::high_resolution_clock::now();

    for (std::size_t iter = 0; iter < ITERATIONS; ++iter) {
        dummy_accumulator = 0;
        for (std::size_t i = 0; i < ENTITY_COUNT; ++i) {
            dummy_accumulator +=
                benchmark::elementwise_addition<COMPONENT_DATA_SIZE>(
                    comp_a_data[i], comp_b_data[i]);
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "SoA baseline: " << duration.count() / ITERATIONS
              << "μs per iteration\n";
    std::cout << "Dummy result: " << dummy_accumulator << "\n";
}

void component_count_scaling()
{
    std::cout << "========Component Count Scaling=========\n";
    uint64_t dummy_accumulator = 0;
    constexpr std::size_t ENTITY_COUNT = 50000;
    constexpr std::size_t COMPONENT_SIZE = 128;

    ecs::World world;
    benchmark::setup_world_with_component_types<32, COMPONENT_SIZE>(
        world, ENTITY_COUNT);

    std::cout << "World Setup Complete\n";

    ecs::Query<benchmark::BenchmarkComponent<1, COMPONENT_SIZE>>().each(
        world, [&dummy_accumulator](
                   benchmark::BenchmarkComponent<1, COMPONENT_SIZE> &comp) {
            dummy_accumulator += comp.data_[0];
        });
    std::cout << dummy_accumulator;
}

int main()
{
    // std::cout << "=== Focused Performance Profile ===" << std::endl;
    // std::cout << "Entity count: " << ENTITY_COUNT << std::endl;
    // std::cout << "Component size: " << COMPONENT_DATA_SIZE << " bytes"
    //           << std::endl;
    // std::cout << "Iterations: " << ITERATIONS << std::endl << std::endl;

    // benchmark_ecs_two_components();
    // std::cout << std::endl;
    // benchmark_baseline_soa();

    component_count_scaling();

    return 0;
}