#pragma once
#include "CthBasicDebugMessenger.hpp"

namespace cth::vk {
class DebugMessenger : public BasicDebugMessenger {
public:
    DebugMessenger(BasicInstance const* instance, Config const& config) : BasicDebugMessenger{config} { BasicDebugMessenger::create(instance); }
    ~DebugMessenger() override;

    void create(BasicInstance const* instance) override;


public:
    DebugMessenger(DebugMessenger const& other) = delete;
    DebugMessenger& operator=(DebugMessenger const& other) = delete;
    DebugMessenger(DebugMessenger&& other) = default;
    DebugMessenger& operator=(DebugMessenger&& other) = default;
};
}
