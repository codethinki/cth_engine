#pragma once
#include "interface/object/HlcStandardObject.hpp"


namespace cth {

class User final : public StandardObject {
public:
    explicit User(const Transform& transform) : StandardObject(transform) {}
    ~User() override = default;

    User(const User& other) = delete;
    User& operator=(const User& other) = delete;
    User(User&& other) = default;
    User& operator=(User&& other) = default; // copy/move operations

};

}
