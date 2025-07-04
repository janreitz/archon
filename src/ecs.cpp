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

namespace ecs
{

ComponentArray::ComponentArray(MetaComponentId meta_id, size_t component_size)
    : meta_id_(meta_id), component_size_(component_size)
{
}

void ComponentArray::resize(size_t new_size)
{
    data_.resize(new_size * component_size_);
}

void ComponentArray::clear() { data_.clear(); }

size_t ComponentArray::size() const { return data_.size() / component_size_; }

void ComponentArray::reserve(size_t size)
{
    data_.reserve(size * component_size_);
}

void ComponentArray::remove(size_t idx)
{
    assert(idx <= size() && "Index out of bounds in remove");
    std::memcpy(
        // to idx
        data_.data() + (idx * component_size_),
        // from idx
        data_.data() + ((size() - 1) * component_size_), component_size_);
    data_.resize(data_.size() - component_size_);
}

void *ComponentArray::get_ptr(size_t index)
{
    return data_.data() + (index * component_size_);
}

ComponentRegistry &ComponentRegistry::instance()
{
    static ComponentRegistry registry;
    return registry;
}

MetaComponentId ComponentRegistry::get_meta_id(std::type_index type_idx) const
{
    assert(component_ids.contains(type_idx) && "Component type not registered");
    return component_ids.at(type_idx);
}

const MetaComponentArray &
ComponentRegistry::get_meta(MetaComponentId component_id) const
{
    assert(component_id < meta_data.size());
    return meta_data[component_id];
}

EntityId World::create_entity() { return next_entity_id_++; }

Archetype::Archetype(const ComponentMask &mask) : mask_(mask)
{
    for (size_t id = 0; id < mask_.size(); id++) {
        if (mask_.test(id)) {
            const auto &meta = ComponentRegistry::instance().get_meta(
                static_cast<ecs::MetaComponentId>(id));
            components[static_cast<ecs::MetaComponentId>(id)] =
                meta.create_array();
        }
    }
}

size_t Archetype::add_entity(EntityId entity)
{
    assert(!entities_to_idx.contains(entity) && "Entity already exists");

    const size_t newIndex = idx_to_entity.size();
    idx_to_entity.push_back(entity);
    entities_to_idx.insert({entity, newIndex});

    // Resize all component arrays
    for (auto &[_, array] : components) {
        array->resize(entities_to_idx.size());
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
Archetype::with_component(const MetaComponentId &new_comp_id) const
{
    // Collect existing component IDs
    auto new_mask = mask_;
    new_mask.set(new_comp_id);
    return std::make_unique<Archetype>(new_mask);
}

std::unique_ptr<Archetype>
Archetype::without_component(const MetaComponentId &remove_comp_id) const
{
    auto new_mask = mask_;
    new_mask.reset(remove_comp_id);
    return std::make_unique<Archetype>(new_mask);
}

Archetype *World::get_or_create_archetype(const ComponentMask &mask)
{
    if (auto kv_it = component_mask_to_archetypes_.find(mask);
        kv_it != component_mask_to_archetypes_.end()) {
        return kv_it->second.get();
    }

    // Archetype needs to be created
    std::unique_ptr<Archetype> new_arch = std::make_unique<Archetype>(mask);
    auto [kv_it, _] =
        component_mask_to_archetypes_.emplace(mask, std::move(new_arch));
    return kv_it->second.get();
}
} // namespace ecs