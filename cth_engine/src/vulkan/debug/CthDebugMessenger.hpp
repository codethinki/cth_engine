#pragma once
#include "CthBasicDebugMessenger.hpp"

namespace cth {
class DebugMessenger : public BasicDebugMessenger {
public:
    DebugMessenger(DeletionQueue* deletion_queue, BasicInstance* instance, const Config& config) : BasicDebugMessenger{config},
        _deletionQueue(deletion_queue) { BasicDebugMessenger::create(instance); }
    ~DebugMessenger() override;

    void create(BasicInstance* instance) override;

    void destroy(DeletionQueue* deletion_queue = nullptr) override;

private:
    DeletionQueue* _deletionQueue;

public:
    DebugMessenger(const DebugMessenger& other) = delete;
    DebugMessenger& operator=(const DebugMessenger& other) = delete;
    DebugMessenger(DebugMessenger&& other) = default;
    DebugMessenger& operator=(DebugMessenger&& other) = default;
};
}
