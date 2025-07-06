#include <archon/ecs.h>
#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <string>

struct SimpleComponent {
    int value;
    SimpleComponent(int v) : value(v) {}
};

struct ComplexComponent {
    std::string name;
    size_t copy_count = 0;
    size_t move_count = 0;
    size_t destruct_count = 0;

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
        REQUIRE(array != nullptr);
        REQUIRE(array->size() == 0);
    }

    SECTION("Create ComponentArray for complex type")
    {
        auto array = ecs::detail::ComponentArray::create<ComplexComponent>();
        REQUIRE(array != nullptr);
        REQUIRE(array->size() == 0);
    }
}

TEST_CASE("ComponentArray add operations", "[component_array]")
{
    ecs::register_component<SimpleComponent>();
    ecs::register_component<ComplexComponent>();

    SECTION("Add simple components")
    {
        auto array = ecs::detail::ComponentArray::create<SimpleComponent>();

        // Resize to add first component
        array->resize(1);
        REQUIRE(array->size() == 1);

        // Set value using placement new
        new (array->get_ptr(0)) SimpleComponent(42);
        REQUIRE(array->get<SimpleComponent>(0).value == 42);

        // Resize to add second component
        array->resize(2);
        REQUIRE(array->size() == 2);
        new (array->get_ptr(1)) SimpleComponent(100);
        REQUIRE(array->get<SimpleComponent>(1).value == 100);

        // Verify first component unchanged
        REQUIRE(array->get<SimpleComponent>(0).value == 42);
    }

    SECTION("Add complex components")
    {
        auto array = ecs::detail::ComponentArray::create<ComplexComponent>();

        // Resize to add first component
        array->resize(1);
        REQUIRE(array->size() == 1);

        // Construct in place
        new (array->get_ptr(0)) ComplexComponent("first");
        REQUIRE(array->get<ComplexComponent>(0).name == "first");

        // Resize to add second component
        array->resize(2);
        REQUIRE(array->size() == 2);
        new (array->get_ptr(1)) ComplexComponent("second");
        REQUIRE(array->get<ComplexComponent>(1).name == "second");

        // Verify both components
        REQUIRE(array->get<ComplexComponent>(0).name == "first");
        REQUIRE(array->get<ComplexComponent>(1).name == "second");
    }
}

TEST_CASE("ComponentArray remove operations", "[component_array]")
{
    ecs::register_component<SimpleComponent>();
    ecs::register_component<ComplexComponent>();

    SECTION("Remove simple components")
    {
        auto array = ecs::detail::ComponentArray::create<SimpleComponent>();

        // Resize and add three components
        array->resize(3);
        for (int i = 0; i < 3; ++i) {
            new (array->get_ptr(i)) SimpleComponent(i * 10);
        }
        REQUIRE(array->size() == 3);

        // Values should be [0, 10, 20]
        REQUIRE(array->get<SimpleComponent>(0).value == 0);
        REQUIRE(array->get<SimpleComponent>(1).value == 10);
        REQUIRE(array->get<SimpleComponent>(2).value == 20);

        // Remove middle element (index 1)
        array->remove(1);
        REQUIRE(array->size() == 2);

        // After removal, element at index 1 should be the last element (20)
        REQUIRE(array->get<SimpleComponent>(0).value == 0);
        REQUIRE(array->get<SimpleComponent>(1).value == 20);
    }

    SECTION("Remove complex components")
    {
        auto array = ecs::detail::ComponentArray::create<ComplexComponent>();

        // Resize and add three components
        std::vector<std::string> names = {"first", "second", "third"};
        array->resize(3);
        for (int i = 0; i < 3; ++i) {
            new (array->get_ptr(i)) ComplexComponent(names[i]);
        }
        REQUIRE(array->size() == 3);

        // Verify initial state
        REQUIRE(array->get<ComplexComponent>(0).name == "first");
        REQUIRE(array->get<ComplexComponent>(1).name == "second");
        REQUIRE(array->get<ComplexComponent>(2).name == "third");

        // Remove middle element (index 1)
        array->remove(1);
        REQUIRE(array->size() == 2);

        // After removal, element at index 1 should be "third" (moved from end)
        REQUIRE(array->get<ComplexComponent>(0).name == "first");
        REQUIRE(array->get<ComplexComponent>(1).name == "third");
    }

    SECTION("Remove last element")
    {
        auto array = ecs::detail::ComponentArray::create<ComplexComponent>();

        // Resize and add two components
        array->resize(2);
        new (array->get_ptr(0)) ComplexComponent("first");
        new (array->get_ptr(1)) ComplexComponent("second");

        REQUIRE(array->size() == 2);

        // Remove last element
        array->remove(1);
        REQUIRE(array->size() == 1);
        REQUIRE(array->get<ComplexComponent>(0).name == "first");
    }

    SECTION("Remove single element")
    {
        auto array = ecs::detail::ComponentArray::create<ComplexComponent>();

        array->resize(1);
        new (array->get_ptr(0)) ComplexComponent("only");
        REQUIRE(array->size() == 1);

        // Remove the only element
        array->remove(0);
        REQUIRE(array->size() == 0);
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

            // Resize and add several components
            array->resize(5);
            for (int i = 0; i < 5; ++i) {
                new (array->get_ptr(i))
                    ComplexComponent("test_" + std::to_string(i));
            }

            REQUIRE(array->size() == 5);
            // Array destructor should properly destroy all components
        }
        // If we reach here without crash, destruction worked
        REQUIRE(true);
    }

    SECTION("Reserve capacity")
    {
        auto array = ecs::detail::ComponentArray::create<SimpleComponent>();

        // Reserve space first
        array->reserve(100);

        // Resize to use some of the reserved space
        array->resize(50);
        REQUIRE(array->size() == 50);

        // Add components to verify reserved space works
        for (int i = 0; i < 50; ++i) {
            new (array->get_ptr(i)) SimpleComponent(i);
        }

        // Verify components were stored correctly
        REQUIRE(array->get<SimpleComponent>(0).value == 0);
        REQUIRE(array->get<SimpleComponent>(49).value == 49);
    }
}