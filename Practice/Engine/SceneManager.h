#pragma once
#include <memory>

#include "Scene.h"

class World;

class SceneManager
{
public:
    void Load(World& world, std::unique_ptr<Scene> scene)
    {
        if (m_current)
        {
            m_current->OnUnload(world);
            m_current.reset();
        }

        m_current = std::move(scene);
        if (m_current)
            m_current->OnLoad(world);
    }

    Scene* Current() const { return m_current.get(); }

private:
    std::unique_ptr<Scene> m_current;
};
