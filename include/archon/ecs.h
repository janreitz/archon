#pragma once

#include <array>
#include <bitset>
#include <cassert>
#include <cstdint>
#include <cstring> // std::memcpy
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace ecs
{
using EntityId = uint32_t;
constexpr size_t MAX_COMPONENTS = 32;
using ComponentMask = std::bitset<MAX_COMPONENTS>;
// This is the index into the ComponentMask
// It's type is selected to be the smallest type
// that can represent MAX_COMPONENTS
using MetaComponentId = decltype([]() {
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

// Forward declaration
class ComponentArray;

struct MetaComponentArray {
    using CreateArrayFn = std::unique_ptr<ComponentArray> (*)();
    using CopyComponentFn = void (*)(void *dst, void *src);
    using MoveComponentFn = void (*)(void *dst, void *src);

    CreateArrayFn create_array;
    CopyComponentFn copy_component;
    MoveComponentFn move_component;
    CopyComponentFn copy_construct;
    MoveComponentFn move_construct;
    void (*destroy_component)(void *ptr);
    size_t component_size;
    std::string_view type_name;
    bool is_trivially_copy_assignable_;
    bool is_nothrow_move_assignable_;
    bool is_trivially_destructible_;
};

class ComponentArray
{
  public:
    template <typename T> static std::unique_ptr<ComponentArray> create();

    [[nodiscard]] size_t size() const;
    void reserve(size_t size);
    void resize(size_t new_size);
    void clear();
    void remove(size_t idx);
    void *get_ptr(size_t index);
    template <typename T> T *data()
    {
        return reinterpret_cast<T *>(data_.data());
    }
    template <typename T> const T *data() const
    {
        return reinterpret_cast<const T *>(data_.data());
    }
    template <typename T> T &get(size_t index) { return data<T>()[index]; }
    template <typename T> const T &get(size_t index) const
    {
        return data<const T>()[index];
    }

  private:
    ComponentArray(MetaComponentId meta_id, const MetaComponentArray &meta);
    MetaComponentId meta_id_;
    MetaComponentArray meta_;
    std::vector<uint8_t> data_;
};

class ComponentRegistry
{
  public:
    static ComponentRegistry &instance();

    template <typename T> void register_component()
    {
        const auto type_idx = std::type_index(typeid(std::decay_t<T>));
        if (component_ids.contains(type_idx)) {
            // Component already registered.
            return;
        }

        const MetaComponentId meta_id = next_id++;
        component_ids.insert({type_idx, meta_id});

        MetaComponentArray meta_array{
            .create_array = []() -> std::unique_ptr<ComponentArray> {
                return ComponentArray::create<T>();
            },
            .copy_component =
                [](void *dst, void *src) {
                    *static_cast<T *>(dst) = *static_cast<T *>(src);
                },
            .move_component =
                [](void *dst, void *src) {
                    *static_cast<T *>(dst) = std::move(*static_cast<T *>(src));
                },
            .copy_construct =
                [](void *dst, void *src) {
                    new (dst) T(*static_cast<T *>(src));
                },
            .move_construct =
                [](void *dst, void *src) {
                    new (dst) T(std::move(*static_cast<T *>(src)));
                },
            .destroy_component = [](void *ptr) { static_cast<T *>(ptr)->~T(); },
            .component_size = sizeof(T),
            .type_name = typeid(T).name(),
            .is_trivially_copy_assignable_ =
                std::is_trivially_copy_assignable_v<T>,
            .is_nothrow_move_assignable_ = std::is_nothrow_move_assignable_v<T>,
            .is_trivially_destructible_ = std::is_trivially_destructible_v<T>};

        meta_data.push_back(meta_array);
    }

    template <typename T> MetaComponentId get_meta_id() const
    {
        return get_meta_id(typeid(std::decay_t<T>));
    }

    MetaComponentId get_meta_id(std::type_index type_idx) const;

    const MetaComponentArray &get_meta(MetaComponentId component_id) const;

  private:
    std::vector<MetaComponentArray> meta_data;
    std::unordered_map<std::type_index, MetaComponentId> component_ids;
    MetaComponentId next_id = 0;
};

class Archetype
{
  public:
    explicit Archetype(const ComponentMask &mask);

    std::unordered_map<EntityId, size_t> entities_to_idx;
    std::vector<EntityId> idx_to_entity;
    std::unordered_map<MetaComponentId, std::unique_ptr<ComponentArray>>
        components;
    const ComponentMask mask_;

    template <typename T> T *data();
    template <typename T> T &get_component(size_t index);
    template <typename T> T &get_component(EntityId entity);
    template <typename... Components>
    std::tuple<Components &...> get_components(EntityId entity);
    /// @brief
    /// @param entity
    /// @return ComponentArray index of the new entity
    size_t add_entity(EntityId entity);
    void remove_entity(EntityId entity);
    void clear_entities();

    // Create archetype with additional component
    std::unique_ptr<Archetype>
    with_component(const MetaComponentId &new_comp_id) const;

    // Create archetype without specific component
    std::unique_ptr<Archetype>
    without_component(const MetaComponentId &remove_comp_id) const;
};

// Helper to get parameter types of a function
template <typename T> struct function_traits;

// For regular functions
template <typename R, typename... Args> struct function_traits<R(Args...)> {
    using argument_types = std::tuple<Args...>;
    using decayed_argument_types = std::tuple<std::decay_t<Args>...>;
    static constexpr size_t argument_count = sizeof...(Args);
};

// For function pointers
template <typename R, typename... Args>
struct function_traits<R (*)(Args...)> : function_traits<R(Args...)> {
};

template <typename R, typename... Args>
struct function_traits<R (&)(Args...)> : function_traits<R(Args...)> {
};

// For member functions
template <typename R, typename C, typename... Args>
struct function_traits<R (C::*)(Args...)> : function_traits<R(Args...)> {
};

// For const member functions
template <typename R, typename C, typename... Args>
struct function_traits<R (C::*)(Args...) const> : function_traits<R(Args...)> {
};

// For lambdas and functors
template <typename T>
struct function_traits
    : function_traits<decltype(&std::remove_reference_t<T>::operator())> {
};

template <typename T> struct has_extra_param : std::false_type {
};

template <typename... ValueComps> struct component_pack {
    static constexpr size_t size = sizeof...(ValueComps);
};

template <typename Func, typename... ValueComps>
struct has_extra_param<std::tuple<Func, ValueComps...>>
    : std::bool_constant<function_traits<Func>::argument_count ==
                         (sizeof...(ValueComps) + 1)> {
};

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
    []<typename... Arguments>(std::tuple<Arguments...> *) {
        return (ConstCompatible<WorldT, Arguments> && ...);
    }(static_cast<typename function_traits<Func>::argument_types *>(nullptr));

template <typename... QueryComponents> class Query
{
    ComponentMask include_mask;
    ComponentMask exclude_mask;

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
};

class World
{
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

    template <typename Component> bool has_component(EntityId entity) const;
    template <typename... Components>
    bool has_components(EntityId entity) const;

  private:
    template <typename... Components> ComponentMask get_component_mask()
    {
        ComponentMask mask;
        (mask.set(ComponentRegistry::instance().get_meta_id<Components>()),
         ...);
        return mask;
    }

    Archetype *get_or_create_archetype(const ComponentMask &mask);

    // Make Query a friend so it can access archetypes
    template <typename... T> friend class Query;

    std::unordered_map<ComponentMask, std::unique_ptr<Archetype>>
        component_mask_to_archetypes_;
    std::unordered_map<EntityId, Archetype *> entity_to_archetype_;
    EntityId next_entity_id_ = 0;
};

} // namespace ecs

// Include template implementations
#include "ecs.impl.h"