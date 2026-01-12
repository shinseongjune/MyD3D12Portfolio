#pragma once
#include "Behaviour.h"
#include <DirectXMath.h>

enum class BillboardMode { Spherical, CylindricalY };

class BillboardBehaviour : public Behaviour
{
public:
    explicit BillboardBehaviour(BillboardMode m = BillboardMode::Spherical) : mode(m) {}
    void Update(SceneContext& ctx) override;

    BillboardMode mode;
};
