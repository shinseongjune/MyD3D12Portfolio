#pragma once
#include <memory>
#include <vector>

class Behaviour;

struct PendingAdd
{
    std::unique_ptr<Behaviour> ptr;
    bool enabled = true;
};

struct PendingRemove
{
    Behaviour* ptr = nullptr;
};

struct ScriptEntry
{
    std::unique_ptr<Behaviour> ptr;
    bool awoken = false;
    bool started = false;
    bool enabled = true;
};

struct ScriptComponent
{
    std::vector<ScriptEntry> scripts;

    std::vector<PendingAdd> pendingAdd;
    std::vector<PendingRemove> pendingRemove;
};
