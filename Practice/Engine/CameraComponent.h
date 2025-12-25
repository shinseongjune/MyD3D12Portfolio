#pragma once

// 최소 카메라 파라미터(지금은 Renderer에 임시 카메라가 있으므로 아직 미사용)
struct CameraComponent
{
    float fovYDegrees = 60.0f;
    float nearZ = 0.1f;
    float farZ = 100.0f;
    bool  active = true;
};
