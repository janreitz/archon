#pragma once

#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#endif

#include <cassert>

namespace ecs
{

namespace detail
{

class ComponentArray
{
  public:
    template <typename T> static ComponentArray create();

    ~ComponentArray();

    [[nodiscard]] size_t size() const;

    // Chooses optimal transition strategy based on type traits.
    void push(void *src, bool allow_move = false);
    void push(const void *src);
    void reserve(size_t size);
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
        return data<T>()[index];
    }

  private:
    ComponentArray(ComponentTypeId meta_id, const ComponentTypeInfo &meta);

    void maybe_grow(size_t required_size);
    void destroy_elements();

    size_t element_count_ = 0;
    ComponentTypeId meta_id_;
    ComponentTypeInfo meta_;
    std::vector<uint8_t> data_;
};

class ComponentRegistry
{
  public:
    static ComponentRegistry &instance();
    template <typename T> void register_component();
    template <typename T> ComponentTypeId get_component_type_id() const;
    ComponentTypeId get_component_type_id(std::type_index type_idx) const;
    const ComponentTypeInfo &
    get_component_type_info(ComponentTypeId component_id) const;

  private:
    std::vector<ComponentTypeInfo> meta_data;
    std::unordered_map<std::type_index, ComponentTypeId> component_ids;
    ComponentTypeId next_id = 0;
};

template <typename T> void ComponentRegistry::register_component()
{
    const auto type_idx = std::type_index(typeid(std::decay_t<T>));
    if (component_ids.contains(type_idx)) {
        // Component already registered.
        return;
    }

    const ComponentTypeId meta_id = next_id++;
    component_ids.insert({type_idx, meta_id});

    ComponentTypeInfo meta_array{
        .create_array = []() -> ComponentArray {
            return ComponentArray::create<T>();
        },
        .default_constructor = [](void *dst) { new (dst) T(); },
        .destructor = [](void *obj) { static_cast<T *>(obj)->~T(); },
        .copy_constructor =
            [](void *dst, const void *src) {
                new (dst) T(*static_cast<const T *>(src));
            },
        .move_constructor =
            [](void *dst, void *src) {
                new (dst) T(std::move(*static_cast<T *>(src)));
            },
        .component_size = sizeof(T),
        .type_name = typeid(T).name(),
        .is_trivially_copyable = std::is_trivially_copyable_v<T>,
        .is_nothrow_move_constructible =
            std::is_nothrow_move_constructible_v<T>,
        .is_trivially_destructible = std::is_trivially_destructible_v<T>};

    meta_data.push_back(meta_array);
}

template <typename T>
ComponentTypeId ComponentRegistry::get_component_type_id() const
{
    return get_component_type_id(typeid(std::decay_t<T>));
}

template <typename... Components> ComponentMask get_component_mask()
{
    ComponentMask mask;
    (mask.set(
         ComponentRegistry::instance().get_component_type_id<Components>()),
     ...);
    return mask;
}

template <typename T> ComponentArray ComponentArray::create()
{
    const auto meta_id =
        ComponentRegistry::instance().get_component_type_id<T>();
    const auto &meta =
        ComponentRegistry::instance().get_component_type_info(meta_id);
    return ComponentArray(meta_id, meta);
}

class Archetype
{
  public:
    explicit Archetype(const ComponentMask &mask);
    Archetype(const Archetype &other) = delete;
    Archetype &operator=(const Archetype &other) = delete;
    Archetype(Archetype &&other) noexcept = default;
    Archetype &operator=(Archetype &&other) noexcept = default;

    bool operator==(const Archetype &other);

    std::unordered_map<ComponentTypeId, ComponentArray> components;
    std::vector<EntityId> idx_to_entity;

    const ComponentMask mask_;

    template <typename T> T *data();
    template <typename T> const T *data() const;
    template <typename T> T &get_component(size_t index);
    template <typename T> T &get_component(EntityId entity);
    template <typename... Components>
    std::tuple<Components &...> get_components(EntityId entity);
    /// @brief
    /// @param entity
    /// @return ComponentArray index of the new entity
    size_t add_entity(EntityId entity);
    size_t idx_of(EntityId entity) const;
    size_t entity_count() const;
    bool contains(EntityId entity) const;
    void remove_entity(EntityId entity);
    void clear_entities();

    // Create archetype with additional component
    std::unique_ptr<Archetype>
    with_component(const ComponentTypeId &new_comp_id) const;

    // Create archetype without specific component
    std::unique_ptr<Archetype>
    without_component(const ComponentTypeId &remove_comp_id) const;

  private:
    std::unordered_map<EntityId, size_t> entities_to_idx;
};

template <typename T> T *Archetype::data()
{
    const ComponentTypeId id =
        ComponentRegistry::instance().get_component_type_id<T>();

    assert(components.contains(id) &&
           "Archetype does not store component type");

    return components.at(id).data<T>();
}

template <typename T> const T *Archetype::data() const
{
    const ComponentTypeId id =
        ComponentRegistry::instance().get_component_type_id<T>();

    assert(components.contains(id) &&
           "Archetype does not store component type");

    return components.at(id).data<T>();
}

template <typename T> T &Archetype::get_component(size_t index)
{
    assert(index < idx_to_entity.size() && "Out of bounds access");

    const ComponentTypeId id =
        ComponentRegistry::instance().get_component_type_id<T>();

    assert(components.contains(id) &&
           "Archetype does not store component type");

    return components.at(id).get<T>(index);
}

template <typename T> T &Archetype::get_component(EntityId entity)
{
    assert(contains(entity) && "Entity not in Archetype");
    return get_component<T>(idx_of(entity));
}

template <typename... Components>
std::tuple<Components &...> Archetype::get_components(EntityId entity)
{
    assert(contains(entity) && "Entity not in Archetype");
    const size_t index = idx_of(entity);
    return std::tuple<Components &...>(get_component<Components>(index)...);
}

} // namespace detail

template <typename T> void register_component()
{
    detail::ComponentRegistry::instance().register_component<T>();
}

// Query implementation
template <typename... QueryComponents> Query<QueryComponents...>::Query()
{
    (include_mask.set(detail::ComponentRegistry::instance()
                          .get_component_type_id<QueryComponents>()),
     ...);
}

template <typename... QueryComponents>
template <typename... WithComponents>
Query<QueryComponents...> &Query<QueryComponents...>::with()
{
    (include_mask.set(detail::ComponentRegistry::instance()
                          .get_component_type_id<WithComponents>()),
     ...);
    return *this;
}

template <typename... QueryComponents>
template <typename... ExcludeComponents>
Query<QueryComponents...> &Query<QueryComponents...>::without()
{
    (exclude_mask.set(detail::ComponentRegistry::instance()
                          .get_component_type_id<ExcludeComponents>()),
     ...);
    return *this;
}

template <typename... QueryComponents>
bool Query<QueryComponents...>::matches(detail::ComponentMask mask) const
{
    const bool matches_all_included = (mask & include_mask) == include_mask;
    const bool matches_no_excluded = (mask & exclude_mask) == 0;
    return matches_all_included && matches_no_excluded;
}

template <typename... QueryComponents>
template <WorldType WorldT, typename Func>
void Query<QueryComponents...>::for_each_matching_archetype(WorldT &&world,
                                                            Func &&func) const
{
    for (auto &archetype : world.archetypes_) {
        if (!matches(archetype.mask_))
            continue;

        func(archetype);
    }
}

template <typename... QueryComponents>
template <WorldType WorldT, typename Func>
requires ArgsConstCompatible<WorldT, Func>
void Query<QueryComponents...>::each(WorldT &&world, Func &&func) const
{
#ifdef TRACY_ENABLE
    ZoneScoped;
#endif
    using ArgumentBaseTypes =
        typename detail::function_traits<Func>::decayed_argument_types;
    using QueriedTypes = std::tuple<std::decay_t<QueryComponents>...>;
    using QueriedTypesWithId =
        std::tuple<std::decay_t<QueryComponents>..., EntityId>;

    static_assert(std::is_same_v<ArgumentBaseTypes, QueriedTypesWithId> ||
                      std::is_same_v<ArgumentBaseTypes, QueriedTypes>,
                  "Function arguments must match query value components");

    for_each_matching_archetype(
        std::forward<WorldT>(world), [&func](auto &archetype) {
            size_t element_count = archetype.entity_count();
            if (element_count == 0)
                return; // Early exit for empty archetypes

            auto component_arrays =
                std::make_tuple(archetype.template data<QueryComponents>()...);

            for (size_t idx = 0; idx < element_count; idx++) {
                [&]<size_t... I>(std::index_sequence<I...>) {
                    if constexpr (detail::has_extra_param<std::tuple<
                                      Func, QueryComponents...>>::value) {
                        func(std::get<I>(component_arrays)[idx]...,
                             archetype.idx_to_entity[idx]);
                    } else {
                        func(std::get<I>(component_arrays)[idx]...);
                    }
                }(std::index_sequence_for<QueryComponents...>{});
            }
        });
}

template <typename... QueryComponents>
template <typename Func>
void Query<QueryComponents...>::each_archetype(Func &&func, World &world) const
{
    for_each_matching_archetype(world, [&func](auto &archetype) {
        func(archetype.entity_count(),
             archetype.template get_component<QueryComponents>(0)...);
    });
}

template <typename... QueryComponents>
void Query<QueryComponents...>::clear(World &world)
{
    for_each_matching_archetype(
        world, [](auto &archetype) { archetype.clear_entities(); });
}

template <typename... QueryComponents>
size_t Query<QueryComponents...>::size(const World &world) const
{
#ifdef TRACY_ENABLE
    ZoneScoped;
#endif
    size_t total = 0;
    for_each_matching_archetype(world, [&total](auto &archetype) {
        total += archetype.entity_count();
    });
    return total;
}

// World implementation
template <typename... Components>
void World::add_components(EntityId entity, Components &&...component)
{
#ifdef TRACY_ENABLE
    ZoneScoped;
#endif
    assert(entity_to_archetype_.contains(entity) && "Entity does not exist");

    const auto current_archetype_idx = entity_to_archetype_.at(entity);

    // Find or create appropriate archetype
    const detail::ComponentMask current_mask =
        archetypes_[current_archetype_idx].mask_;

    // Add the new component types to the mask
    const auto add_mask = detail::get_component_mask<Components...>();
    const detail::ComponentMask target_mask = current_mask | add_mask;

    assert(current_mask != target_mask && "Adding Components twice");

    auto [target_archetype, target_archetype_idx] =
        get_or_create_archetype(target_mask);
    target_archetype.add_entity(entity);

    // Move or copy new components into place
    // Compile-time resolution via fold expression is possible,
    // because new component types are given
    (
        [&]() {
            using DecayedType = std::decay_t<Components>;
            const detail::ComponentTypeId id =
                detail::ComponentRegistry::instance()
                    .get_component_type_id<DecayedType>();
            auto &component_array = target_archetype.components.at(id);
            if constexpr (std::is_const_v<std::remove_reference_t<Components>>) {
                component_array.push(static_cast<const void*>(&component));
            } else {
                component_array.push(&component,
                                     std::is_rvalue_reference_v<Components &&>);
            }
        }(),
        ...);

    // Copy or move existing components from old to new archetype
    // Runtime MetaId Lookup is required, because the old component types
    // are not directly available
    auto &current_archetype = archetypes_[current_archetype_idx];
    const size_t old_index = current_archetype.idx_of(entity);
    for (auto &[comp_id, old_array] : current_archetype.components) {
        auto &target_array = target_archetype.components.at(comp_id);
        target_array.push(old_array.get_ptr(old_index), true);
    }
    current_archetype.remove_entity(entity);

    // Update entity mapping
    entity_to_archetype_[entity] = target_archetype_idx;
}

template <typename... Components> void World::remove_components(EntityId entity)
{
#ifdef TRACY_ENABLE
    ZoneScoped;
#endif

    assert(entity_to_archetype_.contains(entity) && "Entity doesn't exist");

    const auto &current_archetype_idx = entity_to_archetype_[entity];

    assert(archetypes_[current_archetype_idx].contains(entity) &&
           "Entity should exist in its assigned archetype");

    const auto remove_mask = detail::get_component_mask<Components...>();
    const detail::ComponentMask target_mask =
        archetypes_[current_archetype_idx].mask_ & (~remove_mask);

    // Find or create the target archetype
    auto [target_archetype, target_archetype_idx] =
        get_or_create_archetype(target_mask);

    auto &current_archetype = archetypes_[current_archetype_idx];
    // If target archetype is the same as current, no migration needed
    if (target_archetype == current_archetype) {
        return;
    }

    // Get the old index before modifying anything
    const size_t old_idx = current_archetype.idx_of(entity);

    // Add entity to target archetype
    target_archetype.add_entity(entity);

    // Transition remaining components from current to target archetype
    for (auto &[comp_id, old_array] : current_archetype.components) {
        // Only copy components that exist in the target archetype
        if (target_mask.test(comp_id)) {
            target_archetype.components.at(comp_id).push(
                old_array.get_ptr(old_idx), true);
        }
    }

    // Remove entity from current archetype
    current_archetype.remove_entity(entity);

    // Update entity mapping
    entity_to_archetype_[entity] = target_archetype_idx;
}

template <typename Component> Component &World::get_component(EntityId entity)
{
    assert(entity_to_archetype_.contains(entity) && "Entity does not exist");
    auto &archetype = archetypes_[entity_to_archetype_[entity]];
    return archetype.template get_component<Component>(entity);
}

template <typename Component>
const Component &World::get_component(EntityId entity) const
{
    assert(entity_to_archetype_.contains(entity) && "Entity does not exist");
    const auto &archetype = archetypes_.at(entity_to_archetype_.at(entity));
    return archetype.template get_component<Component>(entity);
}

template <typename... Components>
std::tuple<Components &...> World::get_components(EntityId entity)
{
    assert(entity_to_archetype_.contains(entity) && "Entity does not exist");
    auto &archetype = archetypes_[entity_to_archetype_[entity]];
    return archetype.template get_components<Components...>(entity);
}

template <typename... Components>
std::tuple<const Components &...> World::get_components(EntityId entity) const
{
    assert(entity_to_archetype_.contains(entity) && "Entity does not exist");
    const auto &archetype = archetypes_.at(entity_to_archetype_.at(entity));
    return archetype.template get_components<Components...>(entity);
}

template <typename Component> bool World::has_component(EntityId entity) const
{
    assert(entity_to_archetype_.contains(entity) && "Entity does not exist");
    const auto &archetype = archetypes_[entity_to_archetype_.at(entity)];
    return archetype.mask_.test(detail::ComponentRegistry::instance()
                                    .get_component_type_id<Component>());
}

template <typename... Components>
bool World::has_components(EntityId entity) const
{
    return (has_component<Components>(entity) && ...);
}

} // namespace ecs