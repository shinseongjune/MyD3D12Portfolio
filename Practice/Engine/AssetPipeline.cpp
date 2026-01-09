#include "AssetPipeline.h"
#include <DirectXMath.h>
#include "ImportTypes.h"

Result<ModelAsset> AssetPipeline::ImportModel(
    const std::string& path,
    const ImportOptions& importOpt)
{
    IAssetImporter* importer = m_registry.FindImporterForFile(path);
    if (!importer)
        return Result<ModelAsset>::Fail("No importer found for: " + path);

    auto imported = importer->Import(path, importOpt);
    if (!imported.IsOk())
        return Result<ModelAsset>::Fail(imported.error->message);

    const ImportedModel& model = imported.value;
    if (model.meshes.empty())
        return Result<ModelAsset>::Fail("Imported model has no meshes: " + path);

    ModelAsset out{};
    out.sourcePath = model.sourcePath.empty() ? path : model.sourcePath;
    out.meshes.reserve(model.meshes.size());

    for (const auto& mesh : model.meshes)
    {
        // ImportedMesh -> MeshCPUData 변환
        MeshCPUData cpu{};
        cpu.positions.reserve(mesh.vertices.size());
		cpu.uvs.reserve(mesh.vertices.size());
        cpu.normals.reserve(mesh.vertices.size());

        for (const auto& v : mesh.vertices)
        {
            cpu.positions.push_back(DirectX::XMFLOAT3(v.position.x, v.position.y, v.position.z));
			cpu.uvs.push_back(DirectX::XMFLOAT2(v.uv.x, v.uv.y));
            cpu.normals.push_back({ v.normal.x, v.normal.y, v.normal.z });
        }

        cpu.indices.reserve(mesh.indices.size());
        for (uint32_t idx : mesh.indices)
        {
            if (idx > 0xFFFFu)
                return Result<ModelAsset>::Fail("Mesh has index > 65535 (uint16 overflow). Need 32-bit index support.");
            cpu.indices.push_back((uint16_t)idx);
        }

        MeshHandle h = m_meshManager.Create(cpu);

        // 현재는 color만: 기존 규칙 그대로
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
                    m.baseColorFactor.x, m.baseColorFactor.y,
                    m.baseColorFactor.z, m.baseColorFactor.w
                );
            }
        }

        ModelAssetMesh am{};
        am.submeshes.clear();
        if (!mesh.submeshes.empty())
        {
            am.submeshes.reserve(mesh.submeshes.size());
            for (const auto& sm : mesh.submeshes)
            {
                ModelAssetSubmesh asm2{};
                asm2.startIndex = sm.startIndex;
                asm2.indexCount = sm.indexCount;
                asm2.materialIndex = sm.materialIndex;
                asm2.name = sm.name;
                am.submeshes.push_back(std::move(asm2));
            }
        }
        else
        {
            // submesh 정보가 없으면 전체를 1개 submesh로 간주
            ModelAssetSubmesh one{};
            one.startIndex = 0;
            one.indexCount = (uint32_t)cpu.indices.size();
            one.materialIndex = 0;
            one.name = "Submesh0";
            am.submeshes.push_back(one);
        }
        am.name = mesh.name.empty() ? "Mesh" : mesh.name;
        am.mesh = h;
        am.baseColor = color;
        out.meshes.push_back(std::move(am));
    }

    return Result<ModelAsset>::Ok(std::move(out));
}

Result<EntityId> AssetPipeline::InstantiateModel(
    World& world,
    const ModelAsset& asset,
    const SpawnModelOptions& spawnOpt)
{
    if (asset.meshes.empty())
        return Result<EntityId>::Fail("ModelAsset has no meshes.");

    EntityId root = world.CreateEntity(spawnOpt.name);
    world.AddTransform(root);

    for (const auto& m : asset.meshes)
    {
        // mesh 전체가 아니라 submesh별로 draw를 쌓는다
        for (const auto& sm : m.submeshes)
        {
            world.AddMesh(root, MeshComponent{ m.mesh, sm.startIndex, sm.indexCount, sm.materialIndex });
        }
    }

    return Result<EntityId>::Ok(root);
}
