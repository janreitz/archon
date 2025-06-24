#include <archon/ecs.h>
#include <catch2/catch_test_macros.hpp>
// Compile-time concept validation tests

namespace ecs
{
namespace concept_tests
{

// Mock types for testing
struct Position {
    float x, y;
};
struct Velocity {
    float dx, dy;
};
struct Health {
    int value;
};

// Declare actual functions to get their types
void mutable_func(Position &, Velocity &) {}
void const_func(const Position &, const Velocity &) {}
void value_func(Position, Velocity) {}

// Helper function types for testing
using SinglePositionFunc = void(Position &);
using SingleConstPositionFunc = void(const Position &);
using SinglePositionValueFunc = void(Position); // Pass by value
using MultiMutableFunc = void(Position &, Velocity &);
using MultiConstFunc = void(const Position &, const Velocity &);
using MixedFunc = void(Position &, const Velocity &);
using MixedValueFunc = void(Position, const Velocity &); // Mixed value/ref
using EmptyFunc = void();
using RValueRefFunc = void(Position &&);
using ConstRValueRefFunc = void(const Position &&);

// -----------------------------------------------------------------------------
// WorldType Concept Tests
// -----------------------------------------------------------------------------
void test_world_type_concept()
{
    // ✅ Should work - basic World types
    static_assert(WorldType<World>);
    static_assert(WorldType<World &>);
    static_assert(WorldType<World &&>);
    static_assert(WorldType<const World>);
    static_assert(WorldType<const World &>);
    static_assert(WorldType<const World &&>);
    static_assert(WorldType<volatile World &>);
    static_assert(WorldType<const volatile World &>);

    // ❌ Should fail - wrong types
    static_assert(!WorldType<int>);
    static_assert(!WorldType<Position>);
    static_assert(!WorldType<Position &>);
    static_assert(!WorldType<const Position &>);
    static_assert(!WorldType<void>);
    static_assert(!WorldType<World *>); // Pointer, not reference
}

// -----------------------------------------------------------------------------
// ConstCompatible Concept Tests
// -----------------------------------------------------------------------------
void test_const_compatible_concept()
{
    // ✅ Mutable world with any component type should work
    static_assert(ConstCompatible<World, Position>);
    static_assert(ConstCompatible<World, Position &>);
    static_assert(ConstCompatible<World, Position &&>);
    static_assert(ConstCompatible<World, const Position>);
    static_assert(ConstCompatible<World, const Position &>);
    static_assert(ConstCompatible<World, const Position &&>);
    static_assert(ConstCompatible<World &, Position &>);
    static_assert(ConstCompatible<World &, const Position &>);
    static_assert(ConstCompatible<World &&, Position &>);
    static_assert(ConstCompatible<World &&, const Position &>);

    // ✅ Const world with const component types or pass-by-value should work
    static_assert(ConstCompatible<const World, Position>);
    static_assert(ConstCompatible<const World, const Position>);
    static_assert(ConstCompatible<const World, const Position &>);
    static_assert(ConstCompatible<const World, const Position &&>);
    static_assert(ConstCompatible<const World &, Position>);
    static_assert(ConstCompatible<const World &, const Position>);
    static_assert(ConstCompatible<const World &, const Position &>);
    static_assert(ConstCompatible<const World &, const Position &&>);
    static_assert(ConstCompatible<const World &&, Position>);
    static_assert(ConstCompatible<const World &&, const Position &>);

    // ❌ Const world with mutable component types should fail
    static_assert(!ConstCompatible<const World, Position &>);
    static_assert(!ConstCompatible<const World, Position &&>);
    static_assert(!ConstCompatible<const World &, Position &>);
    static_assert(!ConstCompatible<const World &, Position &&>);
    static_assert(!ConstCompatible<const World &&, Position &>);
    static_assert(!ConstCompatible<const World &&, Position &&>);
}

// -----------------------------------------------------------------------------
// ArgsConstCompatible Concept Tests (Updated)
// -----------------------------------------------------------------------------
void test_function_const_compatible_concept()
{
    // ✅ Mutable world with any component combinations
    static_assert(ArgsConstCompatible<World, SinglePositionFunc>);
    static_assert(ArgsConstCompatible<World, SingleConstPositionFunc>);
    static_assert(ArgsConstCompatible<World, SinglePositionValueFunc>);
    static_assert(ArgsConstCompatible<World &, SinglePositionFunc>);
    static_assert(ArgsConstCompatible<World &, SingleConstPositionFunc>);
    static_assert(ArgsConstCompatible<World &&, SinglePositionFunc>);
    static_assert(ArgsConstCompatible<World &&, SingleConstPositionFunc>);

    // ✅ Const world with const components only
    static_assert(ArgsConstCompatible<const World, SingleConstPositionFunc>);
    static_assert(ArgsConstCompatible<const World &, SingleConstPositionFunc>);
    static_assert(ArgsConstCompatible<const World &&, SingleConstPositionFunc>);

    // ✅ Const world with pass-by-value (should work now!)
    static_assert(ArgsConstCompatible<const World, SinglePositionValueFunc>);
    static_assert(ArgsConstCompatible<const World &, SinglePositionValueFunc>);

    // ❌ Const world with mutable reference components
    static_assert(!ArgsConstCompatible<const World, SinglePositionFunc>);
    static_assert(!ArgsConstCompatible<const World &, SinglePositionFunc>);
    static_assert(!ArgsConstCompatible<const World &&, SinglePositionFunc>);

    // Test with multiple component functions

    // ✅ Mutable world with mixed const/mutable components
    static_assert(ArgsConstCompatible<World, MultiMutableFunc>);
    static_assert(ArgsConstCompatible<World, MultiConstFunc>);
    static_assert(ArgsConstCompatible<World, MixedFunc>);
    static_assert(ArgsConstCompatible<World, MixedValueFunc>);

    // ✅ Const world with all const components
    static_assert(ArgsConstCompatible<const World, MultiConstFunc>);

    // ✅ Const world with mixed value/const ref (should work now!)
    static_assert(ArgsConstCompatible<const World, MixedValueFunc>);

    // ❌ Const world with ANY mutable reference components should fail
    static_assert(!ArgsConstCompatible<const World, MultiMutableFunc>);
    static_assert(
        !ArgsConstCompatible<const World, MixedFunc>); // Position& is mutable

    // Test with empty function
    static_assert(ArgsConstCompatible<World, EmptyFunc>);
    static_assert(ArgsConstCompatible<const World, EmptyFunc>);
}

void test_function_types()
{
    using MutableFunc = void(Position &, Velocity &);
    using ConstFunc = void(const Position &, const Velocity &);
    using ValueFunc = void(Position, Velocity);
    using MixedFunc = void(Position &, const Velocity &);
    using MixedValueFunc = void(Position, const Velocity &);
    using EmptyFunc = void();

    static_assert(ArgsConstCompatible<World, MutableFunc>);
    static_assert(ArgsConstCompatible<World, ConstFunc>);
    static_assert(ArgsConstCompatible<World, ValueFunc>);
    static_assert(ArgsConstCompatible<World, MixedFunc>);
    static_assert(ArgsConstCompatible<World, MixedValueFunc>);
    static_assert(ArgsConstCompatible<World, EmptyFunc>);

    // Test const world
    static_assert(ArgsConstCompatible<const World, ConstFunc>);
    static_assert(ArgsConstCompatible<const World, ValueFunc>);
    static_assert(ArgsConstCompatible<const World, MixedValueFunc>);
    static_assert(ArgsConstCompatible<const World, EmptyFunc>);

    // These should fail with const world
    static_assert(!ArgsConstCompatible<const World, MutableFunc>);
    static_assert(!ArgsConstCompatible<const World, MixedFunc>);
}

// Test with lambda functions (more realistic)
void test_with_lambda_functions()
{
    // ✅ Mutable world lambdas
    auto mutable_lambda = [](Position& /*pos*/, Velocity& /*vel*/) {};
    auto const_lambda = [](const Position& /*pos*/, const Velocity& /*vel*/) {};
    auto mixed_lambda = [](Position& /*pos*/, const Velocity& /*vel*/) {};
    auto value_lambda = [](Position /*pos*/, Velocity /*vel*/) {};
    auto mixed_value_lambda = [](Position /*pos*/, const Velocity& /*vel*/) {};

    static_assert(ArgsConstCompatible<World, decltype(mutable_lambda)>);
    static_assert(ArgsConstCompatible<World, decltype(const_lambda)>);
    static_assert(ArgsConstCompatible<World, decltype(mixed_lambda)>);
    static_assert(ArgsConstCompatible<World, decltype(value_lambda)>);
    static_assert(ArgsConstCompatible<World, decltype(mixed_value_lambda)>);

    // ✅ Const world with appropriate lambdas
    static_assert(ArgsConstCompatible<const World, decltype(const_lambda)>);
    static_assert(ArgsConstCompatible<const World, decltype(value_lambda)>);
    static_assert(
        ArgsConstCompatible<const World, decltype(mixed_value_lambda)>);

    // ❌ Const world with mutable reference lambdas
    static_assert(!ArgsConstCompatible<const World, decltype(mutable_lambda)>);
    static_assert(!ArgsConstCompatible<const World, decltype(mixed_lambda)>);
}

void test_function_pointers()
{

    // Test with function pointer types (remove the dereference)
    static_assert(ArgsConstCompatible<World, decltype(mutable_func)>);
    static_assert(ArgsConstCompatible<World, decltype(const_func)>);
    static_assert(ArgsConstCompatible<World, decltype(value_func)>);

    static_assert(ArgsConstCompatible<const World, decltype(const_func)>);
    static_assert(ArgsConstCompatible<const World, decltype(value_func)>);
    static_assert(!ArgsConstCompatible<const World, decltype(mutable_func)>);

    // Test with actual function pointers
    void (*mutable_func_ptr)(Position &, Velocity &) = mutable_func;
    void (*const_func_ptr)(const Position &, const Velocity &) = const_func;
    void (*value_func_ptr)(Position, Velocity) = value_func;

    static_assert(ArgsConstCompatible<World, decltype(mutable_func_ptr)>);
    static_assert(ArgsConstCompatible<World, decltype(const_func_ptr)>);
    static_assert(ArgsConstCompatible<World, decltype(value_func_ptr)>);

    static_assert(ArgsConstCompatible<const World, decltype(const_func_ptr)>);
    static_assert(ArgsConstCompatible<const World, decltype(value_func_ptr)>);
    static_assert(
        !ArgsConstCompatible<const World, decltype(mutable_func_ptr)>);
}

// Edge Cases and Special Scenarios
void test_edge_cases()
{
    // Test with cv-qualified worlds

    // ✅ Volatile worlds (should work like non-const)
    static_assert(ArgsConstCompatible<volatile World, SinglePositionFunc>);
    static_assert(ArgsConstCompatible<volatile World &, MultiMutableFunc>);

    // ❌ Const volatile worlds (should behave like const)
    static_assert(
        !ArgsConstCompatible<const volatile World, SinglePositionFunc>);
    static_assert(
        ArgsConstCompatible<const volatile World, SingleConstPositionFunc>);

    // Test with rvalue reference components
    static_assert(ArgsConstCompatible<World, RValueRefFunc>);
    static_assert(ArgsConstCompatible<World, ConstRValueRefFunc>);
    static_assert(ArgsConstCompatible<const World, ConstRValueRefFunc>);
    static_assert(!ArgsConstCompatible<const World, RValueRefFunc>);
}

// Integration Tests - Realistic ECS Usage Patterns
void test_realistic_ecs_patterns()
{
    // Movement system (mutable world, mutable + const components)
    auto movement_system = [](Position &pos, const Velocity &vel) {
        pos.x += vel.dx;
    };
    static_assert(ArgsConstCompatible<World, decltype(movement_system)>);
    static_assert(ArgsConstCompatible<World &, decltype(movement_system)>);
    static_assert(
        !ArgsConstCompatible<
            const World, decltype(movement_system)>); // Position& not allowed

    // Rendering system (const world, const components)
    auto rendering_system = [](const Position& /*pos*/, const Health& /*health*/) {
        // Read-only rendering
    };
    static_assert(ArgsConstCompatible<World, decltype(rendering_system)>);
    static_assert(ArgsConstCompatible<const World, decltype(rendering_system)>);
    static_assert(
        ArgsConstCompatible<const World &, decltype(rendering_system)>);

    // Analytics system (const world, pass-by-value)
    auto analytics_system = [](Position /*pos*/, Velocity /*vel*/) {
        // Process copies for analytics
    };
    static_assert(ArgsConstCompatible<World, decltype(analytics_system)>);
    static_assert(
        ArgsConstCompatible<const World,
                            decltype(analytics_system)>); // Should work!

    // Mixed system (const world, mixed value/const ref)
    auto mixed_system = [](Position /*pos*/, const Velocity& /*vel*/) {
        // Copy position, reference velocity
    };
    static_assert(ArgsConstCompatible<World, decltype(mixed_system)>);
    static_assert(ArgsConstCompatible<const World,
                                      decltype(mixed_system)>); // Should work!

    // Debug logging (const world, all const)
    auto logging_system = [](const Position& /*pos*/, const Velocity& /*vel*/,
                             const Health& /*health*/) {
        // Log component values
    };
    static_assert(ArgsConstCompatible<World, decltype(logging_system)>);
    static_assert(ArgsConstCompatible<const World, decltype(logging_system)>);
}

// Performance Tests - Ensure Concepts Don't Add Runtime Overhead
void test_compile_time_only()
{
    auto test_func = [](Position& /*pos*/) {};

    constexpr bool test1 = WorldType<World>;
    constexpr bool test2 = ConstCompatible<World, Position &>;
    constexpr bool test3 = ArgsConstCompatible<World, decltype(test_func)>;

    // If these compile as constexpr, the concepts are purely compile-time
    static_assert(test1);
    static_assert(test2);
    static_assert(test3);
}
} // namespace concept_tests
} // namespace ecs

// Wrap concept tests in Catch2 test cases for CMake integration
TEST_CASE("ECS Concept Validation", "[concepts]")
{
    SECTION("WorldType Concept")
    {
        // The real validation happens at compile-time via static_assert
        // This test just ensures the function compiles and can be called
        ecs::concept_tests::test_world_type_concept();
        REQUIRE(true); // If we get here, all static_asserts passed
    }
    
    SECTION("ConstCompatible Concept")
    {
        ecs::concept_tests::test_const_compatible_concept();
        REQUIRE(true);
    }
    
    SECTION("ArgsConstCompatible Concept")
    {
        ecs::concept_tests::test_function_const_compatible_concept();
        REQUIRE(true);
    }
    
    SECTION("Function Types")
    {
        ecs::concept_tests::test_function_types();
        REQUIRE(true);
    }
    
    SECTION("Lambda Functions")
    {
        ecs::concept_tests::test_with_lambda_functions();
        REQUIRE(true);
    }
    
    SECTION("Function Pointers")
    {
        ecs::concept_tests::test_function_pointers();
        REQUIRE(true);
    }
    
    SECTION("Edge Cases")
    {
        ecs::concept_tests::test_edge_cases();
        REQUIRE(true);
    }
    
    SECTION("Realistic ECS Patterns")
    {
        ecs::concept_tests::test_realistic_ecs_patterns();
        REQUIRE(true);
    }
    
    SECTION("Compile-time Only Validation")
    {
        ecs::concept_tests::test_compile_time_only();
        REQUIRE(true);
    }
}