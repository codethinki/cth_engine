#pragma once
#include "CthBasicDebugMessenger.hpp"

namespace cth::vk {
class DebugMessenger : public BasicDebugMessenger {
public:
    DebugMessenger(const BasicInstance* instance, const Config& config) : BasicDebugMessenger{config} { BasicDebugMessenger::create(instance); }
    ~DebugMessenger() override;

    void create(const BasicInstance* instance) override;


public:
    DebugMessenger(const DebugMessenger& other) = delete;
    DebugMessenger& operator=(const DebugMessenger& other) = delete;
    DebugMessenger(DebugMessenger&& other) = default;
    DebugMessenger& operator=(DebugMessenger&& other) = default;
};
}
