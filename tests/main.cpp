#include "test_macros.h"

#include <archon/ecs.h>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

// =============================================================================
// basic_tests
// =============================================================================

namespace basic_tests
{

namespace
{

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

struct MoveOnlyComponent {
    std::unique_ptr<int> data;
    size_t move_counter = 0;

    MoveOnlyComponent() : data(std::make_unique<int>(42)) {}
    explicit MoveOnlyComponent(int value) : data(std::make_unique<int>(value))
    {
    }

    // Move operations
    MoveOnlyComponent(MoveOnlyComponent &&other) noexcept
        : data(std::move(other.data)), move_counter(other.move_counter + 1)
    {
    }

    MoveOnlyComponent &operator=(MoveOnlyComponent &&other) noexcept
    {
        data = std::move(other.data);
        move_counter = other.move_counter + 1;
        return *this;
    }

    // Delete copy operations
    MoveOnlyComponent(const MoveOnlyComponent &) = delete;
    MoveOnlyComponent &operator=(const MoveOnlyComponent &) = delete;
};

} // namespace

// ---------------------------------------------------------------------------
// Basic entity and component operations
// ---------------------------------------------------------------------------

void entity_creation()
{
    ecs::World world;
    ecs::register_component<Position>();
    ecs::register_component<Velocity>();
    ecs::register_component<Health>();
    ecs::register_component<MoveOnlyComponent>();

    auto entity = world.create_entity();
    REQUIRE(entity != std::numeric_limits<ecs::EntityId>::max(),
            "newly created entity should have a valid id");
}

void adding_components()
{
    ecs::World world;
    ecs::register_component<Position>();
    ecs::register_component<Velocity>();
    ecs::register_component<Health>();
    ecs::register_component<MoveOnlyComponent>();

    auto entity = world.create_entity();
    world.add_components(entity, Position{1.0F, 2.0F, 3.0F});

    auto &pos = world.get_component<Position>(entity);
    REQUIRE(pos.x == 1.0F, "position.x should be 1.0");
    REQUIRE(pos.y == 2.0F, "position.y should be 2.0");
    REQUIRE(pos.z == 3.0F, "position.z should be 3.0");
}

void can_add_move_only_component()
{
    ecs::World world;
    ecs::register_component<Position>();
    ecs::register_component<Velocity>();
    ecs::register_component<Health>();
    ecs::register_component<MoveOnlyComponent>();

    auto entity = world.create_entity();
    MoveOnlyComponent comp(123);
    world.add_components(entity, std::move(comp));

    auto &retrieved = world.get_component<MoveOnlyComponent>(entity);
    REQUIRE(retrieved.data != nullptr,
            "move-only component data should not be null");
    REQUIRE(*retrieved.data == 123, "move-only component value should be 123");
    REQUIRE(retrieved.move_counter > 0,
            "move-only component should have been moved");
}

void multiple_components()
{
    ecs::World world;
    ecs::register_component<Position>();
    ecs::register_component<Velocity>();
    ecs::register_component<Health>();
    ecs::register_component<MoveOnlyComponent>();

    auto entity = world.create_entity();
    world.add_components(entity, Position{1.0F, 2.0F, 3.0F},
                         Velocity{4.0F, 5.0F, 6.0F});

    auto [pos, vel] = world.get_components<Position, Velocity>(entity);
    REQUIRE(pos.x == 1.0F, "position.x should be 1.0");
    REQUIRE(vel.vx == 4.0F, "velocity.vx should be 4.0");
}

void move_only_component_with_other_components()
{
    ecs::World world;
    ecs::register_component<Position>();
    ecs::register_component<Velocity>();
    ecs::register_component<Health>();
    ecs::register_component<MoveOnlyComponent>();

    auto entity = world.create_entity();
    world.add_components(entity, MoveOnlyComponent(456),
                         Position{1.0F, 2.0F, 3.0F});

    auto [move_comp, pos] =
        world.get_components<MoveOnlyComponent, Position>(entity);
    REQUIRE(*move_comp.data == 456, "move-only component value should be 456");
    REQUIRE(pos.x == 1.0F, "position.x should be 1.0");
}

// ---------------------------------------------------------------------------
// Basic querying
// ---------------------------------------------------------------------------

void query_with_single_component()
{
    ecs::World world;
    ecs::register_component<Position>();
    ecs::register_component<Velocity>();
    ecs::register_component<MoveOnlyComponent>();

    auto e1 = world.create_entity();
    world.add_components(e1, Position{1.0F, 0.0F, 0.0F},
                         Velocity{1.0F, 0.0F, 0.0F});
    auto e2 = world.create_entity();
    world.add_components(e2, Position{2.0F, 0.0F, 0.0F});
    auto e3 = world.create_entity();
    world.add_components(e3, Position{3.0F, 0.0F, 0.0F},
                         Velocity{3.0F, 0.0F, 0.0F});

    int count = 0;
    ecs::Query<Position>().each(world, [&](Position &pos) {
        REQUIRE(pos.x > 0.0F, "queried position.x should be positive");
        count++;
    });
    REQUIRE(count == 3, "query should visit 3 entities");
}

void query_with_multiple_components()
{
    ecs::World world;
    ecs::register_component<Position>();
    ecs::register_component<Velocity>();
    ecs::register_component<MoveOnlyComponent>();

    auto e1 = world.create_entity();
    world.add_components(e1, Position{1.0F, 0.0F, 0.0F},
                         Velocity{1.0F, 0.0F, 0.0F});
    auto e2 = world.create_entity();
    world.add_components(e2, Position{2.0F, 0.0F, 0.0F});
    auto e3 = world.create_entity();
    world.add_components(e3, Position{3.0F, 0.0F, 0.0F},
                         Velocity{3.0F, 0.0F, 0.0F});

    int count = 0;
    ecs::Query<Position, Velocity>().each(
        world, [&](Position &pos, Velocity &vel) {
            REQUIRE(pos.x == vel.vx, "position.x should match velocity.vx");
            count++;
        });
    REQUIRE(count == 2, "query should visit 2 entities with both components");
}

void query_with_entity_id()
{
    ecs::World world;
    ecs::register_component<Position>();
    ecs::register_component<Velocity>();
    ecs::register_component<MoveOnlyComponent>();

    auto e1 = world.create_entity();
    world.add_components(e1, Position{1.0F, 0.0F, 0.0F},
                         Velocity{1.0F, 0.0F, 0.0F});
    auto e2 = world.create_entity();
    world.add_components(e2, Position{2.0F, 0.0F, 0.0F});
    auto e3 = world.create_entity();
    world.add_components(e3, Position{3.0F, 0.0F, 0.0F},
                         Velocity{3.0F, 0.0F, 0.0F});

    int count = 0;
    ecs::Query<Position>().each(
        world,
        [&](Position & /*pos*/, ecs::EntityId /*entity*/) { count++; });
    REQUIRE(count == 3, "query should visit 3 entities");
}

void query_with_move_only_component()
{
    ecs::World world;
    ecs::register_component<Position>();
    ecs::register_component<Velocity>();
    ecs::register_component<MoveOnlyComponent>();

    auto e1 = world.create_entity();
    world.add_components(e1, Position{1.0F, 0.0F, 0.0F},
                         Velocity{1.0F, 0.0F, 0.0F});
    auto e2 = world.create_entity();
    world.add_components(e2, Position{2.0F, 0.0F, 0.0F});
    auto e3 = world.create_entity();
    world.add_components(e3, Position{3.0F, 0.0F, 0.0F},
                         Velocity{3.0F, 0.0F, 0.0F});

    auto m1 = world.create_entity();
    world.add_components(m1, MoveOnlyComponent(100),
                         Position{1.0F, 0.0F, 0.0F});

    auto m2 = world.create_entity();
    world.add_components(m2, MoveOnlyComponent(200),
                         Position{2.0F, 0.0F, 0.0F});

    int count = 0;
    int sum = 0;
    ecs::Query<MoveOnlyComponent, Position>().each(
        world, [&](MoveOnlyComponent &comp, Position &pos) {
            REQUIRE(comp.data != nullptr,
                    "move-only component data should not be null");
            sum += *comp.data + static_cast<int>(pos.y);
            count++;
        });

    REQUIRE(count == 2, "query should visit 2 move-only entities");
    REQUIRE(sum == 300, "sum of move-only values and y positions should be 300");
}

// ---------------------------------------------------------------------------
// Component removal operations
// ---------------------------------------------------------------------------

void remove_single_component()
{
    ecs::World world;
    ecs::register_component<Position>();
    ecs::register_component<Velocity>();
    ecs::register_component<Health>();

    auto entity = world.create_entity();
    world.add_components(entity, Position{1.0F, 2.0F, 3.0F},
                         Velocity{4.0F, 5.0F, 6.0F});

    auto &pos = world.get_component<Position>(entity);
    REQUIRE(world.has_components<Velocity>(entity),
            "entity should have velocity before removal");

    world.remove_components<Velocity>(entity);
    REQUIRE(!world.has_components<Velocity>(entity),
            "entity should not have velocity after removal");

    pos = world.get_component<Position>(entity);
    REQUIRE(pos.x == 1.0F, "position.x should be unaffected by velocity removal");
    REQUIRE(pos.y == 2.0F, "position.y should be unaffected by velocity removal");
    REQUIRE(pos.z == 3.0F, "position.z should be unaffected by velocity removal");
}

void remove_multiple_components()
{
    ecs::World world;
    ecs::register_component<Position>();
    ecs::register_component<Velocity>();
    ecs::register_component<Health>();

    auto entity = world.create_entity();
    world.add_components(entity, Position{1.0F, 2.0F, 3.0F},
                         Velocity{4.0F, 5.0F, 6.0F}, Health{100.0F, 100.0F});

    world.remove_components<Velocity, Health>(entity);

    auto &pos = world.get_component<Position>(entity);
    REQUIRE(pos.x == 1.0F,
            "position.x should remain after removing other components");
    REQUIRE(!world.has_components<Velocity>(entity), "velocity should be removed");
    REQUIRE(!world.has_components<Health>(entity), "health should be removed");
}

void remove_all_components()
{
    ecs::World world;
    ecs::register_component<Position>();
    ecs::register_component<Velocity>();
    ecs::register_component<Health>();

    auto entity = world.create_entity();
    world.add_components(entity, Position{1.0F, 2.0F, 3.0F},
                         Velocity{4.0F, 5.0F, 6.0F});

    world.remove_components<Position, Velocity>(entity);

    REQUIRE(!world.has_components<Position>(entity), "position should be removed");
    REQUIRE(!world.has_components<Velocity>(entity), "velocity should be removed");
}

void remove_non_existent_component()
{
    ecs::World world;
    ecs::register_component<Position>();
    ecs::register_component<Velocity>();
    ecs::register_component<Health>();

    auto entity = world.create_entity();
    world.add_components(entity, Position{1.0F, 2.0F, 3.0F});

    world.remove_components<Velocity>(entity);

    auto &pos = world.get_component<Position>(entity);
    REQUIRE(pos.x == 1.0F, "position.x should be unaffected by removing a "
                           "component the entity never had");
}

void query_after_component_removal()
{
    ecs::World world;
    ecs::register_component<Position>();
    ecs::register_component<Velocity>();
    ecs::register_component<Health>();

    auto e1 = world.create_entity();
    world.add_components(e1, Position{1.0F, 0.0F, 0.0F},
                         Velocity{1.0F, 0.0F, 0.0F});
    auto e2 = world.create_entity();
    world.add_components(e2, Position{2.0F, 0.0F, 0.0F},
                         Velocity{2.0F, 0.0F, 0.0F});
    auto e3 = world.create_entity();
    world.add_components(e3, Position{3.0F, 0.0F, 0.0F});

    int count_before = 0;
    ecs::Query<Position, Velocity>().each(
        world, [&](Position &, Velocity &) { count_before++; });
    REQUIRE(count_before == 2,
            "2 entities should have both position and velocity before removal");

    world.remove_components<Velocity>(e1);

    int count_after = 0;
    ecs::Query<Position, Velocity>().each(
        world, [&](Position &, Velocity &) { count_after++; });
    REQUIRE(count_after == 1,
            "1 entity should have both position and velocity after removal");

    int pos_count = 0;
    ecs::Query<Position>().each(world, [&](Position &) { pos_count++; });
    REQUIRE(pos_count == 3, "all 3 entities should still have position");
}

// ---------------------------------------------------------------------------
// Archetype transitions with different component types
// ---------------------------------------------------------------------------

void trivial_component_transition_uses_memcpy()
{
    ecs::World world;
    ecs::register_component<TrivialComponent>();
    ecs::register_component<NonTrivialComponent>();

    auto entity = world.create_entity();
    world.add_components(entity, TrivialComponent{42});

    world.add_components(entity, NonTrivialComponent{"test"});

    auto &trivial = world.get_component<TrivialComponent>(entity);
    auto &non_trivial = world.get_component<NonTrivialComponent>(entity);

    REQUIRE(trivial.value == 42,
            "trivial component value should survive archetype transition");
    REQUIRE(non_trivial.name == "test", "non-trivial component name should be set");
    REQUIRE(non_trivial.copy_counter == 0,
            "fresh add should not increment copy counter");
}

void non_trivial_component_transition_uses_move_semantics()
{
    ecs::World world;
    ecs::register_component<TrivialComponent>();
    ecs::register_component<NonTrivialComponent>();

    auto entity = world.create_entity();
    world.add_components(entity, NonTrivialComponent{"original"});

    world.add_components(entity, TrivialComponent{100});

    auto &non_trivial = world.get_component<NonTrivialComponent>(entity);
    REQUIRE(non_trivial.name == "original",
            "non-trivial component name should survive transition");
    REQUIRE(non_trivial.move_counter == 2,
            "non-trivial component should have been moved twice");
}

void multiple_archetype_transitions_preserve_data()
{
    ecs::World world;
    ecs::register_component<TrivialComponent>();
    ecs::register_component<NonTrivialComponent>();

    auto entity = world.create_entity();
    world.add_components(entity, TrivialComponent{1});
    world.add_components(entity, NonTrivialComponent{"step1"});

    world.remove_components<TrivialComponent>(entity);
    world.add_components(entity, TrivialComponent{2});

    auto &trivial = world.get_component<TrivialComponent>(entity);
    auto &non_trivial = world.get_component<NonTrivialComponent>(entity);

    REQUIRE(trivial.value == 2, "trivial component should hold the latest value");
    REQUIRE(non_trivial.name == "step1", "non-trivial component should be unaffected");
    REQUIRE(non_trivial.copy_counter == 0,
            "non-trivial component should not have been copied");
}

// ---------------------------------------------------------------------------
// Component array removal with different types
// ---------------------------------------------------------------------------

void trivial_component_removal()
{
    ecs::World world;
    ecs::register_component<TrivialComponent>();
    ecs::register_component<NonTrivialComponent>();

    auto e1 = world.create_entity();
    auto e2 = world.create_entity();
    auto e3 = world.create_entity();

    world.add_components(e1, TrivialComponent{1});
    world.add_components(e2, TrivialComponent{2});
    world.add_components(e3, TrivialComponent{3});

    world.remove_components<TrivialComponent>(e2);

    REQUIRE(world.has_components<TrivialComponent>(e1),
            "e1 should still have trivial component");
    REQUIRE(!world.has_components<TrivialComponent>(e2),
            "e2 should have had trivial component removed");
    REQUIRE(world.has_components<TrivialComponent>(e3),
            "e3 should still have trivial component");

    REQUIRE(world.get_component<TrivialComponent>(e1).value == 1,
            "e1's trivial component value should be unchanged");
    REQUIRE(world.get_component<TrivialComponent>(e3).value == 3,
            "e3's trivial component value should be unchanged");
}

void non_trivial_component_removal()
{
    ecs::World world;
    ecs::register_component<TrivialComponent>();
    ecs::register_component<NonTrivialComponent>();

    auto e1 = world.create_entity();
    auto e2 = world.create_entity();
    auto e3 = world.create_entity();

    world.add_components(e1, NonTrivialComponent{"first"});
    world.add_components(e2, NonTrivialComponent{"second"});
    world.add_components(e3, NonTrivialComponent{"third"});

    world.remove_components<NonTrivialComponent>(e2);

    REQUIRE(world.has_components<NonTrivialComponent>(e1),
            "e1 should still have non-trivial component");
    REQUIRE(!world.has_components<NonTrivialComponent>(e2),
            "e2 should have had non-trivial component removed");
    REQUIRE(world.has_components<NonTrivialComponent>(e3),
            "e3 should still have non-trivial component");

    REQUIRE(world.get_component<NonTrivialComponent>(e1).name == "first",
            "e1's name should be unchanged");
    REQUIRE(world.get_component<NonTrivialComponent>(e3).name == "third",
            "e3's name should be unchanged");
}

// ---------------------------------------------------------------------------
// Complex archetype transition scenarios
// ---------------------------------------------------------------------------

void add_components_to_entity_with_existing_components()
{
    ecs::World world;
    ecs::register_component<TrivialComponent>();
    ecs::register_component<NonTrivialComponent>();

    auto entity = world.create_entity();
    world.add_components(entity, TrivialComponent{100});

    world.add_components(entity, NonTrivialComponent{"batch"});

    REQUIRE(world.has_components<TrivialComponent>(entity),
            "entity should have trivial component");
    REQUIRE(world.has_components<NonTrivialComponent>(entity),
            "entity should have non-trivial component");

    auto &trivial = world.get_component<TrivialComponent>(entity);
    auto &non_trivial = world.get_component<NonTrivialComponent>(entity);

    REQUIRE(trivial.value == 100, "trivial component value should be 100");
    REQUIRE(non_trivial.name == "batch", "non-trivial component name should be batch");
}

void remove_and_re_add_components()
{
    ecs::World world;
    ecs::register_component<TrivialComponent>();
    ecs::register_component<NonTrivialComponent>();

    auto entity = world.create_entity();
    world.add_components(entity, TrivialComponent{200},
                         NonTrivialComponent{"original"});

    world.remove_components<TrivialComponent>(entity);
    REQUIRE(!world.has_components<TrivialComponent>(entity),
            "trivial component should be removed");
    REQUIRE(world.has_components<NonTrivialComponent>(entity),
            "non-trivial component should remain");

    world.add_components(entity, TrivialComponent{300});

    REQUIRE(world.get_component<TrivialComponent>(entity).value == 300,
            "trivial component should hold the re-added value");
    REQUIRE(world.get_component<NonTrivialComponent>(entity).name == "original",
            "non-trivial component should be unaffected");
}

void stress_test_multiple_entities_with_frequent_transitions()
{
    ecs::World world;
    ecs::register_component<TrivialComponent>();
    ecs::register_component<NonTrivialComponent>();

    const int NUM_ENTITIES = 5;
    std::vector<ecs::EntityId> entities;

    for (int i = 0; i < NUM_ENTITIES; ++i) {
        auto entity = world.create_entity();
        world.add_components(entity, TrivialComponent{i});
        entities.push_back(entity);
    }

    for (auto entity : entities) {
        world.add_components(entity, NonTrivialComponent{"stress"});
    }

    for (int i = 1; i < NUM_ENTITIES; i += 2) {
        world.remove_components<TrivialComponent>(entities[i]);
        world.add_components(entities[i], TrivialComponent{i + 1000});
    }

    for (int i = 0; i < NUM_ENTITIES; ++i) {
        REQUIRE(world.has_components<TrivialComponent>(entities[i]),
                "entity should have trivial component");
        REQUIRE(world.has_components<NonTrivialComponent>(entities[i]),
                "entity should have non-trivial component");

        auto &trivial = world.get_component<TrivialComponent>(entities[i]);
        if (i % 2 == 0) {
            REQUIRE(trivial.value == i,
                    "even-indexed entity should keep its original value");
        } else {
            REQUIRE(trivial.value == i + 1000,
                    "odd-indexed entity should hold its re-added value");
        }
    }
}

// ---------------------------------------------------------------------------
// Entity removal operations
// ---------------------------------------------------------------------------

void remove_entity_with_no_components()
{
    ecs::World world;
    ecs::register_component<Position>();
    ecs::register_component<Velocity>();
    ecs::register_component<Health>();

    auto entity = world.create_entity();

    bool removed = world.remove_entity(entity);
    REQUIRE(removed == true,
            "removing an entity with no components should succeed");
}

void remove_entity_with_single_component()
{
    ecs::World world;
    ecs::register_component<Position>();
    ecs::register_component<Velocity>();
    ecs::register_component<Health>();

    auto entity = world.create_entity();
    world.add_components(entity, Position{1.0F, 2.0F, 3.0F});

    REQUIRE(world.has_components<Position>(entity),
            "entity should have position before removal");

    bool removed = world.remove_entity(entity);
    REQUIRE(removed == true,
            "removing entity with a single component should succeed");
}

void remove_entity_with_multiple_components()
{
    ecs::World world;
    ecs::register_component<Position>();
    ecs::register_component<Velocity>();
    ecs::register_component<Health>();

    auto entity = world.create_entity();
    world.add_components(entity, Position{1.0F, 2.0F, 3.0F},
                         Velocity{4.0F, 5.0F, 6.0F}, Health{100.0F, 100.0F});

    REQUIRE(world.has_components<Position>(entity), "entity should have position");
    REQUIRE(world.has_components<Velocity>(entity), "entity should have velocity");
    REQUIRE(world.has_components<Health>(entity), "entity should have health");

    bool removed = world.remove_entity(entity);
    REQUIRE(removed == true,
            "removing entity with multiple components should succeed");
}

void remove_non_existent_entity()
{
    ecs::World world;
    ecs::register_component<Position>();
    ecs::register_component<Velocity>();
    ecs::register_component<Health>();

    ecs::EntityId fake_entity = 9999;
    bool removed = world.remove_entity(fake_entity);
    REQUIRE(removed == false,
            "removing an entity that was never created should fail");
}

void remove_entity_twice()
{
    ecs::World world;
    ecs::register_component<Position>();
    ecs::register_component<Velocity>();
    ecs::register_component<Health>();

    auto entity = world.create_entity();
    world.add_components(entity, Position{1.0F, 2.0F, 3.0F});

    bool first_removal = world.remove_entity(entity);
    REQUIRE(first_removal == true, "first removal should succeed");

    bool second_removal = world.remove_entity(entity);
    REQUIRE(second_removal == false,
            "second removal of the same entity should fail");
}

void entity_removal_affects_queries()
{
    ecs::World world;
    ecs::register_component<Position>();
    ecs::register_component<Velocity>();
    ecs::register_component<Health>();

    auto e1 = world.create_entity();
    auto e2 = world.create_entity();
    auto e3 = world.create_entity();

    world.add_components(e1, Position{1.0F, 0.0F, 0.0F});
    world.add_components(e2, Position{2.0F, 0.0F, 0.0F});
    world.add_components(e3, Position{3.0F, 0.0F, 0.0F});

    int count_before = 0;
    ecs::Query<Position>().each(world, [&](Position &) { count_before++; });
    REQUIRE(count_before == 3, "3 entities should have position before removal");

    world.remove_entity(e2);

    int count_after = 0;
    ecs::Query<Position>().each(world, [&](Position &) { count_after++; });
    REQUIRE(count_after == 2, "2 entities should have position after removal");
}

void entity_removal_with_mixed_archetypes()
{
    ecs::World world;
    ecs::register_component<Position>();
    ecs::register_component<Velocity>();
    ecs::register_component<Health>();

    auto e1 = world.create_entity(); // Position only
    world.add_components(e1, Position{1.0F, 0.0F, 0.0F});

    auto e2 = world.create_entity(); // Position + Velocity
    world.add_components(e2, Position{2.0F, 0.0F, 0.0F},
                         Velocity{1.0F, 0.0F, 0.0F});

    auto e3 = world.create_entity(); // All three components
    world.add_components(e3, Position{3.0F, 0.0F, 0.0F},
                         Velocity{2.0F, 0.0F, 0.0F}, Health{100.0F, 100.0F});

    int pos_count = 0, vel_count = 0, health_count = 0;
    ecs::Query<Position>().each(world, [&](Position &) { pos_count++; });
    ecs::Query<Velocity>().each(world, [&](Velocity &) { vel_count++; });
    ecs::Query<Health>().each(world, [&](Health &) { health_count++; });

    REQUIRE(pos_count == 3, "3 entities should have position before removal");
    REQUIRE(vel_count == 2, "2 entities should have velocity before removal");
    REQUIRE(health_count == 1, "1 entity should have health before removal");

    world.remove_entity(e2);

    pos_count = 0;
    vel_count = 0;
    health_count = 0;
    ecs::Query<Position>().each(world, [&](Position &) { pos_count++; });
    ecs::Query<Velocity>().each(world, [&](Velocity &) { vel_count++; });
    ecs::Query<Health>().each(world, [&](Health &) { health_count++; });

    REQUIRE(pos_count == 2, "2 entities should have position after removal");
    REQUIRE(vel_count == 1, "1 entity should have velocity after removal");
    REQUIRE(health_count == 1, "1 entity should have health after removal");
}

void entity_removal_preserves_other_entities_component_data()
{
    ecs::World world;
    ecs::register_component<Position>();
    ecs::register_component<Velocity>();
    ecs::register_component<Health>();

    auto e1 = world.create_entity();
    auto e2 = world.create_entity();
    auto e3 = world.create_entity();

    world.add_components(e1, Position{10.0F, 20.0F, 30.0F});
    world.add_components(e2, Position{40.0F, 50.0F, 60.0F});
    world.add_components(e3, Position{70.0F, 80.0F, 90.0F});

    world.remove_entity(e2);

    auto &pos1 = world.get_component<Position>(e1);
    auto &pos3 = world.get_component<Position>(e3);

    REQUIRE(pos1.x == 10.0F, "e1.x should be unaffected by e2's removal");
    REQUIRE(pos1.y == 20.0F, "e1.y should be unaffected by e2's removal");
    REQUIRE(pos1.z == 30.0F, "e1.z should be unaffected by e2's removal");

    REQUIRE(pos3.x == 70.0F, "e3.x should be unaffected by e2's removal");
    REQUIRE(pos3.y == 80.0F, "e3.y should be unaffected by e2's removal");
    REQUIRE(pos3.z == 90.0F, "e3.z should be unaffected by e2's removal");
}

// ---------------------------------------------------------------------------
// Query remove_if operations
// ---------------------------------------------------------------------------

void remove_entities_based_on_single_component_predicate()
{
    ecs::World world;
    ecs::register_component<Position>();
    ecs::register_component<Velocity>();
    ecs::register_component<Health>();

    auto e1 = world.create_entity();
    auto e2 = world.create_entity();
    auto e3 = world.create_entity();
    auto e4 = world.create_entity();

    world.add_components(e1, Position{-5.0F, 0.0F, 0.0F});
    world.add_components(e2, Position{10.0F, 0.0F, 0.0F});
    world.add_components(e3, Position{-2.0F, 0.0F, 0.0F});
    world.add_components(e4, Position{8.0F, 0.0F, 0.0F});

    REQUIRE(ecs::Query<Position>().size(world) == 4, "should start with 4 entities");

    ecs::Query<Position>().remove_if(
        world, [](ecs::EntityId, Position &pos) { return pos.x < 0.0F; });

    REQUIRE(ecs::Query<Position>().size(world) == 2,
            "should have 2 entities remaining");

    ecs::Query<Position>().each(world, [](Position &pos) {
        REQUIRE(pos.x > 0.0F, "remaining entities should have positive x");
    });
}

void remove_entities_based_on_multiple_component_predicate()
{
    ecs::World world;
    ecs::register_component<Position>();
    ecs::register_component<Velocity>();
    ecs::register_component<Health>();

    auto e1 = world.create_entity();
    auto e2 = world.create_entity();
    auto e3 = world.create_entity();
    auto e4 = world.create_entity();

    world.add_components(e1, Position{1.0F, 0.0F, 0.0F}, Velocity{5.0F, 0.0F, 0.0F});
    world.add_components(e2, Position{2.0F, 0.0F, 0.0F}, Velocity{15.0F, 0.0F, 0.0F});
    world.add_components(e3, Position{3.0F, 0.0F, 0.0F}, Velocity{8.0F, 0.0F, 0.0F});
    world.add_components(e4, Position{4.0F, 0.0F, 0.0F}, Velocity{25.0F, 0.0F, 0.0F});

    REQUIRE((ecs::Query<Position, Velocity>().size(world) == 4),
            "should start with 4 entities");

    ecs::Query<Position, Velocity>().remove_if(
        world, [](ecs::EntityId, Position &, Velocity &vel) {
            return vel.vx > 10.0F;
        });

    REQUIRE((ecs::Query<Position, Velocity>().size(world) == 2),
            "should have 2 entities remaining");

    ecs::Query<Position, Velocity>().each(
        world, [](Position &, Velocity &vel) {
            REQUIRE(vel.vx <= 10.0F, "remaining entities should have velocity <= 10");
        });
}

void remove_entities_using_entity_id_in_predicate()
{
    ecs::World world;
    ecs::register_component<Position>();
    ecs::register_component<Velocity>();
    ecs::register_component<Health>();

    auto e1 = world.create_entity();
    auto e2 = world.create_entity();
    auto e3 = world.create_entity();

    world.add_components(e1, Position{1.0F, 0.0F, 0.0F});
    world.add_components(e2, Position{2.0F, 0.0F, 0.0F});
    world.add_components(e3, Position{3.0F, 0.0F, 0.0F});

    ecs::Query<Position>().remove_if(
        world,
        [e2](ecs::EntityId entity, Position &) { return entity == e2; });

    REQUIRE(ecs::Query<Position>().size(world) == 2,
            "should have 2 entities remaining");

    bool found_e1 = false, found_e3 = false;
    ecs::Query<Position>().each(world, [&](Position &, ecs::EntityId entity) {
        if (entity == e1)
            found_e1 = true;
        if (entity == e3)
            found_e3 = true;
        REQUIRE(entity != e2, "e2 should have been removed");
    });
    REQUIRE(found_e1, "e1 should still be present");
    REQUIRE(found_e3, "e3 should still be present");
}

void remove_all_entities_matching_query()
{
    ecs::World world;
    ecs::register_component<Position>();
    ecs::register_component<Velocity>();
    ecs::register_component<Health>();

    auto e1 = world.create_entity();
    auto e2 = world.create_entity();
    auto e3 = world.create_entity();

    world.add_components(e1, Position{1.0F, 0.0F, 0.0F});
    world.add_components(e2, Position{2.0F, 0.0F, 0.0F}, Velocity{1.0F, 0.0F, 0.0F});
    world.add_components(e3, Health{100.0F, 100.0F}); // Different archetype

    ecs::Query<Position>().remove_if(world, [](ecs::EntityId, Position &) {
        return true; // Remove all
    });

    REQUIRE(ecs::Query<Position>().size(world) == 0,
            "no entities should have position remaining");
    REQUIRE(ecs::Query<Health>().size(world) == 1,
            "entity with only health should remain");
}

void remove_no_entities_when_predicate_is_false()
{
    ecs::World world;
    ecs::register_component<Position>();
    ecs::register_component<Velocity>();
    ecs::register_component<Health>();

    auto e1 = world.create_entity();
    auto e2 = world.create_entity();

    world.add_components(e1, Position{1.0F, 0.0F, 0.0F});
    world.add_components(e2, Position{2.0F, 0.0F, 0.0F});

    size_t initial_count = ecs::Query<Position>().size(world);

    ecs::Query<Position>().remove_if(world, [](ecs::EntityId, Position &) {
        return false; // Remove none
    });

    REQUIRE(ecs::Query<Position>().size(world) == initial_count,
            "entity count should be unchanged");
}

void remove_from_empty_query()
{
    ecs::World world;
    ecs::register_component<Position>();
    ecs::register_component<Velocity>();
    ecs::register_component<Health>();

    ecs::Query<Position>().remove_if(
        world, [](ecs::EntityId, Position &) { return true; });

    REQUIRE(ecs::Query<Position>().size(world) == 0,
            "removing from an empty query should not crash and leave 0 entities");
}

void remove_entities_across_multiple_archetypes()
{
    ecs::World world;
    ecs::register_component<Position>();
    ecs::register_component<Velocity>();
    ecs::register_component<Health>();

    auto e1 = world.create_entity(); // Position only
    auto e2 = world.create_entity(); // Position + Velocity
    auto e3 = world.create_entity(); // Position + Health
    auto e4 = world.create_entity(); // Position + Velocity + Health

    world.add_components(e1, Position{1.0F, 0.0F, 0.0F});
    world.add_components(e2, Position{2.0F, 0.0F, 0.0F}, Velocity{1.0F, 0.0F, 0.0F});
    world.add_components(e3, Position{3.0F, 0.0F, 0.0F}, Health{100.0F, 100.0F});
    world.add_components(e4, Position{4.0F, 0.0F, 0.0F}, Velocity{1.0F, 0.0F, 0.0F},
                         Health{100.0F, 100.0F});

    REQUIRE(ecs::Query<Position>().size(world) == 4, "should start with 4 entities");

    ecs::Query<Position>().remove_if(world, [](ecs::EntityId, Position &pos) {
        return static_cast<int>(pos.x) % 2 == 0;
    });

    REQUIRE(ecs::Query<Position>().size(world) == 2,
            "should have 2 entities remaining");

    ecs::Query<Position>().each(world, [](Position &pos) {
        REQUIRE(static_cast<int>(pos.x) % 2 == 1,
                "remaining entities should have odd x position");
    });
}

void remove_non_trivial_components_properly()
{
    ecs::World world;
    ecs::register_component<Position>();
    ecs::register_component<Velocity>();
    ecs::register_component<Health>();
    ecs::register_component<NonTrivialComponent>();

    auto e1 = world.create_entity();
    auto e2 = world.create_entity();
    auto e3 = world.create_entity();
    auto e4 = world.create_entity();

    world.add_components(e1, NonTrivialComponent{"keep1"});
    world.add_components(e2, NonTrivialComponent{"remove"});
    world.add_components(e3, NonTrivialComponent{"keep2"});
    world.add_components(e4, NonTrivialComponent{"remove2"});

    REQUIRE(ecs::Query<NonTrivialComponent>().size(world) == 4,
            "should start with 4 entities");

    ecs::Query<NonTrivialComponent>().remove_if(
        world, [](ecs::EntityId, NonTrivialComponent &comp) {
            return comp.name.find("remove") == 0;
        });

    REQUIRE(ecs::Query<NonTrivialComponent>().size(world) == 2,
            "should have 2 entities remaining");

    ecs::Query<NonTrivialComponent>().each(
        world, [](NonTrivialComponent &comp) {
            REQUIRE(comp.name.find("keep") == 0,
                    "remaining entities should have kept names");
            REQUIRE(comp.move_counter >= 1,
                    "remaining components should show at least one move from "
                    "construction");
        });
}

// ---------------------------------------------------------------------------
// Edge cases and error conditions
// ---------------------------------------------------------------------------

void remove_from_empty_archetype()
{
    ecs::World world;
    ecs::register_component<TrivialComponent>();
    ecs::register_component<NonTrivialComponent>();

    auto entity = world.create_entity();

    world.remove_components<TrivialComponent>(entity);
    world.remove_components<NonTrivialComponent>(entity);

    REQUIRE(!world.has_components<TrivialComponent>(entity),
            "entity should not have trivial component");
    REQUIRE(!world.has_components<NonTrivialComponent>(entity),
            "entity should not have non-trivial component");
}

void component_data_alignment_after_transitions()
{
    ecs::World world;
    ecs::register_component<TrivialComponent>();
    ecs::register_component<NonTrivialComponent>();

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

    REQUIRE(world.get_component<SmallComponent>(entity).c == 'A',
            "small component value should survive transition");
    REQUIRE(world.has_components<LargeComponent>(entity),
            "entity should have large component");
}

} // namespace basic_tests

// =============================================================================
// component_array_tests
// =============================================================================

namespace component_array_tests
{

namespace
{

struct SimpleComponent {
    int value;
    SimpleComponent() = default;
    explicit SimpleComponent(int v) : value(v) {}
};

struct ComplexComponent {
    std::string name;
    size_t copy_count = 0;
    size_t move_count = 0;
    size_t destruct_count = 0;

    ComplexComponent() = default;
    explicit ComplexComponent(std::string n) : name(std::move(n)) {}

    ComplexComponent(const ComplexComponent &other)
        : name(other.name), copy_count(other.copy_count + 1),
          move_count(other.move_count), destruct_count(other.destruct_count)
    {
    }

    ComplexComponent(ComplexComponent &&other) noexcept
        : name(std::move(other.name)), copy_count(other.copy_count),
          move_count(other.move_count + 1), destruct_count(other.destruct_count)
    {
    }

    ComplexComponent &operator=(const ComplexComponent &other)
    {
        name = other.name;
        copy_count = other.copy_count + 1;
        move_count = other.move_count;
        destruct_count = other.destruct_count;
        return *this;
    }

    ComplexComponent &operator=(ComplexComponent &&other) noexcept
    {
        name = std::move(other.name);
        copy_count = other.copy_count;
        move_count = other.move_count + 1;
        destruct_count = other.destruct_count;
        return *this;
    }

    ~ComplexComponent() {}
};

} // namespace

// ---------------------------------------------------------------------------
// ComponentArray basic operations
// ---------------------------------------------------------------------------

void create_component_array_for_simple_type()
{
    ecs::register_component<SimpleComponent>();
    ecs::register_component<ComplexComponent>();

    auto array = ecs::detail::ComponentArray::create<SimpleComponent>();
    REQUIRE(array.size() == 0, "freshly created array should be empty");
}

void create_component_array_for_complex_type()
{
    ecs::register_component<SimpleComponent>();
    ecs::register_component<ComplexComponent>();

    auto array = ecs::detail::ComponentArray::create<ComplexComponent>();
    REQUIRE(array.size() == 0, "freshly created array should be empty");
}

// ---------------------------------------------------------------------------
// ComponentArray add operations
// ---------------------------------------------------------------------------

void add_simple_components()
{
    ecs::register_component<SimpleComponent>();
    ecs::register_component<ComplexComponent>();

    SimpleComponent comp;
    auto array = ecs::detail::ComponentArray::create<SimpleComponent>();

    // Resize to add first component
    array.push(&comp);
    REQUIRE(array.size() == 1, "array should have 1 element after first push");

    // Set value using placement new
    new (array.get_ptr(0)) SimpleComponent(42);
    REQUIRE(array.get<SimpleComponent>(0).value == 42,
            "first element should hold value 42");

    // Resize to add second component
    array.push(&comp);
    REQUIRE(array.size() == 2, "array should have 2 elements after second push");
    new (array.get_ptr(1)) SimpleComponent(100);
    REQUIRE(array.get<SimpleComponent>(1).value == 100,
            "second element should hold value 100");

    // Verify first component unchanged
    REQUIRE(array.get<SimpleComponent>(0).value == 42,
            "first element should still hold value 42");
}

void add_complex_components()
{
    ecs::register_component<SimpleComponent>();
    ecs::register_component<ComplexComponent>();

    auto array = ecs::detail::ComponentArray::create<ComplexComponent>();

    // Resize to add first component
    ComplexComponent complex_comp("first");
    array.push(&complex_comp);
    REQUIRE(array.size() == 1, "array should have 1 element after first push");

    // Construct in place
    REQUIRE(array.get<ComplexComponent>(0).name == "first",
            "first element should be named first");

    // Resize to add second component
    ComplexComponent complex_comp2("second");
    array.push(&complex_comp2);
    REQUIRE(array.size() == 2, "array should have 2 elements after second push");
    REQUIRE(array.get<ComplexComponent>(1).name == "second",
            "second element should be named second");

    // Verify both components
    REQUIRE(array.get<ComplexComponent>(0).name == "first",
            "first element should still be named first");
    REQUIRE(array.get<ComplexComponent>(1).name == "second",
            "second element should still be named second");
}

// ---------------------------------------------------------------------------
// ComponentArray remove operations
// ---------------------------------------------------------------------------

void remove_simple_components()
{
    ecs::register_component<SimpleComponent>();
    ecs::register_component<ComplexComponent>();

    auto array = ecs::detail::ComponentArray::create<SimpleComponent>();

    for (int i = 0; i < 3; ++i) {
        SimpleComponent comp(i * 10);
        array.push(&comp);
    }
    REQUIRE(array.size() == 3, "array should have 3 elements");

    // Values should be [0, 10, 20]
    REQUIRE(array.get<SimpleComponent>(0).value == 0, "element 0 should be 0");
    REQUIRE(array.get<SimpleComponent>(1).value == 10, "element 1 should be 10");
    REQUIRE(array.get<SimpleComponent>(2).value == 20, "element 2 should be 20");

    // Remove middle element (index 1)
    array.remove(1);
    REQUIRE(array.size() == 2, "array should have 2 elements after removal");

    // After removal, element at index 1 should be the last element (20)
    REQUIRE(array.get<SimpleComponent>(0).value == 0,
            "element 0 should be unchanged");
    REQUIRE(array.get<SimpleComponent>(1).value == 20,
            "element 1 should now hold the former last element");
}

void remove_complex_components()
{
    ecs::register_component<SimpleComponent>();
    ecs::register_component<ComplexComponent>();

    auto array = ecs::detail::ComponentArray::create<ComplexComponent>();

    // Resize and add three components
    std::vector<std::string> names = {"first", "second", "third"};

    for (int i = 0; i < 3; ++i) {
        ComplexComponent comp(names[i]);
        array.push(&comp);
    }
    REQUIRE(array.size() == 3, "array should have 3 elements");

    // Verify initial state
    REQUIRE(array.get<ComplexComponent>(0).name == "first", "element 0 should be first");
    REQUIRE(array.get<ComplexComponent>(1).name == "second", "element 1 should be second");
    REQUIRE(array.get<ComplexComponent>(2).name == "third", "element 2 should be third");

    // Remove middle element (index 1)
    array.remove(1);
    REQUIRE(array.size() == 2, "array should have 2 elements after removal");

    // After removal, element at index 1 should be "third" (moved from end)
    REQUIRE(array.get<ComplexComponent>(0).name == "first",
            "element 0 should be unchanged");
    REQUIRE(array.get<ComplexComponent>(1).name == "third",
            "element 1 should now hold the former last element");
}

void remove_last_element()
{
    ecs::register_component<SimpleComponent>();
    ecs::register_component<ComplexComponent>();

    auto array = ecs::detail::ComponentArray::create<ComplexComponent>();

    // Resize and add two components
    ComplexComponent comp1("first");
    array.push(&comp1, true);
    ComplexComponent comp2("second");
    array.push(&comp2, true);

    REQUIRE(array.size() == 2, "array should have 2 elements");

    // Remove last element
    array.remove(1);
    REQUIRE(array.size() == 1, "array should have 1 element after removing the last one");
    REQUIRE(array.get<ComplexComponent>(0).name == "first",
            "remaining element should be first");
}

void remove_single_element()
{
    ecs::register_component<SimpleComponent>();
    ecs::register_component<ComplexComponent>();

    auto array = ecs::detail::ComponentArray::create<ComplexComponent>();

    ComplexComponent comp("only");
    array.push(&comp);
    REQUIRE(array.size() == 1, "array should have 1 element");

    // Remove the only element
    array.remove(0);
    REQUIRE(array.size() == 0, "array should be empty after removing the only element");
}

// ---------------------------------------------------------------------------
// ComponentArray memory management
// ---------------------------------------------------------------------------

void proper_destruction_on_array_destruction()
{
    ecs::register_component<ComplexComponent>();
    ecs::register_component<SimpleComponent>();

    {
        auto array = ecs::detail::ComponentArray::create<ComplexComponent>();

        ComplexComponent comp{"test"};
        // Resize and add several components
        array.push(&comp);
        array.push(&comp);
        array.push(&comp);
        array.push(&comp);
        array.push(&comp);

        REQUIRE(array.size() == 5, "array should have 5 elements");
        // Array destructor should properly destroy all components
    }
    // If we reach here without crash, destruction worked
    REQUIRE(true, "reaching here without crashing means destruction worked");
}

void reserve_capacity()
{
    ecs::register_component<ComplexComponent>();
    ecs::register_component<SimpleComponent>();

    auto array = ecs::detail::ComponentArray::create<SimpleComponent>();

    // Reserve space first
    array.reserve(100);

    SimpleComponent comp{0};
    SimpleComponent comp4{4};
    array.push(&comp);
    array.push(&comp);
    array.push(&comp);
    array.push(&comp);
    array.push(&comp4);
    REQUIRE(array.size() == 5, "array should have 5 elements");

    // Verify components were stored correctly
    REQUIRE(array.get<SimpleComponent>(0).value == 0, "element 0 should be 0");
    REQUIRE(array.get<SimpleComponent>(4).value == 4, "element 4 should be 4");
}

} // namespace component_array_tests

// =============================================================================
// concept_tests (compile-time concept validation)
// =============================================================================

namespace ecs
{
namespace concept_tests
{

namespace
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
    // Should work - basic World types
    static_assert(WorldType<World>);
    static_assert(WorldType<World &>);
    static_assert(WorldType<World &&>);
    static_assert(WorldType<const World>);
    static_assert(WorldType<const World &>);
    static_assert(WorldType<const World &&>);
    static_assert(WorldType<volatile World &>);
    static_assert(WorldType<const volatile World &>);

    // Should fail - wrong types
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
    // Mutable world with any component type should work
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

    // Const world with const component types or pass-by-value should work
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

    // Const world with mutable component types should fail
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
    // Mutable world with any component combinations
    static_assert(ArgsConstCompatible<World, SinglePositionFunc>);
    static_assert(ArgsConstCompatible<World, SingleConstPositionFunc>);
    static_assert(ArgsConstCompatible<World, SinglePositionValueFunc>);
    static_assert(ArgsConstCompatible<World &, SinglePositionFunc>);
    static_assert(ArgsConstCompatible<World &, SingleConstPositionFunc>);
    static_assert(ArgsConstCompatible<World &&, SinglePositionFunc>);
    static_assert(ArgsConstCompatible<World &&, SingleConstPositionFunc>);

    // Const world with const components only
    static_assert(ArgsConstCompatible<const World, SingleConstPositionFunc>);
    static_assert(ArgsConstCompatible<const World &, SingleConstPositionFunc>);
    static_assert(ArgsConstCompatible<const World &&, SingleConstPositionFunc>);

    // Const world with pass-by-value (should work now!)
    static_assert(ArgsConstCompatible<const World, SinglePositionValueFunc>);
    static_assert(ArgsConstCompatible<const World &, SinglePositionValueFunc>);

    // Const world with mutable reference components should fail
    static_assert(!ArgsConstCompatible<const World, SinglePositionFunc>);
    static_assert(!ArgsConstCompatible<const World &, SinglePositionFunc>);
    static_assert(!ArgsConstCompatible<const World &&, SinglePositionFunc>);

    // Test with multiple component functions

    // Mutable world with mixed const/mutable components
    static_assert(ArgsConstCompatible<World, MultiMutableFunc>);
    static_assert(ArgsConstCompatible<World, MultiConstFunc>);
    static_assert(ArgsConstCompatible<World, MixedFunc>);
    static_assert(ArgsConstCompatible<World, MixedValueFunc>);

    // Const world with all const components
    static_assert(ArgsConstCompatible<const World, MultiConstFunc>);

    // Const world with mixed value/const ref (should work now!)
    static_assert(ArgsConstCompatible<const World, MixedValueFunc>);

    // Const world with ANY mutable reference components should fail
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
    // Mutable world lambdas
    auto mutable_lambda = [](Position & /*pos*/, Velocity & /*vel*/) {};
    auto const_lambda = [](const Position & /*pos*/, const Velocity & /*vel*/) {
    };
    auto mixed_lambda = [](Position & /*pos*/, const Velocity & /*vel*/) {};
    auto value_lambda = [](Position /*pos*/, Velocity /*vel*/) {};
    auto mixed_value_lambda = [](Position /*pos*/, const Velocity & /*vel*/) {};

    static_assert(ArgsConstCompatible<World, decltype(mutable_lambda)>);
    static_assert(ArgsConstCompatible<World, decltype(const_lambda)>);
    static_assert(ArgsConstCompatible<World, decltype(mixed_lambda)>);
    static_assert(ArgsConstCompatible<World, decltype(value_lambda)>);
    static_assert(ArgsConstCompatible<World, decltype(mixed_value_lambda)>);

    // Const world with appropriate lambdas
    static_assert(ArgsConstCompatible<const World, decltype(const_lambda)>);
    static_assert(ArgsConstCompatible<const World, decltype(value_lambda)>);
    static_assert(
        ArgsConstCompatible<const World, decltype(mixed_value_lambda)>);

    // Const world with mutable reference lambdas should fail
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

    // Volatile worlds (should work like non-const)
    static_assert(ArgsConstCompatible<volatile World, SinglePositionFunc>);
    static_assert(ArgsConstCompatible<volatile World &, MultiMutableFunc>);

    // Const volatile worlds (should behave like const)
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
    auto rendering_system = [](const Position & /*pos*/,
                               const Health & /*health*/) {
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
    auto mixed_system = [](Position /*pos*/, const Velocity & /*vel*/) {
        // Copy position, reference velocity
    };
    static_assert(ArgsConstCompatible<World, decltype(mixed_system)>);
    static_assert(ArgsConstCompatible<const World,
                                      decltype(mixed_system)>); // Should work!

    // Debug logging (const world, all const)
    auto logging_system = [](const Position & /*pos*/, const Velocity & /*vel*/,
                             const Health & /*health*/) {
        // Log component values
    };
    static_assert(ArgsConstCompatible<World, decltype(logging_system)>);
    static_assert(ArgsConstCompatible<const World, decltype(logging_system)>);
}

// Performance Tests - Ensure Concepts Don't Add Runtime Overhead
void test_compile_time_only()
{
    auto test_func = [](Position & /*pos*/) {};

    constexpr bool test1 = WorldType<World>;
    constexpr bool test2 = ConstCompatible<World, Position &>;
    constexpr bool test3 = ArgsConstCompatible<World, decltype(test_func)>;

    // If these compile as constexpr, the concepts are purely compile-time
    static_assert(test1);
    static_assert(test2);
    static_assert(test3);
}

} // namespace

// The real validation happens at compile-time via static_assert; this just
// ensures each function compiles and can be called.
void run_all()
{
    test_world_type_concept();

    test_const_compatible_concept();

    test_function_const_compatible_concept();

    test_function_types();

    test_with_lambda_functions();

    test_function_pointers();

    test_edge_cases();

    test_realistic_ecs_patterns();

    test_compile_time_only();
}

} // namespace concept_tests
} // namespace ecs

// =============================================================================
// const_correctness_tests
// =============================================================================

namespace const_correctness_tests
{

namespace
{

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

} // namespace

void query_works_with_const_world()
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
    const ecs::World &const_world = world;

    size_t entity_count = 0;
    ecs::Query<Position, Velocity>().each(
        const_world, [&entity_count](const Position &pos, const Velocity &vel) {
            entity_count++;
            REQUIRE(pos.x >= 0.0f, "position.x should be non-negative");
            REQUIRE(vel.dx >= 0.0f, "velocity.dx should be non-negative");
        });

    REQUIRE(entity_count == 3, "query over const world should visit 3 entities");
}

void add_components_accepts_const_component_references()
{
    ecs::register_component<Position>();
    ecs::register_component<Velocity>();
    ecs::register_component<TestData>();

    ecs::World world;

    // Create some const components
    const Position const_pos{1.0f, 2.0f, 3.0f};
    const Velocity const_vel{0.1f, 0.2f, 0.3f};
    const TestData const_data{42, "test"};

    // add_components should accept const references without throwing
    ecs::EntityId entity = world.create_entity();
    world.add_components(entity, const_pos, const_vel, const_data);

    // Verify the components were added correctly
    REQUIRE((world.has_components<Position, Velocity, TestData>(entity)),
            "entity should have all three components");

    auto pos = world.get_component<Position>(entity);
    auto vel = world.get_component<Velocity>(entity);
    auto data = world.get_component<TestData>(entity);

    REQUIRE(pos.x == 1.0f, "position.x should be 1.0");
    REQUIRE(pos.y == 2.0f, "position.y should be 2.0");
    REQUIRE(pos.z == 3.0f, "position.z should be 3.0");
    REQUIRE(vel.dx == 0.1f, "velocity.dx should be 0.1");
    REQUIRE(vel.dy == 0.2f, "velocity.dy should be 0.2");
    REQUIRE(vel.dz == 0.3f, "velocity.dz should be 0.3");
    REQUIRE(data.value == 42, "test data value should be 42");
    REQUIRE(data.name == "test", "test data name should be test");
}

void query_size_works_with_const_world()
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
    const ecs::World &const_world = world;
    auto query = ecs::Query<Position, Velocity>();
    size_t count = query.size(const_world);

    REQUIRE(count == 5, "query size over const world should be 5");
}

} // namespace const_correctness_tests

// =============================================================================
// Test runner
// =============================================================================

int main()
{
    RUN_TEST(basic_tests::entity_creation);
    RUN_TEST(basic_tests::adding_components);
    RUN_TEST(basic_tests::can_add_move_only_component);
    RUN_TEST(basic_tests::multiple_components);
    RUN_TEST(basic_tests::move_only_component_with_other_components);

    RUN_TEST(basic_tests::query_with_single_component);
    RUN_TEST(basic_tests::query_with_multiple_components);
    RUN_TEST(basic_tests::query_with_entity_id);
    RUN_TEST(basic_tests::query_with_move_only_component);

    RUN_TEST(basic_tests::remove_single_component);
    RUN_TEST(basic_tests::remove_multiple_components);
    RUN_TEST(basic_tests::remove_all_components);
    RUN_TEST(basic_tests::remove_non_existent_component);
    RUN_TEST(basic_tests::query_after_component_removal);

    RUN_TEST(basic_tests::trivial_component_transition_uses_memcpy);
    RUN_TEST(basic_tests::non_trivial_component_transition_uses_move_semantics);
    RUN_TEST(basic_tests::multiple_archetype_transitions_preserve_data);

    RUN_TEST(basic_tests::trivial_component_removal);
    RUN_TEST(basic_tests::non_trivial_component_removal);

    RUN_TEST(basic_tests::add_components_to_entity_with_existing_components);
    RUN_TEST(basic_tests::remove_and_re_add_components);
    RUN_TEST(basic_tests::stress_test_multiple_entities_with_frequent_transitions);

    RUN_TEST(basic_tests::remove_entity_with_no_components);
    RUN_TEST(basic_tests::remove_entity_with_single_component);
    RUN_TEST(basic_tests::remove_entity_with_multiple_components);
    RUN_TEST(basic_tests::remove_non_existent_entity);
    RUN_TEST(basic_tests::remove_entity_twice);
    RUN_TEST(basic_tests::entity_removal_affects_queries);
    RUN_TEST(basic_tests::entity_removal_with_mixed_archetypes);
    RUN_TEST(basic_tests::entity_removal_preserves_other_entities_component_data);

    RUN_TEST(basic_tests::remove_entities_based_on_single_component_predicate);
    RUN_TEST(basic_tests::remove_entities_based_on_multiple_component_predicate);
    RUN_TEST(basic_tests::remove_entities_using_entity_id_in_predicate);
    RUN_TEST(basic_tests::remove_all_entities_matching_query);
    RUN_TEST(basic_tests::remove_no_entities_when_predicate_is_false);
    RUN_TEST(basic_tests::remove_from_empty_query);
    RUN_TEST(basic_tests::remove_entities_across_multiple_archetypes);
    RUN_TEST(basic_tests::remove_non_trivial_components_properly);

    RUN_TEST(basic_tests::remove_from_empty_archetype);
    RUN_TEST(basic_tests::component_data_alignment_after_transitions);

    RUN_TEST(component_array_tests::create_component_array_for_simple_type);
    RUN_TEST(component_array_tests::create_component_array_for_complex_type);
    RUN_TEST(component_array_tests::add_simple_components);
    RUN_TEST(component_array_tests::add_complex_components);
    RUN_TEST(component_array_tests::remove_simple_components);
    RUN_TEST(component_array_tests::remove_complex_components);
    RUN_TEST(component_array_tests::remove_last_element);
    RUN_TEST(component_array_tests::remove_single_element);
    RUN_TEST(component_array_tests::proper_destruction_on_array_destruction);
    RUN_TEST(component_array_tests::reserve_capacity);

    RUN_TEST(ecs::concept_tests::run_all);

    RUN_TEST(const_correctness_tests::query_works_with_const_world);
    RUN_TEST(
        const_correctness_tests::add_components_accepts_const_component_references);
    RUN_TEST(const_correctness_tests::query_size_works_with_const_world);

    if (test_detail::expect_failures != 0) {
        std::printf("\n%d expectation failure(s) encountered.\n",
                    test_detail::expect_failures);
        return 1;
    }

    std::printf("\nAll tests passed.\n");
    return 0;
}
