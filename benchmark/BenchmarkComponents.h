#pragma once

#include <algorithm>
#include <archon/ecs.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <random>

namespace benchmark
{
// Tag-based component system for flexible benchmarking
template <std::size_t id, std::size_t size> struct BenchmarkComponent {
    std::array<uint8_t, size> data_;
    
    // Initialize with sequential values starting from 'start_value'
    static BenchmarkComponent initialize_sequential(std::size_t start_value) {
        BenchmarkComponent comp;
        std::iota(comp.data_.begin(), comp.data_.end(), start_value);
        return comp;
    }
    
    // Initialize with random values between 'min_value' and 'max_value'
    static BenchmarkComponent initialize_random(uint8_t min_value = 0, uint8_t max_value = 255) {
        BenchmarkComponent comp;
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<uint8_t> dis(min_value, max_value);
        std::generate(comp.data_.begin(), comp.data_.end(),
                      [&]() { return dis(gen); });
        return comp;
    }
};

// Legacy standalone functions - use static methods instead
template <std::size_t id, std::size_t size>
BenchmarkComponent<id, size> initialize_sequential(std::size_t start_value)
{
    return BenchmarkComponent<id, size>::initialize_sequential(start_value);
}

template <std::size_t id, std::size_t size>
BenchmarkComponent<id, size> initialize_random(uint8_t min_value = 0,
                                               uint8_t max_value = 255)
{
    return BenchmarkComponent<id, size>::initialize_random(min_value, max_value);
}

template <std::size_t Count, std::size_t size> void register_components()
{
    if constexpr (Count == 0) {
        return;
    } else {
        ecs::register_component<BenchmarkComponent<Count, size>>();
        register_components<Count - 1, size>();
    }
}

template <std::size_t id, std::size_t size>
uint64_t sum(const BenchmarkComponent<id, size> &comp)
{
    return std::accumulate(comp.data_.cbegin(), comp.data_.cend(), 0ULL);
}

template <std::size_t id, std::size_t size>
void computation_sort(BenchmarkComponent<id, size> &comp)
{
    std::sort(comp.data_.begin(), comp.data_.end());
}

// Linear computation: element-wise addition
template <std::size_t size, typename... Components>
uint64_t elementwise_addition(const Components &...components)
{
    uint64_t result = 0;
    for (std::size_t i = 0; i < size; ++i) {
        // Use fold expression to add corresponding elements from the rest
        ((result += components.data_[i]), ...);
    }
    return result;
}

// Recursive case: multiplies elements across components
template <std::size_t id, std::size_t size, typename... Components>
uint64_t quadratic_computation(const BenchmarkComponent<id, size> &first,
                               const Components &...rest)
{
    uint64_t result = 0;
    for (const auto &val : first.data_) {
        result += val * quadratic_computation(rest...);
    }
    return result;
}

// Helper to add multiple component types to an entity (OLD - creates
// migrations)
template <std::size_t Count, std::size_t size>
void add_benchmark_components_to_entity_migrating(ecs::World &world,
                                                  ecs::EntityId entity,
                                                  std::size_t base_value)
{
    if constexpr (Count == 0) {
        return;
    } else {
        world.add_components(
            entity, initialize_sequential<Count, size>(base_value + Count));
        add_benchmark_components_to_entity_migrating<Count - 1, size>(
            world, entity, base_value);
    }
}

// Helper to add all components at once (realistic usage pattern)
template <std::size_t Count, std::size_t size>
auto make_all_components(std::size_t base_value)
{
    if constexpr (Count == 0) {
        return std::tuple<>{};
    } else if constexpr (Count == 1) {
        return std::make_tuple(initialize_sequential<1, size>(base_value + 1));
    } else {
        auto rest = make_all_components<Count - 1, size>(base_value);
        auto current = initialize_sequential<Count, size>(base_value + Count);
        return std::tuple_cat(std::make_tuple(current), rest);
    }
}

template <std::size_t Count, std::size_t size>
void add_benchmark_components_to_entity(ecs::World &world, ecs::EntityId entity,
                                        std::size_t base_value)
{
    auto components = make_all_components<Count, size>(base_value);
    std::apply(
        [&world, entity](auto &&...comps) {
            world.add_components(entity,
                                 std::forward<decltype(comps)>(comps)...);
        },
        components);
}

// Helper to setup world with entities having multiple component types
template <std::size_t ComponentCount, std::size_t ComponentSize>
void setup_world_with_component_types(ecs::World &world,
                                      std::size_t entity_count)
{
    register_components<ComponentCount, ComponentSize>();

    for (std::size_t i = 0; i < entity_count; ++i) {
        auto entity = world.create_entity();
        add_benchmark_components_to_entity<ComponentCount, ComponentSize>(
            world, entity, i * ComponentCount);
    }
}

// Helper to setup world with migrating entities (for comparison)
template <std::size_t ComponentCount, std::size_t ComponentSize>
void setup_world_with_component_types_migrating(ecs::World &world,
                                                std::size_t entity_count)
{
    register_components<ComponentCount, ComponentSize>();

    for (std::size_t i = 0; i < entity_count; ++i) {
        auto entity = world.create_entity();
        add_benchmark_components_to_entity_migrating<ComponentCount,
                                                     ComponentSize>(
            world, entity, i * ComponentCount);
    }
}

} // namespace benchmark