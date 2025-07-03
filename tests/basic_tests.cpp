#include <archon/ecs.h>
#include <catch2/catch_test_macros.hpp>
#include <limits>

struct Position {
    float x, y, z;
};

struct Velocity {
    float vx, vy, vz;
};

struct Health {
    float current;
    float max;
};

TEST_CASE("Basic entity and component operations", "[ecs]")
{
    ecs::World world;
    ecs::ComponentRegistry::instance().register_component<Position>();
    ecs::ComponentRegistry::instance().register_component<Velocity>();
    ecs::ComponentRegistry::instance().register_component<Health>();

    SECTION("Entity creation")
    {
        auto entity = world.create_entity();
        REQUIRE(entity !=
                std::numeric_limits<ecs::EntityId>::max()); // Check for valid
                                                            // entity ID
    }

    SECTION("Adding components")
    {
        auto entity = world.create_entity();
        world.add_components(entity, Position{1.0F, 2.0F, 3.0F});

        auto &pos = world.get_component<Position>(entity);
        REQUIRE(pos.x == 1.0F);
        REQUIRE(pos.y == 2.0F);
        REQUIRE(pos.z == 3.0F);
    }

    SECTION("Multiple components")
    {
        auto entity = world.create_entity();
        world.add_components(entity, Position{1.0F, 2.0F, 3.0F},
                             Velocity{4.0F, 5.0F, 6.0F});

        auto [pos, vel] = world.get_components<Position, Velocity>(entity);
        REQUIRE(pos.x == 1.0F);
        REQUIRE(vel.vx == 4.0F);
    }
}

TEST_CASE("Basic querying", "[ecs]")
{
    ecs::World world;
    ecs::ComponentRegistry::instance().register_component<Position>();
    ecs::ComponentRegistry::instance().register_component<Velocity>();

    // Create some test entities
    auto e1 = world.create_entity();
    world.add_components(e1, Position{1.0F, 0.0F, 0.0F},
                         Velocity{1.0F, 0.0F, 0.0F});

    auto e2 = world.create_entity();
    world.add_components(e2, Position{2.0F, 0.0F, 0.0F});

    auto e3 = world.create_entity();
    world.add_components(e3, Position{3.0F, 0.0F, 0.0F},
                         Velocity{3.0F, 0.0F, 0.0F});

    SECTION("Query with single component")
    {
        int count = 0;
        ecs::Query<Position>().each(world, [&](Position &pos) {
            REQUIRE(pos.x > 0.0F);
            count++;
        });
        REQUIRE(count == 3);
    }

    SECTION("Query with multiple components")
    {
        int count = 0;
        ecs::Query<Position, Velocity>().each(
            world, [&](Position &pos, Velocity &vel) {
                REQUIRE(pos.x == vel.vx);
                count++;
            });
        REQUIRE(count == 2);
    }

    SECTION("Query with entity ID")
    {
        int count = 0;
        ecs::Query<Position>().each(
            world,
            [&](Position & /*pos*/, ecs::EntityId /*entity*/) { count++; });
        REQUIRE(count == 3);
    }
}

TEST_CASE("Component removal operations", "[ecs]")
{
    ecs::World world;
    ecs::ComponentRegistry::instance().register_component<Position>();
    ecs::ComponentRegistry::instance().register_component<Velocity>();
    ecs::ComponentRegistry::instance().register_component<Health>();

    SECTION("Remove single component")
    {
        auto entity = world.create_entity();
        world.add_components(entity, Position{1.0F, 2.0F, 3.0F},
                             Velocity{4.0F, 5.0F, 6.0F});

        // Verify components exist
        auto &pos = world.get_component<Position>(entity);
        REQUIRE(world.has_component<Velocity>(entity));

        // Remove one component
        world.remove_components<Velocity>(entity);
        REQUIRE(!world.has_component<Velocity>(entity));

        // Position should still exist, Velocity should be gone
        pos = world.get_component<Position>(entity);
        REQUIRE(pos.x == 1.0F);
        REQUIRE(pos.y == 2.0F);
        REQUIRE(pos.z == 3.0F);
    }

    SECTION("Remove multiple components")
    {
        auto entity = world.create_entity();
        world.add_components(entity, Position{1.0F, 2.0F, 3.0F},
                             Velocity{4.0F, 5.0F, 6.0F},
                             Health{100.0F, 100.0F});

        // Remove multiple components at once
        world.remove_components<Velocity, Health>(entity);

        // Only Position should remain
        auto &pos = world.get_component<Position>(entity);
        REQUIRE(pos.x == 1.0F);
        REQUIRE(!world.has_component<Velocity>(entity));
        REQUIRE(!world.has_component<Health>(entity));
    }

    SECTION("Remove all components")
    {
        auto entity = world.create_entity();
        world.add_components(entity, Position{1.0F, 2.0F, 3.0F},
                             Velocity{4.0F, 5.0F, 6.0F});

        // Remove all components
        world.remove_components<Position, Velocity>(entity);

        // Entity should have no components
        REQUIRE(!world.has_component<Position>(entity));
        REQUIRE(!world.has_component<Velocity>(entity));
    }

    SECTION("Remove non-existent component")
    {
        auto entity = world.create_entity();
        world.add_components(entity, Position{1.0F, 2.0F, 3.0F});

        // Try to remove a component that doesn't exist
        world.remove_components<Velocity>(entity);

        // Position should still exist
        auto &pos = world.get_component<Position>(entity);
        REQUIRE(pos.x == 1.0F);
    }

    SECTION("Remove from non-existent entity")
    {
        // This should not crash
        world.remove_components<Position>(999);
    }

    SECTION("Query after component removal")
    {
        // Create entities with different component combinations
        auto e1 = world.create_entity();
        world.add_components(e1, Position{1.0F, 0.0F, 0.0F},
                             Velocity{1.0F, 0.0F, 0.0F});

        auto e2 = world.create_entity();
        world.add_components(e2, Position{2.0F, 0.0F, 0.0F},
                             Velocity{2.0F, 0.0F, 0.0F});

        auto e3 = world.create_entity();
        world.add_components(e3, Position{3.0F, 0.0F, 0.0F});

        // Initially, 2 entities should have both Position and Velocity
        int count_before = 0;
        ecs::Query<Position, Velocity>().each(
            world, [&](Position &, Velocity &) { count_before++; });
        REQUIRE(count_before == 2);

        // Remove Velocity from one entity
        world.remove_components<Velocity>(e1);

        // Now only 1 entity should have both components
        int count_after = 0;
        ecs::Query<Position, Velocity>().each(
            world, [&](Position &, Velocity &) { count_after++; });
        REQUIRE(count_after == 1);

        // But all 3 entities should still have Position
        int pos_count = 0;
        ecs::Query<Position>().each(world, [&](Position &) { pos_count++; });
        REQUIRE(pos_count == 3);
    }
}