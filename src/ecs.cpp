#include <archon/ecs.h>

#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#endif

#include <cassert>
#include <cstring> // For memcpy, size_t
#include <memory>
#include <typeindex>
#include <utility>
#include <vector>

namespace ecs::detail
{

ComponentArray::ComponentArray(ComponentTypeId meta_id,
                               const ComponentTypeInfo &meta)
    : meta_id_(meta_id), meta_(meta)
{
}

ComponentArray::~ComponentArray() { clear(); }

void ComponentArray::push()
{
    maybe_grow((element_count_ + 1) * meta_.component_size);
    meta_.default_constructor(data_.data() +
                              element_count_ * meta_.component_size);
    element_count_++;
}

void ComponentArray::push(void *src)
{
    maybe_grow((element_count_ + 1) * meta_.component_size);
    if (meta_.is_trivially_copyable) {
        std::memcpy(data_.data() + element_count_ * meta_.component_size, src,
                    meta_.component_size);
    } else if (meta_.is_nothrow_move_constructible) {
        meta_.move_constructor(
            data_.data() + element_count_ * meta_.component_size, src);
    } else {
        meta_.copy_constructor(
            data_.data() + element_count_ * meta_.component_size, src);
    }

    element_count_++;
}

void ComponentArray::maybe_grow(size_t required_size)
{
    if (required_size <= data_.size()) {
        return;
    }

    decltype(data_) new_data(std::max(required_size, data_.size() * 2));

    if (meta_.is_trivially_copyable) {
        std::memcpy(new_data.data(), data_.data(),
                    element_count_ * meta_.component_size);
    } else if (meta_.is_nothrow_move_constructible) {
        for (size_t i = 0; i < element_count_; i++) {
            meta_.move_constructor(new_data.data() + i * meta_.component_size,
                                   data_.data() + i * meta_.component_size);
        }
    } else {
        for (size_t i = 0; i < element_count_; i++) {
            meta_.copy_constructor(new_data.data() + i * meta_.component_size,
                                   data_.data() + i * meta_.component_size);
        }
    }

    clear();

    data_ = std::move(new_data);
}

void ComponentArray::clear()
{
    if (!meta_.is_trivially_destructible) {
        const size_t current_size = size();
        for (size_t i = 0; i < current_size; ++i) {
            meta_.destructor(data_.data() + (i * meta_.component_size));
        }
    }
    element_count_ = 0;
    data_.clear();
}

size_t ComponentArray::size() const { return element_count_; }

void ComponentArray::reserve(size_t size) { maybe_grow(size); }

void ComponentArray::remove(size_t idx)
{
    assert(idx < size() && "Index out of bounds in remove");

    const size_t last_idx = size() - 1;

    if (idx != last_idx) {
        // Only need to move if we're not removing the last element
        if (meta_.is_trivially_copyable) {
            std::memcpy(
                // dst
                data_.data() + (idx * meta_.component_size),
                // src
                data_.data() + (last_idx * meta_.component_size),
                meta_.component_size);
        } else {
            // Destroy the element we're removing
            meta_.destructor(data_.data() + (idx * meta_.component_size));
            // Move the last element to fill the gap
            meta_.move_constructor(data_.data() + (idx * meta_.component_size),
                                   data_.data() +
                                       (last_idx * meta_.component_size));
        }
    }
    // Removing the last element - just need to destroy it for non-trivial
    // types
    if (!meta_.is_trivially_destructible) {
        meta_.destructor(data_.data() + (idx * meta_.component_size));
    }
    element_count_--;
}

void *ComponentArray::get_ptr(size_t index)
{
    return data_.data() + (index * meta_.component_size);
}

ComponentRegistry &ComponentRegistry::instance()
{
    static ComponentRegistry registry;
    return registry;
}

ComponentTypeId
ComponentRegistry::get_component_type_id(std::type_index type_idx) const
{
    assert(component_ids.contains(type_idx) && "Component type not registered");
    return component_ids.at(type_idx);
}

const ComponentTypeInfo &
ComponentRegistry::get_component_type_info(ComponentTypeId component_id) const
{
    assert(component_id < meta_data.size());
    return meta_data[component_id];
}

Archetype::Archetype(const ComponentMask &mask) : mask_(mask)
{
    for (size_t id = 0; id < mask_.size(); id++) {
        if (mask_.test(id)) {
            const auto &meta =
                ComponentRegistry::instance().get_component_type_info(
                    static_cast<ComponentTypeId>(id));
            components[static_cast<ComponentTypeId>(id)] = meta.create_array();
        }
    }
}

size_t Archetype::add_entity(EntityId entity)
{
    assert(!entities_to_idx.contains(entity) && "Entity already exists");

    const size_t newIndex = idx_to_entity.size();
    idx_to_entity.push_back(entity);
    entities_to_idx.insert({entity, newIndex});

    for (auto &[_, array] : components) {
        array->push();
    }
    assert(idx_to_entity.size() == entities_to_idx.size() &&
           "Size mismatch after add");
    return newIndex;
}

void Archetype::remove_entity(EntityId entity)
{
    assert(idx_to_entity.size() == entities_to_idx.size() &&
           "Size mismatch before remove");

    const auto node_handle = entities_to_idx.extract(entity);
    if (!node_handle) {
        return;
    }

    const size_t mid_index = node_handle.mapped();
    const size_t last_index = idx_to_entity.size() - 1;

    // Only update indices if we're not removing the last element
    if (mid_index != last_index) {
        entities_to_idx[idx_to_entity[last_index]] = mid_index;
        std::swap(idx_to_entity[mid_index], idx_to_entity[last_index]);
    }

    idx_to_entity.pop_back();

    // update component arrays
    for (auto &[_, array] : components) {
        array->remove(mid_index);
    }

    assert(idx_to_entity.size() == entities_to_idx.size() &&
           "Size mismatch after remove");
}

void Archetype::clear_entities()
{
    entities_to_idx.clear();
    idx_to_entity.clear();
    for (auto &[_, component_array] : components) {
        component_array->clear();
    }
}

std::unique_ptr<Archetype>
Archetype::with_component(const ComponentTypeId &new_comp_id) const
{
    // Collect existing component IDs
    auto new_mask = mask_;
    new_mask.set(new_comp_id);
    return std::make_unique<Archetype>(new_mask);
}

std::unique_ptr<Archetype>
Archetype::without_component(const ComponentTypeId &remove_comp_id) const
{
    auto new_mask = mask_;
    new_mask.reset(remove_comp_id);
    return std::make_unique<Archetype>(new_mask);
}
} // namespace ecs::detail

namespace ecs
{

EntityId World::create_entity() { return next_entity_id_++; }

detail::Archetype *
World::get_or_create_archetype(const detail::ComponentMask &mask)
{
    if (auto kv_it = component_mask_to_archetypes_.find(mask);
        kv_it != component_mask_to_archetypes_.end()) {
        return kv_it->second.get();
    }

    // Archetype needs to be created
    std::unique_ptr<detail::Archetype> new_arch =
        std::make_unique<detail::Archetype>(mask);
    auto [kv_it, _] =
        component_mask_to_archetypes_.emplace(mask, std::move(new_arch));
    return kv_it->second.get();
}
} // namespace ecs