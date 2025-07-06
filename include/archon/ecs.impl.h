#pragma once

#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#endif

#include <cassert>

namespace ecs
{

template <typename T> std::unique_ptr<ComponentArray> ComponentArray::create()
{
    const auto meta_id = ComponentRegistry::instance().get_meta_id<T>();
    const auto &meta = ComponentRegistry::instance().get_meta(meta_id);
    return std::unique_ptr<ComponentArray>(new ComponentArray(meta_id, meta));
}

template <typename T> void ComponentRegistry::register_component()
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
            [](void *dst, void *src) { new (dst) T(*static_cast<T *>(src)); },
        .move_construct =
            [](void *dst, void *src) {
                new (dst) T(std::move(*static_cast<T *>(src)));
            },
        .destroy_component = [](void *ptr) { static_cast<T *>(ptr)->~T(); },
        .component_size = sizeof(T),
        .type_name = typeid(T).name(),
        .is_trivially_copy_assignable_ = std::is_trivially_copy_assignable_v<T>,
        .is_nothrow_move_assignable_ = std::is_nothrow_move_assignable_v<T>,
        .is_trivially_destructible_ = std::is_trivially_destructible_v<T>};

    meta_data.push_back(meta_array);
}

template <typename T> MetaComponentId ComponentRegistry::get_meta_id() const
{
    return get_meta_id(typeid(std::decay_t<T>));
}

// Archetype implementation
template <typename T> T *Archetype::data()
{
    const MetaComponentId id = ComponentRegistry::instance().get_meta_id<T>();

    assert(components.contains(id) &&
           "Archetype does not store component type");

    return components[id]->data<T>();
}

template <typename T> T &Archetype::get_component(size_t index)
{
    assert(index < idx_to_entity.size() && "Out of bounds access");

    const MetaComponentId id = ComponentRegistry::instance().get_meta_id<T>();

    assert(components.contains(id) &&
           "Archetype does not store component type");

    return components[id]->get<T>(index);
}

template <typename T> T &Archetype::get_component(EntityId entity)
{
    assert(entities_to_idx.contains(entity) && "Entity not in Archetype");
    return get_component<T>(entities_to_idx[entity]);
}

template <typename... Components>
std::tuple<Components &...> Archetype::get_components(EntityId entity)
{
    assert(entities_to_idx.contains(entity) && "Entity not in Archetype");
    const size_t index = entities_to_idx[entity];
    return std::tuple<Components &...>(get_component<Components>(index)...);
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

        auto component_arrays =
            std::make_tuple(archetype->template data<QueryComponents>()...);

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
    Archetype *current_archetype = entity_to_archetype_[entity];

    // Find or create appropriate archetype
    const ComponentMask current_mask =
        current_archetype ? current_archetype->mask_ : ComponentMask();

    // Add the new component types to the mask
    const auto add_mask = get_component_mask<Components...>();
    const ComponentMask target_mask = current_mask | add_mask;

    assert(current_mask != target_mask && "Adding Components twice");

    auto *target_archetype = get_or_create_archetype(target_mask);

    // Add entity to target archetype
    const size_t new_idx = target_archetype->add_entity(entity);

    // Move or copy new components into place
    // Compile-time resolution via fold expression is possible,
    // because new component types are given
    (
        [&]() {
            using DecayedType = std::decay_t<Components>;
            const MetaComponentId id =
                ComponentRegistry::instance().get_meta_id<DecayedType>();
            auto *ptr = target_archetype->components[id]->get_ptr(new_idx);
            new (ptr) DecayedType(std::forward<Components>(component));
        }(),
        ...);

    // Copy existing components from old to new archetype
    // Runtime MetaId Lookup is required, because the old component types
    // are not directly available
    if (current_archetype) {
        const size_t old_index = current_archetype->entities_to_idx[entity];
        for (const auto &[comp_id, old_array] : current_archetype->components) {
            const auto &meta = ComponentRegistry::instance().get_meta(comp_id);

            // Select appropriate transition mechanism based on type traits
            // Use placement new for construction, not assignment
            if (meta.is_trivially_copy_assignable_) {
                std::memcpy(
                    target_archetype->components[comp_id]->get_ptr(new_idx),
                    current_archetype->components[comp_id]->get_ptr(old_index),
                    meta.component_size);
            } else if (meta.is_nothrow_move_assignable_) {
                meta.move_construct(
                    target_archetype->components[comp_id]->get_ptr(new_idx),
                    old_array->get_ptr(old_index));
                // Destroy the moved-from object
                meta.destroy_component(old_array->get_ptr(old_index));
            } else {
                meta.copy_construct(
                    target_archetype->components[comp_id]->get_ptr(new_idx),
                    old_array->get_ptr(old_index));
            }
        }
        current_archetype->remove_entity(entity);
    }

    // Update entity mapping
    entity_to_archetype_[entity] = target_archetype;
}

template <typename... Components> void World::remove_components(EntityId entity)
{
#ifdef TRACY_ENABLE
    ZoneScoped;
#endif
    auto entity_it = entity_to_archetype_.find(entity);
    if (entity_it == entity_to_archetype_.end()) {
        return; // Entity doesn't exist
    }

    Archetype *current_archetype = entity_it->second;
    if (!current_archetype) {
        return; // Entity has no archetype
    }

    assert(current_archetype->entities_to_idx.find(entity) !=
               current_archetype->entities_to_idx.end() &&
           "Entity should exist in its assigned archetype");

    // Create target mask by removing the specified components
    const auto remove_mask = get_component_mask<Components...>();
    const ComponentMask target_mask = current_archetype->mask_ & (~remove_mask);

    // If the mask is empty, remove the entity entirely
    if (target_mask.none()) {
        current_archetype->remove_entity(entity);
        entity_to_archetype_.erase(entity_it);
        return;
    }

    // Find or create the target archetype
    auto *target_archetype = get_or_create_archetype(target_mask);

    // If target archetype is the same as current, no migration needed
    if (target_archetype == current_archetype) {
        return;
    }

    // Get the old index before modifying anything
    const size_t old_idx = current_archetype->entities_to_idx[entity];

    // Add entity to target archetype
    const size_t new_idx = target_archetype->add_entity(entity);

    // Copy remaining components from current to target archetype
    for (const auto &[comp_id, old_array] : current_archetype->components) {
        // Only copy components that exist in the target archetype (use
        // ComponentMask for faster lookup)
        if (target_mask.test(comp_id)) {
            const auto &meta = ComponentRegistry::instance().get_meta(comp_id);
            meta.copy_component(
                target_archetype->components[comp_id]->get_ptr(new_idx),
                old_array->get_ptr(old_idx));
        }
    }

    // Remove entity from current archetype
    current_archetype->remove_entity(entity);

    // Update entity mapping
    entity_to_archetype_[entity] = target_archetype;
}

template <typename Component> Component &World::get_component(EntityId entity)
{
    assert(entity_to_archetype_.contains(entity) && "Entity does not exist");
    return entity_to_archetype_[entity]->template get_component<Component>(
        entity);
}

template <typename Component>
const Component &World::get_component(EntityId entity) const
{
    assert(entity_to_archetype_.contains(entity) && "Entity does not exist");
    return entity_to_archetype_.at(entity)->template get_component<Component>(
        entity);
}

template <typename... Components>
std::tuple<Components &...> World::get_components(EntityId entity)
{
    assert(entity_to_archetype_.contains(entity) && "Entity does not exist");
    return entity_to_archetype_[entity]->template get_components<Components...>(
        entity);
}

template <typename... Components>
std::tuple<const Components &...> World::get_components(EntityId entity) const
{
    assert(entity_to_archetype_.contains(entity) && "Entity does not exist");
    return entity_to_archetype_.at(entity)
        ->template get_components<Components...>(entity);
}

template <typename Component> bool World::has_component(EntityId entity) const
{
    auto it = entity_to_archetype_.find(entity);
    if (it == entity_to_archetype_.end()) {
        return false;
    }
    return it->second->mask_.test(
        ComponentRegistry::instance().get_meta_id<Component>());
}

template <typename... Components>
bool World::has_components(EntityId entity) const
{
    return (has_component<Components>(entity) && ...);
}

} // namespace ecs