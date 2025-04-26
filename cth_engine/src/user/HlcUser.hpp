#pragma once
#include "interface/object/HlcStandardObject.hpp"


namespace cth::vk {

class User final : public StandardObject {
public:
    explicit User(Transform const& transform) : StandardObject(transform) {}
    ~User() override = default;

    User(User const& other) = delete;
    User& operator=(User const& other) = delete;
    User(User&& other) = default;
    User& operator=(User&& other) = default; // copy/move operations

};

}
