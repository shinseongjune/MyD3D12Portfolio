#pragma once
#include "Behaviour.h"
#include <DirectXMath.h>

enum class BillboardMode { Spherical, CylindricalY };

class BillboardComponent : public Behaviour
{
public:
    explicit BillboardComponent(BillboardMode m = BillboardMode::Spherical) : mode(m) {}
    void Update(SceneContext& ctx) override;

    BillboardMode mode;
};
