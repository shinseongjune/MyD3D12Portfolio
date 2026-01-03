#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include "EntityId.h"
#include "TransformComponent.h"
#include "MeshComponent.h"
#include "MaterialComponent.h"
#include "CameraComponent.h"
#include "RigidBodyComponent.h"
#include "ColliderComponent.h"
#include "CollisionEvents.h"

class World
{
public:
    World() = default;

    EntityId CreateEntity(const std::string& name = "");
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
	inline void Detach(EntityId child) { SetParent(child, EntityId::Invalid()); }
    bool IsDescendant(EntityId node, EntityId potentialAncestor) const;

    // 매 프레임 호출(또는 필요할 때 호출): dirty 트리 갱신
    void UpdateTransforms();

    // 순서 가드
    void BeginFrame();
    bool TransformsUpdatedThisFrame() const;

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

	std::vector<EntityId> m_pendingDestroy; // 지연 파괴 대상

    // 순서 가드
    uint64_t m_frameIndex = 0;
    uint64_t m_transformUpdatedFrame = UINT64_MAX;

private:
    void DestroyEntity(EntityId e);

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

    // --- Mesh Storage (sparse set) ---
    std::vector<uint32_t> m_meshSparse;
    std::vector<EntityId> m_meshDenseEntities;
    std::vector<MeshComponent> m_meshes;
    void EnsureMeshSparseSize(uint32_t entityIndex);
    void RemoveMesh(EntityId e);

    // --- Material Storage (sparse set) ---
    std::vector<uint32_t> m_materialSparse;
    std::vector<EntityId> m_materialDenseEntities;
    std::vector<MaterialComponent> m_materials;
    void EnsureMaterialSparseSize(uint32_t entityIndex);
    void RemoveMaterial(EntityId e);

    // --- Camera Storage (sparse set) ---
    std::vector<uint32_t> m_cameraSparse;
    std::vector<EntityId> m_cameraDenseEntities;
    std::vector<CameraComponent> m_cameras;
    void EnsureCameraSparseSize(uint32_t entityIndex);
    void RemoveCamera(EntityId e);

    // Rigidbody storage (dense/sparse)
    std::vector<RigidBodyComponent> m_rigidBodies;
    std::vector<EntityId> m_rigidBodyDenseEntities;
    std::vector<uint32_t> m_rigidBodySparse;

    // Collider storage
    std::vector<ColliderComponent> m_colliders;
    std::vector<EntityId> m_colliderDenseEntities;
    std::vector<uint32_t> m_colliderSparse;

	// Collision Events
    std::vector<CollisionEvent> m_collisionEvents;

public:
    // --- Transform Public API ---
    DirectX::XMFLOAT3 GetLocalPosition(EntityId e) const;
    void SetLocalPosition(EntityId e, const DirectX::XMFLOAT3& p);

    DirectX::XMFLOAT4 GetLocalRotation(EntityId e) const; // quat
    void SetLocalRotation(EntityId e, const DirectX::XMFLOAT4& q);

    DirectX::XMFLOAT3 GetLocalScale(EntityId e) const;
    void SetLocalScale(EntityId e, const DirectX::XMFLOAT3& s);

    void TranslateLocal(EntityId e, const DirectX::XMFLOAT3& delta);

    // World-space
    DirectX::XMFLOAT4X4 GetWorldMatrix(EntityId e) const;
    DirectX::XMFLOAT3  GetWorldPosition(EntityId e) const;

    // --- Mesh API ---
    void AddMesh(EntityId e, const MeshComponent& comp);
    bool HasMesh(EntityId e) const;
    MeshComponent& GetMesh(EntityId e);
    const MeshComponent& GetMesh(EntityId e) const;

    // --- Material API ---
    void AddMaterial(EntityId e, const MaterialComponent& m);
    bool HasMaterial(EntityId e) const;
    MaterialComponent& GetMaterial(EntityId e);
    const MaterialComponent& GetMaterial(EntityId e) const;

    // --- Camera API (아직 렌더러에서 미사용, 뼈대만) ---
    void AddCamera(EntityId e);
    bool HasCamera(EntityId e) const;
    CameraComponent& GetCamera(EntityId e);
    const CameraComponent& GetCamera(EntityId e) const;

    // 단순한 "활성 카메라" 찾기(첫 active 카메라)
    EntityId FindActiveCamera() const;

    // ---- Debug/Iteration ----
    // (임시) dense transform 엔티티 목록을 반환(시스템들이 순회하기 위해 필요)
    const std::vector<EntityId>& GetTransformEntities() const { return m_transformDenseEntities; }

	// ---- 파괴 지연 처리 ----
    void RequestDestroy(EntityId e);
    void FlushDestroy();

    // Rigidbody
	void EnsureRigidBodySparseSize(uint32_t entityIndex);
    void AddRigidBody(EntityId e, const RigidBodyComponent& rb);
    bool HasRigidBody(EntityId e) const;
    RigidBodyComponent& GetRigidBody(EntityId e);
    const RigidBodyComponent& GetRigidBody(EntityId e) const;
	void RemoveRigidBody(EntityId e);

    // Collider
	void EnsureColliderSparseSize(uint32_t entityIndex);
    void AddCollider(EntityId e, const ColliderComponent& c);
    bool HasCollider(EntityId e) const;
    ColliderComponent& GetCollider(EntityId e);
    const ColliderComponent& GetCollider(EntityId e) const;
	void RemoveCollider(EntityId e);

    // 물리 시스템이 후보를 빠르게 순회할 수 있게
    const std::vector<EntityId>& GetColliderEntities() const { return m_colliderDenseEntities; }

    // Collision Events
    void PushCollisionEvent(const CollisionEvent& ev);
    void DrainCollisionEvents(std::vector<CollisionEvent>& out); // out으로 옮기고 내부 비움

};
