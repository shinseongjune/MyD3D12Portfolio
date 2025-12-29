#include "PrimitiveMeshes.h"
#include <DirectXMath.h>
#include <cmath>
#include <algorithm>

using DirectX::XMFLOAT3;

namespace PrimitiveMeshes
{
    MeshCPUData MakeUnitBox()
    {
        MeshCPUData mesh;

        mesh.positions = {
            {-0.5f,-0.5f,-0.5f}, {-0.5f, 0.5f,-0.5f}, { 0.5f, 0.5f,-0.5f}, { 0.5f,-0.5f,-0.5f},
            {-0.5f,-0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f}, { 0.5f, 0.5f, 0.5f}, { 0.5f,-0.5f, 0.5f},
        };

        mesh.indices = {
            0,1,2, 0,2,3,
            4,6,5, 4,7,6,
            4,5,1, 4,1,0,
            3,2,6, 3,6,7,
            1,5,6, 1,6,2,
            4,0,3, 4,3,7
        };

        return mesh;
    }

    MeshCPUData MakeUnitSphereUV(uint32_t stacks, uint32_t slices)
    {
        MeshCPUData mesh;

        stacks = std::max(2u, stacks);
        slices = std::max(3u, slices);

        const float radius = 0.5f;
        const float pi = 3.14159265358979323846f;

        // 정점: (stacks+1) * (slices+1)
        mesh.positions.reserve((stacks + 1) * (slices + 1));

        for (uint32_t i = 0; i <= stacks; ++i)
        {
            // phi: 0..pi (위->아래)
            float v = (float)i / (float)stacks;
            float phi = v * pi;

            float y = std::cos(phi) * radius;
            float r = std::sin(phi) * radius; // 해당 위도 원의 반지름

            for (uint32_t j = 0; j <= slices; ++j)
            {
                // theta: 0..2pi (둘레)
                float u = (float)j / (float)slices;
                float theta = u * (2.0f * pi);

                float x = std::cos(theta) * r;
                float z = std::sin(theta) * r;

                mesh.positions.push_back(XMFLOAT3{ x, y, z });
            }
        }

        // 인덱스: stacks * slices * 2 triangles
        const uint32_t vertPerRow = slices + 1;

        const uint32_t vertCount = (uint32_t)mesh.positions.size();
        // uint16 체크(디버그용으로 충분히 낮게 쓰면 OK)
        if (vertCount > 65535u)
        {
            // 너무 크면 강제로 줄이거나 여기서 assert/로그 처리해도 됨
            // 지금 기본값(stacks=6,slices=12)은 절대 안 걸림.
        }

        mesh.indices.reserve(stacks * slices * 6);

        for (uint32_t i = 0; i < stacks; ++i)
        {
            for (uint32_t j = 0; j < slices; ++j)
            {
                uint32_t a = i * vertPerRow + j;
                uint32_t b = (i + 1) * vertPerRow + j;
                uint32_t c = (i + 1) * vertPerRow + (j + 1);
                uint32_t d = i * vertPerRow + (j + 1);

                // (a,b,c), (a,c,d)
                mesh.indices.push_back((uint16_t)a);
                mesh.indices.push_back((uint16_t)b);
                mesh.indices.push_back((uint16_t)c);

                mesh.indices.push_back((uint16_t)a);
                mesh.indices.push_back((uint16_t)c);
                mesh.indices.push_back((uint16_t)d);
            }
        }

        return mesh;
    }
}
