#include <archon/ecs.h>
#include <catch2/catch_test_macros.hpp>
#include <limits>
#include <string>
#include <utility>
#include <vector>

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

// Test components with different copy/move semantics
struct TrivialComponent {
    int value;
};

struct NonTrivialComponent {
    std::string name;
    size_t copy_counter = 0;
    size_t move_counter = 0;

    NonTrivialComponent() = default;
    explicit NonTrivialComponent(std::string n) : name(std::move(n)) {}
    NonTrivialComponent(const NonTrivialComponent &other)
        : name(other.name), copy_counter(other.copy_counter + 1)
    {
    }
    NonTrivialComponent(NonTrivialComponent &&other) noexcept
        : name(std::move(other.name)), copy_counter(other.copy_counter),
          move_counter(other.move_counter + 1)
    {
    }
};

TEST_CASE("Basic entity and component operations", "[ecs]")
{
    ecs::World world;
    ecs::register_component<Position>();
    ecs::register_component<Velocity>();
    ecs::register_component<Health>();

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
    ecs::register_component<Position>();
    ecs::register_component<Velocity>();

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
    ecs::register_component<Position>();
    ecs::register_component<Velocity>();
    ecs::register_component<Health>();

    SECTION("Remove single component")
    {
        auto entity = world.create_entity();
        world.add_components(entity, Position{1.0F, 2.0F, 3.0F},
                             Velocity{4.0F, 5.0F, 6.0F});

        // Verify components exist
        auto &pos = world.get_component<Position>(entity);
        REQUIRE(world.has_components<Velocity>(entity));

        // Remove one component
        world.remove_components<Velocity>(entity);
        REQUIRE(!world.has_components<Velocity>(entity));

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
        REQUIRE(!world.has_components<Velocity>(entity));
        REQUIRE(!world.has_components<Health>(entity));
    }

    SECTION("Remove all components")
    {
        auto entity = world.create_entity();
        world.add_components(entity, Position{1.0F, 2.0F, 3.0F},
                             Velocity{4.0F, 5.0F, 6.0F});

        // Remove all components
        world.remove_components<Position, Velocity>(entity);

        // Entity should have no components
        REQUIRE(!world.has_components<Position>(entity));
        REQUIRE(!world.has_components<Velocity>(entity));
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

TEST_CASE("Archetype transitions with different component types",
          "[ecs][archetype]")
{
    ecs::World world;
    ecs::register_component<TrivialComponent>();
    ecs::register_component<NonTrivialComponent>();

    SECTION("Trivial component transition uses memcpy")
    {
        auto entity = world.create_entity();
        world.add_components(entity, TrivialComponent{42});

        // Add another component to trigger archetype transition
        world.add_components(entity, NonTrivialComponent{"test"});

        // Verify data integrity after transition
        auto &trivial = world.get_component<TrivialComponent>(entity);
        auto &non_trivial = world.get_component<NonTrivialComponent>(entity);

        REQUIRE(trivial.value == 42);
        REQUIRE(non_trivial.name == "test");
        REQUIRE(non_trivial.copy_counter ==
                0); // Should not increment for fresh add
    }

    SECTION("Non-trivial component transition uses move semantics")
    {
        auto entity = world.create_entity();
        // First move from rvalue temporary
        world.add_components(entity, NonTrivialComponent{"original"});

        // Second move during archetype transition
        world.add_components(entity, TrivialComponent{100});

        // Verify move semantics were used (move_counter incremented)
        auto &non_trivial = world.get_component<NonTrivialComponent>(entity);
        REQUIRE(non_trivial.name == "original");
        REQUIRE(non_trivial.move_counter == 2);
    }

    SECTION("Multiple archetype transitions preserve data")
    {
        auto entity = world.create_entity();
        world.add_components(entity, TrivialComponent{1});
        world.add_components(entity, NonTrivialComponent{"step1"});

        // Remove and add components to trigger multiple transitions
        world.remove_components<TrivialComponent>(entity);
        world.add_components(entity, TrivialComponent{2});

        auto &trivial = world.get_component<TrivialComponent>(entity);
        auto &non_trivial = world.get_component<NonTrivialComponent>(entity);

        REQUIRE(trivial.value == 2);
        REQUIRE(non_trivial.name == "step1");
        REQUIRE(non_trivial.copy_counter == 0);
    }
}

TEST_CASE("Component array removal with different types",
          "[ecs][component_array]")
{
    ecs::World world;
    ecs::register_component<TrivialComponent>();
    ecs::register_component<NonTrivialComponent>();

    SECTION("Trivial component removal")
    {
        // Create multiple entities to test removal from middle
        auto e1 = world.create_entity();
        auto e2 = world.create_entity();
        auto e3 = world.create_entity();

        world.add_components(e1, TrivialComponent{1});
        world.add_components(e2, TrivialComponent{2});
        world.add_components(e3, TrivialComponent{3});

        // Remove middle entity
        world.remove_components<TrivialComponent>(e2);

        // Verify remaining entities have correct values
        REQUIRE(world.has_components<TrivialComponent>(e1));
        REQUIRE(!world.has_components<TrivialComponent>(e2));
        REQUIRE(world.has_components<TrivialComponent>(e3));

        REQUIRE(world.get_component<TrivialComponent>(e1).value == 1);
        REQUIRE(world.get_component<TrivialComponent>(e3).value == 3);
    }

    SECTION("Non-trivial component removal")
    {
        auto e1 = world.create_entity();
        auto e2 = world.create_entity();
        auto e3 = world.create_entity();

        world.add_components(e1, NonTrivialComponent{"first"});
        world.add_components(e2, NonTrivialComponent{"second"});
        world.add_components(e3, NonTrivialComponent{"third"});

        // Remove middle entity
        world.remove_components<NonTrivialComponent>(e2);

        // Verify remaining entities
        REQUIRE(world.has_components<NonTrivialComponent>(e1));
        REQUIRE(!world.has_components<NonTrivialComponent>(e2));
        REQUIRE(world.has_components<NonTrivialComponent>(e3));

        REQUIRE(world.get_component<NonTrivialComponent>(e1).name == "first");
        REQUIRE(world.get_component<NonTrivialComponent>(e3).name == "third");
    }
}

TEST_CASE("Complex archetype transition scenarios", "[ecs][archetype]")
{
    ecs::World world;
    ecs::register_component<TrivialComponent>();
    ecs::register_component<NonTrivialComponent>();

    SECTION("Add components to entity with existing components")
    {
        auto entity = world.create_entity();
        world.add_components(entity, TrivialComponent{100});

        // Add multiple components at once
        world.add_components(entity, NonTrivialComponent{"batch"});

        // Verify all components exist
        REQUIRE(world.has_components<TrivialComponent>(entity));
        REQUIRE(world.has_components<NonTrivialComponent>(entity));

        auto &trivial = world.get_component<TrivialComponent>(entity);
        auto &non_trivial = world.get_component<NonTrivialComponent>(entity);

        REQUIRE(trivial.value == 100);
        REQUIRE(non_trivial.name == "batch");
    }

    SECTION("Remove and re-add components")
    {
        auto entity = world.create_entity();
        world.add_components(entity, TrivialComponent{200},
                             NonTrivialComponent{"original"});

        // Remove one component
        world.remove_components<TrivialComponent>(entity);
        REQUIRE(!world.has_components<TrivialComponent>(entity));
        REQUIRE(world.has_components<NonTrivialComponent>(entity));

        // Re-add the component
        world.add_components(entity, TrivialComponent{300});

        // Verify both components exist with correct values
        REQUIRE(world.get_component<TrivialComponent>(entity).value == 300);
        REQUIRE(world.get_component<NonTrivialComponent>(entity).name ==
                "original");
    }

    SECTION("Stress test: Multiple entities with frequent transitions")
    {
        const int NUM_ENTITIES = 5;
        std::vector<ecs::EntityId> entities;

        // Create entities with initial components
        for (int i = 0; i < NUM_ENTITIES; ++i) {
            auto entity = world.create_entity();
            world.add_components(entity, TrivialComponent{i});
            entities.push_back(entity);
        }

        // Add second component to all entities
        for (auto entity : entities) {
            world.add_components(entity, NonTrivialComponent{"stress"});
        }

        // Remove and re-add components for odd entities
        for (int i = 1; i < NUM_ENTITIES; i += 2) {
            world.remove_components<TrivialComponent>(entities[i]);
            world.add_components(entities[i], TrivialComponent{i + 1000});
        }

        // Verify final state
        for (int i = 0; i < NUM_ENTITIES; ++i) {
            REQUIRE(world.has_components<TrivialComponent>(entities[i]));
            REQUIRE(world.has_components<NonTrivialComponent>(entities[i]));

            auto &trivial = world.get_component<TrivialComponent>(entities[i]);
            if (i % 2 == 0) {
                REQUIRE(trivial.value == i);
            } else {
                REQUIRE(trivial.value == i + 1000);
            }
        }
    }
}

TEST_CASE("Entity removal operations", "[ecs][entity]")
{
    ecs::World world;
    ecs::register_component<Position>();
    ecs::register_component<Velocity>();
    ecs::register_component<Health>();

    SECTION("Remove entity with no components")
    {
        auto entity = world.create_entity();

        // Remove the entity (should succeed even with no components)
        bool removed = world.remove_entity(entity);
        REQUIRE(removed == true);

        // Entity should no longer exist
        // Note: has_components will likely crash or return false for
        // non-existent entity This is expected behavior for invalid entity
        // access
    }

    SECTION("Remove entity with single component")
    {
        auto entity = world.create_entity();
        world.add_components(entity, Position{1.0F, 2.0F, 3.0F});

        // Verify entity has components
        REQUIRE(world.has_components<Position>(entity));

        // Remove the entity
        bool removed = world.remove_entity(entity);
        REQUIRE(removed == true);
    }

    SECTION("Remove entity with multiple components")
    {
        auto entity = world.create_entity();
        world.add_components(entity, Position{1.0F, 2.0F, 3.0F},
                             Velocity{4.0F, 5.0F, 6.0F},
                             Health{100.0F, 100.0F});

        // Verify entity has all components
        REQUIRE(world.has_components<Position>(entity));
        REQUIRE(world.has_components<Velocity>(entity));
        REQUIRE(world.has_components<Health>(entity));

        // Remove the entity
        bool removed = world.remove_entity(entity);
        REQUIRE(removed == true);
    }

    SECTION("Remove non-existent entity")
    {
        // Try to remove an entity that was never created
        ecs::EntityId fake_entity = 9999;
        bool removed = world.remove_entity(fake_entity);
        REQUIRE(removed == false);
    }

    SECTION("Remove entity twice")
    {
        auto entity = world.create_entity();
        world.add_components(entity, Position{1.0F, 2.0F, 3.0F});

        // First removal should succeed
        bool first_removal = world.remove_entity(entity);
        REQUIRE(first_removal == true);

        // Second removal should fail
        bool second_removal = world.remove_entity(entity);
        REQUIRE(second_removal == false);
    }

    SECTION("Entity removal affects queries")
    {
        // Create multiple entities
        auto e1 = world.create_entity();
        auto e2 = world.create_entity();
        auto e3 = world.create_entity();

        world.add_components(e1, Position{1.0F, 0.0F, 0.0F});
        world.add_components(e2, Position{2.0F, 0.0F, 0.0F});
        world.add_components(e3, Position{3.0F, 0.0F, 0.0F});

        // Initially should have 3 entities with Position
        int count_before = 0;
        ecs::Query<Position>().each(world, [&](Position &) { count_before++; });
        REQUIRE(count_before == 3);

        // Remove one entity
        world.remove_entity(e2);

        // Should now have 2 entities with Position
        int count_after = 0;
        ecs::Query<Position>().each(world, [&](Position &) { count_after++; });
        REQUIRE(count_after == 2);
    }

    SECTION("Entity removal with mixed archetypes")
    {
        // Create entities in different archetypes
        auto e1 = world.create_entity(); // Position only
        world.add_components(e1, Position{1.0F, 0.0F, 0.0F});

        auto e2 = world.create_entity(); // Position + Velocity
        world.add_components(e2, Position{2.0F, 0.0F, 0.0F},
                             Velocity{1.0F, 0.0F, 0.0F});

        auto e3 = world.create_entity(); // All three components
        world.add_components(e3, Position{3.0F, 0.0F, 0.0F},
                             Velocity{2.0F, 0.0F, 0.0F},
                             Health{100.0F, 100.0F});

        // Verify initial counts
        int pos_count = 0, vel_count = 0, health_count = 0;
        ecs::Query<Position>().each(world, [&](Position &) { pos_count++; });
        ecs::Query<Velocity>().each(world, [&](Velocity &) { vel_count++; });
        ecs::Query<Health>().each(world, [&](Health &) { health_count++; });

        REQUIRE(pos_count == 3);
        REQUIRE(vel_count == 2);
        REQUIRE(health_count == 1);

        // Remove entity from middle archetype
        world.remove_entity(e2);

        // Verify counts after removal
        pos_count = 0;
        vel_count = 0;
        health_count = 0;
        ecs::Query<Position>().each(world, [&](Position &) { pos_count++; });
        ecs::Query<Velocity>().each(world, [&](Velocity &) { vel_count++; });
        ecs::Query<Health>().each(world, [&](Health &) { health_count++; });

        REQUIRE(pos_count == 2);
        REQUIRE(vel_count == 1);
        REQUIRE(health_count == 1);
    }

    SECTION("Entity removal preserves other entities' component data")
    {
        // Create entities with unique data
        auto e1 = world.create_entity();
        auto e2 = world.create_entity();
        auto e3 = world.create_entity();

        world.add_components(e1, Position{10.0F, 20.0F, 30.0F});
        world.add_components(e2, Position{40.0F, 50.0F, 60.0F});
        world.add_components(e3, Position{70.0F, 80.0F, 90.0F});

        // Remove middle entity
        world.remove_entity(e2);

        // Verify remaining entities have correct data
        auto &pos1 = world.get_component<Position>(e1);
        auto &pos3 = world.get_component<Position>(e3);

        REQUIRE(pos1.x == 10.0F);
        REQUIRE(pos1.y == 20.0F);
        REQUIRE(pos1.z == 30.0F);

        REQUIRE(pos3.x == 70.0F);
        REQUIRE(pos3.y == 80.0F);
        REQUIRE(pos3.z == 90.0F);
    }
}

TEST_CASE("Query remove_if operations", "[ecs][query][remove_if]")
{
    ecs::World world;
    ecs::register_component<Position>();
    ecs::register_component<Velocity>();
    ecs::register_component<Health>();

    SECTION("Remove entities based on single component predicate")
    {
        // Create test entities
        auto e1 = world.create_entity();
        auto e2 = world.create_entity();
        auto e3 = world.create_entity();
        auto e4 = world.create_entity();

        world.add_components(e1, Position{-5.0F, 0.0F, 0.0F});
        world.add_components(e2, Position{10.0F, 0.0F, 0.0F});
        world.add_components(e3, Position{-2.0F, 0.0F, 0.0F});
        world.add_components(e4, Position{8.0F, 0.0F, 0.0F});

        // Initially should have 4 entities
        REQUIRE(ecs::Query<Position>().size(world) == 4);

        // Remove entities with negative x position
        ecs::Query<Position>().remove_if(
            world, [](ecs::EntityId, Position &pos) { return pos.x < 0.0F; });

        // Should have 2 entities remaining
        REQUIRE(ecs::Query<Position>().size(world) == 2);

        // Verify remaining entities have positive x
        ecs::Query<Position>().each(
            world, [](Position &pos) { REQUIRE(pos.x > 0.0F); });
    }

    SECTION("Remove entities based on multiple component predicate")
    {
        // Create entities with different component combinations
        auto e1 = world.create_entity();
        auto e2 = world.create_entity();
        auto e3 = world.create_entity();
        auto e4 = world.create_entity();

        world.add_components(e1, Position{1.0F, 0.0F, 0.0F},
                             Velocity{5.0F, 0.0F, 0.0F});
        world.add_components(e2, Position{2.0F, 0.0F, 0.0F},
                             Velocity{15.0F, 0.0F, 0.0F});
        world.add_components(e3, Position{3.0F, 0.0F, 0.0F},
                             Velocity{8.0F, 0.0F, 0.0F});
        world.add_components(e4, Position{4.0F, 0.0F, 0.0F},
                             Velocity{25.0F, 0.0F, 0.0F});

        // Initially should have 4 entities with both components
        REQUIRE(ecs::Query<Position, Velocity>().size(world) == 4);

        // Remove entities with high velocity (> 10)
        ecs::Query<Position, Velocity>().remove_if(
            world, [](ecs::EntityId, Position &, Velocity &vel) {
                return vel.vx > 10.0F;
            });

        // Should have 2 entities remaining
        REQUIRE(ecs::Query<Position, Velocity>().size(world) == 2);

        // Verify remaining entities have velocity <= 10
        ecs::Query<Position, Velocity>().each(
            world, [](Position &, Velocity &vel) { REQUIRE(vel.vx <= 10.0F); });
    }

    SECTION("Remove entities using EntityId in predicate")
    {
        // Create entities and store their IDs
        auto e1 = world.create_entity();
        auto e2 = world.create_entity();
        auto e3 = world.create_entity();

        world.add_components(e1, Position{1.0F, 0.0F, 0.0F});
        world.add_components(e2, Position{2.0F, 0.0F, 0.0F});
        world.add_components(e3, Position{3.0F, 0.0F, 0.0F});

        // Remove specific entity by ID
        ecs::Query<Position>().remove_if(
            world,
            [e2](ecs::EntityId entity, Position &) { return entity == e2; });

        // Should have 2 entities remaining
        REQUIRE(ecs::Query<Position>().size(world) == 2);

        // Verify e2 is gone, e1 and e3 remain
        bool found_e1 = false, found_e3 = false;
        ecs::Query<Position>().each(
            world, [&](Position &, ecs::EntityId entity) {
                if (entity == e1)
                    found_e1 = true;
                if (entity == e3)
                    found_e3 = true;
                REQUIRE(entity != e2); // e2 should be removed
            });
        REQUIRE(found_e1);
        REQUIRE(found_e3);
    }

    SECTION("Remove all entities matching query")
    {
        // Create entities in different archetypes
        auto e1 = world.create_entity();
        auto e2 = world.create_entity();
        auto e3 = world.create_entity();

        world.add_components(e1, Position{1.0F, 0.0F, 0.0F});
        world.add_components(e2, Position{2.0F, 0.0F, 0.0F},
                             Velocity{1.0F, 0.0F, 0.0F});
        world.add_components(e3, Health{100.0F, 100.0F}); // Different archetype

        // Remove all entities with Position component
        ecs::Query<Position>().remove_if(world, [](ecs::EntityId, Position &) {
            return true; // Remove all
        });

        // Should have 0 entities with Position
        REQUIRE(ecs::Query<Position>().size(world) == 0);

        // Entity with only Health should remain
        REQUIRE(ecs::Query<Health>().size(world) == 1);
    }

    SECTION("Remove no entities when predicate is false")
    {
        // Create test entities
        auto e1 = world.create_entity();
        auto e2 = world.create_entity();

        world.add_components(e1, Position{1.0F, 0.0F, 0.0F});
        world.add_components(e2, Position{2.0F, 0.0F, 0.0F});

        size_t initial_count = ecs::Query<Position>().size(world);

        // Remove nothing (predicate always false)
        ecs::Query<Position>().remove_if(world, [](ecs::EntityId, Position &) {
            return false; // Remove none
        });

        // Count should be unchanged
        REQUIRE(ecs::Query<Position>().size(world) == initial_count);
    }

    SECTION("Remove from empty query")
    {
        // Don't create any entities

        // Should not crash when removing from empty query
        ecs::Query<Position>().remove_if(
            world, [](ecs::EntityId, Position &) { return true; });

        REQUIRE(ecs::Query<Position>().size(world) == 0);
    }

    SECTION("Remove entities across multiple archetypes")
    {
        // Create entities in different archetypes
        auto e1 = world.create_entity(); // Position only
        auto e2 = world.create_entity(); // Position + Velocity
        auto e3 = world.create_entity(); // Position + Health
        auto e4 = world.create_entity(); // Position + Velocity + Health

        world.add_components(e1, Position{1.0F, 0.0F, 0.0F});
        world.add_components(e2, Position{2.0F, 0.0F, 0.0F},
                             Velocity{1.0F, 0.0F, 0.0F});
        world.add_components(e3, Position{3.0F, 0.0F, 0.0F},
                             Health{100.0F, 100.0F});
        world.add_components(e4, Position{4.0F, 0.0F, 0.0F},
                             Velocity{1.0F, 0.0F, 0.0F},
                             Health{100.0F, 100.0F});

        // Initially should have 4 entities with Position
        REQUIRE(ecs::Query<Position>().size(world) == 4);

        // Remove entities with even x position
        ecs::Query<Position>().remove_if(
            world, [](ecs::EntityId, Position &pos) {
                return static_cast<int>(pos.x) % 2 == 0;
            });

        // Should have 2 entities remaining (x=1 and x=3)
        REQUIRE(ecs::Query<Position>().size(world) == 2);

        // Verify remaining entities have odd x position
        ecs::Query<Position>().each(world, [](Position &pos) {
            REQUIRE(static_cast<int>(pos.x) % 2 == 1);
        });
    }

    SECTION("Remove non-trivial components properly")
    {
        ecs::register_component<NonTrivialComponent>();

        // Create entities with non-trivial components
        auto e1 = world.create_entity();
        auto e2 = world.create_entity();
        auto e3 = world.create_entity();
        auto e4 = world.create_entity();

        world.add_components(e1, NonTrivialComponent{"keep1"});
        world.add_components(e2, NonTrivialComponent{"remove"});
        world.add_components(e3, NonTrivialComponent{"keep2"});
        world.add_components(e4, NonTrivialComponent{"remove2"});

        // Initially should have 4 entities
        REQUIRE(ecs::Query<NonTrivialComponent>().size(world) == 4);

        // Remove entities with names starting with "remove"
        ecs::Query<NonTrivialComponent>().remove_if(
            world, [](ecs::EntityId, NonTrivialComponent &comp) {
                return comp.name.find("remove") == 0;
            });

        // Should have 2 entities remaining
        REQUIRE(ecs::Query<NonTrivialComponent>().size(world) == 2);

        // Verify remaining entities have correct names and move semantics were
        // preserved
        ecs::Query<NonTrivialComponent>().each(
            world, [](NonTrivialComponent &comp) {
                REQUIRE(comp.name.find("keep") == 0);
                // The move counters should reflect proper move semantics during
                // swapping
                REQUIRE(comp.move_counter >=
                        1); // At least one move from initial construction
            });
    }
}

TEST_CASE("Edge cases and error conditions", "[ecs][archetype]")
{
    ecs::World world;
    ecs::register_component<TrivialComponent>();
    ecs::register_component<NonTrivialComponent>();

    SECTION("Remove from empty archetype")
    {
        auto entity = world.create_entity();

        // Should not crash
        world.remove_components<TrivialComponent>(entity);
        world.remove_components<NonTrivialComponent>(entity);

        REQUIRE(!world.has_components<TrivialComponent>(entity));
        REQUIRE(!world.has_components<NonTrivialComponent>(entity));
    }

    SECTION("Component data alignment after transitions")
    {
        // Test with components of different sizes
        struct SmallComponent {
            char c;
        };
        struct LargeComponent {
            double data[10];
        };

        ecs::register_component<SmallComponent>();
        ecs::register_component<LargeComponent>();

        auto entity = world.create_entity();
        world.add_components(entity, SmallComponent{'A'});
        world.add_components(entity, LargeComponent{});

        // Verify components are properly aligned and accessible
        REQUIRE(world.get_component<SmallComponent>(entity).c == 'A');
        REQUIRE(world.has_components<LargeComponent>(entity));
    }
}