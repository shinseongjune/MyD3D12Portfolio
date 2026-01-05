#pragma once
#include "EntityId.h"
#include "TextureHandle.h"
#include "DebugDraw.h"
#include "World.h"
#include "Input.h"
#include "MeshCPUData.h"
#include "TextureLoader_WIC.h"
#include "SceneContext.h"
#include "ImportTypes.h"
#include "Utilities.h"
#if defined(_DEBUG)
#include "PrimitiveMeshes.h"
#endif

struct SceneContext;

class Scene
{
public:
    virtual ~Scene() = default;
    virtual void OnLoad(SceneContext& ctx) = 0;
    virtual void OnUnload(SceneContext& ctx) = 0;

    virtual void OnUpdate(SceneContext& ctx) = 0;

	virtual void OnFixedUpdate(SceneContext& ctx) {}
};
