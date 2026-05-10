#pragma once

#include <cth/hash/hash_general.hpp>

#include <cstdint>

#include <cth/hash/hash_aggregates.hpp>

namespace jly {
struct vertex_attribute {
    bool operator==(vertex_attribute const&) const = default;

    std::string name;
    size_t size;
};
}

CTH_HASH_AGGREGATE(jly::vertex_attribute)

namespace jly {
class vertex_key {
public:
    explicit cxpr vertex_key(std::span<vertex_attribute const> attributes) :
        _attributes{std::from_range, attributes},
        _hash{cth::hash::rng(_attributes)} {}

    explicit cxpr vertex_key(std::vector<vertex_attribute> attributes) : _attributes{std::move(attributes)},
        _hash{cth::hash::rng(_attributes)} {}

    cxpr vertex_key(std::initializer_list<vertex_attribute> attributes) : vertex_key{std::span{attributes}} {}

    cxpr bool operator==(vertex_key const& other) const noexcept {
        return _hash == other._hash && _attributes == other._attributes;
    }

private:
    std::vector<vertex_attribute> _attributes;
    std::size_t _hash;

public:
    cxpr size_t hash() const noexcept { return _hash; }
    cxpr std::span<vertex_attribute const> attributes() const noexcept { return _attributes; }
    cxpr size_t vertex_size() const noexcept {
        return std::ranges::fold_left(
            _attributes,
            size_t{0},
            [](size_t acc, auto const& attr) { return acc + attr.size; }
        );
    }
};

cxpr auto hash(vertex_key const& key) noexcept -> size_t { return key.hash(); }
}

CTH_CXPR_HASH_OVERLOAD(jly::vertex_key, jly::hash)
