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

    SimpleComponent comp;

    SECTION("Add simple components")
    {
        auto array = ecs::detail::ComponentArray::create<SimpleComponent>();

        // Resize to add first component
        array->push_copy(&comp);
        REQUIRE(array->size() == 1);

        // Set value using placement new
        new (array->get_ptr(0)) SimpleComponent(42);
        REQUIRE(array->get<SimpleComponent>(0).value == 42);

        // Resize to add second component
        array->push_copy(&comp);
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
        ComplexComponent complex_comp("first");
        array->push_copy(&complex_comp);
        REQUIRE(array->size() == 1);

        // Construct in place
        REQUIRE(array->get<ComplexComponent>(0).name == "first");

        // Resize to add second component
        ComplexComponent complex_comp2("second");
        array->push_copy(&complex_comp2);
        REQUIRE(array->size() == 2);
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

        for (int i = 0; i < 3; ++i) {
            SimpleComponent comp(i * 10);
            array->push_copy(&comp);
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

        for (int i = 0; i < 3; ++i) {
            ComplexComponent comp(names[i]);
            array->push_copy(&comp);
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
        ComplexComponent comp1("first");
        array->push_move(&comp1);
        ComplexComponent comp2("second");
        array->push_move(&comp2);

        REQUIRE(array->size() == 2);

        // Remove last element
        array->remove(1);
        REQUIRE(array->size() == 1);
        REQUIRE(array->get<ComplexComponent>(0).name == "first");
    }

    SECTION("Remove single element")
    {
        auto array = ecs::detail::ComponentArray::create<ComplexComponent>();

        ComplexComponent comp("only");
        array->push_from(&comp);
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

            ComplexComponent comp{"test"};
            // Resize and add several components
            array->push_copy(&comp);
            array->push_copy(&comp);
            array->push_copy(&comp);
            array->push_copy(&comp);
            array->push_copy(&comp);

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

        SimpleComponent comp{0};
        SimpleComponent comp4{4};
        array->push_copy(&comp);
        array->push_copy(&comp);
        array->push_copy(&comp);
        array->push_copy(&comp);
        array->push_copy(&comp4);
        REQUIRE(array->size() == 5);

        // Verify components were stored correctly
        REQUIRE(array->get<SimpleComponent>(0).value == 0);
        REQUIRE(array->get<SimpleComponent>(4).value == 4);
    }
}