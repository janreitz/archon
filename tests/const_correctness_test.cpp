#include <archon/ecs.h>
#include <catch2/catch_test_macros.hpp>
#include <string>

// Test components
struct Position {
    float x, y, z;
};

struct Velocity {
    float dx, dy, dz;
};

struct TestData {
    int value;
    std::string name;
};

TEST_CASE("Query works with const World&", "[ecs][const-correctness]")
{
    ecs::register_component<Position>();
    ecs::register_component<Velocity>();
    
    ecs::World world;

    // Add test entities
    for (int i = 0; i < 3; ++i) {
        ecs::EntityId entity = world.create_entity();
        Position pos{static_cast<float>(i), static_cast<float>(i * 2),
                     static_cast<float>(i * 3)};
        Velocity vel{static_cast<float>(i * 0.1f), static_cast<float>(i * 0.2f),
                     static_cast<float>(i * 0.3f)};
        world.add_components(entity, pos, vel);
    }

    // Test that Query::each() works with const World&
    const ecs::World& const_world = world;
    
    size_t entity_count = 0;
    ecs::Query<Position, Velocity>().each(
        const_world, [&entity_count](const Position &pos, const Velocity &vel) {
            entity_count++;
            REQUIRE(pos.x >= 0.0f);
            REQUIRE(vel.dx >= 0.0f);
        });
    
    REQUIRE(entity_count == 3);
}

TEST_CASE("World::add_components accepts const component references", "[ecs][const-correctness]")
{
    ecs::register_component<Position>();
    ecs::register_component<Velocity>();
    ecs::register_component<TestData>();
    
    ecs::World world;
    
    // Create some const components
    const Position const_pos{1.0f, 2.0f, 3.0f};
    const Velocity const_vel{0.1f, 0.2f, 0.3f};
    const TestData const_data{42, "test"};

    // This should work - add_components should accept const references
    ecs::EntityId entity = world.create_entity();
    REQUIRE_NOTHROW(world.add_components(entity, const_pos, const_vel, const_data));
    
    // Verify the components were added correctly
    REQUIRE(world.has_components<Position, Velocity, TestData>(entity));
    
    auto pos = world.get_component<Position>(entity);
    auto vel = world.get_component<Velocity>(entity);
    auto data = world.get_component<TestData>(entity);
    
    REQUIRE(pos.x == 1.0f);
    REQUIRE(pos.y == 2.0f);
    REQUIRE(pos.z == 3.0f);
    REQUIRE(vel.dx == 0.1f);
    REQUIRE(vel.dy == 0.2f);
    REQUIRE(vel.dz == 0.3f);
    REQUIRE(data.value == 42);
    REQUIRE(data.name == "test");
}

TEST_CASE("Query::size works with const World&", "[ecs][const-correctness]")
{
    ecs::register_component<Position>();
    ecs::register_component<Velocity>();
    
    ecs::World world;

    // Add test entities
    for (int i = 0; i < 5; ++i) {
        ecs::EntityId entity = world.create_entity();
        Position pos{static_cast<float>(i), 0.0f, 0.0f};
        Velocity vel{0.0f, 0.0f, 0.0f};
        world.add_components(entity, pos, vel);
    }

    // Test that Query::size() works with const World&
    const ecs::World& const_world = world;
    auto query = ecs::Query<Position, Velocity>();
    size_t count = query.size(const_world);
    
    REQUIRE(count == 5);
}