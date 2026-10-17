#pragma once
#include <gtl/phmap.hpp>


namespace cvk {
/**
 * @brief hash map without pointer stability, a rehash moves the elements
 */
template<class Key, class T, class... Args>
using flat_hash_map_t = gtl::flat_hash_map<Key, T, Args...>;
}
