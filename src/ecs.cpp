#include <archon/ecs.h>
#include <cstdint>
#include <functional>
#include <iosfwd>
#include <string>
#include <string_view>

#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#endif

#include <algorithm>
#include <cassert>
#include <cstring> // For memcpy, size_t
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <stdexcept>
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

const void *ComponentArray::get_ptr(size_t index) const
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

std::optional<ComponentTypeId>
ComponentRegistry::find_by_display_name(std::string_view name) const
{
    for (ComponentTypeId id = 0; id < meta_data.size(); ++id) {
        if (meta_data[id].display_name == name) {
            return id;
        }
    }
    return std::nullopt;
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
    for (auto &[_, component_array] : components) {
        component_array.clear();
    }
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

bool World::remove_entity(EntityId entity)
{
    auto kv_iter = entity_to_archetype_.find(entity);
    if (kv_iter == entity_to_archetype_.end()) {
        return false;
    }
    kv_iter->second.get().remove_entity(entity);
    entity_to_archetype_.erase(kv_iter);
    return true;
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

size_t World::archetype_count() const
{
    return component_mask_to_archetypes_.size();
}

void World::add_entity_to_archetype(EntityId id, detail::Archetype &archetype)
{
    assert(!entity_to_archetype_.contains(id) && "EntityId already in use");
    archetype.add_entity(id);
    entity_to_archetype_.emplace(id, std::ref(archetype));
    if (id >= next_entity_id_) {
        next_entity_id_ = id + 1;
    }
}

void World::save(const std::filesystem::path &path) const
{
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("Cannot open file for writing: " +
                                 path.string());
    }

    auto &registry = detail::ComponentRegistry::instance();

    out << "ARCHON 1\n";

    // Collect and sort archetypes by mask for deterministic output
    std::vector<const detail::Archetype *> sorted_archetypes;
    for (const auto &[mask, archetype] : component_mask_to_archetypes_) {
        sorted_archetypes.push_back(&archetype);
    }
    std::sort(sorted_archetypes.begin(), sorted_archetypes.end(),
              [](const detail::Archetype *a, const detail::Archetype *b) {
                  return a->mask_.to_ulong() < b->mask_.to_ulong();
              });

    for (const auto *archetype_ptr : sorted_archetypes) {
        const auto &archetype = *archetype_ptr;

        if (archetype.mask_.none() || archetype.entity_count() == 0) {
            continue;
        }

        // Collect serializable components, in ComponentTypeId order
        std::vector<std::pair<detail::ComponentTypeId, std::string_view>>
            ser_comps;
        for (size_t id = 0; id < MAX_COMPONENTS; ++id) {
            if (!archetype.mask_.test(id)) {
                continue;
            }
            const auto &info = registry.get_component_type_info(
                static_cast<detail::ComponentTypeId>(id));
            if (info.serialize && !info.display_name.empty()) {
                ser_comps.emplace_back(static_cast<detail::ComponentTypeId>(id),
                                       std::string_view(info.display_name));
            }
        }
        if (ser_comps.empty()) {
            continue;
        }

        const size_t entity_count = archetype.entity_count();
        out << "ARCHETYPE " << entity_count << "\n";

        out << "ENTITIES";
        for (size_t i = 0; i < entity_count; ++i) {
            out << " " << archetype.get_entity(i);
        }
        out << "\n";

        for (const auto &[comp_id, comp_name] : ser_comps) {
            out << comp_name << "\n";
            const auto &info = registry.get_component_type_info(comp_id);
            const auto &arr = archetype.components.at(comp_id);
            for (size_t i = 0; i < entity_count; ++i) {
                out << info.serialize(arr.get_ptr(i)) << "\n";
            }
        }
    }
}

void World::load(const std::filesystem::path &path)
{
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("Cannot open file for reading: " +
                                 path.string());
    }

    auto &registry = detail::ComponentRegistry::instance();

    std::string line;

    // Read and verify header
    if (!std::getline(in, line) || line != "ARCHON 1") {
        throw std::runtime_error("Invalid or unsupported ARCHON file: " +
                                 path.string());
    }

    while (std::getline(in, line)) {
        if (line.empty()) {
            continue;
        }

        if (line.rfind("ARCHETYPE ", 0) != 0) {
            continue; // skip unexpected lines
        }

        const size_t entity_count =
            static_cast<size_t>(std::stoul(line.substr(10)));

        // Parse ENTITIES line
        if (!std::getline(in, line) || line.rfind("ENTITIES", 0) != 0) {
            throw std::runtime_error("Expected ENTITIES line in " +
                                     path.string());
        }
        std::vector<EntityId> entity_ids;
        entity_ids.reserve(entity_count);
        {
            std::istringstream ss(line.substr(8)); // skip "ENTITIES"
            EntityId eid;
            while (ss >> eid) {
                entity_ids.push_back(eid);
            }
        }
        if (entity_ids.size() != entity_count) {
            throw std::runtime_error("Entity count mismatch in " +
                                     path.string());
        }

        // Buffer all component blocks for this archetype
        struct CompBlock {
            detail::ComponentTypeId id;
            std::vector<std::string> values;
            bool known;
        };
        std::vector<CompBlock> comp_blocks;

        // Peek-based reading: use tellg/seekg to look ahead
        std::streampos before_line;
        while (true) {
            before_line = in.tellg();
            if (!std::getline(in, line)) {
                break; // EOF
            }
            if (line.empty()) {
                continue;
            }
            if (line.rfind("ARCHETYPE ", 0) == 0) {
                // Next archetype block - put line back by seeking
                in.seekg(before_line);
                break;
            }
            // It's a component name
            const std::string comp_name = line;
            auto type_id_opt = registry.find_by_display_name(comp_name);

            CompBlock block;
            block.known = type_id_opt.has_value();
            if (block.known) {
                block.id = *type_id_opt;
            }
            block.values.reserve(entity_count);
            for (size_t i = 0; i < entity_count; ++i) {
                if (!std::getline(in, line)) {
                    throw std::runtime_error(
                        "Unexpected EOF reading component values in " +
                        path.string());
                }
                block.values.push_back(line);
            }
            comp_blocks.push_back(std::move(block));
        }

        // Build target mask from known components only
        detail::ComponentMask mask;
        for (const auto &block : comp_blocks) {
            if (block.known) {
                mask.set(block.id);
            }
        }

        // Create target archetype once and place all entities into it
        auto &target = get_or_create_archetype(mask);
        for (EntityId eid : entity_ids) {
            add_entity_to_archetype(eid, target);
        }

        // Deserialize and push component data
        for (const auto &block : comp_blocks) {
            if (!block.known) {
                continue;
            }
            const auto &info = registry.get_component_type_info(block.id);
            if (!info.deserialize) {
                continue;
            }
            auto &arr = target.components.at(block.id);
            for (size_t j = 0; j < entity_count; ++j) {
                std::vector<uint8_t> buf(info.component_size);
                info.deserialize(buf.data(), block.values[j]);
                arr.push(buf.data(), /*allow_move=*/true);
                if (!info.is_trivially_destructible) {
                    info.destructor(buf.data());
                }
            }
        }
    }
}

} // namespace ecs