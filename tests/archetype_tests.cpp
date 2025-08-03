#include <archon/ecs.h>
#include <catch2/catch_test_macros.hpp>

struct TestComponent1 {
    int value;
    TestComponent1() = default;
    explicit TestComponent1(int v) : value(v) {}
};

struct TestComponent2 {
    float data;
    TestComponent2() = default;
    explicit TestComponent2(float d) : data(d) {}
};

struct TestComponent3 {
    std::string name;
    TestComponent3() = default;
    explicit TestComponent3(std::string n) : name(std::move(n)) {}
};

TEST_CASE("Archetype copy operations", "[archetype]")
{
    ecs::register_component<TestComponent1>();
    ecs::register_component<TestComponent2>();
    ecs::register_component<TestComponent3>();

    SECTION("Copy constructor with single component type")
    {
        // Create archetype with TestComponent1
        auto mask = ecs::detail::get_component_mask<TestComponent1>();
        ecs::detail::Archetype original(mask);
        
        // Add some entities and components
        original.add_entity(1);
        original.add_entity(2);
        
        auto comp_id = ecs::detail::ComponentRegistry::instance().get_component_type_id<TestComponent1>();
        TestComponent1 tc1(10), tc2(20);
        original.components.at(comp_id).push(&tc1);
        original.components.at(comp_id).push(&tc2);
        
        REQUIRE(original.entity_count() == 2);
        REQUIRE(original.get_component<TestComponent1>(size_t(0)).value == 10);
        REQUIRE(original.get_component<TestComponent1>(size_t(1)).value == 20);

        // Copy construct
        auto copy = original;
        
        // Verify copy has same structure
        REQUIRE(copy.entity_count() == 2);
        REQUIRE(copy.mask_ == original.mask_);
        REQUIRE(copy.get_entity(0) == 1);
        REQUIRE(copy.get_entity(1) == 2);
        
        // Verify component data was copied
        REQUIRE(copy.get_component<TestComponent1>(size_t(0)).value == 10);
        REQUIRE(copy.get_component<TestComponent1>(size_t(1)).value == 20);
        
        // Verify independence (deep copy)
        original.get_component<TestComponent1>(size_t(0)).value = 999;
        REQUIRE(copy.get_component<TestComponent1>(size_t(0)).value == 10); // Should be unchanged
    }

    SECTION("Copy constructor with multiple component types")
    {
        // Create archetype with TestComponent1 and TestComponent2
        auto mask = ecs::detail::get_component_mask<TestComponent1, TestComponent2>();
        ecs::detail::Archetype original(mask);
        
        // Add entities and components
        original.add_entity(5);
        original.add_entity(10);
        
        auto comp1_id = ecs::detail::ComponentRegistry::instance().get_component_type_id<TestComponent1>();
        auto comp2_id = ecs::detail::ComponentRegistry::instance().get_component_type_id<TestComponent2>();
        
        TestComponent1 tc1_a(100), tc1_b(200);
        TestComponent2 tc2_a(1.5f), tc2_b(2.5f);
        
        original.components.at(comp1_id).push(&tc1_a);
        original.components.at(comp1_id).push(&tc1_b);
        original.components.at(comp2_id).push(&tc2_a);
        original.components.at(comp2_id).push(&tc2_b);
        
        REQUIRE(original.entity_count() == 2);

        // Copy construct
        auto copy = original;
        
        // Verify structure
        REQUIRE(copy.entity_count() == 2);
        REQUIRE(copy.mask_ == original.mask_);
        REQUIRE(copy.components.size() == 2);
        
        // Verify both component types were copied
        REQUIRE(copy.get_component<TestComponent1>(size_t(0)).value == 100);
        REQUIRE(copy.get_component<TestComponent1>(size_t(1)).value == 200);
        REQUIRE(copy.get_component<TestComponent2>(size_t(0)).data == 1.5f);
        REQUIRE(copy.get_component<TestComponent2>(size_t(1)).data == 2.5f);
        
        // Verify independence
        original.get_component<TestComponent1>(size_t(0)).value = 999;
        original.get_component<TestComponent2>(size_t(0)).data = 999.0f;
        REQUIRE(copy.get_component<TestComponent1>(size_t(0)).value == 100);
        REQUIRE(copy.get_component<TestComponent2>(size_t(0)).data == 1.5f);
    }

    SECTION("Assignment operator with same component mask")
    {
        auto mask = ecs::detail::get_component_mask<TestComponent1>();
        
        // Create source archetype
        ecs::detail::Archetype source(mask);
        source.add_entity(1);
        source.add_entity(2);
        
        auto comp_id = ecs::detail::ComponentRegistry::instance().get_component_type_id<TestComponent1>();
        TestComponent1 tc1(42), tc2(84);
        source.components.at(comp_id).push(&tc1);
        source.components.at(comp_id).push(&tc2);
        
        // Create target archetype with different data
        ecs::detail::Archetype target(mask);
        target.add_entity(99);
        TestComponent1 tc99(999);
        target.components.at(comp_id).push(&tc99);
        
        REQUIRE(target.entity_count() == 1);
        REQUIRE(target.get_component<TestComponent1>(size_t(0)).value == 999);
        
        // Assign source to target
        target = source;
        
        // Verify target now matches source
        REQUIRE(target.entity_count() == 2);
        REQUIRE(target.get_entity(0) == 1);
        REQUIRE(target.get_entity(1) == 2);
        REQUIRE(target.get_component<TestComponent1>(size_t(0)).value == 42);
        REQUIRE(target.get_component<TestComponent1>(size_t(1)).value == 84);
        
        // Verify independence
        source.get_component<TestComponent1>(size_t(0)).value = 999;
        REQUIRE(target.get_component<TestComponent1>(size_t(0)).value == 42);
    }

    SECTION("Copy empty archetype")
    {
        auto mask = ecs::detail::get_component_mask<TestComponent1>();
        ecs::detail::Archetype original(mask);
        
        REQUIRE(original.entity_count() == 0);
        
        // Copy empty archetype
        auto copy = original;
        REQUIRE(copy.entity_count() == 0);
        REQUIRE(copy.mask_ == original.mask_);
        REQUIRE(copy.components.size() == original.components.size());
        
        // Assignment with empty archetype
        ecs::detail::Archetype target(mask);
        target.add_entity(1);
        TestComponent1 tc(42);
        target.components.at(ecs::detail::ComponentRegistry::instance().get_component_type_id<TestComponent1>()).push(&tc);
        REQUIRE(target.entity_count() == 1);
        
        target = original; // Assign empty archetype
        REQUIRE(target.entity_count() == 0);
    }
}

TEST_CASE("Archetype version-based optimization", "[archetype]")
{
    ecs::register_component<TestComponent1>();
    ecs::register_component<TestComponent2>();

    SECTION("Assignment optimization with unchanged ComponentArrays")
    {
        auto mask = ecs::detail::get_component_mask<TestComponent1, TestComponent2>();
        
        // Create archetype and add some data
        ecs::detail::Archetype source(mask);
        source.add_entity(1);
        source.add_entity(2);
        
        auto comp1_id = ecs::detail::ComponentRegistry::instance().get_component_type_id<TestComponent1>();
        auto comp2_id = ecs::detail::ComponentRegistry::instance().get_component_type_id<TestComponent2>();
        
        TestComponent1 tc1_a(10), tc1_b(20);
        TestComponent2 tc2_a(1.0f), tc2_b(2.0f);
        
        source.components.at(comp1_id).push(&tc1_a);
        source.components.at(comp1_id).push(&tc1_b);
        source.components.at(comp2_id).push(&tc2_a);
        source.components.at(comp2_id).push(&tc2_b);
        
        // Create copy - this should copy all ComponentArray versions
        auto target = source;
        
        // Both should have same data initially
        REQUIRE(target.get_component<TestComponent1>(size_t(0)).value == 10);
        REQUIRE(target.get_component<TestComponent2>(size_t(0)).data == 1.0f);
        
        // Assignment with same versions should be optimized
        // (This is hard to test directly, but we can verify behavior)
        target = source;
        
        // Data should still be correct
        REQUIRE(target.get_component<TestComponent1>(size_t(0)).value == 10);
        REQUIRE(target.get_component<TestComponent2>(size_t(0)).data == 1.0f);
    }

    SECTION("Assignment copies when versions differ")
    {
        auto mask = ecs::detail::get_component_mask<TestComponent1>();
        
        ecs::detail::Archetype source(mask);
        ecs::detail::Archetype target(mask);
        
        // Add same initial data to both
        source.add_entity(1);
        target.add_entity(1);
        
        auto comp_id = ecs::detail::ComponentRegistry::instance().get_component_type_id<TestComponent1>();
        TestComponent1 tc1(100), tc2(100);
        source.components.at(comp_id).push(&tc1);
        target.components.at(comp_id).push(&tc2);
        
        // Modify source to change its version
        source.get_component<TestComponent1>(size_t(0)).value = 200; // This increments version
        
        // Now assignment should copy because versions differ
        target = source;
        
        // Target should now have source's data
        REQUIRE(target.get_component<TestComponent1>(size_t(0)).value == 200);
        
        // Verify independence
        source.get_component<TestComponent1>(size_t(0)).value = 999;
        REQUIRE(target.get_component<TestComponent1>(size_t(0)).value == 200);
    }
}

TEST_CASE("Archetype copy with complex components", "[archetype]")
{
    ecs::register_component<TestComponent3>();

    SECTION("Copy archetype with non-trivial components")
    {
        auto mask = ecs::detail::get_component_mask<TestComponent3>();
        ecs::detail::Archetype original(mask);
        
        // Add entities with string components
        original.add_entity(1);
        original.add_entity(2);
        
        auto comp_id = ecs::detail::ComponentRegistry::instance().get_component_type_id<TestComponent3>();
        TestComponent3 tc1("hello"), tc2("world");
        original.components.at(comp_id).push(&tc1);
        original.components.at(comp_id).push(&tc2);
        
        REQUIRE(original.entity_count() == 2);
        REQUIRE(original.get_component<TestComponent3>(size_t(0)).name == "hello");
        REQUIRE(original.get_component<TestComponent3>(size_t(1)).name == "world");
        
        // Copy construct
        auto copy = original;
        
        // Verify copy
        REQUIRE(copy.entity_count() == 2);
        REQUIRE(copy.get_component<TestComponent3>(size_t(0)).name == "hello");
        REQUIRE(copy.get_component<TestComponent3>(size_t(1)).name == "world");
        
        // Verify independence (deep copy of strings)
        original.get_component<TestComponent3>(size_t(0)).name = "modified";
        REQUIRE(copy.get_component<TestComponent3>(size_t(0)).name == "hello"); // Should be unchanged
        
        // Assignment test
        ecs::detail::Archetype target(mask);
        target = copy;
        
        REQUIRE(target.entity_count() == 2);
        REQUIRE(target.get_component<TestComponent3>(size_t(0)).name == "hello");
        REQUIRE(target.get_component<TestComponent3>(size_t(1)).name == "world");
        
        // Verify independence from both original and copy
        copy.get_component<TestComponent3>(size_t(1)).name = "changed";
        REQUIRE(target.get_component<TestComponent3>(size_t(1)).name == "world");
    }
}