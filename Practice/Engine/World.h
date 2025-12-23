#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

#include "EntityId.h"
#include "TransformComponent.h"

class World
{
public:
    World() = default;

    EntityId CreateEntity(const std::string& name = "");
    void DestroyEntity(EntityId e);
    bool IsAlive(EntityId e) const;

    // 이름 관련(소켓 찾기용으로 유용)
    EntityId FindByName(const std::string& name) const;
    const std::string& GetName(EntityId e) const;

    uint32_t AliveCount() const { return m_aliveCount; }

    // --- Transform API ---
    void AddTransform(EntityId e);
    bool HasTransform(EntityId e) const;
    TransformComponent& GetTransform(EntityId e);
    const TransformComponent& GetTransform(EntityId e) const;

    // parent/child 연결 (기존 parent 있으면 자동 분리)
    void SetParent(EntityId child, EntityId newParent);

    // 매 프레임 호출(또는 필요할 때 호출): dirty 트리 갱신
    void UpdateTransforms();

private:
    struct Slot
    {
        uint32_t generation = 0;
        bool alive = false;
        std::string name;
    };

    std::vector<Slot> m_slots;
    std::vector<uint32_t> m_freeList; // 재사용할 index 스택
    std::unordered_map<std::string, EntityId> m_nameToEntity;

    uint32_t m_aliveCount = 0;

private:
    void RemoveNameMapping(EntityId e);

    // --- Transform Storage (sparse set) ---
    static constexpr uint32_t InvalidDenseIndex = 0xFFFFFFFFu;

    std::vector<uint32_t> m_transformSparse;        // [entity.index] -> denseIndex or Invalid
    std::vector<EntityId> m_transformDenseEntities; // denseIndex -> EntityId
    std::vector<TransformComponent> m_transforms;   // denseIndex -> TransformComponent

    void EnsureTransformSparseSize(uint32_t entityIndex);

    void RemoveTransform(EntityId e);

    void MarkDirtyRecursive(EntityId e);

    DirectX::XMMATRIX LocalMatrix(const TransformComponent& t) const;
    void UpdateWorldRecursive(EntityId e, const DirectX::XMMATRIX& parentWorld);

public:
    // --- Transform Public API ---
    DirectX::XMFLOAT3 GetLocalPosition(EntityId e) const;
    void SetLocalPosition(EntityId e, const DirectX::XMFLOAT3& p);

    DirectX::XMFLOAT4 GetLocalRotation(EntityId e) const; // quat
    void SetLocalRotation(EntityId e, const DirectX::XMFLOAT4& q);

    DirectX::XMFLOAT3 GetLocalScale(EntityId e) const;
    void SetLocalScale(EntityId e, const DirectX::XMFLOAT3& s);

    // World-space
    DirectX::XMFLOAT4X4 GetWorldMatrix(EntityId e) const;
    DirectX::XMFLOAT3  GetWorldPosition(EntityId e) const;

    // 편의: Transform 보장(없으면 자동 추가)
    void EnsureTransform(EntityId e);
};
