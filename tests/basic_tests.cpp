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