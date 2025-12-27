#include "AssetPipeline.h"

#include <DirectXMath.h>
#include <string>

Result<EntityId> AssetPipeline::LoadModelIntoWorld(World& world,
    const std::string& path,
    const ImportOptions& importOpt,
    const SpawnModelOptions& spawnOpt)
{
    IAssetImporter* importer = m_registry.FindImporterForFile(path);
    if (!importer)
        return Result<EntityId>::Fail("No importer found for: " + path);

    auto imported = importer->Import(path, importOpt);
    if (!imported.IsOk())
        return Result<EntityId>::Fail(imported.error->message);

    const ImportedModel& model = imported.value;
    if (model.meshes.empty())
        return Result<EntityId>::Fail("Imported model has no meshes: " + path);

    // (현재 엔진 구조: MeshComponent는 MeshHandle 1개만 들 수 있으므로)
    // 가장 단순한 방식으로: root entity + mesh마다 child entity 생성.
    EntityId root = world.CreateEntity(spawnOpt.name);
    world.AddTransform(root);

    for (const auto& mesh : model.meshes)
    {
        EntityId e = world.CreateEntity(mesh.name.empty() ? "Mesh" : mesh.name);
        world.AddTransform(e);
        world.SetParent(e, root);

        // --- ImportedMesh -> MeshResource 변환 ---
        MeshResource mr{};
        mr.positions.reserve(mesh.vertices.size());
        for (const auto& v : mesh.vertices)
        {
            mr.positions.push_back(DirectX::XMFLOAT3(v.position.x, v.position.y, v.position.z));
        }

        mr.indices.reserve(mesh.indices.size());
        for (uint32_t idx : mesh.indices)
        {
            if (idx > 0xFFFFu)
                return Result<EntityId>::Fail("Mesh has index > 65535 (uint16 overflow). Need 32-bit index support.");

            mr.indices.push_back((uint16_t)idx);
        }

        MeshHandle h = m_meshManager.Create(mr);
        world.AddMesh(e, MeshComponent{ h });

        // --- Material(현재는 색만) ---
        world.AddMaterial(e);

        // 최소 규칙:
        // - submesh가 있으면 첫 submesh의 materialIndex를 사용
        // - 없으면 material 0 (있으면) / 아니면 기본 흰색
        DirectX::XMFLOAT4 color{ 1.f, 1.f, 1.f, 1.f };

        if (!model.materials.empty())
        {
            uint32_t mi = 0;
            if (!mesh.submeshes.empty())
                mi = mesh.submeshes[0].materialIndex;

            if (mi < (uint32_t)model.materials.size())
            {
                const auto& m = model.materials[mi];
                color = DirectX::XMFLOAT4(
                    m.baseColorFactor.x,
                    m.baseColorFactor.y,
                    m.baseColorFactor.z,
                    m.baseColorFactor.w
                );
            }
        }

        world.GetMaterial(e).color = color;
    }

    return Result<EntityId>::Ok(root);
}
