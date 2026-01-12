#pragma once
#include <DirectXMath.h>
#include "TransformComponent.h"
#include "FlightRigComponent.h"

// Stateless helper that advances one FlightRigComponent + Transform by dt.
struct FlightRigSystem
{
    static void Step(TransformComponent& tr, FlightRigComponent& rig, const FlightInput& in, float dt);
};
