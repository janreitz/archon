#pragma once

#include <bitset>
#include <cstdint>
#include <cstring> // std::memcpy
#include <functional>
#include <memory>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace ecs
{
using EntityId = uint32_t;
constexpr size_t MAX_COMPONENTS = 32; // TODO find a way for the user to set
                                      // this

template <typename T> void register_component();

namespace detail
{

using ComponentMask = std::bitset<MAX_COMPONENTS>;
// This is the index into the ComponentMask
// It's type is selected to be the smallest type
// that can represent MAX_COMPONENTS
using ComponentTypeId = decltype([]() {
    constexpr auto bits_needed = std::bit_width(MAX_COMPONENTS);

    static_assert(bits_needed <= 64, "MAX_COMPONENT must fit within 64 bits");

    if constexpr (bits_needed <= 8) {
        return std::uint8_t{};
    } else if constexpr (bits_needed <= 16) {
        return std::uint16_t{};
    } else if constexpr (bits_needed <= 32) {
        return std::uint32_t{};
    } else {
        return std::uint64_t{};
    }
}());

class ComponentArray;
class Archetype;

struct ComponentTypeInfo {
    using CreateArrayFn = ComponentArray (*)();
    using DefaultConstructorFn = void (*)(void *dst);
    using DestructorFn = void (*)(void *);
    using CopyConstructorFn = void (*)(void *dst, const void *src);
    using MoveConstructorFn = void (*)(void *dst, void *src);

    CreateArrayFn create_array;
    DefaultConstructorFn default_constructor;
    DestructorFn destructor;
    CopyConstructorFn copy_constructor;
    MoveConstructorFn move_constructor;

    size_t component_size;
    std::string_view type_name;
    bool is_trivially_copyable;
    bool is_nothrow_move_constructible;
    bool is_trivially_destructible;
};

template <typename F> struct signature_extractor {
};

// Specialization - pattern matches specific type shapes
template <typename R, typename... Args>
struct signature_extractor<std::function<R(Args...)>> {
    using argument_types = std::tuple<Args...>;
    using decayed_argument_types = std::tuple<std::decay_t<Args>...>;
    static constexpr size_t argument_count = sizeof...(Args);
};

// Normalize function signature via std::function CTAD
template <typename F>
struct function_traits
    : signature_extractor<decltype(std::function{std::declval<F>()})> {
};

template <typename T> struct has_extra_param : std::false_type {
};

template <typename Func, typename... ValueComps>
struct has_extra_param<std::tuple<Func, ValueComps...>>
    : std::bool_constant<function_traits<Func>::argument_count ==
                         (sizeof...(ValueComps) + 1)> {
};

} // namespace detail

class World;

template <typename T>
concept WorldType = std::is_same_v<std::remove_cvref_t<T>, World>;

template <typename WorldT, typename ArgumentT>
// remove_reference_t is required, because references are never const, only the
// referred to type
concept ConstCompatible =
    // Pass by value always works (you get a copy)
    !std::is_reference_v<ArgumentT> ||
    // OR: if it's a reference, then const world requires const component
    (!std::is_const_v<std::remove_reference_t<WorldT>> ||
     std::is_const_v<std::remove_reference_t<ArgumentT>>);

template <typename WorldT, typename Func>
concept ArgsConstCompatible =
    // Function arguments as parameter pack using template type deduction of...
    []<typename... Arguments>(std::tuple<Arguments...> *) {
        return (ConstCompatible<WorldT, Arguments> && ...);
        // ...the input type
    }(static_cast<typename detail::function_traits<Func>::argument_types *>(
        nullptr));

template <typename... QueryComponents> class Query
{
  public:
    Query();

    template <typename... WithComponents> Query &with();

    template <typename... ExcludeComponents> Query &without();

    template <WorldType WorldT, typename Func>
    requires ArgsConstCompatible<WorldT, Func>
    void each(WorldT &&world, Func &&func) const;

    template <typename Func>
    void each_archetype(Func &&func, World &world) const;

    /// @brief Clear all entities that match the query
    void clear(World &world);
    [[nodiscard]] size_t size(const World &world) const;

  private:
    bool matches(detail::ComponentMask mask) const;

    template <WorldType WorldT, typename Func>
    void for_each_matching_archetype(WorldT &&world, Func &&func) const;

    detail::ComponentMask include_mask;
    detail::ComponentMask exclude_mask;
    std::array<detail::ComponentTypeId, sizeof...(QueryComponents)>
        component_type_ids;
};

class World
{
    // Make Query a friend so it can access archetypes
    template <typename... T> friend class Query;

  public:
    EntityId create_entity();

    template <typename... Components>
    void add_components(EntityId entity, Components &&...component);

    template <typename... Components> void remove_components(EntityId entity);

    template <typename Component> Component &get_component(EntityId entity);
    template <typename Component>
    const Component &get_component(EntityId entity) const;

    template <typename... Components>
    std::tuple<Components &...> get_components(EntityId entity);
    template <typename... Components>
    std::tuple<const Components &...> get_components(EntityId entity) const;

    template <typename... Components>
    bool has_components(EntityId entity) const;

  private:
    detail::Archetype &
    get_or_create_archetype(const detail::ComponentMask &mask);

    std::unordered_map<detail::ComponentMask, detail::Archetype>
        component_mask_to_archetypes_;
    std::unordered_map<EntityId, std::reference_wrapper<detail::Archetype>>
        entity_to_archetype_;
    EntityId next_entity_id_ = 0;
};

} // namespace ecs

// Include template implementations
#include "ecs.impl.h"