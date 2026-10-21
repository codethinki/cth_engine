#pragma once
#include <cth/data/hash_map.hpp>


namespace cvk {
/**
 * @brief hash map without pointer stability, a rehash moves the elements
 */
template<class Key, class T, class... Args>
using flat_hash_map = cth::dt::flat_hash_map<Key, T, Args...>;
}
