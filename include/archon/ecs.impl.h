#pragma once

#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#endif

#include <cassert>

namespace ecs
{

template <typename T> ComponentArray<T>::ComponentArray()
{
    meta_id = ComponentRegistry::instance().get_meta_id<T>();
    component_size = sizeof(T);
}

template <typename T> T *ComponentArray<T>::get_data()
{
    return static_cast<T *>(data);
}

template <typename T> const T *ComponentArray<T>::get_data() const
{
    return static_cast<const T *>(data);
}

// Archetype implementation
template <typename T> T *Archetype::get_component(size_t index)
{
    const MetaComponentId id = ComponentRegistry::instance().get_meta_id<T>();
    return static_cast<T *>(components[id]->get_ptr(index));
}

template <typename T> T *Archetype::get_component(EntityId entity)
{
    const auto idx_it = entities_to_idx.find(entity);
    if (idx_it == entities_to_idx.end()) {
        return nullptr;
    }
    return get_component<T>(idx_it->second);
}

template <typename... Components>
std::tuple<Components *...> Archetype::get_components(EntityId entity)
{
    const auto idx_it = entities_to_idx.find(entity);
    if (idx_it == entities_to_idx.end()) {
        return std::make_tuple(static_cast<Components *>(nullptr)...);
    }
    return std::make_tuple(get_component<Components>(idx_it->second)...);
}

// Query implementation
template <typename... QueryComponents> Query<QueryComponents...>::Query()
{
    (include_mask.set(
         ComponentRegistry::instance().get_meta_id<QueryComponents>()),
     ...);
}

template <typename... QueryComponents>
template <typename... WithComponents>
Query<QueryComponents...> &Query<QueryComponents...>::with()
{
    (include_mask.set(
         ComponentRegistry::instance().get_meta_id<WithComponents>()),
     ...);
    return *this;
}

template <typename... QueryComponents>
template <typename... ExcludeComponents>
Query<QueryComponents...> &Query<QueryComponents...>::without()
{
    (exclude_mask.set(
         ComponentRegistry::instance().get_meta_id<ExcludeComponents>()),
     ...);
    return *this;
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
        typename function_traits<Func>::decayed_argument_types;
    using QueriedTypes = std::tuple<std::decay_t<QueryComponents>...>;
    using QueriedTypesWithId =
        std::tuple<std::decay_t<QueryComponents>..., EntityId>;

    static_assert(std::is_same_v<ArgumentBaseTypes, QueriedTypesWithId> ||
                      std::is_same_v<ArgumentBaseTypes, QueriedTypes>,
                  "Function arguments must match query value components");

    for (auto &[component_mask, archetype] :
         world.component_mask_to_archetypes_) {
        if ((component_mask & include_mask) != include_mask)
            continue;
        if ((component_mask & exclude_mask) != 0)
            continue;

        size_t element_count = archetype->idx_to_entity.size();
        if (element_count == 0)
            continue; // Early exit for empty archetypes

        auto component_arrays = std::make_tuple(
            archetype->template get_component<QueryComponents>(size_t{0})...);

        for (size_t idx = 0; idx < element_count; idx++) {
            if constexpr (has_extra_param<
                              std::tuple<Func, QueryComponents...>>::value) {
                func(std::get<QueryComponents *>(component_arrays)[idx]...,
                     archetype->idx_to_entity[idx]);
            } else {
                func(std::get<QueryComponents *>(component_arrays)[idx]...);
            }
        }
    }
}

template <typename... QueryComponents>
template <typename Func>
void Query<QueryComponents...>::each_archetype(Func &&func, World &world) const
{
    for (const auto &[component_mask, archetype] :
         world.component_mask_to_archetypes_) {
        if ((component_mask & include_mask) != include_mask)
            continue;
        if ((component_mask & exclude_mask) != 0)
            continue;

        func(archetype->entities_to_idx.size(),
             archetype->template get_component<QueryComponents>(0)...);
    }
}

template <typename... QueryComponents>
void Query<QueryComponents...>::clear(World &world)
{
    for (const auto &[component_mask, archetype] :
         world.component_mask_to_archetypes_) {
        if ((component_mask & include_mask) != include_mask)
            continue;
        if ((component_mask & exclude_mask) != 0)
            continue;
        archetype->clear_entities();
    }
}

template <typename... QueryComponents>
size_t Query<QueryComponents...>::size(const World &world) const
{
#ifdef TRACY_ENABLE
    ZoneScoped;
#endif
    size_t total = 0;
    for (const auto &[component_mask, archetype] :
         world.component_mask_to_archetypes_) {
        if ((component_mask & include_mask) != include_mask)
            continue;
        if ((component_mask & exclude_mask) != 0)
            continue;
        total += archetype->entities_to_idx.size();
    }
    return total;
}

// World implementation
template <typename... Components>
void World::add_components(EntityId entity, Components &&...component)
{
#ifdef TRACY_ENABLE
    ZoneScoped;
#endif
    Archetype *oldArchetype = entity_to_archetype_[entity];

    // Find or create appropriate archetype
    ComponentMask target_mask =
        oldArchetype ? oldArchetype->mask_ : ComponentMask();

    // Get the MetaComponentIds for all component types
    const auto component_ids = get_component_meta_ids<Components...>();
    for (auto id : component_ids) {
        target_mask.set(id);
    }

    auto *target_archetype = get_or_create_archetype(target_mask);

    // Add entity to target archetype
    const size_t new_idx = target_archetype->add_entity(entity);

    // Move all components into place
    int component_index = 0;
    (
        [&]() {
            using DecayedType = std::decay_t<Components>;
            auto *component_ptr =
                target_archetype->template get_component<DecayedType>(new_idx);
            *component_ptr = std::forward<Components>(component);
            component_index++;
        }(),
        ...);

    if (oldArchetype) {
        const size_t old_index = oldArchetype->entities_to_idx[entity];
        // Copy existing components from old to new archetype
        for (const auto &[comp_id, old_array] : oldArchetype->components) {
            const auto *meta = ComponentRegistry::instance().get_meta(comp_id);

            // Copy component data
            meta->copy_component(
                target_archetype->components[comp_id]->get_ptr(new_idx),
                old_array->get_ptr(old_index));
        }
        oldArchetype->remove_entity(entity);
    }

    // Update entity mapping
    entity_to_archetype_[entity] = target_archetype;
}

template <typename Component> Component *World::get_component(EntityId entity)
{
    auto archetype_it = entity_to_archetype_.find(entity);
    if (archetype_it == entity_to_archetype_.end()) {
        return nullptr;
    }
    return archetype_it->second->template get_component<Component>(entity);
}

template <typename... Components>
std::tuple<Components *...> World::get_components(EntityId entity)
{
    auto archetype_it = entity_to_archetype_.find(entity);
    if (archetype_it == entity_to_archetype_.end()) {
        return std::make_tuple(static_cast<Components *>(nullptr)...);
    }
    return archetype_it->second->template get_components<Components...>(entity);
}

} // namespace ecs