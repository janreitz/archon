#include "BenchmarkComponents.h"
#include <archon/ecs.h>
#include <chrono>
#include <iostream>

constexpr std::size_t ENTITY_COUNT = 10000;
constexpr std::size_t COMPONENT_DATA_SIZE = 128;
constexpr std::size_t ITERATIONS = 10; // Reduced for cleaner timing

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

void test_component_access_patterns()
{
    std::cout << "\n=== Component Access Pattern Analysis ===" << std::endl;

    ecs::World world;
    setup_world(world);

    volatile uint64_t dummy = 0;

    // Test 1: ECS Query system access
    ecs::Query<ComponentA, ComponentB> query;
    auto start = std::chrono::high_resolution_clock::now();

    for (std::size_t iter = 0; iter < ITERATIONS; ++iter) {
        query.each(world, [&](ComponentA &comp_a, ComponentB &comp_b) {
            dummy += benchmark::elementwise_addition<COMPONENT_DATA_SIZE>(
                comp_a, comp_b);
        });
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto query_duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Test 2: Raw SoA baseline for comparison
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

    start = std::chrono::high_resolution_clock::now();

    for (std::size_t iter = 0; iter < ITERATIONS; ++iter) {
        for (std::size_t i = 0; i < ENTITY_COUNT; ++i) {
            dummy += benchmark::elementwise_addition<COMPONENT_DATA_SIZE>(
                comp_a_data[i], comp_b_data[i]);
        }
    }

    end = std::chrono::high_resolution_clock::now();
    auto baseline_duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Test 3: AoS baseline
    struct AoSData {
        ComponentA comp_a;
        ComponentB comp_b;
    };
    std::vector<AoSData> aos_data;
    aos_data.reserve(ENTITY_COUNT);

    for (std::size_t i = 0; i < ENTITY_COUNT; ++i) {
        aos_data.push_back(
            {benchmark::initialize_sequential<1, COMPONENT_DATA_SIZE>(
                 static_cast<uint8_t>(i)),
             benchmark::initialize_sequential<2, COMPONENT_DATA_SIZE>(
                 static_cast<uint8_t>(i + ENTITY_COUNT))});
    }

    start = std::chrono::high_resolution_clock::now();

    for (std::size_t iter = 0; iter < ITERATIONS; ++iter) {
        for (std::size_t i = 0; i < ENTITY_COUNT; ++i) {
            dummy += benchmark::elementwise_addition<COMPONENT_DATA_SIZE>(
                aos_data[i].comp_a, aos_data[i].comp_b);
        }
    }

    end = std::chrono::high_resolution_clock::now();
    auto aos_duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "ECS Query system:     " << query_duration.count() / ITERATIONS
              << "μs per iteration" << std::endl;
    std::cout << "SoA baseline:         "
              << baseline_duration.count() / ITERATIONS << "μs per iteration"
              << std::endl;
    std::cout << "AoS baseline:         " << aos_duration.count() / ITERATIONS
              << "μs per iteration" << std::endl;
    std::cout << "\nOverhead ratios:" << std::endl;
    std::cout << "  ECS vs SoA:   "
              << (double)query_duration.count() / baseline_duration.count()
              << "x" << std::endl;
    std::cout << "  ECS vs AoS:   "
              << (double)query_duration.count() / aos_duration.count() << "x"
              << std::endl;
    std::cout << "  SoA vs AoS:   "
              << (double)baseline_duration.count() / aos_duration.count() << "x"
              << std::endl;

    std::cout << "Dummy result: " << dummy << std::endl;
}

void test_memory_access_patterns()
{
    std::cout << "\n=== Memory Access Pattern Analysis ===" << std::endl;

    ecs::World world;
    setup_world(world);

    ecs::Query<ComponentA, ComponentB> query;
    volatile uint64_t dummy = 0;

    // Test 1: ECS with minimal computation (just first byte)
    auto start = std::chrono::high_resolution_clock::now();

    for (std::size_t iter = 0; iter < ITERATIONS; ++iter) {
        query.each(world, [&](ComponentA &comp_a, ComponentB &comp_b) {
            dummy += comp_a.data_[0] + comp_b.data_[0]; // Minimal access
        });
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto minimal_duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Test 2: ECS with full computation
    start = std::chrono::high_resolution_clock::now();

    for (std::size_t iter = 0; iter < ITERATIONS; ++iter) {
        query.each(world, [&](ComponentA &comp_a, ComponentB &comp_b) {
            dummy += benchmark::elementwise_addition<COMPONENT_DATA_SIZE>(
                comp_a, comp_b);
        });
    }

    end = std::chrono::high_resolution_clock::now();
    auto full_duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Test 3: SoA with minimal computation
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

    start = std::chrono::high_resolution_clock::now();

    for (std::size_t iter = 0; iter < ITERATIONS; ++iter) {
        for (std::size_t i = 0; i < ENTITY_COUNT; ++i) {
            dummy += comp_a_data[i].data_[0] +
                     comp_b_data[i].data_[0]; // Minimal access
        }
    }

    end = std::chrono::high_resolution_clock::now();
    auto soa_minimal_duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "ECS minimal access:       "
              << minimal_duration.count() / ITERATIONS << "μs per iteration"
              << std::endl;
    std::cout << "ECS full computation:     "
              << full_duration.count() / ITERATIONS << "μs per iteration"
              << std::endl;
    std::cout << "SoA minimal access:       "
              << soa_minimal_duration.count() / ITERATIONS << "μs per iteration"
              << std::endl;
    std::cout << "\nAccess overhead analysis:" << std::endl;
    std::cout << "  ECS access overhead:    "
              << (double)minimal_duration.count() / soa_minimal_duration.count()
              << "x" << std::endl;
    std::cout << "  Computation difference: "
              << (full_duration.count() - minimal_duration.count()) / ITERATIONS
              << "μs vs baseline computation" << std::endl;

    std::cout << "Dummy result: " << dummy << std::endl;
}

void test_query_construction_overhead()
{
    std::cout << "\n=== Query Construction Overhead ===" << std::endl;

    // Register components before testing query construction
    ecs::ComponentRegistry::instance().register_component<ComponentA>();
    ecs::ComponentRegistry::instance().register_component<ComponentB>();

    constexpr size_t MANY_ITERATIONS = 1000;
    auto start = std::chrono::high_resolution_clock::now();

    for (std::size_t i = 0; i < MANY_ITERATIONS; ++i) {
        ecs::Query<ComponentA, ComponentB>
            query;                   // This calls get_meta_id() twice
        volatile auto *ptr = &query; // Prevent optimization
        (void)ptr;
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "Query construction: " << duration.count() / MANY_ITERATIONS
              << "μs per query (" << MANY_ITERATIONS << " queries)"
              << std::endl;
}

int main()
{
    std::cout << "=== ECS Overhead Analysis ===" << std::endl;
    std::cout << "Entity count: " << ENTITY_COUNT << std::endl;
    std::cout << "Component size: " << COMPONENT_DATA_SIZE << " bytes"
              << std::endl;
    std::cout << "Iterations: " << ITERATIONS << std::endl;

    test_query_construction_overhead();
    test_component_access_patterns();
    test_memory_access_patterns();

    return 0;
}