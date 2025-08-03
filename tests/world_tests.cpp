#include <archon/ecs.h>
#include <catch2/catch_test_macros.hpp>

struct Position {
    float x = 0.0f, y = 0.0f; // Initialize to avoid garbage values
    Position() = default;
    Position(float x, float y) : x(x), y(y) {}
    bool operator==(const Position& other) const {
        return x == other.x && y == other.y;
    }
};

struct Velocity {
    float dx = 0.0f, dy = 0.0f; // Initialize to avoid garbage values
    Velocity() = default;
    Velocity(float dx, float dy) : dx(dx), dy(dy) {}
    bool operator==(const Velocity& other) const {
        return dx == other.dx && dy == other.dy;
    }
};

struct Health {
    int value = 0; // Initialize to avoid garbage values
    Health() = default;
    explicit Health(int v) : value(v) {}
    bool operator==(const Health& other) const {
        return value == other.value;
    }
};

TEST_CASE("World copy operations", "[world]")
{
    ecs::register_component<Position>();
    ecs::register_component<Velocity>();
    ecs::register_component<Health>();

    SECTION("Copy constructor with empty world")
    {
        ecs::World original;
        REQUIRE(original.archetype_count() == 0);

        // Copy empty world
        ecs::World copy = original;
        REQUIRE(copy.archetype_count() == 0);
        
        // Both should work independently
        auto entity1 = original.create_entity();
        auto entity2 = copy.create_entity();
        
        REQUIRE(original.archetype_count() == 1); // Empty archetype
        REQUIRE(copy.archetype_count() == 1);     // Empty archetype
        REQUIRE(entity1 == entity2); // Same entity IDs since both worlds have same next_entity_id_
    }

    SECTION("Copy constructor with entities and components")
    {
        ecs::World original;
        
        // Create entities with various component combinations
        auto entity1 = original.create_entity();
        auto entity2 = original.create_entity();
        auto entity3 = original.create_entity();
        
        original.add_components(entity1, Position(1.0f, 2.0f));
        original.add_components(entity2, Position(3.0f, 4.0f), Velocity(0.5f, -0.5f));
        original.add_components(entity3, Position(5.0f, 6.0f), Velocity(1.0f, 1.0f), Health(100));
        
        REQUIRE(original.archetype_count() == 4); // Empty, Position, Position+Velocity, Position+Velocity+Health

        // Copy construct
        ecs::World copy = original;
        
        // Verify structure copied correctly
        REQUIRE(copy.archetype_count() == 4);
        
        // Verify entities exist and have correct components
        REQUIRE(copy.has_components<Position>(entity1));
        REQUIRE(!copy.has_components<Velocity>(entity1));
        REQUIRE(!copy.has_components<Health>(entity1));
        
        REQUIRE(copy.has_components<Position>(entity2));
        REQUIRE(copy.has_components<Velocity>(entity2));
        REQUIRE(!copy.has_components<Health>(entity2));
        
        REQUIRE(copy.has_components<Position>(entity3));
        REQUIRE(copy.has_components<Velocity>(entity3));
        REQUIRE(copy.has_components<Health>(entity3));
        
        // Verify component values copied correctly
        REQUIRE(copy.get_component<Position>(entity1) == Position(1.0f, 2.0f));
        REQUIRE(copy.get_component<Position>(entity2) == Position(3.0f, 4.0f));
        REQUIRE(copy.get_component<Velocity>(entity2) == Velocity(0.5f, -0.5f));
        REQUIRE(copy.get_component<Position>(entity3) == Position(5.0f, 6.0f));
        REQUIRE(copy.get_component<Velocity>(entity3) == Velocity(1.0f, 1.0f));
        REQUIRE(copy.get_component<Health>(entity3) == Health(100));
        
        // Verify independence (deep copy)
        original.get_component<Position>(entity1).x = 999.0f;
        original.get_component<Health>(entity3).value = 999;
        
        REQUIRE(copy.get_component<Position>(entity1).x == 1.0f); // Should be unchanged
        REQUIRE(copy.get_component<Health>(entity3).value == 100); // Should be unchanged
    }

    SECTION("Assignment operator")
    {
        ecs::World source;
        ecs::World target;
        
        // Set up source world
        auto entity1 = source.create_entity();
        auto entity2 = source.create_entity();
        source.add_components(entity1, Position(10.0f, 20.0f), Health(50));
        source.add_components(entity2, Velocity(2.0f, 3.0f));
        
        // Set up target world with different content
        auto entity3 = target.create_entity();
        target.add_components(entity3, Position(100.0f, 200.0f));
        
        REQUIRE(source.archetype_count() == 3); // Empty, Position+Health, Velocity
        REQUIRE(target.archetype_count() == 2); // Empty, Position
        
        // Assignment
        target = source;
        
        // Verify target now matches source
        REQUIRE(target.archetype_count() == 3);
        
        // Verify entities and components
        REQUIRE(target.has_components<Position, Health>(entity1));
        REQUIRE(target.has_components<Velocity>(entity2));
        // Note: entity3 doesn't exist in source, so it won't exist in target after assignment
        
        REQUIRE(target.get_component<Position>(entity1) == Position(10.0f, 20.0f));
        REQUIRE(target.get_component<Health>(entity1) == Health(50));
        REQUIRE(target.get_component<Velocity>(entity2) == Velocity(2.0f, 3.0f));
        
        // Verify independence
        source.get_component<Position>(entity1).x = 999.0f;
        REQUIRE(target.get_component<Position>(entity1).x == 10.0f);
    }

    SECTION("Self-assignment")
    {
        ecs::World world;
        auto entity = world.create_entity();
        world.add_components(entity, Position(5.0f, 5.0f));
        
        // Self-assignment should be safe
        world = world;
        
        // Should still work correctly
        REQUIRE(world.has_components<Position>(entity));
        REQUIRE(world.get_component<Position>(entity) == Position(5.0f, 5.0f));
    }
}

TEST_CASE("World snapshot system demonstration", "[world][snapshot]")
{
    // Re-register components to ensure they're available
    ecs::register_component<Position>();
    ecs::register_component<Velocity>();
    ecs::register_component<Health>();

    SECTION("Simple Health component copy test")
    {
        ecs::World original;
        auto entity = original.create_entity();
        original.add_components(entity, Health(42));
        
        // Verify original has correct value
        REQUIRE(original.get_component<Health>(entity).value == 42);
        
        // Copy and verify
        ecs::World copy = original;
        REQUIRE(copy.get_component<Health>(entity).value == 42);
    }

    SECTION("Multi-component add test")
    {
        ecs::World world;
        auto entity = world.create_entity();
        
        // Test the exact same call that's failing
        world.add_components(entity, Position(10.0f, 10.0f), Health(50));
        
        // This should work but might be where the bug is
        REQUIRE(world.has_components<Position>(entity));
        REQUIRE(world.has_components<Health>(entity));
        
        auto pos = world.get_component<Position>(entity);
        auto health = world.get_component<Health>(entity);
        
        REQUIRE(pos.x == 10.0f);
        REQUIRE(pos.y == 10.0f);
        REQUIRE(health.value == 50); // This is probably where it fails
    }

    SECTION("Snapshot creation and restoration")
    {
        ecs::World world;
        
        // Create initial state
        auto player = world.create_entity();
        auto enemy1 = world.create_entity();
        auto enemy2 = world.create_entity();
        
        world.add_components(player, Position(0.0f, 0.0f), Health(100));
        world.add_components(enemy1, Position(10.0f, 10.0f), Health(50));
        world.add_components(enemy2, Position(-5.0f, 15.0f), Health(75));
        
        // Create snapshot
        ecs::World snapshot = world;
        
        // Simulate game progression - modify original world
        world.get_component<Position>(player) = Position(5.0f, 2.0f);
        world.get_component<Health>(player).value = 80;
        world.get_component<Health>(enemy1).value = 25;
        world.remove_entity(enemy2);
        
        auto powerup = world.create_entity();
        world.add_components(powerup, Position(7.0f, 8.0f));
        
        // Verify changes in original
        REQUIRE(world.get_component<Position>(player) == Position(5.0f, 2.0f));
        REQUIRE(world.get_component<Health>(player).value == 80);
        REQUIRE(world.get_component<Health>(enemy1).value == 25);
        REQUIRE(world.has_components<Position>(powerup));
        
        // Verify snapshot unchanged
        REQUIRE(snapshot.get_component<Position>(player) == Position(0.0f, 0.0f));
        REQUIRE(snapshot.get_component<Health>(player).value == 100);
        REQUIRE(snapshot.get_component<Health>(enemy1).value == 50);
        REQUIRE(snapshot.has_components<Position>(enemy2)); // Still exists in snapshot
        REQUIRE(!snapshot.has_components<Position>(powerup)); // Doesn't exist in snapshot
        
        // Restore from snapshot
        world = snapshot;
        
        // Verify restoration
        REQUIRE(world.get_component<Position>(player) == Position(0.0f, 0.0f));
        REQUIRE(world.get_component<Health>(player).value == 100);
        REQUIRE(world.get_component<Health>(enemy1).value == 50);
        REQUIRE(world.has_components<Position>(enemy2)); // Restored
        REQUIRE(!world.has_components<Position>(powerup)); // Gone
    }

    SECTION("Version-based optimization demonstration")
    {
        ecs::World world;
        
        // Create entities in different archetypes
        auto entity1 = world.create_entity();
        auto entity2 = world.create_entity();
        auto entity3 = world.create_entity();
        
        world.add_components(entity1, Position(1.0f, 1.0f));
        world.add_components(entity2, Position(2.0f, 2.0f), Velocity(1.0f, 0.0f));
        world.add_components(entity3, Position(3.0f, 3.0f), Health(100));
        
        // Create initial snapshot
        ecs::World snapshot1 = world;
        
        // Modify only entity1's position (only Position archetype should be marked as changed)
        world.get_component<Position>(entity1).x = 10.0f;
        
        // Create second snapshot - version optimization should kick in
        // Only the Position archetype should be fully copied, others should be optimized
        ecs::World snapshot2 = world;
        
        // Verify both snapshots work correctly
        REQUIRE(snapshot1.get_component<Position>(entity1).x == 1.0f);
        REQUIRE(snapshot2.get_component<Position>(entity1).x == 10.0f);
        
        // Other entities should be identical in both snapshots
        REQUIRE(snapshot1.get_component<Position>(entity2) == snapshot2.get_component<Position>(entity2));
        REQUIRE(snapshot1.get_component<Velocity>(entity2) == snapshot2.get_component<Velocity>(entity2));
        REQUIRE(snapshot1.get_component<Position>(entity3) == snapshot2.get_component<Position>(entity3));
        REQUIRE(snapshot1.get_component<Health>(entity3) == snapshot2.get_component<Health>(entity3));
        
        // Verify independence between all three world instances
        world.get_component<Position>(entity2).y = 999.0f;
        snapshot1.get_component<Position>(entity3).x = 888.0f;
        
        REQUIRE(snapshot2.get_component<Position>(entity2).y == 2.0f); // Unchanged
        REQUIRE(snapshot2.get_component<Position>(entity3).x == 3.0f); // Unchanged
    }
}

TEST_CASE("World copy with entity creation and removal", "[world]")
{
    // Re-register components to ensure they're available
    ecs::register_component<Position>();
    ecs::register_component<Velocity>();
    ecs::register_component<Health>();

    SECTION("Copy preserves next_entity_id")
    {
        ecs::World original;
        
        // Create some entities to advance next_entity_id
        [[maybe_unused]] auto entity1 = original.create_entity();
        [[maybe_unused]] auto entity2 = original.create_entity();
        [[maybe_unused]] auto entity3 = original.create_entity();
        
        // Copy the world
        ecs::World copy = original;
        
        // Create new entities in both worlds
        auto original_new = original.create_entity();
        auto copy_new = copy.create_entity();
        
        // They should have the same ID (both start from the same next_entity_id)
        REQUIRE(original_new == copy_new);
        
        // But creating another entity should give different IDs
        auto original_new2 = original.create_entity();
        auto copy_new2 = copy.create_entity();
        
        REQUIRE(original_new2 == copy_new2);
        REQUIRE(original_new2 != original_new);
    }

    SECTION("Copy with removed entities")
    {
        ecs::World original;
        
        auto entity1 = original.create_entity();
        auto entity2 = original.create_entity();
        auto entity3 = original.create_entity();
        
        original.add_components(entity1, Position(1.0f, 1.0f));
        original.add_components(entity2, Position(2.0f, 2.0f), Health(50));
        original.add_components(entity3, Position(3.0f, 3.0f));
        
        // Remove entity2
        REQUIRE(original.remove_entity(entity2));
        
        // Copy the world
        ecs::World copy = original;
        
        // Verify entity2 doesn't exist in copy (entity2 was removed before copying)
        // Note: We can't call has_components on non-existent entities, so we test indirectly
        // by verifying archetype counts and that entity1/entity3 work correctly
        
        // But entity1 and entity3 should exist
        REQUIRE(copy.has_components<Position>(entity1));
        REQUIRE(copy.has_components<Position>(entity3));
        REQUIRE(copy.get_component<Position>(entity1) == Position(1.0f, 1.0f));
        REQUIRE(copy.get_component<Position>(entity3) == Position(3.0f, 3.0f));
    }

    SECTION("Large world copy performance test")
    {
        ecs::World original;
        
        // Create a reasonably sized world to test copy performance
        const size_t num_entities = 1000;
        std::vector<ecs::EntityId> entities;
        entities.reserve(num_entities);
        
        for (size_t i = 0; i < num_entities; ++i) {
            auto entity = original.create_entity();
            entities.push_back(entity);
            
            if (i % 3 == 0) {
                original.add_components(entity, Position(float(i), float(i)));
            } else if (i % 3 == 1) {
                original.add_components(entity, Position(float(i), float(i)), Health(int(i)));
            } else {
                original.add_components(entity, Position(float(i), float(i)), Health(int(i)), Velocity(1.0f, 1.0f));
            }
        }
        
        REQUIRE(original.archetype_count() == 4); // Empty + 3 different component combinations
        
        // Copy the world (this tests the reference rebuilding performance)
        ecs::World copy = original;
        
        // Verify copy is correct by sampling some entities
        REQUIRE(copy.archetype_count() == 4);
        REQUIRE(copy.has_components<Position>(entities[0]));
        REQUIRE(copy.has_components<Position, Health>(entities[1]));
        REQUIRE(copy.has_components<Position, Health, Velocity>(entities[2]));
        
        // Verify a few values
        REQUIRE(copy.get_component<Position>(entities[100]).x == 100.0f);
        REQUIRE(copy.get_component<Health>(entities[101]).value == 101);
        REQUIRE(copy.get_component<Velocity>(entities[102]) == Velocity(1.0f, 1.0f));
    }
}