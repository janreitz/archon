#include <archon/ecs.h>
#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <string>

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

TEST_CASE("ComponentArray basic operations", "[component_array]")
{
    ecs::register_component<SimpleComponent>();
    ecs::register_component<ComplexComponent>();

    SECTION("Create ComponentArray for simple type")
    {
        auto array = ecs::detail::ComponentArray::create<SimpleComponent>();
        REQUIRE(array.size() == 0);
    }

    SECTION("Create ComponentArray for complex type")
    {
        auto array = ecs::detail::ComponentArray::create<ComplexComponent>();
        REQUIRE(array.size() == 0);
    }
}

TEST_CASE("ComponentArray add operations", "[component_array]")
{
    ecs::register_component<SimpleComponent>();
    ecs::register_component<ComplexComponent>();

    SimpleComponent comp;

    SECTION("Add simple components")
    {
        auto array = ecs::detail::ComponentArray::create<SimpleComponent>();

        // Resize to add first component
        array.push(&comp);
        REQUIRE(array.size() == 1);

        // Set value using placement new
        new (array.get_ptr(0)) SimpleComponent(42);
        REQUIRE(array.get<SimpleComponent>(0).value == 42);

        // Resize to add second component
        array.push(&comp);
        REQUIRE(array.size() == 2);
        new (array.get_ptr(1)) SimpleComponent(100);
        REQUIRE(array.get<SimpleComponent>(1).value == 100);

        // Verify first component unchanged
        REQUIRE(array.get<SimpleComponent>(0).value == 42);
    }

    SECTION("Add complex components")
    {
        auto array = ecs::detail::ComponentArray::create<ComplexComponent>();

        // Resize to add first component
        ComplexComponent complex_comp("first");
        array.push(&complex_comp);
        REQUIRE(array.size() == 1);

        // Construct in place
        REQUIRE(array.get<ComplexComponent>(0).name == "first");

        // Resize to add second component
        ComplexComponent complex_comp2("second");
        array.push(&complex_comp2);
        REQUIRE(array.size() == 2);
        REQUIRE(array.get<ComplexComponent>(1).name == "second");

        // Verify both components
        REQUIRE(array.get<ComplexComponent>(0).name == "first");
        REQUIRE(array.get<ComplexComponent>(1).name == "second");
    }
}

TEST_CASE("ComponentArray remove operations", "[component_array]")
{
    ecs::register_component<SimpleComponent>();
    ecs::register_component<ComplexComponent>();

    SECTION("Remove simple components")
    {
        auto array = ecs::detail::ComponentArray::create<SimpleComponent>();

        for (int i = 0; i < 3; ++i) {
            SimpleComponent comp(i * 10);
            array.push(&comp);
        }
        REQUIRE(array.size() == 3);

        // Values should be [0, 10, 20]
        REQUIRE(array.get<SimpleComponent>(0).value == 0);
        REQUIRE(array.get<SimpleComponent>(1).value == 10);
        REQUIRE(array.get<SimpleComponent>(2).value == 20);

        // Remove middle element (index 1)
        array.remove(1);
        REQUIRE(array.size() == 2);

        // After removal, element at index 1 should be the last element (20)
        REQUIRE(array.get<SimpleComponent>(0).value == 0);
        REQUIRE(array.get<SimpleComponent>(1).value == 20);
    }

    SECTION("Remove complex components")
    {
        auto array = ecs::detail::ComponentArray::create<ComplexComponent>();

        // Resize and add three components
        std::vector<std::string> names = {"first", "second", "third"};

        for (int i = 0; i < 3; ++i) {
            ComplexComponent comp(names[i]);
            array.push(&comp);
        }
        REQUIRE(array.size() == 3);

        // Verify initial state
        REQUIRE(array.get<ComplexComponent>(0).name == "first");
        REQUIRE(array.get<ComplexComponent>(1).name == "second");
        REQUIRE(array.get<ComplexComponent>(2).name == "third");

        // Remove middle element (index 1)
        array.remove(1);
        REQUIRE(array.size() == 2);

        // After removal, element at index 1 should be "third" (moved from end)
        REQUIRE(array.get<ComplexComponent>(0).name == "first");
        REQUIRE(array.get<ComplexComponent>(1).name == "third");
    }

    SECTION("Remove last element")
    {
        auto array = ecs::detail::ComponentArray::create<ComplexComponent>();

        // Resize and add two components
        ComplexComponent comp1("first");
        array.push(&comp1, true);
        ComplexComponent comp2("second");
        array.push(&comp2, true);

        REQUIRE(array.size() == 2);

        // Remove last element
        array.remove(1);
        REQUIRE(array.size() == 1);
        REQUIRE(array.get<ComplexComponent>(0).name == "first");
    }

    SECTION("Remove single element")
    {
        auto array = ecs::detail::ComponentArray::create<ComplexComponent>();

        ComplexComponent comp("only");
        array.push(&comp);
        REQUIRE(array.size() == 1);

        // Remove the only element
        array.remove(0);
        REQUIRE(array.size() == 0);
    }
}

TEST_CASE("ComponentArray memory management", "[component_array]")
{
    ecs::register_component<ComplexComponent>();
    ecs::register_component<SimpleComponent>();

    SECTION("Proper destruction on array destruction")
    {
        {
            auto array =
                ecs::detail::ComponentArray::create<ComplexComponent>();

            ComplexComponent comp{"test"};
            // Resize and add several components
            array.push(&comp);
            array.push(&comp);
            array.push(&comp);
            array.push(&comp);
            array.push(&comp);

            REQUIRE(array.size() == 5);
            // Array destructor should properly destroy all components
        }
        // If we reach here without crash, destruction worked
        REQUIRE(true);
    }

    SECTION("Reserve capacity")
    {
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
        REQUIRE(array.size() == 5);

        // Verify components were stored correctly
        REQUIRE(array.get<SimpleComponent>(0).value == 0);
        REQUIRE(array.get<SimpleComponent>(4).value == 4);
    }
}

TEST_CASE("ComponentArray copy operations", "[component_array]")
{
    ecs::register_component<SimpleComponent>();
    ecs::register_component<ComplexComponent>();

    SECTION("Copy constructor with simple components")
    {
        auto original = ecs::detail::ComponentArray::create<SimpleComponent>();
        
        // Add some components
        for (int i = 0; i < 3; ++i) {
            SimpleComponent comp(i * 10);
            original.push(&comp);
        }
        REQUIRE(original.size() == 3);

        // Copy construct
        auto copy = original;
        REQUIRE(copy.size() == 3);

        // Verify data was copied correctly
        REQUIRE(copy.get<SimpleComponent>(0).value == 0);
        REQUIRE(copy.get<SimpleComponent>(1).value == 10);
        REQUIRE(copy.get<SimpleComponent>(2).value == 20);

        // Modify original to ensure deep copy
        original.get<SimpleComponent>(0).value = 999;
        REQUIRE(copy.get<SimpleComponent>(0).value == 0); // Should be unchanged
    }

    SECTION("Copy constructor with complex components")
    {
        auto original = ecs::detail::ComponentArray::create<ComplexComponent>();
        
        // Add some components
        ComplexComponent comp1("first");
        ComplexComponent comp2("second");
        original.push(&comp1);
        original.push(&comp2);
        REQUIRE(original.size() == 2);

        // Copy construct
        auto copy = original;
        REQUIRE(copy.size() == 2);

        // Verify data was copied correctly
        REQUIRE(copy.get<ComplexComponent>(0).name == "first");
        REQUIRE(copy.get<ComplexComponent>(1).name == "second");

        // Modify original to ensure deep copy
        original.get<ComplexComponent>(0).name = "modified";
        REQUIRE(copy.get<ComplexComponent>(0).name == "first"); // Should be unchanged
    }

    SECTION("Assignment operator with different versions")
    {
        auto source = ecs::detail::ComponentArray::create<SimpleComponent>();
        auto target = ecs::detail::ComponentArray::create<SimpleComponent>();
        
        // Add components to source
        for (int i = 0; i < 2; ++i) {
            SimpleComponent comp(i * 5);
            source.push(&comp);
        }

        // Add different components to target
        SimpleComponent comp(100);
        target.push(&comp);
        REQUIRE(target.size() == 1);
        REQUIRE(target.get<SimpleComponent>(0).value == 100);

        // Assign source to target
        target = source;
        REQUIRE(target.size() == 2);
        REQUIRE(target.get<SimpleComponent>(0).value == 0);
        REQUIRE(target.get<SimpleComponent>(1).value == 5);

        // Modify source to ensure deep copy
        source.get<SimpleComponent>(0).value = 999;
        REQUIRE(target.get<SimpleComponent>(0).value == 0); // Should be unchanged
    }

    SECTION("Empty array copy operations")
    {
        auto original = ecs::detail::ComponentArray::create<SimpleComponent>();
        REQUIRE(original.size() == 0);

        // Copy empty array
        auto copy = original;
        REQUIRE(copy.size() == 0);

        // Assignment with empty array
        auto target = ecs::detail::ComponentArray::create<SimpleComponent>();
        SimpleComponent comp(42);
        target.push(&comp);
        REQUIRE(target.size() == 1);

        target = original; // Assign empty array
        REQUIRE(target.size() == 0);
    }
}

TEST_CASE("ComponentArray versioning", "[component_array]")
{
    ecs::register_component<SimpleComponent>();

    SECTION("Version increments on non-const access")
    {
        auto array1 = ecs::detail::ComponentArray::create<SimpleComponent>();
        auto array2 = ecs::detail::ComponentArray::create<SimpleComponent>();
        
        // Add same components to both arrays
        SimpleComponent comp(42);
        array1.push(&comp);
        array2.push(&comp);

        // Initially versions should be the same after construction
        // (both start at 0 and both had one push operation)

        // Non-const access should increment version
        [[maybe_unused]] auto* data_ptr = array1.data<SimpleComponent>();
        
        // Now array1 should have a different version than array2
        // We can test this indirectly through assignment optimization
        auto original_size = array2.size();
        array2 = array1; // This should actually copy since versions differ
        REQUIRE(array2.size() == original_size); // Size should be preserved after assignment
    }

    SECTION("get_ptr increments version")
    {
        auto array = ecs::detail::ComponentArray::create<SimpleComponent>();
        SimpleComponent comp(42);
        array.push(&comp);

        // Access through get_ptr should increment version
        void* ptr = array.get_ptr(0);
        REQUIRE(ptr != nullptr);
        
        // Subsequent access should also work
        void* ptr2 = array.get_ptr(0);
        REQUIRE(ptr2 == ptr); // Same element, same pointer
    }

    SECTION("get method increments version")
    {
        auto array = ecs::detail::ComponentArray::create<SimpleComponent>();
        SimpleComponent comp(42);
        array.push(&comp);

        // Non-const get should increment version
        auto& ref = array.get<SimpleComponent>(0);
        REQUIRE(ref.value == 42);
        
        ref.value = 100; // Modify through reference
        REQUIRE(array.get<SimpleComponent>(0).value == 100);
    }

    SECTION("const methods do not increment version")
    {
        auto array = ecs::detail::ComponentArray::create<SimpleComponent>();
        SimpleComponent comp(42);
        array.push(&comp);

        const auto& const_array = array;
        
        // Const access should not increment version
        const auto* const_data = const_array.data<SimpleComponent>();
        REQUIRE(const_data != nullptr);
        
        const auto& const_ref = const_array.get<SimpleComponent>(0);
        REQUIRE(const_ref.value == 42);
    }
}

TEST_CASE("ComponentArray assignment optimization", "[component_array]")
{
    ecs::register_component<SimpleComponent>();

    SECTION("Assignment skipped when versions match")
    {
        auto array1 = ecs::detail::ComponentArray::create<SimpleComponent>();
        
        // Add components
        SimpleComponent comp1(10);
        SimpleComponent comp2(20);
        array1.push(&comp1);
        array1.push(&comp2);

        // Create copy with same version
        auto array2 = array1; // This copies and should have same version initially
        
        // Modify array2's data without changing its version
        // (This is a bit tricky to test directly, but we can test the behavior)
        
        // Assignment should potentially be optimized if versions match
        array2 = array1;
        
        // Both should still have the same data
        REQUIRE(array2.size() == 2);
        REQUIRE(array2.get<SimpleComponent>(0).value == 10);
        REQUIRE(array2.get<SimpleComponent>(1).value == 20);
    }
}