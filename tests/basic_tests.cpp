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
        REQUIRE(entity != std::numeric_limits<ecs::EntityId>::max()); // Check for valid entity ID
    }

    SECTION("Adding components")
    {
        auto entity = world.create_entity();
        world.add_components(entity, Position{1.0F, 2.0F, 3.0F});

        auto *pos = world.get_component<Position>(entity);
        REQUIRE(pos != nullptr);
        REQUIRE(pos->x == 1.0F);
        REQUIRE(pos->y == 2.0F);
        REQUIRE(pos->z == 3.0F);
    }

    SECTION("Multiple components")
    {
        auto entity = world.create_entity();
        world.add_components(entity, Position{1.0F, 2.0F, 3.0F},
                             Velocity{4.0F, 5.0F, 6.0F});

        auto [pos, vel] = world.get_components<Position, Velocity>(entity);
        REQUIRE(pos != nullptr);
        REQUIRE(vel != nullptr);
        REQUIRE(pos->x == 1.0F);
        REQUIRE(vel->vx == 4.0F);
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
            world, [&](Position& /*pos*/, ecs::EntityId /*entity*/) { count++; });
        REQUIRE(count == 3);
    }
}