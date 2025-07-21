#include <archon/ecs.h>

#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#endif

#include <algorithm>
#include <cassert>
#include <cstring> // For memcpy, size_t
#include <typeindex>
#include <utility>
#include <vector>

namespace ecs::detail
{

ComponentArray::ComponentArray(const ComponentTypeInfo &meta) : meta_(meta) {}

ComponentArray::~ComponentArray() { clear(); }

void ComponentArray::push(void *src, bool ok_to_move)
{
    maybe_grow((element_count_ + 1) * meta_.component_size);

    auto *dst = data_.data() + element_count_ * meta_.component_size;

    if (meta_.is_trivially_copyable) {
        std::memcpy(dst, src, meta_.component_size);
    } else if (ok_to_move && meta_.is_nothrow_move_constructible) {
        meta_.move_constructor(dst, src);
    } else {
        meta_.copy_constructor(dst, src);
    }
    element_count_++;
}

void ComponentArray::push(const void *src)
{
    maybe_grow((element_count_ + 1) * meta_.component_size);

    auto *dst = data_.data() + element_count_ * meta_.component_size;

    if (meta_.is_trivially_copyable) {
        std::memcpy(dst, src, meta_.component_size);
    } else {
        meta_.copy_constructor(dst, src);
    }
    element_count_++;
}

void ComponentArray::maybe_grow(size_t required_size)
{
    if (required_size <= data_.size()) {
        return;
    }

    decltype(data_) new_data(std::max(required_size, data_.size() * 2));
    const auto element_count = element_count_;

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

    // Destroy all elements in the current data_ member
    clear();
    // Swap in newly created larger array
    data_ = std::move(new_data);
    element_count_ = element_count;
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

    // Only need to move if we're not removing the last element
    if (idx != last_idx) {
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
        meta_.destructor(data_.data() + (last_idx * meta_.component_size));
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
            components.emplace(static_cast<ComponentTypeId>(id),
                               meta.create_array());
        }
    }
}

bool Archetype::operator==(const Archetype &other) const
{
    return mask_ == other.mask_;
}

Archetype::EntityIdx Archetype::add_entity(EntityId entity)
{
    assert(!contains(entity) && "Entity already exists");

    const auto newIndex = entity_count();
    idx_to_entity.push_back(entity);
    entities_to_idx.insert({entity, newIndex});

    assert(idx_to_entity.size() == entities_to_idx.size() &&
           "Size mismatch after add");
    return newIndex;
}

EntityId Archetype::get_entity(EntityIdx idx) const
{
    return idx_to_entity[idx];
}

Archetype::EntityIdx Archetype::entity_count() const
{
    return idx_to_entity.size();
}

bool Archetype::contains(EntityId entity) const
{
    return entities_to_idx.contains(entity);
}

Archetype::EntityIdx Archetype::idx_of(EntityId entity) const
{
    return entities_to_idx.at(entity);
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
    const size_t last_index = entity_count() - 1;

    // Only update indices if we're not removing the last element
    if (mid_index != last_index) {
        entities_to_idx[idx_to_entity[last_index]] = mid_index;
        std::swap(idx_to_entity[mid_index], idx_to_entity[last_index]);
    }

    idx_to_entity.pop_back();

    // update component arrays
    for (auto &[_, array] : components) {
        array.remove(mid_index);
    }

    assert(idx_to_entity.size() == entities_to_idx.size() &&
           "Size mismatch after remove");
}

void Archetype::clear()
{
    idx_to_entity.clear();
    entities_to_idx.clear();
    components.clear();
}

} // namespace ecs::detail

namespace ecs
{

EntityId World::create_entity()
{
    const EntityId new_entity = next_entity_id_++;
    auto &empty_archetype = get_or_create_archetype(detail::ComponentMask());
    empty_archetype.add_entity(new_entity);
    entity_to_archetype_.emplace(new_entity, empty_archetype);
    return new_entity;
}

detail::Archetype &
World::get_or_create_archetype(const detail::ComponentMask &mask)
{
    // A pair consisting of an iterator to the inserted element (or to the
    // element that prevented the insertion) and a bool value set to true if and
    // only if the insertion took place.
    const auto &[iter, insertion_success] =
        component_mask_to_archetypes_.emplace(mask, mask);
    return iter->second;
}
} // namespace ecs