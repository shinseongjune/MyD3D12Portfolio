#pragma once

struct CameraComponent
{
    float fovYDegrees = 60.0f;
	float FovYRadians() const { return fovYDegrees * 3.14159265f / 180.0f; }
    float nearZ = 0.1f;
    float farZ = 100.0f;
    bool  active = true;
};
